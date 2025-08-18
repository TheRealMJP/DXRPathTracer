//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"

#include "Model.h"

#include "..\\Exceptions.h"
#include "..\\Utility.h"
#include "GraphicsTypes.h"
#include "..\\Serialization.h"
#include "..\\FileIO.h"
#include "..\\MurmurHash.h"
#include "Textures.h"

using std::string;

namespace SampleFramework12
{

static const InputElementType StandardInputElementTypes[5] =
{
    InputElementType::Position,
    InputElementType::Normal,
    InputElementType::UV,
    InputElementType::Tangent,
    InputElementType::Bitangent,
};

static const D3D12_INPUT_ELEMENT_DESC StandardInputElements[5] =
{
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

static const char* DefaultTextures[] =
{
    "..\\Content\\Textures\\DefaultBaseColor.dds",     // Albedo
    "..\\Content\\Textures\\DefaultNormalMap.dds",     // Normal
    "..\\Content\\Textures\\DefaultRoughness.dds",     // Roughness
    "..\\Content\\Textures\\DefaultBlack.dds",         // Metallic
    "..\\Content\\Textures\\Default.dds",              // Opacity
    "..\\Content\\Textures\\DefaultBlack.dds",         // Emissive
};

StaticAssert_(ArraySize_(DefaultTextures) == uint64_t(MaterialTextures::Count));

static Float3 ConvertVector(const aiVector3D& vec)
{
    return Float3(vec.x, vec.y, vec.z);
}

static Float3 ConvertColor(const aiColor3D& clr)
{
    return Float3(clr.r, clr.g, clr.b);
}

static Float4x4 ConvertMatrix(const aiMatrix4x4& mat)
{
    return Float4x4(Float4(mat.a1, mat.a2, mat.a3, mat.a4),
                    Float4(mat.b1, mat.b2, mat.b3, mat.b4),
                    Float4(mat.c1, mat.c2, mat.c3, mat.c4),
                    Float4(mat.d1, mat.d2, mat.d3, mat.d4));
}

static void LoadMaterialResources(Array<MeshMaterial>& materials, const string& directory, bool32 forceSRGB,
                                  List<MaterialTexture*>& materialTextures)
{
    const uint64_t numMaterials = materials.Size();
    for(uint64_t matIdx = 0; matIdx < numMaterials; ++matIdx)
    {
        MeshMaterial& material = materials[matIdx];
        for(uint64_t texType = 0; texType < uint64_t(MaterialTextures::Count); ++texType)
        {
            material.Textures[texType] = nullptr;

            string path = directory + material.TextureNames[texType];
            if(material.TextureNames[texType].length() == 0 || FileExists(path.c_str()) == false)
                path = DefaultTextures[texType];
            else if(texType == uint64_t(MaterialTextures::Opacity))
                material.Opaque = false;

            const uint64_t numLoaded = materialTextures.Count();
            for(uint64_t i = 0; i < numLoaded; ++i)
            {
                if(materialTextures[i]->Name == path)
                {
                    material.Textures[texType] = &materialTextures[i]->Texture;
                    material.TextureIndices[texType] = uint32_t(i);
                    break;
                }
            }

            if(material.Textures[texType] == nullptr)
            {
                MaterialTexture* newMatTexture = new MaterialTexture();
                newMatTexture->Name = path;
                bool useSRGB = forceSRGB && texType == uint64_t(MaterialTextures::Albedo);
                LoadTexture(newMatTexture->Texture, path.c_str(), useSRGB ? true : false);
                uint64_t idx = materialTextures.Add(newMatTexture);

                material.Textures[texType] = &newMatTexture->Texture;
                material.TextureIndices[texType] = uint32_t(idx);
            }
        }
    }
}

static void TransformVertex(MeshVertex& v, const Float3& p, const Float3& s, const Quaternion& q)
{
    v.Position *= s;
    v.Position = Float3::Transform(v.Position, q);
    v.Position += p;

    v.Normal = Float3::Transform(v.Normal, q);
    v.Tangent = Float3::Transform(v.Tangent, q);
    v.Bitangent = Float3::Transform(v.Bitangent, q);
}

static const uint64_t CacheVersion = 6;
static const char* CacheDir = "ModelCache";

static string MakeModelCachePath(ModelLoadSettings settings)
{
    Array<uint8_t> fileData;
    ReadFileAsByteArray(settings.FilePath, fileData);

    Hash settingsHash = GenerateHash(settings.FilePath, int32_t(strlen(settings.FilePath)));
    if(settings.TextureDir != nullptr)
        settingsHash = CombineHashes(settingsHash, GenerateHash(settings.TextureDir, int32_t(strlen(settings.TextureDir))));

    settings.FilePath = nullptr;
    settings.TextureDir = nullptr;
    settingsHash = CombineHashes(settingsHash, GenerateHash(&settings, sizeof(settings)));

    Hash modelHash = GenerateHash(fileData.Data(), int32_t(fileData.Size()));

    return MakeString("%s\\%s_%s_%llu.modelcache", CacheDir, settingsHash.ToString().c_str(), modelHash.ToString().c_str(), CacheVersion);
}

void Mesh::InitFromAssimpMesh(const aiMesh& assimpMesh, const ModelLoadSettings& loadSettings, MeshVertex* dstVertices, uint8_t* dstIndices, IndexType indexType_, const Float4x4& transform)
{
    numVertices = assimpMesh.mNumVertices;
    numIndices = assimpMesh.mNumFaces * 3;
    indexType = indexType_;
    vertices = dstVertices;
    indices = dstIndices;

    if(assimpMesh.HasPositions())
    {
        // Compute the AABB of the mesh, and copy the positions
        aabbMin = FloatMax;
        aabbMax = -FloatMax;

        for(uint64_t i = 0; i < numVertices; ++i)
        {
            Float3 position = ConvertVector(assimpMesh.mVertices[i]);
            position = Float3::Transform(position, transform);
            position *= loadSettings.SceneScale;
            if(loadSettings.ConvertFromZUp)
            {
                const float z = position.z;
                position.z = position.y;
                position.y = -z;
            }

            aabbMin = Min(aabbMin, position);
            aabbMax = Max(aabbMax, position);

            dstVertices[i].Position = position;
        }
    }

    if(assimpMesh.HasNormals())
    {
        for(uint64_t i = 0; i < numVertices; ++i)
            dstVertices[i].Normal = Float3::TransformDirection(ConvertVector(assimpMesh.mNormals[i]), transform);
    }

    if(assimpMesh.HasTextureCoords(0))
    {
        for(uint64_t i = 0; i < numVertices; ++i)
            dstVertices[i].UV = ConvertVector(assimpMesh.mTextureCoords[0][i]).To2D();
    }

    if(assimpMesh.HasTangentsAndBitangents())
    {
        for(uint64_t i = 0; i < numVertices; ++i)
        {
            dstVertices[i].Tangent = Float3::TransformDirection(ConvertVector(assimpMesh.mTangents[i]), transform);
            dstVertices[i].Bitangent = Float3::TransformDirection(ConvertVector(assimpMesh.mBitangents[i]) * -1.0f, transform);
        }
    }

    // Copy the index data
    const uint64_t numTriangles = assimpMesh.mNumFaces;
    if(indexType_ == IndexType::Index16Bit)
    {
        uint16_t* dstIndices16 = (uint16_t*)dstIndices;
        for(uint64_t triIdx = 0; triIdx < numTriangles; ++triIdx)
        {
            dstIndices16[triIdx * 3 + 0] = uint16_t(assimpMesh.mFaces[triIdx].mIndices[0]);
            dstIndices16[triIdx * 3 + 1] = uint16_t(assimpMesh.mFaces[triIdx].mIndices[1]);
            dstIndices16[triIdx * 3 + 2] = uint16_t(assimpMesh.mFaces[triIdx].mIndices[2]);
        }
    }
    else
    {
        uint32_t* dstIndices32 = (uint32_t*)dstIndices;
        for (uint64_t triIdx = 0; triIdx < numTriangles; ++triIdx)
        {
            dstIndices32[triIdx * 3 + 0] = uint32_t(assimpMesh.mFaces[triIdx].mIndices[0]);
            dstIndices32[triIdx * 3 + 1] = uint32_t(assimpMesh.mFaces[triIdx].mIndices[1]);
            dstIndices32[triIdx * 3 + 2] = uint32_t(assimpMesh.mFaces[triIdx].mIndices[2]);
        }
    }
    
    meshParts.Init(1);
    MeshPart& part = meshParts[0];
    part.IndexStart = 0;
    part.IndexCount = numIndices;
    part.VertexStart = 0;
    part.VertexCount = numVertices;
    part.MaterialIdx = assimpMesh.mMaterialIndex;
}

static const uint64_t NumBoxVerts = 24;
static const uint64_t NumBoxIndices = 36;

// Initializes the mesh as a box
void Mesh::InitBox(const Float3& dimensions, const Float3& position,
                   const Quaternion& orientation, uint32_t materialIdx,
                   MeshVertex* dstVertices, uint16_t* dstIndices)
{
    uint64_t vIdx = 0;

    // Top
    dstVertices[vIdx++] = { Float3(-1.0f, 1.0f, 1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(0.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f) };
    dstVertices[vIdx++] = { Float3(1.0f, 1.0f, 1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(1.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f) };
    dstVertices[vIdx++] = { Float3(1.0f, 1.0f, -1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(1.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f) };
    dstVertices[vIdx++] = { Float3(-1.0f, 1.0f, -1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(0.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f) };

    // Bottom
    dstVertices[vIdx++] = { Float3(-1.0f, -1.0f, -1.0f), Float3(0.0f, -1.0f, 0.0f), Float2(0.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, 1.0f) };
    dstVertices[vIdx++] = { Float3(1.0f, -1.0f, -1.0f), Float3(0.0f, -1.0f, 0.0f), Float2(1.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, 1.0f) };
    dstVertices[vIdx++] = { Float3(1.0f, -1.0f, 1.0f), Float3(0.0f, -1.0f, 0.0f), Float2(1.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, 1.0f) };
    dstVertices[vIdx++] = { Float3(-1.0f, -1.0f, 1.0f), Float3(0.0f, -1.0f, 0.0f), Float2(0.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, 1.0f) };

    // Front
    dstVertices[vIdx++] = { Float3(-1.0f, 1.0f, -1.0f), Float3(0.0f, 0.0f, -1.0f), Float2(0.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f) };
    dstVertices[vIdx++] = { Float3(1.0f, 1.0f, -1.0f), Float3(0.0f, 0.0f, -1.0f), Float2(1.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f) };
    dstVertices[vIdx++] = { Float3(1.0f, -1.0f, -1.0f), Float3(0.0f, 0.0f, -1.0f), Float2(1.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f) };
    dstVertices[vIdx++] = { Float3(-1.0f, -1.0f, -1.0f), Float3(0.0f, 0.0f, -1.0f), Float2(0.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f) };

    // Back
    dstVertices[vIdx++] = { Float3(1.0f, 1.0f, 1.0f), Float3(0.0f, 0.0f, 1.0f), Float2(0.0f, 0.0f), Float3(-1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f) };
    dstVertices[vIdx++] = { Float3(-1.0f, 1.0f, 1.0f), Float3(0.0f, 0.0f, 1.0f), Float2(1.0f, 0.0f), Float3(-1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f) };
    dstVertices[vIdx++] = { Float3(-1.0f, -1.0f, 1.0f), Float3(0.0f, 0.0f, 1.0f), Float2(1.0f, 1.0f), Float3(-1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f) };
    dstVertices[vIdx++] = { Float3(1.0f, -1.0f, 1.0f), Float3(0.0f, 0.0f, 1.0f), Float2(0.0f, 1.0f), Float3(-1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f) };

    // Left
    dstVertices[vIdx++] = { Float3(-1.0f, 1.0f, 1.0f), Float3(-1.0f, 0.0f, 0.0f), Float2(0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f), Float3(0.0f, -1.0f, 0.0f) };
    dstVertices[vIdx++] = { Float3(-1.0f, 1.0f, -1.0f), Float3(-1.0f, 0.0f, 0.0f), Float2(1.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f), Float3(0.0f, -1.0f, 0.0f) };
    dstVertices[vIdx++] = { Float3(-1.0f, -1.0f, -1.0f), Float3(-1.0f, 0.0f, 0.0f), Float2(1.0f, 1.0f), Float3(0.0f, 0.0f, -1.0f), Float3(0.0f, -1.0f, 0.0f) };
    dstVertices[vIdx++] = { Float3(-1.0f, -1.0f, 1.0f), Float3(-1.0f, 0.0f, 0.0f), Float2(0.0f, 1.0f), Float3(0.0f, 0.0f, -1.0f), Float3(0.0f, -1.0f, 0.0f) };

    // Right
    dstVertices[vIdx++] = { Float3(1.0f, 1.0f, -1.0f), Float3(1.0f, 0.0f, 0.0f), Float2(0.0f, 0.0f), Float3(0.0f, 0.0f, 1.0f), Float3(0.0f, -1.0f, 0.0f) };
    dstVertices[vIdx++] = { Float3(1.0f, 1.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float2(1.0f, 0.0f), Float3(0.0f, 0.0f, 1.0f), Float3(0.0f, -1.0f, 0.0f) };
    dstVertices[vIdx++] = { Float3(1.0f, -1.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float2(1.0f, 1.0f), Float3(0.0f, 0.0f, 1.0f), Float3(0.0f, -1.0f, 0.0f) };
    dstVertices[vIdx++] = { Float3(1.0f, -1.0f, -1.0f), Float3(1.0f, 0.0f, 0.0f), Float2(0.0f, 1.0f), Float3(0.0f, 0.0f, 1.0f), Float3(0.0f, -1.0f, 0.0f) };

    aabbMin = FloatMax;
    aabbMax = -FloatMax;
    for(uint64_t i = 0; i < NumBoxVerts; ++i)
    {
        TransformVertex(dstVertices[i], position, dimensions * 0.5f, orientation);

        aabbMin = Min(aabbMin, dstVertices[i].Position);
        aabbMax = Max(aabbMax, dstVertices[i].Position);
    }

    uint64_t iIdx = 0;

    // Top
    dstIndices[iIdx++] = 0;
    dstIndices[iIdx++] = 1;
    dstIndices[iIdx++] = 2;
    dstIndices[iIdx++] = 2;
    dstIndices[iIdx++] = 3;
    dstIndices[iIdx++] = 0;

    // Bottom
    dstIndices[iIdx++] = 4 + 0;
    dstIndices[iIdx++] = 4 + 1;
    dstIndices[iIdx++] = 4 + 2;
    dstIndices[iIdx++] = 4 + 2;
    dstIndices[iIdx++] = 4 + 3;
    dstIndices[iIdx++] = 4 + 0;

    // Front
    dstIndices[iIdx++] = 8 + 0;
    dstIndices[iIdx++] = 8 + 1;
    dstIndices[iIdx++] = 8 + 2;
    dstIndices[iIdx++] = 8 + 2;
    dstIndices[iIdx++] = 8 + 3;
    dstIndices[iIdx++] = 8 + 0;

    // Back
    dstIndices[iIdx++] = 12 + 0;
    dstIndices[iIdx++] = 12 + 1;
    dstIndices[iIdx++] = 12 + 2;
    dstIndices[iIdx++] = 12 + 2;
    dstIndices[iIdx++] = 12 + 3;
    dstIndices[iIdx++] = 12 + 0;

    // Left
    dstIndices[iIdx++] = 16 + 0;
    dstIndices[iIdx++] = 16 + 1;
    dstIndices[iIdx++] = 16 + 2;
    dstIndices[iIdx++] = 16 + 2;
    dstIndices[iIdx++] = 16 + 3;
    dstIndices[iIdx++] = 16 + 0;

    // Right
    dstIndices[iIdx++] = 20 + 0;
    dstIndices[iIdx++] = 20 + 1;
    dstIndices[iIdx++] = 20 + 2;
    dstIndices[iIdx++] = 20 + 2;
    dstIndices[iIdx++] = 20 + 3;
    dstIndices[iIdx++] = 20 + 0;

    indexType = IndexType::Index16Bit;

    numVertices = uint32_t(NumBoxVerts);
    numIndices = uint32_t(NumBoxIndices);

    vertices = dstVertices;
    indices = (uint8_t*)dstIndices;

    meshParts.Init(1);
    MeshPart& part = meshParts[0];
    part.IndexStart = 0;
    part.IndexCount = numIndices;
    part.VertexStart = 0;
    part.VertexCount = numVertices;
    part.MaterialIdx = materialIdx;
}

const uint64_t NumPlaneVerts = 4;
const uint64_t NumPlaneIndices = 6;

// Initializes the mesh as a plane
void Mesh::InitPlane(const Float2& dimensions, const Float3& position, const Quaternion& orientation, uint32_t materialIdx,
                     MeshVertex* dstVertices, uint16_t* dstIndices)
{
    uint64_t vIdx = 0;

    dstVertices[vIdx++] = { Float3(-1.0f, 0.0f, 1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(0.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f) };
    dstVertices[vIdx++] = { Float3(1.0f, 0.0f, 1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(1.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f) };
    dstVertices[vIdx++] = { Float3(1.0f, 0.0f, -1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(1.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f) };
    dstVertices[vIdx++] = { Float3(-1.0f, 0.0f, -1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(0.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f) };

    aabbMin = FloatMax;
    aabbMax = -FloatMax;
    for(uint64_t i = 0; i < NumPlaneVerts; ++i)
    {
        TransformVertex(dstVertices[i], position, Float3(dimensions.x, 1.0f, dimensions.y) * 0.5f, orientation);

        aabbMin = Min(aabbMin, dstVertices[i].Position);
        aabbMax = Max(aabbMax, dstVertices[i].Position);
    }

    uint64_t iIdx = 0;
    dstIndices[iIdx++] = 0;
    dstIndices[iIdx++] = 1;
    dstIndices[iIdx++] = 2;
    dstIndices[iIdx++] = 2;
    dstIndices[iIdx++] = 3;
    dstIndices[iIdx++] = 0;

    indexType = IndexType::Index16Bit;

    numVertices = uint32_t(NumPlaneVerts);
    numIndices = uint32_t(NumPlaneIndices);

    vertices = dstVertices;
    indices = (uint8_t*)dstIndices;

    meshParts.Init(1);
    MeshPart& part = meshParts[0];
    part.IndexStart = 0;
    part.IndexCount = numIndices;
    part.VertexStart = 0;
    part.VertexCount = numVertices;
    part.MaterialIdx = materialIdx;
}

void Mesh::InitCommon(const MeshVertex* vertices_, const uint8_t* indices_, uint64_t vbAddress, uint64_t ibAddress, uint64_t vtxOffset_, uint64_t idxOffset_)
{
    Assert_(meshParts.Size() > 0);

    vertices = vertices_;
    indices = indices_;
    vtxOffset = uint32_t(vtxOffset_);
    idxOffset = uint32_t(idxOffset_);

    vbView.BufferLocation = vbAddress;
    vbView.SizeInBytes = sizeof(MeshVertex) * numVertices;
    vbView.StrideInBytes = sizeof(MeshVertex);

    ibView.Format = IndexBufferFormat();
    ibView.SizeInBytes = IndexSize() * numIndices;
    ibView.BufferLocation = ibAddress;
}

void Mesh::Shutdown()
{
    numVertices = 0;
    numIndices = 0;
    meshParts.Shutdown();
    vertices = nullptr;
    indices = nullptr;
}

const char* Mesh::InputElementTypeString(InputElementType elemType)
{
    static const char* ElemStrings[] =
    {
        "POSITION",
        "NORMAL",
        "TANGENT",
        "BITANGENT",
        "UV",
    };

    StaticAssert_(ArraySize_(ElemStrings) == uint64_t(InputElementType::NumTypes));
    Assert_(uint64_t(elemType) < uint64_t(InputElementType::NumTypes));

    return ElemStrings[uint64_t(elemType)];
}

static void GatherMeshTransforms(const aiNode* node, const Float4x4& baseTransform, Array<Float4x4>& meshTransforms)
{
    Float4x4 nodeTransform = Float4x4::Transpose(ConvertMatrix(node->mTransformation)) * baseTransform;

    for(uint32_t i = 0; i < node->mNumMeshes; ++i)
        meshTransforms[node->mMeshes[i]] = nodeTransform;

    for(uint32_t i = 0; i < node->mNumChildren; ++i)
        GatherMeshTransforms(node->mChildren[i], nodeTransform, meshTransforms);
}

// == Model =======================================================================================

void Model::CreateWithAssimp(const ModelLoadSettings& settings)
{
    const char* filePath = settings.FilePath;
    Assert_(filePath != nullptr);
    if(FileExists(filePath) == false)
        throw Exception(MakeString("Model file with path '%s' does not exist", filePath));

    const string cachePath = MakeModelCachePath(settings);
    if(FileExists(cachePath.c_str()))
    {
        WriteLog("Loading scene '%s' from cache...", filePath);
        CreateFromMeshData(cachePath.c_str());
        WriteLog("Finished loading scene");

        return;
    }

    WriteLog("Loading scene '%s' with Assimp...", filePath);

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filePath, 0);

    if(scene == nullptr)
        throw Exception("Failed to load scene " + std::string(filePath) +
                        ": " + importer.GetErrorString());

    if(scene->mNumMeshes == 0)
        throw Exception("Scene " + std::string(filePath) + " has no meshes");

    if(scene->mNumMaterials == 0)
        throw Exception("Scene " + std::string(filePath) + " has no materials");

    forceSRGB = settings.ForceSRGB;

    // Grab the lights before we process the scene
    spotLights.Init(scene->mNumLights);
    pointLights.Init(scene->mNumLights);

    uint64_t numSpotLights = 0;
    uint64_t numPointLights = 0;
    for(uint64_t i = 0; i < scene->mNumLights; ++i)
    {
        const aiLight& srcLight = *scene->mLights[i];
        if(srcLight.mType == aiLightSource_SPOT)
        {
            // Assimp seems to mess up when importing spot light transforms for FBX
            std::string translationName = MakeString("%s_$AssimpFbx$_Translation", srcLight.mName.C_Str());
            const aiNode* translationNode = scene->mRootNode->FindNode(translationName.c_str());
            if(translationNode == nullptr)
                continue;

            std::string rotationName = MakeString("%s_$AssimpFbx$_Rotation", srcLight.mName.C_Str());
            const aiNode* rotationNode = translationNode->FindNode(rotationName.c_str());
            if(rotationNode == nullptr)
                continue;

            ModelSpotLight& dstLight = spotLights[numSpotLights++];

            Float4x4 translation = Float4x4::Transpose(ConvertMatrix(translationNode->mTransformation));
            dstLight.Position = translation.Translation() * settings.SceneScale;
            dstLight.Position.z *= -1.0f;
            dstLight.Intensity = ConvertColor(srcLight.mColorDiffuse) * FP16Scale;
            dstLight.AngularAttenuation.x = srcLight.mAngleInnerCone;
            dstLight.AngularAttenuation.y = srcLight.mAngleOuterCone;

            Float3x3 rotation = ConvertMatrix(rotationNode->mTransformation).To3x3();
            dstLight.Orientation = Quaternion::Normalize(Quaternion(rotation));
            dstLight.Direction = Float3::Normalize(rotation.Forward());
        }
        else if(srcLight.mType == aiLightSource_POINT)
        {
            ModelPointLight& dstLight = pointLights[numPointLights++];
            dstLight.Position = ConvertVector(srcLight.mPosition);
            dstLight.Intensity = ConvertColor(srcLight.mColorDiffuse);
        }
    }

    spotLights.Resize(numSpotLights);
    pointLights.Resize(numPointLights);

    // Post-process the scene
    uint32_t flags = aiProcess_CalcTangentSpace |
                   aiProcess_Triangulate |
                   aiProcess_JoinIdenticalVertices |
                   aiProcess_MakeLeftHanded |
                   aiProcess_RemoveRedundantMaterials |
                   aiProcess_FlipUVs |
                   aiProcess_FlipWindingOrder;

    if(settings.MergeMeshes)
        flags |= aiProcess_PreTransformVertices | aiProcess_OptimizeMeshes;

    scene = importer.ApplyPostProcessing(flags);

    // Load the materials
    const uint64_t numMaterials = scene->mNumMaterials;
    meshMaterials.Init(numMaterials);
    for(uint64_t i = 0; i < numMaterials; ++i)
    {
        MeshMaterial& material = meshMaterials[i];
        const aiMaterial& mat = *scene->mMaterials[i];

        aiString matName;
        mat.Get(AI_MATKEY_NAME, matName);
        material.Name = matName.C_Str();

        aiString diffuseTexPath;
        aiString normalMapPath;
        aiString roughnessMapPath;
        aiString metallicMapPath;
        aiString opacityMapPath;
        aiString emissiveMapPath;
        if(mat.GetTexture(aiTextureType_DIFFUSE, 0, &diffuseTexPath) == aiReturn_SUCCESS)
            material.TextureNames[uint64_t(MaterialTextures::Albedo)] = GetFileName(diffuseTexPath.C_Str());

        if(mat.GetTexture(aiTextureType_NORMALS, 0, &normalMapPath) == aiReturn_SUCCESS
           || mat.GetTexture(aiTextureType_HEIGHT, 0, &normalMapPath) == aiReturn_SUCCESS)
            material.TextureNames[uint64_t(MaterialTextures::Normal)] = GetFileName(normalMapPath.C_Str());

        if(mat.GetTexture(aiTextureType_SHININESS, 0, &roughnessMapPath) == aiReturn_SUCCESS)
            material.TextureNames[uint64_t(MaterialTextures::Roughness)] = GetFileName(roughnessMapPath.C_Str());

        if(mat.GetTexture(aiTextureType_AMBIENT, 0, &metallicMapPath) == aiReturn_SUCCESS)
            material.TextureNames[uint64_t(MaterialTextures::Metallic)] = GetFileName(metallicMapPath.C_Str());

        if(mat.GetTexture(aiTextureType_OPACITY, 0, &opacityMapPath) == aiReturn_SUCCESS)
            material.TextureNames[uint64_t(MaterialTextures::Opacity)] = GetFileName(opacityMapPath.C_Str());

        if(mat.GetTexture(aiTextureType_EMISSIVE, 0, &emissiveMapPath) == aiReturn_SUCCESS)
            material.TextureNames[uint64_t(MaterialTextures::Emissive)] = GetFileName(emissiveMapPath.C_Str());
    }

    std::string fileDirectory = GetDirectoryFromFilePath(filePath);
    textureDirectory = settings.TextureDir ? fileDirectory + "\\" + settings.TextureDir + "\\" : fileDirectory;
    LoadMaterialResources(meshMaterials, textureDirectory, settings.ForceSRGB, materialTextures);

    indexType = IndexType::Index16Bit;

    // Initialize the meshes
    const uint64_t numMeshes = scene->mNumMeshes;
    uint64_t numVertices = 0;
    uint64_t numIndices = 0;
    for(uint64_t i = 0; i < numMeshes; ++i)
    {
        const aiMesh& assimpMesh = *scene->mMeshes[i];

        numVertices += assimpMesh.mNumVertices;
        numIndices += assimpMesh.mNumFaces * 3;

        if(assimpMesh.mNumFaces * 3 > 0xFFFF)
            indexType = IndexType::Index32Bit;
    }

    // Gather transforms
    Array<Float4x4> meshTransforms(numMeshes);
    if(settings.MergeMeshes == false)
    {
        Float4x4 rootTransform;
        GatherMeshTransforms(scene->mRootNode, rootTransform, meshTransforms);
    }

    const uint64_t indexSize = indexType == IndexType::Index32Bit ? 4 : 2;

    vertices.Init(numVertices);
    indices.Init(numIndices * indexSize);
    meshes.Init(numMeshes);

    aabbMin = FloatMax;
    aabbMax = -FloatMax;

    uint64_t vtxOffset = 0;
    uint64_t idxOffset = 0;
    for(uint64_t i = 0; i < numMeshes; ++i)
    {
        meshes[i].InitFromAssimpMesh(*scene->mMeshes[i], settings, &vertices[vtxOffset], &indices[idxOffset], indexType, meshTransforms[i]);
        meshes[i].vtxOffset = uint32_t(vtxOffset);
        meshes[i].idxOffset = uint32_t(idxOffset);

        aabbMin = Min(aabbMin, meshes[i].AABBMin());
        aabbMax = Max(aabbMax, meshes[i].AABBMax());

        vtxOffset += meshes[i].NumVertices();
        idxOffset += meshes[i].NumIndices() * indexSize;
    }

    if(settings.GenerateMeshlets)
        GenerateMeshlets();

    CreateBuffers();

    WriteLog("Finished loading scene '%s'", filePath);

    if(DirectoryExists(CacheDir) == false)
        Win32Call(CreateDirectory(CacheDir, nullptr));

    FileWriteSerializer serializer(cachePath.c_str());
    Serialize(serializer);
}

void Model::CreateFromMeshData(const char* filePath)
{
    if(FileExists(filePath) == false)
        throw Exception(MakeString("Model file with path '%s' does not exist", filePath));

    FileReadSerializer serializer(filePath);
    Serialize(serializer);

    CreateBuffers();

    LoadMaterialResources(meshMaterials, textureDirectory, forceSRGB, materialTextures);
}

void Model::GenerateBoxScene(const BoxSceneInit& init)
{
    meshMaterials.Init(1);
    MeshMaterial& material = meshMaterials[0];
    material.TextureNames[uint64_t(MaterialTextures::Albedo)] = init.ColorMap;
    material.TextureNames[uint64_t(MaterialTextures::Normal)] = init.NormalMap;
    textureDirectory = "..\\Content\\Textures\\";
    LoadMaterialResources(meshMaterials, "..\\Content\\Textures\\", false, materialTextures);

    indexType = IndexType::Index16Bit;

    vertices.Init(NumBoxVerts);
    indices.Init(NumBoxIndices * sizeof(uint16_t));

    meshes.Init(1);
    meshes[0].InitBox(init.Dimensions, init.Position, init.Orientation, 0, vertices.Data(), (uint16_t*)indices.Data());

    aabbMin = meshes[0].AABBMin();
    aabbMax = meshes[0].AABBMax();

    if(init.GenerateMeshlets)
        GenerateMeshlets();

    CreateBuffers();
}

void Model::GenerateBoxTestScene(const BoxTestSceneInit& init)
{
    meshMaterials.Init(1);
    MeshMaterial& material = meshMaterials[0];
    material.TextureNames[uint64_t(MaterialTextures::Albedo)] = "White.png";
    material.TextureNames[uint64_t(MaterialTextures::Normal)] = "Hex.png";
    textureDirectory = "..\\Content\\Textures\\";
    LoadMaterialResources(meshMaterials, "..\\Content\\Textures\\", false, materialTextures);

    indexType = IndexType::Index16Bit;

    vertices.Init(NumBoxVerts * 2);
    indices.Init(NumBoxIndices * 2 * sizeof(uint16_t));

    meshes.Init(2);
    meshes[0].InitBox(init.TopBoxDimensions, init.TopBoxPosition, Quaternion(), 0, vertices.Data(), (uint16_t*)indices.Data());
    meshes[1].InitBox(init.BottomBoxDimensions, init.BottomBoxPosition, Quaternion(), 0, &vertices[NumBoxVerts], (uint16_t*)&indices[NumBoxIndices * sizeof(uint16_t)]);
    meshes[1].vtxOffset = NumBoxVerts;
    meshes[1].idxOffset = NumBoxIndices;

    aabbMin = FloatMax;
    aabbMax = -FloatMax;
    for(const Mesh& mesh : meshes)
    {
        aabbMin = Min(aabbMin, mesh.AABBMin());
        aabbMax = Max(aabbMax, mesh.AABBMax());
    }

    if(init.GenerateMeshlets)
        GenerateMeshlets();

    CreateBuffers();
}

void Model::GeneratePlaneScene(const PlaneSceneInit& init)
{
    meshMaterials.Init(1);
    MeshMaterial& material = meshMaterials[0];
    material.TextureNames[uint64_t(MaterialTextures::Albedo)] = init.ColorMap;
    material.TextureNames[uint64_t(MaterialTextures::Normal)] = init.NormalMap;
    textureDirectory = "..\\Content\\Textures\\";
    LoadMaterialResources(meshMaterials, "..\\Content\\Textures\\", false, materialTextures);

    indexType = IndexType::Index16Bit;

    vertices.Init(NumPlaneVerts);
    indices.Init(NumPlaneIndices * sizeof(uint16_t));

    meshes.Init(1);
    meshes[0].InitPlane(init.Dimensions, init.Position, init.Orientation, 0, vertices.Data(), (uint16_t*)indices.Data());

    aabbMin = meshes[0].AABBMin();
    aabbMax = meshes[0].AABBMax();

    if(init.GenerateMeshlets)
        GenerateMeshlets();

    CreateBuffers();
}

void Model::CreateProcedural(const ProceduralModelInit& init)
{
    meshMaterials.Init(1);
    MeshMaterial& material = meshMaterials[0];
    for(uint64_t i = 0; i < uint64_t(MaterialTextures::Count); ++i)
        material.TextureNames[i] = init.TexturePaths[i] ? init.TexturePaths[i] : "";

    textureDirectory = "";

    LoadMaterialResources(meshMaterials, "", init.ForceSRGB, materialTextures);

    vertices.Init(init.NumVertices);
    aabbMin = FloatMax;
    aabbMax = -FloatMax;

    for(uint32_t i = 0; i < init.NumVertices; ++i)
    {
        const MeshVertex& srcVtx = init.Vertices[i];
        vertices[i] = srcVtx;

        aabbMin = Min(aabbMin, srcVtx.Position);
        aabbMax = Max(aabbMax, srcVtx.Position);
    }

    if(init.NumVertices > 64 * 1024)
    {
        indexType = IndexType::Index32Bit;
        indices.Init(init.NumIndices * sizeof(uint32_t));
        memcpy(indices.Data(), init.Indices, indices.MemorySize());
    }
    else
    {
        indexType = IndexType::Index16Bit;
        indices.Init(init.NumIndices * sizeof(uint16_t));
        uint16_t* dstIndices = (uint16_t*)indices.Data();
        for(uint32_t i = 0; i < init.NumIndices; ++i)
            dstIndices[i] = uint16_t(init.Indices[i]);
    }

    meshes.Init(1);
    Mesh& mesh = meshes[0];

    mesh.indexType = indexType;
    mesh.numVertices = init.NumVertices;
    mesh.numIndices = init.NumIndices;
    mesh.vertices = vertices.Data();
    mesh.indices = (uint8_t*)indices.Data();

    mesh.meshParts.Init(1);
    MeshPart& part = mesh.meshParts[0];
    part.IndexStart = 0;
    part.IndexCount = init.NumIndices;
    part.VertexStart = 0;
    part.VertexCount = init.NumVertices;
    part.MaterialIdx = 0;

    if(init.GenerateMeshlets)
        GenerateMeshlets();

    CreateBuffers();
}

void Model::GenerateMeshlets()
{
    Assert_(meshes.Size() > 0);

    // Calculate the upper bounds on the number of meshlets
    uint32_t maxMeshlets = 0;
    Array<uint32_t> maxMeshletsPerMesh(meshes.Size(), 0);
    for(uint32_t meshIdx = 0; meshIdx < meshes.Size(); ++meshIdx)
    {
        const Mesh& mesh = meshes[meshIdx];
        maxMeshletsPerMesh[meshIdx] = uint32_t(meshopt_buildMeshletsBound(mesh.NumIndices(), MaxMeshletVertices, MaxMeshletTriangles));
        maxMeshlets += maxMeshletsPerMesh[meshIdx];
    }

    meshlets.Init(maxMeshlets, maxMeshlets);
    meshletVertices.Init(maxMeshlets * MaxMeshletVertices, maxMeshlets * MaxMeshletVertices, 0xFFFFFFFF);
    meshletTriangles.Init(maxMeshlets * MaxMeshletTriangles, maxMeshlets * MaxMeshletTriangles);
    meshletBounds.Init(maxMeshlets, maxMeshlets);

    List<Float3> meshletPositions;

    // Build the meshlets
    uint32_t globalMeshletOffset = 0;
    uint32_t globalVertexOffset = 0;
    uint32_t globalTriangleOffset = 0;
    for(uint32_t meshIdx = 0; meshIdx < meshes.Size(); ++meshIdx)
    {
        Mesh& mesh = meshes[meshIdx];
        const MeshVertex* meshVertices = mesh.Vertices();

        const uint32_t maxMeshletsForThisMesh = maxMeshletsPerMesh[meshIdx];
        Array<meshopt_Meshlet> meshOptMeshlets(maxMeshletsForThisMesh);
        memset(meshOptMeshlets.Data(), 0, meshOptMeshlets.MemorySize());

        Array<uint8_t> meshOptTriangles(maxMeshletsForThisMesh * MaxMeshletTriangles, 0);

        size_t numMeshMeshlets = 0;
        if(mesh.IndexBufferType() == IndexType::Index32Bit)
            numMeshMeshlets = meshopt_buildMeshlets(meshOptMeshlets.Data(), &meshletVertices[globalVertexOffset], meshOptTriangles.Data(), mesh.Indices32(), mesh.NumIndices(), (float*)meshVertices, mesh.NumVertices(), sizeof(MeshVertex), MaxMeshletVertices, MaxMeshletTriangles, 0.0f);
        else
            numMeshMeshlets = meshopt_buildMeshlets(meshOptMeshlets.Data(), &meshletVertices[globalVertexOffset], meshOptTriangles.Data(), mesh.Indices(), mesh.NumIndices(), (float*)meshVertices, mesh.NumVertices(), sizeof(MeshVertex), MaxMeshletVertices, MaxMeshletTriangles, 0.0f);

        mesh.numMeshlets = uint32_t(numMeshMeshlets);
        mesh.meshletOffset = uint32_t(globalMeshletOffset);

        uint32_t localTriangleOffset = 0;
        uint32_t localVertexOffset = 0;

        for(uint32_t meshMeshletIdx = 0; meshMeshletIdx < numMeshMeshlets; ++meshMeshletIdx)
        {
            const meshopt_Meshlet& srcMeshlet = meshOptMeshlets[meshMeshletIdx];
            Meshlet& dstMeshlet = meshlets[meshMeshletIdx + globalMeshletOffset];

            dstMeshlet.VertexOffset = uint32_t(srcMeshlet.vertex_offset + globalVertexOffset);
            dstMeshlet.TriangleOffset = uint32_t(localTriangleOffset + globalTriangleOffset);
            dstMeshlet.VertexCount = SafeCast<uint16_t>(srcMeshlet.vertex_count);
            dstMeshlet.TriangleCount = SafeCast<uint16_t>(srcMeshlet.triangle_count);

            Assert_(mesh.NumMeshParts() == 1);
            dstMeshlet.MeshIndex = SafeCast<uint16_t>(meshIdx);
            dstMeshlet.MaterialIndex = SafeCast<uint16_t>(mesh.MeshParts()[0].MaterialIdx);
            dstMeshlet.MeshVertexOffset = mesh.vtxOffset;

            for(uint32_t triIdx = 0; triIdx < srcMeshlet.triangle_count; ++triIdx)
            {
                const uint32_t v0 = meshOptTriangles[srcMeshlet.triangle_offset + triIdx * 3 + 0];
                const uint32_t v1 = meshOptTriangles[srcMeshlet.triangle_offset + triIdx * 3 + 1];
                const uint32_t v2 = meshOptTriangles[srcMeshlet.triangle_offset + triIdx * 3 + 2];
                meshletTriangles[dstMeshlet.TriangleOffset + triIdx].Packed = v0 | (v1 << 8) | (v2 << 16);
            }

            meshletPositions.RemoveAll();
            meshletPositions.Reserve(dstMeshlet.VertexCount);
            for(uint32_t meshletVertexIndex = 0; meshletVertexIndex < dstMeshlet.VertexCount; ++meshletVertexIndex)
            {
                const uint32_t meshVertexIndex = meshletVertices[globalVertexOffset + meshletVertexIndex + srcMeshlet.vertex_offset];
                Assert_(meshVertexIndex < mesh.NumVertices());
                meshletPositions.Add(meshVertices[meshVertexIndex].Position);
            }

            DirectX::BoundingSphere meshletSphere;
            DirectX::BoundingSphere::CreateFromPoints(meshletSphere, dstMeshlet.VertexCount, (const DirectX::XMFLOAT3*)meshletPositions.Data(), sizeof(Float3));
            meshletBounds[meshMeshletIdx + globalMeshletOffset] = { .Center = Float3(meshletSphere.Center), .Radius = meshletSphere.Radius };

            localVertexOffset += uint32_t(srcMeshlet.vertex_count);
            localTriangleOffset += uint32_t(srcMeshlet.triangle_count);
        }

        globalMeshletOffset += uint32_t(numMeshMeshlets);
        globalVertexOffset += localVertexOffset;
        globalTriangleOffset += localTriangleOffset;
    }

    meshlets.Trim(globalMeshletOffset);
    meshletVertices.Trim(globalVertexOffset);
    meshletTriangles.Trim(globalTriangleOffset);
    meshletBounds.Trim(globalMeshletOffset);
}

void Model::Shutdown()
{
    for(uint64_t i = 0; i < meshes.Size(); ++i)
        meshes[i].Shutdown();
    meshes.Shutdown();
    meshMaterials.Shutdown();
    for(uint64_t i = 0; i < materialTextures.Count(); ++i)
    {
        materialTextures[i]->Texture.Shutdown();
        delete materialTextures[i];
        materialTextures[i] = nullptr;
    }
    materialTextures.Shutdown();
    textureDirectory = "";
    forceSRGB = false;

    vertexBuffer.Shutdown();
    indexBuffer.Shutdown();
    vertices.Shutdown();
    indices.Shutdown();
    meshletBuffer.Shutdown();
    meshletVerticesBuffer.Shutdown();
    meshletTrianglesBuffer.Shutdown();
    meshletBoundsBuffer.Shutdown();
}

const D3D12_INPUT_ELEMENT_DESC* Model::InputElements()
{
    return StandardInputElements;
}

const InputElementType* Model::InputElementTypes()
{
    return StandardInputElementTypes;
}

uint64_t Model::NumInputElements()
{
    return ArraySize_(StandardInputElements);
}

void Model::CreateBuffers()
{
    Assert_(meshes.Size() > 0);

    StructuredBufferInit sbInit;
    sbInit.Stride = sizeof(MeshVertex);
    sbInit.NumElements = vertices.Size();;
    sbInit.InitData = vertices.Data();
    sbInit.Name = "Model Vertex Buffer";
    vertexBuffer.Initialize(sbInit);

    const uint32_t indexSize = IndexSize();

    FormattedBufferInit fbInit;
    fbInit.Format = IndexBufferFormat();
    fbInit.NumElements = indices.Size() / indexSize;
    fbInit.InitData = indices.Data();
    sbInit.Name = "Model Index Buffer";
    indexBuffer.Initialize(fbInit);

    uint64_t vtxOffset = 0;
    uint64_t idxOffset = 0;
    const uint64_t numMeshes = meshes.Size();
    for(uint64_t i = 0; i < numMeshes; ++i)
    {
        uint64_t vbOffset = vtxOffset * sizeof(MeshVertex);
        uint64_t ibOffset = idxOffset * indexSize;
        meshes[i].InitCommon(&vertices[vtxOffset], &indices[ibOffset], vertexBuffer.GPUAddress + vbOffset, indexBuffer.GPUAddress + ibOffset, vtxOffset, idxOffset);

        vtxOffset += meshes[i].NumVertices();
        idxOffset += meshes[i].NumIndices();
    }

    if(meshlets.Count() > 0)
    {
        meshletBuffer.Initialize({
            .Stride = sizeof(Meshlet),
            .NumElements = uint32_t(meshlets.Count()),
            .InitData = meshlets.Data(),
            .Name = "Meshlet Buffer",
        });

        meshletVerticesBuffer.Initialize({
            .NumElements = uint32_t(meshletVertices.Count()),
            .InitData = meshletVertices.Data(),
            .Name = "Meshlet Vertices Buffer",
        });

        meshletTrianglesBuffer.Initialize({
            .Stride = sizeof(MeshletTriangle),
            .NumElements = meshletTriangles.Count(),
            .InitData = meshletTriangles.Data(),
            .Name = "Meshlet Triangles Buffer",
        });

        meshletBoundsBuffer.Initialize({
            .Stride = sizeof(MeshletBounds),
            .NumElements = uint32_t(meshlets.Count()),
            .InitData = meshletBounds.Data(),
            .Name = "Meshlet Bounds Buffer",
        });
    }
}

// == Geometry helpers ============================================================================

void MakeSphereGeometry(uint64_t uDivisions, uint64_t vDivisions, StructuredBuffer& vtxBuffer, FormattedBuffer& idxBuffer)
{
    Assert_(uDivisions >= 3);
    Assert_(vDivisions >= 3);

    const uint64_t numSphereVerts = (vDivisions - 1) * uDivisions + 2;
    Assert_(numSphereVerts <= UINT16_MAX);

    Array<Float3> sphereVerts(numSphereVerts);
    uint64_t currSphereVert = 0;

    // Add the vert at the top
    sphereVerts[currSphereVert++] = Float3(0.0f, 0.0f, 1.0f);

    // Add the rings
    for(uint64_t v = 1; v < vDivisions; ++v)
    {
        for(uint64_t u = 0; u < uDivisions; ++u)
        {
            const float theta = (float(v) / vDivisions) * Pi;
            const float phi = (float(u) / uDivisions) * Pi2;

            Float3 pos;
            pos.x = std::sin(theta) * std::cos(phi);
            pos.y = std::sin(theta) * std::sin(phi);
            pos.z = std::cos(theta);

            sphereVerts[currSphereVert++] = pos;
        }
    }

    // Add the vert at the bottom
    const uint64_t lastVertIdx = currSphereVert;
    sphereVerts[currSphereVert++] = Float3(0.0f, 0.0f, -1.0f);
    Assert_(currSphereVert == numSphereVerts);

    // Add the top ring of triangles
    List<uint16_t> sphereIndices;
    for(uint64_t u = 0; u < uDivisions; ++u)
    {
        sphereIndices.Add(uint16_t(0));
        sphereIndices.Add(uint16_t(u + 1));

        if(u < uDivisions - 1)
            sphereIndices.Add(uint16_t(u + 2));
        else
            sphereIndices.Add(uint16_t(1));
    }

    // Add the rest of the rings
    uint64_t prevRowStart = 1;
    uint64_t currRowStart = uDivisions + 1;
    for(uint64_t v = 1; v < vDivisions - 1; ++v)
    {
        for(uint64_t u = 0; u < uDivisions; ++u)
        {
            uint64_t nextBottom = currRowStart + u + 1;
            uint64_t nextTop = prevRowStart + u + 1;
            if(u == uDivisions - 1)
            {
                nextBottom = currRowStart;
                nextTop = prevRowStart;
            }

            sphereIndices.Add(uint16_t(prevRowStart + u));
            sphereIndices.Add(uint16_t(currRowStart + u));
            sphereIndices.Add(uint16_t(nextBottom));
            sphereIndices.Add(uint16_t(nextBottom));
            sphereIndices.Add(uint16_t(nextTop));
            sphereIndices.Add(uint16_t(prevRowStart + u));
        }

        prevRowStart = currRowStart;
        currRowStart += uint64_t(uDivisions);
    }

    // Add the last ring at the bottom
    const uint64_t lastRingStart = uint64_t(lastVertIdx - uDivisions);
    for(uint64_t u = 0; u < uDivisions; ++u)
    {
        sphereIndices.Add(uint16_t(lastVertIdx));

        if(u < uDivisions - 1)
            sphereIndices.Add(uint16_t(lastRingStart + u + 1));
        else
            sphereIndices.Add(uint16_t(lastRingStart));

        sphereIndices.Add(uint16_t(lastRingStart + u));
    }

    StructuredBufferInit vbInit;
    vbInit.Stride = sizeof(Float3);
    vbInit.NumElements = numSphereVerts;
    vbInit.InitData = sphereVerts.Data();
    vtxBuffer.Initialize(vbInit);

    FormattedBufferInit ibInit;
    ibInit.Format = DXGI_FORMAT_R16_UINT;
    ibInit.NumElements = sphereIndices.Count();
    ibInit.InitData = sphereIndices.Data();
    idxBuffer.Initialize(ibInit);
}

void MakeBoxGeometry(StructuredBuffer& vtxBuffer, FormattedBuffer& idxBuffer, float scale)
{
    Array<Float3> boxVerts(NumBoxVerts);
    Array<uint16_t> boxIndices(NumBoxIndices, 0);

    uint64_t vIdx = 0;

    // Top
    boxVerts[vIdx++] = Float3(-0.5f, 0.5f, 0.5f) * scale;
    boxVerts[vIdx++] = Float3(0.5f, 0.5f, 0.5f) * scale;
    boxVerts[vIdx++] = Float3(0.5f, 0.5f, -0.5f) * scale;
    boxVerts[vIdx++] = Float3(-0.5f, 0.5f, -0.5f) * scale;

    // Bottom
    boxVerts[vIdx++] = Float3(-0.5f, -0.5f, -0.5f) * scale;
    boxVerts[vIdx++] = Float3(0.5f, -0.5f, -0.5f) * scale;
    boxVerts[vIdx++] = Float3(0.5f, -0.5f, 0.5f) * scale;
    boxVerts[vIdx++] = Float3(-0.5f, -0.5f, 0.5f) * scale;

    // Front
    boxVerts[vIdx++] = Float3(-0.5f, 0.5f, -0.5f) * scale;
    boxVerts[vIdx++] = Float3(0.5f, 0.5f, -0.5f) * scale;
    boxVerts[vIdx++] = Float3(0.5f, -0.5f, -0.5f) * scale;
    boxVerts[vIdx++] = Float3(-0.5f, -0.5f, -0.5f) * scale;

    // Back
    boxVerts[vIdx++] = Float3(0.5f, 0.5f, 0.5f) * scale;
    boxVerts[vIdx++] = Float3(-0.5f, 0.5f, 0.5f) * scale;
    boxVerts[vIdx++] = Float3(-0.5f, -0.5f, 0.5f) * scale;
    boxVerts[vIdx++] = Float3(0.5f, -0.5f, 0.5f) * scale;

    // Left
    boxVerts[vIdx++] = Float3(-0.5f, 0.5f, 0.5f) * scale;
    boxVerts[vIdx++] = Float3(-0.5f, 0.5f, -0.5f) * scale;
    boxVerts[vIdx++] = Float3(-0.5f, -0.5f, -0.5f) * scale;
    boxVerts[vIdx++] = Float3(-0.5f, -0.5f, 0.5f) * scale;

    // Right
    boxVerts[vIdx++] = Float3(0.5f, 0.5f, -0.5f) * scale;
    boxVerts[vIdx++] = Float3(0.5f, 0.5f, 0.5f) * scale;
    boxVerts[vIdx++] = Float3(0.5f, -0.5f, 0.5f) * scale;
    boxVerts[vIdx++] = Float3(0.5f, -0.5f, -0.5f) * scale;

    uint64_t iIdx = 0;

    // Top
    boxIndices[iIdx++] = 0;
    boxIndices[iIdx++] = 1;
    boxIndices[iIdx++] = 2;
    boxIndices[iIdx++] = 2;
    boxIndices[iIdx++] = 3;
    boxIndices[iIdx++] = 0;

    // Bottom
    boxIndices[iIdx++] = 4 + 0;
    boxIndices[iIdx++] = 4 + 1;
    boxIndices[iIdx++] = 4 + 2;
    boxIndices[iIdx++] = 4 + 2;
    boxIndices[iIdx++] = 4 + 3;
    boxIndices[iIdx++] = 4 + 0;

    // Front
    boxIndices[iIdx++] = 8 + 0;
    boxIndices[iIdx++] = 8 + 1;
    boxIndices[iIdx++] = 8 + 2;
    boxIndices[iIdx++] = 8 + 2;
    boxIndices[iIdx++] = 8 + 3;
    boxIndices[iIdx++] = 8 + 0;

    // Back
    boxIndices[iIdx++] = 12 + 0;
    boxIndices[iIdx++] = 12 + 1;
    boxIndices[iIdx++] = 12 + 2;
    boxIndices[iIdx++] = 12 + 2;
    boxIndices[iIdx++] = 12 + 3;
    boxIndices[iIdx++] = 12 + 0;

    // Left
    boxIndices[iIdx++] = 16 + 0;
    boxIndices[iIdx++] = 16 + 1;
    boxIndices[iIdx++] = 16 + 2;
    boxIndices[iIdx++] = 16 + 2;
    boxIndices[iIdx++] = 16 + 3;
    boxIndices[iIdx++] = 16 + 0;

    // Right
    boxIndices[iIdx++] = 20 + 0;
    boxIndices[iIdx++] = 20 + 1;
    boxIndices[iIdx++] = 20 + 2;
    boxIndices[iIdx++] = 20 + 2;
    boxIndices[iIdx++] = 20 + 3;
    boxIndices[iIdx++] = 20 + 0;

    StructuredBufferInit vbInit;
    vbInit.Stride = sizeof(Float3);
    vbInit.NumElements = NumBoxVerts;
    vbInit.InitData = boxVerts.Data();
    vtxBuffer.Initialize(vbInit);

    FormattedBufferInit ibInit;
    ibInit.Format = DXGI_FORMAT_R16_UINT;
    ibInit.NumElements = NumBoxIndices;
    ibInit.InitData = boxIndices.Data();
    idxBuffer.Initialize(ibInit);
}

void MakeConeGeometry(uint64_t divisions, StructuredBuffer& vtxBuffer, FormattedBuffer& idxBuffer, Array<Float3>& positions)
{
    Assert_(divisions >= 3);

    const uint64_t numVertices = 2 + divisions;
    const uint64_t numIndices = 3 * divisions * 2;
    Assert_(numVertices <= UINT16_MAX);

    positions.Init(numVertices);
    Array<uint16_t> indices(numIndices, 0);

    // The tip
    uint16_t tipIdx = 0;
    positions[0] = Float3(0.0f, 0.0f, 0.0f);

    // The center of the base
    uint16_t centerIdx = 1;
    positions[1] =  Float3(0.0f, 0.0f, 1.0f);

    // The ring at the base
    uint16_t ringStartIdx = 2;
    for(uint64_t i = 0; i < divisions; ++i)
    {
        const float theta = (float(i) / divisions) * Pi2;
        positions[i + ringStartIdx] = Float3(std::cos(theta), std::sin(theta), 1.0f);
    }

    // Tip->ring triangles
    uint64_t currIdx = 0;
    for(uint64_t i = 0; i < divisions; ++i)
    {
        indices[currIdx++] = tipIdx;
        indices[currIdx++] = uint16_t(ringStartIdx + i);

        uint64_t prevRingIdx = i == 0 ? divisions - 1 : i - 1;
        indices[currIdx++] = uint16_t(ringStartIdx + prevRingIdx);
    }

    // Ring->center triangles
    for(uint64_t i = 0; i < divisions; ++i)
    {
        indices[currIdx++] = uint16_t(ringStartIdx + i);
        indices[currIdx++] = centerIdx;

        uint64_t prevRingIdx = i == 0 ? divisions - 1 : i - 1;
        indices[currIdx++] = uint16_t(ringStartIdx + prevRingIdx);
    }

    StructuredBufferInit vbInit;
    vbInit.Stride = sizeof(Float3);
    vbInit.NumElements = numVertices;
    vbInit.InitData = positions.Data();
    vtxBuffer.Initialize(vbInit);

    FormattedBufferInit ibInit;
    ibInit.Format = DXGI_FORMAT_R16_UINT;
    ibInit.NumElements = numIndices;
    ibInit.InitData = indices.Data();
    idxBuffer.Initialize(ibInit);
}

void MakeConeGeometry(uint64_t divisions, StructuredBuffer& vtxBuffer, FormattedBuffer& idxBuffer)
{
    Array<Float3> positions;
    MakeConeGeometry(divisions, vtxBuffer, idxBuffer, positions);
}

}