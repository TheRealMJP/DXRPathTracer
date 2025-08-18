//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include <AppConfig.h>

// Add common controls 6.0 DLL to the manifest
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

// Standard int typedefs
#include <stdint.h>

typedef uint32_t bool32;

// Disabled compiler warnings
#pragma warning(disable : 4100) // unreferenced formal parameter
#pragma warning(disable : 4127) // conditional expression is constant
#pragma warning(disable : 4324) // structure was padded due to alignment specifier

// Platform SDK defines, specifies that our min version is Windows 10
#ifndef WINVER
#define WINVER 0x0A00
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0410
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0A00
#endif

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define STRICT                          // Use strict declarations for Windows types

#define assert(expression) ((void)0)

// DirectX Includes
#if EnablePreviewDX12SDK_
    #include "..\\..\\Externals\\DXSDK_Preview\\Include\\d3d12.h"
    #include "..\\..\\Externals\\DXSDK_Preview\\Include\\d3dx12\\d3dx12.h"
#else
    #include "..\\..\\Externals\\DXSDK\\Include\\d3d12.h"
    #include "..\\..\\Externals\\DXSDK\\Include\\d3dx12\\d3dx12.h"
#endif
#include <dxgi.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>

// Windows Header Files:
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <wrl.h>

#undef IsMinimized
#undef GetWindowStyle

// GDI+
#pragma warning(push)
#pragma warning(disable : 4458)
#include <gdiplus.h>
#pragma warning(pop)

// DirectX Math
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXCollision.h>

// DirectX Tex
#include "..\\..\\Externals\\DirectXTex July 2017\\Include\\DirectXTex.h"
#ifdef _DEBUG
    #pragma comment(lib, "..\\Externals\\DirectXTex July 2017\\Lib 2017\\Debug\\DirectXTex.lib")
#else
    #pragma comment(lib, "..\\Externals\\DirectXTex July 2017\\Lib 2017\\Release\\DirectXTex.lib")
#endif

// Pix for Windows
#define USE_PIX
#include "..\\..\\Externals\\WinPixEventRuntime\\Include\\WinPixEventRuntime\\pix3.h"
#pragma comment(lib, "..\\Externals\\WinPixEventRuntime\\bin\\WinPixEventRuntime.lib")

// Un-define min and max from the windows headers
#ifdef min
    #undef min
#endif

#ifdef max
    #undef max
#endif

// C++ Standard Library Header Files
#include <string>
#include <cmath>
#include <random>

// Assimp
#include "..\\..\\Externals\\Assimp-5.2.4\\include\\assimp\\Importer.hpp"
#include "..\\..\\Externals\\Assimp-5.2.4\\include\\assimp\\scene.h"
#include "..\\..\\Externals\\Assimp-5.2.4\\include\\assimp\\postprocess.h"
#pragma comment(lib, "..\\Externals\\Assimp-5.2.4\\lib\\assimp-vc143-mt.lib")

// Embree
#if EnableEmbree_
    #include "..\\..\\Externals\\Embree-2.8\\include\\embree2\\rtcore.h"
    #include "..\\..\\Externals\\Embree-2.8\\include\\embree2\\rtcore_ray.h"
    #pragma comment(lib, "..\\Externals\\Embree-2.8\\lib\\embree.lib")
#endif

// cxxopts
#include "..\\..\\Externals\\cxxopts\\include\\\cxxopts.hpp"

// Windows SDK imports
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "D3D12.lib")

// DXC
#include "..\\..\\Externals\\DXCompiler\\Include\\dxcapi.h"
#pragma comment(lib, "..\\Externals\\DXCompiler\\Lib\\dxcompiler.lib")

// meshoptimizer
#include "..\\..\\Externals\\meshoptimizer\\meshoptimizer.h"
#if Debug_
    #pragma comment(lib, "..\\Externals\\meshoptimizer\\Lib\\Debug\\meshoptimizer.lib")
#else
    #pragma comment(lib, "..\\Externals\\meshoptimizer\\Lib\\Release\\meshoptimizer.lib")
#endif