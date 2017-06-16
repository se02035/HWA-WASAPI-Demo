#include "pch.h"
#include <fstream>
#include <iostream>
#include <sstream>

#define DebugLogEnable 0
using namespace std;

// log file
static ofstream g_LogOf;

bool InitDxSampleFileLog()
{
#if DebugLogEnable
    if (!g_LogOf.is_open())
    {
        g_LogOf.open("d:\\samplelog.txt", ios::out | ios::binary);
    }
#endif
    return g_LogOf.is_open();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This really should be a light weight instrumentation like ETL, but for the sake of simplicity, log file is not too horrible
void DbgLog(char* pLogStr, ...)
{
#if DebugLogEnable

    char str[4096] = "";

    va_list ArgList;
    va_start(ArgList, pLogStr);

    static LARGE_INTEGER liFreq = {};
    if (0 == liFreq.QuadPart)
    {
        QueryPerformanceFrequency(&liFreq);
    }

    LARGE_INTEGER liNow = {};
    QueryPerformanceCounter(&liNow);
    LONGLONG llNowMS = (liNow.QuadPart * 1000) / liFreq.QuadPart;

    int iLen = 0, iLen2 = 0;
    char* pBuf = str;
    iLen = sprintf_s(pBuf, 4096, "%I64d ", llNowMS);
    pBuf = pBuf + iLen;
    iLen2 = vsprintf_s(pBuf, 4096 - iLen, pLogStr, ArgList);

    va_end(ArgList);

    OutputDebugStringA(str);

    if (g_LogOf.is_open())
    {
        g_LogOf.write(str, iLen + iLen2);
    }
#endif // #if DebugLogEnable
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DbgPrint(char* pLogStr, ...)
{
    char str[4096];

    va_list ArgList;
    va_start(ArgList, pLogStr);

    static LARGE_INTEGER liFreq = {};
    if (0 == liFreq.QuadPart)
    {
        QueryPerformanceFrequency(&liFreq);
    }

    LARGE_INTEGER liNow = {};
    QueryPerformanceCounter(&liNow);
    LONGLONG llNowMS = (liNow.QuadPart * 1000) / liFreq.QuadPart;

    char* pBuf = str;

    int iLen = sprintf_s(pBuf, 4096, "%I64d ", llNowMS);

    pBuf = pBuf + iLen;
    vsprintf_s(pBuf, 4096 - iLen, pLogStr, ArgList);

    OutputDebugStringA(str);

    va_end(ArgList);
}