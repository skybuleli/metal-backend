#!/usr/bin/env bash
set -euo pipefail

# test_shader_diagnostic_chain.sh — P4.6.4 着色器诊断证据链生成器
#
# 目标：
#   1. 为每个样本保留源码、Slang 输出、DXIL 摘要、metallib 产物与反射文件
#   2. 统一 failure tags，方便后续排查和进度记账
#   3. 生成可长期引用的 JSON 索引

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TEMPLATE_DIR="$REPO_ROOT/src/shader_templates/slang_graphics"
EVIDENCE_ROOT="$REPO_ROOT/docs/evidence/P4.6.4-diagnostics"
LOG_PATH="$REPO_ROOT/docs/evidence/P4.6.4-diagnostic-chain.log"
META_PATH="$REPO_ROOT/docs/evidence/P4.6.4-meta.json"
SLANGC="${SLANGC:-slangc}"
MSC="${MSC:-metal-shaderconverter}"

mkdir -p "$EVIDENCE_ROOT"
exec >"$LOG_PATH" 2>&1

python3 - "$EVIDENCE_ROOT" <<'PY'
import hashlib
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])

def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def summarize_binary(path: pathlib.Path) -> dict:
    data = path.read_bytes()
    return {
        "path": str(path.relative_to(root.parent.parent)),
        "bytes": len(data),
        "sha256": sha256(data),
        "magic_hex": data[:16].hex(),
    }

print("诊断目录:", root)
PY

run_case() {
    local name="$1"
    local shader="$2"
    local entry="$3"
    local stage="$4"
    local profile="$5"

    local case_dir="$EVIDENCE_ROOT/$name"
    mkdir -p "$case_dir"

    local source_dump="$case_dir/source_dump.slang"
    local slang_log="$case_dir/slang_output.log"
    local dxil_file="$case_dir/dxil.bin"
    local dxil_summary="$case_dir/dxil.summary.json"
    local metallib_file="$case_dir/metallib.bin"
    local metallib_summary="$case_dir/metallib.summary.json"
    local reflect_file="$case_dir/reflection.json"
    local diagnosis_file="$case_dir/diagnosis.json"
    local slang_status=0
    local msc_status=0

    cp "$shader" "$source_dump"

    {
        echo "case=$name"
        echo "shader=$shader"
        echo "entry=$entry"
        echo "stage=$stage"
        echo "profile=$profile"
        echo "command=$SLANGC $shader -target dxil -entry $entry -stage $stage -profile $profile -o $dxil_file"
        "$SLANGC" "$shader" -target dxil -entry "$entry" -stage "$stage" -profile "$profile" -o "$dxil_file" 2>&1
    } >"$slang_log" || slang_status=$?

    local failure_tags=()
    if [ "$slang_status" -ne 0 ] || [ ! -s "$dxil_file" ]; then
        failure_tags+=("slang_failed")
        if [ ! -s "$dxil_file" ]; then
            failure_tags+=("empty_dxil")
        fi
    else
        failure_tags+=("slang_ok" "dxil_ok")
    fi

    if [ "${#failure_tags[@]}" -eq 0 ] || [[ " ${failure_tags[*]} " != *" slang_failed "* ]]; then
        if "$MSC" "$dxil_file" -o "$metallib_file" --output-reflection-file "$reflect_file" >"$case_dir/msc_output.log" 2>&1; then
            if [ -s "$metallib_file" ]; then
                failure_tags+=("msc_ok" "metallib_ok")
            else
                failure_tags+=("msc_failed" "empty_metallib")
                msc_status=1
            fi
        else
            msc_status=$?
            failure_tags+=("msc_failed")
        fi
    fi

    if [ ! -s "$reflect_file" ]; then
        touch "$reflect_file"
    fi

    python3 - "$source_dump" "$slang_log" "$dxil_file" "$dxil_summary" "$metallib_file" "$metallib_summary" "$reflect_file" "$diagnosis_file" "${failure_tags[*]}" "$slang_status" "$msc_status" <<'PY'
import hashlib
import json
import pathlib
import sys

source_dump = pathlib.Path(sys.argv[1])
slang_log = pathlib.Path(sys.argv[2])
dxil_file = pathlib.Path(sys.argv[3])
dxil_summary = pathlib.Path(sys.argv[4])
metallib_file = pathlib.Path(sys.argv[5])
metallib_summary = pathlib.Path(sys.argv[6])
reflect_file = pathlib.Path(sys.argv[7])
diagnosis_file = pathlib.Path(sys.argv[8])
failure_tags = [tag for tag in sys.argv[9].split() if tag]
slang_status = int(sys.argv[10])
msc_status = int(sys.argv[11])

def sha256(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else ""

def summarize(path: pathlib.Path) -> dict:
    if not path.exists():
        return {"path": str(path), "exists": False}
    data = path.read_bytes()
    return {
        "path": str(path),
        "exists": True,
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "magic_hex": data[:16].hex(),
    }

dxil_info = summarize(dxil_file)
metallib_info = summarize(metallib_file)

dxil_summary.write_text(json.dumps(dxil_info, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
metallib_summary.write_text(json.dumps(metallib_info, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

diagnosis = {
    "source_dump": str(source_dump),
    "slang_output": str(slang_log),
    "dxil": dxil_info,
    "metallib": metallib_info,
    "reflection": str(reflect_file),
    "failure_tags": failure_tags,
    "status": {
        "slang_exit": slang_status,
        "msc_exit": msc_status,
    },
}

if not source_dump.exists():
    diagnosis["failure_tags"].append("missing_input")

diagnosis_file.write_text(json.dumps(diagnosis, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
PY

    echo "case=$name"
    echo "  source_dump=$source_dump"
    echo "  slang_log=$slang_log"
    echo "  dxil=$dxil_file"
    echo "  metallib=$metallib_file"
    echo "  reflection=$reflect_file"
    echo "  diagnosis=$diagnosis_file"
}

run_case "fullscreen_vertex" "$TEMPLATE_DIR/fullscreen_quad.slang" "vertexMain" "vertex" "sm_6_0"
run_case "fullscreen_fragment" "$TEMPLATE_DIR/fullscreen_quad.slang" "fragmentMain" "fragment" "ps_6_0"
run_case "sprite_alpha_vertex" "$TEMPLATE_DIR/sprite_alpha.slang" "vertexMain" "vertex" "sm_6_0"
run_case "sprite_alpha_fragment" "$TEMPLATE_DIR/sprite_alpha.slang" "fragmentMain" "fragment" "ps_6_0"
run_case "tilemap_vertex" "$TEMPLATE_DIR/tilemap_camera.slang" "vertexMain" "vertex" "sm_6_0"
run_case "tilemap_fragment" "$TEMPLATE_DIR/tilemap_camera.slang" "fragmentMain" "fragment" "ps_6_0"

python3 - "$EVIDENCE_ROOT" <<'PY'
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
cases = sorted([p for p in root.iterdir() if p.is_dir()], key=lambda p: p.name)
index = []

for case in cases:
    diag = json.loads((case / "diagnosis.json").read_text(encoding="utf-8"))
    index.append({
        "case": case.name,
        "source_dump": diag["source_dump"],
        "slang_output": diag["slang_output"],
        "dxil": diag["dxil"],
        "metallib": diag["metallib"],
        "reflection": diag["reflection"],
        "failure_tags": diag["failure_tags"],
    })

(root / "index.json").write_text(json.dumps(index, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(json.dumps(index, ensure_ascii=False, indent=2))
PY

cp "$EVIDENCE_ROOT/index.json" "$META_PATH"
