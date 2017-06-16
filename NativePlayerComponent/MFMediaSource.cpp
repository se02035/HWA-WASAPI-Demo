#include "pch.h"
#include "mfidl.h"
#include "mfapi.h"
#include "mferror.h"
#include "mfmediasource.h"
#include "assert.h"
#include <windows.foundation.h>
#include <wrl.h>
using namespace Platform;  

#define SIMULATE_SUB_SAMPLING 0  // Simulate sub sampling for celar content to make sure decoder handles sub-sampling case

#define CHECKHR_GOTO( val, label ) { hr = (val); if( FAILED( hr ) ) { goto label; } }

#define GENERIC_TIMEOUT_MS 1000000

#define ES_SOURCE_CACHE_SIZE 512*1024

//#define USE_INBOX_MBR
#define USE_SSPK_SDK



#define PLAYREADY_MEDIA_PROTECTION_SYSTEM_ID L"{F4637010-03C3-42CD-B932-B48ADF3A6A54}"
#define PLAYREADY_ITA_ActivatableClassId L"Windows.Media.Protection.PlayReady.PlayReadyWinRTTrustedInput"

#include "initguid.h"
DEFINE_GUID( CLSID_PlayReadySystemID, 0xF4637010, 0x03C3, 0x42CD, 0xB9, 0x32, 0xB4, 0x8A, 0xDF, 0x3A, 0x6A, 0x54 );  
DEFINE_GUID( MEDIASUBTYPE_RAW_AAC1, 0x000000FF, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID( MFSampleExtension_Encryption_SubSampleMapping, 0x8444F27A, 0x69A1, 0x48DA, 0xBD, 0x08, 0x11, 0xCE, 0xF3, 0x68, 0x30, 0xD2 );
//richfr DEFINE_GUID( MFSampleExtension_Encryption_SubSampleMappingSplit, 0xfe0254b9, 0x2aa5, 0x4edc, 0x99, 0xf7, 0x17, 0xe8, 0x9d, 0xbf, 0x91, 0x74 );
//richfr DEFINE_GUID( MFSampleExtension_Content_KeyID, 0xc6c7f5b0, 0xacca, 0x415b, 0x87, 0xd9, 0x10, 0x44, 0x14, 0x69, 0xef, 0xc6 );
DEFINE_GUID( MFASFSampleExtension_Encryption_SampleID,  0x6698B84E, 0x0AFA, 0x4330, 0xAE, 0xB2, 0x1C, 0x0A, 0x98, 0xD7, 0xA4, 0x4D );

CMFMediaSource::CMFMediaSource()
    :m_nRefCount( 1 )
    ,m_hSourceSyncEvent( NULL )
    ,m_hAudioSyncEvent( NULL )
    ,m_hVideoSyncEvent( NULL )
{
}

HRESULT CMFMediaSource::CreateInstance( LPCWSTR url, IMediaSource ** pMediaSource )
{
    HRESULT hr = S_OK;
    ComPtr<CMFMediaSource> spSource = new CMFMediaSource();
    if( spSource == NULL ) return E_OUTOFMEMORY;
    CHECKHR_GOTO( spSource->Open( url ), done );
done:
    *pMediaSource = spSource.Get();
    return hr;
}

ULONG CMFMediaSource::AddRef()
{
    assert( m_nRefCount >= 0 );
    return InterlockedIncrement(&m_nRefCount);
}

ULONG CMFMediaSource::Release()
{
    assert( m_nRefCount > 0 );
    ULONG uCount = InterlockedDecrement(&m_nRefCount);
    if (uCount == 0)
    {
        delete this;
    }
    return uCount;
}

HRESULT CMFMediaSource::QueryInterface(REFIID iid, void** ppv)
{
    HRESULT hr = S_OK;
    if( ppv == NULL ) return E_POINTER;

    if (iid == __uuidof(IUnknown) || iid == __uuidof(IMediaSource) )
    {
        *ppv = static_cast<IMediaSource*>(this);
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    AddRef();
    return hr;
}

CMFMediaSource::~CMFMediaSource()
{
    Close();
}

void CMFMediaSource::Close()
{
    if( m_spSource )
    {
        m_spSource->Shutdown();
        m_spSource.Reset();
    }
    if( m_hSourceSyncEvent )
    {
        CloseHandle( m_hSourceSyncEvent );
        m_hSourceSyncEvent = NULL;
    }
    if( m_hAudioSyncEvent )
    {
        CloseHandle( m_hAudioSyncEvent );
        m_hAudioSyncEvent = NULL;
    }
    if( m_hVideoSyncEvent )
    {
        CloseHandle( m_hVideoSyncEvent );
        m_hVideoSyncEvent = NULL;
    }


    for( int i = 0; i < (int)MAXSTREAM; i++ )
    {
        m_rgMediaType[i].Reset();
        m_rgStream[i].Reset();
    }
}

HRESULT CMFMediaSource::Open( LPCWSTR url )
{
    HRESULT hr = S_OK;
    ComPtr<IMFSourceResolver> spResolver;
    ComPtr<IUnknown> spUnk;
    ComPtr<IMFAsyncResult> spResult;
    MF_OBJECT_TYPE resolveType;
    
    ComPtr<IPropertyStore>    spPropertyStore;

    m_hSourceSyncEvent = CreateEventEx( NULL, NULL, 0, EVENT_ALL_ACCESS );
    m_hAudioSyncEvent = CreateEventEx( NULL, NULL, 0, EVENT_ALL_ACCESS );
    m_hVideoSyncEvent = CreateEventEx( NULL, NULL, 0, EVENT_ALL_ACCESS );

    CHECKHR_GOTO( MFCreateSourceResolver( spResolver.GetAddressOf() ), done );
    
    CHECKHR_GOTO( spResolver->BeginCreateObjectFromURL( url, MF_RESOLUTION_MEDIASOURCE, spPropertyStore.Get(), NULL, &m_xOnOpenCompleted, NULL ), done );
    CHECKHR_GOTO( WaitForCallbackComplete( spResult.ReleaseAndGetAddressOf(), GENERIC_TIMEOUT_MS ), done );
    CHECKHR_GOTO( spResolver->EndCreateObjectFromURL( spResult.Get(), &resolveType, spUnk.ReleaseAndGetAddressOf() ), done );
    CHECKHR_GOTO( spUnk.As( &m_spSource ), done );
    CHECKHR_GOTO( Seek( 0 ), done );
    
done:
    return hr;
}

HRESULT CMFMediaSource::WaitForCallbackComplete(IMFAsyncResult ** ppResult, DWORD msTimeOut )
{
    if( WaitForSingleObjectEx( m_hSourceSyncEvent, msTimeOut, FALSE ) == WAIT_TIMEOUT )
    {
        return E_ABORT;
    }

    *ppResult = m_spAsyncResult.Get();
    (*ppResult)->AddRef();
    m_spAsyncResult.Reset();

    return (*ppResult)->GetStatus();
}

void CMFMediaSource::OnVideoStreamEvent( IMFAsyncResult * pResult )
{
    m_spVideoEventResult = pResult;
    SetEvent( m_hVideoSyncEvent );
}

void CMFMediaSource::OnAudioStreamEvent( IMFAsyncResult * pResult )
{
    m_spAudioEventResult = pResult;
    SetEvent( m_hAudioSyncEvent );
}

void CMFMediaSource::OnOpenCompleted( IMFAsyncResult * pResult )
{
    m_spAsyncResult = pResult;
    SetEvent( m_hSourceSyncEvent );
}

HRESULT CMFMediaSource::ReadSample( eStreamType streamType,  IMFSample **ppSample )
{
    HRESULT hr = S_OK;
    ComPtr<IMFAsyncCallback> spStreamEventCallback;
    
    if( ppSample == NULL ) CHECKHR_GOTO( E_POINTER, done );

    *ppSample = NULL;

    if( streamType >= MAXSTREAM || m_rgMediaType[streamType] == NULL || ppSample == NULL )
    {
        CHECKHR_GOTO( E_INVALIDARG, done );
    }

    if( m_rgStream[streamType] == NULL )
    {
        CHECKHR_GOTO( MF_E_END_OF_STREAM , done );
    }


    if( streamType == AUDIOSTREAM )
    {
        spStreamEventCallback = &m_xOnAudioStreamEvent;
    }
    else
    {
        spStreamEventCallback = &m_xOnVideoStreamEvent;
    }

    if (VIDEOSTREAM == streamType && m_spPendingVideoSampleQueue.size() > 0)
    {
        ComPtr<IMFSample> spSample = m_spPendingVideoSampleQueue.front();
        m_spPendingVideoSampleQueue.pop();
        *ppSample = spSample.Detach();
        hr = S_OK;
        goto done;
    }

    CHECKHR_GOTO( m_rgStream[streamType]->BeginGetEvent( spStreamEventCallback.Get(), NULL ),done);
    CHECKHR_GOTO( m_rgStream[streamType]->RequestSample( NULL ), done );
    *ppSample = NULL;

    while( *ppSample == NULL )
    {
        ComPtr<IMFMediaEvent> spEvent;
        ComPtr<IMFAsyncResult> spEventResult;

        WaitForSingleObjectEx(streamType == AUDIOSTREAM? m_hAudioSyncEvent: m_hVideoSyncEvent, INFINITE, FALSE );
        CHECKHR_GOTO(m_rgStream[streamType]->EndGetEvent(streamType == AUDIOSTREAM? m_spAudioEventResult.Get():  m_spVideoEventResult.Get(), &spEvent),done);

        MediaEventType met;
        HRESULT hrStatus;
        CHECKHR_GOTO( spEvent->GetStatus(&hrStatus), done );
        CHECKHR_GOTO( hrStatus, done );
        CHECKHR_GOTO( spEvent->GetType( &met ), done );
        if( met == MEMediaSample )
        {
            ComPtr<IMFSample> spSample;
            PROPVARIANT varEvent;
            CHECKHR_GOTO( spEvent->GetValue( &varEvent ), done );
            hr = varEvent.punkVal->QueryInterface( IID_IMFSample, (void **)spSample.GetAddressOf());
            PropVariantClear(&varEvent);
            CHECKHR_GOTO( hr, done );
#if 1
            // DemoCreateSample just to demostrate how app can create a media sample to be used by MFTs.
            CHECKHR_GOTO( DemoCreateMediaSample( spSample.Get() , streamType, ppSample ), done );
#else
            *ppSample = spSample.Detach();
#endif
            assert( *ppSample );
            goto done;                                 
        }
        else if( met == MEEndOfStream  )
        {
            // reset rgStream to indicate EOS event.
            m_rgStream[streamType].Reset();
            CHECKHR_GOTO( MF_E_END_OF_STREAM , done );
        }

        CHECKHR_GOTO( m_rgStream[streamType]->BeginGetEvent( spStreamEventCallback.Get(), NULL ),done);
    }

done:
    return hr;
}

//  this function just to show how to create a mediatype to be used by MFTs. 
HRESULT CMFMediaSource::DemoCreateMediaType( IMFMediaType * pNativeMT,  IMFMediaType ** ppMT )
{
    HRESULT hr = S_OK;
    ComPtr<IMFMediaType> spMT;
    GUID subType;
    GUID majorType;

    *ppMT = NULL;
    CHECKHR_GOTO( pNativeMT->GetMajorType( &majorType ), done);
    CHECKHR_GOTO( pNativeMT->GetGUID( MF_MT_SUBTYPE , &subType ), done );
    CHECKHR_GOTO( MFCreateMediaType( spMT.GetAddressOf() ), done );

    if( majorType == MFMediaType_Video )
    {
        UINT32 dwWidth, dwHeight;

        // first major type
        CHECKHR_GOTO( spMT->SetGUID( MF_MT_MAJOR_TYPE, MFMediaType_Video ), done);

        // set frame size if exist. 
        if( S_OK == MFGetAttributeSize( pNativeMT, MF_MT_FRAME_SIZE, &dwWidth, &dwHeight ) ) 
        {
            MFSetAttributeSize( spMT.Get(), MF_MT_FRAME_SIZE, dwWidth, dwHeight );
        }

        UINT32 dwNumerator = 0;
        UINT32 dwDenominator = 0;
        if (S_OK == MFGetAttributeRatio( pNativeMT, MF_MT_FRAME_RATE, &dwNumerator, &dwDenominator))
        {
            MFSetAttributeRatio( spMT.Get(), MF_MT_FRAME_RATE, dwNumerator, dwDenominator);
        }

        // set codec specific attribute.
        if( subType == MFVideoFormat_WMV1 || subType == MFVideoFormat_WMV2 || subType == MFVideoFormat_WMV3 || subType == MFVideoFormat_WVC1 )
        {
            UINT32  dwBlobSize;
            CHECKHR_GOTO( spMT->SetGUID( MF_MT_SUBTYPE, subType ), done );
            if( SUCCEEDED( pNativeMT->GetBlobSize(MF_MT_USER_DATA, &dwBlobSize) ) && dwBlobSize > 0 )
            {
                BYTE * UserData = new BYTE[dwBlobSize];
                (void)pNativeMT->GetBlob(MF_MT_USER_DATA, UserData, dwBlobSize, NULL);
                (void)spMT->SetBlob(MF_MT_USER_DATA, UserData, dwBlobSize);
                delete [] UserData;
                CHECKHR_GOTO( hr, done );
            }
        }
        else if( subType == MFVideoFormat_H264 || subType == MFVideoFormat_H264_ES  )
        {
            if( m_rgITA[VIDEOSTREAM] != NULL )
            {
                // playready subsampling will split video samples, MFVideoFormat_H264_ES needs to be set in this case.
                CHECKHR_GOTO( spMT->SetGUID( MF_MT_SUBTYPE, MFVideoFormat_H264_ES ), done );
            }
            else
            {
                CHECKHR_GOTO( spMT->SetGUID( MF_MT_SUBTYPE, subType ), done );
            }
        }
        else if( subType == MFVideoFormat_HEVC_ES || subType == MFVideoFormat_HEVC )
        {
            if( m_rgITA[VIDEOSTREAM] != NULL )
            {
                // playready subsampling will split video samples, MFVideoFormat_H264_ES needs to be set in this case.
                CHECKHR_GOTO( spMT->SetGUID( MF_MT_SUBTYPE, MFVideoFormat_HEVC_ES ), done );
            }
            else
            {
                CHECKHR_GOTO( spMT->SetGUID( MF_MT_SUBTYPE, subType ), done );
            }
        }
        else
        {
            spMT = pNativeMT; // // other media types
        }
    }
    else if( majorType == MFMediaType_Audio )
    {
        UINT32 Playload;
        UINT32 Channels;
        UINT32 Mask;
        UINT32 SamplesPerSecond;
        UINT32  dwBlobSize;

        CHECKHR_GOTO( spMT->SetGUID( MF_MT_MAJOR_TYPE, MFMediaType_Audio ), done);
        CHECKHR_GOTO(pNativeMT->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &Channels), done);
        CHECKHR_GOTO(spMT->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, Channels), done);

        if(SUCCEEDED( pNativeMT->GetUINT32(MF_MT_AUDIO_CHANNEL_MASK, &Mask)) )
        {
            CHECKHR_GOTO(spMT->SetUINT32(MF_MT_AUDIO_CHANNEL_MASK, Mask), done);
        }

        if( SUCCEEDED( pNativeMT->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &SamplesPerSecond)))
        {
            CHECKHR_GOTO(spMT->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, SamplesPerSecond), done);
        }

        if( subType == MFAudioFormat_AAC || subType == MEDIASUBTYPE_RAW_AAC1 )
        {
            CHECKHR_GOTO(spMT->SetGUID(MF_MT_SUBTYPE, subType), done);
            if( SUCCEEDED(pNativeMT->GetUINT32(MF_MT_AAC_PAYLOAD_TYPE, &Playload)))
            {
                CHECKHR_GOTO(spMT->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, Playload), done); 
            }
            else
            {
                CHECKHR_GOTO(spMT->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0), done); 
            }

            if( SUCCEEDED( pNativeMT->GetBlobSize(MF_MT_USER_DATA, &dwBlobSize) ) && dwBlobSize > 0 )
            {
                BYTE * UserData = new BYTE[dwBlobSize];
                (void)pNativeMT->GetBlob(MF_MT_USER_DATA, UserData, dwBlobSize, NULL);
                (void)spMT->SetBlob(MF_MT_USER_DATA, UserData, dwBlobSize);
                delete [] UserData;
                CHECKHR_GOTO( hr, done );
            }
        }
        else if( subType == MFAudioFormat_Dolby_AC3 || subType == MFAudioFormat_Dolby_DDPlus )
        {
            CHECKHR_GOTO(spMT->SetGUID(MF_MT_SUBTYPE, subType), done);
        }
        else
        {
            spMT = pNativeMT; // // other media types
        }
    }
    else
    {
        spMT = pNativeMT; // // other media types
    }
    
