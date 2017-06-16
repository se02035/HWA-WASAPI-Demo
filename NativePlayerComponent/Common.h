#pragma once
#include "mfapi.h"
#include "mferror.h"
#include "Mftransform.h"
#include <Audioclient.h> 
#include "mmdeviceapi.h"
#include "IMediaSource.h"
#include <Windows.UI.Composition.Interop.h>
#include <WindowsNumerics.h>
#include <Windows.UI.Composition.Interop.h>
#include <Windows.Foundation.Numerics.h>
#include <vector>

using namespace ABI::Windows::Foundation::Numerics;

//
// Define this flag to always run video swap chain through MPO path where video frames
// are presented on hardware plane 1 separate from plane 0 used by UI composition.
// It is more efficient to present video frames on plane 1 in terms of GPU bandwidth and
// color conversion. However, App must be in SRA Turbo mode which requires “hevcPlayback"
// capability in its Appx manifest. App will lose some SRA functionalities related
// to interaction with other SRA and ERA App like SNAP mode.
//
#define VIDEO_MPO_ALWAYS                    1



#define COLORSPACE_DEFAULT      0
#define COLORSPACE_BT2020       1
#define COLORSPACE_YCC709       2

#define HDR_NONE            0
#define HDR_EOTF_SDR        1
#define HDR_EOTF_2084       2

#define UINT_REFRESH_RATE(fltRate) ((UINT)((fltRate)+0.5))
#define DISPLAY_REFRESH_RATE_SYSTEM_DEFAULT 60
#define DISPLAY_REFRESH_RATE_24HZ           24
#define DISPLAY_REFRESH_RATE_50HZ           50
#define DISPLAY_REFRESH_RATE_60HZ           60

#define HD_1080P_WIDTH 1920
#define HD_1080P_HEIGHT 1080
#define UHD_4K_WIDTH 3840
#define UHD_4K_HEIGHT 2160
#define CINEMA_4K_WIDTH 4096
#define CINEMA_4K_HEIGHT 2160

#define UI_SWAPCHAIN_WIDTH 1920
#define UI_SWAPCHAIN_HEIGHT 1080

// {47537213-8cfb-4722-aa34-fbc9e24d77b8}   MF_MT_CUSTOM_VIDEO_PRIMARIES    {BLOB (MT_CUSTOM_VIDEO_PRIMARIES)}
DEFINE_GUID(MF_MT_CUSTOM_VIDEO_PRIMARIES,
    0x47537213, 0x8cfb, 0x4722, 0xaa, 0x34, 0xfb, 0xc9, 0xe2, 0x4d, 0x77, 0xb8);

LONGLONG ConvertQPCToHNS(LONGLONG llQPC);
LONGLONG GetCurrentTimeInHNS();

struct VideoFrameStatistics
{
    DXGI_FRAME_STATISTICS dxgiFrameStat;
    UINT dwIssuedPresentCount;
};

typedef struct _MT_CUSTOM_VIDEO_PRIMARIES {
    float fGx;
    float fGy;
    float fBx;
    float fBy;
    float fRx;
    float fRy;
    float fWx;
    float fWy;
} MT_CUSTOM_VIDEO_PRIMARIES;

typedef struct _METADATA_PLAYLIST {
    MT_CUSTOM_VIDEO_PRIMARIES primaries;
    UINT32 maxLum;
    UINT32 minLum;
    UINT32 maxCLL;
    UINT32 maxFALL;
} METADATA_PLAYLIST;

namespace DX
{
    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            // Set a breakpoint on this line to catch DirectX API errors
            throw Platform::Exception::CreateException(hr);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SampleSink
{
public:
    virtual bool GetDXGIDeviceManager(IMFDXGIDeviceManager** ppDxgiDevMgr) = 0;
    virtual bool QueueSample(IMFSample* pSample) = 0;
    virtual bool IsQueueFull() = 0;
    virtual void Apply2086MetaDataPlaylist() = 0;
    virtual HRESULT SetMediaType(IMFTransform* pMFT, IMFMediaType *pMediaType) = 0;
    virtual void SetColorPrimaries(MT_CUSTOM_VIDEO_PRIMARIES *pPrimaries) = 0;
    virtual void SetLuminance(REFGUID guidKey, UINT32 *pValue) = 0;
    virtual LONGLONG GetVideoTime() = 0;
    virtual void Flush() = 0;
    virtual double GetAvgFrameRate() = 0;
    virtual void NotifyVideoPrerollEnd() = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class AudioClock
{
public:
    virtual bool GetCorrelatedTime(LONGLONG* pllClockTime, LONGLONG* pllQpcInHns) = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class AutoLock
{
private:
    CRITICAL_SECTION* m_pCS;

public:
    AutoLock(CRITICAL_SECTION* pCS) : m_pCS(pCS)
    {
        EnterCriticalSection(m_pCS);
    }

    ~AutoLock()
    {
        LeaveCriticalSection(m_pCS);
        m_pCS = nullptr;
    }
};