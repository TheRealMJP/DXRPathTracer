//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
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
#include "Textures.h"

using std::string;
using std::wstring;
using std::vector;
using std::map;
using std::wifstream;

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

static const wchar* DefaultTextures[] =
{
    L"..\\Content\\Textures\\DefaultBaseColor.dds",     // Albedo
    L"..\\Content\\Textures\\DefaultNormalMap.dds",     // Normal
    L"..\\Content\\Textures\\DefaultRoughness.dds",     // Roughness
    L"..\\Content\\Textures\\DefaultBlack.dds",         // Metallic
    L"..\\Content\\Textures\\Default.dds",              // Opacity
    L"..\\Content\\Textures\\DefaultBlack.dds",         // Emissive
};

StaticAssert_(ArraySize_(DefaultTextures) == uint64(MaterialTextures::Count));

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

void LoadMaterialResources(Array<MeshMaterial>& materials, const wstring& directory, bool32 forceSRGB,
                           GrowableList<MaterialTexture*>& materialTextures)
{
    const uint64 numMaterials = materials.Size();
    for(uint64 matIdx = 0; matIdx < numMaterials; ++matIdx)
    {
        MeshMaterial& material = materials[matIdx];
        for(uint64 texType = 0; texType < uint64(MaterialTextures::Count); ++texType)
        {
            material.Textures[texType] = nullptr;

            wstring path = directory + material.TextureNames[texType];
            if(material.TextureNames[texType].length() == 0 || FileExists(path.c_str()) == false)
                path = DefaultTextures[texType];

            const uint64 numLoaded = materialTextures.Count();
            for(uint64 i = 0; i < numLoaded; ++i)
            {
                if(materialTextures[i]->Name == path)
                {
                    material.Textures[texType] = &materialTextures[i]->Texture;
                    material.TextureIndices[texType] = uint32(i);
                    break;
                }
            }

            if(material.Textures[texType] == nullptr)
            {
                MaterialTexture* newMatTexture = new MaterialTexture();
                newMatTexture->Name = path;
                bool useSRGB = forceSRGB && texType == uint64(MaterialTextures::Albedo);
                LoadTexture(newMatTexture->Texture, path.c_str(), useSRGB ? true : false);
                uint64 idx = materialTextures.Add(newMatTexture);

                material.Textures[texType] = &newMatTexture->Texture;
                material.TextureIndices[texType] = uint32(idx);
            }
        }
    }
}

void Mesh::InitFromAssimpMesh(const aiMesh& assimpMesh, float sceneScale, MeshVertex* dstVertices, uint8* dstIndices, IndexType indexType_)
{
    numVertices = assimpMesh.mNumVertices;
    numIndices = assimpMesh.mNumFaces * 3;
    indexType = indexType_;

    if(assimpMesh.HasPositions())
    {
        // Compute the AABB of the mesh, and copy the positions
        aabbMin = FloatMax;
        aabbMax = -FloatMax;

        for(uint64 i = 0; i < numVertices; ++i)
        {
            Float3 position = ConvertVector(assimpMesh.mVertices[i]) * sceneScale;
            aabbMin.x = Min(aabbMin.x, position.x);
            aabbMin.y = Min(aabbMin.y, position.y);
            aabbMin.z = Min(aabbMin.z, position.z);

            aabbMax.x = Max(aabbMax.x, position.x);
            aabbMax.y = Max(aabbMax.y, position.y);
            aabbMax.z = Max(aabbMax.z, position.z);

            dstVertices[i].Position = position;
        }
    }

    if(assimpMesh.HasNormals())
    {
        for(uint64 i = 0; i < numVertices; ++i)
            dstVertices[i].Normal = ConvertVector(assimpMesh.mNormals[i]);
    }

    if(assimpMesh.HasTextureCoords(0))
    {
        for(uint64 i = 0; i < numVertices; ++i)
            dstVertices[i].UV = ConvertVector(assimpMesh.mTextureCoords[0][i]).To2D();
    }

    if(assimpMesh.HasTangentsAndBitangents())
    {
        for(uint64 i = 0; i < numVertices; ++i)
        {
            dstVertices[i].Tangent = ConvertVector(assimpMesh.mTangents[i]);
            dstVertices[i].Bitangent = ConvertVector(assimpMesh.mBitangents[i]) * -1.0f;
        }
    }

    // Copy the index data
    const uint64 numTriangles = assimpMesh.mNumFaces;
    if(indexType_ == IndexType::Index16Bit)
    {
        uint16* dstIndices16 = (uint16*)dstIndices;
        for(uint64 triIdx = 0; triIdx < numTriangles; ++triIdx)
        {
            dstIndices16[triIdx * 3 + 0] = uint16(assimpMesh.mFaces[triIdx].mIndices[0]);
            dstIndices16[triIdx * 3 + 1] = uint16(assimpMesh.mFaces[triIdx].mIndices[1]);
            dstIndices16[triIdx * 3 + 2] = uint16(assimpMesh.mFaces[triIdx].mIndices[2]);
        }
    }
    else
    {
        uint32* dstIndices32 = (uint32*)dstIndices;
        for (uint64 triIdx = 0; triIdx < numTriangles; ++triIdx)
        {
            dstIndices32[triIdx * 3 + 0] = uint32(assimpMesh.mFaces[triIdx].mIndices[0]);
            dstIndices32[triIdx * 3 + 1] = uint32(assimpMesh.mFaces[triIdx].mIndices[1]);
            dstIndices32[triIdx * 3 + 2] = uint32(assimpMesh.mFaces[triIdx].mIndices[2]);
        }
    }

    const uint64 numSubsets = 1;
    meshParts.Init(1);
    MeshPart& part = meshParts[0];
    part.IndexStart = 0;
    part.IndexCount = numIndices;
    part.VertexStart = 0;
    part.VertexCount = numVertices;
    part.MaterialIdx = assimpMesh.mMaterialIndex;
}

