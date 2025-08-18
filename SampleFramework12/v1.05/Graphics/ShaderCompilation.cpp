//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"

#include "ShaderCompilation.h"
#include "DX12.h"

#include "../Utility.h"
#include "../Exceptions.h"
#include "../InterfacePointers.h"
#include "../FileIO.h"
#include "../MurmurHash.h"
#include "../Containers.h"

using std::vector;
using std::wstring;
using std::string;
using std::map;

// AppSettings framework
namespace AppSettings
{
    void GetShaderCompileOptions(SampleFramework12::CompileOptions& opts);
    bool ShaderCompileOptionsChanged();
}

namespace SampleFramework12
{

static const uint64_t CacheVersion = 1;

static const char* TypeStrings[] = { "vertex", "hull", "domain", "geometry", "amplification", "mesh", "pixel", "compute", "lib" };
StaticAssert_(ArraySize_(TypeStrings) == uint64_t(ShaderType::NumTypes));

static const char* ProfileStrings[] =
{
#if EnablePreviewDX12SDK_
    "vs_6_9", "hs_6_9", "ds_6_9", "gs_6_9", "as_6_9", "ms_6_9", "ps_6_9", "cs_6_9", "lib_6_9",
#else
    "vs_6_8", "hs_6_8", "ds_6_8", "gs_6_8", "as_6_8", "ms_6_8", "ps_6_8", "cs_6_8", "lib_6_8",
#endif
};

StaticAssert_(ArraySize_(ProfileStrings) == uint64_t(ShaderType::NumTypes));

static Hash MakeCompilerHash()
{
    HMODULE module = LoadLibraryA("dxcompiler.dll");

    if(module == nullptr)
        throw Exception("Failed to load compiler DLL");

    char dllPath[1024] = { };
    GetModuleFileNameA(module, dllPath, ArraySize_(dllPath));

    File dllFile(dllPath, FileOpenMode::Read);
    uint64_t fileSize = dllFile.Size();
    Array<uint8_t> fileData(fileSize);
    dllFile.Read(fileSize, fileData.Data());

    return GenerateHash(fileData.Data(), int32_t(fileSize));
}

static Hash CompilerHash = MakeCompilerHash();

static string GetExpandedShaderCode(const char* path, List<string>& filePaths)
{
    for(uint64_t i = 0; i < filePaths.Count(); ++i)
        if(filePaths[i] == path)
            return string();

    filePaths.Add(path);

    string fileContents = ReadFileAsString(path);

    string fileDirectory = GetDirectoryFromFilePath(path);
    if(fileDirectory.length() > 0)
        fileDirectory += "\\";

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
            string fullIncludePath;
            size_t startQuote = line.find('\"');
            if(startQuote != -1)
            {
                size_t endQuote = line.find('\"', startQuote + 1);
                string includePath = line.substr(startQuote + 1, endQuote - startQuote - 1);
                fullIncludePath = fileDirectory + includePath;
            }
            else
            {
                startQuote = line.find('<');
                if(startQuote == -1)
                    throw Exception("Malformed include statement: \"" + line + "\" in file " + path);
                size_t endQuote = line.find('>', startQuote + 1);
                string includePath = line.substr(startQuote + 1, endQuote - startQuote - 1);
                fullIncludePath = SampleFrameworkDir() + "Shaders\\" + includePath.c_str();
            }

            if(FileExists(fullIncludePath.c_str()) == false)
                throw Exception("Couldn't find #included file \"" + fullIncludePath + "\" in file " + path);

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

static const string baseCacheDir = "ShaderCache\\";

#if Debug_
    static const string cacheSubDir = "Debug\\";
#else
    static const std::string cacheSubDir = "Release\\";
#endif

static const string cacheDir = baseCacheDir + cacheSubDir;

static string MakeShaderCacheName(const std::string& shaderCode, const char* functionName,
                                 const char* profile, const CompileOptions& options)
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

    hashString += options.MakeDefinesString();

    hashString += MakeString("%llu", CacheVersion);

    Hash codeHash = GenerateHash(hashString.data(), int(hashString.length()), 0);
    codeHash = CombineHashes(codeHash, CompilerHash);

    return cacheDir + codeHash.ToString() + ".cache";
}

static HRESULT CompileShaderDXC(const char* path, const CompileOptions& opts, const char* functionName,
                                ShaderType shaderType, const char* profileString, IDxcBlobPtr& compiledShader,
                                IDxcBlobEncodingPtr& errorMessages)
{
    IDxcUtilsPtr utils;
    DXCall(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)));

    IDxcBlobEncodingPtr sourceCode;
    DXCall(utils->LoadFile(ToWString(path).c_str(), nullptr, &sourceCode));

    IDxcCompiler3Ptr compiler;
    DXCall(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)));

    // Convert the defines to wide strings
    const uint64_t numDefines = opts.Options.Count();
    const uint64_t extraDefines = 4;
    const uint64_t totalNumDefines = numDefines + extraDefines;

    Array<DxcDefine> dxcDefines(totalNumDefines);
    Array<wstring> defineStrings(numDefines * 2);
    for(uint64_t i = 0; i < numDefines; ++i)
    {
        defineStrings[i * 2 + 0] = ToWString(opts.Options[i].Name);
        defineStrings[i * 2 + 1] = MakeString(L"%i", opts.Options[i].Value);
        dxcDefines[i].Name = defineStrings[i * 2 + 0].c_str();
        dxcDefines[i].Value = defineStrings[i * 2 + 1].c_str();
    }

    dxcDefines[numDefines + 0].Name = L"DXC_";
    dxcDefines[numDefines + 0].Value = L"1";
    dxcDefines[numDefines + 1].Name = L"SM60_";
    dxcDefines[numDefines + 1].Value = L"1";
    dxcDefines[numDefines + 2].Name = L"HLSL_";
    dxcDefines[numDefines + 2].Value = L"1";
    dxcDefines[numDefines + 3].Name = L"Library_";
    dxcDefines[numDefines + 3].Value = shaderType == ShaderType::Library ? L"1" : L"0";

    string frameworkShaderDir = SampleFrameworkDir() + "Shaders";
    char expandedFrameworkShaderDir[1024] = { };
    GetFullPathName(frameworkShaderDir.c_str(), ArraySize_(expandedFrameworkShaderDir), expandedFrameworkShaderDir, nullptr);
    std::wstring expandedFrameworkShaderDirW = ToWString(expandedFrameworkShaderDir);

    const wchar_t* arguments[] =
    {
        L"-O3",
        L"-all_resources_bound",
        L"-WX",
        L"-HV 2021",
        L"-enable-16bit-types",
        L"-Zpr",
        L"-I",
        expandedFrameworkShaderDirW.c_str(),

        #if Debug_
            L"-Zi",
            L"-Qembed_debug",
        #endif
    };

    IDxcCompilerArgsPtr compilerArgs;
    DXCall(utils->BuildArguments(ToWString(path).c_str(), functionName ? ToWString(functionName).c_str() : L"", ToWString(profileString).c_str(), arguments, ArraySize_(arguments), dxcDefines.Data(), uint32_t(dxcDefines.Size()), &compilerArgs));

    IDxcIncludeHandlerPtr includeHandler;
    DXCall(utils->CreateDefaultIncludeHandler(&includeHandler));

    DxcBuffer sourceBuffer;
    sourceBuffer.Ptr = sourceCode->GetBufferPointer();
    sourceBuffer.Size = sourceCode->GetBufferSize();
    sourceBuffer.Encoding = 0;

    IDxcResultPtr operationResult;
    DXCall(compiler->Compile(&sourceBuffer, compilerArgs->GetArguments(), compilerArgs->GetCount(), includeHandler.Get(), IID_PPV_ARGS(&operationResult)));

    HRESULT hr = S_OK;
    operationResult->GetStatus(&hr);
    if(SUCCEEDED(hr))
        DXCall(operationResult->GetResult(&compiledShader));
    else
        operationResult->GetErrorBuffer(&errorMessages);

    return hr;
}

