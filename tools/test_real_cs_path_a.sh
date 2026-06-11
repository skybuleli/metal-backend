#!/bin/bash
# test_real_cs_path_a.sh — P1.8: 真实/近真实计算着色器 Path A 测试
# 数据源: Ryujinx 缓存扫描 + deko3d compute 样本 + 规整 Slang compute 语料
# 目标: 验证计算阶段 Path A 管线不崩，并记录真实缓存是否存在 compute 样本
# 用法: bash tools/test_real_cs_path_a.sh
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

PASS=0
FAIL=0
SKIP=0
TOTAL=0

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

echo "============================================"
echo " P1.8 — 真实/近真实计算着色器 Path A 测试"
echo " 时间: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"
echo ""

# ── Path A 管线函数 ──
# 参数: $1=名称 $2=源文件路径 $3=来源类型(slang|glsl)
run_path_a_compute() {
    local name="$1"
    local src="$2"
    local kind="${3:-slang}"
    local dxil="$TMPDIR/${name}.dxil"
    local ml="$TMPDIR/${name}.metallib"
    TOTAL=$((TOTAL + 1))

    echo -e "${CYAN}[$TOTAL] ${name}${NC}"
    echo -e "   来源: $src"
    echo -e "   类型: $kind"

    if [ ! -f "$src" ]; then
        echo -e "   ${RED}FAIL${NC} — 文件不存在"
        FAIL=$((FAIL + 1))
        echo ""
        return 1
    fi

    local src_size
    src_size=$(wc -c < "$src")
    echo -e "   源文件大小: $src_size bytes"

    echo -n "   [slangc→DXIL]     ... "
    if slangc "$src" -target dxil -entry main -stage compute -profile cs_6_0 -o "$dxil" 2>"$TMPDIR/${name}_slang.err"; then
        if [ -s "$dxil" ]; then
            local dxil_size
            dxil_size=$(wc -c < "$dxil")
            echo -e "${GREEN}OK${NC} ($dxil_size bytes)"
            if [ -s "$TMPDIR/${name}_slang.err" ]; then
                local warn_msg
                warn_msg=$(head -3 "$TMPDIR/${name}_slang.err" | tr '\n' ' ')
                echo -e "   ${YELLOW}slangc 提示: $warn_msg${NC}"
            fi
        else
            echo -e "${RED}FAIL${NC} (未生成 DXIL 或 DXIL 为空)"
            FAIL=$((FAIL + 1))
            echo ""
            return 1
        fi
    else
        local err_msg
        err_msg=$(head -3 "$TMPDIR/${name}_slang.err" 2>/dev/null | tr '\n' ' ' || echo "未知错误")
        echo -e "${RED}FAIL${NC} (slangc)"
        echo -e "   ${YELLOW}${err_msg}${NC}"
        FAIL=$((FAIL + 1))
        echo ""
        return 1
    fi

    echo -n "   [MSC→metallib]    ... "
    if metal-shaderconverter "$dxil" -o "$ml" 2>"$TMPDIR/${name}_msc.err"; then
        if [ -s "$ml" ]; then
            local dxil_size ml_size
            dxil_size=$(wc -c < "$dxil")
            ml_size=$(wc -c < "$ml")
            if [ "$ml_size" -gt "$dxil_size" ]; then
                echo -e "${GREEN}OK${NC} ($dxil_size → $ml_size bytes)"
                PASS=$((PASS + 1))
            else
                echo -e "${RED}FAIL${NC} (metallib 未大于 DXIL: $ml_size ≤ $dxil_size)"
                FAIL=$((FAIL + 1))
                echo ""
                return 1
            fi
        else
            echo -e "${RED}FAIL${NC} (未生成 metallib 或 metallib 为空)"
            FAIL=$((FAIL + 1))
            echo ""
            return 1
        fi
    else
        local err_msg
        err_msg=$(head -3 "$TMPDIR/${name}_msc.err" 2>/dev/null | tr '\n' ' ' || echo "未知错误")
        echo -e "${RED}FAIL${NC} (MSC)"
        echo -e "   ${YELLOW}${err_msg}${NC}"
        FAIL=$((FAIL + 1))
        echo ""
        return 1
    fi

    echo -e "   ${GREEN}Path A 端到端 PASS${NC}"
    echo ""
    return 0
}

