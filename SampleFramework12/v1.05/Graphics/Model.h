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
#include "..\\SF12_Math.h"
#include "..\\Serialization.h"
#include "..\\Containers.h"
#include "GraphicsTypes.h"
#include "..\\Shaders\Mesh_Shared.h"

struct aiMesh;

namespace SampleFramework12
{

enum class MaterialTextures
{
    Albedo = 0,
    Normal,
    Roughness,
    Metallic,
    Opacity,
    Emissive,

    Count
};

struct MeshMaterial
{
    std::string Name;
    std::string TextureNames[uint64_t(MaterialTextures::Count)];
    const Texture* Textures[uint64_t(MaterialTextures::Count)] = { };
    uint32_t TextureIndices[uint64_t(MaterialTextures::Count)] = { };
    uint16_t Opaque = true;
    uint16_t OpacityInAlphaChannel = false;

    uint32_t Texture(MaterialTextures texType) const
    {
        Assert_(uint64_t(texType) < uint64_t(MaterialTextures::Count));
        Assert_(Textures[uint64_t(texType)] != nullptr);
        return Textures[uint64_t(texType)]->SRV;
    }

    template<typename TSerializer> void Serialize(TSerializer& serializer)
    {
        for(uint64_t i = 0; i < uint64_t(MaterialTextures::Count); ++i)
            SerializeItem(serializer, TextureNames[i]);
        BulkSerializeArray(serializer, TextureIndices, ArraySize_(TextureIndices));
        SerializeItem(serializer, Opaque);
        SerializeItem(serializer, OpacityInAlphaChannel);
    }
};

struct MeshPart
{
    uint32_t VertexStart;
    uint32_t VertexCount;
    uint32_t IndexStart;
    uint32_t IndexCount;
    uint32_t MaterialIdx;

    MeshPart() : VertexStart(0), VertexCount(0), IndexStart(0), IndexCount(0), MaterialIdx(0)
    {
    }
};

enum class IndexType
{
    Index16Bit = 0,
    Index32Bit = 1
};

enum class InputElementType : uint64_t
{
    Position = 0,
    Normal,
    Tangent,
    Bitangent,
    UV,

    NumTypes,
};

struct MaterialTexture
{
    std::string Name;
    Texture Texture;
};

struct ModelSpotLight
{
    Float3 Position;
    Float3 Intensity;
    Float3 Direction;
    Quaternion Orientation;
    Float2 AngularAttenuation;
};

struct ModelPointLight
{
    Float3 Position;
    Float3 Intensity;
};

struct ModelLoadSettings;

class Mesh
{
    friend class Model;

public:

    ~Mesh()
    {
        Assert_(numVertices == 0);
    }

    // Init from loaded files
    void InitFromAssimpMesh(const aiMesh& assimpMesh, const ModelLoadSettings& loadSettings,
                            MeshVertex* dstVertices, uint8_t* dstIndices, IndexType indexType,
                            const Float4x4& transform);

    // Procedural generation
    void InitBox(const Float3& dimensions, const Float3& position,
                 const Quaternion& orientation, uint32_t materialIdx,
                 MeshVertex* dstVertices, uint16_t* dstIndices);

    void InitPlane(const Float2& dimensions, const Float3& position,
                   const Quaternion& orientation, uint32_t materialIdx,
                   MeshVertex* dstVertices, uint16_t* dstIndices);

    void InitCommon(const MeshVertex* vertices, const uint8_t* indices, uint64_t vbAddress, uint64_t ibAddress, uint64_t vtxOffset, uint64_t idxOffset);

    void Shutdown();

    // Accessors
    const Array<MeshPart>& MeshParts() const { return meshParts; }
    uint64_t NumMeshParts() const { return meshParts.Size(); }

    uint32_t NumVertices() const { return numVertices; }
    uint32_t NumIndices() const { return numIndices; }
    uint32_t VertexOffset() const { return vtxOffset; }
    uint32_t IndexOffset() const { return idxOffset; }

    IndexType IndexBufferType() const { return indexType; }
    DXGI_FORMAT IndexBufferFormat() const { return indexType == IndexType::Index32Bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT; }
    uint32_t IndexSize() const { return indexType == IndexType::Index32Bit ? 4 : 2; }

    const MeshVertex* Vertices() const { return vertices; }
    const uint16_t* Indices() const { Assert_(indexType == IndexType::Index16Bit); return (const uint16_t*)indices; }
    const uint32_t* Indices32() const { Assert_(indexType == IndexType::Index32Bit); return (const uint32_t*)indices; }

