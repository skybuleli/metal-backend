#!/bin/bash
# bench_compile.sh — 着色器编译基准测试脚本（P1.9）
# 用法: bash tools/bench_compile.sh [--json] [--csv <路径>]
#
# 三桶分组：
#   must-pass      — 必须能编译通过，失败即回归
#   known-good     — 已知可编译，失败可能是回归或统计漂移
#   known-failure  — 已知会失败，不阻断但错误类型变化需提示
#
# 输出字段（每条样本）：
#   bucket name stage path result fail_stage error_kind
#   src_bytes dxil_bytes metallib_bytes slang_ms msc_ms total_ms
#
# 产出证据：docs/evidence/bench-$(date +%Y%m%d-%H%M%S).json
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# ── 配置 ──
SLANGC="${SLANGC:-slangc}"
MSC="${MSC:-metal-shaderconverter}"
WORKDIR=""
EVIDENCE_DIR="docs/evidence"
CSV_OUT=""
JSON_FLAG=false

# ── 参数解析 ──
i=1
while [ $i -le $# ]; do
    arg="${!i}"
    case "$arg" in
        --json) JSON_FLAG=true ;;
        --csv)
            i=$((i + 1))
            CSV_OUT="${!i:-}"
            ;;
        --csv=*) CSV_OUT="${arg#*=}" ;;
    esac
    i=$((i + 1))
done

# ── 结果暂存文件 ──
RESULTS_FILE=""  # 在 main 中初始化

# ── 工具函数 ──
now_ms() {
    python3 -c 'import time; print(int(time.time()*1000))' 2>/dev/null || \
    perl -MTime::HiRes=time -e 'printf("%.0f", time()*1000)' 2>/dev/null || \
    echo $(( $(date +%s) * 1000 ))
}

file_size() {
    local f="$1"
    if [ -f "$f" ] && [ -s "$f" ]; then
        wc -c < "$f" | tr -d ' '
    else
        echo "0"
    fi
}

classify_error() {
    local stderr="$1"
    # 注意：检查顺序很重要。entry point 相关错误必须在 undefined 之前检查，
    # 因为 Slang 对缺少 main 的错误同时包含 "undefined identifier" 和
    # "no function found matching entry point name 'main'"
    if echo "$stderr" | grep -qi "syntax error\|unexpected token\|expected.*;\|expected.*)"; then
        echo "syntax_error"
    elif echo "$stderr" | grep -qi "entry point\|entrypoint\|no entry\|no function found matching entry\|missing.*main\|function.*main"; then
        echo "missing_entry"
    elif echo "$stderr" | grep -qi "undefined\|not found\|cannot find\|unknown identifier\|undeclared"; then
        echo "undefined_reference"
    elif echo "$stderr" | grep -qi "type\|mismatch\|cannot convert\|incompatible\|not a valid\|cannot be used"; then
        echo "type_error"
    elif echo "$stderr" | grep -qi "profile\|target\|unsupported"; then
        echo "profile_error"
    elif echo "$stderr" | grep -qi "internal\|ICE\|crash\|assert\|segfault"; then
        echo "compiler_crash"
    elif echo "$stderr" | grep -qi "MSC\|metal-shaderconverter\|metalirconverter"; then
        echo "msc_error"
    elif [ -n "$stderr" ]; then
        echo "unknown_error"
    else
        echo "none"
    fi
}

# ── 预期结果查询（用 case 替代关联数组，bash 3.2 兼容） ──
expected_result() {
    case "${1}|${2}" in
        "must-pass|vs-simple")      echo "pass" ;;
        "must-pass|ps-simple")      echo "pass" ;;
        "must-pass|cs-simple")      echo "pass" ;;
        "known-good|vs-multi-io")  echo "pass" ;;
        "known-good|ps-texture")    echo "pass" ;;
        "known-good|cs-buffer")     echo "pass" ;;
        "known-failure|vs-syntax-err") echo "fail" ;;
        "known-failure|ps-no-entry")   echo "fail" ;;
        "known-failure|vs-type-err")   echo "fail" ;;
        *) echo "" ;;
    esac
}

