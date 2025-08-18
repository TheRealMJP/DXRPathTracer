//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"
#include "ImGuiHelper.h"
#include "Window.h"
#include "Graphics/DX12.h"
#include "Graphics/DX12_Helpers.h"
#include "Graphics/GraphicsTypes.h"
#include "Graphics/ShaderCompilation.h"
#include "Graphics/Textures.h"
#include "ImGui/imgui.h"

namespace SampleFramework12
{

namespace ImGuiHelper
{

struct ImGuiVSConstants
{
    Float4x4 ProjectionMatrix;
};

static CompiledShaderPtr VS;
static CompiledShaderPtr PS;
static ID3D12PipelineState* PSO = nullptr;
static Texture FontTexture;
static float DPIScale = 1.0f;

#if UseAsserts_
    static uint64_t CurrBeginFrame = uint64_t(-1);
    static uint64_t CurrEndFrame = uint64_t(-1);
#endif

ImGuiContext* GUIContext = nullptr;

ImGuiKey KeyEventToImGuiKey(WPARAM wParam, LPARAM lParam)
{
    // There is no distinct VK_xxx for keypad enter, instead it is VK_RETURN + KF_EXTENDED.
    if ((wParam == VK_RETURN) && (HIWORD(lParam) & KF_EXTENDED))
        return ImGuiKey_KeypadEnter;

    const int scancode = (int)LOBYTE(HIWORD(lParam));
    //IMGUI_DEBUG_LOG("scancode %3d, keycode = 0x%02X\n", scancode, wParam);
    switch (wParam)
    {
        case VK_TAB: return ImGuiKey_Tab;
        case VK_LEFT: return ImGuiKey_LeftArrow;
        case VK_RIGHT: return ImGuiKey_RightArrow;
        case VK_UP: return ImGuiKey_UpArrow;
        case VK_DOWN: return ImGuiKey_DownArrow;
        case VK_PRIOR: return ImGuiKey_PageUp;
        case VK_NEXT: return ImGuiKey_PageDown;
        case VK_HOME: return ImGuiKey_Home;
        case VK_END: return ImGuiKey_End;
        case VK_INSERT: return ImGuiKey_Insert;
        case VK_DELETE: return ImGuiKey_Delete;
        case VK_BACK: return ImGuiKey_Backspace;
        case VK_SPACE: return ImGuiKey_Space;
        case VK_RETURN: return ImGuiKey_Enter;
        case VK_ESCAPE: return ImGuiKey_Escape;
        //case VK_OEM_7: return ImGuiKey_Apostrophe;
        case VK_OEM_COMMA: return ImGuiKey_Comma;
        //case VK_OEM_MINUS: return ImGuiKey_Minus;
        case VK_OEM_PERIOD: return ImGuiKey_Period;
        //case VK_OEM_2: return ImGuiKey_Slash;
        //case VK_OEM_1: return ImGuiKey_Semicolon;
        //case VK_OEM_PLUS: return ImGuiKey_Equal;
        //case VK_OEM_4: return ImGuiKey_LeftBracket;
        //case VK_OEM_5: return ImGuiKey_Backslash;
        //case VK_OEM_6: return ImGuiKey_RightBracket;
        //case VK_OEM_3: return ImGuiKey_GraveAccent;
        case VK_CAPITAL: return ImGuiKey_CapsLock;
        case VK_SCROLL: return ImGuiKey_ScrollLock;
        case VK_NUMLOCK: return ImGuiKey_NumLock;
        case VK_SNAPSHOT: return ImGuiKey_PrintScreen;
        case VK_PAUSE: return ImGuiKey_Pause;
        case VK_NUMPAD0: return ImGuiKey_Keypad0;
        case VK_NUMPAD1: return ImGuiKey_Keypad1;
        case VK_NUMPAD2: return ImGuiKey_Keypad2;
        case VK_NUMPAD3: return ImGuiKey_Keypad3;
        case VK_NUMPAD4: return ImGuiKey_Keypad4;
        case VK_NUMPAD5: return ImGuiKey_Keypad5;
        case VK_NUMPAD6: return ImGuiKey_Keypad6;
        case VK_NUMPAD7: return ImGuiKey_Keypad7;
        case VK_NUMPAD8: return ImGuiKey_Keypad8;
        case VK_NUMPAD9: return ImGuiKey_Keypad9;
        case VK_DECIMAL: return ImGuiKey_KeypadDecimal;
        case VK_DIVIDE: return ImGuiKey_KeypadDivide;
        case VK_MULTIPLY: return ImGuiKey_KeypadMultiply;
        case VK_SUBTRACT: return ImGuiKey_KeypadSubtract;
        case VK_ADD: return ImGuiKey_KeypadAdd;
        case VK_LSHIFT: return ImGuiKey_LeftShift;
        case VK_LCONTROL: return ImGuiKey_LeftCtrl;
        case VK_LMENU: return ImGuiKey_LeftAlt;
        case VK_LWIN: return ImGuiKey_LeftSuper;
        case VK_RSHIFT: return ImGuiKey_RightShift;
        case VK_RCONTROL: return ImGuiKey_RightCtrl;
        case VK_RMENU: return ImGuiKey_RightAlt;
        case VK_RWIN: return ImGuiKey_RightSuper;
        case VK_APPS: return ImGuiKey_Menu;
        case '0': return ImGuiKey_0;
        case '1': return ImGuiKey_1;
        case '2': return ImGuiKey_2;
        case '3': return ImGuiKey_3;
        case '4': return ImGuiKey_4;
        case '5': return ImGuiKey_5;
        case '6': return ImGuiKey_6;
        case '7': return ImGuiKey_7;
        case '8': return ImGuiKey_8;
        case '9': return ImGuiKey_9;
        case 'A': return ImGuiKey_A;
        case 'B': return ImGuiKey_B;
        case 'C': return ImGuiKey_C;
        case 'D': return ImGuiKey_D;
        case 'E': return ImGuiKey_E;
        case 'F': return ImGuiKey_F;
        case 'G': return ImGuiKey_G;
        case 'H': return ImGuiKey_H;
        case 'I': return ImGuiKey_I;
        case 'J': return ImGuiKey_J;
        case 'K': return ImGuiKey_K;
        case 'L': return ImGuiKey_L;
        case 'M': return ImGuiKey_M;
        case 'N': return ImGuiKey_N;
        case 'O': return ImGuiKey_O;
        case 'P': return ImGuiKey_P;
        case 'Q': return ImGuiKey_Q;
        case 'R': return ImGuiKey_R;
        case 'S': return ImGuiKey_S;
        case 'T': return ImGuiKey_T;
        case 'U': return ImGuiKey_U;
        case 'V': return ImGuiKey_V;
        case 'W': return ImGuiKey_W;
        case 'X': return ImGuiKey_X;
        case 'Y': return ImGuiKey_Y;
        case 'Z': return ImGuiKey_Z;
        case VK_F1: return ImGuiKey_F1;
        case VK_F2: return ImGuiKey_F2;
        case VK_F3: return ImGuiKey_F3;
        case VK_F4: return ImGuiKey_F4;
        case VK_F5: return ImGuiKey_F5;
        case VK_F6: return ImGuiKey_F6;
        case VK_F7: return ImGuiKey_F7;
        case VK_F8: return ImGuiKey_F8;
        case VK_F9: return ImGuiKey_F9;
        case VK_F10: return ImGuiKey_F10;
        case VK_F11: return ImGuiKey_F11;
        case VK_F12: return ImGuiKey_F12;
        case VK_F13: return ImGuiKey_F13;
        case VK_F14: return ImGuiKey_F14;
        case VK_F15: return ImGuiKey_F15;
        case VK_F16: return ImGuiKey_F16;
        case VK_F17: return ImGuiKey_F17;
        case VK_F18: return ImGuiKey_F18;
        case VK_F19: return ImGuiKey_F19;
        case VK_F20: return ImGuiKey_F20;
        case VK_F21: return ImGuiKey_F21;
        case VK_F22: return ImGuiKey_F22;
        case VK_F23: return ImGuiKey_F23;
        case VK_F24: return ImGuiKey_F24;
        case VK_BROWSER_BACK: return ImGuiKey_AppBack;
        case VK_BROWSER_FORWARD: return ImGuiKey_AppForward;
        default: break;
    }

    // Fallback to scancode
    // https://handmade.network/forums/t/2011-keyboard_inputs_-_scancodes,_raw_input,_text_input,_key_names
    switch (scancode)
    {
    case 41: return ImGuiKey_GraveAccent;  // VK_OEM_8 in EN-UK, VK_OEM_3 in EN-US, VK_OEM_7 in FR, VK_OEM_5 in DE, etc.
    case 12: return ImGuiKey_Minus;
    case 13: return ImGuiKey_Equal;
    case 26: return ImGuiKey_LeftBracket;
    case 27: return ImGuiKey_RightBracket;
    case 86: return ImGuiKey_Oem102;
    case 43: return ImGuiKey_Backslash;
    case 39: return ImGuiKey_Semicolon;
    case 40: return ImGuiKey_Apostrophe;
    case 51: return ImGuiKey_Comma;
    case 52: return ImGuiKey_Period;
    case 53: return ImGuiKey_Slash;
    default: break;
    }

    return ImGuiKey_None;
}

static void AddKeyEvent(ImGuiIO& io, ImGuiKey key, bool down, int32_t native_keycode, int32_t native_scancode = -1)
{
    io.AddKeyEvent(key, down);
    io.SetKeyEventNativeData(key, native_keycode, native_scancode); // To support legacy indexing (<1.87 user code)
}

static ImGuiMouseSource GetMouseSourceFromMessageExtraInfo()
{
    LPARAM extra_info = ::GetMessageExtraInfo();
    if ((extra_info & 0xFFFFFF80) == 0xFF515700)
        return ImGuiMouseSource_Pen;
    if ((extra_info & 0xFFFFFF80) == 0xFF515780)
        return ImGuiMouseSource_TouchScreen;
    return ImGuiMouseSource_Mouse;
}

static bool IsVkDown(int vk)
{
    return (::GetKeyState(vk) & 0x8000) != 0;
}

static void UpdateKeyModifiers(ImGuiIO& io)
{
    io.AddKeyEvent(ImGuiMod_Ctrl, IsVkDown(VK_CONTROL));
    io.AddKeyEvent(ImGuiMod_Shift, IsVkDown(VK_SHIFT));
    io.AddKeyEvent(ImGuiMod_Alt, IsVkDown(VK_MENU));
    io.AddKeyEvent(ImGuiMod_Super, IsVkDown(VK_LWIN) || IsVkDown(VK_RWIN));
}

static void WindowMessageCallback(void* context, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    ImGuiIO& io = ImGui::GetIO();
    switch(msg)
    {
    case WM_MOUSEMOVE:
    case WM_NCMOUSEMOVE:
    {
        ImGuiMouseSource mouse_source = GetMouseSourceFromMessageExtraInfo();
        POINT mouse_pos = { (LONG)GET_X_LPARAM(lParam), (LONG)GET_Y_LPARAM(lParam) };
        if (msg == WM_NCMOUSEMOVE && ::ScreenToClient(hWnd, &mouse_pos) == FALSE) // WM_NCMOUSEMOVE are provided in absolute coordinates.
            return;
        io.AddMouseSourceEvent(mouse_source);
        io.AddMousePosEvent((float)mouse_pos.x, (float)mouse_pos.y);
        return;
    }
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
    {
        ImGuiMouseSource mouse_source = GetMouseSourceFromMessageExtraInfo();
        int button = 0;
        if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) { button = 0; }
        if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) { button = 1; }
        if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK) { button = 2; }
        if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK) { button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4; }
        io.AddMouseSourceEvent(mouse_source);
        io.AddMouseButtonEvent(button, true);
        return;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP:
    {
        ImGuiMouseSource mouse_source = GetMouseSourceFromMessageExtraInfo();
        int button = 0;
        if (msg == WM_LBUTTONUP) { button = 0; }
        if (msg == WM_RBUTTONUP) { button = 1; }
        if (msg == WM_MBUTTONUP) { button = 2; }
        if (msg == WM_XBUTTONUP) { button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4; }
        io.AddMouseSourceEvent(mouse_source);
        io.AddMouseButtonEvent(button, false);
        return;
    }
    case WM_MOUSEWHEEL:
        io.AddMouseWheelEvent(0.0f, (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA);
        return;
    case WM_MOUSEHWHEEL:
        io.AddMouseWheelEvent(-(float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA, 0.0f);
        return;
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    {
        const bool isKeyDown = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
        if (wParam < 256)
        {
            // Submit modifiers
            UpdateKeyModifiers(io);

            // Obtain virtual key code and convert to ImGuiKey
            const ImGuiKey key = KeyEventToImGuiKey(wParam, lParam);
            const int vk = (int)wParam;
            const int scancode = (int)LOBYTE(HIWORD(lParam));

            // Special behavior for VK_SNAPSHOT / ImGuiKey_PrintScreen as Windows doesn't emit the key down event.
            if (key == ImGuiKey_PrintScreen && !isKeyDown)
                AddKeyEvent(io, key, true, vk, scancode);

            // Submit key event
            if (key != ImGuiKey_None)
                AddKeyEvent(io, key, isKeyDown, vk, scancode);

            // Submit individual left/right modifier events
            if (vk == VK_SHIFT)
            {
                // Important: Shift keys tend to get stuck when pressed together, missing key-up events are corrected in ImGui_ImplWin32_ProcessKeyEventsWorkarounds()
                if (IsVkDown(VK_LSHIFT) == isKeyDown) { AddKeyEvent(io, ImGuiKey_LeftShift, isKeyDown, VK_LSHIFT, scancode); }
                if (IsVkDown(VK_RSHIFT) == isKeyDown) { AddKeyEvent(io, ImGuiKey_RightShift, isKeyDown, VK_RSHIFT, scancode); }
            }
            else if (vk == VK_CONTROL)
            {
                if (IsVkDown(VK_LCONTROL) == isKeyDown) { AddKeyEvent(io, ImGuiKey_LeftCtrl, isKeyDown, VK_LCONTROL, scancode); }
                if (IsVkDown(VK_RCONTROL) == isKeyDown) { AddKeyEvent(io, ImGuiKey_RightCtrl, isKeyDown, VK_RCONTROL, scancode); }
            }
            else if (vk == VK_MENU)
            {
                if (IsVkDown(VK_LMENU) == isKeyDown) { AddKeyEvent(io, ImGuiKey_LeftAlt, isKeyDown, VK_LMENU, scancode); }
                if (IsVkDown(VK_RMENU) == isKeyDown) { AddKeyEvent(io, ImGuiKey_RightAlt, isKeyDown, VK_RMENU, scancode); }
            }
        }
        return;
    }
    case WM_CHAR:
        if (::IsWindowUnicode(hWnd))
        {
            if (wParam > 0 && wParam < 0x10000)
                io.AddInputCharacterUTF16((uint16_t)wParam);
        }
        else
        {
            wchar_t wch = 0;
            MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, (char*)&wParam, 1, &wch, 1);
            io.AddInputCharacter(wch);
        }
        return;
    }
}

