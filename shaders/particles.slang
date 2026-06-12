struct VSInput {
    [[vk::location(0)]] float2 inPosition;
    [[vk::location(1)]] float4 inColor;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float pointSize : SV_PointSize;
    float3 fragColor : COLOR0;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float3 fragColor : COLOR0;
    float2 pointCoord : SV_PointCoord;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pointSize = 14.0;
    output.pos = float4(input.inPosition, 0.0, 1.0);
    output.fragColor = input.inColor.rgb;
    return output;
}

[shader("fragment")]
float4 fragMain(PSInput input) : SV_TARGET {
    float2 coord = input.pointCoord - float2(0.5);
    return float4(input.fragColor, 0.5 - length(coord));
}

struct Particle {
	float2 position;
	float2 velocity;
    float4 color;
};

struct UniformBuffer {
    float deltaTime;
};
[vk::binding(0, 0)] ConstantBuffer<UniformBuffer> ubo;

struct PushConstants
{
    uint startIndex;
    uint count;
};
[[vk::push_constant]] PushConstants pushConstants;

struct ParticleSSBO {
    Particle particles;
};
[vk::binding(1, 0)] StructuredBuffer<ParticleSSBO> particlesIn;
[vk::binding(2, 0)] RWStructuredBuffer<ParticleSSBO> particlesOut;

[shader("compute")]
[numthreads(256,1,1)]
void compMain(uint3 threadId : SV_DispatchThreadID)
{
    if (threadId.x >= pushConstants.count) return;
    uint globalIndex = pushConstants.startIndex + threadId.x;

    particlesOut[globalIndex].particles.position = particlesIn[globalIndex].particles.position
                                            + particlesIn[globalIndex].particles.velocity * ubo.deltaTime;
    
    particlesOut[globalIndex].particles.velocity = particlesIn[globalIndex].particles.velocity;

    // Bounce at screen edges
    if (abs(particlesOut[globalIndex].particles.position.x) >= 1.0)
        particlesOut[globalIndex].particles.velocity.x *= -1.0;

    if (abs(particlesOut[globalIndex].particles.position.y) >= 1.0)
        particlesOut[globalIndex].particles.velocity.y *= -1.0;

    particlesOut[globalIndex].particles.color = particlesIn[globalIndex].particles.color;
}