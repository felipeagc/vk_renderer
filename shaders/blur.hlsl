struct PushConstant
{
	uint image_index;
	uint sampler_index;

	uint horizontal;
};

[[vk::binding(1)]] Texture2D<float4> textures[];
[[vk::binding(2)]] SamplerState samplers[];

[[vk::push_constant]] PushConstant pc;

#define BLUR_KERNEL_SIZE 5

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
	Texture2D<float4> image = textures[pc.image_index];
	SamplerState image_sampler = samplers[pc.sampler_index];

	float weight[BLUR_KERNEL_SIZE] = {0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216};

	float width;
	float height;
	float levels;
	image.GetDimensions(0, width, height, levels);

	float2 tex_offset = float2(1.0f, 1.0f) / float2(width, height); // gets size of single texel
	float3 result = image.Sample(image_sampler, uv).rgb * weight[0]; // current fragment's contribution
	if (pc.horizontal != 0)
	{
		for(int i = 1; i < BLUR_KERNEL_SIZE; ++i)
		{
			result += image.Sample(image_sampler, uv + float2(tex_offset.x * float(i), 0.0)).rgb * weight[i];
			result += image.Sample(image_sampler, uv - float2(tex_offset.x * float(i), 0.0)).rgb * weight[i];
		}
	}
	else
	{
		for(int i = 1; i < BLUR_KERNEL_SIZE; ++i)
		{
			result += image.Sample(image_sampler, uv + float2(0.0, tex_offset.y * float(i))).rgb * weight[i];
			result += image.Sample(image_sampler, uv - float2(0.0, tex_offset.y * float(i))).rgb * weight[i];
		}
	}

	return float4(result, 1.0f);
}
