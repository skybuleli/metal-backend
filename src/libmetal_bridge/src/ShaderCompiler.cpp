// ShaderCompiler.cpp — 编译器生命周期与着色器编译实现
//
// P3.1b   — 收口了单例策略、workaround 位掩码与默认配置
// P4.2.1  — popen() 调 slangc + MSC CLI 打通全链路
// P4.2.2  — slangc popen 替换为 Slang C API（本篇当前实现）
// P4.2.3  — MSC popen 替换为 IRCompiler SDK API（保留 popen 回退）
//
// 编译流程（Path A）：
//   Slang 原生语法 → slang C API (libslang.dylib) → DXIL byte[]
//          → IRCompiler SDK API (libmetalirconverter.dylib) → metallib byte[]
//          → 回退：popen("metal-shaderconverter") → metallib byte[]

#include "metal_bridge.h"
#include "metal_internal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <CommonCrypto/CommonCrypto.h>

// Slang C API：仅在可用时引入
#if METAL_SLANG_FOUND
#include <slang.h>
#include <slang-com-ptr.h>
#endif

// Metal IR Converter SDK：仅在可用时引入
#if METAL_IRCONVERTER_FOUND
#include <metal_irconverter/metal_irconverter.h>
#endif

// ── 内部辅助函数 ──

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

/// 执行命令并捕获输出（仅用于 MSC 步骤，P4.2.3 将替换）
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

static bool should_keep_failed_shader_temp()
{
    const char* value = getenv("SWITCH_METAL_KEEP_FAILED_SHADER_TEMP");
    return value && value[0] != '\0' && strcmp(value, "0") != 0;
}

static std::string preprocess_glsl_source_for_dxil(const char* source_code, const char* stage)
{
    bool is_vertex_stage = strcmp(stage, "vertex") == 0;
    std::istringstream input(source_code);
    std::ostringstream output;
    std::string line;

    while (std::getline(input, line))
    {
        if (line.find("#pragma optionNV(") != std::string::npos)
        {
            continue;
        }

        if (is_vertex_stage && line.find("gl_PointSize") != std::string::npos)
        {
            continue;
        }

        output << line << '\n';
    }

    return output.str();
}

/// 从 HLSL 源码中提取 varying 变量声明
/// 返回格式: "VAR_NAME:TYPE:SEMANTIC;..."
static std::string extract_hlsl_varying_signatures(const std::string& hlsl_source)
{
    std::ostringstream sigs;
    std::istringstream input(hlsl_source);
    std::string line;
    
    while (std::getline(input, line))
    {
        size_t colon_pos = line.find(" : ");
        if (colon_pos == std::string::npos) continue;
        
        std::string after_colon = line.substr(colon_pos + 3);
        size_t semicolon_pos = after_colon.find(';');
        if (semicolon_pos == std::string::npos) continue;
        std::string semantic = after_colon.substr(0, semicolon_pos);
        
        if (semantic.find("SV_") == 0) continue;
        
        std::string before_colon = line.substr(0, colon_pos);
        size_t last_space = before_colon.rfind(' ');
        if (last_space == std::string::npos) continue;
        std::string var_name = before_colon.substr(last_space + 1);
        
        size_t type_start = 0;
        while (type_start < last_space && before_colon[type_start] == ' ') type_start++;
        size_t type_end = last_space;
        while (type_end > type_start && before_colon[type_end-1] == ' ') type_end--;
        std::string type_name = before_colon.substr(type_start, type_end - type_start);
        
        if (!sigs.str().empty()) sigs << ";";
        sigs << var_name << ":" << type_name << ":" << semantic;
    }
    
    return sigs.str();
}

/// 将 HLSL 中的 varying 语义从 spirv-cross 默认分配统一为规范化格式
/// 确保同一 varying 在 VS 输出和 FS 输入中使用相同的语义
static std::string normalize_hlsl_varying_semantics(
    const std::string& hlsl_source,
    const std::string& stage)
{
    std::istringstream input(hlsl_source);
    std::ostringstream output;
    std::string line;
    
    while (std::getline(input, line))
    {
        std::string modified = line;
        
        // 替换 COLOR{N} → TEXCOORD{N}（在 HLSL varying 声明中的语义）
        size_t pos = 0;
        while ((pos = modified.find(" : COLOR", pos)) != std::string::npos)
        {
            size_t num_start = pos + 8;
            if (num_start < modified.size() && std::isdigit(modified[num_start]))
            {
                size_t num_end = num_start;
                while (num_end < modified.size() && std::isdigit(modified[num_end])) num_end++;
                std::string number = modified.substr(num_start, num_end - num_start);
                modified.replace(pos + 3, 5 + number.size(), "TEXCOORD" + number);
                pos = pos + 8 + number.size();
            }
            else
            {
                pos += 8;
            }
        }
        
        output << modified << "\n";
    }
    
    return output.str();
}

