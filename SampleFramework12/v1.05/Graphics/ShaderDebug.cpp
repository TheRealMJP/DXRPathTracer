//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"

#include "ShaderDebug.h"
#include "GraphicsTypes.h"
#include "DX12_Helpers.h"
#include "ShaderCompilation.h"
#include "../App.h"
#include "../Shaders/ShaderDebug_Shared.h"

using namespace ShaderDebug;

namespace SampleFramework12
{

namespace ShaderDebug
{

static RawBuffer DebugInfoBuffer;
static RawBuffer PrintBuffer;
static ReadbackBuffer PrintReadbackBuffers[DX12::RenderLatency];
static RawBuffer LineBuffer;
static RawBuffer SphereBuffer;

static CompiledShaderPtr InitDrawBuffersCS;
static CompiledShaderPtr DebugDrawLineVS;
static CompiledShaderPtr DebugDrawSphereVS;
static CompiledShaderPtr DebugDrawPS;

static ID3D12PipelineState* InitDrawBuffersPSO = nullptr;
static ID3D12PipelineState* LinePSO = nullptr;
static ID3D12PipelineState* SpherePSO = nullptr;

static ID3D12CommandSignature* DrawCommandSignature = nullptr;
static ID3D12CommandSignature* DrawIndexedCommandSignature = nullptr;

static StructuredBuffer SphereVertexBuffer;
static FormattedBuffer SphereIndexBuffer;

static const std::string ArgPlaceHolders[] =
{
    "{0}",
    "{1}",
    "{2}",
    "{3}",
    "{4}",
    "{5}",
    "{6}",
    "{7}",
    "{8}",
    "{9}",
    "{10}",
    "{11}",
    "{12}",
    "{13}",
    "{14}",
    "{15}",
};
StaticAssert_(ArraySize_(ArgPlaceHolders) == MaxDebugPrintArgs);

static const uint32_t ArgCodeSizes[] =
{
    sizeof(uint32_t),   // DebugPrint_Uint
    sizeof(Uint2),      // DebugPrint_Uint2
    sizeof(Uint3),      // DebugPrint_Uint3
    sizeof(Uint4),      // DebugPrint_Uint4
    sizeof(int32_t),    // DebugPrint_Int
    sizeof(Int2),       // DebugPrint_Int2
    sizeof(Int3),       // DebugPrint_Int3
    sizeof(Int4),       // DebugPrint_Int4
    sizeof(float),      // DebugPrint_Float
    sizeof(Float2),     // DebugPrint_Float2
    sizeof(Float3),     // DebugPrint_Float3
    sizeof(Float4),     // DebugPrint_Float4
};

static const uint32_t MaxLines = 1024 * 1024;
static const uint32_t MaxSpheres = 512 * 1024;

static void ReplaceStringInPlace(std::string& subject, const std::string& search, const std::string& replace)
{
    size_t pos = 0;
    while((pos = subject.find(search, pos)) != std::string::npos)
    {
         subject.replace(pos, search.length(), replace);
         pos += replace.length();
    }
}

static void BuildSphereData()
{
    List<Float3> vertices;
    List<uint16_t> indices;

    const uint32_t tessellation = 5;
    Assert_(tessellation >= 3);

    const uint32_t verticalSegments = tessellation;
    const uint32_t horizontalSegments = tessellation * 2;
    const float radius = 1.0f;

    // Create rings of vertices at progressively higher latitudes.
    for (uint32_t i = 0; i <= verticalSegments; i++)
    {
        const float latitude = (float(i) * Pi / float(verticalSegments)) - Pi_2;
        float dy, dxz;
        DirectX::XMScalarSinCos(&dy, &dxz, latitude);

        // Create a single ring of vertices at this latitude.
        for (size_t j = 0; j <= horizontalSegments; j++)
        {
            const float longitude = float(j) * Pi2 / float(horizontalSegments);
            float dx, dz;

            DirectX::XMScalarSinCos(&dx, &dz, longitude);

            dx *= dxz;
            dz *= dxz;

            vertices.Add(Float3(dx, dy, dz) * radius);
        }
    }

    Assert_(vertices.Count() < UINT16_MAX);

    // Fill the index buffer with triangles joining each pair of latitude rings.
    const uint32_t stride = horizontalSegments + 1;

    for (uint32_t i = 0; i < verticalSegments; i++)
    {
        for (uint32_t j = 0; j <= horizontalSegments; j++)
        {
            const uint32_t nextI = i + 1;
            const uint32_t nextJ = (j + 1) % stride;

            indices.Add(uint16_t(i * stride + j));
            indices.Add(uint16_t(nextI * stride + j));

            indices.Add(uint16_t(nextI * stride + j));
            indices.Add(uint16_t(nextI * stride + nextJ));

            indices.Add(uint16_t(nextI * stride + nextJ));
            indices.Add(uint16_t(i * stride + nextJ));

            indices.Add(uint16_t(i * stride + nextJ));
            indices.Add(uint16_t(i * stride + j));
        }
    }

    SphereVertexBuffer.Initialize({
        .Stride = sizeof(Float3),
        .NumElements = vertices.Count(),
        .InitData = vertices.Data(),
        .Name = "Debug Sphere Vertex Buffer",
    });

    SphereIndexBuffer.Initialize({
        .Format = DXGI_FORMAT_R16_UINT,
        .NumElements = indices.Count(),
        .InitData = indices.Data(),
        .Name = "Debug Sphere Index Buffer",
    });
}

void Initialize()
{
    DebugInfoBuffer.Initialize({
        .NumElements = sizeof(DebugInfo) / 4,
        .Dynamic = true,
        .CPUAccessible = true,
        .Name = "Debug Info Buffer",
    });

    const PersistentDescriptorAlloc alloc = DX12::SRVDescriptorHeap.AllocatePersistent(MagicDebugBufferIndex);
    DX12::SRVDescriptorHeap.FreePersistent(DebugInfoBuffer.SRV);
    DebugInfoBuffer.SRV = alloc.Index;

    for(uint32_t i = 0; i < ArraySize_(alloc.Handles); ++i)
    {
        const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = DebugInfoBuffer.SRVDesc(i);
        DX12::Device->CreateShaderResourceView(DebugInfoBuffer.Resource(), &srvDesc, alloc.Handles[i]);
    }

    PrintBuffer.Initialize({
        .NumElements = 1024 * 1024  * 4,
        .CreateUAV = true,
        .Name = "Shader Debug Print Buffer",
    });

    for(ReadbackBuffer& buffer : PrintReadbackBuffers)
        buffer.Initialize(PrintBuffer.InternalBuffer.Size);

    LineBuffer.Initialize({
        .NumElements = (sizeof(DrawArgs) + sizeof(DebugLine) * MaxLines),
        .CreateUAV = true,
        .Name = "Shader Debug Line Buffer",
    });

    SphereBuffer.Initialize({
        .NumElements = (sizeof(DrawIndexedArgs) + sizeof(DebugSphere) * MaxSpheres),
        .CreateUAV = true,
        .Name = "Shader Debug Sphere Buffer",
    });

    BuildSphereData();

    const std::string shaderPath = SampleFrameworkDir() + "Shaders\\ShaderDebugDraw.hlsl";
    InitDrawBuffersCS = CompileFromFile(shaderPath.c_str(), "InitDrawBuffersCS", ShaderType::Compute);
    DebugDrawLineVS = CompileFromFile(shaderPath.c_str(), "DebugDrawLineVS", ShaderType::Vertex);
    DebugDrawSphereVS = CompileFromFile(shaderPath.c_str(), "DebugDrawSphereVS", ShaderType::Vertex);
    DebugDrawPS = CompileFromFile(shaderPath.c_str(), "DebugDrawPS", ShaderType::Pixel);

    {
        D3D12_INDIRECT_ARGUMENT_DESC indirectArgDesc = { .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW };
        D3D12_COMMAND_SIGNATURE_DESC cmdSigDesc =
        {
            .ByteStride = sizeof(D3D12_DRAW_ARGUMENTS),
            .NumArgumentDescs = 1,
            .pArgumentDescs = &indirectArgDesc,
        };
        DXCall(DX12::Device->CreateCommandSignature(&cmdSigDesc, nullptr, IID_PPV_ARGS(&DrawCommandSignature)));
    }

    {
        D3D12_INDIRECT_ARGUMENT_DESC indirectArgDesc = { .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED };
        D3D12_COMMAND_SIGNATURE_DESC cmdSigDesc =
        {
            .ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS),
            .NumArgumentDescs = 1,
            .pArgumentDescs = &indirectArgDesc,
        };
        DXCall(DX12::Device->CreateCommandSignature(&cmdSigDesc, nullptr, IID_PPV_ARGS(&DrawIndexedCommandSignature)));
    }
}

void Shutdown()
{
    DebugInfoBuffer.Shutdown();
    PrintBuffer.Shutdown();
    for(ReadbackBuffer& buffer : PrintReadbackBuffers)
        buffer.Shutdown();
    LineBuffer.Shutdown();
    SphereBuffer.Shutdown();
    SphereVertexBuffer.Shutdown();
    SphereIndexBuffer.Shutdown();
    DX12::Release(DrawCommandSignature);
    DX12::Release(DrawIndexedCommandSignature);
}

void CreatePSOs(DXGI_FORMAT rtFormat, DXGI_FORMAT dsvFormat)
{
    InitDrawBuffersPSO = DX12::CreateComputePSO(InitDrawBuffersCS);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = DX12::UniversalRootSignature;
    psoDesc.VS = DebugDrawLineVS.ByteCode();
    psoDesc.PS = DebugDrawPS.ByteCode();
    psoDesc.RasterizerState = DX12::GetRasterizerState(RasterizerState::NoCull);
    psoDesc.BlendState = DX12::GetBlendState(BlendState::AlphaBlend);
    psoDesc.DepthStencilState = DX12::GetDepthState(DepthState::Enabled);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = rtFormat;
    psoDesc.DSVFormat = dsvFormat;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&LinePSO)));

    psoDesc.VS = DebugDrawSphereVS.ByteCode();
    DXCall(DX12::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&SpherePSO)));
}

