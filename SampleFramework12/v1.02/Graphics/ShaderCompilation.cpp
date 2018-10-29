//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"

#include "ShaderCompilation.h"
#include "DX12.h"

#include "..\\Utility.h"
#include "..\\Exceptions.h"
#include "..\\InterfacePointers.h"
#include "..\\FileIO.h"
#include "..\\MurmurHash.h"
#include "..\\Containers.h"

using std::vector;
using std::wstring;
using std::string;
using std::map;

namespace SampleFramework12
{

static const uint64 CacheVersion = 0;

static const char* TypeStrings[] = { "vertex", "hull", "domain", "geometry", "pixel", "compute", "lib" };
StaticAssert_(ArraySize_(TypeStrings) == uint64(ShaderType::NumTypes));

static const char* ProfileStrings[] =
{
    #if EnableDXC_
        "vs_6_1", "hs_6_1", "ds_6_1", "gs_6_1", "ps_6_1", "cs_6_1", "lib_6_3",
    #else
        "vs_5_1", "hs_5_1", "ds_5_1", "gs_5_1", "ps_5_1", "cs_5_1", "<invalid>",
    #endif
};

StaticAssert_(ArraySize_(ProfileStrings) == uint64(ShaderType::NumTypes));

static Hash MakeCompilerHash()
{
    #if EnableDXC_
        HMODULE module = LoadLibrary(L"dxcompiler.dll");
    #else
        HMODULE module = LoadLibrary(L"d3dcompiler_47.dll");
    #endif

    if(module == nullptr)
        throw Exception(L"Failed to load compiler DLL");

    wchar dllPath[1024] = { };
    GetModuleFileName(module, dllPath, ArraySize_(dllPath));

    File dllFile(dllPath, FileOpenMode::Read);
    uint64 fileSize = dllFile.Size();
    Array<uint8> fileData(fileSize);
    dllFile.Read(fileSize, fileData.Data());

    return GenerateHash(fileData.Data(), int32(fileSize));
}

static Hash CompilerHash = MakeCompilerHash();

static string GetExpandedShaderCode(const wchar* path, GrowableList<wstring>& filePaths)
{
    for(uint64 i = 0; i < filePaths.Count(); ++i)
        if(filePaths[i] == path)
            return string();

    filePaths.Add(path);

    string fileContents = ReadFileAsString(path);

    wstring fileDirectory = GetDirectoryFromFilePath(path);
    if(fileDirectory.length() > 0)
        fileDirectory += L"\\";

    // Look for includes
    size_t lineStart = 0;
    while(true)
    {
        size_t lineEnd = fileContents.find('\n', lineStart);
        size_t lineLength = 0;
        if(lineEnd == string::npos)
            lineLength = string::npos;
        else
            lineLength = lineEnd - lineStart;

        string line = fileContents.substr(lineStart, lineLength);
        if(line.find("#include") == 0)
        {
            wstring fullIncludePath;
            size_t startQuote = line.find('\"');
            if(startQuote != -1)
            {
                size_t endQuote = line.find('\"', startQuote + 1);
                string includePath = line.substr(startQuote + 1, endQuote - startQuote - 1);
                fullIncludePath = fileDirectory + AnsiToWString(includePath.c_str());
            }
            else
            {
                startQuote = line.find('<');
                if(startQuote == -1)
                    throw Exception(L"Malformed include statement: \"" + AnsiToWString(line.c_str()) + L"\" in file " + path);
                size_t endQuote = line.find('>', startQuote + 1);
                string includePath = line.substr(startQuote + 1, endQuote - startQuote - 1);
                fullIncludePath = SampleFrameworkDir() + L"Shaders\\" + AnsiToWString(includePath.c_str());
            }

            if(FileExists(fullIncludePath.c_str()) == false)
                throw Exception(L"Couldn't find #included file \"" + fullIncludePath + L"\" in file " + path);

            string includeCode = GetExpandedShaderCode(fullIncludePath.c_str(), filePaths);
            fileContents.insert(lineEnd + 1, includeCode);
            lineEnd += includeCode.length();
        }

        if(lineEnd == string::npos)
            break;

        lineStart = lineEnd + 1;
    }

    return fileContents;
}

static const wstring baseCacheDir = L"ShaderCache\\";

#if _DEBUG
    static const wstring cacheSubDir = L"Debug\\";
#else
    static const std::wstring cacheSubDir = L"Release\\";
#endif

static const wstring cacheDir = baseCacheDir + cacheSubDir;

static string MakeDefinesString(const D3D_SHADER_MACRO* defines)
{
    string definesString = "";
    while(defines && defines->Name != nullptr && defines != nullptr)
    {
        if(definesString.length() > 0)
            definesString += "|";
        definesString += defines->Name;
        definesString += "=";
        definesString += defines->Definition;
        ++defines;
    }

    return definesString;
}

static wstring MakeShaderCacheName(const std::string& shaderCode, const char* functionName,
                                   const char* profile, const D3D_SHADER_MACRO* defines)
{
    string hashString = shaderCode;
    hashString += "\n";
    if(functionName != nullptr)
    {
        hashString += functionName;
        hashString += "\n";
    }
    hashString += profile;
    hashString += "\n";

    hashString += MakeDefinesString(defines);

    hashString += ToAnsiString(CacheVersion);

    Hash codeHash = GenerateHash(hashString.data(), int(hashString.length()), 0);
    codeHash = CombineHashes(codeHash, CompilerHash);

    return cacheDir + codeHash.ToString() + L".cache";
}

class FrameworkInclude : public ID3DInclude
{
    HRESULT Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes) override
    {
        std::wstring filePath;
        if(IncludeType == D3D_INCLUDE_LOCAL)
            filePath = AnsiToWString(pFileName);
        else if(IncludeType == D3D_INCLUDE_SYSTEM)
            filePath = SampleFrameworkDir() + L"Shaders\\" + AnsiToWString(pFileName);
        else
            return E_FAIL;

        if(FileExists(filePath.c_str()) == false)
            return E_FAIL;
        File file(filePath.c_str(), FileOpenMode::Read);
        *pBytes = UINT(file.Size());
        uint8* data = reinterpret_cast<uint8*>(std::malloc(*pBytes));
        file.Read(*pBytes, data);
        *ppData = data;
        return S_OK;
    }

