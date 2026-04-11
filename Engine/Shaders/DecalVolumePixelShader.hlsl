#include "ShaderCommon.hlsli"

cbuffer MaterialData : register(b2)
{
    float4 BaseColor;
};

float4 main(VS_OUTPUT Input) : SV_TARGET
{
    return BaseColor;
}
