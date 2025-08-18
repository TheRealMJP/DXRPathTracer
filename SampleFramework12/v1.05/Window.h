//=================================================================================================
//
//  MJP's DX11 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "PCH.h"

#include "Exceptions.h"
#include "Containers.h"

namespace SampleFramework12
{

class Window
{

// Public types
public:

    typedef void (*MsgFunction)(void*, HWND, uint32_t, WPARAM, LPARAM);

// Constructor and destructor
public:

    Window(HINSTANCE hinstance,
           const char* name = "SampleCommon Window",
           uint32_t style = WS_CAPTION|WS_OVERLAPPED|WS_SYSMENU,
           uint32_t exStyle = WS_EX_APPWINDOW,
           uint32_t clientWidth = 1280,
           uint32_t clientHeight = 720,
           const char* iconResource = nullptr,
           const char* smallIconResource = nullptr,
           const char* menuResource = nullptr,
           const char* accelResource = nullptr);
    ~Window();



// Public methods
public:

    HWND GetHwnd() const;
    HMENU GetMenu() const;
    HINSTANCE GetHinstance() const;
    void MessageLoop();

    bool32 IsAlive() const;
    bool32 IsMinimized() const;
    bool32 HasFocus() const;
    LONG_PTR GetWindowStyle() const;
    LONG_PTR GetExtendedStyle() const;
    void SetWindowStyle(uint32_t newStyle);
    void SetExtendedStyle(uint32_t newExStyle);
    void Maximize();
    void SetWindowPos(int32_t posX, int32_t posY);
    void GetWindowPos(int32_t& posX, int32_t& posY) const;
    void ShowWindow(bool show = true);
    void SetClientArea(int32_t clientX, int32_t clientY);
    void GetClientArea(int32_t& clientX, int32_t& clientY) const;
    void SetWindowTitle(const char* title);
    void SetScrollRanges(int32_t scrollRangeX, int32_t scrollRangeY, int32_t posX, int32_t posY);
    void SetBorderless(bool borderless);
    bool Borderless() const;
    void Destroy();

    int32_t CreateMessageBox(const char* message, const char* title = nullptr, uint32_t type = MB_OK);

    void RegisterMessageCallback(MsgFunction msgFunction, void* context);

    operator HWND() { return hwnd; }        //conversion operator

// Private methods
private:
    void MakeWindow(const char* iconResource, const char* smallIconResource, const char* menuResource);

    LRESULT MessageHandler(HWND hWnd, uint32_t uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT WINAPI WndProc(HWND hWnd, uint32_t uMsg, WPARAM wParam, LPARAM lParam);

// Private members
private:

    // Window properties
    HWND hwnd;                  // The window handle
    HINSTANCE hinstance;        // The HINSTANCE of the application
    std::string appName;       // The name of the application
    uint32_t style;               // The current window style
    uint32_t exStyle;             // The extended window style
    HACCEL accelTable;          // Accelerator table handle
    uint32_t nonFSWidth = 0;
    uint32_t nonFSHeight = 0;
    int32_t nonFSPosX = 0;
    int32_t nonFSPosY = 0;
    uint64_t nonFSStyle = 0;
    bool borderless = false;

    struct Callback
    {
        MsgFunction Function;
        void* Context;
    };

    List<Callback> messageCallbacks;            // Message callback list
};

}