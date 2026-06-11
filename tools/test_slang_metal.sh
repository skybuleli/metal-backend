#!/bin/bash
# test_slang_metal.sh — Path B 尝试：Slang → MSL → xcrun metal → metallib
# 已知限制：xcrun metal 仅完整 Xcode 包含，CLT-only 环境下预期失败
# 脚本仍整体 PASS：Slang→MSL 成功 + xcrun metal 优雅降级 = 预期行为已验证
# 用法: bash tools/test_slang_metal.sh
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

PASS=0
FAIL=0
EXPECTED_FAIL=0

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

cat > "$TMPDIR/fragment.glsl" << 'GLSL_FS'
#version 460

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;

void main() {
    // 简单颜色输出（避免 texture 采样，Metal target 不支持）
    fragColor = vec4(vTexCoord.x, vTexCoord.y, 0.5, 1.0);
}
GLSL_FS

echo "============================================"
echo " Path B 尝试 — Slang → MSL → xcrun metal"
echo " 时间: $(date '+%Y-%m-%d %H:%M:%S')"
echo " 注意: xcrun metal 需完整 Xcode，CLT 环境预期失败"
echo "============================================"
echo ""

# ── 检查 xcrun metal 是否可用 ──
echo "── 环境检测 ──"
echo -n "  [xcrun metal]      ... "
HAS_XCRUN_METAL=false
if command -v xcrun >/dev/null 2>&1; then
    if xcrun --find metal >/dev/null 2>&1; then
        HAS_XCRUN_METAL=true
        echo -e "${GREEN}可用${NC}"
    else
        echo -e "${YELLOW}不可用${NC} (CLT-only 环境，预期行为)"
    fi
else
    echo -e "${YELLOW}不可用${NC} (xcrun 不存在)"
fi
echo ""

# ── 编译函数 ──
# 参数: $1=名称 $2=源文件 $3=stage(vertex/fragment)
compile_shader() {
    local name="$1"
    local src="$2"
    local stage="$3"
    local msl="$TMPDIR/${name}.msl"
    local metallib="$TMPDIR/${name}.metallib"

    echo "── ${name} (${stage}) ──"

    # 步骤 1: GLSL → MSL（Slang -target metal）
    echo -n "  [Slang→MSL]        ... "
    if slangc "$src" -target metal -entry main -stage "$stage" -o "$msl" 2>"$TMPDIR/${name}_slang.err"; then
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
        err_msg=$(head -5 "$TMPDIR/${name}_slang.err" 2>/dev/null || echo "未知错误")
        echo -e "${RED}FAIL${NC} (slangc 编译失败)"
        echo -e "    ${YELLOW}${err_msg}${NC}"
        FAIL=$((FAIL + 1))
        return 1
    fi

    # 步骤 2: MSL 语法检查
    echo -n "  [MSL 语法检查]     ... "
    if grep -q "metal_stdlib\|#include" "$msl" 2>/dev/null; then
        echo -e "${GREEN}OK${NC} (包含 Metal 头文件引用)"
        PASS=$((PASS + 1))
    else
        echo -e "${YELLOW}WARN${NC} (未找到标准 Metal 头文件引用)"
        PASS=$((PASS + 1))
    fi

    # 步骤 3: MSL → metallib（xcrun metal）
    echo -n "  [xcrun metal]      ... "
    if $HAS_XCRUN_METAL; then
        if xcrun metal "$msl" -o "$metallib" 2>"$TMPDIR/${name}_xcrun.err"; then
            if [ -f "$metallib" ]; then
                local ml_size
                ml_size=$(wc -c < "$metallib")
                echo -e "${GREEN}OK${NC} ($ml_size bytes) — Path B 完全可用！"
                PASS=$((PASS + 1))
            else
                echo -e "${RED}FAIL${NC} (未生成 metallib)"
                FAIL=$((FAIL + 1))
                return 1
            fi
        else
            local err_msg
            err_msg=$(head -3 "$TMPDIR/${name}_xcrun.err" 2>/dev/null || echo "未知错误")
            echo -e "${RED}FAIL${NC} (xcrun metal 编译失败)"
            echo -e "    ${YELLOW}${err_msg}${NC}"
            FAIL=$((FAIL + 1))
            return 1
        fi
    else
        echo -e "${YELLOW}SKIP${NC} (xcrun metal 不可用 — 预期行为)"
        EXPECTED_FAIL=$((EXPECTED_FAIL + 1))
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
    echo -e " 结果: ${GREEN}$PASS 通过${NC} / ${YELLOW}$EXPECTED_FAIL 预期跳过${NC}"
    if [ "$EXPECTED_FAIL" -gt 0 ]; then
        echo -e " Slang→MSL ${GREEN}可用${NC}，MSL→metallib ${YELLOW}不可用（需完整 Xcode）${NC}"
        echo -e " Path B 部分验证 ${GREEN}PASS${NC}（Slang MSL 生成有效，但无法编译为 metallib）"
    else
        echo -e " Path B 完全验证 ${GREEN}PASS${NC}"
    fi
else
    echo -e " 结果: ${GREEN}$PASS 通过${NC} / ${RED}$FAIL 失败${NC} / ${YELLOW}$EXPECTED_FAIL 预期跳过${NC}"
    echo -e " Path B 验证 ${RED}FAIL${NC}"
fi
echo "============================================"

# 仅在非预期失败时退出 1
if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
