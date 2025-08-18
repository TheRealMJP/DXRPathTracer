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

namespace SampleFramework12
{

struct Hash
{
    uint64_t A;
    uint64_t B;

    Hash() : A(0), B(0) {}
    Hash(uint64_t a, uint64_t b) : A(a), B(b) {}

    std::string ToString() const;
};

inline bool operator==(const Hash& a, const Hash& b)
{
    return a.A == b.A && a.B == b.B;
}

Hash GenerateHash(const void* key, int32_t len, uint32_t seed = 0);
Hash CombineHashes(Hash a, Hash b);

}