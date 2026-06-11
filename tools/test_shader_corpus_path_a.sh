#!/bin/bash
# test_shader_corpus_path_a.sh — P1.8b: 扩展真实/近真实着色器语料回归测试
# 数据源: 本地 deko3d_slang_poc 语料 + 内嵌 Slang 控制样本 + GLSL 对照样本
# 目标: 扩大 P1.6~P1.8 的样本面，记录 Path A 可通过样本和待修失败模式
# 用法: bash tools/test_shader_corpus_path_a.sh
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

PASS=0
FAIL=0
ISSUE=0
SKIP=0
TOTAL=0

VERTEX_PASS=0
FRAGMENT_PASS=0
COMPUTE_PASS=0
LOCAL_ATTEMPT=0
LOCAL_ISSUE=0

TARGET_VERTEX=12
TARGET_FRAGMENT=12
TARGET_COMPUTE=5
MAX_LOCAL_ATTEMPT=48
MAX_LOCAL_ISSUE=24

CORPUS_ROOT="${SHADER_CORPUS_ROOT:-$HOME/autommes/deko3d_slang_poc/test_output}"
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
    TMPDIR="$SCRIPT_DIR/.tmp_shader_corpus"
    mkdir -p "$TMPDIR"
fi

echo "============================================"
echo " P1.8b — 扩展真实/近真实着色器语料回归测试"
echo " 时间: $(date '+%Y-%m-%d %H:%M:%S')"
echo " 语料目录: $CORPUS_ROOT"
echo "============================================"
echo ""

profile_for_stage() {
    case "$1" in
        vertex) echo "sm_6_0" ;;
        fragment) echo "ps_6_0" ;;
        compute) echo "cs_6_0" ;;
        *) echo "" ;;
    esac
}

record_pass_stage() {
    case "$1" in
        vertex) VERTEX_PASS=$((VERTEX_PASS + 1)) ;;
        fragment) FRAGMENT_PASS=$((FRAGMENT_PASS + 1)) ;;
        compute) COMPUTE_PASS=$((COMPUTE_PASS + 1)) ;;
    esac
}

stage_label() {
    case "$1" in
        vertex) echo "VS" ;;
        fragment) echo "FS" ;;
        compute) echo "CS" ;;
        *) echo "UNKNOWN" ;;
    esac
}

detect_stage() {
    local src="$1"

    if grep -Eq '\[shader\("compute"\)|\[numthreads\]' "$src" 2>/dev/null; then
        echo "compute"
    elif grep -Eq '\[shader\("pixel"\)|SV_Target' "$src" 2>/dev/null; then
        echo "fragment"
    elif grep -Eq '\[shader\("vertex"\)|SV_Position' "$src" 2>/dev/null; then
        echo "vertex"
    else
        echo "unknown"
    fi
}

