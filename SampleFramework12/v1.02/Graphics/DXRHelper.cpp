//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"
#include "DXRHelper.h"
#include "DX12.h"
#include "../Exceptions.h"

namespace SampleFramework12
{

static const uint64 MaxSubObjDescSize = sizeof(D3D12_HIT_GROUP_DESC);

void StateObjectBuilder::Init(uint64 maxSubObjects)
{
    Assert_(maxSubObjects > 0);

    MaxSubObjects = maxSubObjects;
    SubObjectData.Init(maxSubObjects * MaxSubObjDescSize, 0);

    D3D12_STATE_SUBOBJECT defSubObj = { };
    SubObjects.Init(maxSubObjects, defSubObj);
}

const D3D12_STATE_SUBOBJECT* StateObjectBuilder::AddSubObject(const void* subObjDesc, uint64 subObjDescSize, D3D12_STATE_SUBOBJECT_TYPE type)
{
    Assert_(subObjDesc != nullptr);
    Assert_(subObjDescSize > 0);
    Assert_(type < D3D12_STATE_SUBOBJECT_TYPE_MAX_VALID);
    Assert_(subObjDescSize <= MaxSubObjDescSize);
    Assert_(NumSubObjects < MaxSubObjects);

    const uint64 subObjOffset = NumSubObjects * MaxSubObjDescSize;
    memcpy(SubObjectData.Data() + subObjOffset, subObjDesc, subObjDescSize);

    D3D12_STATE_SUBOBJECT& newSubObj = SubObjects[NumSubObjects];
    newSubObj.Type = type;
    newSubObj.pDesc = SubObjectData.Data() + subObjOffset;

    NumSubObjects += 1;

    return &newSubObj;
}

void StateObjectBuilder::BuildDesc(D3D12_STATE_OBJECT_TYPE type, D3D12_STATE_OBJECT_DESC& desc)
{
    desc.Type = type;
    desc.NumSubobjects = uint32(NumSubObjects);
    desc.pSubobjects = NumSubObjects ? SubObjects.Data() : nullptr;
}

ID3D12StateObject* StateObjectBuilder::CreateStateObject(D3D12_STATE_OBJECT_TYPE type)
{
    D3D12_STATE_OBJECT_DESC desc = { };
    BuildDesc(type, desc);

    ID3D12StateObject* stateObj = nullptr;
    DXCall(DX12::Device->CreateStateObject(&desc, IID_PPV_ARGS(&stateObj)));

    return stateObj;
}

} // namespace SampleFramework12