static bool compile_glsl_via_spirv_hlsl_bridge(
    const char* source_code,
    const char* stage,
    const char* profile,
    std::vector<uint8_t>& out_dxil_data,
    char* out_error_message,
    size_t out_error_message_size)
{
    char tmpdir_template[] = "/tmp/metal_shader_bridge_XXXXXX";
    char* tmpdir = mkdtemp(tmpdir_template);
    if (!tmpdir)
    {
        snprintf(out_error_message, out_error_message_size,
                 "无法创建 GLSL 桥接临时目录：%s", strerror(errno));
        return false;
    }

    std::string tmpdir_str(tmpdir);
    std::string stage_extension =
        strcmp(stage, "vertex") == 0 ? ".vert" :
        strcmp(stage, "fragment") == 0 ? ".frag" :
        strcmp(stage, "compute") == 0 ? ".comp" : ".glsl";

    std::string glsl_path = tmpdir_str + "/shader" + stage_extension;
    std::string spv_path = tmpdir_str + "/shader.spv";
    std::string hlsl_path = tmpdir_str + "/shader.hlsl";
    std::string dxil_path = tmpdir_str + "/shader.dxil";
    std::string glslang_err_path = tmpdir_str + "/glslang.err";
    std::string cross_err_path = tmpdir_str + "/spirv-cross.err";
    std::string slang_err_path = tmpdir_str + "/slang.err";

    std::string preprocessed_source = preprocess_glsl_source_for_dxil(source_code, stage);

    // ── 临时调试：保存所有 GLSL 着色器源码 ──
    {
        static int s_shader_dump_counter = 0;
        int idx = __sync_fetch_and_add(&s_shader_dump_counter, 1);
        if (idx < 200)
        {
            char dump_path[512];
            const char* home = getenv("HOME");
            snprintf(dump_path, sizeof(dump_path),
                     "%s/Library/Application Support/Ryujinx/glsl_dump", home ? home : "/tmp");
            mkdir(dump_path, 0755);
            snprintf(dump_path, sizeof(dump_path),
                     "%s/Library/Application Support/Ryujinx/glsl_dump/shader_%03d_%s.glsl",
                     home ? home : "/tmp", idx, stage);
            FILE* df = fopen(dump_path, "w");
            if (df)
            {
                fwrite(source_code, 1, strlen(source_code), df);
                fclose(df);
            }
        }
    }

    {
        FILE* f = fopen(glsl_path.c_str(), "w");
        if (!f)
        {
            snprintf(out_error_message, out_error_message_size,
                     "无法写入桥接 GLSL 源码。");
            if (!should_keep_failed_shader_temp())
            {
                std::string cleanup = "rm -rf " + tmpdir_str + " 2>/dev/null";
                system(cleanup.c_str());
            }
            return false;
        }

        fwrite(preprocessed_source.data(), 1, preprocessed_source.size(), f);
        fclose(f);
    }

    {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
            "glslangValidator -V \"%s\" -o \"%s\" 2>\"%s\"",
            glsl_path.c_str(), spv_path.c_str(), glslang_err_path.c_str());

        std::string cmd_output;
        int rc = run_command(cmd, cmd_output);
        auto err_data = read_file(glslang_err_path);
        std::string err_msg(err_data.begin(), err_data.end());
        if (err_msg.empty()) err_msg = cmd_output;

        if (rc != 0 || read_file(spv_path).empty())
        {
            snprintf(out_error_message, out_error_message_size,
                     "glslangValidator 失败（exit=%d）：%.400s", rc, err_msg.c_str());
            if (!should_keep_failed_shader_temp())
            {
                std::string cleanup = "rm -rf " + tmpdir_str + " 2>/dev/null";
                system(cleanup.c_str());
            }
            return false;
        }
    }

    // ── 步骤 2a: spirv-opt 规范化 SPIR-V（修复 VS/FS varying location 不一致）──
    std::string spv_opt_path = tmpdir_str + "/shader.opt.spv";
    {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
            "spirv-opt \"%s\" --legalize-vector-shuffle --compact-ids -o \"%s\" 2>\"%s\"",
            spv_path.c_str(), spv_opt_path.c_str(), cross_err_path.c_str());
        
        std::string cmd_output;
        int rc = run_command(cmd, cmd_output);
        // spirv-opt 失败不退避，继续使用原始 SPIR-V
        if (rc == 0)
        {
            // 检查输出文件是否有效
            auto opt_data = read_file(spv_opt_path);
            if (!opt_data.empty())
            {
                spv_path = spv_opt_path; // 使用优化后的 SPIR-V
            }
        }
    }

    // ── 步骤 2b: spirv-cross SPIR-V → HLSL ──
    {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
            "spirv-cross \"%s\" --hlsl --shader-model 60 --output \"%s\" 2>\"%s\"",
            spv_path.c_str(), hlsl_path.c_str(), cross_err_path.c_str());

        std::string cmd_output;
        int rc = run_command(cmd, cmd_output);
        auto err_data = read_file(cross_err_path);
        std::string err_msg(err_data.begin(), err_data.end());
        if (err_msg.empty()) err_msg = cmd_output;

        if (rc != 0 || read_file(hlsl_path).empty())
        {
            snprintf(out_error_message, out_error_message_size,
                     "spirv-cross 失败（exit=%d）：%.400s", rc, err_msg.c_str());
            if (!should_keep_failed_shader_temp())
            {
                std::string cleanup = "rm -rf " + tmpdir_str + " 2>/dev/null";
                system(cleanup.c_str());
            }
            return false;
        }
        
        // ── 步骤 2c: 规范化 HLSL varying 语义（修复 VS/FS COLOR↔TEXCOORD 不匹配）──
        {
            auto hlsl_data = read_file(hlsl_path);
            if (!hlsl_data.empty())
            {
                std::string hlsl_source(hlsl_data.begin(), hlsl_data.end());
                std::string normalized = normalize_hlsl_varying_semantics(hlsl_source, stage);
                if (normalized != hlsl_source)
                {
                    FILE* f = fopen(hlsl_path.c_str(), "w");
                    if (f)
                    {
                        fwrite(normalized.data(), 1, normalized.size(), f);
                        fclose(f);
                        std::fprintf(stderr, "[shader-compiler] varying 语义已规范化 (%s)\n", stage);
                    }
                }
            }
        }
    }

    {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
            "slangc \"%s\" -target dxil -entry main -stage %s -profile %s -o \"%s\" 2>\"%s\"",
            hlsl_path.c_str(), stage, profile, dxil_path.c_str(), slang_err_path.c_str());

        std::string cmd_output;
        int rc = run_command(cmd, cmd_output);
        out_dxil_data = read_file(dxil_path);
        auto err_data = read_file(slang_err_path);
        std::string err_msg(err_data.begin(), err_data.end());
        if (err_msg.empty()) err_msg = cmd_output;

        if (rc != 0 || out_dxil_data.empty())
        {
            snprintf(out_error_message, out_error_message_size,
                     "GLSL->SPIR-V->HLSL->DXIL 失败（exit=%d）：%.400s", rc, err_msg.c_str());
            if (!should_keep_failed_shader_temp())
            {
                std::string cleanup = "rm -rf " + tmpdir_str + " 2>/dev/null";
                system(cleanup.c_str());
            }
            return false;
        }
    }

    if (!should_keep_failed_shader_temp())
    {
        std::string cleanup = "rm -rf " + tmpdir_str + " 2>/dev/null";
        system(cleanup.c_str());
    }

    return true;
}

