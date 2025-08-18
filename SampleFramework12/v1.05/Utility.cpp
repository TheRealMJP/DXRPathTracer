//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"

#include "Utility.h"
#include "Exceptions.h"
#include "App.h"

namespace SampleFramework12
{

void WriteLog(const char* format, ...)
{
    char buffer[1024 * 16] = { 0 };
    va_list args;
    va_start(args, format);
    int32_t len = vsprintf_s(buffer, ArraySize_(buffer) - 1, format, args);
    if(GlobalApp != nullptr)
        GlobalApp->AddToLog(buffer);

    buffer[len] = '\n';
    buffer[len + 1] = 0;
    OutputDebugStringA(buffer);
}

std::wstring MakeString(const wchar_t* format, ...)
{
    wchar_t buffer[1024 * 16] = { 0 };
    va_list args;
    va_start(args, format);
    vswprintf_s(buffer, ArraySize_(buffer), format, args);
    return std::wstring(buffer);
}

std::string MakeString(const char* format, ...)
{
    char buffer[1024 * 16] = { 0 };
    va_list args;
    va_start(args, format);
    vsprintf_s(buffer, ArraySize_(buffer), format, args);
    return std::string(buffer);
}

std::string SampleFrameworkDir()
{
    return std::string(SampleFrameworkDir_);
}

}