expected_error_kind() {
    case "${1}|${2}" in
        "known-failure|vs-syntax-err") echo "syntax_error" ;;
        "known-failure|ps-no-entry")   echo "missing_entry" ;;
        "known-failure|vs-type-err")   echo "type_error" ;;
        *) echo "" ;;
    esac
}

# ── 定义测试样本 ──
# 格式: "bucket|name|stage|src_name"
SAMPLES="must-pass|vs-simple|vertex|vs_simple
must-pass|ps-simple|fragment|ps_simple
must-pass|cs-simple|compute|cs_simple
known-good|vs-multi-io|vertex|vs_multi_io
known-good|ps-texture|fragment|ps_texture
known-good|cs-buffer|compute|cs_buffer
known-failure|vs-syntax-err|vertex|vs_syntax_err
known-failure|ps-no-entry|fragment|ps_no_entry
known-failure|vs-type-err|vertex|vs_type_err"

# ── 着色器源码 ──
get_shader_source() {
    local name="$1"
    case "$name" in
        # --- must-pass: 必须编译通过 ---
        vs_simple)
            cat << 'GLSLEOF'
#version 460
layout(location = 0) in vec3 aPos;
void main() {
    gl_Position = vec4(aPos, 1.0);
}
GLSLEOF
            ;;
        ps_simple)
            cat << 'GLSLEOF'
#version 460
layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = vec4(0.2, 0.5, 0.8, 1.0);
}
GLSLEOF
            ;;
        cs_simple)
            cat << 'GLSLEOF'
#version 460
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
}
GLSLEOF
            ;;
        # --- known-good: 已知可编译 ---
        vs_multi_io)
            cat << 'GLSLEOF'
#version 460
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec2 vUV;
void main() {
    gl_Position = vec4(aPos, 1.0);
    vWorldPos = aPos;
    vUV = aUV;
}
GLSLEOF
            ;;
        ps_texture)
            cat << 'GLSLEOF'
#version 460
layout(location = 0) out vec4 fragColor;
layout(location = 1) in vec2 vUV;
layout(binding = 1) uniform sampler2D tex;
void main() {
    fragColor = texture(tex, vUV);
}
GLSLEOF
            ;;
        cs_buffer)
            cat << 'GLSLEOF'
#version 460
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
layout(binding = 0) buffer InputBuf {
    float data[];
};
layout(binding = 1) buffer OutputBuf {
    float result[];
};
void main() {
    uint idx = gl_GlobalInvocationID.x;
    result[idx] = data[idx] * 2.0;
}
GLSLEOF
            ;;
        # --- known-failure: 已知会失败 ---
        vs_syntax_err)
            cat << 'GLSLEOF'
#version 460
layout(location = 0) in vec3 aPos
void main() {
    gl_Position = vec4(aPos, 1.0);
}
GLSLEOF
            ;;
        ps_no_entry)
            cat << 'GLSLEOF'
#version 460
layout(location = 0) out vec4 fragColor;
// 缺少 main 函数 — 编译必定失败
GLSLEOF
            ;;
        vs_type_err)
            cat << 'GLSLEOF'
#version 460
layout(location = 0) in vec3 aPos;
void main() {
    gl_Position = aPos; // vec4 期望，vec3 提供 — 类型不匹配
}
GLSLEOF
            ;;
        *)
            echo ""
            ;;
    esac
}

# ── 结果行格式 ──
# bucket|name|stage|path|result|fail_stage|error_kind|src_bytes|dxil_bytes|metallib_bytes|slang_ms|msc_ms|total_ms
make_result_row() {
    local bucket="$1" name="$2" stage="$3" path="$4" result="$5"
    local fail_stage="$6" error_kind="$7" src_bytes="$8" dxil_bytes="$9"
    local metallib_bytes="${10}" slang_ms="${11}" msc_ms="${12}" total_ms="${13}"
    echo "${bucket}|${name}|${stage}|${path}|${result}|${fail_stage}|${error_kind}|${src_bytes}|${dxil_bytes}|${metallib_bytes}|${slang_ms}|${msc_ms}|${total_ms}"
}

