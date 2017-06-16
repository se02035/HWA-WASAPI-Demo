#pragma once
#include "mfidl.h"

EXTERN_C const IID IID_IMediaSource;  

enum eStreamType
{
    AUDIOSTREAM = 0,
    VIDEOSTREAM = 1,
    MAXSTREAM = 2
};
      
//DEFINE_GUID(IMediaSource, 0xe1f82e69, 0xf69e, 0x4cbf, 0x8d, 0x3a, 0x59, 0x2b, 0x43, 0x1e, 0x8, 0x5a);

EXTERN_C const IID IID_IMediaSource;  

MIDL_INTERFACE("E1F82E69-F69E-4CBF-8D3A-592B431E085A")  
IMediaSource : public IUnknown  
{  
public:  
    virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE ReadSample(   
        /* [annotation][in] */   
        _In_  eStreamType streamType,  
       /* [annotation][out] */   
       _Out_  IMFSample **ppSample ) = 0;  
          
    virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE GetMediaType(   
        /* [annotation][in] */   
        _In_  eStreamType streamType,  
       /* [annotation][out] */   
       _Out_  IMFMediaType **ppMediaType, 
        /* [annotation][out] */   
       _Out_  IMFInputTrustAuthority **ppITA
       ) = 0;  

    virtual /* [helpstring][id] */ HRESULT STDMETHODCALLTYPE Seek(   
        /* [annotation][in] */   
        _In_  LONGLONG hnsTime ) = 0;  

    virtual /* [helpstring][id] */ void STDMETHODCALLTYPE Close() = 0;  

};  
      