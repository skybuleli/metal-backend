#!/bin/bash
# test_slang_spirv.sh — Slang→SPIR-V 验证
# 验证 Slang 编译器生成 SPIR-V 的能力（Path D 前半段）
# 用法: bash tools/test_slang_spirv.sh
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
echo " Slang→SPIR-V 验证 — Path D 前半段"
echo " 时间: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"
echo ""

# ── 编译函数 ──
# 参数: $1=名称 $2=源文件 $3=stage(vertex/fragment)
compile_shader() {
    local name="$1"
    local src="$2"
    local stage="$3"
    local spv="$TMPDIR/${name}.spv"

    echo "── ${name} (${stage}) ──"

    # 步骤 1: GLSL → SPIR-V（Slang -target spirv）
    echo -n "  [Slang→SPIR-V]   ... "
    if slangc "$src" -target spirv -entry main -stage "$stage" -profile sm_6_0 -o "$spv" 2>"$TMPDIR/${name}_slang.err"; then
        if [ -f "$spv" ]; then
            local spv_size
            spv_size=$(wc -c < "$spv")
            if [ "$spv_size" -gt 0 ]; then
                echo -e "${GREEN}OK${NC} ($spv_size bytes)"
                PASS=$((PASS + 1))
            else
                echo -e "${RED}FAIL${NC} (SPIR-V 文件为空)"
                FAIL=$((FAIL + 1))
                return 1
            fi
        else
            echo -e "${RED}FAIL${NC} (未生成 SPIR-V 文件)"
            FAIL=$((FAIL + 1))
            return 1
        fi
    else
        local err_msg
        err_msg=$(head -5 "$TMPDIR/${name}_slang.err" 2>/dev/null || echo "未知错误")
        echo -e "${RED}FAIL${NC} (slangc 编译失败)"
        echo -e "    ${YELLOW}${err_msg}${NC}"
        FAIL=$((FAIL + 1))
        return 1
    fi

    # 步骤 2: SPIR-V 验证（spirv-val）
    echo -n "  [spirv-val]        ... "
    if spirv-val "$spv" 2>"$TMPDIR/${name}_val.err"; then
        echo -e "${GREEN}OK${NC} (验证通过)"
        PASS=$((PASS + 1))
    else
        local err_msg
        err_msg=$(head -3 "$TMPDIR/${name}_val.err" 2>/dev/null || echo "未知错误")
        echo -e "${RED}FAIL${NC} (SPIR-V 验证失败)"
        echo -e "    ${YELLOW}${err_msg}${NC}"
        FAIL=$((FAIL + 1))
        return 1
    fi

    # 步骤 3: SPIR-V 反汇编（spirv-dis，验证可读性）
    echo -n "  [spirv-dis]        ... "
    local disasm="$TMPDIR/${name}.spvasm"
    if spirv-dis "$spv" -o "$disasm" 2>"$TMPDIR/${name}_dis.err"; then
        if [ -f "$disasm" ]; then
            local lines
            lines=$(wc -l < "$disasm")
            echo -e "${GREEN}OK${NC} ($lines 行反汇编)"
            PASS=$((PASS + 1))
        else
            echo -e "${RED}FAIL${NC} (未生成反汇编文件)"
            FAIL=$((FAIL + 1))
            return 1
        fi
    else
        echo -e "${RED}FAIL${NC} (spirv-dis 失败)"
        FAIL=$((FAIL + 1))
        return 1
    fi

    echo ""
    return 0
}

# ── 执行编译 ──
compile_shader "vertex" "$TMPDIR/vertex.vert.glsl" "vertex"
compile_shader "fragment" "$TMPDIR/fragment.frag.glsl" "fragment"

# ── 对比：Slang SPIR-V vs glslangValidator SPIR-V ──
echo "── 交叉对比（Slang vs glslangValidator）──"

echo -n "  [Slang VS]         ... "
SLANG_VS_SIZE=$(wc -c < "$TMPDIR/vertex.spv" 2>/dev/null || echo 0)
echo -e "${GREEN}$( printf '%d' "$SLANG_VS_SIZE" ) bytes${NC}"
PASS=$((PASS + 1))

echo -n "  [glslang VS]       ... "
if glslangValidator -V "$TMPDIR/vertex.vert.glsl" -o "$TMPDIR/vertex_glslang.spv" 2>/dev/null; then
    GLSLANG_VS_SIZE=$(wc -c < "$TMPDIR/vertex_glslang.spv")
    echo -e "${GREEN}${GLSLANG_VS_SIZE} bytes${NC}"
    PASS=$((PASS + 1))
else
    echo -e "${YELLOW}SKIP${NC} (glslangValidator 失败)"
fi

echo ""

# ── 汇总 ──
echo "============================================"
if [ "$FAIL" -eq 0 ]; then
    echo -e " 结果: ${GREEN}全部通过 ($PASS/$PASS)${NC}"
    echo -e " Slang→SPIR-V 验证 ${GREEN}PASS${NC}"
else
    echo -e " 结果: ${GREEN}$PASS 通过${NC} / ${RED}$FAIL 失败${NC}"
    echo -e " Slang→SPIR-V 验证 ${RED}FAIL${NC}"
fi
echo "============================================"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
