#include "ShaderCommon.hlsli"

struct DECAL_VS_OUTPUT
{
	float4 Position : SV_POSITION;
	float4 ScreenPosition : TEXCOORD0;
	float3 LocalPosition : TEXCOORD1;
};

DECAL_VS_OUTPUT main(VS_INPUT Input)
{
	DECAL_VS_OUTPUT Output;

	float4 WorldPos = mul(float4(Input.Position, 1.0f), World);
	float4 ViewPos = mul(WorldPos, View);
	Output.Position = mul(ViewPos, Projection);
	Output.ScreenPosition = Output.Position;
	Output.LocalPosition = Input.Position;
	return Output;
}
