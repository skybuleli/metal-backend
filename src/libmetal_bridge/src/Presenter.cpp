// Presenter.cpp — CAMetalLayer 呈现器：drawable 管理与简单交换链
//
// 这一层只负责把最终颜色纹理送进 CAMetalLayer：
//   1. 绑定设备与 layer
//   2. 维护 drawable 尺寸
//   3. 从 layer 获取 drawable
//   4. 通过 blit encoder 将源纹理拷贝到 drawable.texture
//   5. presentDrawable + commit + waitUntilCompleted
//
// 约束：
//   - 仅支持 2D、单采样、可直接映射到 CAMetalLayer 的颜色格式
//   - 不做缩放/裁剪/后处理，后续可由上层在进入 presenter 前完成

#include "metal_bridge.h"
#include "metal_internal.h"

#define CA_PRIVATE_IMPLEMENTATION
#include <QuartzCore/QuartzCore.hpp>

#include <new>

namespace
{
static MTL::PixelFormat to_layer_pixel_format(metal_pixel_format format)
{
    switch (format)
    {
    case METAL_PIXEL_FORMAT_BGRA8_UNORM:
        return MTL::PixelFormatBGRA8Unorm;
    case METAL_PIXEL_FORMAT_BGRA8_SRGB:
        return MTL::PixelFormatBGRA8Unorm_sRGB;
    case METAL_PIXEL_FORMAT_RGBA8_UNORM:
        return MTL::PixelFormatRGBA8Unorm;
    case METAL_PIXEL_FORMAT_RGBA8_SRGB:
        return MTL::PixelFormatRGBA8Unorm_sRGB;
    default:
        return MTL::PixelFormatInvalid;
    }
}

static bool is_layer_compatible_format(metal_pixel_format format)
{
    return to_layer_pixel_format(format) != MTL::PixelFormatInvalid;
}

static void configure_layer(metal_presenter* presenter, uint32_t width, uint32_t height)
{
    presenter->drawable_width = width;
    presenter->drawable_height = height;
    presenter->layer->setDrawableSize(
        CGSizeMake(static_cast<CGFloat>(width), static_cast<CGFloat>(height)));
}
} // namespace

metal_result metal_create_presenter(
    metal_device* device,
    void* metal_layer,
    metal_presenter** out_presenter)
{
    if (device == nullptr || metal_layer == nullptr || out_presenter == nullptr)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    if (device->device == nullptr)
    {
        return METAL_RESULT_RUNTIME_ERROR;
    }

    CA::MetalLayer* layer = static_cast<CA::MetalLayer*>(metal_layer);
    layer->retain();
    device->device->retain();

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    MTL::CommandQueue* command_queue = device->device->newCommandQueue();
    if (command_queue == nullptr)
    {
        layer->release();
        device->device->release();
        pool->release();
        return METAL_RESULT_RUNTIME_ERROR;
    }

    metal_presenter* presenter = new (std::nothrow) metal_presenter();
    if (presenter == nullptr)
    {
        command_queue->release();
        layer->release();
        device->device->release();
        pool->release();
        return METAL_RESULT_OUT_OF_MEMORY;
    }

    presenter->base.type = METAL_HANDLE_TYPE_PRESENTER;
    presenter->base.abi_version = METAL_BRIDGE_ABI_VERSION;
    presenter->device = device->device;
    presenter->command_queue = command_queue;
    presenter->layer = layer;
    presenter->drawable_width = 0;
    presenter->drawable_height = 0;
    presenter->pixel_format = METAL_PIXEL_FORMAT_BGRA8_UNORM;

    presenter->layer->setDevice(presenter->device);
    presenter->layer->setFramebufferOnly(false);
    presenter->layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

    *out_presenter = presenter;
    pool->release();
    return METAL_RESULT_OK;
}

