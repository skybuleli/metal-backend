#!/usr/bin/env bash
set -euo pipefail

# test_slang_graphics_templates.sh — P4.6.2 Slang 原生图形模板验证
#
# 验证目标：
#   1. 全部模板可走 Path A：Slang → DXIL → MSC → metallib
#   2. 关键模板反射结果符合预期：CBV / SRV / Sampler / varying
#   3. 产出可归档的日志与元数据，供 PROGRESS.md 记账

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TEMPLATE_DIR="$REPO_ROOT/src/shader_templates/slang_graphics"
EVIDENCE_DIR="$REPO_ROOT/docs/evidence"
LOG_PATH="$EVIDENCE_DIR/P4.6.2-slang-graphics-templates.log"
META_PATH="$EVIDENCE_DIR/P4.6.2-meta.json"
TMPDIR="$(mktemp -d "${TMPDIR:-/tmp}/p4_6_2_slang_templates_XXXXXX")"
trap 'rm -rf "$TMPDIR"' EXIT

SLANGC="${SLANGC:-slangc}"
MSC="${MSC:-metal-shaderconverter}"

mkdir -p "$EVIDENCE_DIR"

run_case() {
    local name="$1"
    local shader="$2"
    local entry="$3"
    local stage="$4"
    local profile="$5"

    local dxil="$TMPDIR/${name}.dxil"
    local metallib="$TMPDIR/${name}.metallib"
    local reflect="$TMPDIR/${name}.reflect.json"
    local slang_err="$TMPDIR/${name}.slang.err"
    local msc_err="$TMPDIR/${name}.msc.err"

    echo "== $name =="
    echo "shader: $shader"
    echo "entry: $entry"
    echo "stage: $stage"
    echo "profile: $profile"

    "$SLANGC" "$shader" -target dxil -entry "$entry" -stage "$stage" -profile "$profile" -o "$dxil" 2>"$slang_err"
    if [ ! -s "$dxil" ]; then
        echo "DXIL 为空" >&2
        return 1
    fi

    "$MSC" "$dxil" -o "$metallib" --output-reflection-file "$reflect" 2>"$msc_err"
    if [ ! -s "$metallib" ]; then
        echo "metallib 为空" >&2
        return 1
    fi

    local dxil_size
    local metallib_size
    dxil_size="$(wc -c < "$dxil" | tr -d ' ')"
    metallib_size="$(wc -c < "$metallib" | tr -d ' ')"
    echo "dxil_bytes: $dxil_size"
    echo "metallib_bytes: $metallib_size"
    echo "reflection: $reflect"
    echo
}

{
    echo "P4.6.2 Slang 原生图形模板验证"
    echo "模板目录: $TEMPLATE_DIR"
    echo

    run_case "fullscreen_vertex" "$TEMPLATE_DIR/fullscreen_quad.slang" "vertexMain" "vertex" "sm_6_0"
    run_case "fullscreen_fragment" "$TEMPLATE_DIR/fullscreen_quad.slang" "fragmentMain" "fragment" "ps_6_0"
    run_case "sprite_alpha_vertex" "$TEMPLATE_DIR/sprite_alpha.slang" "vertexMain" "vertex" "sm_6_0"
    run_case "sprite_alpha_fragment" "$TEMPLATE_DIR/sprite_alpha.slang" "fragmentMain" "fragment" "ps_6_0"
    run_case "tilemap_vertex" "$TEMPLATE_DIR/tilemap_camera.slang" "vertexMain" "vertex" "sm_6_0"
    run_case "tilemap_fragment" "$TEMPLATE_DIR/tilemap_camera.slang" "fragmentMain" "fragment" "ps_6_0"

    python3 - "$TMPDIR" <<'PY'
import json
import pathlib
import sys

tmpdir = pathlib.Path(sys.argv[1])

def load(name: str) -> dict:
    return json.loads((tmpdir / f"{name}.reflect.json").read_text(encoding="utf-8"))

checks = []

fullscreen_vertex = load("fullscreen_vertex")
fullscreen_fragment = load("fullscreen_fragment")
sprite_vertex = load("sprite_alpha_vertex")
sprite_fragment = load("sprite_alpha_fragment")
tilemap_vertex = load("tilemap_vertex")
tilemap_fragment = load("tilemap_fragment")

checks.append({
    "name": "fullscreen_vertex_varyings",
    "ok": fullscreen_vertex["state"]["vertex_outputs"][0]["name"] == "sv_position0"
          and fullscreen_vertex["state"]["vertex_outputs"][1]["name"] == "texcoord0",
    "detail": "fullscreen quad 顶点输出固定为 SV_Position + TEXCOORD0"
})
checks.append({
    "name": "fullscreen_fragment_sampling",
    "ok": [item["Type"] for item in fullscreen_fragment["TopLevelArgumentBuffer"]] == ["SRV", "Sampler"],
    "detail": "fullscreen quad 片段模板固定为 1 个纹理 + 1 个采样器"
})
checks.append({
    "name": "sprite_vertex_constant_buffer",
    "ok": [item["Type"] for item in sprite_vertex["TopLevelArgumentBuffer"]] == ["CBV"],
    "detail": "sprite alpha 顶点模板通过 ConstantBuffer<T> 提供场景数据"
})
checks.append({
    "name": "sprite_fragment_sampling",
    "ok": [item["Type"] for item in sprite_fragment["TopLevelArgumentBuffer"]] == ["SRV", "Sampler"],
    "detail": "sprite alpha 片段模板固定为采样 + alpha 输出"
})
checks.append({
    "name": "sprite_varying_color_uv",
    "ok": {item["name"] for item in sprite_vertex["state"]["vertex_outputs"]} >= {"sv_position0", "texcoord0", "color0"},
    "detail": "sprite alpha 顶点输出保留 TEXCOORD0 与 COLOR0"
})
checks.append({
    "name": "tilemap_vertex_constant_buffer",
    "ok": [item["Type"] for item in tilemap_vertex["TopLevelArgumentBuffer"]] == ["CBV"],
    "detail": "tilemap 顶点模板通过 ConstantBuffer<T> 提供 scroll/atlas 参数"
})
checks.append({
    "name": "tilemap_fragment_cbv_srv_sampler",
    "ok": [item["Type"] for item in tilemap_fragment["TopLevelArgumentBuffer"]] == ["SRV", "CBV", "Sampler"],
    "detail": "tilemap 片段模板同时依赖 atlas 纹理、场景常量缓冲和采样器"
})

summary = {
    "task": "P4.6.2",
    "template_dir": "src/shader_templates/slang_graphics",
    "checks": checks,
    "all_passed": all(item["ok"] for item in checks),
}

if not summary["all_passed"]:
    failed = [item["name"] for item in checks if not item["ok"]]
    raise SystemExit("反射校验失败: " + ", ".join(failed))

print("反射校验全部通过")
(tmpdir / "summary.json").write_text(
    json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
    encoding="utf-8",
)
PY

    echo
    echo "反射校验全部通过"
} | tee "$LOG_PATH"

cp "$TMPDIR/summary.json" "$META_PATH"
echo "已写入:"
echo "  $LOG_PATH"
echo "  $META_PATH"
