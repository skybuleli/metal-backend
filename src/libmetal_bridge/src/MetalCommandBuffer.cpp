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

// ════════════════════════════════════════════════════════════════════
// P4.4.2 — SharedEvent 同步原语
// 依据 metal-cpp:
//   - MTL::Device::newSharedEvent()
//   - MTL::CommandBuffer::encodeSignalEvent(event, value)
//   - MTL::SharedEvent::signaledValue()
// ════════════════════════════════════════════════════════════════════

metal_result metal_create_shared_event(
    metal_device* device,
    metal_shared_event** out_event)
{
    if (device == nullptr || out_event == nullptr || device->device == nullptr)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    MTL::SharedEvent* shared_event = device->device->newSharedEvent();
    if (shared_event == nullptr)
    {
        pool->release();
        return METAL_RESULT_RUNTIME_ERROR;
    }

    metal_shared_event* handle = new (std::nothrow) metal_shared_event();
    if (handle == nullptr)
    {
        shared_event->release();
        pool->release();
        return METAL_RESULT_OUT_OF_MEMORY;
    }

    handle->base.type = METAL_HANDLE_TYPE_SHARED_EVENT;
    handle->base.abi_version = METAL_BRIDGE_ABI_VERSION;
    handle->event = shared_event;

    *out_event = handle;
    pool->release();
    return METAL_RESULT_OK;
}

metal_result metal_encode_signal_shared_event(
    metal_command_buffer* command_buffer,
    metal_shared_event* event,
    uint64_t value)
{
    if (command_buffer == nullptr ||
        command_buffer->command_buffer == nullptr ||
        event == nullptr ||
        event->event == nullptr)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    command_buffer->command_buffer->encodeSignalEvent(event->event, value);
    return METAL_RESULT_OK;
}

metal_result metal_get_shared_event_signaled_value(
    metal_shared_event* event,
    uint64_t* out_value)
{
    if (event == nullptr || event->event == nullptr || out_value == nullptr)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    *out_value = event->event->signaledValue();
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
    handle->encoding_ended = false;
    handle->color_targets[0] = color_target;
    handle->color_target_count = 1;
    for (uint32_t i = 1; i < 8; i++)
        handle->color_targets[i] = nullptr;
    handle->depth_stencil_target = nullptr;

    *out_render_encoder = handle;
    pool->release();
    return METAL_RESULT_OK;
}

// ════════════════════════════════════════════════════════════════════
// P4.3.7 — SetRenderTargets: MTLRenderPassDescriptor
// ════════════════════════════════════════════════════════════════════

