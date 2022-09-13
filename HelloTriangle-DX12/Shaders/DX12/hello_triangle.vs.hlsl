cbuffer cb : register(b0)
{
    float3 color : packoffset(c0);
};

struct VertexInput
{
    float3 in_pos : POSITION;
    float3 in_color : COLOR;
};

struct VertexOutput
{
    float3 color : COLOR;
    float4 position : SV_Position;
};

VertexOutput main(VertexInput vertexInput)
{
    VertexOutput output;
    output.color = vertexInput.in_color;
    output.position = float4(vertexInput.in_pos, 0.5f);
    return output;
}