static const uint64 NumBoxVerts = 24;
static const uint64 NumBoxIndices = 36;

// Initializes the mesh as a box
void Mesh::InitBox(const Float3& dimensions, const Float3& position,
                   const Quaternion& orientation, uint32 materialIdx,
                   MeshVertex* dstVertices, uint16* dstIndices)
{
    uint64 vIdx = 0;

    // Top
    dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, 1.0f, 1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(0.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(1.0f, 1.0f, 1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(1.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(1.0f, 1.0f, -1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(1.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, 1.0f, -1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(0.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f));

    // Bottom
    dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, -1.0f, -1.0f), Float3(0.0f, -1.0f, 0.0f), Float2(0.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, 1.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(1.0f, -1.0f, -1.0f), Float3(0.0f, -1.0f, 0.0f), Float2(1.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, 1.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(1.0f, -1.0f, 1.0f), Float3(0.0f, -1.0f, 0.0f), Float2(1.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, 1.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, -1.0f, 1.0f), Float3(0.0f, -1.0f, 0.0f), Float2(0.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, 1.0f));

    // Front
    dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, 1.0f, -1.0f), Float3(0.0f, 0.0f, -1.0f), Float2(0.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(1.0f, 1.0f, -1.0f), Float3(0.0f, 0.0f, -1.0f), Float2(1.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(1.0f, -1.0f, -1.0f), Float3(0.0f, 0.0f, -1.0f), Float2(1.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, -1.0f, -1.0f), Float3(0.0f, 0.0f, -1.0f), Float2(0.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f));

    // Back
    dstVertices[vIdx++] = MeshVertex(Float3(1.0f, 1.0f, 1.0f), Float3(0.0f, 0.0f, 1.0f), Float2(0.0f, 0.0f), Float3(-1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, 1.0f, 1.0f), Float3(0.0f, 0.0f, 1.0f), Float2(1.0f, 0.0f), Float3(-1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, -1.0f, 1.0f), Float3(0.0f, 0.0f, 1.0f), Float2(1.0f, 1.0f), Float3(-1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(1.0f, -1.0f, 1.0f), Float3(0.0f, 0.0f, 1.0f), Float2(0.0f, 1.0f), Float3(-1.0f, 0.0f, 0.0f), Float3(0.0f, -1.0f, 0.0f));

    // Left
    dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, 1.0f, 1.0f), Float3(-1.0f, 0.0f, 0.0f), Float2(0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f), Float3(0.0f, -1.0f, 0.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, 1.0f, -1.0f), Float3(-1.0f, 0.0f, 0.0f), Float2(1.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f), Float3(0.0f, -1.0f, 0.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, -1.0f, -1.0f), Float3(-1.0f, 0.0f, 0.0f), Float2(1.0f, 1.0f), Float3(0.0f, 0.0f, -1.0f), Float3(0.0f, -1.0f, 0.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, -1.0f, 1.0f), Float3(-1.0f, 0.0f, 0.0f), Float2(0.0f, 1.0f), Float3(0.0f, 0.0f, -1.0f), Float3(0.0f, -1.0f, 0.0f));

    // Right
    dstVertices[vIdx++] = MeshVertex(Float3(1.0f, 1.0f, -1.0f), Float3(1.0f, 0.0f, 0.0f), Float2(0.0f, 0.0f), Float3(0.0f, 0.0f, 1.0f), Float3(0.0f, -1.0f, 0.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(1.0f, 1.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float2(1.0f, 0.0f), Float3(0.0f, 0.0f, 1.0f), Float3(0.0f, -1.0f, 0.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(1.0f, -1.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float2(1.0f, 1.0f), Float3(0.0f, 0.0f, 1.0f), Float3(0.0f, -1.0f, 0.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(1.0f, -1.0f, -1.0f), Float3(1.0f, 0.0f, 0.0f), Float2(0.0f, 1.0f), Float3(0.0f, 0.0f, 1.0f), Float3(0.0f, -1.0f, 0.0f));

    for(uint64 i = 0; i < NumBoxVerts; ++i)
        dstVertices[i].Transform(position, dimensions * 0.5f, orientation);

    uint64 iIdx = 0;

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

    const uint32 indexSize = 2;
    indexType = IndexType::Index16Bit;

    numVertices = uint32(NumBoxVerts);
    numIndices = uint32(NumBoxIndices);

    meshParts.Init(1);
    MeshPart& part = meshParts[0];
    part.IndexStart = 0;
    part.IndexCount = numIndices;
    part.VertexStart = 0;
    part.VertexCount = numVertices;
    part.MaterialIdx = materialIdx;

    aabbMin = -dimensions * 0.5f;
    aabbMax = dimensions * 0.5f;
}

const uint64 NumPlaneVerts = 4;
const uint64 NumPlaneIndices = 6;

// Initializes the mesh as a plane
void Mesh::InitPlane(const Float2& dimensions, const Float3& position, const Quaternion& orientation, uint32 materialIdx,
                     MeshVertex* dstVertices, uint16* dstIndices)
{
    uint64 vIdx = 0;

    dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, 0.0f, 1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(0.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(1.0f, 0.0f, 1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(1.0f, 0.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(1.0f, 0.0f, -1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(1.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f));
    dstVertices[vIdx++] = MeshVertex(Float3(-1.0f, 0.0f, -1.0f), Float3(0.0f, 1.0f, 0.0f), Float2(0.0f, 1.0f), Float3(1.0f, 0.0f, 0.0f), Float3(0.0f, 0.0f, -1.0f));

    for(uint64 i = 0; i < NumPlaneVerts; ++i)
        dstVertices[i].Transform(position, Float3(dimensions.x, 1.0f, dimensions.y) * 0.5f, orientation);

    uint64 iIdx = 0;
    dstIndices[iIdx++] = 0;
    dstIndices[iIdx++] = 1;
    dstIndices[iIdx++] = 2;
    dstIndices[iIdx++] = 2;
    dstIndices[iIdx++] = 3;
    dstIndices[iIdx++] = 0;

    const uint32 indexSize = 2;
    indexType = IndexType::Index16Bit;

    numVertices = uint32(NumPlaneVerts);
    numIndices = uint32(NumPlaneIndices);

    meshParts.Init(1);
    MeshPart& part = meshParts[0];
    part.IndexStart = 0;
    part.IndexCount = numIndices;
    part.VertexStart = 0;
    part.VertexCount = numVertices;
    part.MaterialIdx = materialIdx;

    aabbMin = Float3(-dimensions.x, 0.0f, -dimensions.y) * 0.5f;
    aabbMax = Float3(dimensions.x, 0.0f, dimensions.y) * 0.5f;
}

void Mesh::InitCommon(const MeshVertex* vertices_, const uint8* indices_, uint64 vbAddress, uint64 ibAddress, uint64 vtxOffset_, uint64 idxOffset_)
{
    Assert_(meshParts.Size() > 0);

    vertices = vertices_;
    indices = indices_;
    vtxOffset = uint32(vtxOffset_);
    idxOffset = uint32(idxOffset_);

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

    StaticAssert_(ArraySize_(ElemStrings) == uint64(InputElementType::NumTypes));
    Assert_(uint64(elemType) < uint64(InputElementType::NumTypes));

    return ElemStrings[uint64(elemType)];
}

// == Model =======================================================================================

void Model::CreateWithAssimp(const ModelLoadSettings& settings)
{
    const wchar* filePath = settings.FilePath;
    Assert_(filePath != nullptr);
    if(FileExists(filePath) == false)
        throw Exception(MakeString(L"Model file with path '%ls' does not exist", filePath));

    WriteLog("Loading scene '%ls' with Assimp...", filePath);

    std::string fileNameAnsi = WStringToAnsi(filePath);

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(fileNameAnsi, 0);

    if(scene == nullptr)
        throw Exception(L"Failed to load scene " + std::wstring(filePath) +
                        L": " + AnsiToWString(importer.GetErrorString()));

    if(scene->mNumMeshes == 0)
        throw Exception(L"Scene " + std::wstring(filePath) + L" has no meshes");

    if(scene->mNumMaterials == 0)
        throw Exception(L"Scene " + std::wstring(filePath) + L" has no materials");

    fileDirectory = GetDirectoryFromFilePath(filePath);
    forceSRGB = settings.ForceSRGB;

    // Grab the lights before we process the scene
    spotLights.Init(scene->mNumLights);
    pointLights.Init(scene->mNumLights);

    uint64 numSpotLights = 0;
    uint64 numPointLights = 0;
    for(uint64 i = 0; i < scene->mNumLights; ++i)
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
            PointLight& dstLight = pointLights[numPointLights++];
            dstLight.Position = ConvertVector(srcLight.mPosition);
            dstLight.Intensity = ConvertColor(srcLight.mColorDiffuse);
        }
    }

    spotLights.Resize(numSpotLights);
    pointLights.Resize(numPointLights);

    // Post-process the scene
    uint32 flags = aiProcess_CalcTangentSpace |
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
    const uint64 numMaterials = scene->mNumMaterials;
    meshMaterials.Init(numMaterials);
    for(uint64 i = 0; i < numMaterials; ++i)
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
            material.TextureNames[uint64(MaterialTextures::Albedo)] = GetFileName(AnsiToWString(diffuseTexPath.C_Str()).c_str());

        if(mat.GetTexture(aiTextureType_NORMALS, 0, &normalMapPath) == aiReturn_SUCCESS
           || mat.GetTexture(aiTextureType_HEIGHT, 0, &normalMapPath) == aiReturn_SUCCESS)
            material.TextureNames[uint64(MaterialTextures::Normal)] = GetFileName(AnsiToWString(normalMapPath.C_Str()).c_str());

        if(mat.GetTexture(aiTextureType_SHININESS, 0, &roughnessMapPath) == aiReturn_SUCCESS)
            material.TextureNames[uint64(MaterialTextures::Roughness)] = GetFileName(AnsiToWString(roughnessMapPath.C_Str()).c_str());

        if(mat.GetTexture(aiTextureType_AMBIENT, 0, &metallicMapPath) == aiReturn_SUCCESS)
            material.TextureNames[uint64(MaterialTextures::Metallic)] = GetFileName(AnsiToWString(metallicMapPath.C_Str()).c_str());

        if(mat.GetTexture(aiTextureType_OPACITY, 0, &opacityMapPath) == aiReturn_SUCCESS)
            material.TextureNames[uint64(MaterialTextures::Opacity)] = GetFileName(AnsiToWString(opacityMapPath.C_Str()).c_str());

        if(mat.GetTexture(aiTextureType_EMISSIVE, 0, &emissiveMapPath) == aiReturn_SUCCESS)
            material.TextureNames[uint64(MaterialTextures::Emissive)] = GetFileName(AnsiToWString(emissiveMapPath.C_Str()).c_str());
    }

    std::wstring textureDir = settings.TextureDir ? fileDirectory + L"\\" + settings.TextureDir + L"\\" : fileDirectory;
    LoadMaterialResources(meshMaterials, textureDir, settings.ForceSRGB, materialTextures);

    aabbMin = FloatMax;
    aabbMax = -FloatMax;

    indexType = IndexType::Index16Bit;

    // Initialize the meshes
    const uint64 numMeshes = scene->mNumMeshes;
    uint64 numVertices = 0;
    uint64 numIndices = 0;
    for(uint64 i = 0; i < numMeshes; ++i)
    {
        const aiMesh& assimpMesh = *scene->mMeshes[i];

        numVertices += assimpMesh.mNumVertices;
        numIndices += assimpMesh.mNumFaces * 3;

        if(assimpMesh.mNumFaces * 3 > 0xFFFF)
            indexType = IndexType::Index32Bit;
    }


    const uint64 indexSize = indexType == IndexType::Index32Bit ? 4 : 2;

    vertices.Init(numVertices);
    indices.Init(numIndices * indexSize);

    meshes.Init(numMeshes);
    uint64 vtxOffset = 0;
    uint64 idxOffset = 0;
    for(uint64 i = 0; i < numMeshes; ++i)
    {
        meshes[i].InitFromAssimpMesh(*scene->mMeshes[i], settings.SceneScale, &vertices[vtxOffset], &indices[idxOffset], indexType);

        aabbMin.x = Min(aabbMin.x, meshes[i].AABBMin().x);
        aabbMin.y = Min(aabbMin.y, meshes[i].AABBMin().y);
        aabbMin.z = Min(aabbMin.z, meshes[i].AABBMin().z);

        aabbMax.x = Max(aabbMax.x, meshes[i].AABBMax().x);
        aabbMax.y = Max(aabbMax.y, meshes[i].AABBMax().y);
        aabbMax.z = Max(aabbMax.z, meshes[i].AABBMax().z);

        vtxOffset += meshes[i].NumVertices();
        idxOffset += meshes[i].NumIndices() * indexSize;
    }

    CreateBuffers();

    WriteLog("Finished loading scene '%ls'", filePath);
}

void Model::CreateFromMeshData(const wchar* filePath)
{
    if(FileExists(filePath) == false)
        throw Exception(MakeString(L"Model file with path '%ls' does not exist", filePath));

    fileDirectory = GetDirectoryFromFilePath(filePath);

    FileReadSerializer serializer(filePath);
    Serialize(serializer);

    CreateBuffers();

    LoadMaterialResources(meshMaterials, fileDirectory, forceSRGB, materialTextures);
}

void Model::GenerateBoxScene(const Float3& dimensions, const Float3& position,
                             const Quaternion& orientation, const wchar* colorMap,
                             const wchar* normalMap)
{
    meshMaterials.Init(1);
    MeshMaterial& material = meshMaterials[0];
    material.TextureNames[uint64(MaterialTextures::Albedo)] = colorMap;
    material.TextureNames[uint64(MaterialTextures::Normal)] = normalMap;
    fileDirectory = L"..\\Content\\Textures\\";
    LoadMaterialResources(meshMaterials, L"..\\Content\\Textures\\", false, materialTextures);

    indexType = IndexType::Index16Bit;

    vertices.Init(NumBoxVerts);
    indices.Init(NumBoxIndices * sizeof(uint16));

    meshes.Init(1);
    meshes[0].InitBox(dimensions, position, orientation, 0, vertices.Data(), (uint16*)indices.Data());

    CreateBuffers();
}

void Model::GenerateBoxTestScene()
{
    meshMaterials.Init(1);
    MeshMaterial& material = meshMaterials[0];
    material.TextureNames[uint64(MaterialTextures::Albedo)] = L"White.png";
    material.TextureNames[uint64(MaterialTextures::Normal)] = L"Hex.png";
    fileDirectory = L"..\\Content\\Textures\\";
    LoadMaterialResources(meshMaterials, L"..\\Content\\Textures\\", false, materialTextures);

    indexType = IndexType::Index16Bit;

    vertices.Init(NumBoxVerts * 2);
    indices.Init(NumBoxIndices * 2 * sizeof(uint16));

    meshes.Init(2);
    meshes[0].InitBox(Float3(2.0f), Float3(0.0f, 1.5f, 0.0f), Quaternion(), 0, vertices.Data(), (uint16*)indices.Data());
    meshes[1].InitBox(Float3(10.0f, 0.25f, 10.0f), Float3(0.0f), Quaternion(), 0, &vertices[NumBoxVerts], (uint16*)&indices[NumBoxIndices * sizeof(uint16)]);

    CreateBuffers();
}

void Model::GeneratePlaneScene(const Float2& dimensions, const Float3& position,
                               const Quaternion& orientation, const wchar* colorMap,
                               const wchar* normalMap)
{
    meshMaterials.Init(1);
    MeshMaterial& material = meshMaterials[0];
    material.TextureNames[uint64(MaterialTextures::Albedo)] = colorMap;
    material.TextureNames[uint64(MaterialTextures::Normal)] = normalMap;
    fileDirectory = L"..\\Content\\Textures\\";
    LoadMaterialResources(meshMaterials, L"..\\Content\\Textures\\", false, materialTextures);

    indexType = IndexType::Index16Bit;

    vertices.Init(NumPlaneVerts);
    indices.Init(NumPlaneIndices * sizeof(uint16));

    meshes.Init(1);
    meshes[0].InitPlane(dimensions, position, orientation, 0, vertices.Data(), (uint16*)indices.Data());

    CreateBuffers();
}

void Model::Shutdown()
{
    for(uint64 i = 0; i < meshes.Size(); ++i)
        meshes[i].Shutdown();
    meshes.Shutdown();
    meshMaterials.Shutdown();
    for(uint64 i = 0; i < materialTextures.Count(); ++i)
    {
        materialTextures[i]->Texture.Shutdown();
        delete materialTextures[i];
        materialTextures[i] = nullptr;
    }
    materialTextures.Shutdown();
    fileDirectory = L"";
    forceSRGB = false;

    vertexBuffer.Shutdown();
    indexBuffer.Shutdown();
    vertices.Shutdown();
    indices.Shutdown();
}

const D3D12_INPUT_ELEMENT_DESC* Model::InputElements()
{
    return StandardInputElements;
}

const InputElementType* Model::InputElementTypes()
{
    return StandardInputElementTypes;
}

uint64 Model::NumInputElements()
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
    vertexBuffer.Initialize(sbInit);

    const uint32 indexSize = IndexSize();

    FormattedBufferInit fbInit;
    fbInit.Format = IndexBufferFormat();
    fbInit.NumElements = indices.Size() / indexSize;
    fbInit.InitData = indices.Data();
    indexBuffer.Initialize(fbInit);

    uint64 vtxOffset = 0;
    uint64 idxOffset = 0;
    const uint64 numMeshes = meshes.Size();
    for(uint64 i = 0; i < numMeshes; ++i)
    {
        uint64 vbOffset = vtxOffset * sizeof(MeshVertex);
        uint64 ibOffset = idxOffset * indexSize;
        meshes[i].InitCommon(&vertices[vtxOffset], &indices[ibOffset], vertexBuffer.GPUAddress + vbOffset, indexBuffer.GPUAddress + ibOffset, vtxOffset, idxOffset);

        vtxOffset += meshes[i].NumVertices();
        idxOffset += meshes[i].NumIndices();
    }
}

// == Geometry helpers ============================================================================

void MakeSphereGeometry(uint64 uDivisions, uint64 vDivisions, StructuredBuffer& vtxBuffer, FormattedBuffer& idxBuffer)
{
    Assert_(uDivisions >= 3);
    Assert_(vDivisions >= 3);

    const uint64 numSphereVerts = (vDivisions - 1) * uDivisions + 2;
    Assert_(numSphereVerts <= UINT16_MAX);

    Array<Float3> sphereVerts(numSphereVerts);
    uint64 currSphereVert = 0;

    // Add the vert at the top
    sphereVerts[currSphereVert++] = Float3(0.0f, 0.0f, 1.0f);

    // Add the rings
    for(uint64 v = 1; v < vDivisions; ++v)
    {
        for(uint64 u = 0; u < uDivisions; ++u)
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
    const uint64 lastVertIdx = currSphereVert;
    sphereVerts[currSphereVert++] = Float3(0.0f, 0.0f, -1.0f);
    Assert_(currSphereVert == numSphereVerts);

    // Add the top ring of triangles
    GrowableList<uint16> sphereIndices;
    for(uint64 u = 0; u < uDivisions; ++u)
    {
        sphereIndices.Add(uint16(0));
        sphereIndices.Add(uint16(u + 1));

        if(u < uDivisions - 1)
            sphereIndices.Add(uint16(u + 2));
        else
            sphereIndices.Add(uint16(1));
    }

    // Add the rest of the rings
    uint64 prevRowStart = 1;
    uint64 currRowStart = uDivisions + 1;
    for(uint64 v = 1; v < vDivisions - 1; ++v)
    {
        for(uint64 u = 0; u < uDivisions; ++u)
        {
            uint64 nextBottom = currRowStart + u + 1;
            uint64 nextTop = prevRowStart + u + 1;
            if(u == uDivisions - 1)
            {
                nextBottom = currRowStart;
                nextTop = prevRowStart;
            }

            sphereIndices.Add(uint16(prevRowStart + u));
            sphereIndices.Add(uint16(currRowStart + u));
            sphereIndices.Add(uint16(nextBottom));
            sphereIndices.Add(uint16(nextBottom));
            sphereIndices.Add(uint16(nextTop));
            sphereIndices.Add(uint16(prevRowStart + u));
        }

        prevRowStart = currRowStart;
        currRowStart += uint64(uDivisions);
    }

    // Add the last ring at the bottom
    const uint64 lastRingStart = uint64(lastVertIdx - uDivisions);
    for(uint64 u = 0; u < uDivisions; ++u)
    {
        sphereIndices.Add(uint16(lastVertIdx));
        sphereIndices.Add(uint16(lastRingStart + u));

        if(u < uDivisions - 1)
            sphereIndices.Add(uint16(lastRingStart + u + 1));
        else
            sphereIndices.Add(uint16(lastRingStart));
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
    Array<uint16> boxIndices(NumBoxIndices, 0);

    uint64 vIdx = 0;

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

    uint64 iIdx = 0;

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

void MakeConeGeometry(uint64 divisions, StructuredBuffer& vtxBuffer, FormattedBuffer& idxBuffer, Array<Float3>& positions)
{
    Assert_(divisions >= 3);

    const uint64 numVertices = 2 + divisions;
    const uint64 numIndices = 3 * divisions * 2;
    Assert_(numVertices <= UINT16_MAX);

    positions.Init(numVertices);
    Array<uint16> indices(numIndices, 0);

    // The tip
    uint16 tipIdx = 0;
    positions[0] = Float3(0.0f, 0.0f, 0.0f);

    // The center of the base
    uint16 centerIdx = 1;
    positions[1] =  Float3(0.0f, 0.0f, 1.0f);

    // The ring at the base
    uint16 ringStartIdx = 2;
    for(uint64 i = 0; i < divisions; ++i)
    {
        const float theta = (float(i) / divisions) * Pi2;
        positions[i + ringStartIdx] = Float3(std::cos(theta), std::sin(theta), 1.0f);
    }

    // Tip->ring triangles
    uint64 currIdx = 0;
    for(uint64 i = 0; i < divisions; ++i)
    {
        indices[currIdx++] = tipIdx;
        indices[currIdx++] = uint16(ringStartIdx + i);

        uint64 prevRingIdx = i == 0 ? divisions - 1 : i - 1;
        indices[currIdx++] = uint16(ringStartIdx + prevRingIdx);
    }

    // Ring->center triangles
    for(uint64 i = 0; i < divisions; ++i)
    {
        indices[currIdx++] = uint16(ringStartIdx + i);
        indices[currIdx++] = centerIdx;

        uint64 prevRingIdx = i == 0 ? divisions - 1 : i - 1;
        indices[currIdx++] = uint16(ringStartIdx + prevRingIdx);
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

void MakeConeGeometry(uint64 divisions, StructuredBuffer& vtxBuffer, FormattedBuffer& idxBuffer)
{
    Array<Float3> positions;
    MakeConeGeometry(divisions, vtxBuffer, idxBuffer, positions);
}

}