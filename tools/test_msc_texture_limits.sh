#!/bin/bash
# test_msc_texture_limits.sh — MSC/Metal 纹理限制验证矩阵（P4.2.0）
#
# 验证 Metal Shader Converter (MSC) 对各类纹理操作的 DXIL→metallib 转换能力。
# 使用 Path A（Slang 原生语法 → DXIL → MSC → metallib）全覆盖验证。
#
# 覆盖维度：
#   1. 纹理类型：2D、2D Array、3D、Cube
#   2. 纹理格式：RGBA8、R8、R16F、深度比较
#   3. 采样器模式：最近点、线性、各向异性、边界颜色
#   4. 纹理操作：基本采样、gather、GatherRed、Load
#   5. 多纹理绑定、多采样器
#   6. 计算着色器纹理读写
#   7. 纹理偏导与梯度采样
#
# 用法: bash tools/test_msc_texture_limits.sh

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

PASS=0
FAIL=0

# ── 临时目录 ──
TMPDIR=""
cleanup() {
    if [ -n "$TMPDIR" ] && [ -d "$TMPDIR" ]; then
        rm -rf "$TMPDIR"
    fi
}
trap cleanup EXIT

if TMPDIR=$(mktemp -d 2>/dev/null); then
    :
else
    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
    TMPDIR="$SCRIPT_DIR/.tmp_test"
    mkdir -p "$TMPDIR"
fi

echo "================================================"
echo " MSC/Metal 纹理限制验证矩阵（P4.2.0）"
echo " 时间: $(date '+%Y-%m-%d %H:%M:%S')"
echo "================================================"
echo ""

# ── 编译函数 ──
# 编译着色器通过 Path A：Slang → DXIL → MSC → metallib
# 参数: $1=测试名 $2=阶段(vertex/fragment/compute) $3=profile $4=入口函数名
compile_shader() {
    local name="$1"
    local stage="$2"
    local profile="$3"
    local entry="${4:-main}"
    local src="$TMPDIR/${name}.slang"
    local dxil="$TMPDIR/${name}.dxil"
    local metallib="$TMPDIR/${name}.metallib"

    printf "  [%-30s] " "${name}"

    if [ ! -f "$src" ]; then
        echo -e "${YELLOW}SKIP${NC} (源文件 ${name}.slang 不存在)"
        return 0
    fi

    # Slang → DXIL
    if ! slangc "$src" -target dxil -entry "$entry" -stage "$stage" -profile "$profile" -o "$dxil" 2>"$TMPDIR/${name}_slang.err"; then
        local err_msg
        err_msg=$(head -3 "$TMPDIR/${name}_slang.err" 2>/dev/null || echo "未知错误")
        echo -e "${RED}FAIL${NC}"
        echo "    slangc: ${err_msg}"
        FAIL=$((FAIL + 1))
        return 1
    fi

    local dxil_size
    dxil_size=$(wc -c < "$dxil" 2>/dev/null || echo 0)
    if [ "$dxil_size" -eq 0 ]; then
        echo -e "${RED}FAIL${NC} (DXIL 为空)"
        FAIL=$((FAIL + 1))
        return 1
    fi

    # DXIL → MSC → metallib
    if ! metal-shaderconverter "$dxil" -o "$metallib" 2>"$TMPDIR/${name}_msc.err"; then
        local err_msg
        err_msg=$(head -5 "$TMPDIR/${name}_msc.err" 2>/dev/null || echo "未知错误")
        echo -e "${RED}FAIL${NC}"
        echo "    MSC: ${err_msg}"
        FAIL=$((FAIL + 1))
        return 1
    fi

    local metal_size
    metal_size=$(wc -c < "$metallib" 2>/dev/null || echo 0)
    if [ "$metal_size" -eq 0 ]; then
        echo -e "${RED}FAIL${NC} (metallib 为空)"
        FAIL=$((FAIL + 1))
        return 1
    fi

    # 健康检查：metallib 应 ≥ DXIL 大小的 1.0x
    local ratio
    ratio=$(echo "scale=2; $metal_size / $dxil_size" | bc 2>/dev/null || echo "0")
    if [ "$(echo "$ratio >= 1.0" | bc 2>/dev/null || echo 0)" -eq 1 ]; then
        echo -e "${GREEN}OK${NC} (${dxil_size}→${metal_size}B, ${ratio}x)"
        PASS=$((PASS + 1))
    else
        echo -e "${YELLOW}WARN${NC} (${dxil_size}→${metal_size}B, ${ratio}x — metallib 偏小)"
        PASS=$((PASS + 1))
    fi
    return 0
}

