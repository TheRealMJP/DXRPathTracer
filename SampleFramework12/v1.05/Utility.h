//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "PCH.h"

#include "Exceptions.h"
#include "InterfacePointers.h"
#include "SF12_Math.h"
#include "SF12_Assert.h"
#include "Containers.h"

namespace SampleFramework12
{

template<typename T, uint64_t N>
uint64_t ArraySize(T(&)[N])
{
    return N;
}

#define ArraySize_(x) ((sizeof(x) / sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

// Converts an ANSI string to a std::wstring
inline std::wstring ToWString(const char* ansiString)
{
    wchar_t buffer[512] = { };
    Win32Call(MultiByteToWideChar(CP_UTF8, 0, ansiString, -1, buffer, ArraySize_(buffer)));
    return std::wstring(buffer);
}

inline std::string ToAnsi(const wchar_t* wideString)
{
    char buffer[512] = { };
    Win32Call(WideCharToMultiByte(CP_UTF8, 0, wideString, -1, buffer, ArraySize_(buffer), nullptr, nullptr));
    return std::string(buffer);
}

// Splits up a string using a delimiter
inline void Split(const std::string& str, List<std::string>& parts, const std::string& delimiters = " ")
{
    // Skip delimiters at beginning
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);

    // Find first "non-delimiter"
    std::string::size_type pos = str.find_first_of(delimiters, lastPos);

    while(std::string::npos != pos || std::string::npos != lastPos)
    {
        // Found a token, add it to the vector
        parts.Add(str.substr(lastPos, pos - lastPos));

        // Skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);

        // Find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }
}

// Splits up a string using a delimiter
inline List<std::string> Split(const std::string& str, const std::string& delimiters = " ")
{
    List<std::string> parts;
    Split(str, parts, delimiters);
    return parts;
}

void WriteLog(const char* format, ...);

std::wstring MakeString(const wchar_t* format, ...);
std::string MakeString(const char* format, ...);

std::string SampleFrameworkDir();

// Gets an index from an index buffer
inline uint32_t GetIndex(const void* indices, uint32_t idx, uint32_t indexSize)
{
    if(indexSize == 2)
        return reinterpret_cast<const uint16_t*>(indices)[idx];
    else
        return reinterpret_cast<const uint32_t*>(indices)[idx];
}

inline uint32_t AlignTo(uint32_t num, uint32_t alignment)
{
    Assert_(alignment > 0);
    return ((num + alignment - 1) / alignment) * alignment;
}

inline uint64_t AlignTo(uint64_t num, uint64_t alignment)
{
    Assert_(alignment > 0);
    return ((num + alignment - 1) / alignment) * alignment;
}

template<typename To, typename From> To SafeCast(From x)
{
    Assert_(x >= std::numeric_limits<To>::min());
    Assert_(x <= std::numeric_limits<To>::max());
    return To(x);
}

}