    const uint32_t Index(uint32_t idx) const
    {
        Assert_(idx < numIndices);
        if(indexType == IndexType::Index16Bit)
            return reinterpret_cast<const uint16_t*>(indices)[idx];
        else
            return reinterpret_cast<const uint32_t*>(indices)[idx];
    }

    const D3D12_VERTEX_BUFFER_VIEW* VBView() const { return &vbView; }
    const D3D12_INDEX_BUFFER_VIEW* IBView() const { return &ibView; }

    const Float3& AABBMin() const { return aabbMin; }
    const Float3& AABBMax() const { return aabbMax; }

    uint32_t MeshletOffset() const { return meshletOffset; }
    uint32_t NumMeshlets() const { return numMeshlets; }

    static const char* InputElementTypeString(InputElementType elemType);

    template<typename TSerializer> void Serialize(TSerializer& serializer)
    {
        BulkSerializeItem(serializer, meshParts);
        SerializeItem(serializer, numVertices);
        SerializeItem(serializer, numIndices);
        SerializeItem(serializer, vtxOffset);
        SerializeItem(serializer, idxOffset);
        uint32_t idxType = uint32_t(indexType);
        SerializeItem(serializer, idxType);
        indexType = IndexType(idxType);
        SerializeItem(serializer, aabbMin);
        SerializeItem(serializer, aabbMax);
        SerializeItem(serializer, meshletOffset);
        SerializeItem(serializer, numMeshlets);
    }

protected:

    Array<MeshPart> meshParts;

    uint32_t numVertices = 0;
    uint32_t numIndices = 0;
    uint32_t vtxOffset = 0;
    uint32_t idxOffset = 0;

    IndexType indexType = IndexType::Index16Bit;

    const MeshVertex* vertices = nullptr;
    const uint8_t* indices = nullptr;

    D3D12_VERTEX_BUFFER_VIEW vbView = { };
    D3D12_INDEX_BUFFER_VIEW ibView = { };

    Float3 aabbMin;
    Float3 aabbMax;

    uint32_t meshletOffset = 0;
    uint32_t numMeshlets = 0;
};

struct ModelLoadSettings
{
    const char* FilePath = nullptr;
    const char* TextureDir = nullptr;
    float SceneScale = 1.0f;
    bool ForceSRGB = false;
    bool MergeMeshes = true;
    bool ConvertFromZUp = false;
    bool GenerateMeshlets = false;
};

struct ProceduralModelInit
{
    const MeshVertex* Vertices = nullptr;
    const uint32_t* Indices = nullptr;
    uint32_t NumVertices = 0;
    uint32_t NumIndices = 0;
    const char* TexturePaths[uint64_t(MaterialTextures::Count)] = { };
    bool ForceSRGB = false;
    bool GenerateMeshlets = false;
};

struct BoxSceneInit
{
    Float3 Dimensions = Float3(1.0f, 1.0f, 1.0f);
    Float3 Position = Float3();
    Quaternion Orientation = Quaternion();
    const char* ColorMap = "";
    const char* NormalMap = "";
    bool GenerateMeshlets = false;
};

struct BoxTestSceneInit
{
    Float3 BottomBoxDimensions = Float3(10.0f, 0.25f, 10.0f);
    Float3 BottomBoxPosition = Float3(0.0f);
    Float3 TopBoxDimensions = Float3(2.0f);
    Float3 TopBoxPosition = Float3(0.0f, 1.5f, 0.0f);
    bool GenerateMeshlets = false;
};

struct PlaneSceneInit
{
    Float2 Dimensions = Float2(1.0f, 1.0f);
    Float3 Position = Float3();
    Quaternion Orientation = Quaternion();
    const char* ColorMap = "";
    const char* NormalMap = "";
    bool GenerateMeshlets = false;
};

class Model
{
public:

    ~Model()
    {
        Assert_(meshes.Size() == 0);
    }

    // Loading from file formats
    void CreateWithAssimp(const ModelLoadSettings& settings);

    void CreateFromMeshData(const char* filePath);

    // Procedural generation
    void GenerateBoxScene(const BoxSceneInit& init);
    void GenerateBoxTestScene(const BoxTestSceneInit& init);
    void GeneratePlaneScene(const PlaneSceneInit& init);

    void CreateProcedural(const ProceduralModelInit& init);

    void Shutdown();

    // Accessors
    const Array<Mesh>& Meshes() const { return meshes; }
    uint64_t NumMeshes() const { return meshes.Size(); }

    const Float3& AABBMin() const { return aabbMin; }
    const Float3& AABBMax() const { return aabbMax; }

