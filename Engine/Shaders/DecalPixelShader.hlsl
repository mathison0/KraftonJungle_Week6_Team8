#include "ShaderCommon.hlsli"

Texture2D DecalTexture : register(t0);
SamplerState DecalSampler : register(s0);
Texture2D DepthTexture : register(t1);

cbuffer DecalMaterialData : register(b2)
{
	float4 BaseColor;
	float4x4 DecalViewProjection;
};

struct DECAL_VS_OUTPUT
{
	float4 Position : SV_POSITION;
	float4 ScreenPosition : TEXCOORD0;
	float3 LocalPosition : TEXCOORD1;
};

float3 ReconstructWorldPosition(float4 ScreenPosition)
{
	int Width = 0;
	int Height = 0;
	DepthTexture.GetDimensions(Width, Height);

	float2 ScreenUV = ScreenPosition.xy / float2((float) Width, (float) Height);
	float Depth = DepthTexture.Sample(DecalSampler, ScreenUV).r;
	float2 NDCXY = float2(ScreenUV.x * 2.0f - 1.0f, 1.0f - ScreenUV.y * 2.0f);
	float4 ClipPosition = float4(NDCXY, Depth * 2.0f - 1.0f, 1.0f);

	float4 ViewPosition = mul(ClipPosition, InvProjection);
	ViewPosition /= ViewPosition.w;

	float4 WorldPosition = mul(ViewPosition, InvView);
	return WorldPosition.xyz;
}

float4 main(DECAL_VS_OUTPUT Input) : SV_TARGET
{
	float3 ReceiverWorldPosition = ReconstructWorldPosition(Input.ScreenPosition);
	float4 DecalClipPosition = mul(float4(ReceiverWorldPosition, 1.0f), DecalViewProjection);
	float3 DecalNDC = DecalClipPosition.xyz / DecalClipPosition.w;

	if (abs(DecalNDC.x) > 1.0f ||
		abs(DecalNDC.y) > 1.0f ||
		DecalNDC.z < 0.0f ||
		DecalNDC.z > 1.0f)
	{
		discard;
	}

	float2 ProjectedUV;
	ProjectedUV.x = DecalNDC.x * 0.5f + 0.5f;
	ProjectedUV.y = -DecalNDC.y * 0.5f + 0.5f;

	float4 Sampled = DecalTexture.Sample(DecalSampler, ProjectedUV);
	float4 FinalColor = Sampled * BaseColor;
	clip(FinalColor.a - 0.01f);
	return FinalColor;
}