# 参数: 名称、源文件、阶段、来源(control|corpus)、失败是否致命(yes|no)
run_path_a_case() {
    local name="$1"
    local src="$2"
    local stage="$3"
    local origin="$4"
    local fatal_on_fail="$5"
    local profile
    profile="$(profile_for_stage "$stage")"

    TOTAL=$((TOTAL + 1))
    echo -e "${CYAN}[$TOTAL] $name ($(stage_label "$stage"), $origin)${NC}"
    echo -e "   来源: $src"

    if [ ! -f "$src" ]; then
        echo -e "   ${YELLOW}跳过${NC} — 文件不存在"
        SKIP=$((SKIP + 1))
        echo ""
        return 0
    fi

    if [ -z "$profile" ]; then
        echo -e "   ${YELLOW}跳过${NC} — 无法识别阶段"
        SKIP=$((SKIP + 1))
        echo ""
        return 0
    fi

    local dxil="$TMPDIR/${name}.dxil"
    local ml="$TMPDIR/${name}.metallib"
    local slang_err="$TMPDIR/${name}.slang.err"
    local msc_err="$TMPDIR/${name}.msc.err"
    local src_size
    src_size=$(wc -c < "$src")
    echo -e "   源文件大小: $src_size bytes"

    echo -n "   [slangc→DXIL]     ... "
    if slangc "$src" -target dxil -entry main -stage "$stage" -profile "$profile" -o "$dxil" 2>"$slang_err"; then
        if [ -s "$dxil" ]; then
            local dxil_size
            dxil_size=$(wc -c < "$dxil")
            echo -e "${GREEN}OK${NC} ($dxil_size bytes)"
            if [ -s "$slang_err" ]; then
                local warn_msg
                warn_msg=$(head -3 "$slang_err" | tr '\n' ' ')
                echo -e "   ${YELLOW}slangc 提示: $warn_msg${NC}"
            fi
        else
            echo -e "${RED}问题${NC} (slangc 返回 0 但无 DXIL)"
            if [ "$fatal_on_fail" = "yes" ]; then
                FAIL=$((FAIL + 1))
            else
                ISSUE=$((ISSUE + 1))
                LOCAL_ISSUE=$((LOCAL_ISSUE + 1))
            fi
            echo ""
            return 0
        fi
    else
        local err_msg
        err_msg=$(head -5 "$slang_err" 2>/dev/null | tr '\n' ' ' || echo "未知错误")
        echo -e "${RED}问题${NC} (slangc)"
        echo -e "   ${YELLOW}$err_msg${NC}"
        if [ "$fatal_on_fail" = "yes" ]; then
            FAIL=$((FAIL + 1))
        else
            ISSUE=$((ISSUE + 1))
            LOCAL_ISSUE=$((LOCAL_ISSUE + 1))
        fi
        echo ""
        return 0
    fi

    echo -n "   [MSC→metallib]    ... "
    if metal-shaderconverter "$dxil" -o "$ml" 2>"$msc_err"; then
        if [ -s "$ml" ]; then
            local dxil_size ml_size
            dxil_size=$(wc -c < "$dxil")
            ml_size=$(wc -c < "$ml")
            echo -e "${GREEN}OK${NC} ($dxil_size → $ml_size bytes)"
            PASS=$((PASS + 1))
            record_pass_stage "$stage"
        else
            echo -e "${RED}问题${NC} (未生成 metallib 或 metallib 为空)"
            if [ "$fatal_on_fail" = "yes" ]; then
                FAIL=$((FAIL + 1))
            else
                ISSUE=$((ISSUE + 1))
                LOCAL_ISSUE=$((LOCAL_ISSUE + 1))
            fi
        fi
    else
        local err_msg
        err_msg=$(head -5 "$msc_err" 2>/dev/null | tr '\n' ' ' || echo "未知错误")
        echo -e "${RED}问题${NC} (MSC)"
        echo -e "   ${YELLOW}$err_msg${NC}"
        if [ "$fatal_on_fail" = "yes" ]; then
            FAIL=$((FAIL + 1))
        else
            ISSUE=$((ISSUE + 1))
            LOCAL_ISSUE=$((LOCAL_ISSUE + 1))
        fi
    fi

    echo ""
    return 0
}

check_expected_issue() {
    local name="$1"
    local src="$2"
    local stage="$3"
    local reason="$4"
    local profile
    profile="$(profile_for_stage "$stage")"

    TOTAL=$((TOTAL + 1))
    echo -e "${CYAN}[$TOTAL] $name ($(stage_label "$stage"), expected)${NC}"
    echo -e "   来源: $src"
    echo -e "   预期原因: $reason"

    local dxil="$TMPDIR/${name}.dxil"
    local slang_err="$TMPDIR/${name}.slang.err"
    echo -n "   [slangc→DXIL]     ... "
    if slangc "$src" -target dxil -entry main -stage "$stage" -profile "$profile" -o "$dxil" 2>"$slang_err"; then
        if [ -s "$dxil" ]; then
            local dxil_size
            dxil_size=$(wc -c < "$dxil")
            echo -e "${YELLOW}意外可用${NC} ($dxil_size bytes，后续可升级为正式样本)"
        else
            echo -e "${YELLOW}预期问题${NC} (无 DXIL)"
        fi
    else
        local err_msg
        err_msg=$(head -5 "$slang_err" 2>/dev/null | tr '\n' ' ' || echo "未知错误")
        echo -e "${YELLOW}预期问题${NC}"
        echo -e "   ${YELLOW}$err_msg${NC}"
    fi
    SKIP=$((SKIP + 1))
    echo ""
}

write_control_samples() {
    cat > "$TMPDIR/control_vertex.slang" <<'SLANG'
struct VertexInput {
    uint vertex_id : SV_VertexID;
};

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VertexOutput main(VertexInput input)
{
    float2 pos[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0)
    };

    VertexOutput output;
    output.position = float4(pos[input.vertex_id], 0.0, 1.0);
    output.uv = pos[input.vertex_id] * 0.5 + 0.5;
    return output;
}
SLANG

    cat > "$TMPDIR/control_fragment.slang" <<'SLANG'
