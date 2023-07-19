//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "PCH.h"

namespace SampleFramework12
{

struct Hash
{
    uint64 A;
    uint64 B;

    Hash() : A(0), B(0) {}
    Hash(uint64 a, uint64 b) : A(a), B(b) {}

    std::wstring ToString() const;
};

inline bool operator==(const Hash& a, const Hash& b)
{
    return a.A == b.A && a.B == b.B;
}

Hash GenerateHash(const void* key, int32 len, uint32 seed = 0);
Hash CombineHashes(Hash a, Hash b);

}