    HRESULT Close(LPCVOID pData) override
    {
        std::free(const_cast<void*>(pData));
        return S_OK;
    }
};

#if EnableDXC_

struct Blob : public ID3DBlob
{
    Array<uint8> Data;
    uint32 RefCount = 1;

    void* GetBufferPointer() override
    {
        return Data.Data();
    }

    size_t GetBufferSize() override
    {
        return Data.MemorySize();
    }

    ULONG AddRef() override
    {
        return ++RefCount;
    }

    ULONG Release() override
    {
        if(RefCount == 1)
        {
            delete this;
            return 0;
        }
        else if(RefCount > 1)
        {
            return --RefCount;
        }
        else
        {
            Assert_(false);
            return 0;
        }
    }

    HRESULT QueryInterface(REFIID riid, void** ppvObject) override
    {
        return E_FAIL;
    }
};

static HRESULT CompileShaderDXC(const wchar* path, const D3D_SHADER_MACRO* defines, const char* functionName,
                                const char* profileString, ID3DBlob** compiledShader, ID3DBlob** errorMessages)
{
    IDxcLibrary* library = nullptr;
    DXCall(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library)));

    IDxcBlobEncoding* sourceCode = nullptr;
    DXCall(library->CreateBlobFromFile(path, nullptr, &sourceCode));

    IDxcCompiler* compiler = nullptr;
    DXCall(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)));

    // Convert the defines to wide strings
    uint64 numDefines = 0;
    while(defines && defines[numDefines].Name)
        ++numDefines;

    const uint64 extraDefines = 2;
    uint64 totalNumDefines = numDefines + extraDefines;

    Array<DxcDefine> dxcDefines(totalNumDefines);
    Array<wstring> defineStrings(numDefines * 2);
    for(uint64 i = 0; i < numDefines; ++i)
    {
        defineStrings[i * 2 + 0] = AnsiToWString(defines[i].Name);
        defineStrings[i * 2 + 1] = AnsiToWString(defines[i].Definition);
        dxcDefines[i].Name = defineStrings[i * 2 + 0].c_str();
        dxcDefines[i].Value = defineStrings[i * 2 + 1].c_str();
    }

    dxcDefines[numDefines + 0].Name = L"DXC_";
    dxcDefines[numDefines + 0].Value = L"1";
    dxcDefines[numDefines + 1].Name = L"SM60_";
    dxcDefines[numDefines + 1].Value = L"1";

    wstring frameworkShaderDir = SampleFrameworkDir() + L"Shaders";
    wchar expandedFrameworkShaderDir[1024] = { };
    GetFullPathName(frameworkShaderDir.c_str(), ArraySize_(expandedFrameworkShaderDir), expandedFrameworkShaderDir, nullptr);

    const wchar* arguments[] =
    {
        L"/O3",
        L"-all_resources_bound",
        L"-WX",
        L"-I",
        expandedFrameworkShaderDir,

        // LLVM debug info seems to make the RT PSO croak
        #if Debug_ && !EnableDXR_
            L"/Zi",
        #endif
    };

    IDxcIncludeHandler* includeHandler = nullptr;
    DXCall(library->CreateIncludeHandler(&includeHandler));

    IDxcOperationResult* operationResult = nullptr;
    DXCall(compiler->Compile(sourceCode, path, functionName ? AnsiToWString(functionName).c_str() : L"",
                             AnsiToWString(profileString).c_str(), arguments,
                             ArraySize_(arguments), dxcDefines.Data(), uint32(dxcDefines.Size()),
                             includeHandler, &operationResult));

    HRESULT hr = S_OK;
    operationResult->GetStatus(&hr);
    if(SUCCEEDED(hr))
        DXCall(operationResult->GetResult(reinterpret_cast<IDxcBlob**>(compiledShader)));
    else
        operationResult->GetErrorBuffer(reinterpret_cast<IDxcBlobEncoding**>(errorMessages));

    DX12::Release(operationResult);
    DX12::Release(includeHandler);
    DX12::Release(compiler);
    DX12::Release(sourceCode);
    DX12::Release(library);

    return hr;
}