done:
    if( SUCCEEDED( hr ) )
    {
        *ppMT = spMT.Detach();
    }
    return hr;
}

//  this function just to show how to create a mediasample to be used by MFTs. 
HRESULT CMFMediaSource::DemoCreateMediaSample( IMFSample * pNativeSample,  eStreamType streamType, IMFSample ** ppOutputSample )
{
    HRESULT hr = S_OK;

    ComPtr<IMFSample> spSample ;
    ComPtr<IMFMediaBuffer> spNativeBuffer;
    ComPtr<IMFMediaBuffer> spSampleBuffer;
    DWORD dwBufferSize = 0;
    LONGLONG llSampleTime = 0;
    LONGLONG llSampleDuration = 0;
    UINT32 fKeyFrame = 0;
    UINT32 fDiscontinuity = 0;
    UINT32 cbBlobSize;
    BYTE * pbNativeBuffer;
    BYTE * pbNewBuffer;
    DWORD dwMaxBufferSize;
    GUID  guid;
    UINT32 uiSampleKeyID;
    bool fNativeBufferLocked = false;
    DWORD dwNativeBufferOffset = 0;

    CHECKHR_GOTO( pNativeSample->ConvertToContiguousBuffer( spNativeBuffer.ReleaseAndGetAddressOf() ), done);
    CHECKHR_GOTO( spNativeBuffer->GetCurrentLength( &dwBufferSize ), done );
    CHECKHR_GOTO( spNativeBuffer->Lock( &pbNativeBuffer, &dwMaxBufferSize, &dwBufferSize ), done );
    fNativeBufferLocked = true;
    // Split sample into multiple samples to simulate playready case
    DWORD dwNewSampleCount = 1;
    DWORD dwNewSampleSize = dwBufferSize;
    DWORD dwNewSizeTotal = 0;

#if SIMULATE_SUB_SAMPLING

    if (VIDEOSTREAM == streamType)
    {
        // Get a semi-random sub-sampling size
        LARGE_INTEGER liNow = {};
        QueryPerformanceCounter(&liNow);

        dwNewSampleSize = liNow.QuadPart % 10000;
        if (dwNewSampleSize < 2001)
        {
            dwNewSampleSize = 2001;
        }
        dwNewSampleCount = dwBufferSize / dwNewSampleSize;
        if (0 == dwNewSampleCount)
        {
            dwNewSampleCount = 1;
        }
        dwNewSampleSize = dwBufferSize / dwNewSampleCount;
    }
#endif

    for (DWORD i = 0; i < dwNewSampleCount; i ++)
    {
        CHECKHR_GOTO( MFCreateSample( spSample.GetAddressOf() ) , done ); 
        DWORD dwNewBufferSize = (i < (dwNewSampleCount - 1)) ? dwNewSampleSize : dwBufferSize - dwNewSampleSize * i;
        CHECKHR_GOTO( MFCreateMemoryBuffer(dwNewBufferSize , spSampleBuffer.GetAddressOf() ), done );
        CHECKHR_GOTO( spSample->AddBuffer( spSampleBuffer.Get() ), done );

        // copy sample buffer
        DWORD dwSize = 0;
        spSampleBuffer->Lock( &pbNewBuffer,  &dwMaxBufferSize, &dwSize );
        memcpy(pbNewBuffer, pbNativeBuffer + dwNativeBufferOffset, dwNewBufferSize );
        dwNativeBufferOffset += dwNewBufferSize;
        spSampleBuffer->SetCurrentLength( dwNewBufferSize );
        dwNewSizeTotal += dwNewBufferSize;
        spSampleBuffer->Unlock();

        if (VIDEOSTREAM == streamType)
        {
            m_spPendingVideoSampleQueue.push(spSample);
        }

        // now set new sample attribute.
        if (0 == i)
        {
            // Sample time is only for the first sub sample
            if( SUCCEEDED( pNativeSample->GetSampleTime(&llSampleTime) ) )
            {
                CHECKHR_GOTO( spSample->SetSampleTime( llSampleTime ), done );
            }

            if( SUCCEEDED( pNativeSample->GetSampleDuration(&llSampleDuration) ) )
            {
                // duration is optional
                CHECKHR_GOTO( spSample->SetSampleDuration( llSampleDuration ), done );
            }

            if( SUCCEEDED( pNativeSample->GetUINT32( MFSampleExtension_CleanPoint, &fKeyFrame ) ) )
            {
                // cleanpoint(keyframe ) is optional
                CHECKHR_GOTO( spSample->SetUINT32( MFSampleExtension_CleanPoint, fKeyFrame? 1: 0 ), done );
            }

            if( SUCCEEDED( pNativeSample->GetUINT32( MFSampleExtension_Discontinuity, &fDiscontinuity ) ) )
            {
                // discontinuity flag is optional
                CHECKHR_GOTO( spSample->SetUINT32( MFSampleExtension_Discontinuity, fDiscontinuity? 1: 0 ), done );
            }
        }

        // set playready attributes
        if( SUCCEEDED( pNativeSample->GetBlobSize(MFSampleExtension_Encryption_SubSampleMapping, &cbBlobSize ) ) )
        {
            BYTE * pBlob = new BYTE[cbBlobSize];
            hr = pNativeSample->GetBlob(MFSampleExtension_Encryption_SubSampleMapping, pBlob, cbBlobSize, &cbBlobSize );
            if( SUCCEEDED( hr ) )
            {
                hr = spSample->SetBlob( MFSampleExtension_Encryption_SubSampleMapping, pBlob, cbBlobSize );
            }
            delete [] pBlob;
            CHECKHR_GOTO( hr, done );
        }

        if( SUCCEEDED( pNativeSample->GetBlobSize(MFSampleExtension_Encryption_SubSampleMappingSplit, &cbBlobSize ) ) )
        {
            BYTE * pBlob = new BYTE[cbBlobSize];
            hr = pNativeSample->GetBlob(MFSampleExtension_Encryption_SubSampleMappingSplit, pBlob, cbBlobSize, &cbBlobSize );
            if( SUCCEEDED( hr ) )
            {
                hr = spSample->SetBlob( MFSampleExtension_Encryption_SubSampleMappingSplit, pBlob, cbBlobSize );
            }
            delete [] pBlob;
            CHECKHR_GOTO( hr, done );
        }

        if( SUCCEEDED( pNativeSample->GetGUID(MFSampleExtension_Content_KeyID, &guid ) ) )
        {
            CHECKHR_GOTO( spSample->SetGUID( MFSampleExtension_Content_KeyID, guid), done );
        }

        if( SUCCEEDED( pNativeSample->GetBlobSize(MFASFSampleExtension_Encryption_SampleID, &cbBlobSize ) ) )
        {
            BYTE * pBlob = new BYTE[cbBlobSize];
            hr = pNativeSample->GetBlob(MFASFSampleExtension_Encryption_SampleID, pBlob, cbBlobSize, &cbBlobSize );
            if( SUCCEEDED( hr ) )
            {
                hr = spSample->SetBlob( MFASFSampleExtension_Encryption_SampleID, pBlob, cbBlobSize );
            }
            delete [] pBlob;
            CHECKHR_GOTO( hr, done );
        }

        if( SUCCEEDED( pNativeSample->GetUINT32( MFSampleExtension_SampleKeyID, &uiSampleKeyID ) ) )
        {
            // discontinuity flag is optional
            CHECKHR_GOTO( spSample->SetUINT32( MFSampleExtension_SampleKeyID, uiSampleKeyID ), done );
        }
    }

    if (dwNewSizeTotal != dwBufferSize)
    {
        __debugbreak();
    }

done:
    if (fNativeBufferLocked)
    {
        spNativeBuffer->Unlock();
    }
    if( SUCCEEDED(hr) )
    {
        if (VIDEOSTREAM == streamType)
        {
            ComPtr<IMFSample> spSample = m_spPendingVideoSampleQueue.front();
            m_spPendingVideoSampleQueue.pop();
            *ppOutputSample = spSample.Detach();
        }
        else
        {
            *ppOutputSample = spSample.Detach();
        }
    }
    return hr;
}
/*richfr
//  this function just to show how to create ITA from drm header, it is not used in MF source right now, 
// because MF source internally created the ITA using the same logic and we can not get the DRM header from MF source right now, 

HRESULT CMFMediaSource::DemoCreateITAFromDRMHeader( DWORD dwStreamID, void * pDrmHeaderBuffer, DWORD cbDrmHeaderSize, IMFInputTrustAuthority ** ppITA )
{
    HRESULT hr = S_OK;
    ComPtr<IStream>  spInitStm;  
    DWORD  cStreams = 2;  // assume both audio/video stream are protected and using the same license for now.
    ComPtr<IMFInputTrustAuthority> spITA;

    if( m_spTrustedInput == NULL )
    {
        CHECKHR_GOTO( CreateStreamOnHGlobal( NULL, TRUE, &spInitStm ), done );  

        // Initialize spInitStm with the required data  
        // Format: (All DWORD values are serialized in little-endian order)  
        // [GUID (content protection system guid, see msprita.idl)]  
        // [DWORD (stream count, use the actual stream count even if all streams are encrypted using the same data, note that zero is invalid)]  
        // [DWORD (next stream ID, use -1 if all remaining streams are encrypted using the same data)]  
        // [DWORD (next stream's binary data size)]  
        // [BYTE* (next stream's binary data)]  
        // { Repeat from "next stream ID" above for each stream }  

        ULONG cbWritten = 0;  
        CHECKHR_GOTO( spInitStm->Write( &CLSID_PlayReadySystemID, sizeof(CLSID_PlayReadySystemID), &cbWritten ), done);  
        CHECKHR_GOTO( spInitStm->Write( &cStreams, sizeof(cStreams), &cbWritten ), done );

        static const DWORD dwAllStreams = (DWORD)-1;  
        CHECKHR_GOTO( spInitStm->Write( &dwAllStreams, sizeof(dwAllStreams), &cbWritten ), done );
        CHECKHR_GOTO( spInitStm->Write( &cbDrmHeaderSize, sizeof(cbDrmHeaderSize), &cbWritten ), done );
        if( 0 != cbDrmHeaderSize )  
        {  
            CHECKHR_GOTO( spInitStm->Write( pDrmHeaderBuffer, cbDrmHeaderSize, &cbWritten ), done );
        }  

        //  
        // move the pointer location in the stream to the beginning of the stream  
        // because the Load of an IPersistStream expects the pointer to be  
        // BEFORE the data  
        //  
        {  
            LARGE_INTEGER liSeekPosition = { 0 };  
            CHECKHR_GOTO( spInitStm->Seek( liSeekPosition , STREAM_SEEK_SET, NULL ), done );
        }  

        {
            ComPtr<IInspectable> spObject;  
            ComPtr<IPersistStream> spPersistStream;  
            CHECKHR_GOTO( ::Windows::Foundation::ActivateInstance( ::Microsoft::WRL::Wrappers::HStringReference( L"Windows.Media.Protection.PlayReady.PlayReadyWinRTTrustedInput" ).Get(), &spObject ), done );
            CHECKHR_GOTO( spObject.As( &spPersistStream ), done );
            CHECKHR_GOTO( spPersistStream->Load( spInitStm.Get() ), done );
            CHECKHR_GOTO( spObject.As( &m_spTrustedInput ), done );
        }
    }

    {
        ComPtr<IUnknown> spUnk;  
        CHECKHR_GOTO( m_spTrustedInput->GetInputTrustAuthority( dwStreamID, IID_IMFInputTrustAuthority, spUnk.ReleaseAndGetAddressOf() ), done);  
        CHECKHR_GOTO( spUnk.As( &spITA ), done );
        *ppITA = spITA.Detach();
    }

done:
    return hr;
}
*/
HRESULT CMFMediaSource::GetMediaType( eStreamType streamType,  IMFMediaType **ppMediaType, IMFInputTrustAuthority **ppITA )
{
    HRESULT hr = S_OK;
    if( streamType >= MAXSTREAM )
    {
        CHECKHR_GOTO( E_INVALIDARG, done  );
    }

    if( ppMediaType == NULL ) 
    {
        CHECKHR_GOTO( E_POINTER, done  );
    }

    *ppMediaType = m_rgMediaType[streamType].Get();
    if( *ppMediaType )
    {
        (*ppMediaType)->AddRef();
    }
    else
    {
        CHECKHR_GOTO( E_INVALIDARG, done );
    }

    if( ppITA )
    {
        *ppITA = m_rgITA[streamType].Get();
        if( *ppITA )
        {
            (*ppITA)->AddRef();
        }
    }
    
done:
    return hr;
}

