cbuffer ConstantBuffer : register(b0)
{
	matrix World;
	matrix View;
	matrix Projection;
}

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float4 Color : COLOR0;
};

VS_OUTPUT vertex(float4 Pos : POSITION, float4 Color : COLOR)
{
	VS_OUTPUT output = (VS_OUTPUT)0;
	output.Pos = mul(World, Pos);
	output.Pos = mul(View, output.Pos);
	output.Pos = mul(Projection, output.Pos);
	output.Color = Color;
	return output;
}

float4 pixel(VS_OUTPUT input) : SV_Target
{
	return input.Color;
}
