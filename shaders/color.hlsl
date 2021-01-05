struct VsOutput
{
    float4 sv_pos : SV_Position;
	float2 uv : TEXCOORD0;
};

VsOutput vertex(in uint vertexIndex : SV_VertexID)
{
    VsOutput vs_out;
	vs_out.uv = float2(float((vertexIndex << 1) & 2), float(vertexIndex & 2));
    vs_out.sv_pos = float4(vs_out.uv * 2.0 - 1.0, 0.0, 1.0);
    return vs_out;
}

float4 pixel(in VsOutput vs_out) : SV_Target
{
	return float4(vs_out.uv, 0.0, 1.0);
}