eStreamType CMFMediaSource::GetStreamTypeByMajorType( GUID & guidMajorType )
{
    if( guidMajorType == MFMediaType_Audio )
    {
        return AUDIOSTREAM;
    }
    else if( guidMajorType == MFMediaType_Video )
    {
        return VIDEOSTREAM;
    }
    else 
    {
        return MAXSTREAM;
    }
}

HRESULT CMFMediaSource::Seek( LONGLONG hnsTime )
{
    ComPtr<IMFPresentationDescriptor> spPD;
    HRESULT hr = S_OK;
    DWORD cStream = 0;
    ComPtr<IMFMediaEvent> spEvent;
    ComPtr<IMFMediaTypeHandler> spMediatypeHandler;
    ComPtr<IMFStreamDescriptor> spSD;
    GUID guidMajorType;
    eStreamType streamType;
    BOOL fSourceStarted = FALSE;
    bool fSeekCompleted = FALSE; 

    if( m_spSource == NULL ) return MF_E_NOT_INITIALIZED;
    CHECKHR_GOTO( m_spSource->CreatePresentationDescriptor( spPD.GetAddressOf() ), done );
    CHECKHR_GOTO( spPD->GetStreamDescriptorCount( &cStream ), done );

    for( int i = 0; i < (int)MAXSTREAM; i++ )
    {
        m_rgMediaType[i].Reset();
        m_rgStream[i].Reset();
    }

    for( DWORD iStream = 0; iStream < cStream; iStream++ )
    {
        BOOL fSelected = FALSE;
        BOOL fProtected = FALSE;
        DWORD dwStreamID = 0;
        CHECKHR_GOTO( spPD->GetStreamDescriptorByIndex( iStream, &fSelected, spSD.ReleaseAndGetAddressOf() ), done );
        CHECKHR_GOTO( spSD->GetMediaTypeHandler( spMediatypeHandler.ReleaseAndGetAddressOf() ), done );
        CHECKHR_GOTO( spSD->GetStreamIdentifier( &dwStreamID ), done );
        CHECKHR_GOTO( spMediatypeHandler->GetMajorType( &guidMajorType ), done );
        streamType  = GetStreamTypeByMajorType( guidMajorType );
        if( streamType == MAXSTREAM ) continue;
        // select the first stream for each mediatype 
        if( m_rgMediaType[streamType] == NULL )
        {
            ComPtr<IMFMediaType> spMediaType;
            ComPtr<IMFMediaType> spNativeMediaType;
            GUID guidMediaType;

            CHECKHR_GOTO( spMediatypeHandler->GetCurrentMediaType( spMediaType.ReleaseAndGetAddressOf() ), done );
            CHECKHR_GOTO( spMediaType->GetMajorType( &guidMediaType ), done );
            if( MFMediaType_Protected == guidMediaType )
            {
                // get clear type
                CHECKHR_GOTO( MFUnwrapMediaType(spMediaType.Get(), spNativeMediaType.ReleaseAndGetAddressOf() ), done );
            }
            else
            {
                spNativeMediaType = spMediaType;
            }

#if 1
            // demonstrate how to create a usable media type by app to used by MFT. 
            CHECKHR_GOTO( DemoCreateMediaType( spNativeMediaType.Get() , m_rgMediaType[streamType].ReleaseAndGetAddressOf() ), done );
#else
            m_rgMediaType[streamType] = spNativeMediaType.Get();
#endif

            CHECKHR_GOTO( spPD->SelectStream( iStream ), done );
        }

        fProtected = MFGetAttributeUINT32(spSD.Get(), MF_SD_PROTECTED, FALSE);

        if( fProtected && m_rgITA[streamType] == NULL )
        {
            // 
            // MF media source has exposed the ITA from media source interface, 
            // for none MF media source, please reference to DemoCreateITAFromDRMHeader to check how app can create ITA from DRM protected content header. 
            // 

            ComPtr<IMFTrustedInput> spTrustInput; 
            DWORD dwStreamID = 0;
            CHECKHR_GOTO( m_spSource.As( &spTrustInput ), done );
            CHECKHR_GOTO( spSD->GetStreamIdentifier( &dwStreamID ), done );
            CHECKHR_GOTO( spTrustInput->GetInputTrustAuthority( dwStreamID, IID_IMFInputTrustAuthority, (IUnknown **)m_rgITA[streamType].ReleaseAndGetAddressOf() ), done );
        }
    }
    {
        PROPVARIANT var;
        PropVariantInit(&var);
        var.vt = VT_I8;
        var.hVal.QuadPart = hnsTime;
        CHECKHR_GOTO( m_spSource->Start( spPD.Get(), NULL, &var ), done );
    }

    // blocking wait for Source/Stream started events
    fSeekCompleted = FALSE;
    while( SUCCEEDED( m_spSource->GetEvent( 0, spEvent.ReleaseAndGetAddressOf() ) ) )
    {
        MediaEventType met;
        HRESULT hrStatus;
        CHECKHR_GOTO( spEvent->GetStatus(&hrStatus), done );
        CHECKHR_GOTO( hrStatus, done );
        CHECKHR_GOTO( spEvent->GetType( &met ), done );
        switch ( met )
        {
            case MENewStream:
            case MEUpdatedStream:
                {
                    ComPtr<IMFMediaStream> spStream;
                    PROPVARIANT varEvent;

                    CHECKHR_GOTO( spEvent->GetValue( &varEvent ), done );
                    hr = varEvent.punkVal->QueryInterface( IID_IMFMediaStream, (void **)spStream.GetAddressOf());
                    PropVariantClear(&varEvent);
                    CHECKHR_GOTO( hr, done );
                    CHECKHR_GOTO( spStream->GetStreamDescriptor( spSD.GetAddressOf() ), done );
                    CHECKHR_GOTO( spSD->GetMediaTypeHandler( spMediatypeHandler.ReleaseAndGetAddressOf() ), done );
                    CHECKHR_GOTO( spMediatypeHandler->GetMajorType( &guidMajorType ), done );
                    streamType  = GetStreamTypeByMajorType( guidMajorType );
                    m_rgStream[streamType] = spStream;
                }
                break;
            case MESourceStarted:
            case MESourceSeeked:
                    fSourceStarted = TRUE;
                break;
        }

        if( fSourceStarted )
        {
            fSeekCompleted = TRUE;
            for( int i = 0; i < MAXSTREAM; i++ )
            {
                if( m_rgMediaType[i] && ( m_rgStream[i] == 0 ) )
                {
                    // stil have stream not started.
                    fSeekCompleted = FALSE;
                }
            }
            if( fSeekCompleted )
            {
                break;
            }
        }
    }

done:
    return hr;
}