    const Array<MeshMaterial>& Materials() const { return meshMaterials; }
    Array<MeshMaterial>& Materials() { return meshMaterials; }
    const List<MaterialTexture*>& MaterialTextures() const { return materialTextures; }

    const Array<ModelSpotLight>& SpotLights() const { return spotLights; }
    const Array<ModelPointLight>& PointLights() const { return pointLights; }

    const StructuredBuffer& VertexBuffer() const { return vertexBuffer; }
    const FormattedBuffer& IndexBuffer() const { return indexBuffer; }

    const List<Meshlet>& Meshlets() const { return meshlets; }
    const List<uint32_t>& MeshletVertices() const { return meshletVertices; }
    const List<MeshletTriangle>& MeshletTriangles() const { return meshletTriangles; }

    const StructuredBuffer& MeshletBuffer() const { return meshletBuffer; }
    const RawBuffer& MeshletVerticesBuffer() const { return meshletVerticesBuffer; }
    const StructuredBuffer& MeshletTrianglesBuffer() const { return meshletTrianglesBuffer; }
    const StructuredBuffer& MeshletBoundsBuffer() const { return meshletBoundsBuffer; }

    const MeshVertex* Vertices() const { return vertices.Data(); }
    const uint16_t* Indices() const { Assert_(indexType == IndexType::Index16Bit); return (const uint16_t*)indices.Data(); }
    const uint32_t* Indices32() const { Assert_(indexType == IndexType::Index32Bit); return (const uint32_t*)indices.Data(); }

    static const D3D12_INPUT_ELEMENT_DESC* InputElements();
    static const InputElementType* InputElementTypes();
    static uint64_t NumInputElements();

    IndexType IndexBufferType() const { return indexType; }
    DXGI_FORMAT IndexBufferFormat() const { return indexType == IndexType::Index32Bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT; }
    uint32_t IndexSize() const { return indexType == IndexType::Index32Bit ? 4 : 2; }

    // Serialization
    template<typename TSerializer>
    void Serialize(TSerializer& serializer)
    {
        SerializeItem(serializer, meshes);
        SerializeItem(serializer, meshMaterials);
        BulkSerializeItem(serializer, spotLights);
        BulkSerializeItem(serializer, pointLights);
        SerializeItem(serializer, textureDirectory);
        SerializeItem(serializer, forceSRGB);
        SerializeItem(serializer, aabbMin);
        SerializeItem(serializer, aabbMax);
        BulkSerializeItem(serializer, vertices);
        BulkSerializeItem(serializer, indices);
        uint32_t idxType = uint32_t(indexType);
        SerializeItem(serializer, idxType);
        indexType = IndexType(idxType);
        BulkSerializeItem(serializer, meshlets);
        BulkSerializeItem(serializer, meshletVertices);
        BulkSerializeItem(serializer, meshletTriangles);
        BulkSerializeItem(serializer, meshletBounds);
    }

protected:

    void GenerateMeshlets();
    void CreateBuffers();

    Array<Mesh> meshes;
    Array<MeshMaterial> meshMaterials;
    Array<ModelSpotLight> spotLights;
    Array<ModelPointLight> pointLights;
    std::string textureDirectory;
    bool32 forceSRGB = false;
    Float3 aabbMin;
    Float3 aabbMax;

    StructuredBuffer vertexBuffer;
    FormattedBuffer indexBuffer;
    Array<MeshVertex> vertices;
    Array<uint8_t> indices;
    IndexType indexType = IndexType::Index16Bit;

    List<Meshlet> meshlets;
    List<uint32_t> meshletVertices;
    List<MeshletTriangle> meshletTriangles;
    List<MeshletBounds> meshletBounds;

    StructuredBuffer meshletBuffer;
    RawBuffer meshletVerticesBuffer;
    StructuredBuffer meshletTrianglesBuffer;
    StructuredBuffer meshletBoundsBuffer;

    List<MaterialTexture*> materialTextures;
};

void MakeSphereGeometry(uint64_t uDivisions, uint64_t vDivisions, StructuredBuffer& vtxBuffer, FormattedBuffer& idxBuffer);
void MakeBoxGeometry(StructuredBuffer& vtxBuffer, FormattedBuffer& idxBuffer, float scale = 1.0f);
void MakeConeGeometry(uint64_t divisions, StructuredBuffer& vtxBuffer, FormattedBuffer& idxBuffer, Array<Float3>& positions);
void MakeConeGeometry(uint64_t divisions, StructuredBuffer& vtxBuffer, FormattedBuffer& idxBuffer);

}