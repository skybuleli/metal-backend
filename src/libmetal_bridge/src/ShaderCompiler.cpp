// ShaderCompiler.cpp — 编译器生命周期与着色器编译实现（P4.2.1）
//
// P3.1b 阶段收口了单例策略、workaround 位掩码与默认配置。
// P4.2.1 实现具体的编译管线：通过 popen() 调用 slangc + metal-shaderconverter CLI 工具。
//
// 编译流程（Path A）：
//   Slang 原生语法 → slangc (-target dxil) → DXIL → metal-shaderconverter → metallib
//
// 后续 P4.2.2/P4.2.3 将分别替换为 Slang C API 和 MSC SDK C API 的直接调用。

#include "metal_bridge.h"
#include "metal_internal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

// ── 内部辅助函数 ──

/// 写入字符串到文件（覆盖）
static bool write_file(const std::string& path, const std::string& content)
{
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;
    size_t written = fwrite(content.data(), 1, content.size(), f);
    fclose(f);
    return written == content.size();
}

/// 读取文件全部字节
static std::vector<uint8_t> read_file(const std::string& path)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return {}; }
    std::vector<uint8_t> data(static_cast<size_t>(sz));
    size_t n = fread(data.data(), 1, data.size(), f);
    fclose(f);
    if (n != static_cast<size_t>(sz)) return {};
    return data;
}

/// 执行命令并捕获 stdout/stderr，返回退出码
/// @param cmd        命令字符串
/// @param out_output 捕获到的输出（stdout + stderr 混合）
/// @param max_output 最大输出字节数
/// @return 子进程退出码（-1 表示 popen 失败）
static int run_command(const std::string& cmd, std::string& out_output, size_t max_output = 4096)
{
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) return -1;

    char buf[256];
    out_output.clear();
    size_t total = 0;
    while (fgets(buf, sizeof(buf), fp) != nullptr)
    {
        size_t len = strlen(buf);
        if (total + len > max_output) len = max_output - total;
        out_output.append(buf, len);
        total += len;
        if (total >= max_output) break;
    }
    int rc = pclose(fp);
    return rc;
}

// ── 内部结构体 ──

struct metal_shader_compiler
{
    METAL_HANDLE_HEADER
    metal_device* device;
};

// ── 编译器生命周期 ──

metal_result metal_acquire_shader_compiler(
    metal_device* device,
    metal_shader_compiler** out_compiler)
{
    if (!device || !out_compiler)
        return METAL_RESULT_INVALID_ARGUMENT;

    metal_shader_compiler* compiler = (metal_shader_compiler*)calloc(1, sizeof(metal_shader_compiler));
    if (!compiler) return METAL_RESULT_OUT_OF_MEMORY;

    compiler->base.type = METAL_HANDLE_TYPE_SHADER_COMPILER;
    compiler->base.abi_version = METAL_BRIDGE_ABI_VERSION;
    compiler->device = device;

    *out_compiler = compiler;
    return METAL_RESULT_OK;
}

metal_result metal_get_default_shader_compiler_config(
    metal_shader_compiler_config* out_config)
{
    if (!out_config) return METAL_RESULT_INVALID_ARGUMENT;

    out_config->abi_version = METAL_BRIDGE_ABI_VERSION;
    out_config->enabled_workarounds = 0;
    out_config->disabled_workarounds = 0;
    out_config->metal_language_version = 30000; // Metal 3.0
    out_config->reserved = 0;
    return METAL_RESULT_OK;
}

metal_result metal_configure_shader_compiler(
    metal_shader_compiler* compiler,
    const metal_shader_compiler_config* config)
{
    if (!compiler || !config)
        return METAL_RESULT_INVALID_ARGUMENT;
    // P4.2.1 暂不处理 workaround 配置；后续 P4.2.4 缓存 + P4.2.5 回退会用到
    (void)compiler;
    (void)config;
    return METAL_RESULT_OK;
}

uint32_t metal_shader_compiler_get_workarounds(
    metal_shader_compiler* compiler)
{
    if (!compiler) return 0;
    (void)compiler;
    // P4.2.1 返回空；P4.2.5 回退逻辑会按需更新
    return 0;
}

// ── 编译实现（P4.2.1）──

