//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "..\\PCH.h"

#include "..\\InterfacePointers.h"
#include "..\\SF12_Assert.h"
#include "..\\MurmurHash.h"
#include "..\\Containers.h"

namespace SampleFramework12
{

struct CompileOption
{
    const char* Name = "";
    const int32_t Value = 0;
};

class CompileOptions
{
public:

    CompileOptions();
    CompileOptions(const char* name, int32_t value);

    void Add(const char* name, int32_t value);
    void Add(CompileOption option);
    void Reset();

    std::string MakeDefinesString() const;

    List<CompileOption> Options;
};

enum class ShaderType
{
    Vertex = 0,
    Hull,
    Domain,
    Geometry,
    Amplification,
    Mesh,
    Pixel,
    Compute,
    Library,

    NumTypes
};

class CompiledShader
{

public:

    std::string FilePath;
    std::string FunctionName;
    CompileOptions CompileOpts;
    Array<uint8_t> ByteCode;
    ShaderType Type;
    Hash ByteCodeHash;
    bool IncludesAppSettings = false;

    CompiledShader(const char* filePath, const char* functionName,
                   const CompileOptions& compileOptions, ShaderType type) : FilePath(filePath),
                                                                            CompileOpts(compileOptions),
                                                                            Type(type)
    {
        if(functionName != nullptr)
            FunctionName = functionName;
    }
};

class CompiledShaderPtr
{
public:

    CompiledShaderPtr() : ptr(nullptr)
    {
    }

    CompiledShaderPtr(const CompiledShader* ptr_) : ptr(ptr_)
    {
    }

    const CompiledShader* operator->() const
    {
        Assert_(ptr != nullptr);
        return ptr;
    }

    const CompiledShader& operator*() const
    {
        Assert_(ptr != nullptr);
        return *ptr;
    }

    bool Valid() const
    {
        return ptr != nullptr;
    }

    D3D12_SHADER_BYTECODE ByteCode() const
    {
        Assert_(ptr != nullptr);
        D3D12_SHADER_BYTECODE byteCode;
        byteCode.pShaderBytecode = ptr->ByteCode.Data();
        byteCode.BytecodeLength = ptr->ByteCode.Size();
        return byteCode;
    }

private:

    const CompiledShader* ptr;
};

typedef CompiledShaderPtr ShaderPtr;

// Compiles a shader from file and loads the compiled shader binary
CompiledShaderPtr CompileFromFile(const char* path, const char* functionName, ShaderType type,
                                  const CompileOptions& compileOpts = CompileOptions());

bool UpdateShaders(bool updateAll);
void ShutdownShaders();

}
