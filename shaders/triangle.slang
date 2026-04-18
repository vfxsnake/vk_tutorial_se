struct VertexInput {
    float2 position;
    float3 color;
};

struct VertexOutput {
    float4 position : SV_Position;
    float3 color;
};


[shader("vertex")]
VertexOutput vertMain(VertexInput vertex_in) {
    VertexOutput out_vertex;
    out_vertex.position = float4(vertex_in.position, 0.0, 1.0);
    out_vertex.color = vertex_in.color;
    return out_vertex;
}


[shader("fragment")]
float4 fragMain(VertexOutput vertex_in) : SV_Target
{
    float3 color = vertex_in.color;
    return float4(color, 1.0);
}