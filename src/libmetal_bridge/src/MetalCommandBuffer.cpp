// MetalCommandBuffer.cpp — 最小命令缓冲区 / RenderCommandEncoder / Draw 链路（P4.3.6）
//
// 当前实现只为打通 Draw/DrawIndexed 的真实 Metal 调用路径：
// queue -> command buffer -> render encoder -> draw -> commit/wait。
// 由于 SetRenderTargets 尚未实现，这里内部创建一个临时 1x1 BGRA8Unorm 颜色附件。

#include "metal_bridge.h"
#include "metal_internal.h"

#include <new>

static MTL::PrimitiveType to_mtl_primitive_type(metal_primitive_type primitive_type)
{
    switch (primitive_type)
    {
    case METAL_PRIMITIVE_TYPE_POINT:
        return MTL::PrimitiveTypePoint;
    case METAL_PRIMITIVE_TYPE_LINE:
        return MTL::PrimitiveTypeLine;
    case METAL_PRIMITIVE_TYPE_LINE_STRIP:
        return MTL::PrimitiveTypeLineStrip;
    case METAL_PRIMITIVE_TYPE_TRIANGLE_STRIP:
        return MTL::PrimitiveTypeTriangleStrip;
    case METAL_PRIMITIVE_TYPE_TRIANGLE:
    default:
        return MTL::PrimitiveTypeTriangle;
    }
}

static MTL::IndexType to_mtl_index_type(metal_index_type index_type)
{
    switch (index_type)
    {
    case METAL_INDEX_TYPE_UINT32:
        return MTL::IndexTypeUInt32;
    case METAL_INDEX_TYPE_UINT16:
    default:
        return MTL::IndexTypeUInt16;
    }
}

static MTL::Texture* create_temporary_color_target(MTL::Device* device)
{
    if (device == nullptr)
    {
        return nullptr;
    }

    MTL::TextureDescriptor* texture_desc = MTL::TextureDescriptor::alloc()->init();
    texture_desc->setTextureType(MTL::TextureType2D);
    texture_desc->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    texture_desc->setWidth(1);
    texture_desc->setHeight(1);
    texture_desc->setDepth(1);
    texture_desc->setMipmapLevelCount(1);
    texture_desc->setSampleCount(1);
    texture_desc->setStorageMode(MTL::StorageModePrivate);
    texture_desc->setUsage(MTL::TextureUsageRenderTarget);

    MTL::Texture* texture = device->newTexture(texture_desc);
    texture_desc->release();
    return texture;
}

metal_result metal_begin_command_buffer(
    metal_queue* queue,
    metal_command_buffer** out_command_buffer)
{
    if (queue == nullptr || out_command_buffer == nullptr || queue->queue == nullptr)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    MTL::CommandBuffer* command_buffer = queue->queue->commandBuffer();

    if (command_buffer == nullptr)
    {
        pool->release();
        return METAL_RESULT_RUNTIME_ERROR;
    }

    metal_command_buffer* handle = new (std::nothrow) metal_command_buffer();
    if (handle == nullptr)
    {
        pool->release();
        return METAL_RESULT_OUT_OF_MEMORY;
    }

    command_buffer->retain();

    handle->base.type = METAL_HANDLE_TYPE_COMMAND_BUFFER;
    handle->base.abi_version = METAL_BRIDGE_ABI_VERSION;
    handle->command_buffer = command_buffer;

    *out_command_buffer = handle;
    pool->release();
    return METAL_RESULT_OK;
}

metal_result metal_begin_render_encoding(
    metal_command_buffer* command_buffer,
    metal_render_pipeline* pipeline,
    metal_render_encoder** out_render_encoder)
{
    if (command_buffer == nullptr ||
        pipeline == nullptr ||
        out_render_encoder == nullptr ||
        command_buffer->command_buffer == nullptr ||
        pipeline->pipeline_state == nullptr)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    MTL::Device* device = pipeline->pipeline_state->device();
    MTL::Texture* color_target = create_temporary_color_target(device);

    if (color_target == nullptr)
    {
        pool->release();
        return METAL_RESULT_RUNTIME_ERROR;
    }

    MTL::RenderPassDescriptor* pass_desc = MTL::RenderPassDescriptor::alloc()->init();
    MTL::RenderPassColorAttachmentDescriptor* color_attachment =
        pass_desc->colorAttachments()->object(0);
    color_attachment->setTexture(color_target);
    color_attachment->setLoadAction(MTL::LoadActionDontCare);
    color_attachment->setStoreAction(MTL::StoreActionDontCare);

    MTL::RenderCommandEncoder* encoder =
        command_buffer->command_buffer->renderCommandEncoder(pass_desc);
    pass_desc->release();

    if (encoder == nullptr)
    {
        color_target->release();
        pool->release();
        return METAL_RESULT_RUNTIME_ERROR;
    }

    encoder->setRenderPipelineState(pipeline->pipeline_state);

    metal_render_encoder* handle = new (std::nothrow) metal_render_encoder();
    if (handle == nullptr)
    {
        encoder->endEncoding();
        encoder->release();
        color_target->release();
        pool->release();
        return METAL_RESULT_OUT_OF_MEMORY;
    }

    handle->base.type = METAL_HANDLE_TYPE_RENDER_ENCODER;
    handle->base.abi_version = METAL_BRIDGE_ABI_VERSION;
    handle->owner = command_buffer;
    handle->encoder = encoder;
    handle->color_target = color_target;

    *out_render_encoder = handle;
    pool->release();
    return METAL_RESULT_OK;
}