// ════════════════════════════════════════════════════════════════════
// 磁盘着色器缓存（P4.2.4）
// ════════════════════════════════════════════════════════════════════

/// 缓存目录的根路径
static const char* kCacheRoot = "Library/Caches/SwitchMetal";

/// 计算 SHA256 缓存键：source_code + source_language + entry_point + stage + profile
static std::string compute_cache_key(
    const char* source_code,
    const char* source_language,
    const char* entry_point,
    const char* stage,
    const char* profile)
{
    // 将五个输入拼接后计算 SHA256
    std::string input = std::string(source_code) + "\0" +
                        source_language + "\0" +
                        entry_point + "\0" +
                        stage + "\0" +
                        profile;

    unsigned char hash[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(input.data(), (CC_LONG)input.size(), hash);

    // 转 hex 字符串
    char hex[CC_SHA256_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < CC_SHA256_DIGEST_LENGTH; i++)
    {
        snprintf(hex + i * 2, 3, "%02x", hash[i]);
    }
    return std::string(hex);
}

/// 获取缓存目录路径：~/Library/Caches/SwitchMetal/abi_v1/
static std::string get_cache_root()
{
    const char* home = getenv("HOME");
    if (!home)
    {
        struct passwd* pw = getpwuid(getuid());
        if (pw && pw->pw_dir) home = pw->pw_dir;
    }
    if (!home) return "/tmp/SwitchMetalCache";

    return std::string(home) + "/" + kCacheRoot + "/abi_v1";
}

/// 创建目录（如果不存在）
static bool ensure_directory(const std::string& dir)
{
    struct stat st;
    if (stat(dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
        return true;
    return (mkdir(dir.c_str(), 0755) == 0);
}

/// 构造缓存条目路径：root/ab/cd1234.../
static std::string cache_entry_path(const std::string& root, const std::string& key)
{
    // key 是 64 字符的 SHA256 hex
    std::string subdir = key.substr(0, 2);
    std::string full = root + "/" + subdir + "/" + key;
    return full;
}

/// 从缓存加载 metallib 数据
/// @return true 如果缓存命中且加载成功
static bool load_from_cache(
    const std::string& root,
    const std::string& cache_key,
    std::vector<uint8_t>& out_metallib)
{
    std::string entry = cache_entry_path(root, cache_key);
    std::string metallib_path = entry + "/metallib.bin";

    struct stat st;
    if (stat(metallib_path.c_str(), &st) != 0)
        return false; // 缓存未命中

    out_metallib = read_file(metallib_path);
    return !out_metallib.empty();
}

/// 存储 metallib 数据到缓存
static void store_to_cache(
    const std::string& root,
    const std::string& cache_key,
    const uint8_t* metallib_data,
    size_t metallib_size,
    const uint8_t* dxil_data,
    size_t dxil_size,
    const char* stage,
    const char* entry_point,
    const char* profile)
{
    // 确保缓存目录结构存在
    if (!ensure_directory(root)) return;

    std::string entry = cache_entry_path(root, cache_key);
    std::string parent = root + "/" + cache_key.substr(0, 2);
    if (!ensure_directory(parent)) return;
    if (!ensure_directory(entry)) return;

    // 写入 metallib.bin
    {
        std::string path = entry + "/metallib.bin";
        FILE* f = fopen(path.c_str(), "wb");
        if (f)
        {
            fwrite(metallib_data, 1, metallib_size, f);
            fclose(f);
        }
    }

    // 写入 dxil.bin
    if (dxil_data && dxil_size > 0)
    {
        std::string path = entry + "/dxil.bin";
        FILE* f = fopen(path.c_str(), "wb");
        if (f)
        {
            fwrite(dxil_data, 1, dxil_size, f);
            fclose(f);
        }
    }

    // 写入 meta.json
    {
        std::string path = entry + "/meta.json";
        FILE* f = fopen(path.c_str(), "w");
        if (f)
        {
            fprintf(f, "{\n");
            fprintf(f, "  \"stage\": \"%s\",\n", stage);
            fprintf(f, "  \"entry_point\": \"%s\",\n", entry_point);
            fprintf(f, "  \"profile\": \"%s\",\n", profile);
            fprintf(f, "  \"metallib_size\": %zu,\n", metallib_size);
            fprintf(f, "  \"dxil_size\": %zu\n", dxil_size);
            fprintf(f, "}\n");
            fclose(f);
        }
    }
}

/// 清空缓存目录（删除 ~/Library/Caches/SwitchMetal/ 下所有内容）
metal_result metal_shader_cache_clear(void)
{
    std::string root = get_cache_root();
    if (root.empty()) return METAL_RESULT_RUNTIME_ERROR;

    // 使用系统命令删除整个缓存目录
    std::string cmd = "rm -rf \"" + root + "\" 2>/dev/null";
    int rc = system(cmd.c_str());
    (void)rc;
    return METAL_RESULT_OK;
}

// ── Slang 全局会话（lazy singleton，进程生命周期）──
#if METAL_SLANG_FOUND
static slang::IGlobalSession* g_slang_global_session = nullptr;
static int g_slang_refcount = 0;

static slang::IGlobalSession* acquire_global_session()
{
    if (!g_slang_global_session)
    {
        slang::createGlobalSession(&g_slang_global_session);
        if (g_slang_global_session)
        {
            g_slang_refcount = 1;
        }
    }
    else
    {
        g_slang_refcount++;
    }
    return g_slang_global_session;
}

static void release_global_session()
{
    if (g_slang_global_session && --g_slang_refcount <= 0)
    {
        g_slang_global_session->release();
        g_slang_global_session = nullptr;
    }
}
#endif

// ── metal_shader_compiler_release：由 metal_release 调用，清理 Slang 会话 ──
void metal_shader_compiler_release(metal_shader_compiler* compiler)
{
    if (!compiler) return;
#if METAL_SLANG_FOUND
    release_global_session();
#endif
}

// ════════════════════════════════════════════════════════════════════
// 编译器生命周期
// ════════════════════════════════════════════════════════════════════

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

#if METAL_SLANG_FOUND
    // 只需要获取全局会话，ISession 在每次编译时按 profile 创建
    slang::IGlobalSession* globalSession = acquire_global_session();
    if (!globalSession)
    {
        free(compiler);
        return METAL_RESULT_RUNTIME_ERROR;
    }
    // 注意：release_global_session 在 metal_release 处理此编译器时调用
    // 此处 acquire 的 ref 由编译器的析构平衡
#else
    (void)device;
#endif

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
    (void)compiler;
    (void)config;
    return METAL_RESULT_OK;
}

uint32_t metal_shader_compiler_get_workarounds(
    metal_shader_compiler* compiler)
{
    if (!compiler) return 0;
    (void)compiler;
    return 0;
}

// ════════════════════════════════════════════════════════════════════
// Path B 回退：Slang C API → MSL → xcrun metal → metallib（P4.2.5）
// ════════════════════════════════════════════════════════════════════

/// 使用 Slang C API 的 SLANG_METAL 目标编译到 MSL，再通过 xcrun metal 转为 metallib
/// @return true 如果成功产出了 metallib 数据
static bool compile_path_b_metallib(
    const char* source_code,
    const char* entry_point,
    const char* stage,
    const char* profile,
    metal_shader_compile_result& result)
{
#if !METAL_SLANG_FOUND
    (void)source_code;
    (void)entry_point;
    (void)stage;
    (void)profile;
    (void)result;
    return false;
#else
    // 获取全局会话
    slang::IGlobalSession* globalSession = acquire_global_session();
    if (!globalSession) return false;

    // 创建 ISession，target 设为 SLANG_METAL
    slang::TargetDesc targetDesc = {};
    targetDesc.structureSize = sizeof(targetDesc);
    targetDesc.format = SLANG_METAL;
    targetDesc.profile = globalSession->findProfile(profile);

    slang::SessionDesc sessionDesc = {};
    sessionDesc.structureSize = sizeof(sessionDesc);
    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;

    slang::ISession* session = nullptr;
    SlangResult sr = globalSession->createSession(sessionDesc, &session);
    if (SLANG_FAILED(sr) || !session)
    {
        release_global_session();
        return false;
    }

    // 加载模块
    slang::IModule* module = session->loadModuleFromSourceString(
        "path_b_module", ".", source_code, nullptr);
    if (!module)
    {
        session->release();
        release_global_session();
        return false;
    }

    // 查找入口点
    slang::IEntryPoint* entryPoint = nullptr;
    sr = module->findEntryPointByName(entry_point, &entryPoint);
    if (SLANG_FAILED(sr) || !entryPoint)
    {
        session->release();
        release_global_session();
        return false;
    }

    // 创建 composite
    slang::IComponentType* components[] = { module, entryPoint };
    slang::IComponentType* composite = nullptr;
    sr = session->createCompositeComponentType(components, 2, &composite, nullptr);
    if (SLANG_FAILED(sr) || !composite)
    {
        entryPoint->release();
        session->release();
        release_global_session();
        return false;
    }
    entryPoint->release();

    // Link
    slang::IComponentType* linked = nullptr;
    sr = composite->link(&linked, nullptr);
    if (SLANG_FAILED(sr) || !linked)
    {
        session->release();
        release_global_session();
        return false;
    }

    // 获取 MSL 入口点代码（SLANG_METAL 目标下返回 MSL 源码文本）
    slang::IBlob* mslBlob = nullptr;
    sr = linked->getEntryPointCode(0, 0, &mslBlob, nullptr);
    if (SLANG_FAILED(sr) || !mslBlob)
    {
        session->release();
        release_global_session();
        return false;
    }

    const void* mslData = mslBlob->getBufferPointer();
    size_t mslSize = mslBlob->getBufferSize();
    if (!mslData || mslSize == 0)
    {
        mslBlob->release();
        session->release();
        release_global_session();
        return false;
    }

    // 写 MSL 到临时文件
    char tmpdir_template[] = "/tmp/metal_shader_pathb_XXXXXX";
    char* tmpdir = mkdtemp(tmpdir_template);
    if (!tmpdir)
    {
        mslBlob->release();
        session->release();
        release_global_session();
        return false;
    }

    std::string tmpdir_str(tmpdir);
    std::string msl_path = tmpdir_str + "/shader.msl";
    std::string metallib_path = tmpdir_str + "/shader.metallib";

    bool ok = false;
    FILE* f = fopen(msl_path.c_str(), "wb");
    if (f)
    {
        fwrite(mslData, 1, mslSize, f);
        fclose(f);

        // 尝试 xcrun metal 编译 MSL→metallib
        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
            "xcrun metal -x metal-staticlib -o \"%s\" \"%s\" 2>/dev/null",
            metallib_path.c_str(), msl_path.c_str());

        int rc = system(cmd);
        if (rc == 0)
        {
            auto data = read_file(metallib_path);
            if (!data.empty())
            {
                result.metallib_size = data.size();
                result.metallib_data = malloc(result.metallib_size);
                if (result.metallib_data)
                {
                    memcpy(result.metallib_data, data.data(), result.metallib_size);
                    ok = true;
                }
            }
        }
    }

    // 清理
    mslBlob->release();
    // linked/composite/module — session 释放时自动清理
    session->release();
    release_global_session();

    // 清理临时文件
    std::string cleanup = "rm -rf " + tmpdir_str + " 2>/dev/null";
    system(cleanup.c_str());

    return ok;
#endif
}

// ════════════════════════════════════════════════════════════════════
// 编译实现（P4.2.2：Slang C API → DXIL → MSC popen → metallib）
// ════════════════════════════════════════════════════════════════════

metal_shader_compile_result metal_compile_shader(
    metal_shader_compiler* compiler,
    const char* source_code,
    const char* source_language,
    const char* stage,
    const char* entry_point,
    const char* profile)
{
    metal_shader_compile_result result = {};
    result.result = METAL_RESULT_OK;
    result.metallib_data = nullptr;
    result.metallib_size = 0;

    if (!compiler || !source_code || !source_language || !stage || !entry_point || !profile)
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
                 "不支持的着色器阶段：%s", stage);
        return result;
    }

    // ── 步骤 0：检查磁盘着色器缓存（P4.2.4）──
    std::string cache_root = get_cache_root();
    std::string cache_key = compute_cache_key(source_code, source_language, entry_point, stage, profile);
    std::vector<uint8_t> cached_metallib;
    bool cache_hit = load_from_cache(cache_root, cache_key, cached_metallib);

    if (cache_hit)
    {
        // 缓存命中：直接返回 metallib 数据
        result.metallib_size = cached_metallib.size();
        result.metallib_data = malloc(result.metallib_size);
        if (result.metallib_data)
        {
            memcpy(result.metallib_data, cached_metallib.data(), result.metallib_size);
        }
        result.result = METAL_RESULT_OK;
        return result;
    }

    // ── 步骤 1：Slang → DXIL ──
    std::vector<uint8_t> dxil_data;
    bool is_glsl_source = (strcmp(source_language, "glsl") == 0);

    // GLSL 源码跳过 Slang 直接编译路径：Slang 编译 GLSL 产出 COLOR 语义，
    // 与桥接路径的 TEXCOORD 语义不一致，导致 varyings 不匹配。
    // 通过 spirv-opt + normalize_hlsl_varying_semantics 确保语义对齐。
    if (is_glsl_source)
    {
        goto glsl_bridge;
    }

