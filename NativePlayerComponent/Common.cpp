#include "pch.h"
#include "Common.h"

// Helper methods.
static LARGE_INTEGER s_liFreq = {};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
LONGLONG ConvertQPCToHNS(LONGLONG llQPC)
{
    if (0 == s_liFreq.QuadPart)
    {
        QueryPerformanceFrequency(&s_liFreq);
    }
    return MFllMulDiv(llQPC, 10000000ll, s_liFreq.QuadPart, 0);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
LONGLONG GetCurrentTimeInHNS()
{
    LARGE_INTEGER liCurrent = {};

    if (0 == s_liFreq.QuadPart)
    {
        QueryPerformanceFrequency(&s_liFreq);
    }
    QueryPerformanceCounter(&liCurrent);
    return MFllMulDiv(liCurrent.QuadPart, 10000000ll, s_liFreq.QuadPart, 0);
}