void DestroyPSOs()
{
    DX12::DeferredRelease(InitDrawBuffersPSO);
    DX12::DeferredRelease(LinePSO);
    DX12::DeferredRelease(SpherePSO);
}

struct DebugPrintReader
{
    const uint8_t* PrintBufferData = nullptr;
    uint32_t BufferSize = 0;
    uint32_t TotalNumBytes = 0;
    uint32_t CurrOffset = 0;

    DebugPrintReader(const uint8_t* printBufferData, uint32_t bufferSize)
    {
        PrintBufferData = printBufferData;
        BufferSize = bufferSize;
        memcpy(&TotalNumBytes, printBufferData, sizeof(uint32_t));
        TotalNumBytes += sizeof(uint32_t);
        TotalNumBytes = Min(TotalNumBytes, BufferSize);
        CurrOffset = sizeof(uint32_t);
    }

    template<typename T> T Consume(T defVal = T())
    {
        T x;

        if(CurrOffset + sizeof(T) > TotalNumBytes)
            return defVal;

        memcpy(&x, PrintBufferData + CurrOffset, sizeof(T));
        CurrOffset += sizeof(T);
        return x;
    }

    const char* ConsumeString(uint32_t expectedStringSize)
    {
        if(expectedStringSize == 0)
            return "";

        if(CurrOffset + expectedStringSize > TotalNumBytes)
            return "";

        const char* stringData = reinterpret_cast<const char*>(PrintBufferData + CurrOffset);
        CurrOffset += expectedStringSize;
        if(stringData[expectedStringSize - 1] != '\0')
            return "";

        return stringData;
    }