metal_result metal_begin_render_encoding_with_targets(
    metal_command_buffer* command_buffer,
    metal_render_pipeline* pipeline,
    const metal_color_attachment_descriptor* color_attachments,
    uint32_t color_attachment_count,
    const metal_depth_stencil_attachment_descriptor* depth_stencil,
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

    // 校验颜色附件数
    if (color_attachment_count > 8)
        return METAL_RESULT_INVALID_ARGUMENT;

    // color_attachments 可以为 nullptr（当 count==0 时），但 count>0 时必须提供
    if (color_attachment_count > 0 && color_attachments == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    MTL::RenderPassDescriptor* pass_desc = MTL::RenderPassDescriptor::alloc()->init();

    // ── 配置颜色附件 ──
    for (uint32_t i = 0; i < color_attachment_count; i++)
    {
        const metal_color_attachment_descriptor& src = color_attachments[i];

        if (src.texture == nullptr || src.texture->texture == nullptr)
        {
            // 跳过空的颜色附件槽位
            continue;
        }

        MTL::RenderPassColorAttachmentDescriptor* attachment =
            pass_desc->colorAttachments()->object(i);
        attachment->setTexture(src.texture->texture);
        attachment->setLevel(src.level);
        attachment->setSlice(src.slice);

        // 加载动作
        switch (src.load_action)
        {
        case METAL_LOAD_ACTION_DONT_CARE:
            attachment->setLoadAction(MTL::LoadActionDontCare);
            break;
        case METAL_LOAD_ACTION_LOAD:
            attachment->setLoadAction(MTL::LoadActionLoad);
            break;
        case METAL_LOAD_ACTION_CLEAR:
            attachment->setLoadAction(MTL::LoadActionClear);
            attachment->setClearColor(MTL::ClearColor(
                src.clear_color.red,
                src.clear_color.green,
                src.clear_color.blue,
                src.clear_color.alpha));
            break;
        }

        // 存储动作
        switch (src.store_action)
        {
        case METAL_STORE_ACTION_DONT_CARE:
            attachment->setStoreAction(MTL::StoreActionDontCare);
            break;
        case METAL_STORE_ACTION_STORE:
            attachment->setStoreAction(MTL::StoreActionStore);
            break;
        case METAL_STORE_ACTION_MULTISAMPLE_RESOLVE:
            attachment->setStoreAction(MTL::StoreActionMultisampleResolve);
            break;
        }
    }

    // ── 配置深度/模板附件 ──
    if (depth_stencil != nullptr && depth_stencil->texture != nullptr &&
        depth_stencil->texture->texture != nullptr)
    {
        const metal_depth_stencil_attachment_descriptor& ds = *depth_stencil;

        // 判断是否为纯深度格式（不含模板）
        metal_pixel_format_info fmt_info = metal_pixel_format_get_info(ds.texture->pixel_format);
        bool has_stencil = !fmt_info.is_depth;
        // 如果 is_depth 为 true 但格式名包含 "Stencil" 或 "S8"，也有模板
        // 安全做法：检查像素格式本身
        if (ds.texture->pixel_format == METAL_PIXEL_FORMAT_D24_UNORM_S8_UINT ||
            ds.texture->pixel_format == METAL_PIXEL_FORMAT_D32_FLOAT_S8_UINT)
        {
            has_stencil = true;
        }

        // 深度附件
        MTL::RenderPassDepthAttachmentDescriptor* depth_attach =
            pass_desc->depthAttachment();
        depth_attach->setTexture(ds.texture->texture);
        depth_attach->setLevel(ds.level);
        depth_attach->setSlice(ds.slice);

        switch (ds.depth_load_action)
        {
        case METAL_LOAD_ACTION_DONT_CARE:
            depth_attach->setLoadAction(MTL::LoadActionDontCare);
            break;
        case METAL_LOAD_ACTION_LOAD:
            depth_attach->setLoadAction(MTL::LoadActionLoad);
            break;
        case METAL_LOAD_ACTION_CLEAR:
            depth_attach->setLoadAction(MTL::LoadActionClear);
            depth_attach->setClearDepth(ds.clear_value.depth);
            break;
        }

        switch (ds.depth_store_action)
        {
        case METAL_STORE_ACTION_DONT_CARE:
            depth_attach->setStoreAction(MTL::StoreActionDontCare);
            break;
        case METAL_STORE_ACTION_STORE:
            depth_attach->setStoreAction(MTL::StoreActionStore);
            break;
        default:
            depth_attach->setStoreAction(MTL::StoreActionDontCare);
            break;
        }

        // 模板附件（仅当纹理包含模板分量时）
        if (has_stencil)
        {
            MTL::RenderPassStencilAttachmentDescriptor* stencil_attach =
                pass_desc->stencilAttachment();
            stencil_attach->setTexture(ds.texture->texture);
            stencil_attach->setLevel(ds.level);
            stencil_attach->setSlice(ds.slice);

            switch (ds.stencil_load_action)
            {
            case METAL_LOAD_ACTION_DONT_CARE:
                stencil_attach->setLoadAction(MTL::LoadActionDontCare);
                break;
            case METAL_LOAD_ACTION_LOAD:
                stencil_attach->setLoadAction(MTL::LoadActionLoad);
                break;
            case METAL_LOAD_ACTION_CLEAR:
                stencil_attach->setLoadAction(MTL::LoadActionClear);
                stencil_attach->setClearStencil(ds.clear_value.stencil);
                break;
            }

            switch (ds.stencil_store_action)
            {
            case METAL_STORE_ACTION_DONT_CARE:
                stencil_attach->setStoreAction(MTL::StoreActionDontCare);
                break;
            case METAL_STORE_ACTION_STORE:
                stencil_attach->setStoreAction(MTL::StoreActionStore);
                break;
            default:
                stencil_attach->setStoreAction(MTL::StoreActionDontCare);
                break;
            }
        }
    }

    // ── 创建编码器 ──
    MTL::RenderCommandEncoder* encoder =
        command_buffer->command_buffer->renderCommandEncoder(pass_desc);
    pass_desc->release();

    if (encoder == nullptr)
    {
        pool->release();
        return METAL_RESULT_RUNTIME_ERROR;
    }

    encoder->setRenderPipelineState(pipeline->pipeline_state);

    metal_render_encoder* handle = new (std::nothrow) metal_render_encoder();
    if (handle == nullptr)
    {
        encoder->endEncoding();
        encoder->release();
        pool->release();
        return METAL_RESULT_OUT_OF_MEMORY;
    }

    handle->base.type = METAL_HANDLE_TYPE_RENDER_ENCODER;
    handle->base.abi_version = METAL_BRIDGE_ABI_VERSION;
    handle->owner = command_buffer;
    handle->encoder = encoder;
    handle->encoding_ended = false;
    handle->color_target_count = color_attachment_count;

    // 保留颜色附件纹理引用
    for (uint32_t i = 0; i < color_attachment_count && i < 8; i++)
    {
        if (color_attachments[i].texture != nullptr &&
            color_attachments[i].texture->texture != nullptr)
        {
            handle->color_targets[i] = color_attachments[i].texture->texture;
            handle->color_targets[i]->retain();
        }
        else
        {
            handle->color_targets[i] = nullptr;
        }
    }
    for (uint32_t i = color_attachment_count; i < 8; i++)
        handle->color_targets[i] = nullptr;

    // 保留深度/模板纹理引用
    if (depth_stencil != nullptr && depth_stencil->texture != nullptr &&
        depth_stencil->texture->texture != nullptr)
    {
        handle->depth_stencil_target = depth_stencil->texture->texture;
        handle->depth_stencil_target->retain();
    }
    else
    {
        handle->depth_stencil_target = nullptr;
    }

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
    encoder->encoding_ended = true;
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