# ── Path A 对照样本：预期不走主线的输入只记录为预期问题 ──
# 参数: $1=名称 $2=源文件路径 $3=原因
check_expected_path_a_issue() {
    local name="$1"
    local src="$2"
    local reason="$3"
    local dxil="$TMPDIR/${name}.dxil"
    TOTAL=$((TOTAL + 1))

    echo -e "${CYAN}[$TOTAL] ${name}${NC}"
    echo -e "   来源: $src"

    if [ ! -f "$src" ]; then
        echo -e "   ${YELLOW}跳过${NC} — 文件不存在"
        SKIP=$((SKIP + 1))
        echo ""
        return 0
    fi

    echo -n "   [slangc→DXIL]     ... "
    if slangc "$src" -target dxil -entry main -stage compute -profile cs_6_0 -o "$dxil" 2>"$TMPDIR/${name}_slang.err"; then
        if [ -s "$dxil" ]; then
            local dxil_size
            dxil_size=$(wc -c < "$dxil")
            echo -e "${YELLOW}意外可用${NC} ($dxil_size bytes)"
            echo -e "   ${YELLOW}说明: $reason 已不再复现，可考虑后续改为正式通过样本${NC}"
            SKIP=$((SKIP + 1))
        else
            local warn_msg
            warn_msg=$(head -3 "$TMPDIR/${name}_slang.err" 2>/dev/null | tr '\n' ' ' || echo "无错误输出")
            echo -e "${YELLOW}预期问题${NC} (slangc 返回 0 但无 DXIL)"
            echo -e "   ${YELLOW}原因: $reason${NC}"
            echo -e "   ${YELLOW}${warn_msg}${NC}"
            SKIP=$((SKIP + 1))
        fi
    else
        local err_msg
        err_msg=$(head -3 "$TMPDIR/${name}_slang.err" 2>/dev/null | tr '\n' ' ' || echo "未知错误")
        echo -e "${YELLOW}预期问题${NC} (slangc 失败)"
        echo -e "   ${YELLOW}原因: $reason${NC}"
        echo -e "   ${YELLOW}${err_msg}${NC}"
        SKIP=$((SKIP + 1))
    fi
    echo ""
    return 0
}

# ── MSL kernel 检测函数（仅报告，不跑 Path A）──
check_compute_msl() {
    local name="$1"
    local src="$2"
    TOTAL=$((TOTAL + 1))

    echo -e "${CYAN}[$TOTAL] ${name}${NC}"
    echo -e "   来源: $src"

    if [ ! -f "$src" ]; then
        echo -e "   ${YELLOW}跳过${NC} — 文件不存在"
        SKIP=$((SKIP + 1))
        echo ""
        return 0
    fi

    local src_size src_lines
    src_size=$(wc -c < "$src")
    src_lines=$(wc -l < "$src")

    if grep -q "metal_stdlib" "$src" 2>/dev/null && grep -q "kernel" "$src" 2>/dev/null; then
        echo -e "   大小: $src_size bytes ($src_lines 行)"
        echo -e "   ${GREEN}MSL kernel 有效${NC} (含 metal_stdlib + kernel)"
        echo -e "   ${YELLOW}跳过${NC} — MSL→metallib 需 xcrun metal (完整 Xcode)"
        SKIP=$((SKIP + 1))
    else
        echo -e "   ${YELLOW}非 Compute MSL${NC} (缺少 metal_stdlib 或 kernel 关键字)"
        SKIP=$((SKIP + 1))
    fi
    echo ""
    return 0
}

echo "══ 第一部分: Ryujinx 本地缓存 compute 侦察 ══"
echo ""

RYUJINX_ROOT="$HOME/Library/Application Support/Ryujinx"
RYUJINX_COMPUTE_FOUND=0

if [ -d "$RYUJINX_ROOT/msl_dump" ]; then
    while IFS= read -r file; do
        if grep -q "kernel" "$file" 2>/dev/null; then
            RYUJINX_COMPUTE_FOUND=1
            check_compute_msl "ryujinx_$(basename "$file" .msl)" "$file" || true
        fi
    done <<EOF
$(find "$RYUJINX_ROOT/msl_dump" -maxdepth 1 -type f -name "*.msl" | sort)
EOF
fi

if [ -d "$RYUJINX_ROOT/shader_dump" ]; then
    while IFS= read -r file; do
        if grep -Eq "kernel|numthreads|SV_DispatchThreadID|gl_GlobalInvocationID|local_size" "$file" 2>/dev/null; then
            RYUJINX_COMPUTE_FOUND=1
            check_compute_msl "ryujinx_shader_dump_$(basename "$file")" "$file" || true
        fi
    done <<EOF
$(find "$RYUJINX_ROOT/shader_dump" -maxdepth 1 -type f | sort)
EOF
fi

if [ "$RYUJINX_COMPUTE_FOUND" -eq 0 ]; then
    TOTAL=$((TOTAL + 1))
    SKIP=$((SKIP + 1))
    echo -e "${CYAN}[$TOTAL] ryujinx_compute_cache_scan${NC}"
    echo -e "   ${YELLOW}未发现真实 Ryujinx compute MSL / compute shader dump${NC}"
    echo -e "   ${YELLOW}说明: 当前本地缓存仅有 Vertex/Fragment；P1.8 使用 deko3d 近真实 compute 与规整 Slang 样本继续验证 Path A${NC}"
    echo ""
fi

echo "══ 第二部分: deko3d 近真实 compute 样本 ══"
echo ""

RAW_SINE="$HOME/autommes/deko3d_slang_poc/test_output/real_deko3d_sinewave.glsl.slang"
run_path_a_compute "deko3d_raw_sinewave_cs" "$RAW_SINE" "slang" || true