# ── 单样本编译 ──
compile_sample() {
    local bucket="$1"
    local name="$2"
    local stage="$3"
    local src_name="$4"

    local src_file="${WORKDIR}/${name}.glsl"
    local dxil_file="${WORKDIR}/${name}.dxil"
    local metal_file="${WORKDIR}/${name}.metallib"

    # 写入源码
    get_shader_source "$src_name" > "$src_file"
    local src_bytes
    src_bytes=$(file_size "$src_file")

    local result="pass"
    local fail_stage="none"
    local error_kind="none"
    local dxil_bytes=0
    local metallib_bytes=0
    local slang_ms=0
    local msc_ms=0
    local total_ms=0

    local t_start t_slang_end t_msc_end
    t_start=$(now_ms)

    # ── Stage 1: Slang → DXIL ──
    local sc_rc=0
    local sc_stderr=""
    sc_stderr=$("$SLANGC" "$src_file" \
        -target dxil \
        -entry main \
        -stage "$stage" \
        -profile sm_6_0 \
        -o "$dxil_file" 2>&1) || sc_rc=$?

    t_slang_end=$(now_ms)
    slang_ms=$(( t_slang_end - t_start ))

    dxil_bytes=$(file_size "$dxil_file")

    if [ "$sc_rc" -ne 0 ] || [ "$dxil_bytes" -eq 0 ]; then
        result="fail"
        fail_stage="slang"
        error_kind=$(classify_error "$sc_stderr")
        total_ms=$(( t_slang_end - t_start ))
        msc_ms=0
    else
        # ── Stage 2: MSC DXIL → metallib ──
        local mc_rc=0
        local mc_stderr=""
        mc_stderr=$("$MSC" "$dxil_file" -o "$metal_file" 2>&1) || mc_rc=$?

        t_msc_end=$(now_ms)
        msc_ms=$(( t_msc_end - t_slang_end ))
        total_ms=$(( t_msc_end - t_start ))

        metallib_bytes=$(file_size "$metal_file")

        if [ "$mc_rc" -ne 0 ] || [ "$metallib_bytes" -eq 0 ]; then
            result="fail"
            fail_stage="msc"
            error_kind=$(classify_error "$mc_stderr")
        fi
    fi

    # 写入结果文件
    make_result_row "$bucket" "$name" "$stage" "$src_file" \
        "$result" "$fail_stage" "$error_kind" \
        "$src_bytes" "$dxil_bytes" "$metallib_bytes" \
        "$slang_ms" "$msc_ms" "$total_ms" >> "$RESULTS_FILE"
}

# ── 判定与报警 ──
judge_result() {
    local bucket="$1" name="$2" result="$3" error_kind="$4"
    local expected
    expected=$(expected_result "$bucket" "$name")

    case "$bucket" in
        "must-pass")
            if [ "$result" = "fail" ]; then
                echo -e "  ${RED}🚨 回归！must-pass 样本失败${NC}"
                return 1
            fi
            ;;
        "known-good")
            if [ "$result" = "fail" ]; then
                if [ "$expected" = "pass" ]; then
                    echo -e "  ${YELLOW}⚠️  统计漂移：known-good 样本本次失败${NC}"
                    return 1
                fi
            fi
            ;;
        "known-failure")
            if [ "$result" = "fail" ]; then
                local exp_err
                exp_err=$(expected_error_kind "$bucket" "$name")
                if [ -n "$exp_err" ] && [ "$error_kind" != "$exp_err" ]; then
                    echo -e "  ${YELLOW}⚠️  错误类型漂移：预期 ${exp_err}，实际 ${error_kind}${NC}"
                    return 1
                fi
            elif [ "$result" = "pass" ]; then
                echo -e "  ${CYAN}ℹ️  known-failure 样本意外通过（可能已修复）${NC}"
            fi
            ;;
    esac
    return 0
}

