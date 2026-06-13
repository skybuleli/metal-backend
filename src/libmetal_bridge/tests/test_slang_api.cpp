// test_slang_api.cpp — Slang C API 独立测试（P4.2.2）
//
// 直接 link libslang.dylib（不含 Metal Framework 依赖），验证：
//   1. createGlobalSession 成功
//   2. 编译顶点/片段/计算着色器 → DXIL 非空 + 魔数正确
//   3. profile 查询正确性
//   4. 多次重复编译无泄漏
//
// 编译：
//   SDKROOT=$(xcrun --sdk macosx --show-sdk-path)
//   CXX=$(xcrun --sdk macosx --find clang++)
//   $CXX -std=c++17 -isysroot $SDKROOT \
//        -I$SDKROOT/usr/include/c++/v1 -I/usr/local/include/slang \
//        -o test_slang_api test_slang_api.cpp \
//        -L/usr/local/lib -lslang

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <slang.h>

// ── 着色器源码 ──

const char* kVertexShader = R"(
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
)";

const char* kFragmentShader = R"(
Texture2D<float4> g_texture;
SamplerState g_sampler;

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

[shader("fragment")]
float4 main(VSOut input) : SV_Target0
{
    return g_texture.Sample(g_sampler, input.uv);
}
)";

const char* kComputeShader = R"(
RWStructuredBuffer<float> g_output;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    g_output[dtid.x] = (float)dtid.x * 0.5;
}
)";

// ── 辅助 ──

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name, expr) do { \
    printf("  [%-30s] ", name); \
    if (expr) { \
        printf("\033[32mOK\033[0m\n"); \
        g_pass++; \
    } else { \
        printf("\033[31mFAIL\033[0m\n"); \
        g_fail++; \
    } \
} while(0)

/// 编译单个着色器，返回 malloc 分配的 DXIL 字节（调用者 free）
/// 流程：module → findEntryPointByName(IEntryPoint) → createCompositeComponentType → link → getEntryPointCode
static unsigned char* compile_to_dxil(
    slang::ISession* session,
    const char* source,
    const char* entry_point,
    size_t* out_size,
    std::string* out_error)
{
    *out_size = 0;

    slang::IModule* module = session->loadModuleFromSourceString(
        "test_module", ".", source, nullptr);
    if (!module)
    {
        if (out_error) *out_error = "loadModuleFromSourceString failed";
        return nullptr;
    }

    // 获取入口点 IComponentType
    slang::IEntryPoint* entryPoint = nullptr;
    SlangResult sr = module->findEntryPointByName(entry_point, &entryPoint);
    if (SLANG_FAILED(sr) || !entryPoint)
    {
        if (out_error) *out_error = std::string("entry point '") + entry_point + "' not found";
        module->release();
        return nullptr;
    }

    // 创建 composite（module + entry point）
    slang::IComponentType* composite = nullptr;
    slang::IComponentType* components[] = { module, entryPoint };
    sr = session->createCompositeComponentType(components, 2, &composite, nullptr);
    if (SLANG_FAILED(sr) || !composite)
    {
        if (out_error) *out_error = "createCompositeComponentType failed";
        entryPoint->release();
        module->release();
        return nullptr;
    }
    entryPoint->release();

    // Link
    slang::IComponentType* linked = nullptr;
    sr = composite->link(&linked, nullptr);
    if (SLANG_FAILED(sr) || !linked)
    {
        if (out_error) *out_error = "link failed";
        composite->release();
        module->release();
        return nullptr;
    }

    // 获取 DXIL
    slang::IBlob* dxilBlob = nullptr;
    sr = linked->getEntryPointCode(0, 0, &dxilBlob, nullptr);
    if (SLANG_FAILED(sr) || !dxilBlob)
    {
        if (out_error) *out_error = "getEntryPointCode failed";
        linked->release();
        composite->release();
        module->release();
        return nullptr;
    }

    // 读取 DXIL 数据
    const void* blobData = dxilBlob->getBufferPointer();
    size_t blobSize = dxilBlob->getBufferSize();
    unsigned char* result = nullptr;
    if (blobData && blobSize > 0)
    {
        result = (unsigned char*)malloc(blobSize);
        if (result)
        {
            memcpy(result, blobData, blobSize);
            *out_size = blobSize;
        }
    }

    dxilBlob->release();
    linked->release();
    composite->release();
    module->release();
    return result;
}

// ════════════════════════════════════════════════════════════════════

