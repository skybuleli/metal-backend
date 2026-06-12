#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace d4
{

// ============================================================================
// 通用渲染尺寸与清屏色
// ============================================================================
constexpr std::uint32_t kOffscreenWidth = 512;
constexpr std::uint32_t kOffscreenHeight = 512;
constexpr double kClearColorR = 0.06;
constexpr double kClearColorG = 0.08;
constexpr double kClearColorB = 0.12;

// ============================================================================
// 像素结构（BGRA 读取）
// ============================================================================
struct Pixel
{
    std::uint8_t b;
    std::uint8_t g;
    std::uint8_t r;
    std::uint8_t a;
};

// ============================================================================
// 4×4 矩阵数学（列主序，与 Metal/MSL 对齐）
// ============================================================================
struct alignas(16) Mat4
{
    float m[16] = {};
};

inline Mat4 Mat4Mul(const Mat4& a, const Mat4& b)
{
    Mat4 result;
    for (int col = 0; col < 4; ++col)
    {
        for (int row = 0; row < 4; ++row)
        {
            float sum = 0.0f;
            for (int index = 0; index < 4; ++index)
            {
                sum += a.m[index * 4 + row] * b.m[col * 4 + index];
            }
            result.m[col * 4 + row] = sum;
        }
    }
    return result;
}

inline Mat4 Mat4Perspective(float fov_y, float aspect, float near_z, float far_z)
{
    const float tan_half = std::tan(fov_y * 0.5f);
    Mat4 result;
    result.m[0] = 1.0f / (aspect * tan_half);
    result.m[5] = 1.0f / tan_half;
    result.m[10] = far_z / (near_z - far_z);
    result.m[11] = -1.0f;
    result.m[14] = near_z * far_z / (near_z - far_z);
    return result;
}

inline Mat4 Mat4LookAt(float eye_x, float eye_y, float eye_z,
                       float target_x, float target_y, float target_z,
                       float up_x, float up_y, float up_z)
{
    float fx = target_x - eye_x;
    float fy = target_y - eye_y;
    float fz = target_z - eye_z;
    const float forward_length = std::sqrt(fx * fx + fy * fy + fz * fz);
    fx /= forward_length;
    fy /= forward_length;
    fz /= forward_length;

    float sx = fy * up_z - fz * up_y;
    float sy = fz * up_x - fx * up_z;
    float sz = fx * up_y - fy * up_x;
    const float side_length = std::sqrt(sx * sx + sy * sy + sz * sz);
    sx /= side_length;
    sy /= side_length;
    sz /= side_length;

    const float ux = sy * fz - sz * fy;
    const float uy = sz * fx - sx * fz;
    const float uz = sx * fy - sy * fx;

    Mat4 result;
    result.m[0] = sx;
    result.m[4] = sy;
    result.m[8] = sz;
    result.m[12] = -(sx * eye_x + sy * eye_y + sz * eye_z);
    result.m[1] = ux;
    result.m[5] = uy;
    result.m[9] = uz;
    result.m[13] = -(ux * eye_x + uy * eye_y + uz * eye_z);
    result.m[2] = -fx;
    result.m[6] = -fy;
    result.m[10] = -fz;
    result.m[14] = (fx * eye_x + fy * eye_y + fz * eye_z);
    result.m[3] = 0.0f;
    result.m[7] = 0.0f;
    result.m[11] = 0.0f;
    result.m[15] = 1.0f;
    return result;
}

inline Mat4 Mat4RotateY(float angle)
{
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    Mat4 result;
    result.m[0] = c;
    result.m[4] = 0.0f;
    result.m[8] = s;
    result.m[12] = 0.0f;
    result.m[1] = 0.0f;
    result.m[5] = 1.0f;
    result.m[9] = 0.0f;
    result.m[13] = 0.0f;
    result.m[2] = -s;
    result.m[6] = 0.0f;
    result.m[10] = c;
    result.m[14] = 0.0f;
    result.m[3] = 0.0f;
    result.m[7] = 0.0f;
    result.m[11] = 0.0f;
    result.m[15] = 1.0f;
    return result;
}

inline Mat4 Mat4RotateX(float angle)
{
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    Mat4 result;
    result.m[0] = 1.0f;
    result.m[4] = 0.0f;
    result.m[8] = 0.0f;
    result.m[12] = 0.0f;
    result.m[1] = 0.0f;
    result.m[5] = c;
    result.m[9] = -s;
    result.m[13] = 0.0f;
    result.m[2] = 0.0f;
    result.m[6] = s;
    result.m[10] = c;
    result.m[14] = 0.0f;
    result.m[3] = 0.0f;
    result.m[7] = 0.0f;
    result.m[11] = 0.0f;
    result.m[15] = 1.0f;
    return result;
}

// ============================================================================
// Uniform 数据与相机/光源配置
// ============================================================================
struct alignas(16) UniformData
{
    float mvpMatrix[16];
    float modelMatrix[16];
    float lightPos[3];
    float _pad0;
    float cameraPos[3];
    float _pad1;
};

static_assert(sizeof(UniformData) == 160, "UniformData 大小必须为 160 字节");

struct SceneMatrices
{
    Mat4 model;
    Mat4 view;
    Mat4 proj;
    Mat4 mvp;
};

constexpr std::array<float, 3> kLightPosition = {2.5f, 3.0f, 2.0f};
constexpr std::array<float, 3> kCameraPosition = {2.2f, 1.6f, 2.2f};

inline SceneMatrices BuildSceneMatrices(float aspect, float angle_x, float angle_y)
{
    SceneMatrices matrices;
    matrices.model = Mat4Mul(Mat4RotateX(angle_x), Mat4RotateY(angle_y));
    matrices.view = Mat4LookAt(kCameraPosition[0], kCameraPosition[1], kCameraPosition[2],
                               0.0f, 0.0f, 0.0f,
                               0.0f, 1.0f, 0.0f);
    matrices.proj = Mat4Perspective(3.14159f * 0.45f, aspect, 0.1f, 100.0f);
    matrices.mvp = Mat4Mul(Mat4Mul(matrices.proj, matrices.view), matrices.model);
    return matrices;
}

inline UniformData BuildUniformData(float aspect, float angle_x, float angle_y)
{
    const SceneMatrices matrices = BuildSceneMatrices(aspect, angle_x, angle_y);

    UniformData uniform_data = {};
    std::memcpy(uniform_data.mvpMatrix, matrices.mvp.m, sizeof(matrices.mvp.m));
    std::memcpy(uniform_data.modelMatrix, matrices.model.m, sizeof(matrices.model.m));
    uniform_data.lightPos[0] = kLightPosition[0];
    uniform_data.lightPos[1] = kLightPosition[1];
    uniform_data.lightPos[2] = kLightPosition[2];
    uniform_data.cameraPos[0] = kCameraPosition[0];
    uniform_data.cameraPos[1] = kCameraPosition[1];
    uniform_data.cameraPos[2] = kCameraPosition[2];
    return uniform_data;
}

// ============================================================================
// 立方体顶点数据
// ============================================================================
struct Vertex
{
    float position[3];
    float normal[3];
};

constexpr Vertex kCubeVertices[] = {
    {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},

    {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},

    {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},

    {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}},
    {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},
    {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},
    {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}},
    {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},
    {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}},

    {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},

    {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
    {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}},
    {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
    {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}},
    {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}},
};

