#pragma cull_mode front

#define PI 3.14159265359

float4 SRGBtoLINEAR(float4 srgb_in)
{
	return float4(pow(srgb_in.xyz, float3(2.2, 2.2, 2.2)), srgb_in.w);
}

struct Camera
{
	float4 pos;
	float4x4 view;
	float4x4 proj;
};

struct Model
{
	float4x4 transform;
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

	uint brdf_image_index;
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
	float4 sv_pos    : SV_Position;
	float3 world_pos : POSITION;
	float3 normal    : NORMAL;
	float2 uv        : TEXCOORD0;
	float3x3 tbn     : TBN_MATRIX;
};

[[vk::binding(0)]] StructuredBuffer<Camera> camera_buffers[];
[[vk::binding(0)]] StructuredBuffer<Model> model_buffers[];
[[vk::binding(0)]] StructuredBuffer<Material> material_buffers[];
[[vk::binding(1)]] Texture2D<float4> textures[];
[[vk::binding(2)]] SamplerState samplers[];

[[vk::push_constant]] PushConstant pc;

float distribution_ggx(float3 N, float3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0000001); // prevent divide by zero for roughness=0.0 and NdotH=1.0
}

float geometry_schlick_ggx(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float geometry_smith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometry_schlick_ggx(NdotV, roughness);
    float ggx1 = geometry_schlick_ggx(NdotL, roughness);

    return ggx1 * ggx2;
}

float3 fresnel_schlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

VsOutput vertex(in VsInput vs_in)
{
	Model model = model_buffers[pc.model_buffer_index][pc.model_index];
	Camera camera = camera_buffers[pc.camera_buffer_index][pc.camera_index];

	VsOutput vs_out;
	vs_out.world_pos = mul(model.transform, float4(vs_in.pos, 1.0)).xyz;
	vs_out.sv_pos = mul(camera.proj, mul(camera.view, float4(vs_out.world_pos, 1.0)));
	vs_out.uv = vs_in.uv;

	Material mat = material_buffers[pc.material_buffer_index][pc.material_index];

	if (mat.is_normal_mapped != 0)
	{
		float3 T = normalize(mul(model.transform, float4(vs_in.tangent.xyz, 0.0f)).xyz);
		float3 N = normalize(mul(model.transform, float4(vs_in.normal, 0.0f)).xyz);
		T = normalize(T - dot(T, N) * N); // re-orthogonalize
		float3 B = vs_in.tangent.w * cross(N, T);
		vs_out.tbn[0] = T;
		vs_out.tbn[1] = B;
		vs_out.tbn[2] = N;
		vs_out.tbn = transpose(vs_out.tbn);
	}
	else
	{
		float3x3 model3;
		model3[0] = model.transform[0].xyz;
		model3[1] = model.transform[1].xyz;
		model3[2] = model.transform[2].xyz;
		vs_out.normal = normalize(mul(model3, vs_in.normal));
	}

	return vs_out;
}

void pixel(
		in VsOutput vs_out,
		out float4 main_color : SV_Target0,
		out float4 bright_color : SV_Target1)
{
	Camera camera = camera_buffers[pc.camera_buffer_index][pc.camera_index];
	Material mat = material_buffers[pc.material_buffer_index][pc.material_index];
	Texture2D<float4> brdf_image = textures[mat.brdf_image_index];
	Texture2D<float4> albedo_image = textures[mat.albedo_image_index];
	Texture2D<float4> normal_image = textures[mat.normal_image_index];
	Texture2D<float4> metallic_roughness_image = textures[mat.metallic_roughness_image_index];
	Texture2D<float4> emissive_image = textures[mat.emissive_image_index];
	SamplerState model_sampler = samplers[mat.sampler_index];

	float3 albedo = SRGBtoLINEAR(albedo_image.Sample(model_sampler, vs_out.uv)).rgb;
	float2 metallic_roughness = SRGBtoLINEAR(metallic_roughness_image.Sample(model_sampler, vs_out.uv)).rg;

	float metallic = metallic_roughness.r * mat.metallic;
	float roughness = metallic_roughness.g * mat.roughness;

	float3 N;
	if (mat.is_normal_mapped != 0)
	{
		N = normal_image.Sample(model_sampler, vs_out.uv).rgb;
		N = normalize(N * 2.0 - 1.0); // Remap from [0, 1] to [-1, 1]
		N = normalize(mul(vs_out.tbn, N));
	}
	else
	{
		N = vs_out.normal;
	}

	float3 V = normalize(camera.pos.xyz - vs_out.world_pos);

	float3 F0 = 0.04f;
	F0 = lerp(F0, albedo, float3(metallic, metallic, metallic));

	float3 Lo = 0.0f;
	float3 light_positions[] = { float3(0.0, 0.0, 0.0) };

	for (int i = 0; i < 1; i++)
	{
		float3 L = normalize(light_positions[i] - vs_out.world_pos);
		float3 H = normalize(V + L);

		float dist = length(light_positions[i] - vs_out.world_pos);
		float attenuation = 1.0f / (dist * dist);
		float3 radiance = float3(1.0f, 2.0f, 2.0f) * attenuation;

		float NDF = distribution_ggx(N, H, roughness);
		float G = geometry_smith(N, V, L, roughness);
		float3 F = fresnel_schlick(clamp(dot(H, V), 0.0, 1.0), F0);

		float3 nominator = NDF * G * F;
		float denominator = 4 * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0);
		float3 specular = nominator / max(denominator, 0.001);

		float3 kS = F;

		float3 kD = 1.0f - kS;

		kD *= 1.0f - metallic;

		float NdotL = max(dot(N, L), 0.0);

		Lo += (kD * albedo / PI + specular) * radiance * NdotL;
	}

	float3 ambient = 0.03 * albedo;
	float3 color = ambient + Lo;

	float3 emissive = SRGBtoLINEAR(emissive_image.Sample(model_sampler, vs_out.uv)).rgb;

	main_color = float4(color + emissive, 1.0);

	bright_color = float4(emissive, 1.0);
	float3 brightness_factor = float3(0.2126, 0.7152, 0.0722);
	float brightness = dot(main_color.rgb, brightness_factor);
	if(brightness > 1.0)
	{
		bright_color += float4(main_color.rgb, 1.0);
	}
	else
	{
		bright_color += float4(0.0, 0.0, 0.0, 1.0);
	}
}
