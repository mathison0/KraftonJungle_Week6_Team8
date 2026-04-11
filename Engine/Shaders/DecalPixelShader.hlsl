#include "ShaderCommon.hlsli"

Texture2D DecalTexture : register(t0);
Texture2D DepthTexture : register(t1);
SamplerState DecalSampler : register(s0);

cbuffer DecalMaterialData : register(b2)
{
	float4 BaseColor;
	float4 DecalExtent;
	float4 ProjectionParams;
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
	int3 DepthSampleCoord = int3(int2(Input.Position.xy), 0);
	float SceneDepth = DepthTexture.Load(DepthSampleCoord).r;
	if (SceneDepth >= 0.9999f)
	{
		discard;
	}

	float DepthWidth = 1.0f;
	float DepthHeight = 1.0f;
	DepthTexture.GetDimensions(DepthWidth, DepthHeight);

	float2 ScreenUV = Input.Position.xy / float2(DepthWidth, DepthHeight);
	float2 NDCXY = float2(ScreenUV.x * 2.0f - 1.0f, 1.0f - ScreenUV.y * 2.0f);

	float ProjectionDepthScale = ProjectionParams.x;
	float ProjectionDepthBias = ProjectionParams.y;
	float ProjectionRightScale = ProjectionParams.z;
	float ProjectionUpScale = ProjectionParams.w;

	float ViewForward = ProjectionDepthBias / max(SceneDepth - ProjectionDepthScale, -1.0e-6f);
	float ViewRight = NDCXY.x * ViewForward / ProjectionRightScale;
	float ViewUp = NDCXY.y * ViewForward / ProjectionUpScale;
	float4 ViewPosition = float4(ViewForward, ViewRight, ViewUp, 1.0f);
	float4 WorldPosition = mul(ViewPosition, InvView);

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

	return float4(ProjectedUV.x, ProjectedUV.y, 0.0f, 1.0f);
}