Texture2D<float4> src_tex : register(t0);
SamplerState src_sampler : register(s0);

struct FragmentInput {
    float2 uv : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : SV_Target0;
};

[shader("pixel")]
FragmentOutput main(FragmentInput input)
{
    FragmentOutput output;
    output.color = src_tex.Sample(src_sampler, input.uv);
    return output;
}
SLANG

    cat > "$TMPDIR/control_compute_rwbuffer.slang" <<'SLANG'
RWStructuredBuffer<float4> out_buf : register(u0);

[shader("compute")]
[numthreads(32, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    out_buf[dispatch_id.x] = float4((float)dispatch_id.x, 0.0, 0.0, 1.0);
}
SLANG

    cat > "$TMPDIR/control_compute_groupshared.slang" <<'SLANG'
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

    cat > "$TMPDIR/control_compute_atomic.slang" <<'SLANG'
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

    cat > "$TMPDIR/control_compute_byteaddress.slang" <<'SLANG'
RWByteAddressBuffer out_bytes : register(u0);

[shader("compute")]
[numthreads(16, 1, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint offset = dispatch_id.x * 4;
    out_bytes.Store(offset, dispatch_id.x);
}
SLANG

    cat > "$TMPDIR/control_compute_rwtexture.slang" <<'SLANG'
RWTexture2D<float4> out_tex : register(u0);

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    float2 uv = float2((float)dispatch_id.x, (float)dispatch_id.y) / 64.0;
    out_tex[dispatch_id.xy] = float4(uv, 0.0, 1.0);
}
SLANG

    cat > "$TMPDIR/expected_glsl_std430.comp.glsl" <<'GLSL'
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
}

echo "══ 第一部分: 确定性控制样本 ══"
echo ""
write_control_samples
run_path_a_case "control_vertex_fullscreen" "$TMPDIR/control_vertex.slang" "vertex" "control" "yes"
run_path_a_case "control_fragment_texture" "$TMPDIR/control_fragment.slang" "fragment" "control" "yes"
run_path_a_case "control_compute_rwbuffer" "$TMPDIR/control_compute_rwbuffer.slang" "compute" "control" "yes"
run_path_a_case "control_compute_groupshared" "$TMPDIR/control_compute_groupshared.slang" "compute" "control" "yes"
run_path_a_case "control_compute_atomic" "$TMPDIR/control_compute_atomic.slang" "compute" "control" "yes"
run_path_a_case "control_compute_byteaddress" "$TMPDIR/control_compute_byteaddress.slang" "compute" "control" "yes"
run_path_a_case "control_compute_rwtexture" "$TMPDIR/control_compute_rwtexture.slang" "compute" "control" "yes"
check_expected_issue "expected_glsl_compute_std430" "$TMPDIR/expected_glsl_std430.comp.glsl" "compute" \
    "GLSL std430/storage buffer 是 Path C 对照语料；Path A 主线应使用 Slang 原生资源声明"

echo "══ 第二部分: 本地真实/近真实 Slang 语料扫描 ══"
echo ""

if [ ! -d "$CORPUS_ROOT" ]; then
    echo -e "${YELLOW}跳过${NC} — 未找到语料目录: $CORPUS_ROOT"
    SKIP=$((SKIP + 1))