static void CompileShader(const char* path, const char* functionName, ShaderType type,
                          const CompileOptions& baseCompileOpts, List<string>& filePaths,
                          Array<uint8_t>& byteCode, bool& includesAppSettings)
{
    if(FileExists(path) == false)
    {
        Assert_(false);
        throw Exception("Shader file " + std::string(path) + " does not exist");
    }

    uint64_t profileIdx = uint64_t(type);
    Assert_(profileIdx < ArraySize_(ProfileStrings));
    const char* profileString = ProfileStrings[profileIdx];
    includesAppSettings = false;

    // Make a hash off the expanded shader code
    string shaderCode = GetExpandedShaderCode(path, filePaths);

    for(const string& filePath : filePaths)
    {
        if(filePath.ends_with("AppSettings.hlsli"))
        {
            includesAppSettings = true;
            break;
        }
    }

    // Add AppSettings compile-time constants if necessary
    CompileOptions opts = baseCompileOpts;
    if(includesAppSettings)
        AppSettings::GetShaderCompileOptions(opts);

    string cacheName = MakeShaderCacheName(shaderCode, functionName, profileString, opts);

    if(FileExists(cacheName.c_str()))
    {
        ReadFileAsByteArray(cacheName.c_str(), byteCode);
        return;
    }

    if(type == ShaderType::Library)
    {
        WriteLog("Compiling shader library %s %s\n", GetFileName(path).c_str(),
                 opts.MakeDefinesString().c_str());
    }
    else
    {
        WriteLog("Compiling %s shader %s_%s %s\n", TypeStrings[uint64_t(type)],
                 GetFileName(path).c_str(), functionName, opts.MakeDefinesString().c_str());
    }

    // Loop until we succeed, or an exception is thrown
    while(true)
    {
        IDxcBlobPtr compiledShader;
        IDxcBlobEncodingPtr errorMessages;

        HRESULT hr = CompileShaderDXC(path, opts, functionName, type, profileString, compiledShader, errorMessages);
        if(FAILED(hr))
        {
            if(errorMessages)
            {
                const char* errMsgStr = reinterpret_cast<const char*>(errorMessages->GetBufferPointer());
                std::string fullMessage = MakeString("Error compiling shader file \"%s\" - ", path);
                fullMessage += errMsgStr;

                // Pop up a message box allowing user to retry compilation
                int32_t retVal = MessageBox(nullptr, fullMessage.c_str(), "Shader Compilation Error", MB_RETRYCANCEL);
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
            // Create the cache directory if it doesn't exist
            if(DirectoryExists(baseCacheDir.c_str()) == false)
                Win32Call(CreateDirectory(baseCacheDir.c_str(), nullptr));

            if(DirectoryExists(cacheDir.c_str()) == false)
                Win32Call(CreateDirectory(cacheDir.c_str(), nullptr));

            File cacheFile(cacheName.c_str(), FileOpenMode::Write);

            // Write the compiled shader to disk
            const uint64_t shaderSize = compiledShader->GetBufferSize();
            cacheFile.Write(shaderSize, compiledShader->GetBufferPointer());

            // Return the compiled shader bytecode
            byteCode.Init(shaderSize);
            memcpy(byteCode.Data(), compiledShader->GetBufferPointer(), shaderSize);

            return;
        }
    }
}

struct ShaderFile
{
    string FilePath;
    uint64_t TimeStamp;
    List<CompiledShader*> Shaders;

    ShaderFile(const string& filePath) : TimeStamp(0), FilePath(filePath)
    {
    }
};

static List<ShaderFile*> ShaderFiles;
static List<CompiledShader*> CompiledShaders;
static SRWLOCK ShaderFilesLock = SRWLOCK_INIT;
static SRWLOCK CompiledShadersLock = SRWLOCK_INIT;

static void CompileShader(CompiledShader* shader)
{
    Assert_(shader != nullptr);

    const char* functionName = shader->Type != ShaderType::Library ? shader->FunctionName.c_str() : nullptr;

    List<string> filePaths;
    CompileShader(shader->FilePath.c_str(), functionName, shader->Type, shader->CompileOpts, filePaths, shader->ByteCode, shader->IncludesAppSettings);
    shader->ByteCodeHash = GenerateHash(shader->ByteCode.Data(), int(shader->ByteCode.Size()));

    for(uint64_t fileIdx = 0; fileIdx < filePaths.Count(); ++ fileIdx)
    {
        const string& filePath = filePaths[fileIdx];
        ShaderFile* shaderFile = nullptr;
        const uint64_t numShaderFiles = ShaderFiles.Count();
        for(uint64_t shaderFileIdx = 0; shaderFileIdx < numShaderFiles; ++shaderFileIdx)
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
        for(uint64_t shaderIdx = 0; shaderIdx < shaderFile->Shaders.Count(); ++shaderIdx)
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

CompiledShaderPtr CompileFromFile(const char* path, const char* functionName,
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
    uint64_t numShaderFiles = ShaderFiles.Count();
    if(numShaderFiles == 0)
        return false;

    if(AppSettings::ShaderCompileOptionsChanged())
    {
        WriteLog("Hot-swapping shaders that use compile-time constants from AppSettings");

        AcquireSRWLockExclusive(&CompiledShadersLock);

        // Re-compile all shaders that included AppSettings.hlsl
        for(CompiledShader* shader : CompiledShaders)
        {
            if(shader->IncludesAppSettings)
                CompileShader(shader);
        }

        ReleaseSRWLockExclusive(&CompiledShadersLock);

        return true;
    }

    static uint64_t currFile = 0;

    const uint64_t numShadersToCheck = updateAll ? numShaderFiles : 1;
    bool shaderChanged = false;

    for(uint64_t i = 0; i < numShadersToCheck; ++i)
    {
        currFile = (currFile + 1) % uint64_t(numShaderFiles);

        ShaderFile* file = ShaderFiles[currFile];
        const uint64_t newTimeStamp = GetFileTimestamp(file->FilePath.c_str());
        if(file->TimeStamp == 0)
        {
            file->TimeStamp = newTimeStamp;
            return false;
        }

        if(file->TimeStamp < newTimeStamp)
        {
            WriteLog("Hot-swapping shaders for %s\n", file->FilePath.c_str());
            file->TimeStamp = newTimeStamp;
            for(uint64_t fileIdx = 0; fileIdx < file->Shaders.Count(); ++fileIdx)
            {
                // Retry a few times to avoid file conflicts with text editors
                const uint64_t NumRetries = 1000;
                for(uint64_t retryCount = 0; retryCount < NumRetries; ++retryCount)
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
    for(uint64_t i = 0; i < ShaderFiles.Count(); ++i)
        delete ShaderFiles[i];

    for(uint64_t i = 0; i < CompiledShaders.Count(); ++i)
        delete CompiledShaders[i];
}

// == CompileOptions ==============================================================================

CompileOptions::CompileOptions()
{
    Reset();
}

CompileOptions::CompileOptions(const char* name, int32_t value)
{
    Reset();
    Add(name, value);
}

void CompileOptions::Add(const char* name, int32_t value)
{
    Options.Add({ .Name = name, .Value = value });
}

void CompileOptions::Reset()
{
    Options.RemoveAll();
}

std::string CompileOptions::MakeDefinesString() const
{
    std::string definesString = "";
    for (const CompileOption& option : Options)
    {
        if (definesString.empty() == false)
            definesString += "|";
        definesString += MakeString("%s=%i", option.Name, option.Value);
    }

    return definesString;
}

}