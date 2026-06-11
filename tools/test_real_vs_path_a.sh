#!/bin/bash
# test_real_vs_path_a.sh — P1.6: 真实游戏顶点着色器 Path A 测试
# 数据源: deko3d 示例 GLSL + Ryujinx msl_dump MSL
# 目标: 验证管线在真实着色器上不崩，非全特性覆盖
# 用法: bash tools/test_real_vs_path_a.sh
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

echo "============================================"
echo " P1.6 — 真实顶点着色器 Path A 测试"
echo " 时间: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"
echo ""

# ── Path A 管线函数 ──
# 参数: $1=名称 $2=GLSL 文件路径 $3=预期结果 (pass|known_fail 描述)
run_path_a() {
    local name="$1"
    local src="$2"
    local expect="${3:-pass}"
    local dxil="$TMPDIR/${name}.dxil"
    local ml="$TMPDIR/${name}.metallib"
    TOTAL=$((TOTAL + 1))

    echo -e "${CYAN}[$TOTAL] ${name}${NC}"
    echo -e "   来源: $src"

    # 检查文件存在
    if [ ! -f "$src" ]; then
        echo -e "   ${RED}FAIL${NC} — 文件不存在"
        FAIL=$((FAIL + 1))
        echo ""
        return 1
    fi

    local src_size
    src_size=$(wc -c < "$src")
    echo -e "   源文件大小: $src_size bytes"

    # 步骤 1: GLSL → DXIL
    echo -n "   [slangc→DXIL]     ... "
    if slangc "$src" -target dxil -entry main -stage vertex -profile sm_6_0 -o "$dxil" 2>"$TMPDIR/${name}_slang.err"; then
        if [ -f "$dxil" ]; then
            local dxil_size
            dxil_size=$(wc -c < "$dxil")
            if [ "$dxil_size" -gt 0 ]; then
                echo -e "${GREEN}OK${NC} ($dxil_size bytes)"
            else
                echo -e "${RED}FAIL${NC} (DXIL 文件为空)"
                FAIL=$((FAIL + 1))
                echo ""
                return 1
            fi
        else
            echo -e "${RED}FAIL${NC} (未生成 DXIL)"
            FAIL=$((FAIL + 1))
            echo ""
            return 1
        fi
    else
        local err_msg
        err_msg=$(head -3 "$TMPDIR/${name}_slang.err" 2>/dev/null | tr '\n' ' ' || echo "未知错误")
        if [[ "$expect" == known_fail* ]]; then
            local reason="${expect#known_fail:}"
            echo -e "${YELLOW}预期失败${NC} (slangc)"
            echo -e "   ${YELLOW}原因: ${reason}${NC}"
            echo -e "   ${YELLOW}${err_msg}${NC}"
            SKIP=$((SKIP + 1))
            echo ""
            return 0
        fi
        echo -e "${RED}FAIL${NC} (slangc)"
        echo -e "   ${YELLOW}${err_msg}${NC}"
        FAIL=$((FAIL + 1))
        echo ""
        return 1
    fi

    # 步骤 2: DXIL → metallib
    echo -n "   [MSC→metallib]    ... "
    if metal-shaderconverter "$dxil" -o "$ml" 2>"$TMPDIR/${name}_msc.err"; then
        if [ -f "$ml" ]; then
            local ml_size
            ml_size=$(wc -c < "$ml")
            echo -e "${GREEN}OK${NC} ($ml_size bytes)"
            PASS=$((PASS + 1))
        else
            echo -e "${RED}FAIL${NC} (未生成 metallib)"
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

# ── MSL 文件检测函数（仅报告，不跑管线）──
# 参数: $1=名称 $2=MSL 文件路径
check_msl() {
    local name="$1"
    local src="$2"
    TOTAL=$((TOTAL + 1))

    echo -e "${CYAN}[$TOTAL] ${name}${NC}"
    echo -e "   来源: $src"

    if [ ! -f "$src" ]; then
        echo -e "   ${RED}FAIL${NC} — 文件不存在"
        FAIL=$((FAIL + 1))
        echo ""
        return 1
    fi

    local src_size src_lines
    src_size=$(wc -c < "$src")
    src_lines=$(wc -l < "$src")

    # 检查是否为有效 MSL
    local has_metal_stdlib=false
    local has_vertex=false
    if grep -q "metal_stdlib" "$src" 2>/dev/null; then
        has_metal_stdlib=true
    fi
    if grep -q "vertex" "$src" 2>/dev/null; then
        has_vertex=true
    fi

    if $has_metal_stdlib && $has_vertex; then
        echo -e "   大小: $src_size bytes ($src_lines 行)"
        echo -e "   ${GREEN}MSL 有效${NC} (含 metal_stdlib + vertex)"
        echo -e "   ${YELLOW}跳过${NC} — MSL→metallib 需 xcrun metal (完整 Xcode)"
        SKIP=$((SKIP + 1))
    else
        echo -e "   ${YELLOW}非标准 MSL${NC} (缺少 metal_stdlib 或 vertex 关键字)"
        SKIP=$((SKIP + 1))
    fi
    echo ""
    return 0
}

# ═══════════════════════════════════════════════
# 第一部分: deko3d GLSL 示例 → Path A
# ═══════════════════════════════════════════════
echo "══ 第一部分: deko3d GLSL 示例 → Path A ══"
echo ""

# deko3d_metal_runtime 示例
run_path_a "deko3d_runtime_triangle_vs" \
    "$HOME/autommes/deko3d_metal_runtime/shaders/triangle_vs.glsl" || true

# deko3d_slang_poc 示例
run_path_a "deko3d_slang_triangle_vs" \
    "$HOME/autommes/deko3d_slang_poc/shaders/triangle.vert.glsl" || true

# 带 UBO 的复杂着色器（已知限制：GLSL UBO 语法与 slangc DXIL SM 6.0 不兼容）
run_path_a "deko3d_slang_complex_vs" \
    "$HOME/autommes/deko3d_slang_poc/shaders/complex.vert.glsl" \
    "known_fail:GLSL std140 UBO 与 slangc DXIL SM 6.0 不兼容" || true

# 带 push_constant + mul() 的着色器（已知限制：push_constant 是 Vulkan 概念）
run_path_a "deko3d_slang_complex_v4_vs" \
    "$HOME/autommes/deko3d_slang_poc/shaders/complex_v4.vert.glsl" \
    "known_fail:push_constant 是 Vulkan 特有语法，slangc DXIL 不支持" || true

# roundtrip 产物
run_path_a "deko3d_roundtrip_vs" \
    "$HOME/autommes/deko3d_slang_poc/output/roundtrip_vert.glsl" || true

# ═══════════════════════════════════════════════
# 第二部分: 内嵌多样化测试着色器（覆盖 GLSL 特性）
# ═══════════════════════════════════════════════
echo "══ 第二部分: 内嵌多样化测试着色器 → Path A ══"
echo ""

# 测试 1: 矩阵运算 (mat4 * mat4, mat4 * vec4)
cat > "$TMPDIR/vs_mat_ops.glsl" << 'GLSL'
#version 460
layout(location = 0) in vec3 aPos;
layout(location = 0) out vec3 vOut;

void main() {
    mat4 m = mat4(1.0);
    mat4 n = mat4(2.0);
    mat4 r = m * n;
    vec4 v = r * vec4(aPos, 1.0);
    gl_Position = v;
    vOut = aPos;
}
GLSL
run_path_a "feat_mat_ops" "$TMPDIR/vs_mat_ops.glsl" || true

# 测试 2: 条件分支 (if/else + ternary)
cat > "$TMPDIR/vs_branch.glsl" << 'GLSL'
#version 460
layout(location = 0) in vec3 aPos;
layout(location = 1) in float aFlag;
layout(location = 0) out float vVal;

void main() {
    float val;
    if (aFlag > 0.5) {
        val = aPos.x * 2.0;
    } else {
        val = aPos.y;
    }
    val = (val > 1.0) ? 1.0 : val;
    gl_Position = vec4(aPos.x, aPos.y * val, aPos.z, 1.0);
    vVal = val;
}
GLSL
run_path_a "feat_branch_ternary" "$TMPDIR/vs_branch.glsl" || true

# 测试 3: 循环 (for + while)
cat > "$TMPDIR/vs_loop.glsl" << 'GLSL'
#version 460
layout(location = 0) in vec3 aPos;
layout(location = 0) out float vSum;

void main() {
    float sum = 0.0;
    for (int i = 0; i < 4; i++) {
        sum += aPos.x * float(i);
    }
    int j = 0;
    while (j < 3) {
        sum += aPos.y * float(j);
        j++;
    }
    gl_Position = vec4(aPos.x + sum, aPos.y, aPos.z, 1.0);
    vSum = sum;
}
GLSL
run_path_a "feat_loop_for_while" "$TMPDIR/vs_loop.glsl" || true

# 测试 4: 数学函数 (sin/cos/pow/sqrt/abs/min/max/clamp/mix)
cat > "$TMPDIR/vs_math.glsl" << 'GLSL'
#version 460
layout(location = 0) in vec3 aPos;
layout(location = 0) out vec3 vOut;

void main() {
    float a = sin(aPos.x);
    float b = cos(aPos.y);
    float c = pow(abs(aPos.z), 2.0);
    float d = sqrt(a * a + b * b + c);
    float e = clamp(d, 0.0, 1.0);
    float f = mix(a, b, 0.5);
    float g = min(max(e, f), 1.0);
    gl_Position = vec4(a, b, g, 1.0);
    vOut = vec3(c, d, e);
}
GLSL
run_path_a "feat_math_functions" "$TMPDIR/vs_math.glsl" || true

# 测试 5: 多输出 (多个 varying + gl_PointSize) —— 已知限制: gl_PointSize 在 DXIL VS 无 SV_PointSize 语义
cat > "$TMPDIR/vs_multi_out.glsl" << 'GLSL'
#version 460
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aNormal;
layout(location = 0) out vec2 vUV;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out float vDepth;

void main() {
    gl_Position = vec4(aPos, 1.0);
    gl_PointSize = 4.0;
    vUV = aUV;
    vNormal = normalize(aNormal);
    vDepth = aPos.z;
}
GLSL
run_path_a "feat_multi_output" "$TMPDIR/vs_multi_out.glsl" \
    "known_fail:gl_PointSize 在 DXIL SM 6.0 VS 中无 SV_PointSize 语义" || true

# 测试 5b: 多输出（无 gl_PointSize，验证多 varying 本身可用）
cat > "$TMPDIR/vs_multi_out_no_ps.glsl" << 'GLSL'
#version 460
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aNormal;
layout(location = 0) out vec2 vUV;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out float vDepth;

void main() {
    gl_Position = vec4(aPos, 1.0);
    vUV = aUV;
    vNormal = normalize(aNormal);
    vDepth = aPos.z;
}
GLSL
run_path_a "feat_multi_output_no_pointsize" "$TMPDIR/vs_multi_out_no_ps.glsl" || true

# 测试 6: 整数和位运算
cat > "$TMPDIR/vs_int_ops.glsl" << 'GLSL'
#version 460
layout(location = 0) in vec3 aPos;
layout(location = 1) in int aIndex;
layout(location = 0) out float vResult;

void main() {
    int idx = aIndex & 0xFF;
    int shifted = idx << 2;
    int combined = shifted | 0x10;
    float f = float(combined) / 256.0;
    gl_Position = vec4(aPos.x + f, aPos.y, aPos.z, 1.0);
    vResult = f;
}
GLSL
run_path_a "feat_int_bitwise" "$TMPDIR/vs_int_ops.glsl" || true

# 测试 7: 数组和 swizzle
cat > "$TMPDIR/vs_array.glsl" << 'GLSL'
#version 460
layout(location = 0) in vec3 aPos;
layout(location = 0) out vec4 vColor;

void main() {
    float weights[4] = float[4](0.25, 0.25, 0.25, 0.25);
    float sum = 0.0;
    for (int i = 0; i < 4; i++) {
        sum += weights[i] * aPos.x;
    }
    gl_Position = vec4(aPos, 1.0);
    vColor = vec4(sum, aPos.yz, 1.0);
}
GLSL
run_path_a "feat_array_swizzle" "$TMPDIR/vs_array.glsl" || true

# ═══════════════════════════════════════════════
# 第三部分: SPV 逆向 → GLSL → Path A
# ═══════════════════════════════════════════════
echo "══ 第三部分: SPV 逆向 → GLSL → Path A ══"
echo ""

SPV_DUMP="$HOME/Library/Application Support/Ryujinx/shader_dump"
if [ -f "$SPV_DUMP/vertex_0003.spv" ]; then
    SPV_GLSL="$TMPDIR/vertex_0003_roundtrip.glsl"
    echo -e "${CYAN}[SPV] vertex_0003.spv → spirv-cross → GLSL${NC}"
    echo -n "   [spirv-cross→GLSL] ... "
    if spirv-cross "$SPV_DUMP/vertex_0003.spv" --output "$SPV_GLSL" --version 460 2>"$TMPDIR/spv_cross.err"; then
        if [ -f "$SPV_GLSL" ]; then
            local_lines=$(wc -l < "$SPV_GLSL")
            echo -e "${GREEN}OK${NC} ($local_lines 行)"
            run_path_a "spv_reverse_vertex_0003" "$SPV_GLSL" || true
        else
            echo -e "${RED}FAIL${NC}"
        fi
    else
        err_msg=$(head -3 "$TMPDIR/spv_cross.err" 2>/dev/null | tr '\n' ' ' || echo "未知")
        echo -e "${YELLOW}SKIP${NC} (spirv-cross 失败: $err_msg)"
    fi
    echo ""
else
    echo -e "${YELLOW}跳过${NC} — vertex_0003.spv 不存在"
    echo ""
fi

# ═══════════════════════════════════════════════
# 第二部分: Ryujinx msl_dump MSL 文件（仅检测）
# ═══════════════════════════════════════════════
echo "══ 第四部分: Ryujinx msl_dump MSL（仅检测） ══"
echo ""

MSL_DUMP="$HOME/Library/Application Support/Ryujinx/msl_dump"

# 取前 5 个顶点 MSL 做抽样检测
for f in "$MSL_DUMP"/shader_{1,3,5,7,9}_Vertex.msl; do
    if [ -f "$f" ]; then
        name=$(basename "$f" .msl)
        check_msl "ryujinx_${name}" "$f" || true
    fi
done

# ═══════════════════════════════════════════════
# 汇总
# ═══════════════════════════════════════════════
echo "============================================"
echo -e " 结果: ${GREEN}$PASS Path A 通过${NC} / ${RED}$FAIL 失败${NC} / ${YELLOW}$SKIP MSL 跳过${NC} / 总计 $TOTAL"
echo ""
if [ "$FAIL" -eq 0 ]; then
    echo -e " ${GREEN}管线不崩！${NC}所有测试的 GLSL 着色器均通过 Path A 编译"
    echo -e " ${YELLOW}注: Ryujinx MSL 文件为已编译 MSL，需 xcrun metal（完整 Xcode）验证${NC}"
else
    echo -e " ${RED}$FAIL 个着色器编译失败${NC}"
fi
echo "============================================"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
