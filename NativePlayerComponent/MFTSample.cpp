#include "pch.h"
#include "MFTSample.h"
#include "MfMediaSource.h"
//richfr #include "securestopcert.h"
#include "DbgLog.h"
#include <windows.media.protection.playready.h>


using namespace Windows::Media::Protection::PlayReady;
using namespace Microsoft::WRL::Wrappers;
using namespace Windows::Foundation;

#define PR_LICENSE_URL L"http://playready.directtaps.net/svc/pr30/rightsmanager.asmx?PlayRight=1&UncompressedDigitalVideoOPL=280&UseSimpleNonPersistentLicense=1&SecureStop=1"

#include "initguid.h" 
DEFINE_GUID(MEDIASUBTYPE_RAW_AAC1, 0x000000FF, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(MF_H265_ENABLE_GPUSHADER, 0x7f176dbd, 0x801c, 0x49ee, 0xa4, 0x65, 0x1b, 0xe0, 0xa4, 0x0e, 0x13, 0xf4);
DEFINE_GUID(CLSID_CMpeg4DecMediaObject, 0xf371728a, 0x6052, 0x4d47, 0x82, 0x7c, 0xd0, 0x39, 0x33, 0x5d, 0xfe, 0x0a);
DEFINE_GUID(MEDIASUBTYPE_MP42, 0x3234504D, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
GUID GetCLSIDFromMediaType(IMFMediaType * pMediaType)
{
    GUID subType;
    DX::ThrowIfFailed(pMediaType->GetGUID(MF_MT_SUBTYPE, &subType));
    if (subType == MFVideoFormat_WMV1
        || subType == MFVideoFormat_WMV2
        || subType == MFVideoFormat_WMV3
        || subType == MFVideoFormat_WVC1)
    {
        return CLSID_WMVDecoderMFT;
    }
    else if (subType == MFVideoFormat_H264
        || subType == MFVideoFormat_H264_ES)
    {
        return CLSID_MSH264DecoderMFT;
    }
    else if (subType == MFVideoFormat_HEVC
        || subType == MFVideoFormat_HEVC_ES)
    {
        return CLSID_MSH265DecoderMFT;
    }
    else if (subType == MFAudioFormat_AAC
        || subType == MEDIASUBTYPE_RAW_AAC1)
    {
        return CLSID_MSAACDecMFT;
    }
    else if (subType == MFAudioFormat_Dolby_AC3
        || subType == MFAudioFormat_Dolby_DDPlus)
    {
        return CLSID_MSDDPlusDecMFT;
    }
    else if (subType == MFAudioFormat_WMAudioV8
        || subType == MFAudioFormat_WMAudioV9
        || subType == MFAudioFormat_WMAudio_Lossless)
    {
        // WMA decode is not valiable for apps now in durango, Add it later on
        return CLSID_WMADecMediaObject;
    }

    else if (subType == MFAudioFormat_MP3
        || subType == MFAudioFormat_MPEG)
    {
        return CLSID_MP3DecMediaObject;
    }
    else if (subType == MEDIASUBTYPE_MP42)
    {
        return CLSID_CMpeg4DecMediaObject;
    }
    else
    {
        return GUID_NULL;
    }
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------  
// Name: ActivationCompletionDelegate  
// Desc: Used with ActivateAudioInterfaceAsync() to activate IAudioClient  
//--------------------------------------------------------------------------------------  
class ActivationCompletionDelegate : public RuntimeClass < RuntimeClassFlags < ClassicCom >, FtmBase, IActivateAudioInterfaceCompletionHandler >
{
private:
    HANDLE                                 m_hCompletionEvent;
    IActivateAudioInterfaceAsyncOperation* m_spOperation;

public:
    ActivationCompletionDelegate(HANDLE hCompletedEvent) : m_hCompletionEvent(hCompletedEvent) {}
    ~ActivationCompletionDelegate() {}

    STDMETHODIMP ActivateCompleted(IActivateAudioInterfaceAsyncOperation* operation)
    {
        if (NULL == operation)
        {
            return E_POINTER;
        }

        m_spOperation = operation;
        m_spOperation->AddRef();

        SetEvent(m_hCompletionEvent);
        return S_OK;
    }

    STDMETHODIMP GetOperationInterface(IActivateAudioInterfaceAsyncOperation** ppOperation)
    {
        if (NULL == ppOperation)
        {
            return E_POINTER;
        }

        if (m_spOperation == NULL)
        {
            return E_FAIL;
        }
        else
        {
            *ppOperation = m_spOperation;
        }

        return S_OK;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CMFTSample Class Object implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CMFTSample::CMFTSample()
    : m_llStartTimeStamp(0)
    , m_fAttemptToEnableHDCP(FALSE)
    , m_fHDCPFailureIsIgnorable(FALSE)
    , m_dwRestrictedVideoSize((UINT32)-1)
    , m_qwPreviousHDCPCheckTick(0)
    , m_hAudioStopEvent(NULL)
    , m_hVideoStopEvent(NULL)
    , m_hAudioRenderThread(NULL)
    , m_hVideoDeliverThread(NULL)
    , m_dwResamplerOutputSize(0)
    , m_fIsAudioStarted(false)
    , m_fIsVideoStarted(false)
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CMFTSample::~CMFTSample()
{

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMFTSample::InitializeTransforms()
{
    for (int iStream = 0; iStream < (int)MAXSTREAM; iStream++)
    {
        ComPtr<IMFMediaType>      spSourceMediaType;
        ComPtr<IMFInputTrustAuthority>      spITA;
        DWORD dwFlags;
        eStreamType streamType = (eStreamType)iStream;
        CStreamInfo * pStream = &m_StreamInfos[streamType];

        if (pStream->m_EOP)  continue;

        m_spSource->GetMediaType(streamType, spSourceMediaType.ReleaseAndGetAddressOf(), spITA.ReleaseAndGetAddressOf());
        if (nullptr == spSourceMediaType)
        {
            pStream->m_EOP = TRUE;
            continue;
        }

        GUID decoderClsID = GetCLSIDFromMediaType(spSourceMediaType.Get());

        if (decoderClsID == GUID_NULL)
        {
            pStream->m_EOP = TRUE;
            continue;
        }

        // Create decoder MFT
        DX::ThrowIfFailed(CreateMediaTransformByCLSID(decoderClsID,
            spSourceMediaType.Get(),
            NULL,
            pStream->m_spTransForms[DECODERTRANSFORM].ReleaseAndGetAddressOf()));

        //sample currently only supports audio stream
		if (streamType == AUDIOSTREAM)
        {
            ComPtr<IMFMediaType>      spDecoderOutputMediaType;

            // find matched output type for resampler. 
            for (int i = 0;
                SUCCEEDED(pStream->m_spTransForms[DECODERTRANSFORM]->GetOutputAvailableType(0, i, spDecoderOutputMediaType.ReleaseAndGetAddressOf()));
                i++)
            {
                // find first available decoder output media type what resampler can accept. 
                if (S_OK != spDecoderOutputMediaType->IsEqual(m_spRenderAudioStreamType.Get(), &dwFlags))
                {
                    // the decoder output does not match the audio device output, create resampler.
                    HRESULT hrResampler = CreateMediaTransformByCLSID(
                        CLSID_AudioResamplerMediaObject,
                        spDecoderOutputMediaType.Get(),
                        m_spRenderAudioStreamType.Get(),
                        pStream->m_spTransForms[RESAMPLERTRANSFORM].ReleaseAndGetAddressOf());
                    if (SUCCEEDED(hrResampler))
                    {
                        DX::ThrowIfFailed(pStream->m_spTransForms[DECODERTRANSFORM]->SetOutputType(0, spDecoderOutputMediaType.Get(), 0));
                        break;
                    }
                }
            }
        }
        // set message to MFT to start streaming. 
        for (int iMFT = 0; iMFT < MAXTRANSFORM; iMFT++)
        {
            if (pStream->m_spTransForms[iMFT])
            {
                DX::ThrowIfFailed(pStream->m_spTransForms[iMFT]->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0));
                DX::ThrowIfFailed(pStream->m_spTransForms[iMFT]->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL));
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CMFTSample::CreateMediaTransformByCLSID(GUID MFTClsID, IMFMediaType * pInputMediaType, IMFMediaType * pOutputMediaType, IMFTransform ** ppTransform)
{
    HRESULT hr = S_OK;
    ComPtr<IMFTransform>      spMFT;

    MULTI_QI mqi;
    mqi.hr = 0;
    mqi.pItf = NULL;
    mqi.pIID = &IID_IMFTransform;

    hr = ::CoCreateInstanceFromApp(MFTClsID, NULL, CLSCTX_INPROC_SERVER, NULL, 1, &mqi);
    DX::ThrowIfFailed(hr);

    spMFT = static_cast<IMFTransform*>(mqi.pItf);

    hr = spMFT->SetInputType(0, pInputMediaType, 0);
    *ppTransform = NULL;
    if (SUCCEEDED(hr))
    {
        if (pOutputMediaType != NULL)
        {
            // NOTES: always setup video decoder MFT output type after MFT_MESSAGE_SET_D3D_MANAGER message posted, otherwise 
            // decoder may force a internal switch from software mode to DXVA HW device.

            hr = spMFT->SetOutputType(0, pOutputMediaType, 0);
        }
        if (SUCCEEDED(hr))
        {
            *ppTransform = *spMFT.GetAddressOf();
        }
    }

    return hr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMFTSample::InitializeAudioRender()
{
    // maximum audio buffer( in duration HNS) in audio device. 
    REFERENCE_TIME hnsRequestedDuration = 10000000ll / 2;
    ComPtr<IAudioClient2> audioClient;
    ComPtr<IUnknown> unknownInterface;
    ComPtr<IActivateAudioInterfaceAsyncOperation> operation;
    ComPtr<ActivationCompletionDelegate> completionObject;
    ComPtr<IActivateAudioInterfaceCompletionHandler> completionHandler;
    ComPtr<IMFMediaType> spAudioRenderMediaType;

    // Query the defauilt render device ID      
    //Platform::String^ id = Windows::Media::Devices::MediaDevice::GetDefaultAudioRenderId(Windows::Media::Devices::AudioDeviceRole::Default);
	//Instead of the above where a specific device is being specified, use the default so that Stream Switching will work
	PWSTR audioRenderGuidString;
	StringFromIID(DEVINTERFACE_AUDIO_RENDER, &audioRenderGuidString);
	
    // Create the completion event for ActivateAudioInterfaceAsync()       
    HANDLE completionEvent = CreateEventEx(NULL, L"", NULL, EVENT_ALL_ACCESS);
    DX::ThrowIfFailed((completionEvent == NULL) ? HRESULT_FROM_WIN32(GetLastError()) : S_OK);
    completionObject = Make<ActivationCompletionDelegate>(completionEvent);
    DX::ThrowIfFailed(completionObject.As(&completionHandler));

    // Activate the default audio interface      
    //DX::ThrowIfFailed(ActivateAudioInterfaceAsync(id->Data(), __uuidof(IAudioClient), NULL, completionHandler.Get(), operation.GetAddressOf()));
	DX::ThrowIfFailed(ActivateAudioInterfaceAsync(audioRenderGuidString, __uuidof(IAudioClient2), NULL, completionHandler.Get(), operation.GetAddressOf()));

    // Wait for the async operation to complete          
    WaitForSingleObjectEx(completionEvent, INFINITE, FALSE);

    DX::ThrowIfFailed(completionObject->GetOperationInterface(operation.ReleaseAndGetAddressOf()));

    // Verify that the interface was activated      
    HRESULT hrActivated = S_OK;
    DX::ThrowIfFailed(operation->GetActivateResult(&hrActivated, unknownInterface.GetAddressOf()));
    DX::ThrowIfFailed(hrActivated);

    // Return the IAudioClient interface      
    DX::ThrowIfFailed(unknownInterface.As(&audioClient));
    m_spAudioClient = audioClient.Get();

	// Set the render client options 
	AudioClientProperties renderProperties = {
		sizeof(AudioClientProperties),
		false, // Request hardware offload 
		AudioCategory_Media,
		AUDCLNT_STREAMOPTIONS_NONE // Request raw stream if supported 
	};
	HRESULT hr = m_spAudioClient->SetClientProperties(&renderProperties);

    // Cleanup      
    CloseHandle(completionEvent);

    DX::ThrowIfFailed(m_spAudioClient->GetMixFormat(&m_pAudioClientWFX));

    // limit the resampler output size to 10ms ( by default, resample only output 1 frame per sample )
    m_dwResamplerOutputSize = m_pAudioClientWFX->nAvgBytesPerSec / 100;
    m_dwResamplerOutputSize = (m_dwResamplerOutputSize + m_pAudioClientWFX->nBlockAlign - 1) / m_pAudioClientWFX->nBlockAlign  * m_pAudioClientWFX->nBlockAlign;

    DX::ThrowIfFailed(m_spAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,
        hnsRequestedDuration,
        0,
        m_pAudioClientWFX,
        NULL));

    DX::ThrowIfFailed(m_spAudioClient->GetService(
        _uuidof(IAudioRenderClient),
        (void**)m_spAudioRenderClient.ReleaseAndGetAddressOf()));


	// Audio handling
	//DX::ThrowIfFailed(m_spAudioClient->GetService(
	//	_uuidof(IAudioSessionControl),
	//	(void**)m_spAudioSessionControl));

	m_spAudioClient->GetService(
		__uuidof (IAudioSessionControl), 
		(void**)m_spAudioSessionControl.ReleaseAndGetAddressOf());
	if (m_spAudioSessionControl != nullptr)
	{
		m_spSampleAudioEvents = new SampleAudioEvents();
		m_spAudioSessionControl->RegisterAudioSessionNotification(m_spSampleAudioEvents);
	}

	DX::ThrowIfFailed(m_spAudioClient->GetService(
		_uuidof(ISimpleAudioVolume),
		(void**)m_spSimpleAudioVolume.ReleaseAndGetAddressOf()));

    // Create audio media type from output waveformatex.
    DX::ThrowIfFailed(MFCreateMediaType(spAudioRenderMediaType.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(spAudioRenderMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
    DX::ThrowIfFailed(MFInitMediaTypeFromWaveFormatEx(spAudioRenderMediaType.Get(), m_pAudioClientWFX, sizeof(*m_pAudioClientWFX) + m_pAudioClientWFX->cbSize));
    DX::ThrowIfFailed(spAudioRenderMediaType.As(&m_spRenderAudioStreamType));
    m_hAudioStopEvent = CreateEventEx(NULL, NULL, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMFTSample::Initialize(LPCWSTR pInputStream)
{
    ComPtr<IMFMediaType> spMediaType;
    ComPtr<IMFMediaType> spAudioMediaType;

    QueryPerformanceFrequency(&m_liFrequency);

    //richfr m_pVideoSampleSink = pVideoSampleSink;
    // Use the hardware decoder
    DX::ThrowIfFailed(MFStartup(MF_VERSION));

    InitializeCriticalSectionEx(&m_cs, 0, 0);

	//const wchar_t *newInput = L"http://www.hochmuth.com/mp3/Beethoven_12_Variation.mp3";
    // create a media source to get mediatype and read sample from. in the sample I am using implemented 
    // the interface using MF media source, app can provide their own IMediaSource interface if needed.

    DX::ThrowIfFailed(CMFMediaSource::CreateInstance(pInputStream, &m_spSource));

    // initialize audio render
    if (SUCCEEDED(m_spSource->GetMediaType(AUDIOSTREAM, spMediaType.ReleaseAndGetAddressOf(), NULL)) && spMediaType)
    {
        InitializeAudioRender();
    }
	m_StreamInfos[VIDEOSTREAM].m_EOP = TRUE;//sample does not currently support video

    // intialize MFTs ( decrytor / decoder / resamplers )
    InitializeTransforms();

    if (!m_StreamInfos[AUDIOSTREAM].m_EOP)
    {
        // Only initialize the audio clock if a valid decoder is present
        m_spAudioClient->GetService(
            _uuidof(IAudioClock),
            (void**)m_spAudioClock.ReleaseAndGetAddressOf());

        m_hAudioRenderThread = CreateThread(NULL, 0, AudioRenderThreadProc, this, 0, NULL);
    }

    if (!m_StreamInfos[VIDEOSTREAM].m_EOP)
    {
        m_hVideoDeliverThread = CreateThread(NULL, 0, VideoDeliverThreadProc, this, 0, NULL);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMFTSample::StopSampleDeliver()
{
    if (m_hAudioStopEvent)
    {
        SetEvent(m_hAudioStopEvent);
    }
    if (m_hVideoStopEvent)
    {
        SetEvent(m_hVideoStopEvent);
    }

    if (m_hAudioRenderThread)
    {
        WaitForSingleObjectEx(m_hAudioRenderThread, INFINITE, FALSE);
        CloseHandle(m_hAudioRenderThread);
        m_hAudioRenderThread = NULL;
    }
    if (m_hVideoDeliverThread)
    {
        WaitForSingleObjectEx(m_hVideoDeliverThread, INFINITE, FALSE);
        CloseHandle(m_hVideoDeliverThread);
        m_hVideoDeliverThread = NULL;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CMFTSample::IsEndOfStream()
{
    return m_StreamInfos[VIDEOSTREAM].m_EOP && m_StreamInfos[AUDIOSTREAM].m_EOP;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMFTSample::ShutDown()
{
    StopSampleDeliver();

    if (m_pAudioClientWFX)
    {
        CoTaskMemFree(m_pAudioClientWFX);
        m_pAudioClientWFX = NULL;
    }
    if (m_spAudioClient)
    {
        m_spAudioClient->Stop();
        m_spAudioClient.Reset();
    }
    if (m_spAudioRenderClient)
    {
        m_spAudioRenderClient.Reset();
    }
    if (m_spRenderAudioStreamType)
    {
        m_spRenderAudioStreamType.Reset();
    }
    if (m_spAudioClock)
    {
        m_spAudioClock.Reset();
    }
    if (m_spSource)
    {
        m_spSource->Close();
        m_spSource.Reset();
    }

    for (int iStream = 0; iStream < (int)MAXSTREAM; iStream++)
    {
        m_StreamInfos[iStream].Reset();
    }

    m_pVideoSampleSink = nullptr;
    m_llStartTimeStamp = 0;
    m_fAttemptToEnableHDCP = FALSE;
    m_fHDCPFailureIsIgnorable = FALSE;
    m_dwRestrictedVideoSize = (UINT32)-1;
    m_qwPreviousHDCPCheckTick = 0;
    m_dwResamplerOutputSize = 0;
    m_fIsAudioStarted = false;
    m_fIsVideoStarted = false;

    DeleteCriticalSection(&m_cs);
    MFShutdown();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CMFTSample::RenderAudioSample(IMFSample * pSample)
{
    UINT32 numFramesPadding = 0;
    UINT32 numFramesAvailable = 0;
    BYTE * pOutputData;
    BYTE * pDecompressedData;

    ComPtr<IMFMediaBuffer> spAudioOutputBuf;
    DWORD dwBufferLen;
    DWORD dwMaxBufferLen;
    DWORD dwFrameCount;
    DWORD cbFrameSize = m_pAudioClientWFX->wBitsPerSample * m_pAudioClientWFX->nChannels / 8;
    UINT32 uiDeviceTotalFrameSize = 0;

    LONGLONG llTimeStamp = 0;
    pSample->GetSampleTime(&llTimeStamp);
    DbgLog(__FUNCTION__ " llTimeStamp %I64d\r\n", llTimeStamp);

    AutoLock lock(&m_cs);

    if (m_StreamInfos[AUDIOSTREAM].m_numberOfFramesRendered == 0)
    {
        m_llStartTimeStamp = llTimeStamp;
    }

    // find how many frames that the sample hold
    DX::ThrowIfFailed(pSample->ConvertToContiguousBuffer(spAudioOutputBuf.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(spAudioOutputBuf->Lock(&pDecompressedData, &dwMaxBufferLen, &dwBufferLen));
    DX::ThrowIfFailed(m_spAudioClient->GetCurrentPadding(&numFramesPadding));

    if (m_StreamInfos[AUDIOSTREAM].m_numberOfFramesRendered == 0 && numFramesPadding != 0)
    {
        DX::ThrowIfFailed(E_UNEXPECTED);
    }
	//get number of samples passed in from buffer
    dwFrameCount = dwBufferLen / cbFrameSize;

    assert(dwFrameCount > 0);

    // Get the actual size of the allocated buffer.
    DX::ThrowIfFailed(m_spAudioClient->GetBufferSize(&uiDeviceTotalFrameSize));

    // check if the audio device has enough buffer left to put all the frames.
    numFramesAvailable = uiDeviceTotalFrameSize - numFramesPadding;

    if (numFramesAvailable < dwFrameCount)
    {
        // don't have enough buffer in the device, return false and sample will be keep as pending sample.
        if (!m_fIsAudioStarted)
        {
            m_spAudioClient->Start();
            m_fIsAudioStarted = true;
        }
        spAudioOutputBuf->Unlock();

        return FALSE;
    }

    //if( dwFrameCount > 0 )
    {
        // now put all frames into the audio device buffer
        DX::ThrowIfFailed(m_spAudioRenderClient->GetBuffer(dwFrameCount, &pOutputData));
        memcpy(pOutputData, pDecompressedData, dwBufferLen);

        DX::ThrowIfFailed(m_spAudioRenderClient->ReleaseBuffer(dwFrameCount, 0));
    }

    spAudioOutputBuf->Unlock();

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD WINAPI CMFTSample::AudioRenderThreadProc(LPVOID p)
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    CMFTSample * pThis = (CMFTSample *)p;
    while (WaitForSingleObjectEx(pThis->m_hAudioStopEvent, 1, FALSE) == WAIT_TIMEOUT)
    {
        if ((pThis->m_fIsVideoStarted || pThis->m_StreamInfos[VIDEOSTREAM].m_EOP) && TRUE == pThis->ProcessStream(AUDIOSTREAM))
        {
            // reached EOS for the stream.
            break;
        }
    }

    CoUninitialize();
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD WINAPI CMFTSample::VideoDeliverThreadProc(LPVOID p)
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    CMFTSample * pThis = (CMFTSample *)p;
    while (WaitForSingleObjectEx(pThis->m_hVideoStopEvent, 1, FALSE) == WAIT_TIMEOUT)
    {
        if (TRUE == pThis->ProcessStream(VIDEOSTREAM))
        {
            // reached EOS for the stream.
            break;
        }
    }

    CoUninitialize();
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CMFTSample::RequestSample(int iMFT, eStreamType streamType, IMFSample ** ppSample)
{
    HRESULT hr = S_OK;
    BOOL bRet = FALSE;

    if (iMFT < 0)
    {
        //iMFT < 0 means it is asking sample from media source instead of MFT. 
        // Try to get the sample from media source
        hr = m_spSource->ReadSample(streamType, ppSample);
        if (hr == MF_E_END_OF_STREAM)
        {
            // input stream has completed, return TRUE(EOS) to drain the next MFT.
            return TRUE;
        }
        else
        {
            DX::ThrowIfFailed(hr);
            assert(*ppSample);
            return FALSE;
        }
    }
    else
    {
        ComPtr<IMFTransform> spMFT = m_StreamInfos[streamType].m_spTransForms[iMFT];
        HRESULT hrProcessOutput = S_OK;

        if (spMFT == NULL)
        {
            // current MFT is not valid, using upstream one.
            return RequestSample(iMFT - 1, streamType, ppSample);
        }

        do
        {
            MFT_OUTPUT_STREAM_INFO OutputStreamInfo;
            MFT_OUTPUT_DATA_BUFFER mftOutput = { 0 };
            DWORD dwStatus;
            mftOutput.dwStreamID = 0;


            DX::ThrowIfFailed(spMFT->GetOutputStreamInfo(0, &OutputStreamInfo));
            if (!(OutputStreamInfo.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)))
            {
                // decoder does not allocate sample, caller has to allocate sample here.
                ComPtr<IMFMediaBuffer> spOutputBuffer;
                ComPtr<IMFSample> spOutputSample;
                if (iMFT == RESAMPLERTRANSFORM && OutputStreamInfo.cbSize < m_dwResamplerOutputSize)
                {
                    OutputStreamInfo.cbSize = m_dwResamplerOutputSize;
                }

                if (OutputStreamInfo.cbAlignment > 0)
                {
                    DX::ThrowIfFailed(MFCreateAlignedMemoryBuffer(OutputStreamInfo.cbSize, OutputStreamInfo.cbAlignment, spOutputBuffer.GetAddressOf()));
                }
                else
                {
                    DX::ThrowIfFailed(MFCreateMemoryBuffer(OutputStreamInfo.cbSize, spOutputBuffer.GetAddressOf()));
                }
                DX::ThrowIfFailed(MFCreateSample(spOutputSample.GetAddressOf()));
                DX::ThrowIfFailed(spOutputSample->AddBuffer(spOutputBuffer.Get()));
                mftOutput.pSample = spOutputSample.Detach();
            }

            hrProcessOutput = spMFT->ProcessOutput(0, 1, &mftOutput, &dwStatus);
            if (SUCCEEDED(hrProcessOutput))
            {
                // mftOutput.pSample has already hold a reference, don't need to addref here.
                *ppSample = mftOutput.pSample;
                mftOutput.pSample = NULL;
            }
            else
            {
                // release caller samples
                if (mftOutput.pEvents)
                {
                    mftOutput.pEvents->Release();
                    mftOutput.pEvents = NULL;
                }
                if (mftOutput.pSample)
                {
                    mftOutput.pSample->Release();
                    mftOutput.pSample = NULL;
                }

                if (hrProcessOutput == MF_E_TRANSFORM_NEED_MORE_INPUT)
                {
                    // Need a input
                    // check if upstream MFT has any output or not. 
                    ComPtr<IMFSample> spInputSample;
                    if (m_StreamInfos[streamType].m_MFTDrainning[iMFT])
                    {
                        // we are in draining, but decoder still can not product output, it must hit end of stream then. 
                        return TRUE;
                    }

                    // yeah, recurisive in general is not pretty, but it making code much simpler here and we only have very limited (2 or 3 ) mfts.
                    BOOL fEOS = RequestSample(iMFT - 1, streamType, spInputSample.GetAddressOf());
                    if (fEOS)
                    {
                        // upstream has hit EOS, drain current MFT
                        DX::ThrowIfFailed(spMFT->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, NULL));
                        DX::ThrowIfFailed(spMFT->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, NULL));
                        m_StreamInfos[streamType].m_MFTDrainning[iMFT] = TRUE;
                    }
                    else
                    {
                        DX::ThrowIfFailed(spMFT->ProcessInput(0, spInputSample.Get(), 0));
                    }
                }
                else if (hrProcessOutput == MF_E_TRANSFORM_STREAM_CHANGE)
                {
                    ComPtr<IMFMediaType> spMediaType;

                    // other then just set current MFT, also need to set all input/output type for down streams such as resampler. 
                    for (int iDownStreamMFT = iMFT; iDownStreamMFT < (int)MAXTRANSFORM; iDownStreamMFT++)
                    {
                        ComPtr<IMFTransform> spDownMFT = m_StreamInfos[streamType].m_spTransForms[iDownStreamMFT];
                        if (spDownMFT == NULL) continue;

                        if (spMediaType != NULL && iDownStreamMFT != RESAMPLERTRANSFORM)
                        {
                            // the current MFT ( iMFT ) does not need to set input type.
                            DX::ThrowIfFailed(spDownMFT->SetInputType(0, spMediaType.Get(), 0));
                        }

                        if (streamType == AUDIOSTREAM
                            && iDownStreamMFT == DECODERTRANSFORM
                            &&  m_StreamInfos[streamType].m_spTransForms[RESAMPLERTRANSFORM])
                        {
                            // find a decoder output type that resampler MFT can accept.
                            ComPtr<IMFMediaType> spDecoderOutputType;
                            for (int iOutputType = 0;
                                SUCCEEDED(m_StreamInfos[streamType].m_spTransForms[DECODERTRANSFORM]->GetOutputAvailableType(0, iOutputType, spDecoderOutputType.ReleaseAndGetAddressOf()));
                                iOutputType++)
                            {
                                if (SUCCEEDED(m_StreamInfos[streamType].m_spTransForms[RESAMPLERTRANSFORM]->SetInputType(0, spDecoderOutputType.Get(), 0))
                                    && SUCCEEDED(m_StreamInfos[streamType].m_spTransForms[RESAMPLERTRANSFORM]->SetOutputType(0, m_spRenderAudioStreamType.Get(), 0)))
                                {
                                    spMediaType = spDecoderOutputType;
                                    DX::ThrowIfFailed(spDownMFT->SetOutputType(0, spMediaType.Get(), 0));
                                    break;
                                }
                            }

                            assert(spMediaType.Get() != nullptr);
                        }
                        else if (iDownStreamMFT != RESAMPLERTRANSFORM)
                        {
                            // don't reset resampler output type , it always match audio client media type
                            DX::ThrowIfFailed(spDownMFT->GetOutputAvailableType(0, 0, spMediaType.ReleaseAndGetAddressOf()));
							DX::ThrowIfFailed(spDownMFT->SetOutputType(0, spMediaType.Get(), 0));
                        }
                    }
                }
                else
                {
                    // Do not throw a failure if we have reached EOS for the given stream.
                    if ((TRUE == m_StreamInfos[streamType].m_MFTDrainning[iMFT]) && (NULL == mftOutput.pSample))
                    {
                        bRet = TRUE;
                        break;
                    }

                    DX::ThrowIfFailed(hrProcessOutput);
                }
            }
        } while (FAILED(hrProcessOutput)); // loop until we got a sample

    }

    if (FALSE == bRet)
    {
        // ASSERT is valid only if we haven't hit EOS
        assert(*ppSample);
    }

    return bRet;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// return TRUE if sample expired and pipeline should grab the next sample. 
BOOL CMFTSample::DeliverSample(eStreamType streamType, IMFSample * pSample)
{
	if (streamType == AUDIOSTREAM)
	{
		return RenderAudioSample(pSample);
	}
	else
	{
		return false;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL CMFTSample::ProcessStream(eStreamType streamType)
{
    if (m_StreamInfos[streamType].m_EOP)
    {
        return (TRUE);
    }

    if (m_StreamInfos[streamType].m_spPendingDeliverSample == NULL)
    {
        m_StreamInfos[streamType].m_EOP
            = RequestSample(MAXTRANSFORM - 1, streamType, m_StreamInfos[streamType].m_spPendingDeliverSample.ReleaseAndGetAddressOf());
    }

    // loop under audio device buffer is full or video frame not expired or EOS
    while (!m_StreamInfos[streamType].m_EOP
        && DeliverSample(streamType, m_StreamInfos[streamType].m_spPendingDeliverSample.Get()))
    {
        m_StreamInfos[streamType].m_numberOfFramesRendered++;
        m_StreamInfos[streamType].m_EOP
            = RequestSample(MAXTRANSFORM - 1, streamType, m_StreamInfos[streamType].m_spPendingDeliverSample.ReleaseAndGetAddressOf());
    }

    if (streamType == VIDEOSTREAM)
    {
        m_fIsVideoStarted = true;
    }
    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMFTSample::GetCorrelatedTime(LONGLONG* pllClockTime, LONGLONG* pllQpcInHns)
{
    AutoLock lock(&m_cs);

    // Switch to use video clock if audio ends early than video.
    if (m_spAudioClock && !m_StreamInfos[AUDIOSTREAM].m_EOP)
    {
        UINT64 curPos = 0;
        UINT64 qpc = 0;
        UINT64 freq = 0;

        m_spAudioClock->GetFrequency(&freq);
        m_spAudioClock->GetPosition(&curPos, &qpc);

        static LARGE_INTEGER liFreq = {};
        if (0 == liFreq.QuadPart)
        {
            QueryPerformanceFrequency(&liFreq);
        }

        *pllClockTime = m_llStartTimeStamp + curPos * 10000000ll / freq;
        *pllQpcInHns = qpc;

        return true;
    }
    else
    {
        // No audio clock
        return false;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
LONGLONG CMFTSample::GetCurrentTime()
{
    LONGLONG llClockTime = -1;
    LONGLONG llQPCInHns = -1;

    if (GetCorrelatedTime(&llClockTime, &llQPCInHns))
    {
        return llClockTime + GetCurrentTimeInHNS() - llQPCInHns;
    }
    else
    {
        return m_pVideoSampleSink->GetVideoTime();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMFTSample::Seek(LONGLONG hnsTime)
{
    // exit audio render thread first.
    if (m_hAudioStopEvent)
    {
        SetEvent(m_hAudioStopEvent);
    }
    if (m_hAudioRenderThread)
    {
        WaitForSingleObjectEx(m_hAudioRenderThread, INFINITE, FALSE);
        CloseHandle(m_hAudioRenderThread);
        m_hAudioRenderThread = NULL;
    }

    if (m_hVideoStopEvent)
    {
        SetEvent(m_hVideoStopEvent);
    }
    if (m_hVideoDeliverThread)
    {
        WaitForSingleObjectEx(m_hVideoDeliverThread, INFINITE, FALSE);
        CloseHandle(m_hVideoDeliverThread);
        m_hVideoDeliverThread = NULL;
    }

    //m_pVideoSampleSink->Flush();

    // flush all MFTs after seek.
    for (int iStream = 0; iStream < MAXSTREAM; iStream++)
    {
        for (int iMFT = 0; iMFT < MAXTRANSFORM; iMFT++)
        {
            m_StreamInfos[iStream].m_MFTDrainning[iMFT] = FALSE;

            if (m_StreamInfos[iStream].m_spTransForms[iMFT])
            {
                DX::ThrowIfFailed(m_StreamInfos[iStream].m_spTransForms[iMFT]->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0));
            }
        }

        m_StreamInfos[iStream].m_spPendingDeliverSample.Reset();
        m_StreamInfos[iStream].m_numberOfFramesRendered = 0;
    }

    // reset clock. 
    if (m_spAudioClient.Get())
    {
        DX::ThrowIfFailed(m_spAudioClient->Stop());
        DX::ThrowIfFailed(m_spAudioClient->Reset());
    }
    {
        HRESULT hr = m_spSource->Seek(hnsTime);
        // It will fail if over the end of stream point.
        if (FAILED(hr))
        {
            DbgPrint("failed to seek: %08x, pos=%.2f\r\n", hr, (float)hnsTime / 10000000.0f);
        }
    }

    m_llStartTimeStamp = 0;
    m_fIsAudioStarted = false;
    m_fIsVideoStarted = false;

    m_StreamInfos[AUDIOSTREAM].m_EOP = (m_StreamInfos[AUDIOSTREAM].m_spTransForms[DECODERTRANSFORM].Get() == nullptr);
    m_StreamInfos[VIDEOSTREAM].m_EOP = (m_StreamInfos[VIDEOSTREAM].m_spTransForms[DECODERTRANSFORM].Get() == nullptr);
    if (!m_StreamInfos[AUDIOSTREAM].m_EOP)
    {
        ResetEvent(m_hAudioStopEvent);
        m_hAudioRenderThread = CreateThread(NULL, 0, AudioRenderThreadProc, this, 0, NULL);
    }
    if (!m_StreamInfos[VIDEOSTREAM].m_EOP)
    {
        ResetEvent(m_hVideoStopEvent);
        m_hVideoDeliverThread = CreateThread(NULL, 0, VideoDeliverThreadProc, this, 0, NULL);
    }
}

void CMFTSample::VolumeIncrease()
{
	auto res = m_spSimpleAudioVolume->SetMasterVolume(static_cast<float>(0.8f), NULL);
	return;
}

void CMFTSample::VolumeDecrease()
{
	auto res = m_spSimpleAudioVolume->SetMasterVolume(static_cast<float>(0.2f), NULL);
	return;
}

void CMFTSample::Mute()
{
	auto res = m_spSimpleAudioVolume->SetMute(true, NULL);
	return;
}

void CMFTSample::Unmute()
{
	auto res = m_spSimpleAudioVolume->SetMute(false, NULL);
	return;
}

