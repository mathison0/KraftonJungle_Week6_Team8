#include "ShaderCommon.hlsli"

Texture2D DecalTexture : register(t0);
SamplerState DecalSampler : register(s0);

cbuffer DecalMaterialData : register(b2)
{
	float4 BaseColor;
	float4 DecalExtent;
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
	float3 WorldPosition : TEXCOORD0;
};

float4 main(DECAL_VS_OUTPUT Input) : SV_TARGET
{
	float3 DeltaToReceiver = Input.WorldPosition.xyz - DecalOrigin.xyz;
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
