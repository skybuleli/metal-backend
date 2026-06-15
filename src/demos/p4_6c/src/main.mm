#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace
{
constexpr std::uint32_t kWidth = 960;
constexpr std::uint32_t kHeight = 540;
constexpr std::uint32_t kAtlasCells = 8;
constexpr std::uint32_t kAtlasSize = kAtlasCells * 8;
constexpr std::uint32_t kBoardCols = 10;
constexpr std::uint32_t kBoardRows = 20;
constexpr std::uint32_t kBoardCell = 18;
constexpr std::uint32_t kBoardX = 48;
constexpr std::uint32_t kBoardY = 104;
constexpr std::uint32_t kCourtX = 504;
constexpr std::uint32_t kCourtY = 104;

struct Pixel
{
    std::uint8_t b;
    std::uint8_t g;
    std::uint8_t r;
    std::uint8_t a;
};

struct alignas(16) SceneData
{
    float screenSize[2];
    float atlasSize[2];
    float pad[4];
};

static_assert(sizeof(SceneData) == 32, "SceneData 大小必须为 32 字节");

struct alignas(16) QuadInstance
{
    float position[2];
    float size[2];
    float uvMin[2];
    float uvMax[2];
    float tint[4];
};

static_assert(sizeof(QuadInstance) == 48, "QuadInstance 大小必须为 48 字节");

struct AtlasCell
{
    std::uint32_t x;
    std::uint32_t y;
};

struct GlyphDef
{
    char ch;
    AtlasCell cell;
    std::array<const char*, 7> rows;
};

std::string ErrorToString(NS::Error* error)
{
    if (error == nullptr)
    {
        return "未知错误";
    }

    NS::String* description = error->localizedDescription();
    return description != nullptr ? description->utf8String() : "未知错误";
}

bool WritePpm(const std::filesystem::path& path,
              const std::vector<Pixel>& pixels,
              std::uint32_t width,
              std::uint32_t height)
{
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
        return false;
    }

    output << "P6\n" << width << " " << height << "\n255\n";
    for (const Pixel& pixel : pixels)
    {
        output.put(static_cast<char>(pixel.r));
        output.put(static_cast<char>(pixel.g));
        output.put(static_cast<char>(pixel.b));
    }
    return true;
}

void SetAtlasPixel(std::vector<Pixel>& atlas, std::uint32_t x, std::uint32_t y, Pixel pixel)
{
    atlas[y * kAtlasSize + x] = pixel;
}

void FillAtlasCell(std::vector<Pixel>& atlas, std::uint32_t cell_x, std::uint32_t cell_y, Pixel pixel)
{
    for (std::uint32_t y = 0; y < 8; ++y)
    {
        for (std::uint32_t x = 0; x < 8; ++x)
        {
            SetAtlasPixel(atlas, cell_x * 8 + x, cell_y * 8 + y, pixel);
        }
    }
}

void DrawGlyph(std::vector<Pixel>& atlas, const GlyphDef& glyph, Pixel pixel)
{
    const std::uint32_t origin_x = glyph.cell.x * 8;
    const std::uint32_t origin_y = glyph.cell.y * 8;
    for (std::uint32_t row = 0; row < glyph.rows.size(); ++row)
    {
        const char* pattern = glyph.rows[row];
        for (std::uint32_t col = 0; col < 5; ++col)
        {
            if (pattern[col] == '#')
            {
                SetAtlasPixel(atlas, origin_x + 1 + col, origin_y + 1 + row, pixel);
            }
        }
    }
}

std::optional<AtlasCell> GlyphCellFor(char ch)
{
    switch (ch)
    {
        case ' ': return AtlasCell{7, 7};
        case 'A': return AtlasCell{0, 1};
        case 'B': return AtlasCell{1, 1};
        case 'E': return AtlasCell{2, 1};
        case 'G': return AtlasCell{3, 1};
        case 'H': return AtlasCell{4, 1};
        case 'I': return AtlasCell{5, 1};
        case 'K': return AtlasCell{6, 1};
        case 'M': return AtlasCell{7, 1};
        case 'F': return AtlasCell{0, 3};
        case 'N': return AtlasCell{0, 2};
        case 'O': return AtlasCell{1, 2};
        case 'P': return AtlasCell{2, 2};
        case 'R': return AtlasCell{3, 2};
        case 'S': return AtlasCell{4, 2};
        case 'T': return AtlasCell{5, 2};
        case 'U': return AtlasCell{6, 2};
        case 'W': return AtlasCell{7, 2};
        case '0': return AtlasCell{0, 5};
        case '1': return AtlasCell{1, 5};
        case '2': return AtlasCell{2, 5};
        case '3': return AtlasCell{3, 5};
        case '4': return AtlasCell{4, 5};
        case '5': return AtlasCell{5, 5};
        case '6': return AtlasCell{6, 5};
        case '7': return AtlasCell{7, 5};
        case '8': return AtlasCell{0, 6};
        case '9': return AtlasCell{1, 6};
        default: return std::nullopt;
    }
}