static void InitDPISpecific()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    ImGui::StyleColorsDark(&style);
    style.ScaleAllSizes(DPIScale);

    ImGuiIO& io = ImGui::GetIO();

    io.Fonts->Clear();
    io.Fonts->AddFontFromFileTTF("..\\Content\\Fonts\\Roboto-Medium.ttf", 16.0f * DPIScale);

    uint8_t* pixels = nullptr;
    int32_t texWidth = 0;
    int32_t texHeight = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &texWidth, &texHeight);

    Create2DTexture(FontTexture, texWidth, texHeight, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, false, pixels);
    io.Fonts->TexID = ToImTextureID(FontTexture.SRV, 0);
}

void Initialize(Window& window, float dpiScale)
{
    window.RegisterMessageCallback(WindowMessageCallback, nullptr);

    DPIScale = dpiScale;

    GUIContext = ImGui::CreateContext();

    const std::string shaderPath = SampleFrameworkDir() + "Shaders\\ImGui.hlsl";
    VS = CompileFromFile(shaderPath.c_str(), "ImGuiVS", ShaderType::Vertex);
    PS = CompileFromFile(shaderPath.c_str(), "ImGuiPS", ShaderType::Pixel);

   InitDPISpecific();
}

void Shutdown()
{
    ImGui::DestroyContext(GUIContext);
    GUIContext = nullptr;

    FontTexture.Shutdown();

    DestroyPSOs();
}

