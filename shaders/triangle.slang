static float2 positions[3] = float2[](
    float2(0.0, -0.5),
    float2(0.5, 0.5),
    float2(-0.5, 0.5)
);

static float3 colors[3] = float3[](
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0)
);


struct VertexOutput {
    float3 color;
    float4 position : SV_Position;
};


[shader("vertex")]
VertexOutput vertMain(uint vertex_id: SV_VertexID) {
    VertexOutput out_vertex;
    out_vertex.position = float4(positions[vertex_id], 0.0, 1.0);
    out_vertex.color = colors[vertex_id];
    return out_vertex;
}


[shader("fragment")]
float4 fragMain(VertexOutput vertex_in) : SV_Target
{
    float3 color = vertex_in.color;
    return float4(color, 1.0);
}