#endif // EnableDXC_

static ID3DBlob* CompileShader(const wchar* path, const char* functionName, ShaderType type,
                               const D3D_SHADER_MACRO* defines, GrowableList<wstring>& filePaths)
{
    if(FileExists(path) == false)
    {
        Assert_(false);
        throw Exception(L"Shader file " + std::wstring(path) + L" does not exist");
    }

    uint64 profileIdx = uint64(type);
    Assert_(profileIdx < ArraySize_(ProfileStrings));
    const char* profileString = ProfileStrings[profileIdx];

    // Make a hash off the expanded shader code
    string shaderCode = GetExpandedShaderCode(path, filePaths);
    wstring cacheName = MakeShaderCacheName(shaderCode, functionName, profileString, defines);

    if(FileExists(cacheName.c_str()))
    {
        File cacheFile(cacheName.c_str(), FileOpenMode::Read);
        const uint64 shaderSize = cacheFile.Size();

        #if EnableDXC_
            Blob* shaderBlob = new Blob();
            shaderBlob->Data.Init(shaderSize);
            cacheFile.Read(shaderSize, shaderBlob->Data.Data());
            return shaderBlob;
        #else
            Array<uint8> compressedShader;
            compressedShader.Init(shaderSize);
            cacheFile.Read(shaderSize, compressedShader.Data());

            ID3DBlob* decompressedShader[1] = { nullptr };
            uint32 indices[1] = { 0 };
            DXCall(D3DDecompressShaders(compressedShader.Data(), shaderSize, 1, 0,
                                        indices, 0, decompressedShader, nullptr));

            return decompressedShader[0];
        #endif
    }

    if(type == ShaderType::Library)
    {
        WriteLog("Compiling shader library %s %s\n", WStringToAnsi(GetFileName(path).c_str()).c_str(),
                 MakeDefinesString(defines).c_str());
    }
    else
    {
        WriteLog("Compiling %s shader %s_%s %s\n", TypeStrings[uint64(type)],
                 WStringToAnsi(GetFileName(path).c_str()).c_str(),
                 functionName, MakeDefinesString(defines).c_str());
    }

    // Loop until we succeed, or an exception is thrown
    while(true)
    {
        ID3DBlob* compiledShader = nullptr;
        ID3DBlobPtr errorMessages;

        #if EnableDXC_
            HRESULT hr = CompileShaderDXC(path, defines, functionName, profileString, &compiledShader, &errorMessages);
        #else
            UINT flags = D3DCOMPILE_WARNINGS_ARE_ERRORS;
            flags |= D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES;
            flags |= D3DCOMPILE_ALL_RESOURCES_BOUND;
            #ifdef _DEBUG
                flags |= D3DCOMPILE_DEBUG;
            #endif

            FrameworkInclude include;
            HRESULT hr = D3DCompileFromFile(path, defines, &include, functionName,
                                            profileString, flags, 0, &compiledShader, &errorMessages);
        #endif

        if(FAILED(hr))
        {
            if(errorMessages)
            {
                wchar message[1024] = { 0 };
                char* blobdata = reinterpret_cast<char*>(errorMessages->GetBufferPointer());

                MultiByteToWideChar(CP_ACP, 0, blobdata, static_cast<int>(errorMessages->GetBufferSize()), message, 1024);
                std::wstring fullMessage = L"Error compiling shader file \"";
                fullMessage += path;
                fullMessage += L"\" - ";
                fullMessage += message;

                // Pop up a message box allowing user to retry compilation
                int retVal = MessageBoxW(nullptr, fullMessage.c_str(), L"Shader Compilation Error", MB_RETRYCANCEL);
                if(retVal != IDRETRY)
                    throw DXException(hr, fullMessage.c_str());
            }
            else
            {
                Assert_(false);
                throw DXException(hr);
            }
        }
        else
        {
            ID3DBlobPtr compressedShader;

            #if EnableDXC_
                compressedShader = compiledShader;
            #else
                // Compress the shader
                D3D_SHADER_DATA shaderData;
                shaderData.pBytecode = compiledShader->GetBufferPointer();
                shaderData.BytecodeLength = compiledShader->GetBufferSize();
                DXCall(D3DCompressShaders(1, &shaderData, D3D_COMPRESS_SHADER_KEEP_ALL_PARTS, &compressedShader));
            #endif

            // Create the cache directory if it doesn't exist
            if(DirectoryExists(baseCacheDir.c_str()) == false)
                Win32Call(CreateDirectory(baseCacheDir.c_str(), nullptr));

            if(DirectoryExists(cacheDir.c_str()) == false)
                Win32Call(CreateDirectory(cacheDir.c_str(), nullptr));

            File cacheFile(cacheName.c_str(), FileOpenMode::Write);

            // Write the compiled shader to disk
            uint64 shaderSize = compressedShader->GetBufferSize();
            cacheFile.Write(shaderSize, compressedShader->GetBufferPointer());

            return compiledShader;
        }
    }
}