std::vector<Pixel> BuildAtlas()
{
    std::vector<Pixel> atlas(kAtlasSize * kAtlasSize, Pixel{0, 0, 0, 0});

    // 世界 tile / 球场元件 / HUD 面板。
    FillAtlasCell(atlas, 0, 0, Pixel{34, 44, 68, 255});   // 背景深色
    FillAtlasCell(atlas, 1, 0, Pixel{88, 124, 80, 255});  // Tetris block A
    FillAtlasCell(atlas, 2, 0, Pixel{68, 156, 180, 255}); // Tetris block B
    FillAtlasCell(atlas, 3, 0, Pixel{146, 108, 200, 255}); // Tetris block C
    FillAtlasCell(atlas, 4, 0, Pixel{42, 72, 52, 255});   // Pong court dark
    FillAtlasCell(atlas, 5, 0, Pixel{226, 224, 210, 255}); // Paddle
    FillAtlasCell(atlas, 6, 0, Pixel{246, 212, 94, 255});  // Ball
    FillAtlasCell(atlas, 7, 0, Pixel{255, 255, 255, 255}); // White / panel base

    const std::array<GlyphDef, 18> glyphs = {{
        {'A', {0, 1}, {{".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}}},
        {'B', {1, 1}, {{"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."}}},
        {'E', {2, 1}, {{"#####", "#....", "#....", "####.", "#....", "#....", "#####"}}},
        {'G', {3, 1}, {{".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###."}}},
        {'H', {4, 1}, {{"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}}},
        {'I', {5, 1}, {{"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "#####"}}},
        {'K', {6, 1}, {{"#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"}}},
        {'M', {7, 1}, {{"#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"}}},
        {'F', {0, 3}, {{"#####", "#....", "#....", "####.", "#....", "#....", "#...."}}},
        {'N', {0, 2}, {{"#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"}}},
        {'O', {1, 2}, {{".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}}},
        {'P', {2, 2}, {{"####.", "#...#", "#...#", "####.", "#....", "#....", "#...."}}},
        {'R', {3, 2}, {{"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"}}},
        {'S', {4, 2}, {{".###.", "#...#", "#....", ".###.", "....#", "#...#", ".###."}}},
        {'T', {5, 2}, {{"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."}}},
        {'U', {6, 2}, {{"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}}},
        {'W', {7, 2}, {{"#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#", "#...#"}}},
        {'0', {0, 5}, {{".###.", "#..##", "#.#.#", "##..#", "#...#", "#...#", ".###."}}},
    }};

    for (const GlyphDef& glyph : glyphs)
    {
        DrawGlyph(atlas, glyph, Pixel{248, 244, 236, 255});
    }

    return atlas;
}

AtlasCell TileCellFor(std::uint32_t tile_id)
{
    switch (tile_id % 4)
    {
        case 0: return AtlasCell{1, 0};
        case 1: return AtlasCell{2, 0};
        case 2: return AtlasCell{3, 0};
        default: return AtlasCell{0, 0};
    }
}

bool BoardOccupied(std::uint32_t col, std::uint32_t row)
{
    if (row >= 18)
    {
        return true;
    }
    if (row == 17 && col >= 1 && col <= 8)
    {
        return true;
    }
    if (row == 16 && (col == 2 || col == 3 || col == 6 || col == 7))
    {
        return true;
    }
    if (row == 15 && (col == 3 || col == 4 || col == 5 || col == 6))
    {
        return true;
    }
    if (row == 13 && col >= 4 && col <= 6)
    {
        return true;
    }
    if (row == 11 && (col == 2 || col == 7))
    {
        return true;
    }
    if (row == 9 && col >= 4 && col <= 8)
    {
        return true;
    }
    if (row == 7 && (col == 0 || col == 9))
    {
        return true;
    }
    if (row == 5 && col >= 4 && col <= 5)
    {
        return true;
    }
    if (row == 10 && col == 5)
    {
        return true;
    }
    return false;
}

std::vector<QuadInstance> BuildSceneInstances()
{
    std::vector<QuadInstance> instances;
    instances.reserve(320);

    // 背景。
    instances.push_back({
        {0.0f, 0.0f},
        {static_cast<float>(kWidth), static_cast<float>(kHeight)},
        {0.0f, 0.0f},
        {8.0f, 8.0f},
        {0.10f, 0.12f, 0.18f, 1.0f},
    });

    // 顶部 HUD 底板。
    instances.push_back({
        {24.0f, 18.0f},
        {912.0f, 58.0f},
        {56.0f, 0.0f},
        {64.0f, 8.0f},
        {0.08f, 0.10f, 0.16f, 0.86f},
    });
    instances.push_back({
        {24.0f, 18.0f},
        {912.0f, 8.0f},
        {56.0f, 0.0f},
        {64.0f, 8.0f},
        {0.96f, 0.78f, 0.32f, 0.92f},
    });

    // Tetris 面板。
    instances.push_back({
        {32.0f, 96.0f},
        {208.0f, 384.0f},
        {56.0f, 0.0f},
        {64.0f, 8.0f},
        {0.08f, 0.08f, 0.12f, 1.0f},
    });
    instances.push_back({
        {504.0f, 96.0f},
        {408.0f, 384.0f},
        {56.0f, 0.0f},
        {64.0f, 8.0f},
        {0.06f, 0.12f, 0.10f, 1.0f},
    });

    // Tetris 网格和方块。
    for (std::uint32_t row = 0; row < kBoardRows; ++row)
    {
        for (std::uint32_t col = 0; col < kBoardCols; ++col)
        {
            const bool occupied = BoardOccupied(col, row);
            const AtlasCell cell = occupied ? TileCellFor(col + row) : AtlasCell{0, 0};
            const std::uint32_t x = kBoardX + col * kBoardCell;
            const std::uint32_t y = kBoardY + row * kBoardCell;
            const Pixel tint = occupied
                ? Pixel{255, 255, 255, 255}
                : Pixel{60, 70, 92, 255};
            instances.push_back({
                {static_cast<float>(x), static_cast<float>(y)},
                {static_cast<float>(kBoardCell - 1), static_cast<float>(kBoardCell - 1)},
                {static_cast<float>(cell.x * 8), static_cast<float>(cell.y * 8)},
                {static_cast<float>(cell.x * 8 + 8), static_cast<float>(cell.y * 8 + 8)},
                {static_cast<float>(tint.r) / 255.0f, static_cast<float>(tint.g) / 255.0f, static_cast<float>(tint.b) / 255.0f, 1.0f},
            });
        }
    }

    // Pong 球场背景与中线。
    for (std::uint32_t y = 0; y < 12; ++y)
    {
        instances.push_back({
            {static_cast<float>(kCourtX + 200), static_cast<float>(kCourtY + 12 + y * 28)},
            {8.0f, 18.0f},
            {56.0f, 0.0f},
            {64.0f, 8.0f},
            {0.85f, 0.92f, 0.90f, 0.85f},
        });
    }

    // 左右挡板。
    instances.push_back({
        {static_cast<float>(kCourtX + 20), static_cast<float>(kCourtY + 140)},
        {12.0f, 72.0f},
        {40.0f, 0.0f},
        {48.0f, 8.0f},
        {0.92f, 0.90f, 0.84f, 1.0f},
    });
    instances.push_back({
        {static_cast<float>(kCourtX + 376), static_cast<float>(kCourtY + 188)},
        {12.0f, 72.0f},
        {40.0f, 0.0f},
        {48.0f, 8.0f},
        {0.92f, 0.90f, 0.84f, 1.0f},
    });

    // 球。
    instances.push_back({
        {static_cast<float>(kCourtX + 198), static_cast<float>(kCourtY + 170)},
        {14.0f, 14.0f},
        {48.0f, 0.0f},
        {56.0f, 8.0f},
        {0.98f, 0.86f, 0.32f, 1.0f},
    });

    // HUD 文本。
    auto append_text = [&instances](float x, float y, float scale, const std::string& text, const std::array<float, 4>& tint) {
        float cursor = x;
        const float glyph_w = 8.0f * scale;
        const float glyph_h = 8.0f * scale;
        for (char ch : text)
        {
            const std::optional<AtlasCell> cell = GlyphCellFor(ch);
            if (!cell.has_value())
            {
                cursor += glyph_w;
                continue;
            }

            instances.push_back({
                {cursor, y},
                {glyph_w, glyph_h},
                {static_cast<float>(cell->x * 8), static_cast<float>(cell->y * 8)},
                {static_cast<float>(cell->x * 8 + 8), static_cast<float>(cell->y * 8 + 8)},
                {tint[0], tint[1], tint[2], tint[3]},
            });
            cursor += glyph_w;
        }
    };

    append_text(44.0f, 28.0f, 2.0f, "SMOKE HOME BREW", {0.97f, 0.95f, 0.90f, 1.0f});
    append_text(44.0f, 52.0f, 2.0f, "TETRIS  PONG  FIRST FRAME", {0.79f, 0.90f, 1.0f, 1.0f});
    append_text(512.0f, 470.0f, 1.75f, "SMOKE HOME BREW", {0.98f, 0.92f, 0.64f, 1.0f});
    append_text(512.0f, 494.0f, 1.75f, "TETRIS  PONG", {0.88f, 0.95f, 0.82f, 1.0f});

    return instances;
}

MTL::Texture* CreateAtlasTexture(MTL::Device* device, const std::vector<Pixel>& atlas_pixels)
{
    MTL::TextureDescriptor* descriptor =
        MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatBGRA8Unorm, kAtlasSize, kAtlasSize, false);
    descriptor->setUsage(MTL::TextureUsageShaderRead);
    descriptor->setStorageMode(MTL::StorageModeShared);

    MTL::Texture* texture = device->newTexture(descriptor);
    if (texture == nullptr)
    {
        return nullptr;
    }

    texture->replaceRegion(MTL::Region::Make2D(0, 0, kAtlasSize, kAtlasSize), 0, atlas_pixels.data(),
                           kAtlasSize * sizeof(Pixel));
    return texture;
}

MTL::Texture* CreateColorTarget(MTL::Device* device)
{
    MTL::TextureDescriptor* descriptor =
        MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatBGRA8Unorm, kWidth, kHeight, false);
    descriptor->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
    descriptor->setStorageMode(MTL::StorageModeShared);
    return device->newTexture(descriptor);
}

template <typename T>
MTL::Buffer* MakeSharedBuffer(MTL::Device* device, const T* data, std::size_t count)
{
    const std::size_t bytes = sizeof(T) * count;
    MTL::Buffer* buffer = device->newBuffer(static_cast<NS::UInteger>(bytes), MTL::ResourceStorageModeShared);
    if (buffer == nullptr)
    {
        return nullptr;
    }

    std::memcpy(buffer->contents(), data, bytes);
    return buffer;
}

void WriteDiagnostics(const std::filesystem::path& out_dir,
                      const std::filesystem::path& image_path,
                      const std::vector<Pixel>& image_pixels,
                      const std::vector<Pixel>& atlas_pixels)
{
    std::filesystem::create_directories(out_dir);
    WritePpm(image_path, image_pixels, kWidth, kHeight);
    WritePpm(out_dir / "p4_6c_homebrew_atlas.ppm", atlas_pixels, kAtlasSize, kAtlasSize);

    const Pixel board_sample = image_pixels[(260 * kWidth) + 132];
    const Pixel court_sample = image_pixels[(260 * kWidth) + 708];
    const Pixel hud_sample = image_pixels[(42 * kWidth) + 180];
    const Pixel backdrop_sample = image_pixels[(20 * kWidth) + 12];

    std::ofstream log(out_dir / "p4_6c_render.log", std::ios::binary);
    log << "P4.6.8 轻量 2D homebrew smoke\n";
    log << "board=(" << static_cast<int>(board_sample.r) << ", " << static_cast<int>(board_sample.g) << ", "
        << static_cast<int>(board_sample.b) << ", " << static_cast<int>(board_sample.a) << ")\n";
    log << "court=(" << static_cast<int>(court_sample.r) << ", " << static_cast<int>(court_sample.g) << ", "
        << static_cast<int>(court_sample.b) << ", " << static_cast<int>(court_sample.a) << ")\n";
    log << "hud=(" << static_cast<int>(hud_sample.r) << ", " << static_cast<int>(hud_sample.g) << ", "
        << static_cast<int>(hud_sample.b) << ", " << static_cast<int>(hud_sample.a) << ")\n";
    log << "background=(" << static_cast<int>(backdrop_sample.r) << ", " << static_cast<int>(backdrop_sample.g) << ", "
        << static_cast<int>(backdrop_sample.b) << ", " << static_cast<int>(backdrop_sample.a) << ")\n";
}

std::vector<Pixel> RenderFrame(MTL::Device* device,
                               MTL::CommandQueue* command_queue,
                               MTL::RenderPipelineState* pipeline_state,
                               MTL::SamplerState* sampler_state,
                               MTL::Texture* atlas_texture,
                               const std::vector<QuadInstance>& instances)
{
    MTL::Buffer* instance_buffer = MakeSharedBuffer(device, instances.data(), instances.size());
    if (instance_buffer == nullptr)
    {
        return {};
    }

    const SceneData scene_data = {
        {static_cast<float>(kWidth), static_cast<float>(kHeight)},
        {static_cast<float>(kAtlasSize), static_cast<float>(kAtlasSize)},
        {0.0f, 0.0f, 0.0f, 0.0f},
    };
    MTL::Buffer* scene_buffer = MakeSharedBuffer(device, &scene_data, 1);
    if (scene_buffer == nullptr)
    {
        return {};
    }

    MTL::Texture* color_texture = CreateColorTarget(device);
    if (color_texture == nullptr)
    {
        return {};
    }

    MTL::RenderPassDescriptor* pass_descriptor = MTL::RenderPassDescriptor::alloc()->init();
    pass_descriptor->colorAttachments()->object(0)->setTexture(color_texture);
    pass_descriptor->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
    pass_descriptor->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
    pass_descriptor->colorAttachments()->object(0)->setClearColor(MTL::ClearColor(0.06, 0.08, 0.12, 1.0));

    MTL::CommandBuffer* command_buffer = command_queue->commandBuffer();
    MTL::RenderCommandEncoder* encoder = command_buffer->renderCommandEncoder(pass_descriptor);
    encoder->setRenderPipelineState(pipeline_state);
    encoder->setFragmentTexture(atlas_texture, 0);
    encoder->setFragmentSamplerState(sampler_state, 0);
    encoder->setVertexBuffer(instance_buffer, 0, 0);
    encoder->setVertexBuffer(scene_buffer, 0, 1);
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(6), NS::UInteger(instances.size()));
    encoder->endEncoding();
    command_buffer->commit();
    command_buffer->waitUntilCompleted();

    std::vector<Pixel> pixels(kWidth * kHeight);
    color_texture->getBytes(pixels.data(),
                            static_cast<NS::UInteger>(kWidth * sizeof(Pixel)),
                            MTL::Region::Make2D(0, 0, kWidth, kHeight),
                            0);
    return pixels;
}

} // namespace

int main()
{
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    MTL::Device* device = MTL::CreateSystemDefaultDevice();
    if (device == nullptr)
    {
        std::cerr << "无法创建 MTLDevice。\n";
        pool->drain();
        return 1;
    }

    MTL::CommandQueue* command_queue = device->newCommandQueue();
    if (command_queue == nullptr)
    {
        std::cerr << "无法创建 MTLCommandQueue。\n";
        pool->drain();
        return 1;
    }

    const std::vector<Pixel> atlas_pixels = BuildAtlas();
    MTL::Texture* atlas_texture = CreateAtlasTexture(device, atlas_pixels);
    if (atlas_texture == nullptr)
    {
        std::cerr << "无法创建 atlas 纹理。\n";
        pool->drain();
        return 1;
    }

    const char* shader_source = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct SceneData
{
    float2 screenSize;
    float2 atlasSize;
    float4 pad;
};

struct QuadInstance
{
    float2 position;
    float2 size;
    float2 uvMin;
    float2 uvMax;
    float4 tint;
};

struct VertexOut
{
    float4 position [[position]];
    float2 uv;
    float4 tint;
};

vertex VertexOut vertexMain(uint vertexID [[vertex_id]],
                            uint instanceID [[instance_id]],
                            const device QuadInstance* instances [[buffer(0)]],
                            constant SceneData& sceneData [[buffer(1)]])
{
    constexpr float2 cornerPosition[6] = {
        float2(0.0, 1.0),
        float2(1.0, 1.0),
        float2(0.0, 0.0),
        float2(0.0, 0.0),
        float2(1.0, 1.0),
        float2(1.0, 0.0),
    };

    const QuadInstance instance = instances[instanceID];
    const float2 local = instance.position + cornerPosition[vertexID] * instance.size;
    const float2 clip = float2((local.x / sceneData.screenSize.x) * 2.0 - 1.0,
                               1.0 - (local.y / sceneData.screenSize.y) * 2.0);

    VertexOut out;
    out.position = float4(clip, 0.0, 1.0);
    out.uv = mix(instance.uvMin, instance.uvMax, cornerPosition[vertexID]) / sceneData.atlasSize;
    out.tint = instance.tint;
    return out;
}

fragment float4 fragmentMain(VertexOut in [[stage_in]],
                             texture2d<float> atlas [[texture(0)]],
                             sampler atlasSampler [[sampler(0)]])
{
    return atlas.sample(atlasSampler, in.uv) * in.tint;
}
)MSL";

    NS::Error* error = nullptr;
    NS::String* source = NS::String::string(shader_source, NS::UTF8StringEncoding);
    MTL::Library* library = device->newLibrary(source, nullptr, &error);
    if (library == nullptr)
    {
        std::cerr << "手写 smoke MSL 编译失败: " << ErrorToString(error) << "\n";
        pool->drain();
        return 1;
    }

    MTL::Function* vertex_function = library->newFunction(MTLSTR("vertexMain"));
    MTL::Function* fragment_function = library->newFunction(MTLSTR("fragmentMain"));
    if (vertex_function == nullptr || fragment_function == nullptr)
    {
        std::cerr << "无法获取 smoke 着色器入口函数。\n";
        pool->drain();
        return 1;
    }

    MTL::RenderPipelineDescriptor* pipeline_descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    pipeline_descriptor->setVertexFunction(vertex_function);
    pipeline_descriptor->setFragmentFunction(fragment_function);
    pipeline_descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    pipeline_descriptor->colorAttachments()->object(0)->setBlendingEnabled(true);
    pipeline_descriptor->colorAttachments()->object(0)->setRgbBlendOperation(MTL::BlendOperationAdd);
    pipeline_descriptor->colorAttachments()->object(0)->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
    pipeline_descriptor->colorAttachments()->object(0)->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    pipeline_descriptor->colorAttachments()->object(0)->setAlphaBlendOperation(MTL::BlendOperationAdd);
    pipeline_descriptor->colorAttachments()->object(0)->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
    pipeline_descriptor->colorAttachments()->object(0)->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);

    MTL::RenderPipelineState* pipeline_state = device->newRenderPipelineState(pipeline_descriptor, &error);
    if (pipeline_state == nullptr)
    {
        std::cerr << "无法创建 smoke pipeline: " << ErrorToString(error) << "\n";
        pool->drain();
        return 1;
    }

    MTL::SamplerDescriptor* sampler_descriptor = MTL::SamplerDescriptor::alloc()->init();
    sampler_descriptor->setMinFilter(MTL::SamplerMinMagFilterNearest);
    sampler_descriptor->setMagFilter(MTL::SamplerMinMagFilterNearest);
    sampler_descriptor->setMipFilter(MTL::SamplerMipFilterNotMipmapped);
    sampler_descriptor->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    sampler_descriptor->setTAddressMode(MTL::SamplerAddressModeClampToEdge);

    MTL::SamplerState* sampler_state = device->newSamplerState(sampler_descriptor);
    if (sampler_state == nullptr)
    {
        std::cerr << "无法创建 smoke sampler。\n";
        pool->drain();
        return 1;
    }

    const std::vector<QuadInstance> instances = BuildSceneInstances();
    const std::vector<Pixel> frame = RenderFrame(device, command_queue, pipeline_state, sampler_state, atlas_texture, instances);
    if (frame.empty())
    {
        std::cerr << "渲染 smoke 首帧失败。\n";
        pool->drain();
        return 1;
    }

    const std::filesystem::path out_dir = "out";
    std::filesystem::create_directories(out_dir);
    const std::filesystem::path image_path = out_dir / "p4_6c_homebrew_smoke.ppm";
    WriteDiagnostics(out_dir, image_path, frame, atlas_pixels);

    std::cout << "P4.6.8 轻量 2D homebrew smoke 完成: " << image_path << "\n";
    std::cout << "atlas: out/p4_6c_homebrew_atlas.ppm\n";
    std::cout << "instances=" << instances.size() << "\n";

    pool->drain();
    return 0;
}