void CreatePSOs(DXGI_FORMAT rtFormat)
{
    D3D12_INPUT_ELEMENT_DESC inputElements[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = { };
    psoDesc.pRootSignature = DX12::UniversalRootSignatureWithIA;
    psoDesc.VS = VS.ByteCode();
    psoDesc.PS = PS.ByteCode();
    psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::NoCull);
    psoDesc.BlendState = DX12::GetBlendState(BlendState::AlphaBlend);
    psoDesc.DepthStencilState = DX12::GetDepthState(DepthState::Disabled);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = rtFormat;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.InputLayout.pInputElementDescs = inputElements;
    psoDesc.InputLayout.NumElements = ArraySize_(inputElements);
    DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&PSO)));
}

void DestroyPSOs()
{
    DX12::DeferredRelease(PSO);
}

void BeginFrame(uint32_t displayWidth, uint32_t displayHeight, float timeDelta, float dpiScale)
{
    Assert_(CurrBeginFrame != DX12::CurrentCPUFrame);

    if(dpiScale != DPIScale)
    {
        DPIScale = dpiScale;
        InitDPISpecific();
    }

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(float(displayWidth), float(displayHeight));
    io.DeltaTime = timeDelta;

    // Read keyboard modifiers inputs
    io.KeyCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    io.KeyShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    io.KeyAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;

    ImGui::NewFrame();

    #if UseAsserts_
        CurrBeginFrame = DX12::CurrentCPUFrame;
    #endif
}

