//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "..\\PCH.h"

#include "..\\InterfacePointers.h"
#include "..\\Assert.h"
#include "..\\MurmurHash.h"

namespace SampleFramework12
{

class CompileOptions
{
public:

    // constants
    static const uint32 MaxDefines = 16;
    static const uint32 BufferSize = 1024;

    CompileOptions();

    void Add(const std::string& name, uint32 value);
    void Reset();

    void MakeDefines(D3D_SHADER_MACRO defines[MaxDefines + 1]) const;

private:

    uint32 nameOffsets[MaxDefines];
    uint32 defineOffsets[MaxDefines];
    char buffer[BufferSize];
    uint32 numDefines;
    uint32 bufferIdx;
};

enum class ShaderType
{
    Vertex = 0,
    Hull,
    Domain,
    Geometry,
    Pixel,
    Compute,
    Library,

    NumTypes
};

class CompiledShader
{

public:

    std::wstring FilePath;
    std::string FunctionName;
    CompileOptions CompileOpts;
    ID3DBlobPtr ByteCode;
    ShaderType Type;
    Hash ByteCodeHash;

    CompiledShader(const wchar* filePath, const char* functionName,
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
        byteCode.pShaderBytecode = ptr->ByteCode->GetBufferPointer();
        byteCode.BytecodeLength = ptr->ByteCode->GetBufferSize();
        return byteCode;
    }

private:

    const CompiledShader* ptr;
};

typedef CompiledShaderPtr ShaderPtr;

// Compiles a shader from file and loads the compiled shader binary
CompiledShaderPtr CompileFromFile(const wchar* path, const char* functionName, ShaderType type,
                                  const CompileOptions& compileOpts = CompileOptions());

bool UpdateShaders(bool updateAll);
void ShutdownShaders();

}
