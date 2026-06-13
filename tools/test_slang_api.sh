#!/bin/bash
# test_slang_api.sh — Slang C API 独立测试（P4.2.2）
#
# 编译并运行 test_slang_api.cpp，直接 link libslang.dylib（无需 Metal 框架）。
# 用法: bash tools/test_slang_api.sh

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TEST_SRC="$PROJECT_DIR/src/libmetal_bridge/tests/test_slang_api.cpp"
TEST_BIN="/tmp/test_slang_api"

# 获取 SDK 路径（CLT-only 需要 -isysroot 才能找到标准库头文件）
SDKROOT=""
if SDKROOT=$(xcrun --sdk macosx --show-sdk-path 2>/dev/null); then
    :
else
    echo "错误：无法获取 macOS SDK 路径。"
    exit 1
fi

echo "============================================"
echo " Slang C API 独立测试（P4.2.2）"
echo " 时间: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"
echo ""

# ── 步骤 1：编译测试程序 ──
echo "── 1. 编译测试程序 ──"
echo -n "  [编译] ... "

CXX=$(xcrun --sdk macosx --find clang++ 2>/dev/null || echo "clang++")

if $CXX -std=c++17 \
    -isysroot "$SDKROOT" \
    -I"$SDKROOT/usr/include/c++/v1" \
    -I/usr/local/include/slang \
    -o "$TEST_BIN" \
    "$TEST_SRC" \
    -L/usr/local/lib -lslang \
    -Wl,-rpath,/usr/local/lib \
    2>/tmp/test_slang_api_build.log; then
    echo -e "${GREEN}OK${NC}"
else
    echo -e "${RED}FAIL${NC}"
    cat /tmp/test_slang_api_build.log
    exit 1
fi

# ── 步骤 2：运行测试 ──
echo ""
echo "── 2. 运行测试 ──"
echo ""
if "$TEST_BIN" 2>&1; then
    echo ""
    echo -e "  ${GREEN}Slang C API 独立测试通过${NC}"
else
    echo ""
    echo -e "  ${RED}Slang C API 独立测试失败${NC}"
    exit 1
fi

# ── 步骤 3（可选）：MSC 全链路验证 ──
echo ""
echo "── 3. 可选：MSC 全链路验证 ──"

if ! command -v metal-shaderconverter &>/dev/null; then
    echo -e "  ${YELLOW}SKIP${NC} (metal-shaderconverter 不可用)"
else
    TMPDIR=""
    if TMPDIR=$(mktemp -d 2>/dev/null); then
        :
    else
        TMPDIR="$SCRIPT_DIR/.tmp_test"
        mkdir -p "$TMPDIR"
    fi

    cat > "$TMPDIR/vs.slang" << 'EOF'
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

    echo -n "  [slangc->DXIL] ... "
    if slangc "$TMPDIR/vs.slang" -target dxil -entry main -stage vertex -profile sm_6_0 \
        -o "$TMPDIR/vs.dxil" 2>/dev/null && [ -s "$TMPDIR/vs.dxil" ]; then
        local dxil_size
        dxil_size=$(wc -c < "$TMPDIR/vs.dxil")
        echo -e "${GREEN}OK${NC} (${dxil_size} bytes)"

        echo -n "  [MSC->metallib] ... "
        if metal-shaderconverter "$TMPDIR/vs.dxil" -o "$TMPDIR/vs.metallib" 2>/dev/null \
            && [ -s "$TMPDIR/vs.metallib" ]; then
            local metal_size
            metal_size=$(wc -c < "$TMPDIR/vs.metallib")
            echo -e "${GREEN}OK${NC} (${metal_size} bytes)"
        else
            echo -e "${RED}FAIL${NC} (MSC 转换失败)"
        fi
    else
        echo -e "${RED}FAIL${NC} (slangc CLI DXIL 生成失败)"
    fi

    rm -rf "$TMPDIR"
fi

echo ""
echo "============================================"
echo " 测试完成。"
echo "============================================"
