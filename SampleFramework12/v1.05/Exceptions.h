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
#include "SF12_Assert.h"

namespace SampleFramework12
{

// Error string functions
inline std::string GetWin32ErrorString(DWORD errorCode)
{
    char errorString[MAX_PATH] = { };
    ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
                     0,
                     errorCode,
                     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                     errorString,
                     MAX_PATH,
                     nullptr);

    std::string message = "Win32 Error: ";
    message += errorString;
    return message;
}


inline std::string GetDXErrorString(HRESULT hr)
{
    char errorString[MAX_PATH] = {};
    ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
                     0,
                     hr,
                     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                     errorString,
                     MAX_PATH,
                     nullptr);

    std::string message = "DirectX Error: ";
    message += errorString;
    return message;
}


inline std::string GetGdiPlusErrorString(Gdiplus::Status status)
{
    std::string errorString;

    if (status == Gdiplus::GenericError)
        errorString = "Generic Error";
    else if (status == Gdiplus::InvalidParameter)
        errorString = "Invalid Parameter";
    else if (status == Gdiplus::OutOfMemory)
        errorString = "Out Of Memory";
    else if (status == Gdiplus::ObjectBusy)
        errorString = "Object Busy";
    else if (status == Gdiplus::InsufficientBuffer)
        errorString = "Insufficient Buffer";
    else if (status == Gdiplus::NotImplemented)
        errorString = "Not Implemented";
    else if (status == Gdiplus::Win32Error)
        errorString = "Win32 Error";
    else if (status == Gdiplus::WrongState)
        errorString = "Wrong State";
    else if (status == Gdiplus::Aborted)
        errorString = "Aborted";
    else if (status == Gdiplus::FileNotFound)
        errorString = "File Not Found";
    else if (status == Gdiplus::ValueOverflow)
        errorString = "Value Overflow";
    else if (status == Gdiplus::AccessDenied)
        errorString = "Access Denied";
    else if (status == Gdiplus::UnknownImageFormat)
        errorString = "Unknown Image Format";
    else if (status == Gdiplus::FontFamilyNotFound)
        errorString = "Font Family Not Found";
    else if (status == Gdiplus::FontStyleNotFound)
        errorString = "Font Style Not Found";
    else if (status == Gdiplus::NotTrueTypeFont)
        errorString = "Not TrueType Font";
    else if (status == Gdiplus::UnsupportedGdiplusVersion)
        errorString = "Unsupported GDI+ Version";
    else if (status == Gdiplus::GdiplusNotInitialized)
        errorString = "GDI+ Not Initialized";
    else if (status == Gdiplus::PropertyNotFound)
        errorString = "Property Not Found";
    else if (status == Gdiplus::PropertyNotSupported)
        errorString = "Property Not Supported";

    return "GDI+ Error: " + errorString;
}

// Generic exception, used as base class for other types
class Exception
{

public:

    Exception()
    {
    }

    // Specify an actual error message
    Exception(const std::string& exceptionMessage) : message(exceptionMessage)
    {
    }

    // Retrieve that error message
    const std::string& GetMessage() const throw()
    {
        return message;
    }

    void ShowErrorMessage() const throw ()
    {
        MessageBox(nullptr, message.c_str(), "Error", MB_OK|MB_ICONERROR);
    }

protected:

    std::string  message;    // The error message
};

// Exception thrown when a Win32 function fails.
class Win32Exception : public Exception
{

public:

    // Obtains a string for the specified Win32 error code
    Win32Exception(DWORD code, const char* msgPrefix = nullptr) : errorCode(code)
    {
        char errorString[MAX_PATH] = {};
        ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                        0,
                        errorCode,
                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                        errorString,
                        MAX_PATH,
                        NULL  );

        message = "Win32 Error: ";
        if(msgPrefix)
            message += msgPrefix;
        message += errorString;
    }

    // Retrieve the error code
    DWORD GetErrorCode() const throw ()
    {
        return errorCode;
    }

protected:

  DWORD  errorCode;    // The Win32 error code

};

// Exception thrown when a DirectX Function fails
class DXException : public Exception
{

public:

    // Obtains a string for the specified HRESULT error code
    DXException(HRESULT hresult) : errorCode(hresult)
    {
        message = GetDXErrorString(hresult);
    }

    DXException(HRESULT hresult, const char* errorMsg) : errorCode(hresult)
    {
        message = "DirectX Error: ";
        message += errorMsg;
    }

    // Retrieve the error code
    HRESULT GetErrorCode() const throw ()
    {
        return errorCode;
    }

protected:

    HRESULT errorCode;    // The DX error code
};

// Exception thrown when a GDI+ function fails
class GdiPlusException : public Exception
{

public:

    // Obtains a string for the specified error code
    GdiPlusException(Gdiplus::Status status) : errorCode(status)
    {
        message = GetGdiPlusErrorString(status);
    }

    // Retrieve the error code
    Gdiplus::Status GetErrorCode() const throw ()
    {
        return errorCode;
    }

protected:

    Gdiplus::Status  errorCode;    // The GDI+ error code
};

// Error-handling functions

#if UseAsserts_

#define DXCall(x)                                                           \
    do                                                                      \
    {                                                                       \
        HRESULT hr_ = x;                                                    \
        AssertMsg_(SUCCEEDED(hr_), GetDXErrorString(hr_).c_str());          \
    }                                                                       \
    while(0)

#define Win32Call(x)                                                            \
    do                                                                          \
    {                                                                           \
        BOOL res_ = x;                                                          \
        AssertMsg_(res_ != 0, GetWin32ErrorString(GetLastError()).c_str());     \
    }                                                                           \
    while(0)

#define GdiPlusCall(x)                                                                  \
    do                                                                                  \
    {                                                                                   \
        Gdiplus::Status status_ = x;                                                    \
        AssertMsg_(status_ == Gdiplus::Ok, GetGdiPlusErrorString(status_).c_str());     \
    }                                                                                   \
    while(0)

#else

// Throws a DXException on failing HRESULT
inline void DXCall(HRESULT hr)
{
    if(FAILED(hr))
        throw DXException(hr);
}

// Throws a Win32Exception on failing return value
inline void Win32Call(BOOL retVal)
{
    if(retVal == 0)
        throw Win32Exception(GetLastError());
}

// Throws a GdiPlusException on failing Status value
inline void GdiPlusCall(Gdiplus::Status status)
{
    if(status != Gdiplus::Ok)
        throw GdiPlusException(status);
}

#endif

}