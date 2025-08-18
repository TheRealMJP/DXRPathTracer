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

#include "Exceptions.h"
#include "Utility.h"
#include "Containers.h"

namespace SampleFramework12
{

// Utility functions
bool FileExists(const char* filePath);
bool DirectoryExists(const char* dirPath);
std::string GetDirectoryFromFilePath(const char* filePath);
std::string GetFileName(const char* filePath);
std::string GetFilePathWithoutExtension(const char* filePath);
std::string GetFileExtension(const char* filePath);
std::string ResolveFilePath(const char* filePath);
uint64_t GetFileTimestamp(const char* filePath);

std::string ReadFileAsString(const char* filePath);
void WriteStringAsFile(const char* filePath, const std::string& data);

void ReadFileAsByteArray(const char* filePath, Array<uint8_t>& data);
void WriteFileAsByteArray(const char* filePath, const Array<uint8_t>& data);

enum class FileOpenMode
{
    Read = 0,
    Write = 1,
};

class File
{

private:

    HANDLE fileHandle;
    FileOpenMode openMode;

public:

    // Lifetime
    File();
    File(const char* filePath, FileOpenMode openMode);
    ~File();

    // Explicit Open and close
    void Open(const char* filePath, FileOpenMode openMode);
    void Close();

    // I/O
    void Read(uint64_t size, void* data) const;
    void Write(uint64_t size, const void* data) const;

    template<typename T> void Read(T& data) const;
    template<typename T> void Write(const T& data) const;

    // Accessors
    uint64_t Size() const;
};

// == File ========================================================================================

template<typename T> void File::Read(T& data) const
{
    Read(sizeof(T), &data);
}

template<typename T> void File::Write(const T& data) const
{
    Write(sizeof(T), &data);
}

// Templated helper functions

// Reads a POD type from a file
template<typename T> void ReadFromFile(const char* fileName, T& val)
{
    File file(fileName, FileOpenMode::Read);
    file.Read(val);
}

// Writes a POD type to a file
template<typename T> void WriteToFile(const char* fileName, const T& val)
{
    File file(fileName, FileOpenMode::Write);
    file.Write(val);
}

}