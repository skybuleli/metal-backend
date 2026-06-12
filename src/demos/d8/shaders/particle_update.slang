// D8 Complex Showcase — Compute 粒子更新着色器 (Path A: Slang→DXIL→MSC→metallib)
// 功能：每帧更新粒子位置、速度、生命周期；线程 0 写入间接绘制参数

struct Particle
{
    float2 position;
    float2 velocity;
    float4 color;
    float size;
    float age;
    float lifetime;
    float phase;
};

struct DrawArgs
{
    uint vertexCount;
    uint instanceCount;
    uint vertexStart;
    uint baseInstance;
};

cbuffer SimParams : register(b0)
{
    uint particleCount;
    float deltaTime;
    float time;
    float aspect;
    uint frameIndex;
    float speedScale;
    float drag;
    float verticalLimit;
};

RWStructuredBuffer<Particle> particles : register(u0);
RWStructuredBuffer<DrawArgs> indirectArgs : register(u1);

float Hash11(float value)
{
    return frac(sin(value * 12.9898) * 43758.5453);
}

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint idx = dispatch_id.x;
    if (idx >= particleCount)
    {
        return;
    }

    Particle particle = particles[idx];

    // 轨道运动 + 漂移
    float orbit_seed = particle.phase + time * 0.35;
    float2 radial = normalize(float2(-particle.position.y, particle.position.x) + float2(0.0001, 0.0001));
    float2 drift = float2(sin(orbit_seed * 1.7), cos(orbit_seed * 1.3)) * 0.16;
    particle.velocity = normalize(particle.velocity * drag + radial * 0.72 + drift) *
        (0.18 + 0.09 * (0.5 + 0.5 * sin(orbit_seed)));

    particle.position += particle.velocity * deltaTime * speedScale;
    particle.age += deltaTime;

    // 边界检查与重生
    float horizontal_limit = max(aspect, 1.0) * 1.30;
    if (abs(particle.position.x) > horizontal_limit ||
        abs(particle.position.y) > verticalLimit ||
        particle.age >= particle.lifetime)
    {
        float respawn_seed = float(idx) * 0.173 + time * 0.41 + float(frameIndex) * 0.013;
        float angle = Hash11(respawn_seed) * 6.2831853;
        float radius = lerp(0.18, 1.08, Hash11(respawn_seed + 3.1));
        float speed = lerp(0.12, 0.28, Hash11(respawn_seed + 5.3));
        particle.position = float2(cos(angle), sin(angle)) * radius * float2(aspect, 1.0);
        particle.velocity = normalize(float2(-sin(angle), cos(angle)) + drift) * speed;
        particle.age = 0.0;
        particle.lifetime = lerp(1.6, 4.4, Hash11(respawn_seed + 8.7));
        particle.size = lerp(0.010, 0.030, Hash11(respawn_seed + 11.2));
        particle.phase = angle;
    }

    particles[idx] = particle;

    // 线程 0 写入间接绘制参数 (动态活跃粒子数量)
    if (idx == 0)
    {
        float density = 0.52 + 0.48 * (0.5 + 0.5 * sin(time * 0.85));
        uint active_count = max(256u, (uint)(density * (float)particleCount));

        DrawArgs args;
        args.vertexCount = 6;
        args.instanceCount = active_count;
        args.vertexStart = 0;
        args.baseInstance = 0;
        indirectArgs[0] = args;
    }
}