# ── 输出单行结果 ──
print_result_row() {
    local row="$1"
    # 用临时文件解析，避免 IFS 在管道中的复杂性
    local bucket name stage path result fail_stage error_kind
    local src_bytes dxil_bytes metallib_bytes slang_ms msc_ms total_ms

    bucket=$(echo "$row" | cut -d'|' -f1)
    name=$(echo "$row" | cut -d'|' -f2)
    stage=$(echo "$row" | cut -d'|' -f3)
    path=$(echo "$row" | cut -d'|' -f4)
    result=$(echo "$row" | cut -d'|' -f5)
    fail_stage=$(echo "$row" | cut -d'|' -f6)
    error_kind=$(echo "$row" | cut -d'|' -f7)
    src_bytes=$(echo "$row" | cut -d'|' -f8)
    dxil_bytes=$(echo "$row" | cut -d'|' -f9)
    metallib_bytes=$(echo "$row" | cut -d'|' -f10)
    slang_ms=$(echo "$row" | cut -d'|' -f11)
    msc_ms=$(echo "$row" | cut -d'|' -f12)
    total_ms=$(echo "$row" | cut -d'|' -f13)

    local status_icon=""
    local status_color=""
    case "$result" in
        pass) status_icon="✅" ; status_color="$GREEN" ;;
        fail) status_icon="❌" ; status_color="$RED" ;;
    esac

    printf " ${status_color}%s${NC} %-18s %-6s %-8s %5sB → %5sB → %5sB  %4sms + %4sms = %5sms\n" \
        "$status_icon" "$name" "$stage" "$result" \
        "$src_bytes" "$dxil_bytes" "$metallib_bytes" \
        "$slang_ms" "$msc_ms" "$total_ms"

    if [ "$result" = "fail" ]; then
        echo -e "                                 失败阶段: ${YELLOW}${fail_stage}${NC}  错误类型: ${YELLOW}${error_kind}${NC}"
        judge_result "$bucket" "$name" "$result" "$error_kind" || true
    fi
}

