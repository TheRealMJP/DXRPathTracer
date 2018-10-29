//=================================================================================================
//
//	MJP's DX11 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

//=================================================================================================
// Input/Output structs
//=================================================================================================
struct VSOutput
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};

//=================================================================================================
// Vertex Shader
//=================================================================================================
VSOutput FullScreenTriangleVS(in uint VertexIdx : SV_VertexID)
{
    VSOutput output;

    if(VertexIdx == 0)
    {
        output.Position = float4(-1.0f, 1.0f, 1.0f, 1.0f);
        output.TexCoord = float2(0.0f, 0.0f);
    }
    else if(VertexIdx == 1)
    {
        output.Position = float4(3.0f, 1.0f, 1.0f, 1.0f);
        output.TexCoord = float2(2.0f, 0.0f);
    }
    else
    {
        output.Position = float4(-1.0f, -3.0f, 1.0f, 1.0f);
        output.TexCoord = float2(0.0f, 2.0f);
    }

    return output;
}