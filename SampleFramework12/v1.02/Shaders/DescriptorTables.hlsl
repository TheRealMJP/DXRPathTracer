//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

// This is the standard set of descriptor tables that shaders use for accessing the SRV's that they need.
// These must match "NumStandardDescriptorRanges" and "StandardDescriptorRanges()" from DX12_Helpers.h
Texture2D Tex2DTable[] : register(t0, space0);
Texture2DArray Tex2DArrayTable[] : register(t0, space1);
TextureCube TexCubeTable[] : register(t0, space2);
Texture3D Tex3DTable[] : register(t0, space3);
Texture2DMS<float4> Tex2DMSTable[] : register(t0, space4);
ByteAddressBuffer RawBufferTable[] : register(t0, space5);
Buffer<uint> BufferUintTable[] : register(t0, space6);