    bool HasMoreData(uint32_t numBytes) const
    {
        return (CurrOffset + numBytes) <= TotalNumBytes;
    }
};

static std::string MakeArgString(DebugPrintReader& printReader, ArgCode argCode)
{
    if(argCode == DebugPrint_Uint)
    {
        return MakeString("%u", printReader.Consume<uint32_t>());
    }
    if(argCode == DebugPrint_Uint2)
    {
        const Uint2 x = printReader.Consume<Uint2>();
        return MakeString("(%u, %u)", x.x, x.y);
    }
    if(argCode == DebugPrint_Uint3)
    {
        const Uint3 x = printReader.Consume<Uint3>();
        return MakeString("(%u, %u, %u)", x.x, x.y, x.z);
    }
    if(argCode == DebugPrint_Uint4)
    {
        const Uint4 x = printReader.Consume<Uint4>();
        return MakeString("(%u, %u, %u, %u)", x.x, x.y, x.z, x.w);
    }
    if(argCode == DebugPrint_Int)
    {
        return MakeString("%i", printReader.Consume<int32_t>());
    }
    if(argCode == DebugPrint_Int2)
    {
        const Int2 x = printReader.Consume<Int2>();
        return MakeString("(%i, %i)", x.x, x.y);
    }
    if(argCode == DebugPrint_Int3)
    {
        const Int3 x = printReader.Consume<Int3>();
        return MakeString("(%i, %i, %i)", x.x, x.y, x.z);
    }
    if(argCode == DebugPrint_Int4)
    {
        const Int4 x = printReader.Consume<Int4>();
        return MakeString("(%i, %i, %i, %i)", x.x, x.y, x.z, x.w);
    }
    if(argCode == DebugPrint_Float)
    {
        return MakeString("%f", printReader.Consume<float>());
    }
    if(argCode == DebugPrint_Float2)
    {
        const Float2 x = printReader.Consume<Float2>();
        return MakeString("(%f, %f)", x.x, x.y);
    }
    if(argCode == DebugPrint_Float3)
    {
        const Float3 x = printReader.Consume<Float3>();
        return MakeString("(%f, %f, %f)", x.x, x.y, x.z);
    }
    if(argCode == DebugPrint_Float4)
    {
        const Float4 x = printReader.Consume<Float4>();
        return MakeString("(%f, %f, %f, %f)", x.x, x.y, x.z, x.w);
    }

    Assert_(false);
    return "???";
}

