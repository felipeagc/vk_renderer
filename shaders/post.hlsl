[[vk::binding(1)]] Texture2D<float4> textures[];
[[vk::binding(2)]] SamplerState samplers[];

struct Indices
{
	uint offscreen_image_index;
	uint bloom_image_index;
	uint sampler_index;
};

[[vk::push_constant]] Indices pc;

float4 LINEARtoSRGB(float4 linear_in)
{
	return float4(pow(linear_in.xyz, float3(1.0/2.2, 1.0/2.2, 1.0/2.2)), linear_in.w);
}

void vertex(
	in uint vertexIndex : SV_VertexID,
	out float4 out_pos : SV_Position,
	out float2 out_uv : TEXCOORD0)
{
	out_uv = float2(float((vertexIndex << 1) & 2), float(vertexIndex & 2));
	float2 temp = out_uv * 2.0 - 1.0;
    out_pos = float4(temp.x, temp.y, 0.0, 1.0);
}

float4 pixel(in float2 uv : TEXCOORD0) : SV_Target
{
	Texture2D<float4> offscreen_image = textures[pc.offscreen_image_index];
	Texture2D<float4> bloom_image = textures[pc.bloom_image_index];
	SamplerState offscreen_sampler = samplers[pc.sampler_index];

    float3 col = offscreen_image.Sample(offscreen_sampler, uv).rgb;
    col += bloom_image.Sample(offscreen_sampler, uv).rgb;

	float exposure = 5.8f;
	float3 result = 1.0f - exp(-col * exposure);
	return LINEARtoSRGB(float4(result, 1.0));
}
