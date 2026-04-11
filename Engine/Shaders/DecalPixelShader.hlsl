cbuffer FDecalPassConstants : register(b0)
{
    row_major float4x4 InvViewProj;

    row_major float4x4 DecalWorld;
    row_major float4x4 WorldToDecal;
    row_major float4x4 ViewProjection;

    float4 ScreenSize; // x=width, y=height, z=1/w, w=1/h
    float4 DecalColor;
};

Texture2D SceneDepthTexture : register(t0);
Texture2D DecalTexture      : register(t1);

SamplerState PointerSampler : register(s0);
SamplerState LinearSampler  : register(s1);

float4 main(float4 PositionCS : SV_POSITION) : SV_TARGET
{
    float2 uv = PositionCS.xy * ScreenSize.zw;
    float depth = SceneDepthTexture.Sample(PointerSampler, uv).r;

    if (depth >= 1.0f)
        discard;
    
    float2 ndc; // [0, 1] -> [-1, 1]
    ndc.x = uv.x * 2.0f - 1.0f;
    ndc.y = (1.0f - uv.y) * 2.0f - 1.0f;
    
    float4 clipPos = float4(ndc, depth, 1.0f);
    float4 worldPosH = mul(clipPos, InvViewProj);
    float3 worldPos = worldPosH.xyz / worldPosH.w;
    float3 localPos = mul(float4(worldPos, 1.0f), WorldToDecal).xyz;

    if (abs(localPos.x) > 0.5f ||
        abs(localPos.y) > 0.5f ||
        abs(localPos.z) > 0.5f)
    {
        discard;
    }

    float2 decalUV = localPos.xy + 0.5f;
    decalUV.y = 1.0f - decalUV.y;

    float4 col = DecalTexture.Sample(LinearSampler, decalUV);
    return col * DecalColor;
}