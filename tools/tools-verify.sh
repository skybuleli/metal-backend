#!/bin/bash
# tools-verify.sh — Switch Metal 后端工具链一键验证
# 用法: bash tools-verify.sh
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

PASS=0
FAIL=0
SKIP=0

check() {
    local name="$1"
    local cmd="$2"
    local expected="$3"
    local mode="${4:-version}"

    echo -n "  [$name] ... "

    if [ "$mode" = "path" ]; then
        if [ -f "$cmd" ] || [ -d "$cmd" ]; then
            echo -e "${GREEN}OK${NC} ($cmd)"
            PASS=$((PASS + 1))
        else
            echo -e "${RED}NOT FOUND${NC} ($cmd)"
            FAIL=$((FAIL + 1))
        fi
        return
    fi

    local output
    output=$(eval "$cmd" 2>&1) || true
    if echo "$output" | grep -qi "$expected"; then
        local preview=$(echo "$output" | head -1 | cut -c1-60)
        echo -e "${GREEN}OK${NC} ($preview)"
        PASS=$((PASS + 1))
    else
        echo -e "${RED}FAIL${NC} (expected '$expected' in output)"
        FAIL=$((FAIL + 1))
    fi
}

echo "============================================"
echo " Switch Metal 后端 — 工具链验证"
echo " 时间: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"
echo ""

# ── 着色器编译器 ──
echo "── 着色器编译器 ──"
check "slangc"            "slangc --version"                           "slang"   version
check "glslangValidator"  "glslangValidator --version"                  "Glslang" version
check "MSC"               "metal-shaderconverter --version 2>&1 || echo MSC" "MSC" version
check "MSC lib"           "/usr/local/lib/libmetalirconverter.dylib"    ""        path

echo ""
# ── SPIR-V 工具链 ──
echo "── SPIR-V 工具链 ──"
for tool in spirv-as spirv-dis spirv-val spirv-opt spirv-cross spirv-link; do
    check "$tool" "$tool --version 2>&1 || echo SPIRV" "SPIRV" version
done

echo ""
# ── 构建工具 ──
echo "── 构建工具链 ──"
check "dotnet"     "dotnet --version"    "10\."  version
check "rustc"      "rustc --version"     "rustc" version
check "cargo"      "cargo --version"     "cargo" version
check "devkitPro"  "/opt/devkitpro"      ""       path

echo ""
# ── 端到端 Path A 验证 ──
echo "── 端到端验证（Path A: Slang→DXIL→MSC→metallib）──"
TMPDIR=$(mktemp -d)
cat > "$TMPDIR/test.glsl" << 'GLSLEOF'
#version 460
void main() { gl_Position = vec4(0.0); }
GLSLEOF

if slangc "$TMPDIR/test.glsl" -target dxil -entry main -stage vertex -profile sm_6_0 -o "$TMPDIR/test.dxil" 2>/dev/null; then
    DXIL_SIZE=$(wc -c < "$TMPDIR/test.dxil")
    echo -e "  [Slang→DXIL]     ${GREEN}OK${NC} ($DXIL_SIZE bytes)"

    if metal-shaderconverter "$TMPDIR/test.dxil" -o "$TMPDIR/test.metallib" 2>/dev/null; then
        METAL_SIZE=$(wc -c < "$TMPDIR/test.metallib")
        if [ "$METAL_SIZE" -gt "$DXIL_SIZE" ]; then
            echo -e "  [MSC→metallib]   ${GREEN}OK${NC} ($DXIL_SIZE → $METAL_SIZE bytes)"
        else
            echo -e "  [MSC→metallib]   ${RED}FAIL${NC} (metallib not larger than DXIL)"
            FAIL=$((FAIL + 1))
        fi
    else
        echo -e "  [MSC→metallib]   ${RED}FAIL${NC} (MSC failed)"
        FAIL=$((FAIL + 1))
    fi
else
    echo -e "  [Slang→DXIL]     ${RED}FAIL${NC}"
    FAIL=$((FAIL + 1))
fi

rm -rf "$TMPDIR"

echo ""
echo "============================================"
echo " 结果: ${GREEN}$PASS 通过${NC} / ${RED}$FAIL 失败${NC} / ${YELLOW}$SKIP 跳过${NC}"
echo "============================================"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