MSL_SINE="$HOME/autommes/deko3d_slang_poc/test_output/real_deko3d_sinewave.glsl.msl"
check_compute_msl "deko3d_sinewave_msl_kernel" "$MSL_SINE" || true

cat > "$TMPDIR/cs_sinewave_normalized.slang" << 'SLANG'
RWStructuredBuffer<float4> buf_0 : register(u0);

cbuffer ConstBuf_1 : register(b1) {
    float4 color_a;
    float4 color_b;
    float4 params;
};

[shader("compute")]
[numthreads(32, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID, uint3 group_id : SV_GroupID)
{
    uint idx = dispatch_id.x;
    float denom = 1023.0;
    float t = (float)idx / denom;
    float wave = sin((params.x + t) * 6.3) * params.y;
    float4 pos = float4(t * 2.0 - 1.0, wave, 1.0, 1.0);
    float4 color = lerp(color_a, color_b, t);
    buf_0[idx * 2] = pos;
    buf_0[idx * 2 + 1] = color + float4((float)group_id.x * 0.0, 0.0, 0.0, 0.0);
}
SLANG
run_path_a_compute "deko3d_sinewave_normalized_cs" "$TMPDIR/cs_sinewave_normalized.slang" "slang" || true

echo "══ 第三部分: 内嵌 compute 特性覆盖 → Path A ══"
echo ""

cat > "$TMPDIR/cs_min_rwbuffer.slang" << 'SLANG'
RWStructuredBuffer<float4> out_buf : register(u0);

[shader("compute")]
[numthreads(32, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    out_buf[dispatch_id.x] = float4((float)dispatch_id.x, 0.0, 0.0, 1.0);
}
SLANG
run_path_a_compute "feat_min_rwbuffer" "$TMPDIR/cs_min_rwbuffer.slang" "slang" || true

cat > "$TMPDIR/cs_groupshared_barrier.slang" << 'SLANG'
RWStructuredBuffer<uint> out_buf : register(u0);
groupshared uint shared_values[32];

[shader("compute")]
[numthreads(32, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID, uint3 local_id : SV_GroupThreadID)
{
    shared_values[local_id.x] = dispatch_id.x;
    GroupMemoryBarrierWithGroupSync();
    out_buf[dispatch_id.x] = shared_values[31 - local_id.x];
}
SLANG
run_path_a_compute "feat_groupshared_barrier" "$TMPDIR/cs_groupshared_barrier.slang" "slang" || true

cat > "$TMPDIR/cs_atomic.slang" << 'SLANG'
RWStructuredBuffer<uint> counters : register(u0);

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint old_value = 0;
    InterlockedAdd(counters[0], 1, old_value);
    counters[dispatch_id.x + 1] = old_value;
}
SLANG
run_path_a_compute "feat_atomic_add" "$TMPDIR/cs_atomic.slang" "slang" || true

cat > "$TMPDIR/cs_byteaddress.slang" << 'SLANG'
RWByteAddressBuffer out_bytes : register(u0);

[shader("compute")]
[numthreads(16, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint offset = dispatch_id.x * 4;
    out_bytes.Store(offset, dispatch_id.x);
}
SLANG
run_path_a_compute "feat_byteaddress_store" "$TMPDIR/cs_byteaddress.slang" "slang" || true

cat > "$TMPDIR/cs_rwtexture.slang" << 'SLANG'
RWTexture2D<float4> out_tex : register(u0);

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    float2 uv = float2((float)dispatch_id.x, (float)dispatch_id.y) / 64.0;
    out_tex[dispatch_id.xy] = float4(uv, 0.0, 1.0);
}
SLANG
run_path_a_compute "feat_rwtexture_store" "$TMPDIR/cs_rwtexture.slang" "slang" || true

cat > "$TMPDIR/cs_glsl_control.comp.glsl" << 'GLSL'
#version 460
layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
layout(std430, binding = 0) buffer Output {
    vec4 values[];
} out_buf;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    out_buf.values[idx] = vec4(float(idx), 0.0, 0.0, 1.0);
}
GLSL
check_expected_path_a_issue "glsl_compute_std430_control" "$TMPDIR/cs_glsl_control.comp.glsl" \
    "GLSL compute 的 std430/storage buffer 属于 Path C 对照语料；Path A 主线应由 CommandMapper 输出 Slang 原生语法" || true

echo "============================================"
echo -e " 结果: ${GREEN}$PASS Path A 通过${NC} / ${RED}$FAIL 失败${NC} / ${YELLOW}$SKIP 侦察或预期问题${NC} / 总计 $TOTAL"
echo ""
if [ "$FAIL" -eq 0 ]; then
    echo -e " ${GREEN}计算阶段管线不崩！${NC}规整 Slang compute 样本均通过 Path A 编译"
    echo -e " ${YELLOW}注: 当前本地 Ryujinx 缓存未发现 compute；GLSL std430 compute 仅作为 Path C 对照语料${NC}"
else
    echo -e " ${RED}$FAIL 个计算着色器编译失败${NC}"
fi
echo "============================================"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
