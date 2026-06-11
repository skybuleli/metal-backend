#!/bin/bash
# test_glslang_spirv.sh — Path C SPIR-V 验证：GLSL → SPIR-V → 验证 → 优化
# 验证 glslangValidator + SPIRV-Tools 的 SPIR-V 管线
# 用法: bash tools/test_glslang_spirv.sh
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

# 顶点着色器：位置 + 纹理坐标传递
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

# 片段着色器：纹理采样 + 简单亮度调节
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
echo " Path C SPIR-V 验证 — glslangValidator + SPIRV-Tools"
echo " 时间: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"
echo ""

# ── 编译函数 ──
# 编译单个着色器：GLSL → SPIR-V → 验证 → 优化
# 参数: $1=名称 $2=源文件 $3=stage(vert/frag)
compile_shader() {
    local name="$1"
    local src="$2"
    local stage="$3"
    local spv="$TMPDIR/${name}.spv"
    local opt_spv="$TMPDIR/${name}_opt.spv"

    echo "── ${name} (${stage}) ──"

    # 步骤 1: GLSL → SPIR-V（glslangValidator）
    echo -n "  [glslangValidator] ... "
    if glslangValidator -V "$src" -o "$spv" 2>"$TMPDIR/${name}_glslang.err"; then
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
        err_msg=$(head -5 "$TMPDIR/${name}_glslang.err" 2>/dev/null || echo "未知错误")
        echo -e "${RED}FAIL${NC} (glslangValidator 编译失败)"
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

    # 步骤 3: SPIR-V 优化（spirv-opt -O）
    echo -n "  [spirv-opt]        ... "
    if spirv-opt -O "$spv" -o "$opt_spv" 2>"$TMPDIR/${name}_opt.err"; then
        if [ -f "$opt_spv" ]; then
            local opt_size
            opt_size=$(wc -c < "$opt_spv")
            local spv_size
            spv_size=$(wc -c < "$spv")
            echo -e "${GREEN}OK${NC} ($spv_size → $opt_size bytes)"
            PASS=$((PASS + 1))
        else
            echo -e "${RED}FAIL${NC} (未生成优化后的 SPIR-V)"
            FAIL=$((FAIL + 1))
            return 1
        fi
    else
        local err_msg
        err_msg=$(head -3 "$TMPDIR/${name}_opt.err" 2>/dev/null || echo "未知错误")
        echo -e "${RED}FAIL${NC} (spirv-opt 优化失败)"
        echo -e "    ${YELLOW}${err_msg}${NC}"
        FAIL=$((FAIL + 1))
        return 1
    fi

    # 步骤 4: 验证优化后的 SPIR-V
    echo -n "  [spirv-val opt]    ... "
    if spirv-val "$opt_spv" 2>"$TMPDIR/${name}_val_opt.err"; then
        echo -e "${GREEN}OK${NC} (优化后验证通过)"
        PASS=$((PASS + 1))
    else
        local err_msg
        err_msg=$(head -3 "$TMPDIR/${name}_val_opt.err" 2>/dev/null || echo "未知错误")
        echo -e "${RED}FAIL${NC} (优化后 SPIR-V 验证失败)"
        echo -e "    ${YELLOW}${err_msg}${NC}"
        FAIL=$((FAIL + 1))
        return 1
    fi

    echo ""
    return 0
}

# ── 执行编译 ──
compile_shader "vertex" "$TMPDIR/vertex.vert.glsl" "vert"
compile_shader "fragment" "$TMPDIR/fragment.frag.glsl" "frag"

# ── 汇总 ──
echo "============================================"
if [ "$FAIL" -eq 0 ]; then
    echo -e " 结果: ${GREEN}全部通过 ($PASS/$PASS)${NC}"
    echo -e " Path C SPIR-V 验证 ${GREEN}PASS${NC}"
else
    echo -e " 结果: ${GREEN}$PASS 通过${NC} / ${RED}$FAIL 失败${NC}"
    echo -e " Path C SPIR-V 验证 ${RED}FAIL${NC}"
fi
echo "============================================"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