static void ProcessDebugPrints()
{
    List<std::string> argStrings;

    if(DX12::CurrentCPUFrame >= DX12::RenderLatency)
    {
        const ReadbackBuffer& readbackBuffer = PrintReadbackBuffers[(DX12::CurrentCPUFrame + 1) % DX12::RenderLatency];
        DebugPrintReader printReader(readbackBuffer.Map<uint8_t>(), uint32_t(readbackBuffer.Size));
        while(printReader.HasMoreData(sizeof(DebugPrintHeader)))
        {
            const DebugPrintHeader header = printReader.Consume<DebugPrintHeader>(DebugPrintHeader{});
            if(header.NumBytes == 0 || printReader.HasMoreData(header.NumBytes) == false)
                break;

            std::string formatStr = printReader.ConsumeString(header.StringSize);
            if(formatStr.length() == 0)
                break;

            if(header.NumArgs > MaxDebugPrintArgs)
                break;

            argStrings.Reserve(header.NumArgs);
            for(uint32_t argIdx = 0; argIdx < header.NumArgs; ++argIdx)
            {

                const ArgCode argCode = (ArgCode)printReader.Consume<uint8_t>(0xFF);
                if(argCode >= NumDebugPrintArgCodes)
                    break;

                const uint32_t argSize = ArgCodeSizes[argCode];
                if(printReader.HasMoreData(argSize) == false)
                    break;

                const std::string argStr = MakeArgString(printReader, argCode);
                ReplaceStringInPlace(formatStr, ArgPlaceHolders[argIdx], argStr);
            }

            GlobalApp->AddToGPULog(formatStr.c_str());

            printReader.CurrOffset = AlignTo(printReader.CurrOffset, 4);
        }

        readbackBuffer.Unmap();
    }
};

