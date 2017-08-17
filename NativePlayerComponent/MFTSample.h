#pragma once
#include "Common.h"
#include "SampleAudioEvents.h"
using namespace Microsoft::WRL;


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum eTransformType
{
    DECRYPTERTRANSFORM = 0,
    DECODERTRANSFORM,
    RESAMPLERTRANSFORM,
    MAXTRANSFORM
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct CStreamInfo
{
    CStreamInfo()
        :m_EOP(FALSE)
        , m_numberOfFramesRendered(0)
    {
        for (int i = 0; i < MAXTRANSFORM; i++)
        {
            m_MFTDrainning[i] = FALSE;
        }
    }
    ComPtr<IMFTransform> m_spTransForms[MAXTRANSFORM]; // transform train for the giving stream
    BOOL m_MFTDrainning[MAXTRANSFORM]; // transform train for the giving stream
    ComPtr<IMFSample>    m_spPendingDeliverSample; // the pending output sample to be delivered to sink
    BOOL m_EOP; // has presented the last frame
    BOOL m_numberOfFramesRendered; // record how many frames has been rendered.

    void Reset()
    {
        for (int i = 0; i < MAXTRANSFORM; i++)
        {
            m_MFTDrainning[i] = FALSE;
            m_spTransForms[i].Reset();
            m_spPendingDeliverSample.Reset();
            m_EOP = FALSE;
            m_numberOfFramesRendered = 0;
        }
    }
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Note this is apparently missing uninitialization
class CMFTSample : public AudioClock
{
public:
    CMFTSample();
    void Initialize(LPCWSTR pInputStream);
    void ShutDown();
    ~CMFTSample();
    BOOL ProcessStream(eStreamType streamType);
    void Seek(LONGLONG hnsTime);
    virtual bool GetCorrelatedTime(LONGLONG* pllClockTime, LONGLONG* pllQpcInHns);
    LONGLONG GetCurrentTime();
    void StopSampleDeliver();
    BOOL IsEndOfStream();
	
	void VolumeIncrease();
	void VolumeDecrease();
	void Mute();
	void Unmute();

private:
    BOOL RequestSample(int iTransform, eStreamType streamType, IMFSample ** ppSample);
    BOOL DeliverSample(eStreamType streamType, IMFSample * pSample);
    BOOL RenderAudioSample(IMFSample * pSample);

    void InitializeTransforms();
    /*richfr
	HRESULT CheckVideoOutputPolicy(IMFInputTrustAuthority * pITA);
    void CreateDecrypterMFT(
        eStreamType streamType,
        IMFMediaType * pMediaType,
        IMFInputTrustAuthority * pITA,
        IMFTransform * pDecoderMFT,
        IMFTransform ** ppDecrytorTransform
    );
	*/
    HRESULT CreateMediaTransformByCLSID(GUID MFTClsID, IMFMediaType * pInputMediaType, IMFMediaType * pOutputMediaType, IMFTransform ** ppTransform);
    void InitializeAudioRender();
    //richfr HRESULT DoSecureStopServiceRequest();

    static DWORD WINAPI AudioRenderThreadProc(LPVOID p);
    static DWORD WINAPI VideoDeliverThreadProc(LPVOID p);
   //richfr  static DWORD WINAPI PlayreadyServiceRequestProc(void* pv);

private:
    ComPtr<IMFDXGIDeviceManager> m_spDXGIManager;

    CStreamInfo m_StreamInfos[MAXSTREAM];

    // members for video processor for video
    ComPtr<ID3D11VideoDevice> m_spVideoDevice;
    ComPtr<ID3D11VideoProcessorEnumerator> m_spD3D11VideoProcEnum;
    ComPtr<ID3D11VideoProcessor> m_spD3D11VideoProc;
    ComPtr<ID3D11VideoContext> m_spD3D11VideoContext;
    ComPtr<ID3D11Texture2D> m_spRenderTexture;

    // members for audio render
    ComPtr<IAudioClient2> m_spAudioClient;
    ComPtr<IAudioRenderClient> m_spAudioRenderClient;
    ComPtr<IAudioClock> m_spAudioClock;
    WAVEFORMATEX *m_pAudioClientWFX;
    DWORD m_dwResamplerOutputSize;
    ComPtr<IMFMediaType> m_spRenderAudioStreamType;

	//ComPtr<IAudioSessionControl> m_spAudioSessionControl;
	//ComPtr<IAudioSessionEvents> m_spAudioSessionEvents;
	//IAudioSessionControl *m_spAudioSessionControl;
	ComPtr<IAudioSessionControl> m_spAudioSessionControl;
	SampleAudioEvents *m_spSampleAudioEvents;
	//SampleAudioEvents *m_spSampleAudioEvents;
	ComPtr<ISimpleAudioVolume> m_spSimpleAudioVolume;

    // source reader to read from media file
    ComPtr<IMediaSource> m_spSource;

    // members used to calculate audio clock. 
    LONGLONG m_llStartTimeStamp;
    bool m_fIsAudioStarted;
    bool m_fIsVideoStarted;
    LARGE_INTEGER   m_liFrequency;

    // HDCP related attribute
    BOOL m_fAttemptToEnableHDCP;
    BOOL m_fHDCPFailureIsIgnorable;
    DWORD m_dwRestrictedVideoSize;

    static const DWORD c_HDCPCheckLatencyInMS = 2000; // check HDCP status every 2 seconds if enabled.
    ULONGLONG m_qwPreviousHDCPCheckTick;
    HANDLE m_hAudioRenderThread;
    HANDLE m_hVideoDeliverThread;

    CRITICAL_SECTION m_cs;
    SampleSink* m_pVideoSampleSink;

public:
    HANDLE m_hAudioStopEvent;
    HANDLE m_hVideoStopEvent;
};