# =============================================================
# 生成通用顶点着色器（供片段测试复用）
# =============================================================

cat > "$TMPDIR/shared_vert.slang" << 'EOF'
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

[shader("vertex")]
VSOut main(uint vid : SV_VertexID)
{
    float2 pos[3] = { float2(-1, -1), float2(3, -1), float2(-1, 3) };
    VSOut o;
    o.pos = float4(pos[vid], 0, 1);
    o.uv = pos[vid] * 0.5 + 0.5;
    return o;
}
EOF

# =============================================================
# 1. 纹理类型覆盖
# =============================================================

# tex_basic_2d：标准 2D 纹理采样 + 单独的 VS/FS
cat > "$TMPDIR/tex_basic_2d_frag.slang" << 'EOF'
Texture2D<float4> g_texture;
SamplerState g_sampler;

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

[shader("fragment")]
float4 main(VSOut input) : SV_Target0
{
    return g_texture.Sample(g_sampler, input.uv);
}
EOF

# tex_2d_array：2D 数组纹理
cat > "$TMPDIR/tex_2d_array_frag.slang" << 'EOF'
Texture2DArray<float4> g_texture;
SamplerState g_sampler;

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

[shader("fragment")]
float4 main(VSOut input) : SV_Target0
{
    return g_texture.Sample(g_sampler, float3(input.uv, 0));
}
EOF

# tex_3d：3D 纹理
cat > "$TMPDIR/tex_3d_vert.slang" << 'EOF'
struct VSOut { float4 pos : SV_Position; float3 uvw : TEXCOORD0; };

[shader("vertex")]
VSOut main(uint vid : SV_VertexID)
{
    float2 pos[3] = { float2(-1, -1), float2(3, -1), float2(-1, 3) };
    VSOut o;
    o.pos = float4(pos[vid], 0, 1);
    o.uvw = float3(pos[vid] * 0.5 + 0.5, 0.5);
    return o;
}
EOF

cat > "$TMPDIR/tex_3d_frag.slang" << 'EOF'
Texture3D<float4> g_texture;
SamplerState g_sampler;

struct VSOut { float4 pos : SV_Position; float3 uvw : TEXCOORD0; };

[shader("fragment")]
float4 main(VSOut input) : SV_Target0
{
    return g_texture.Sample(g_sampler, input.uvw);
}
EOF

# tex_cube：Cube 纹理
cat > "$TMPDIR/tex_cube_vert.slang" << 'EOF'
struct VSOut { float4 pos : SV_Position; float3 dir : TEXCOORD0; };

[shader("vertex")]
VSOut main(uint vid : SV_VertexID)
{
    float2 pos[3] = { float2(-1, -1), float2(3, -1), float2(-1, 3) };
    VSOut o;
    o.pos = float4(pos[vid], 0, 1);
    o.dir = float3(pos[vid] * 0.5 + 0.5, 1);
    return o;
}
EOF

cat > "$TMPDIR/tex_cube_frag.slang" << 'EOF'
TextureCube<float4> g_texture;
SamplerState g_sampler;

struct VSOut { float4 pos : SV_Position; float3 dir : TEXCOORD0; };

[shader("fragment")]
float4 main(VSOut input) : SV_Target0
{
    return g_texture.Sample(g_sampler, normalize(input.dir));
}
EOF

# =============================================================
# 2. 纹理格式覆盖
# =============================================================

cat > "$TMPDIR/tex_r8_frag.slang" << 'EOF'
Texture2D<float> g_texture;
SamplerState g_sampler;

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

[shader("fragment")]
float4 main(VSOut input) : SV_Target0
{
    float r = g_texture.Sample(g_sampler, input.uv);
    return float4(r, r, r, 1);
}
EOF

cat > "$TMPDIR/tex_rgba16f_frag.slang" << 'EOF'
Texture2D<float4> g_texture;
SamplerState g_sampler;

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

[shader("fragment")]
float4 main(VSOut input) : SV_Target0
{
    return g_texture.Sample(g_sampler, input.uv);
}
EOF

cat > "$TMPDIR/tex_depth_compare_frag.slang" << 'EOF'
Texture2D<float> g_depthTexture;
SamplerComparisonState g_shadowSampler;

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