void BeginRender(ID3D12GraphicsCommandList7* cmdList, uint32_t cursorX, uint32_t cursorY)
{
    PIXMarker marker(cmdList, "ShaderDebug - BeginRender");

    DebugInfo debugInfo =
    {
        .CursorXY = { cursorX, cursorY },
        .PrintBuffer = PrintBuffer.UAV,
        .LineBuffer = LineBuffer.UAV,
        .SphereBuffer = SphereBuffer.UAV,
        .PrintBufferSize = uint32_t(PrintBuffer.InternalBuffer.Size),
        .MaxLines = MaxLines,
        .MaxSpheres = MaxSpheres,

    };
    DebugInfoBuffer.MapAndSetData(&debugInfo, sizeof(debugInfo) / 4);

    ProcessDebugPrints();

    DX12::ClearRawBuffer(cmdList, PrintBuffer, Uint4(0, 0, 0, 0));

    {
        // Init the debug draw buffers
        InitDrawBuffersConstants constants =
        {
            .NumSphereIndices = uint32_t(SphereIndexBuffer.NumElements),
        };
        DX12::BindTempConstantBuffer(cmdList, constants, URS_ConstantBuffers + 0, CmdListMode::Compute);

        cmdList->SetComputeRootSignature(DX12::UniversalRootSignature);

        cmdList->SetPipelineState(InitDrawBuffersPSO);
        cmdList->Dispatch(1, 1, 1);
    }

    D3D12_GLOBAL_BARRIER globalBarrier =
    {
        .SyncBefore = D3D12_BARRIER_SYNC_COMPUTE_SHADING,
        .SyncAfter = D3D12_BARRIER_SYNC_ALL_SHADING,
        .AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
        .AccessAfter = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
    };
    DX12::Barrier(cmdList, globalBarrier);
}

static void RenderDebugDraws(ID3D12GraphicsCommandList7* cmdList, const Float4x4& viewProjection)
{
    DebugDrawConstants constants =
    {
        .ViewProjection = viewProjection,
        .SphereVertexBuffer = SphereVertexBuffer.SRV,
    };
    DX12::BindTempConstantBuffer(cmdList, constants, URS_ConstantBuffers + 0, CmdListMode::Graphics);

    cmdList->SetGraphicsRootSignature(DX12::UniversalRootSignature);
    cmdList->SetPipelineState(LinePSO);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    cmdList->ExecuteIndirect(DrawCommandSignature, 1, LineBuffer.Resource(), sizeof(D3D12_DRAW_ARGUMENTS) * 0, nullptr, 0);

    const D3D12_INDEX_BUFFER_VIEW ibView = SphereIndexBuffer.IBView();

    cmdList->SetPipelineState(SpherePSO);
    cmdList->IASetIndexBuffer(&ibView);
    cmdList->ExecuteIndirect(DrawIndexedCommandSignature, 1, SphereBuffer.Resource(), sizeof(D3D12_DRAW_INDEXED_ARGUMENTS) * 0, nullptr, 0);
}

void EndRender(ID3D12GraphicsCommandList7* cmdList, const Float4x4& viewProjection)
{
    PIXMarker marker(cmdList, "ShaderDebug - EndRender");

    D3D12_GLOBAL_BARRIER globalBarrier =
    {
        .SyncBefore = D3D12_BARRIER_SYNC_COMPUTE_SHADING,
        .SyncAfter = D3D12_BARRIER_SYNC_ALL_SHADING,
        .AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
        .AccessAfter = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
    };
    DX12::Barrier(cmdList, globalBarrier);

    RenderDebugDraws(cmdList, viewProjection);

    {
        // Copy the print buffer to a readback buffer
        DX12::Barrier(cmdList, PrintBuffer.InternalBuffer.WriteToReadBarrier( { .SyncAfter = D3D12_BARRIER_SYNC_COPY, .AccessAfter = D3D12_BARRIER_ACCESS_COPY_SOURCE } ));
        const ReadbackBuffer& readbackBuffer = PrintReadbackBuffers[DX12::CurrentCPUFrame % DX12::RenderLatency];
        cmdList->CopyResource(readbackBuffer.Resource, PrintBuffer.Resource());
    }
}

}

}