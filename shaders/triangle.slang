struct VertexInput {
    float2 position;
    float3 color;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 projection;
};
ConstantBuffer<UniformBuffer> uniform_buffer_object;

struct VertexOutput {
    float4 position : SV_Position;
    float3 color;
};


[shader("vertex")]
VertexOutput vertMain(VertexInput vertex_in) {
    VertexOutput out_vertex;

    float4 world_space_position = mul(uniform_buffer_object.model, float4(vertex_in.position, 0.0, 1.0));
    float4 view_position = mul(uniform_buffer_object.view, world_space_position);

    out_vertex.position = mul(uniform_buffer_object.projection, view_position);
    out_vertex.color = vertex_in.color;
    return out_vertex;
}


[shader("fragment")]
float4 fragMain(VertexOutput vertex_in) : SV_Target
{
    float3 color = vertex_in.color;
    return float4(color, 1.0);
}