metal_shader_compile_result metal_compile_shader(
    metal_shader_compiler* compiler,
    const char* source_code,
    const char* stage,
    const char* entry_point,
    const char* profile)
{
    metal_shader_compile_result result = {};
    result.result = METAL_RESULT_OK;
    result.metallib_data = nullptr;
    result.metallib_size = 0;

    if (!compiler || !source_code || !stage || !entry_point || !profile)
    {
        result.result = METAL_RESULT_INVALID_ARGUMENT;
        snprintf(result.error_message, sizeof(result.error_message),
                 "参数错误：收到空指针。");
        return result;
    }

    // 校验 stage
    bool valid_stage = (strcmp(stage, "vertex") == 0 ||
                        strcmp(stage, "fragment") == 0 ||
                        strcmp(stage, "compute") == 0);
    if (!valid_stage)
    {
        result.result = METAL_RESULT_INVALID_ARGUMENT;
        snprintf(result.error_message, sizeof(result.error_message),
                 "不支持的着色器阶段：%s（仅支持 vertex/fragment/compute）", stage);
        return result;
    }

    // ── 1. 创建临时目录 ──
    char tmpdir_template[] = "/tmp/metal_shader_XXXXXX";
    char* tmpdir = mkdtemp(tmpdir_template);
    if (!tmpdir)
    {
        result.result = METAL_RESULT_RUNTIME_ERROR;
        snprintf(result.error_message, sizeof(result.error_message),
                 "无法创建临时目录：%s", strerror(errno));
        return result;
    }

    std::string tmpdir_str(tmpdir);
    std::string slang_path   = tmpdir_str + "/shader.slang";
    std::string dxil_path    = tmpdir_str + "/shader.dxil";
    std::string metallib_path = tmpdir_str + "/shader.metallib";
    std::string slang_err_path = tmpdir_str + "/slang.err";
    std::string msc_err_path   = tmpdir_str + "/msc.err";

    // ── 2. 写入 Slang 源码 ──
    if (!write_file(slang_path, source_code))
    {
        result.result = METAL_RESULT_RUNTIME_ERROR;
        snprintf(result.error_message, sizeof(result.error_message),
                 "无法写入 Slang 源码到 %s", slang_path.c_str());
        // 清理临时目录
        std::string cleanup = "rm -rf " + tmpdir_str + " 2>/dev/null";
        system(cleanup.c_str());
        return result;
    }

    // ── 3. 运行 slangc（Slang → DXIL）──
    {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
            "slangc \"%s\" -target dxil -entry %s -stage %s -profile %s -o \"%s\" 2>\"%s\"",
            slang_path.c_str(), entry_point, stage, profile,
            dxil_path.c_str(), slang_err_path.c_str());

        std::string cmd_output;
        int slang_rc = run_command(cmd, cmd_output);

        // 检查 DXIL 是否生成
        auto dxil_data = read_file(dxil_path);
        if (slang_rc != 0 || dxil_data.empty())
        {
            auto err_data = read_file(slang_err_path);
            std::string err_msg(err_data.begin(), err_data.end());
            if (err_msg.empty()) err_msg = cmd_output;

            result.result = METAL_RESULT_COMPILE_FAILED;
            snprintf(result.error_message, sizeof(result.error_message),
                     "slangc 失败（exit=%d）：%.400s", slang_rc, err_msg.c_str());

            std::string cleanup = "rm -rf " + tmpdir_str + " 2>/dev/null";
            system(cleanup.c_str());
            return result;
        }
    }

    // ── 4. 运行 metal-shaderconverter（DXIL → metallib）──
    {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
            "metal-shaderconverter \"%s\" -o \"%s\" 2>\"%s\"",
            dxil_path.c_str(), metallib_path.c_str(), msc_err_path.c_str());

        std::string cmd_output;
        int msc_rc = run_command(cmd, cmd_output);

        auto metallib_data_vec = read_file(metallib_path);
        if (msc_rc != 0 || metallib_data_vec.empty())
        {
            auto err_data = read_file(msc_err_path);
            std::string err_msg(err_data.begin(), err_data.end());
            if (err_msg.empty()) err_msg = cmd_output;

            result.result = METAL_RESULT_COMPILE_FAILED;
            snprintf(result.error_message, sizeof(result.error_message),
                     "MSC 失败（exit=%d）：%.400s", msc_rc, err_msg.c_str());

            std::string cleanup = "rm -rf " + tmpdir_str + " 2>/dev/null";
            system(cleanup.c_str());
            return result;
        }

        // ── 5. 分配内存并拷贝 metallib 数据 ──
        result.metallib_size = metallib_data_vec.size();
        result.metallib_data = malloc(result.metallib_size);
        if (!result.metallib_data)
        {
            result.result = METAL_RESULT_OUT_OF_MEMORY;
            snprintf(result.error_message, sizeof(result.error_message),
                     "无法分配 %llu 字节 metallib 数据",
                     (unsigned long long)result.metallib_size);

            std::string cleanup = "rm -rf " + tmpdir_str + " 2>/dev/null";
            system(cleanup.c_str());
            return result;
        }
        memcpy(result.metallib_data, metallib_data_vec.data(), result.metallib_size);
    }

    // ── 6. 清理临时文件（保留可调试，后续可配置）──
    {
        std::string cleanup = "rm -rf " + tmpdir_str + " 2>/dev/null";
        system(cleanup.c_str());
    }

    result.result = METAL_RESULT_OK;
    return result;
}

/// 释放 metal_compile_shader 返回的 metallib 数据
void metal_free_shader_data(void* data)
{
    if (data)
    {
        free(data);
    }
}
