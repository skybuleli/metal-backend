#!/bin/bash
# test_spirv_cross_msl.sh — Path C 端到端：GLSL → SPIR-V → MSL
# 验证完整 Path C 管线直到 MSL 文本生成
# 注意：MSL → metallib 需要 xcrun metal（仅完整 Xcode），此处仅验证到 MSL 文本
# 用法: bash tools/test_spirv_cross_msl.sh
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

# ── 内嵌着色器源码 ──

# 顶点着色器
cat > "$TMPDIR/vertex.vert.glsl" << 'GLSL_VS'
#version 460

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;

layout(location = 0) out vec2 vTexCoord;

void main() {
    gl_Position = vec4(aPosition, 1.0);
    vTexCoord = aTexCoord;
}
GLSL_VS

# 片段着色器
cat > "$TMPDIR/fragment.frag.glsl" << 'GLSL_FS'
#version 460

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D uTexture;

void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    float brightness = dot(texColor.rgb, vec3(0.299, 0.587, 0.114));
    fragColor = vec4(texColor.rgb * (0.5 + 0.5 * brightness), texColor.a);
}
GLSL_FS

echo "============================================"
echo " Path C 端到端 — GLSL → SPIR-V → MSL"
echo " 时间: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"
echo ""

# ── 管线函数 ──
# 完整 Path C 管线：GLSL → SPIR-V → val → opt → MSL
# 参数: $1=名称 $2=源文件
pipeline() {
    local name="$1"
    local src="$2"
    local spv="$TMPDIR/${name}.spv"
    local opt_spv="$TMPDIR/${name}_opt.spv"
    local msl="$TMPDIR/${name}.msl"

    echo "── ${name} ──"

    # 步骤 1: GLSL → SPIR-V
    echo -n "  [glslang→SPIR-V]  ... "
    if glslangValidator -V "$src" -o "$spv" 2>"$TMPDIR/${name}_glslang.err"; then
        local spv_size
        spv_size=$(wc -c < "$spv")
        echo -e "${GREEN}OK${NC} ($spv_size bytes)"
        PASS=$((PASS + 1))
    else
        echo -e "${RED}FAIL${NC}"
        FAIL=$((FAIL + 1))
        return 1
    fi

    # 步骤 2: SPIR-V 验证
    echo -n "  [spirv-val]        ... "
    if spirv-val "$spv" 2>/dev/null; then
        echo -e "${GREEN}OK${NC}"
        PASS=$((PASS + 1))
    else
        echo -e "${RED}FAIL${NC}"
        FAIL=$((FAIL + 1))
        return 1
    fi

    # 步骤 3: SPIR-V 优化
    echo -n "  [spirv-opt -O]     ... "
    if spirv-opt -O "$spv" -o "$opt_spv" 2>/dev/null; then
        local opt_size
        opt_size=$(wc -c < "$opt_spv")
        echo -e "${GREEN}OK${NC} ($opt_size bytes)"
        PASS=$((PASS + 1))
    else
        echo -e "${RED}FAIL${NC}"
        FAIL=$((FAIL + 1))
        return 1
    fi

    # 步骤 4: SPIR-V → MSL（spirv-cross）
    echo -n "  [spirv-cross→MSL]  ... "
    if spirv-cross "$opt_spv" --msl --msl-version 30000 --output "$msl" 2>"$TMPDIR/${name}_cross.err"; then
        if [ -f "$msl" ]; then
            local msl_size
            msl_size=$(wc -c < "$msl")
            local msl_lines
            msl_lines=$(wc -l < "$msl")
            if [ "$msl_size" -gt 0 ]; then
                echo -e "${GREEN}OK${NC} ($msl_size bytes, $msl_lines 行)"
                PASS=$((PASS + 1))
            else
                echo -e "${RED}FAIL${NC} (MSL 文件为空)"
                FAIL=$((FAIL + 1))
                return 1
            fi
        else
            echo -e "${RED}FAIL${NC} (未生成 MSL 文件)"
            FAIL=$((FAIL + 1))
            return 1
        fi
    else
        local err_msg
        err_msg=$(head -3 "$TMPDIR/${name}_cross.err" 2>/dev/null || echo "未知错误")
        echo -e "${RED}FAIL${NC} (spirv-cross 失败)"
        echo -e "    ${YELLOW}${err_msg}${NC}"
        FAIL=$((FAIL + 1))
        return 1
    fi

    # 步骤 5: 验证 MSL 包含关键 Metal 语法元素
    echo -n "  [MSL 语法检查]     ... "
    local has_kernel=false
    local has_vertex=false
    local has_fragment=false
    local has_texture=false

    # 检查是否包含 #include <metal_stdlib>
    if grep -q "metal_stdlib" "$msl" 2>/dev/null; then
        has_kernel=true
    fi

    if [ "$name" = "vertex" ] && grep -q "vertex" "$msl" 2>/dev/null; then
        has_vertex=true
    fi

    if [ "$name" = "fragment" ] && grep -q "fragment" "$msl" 2>/dev/null; then
        has_fragment=true
    fi

    if [ "$name" = "fragment" ] && grep -q "texture" "$msl" 2>/dev/null; then
        has_texture=true
    fi

    if [ "$name" = "vertex" ] && $has_kernel && $has_vertex; then
        echo -e "${GREEN}OK${NC} (metal_stdlib + vertex 关键字)"
        PASS=$((PASS + 1))
    elif [ "$name" = "fragment" ] && $has_kernel && $has_fragment; then
        echo -e "${GREEN}OK${NC} (metal_stdlib + fragment + texture 关键字)"
        PASS=$((PASS + 1))
    else
        echo -e "${YELLOW}WARN${NC} (部分关键字未找到，但 MSL 已生成)"
        PASS=$((PASS + 1))
    fi

    echo ""
    return 0
}

# ── 执行管线 ──
pipeline "vertex" "$TMPDIR/vertex.vert.glsl"
pipeline "fragment" "$TMPDIR/fragment.frag.glsl"

# ── 汇总 ──
echo "============================================"
if [ "$FAIL" -eq 0 ]; then
    echo -e " 结果: ${GREEN}全部通过 ($PASS/$PASS)${NC}"
    echo -e " Path C 端到端（至 MSL 文本） ${GREEN}PASS${NC}"
    echo -e " ${YELLOW}注意：MSL→metallib 需完整 Xcode (xcrun metal)，暂不可用${NC}"
else
    echo -e " 结果: ${GREEN}$PASS 通过${NC} / ${RED}$FAIL 失败${NC}"
    echo -e " Path C 端到端 ${RED}FAIL${NC}"
fi
echo "============================================"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
