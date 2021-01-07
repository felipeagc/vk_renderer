struct Camera
{
	float4 pos;
	float4x4 view;
	float4x4 proj;
};

struct VsInput
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float2 uv : TEXCOORD0;
};

struct VsOutput
{
    float4 sv_pos : SV_Position;
	float2 uv : TEXCOORD0;
};

[[vk::binding(0, 0)]] ConstantBuffer<Camera> camera;

VsOutput vertex(in VsInput vs_in)
{
    VsOutput vs_out;
	vs_out.sv_pos = mul(mul(camera.proj, camera.view), float4(vs_in.pos, 1.0));
	vs_out.uv = vs_in.uv;
    return vs_out;
}

float4 pixel(VsOutput vs_out) : SV_Target
{
	return float4(vs_out.sv_pos.xyz, 1.0);
}
