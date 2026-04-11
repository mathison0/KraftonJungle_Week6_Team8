cbuffer FDecalPassConstants : register(b7)
{
    float4x4 InvViewProj;
    float4x4 DecalWorld;
    float4x4 WorldToDecal;
    float4x4 ViewProjection;
    float4 ScreenSize; // x=width, y=height, z=1/w, w=1/h
    float4 DecalColor;
};

Texture2D SceneDepthTexture : register(t0);
Texture2D DecalTexture      : register(t1);

SamplerState PointSampler   : register(s0);
SamplerState LinearSampler  : register(s1);

float4 main(float4 PositionCS : SV_POSITION) : SV_TARGET
{
    float2 uv = PositionCS.xy * ScreenSize.zw;
    float depth = SceneDepthTexture.Load(int3((int2) PositionCS.xy, 0)).r;

    if (depth >= 1.0f)
    {
        discard;
    }

    float2 ndc;
    ndc.x = uv.x * 2.0f - 1.0f;
    ndc.y = 1.0f - uv.y * 2.0f;

    float4 clipPos = float4(ndc, depth, 1.0f);
    float4 worldPosH = mul(clipPos, InvViewProj);
    float safeW = abs(worldPosH.w) > 1e-6f ? worldPosH.w : (worldPosH.w >= 0.0f ? 1e-6f : -1e-6f);
    float3 worldPos = worldPosH.xyz / safeW;

    float3 localPos = mul(float4(worldPos, 1.0f), WorldToDecal).xyz;

    if (abs(localPos.x) > 0.5f ||
        abs(localPos.y) > 0.5f ||
        abs(localPos.z) > 0.5f)
    {
        discard;
    }

    float2 decalUV = localPos.xy + 0.5f;
    decalUV.y = 1.0f - decalUV.y;

    float4 decalSample = DecalTexture.Sample(LinearSampler, decalUV);
    return decalSample * DecalColor;
}