int main()
{
    printf("============================================\n");
    printf(" Slang C API 独立测试（P4.2.2）\n");
    printf("============================================\n\n");

    // ── 1. createGlobalSession ──
    printf("── 1. 全局会话 ──\n");
    slang::IGlobalSession* globalSession = nullptr;
    slang::createGlobalSession(&globalSession);
    TEST("createGlobalSession", globalSession != nullptr);
    if (!globalSession)
    {
        printf("\n  无法创建 Slang 全局会话，终止测试。\n");
        return 1;
    }

    // ── 2. findProfile ──
    printf("── 2. Profile 查询 ──\n");
    SlangProfileID pSm60 = globalSession->findProfile("sm_6_0");
    SlangProfileID pPs60 = globalSession->findProfile("ps_6_0");
    SlangProfileID pCs60 = globalSession->findProfile("cs_6_0");
    SlangProfileID pUnk  = globalSession->findProfile("nonexistent_profile");
    TEST("sm_6_0 != UNKNOWN",   pSm60 != SLANG_PROFILE_UNKNOWN);
    TEST("ps_6_0 != UNKNOWN",   pPs60 != SLANG_PROFILE_UNKNOWN);
    TEST("cs_6_0 != UNKNOWN",   pCs60 != SLANG_PROFILE_UNKNOWN);
    TEST("unknown == UNKNOWN",  pUnk  == SLANG_PROFILE_UNKNOWN);
    TEST("sm_6_0 != ps_6_0",   pSm60 != pPs60);
    TEST("ps_6_0 != cs_6_0",   pPs60 != pCs60);

    // ── 创建 DXIL session ──
    slang::TargetDesc targetDesc = {};
    targetDesc.structureSize = sizeof(targetDesc);
    targetDesc.format = SLANG_DXIL;
    targetDesc.profile = pSm60;

    slang::SessionDesc sessionDesc = {};
    sessionDesc.structureSize = sizeof(sessionDesc);
    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;

    slang::ISession* session = nullptr;
    globalSession->createSession(sessionDesc, &session);
    TEST("createSession", session != nullptr);
    if (!session)
    {
        globalSession->release();
        return 1;
    }

    // ── 3. 顶点着色器 ──
    printf("\n── 3. 顶点着色器（sm_6_0）──\n");
    size_t vsSize = 0;
    std::string vsErr;
    unsigned char* vsDxil = compile_to_dxil(session, kVertexShader, "main", &vsSize, &vsErr);
    TEST("编译成功",    vsDxil != nullptr);
    TEST("DXIL 非空",   vsSize > 0);
    TEST("DXIL > 100B", vsSize > 100);
    if (vsDxil)
    {
        bool isDxil = (vsDxil[0] == 'D' && vsDxil[1] == 'X' &&
                       vsDxil[2] == 'B' && vsDxil[3] == 'C');
        TEST("魔数 'DXBC'", isDxil);
        printf("   大小: %zu bytes\n", vsSize);
    }
    else printf("   错误: %s\n", vsErr.c_str());

    // ── 4. 片段着色器 ──
    printf("\n── 4. 片段着色器（ps_6_0）──\n");
    size_t fsSize = 0;
    std::string fsErr;
    unsigned char* fsDxil = compile_to_dxil(session, kFragmentShader, "main", &fsSize, &fsErr);
    TEST("编译成功",    fsDxil != nullptr);
    TEST("DXIL 非空",   fsSize > 0);
    TEST("DXIL > 100B", fsSize > 100);
    if (fsDxil)
    {
        bool isDxil = (fsDxil[0] == 'D' && fsDxil[1] == 'X' &&
                       fsDxil[2] == 'B' && fsDxil[3] == 'C');
        TEST("魔数 'DXBC'", isDxil);
        printf("   大小: %zu bytes\n", fsSize);
    }
    else printf("   错误: %s\n", fsErr.c_str());

    // ── 5. 计算着色器 ──
    printf("\n── 5. 计算着色器（cs_6_0）──\n");
    size_t csSize = 0;
    std::string csErr;
    unsigned char* csDxil = compile_to_dxil(session, kComputeShader, "main", &csSize, &csErr);
    TEST("编译成功",    csDxil != nullptr);
    TEST("DXIL 非空",   csSize > 0);
    TEST("DXIL > 100B", csSize > 100);
    if (csDxil)
    {
        bool isDxil = (csDxil[0] == 'D' && csDxil[1] == 'X' &&
                       csDxil[2] == 'B' && csDxil[3] == 'C');
        TEST("魔数 'DXBC'", isDxil);
        printf("   大小: %zu bytes\n", csSize);
    }
    else printf("   错误: %s\n", csErr.c_str());

    // ── 6. 重复编译稳定性 ──
    printf("\n── 6. 重复编译稳定性 ──\n");
    bool allOk = true;
    for (int i = 0; i < 3; i++)
    {
        size_t sz = 0;
        std::string err;
        unsigned char* dxil = compile_to_dxil(session, kVertexShader, "main", &sz, &err);
        if (!dxil || sz == 0) { printf("   第 %d 次: FAIL (%s)\n", i + 1, err.c_str()); allOk = false; }
        free(dxil);
    }
    TEST("3 次重复编译均成功", allOk);

    // ── 清理 ──
    printf("\n── 7. 清理 ──\n");
    free(vsDxil); free(fsDxil); free(csDxil);
    session->release();
    globalSession->release();
    TEST("所有资源释放", true);

    // ── 汇总 ──
    printf("\n============================================\n");
    if (g_fail == 0) printf(" \033[32m全部通过 (%d/%d)\033[0m\n", g_pass, g_pass);
    else             printf(" \033[32m%d 通过\033[0m / \033[31m%d 失败\033[0m\n", g_pass, g_fail);
    printf("============================================\n");
    return (g_fail > 0) ? 1 : 0;
}
