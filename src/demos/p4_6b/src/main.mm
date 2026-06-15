#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

#include <algorithm>
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
constexpr std::uint32_t kWidth = 512;
constexpr std::uint32_t kHeight = 512;
constexpr std::uint32_t kTileSize = 32;
constexpr std::uint32_t kMapWidth = 24;
constexpr std::uint32_t kMapHeight = 24;
constexpr std::uint32_t kAtlasCells = 8;
constexpr std::uint32_t kAtlasSize = kAtlasCells * 8;
constexpr std::uint32_t kGlyphWidth = 8;
constexpr std::uint32_t kGlyphHeight = 8;

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
    float scrollPixels[2];
    float atlasSize[2];
    float pad[2];
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
        case '+': return AtlasCell{6, 7};
        case '.': return AtlasCell{5, 7};
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
        case 'A': return AtlasCell{0, 1};
        case 'C': return AtlasCell{1, 1};
        case 'D': return AtlasCell{2, 1};
        case 'E': return AtlasCell{3, 1};
        case 'H': return AtlasCell{4, 1};
        case 'I': return AtlasCell{5, 1};
        case 'L': return AtlasCell{6, 1};
        case 'M': return AtlasCell{7, 1};
        case 'N': return AtlasCell{0, 2};
        case 'O': return AtlasCell{1, 2};
        case 'P': return AtlasCell{2, 2};
        case 'R': return AtlasCell{3, 2};
        case 'S': return AtlasCell{4, 2};
        case 'T': return AtlasCell{5, 2};
        case 'U': return AtlasCell{6, 2};
        case 'X': return AtlasCell{7, 2};
        default: return std::nullopt;
    }
}