constexpr std::uint32_t kNumCubeVertices = 36;

inline std::vector<float> BuildPositionStream()
{
    std::vector<float> positions(kNumCubeVertices * 3);
    for (std::uint32_t index = 0; index < kNumCubeVertices; ++index)
    {
        positions[index * 3 + 0] = kCubeVertices[index].position[0];
        positions[index * 3 + 1] = kCubeVertices[index].position[1];
        positions[index * 3 + 2] = kCubeVertices[index].position[2];
    }
    return positions;
}

inline std::vector<float> BuildNormalStream()
{
    std::vector<float> normals(kNumCubeVertices * 3);
    for (std::uint32_t index = 0; index < kNumCubeVertices; ++index)
    {
        normals[index * 3 + 0] = kCubeVertices[index].normal[0];
        normals[index * 3 + 1] = kCubeVertices[index].normal[1];
        normals[index * 3 + 2] = kCubeVertices[index].normal[2];
    }
    return normals;
}

// ============================================================================
// 手写 MSL 着色器源代码
// ============================================================================
inline constexpr const char* kShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct UniformData {
    float4x4 mvpMatrix;
    float4x4 modelMatrix;
    packed_float3 lightPos;
    packed_float3 cameraPos;
};

struct VertexOut {
    float4 position [[position]];
    float3 worldPos;
    float3 worldNormal;
};

vertex VertexOut vertexMain(uint vertexId [[vertex_id]],
                            constant packed_float3* positions [[buffer(0)]],
                            constant packed_float3* normals [[buffer(1)]],
                            constant UniformData& uniforms [[buffer(2)]])
{
    packed_float3 pos = positions[vertexId];
    packed_float3 norm = normals[vertexId];
    float4 worldPos4 = uniforms.modelMatrix * float4(pos, 1.0);
    float3x3 normalMatrix = float3x3(
        uniforms.modelMatrix[0].xyz,
        uniforms.modelMatrix[1].xyz,
        uniforms.modelMatrix[2].xyz);

    VertexOut out;
    out.position = uniforms.mvpMatrix * float4(pos, 1.0);
    out.worldPos = worldPos4.xyz;
    out.worldNormal = normalize(normalMatrix * float3(norm));
    return out;
}

fragment float4 fragmentMain(VertexOut in [[stage_in]],
                             constant UniformData& uniforms [[buffer(0)]])
{
    float3 N = normalize(in.worldNormal);
    float3 L = normalize(uniforms.lightPos - in.worldPos);
    float3 V = normalize(uniforms.cameraPos - in.worldPos);
    float3 R = reflect(-L, N);

    float3 ambient = float3(0.18, 0.18, 0.20);
    float3 diffuse = float3(0.8, 0.6, 0.4) * max(dot(N, L), 0.0);
    float3 specular = float3(1.0, 1.0, 1.0) * pow(max(dot(R, V), 0.0), 48.0);

    return float4(ambient + diffuse + specular, 1.0);
}
)";

} // namespace d4
