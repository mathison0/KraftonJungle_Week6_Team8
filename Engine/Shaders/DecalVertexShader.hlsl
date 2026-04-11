cbuffer FDecalPassConstants : register(b2)
{
    row_major float4x4 InvViewProj;

    row_major float4x4 DecalWorld;
    row_major float4x4 WorldToDecal;
    row_major float4x4 ViewProjection;

    float4 ScreenSize; // x=width, y=height, z=1/w, w=1/h
    float4 DecalColor;
};

struct VSInput
{
    float3 Position : POSITION;
};

struct PSInput
{
    float4 PositionCS : SV_POSITION;
};

PSInput main(VSInput In)
{
    PSInput Out;

    float4 worldPos = mul(float4(In.Position, 1.0f), DecalWorld);
    Out.PositionCS = mul(worldPos, ViewProjection);

    return Out;
}