#if METAL_SLANG_FOUND
    {
        slang::IGlobalSession* globalSession = acquire_global_session();
        if (!globalSession)
        {
            result.result = METAL_RESULT_RUNTIME_ERROR;
            snprintf(result.error_message, sizeof(result.error_message),
                     "无法初始化 Slang 全局会话。");
            return result;
        }

        // 每次编译创建新的 ISession（确保 profile 与着色器阶段匹配）
        slang::TargetDesc targetDesc = {};
        targetDesc.structureSize = sizeof(targetDesc);
        targetDesc.format = SLANG_DXIL;
        targetDesc.profile = globalSession->findProfile(profile);

        slang::SessionDesc sessionDesc = {};
        sessionDesc.structureSize = sizeof(sessionDesc);
        sessionDesc.targets = &targetDesc;
        sessionDesc.targetCount = 1;
        sessionDesc.allowGLSLSyntax = is_glsl_source;

        slang::ISession* compileSession = nullptr;
        SlangResult sr = globalSession->createSession(sessionDesc, &compileSession);
        if (SLANG_FAILED(sr) || !compileSession)
        {
            result.result = METAL_RESULT_RUNTIME_ERROR;
            snprintf(result.error_message, sizeof(result.error_message),
                     "Slang createSession 失败（%d）", (int)sr);
            release_global_session();
            return result;
        }

        // 加载模块
        slang::IBlob* diagnostics = nullptr;
        slang::IModule* module = compileSession->loadModuleFromSourceString(
            "shader_module", ".", source_code, &diagnostics);

        if (!module)
        {
            std::string diagnostic_text;
            if (diagnostics)
            {
                const char* diag_ptr = static_cast<const char*>(diagnostics->getBufferPointer());
                size_t diag_size = diagnostics->getBufferSize();
                if (diag_ptr && diag_size > 0)
                {
                    diagnostic_text.assign(diag_ptr, diag_size);
                }
                diagnostics->release();
            }

            result.result = METAL_RESULT_COMPILE_FAILED;
            snprintf(result.error_message, sizeof(result.error_message),
                     "Slang loadModuleFromSourceString 失败（lang=%s）：%.400s",
                     source_language,
                     diagnostic_text.empty() ? "无诊断信息" : diagnostic_text.c_str());
        }
        else
        {
            // 查找入口点，获取 IEntryPoint（继承自 IComponentType）
            slang::IEntryPoint* entryPointComponent = nullptr;
            sr = module->findEntryPointByName(entry_point, &entryPointComponent);
            if (SLANG_FAILED(sr) || !entryPointComponent)
            {
                result.result = METAL_RESULT_COMPILE_FAILED;
                snprintf(result.error_message, sizeof(result.error_message),
                         "未找到入口点 '%s'（阶段不匹配或函数名错误）", entry_point);
            }
            else
            {
                // 创建复合组件类型（module + entry point）
                slang::IComponentType* components[2] = { module, entryPointComponent };
                slang::IComponentType* composite = nullptr;
                sr = compileSession->createCompositeComponentType(
                    components, 2, &composite, nullptr);

                if (SLANG_FAILED(sr) || !composite)
                {
                    result.result = METAL_RESULT_COMPILE_FAILED;
                    snprintf(result.error_message, sizeof(result.error_message),
                             "Slang createCompositeComponentType 失败（%d）", (int)sr);
                }
                else
                {
                    // Link
                    slang::IComponentType* linkedProgram = nullptr;
                    sr = composite->link(&linkedProgram, nullptr);
                    if (SLANG_FAILED(sr) || !linkedProgram)
                    {
                        result.result = METAL_RESULT_COMPILE_FAILED;
                        snprintf(result.error_message, sizeof(result.error_message),
                                 "Slang link 失败（%d）", (int)sr);
                    }
                    else
                    {
                        // 获取 DXIL 入口点代码
                        slang::IBlob* dxilBlob = nullptr;
                        sr = linkedProgram->getEntryPointCode(0, 0, &dxilBlob, nullptr);
                        if (SLANG_FAILED(sr) || !dxilBlob)
                        {
                            result.result = METAL_RESULT_COMPILE_FAILED;
                            snprintf(result.error_message, sizeof(result.error_message),
                                     "Slang getEntryPointCode 失败（%d）", (int)sr);
                        }
                        else
                        {
                            const void* blobData = dxilBlob->getBufferPointer();
                            size_t blobSize = dxilBlob->getBufferSize();
                            if (blobData && blobSize > 0)
                            {
                                dxil_data.assign(
                                    static_cast<const uint8_t*>(blobData),
                                    static_cast<const uint8_t*>(blobData) + blobSize);
                            }

                            dxilBlob->release();
                        }
                    }
                }

                entryPointComponent->release();
            }
        }

        // 不手动释放 linkedProgram/composite/module — session 释放时自动清理
        compileSession->release();
        release_global_session();
    }
