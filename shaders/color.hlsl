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

	uint sampler_index;
	uint albedo_image_index;
	uint normal_image_index;
	uint metallic_roughness_image_index;
	uint occlusion_image_index;
	uint emissive_image_index;
};

struct PushConstant
{
	uint camera_buffer_index;
	uint camera_index;

	uint model_buffer_index;
	uint model_index;

	uint material_buffer_index;
	uint material_index;
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

[[vk::binding(0)]] StructuredBuffer<Camera> camera_buffers[];
[[vk::binding(0)]] StructuredBuffer<Model> model_buffers[];
[[vk::binding(0)]] StructuredBuffer<Material> material_buffers[];
[[vk::binding(1)]] Texture2D<float4> textures[];
[[vk::binding(2)]] SamplerState samplers[];

[[vk::push_constant]] PushConstant pc;

VsOutput vertex(in VsInput vs_in)
{
	Camera camera = camera_buffers[pc.camera_buffer_index][pc.camera_index];

    VsOutput vs_out;
	vs_out.sv_pos = mul(mul(camera.proj, camera.view), float4(vs_in.pos, 1.0));
	vs_out.uv = vs_in.uv;
    return vs_out;
}

float4 pixel(VsOutput vs_out) : SV_Target
{
	Material mat = material_buffers[pc.material_buffer_index][pc.material_index];
	Texture2D<float4> albedo_image = textures[mat.albedo_image_index];
	SamplerState model_sampler = samplers[mat.sampler_index];

    float3 albedo = albedo_image.Sample(model_sampler, vs_out.uv).xyz;
	return float4(albedo, 1.0);
}
