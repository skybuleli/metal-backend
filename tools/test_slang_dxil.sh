#!/bin/bash
# test_slang_dxil.sh — Path A 端到端验证：GLSL → DXIL → metallib
# 验证 Slang 编译器 + Metal Shader Converter 的完整管线
# 用法: bash tools/test_slang_dxil.sh
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

# 优先使用 mktemp，失败则回退到脚本同目录下的 .tmp_test
if TMPDIR=$(mktemp -d 2>/dev/null); then
    :
else
    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
    TMPDIR="$SCRIPT_DIR/.tmp_test"
    mkdir -p "$TMPDIR"
fi

# ── 内嵌着色器源码 ──

# 顶点着色器：位置 + 纹理坐标传递
# 注意：GLSL 460 UBO 语法与 DXIL SM 6.0 不完全兼容，此处使用简化写法
cat > "$TMPDIR/vertex.glsl" << 'GLSL_VS'
#version 460

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;

layout(location = 0) out vec2 vTexCoord;

void main() {
    gl_Position = vec4(aPosition, 1.0);
    vTexCoord = aTexCoord;
}
GLSL_VS

# 片段着色器：纹理采样 + 简单亮度调节
cat > "$TMPDIR/fragment.glsl" << 'GLSL_FS'
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
echo " Path A 端到端验证 — Slang→DXIL→MSC→metallib"
echo " 时间: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"
echo ""

# ── 编译函数 ──
# 编译单个着色器：GLSL → DXIL → metallib
# 参数: $1=名称 $2=源文件 $3=stage(vertex/fragment)
compile_shader() {
    local name="$1"
    local src="$2"
    local stage="$3"
    local dxil="$TMPDIR/${name}.dxil"
    local metallib="$TMPDIR/${name}.metallib"

    echo "── ${name} (${stage}) ──"

    # 步骤 1: GLSL → DXIL（Slang）
    echo -n "  [Slang→DXIL]    ... "
    if slangc "$src" -target dxil -entry main -stage "$stage" -profile sm_6_0 -o "$dxil" 2>"$TMPDIR/${name}_slang.err"; then
        if [ -f "$dxil" ]; then
            local dxil_size
            dxil_size=$(wc -c < "$dxil")
            if [ "$dxil_size" -gt 0 ]; then
                echo -e "${GREEN}OK${NC} ($dxil_size bytes)"
                PASS=$((PASS + 1))
            else
                echo -e "${RED}FAIL${NC} (DXIL 文件为空)"
                FAIL=$((FAIL + 1))
                return 1
            fi
        else
            echo -e "${RED}FAIL${NC} (未生成 DXIL 文件)"
            FAIL=$((FAIL + 1))
            return 1
        fi
    else
        local err_msg
        err_msg=$(head -3 "$TMPDIR/${name}_slang.err" 2>/dev/null || echo "未知错误")
        echo -e "${RED}FAIL${NC} (slangc 编译失败)"
        echo -e "    ${YELLOW}${err_msg}${NC}"
        FAIL=$((FAIL + 1))
        return 1
    fi

    # 步骤 2: DXIL → metallib（Metal Shader Converter）
    echo -n "  [MSC→metallib]  ... "
    if metal-shaderconverter "$dxil" -o "$metallib" 2>"$TMPDIR/${name}_msc.err"; then
        if [ -f "$metallib" ]; then
            local metal_size
            metal_size=$(wc -c < "$metallib")
            if [ "$metal_size" -gt 0 ]; then
                local dxil_size
                dxil_size=$(wc -c < "$dxil")
                if [ "$metal_size" -gt "$dxil_size" ]; then
                    echo -e "${GREEN}OK${NC} ($dxil_size → $metal_size bytes)"
                    PASS=$((PASS + 1))
                else
                    echo -e "${RED}FAIL${NC} (metallib 未大于 DXIL: $metal_size ≤ $dxil_size)"
                    FAIL=$((FAIL + 1))
                    return 1
                fi
            else
                echo -e "${RED}FAIL${NC} (metallib 文件为空)"
                FAIL=$((FAIL + 1))
                return 1
            fi
        else
            echo -e "${RED}FAIL${NC} (未生成 metallib 文件)"
            FAIL=$((FAIL + 1))
            return 1
        fi
    else
        local err_msg
        err_msg=$(head -3 "$TMPDIR/${name}_msc.err" 2>/dev/null || echo "未知错误")
        echo -e "${RED}FAIL${NC} (MSC 转换失败)"
        echo -e "    ${YELLOW}${err_msg}${NC}"
        FAIL=$((FAIL + 1))
        return 1
    fi

    echo ""
    return 0
}

# ── 执行编译 ──
compile_shader "vertex" "$TMPDIR/vertex.glsl" "vertex"
compile_shader "fragment" "$TMPDIR/fragment.glsl" "fragment"

# ── 汇总 ──
echo "============================================"
if [ "$FAIL" -eq 0 ]; then
    echo -e " 结果: ${GREEN}全部通过 ($PASS/$PASS)${NC}"
    echo -e " Path A 端到端验证 ${GREEN}PASS${NC}"
else
    echo -e " 结果: ${GREEN}$PASS 通过${NC} / ${RED}$FAIL 失败${NC}"
    echo -e " Path A 端到端验证 ${RED}FAIL${NC}"
fi
echo "============================================"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