#else
    (void)stage;
    (void)entry_point;
    (void)profile;
#endif

    // Slang API 失败或库未链接时，回退到 popen（仅非 GLSL，GLSL 走桥接）
    if (dxil_data.empty() && !is_glsl_source)
    {
        // ── 回退：popen slangc CLI ──
        char tmpdir_template[] = "/tmp/metal_shader_XXXXXX";
        char* tmpdir = mkdtemp(tmpdir_template);
        if (!tmpdir)
        {
            result.result = METAL_RESULT_RUNTIME_ERROR;
            snprintf(result.error_message, sizeof(result.error_message),
                     "无法创建临时目录：%s", strerror(errno));
            return result;
        }

        bool is_glsl_source = strcmp(source_language, "glsl") == 0;
        std::string tmpdir_str(tmpdir);
        std::string slang_path = tmpdir_str + (is_glsl_source ? "/shader.glsl" : "/shader.slang");
        std::string dxil_path    = tmpdir_str + "/shader.dxil";
        std::string slang_err_path = tmpdir_str + "/slang.err";

        // 写入源码
        {
            FILE* f = fopen(slang_path.c_str(), "w");
            if (!f)
            {
                result.result = METAL_RESULT_RUNTIME_ERROR;
                snprintf(result.error_message, sizeof(result.error_message),
                         "无法写入着色器源码。");
                std::string cleanup = "rm -rf " + tmpdir_str + " 2>/dev/null";
                system(cleanup.c_str());
                return result;
            }
            fwrite(source_code, 1, strlen(source_code), f);
            fclose(f);
        }

        // 运行 slangc
        {
            char cmd[4096];
            if (is_glsl_source)
            {
                snprintf(cmd, sizeof(cmd),
                    "slangc -allow-glsl -lang glsl \"%s\" -target dxil -entry %s -stage %s -profile %s -o \"%s\" 2>\"%s\"",
                    slang_path.c_str(), entry_point, stage, profile,
                    dxil_path.c_str(), slang_err_path.c_str());
            }
            else
            {
                snprintf(cmd, sizeof(cmd),
                    "slangc \"%s\" -target dxil -entry %s -stage %s -profile %s -o \"%s\" 2>\"%s\"",
                    slang_path.c_str(), entry_point, stage, profile,
                    dxil_path.c_str(), slang_err_path.c_str());
            }

            std::string cmd_output;
            int slang_rc = run_command(cmd, cmd_output);

            dxil_data = read_file(dxil_path);
            if (slang_rc != 0 || dxil_data.empty())
            {
                auto err_data = read_file(slang_err_path);
                std::string err_msg(err_data.begin(), err_data.end());
                if (err_msg.empty()) err_msg = cmd_output;

                result.result = METAL_RESULT_COMPILE_FAILED;
                snprintf(result.error_message, sizeof(result.error_message),
                         "slangc 失败（exit=%d）：%.400s", slang_rc, err_msg.c_str());

                if (!should_keep_failed_shader_temp())
                {
                    std::string cleanup = "rm -rf " + tmpdir_str + " 2>/dev/null";
                    system(cleanup.c_str());
                }

                if (!is_glsl_source)
                {
                    return result;
                }

                dxil_data.clear();
            }
        }

        // 清理临时文件
        {
            if (!should_keep_failed_shader_temp())
            {
                std::string cleanup = "rm -rf " + tmpdir_str + " 2>/dev/null";
                system(cleanup.c_str());
            }
        }
    }