metal_result metal_render_encoder_set_vertex_buffer(
    metal_render_encoder* encoder,
    uint32_t index,
    metal_buffer* buffer,
    uint64_t offset)
{
    if (encoder == nullptr || encoder->encoder == nullptr || buffer == nullptr || buffer->buffer == nullptr)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    encoder->encoder->setVertexBuffer(buffer->buffer, offset, index);
    return METAL_RESULT_OK;
}

metal_result metal_render_encoder_set_fragment_buffer(
    metal_render_encoder* encoder,
    uint32_t index,
    metal_buffer* buffer,
    uint64_t offset)
{
    if (encoder == nullptr || encoder->encoder == nullptr || buffer == nullptr || buffer->buffer == nullptr)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    encoder->encoder->setFragmentBuffer(buffer->buffer, offset, index);
    return METAL_RESULT_OK;
}

metal_result metal_render_encoder_set_fragment_texture(
    metal_render_encoder* encoder,
    uint32_t index,
    metal_texture* texture)
{
    if (encoder == nullptr || encoder->encoder == nullptr || texture == nullptr || texture->texture == nullptr)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    encoder->encoder->setFragmentTexture(texture->texture, index);
    return METAL_RESULT_OK;
}

metal_result metal_render_encoder_set_fragment_sampler(
    metal_render_encoder* encoder,
    uint32_t index,
    metal_sampler* sampler)
{
    if (encoder == nullptr || encoder->encoder == nullptr || sampler == nullptr || sampler->sampler_state == nullptr)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    encoder->encoder->setFragmentSamplerState(sampler->sampler_state, index);
    return METAL_RESULT_OK;
}

metal_result metal_render_encoder_draw_primitives(
    metal_render_encoder* encoder,
    metal_primitive_type primitive_type,
    uint32_t vertex_start,
    uint32_t vertex_count,
    uint32_t instance_count,
    uint32_t base_instance)
{
    if (encoder == nullptr || encoder->encoder == nullptr || vertex_count == 0 || instance_count == 0)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    encoder->encoder->drawPrimitives(
        to_mtl_primitive_type(primitive_type),
        static_cast<NS::UInteger>(vertex_start),
        static_cast<NS::UInteger>(vertex_count),
        static_cast<NS::UInteger>(instance_count),
        static_cast<NS::UInteger>(base_instance));
    return METAL_RESULT_OK;
}

metal_result metal_render_encoder_draw_indexed_primitives(
    metal_render_encoder* encoder,
    metal_primitive_type primitive_type,
    uint32_t index_count,
    metal_index_type index_type,
    metal_buffer* index_buffer,
    uint64_t index_buffer_offset,
    uint32_t instance_count,
    int32_t base_vertex,
    uint32_t base_instance)
{
    if (encoder == nullptr ||
        encoder->encoder == nullptr ||
        index_buffer == nullptr ||
        index_buffer->buffer == nullptr ||
        index_count == 0 ||
        instance_count == 0)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    encoder->encoder->drawIndexedPrimitives(
        to_mtl_primitive_type(primitive_type),
        static_cast<NS::UInteger>(index_count),
        to_mtl_index_type(index_type),
        index_buffer->buffer,
        index_buffer_offset,
        static_cast<NS::UInteger>(instance_count),
        base_vertex,
        static_cast<NS::UInteger>(base_instance));
    return METAL_RESULT_OK;
}

metal_result metal_end_render_encoding(
    metal_render_encoder* encoder)
{
    if (encoder == nullptr || encoder->encoder == nullptr)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    encoder->encoder->endEncoding();
    return METAL_RESULT_OK;
}

metal_result metal_commit_command_buffer(
    metal_command_buffer* command_buffer)
{
    if (command_buffer == nullptr || command_buffer->command_buffer == nullptr)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    command_buffer->command_buffer->commit();
    return METAL_RESULT_OK;
}

metal_result metal_wait_command_buffer(
    metal_command_buffer* command_buffer)
{
    if (command_buffer == nullptr || command_buffer->command_buffer == nullptr)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    command_buffer->command_buffer->waitUntilCompleted();
    return METAL_RESULT_OK;
}
