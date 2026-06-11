#!/bin/bash
# test_real_fs_path_a.sh — P1.7: 真实游戏片段着色器 Path A 测试
# 数据源: deko3d 片段 GLSL + Slang 原生片段样本 + Ryujinx Fragment MSL
# 目标: 验证片段阶段 Path A 管线不崩，非全特性覆盖
# 用法: bash tools/test_real_fs_path_a.sh
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
echo " P1.7 — 真实片段着色器 Path A 测试"
echo " 时间: $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================"
echo ""

# ── Path A 管线函数 ──
# 参数: $1=名称 $2=源文件路径 $3=来源类型(glsl|slang)
run_path_a() {
    local name="$1"
    local src="$2"
    local kind="${3:-glsl}"
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

    # 片段阶段必须使用 ps_6_0。部分简单片段用 sm_6_0 时 slangc 返回 0 但不产出 DXIL。
    echo -n "   [slangc→DXIL]     ... "
    if slangc "$src" -target dxil -entry main -stage fragment -profile ps_6_0 -o "$dxil" 2>"$TMPDIR/${name}_slang.err"; then
        if [ -s "$dxil" ]; then
            local dxil_size
            dxil_size=$(wc -c < "$dxil")
            echo -e "${GREEN}OK${NC} ($dxil_size bytes)"
            if [ -s "$TMPDIR/${name}_slang.err" ]; then
                local warn_msg
                warn_msg=$(head -1 "$TMPDIR/${name}_slang.err" | tr '\n' ' ')
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

# ── MSL 文件检测函数（仅报告，不跑 Path A）──
# 参数: $1=名称 $2=MSL 文件路径
check_fragment_msl() {
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

    local has_metal_stdlib=false
    local has_fragment=false
    if grep -q "metal_stdlib" "$src" 2>/dev/null; then
        has_metal_stdlib=true
    fi
    if grep -q "fragment" "$src" 2>/dev/null; then
        has_fragment=true
    fi

    if $has_metal_stdlib && $has_fragment; then
        echo -e "   大小: $src_size bytes ($src_lines 行)"
        echo -e "   ${GREEN}MSL 有效${NC} (含 metal_stdlib + fragment)"
        echo -e "   ${YELLOW}跳过${NC} — MSL→metallib 需 xcrun metal (完整 Xcode)"
        SKIP=$((SKIP + 1))
    else
        echo -e "   ${YELLOW}非标准 Fragment MSL${NC} (缺少 metal_stdlib 或 fragment 关键字)"
        SKIP=$((SKIP + 1))
    fi
    echo ""
    return 0
}

echo "══ 第一部分: deko3d GLSL 片段样本 → Path A ══"
echo ""

run_path_a "deko3d_runtime_triangle_fs" \
    "$HOME/autommes/deko3d_metal_runtime/shaders/triangle_fs.glsl" "glsl" || true

run_path_a "deko3d_slang_triangle_fs" \
    "$HOME/autommes/deko3d_slang_poc/shaders/triangle.frag.glsl" "glsl" || true

run_path_a "deko3d_roundtrip_fs" \
    "$HOME/autommes/deko3d_slang_poc/output/roundtrip_frag.glsl" "glsl" || true

echo "══ 第二部分: Slang 原生真实片段样本 → Path A ══"
echo ""

run_path_a "slang_real_clear_float_fs" \
    "$HOME/autommes/deko3d_slang_poc/test_output/real_ColorClearFFragmentShaderSource.frag.slang" "slang" || true

run_path_a "slang_real_clear_uint_fs" \
    "$HOME/autommes/deko3d_slang_poc/test_output/real_ColorClearUIFragmentShaderSource.frag.slang" "slang" || true

run_path_a "slang_real_texture_fs" \
    "$HOME/autommes/deko3d_slang_poc/test_output/real_deko3d_texture_fsh.glsl.slang" "slang" || true

run_path_a "slang_real_lighting_fs" \
    "$HOME/autommes/deko3d_slang_poc/test_output/real_deko3d_basic_lighting_fsh.glsl.slang" "slang" || true

echo "══ 第三部分: 内嵌片段特性覆盖 → Path A ══"
echo ""

cat > "$TMPDIR/fs_texture_branch.glsl" << 'GLSL'
#version 460
layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 outColor;
layout(binding = 0) uniform sampler2D uTexture;

void main() {
    vec4 texColor = texture(uTexture, vUV);
    float lum = dot(texColor.rgb, vec3(0.299, 0.587, 0.114));
    vec3 mixedColor = mix(texColor.rgb, vColor.rgb, clamp(lum, 0.0, 1.0));
    outColor = vec4(mixedColor, texColor.a * vColor.a);
}
GLSL
run_path_a "feat_texture_branch" "$TMPDIR/fs_texture_branch.glsl" "glsl" || true

cat > "$TMPDIR/fs_discard.glsl" << 'GLSL'
#version 460
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

void main() {
    float mask = vUV.x + vUV.y;
    if (mask < 0.25) {
        discard;
    }
    outColor = vec4(mask, 1.0 - mask, 0.5, 1.0);
}
GLSL
run_path_a "feat_discard_alpha_test" "$TMPDIR/fs_discard.glsl" "glsl" || true

cat > "$TMPDIR/fs_mrt.glsl" << 'GLSL'
#version 460
layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUV;
layout(location = 0) out vec4 outColor0;
layout(location = 1) out vec4 outColor1;
layout(location = 2) out vec4 outColor2;

void main() {
    vec3 n = normalize(vNormal);
    outColor0 = vec4(n * 0.5 + 0.5, 1.0);
    outColor1 = vec4(vUV, 0.0, 1.0);
    outColor2 = vec4(abs(n.z), abs(n.x), abs(n.y), 1.0);
}
GLSL
run_path_a "feat_multi_render_target" "$TMPDIR/fs_mrt.glsl" "glsl" || true

cat > "$TMPDIR/fs_math_loop.glsl" << 'GLSL'
#version 460
layout(location = 0) in vec3 vValue;
layout(location = 0) out vec4 outColor;

void main() {
    float sum = 0.0;
    for (int i = 0; i < 4; i++) {
        sum += sin(vValue.x + float(i)) * 0.25;
    }
    float a = sqrt(abs(vValue.y)) + pow(abs(vValue.z), 2.0);
    float b = min(max(sum + a, 0.0), 1.0);
    outColor = vec4(b, 1.0 - b, clamp(a, 0.0, 1.0), 1.0);
}
GLSL
run_path_a "feat_math_loop" "$TMPDIR/fs_math_loop.glsl" "glsl" || true

echo "══ 第四部分: Ryujinx Fragment MSL 抽样（仅检测） ══"
echo ""

MSL_DUMP="$HOME/Library/Application Support/Ryujinx/msl_dump"
for f in "$MSL_DUMP"/shader_{2,4,6,8,10}_Fragment.msl; do
    if [ -f "$f" ]; then
        name=$(basename "$f" .msl)
        check_fragment_msl "ryujinx_${name}" "$f" || true
    fi
done

SHADER_DUMP="$HOME/Library/Application Support/Ryujinx/shader_dump/fragment_0002.metal"
if [ -f "$SHADER_DUMP" ]; then
    check_fragment_msl "ryujinx_shader_dump_fragment_0002" "$SHADER_DUMP" || true
fi

echo "============================================"
echo -e " 结果: ${GREEN}$PASS Path A 通过${NC} / ${RED}$FAIL 失败${NC} / ${YELLOW}$SKIP MSL 跳过${NC} / 总计 $TOTAL"
echo ""
if [ "$FAIL" -eq 0 ]; then
    echo -e " ${GREEN}管线不崩！${NC}所有 GLSL/Slang 片段样本均通过 Path A 编译"
    echo -e " ${YELLOW}注: Ryujinx MSL 文件为已生成 MSL，CLT-only 环境下仅做格式抽检${NC}"
else
    echo -e " ${RED}$FAIL 个片段着色器编译失败${NC}"
fi
echo "============================================"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