glsl_bridge:
    if (dxil_data.empty() && strcmp(source_language, "glsl") == 0)
    {
        if (!compile_glsl_via_spirv_hlsl_bridge(
                source_code,
                stage,
                profile,
                dxil_data,
                result.error_message,
                sizeof(result.error_message)))
        {
            result.result = METAL_RESULT_COMPILE_FAILED;
            return result;
        }
    }

    if (dxil_data.empty())
    {
        result.result = METAL_RESULT_COMPILE_FAILED;
        snprintf(result.error_message, sizeof(result.error_message),
                 "DXIL 数据为空（Slang API 回退和 popen 均失败）。");
        return result;
    }

    // ── 步骤 2：MSC（DXIL → metallib），使用 IRCompiler SDK API（P4.2.3）──
    // 回退：MSC SDK 不可用或失败时使用 popen("metal-shaderconverter")
    bool msc_ok = false;

#if METAL_IRCONVERTER_FOUND
    {
        // 映射着色器阶段到 IRShaderStage
        IRShaderStage msc_stage = IRShaderStageInvalid;
        if (strcmp(stage, "vertex") == 0)
            msc_stage = IRShaderStageVertex;
        else if (strcmp(stage, "fragment") == 0)
            msc_stage = IRShaderStageFragment;
        else if (strcmp(stage, "compute") == 0)
            msc_stage = IRShaderStageCompute;

        if (msc_stage != IRShaderStageInvalid)
        {
            // 创建 IRCompiler
            IRCompiler* ir_compiler = IRCompilerCreate();
            if (ir_compiler)
            {
                // 从 DXIL 字节创建 IRObject
                IRObject* dxil_obj = IRObjectCreateFromDXIL(
                    dxil_data.data(), dxil_data.size(), IRBytecodeOwnershipCopy);

                if (dxil_obj)
                {
                    // 编译并链接（产出 Metal IR）
                    IRError* ir_error = nullptr;
                    IRObject* compiled_obj = IRCompilerAllocCompileAndLink(
                        ir_compiler, entry_point, dxil_obj, &ir_error);

                    if (compiled_obj)
                    {
                        // 获取编译后对象的着色器阶段
                        IRShaderStage compiled_stage = IRObjectGetMetalIRShaderStage(compiled_obj);

                        // 提取 metallib 二进制数据
                        // IRMetalLibBinary 是不透明结构体，需通过 Create 分配
                        IRMetalLibBinary* lib_binary = IRMetalLibBinaryCreate();
                        if (lib_binary && IRObjectGetMetalLibBinary(
                                compiled_obj, compiled_stage, lib_binary))
                        {
                            size_t metallib_sz = IRMetalLibGetBytecodeSize(lib_binary);
                            if (metallib_sz > 0)
                            {
                                result.metallib_data = malloc(metallib_sz);
                                if (result.metallib_data)
                                {
                                    IRMetalLibGetBytecode(lib_binary,
                                        static_cast<uint8_t*>(result.metallib_data));
                                    result.metallib_size = metallib_sz;
                                    msc_ok = true;
                                }
                            }
                            IRMetalLibBinaryDestroy(lib_binary);
                        }
                        IRObjectDestroy(compiled_obj);
                    }
                    else if (ir_error)
                    {
                        uint32_t err_code = IRErrorGetCode(ir_error);
                        const void* err_payload = IRErrorGetPayload(ir_error);
                        if (err_payload)
                        {
                            snprintf(result.error_message, sizeof(result.error_message),
                                     "MSC 编译失败（code=%u）：%.400s",
                                     err_code, static_cast<const char*>(err_payload));
                        }
                        else
                        {
                            snprintf(result.error_message, sizeof(result.error_message),
                                     "MSC 编译失败（code=%u）", err_code);
                        }
                    }

                    IRObjectDestroy(dxil_obj);
                }
                else
                {
                    snprintf(result.error_message, sizeof(result.error_message),
                             "MSC IRObjectCreateFromDXIL 失败。");
                }

                IRCompilerDestroy(ir_compiler);
            }
            else
            {
                snprintf(result.error_message, sizeof(result.error_message),
                         "MSC IRCompilerCreate 失败。");
            }
        }
    }