void EndFrame(ID3D12GraphicsCommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE rtv, uint32_t displayWidth, uint32_t displayHeight)
{
    Assert_(CurrBeginFrame == DX12::CurrentCPUFrame);
    Assert_(CurrEndFrame != DX12::CurrentCPUFrame);

    ImGui::Render();

    PIXMarker pixMarker(cmdList, "ImGui Rendering");

    ImDrawData* drawData = ImGui::GetDrawData();

    // Get memory for vertex and index buffers
    const uint64_t vbSize = sizeof(ImDrawVert) * drawData->TotalVtxCount;
    const uint64_t ibSize = sizeof(ImDrawIdx) * drawData->TotalIdxCount;
    MapResult vertexMem = DX12::AcquireTempBufferMem(vbSize, 4);
    MapResult indexMem = DX12::AcquireTempBufferMem(ibSize, 4);

    // Copy and convert all vertices into a single contiguous buffer
    ImDrawVert* vertexCPUMem = reinterpret_cast<ImDrawVert*>(vertexMem.CPUAddress);
    ImDrawIdx* indexCPUMem = reinterpret_cast<ImDrawIdx*>(indexMem.CPUAddress);
    for(int32_t i = 0; i < drawData->CmdListsCount; i++)
    {
        const ImDrawList* drawList = drawData->CmdLists[i];
        memcpy(vertexCPUMem, &drawList->VtxBuffer[0], drawList->VtxBuffer.size() * sizeof(ImDrawVert));
        memcpy(indexCPUMem, &drawList->IdxBuffer[0], drawList->IdxBuffer.size() * sizeof(ImDrawIdx));
        vertexCPUMem += drawList->VtxBuffer.size();
        indexCPUMem += drawList->IdxBuffer.size();
    }

    // Setup orthographic projection matrix into our constant buffer
    ImGuiVSConstants constants;

    {
        const float L = 0.0f;
        const float R = float(displayWidth);
        const float B = float(displayHeight);
        const float T = 0.0f;

        constants.ProjectionMatrix = Float4x4(Float4(2.0f/(R-L),   0.0f,           0.0f,       0.0f),
                                              Float4(0.0f,         2.0f/(T-B),     0.0f,       0.0f),
                                              Float4(0.0f,         0.0f,           0.5f,       0.0f),
                                              Float4((R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f));
    }

    // Setup viewport
    DX12::SetViewport(cmdList, displayWidth, displayHeight);

    cmdList->OMSetRenderTargets(1, &rtv, false, nullptr);

    // Bind shader and vertex buffers
    cmdList->SetPipelineState(PSO);
    cmdList->SetGraphicsRootSignature(DX12::UniversalRootSignatureWithIA);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    DX12::BindTempConstantBuffer(cmdList, constants, URS_ConstantBuffers + 0, CmdListMode::Graphics);

    D3D12_VERTEX_BUFFER_VIEW vbView = { };
    vbView.BufferLocation = vertexMem.GPUAddress;
    vbView.SizeInBytes = uint32_t(vbSize);
    vbView.StrideInBytes = sizeof(ImDrawVert);
    cmdList->IASetVertexBuffers(0, 1, &vbView);

    D3D12_INDEX_BUFFER_VIEW ibView = { };
    ibView.BufferLocation = indexMem.GPUAddress;
    ibView.SizeInBytes = uint32_t(ibSize);
    ibView.Format = DXGI_FORMAT_R16_UINT;
    cmdList->IASetIndexBuffer(&ibView);

    // Render command lists
    uint32_t vtxOffset = 0;
    uint32_t idxOffset = 0;
    for(int32_t cmdListIdx = 0; cmdListIdx < drawData->CmdListsCount; cmdListIdx++)
    {
        const ImDrawList* drawList = drawData->CmdLists[cmdListIdx];
        for(int32_t cmdIdx = 0; cmdIdx < drawList->CmdBuffer.size(); cmdIdx++)
        {
            const ImDrawCmd* drawCmd = &drawList->CmdBuffer[cmdIdx];
            if(drawCmd->UserCallback)
            {
                drawCmd->UserCallback(drawList, drawCmd);
            }
            else
            {
                const D3D12_RECT r = { int32_t(drawCmd->ClipRect.x), int32_t(drawCmd->ClipRect.y), int32_t(drawCmd->ClipRect.z), int32_t(drawCmd->ClipRect.w) };

                if(r.left < r.right && r.top < r.bottom)
                {
                    DX12::BindTempConstantBuffer(cmdList, drawCmd->GetTexID(), URS_ConstantBuffers + 1, CmdListMode::Graphics);

                    cmdList->RSSetScissorRects(1, &r);

                    cmdList->DrawIndexedInstanced(drawCmd->ElemCount, 1, idxOffset, vtxOffset, 0);
                }
            }
            idxOffset += drawCmd->ElemCount;
        }
        vtxOffset += drawList->VtxBuffer.size();
    }

    #if UseAsserts_
        CurrEndFrame = DX12::CurrentCPUFrame;
    #endif
}

} // namespace ImGuiHelper

} // namespace SampleFramework12