struct ShaderFile
{
    wstring FilePath;
    uint64 TimeStamp;
    GrowableList<CompiledShader*> Shaders;

    ShaderFile(const wstring& filePath) : TimeStamp(0), FilePath(filePath)
    {
    }
};

static GrowableList<ShaderFile*> ShaderFiles;
static GrowableList<CompiledShader*> CompiledShaders;
static SRWLOCK ShaderFilesLock = SRWLOCK_INIT;
static SRWLOCK CompiledShadersLock = SRWLOCK_INIT;

static void CompileShader(CompiledShader* shader)
{
    Assert_(shader != nullptr);

    const char* functionName = shader->Type != ShaderType::Library ? shader->FunctionName.c_str() : nullptr;

    GrowableList<wstring> filePaths;
    D3D_SHADER_MACRO defines[CompileOptions::MaxDefines + 1];
    shader->CompileOpts.MakeDefines(defines);
    shader->ByteCode = CompileShader(shader->FilePath.c_str(), functionName, shader->Type, defines, filePaths);
    shader->ByteCodeHash = GenerateHash(shader->ByteCode->GetBufferPointer(), int(shader->ByteCode->GetBufferSize()));

    for(uint64 fileIdx = 0; fileIdx < filePaths.Count(); ++ fileIdx)
    {
        const wstring& filePath = filePaths[fileIdx];
        ShaderFile* shaderFile = nullptr;
        const uint64 numShaderFiles = ShaderFiles.Count();
        for(uint64 shaderFileIdx = 0; shaderFileIdx < numShaderFiles; ++shaderFileIdx)
        {
            if(ShaderFiles[shaderFileIdx]->FilePath == filePath)
            {
                shaderFile = ShaderFiles[shaderFileIdx];
                break;
            }
        }
        if(shaderFile == nullptr)
        {
            shaderFile = new ShaderFile(filePath);

            AcquireSRWLockExclusive(&ShaderFilesLock);

            ShaderFiles.Add(shaderFile);

            ReleaseSRWLockExclusive(&ShaderFilesLock);
        }

        bool containsShader = false;
        for(uint64 shaderIdx = 0; shaderIdx < shaderFile->Shaders.Count(); ++shaderIdx)
        {
            if(shaderFile->Shaders[shaderIdx] == shader)
            {
                containsShader = true;
                break;
            }
        }

        if(containsShader == false)
            shaderFile->Shaders.Add(shader);
    }
}