# ── 汇总报告 ──
print_summary() {
    # 用 cut 提取各列，避免 while+pipe 的 subshell 问题
    local total pass_count fail_count

    total=$(wc -l < "$RESULTS_FILE" | tr -d ' ')
    pass_count=$(cut -d'|' -f5 "$RESULTS_FILE" | grep -c "^pass$" 2>/dev/null || true)
    fail_count=$(cut -d'|' -f5 "$RESULTS_FILE" | grep -c "^fail$" 2>/dev/null || true)

    echo ""
    echo "============================================"
    echo " 基准测试汇总"
    echo "============================================"

    # ── 总体汇总 ──
    echo ""
    echo -e "${BOLD}── 总体汇总 ──${NC}"
    echo "  总样本数: $total"
    echo -e "  通过:     ${GREEN}${pass_count}${NC}"
    echo -e "  失败:     ${RED}${fail_count}${NC}"
    local pass_rate=0
    if [ "$total" -gt 0 ]; then
        pass_rate=$(( pass_count * 100 / total ))
    fi
    echo "  通过率:   ${pass_rate}%"

    # ── 按桶汇总 ──
    echo ""
    echo -e "${BOLD}── 按桶汇总 ──${NC}"

    for b in must-pass known-good known-failure; do
        local bt=0 bp=0 bf=0

        # 过滤出该桶的行
        grep "^${b}|" "$RESULTS_FILE" > "${WORKDIR}/.bucket_${b}" 2>/dev/null || true
        bt=$(wc -l < "${WORKDIR}/.bucket_${b}" | tr -d ' ')
        bp=$(cut -d'|' -f5 "${WORKDIR}/.bucket_${b}" | grep -c "^pass$" 2>/dev/null || true)
        bf=$(cut -d'|' -f5 "${WORKDIR}/.bucket_${b}" | grep -c "^fail$" 2>/dev/null || true)

        local br=0
        [ "$bt" -gt 0 ] && br=$(( bp * 100 / bt ))

        local b_icon=""
        case "$b" in
            "must-pass")      b_icon="🔴" ;;
            "known-good")     b_icon="🟡" ;;
            "known-failure")  b_icon="🟢" ;;
        esac

        printf "  %s %-18s  总计: %2d  通过: %2d  失败: %2d  通过率: %3d%%\n" \
            "$b_icon" "$b" "$bt" "$bp" "$bf" "$br"

        # must-pass 失败语义
        if [ "$b" = "must-pass" ] && [ "$bf" -gt 0 ]; then
            echo -e "    ${RED}↑ 存在回归 — must-pass 样本编译失败！${NC}"
        fi
        # known-good 失败语义
        if [ "$b" = "known-good" ] && [ "$bf" -gt 0 ]; then
            echo -e "    ${YELLOW}↑ 统计漂移 — known-good 样本编译失败${NC}"
        fi
        # known-failure 语义
        if [ "$b" = "known-failure" ]; then
            if [ "$bf" -gt 0 ]; then
                echo -e "    ${GREEN}↑ 符合预期 — known-failure 样本正确失败${NC}"
            else
                echo -e "    ${CYAN}↑ 所有 known-failure 样本意外通过（可能已修复）${NC}"
            fi
        fi
    done

    # ── 按阶段汇总 ──
    echo ""
    echo -e "${BOLD}── 按阶段汇总 ──${NC}"
    for s in vertex fragment compute; do
        grep "|${s}|" "$RESULTS_FILE" > "${WORKDIR}/.stage_${s}" 2>/dev/null || true
        local st=0 sp=0 sf=0
        st=$(wc -l < "${WORKDIR}/.stage_${s}" | tr -d ' ')
        [ "$st" -eq 0 ] && continue
        sp=$(cut -d'|' -f5 "${WORKDIR}/.stage_${s}" | grep -c "^pass$" 2>/dev/null || true)
        sf=$(cut -d'|' -f5 "${WORKDIR}/.stage_${s}" | grep -c "^fail$" 2>/dev/null || true)
        printf "  %-10s  总计: %2d  通过: %2d  失败: %2d\n" "$s" "$st" "$sp" "$sf"
    done

    # ── 按错误类型汇总 ──
    if [ "$fail_count" -gt 0 ]; then
        echo ""
        echo -e "${BOLD}── 按错误类型汇总 ──${NC}"
        grep "|fail|" "$RESULTS_FILE" | cut -d'|' -f7 | sort | uniq -c | while read -r cnt ek; do
            printf "  %-22s  %2d 次\n" "$ek" "$cnt"
        done
    fi

    # ── 语义判定 ──
    echo ""
    echo -e "${BOLD}── 语义判定 ──${NC}"
    local regression=0 drift=0
    regression=$(grep "^must-pass|" "$RESULTS_FILE" | cut -d'|' -f5 | grep -c "^fail$" 2>/dev/null || true)
    drift=$(grep "^known-good|" "$RESULTS_FILE" | cut -d'|' -f5 | grep -c "^fail$" 2>/dev/null || true)

    if [ "$regression" -gt 0 ]; then
        echo -e "  ${RED}🚨 回归检测：${regression} 个 must-pass 样本失败${NC}"
    else
        echo -e "  ${GREEN}✅ 无回归 — 所有 must-pass 样本通过${NC}"
    fi

    if [ "$drift" -gt 0 ]; then
        echo -e "  ${YELLOW}⚠️  统计漂移：${drift} 个 known-good 样本失败${NC}"
    else
        echo -e "  ${GREEN}✅ 无统计漂移 — 所有 known-good 样本通过${NC}"
    fi

    echo ""
    echo "============================================"
}