metal_result metal_presenter_get_info(
    metal_presenter* presenter,
    metal_presenter_info* out_info)
{
    if (presenter == nullptr || out_info == nullptr)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    if (presenter->base.type != METAL_HANDLE_TYPE_PRESENTER)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    out_info->abi_version = METAL_BRIDGE_ABI_VERSION;
    out_info->drawable_width = presenter->drawable_width;
    out_info->drawable_height = presenter->drawable_height;
    out_info->pixel_format = presenter->pixel_format;
    out_info->reserved = 0;
    return METAL_RESULT_OK;
}

metal_result metal_presenter_resize(
    metal_presenter* presenter,
    uint32_t drawable_width,
    uint32_t drawable_height)
{
    if (presenter == nullptr)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    if (presenter->base.type != METAL_HANDLE_TYPE_PRESENTER)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    if (drawable_width == 0 || drawable_height == 0)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    if (presenter->layer == nullptr)
    {
        return METAL_RESULT_RUNTIME_ERROR;
    }

    configure_layer(presenter, drawable_width, drawable_height);
    return METAL_RESULT_OK;
}

metal_result metal_presenter_present_texture(
    metal_presenter* presenter,
    metal_texture* texture)
{
    if (presenter == nullptr || texture == nullptr)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    if (presenter->base.type != METAL_HANDLE_TYPE_PRESENTER ||
        texture->base.type != METAL_HANDLE_TYPE_TEXTURE)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    if (presenter->command_queue == nullptr ||
        presenter->layer == nullptr ||
        presenter->device == nullptr ||
        texture->texture == nullptr)
    {
        return METAL_RESULT_RUNTIME_ERROR;
    }

    if (texture->type != METAL_TEXTURE_TYPE_2D || texture->samples != 1u)
    {
        return METAL_RESULT_UNSUPPORTED;
    }

    const MTL::PixelFormat layer_pixel_format = to_layer_pixel_format(texture->pixel_format);
    if (layer_pixel_format == MTL::PixelFormatInvalid)
    {
        return METAL_RESULT_UNSUPPORTED;
    }

    if (presenter->drawable_width == 0 || presenter->drawable_height == 0)
    {
        configure_layer(presenter, texture->width, texture->height);
    }

    if (presenter->drawable_width != texture->width ||
        presenter->drawable_height != texture->height)
    {
        configure_layer(presenter, texture->width, texture->height);
    }

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    if (presenter->layer->pixelFormat() != layer_pixel_format)
    {
        presenter->layer->setPixelFormat(layer_pixel_format);
    }
    presenter->pixel_format = texture->pixel_format;

    CA::MetalDrawable* drawable = presenter->layer->nextDrawable();
    if (drawable == nullptr)
    {
        pool->release();
        return METAL_RESULT_RUNTIME_ERROR;
    }

    MTL::Texture* drawable_texture = drawable->texture();
    if (drawable_texture == nullptr)
    {
        pool->release();
        return METAL_RESULT_RUNTIME_ERROR;
    }

    MTL::CommandBuffer* command_buffer = presenter->command_queue->commandBuffer();
    if (command_buffer == nullptr)
    {
        pool->release();
        return METAL_RESULT_RUNTIME_ERROR;
    }

    MTL::BlitCommandEncoder* blit_encoder = command_buffer->blitCommandEncoder();
    if (blit_encoder == nullptr)
    {
        pool->release();
        return METAL_RESULT_RUNTIME_ERROR;
    }

    const MTL::Origin origin = MTL::Origin::Make(0, 0, 0);
    const MTL::Size size = MTL::Size::Make(texture->width, texture->height, 1);
    blit_encoder->copyFromTexture(
        texture->texture,
        0,
        0,
        origin,
        size,
        drawable_texture,
        0,
        0,
        origin);
    blit_encoder->endEncoding();

    command_buffer->presentDrawable(drawable);
    command_buffer->commit();
    command_buffer->waitUntilCompleted();

    pool->release();
    return METAL_RESULT_OK;
}