CompiledShaderPtr CompileFromFile(const wchar* path, const char* functionName,
                                  ShaderType type, const CompileOptions& compileOpts)
{
    if(type == ShaderType::Library)
    {
        Assert_(functionName == nullptr);
    }

    CompiledShader* compiledShader = new CompiledShader(path, functionName, compileOpts, type);
    CompileShader(compiledShader);

    AcquireSRWLockExclusive(&CompiledShadersLock);

    CompiledShaders.Add(compiledShader);

    ReleaseSRWLockExclusive(&CompiledShadersLock);

    return compiledShader;
}

bool UpdateShaders(bool updateAll)
{
    uint64 numShaderFiles = ShaderFiles.Count();
    if(numShaderFiles == 0)
        return false;

    static uint64 currFile = 0;

    const uint64 numShadersToCheck = updateAll ? numShaderFiles : 1;
    bool shaderChanged = false;

    for(uint64 i = 0; i < numShadersToCheck; ++i)
    {
        currFile = (currFile + 1) % uint64(numShaderFiles);

        ShaderFile* file = ShaderFiles[currFile];
        const uint64 newTimeStamp = GetFileTimestamp(file->FilePath.c_str());
        if(file->TimeStamp == 0)
        {
            file->TimeStamp = newTimeStamp;
            return false;
        }

        if(file->TimeStamp < newTimeStamp)
        {
            WriteLog("Hot-swapping shaders for %ls\n", file->FilePath.c_str());
            file->TimeStamp = newTimeStamp;
            for(uint64 fileIdx = 0; fileIdx < file->Shaders.Count(); ++fileIdx)
            {
                // Retry a few times to avoid file conflicts with text editors
                const uint64 NumRetries = 10;
                for(uint64 retryCount = 0; retryCount < NumRetries; ++retryCount)
                {
                    try
                    {
                        CompiledShader* shader = file->Shaders[fileIdx];
                        CompileShader(shader);
                        break;
                    }
                    catch(Win32Exception& exception)
                    {
                        if(retryCount == NumRetries - 1)
                            throw exception;
                        Sleep(15);
                    }
                }
            }

            shaderChanged = true;
        }
    }

    return shaderChanged;
}

void ShutdownShaders()
{
    for(uint64 i = 0; i < ShaderFiles.Count(); ++i)
        delete ShaderFiles[i];

    for(uint64 i = 0; i < CompiledShaders.Count(); ++i)
        delete CompiledShaders[i];
}

// == CompileOptions ==============================================================================

CompileOptions::CompileOptions()
{
    Reset();
}

void CompileOptions::Add(const std::string& name, uint32 value)
{
    Assert_(numDefines < MaxDefines);

    nameOffsets[numDefines] = bufferIdx;
    for(uint32 i = 0; i < name.length(); ++i)
        buffer[bufferIdx++] = name[i];
    ++bufferIdx;

    std::string stringVal = ToAnsiString(value);
    defineOffsets[numDefines] = bufferIdx;
    for(uint32 i = 0; i < stringVal.length(); ++i)
        buffer[bufferIdx++] = stringVal[i];
    ++bufferIdx;

    ++numDefines;
}

void CompileOptions::Reset()
{
    numDefines = 0;
    bufferIdx = 0;

    for(uint32 i = 0; i < MaxDefines; ++i)
    {
        nameOffsets[i] = 0xFFFFFFFF;
        defineOffsets[i] = 0xFFFFFFFF;
    }

    ZeroMemory(buffer, BufferSize);
}

void CompileOptions::MakeDefines(D3D_SHADER_MACRO defines[MaxDefines + 1]) const
{
    for(uint32 i = 0; i < numDefines; ++i)
    {
        defines[i].Name = buffer + nameOffsets[i];
        defines[i].Definition = buffer + defineOffsets[i];
    }

    defines[numDefines].Name = nullptr;
    defines[numDefines].Definition = nullptr;
}

}