# ── 输出 JSON 证据 ──
write_json_evidence() {
    local json_file="$1"
    local timestamp
    timestamp=$(date -u '+%Y-%m-%dT%H:%M:%SZ')

    local slangc_path msc_path
    slangc_path=$(command -v "$SLANGC" 2>/dev/null || echo "$SLANGC")
    msc_path=$(command -v "$MSC" 2>/dev/null || echo "$MSC")

    # 总体统计 (直接对结果文件操作，无 subshell 问题)
    local total pass_count fail_count pass_rate regression drift
    total=$(wc -l < "$RESULTS_FILE" | tr -d ' ')
    pass_count=$(cut -d'|' -f5 "$RESULTS_FILE" | grep -c "^pass$" 2>/dev/null || true)
    fail_count=$(cut -d'|' -f5 "$RESULTS_FILE" | grep -c "^fail$" 2>/dev/null || true)
    pass_rate=0
    [ "$total" -gt 0 ] && pass_rate=$(( pass_count * 100 / total ))
    regression=$(grep "^must-pass|" "$RESULTS_FILE" | cut -d'|' -f5 | grep -c "^fail$" 2>/dev/null || true)
    drift=$(grep "^known-good|" "$RESULTS_FILE" | cut -d'|' -f5 | grep -c "^fail$" 2>/dev/null || true)

    # 写 JSON 头部
    cat > "$json_file" << HEADEREOF
{
  "meta": {
    "task": "P1.9",
    "tool": "bench_compile.sh",
    "timestamp": "${timestamp}",
    "slangc": "${slangc_path}",
    "msc": "${msc_path}"
  },
  "summary": {
    "total": ${total},
    "pass": ${pass_count},
    "fail": ${fail_count},
    "pass_rate": ${pass_rate},
    "regression_count": ${regression},
    "drift_count": ${drift}
  },
HEADEREOF

    # 按桶统计
    echo '  "by_bucket": {' >> "$json_file"
    local first_bucket=true
    for b in must-pass known-good known-failure; do
        grep "^${b}|" "$RESULTS_FILE" > "${WORKDIR}/.json_b_${b}" 2>/dev/null || true
        local bt bp bf br
        bt=$(wc -l < "${WORKDIR}/.json_b_${b}" | tr -d ' ')
        bp=$(cut -d'|' -f5 "${WORKDIR}/.json_b_${b}" | grep -c "^pass$" 2>/dev/null || true)
        bf=$(cut -d'|' -f5 "${WORKDIR}/.json_b_${b}" | grep -c "^fail$" 2>/dev/null || true)
        br=0
        [ "$bt" -gt 0 ] && br=$(( bp * 100 / bt ))

        if [ "$first_bucket" = true ]; then first_bucket=false; else echo "    ," >> "$json_file"; fi
        printf '    "%s": {"total": %d, "pass": %d, "fail": %d, "pass_rate": %d}\n' \
            "$b" "$bt" "$bp" "$bf" "$br" >> "$json_file"
    done
    echo '  },' >> "$json_file"

    # 按阶段统计
    echo '  "by_stage": {' >> "$json_file"
    local first_stage=true
    for s in vertex fragment compute; do
        grep "|${s}|" "$RESULTS_FILE" > "${WORKDIR}/.json_s_${s}" 2>/dev/null || true
        local st sp sf
        st=$(wc -l < "${WORKDIR}/.json_s_${s}" | tr -d ' ')
        [ "$st" -eq 0 ] && continue
        sp=$(cut -d'|' -f5 "${WORKDIR}/.json_s_${s}" | grep -c "^pass$" 2>/dev/null || true)
        sf=$(cut -d'|' -f5 "${WORKDIR}/.json_s_${s}" | grep -c "^fail$" 2>/dev/null || true)

        if [ "$first_stage" = true ]; then first_stage=false; else echo "    ," >> "$json_file"; fi
        printf '    "%s": {"total": %d, "pass": %d, "fail": %d}\n' \
            "$s" "$st" "$sp" "$sf" >> "$json_file"
    done
    echo '  },' >> "$json_file"

    # 按错误类型
    echo '  "by_error_kind": {' >> "$json_file"
    if [ "$fail_count" -gt 0 ]; then
        grep "|fail|" "$RESULTS_FILE" | cut -d'|' -f7 | sort | uniq -c | \
        while read -r cnt ek; do
            [ -z "$ek" ] && continue
            echo "    \"${ek}\": ${cnt}," >> "$json_file"
        done
    fi
    # 移除最后一个逗号（如果有的话）— 在 macOS 上用 sed
    sed -i '' '$ s/,$//' "$json_file" 2>/dev/null || true
    echo '  },' >> "$json_file"

    # 样本详情
    echo '  "samples": [' >> "$json_file"
    local total_lines
    total_lines=$(wc -l < "$RESULTS_FILE" | tr -d ' ')
    local line_num=0
    while IFS='|' read -r bucket name stage path result fail_stage error_kind \
        src_bytes dxil_bytes metallib_bytes slang_ms msc_ms total_ms; do
        [ -z "$bucket" ] && continue
        line_num=$((line_num + 1))

        if [ "$line_num" -lt "$total_lines" ]; then
            echo "    {" >> "$json_file"
        else
            echo "    {" >> "$json_file"
        fi

        cat >> "$json_file" << SAMEEOF
      "bucket": "${bucket}",
      "name": "${name}",
      "stage": "${stage}",
      "path": "${path}",
      "result": "${result}",
      "fail_stage": "${fail_stage}",
      "error_kind": "${error_kind}",
      "src_bytes": ${src_bytes},
      "dxil_bytes": ${dxil_bytes},
      "metallib_bytes": ${metallib_bytes},
      "slang_ms": ${slang_ms},
      "msc_ms": ${msc_ms},
      "total_ms": ${total_ms}
SAMEEOF

        if [ "$line_num" -lt "$total_lines" ]; then
            echo "    }," >> "$json_file"
        else
            echo "    }" >> "$json_file"
        fi
    done < "$RESULTS_FILE"
    echo '  ]' >> "$json_file"
    echo '}' >> "$json_file"
}

# ════════════════════════════════════════════════════════════════
# 主流程
# ════════════════════════════════════════════════════════════════

main() {
    WORKDIR=$(mktemp -d)
    RESULTS_FILE="${WORKDIR}/results.txt"
    : > "$RESULTS_FILE"  # 初始化空文件
    trap "rm -rf $WORKDIR" EXIT

    echo "============================================"
    echo " Switch Metal 后端 — 着色器编译基准测试 (P1.9)"
    echo " 时间: $(date '+%Y-%m-%d %H:%M:%S CST')"
    echo " slangc: $(command -v "$SLANGC" 2>/dev/null || echo "$SLANGC")"
    echo " MSC:    $(command -v "$MSC" 2>/dev/null || echo "$MSC")"
    echo " 样本数: 9 (must-pass: 3, known-good: 3, known-failure: 3)"
    echo "============================================"

    # ── 编译所有样本（用 here-string 避免 subshell） ──
    local current_bucket=""
    while IFS='|' read -r bucket name stage src_name; do
        [ -z "$bucket" ] && continue

        if [ "$bucket" != "$current_bucket" ]; then
            current_bucket="$bucket"
            case "$bucket" in
                "must-pass")      echo -e "\n${BOLD}── 🔴 must-pass ──${NC}" ;;
                "known-good")     echo -e "\n${BOLD}── 🟡 known-good ──${NC}" ;;
                "known-failure")  echo -e "\n${BOLD}── 🟢 known-failure ──${NC}" ;;
            esac
        fi

        compile_sample "$bucket" "$name" "$stage" "$src_name"
    done <<< "$SAMPLES"

    # ── 逐行输出结果（用 here-string 避免 subshell） ──
    while IFS= read -r row; do
        [ -z "$row" ] && continue
        print_result_row "$row"
    done < "$RESULTS_FILE"

    # ── 汇总 ──
    print_summary

    # ── 证据输出 ──
    local ts
    ts=$(date '+%Y%m%d-%H%M%S')
    local json_file="${EVIDENCE_DIR}/bench-${ts}.json"
    mkdir -p "$EVIDENCE_DIR"
    write_json_evidence "$json_file"
    echo -e "证据日志: ${GREEN}${json_file}${NC}"

    # ── CSV 输出 ──
    if [ -n "$CSV_OUT" ]; then
        {
            echo "bucket,name,stage,path,result,fail_stage,error_kind,src_bytes,dxil_bytes,metallib_bytes,slang_ms,msc_ms,total_ms"
            cat "$RESULTS_FILE"
        } > "$CSV_OUT"
        echo -e "CSV 输出: ${GREEN}${CSV_OUT}${NC}"
    fi

    if [ "$JSON_FLAG" = true ]; then
        echo ""
        cat "$json_file"
    fi

    # ── 退出码 ──
    local has_regression
    has_regression=$(grep "^must-pass|" "$RESULTS_FILE" | cut -d'|' -f5 | grep -c "^fail$" 2>/dev/null || true)

    if [ "$has_regression" -gt 0 ]; then
        echo -e "\n${RED}退出码 1 — 存在回归${NC}"
        exit 1
    fi

    echo -e "\n${GREEN}退出码 0 — 无回归${NC}"
    exit 0
}

main "$@"
