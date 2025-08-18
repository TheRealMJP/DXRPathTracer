//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"

#include "FileIO.h"

namespace SampleFramework12
{

// Returns true if a file exits
bool FileExists(const char* filePath)
{
    if(filePath == NULL)
        return false;

    DWORD fileAttr = GetFileAttributesA(filePath);
    if (fileAttr == INVALID_FILE_ATTRIBUTES)
        return false;

    return true;
}

// Retursn true if a directory exists
bool DirectoryExists(const char* dirPath)
{
    if(dirPath == NULL)
        return false;

    DWORD fileAttr = GetFileAttributesA(dirPath);
    return (fileAttr != INVALID_FILE_ATTRIBUTES && (fileAttr & FILE_ATTRIBUTE_DIRECTORY));
}

// Returns the directory containing a file
std::string GetDirectoryFromFilePath(const char* filePath_)
{
    Assert_(filePath_);

    std::string filePath(filePath_);
    size_t idx = filePath.rfind('\\');
    if(idx != std::string::npos)
        return filePath.substr(0, idx + 1);
    else
        return std::string("");
}

// Returns the name of the file given the path (extension included)
std::string GetFileName(const char* filePath_)
{
    Assert_(filePath_);

    std::string filePath(filePath_);
    size_t idx = filePath.rfind('\\');
    if(idx != std::string::npos && idx < filePath.length() - 1)
        return filePath.substr(idx + 1);
    else
    {
        idx = filePath.rfind('/');
        if(idx != std::string::npos && idx < filePath.length() - 1)
            return filePath.substr(idx + 1);
        else
            return filePath;
    }
}

// Returns the given file path, minus the extension
std::string GetFilePathWithoutExtension(const char* filePath_)
{
    Assert_(filePath_);

    std::string filePath(filePath_);
    size_t idx = filePath.rfind('.');
    if (idx != std::string::npos)
        return filePath.substr(0, idx);
    else
        return std::string("");
}

// Returns the extension of the file path
std::string GetFileExtension(const char* filePath_)
{
    Assert_(filePath_);

    std::string filePath(filePath_);
    size_t idx = filePath.rfind('.');
    if (idx != std::string::npos)
        return filePath.substr(idx + 1, filePath.length() - idx - 1);
    else
        return std::string("");
}

// Resolves a relative file path
std::string ResolveFilePath(const char* filePath)
{
    char resolvedPath[MAX_PATH] = { };
    GetFullPathNameA(filePath, ArraySize_(resolvedPath), resolvedPath, nullptr);
    return std::string(resolvedPath);
}

// Gets the last written timestamp of the file
uint64_t GetFileTimestamp(const char* filePath)
{
    Assert_(filePath);

    WIN32_FILE_ATTRIBUTE_DATA attributes;
    Win32Call(GetFileAttributesExA(filePath, GetFileExInfoStandard, &attributes));
    return attributes.ftLastWriteTime.dwLowDateTime | (uint64_t(attributes.ftLastWriteTime.dwHighDateTime) << 32);
}

// Returns the contents of a file as a string
std::string ReadFileAsString(const char* filePath)
{
    File file(filePath, FileOpenMode::Read);
    uint64_t fileSize = file.Size();

    std::string fileContents;
    fileContents.resize(size_t(fileSize), 0);
    file.Read(fileSize, &fileContents[0]);

    return fileContents;
}

// Writes the contents of a string to a file
void WriteStringAsFile(const char* filePath, const std::string& data)
{
    File file(filePath, FileOpenMode::Write);
    file.Write(data.length(), data.c_str());
}

void ReadFileAsByteArray(const char* filePath, Array<uint8_t>& data)
{
    File file(filePath, FileOpenMode::Read);
    uint64_t fileSize = file.Size();

    data.Init(fileSize);
    file.Read(fileSize, data.Data());
}

void WriteFileAsByteArray(const char* filePath, const Array<uint8_t>& data)
{
    File file(filePath, FileOpenMode::Write);
    file.Write(data.Size(), data.Data());
}

// == File ========================================================================================

File::File() : fileHandle(INVALID_HANDLE_VALUE), openMode(FileOpenMode::Read)
{
}

File::File(const char* filePath, FileOpenMode openMode) : fileHandle(INVALID_HANDLE_VALUE),
                                                           openMode(FileOpenMode::Read)
{
    Open(filePath, openMode);
}

File::~File()
{
    Close();
    Assert_(fileHandle == INVALID_HANDLE_VALUE);
}

void File::Open(const char* filePath, FileOpenMode openMode_)
{
    Assert_(fileHandle == INVALID_HANDLE_VALUE);
    openMode = openMode_;

    if(openMode == FileOpenMode::Read)
    {
        Assert_(FileExists(filePath));

        // Open the file
        fileHandle = CreateFileA(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if(fileHandle == INVALID_HANDLE_VALUE)
        {
            std::string errPrefix = std::string("Failed to open file ") + filePath + ":\n";
            // Assert_(false);
            throw Win32Exception(GetLastError(), errPrefix.c_str());
        }
    }
    else
    {
        // If the exists, delete it
        if(FileExists(filePath))
            Win32Call(DeleteFile(filePath));

        // Create the file
        fileHandle = CreateFileA(filePath, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if(fileHandle == INVALID_HANDLE_VALUE)
        {
            std::string errPrefix = std::string("Failed to open file ") + filePath + ":\n";
            Assert_(false);
            throw Win32Exception(GetLastError(), errPrefix.c_str());
        }
    }
}

void File::Close()
{
    if(fileHandle == INVALID_HANDLE_VALUE)
        return;

    // Close the file
    Win32Call(CloseHandle(fileHandle));

    fileHandle = INVALID_HANDLE_VALUE;
}

void File::Read(uint64_t size, void* data) const
{
    Assert_(fileHandle != INVALID_HANDLE_VALUE);
    Assert_(openMode == FileOpenMode::Read);

    DWORD bytesRead = 0;
    Win32Call(ReadFile(fileHandle, data, static_cast<DWORD>(size), &bytesRead, NULL));
}

void File::Write(uint64_t size, const void* data) const
{
    Assert_(fileHandle != INVALID_HANDLE_VALUE);
    Assert_(openMode == FileOpenMode::Write);

    DWORD bytesWritten = 0;
    Win32Call(WriteFile(fileHandle, data, static_cast<DWORD>(size), &bytesWritten, NULL));
}

uint64_t File::Size() const
{
    Assert_(fileHandle != INVALID_HANDLE_VALUE);

    LARGE_INTEGER fileSize;
    Win32Call(GetFileSizeEx(fileHandle, &fileSize));

    return fileSize.QuadPart;
}

}