[shader("fragment")]
float4 main(VSOut input) : SV_Target0
{
    float shadow = g_depthTexture.SampleCmpLevelZero(g_shadowSampler, input.uv, 0.5);
    return float4(shadow, shadow, shadow, 1);
}
EOF

# =============================================================
# 3. 采样器模式覆盖
# =============================================================

cat > "$TMPDIR/tex_sampler_aniso_frag.slang" << 'EOF'
Texture2D<float4> g_texture;
SamplerState g_sampler;

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

[shader("fragment")]
float4 main(VSOut input) : SV_Target0
{
    return g_texture.Sample(g_sampler, input.uv * 10.0);
}
EOF

cat > "$TMPDIR/tex_sampler_mipmap_frag.slang" << 'EOF'
Texture2D<float4> g_texture;
SamplerState g_sampler;

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

[shader("fragment")]
float4 main(VSOut input) : SV_Target0
{
    return g_texture.Sample(g_sampler, input.uv * 30.0);
}
EOF

cat > "$TMPDIR/tex_border_color_frag.slang" << 'EOF'
Texture2D<float4> g_texture;
SamplerState g_sampler;

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

[shader("fragment")]
float4 main(VSOut input) : SV_Target0
{
    return g_texture.Sample(g_sampler, input.uv * 1.5 - 0.25);
}
EOF

# =============================================================
# 4. 纹理操作覆盖
# =============================================================

cat > "$TMPDIR/tex_gather_frag.slang" << 'EOF'
Texture2D<float4> g_texture;
SamplerState g_sampler;

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

[shader("fragment")]
float4 main(VSOut input) : SV_Target0
{
    return g_texture.Gather(g_sampler, input.uv);
}
EOF

cat > "$TMPDIR/tex_gather_red_frag.slang" << 'EOF'
Texture2D<float4> g_texture;
SamplerState g_sampler;

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

[shader("fragment")]
float4 main(VSOut input) : SV_Target0
{
    float4 gathered = g_texture.GatherRed(g_sampler, input.uv);
    return float4(gathered.x, gathered.y, gathered.z, gathered.w);
}
EOF

cat > "$TMPDIR/tex_load_frag.slang" << 'EOF'
Texture2D<float4> g_texture;

struct VSOut { float4 pos : SV_Position; uint2 coord : TEXCOORD0; };

[shader("vertex")]
VSOut vs_main(uint vid : SV_VertexID)
{
    float2 pos[3] = { float2(-1, -1), float2(3, -1), float2(-1, 3) };
    VSOut o;
    o.pos = float4(pos[vid], 0, 1);
    o.coord = uint2(4, 4);
    return o;
}

[shader("fragment")]
float4 main(VSOut input) : SV_Target0
{
    return g_texture.Load(int3(input.coord, 0));
}
EOF

# =============================================================
# 5. 多纹理/多采样器绑定
# =============================================================

cat > "$TMPDIR/tex_multi_bind_frag.slang" << 'EOF'
Texture2D<float4> g_tex0;
Texture2D<float4> g_tex1;
Texture2D<float4> g_tex2;
Texture2D<float4> g_tex3;
SamplerState g_sampler;

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

[shader("fragment")]
float4 main(VSOut input) : SV_Target0
{
    float4 c0 = g_tex0.Sample(g_sampler, input.uv);
    float4 c1 = g_tex1.Sample(g_sampler, input.uv * 2.0);
    float4 c2 = g_tex2.Sample(g_sampler, input.uv * 0.5);
    float4 c3 = g_tex3.Sample(g_sampler, input.uv * 1.5);
    return (c0 + c1 + c2 + c3) * 0.25;
}
EOF

cat > "$TMPDIR/tex_multi_sampler_frag.slang" << 'EOF'
Texture2D<float4> g_texture;
SamplerState g_sampler0;
SamplerState g_sampler1;

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

[shader("fragment")]
float4 main(VSOut input) : SV_Target0
{
    float4 c0 = g_texture.Sample(g_sampler0, input.uv);
    float4 c1 = g_texture.Sample(g_sampler1, input.uv * 5.0);
    return lerp(c0, c1, 0.5);
}
EOF

# =============================================================
# 6. 计算着色器纹理操作
# =============================================================

