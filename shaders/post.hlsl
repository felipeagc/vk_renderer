[[vk::binding(0, 0)]] Texture2D<float4> gImage;
[[vk::binding(1, 0)]] SamplerState gSampler;

void vertex(
	in uint vertexIndex : SV_VertexID,
	out float4 out_pos : SV_Position,
	out float2 out_uv : TEXCOORD0)
{
	out_uv = float2(float((vertexIndex << 1) & 2), float(vertexIndex & 2));
	float2 temp = out_uv * 2.0 - 1.0;
    out_pos = float4(temp.x, temp.y, 0.0, 1.0);
}

void pixel(
	in float2 uv : TEXCOORD0,
	out float4 fragColor : SV_Target)
{
    float3 col = gImage.Sample(gSampler, uv).xyz;
	fragColor = float4(col, 1.0);
}
