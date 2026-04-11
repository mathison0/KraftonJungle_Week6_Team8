#include "ShaderCommon.hlsli"

Texture2D DecalTexture : register(t0);
Texture2D SceneDepthTexture : register(t1);
SamplerState DecalSampler : register(s0);

cbuffer DecalMaterialData : register(b2)
{
	float4 BaseColor;
	float4 DecalExtent;
	float4 ScreenSize;
};

cbuffer DecalTransformData : register(b3)
{
	float4 DecalOrigin;
	float4 DecalAxisX;
	float4 DecalAxisY;
	float4 DecalAxisZ;
};

struct DECAL_VS_OUTPUT
{
	float4 Position : SV_POSITION;
};

float4 main(DECAL_VS_OUTPUT Input) : SV_TARGET
{
	int2 PixelCoord = int2(Input.Position.xy);
	float SceneDepth = SceneDepthTexture.Load(int3(PixelCoord, 0)).r;
	clip(0.999999f - SceneDepth);

	float2 ScreenUV = Input.Position.xy * ScreenSize.zw;
	float2 ClipXY = float2(ScreenUV.x * 2.0f - 1.0f, 1.0f - ScreenUV.y * 2.0f);
	float4 ClipPosition = float4(ClipXY, SceneDepth, 1.0f);

	float4 ViewPosition = mul(ClipPosition, InvProjection);
	ViewPosition.xyz /= ViewPosition.w;

	float4 WorldPosition = mul(float4(ViewPosition.xyz, 1.0f), InvView);
	float3 DeltaToReceiver = WorldPosition.xyz - DecalOrigin.xyz;
	float AxisXLengthSq = dot(DecalAxisX.xyz, DecalAxisX.xyz);
	float AxisYLengthSq = dot(DecalAxisY.xyz, DecalAxisY.xyz);
	float AxisZLengthSq = dot(DecalAxisZ.xyz, DecalAxisZ.xyz);

	float3 DecalLocalPosition;
	DecalLocalPosition.x = AxisXLengthSq > 1.0e-6f ? dot(DeltaToReceiver, DecalAxisX.xyz) / AxisXLengthSq : 0.0f;
	DecalLocalPosition.y = AxisYLengthSq > 1.0e-6f ? dot(DeltaToReceiver, DecalAxisY.xyz) / AxisYLengthSq : 0.0f;
	DecalLocalPosition.z = AxisZLengthSq > 1.0e-6f ? dot(DeltaToReceiver, DecalAxisZ.xyz) / AxisZLengthSq : 0.0f;
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