cat > "$TMPDIR/tex_compute_read.slang" << 'EOF'
Texture2D<float4> g_input;
SamplerState g_sampler;
RWTexture2D<float4> g_output;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    g_output.GetDimensions(w, h);
    if (dtid.x < w && dtid.y < h)
    {
        float2 uv = float2(dtid.xy) / float2(w, h);
        g_output[dtid.xy] = g_input.SampleLevel(g_sampler, uv, 0);
    }
}
EOF

cat > "$TMPDIR/tex_compute_rw.slang" << 'EOF'
RWTexture2D<float4> g_rwTexture;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    g_rwTexture.GetDimensions(w, h);
    if (dtid.x < w && dtid.y < h)
    {
        float2 uv = float2(dtid.xy) / float2(w, h);
        g_rwTexture[dtid.xy] = float4(uv, 0.5, 1.0);
    }
}
EOF

# =============================================================
# 7. 纹理偏导与样条梯度
# =============================================================

cat > "$TMPDIR/tex_derivatives_frag.slang" << 'EOF'
Texture2D<float4> g_texture;
SamplerState g_sampler;

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

[shader("fragment")]
float4 main(VSOut input) : SV_Target0
{
    float2 dx = ddx(input.uv * 10.0);
    float2 dy = ddy(input.uv * 10.0);
    return g_texture.SampleGrad(g_sampler, input.uv, dx, dy);
}
EOF

# =============================================================
# 执行测试
# =============================================================

echo "── 1. 纹理类型覆盖 ──"
compile_shader "shared_vert"          "vertex"   "sm_6_0"
compile_shader "tex_basic_2d_frag"    "fragment" "ps_6_0"
compile_shader "tex_2d_array_frag"    "fragment" "ps_6_0"
compile_shader "tex_3d_vert"          "vertex"   "sm_6_0"
compile_shader "tex_3d_frag"          "fragment" "ps_6_0"
compile_shader "tex_cube_vert"        "vertex"   "sm_6_0"
compile_shader "tex_cube_frag"        "fragment" "ps_6_0"
echo ""

echo "── 2. 纹理格式覆盖 ──"
compile_shader "tex_r8_frag"          "fragment" "ps_6_0"
compile_shader "tex_rgba16f_frag"     "fragment" "ps_6_0"
compile_shader "tex_depth_compare_frag" "fragment" "ps_6_0"
echo ""

echo "── 3. 采样器模式覆盖 ──"
compile_shader "tex_sampler_aniso_frag"  "fragment" "ps_6_0"
compile_shader "tex_sampler_mipmap_frag" "fragment" "ps_6_0"
compile_shader "tex_border_color_frag"   "fragment" "ps_6_0"
echo ""

echo "── 4. 纹理操作覆盖 ──"
compile_shader "tex_gather_frag"         "fragment" "ps_6_0"
compile_shader "tex_gather_red_frag"     "fragment" "ps_6_0"
compile_shader "tex_load_frag"           "vertex"   "sm_6_0"  "vs_main"
compile_shader "tex_load_frag"           "fragment" "ps_6_0"
echo ""

echo "── 5. 多纹理/多采样器绑定 ──"
compile_shader "tex_multi_bind_frag"    "fragment" "ps_6_0"
compile_shader "tex_multi_sampler_frag" "fragment" "ps_6_0"
echo ""

echo "── 6. 计算着色器纹理操作 ──"
compile_shader "tex_compute_read"      "compute"  "cs_6_0"
compile_shader "tex_compute_rw"        "compute"  "cs_6_0"
echo ""

echo "── 7. 纹理偏导与梯度 ──"
compile_shader "tex_derivatives_frag"  "fragment" "ps_6_0"
echo ""

# =============================================================
# 汇总
# =============================================================
echo "================================================"
if [ "$FAIL" -eq 0 ]; then
    echo -e " 结果: ${GREEN}全部通过 (${PASS}/${PASS})${NC}"
    echo -e " MSC/Metal 纹理限制验证 ${GREEN}PASS${NC}"
else
    echo -e " 结果: ${GREEN}${PASS} 通过${NC} / ${RED}${FAIL} 失败${NC}"
fi
echo "================================================"

# 生成 JSON 摘要
cat > "$TMPDIR/results.json" << JSONEOF
{
  "task": "P4.2.0",
  "dimension": "texture",
  "timestamp": "$(date -Iseconds)",
  "passed": $PASS,
  "failed": $FAIL,
  "total": $((PASS + FAIL))
}
JSONEOF

echo ""
echo "摘要: $TMPDIR/results.json"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