#endif

    // MSC SDK 失败或不可用时回退到 popen
    if (!msc_ok)
    {
        // ── 回退：popen metal-shaderconverter CLI ──
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
        std::string dxil_path    = tmpdir_str + "/shader.dxil";
        std::string metallib_path = tmpdir_str + "/shader.metallib";
        std::string msc_err_path   = tmpdir_str + "/msc.err";

        // 将 DXIL 数据写入临时文件
        {
            FILE* f = fopen(dxil_path.c_str(), "wb");
            if (!f)
            {
                result.result = METAL_RESULT_RUNTIME_ERROR;
                snprintf(result.error_message, sizeof(result.error_message),
                         "无法写入 DXIL 数据。");
                std::string cleanup = "rm -rf " + tmpdir_str + " 2>/dev/null";
                system(cleanup.c_str());
                return result;
            }
            fwrite(dxil_data.data(), 1, dxil_data.size(), f);
            fclose(f);
        }

        // 运行 MSC
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

            // 分配内存并拷贝 metallib 数据
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

        // 清理临时文件
        {
            std::string cleanup = "rm -rf " + tmpdir_str + " 2>/dev/null";
            system(cleanup.c_str());
        }
    }

    // ── 步骤 3：Path B 回退（P4.2.5）—— Path A 全部失败时尝试 Slang→MSL→xcrun metal ──
    if (!result.metallib_data || result.metallib_size == 0)
    {
        bool pathb_ok = compile_path_b_metallib(
            source_code, entry_point, stage, profile, result);

        if (pathb_ok)
        {
            result.result = METAL_RESULT_OK;
            snprintf(result.error_message, sizeof(result.error_message),
                     "Path B 回退成功：Slang→MSL→xcrun metal");
        }
        else if (!result.metallib_data || result.metallib_size == 0)
        {
            // 所有路径均失败
            if (result.error_message[0] == '\0')
            {
                snprintf(result.error_message, sizeof(result.error_message),
                         "所有着色器编译路径均失败（Path A + Path B）");
            }
            result.result = METAL_RESULT_COMPILE_FAILED;
        }
    }

    if (result.metallib_data && result.metallib_size > 0)
    {
        result.result = METAL_RESULT_OK;
    }

    // ── 编译成功：写入磁盘缓存（P4.2.4）──
    if (result.result == METAL_RESULT_OK && result.metallib_data && result.metallib_size > 0)
    {
        store_to_cache(
            cache_root, cache_key,
            static_cast<const uint8_t*>(result.metallib_data),
            result.metallib_size,
            dxil_data.empty() ? nullptr : dxil_data.data(),
            dxil_data.size(),
            stage, entry_point, profile);
    }

    return result;
}

