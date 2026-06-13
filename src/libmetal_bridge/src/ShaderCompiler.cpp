// ShaderCompiler.cpp — 编译器生命周期与着色器编译实现
//
// P3.1b   — 收口了单例策略、workaround 位掩码与默认配置
// P4.2.1  — popen() 调 slangc + MSC CLI 打通全链路
// P4.2.2  — slangc popen 替换为 Slang C API（本篇当前实现）
// P4.2.3  — 后续将 MSC popen 替换为 IRCompiler SDK API
//
// 编译流程（Path A）：
//   Slang 原生语法 → slang C API (libslang.dylib) → DXIL byte[]
//                                                → popen("metal-shaderconverter") → metallib byte[]

#include "metal_bridge.h"
#include "metal_internal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

// Slang C API：仅在可用时引入
#if METAL_SLANG_FOUND
#include <slang.h>
#include <slang-com-ptr.h>
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

// ── 内部结构体（opaque handle 的实际定义）──

struct metal_shader_compiler
{
    METAL_HANDLE_HEADER
    metal_device* device;
};

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
// 编译实现（P4.2.2：Slang C API → DXIL → MSC popen → metallib）
// ════════════════════════════════════════════════════════════════════

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
                 "不支持的着色器阶段：%s", stage);
        return result;
    }

    // ── 步骤 1：Slang → DXIL（使用 Slang C API，每次编译新建 ISession）──
    std::vector<uint8_t> dxil_data;

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
        slang::IModule* module = compileSession->loadModuleFromSourceString(
            "shader_module", ".", source_code, nullptr);

        if (!module)
        {
            result.result = METAL_RESULT_COMPILE_FAILED;
            snprintf(result.error_message, sizeof(result.error_message),
                     "Slang loadModuleFromSourceString 失败。");
            compileSession->release();
            release_global_session();
            return result;
        }

        // 查找入口点，获取 IEntryPoint（继承自 IComponentType）
        slang::IEntryPoint* entryPointComponent = nullptr;
        sr = module->findEntryPointByName(entry_point, &entryPointComponent);
        if (SLANG_FAILED(sr) || !entryPointComponent)
        {
            result.result = METAL_RESULT_COMPILE_FAILED;
            snprintf(result.error_message, sizeof(result.error_message),
                     "未找到入口点 '%s'（阶段不匹配或函数名错误）", entry_point);
            module->release();
            compileSession->release();
            release_global_session();
            return result;
        }

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
            entryPointComponent->release();
            module->release();
            compileSession->release();
            release_global_session();
            return result;
        }
        entryPointComponent->release();

        // Link
        slang::IComponentType* linkedProgram = nullptr;
        sr = composite->link(&linkedProgram, nullptr);
        if (SLANG_FAILED(sr) || !linkedProgram)
        {
            result.result = METAL_RESULT_COMPILE_FAILED;
            snprintf(result.error_message, sizeof(result.error_message),
                     "Slang link 失败（%d）", (int)sr);
            composite->release();
            module->release();
            compileSession->release();
            release_global_session();
            return result;
        }

        // 获取 DXIL 入口点代码
        slang::IBlob* dxilBlob = nullptr;
        sr = linkedProgram->getEntryPointCode(0, 0, &dxilBlob, nullptr);
        if (SLANG_FAILED(sr) || !dxilBlob)
        {
            result.result = METAL_RESULT_COMPILE_FAILED;
            snprintf(result.error_message, sizeof(result.error_message),
                     "Slang getEntryPointCode 失败（%d）", (int)sr);
            linkedProgram->release();
            composite->release();
            module->release();
            compileSession->release();
            release_global_session();
            return result;
        }

        // 读取 DXIL 数据
        const void* blobData = dxilBlob->getBufferPointer();
        size_t blobSize = dxilBlob->getBufferSize();
        if (blobData && blobSize > 0)
        {
            dxil_data.assign(
                static_cast<const uint8_t*>(blobData),
                static_cast<const uint8_t*>(blobData) + blobSize);
        }

        dxilBlob->release();
        linkedProgram->release();
        composite->release();
        module->release();
        compileSession->release();
        release_global_session();
    }
#else
    (void)stage;
    (void)entry_point;
    (void)profile;
#endif

    // Slang API 失败或库未链接时，回退到 popen
    if (dxil_data.empty())
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

        std::string tmpdir_str(tmpdir);
        std::string slang_path   = tmpdir_str + "/shader.slang";
        std::string dxil_path    = tmpdir_str + "/shader.dxil";
        std::string slang_err_path = tmpdir_str + "/slang.err";

        // 写入 Slang 源码
        {
            FILE* f = fopen(slang_path.c_str(), "w");
            if (!f)
            {
                result.result = METAL_RESULT_RUNTIME_ERROR;
                snprintf(result.error_message, sizeof(result.error_message),
                         "无法写入 Slang 源码。");
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
            snprintf(cmd, sizeof(cmd),
                "slangc \"%s\" -target dxil -entry %s -stage %s -profile %s -o \"%s\" 2>\"%s\"",
                slang_path.c_str(), entry_point, stage, profile,
                dxil_path.c_str(), slang_err_path.c_str());

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

                std::string cleanup = "rm -rf " + tmpdir_str + " 2>/dev/null";
                system(cleanup.c_str());
                return result;
            }
        }

        // 清理临时文件
        {
            std::string cleanup = "rm -rf " + tmpdir_str + " 2>/dev/null";
            system(cleanup.c_str());
        }
    }

    if (dxil_data.empty())
    {
        result.result = METAL_RESULT_COMPILE_FAILED;
        snprintf(result.error_message, sizeof(result.error_message),
                 "DXIL 数据为空（Slang API 回退和 popen 均失败）。");
        return result;
    }

    // ── 步骤 2：MSC（DXIL → metallib），仍使用 popen，P4.2.3 将替换 ──
    {
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