else
    SEEN_FILES="$TMPDIR/seen_files.txt"
    : > "$SEEN_FILES"

    already_seen() {
        grep -Fqx "$1" "$SEEN_FILES" 2>/dev/null
    }

    mark_seen() {
        echo "$1" >> "$SEEN_FILES"
    }

    # 先抽真实/近真实样本族，避免简单 batch 样本过早吃满配额。
    for pattern in "real_*.slang" "kirby_*.slang" "large_*.slang" "final_*.slang" "smp_*.slang" "v3_*.slang" "v4_*.slang"; do
        PROBE_COUNT=0
        find "$CORPUS_ROOT" -maxdepth 1 -type f -name "$pattern" | sort > "$TMPDIR/probe_files.txt"
        while IFS= read -r file; do
            if already_seen "$file"; then
                continue
            fi

            stage="$(detect_stage "$file")"
            if [ "$stage" = "unknown" ]; then
                continue
            fi

            mark_seen "$file"
            LOCAL_ATTEMPT=$((LOCAL_ATTEMPT + 1))
            base="$(basename "$file")"
            safe_name="$(echo "$base" | sed 's/[^A-Za-z0-9_]/_/g' | sed 's/_slang$//')"
            run_path_a_case "probe_${safe_name}" "$file" "$stage" "corpus" "no"

            PROBE_COUNT=$((PROBE_COUNT + 1))
            if [ "$PROBE_COUNT" -ge 2 ] || [ "$LOCAL_ATTEMPT" -ge "$MAX_LOCAL_ATTEMPT" ]; then
                break
            fi
        done < "$TMPDIR/probe_files.txt"
    done

    : > "$TMPDIR/corpus_files.txt"
    for pattern in "deko3d*.slang" "batch*.slang" "triangle_*.slang" "texture_*.slang" "depthstencil*.slang" "comp_*.slang" "*.slang"; do
        find "$CORPUS_ROOT" -maxdepth 1 -type f -name "$pattern" | sort >> "$TMPDIR/corpus_files.txt"
    done
    while IFS= read -r file; do
        if already_seen "$file"; then
            continue
        fi

        base="$(basename "$file")"
        case "$base" in
            real_*|kirby_*|large_*|final_*|smp_*|v3_*|v4_*|batch*|deko3d*|triangle_*|texture_*|depthstencil*|comp_*)
                ;;
            *)
                continue
                ;;
        esac

        stage="$(detect_stage "$file")"
        if [ "$stage" = "unknown" ]; then
            continue
        fi

        if [ "$stage" = "vertex" ] && [ "$VERTEX_PASS" -ge "$TARGET_VERTEX" ]; then
            continue
        fi
        if [ "$stage" = "fragment" ] && [ "$FRAGMENT_PASS" -ge "$TARGET_FRAGMENT" ]; then
            continue
        fi
        if [ "$stage" = "compute" ] && [ "$COMPUTE_PASS" -ge "$TARGET_COMPUTE" ]; then
            continue
        fi
        if [ "$LOCAL_ATTEMPT" -ge "$MAX_LOCAL_ATTEMPT" ]; then
            break
        fi
        if [ "$LOCAL_ISSUE" -ge "$MAX_LOCAL_ISSUE" ]; then
            echo -e "${YELLOW}已达到本轮问题记录上限 $MAX_LOCAL_ISSUE，停止扫描以控制日志长度${NC}"
            break
        fi

        mark_seen "$file"
        LOCAL_ATTEMPT=$((LOCAL_ATTEMPT + 1))
        safe_name="$(echo "$base" | sed 's/[^A-Za-z0-9_]/_/g' | sed 's/_slang$//')"
        run_path_a_case "corpus_${safe_name}" "$file" "$stage" "corpus" "no"

        if [ "$VERTEX_PASS" -ge "$TARGET_VERTEX" ] \
            && [ "$FRAGMENT_PASS" -ge "$TARGET_FRAGMENT" ] \
            && [ "$COMPUTE_PASS" -ge "$TARGET_COMPUTE" ]; then
            break
        fi
    done < "$TMPDIR/corpus_files.txt"
fi

echo "============================================"
echo -e " 结果: ${GREEN}$PASS Path A 通过${NC} / ${RED}$FAIL 控制样本失败${NC} / ${YELLOW}$ISSUE 语料发现问题${NC} / $SKIP 跳过或预期问题 / 总计 $TOTAL"
echo " 阶段通过数: VS=$VERTEX_PASS / FS=$FRAGMENT_PASS / CS=$COMPUTE_PASS"
echo " 本地语料尝试: ${LOCAL_ATTEMPT}，记录问题: ${LOCAL_ISSUE}"
echo ""

if [ "$FAIL" -gt 0 ]; then
    echo -e " ${RED}控制样本失败，Path A 基础能力不稳定${NC}"
    exit 1
fi

if [ "$PASS" -lt 20 ]; then
    echo -e " ${RED}通过样本不足：$PASS < 20${NC}"
    exit 1
fi

if [ "$VERTEX_PASS" -lt 8 ] || [ "$FRAGMENT_PASS" -lt 8 ] || [ "$COMPUTE_PASS" -lt 5 ]; then
    echo -e " ${RED}阶段覆盖不足：至少需要 VS≥8 / FS≥8 / CS≥5${NC}"
    exit 1
fi

echo -e " ${GREEN}扩展语料回归完成${NC}：Path A 基础能力通过，真实/近真实失败模式已记录为后续修复输入"
echo "============================================"