/// 通过缓存键直接加载 metallib 数据（P4.2.6）
/// C# 侧已知缓存键时可直接调用，跳过整个编译管线。
metal_shader_compile_result metal_load_program_binary(const char* cache_key)
{
    metal_shader_compile_result result = {};
    result.result = METAL_RESULT_OK;
    result.metallib_data = nullptr;
    result.metallib_size = 0;

    if (!cache_key || strlen(cache_key) != 64)
    {
        result.result = METAL_RESULT_INVALID_ARGUMENT;
        snprintf(result.error_message, sizeof(result.error_message),
                 "无效的缓存键：必须是 64 字符 SHA256 hex。");
        return result;
    }

    // 校验缓存键是否为合法 hex
    for (int i = 0; i < 64; i++)
    {
        if (!isxdigit((unsigned char)cache_key[i]))
        {
            result.result = METAL_RESULT_INVALID_ARGUMENT;
            snprintf(result.error_message, sizeof(result.error_message),
                     "缓存键包含无效十六进制字符：位置 %d。", i);
            return result;
        }
    }

    std::string root = get_cache_root();
    std::vector<uint8_t> metallib;

    if (load_from_cache(root, std::string(cache_key), metallib) && !metallib.empty())
    {
        result.metallib_size = metallib.size();
        result.metallib_data = malloc(result.metallib_size);
        if (result.metallib_data)
        {
            memcpy(result.metallib_data, metallib.data(), result.metallib_size);
        }
        else
        {
            result.result = METAL_RESULT_OUT_OF_MEMORY;
        }
    }
    else
    {
        result.result = METAL_RESULT_COMPILE_FAILED;
        snprintf(result.error_message, sizeof(result.error_message),
                 "缓存未命中：键 %s 不存在。", cache_key);
    }

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
