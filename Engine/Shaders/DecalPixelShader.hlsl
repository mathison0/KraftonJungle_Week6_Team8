#include "ShaderCommon.hlsli"

Texture2D DecalTexture : register(t0);
Texture2D DepthTexture : register(t1);
SamplerState DecalSampler : register(s0);

cbuffer DecalMaterialData : register(b2)
{
	float4 BaseColor;
	float4 DecalExtent;
};

cbuffer DecalTransformData : register(b3)
{
	float4x4 WorldToDecal;
};

struct DECAL_VS_OUTPUT
{
	float4 Position : SV_POSITION;
};

float4 main(DECAL_VS_OUTPUT Input) : SV_TARGET
{
	float DepthWidth = 1.0f;
	float DepthHeight = 1.0f;
	DepthTexture.GetDimensions(DepthWidth, DepthHeight);

	float2 ScreenUV = Input.Position.xy / float2(DepthWidth, DepthHeight);
	float SceneDepth = DepthTexture.Sample(DecalSampler, ScreenUV).r;
	if (SceneDepth >= 0.9999f)
	{
		discard;
	}

	float2 NDCXY = float2(ScreenUV.x * 2.0f - 1.0f, 1.0f - ScreenUV.y * 2.0f);
	float4 ClipPosition = float4(NDCXY, SceneDepth, 1.0f);
	float4 ViewPosition = mul(ClipPosition, InvProjection);
	ViewPosition /= ViewPosition.w;
	float4 WorldPosition = mul(ViewPosition, InvView);

	float3 DecalLocalPosition = mul(WorldPosition, WorldToDecal).xyz;

	if (DecalLocalPosition.x < 0.0f ||
		DecalLocalPosition.x > DecalExtent.x ||
		abs(DecalLocalPosition.y) > DecalExtent.y ||
		abs(DecalLocalPosition.z) > DecalExtent.z)
	{
		discard;
	}

	float2 ProjectedUV;
	ProjectedUV.x = DecalLocalPosition.y / (DecalExtent.y * 2.0f) + 0.5f;
	ProjectedUV.y = 0.5f - DecalLocalPosition.z / (DecalExtent.z * 2.0f);

	float4 Sampled = DecalTexture.Sample(DecalSampler, ProjectedUV);
	float4 FinalColor = Sampled * BaseColor;
	clip(FinalColor.a - 0.01f);
	return FinalColor;
}
