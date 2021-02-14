#pragma cull_mode back

struct Camera
{
	float4 pos;
	float4x4 view;
	float4x4 proj;
};

struct Model
{
	float4x4 matrix;
};

struct Material
{
	float4 base_color;
	float4 emissive;
	float metallic;
	float roughness;
	uint is_normal_mapped;
};

struct VsInput
{
	float3 pos     : POSITION;
	float3 normal  : NORMAL;
	float4 tangent : TANGENT;
	float2 uv      : TEXCOORD0;
};

struct VsOutput
{
    float4 sv_pos : SV_Position;
	float2 uv : TEXCOORD0;
};

[[vk::binding(0, 0)]] ConstantBuffer<Camera> camera;

[[vk::binding(0, 1)]] ConstantBuffer<Model> model;
[[vk::binding(1, 1)]] ConstantBuffer<Material> material;
[[vk::binding(2, 1)]] SamplerState model_sampler;
[[vk::binding(3, 1)]] Texture2D<float4> albedo_image;
[[vk::binding(4, 1)]] Texture2D<float4> normal_image;
[[vk::binding(5, 1)]] Texture2D<float4> metallic_roughness_image;
[[vk::binding(6, 1)]] Texture2D<float4> occlusion_image;
[[vk::binding(7, 1)]] Texture2D<float4> emissive_image;


VsOutput vertex(in VsInput vs_in)
{
    VsOutput vs_out;
	vs_out.sv_pos = mul(mul(camera.proj, camera.view), float4(vs_in.pos, 1.0));
	vs_out.uv = vs_in.uv;
    return vs_out;
}

float4 pixel(VsOutput vs_out) : SV_Target
{
    float3 albedo = albedo_image.Sample(model_sampler, vs_out.uv).xyz;
	return float4(albedo, 1.0);
}