std::vector<Pixel> BuildAtlas()
{
    std::vector<Pixel> atlas(kAtlasSize * kAtlasSize, Pixel{0, 0, 0, 0});

    auto paint_world_tile = [](std::uint32_t cell_x, std::uint32_t cell_y, auto painter) {
        for (std::uint32_t y = 0; y < 8; ++y)
        {
            for (std::uint32_t x = 0; x < 8; ++x)
            {
                painter(cell_x * 8 + x, cell_y * 8 + y, x, y);
            }
        }
    };

    // 世界 tile：草地、泥土、石块、水面、路径、告示牌、面板光带、纯白。
    paint_world_tile(0, 0, [&atlas](std::uint32_t px, std::uint32_t py, std::uint32_t x, std::uint32_t y) {
        const bool checker = ((x + y) % 2) == 0;
        const Pixel pixel = checker ? Pixel{72, 148, 72, 255} : Pixel{52, 112, 58, 255};
        SetAtlasPixel(atlas, px, py, pixel);
    });
    paint_world_tile(1, 0, [&atlas](std::uint32_t px, std::uint32_t py, std::uint32_t x, [[maybe_unused]] std::uint32_t y) {
        const bool band = ((x / 2) % 2) == 0;
        const Pixel pixel = band ? Pixel{76, 92, 156, 255} : Pixel{44, 56, 120, 255};
        SetAtlasPixel(atlas, px, py, pixel);
    });
    paint_world_tile(2, 0, [&atlas](std::uint32_t px, std::uint32_t py, std::uint32_t x, std::uint32_t y) {
        const bool brick = ((x % 4) < 2) ^ ((y % 4) < 2);
        const Pixel pixel = brick ? Pixel{132, 116, 100, 255} : Pixel{92, 80, 72, 255};
        SetAtlasPixel(atlas, px, py, pixel);
    });
    paint_world_tile(3, 0, [&atlas](std::uint32_t px, std::uint32_t py, std::uint32_t x, std::uint32_t y) {
        const float dx = static_cast<float>(x) / 7.0f;
        const float dy = static_cast<float>(y) / 7.0f;
        const float wave = std::sin((dx + dy) * 5.0f);
        const std::uint8_t crest = static_cast<std::uint8_t>(160 + 28 * wave);
        SetAtlasPixel(atlas, px, py, Pixel{crest, static_cast<std::uint8_t>(120 + 18 * wave), static_cast<std::uint8_t>(72 + 12 * wave), 255});
    });
    paint_world_tile(4, 0, [&atlas](std::uint32_t px, std::uint32_t py, [[maybe_unused]] std::uint32_t x, std::uint32_t y) {
        const bool stripe = ((y / 2) % 2) == 0;
        const Pixel pixel = stripe ? Pixel{110, 150, 92, 255} : Pixel{92, 122, 70, 255};
        SetAtlasPixel(atlas, px, py, pixel);
    });
    paint_world_tile(5, 0, [&atlas](std::uint32_t px, std::uint32_t py, std::uint32_t x, std::uint32_t y) {
        const bool board = (x == 3 || x == 4 || y >= 5);
        const Pixel pixel = board ? Pixel{210, 188, 128, 255} : Pixel{130, 94, 54, 255};
        SetAtlasPixel(atlas, px, py, pixel);
    });
    paint_world_tile(6, 0, [&atlas](std::uint32_t px, std::uint32_t py, std::uint32_t x, std::uint32_t y) {
        const bool glow = x < 2 || y < 2 || x > 5 || y > 5;
        const Pixel pixel = glow ? Pixel{240, 208, 104, 255} : Pixel{255, 242, 160, 255};
        SetAtlasPixel(atlas, px, py, pixel);
    });
    FillAtlasCell(atlas, 7, 0, Pixel{255, 255, 255, 255});

    const std::array<GlyphDef, 22> glyphs = {{
        {'A', {0, 1}, {{".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}}},
        {'C', {1, 1}, {{".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."}}},
        {'D', {2, 1}, {{"####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."}}},
        {'E', {3, 1}, {{"#####", "#....", "#....", "####.", "#....", "#....", "#####"}}},
        {'H', {4, 1}, {{"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}}},
        {'I', {5, 1}, {{"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "#####"}}},
        {'L', {6, 1}, {{"#....", "#....", "#....", "#....", "#....", "#....", "#####"}}},
        {'M', {7, 1}, {{"#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"}}},
        {'N', {0, 2}, {{"#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"}}},
        {'O', {1, 2}, {{".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}}},
        {'P', {2, 2}, {{"####.", "#...#", "#...#", "####.", "#....", "#....", "#...."}}},
        {'R', {3, 2}, {{"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"}}},
        {'S', {4, 2}, {{".###.", "#...#", "#....", ".###.", "....#", "#...#", ".###."}}},
        {'T', {5, 2}, {{"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."}}},
        {'U', {6, 2}, {{"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}}},
        {'X', {7, 2}, {{"#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"}}},
        {'0', {0, 5}, {{".###.", "#..##", "#.#.#", "##..#", "#...#", "#...#", ".###."}}},
        {'1', {1, 5}, {{"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."}}},
        {'2', {2, 5}, {{".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"}}},
        {'3', {3, 5}, {{"####.", "....#", "...#.", "..##.", "....#", "#...#", ".###."}}},
        {'4', {4, 5}, {{"...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."}}},
        {'5', {5, 5}, {{"#####", "#....", "####.", "....#", "....#", "#...#", ".###."}}},
    }};

    for (const GlyphDef& glyph : glyphs)
    {
        DrawGlyph(atlas, glyph, Pixel{248, 244, 236, 255});
    }

    return atlas;
}

AtlasCell TileCellFor(std::uint32_t tile_id)
{
    switch (tile_id % 6)
    {
        case 0: return AtlasCell{0, 0};
        case 1: return AtlasCell{1, 0};
        case 2: return AtlasCell{2, 0};
        case 3: return AtlasCell{3, 0};
        case 4: return AtlasCell{4, 0};
        default: return AtlasCell{5, 0};
    }
}

std::uint32_t TileIdAt(std::uint32_t x, std::uint32_t y)
{
    if (x == 0 || y == 0 || x + 1 == kMapWidth || y + 1 == kMapHeight)
    {
        return 2;
    }

    if (x == y || x + 1 == y || y + 1 == x)
    {
        return 3;
    }

    if ((x + 2 * y) % 7 == 0)
    {
        return 1;
    }

    if ((x / 3 + y / 2) % 3 == 0)
    {
        return 0;
    }

    if ((x + y) % 5 == 0)
    {
        return 4;
    }

    return 5;
}

std::vector<QuadInstance> BuildWorldInstances()
{
    std::vector<QuadInstance> instances;
    instances.reserve(kMapWidth * kMapHeight);

    for (std::uint32_t y = 0; y < kMapHeight; ++y)
    {
        for (std::uint32_t x = 0; x < kMapWidth; ++x)
        {
            const AtlasCell cell = TileCellFor(TileIdAt(x, y));
            const float px = static_cast<float>(x * kTileSize);
            const float py = static_cast<float>(y * kTileSize);
            const float uv_x = static_cast<float>(cell.x * 8);
            const float uv_y = static_cast<float>(cell.y * 8);

            QuadInstance instance = {};
            instance.position[0] = px;
            instance.position[1] = py;
            instance.size[0] = static_cast<float>(kTileSize);
            instance.size[1] = static_cast<float>(kTileSize);
            instance.uvMin[0] = uv_x;
            instance.uvMin[1] = uv_y;
            instance.uvMax[0] = uv_x + 8.0f;
            instance.uvMax[1] = uv_y + 8.0f;
            instance.tint[0] = 1.0f;
            instance.tint[1] = 1.0f;
            instance.tint[2] = 1.0f;
            instance.tint[3] = 1.0f;
            instances.push_back(instance);
        }
    }

    return instances;
}

void AppendText(std::vector<QuadInstance>& instances,
                float start_x,
                float start_y,
                float scale,
                const std::string& text,
                const std::array<float, 4>& tint)
{
    float cursor_x = start_x;
    const float glyph_size = static_cast<float>(kGlyphWidth) * scale;
    for (char ch : text)
    {
        const std::optional<AtlasCell> cell = GlyphCellFor(ch);
        if (!cell.has_value())
        {
            cursor_x += glyph_size;
            continue;
        }

        QuadInstance instance = {};
        instance.position[0] = cursor_x;
        instance.position[1] = start_y;
        instance.size[0] = glyph_size;
        instance.size[1] = static_cast<float>(kGlyphHeight) * scale;
        instance.uvMin[0] = static_cast<float>(cell->x * 8);
        instance.uvMin[1] = static_cast<float>(cell->y * 8);
        instance.uvMax[0] = instance.uvMin[0] + 8.0f;
        instance.uvMax[1] = instance.uvMin[1] + 8.0f;
        instance.tint[0] = tint[0];
        instance.tint[1] = tint[1];
        instance.tint[2] = tint[2];
        instance.tint[3] = tint[3];
        instances.push_back(instance);
        cursor_x += glyph_size;
    }
}

std::vector<QuadInstance> BuildHudInstances()
{
    std::vector<QuadInstance> instances;
    instances.reserve(48);

    // 半透明底板。
    QuadInstance panel = {};
    panel.position[0] = 16.0f;
    panel.position[1] = 376.0f;
    panel.size[0] = 480.0f;
    panel.size[1] = 120.0f;
    panel.uvMin[0] = 56.0f;
    panel.uvMin[1] = 0.0f;
    panel.uvMax[0] = 64.0f;
    panel.uvMax[1] = 8.0f;
    panel.tint[0] = 18.0f / 255.0f;
    panel.tint[1] = 24.0f / 255.0f;
    panel.tint[2] = 42.0f / 255.0f;
    panel.tint[3] = 0.84f;
    instances.push_back(panel);

    QuadInstance stripe = panel;
    stripe.position[0] = 16.0f;
    stripe.position[1] = 376.0f;
    stripe.size[1] = 8.0f;
    stripe.tint[0] = 242.0f / 255.0f;
    stripe.tint[1] = 200.0f / 255.0f;
    stripe.tint[2] = 104.0f / 255.0f;
    stripe.tint[3] = 0.90f;
    instances.push_back(stripe);

    AppendText(instances,
               32.0f,
               392.0f,
               2.0f,
               "TILEMAP HUD",
               {248.0f / 255.0f, 244.0f / 255.0f, 236.0f / 255.0f, 1.0f});
    AppendText(instances,
               32.0f,
               420.0f,
               2.0f,
               "SCROLL +1.5 +0.5",
               {180.0f / 255.0f, 228.0f / 255.0f, 255.0f / 255.0f, 1.0f});
    AppendText(instances,
               32.0f,
               448.0f,
               2.0f,
               "MAP 24 X 24",
               {246.0f / 255.0f, 214.0f / 255.0f, 156.0f / 255.0f, 1.0f});

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
                      const std::filesystem::path& image_a_path,
                      const std::filesystem::path& image_b_path,
                      const std::vector<Pixel>& image_a,
                      const std::vector<Pixel>& image_b,
                      const std::vector<Pixel>& atlas_pixels)
{
    std::filesystem::create_directories(out_dir);
    WritePpm(image_a_path, image_a, kWidth, kHeight);
    WritePpm(image_b_path, image_b, kWidth, kHeight);
    WritePpm(out_dir / "p4_6b_tilemap_atlas.ppm", atlas_pixels, kAtlasSize, kAtlasSize);

    const Pixel center_a = image_a[(kHeight / 2) * kWidth + (kWidth / 2)];
    const Pixel center_b = image_b[(kHeight / 2) * kWidth + (kWidth / 2)];
    const Pixel panel_pixel = image_a[(400 * kWidth) + 40];
    const Pixel text_pixel = image_a[(428 * kWidth) + 90];

    std::ofstream log(out_dir / "p4_6b_render.log", std::ios::binary);
    log << "P4.6.7 手写 2D 样本 B\n";
    log << "world=" << kMapWidth << "x" << kMapHeight << ", tile=" << kTileSize << "x" << kTileSize << "\n";
    log << "camera_a=(72.0, 48.0), camera_b=(120.0, 88.0)\n";
    log << "frame_a_center=(" << static_cast<int>(center_a.r) << ", " << static_cast<int>(center_a.g) << ", "
        << static_cast<int>(center_a.b) << ", " << static_cast<int>(center_a.a) << ")\n";
    log << "frame_b_center=(" << static_cast<int>(center_b.r) << ", " << static_cast<int>(center_b.g) << ", "
        << static_cast<int>(center_b.b) << ", " << static_cast<int>(center_b.a) << ")\n";
    log << "hud_panel_pixel=(" << static_cast<int>(panel_pixel.r) << ", " << static_cast<int>(panel_pixel.g) << ", "
        << static_cast<int>(panel_pixel.b) << ", " << static_cast<int>(panel_pixel.a) << ")\n";
    log << "hud_text_pixel=(" << static_cast<int>(text_pixel.r) << ", " << static_cast<int>(text_pixel.g) << ", "
        << static_cast<int>(text_pixel.b) << ", " << static_cast<int>(text_pixel.a) << ")\n";
}

std::vector<Pixel> RenderFrame(MTL::Device* device,
                               MTL::CommandQueue* command_queue,
                               MTL::RenderPipelineState* pipeline_state,
                               MTL::SamplerState* sampler_state,
                               MTL::Texture* atlas_texture,
                               const std::vector<QuadInstance>& world_instances,
                               const std::vector<QuadInstance>& hud_instances,
                               float scroll_x,
                               float scroll_y)
{
    MTL::Buffer* world_buffer = MakeSharedBuffer(device, world_instances.data(), world_instances.size());
    MTL::Buffer* hud_buffer = MakeSharedBuffer(device, hud_instances.data(), hud_instances.size());
    if (world_buffer == nullptr || hud_buffer == nullptr)
    {
        return {};
    }

    const SceneData world_scene = {
        {static_cast<float>(kWidth), static_cast<float>(kHeight)},
        {scroll_x, scroll_y},
        {static_cast<float>(kAtlasSize), static_cast<float>(kAtlasSize)},
        {0.0f, 0.0f},
    };
    const SceneData hud_scene = {
        {static_cast<float>(kWidth), static_cast<float>(kHeight)},
        {0.0f, 0.0f},
        {static_cast<float>(kAtlasSize), static_cast<float>(kAtlasSize)},
        {0.0f, 0.0f},
    };

    MTL::Buffer* world_scene_buffer = MakeSharedBuffer(device, &world_scene, 1);
    MTL::Buffer* hud_scene_buffer = MakeSharedBuffer(device, &hud_scene, 1);
    if (world_scene_buffer == nullptr || hud_scene_buffer == nullptr)
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
    pass_descriptor->colorAttachments()->object(0)->setClearColor(MTL::ClearColor(0.08, 0.10, 0.14, 1.0));

    MTL::CommandBuffer* command_buffer = command_queue->commandBuffer();
    MTL::RenderCommandEncoder* encoder = command_buffer->renderCommandEncoder(pass_descriptor);
    encoder->setRenderPipelineState(pipeline_state);
    encoder->setFragmentTexture(atlas_texture, 0);
    encoder->setFragmentSamplerState(sampler_state, 0);

    encoder->setVertexBuffer(world_buffer, 0, 0);
    encoder->setVertexBuffer(world_scene_buffer, 0, 1);
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(6), NS::UInteger(world_instances.size()));

    encoder->setVertexBuffer(hud_buffer, 0, 0);
    encoder->setVertexBuffer(hud_scene_buffer, 0, 1);
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(6), NS::UInteger(hud_instances.size()));

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
        std::cerr << "无法创建 tilemap atlas 纹理。\n";
        pool->drain();
        return 1;
    }

    const char* shader_source = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct SceneData
{
    float2 screenSize;
    float2 scrollPixels;
    float2 atlasSize;
    float2 pad;
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
    const float2 local = instance.position + cornerPosition[vertexID] * instance.size - sceneData.scrollPixels;
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
        std::cerr << "手写 tilemap MSL 编译失败: " << ErrorToString(error) << "\n";
        pool->drain();
        return 1;
    }

    MTL::Function* vertex_function = library->newFunction(MTLSTR("vertexMain"));
    MTL::Function* fragment_function = library->newFunction(MTLSTR("fragmentMain"));
    if (vertex_function == nullptr || fragment_function == nullptr)
    {
        std::cerr << "无法获取 tilemap 着色器入口函数。\n";
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
        std::cerr << "无法创建 tilemap RenderPipelineState: " << ErrorToString(error) << "\n";
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
        std::cerr << "无法创建 tilemap sampler。\n";
        pool->drain();
        return 1;
    }

    const std::vector<QuadInstance> world_instances = BuildWorldInstances();
    const std::vector<QuadInstance> hud_instances = BuildHudInstances();

    const std::vector<Pixel> frame_a = RenderFrame(device, command_queue, pipeline_state, sampler_state, atlas_texture,
                                                   world_instances, hud_instances, 72.0f, 48.0f);
    const std::vector<Pixel> frame_b = RenderFrame(device, command_queue, pipeline_state, sampler_state, atlas_texture,
                                                   world_instances, hud_instances, 120.0f, 88.0f);
    if (frame_a.empty() || frame_b.empty())
    {
        std::cerr << "渲染 tilemap/HUD 失败。\n";
        pool->drain();
        return 1;
    }

    const std::filesystem::path out_dir = "out";
    std::filesystem::create_directories(out_dir);
    const std::filesystem::path frame_a_path = out_dir / "p4_6b_tilemap_scroll_a.ppm";
    const std::filesystem::path frame_b_path = out_dir / "p4_6b_tilemap_scroll_b.ppm";
    WriteDiagnostics(out_dir, frame_a_path, frame_b_path, frame_a, frame_b, atlas_pixels);

    std::cout << "P4.6.7 手写 2D 样本完成:\n";
    std::cout << "  frame_a: " << frame_a_path << "\n";
    std::cout << "  frame_b: " << frame_b_path << "\n";
    std::cout << "  atlas:   out/p4_6b_tilemap_atlas.ppm\n";
    std::cout << "  camera_a=(72.0, 48.0), camera_b=(120.0, 88.0)\n";
    std::cout << "  world tiles=" << world_instances.size() << ", hud quads=" << hud_instances.size() << "\n";

    pool->drain();
    return 0;
}
