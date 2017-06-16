// implementation of IMediaSource using MF native source

#include "imediasource.h"
#include <queue>
using namespace Microsoft::WRL;

enum ESType
{
    ES_NONE = 0,
    ES_H264,
    ES_HEVC
};

#ifndef METHODASYNCCALLBACK

#define METHODASYNCCALLBACKEX(Callback, Parent, Flag, Queue) \
class Callback##AsyncCallback; \
friend class Callback##AsyncCallback; \
class Callback##AsyncCallback : public IMFAsyncCallback \
{ \
public: \
    STDMETHOD_( ULONG, AddRef )() \
    { \
        Parent * pThis = ((Parent*)((BYTE*)this - offsetof(Parent, m_x##Callback))); \
        return pThis->AddRef(); \
    } \
    STDMETHOD_( ULONG, Release )() \
    { \
        Parent * pThis = ((Parent*)((BYTE*)this - offsetof(Parent, m_x##Callback))); \
        return pThis->Release(); \
    } \
    STDMETHOD( QueryInterface )( REFIID riid, void **ppvObject ) \
    { \
        if(riid == __uuidof(IMFAsyncCallback) || riid == __uuidof(IUnknown)) \
        { \
            (*ppvObject) = this; \
            AddRef(); \
            return S_OK; \
        } \
        (*ppvObject) = NULL; \
        return E_NOINTERFACE; \
    } \
    STDMETHOD( GetParameters )( \
        DWORD *pdwFlags, \
        DWORD *pdwQueue) \
    { \
        *pdwFlags = Flag; \
        *pdwQueue = Queue; \
        return S_OK; \
    } \
    STDMETHOD( Invoke )( IMFAsyncResult * pResult ) \
    { \
        Parent * pThis = ((Parent*)((BYTE*)this - offsetof(Parent, m_x##Callback))); \
        pThis->Callback( pResult ); \
        return S_OK; \
    } \
} m_x##Callback;

#define METHODASYNCCALLBACK(Callback, Parent) \
    METHODASYNCCALLBACKEX(Callback, Parent, 0, 0)

#endif //METHODASYNCCALLBACK

using namespace std;

class CMFMediaSource : public IMediaSource
{
public:
    // IUnknown
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();
    STDMETHODIMP QueryInterface( REFIID iid, void** ppv);

    // IMediaSource
    STDMETHODIMP ReadSample( eStreamType streamType,  IMFSample **ppSample );  
    STDMETHODIMP GetMediaType( eStreamType streamType,  IMFMediaType **ppMediaType, IMFInputTrustAuthority **ppITA );  
    STDMETHODIMP Seek( LONGLONG hnsTime );  
    STDMETHODIMP_(void) Close();  

    static HRESULT CreateInstance( LPCWSTR url, IMediaSource ** pMediaSource );

protected:
    CMFMediaSource();
    ~CMFMediaSource();
    HRESULT Open( LPCWSTR url );

private: // callbacks from MF
    HRESULT WaitForCallbackComplete( IMFAsyncResult ** ppResult, DWORD msTimeOut );

    void OnOpenCompleted(  IMFAsyncResult * pResult );
    METHODASYNCCALLBACK( OnOpenCompleted, CMFMediaSource );

    void OnVideoStreamEvent( IMFAsyncResult * pResult );
    METHODASYNCCALLBACK( OnVideoStreamEvent, CMFMediaSource );

    void OnAudioStreamEvent( IMFAsyncResult * pResult );
    METHODASYNCCALLBACK( OnAudioStreamEvent, CMFMediaSource );

    eStreamType GetStreamTypeByMajorType( GUID & guidMajorType );
    //HRESULT DemoCreateITAFromDRMHeader( DWORD dwStreamID, void * pDrmHeaderBuffer, DWORD cbDrmHeaderSize, IMFInputTrustAuthority ** ppITA );
    HRESULT DemoCreateMediaType( IMFMediaType * pNativeMT,  IMFMediaType ** ppMT );
    HRESULT DemoCreateMediaSample( IMFSample * pNativeSample,  eStreamType streamType, IMFSample ** ppOutputSample );
    

protected:
    long  m_nRefCount;            // reference count
    ComPtr<IMFMediaSource> m_spSource;

    HANDLE m_hSourceSyncEvent;

    ComPtr<IMFAsyncResult> m_spVideoEventResult;
    HANDLE m_hVideoSyncEvent;

    ComPtr<IMFAsyncResult> m_spAudioEventResult;
    HANDLE m_hAudioSyncEvent;

    ComPtr<IMFAsyncResult> m_spAsyncResult;
    ComPtr<IMFMediaStream> m_rgStream[MAXSTREAM]; 
    ComPtr<IMFMediaType> m_rgMediaType[MAXSTREAM]; 
    ComPtr<IMFInputTrustAuthority> m_rgITA[MAXSTREAM]; 
    ComPtr<IMFTrustedInput> m_spTrustedInput;

    queue<ComPtr<IMFSample>> m_spPendingVideoSampleQueue;
};
