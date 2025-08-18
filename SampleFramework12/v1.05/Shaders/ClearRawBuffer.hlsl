//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

//=================================================================================================
// Resources
//=================================================================================================

// Inputs
struct ClearRawBufferConstants
{
    uint4 ClearValue;
    uint DescriptorIdx;
    uint Num16ByteElements;
};

ConstantBuffer<ClearRawBufferConstants> CBuffer : register(b0);

[numthreads(TGSize_, 1, 1)]
void ClearRawBufferCS(in uint3 dispatchThreadID : SV_DispatchThreadID)
{
    RWByteAddressBuffer output = ResourceDescriptorHeap[CBuffer.DescriptorIdx];
    if(dispatchThreadID.x < CBuffer.Num16ByteElements)
        output.Store4(dispatchThreadID.x * 16, CBuffer.ClearValue);
}