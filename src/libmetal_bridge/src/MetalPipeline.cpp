// MetalPipeline.cpp — 渲染管线状态：MTLRenderPipelineState 创建（P4.3.1）
//
// 职责：从 metallib 数据和基本像素格式创建 MTLRenderPipelineState。
// 更复杂的管线状态（blend/depth/stencil/vertex descriptor）在后续任务中逐步扩展。

#include "metal_bridge.h"
#include "metal_internal.h"

#include <cstring>
#include <cstdio>
#include <new>
#include <vector>

static MTL::VertexDescriptor* create_vertex_descriptor(
    const metal_render_pipeline_descriptor* descriptor)
{
    if (descriptor == nullptr)
        return nullptr;

    MTL::VertexDescriptor* vertexDesc = MTL::VertexDescriptor::alloc()->init();

    uint32_t attrCount = descriptor->vertex_attribute_count;
    if (attrCount > METAL_MAX_VERTEX_ATTRIBUTES)
    {
        attrCount = METAL_MAX_VERTEX_ATTRIBUTES;
    }

    for (uint32_t i = 0; i < attrCount; i++)
    {
        const metal_vertex_attribute_descriptor& src = descriptor->vertex_attributes[i];

        if (src.attribute_index >= METAL_MAX_VERTEX_ATTRIBUTES ||
            src.buffer_index >= METAL_MAX_VERTEX_BUFFER_BINDINGS ||
            src.format == METAL_VERTEX_FORMAT_INVALID)
        {
            continue;
        }

        MTL::VertexAttributeDescriptor* attr =
            vertexDesc->attributes()->object(src.attribute_index);
        attr->setBufferIndex(src.buffer_index);
        attr->setOffset(src.offset);
        attr->setFormat(static_cast<MTL::VertexFormat>(src.format));
    }

    uint32_t layoutCount = descriptor->vertex_buffer_layout_count;
    if (layoutCount > METAL_MAX_VERTEX_BUFFER_BINDINGS)
    {
        layoutCount = METAL_MAX_VERTEX_BUFFER_BINDINGS;
    }

    for (uint32_t i = 0; i < layoutCount; i++)
    {
        const metal_vertex_buffer_layout_descriptor& src =
            descriptor->vertex_buffer_layouts[i];

        if (src.buffer_index >= METAL_MAX_VERTEX_BUFFER_BINDINGS)
        {
            continue;
        }

        MTL::VertexBufferLayoutDescriptor* layout =
            vertexDesc->layouts()->object(src.buffer_index);
        layout->setStride(src.stride);
        layout->setStepRate(src.step_rate);
        layout->setStepFunction(static_cast<MTL::VertexStepFunction>(src.step_function));
    }

    return vertexDesc;
}

// ════════════════════════════════════════════════════════════════════
// 内部辅助函数：从 metallib 二进制数据创建 MTL::Library
// ════════════════════════════════════════════════════════════════════

/// 从 metallib 二进制数据创建 MTL::Library
/// @return 创建的 library 指针，失败返回 nullptr
static MTL::Library* create_library_from_metallib(
    MTL::Device* device,
    const void* data,
    size_t size)
{
    if (device == nullptr || data == nullptr || size == 0)
        return nullptr;

    // 使用 dispatch_data_t 包装 metallib 数据
    dispatch_data_t dispatchData = dispatch_data_create(
        data, size, dispatch_get_main_queue(), ^{ /* 不释放 data — 调用方管理 */ });

    NS::Error* error = nullptr;
    MTL::Library* library = device->newLibrary(dispatchData, &error);

    // dispatch_data_t 在 ARC 环境下自动管理，在非 ARC 下需要 release
    // 这里显式 release 以避免泄漏
    dispatch_release(dispatchData);

    if (library == nullptr)
    {
        // 错误已在 newLibrary 中记录到 error
        (void)error; // 后续可扩展错误消息
        return nullptr;
    }

    return library;
}

// ════════════════════════════════════════════════════════════════════
// metal_create_render_pipeline（P4.3.1）
// ════════════════════════════════════════════════════════════════════

metal_result metal_create_render_pipeline(
    metal_device* device,
    const metal_render_pipeline_descriptor* descriptor,
    metal_render_pipeline** out_pipeline)
{
    if (device == nullptr || descriptor == nullptr || out_pipeline == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (device->device == nullptr)
        return METAL_RESULT_RUNTIME_ERROR;

    // 校验 ABI 版本
    if (descriptor->abi_version != METAL_BRIDGE_ABI_VERSION)
        return METAL_RESULT_INVALID_ARGUMENT;

    // 校验必须的数据
    if (descriptor->vertex_metallib_data == nullptr ||
        descriptor->vertex_metallib_size == 0)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    MTL::PipelineOption options = MTL::PipelineOptionNone;

    // ── 步骤 1：从 metallib 数据创建 MTL::Library ──
    MTL::Library* vertexLibrary = create_library_from_metallib(
        device->device,
        descriptor->vertex_metallib_data,
        descriptor->vertex_metallib_size);

    if (vertexLibrary == nullptr)
    {
        pool->release();
        return METAL_RESULT_COMPILE_FAILED;
    }

    MTL::Library* fragmentLibrary = nullptr;
    if (descriptor->fragment_metallib_data != nullptr &&
        descriptor->fragment_metallib_size > 0)
    {
        fragmentLibrary = create_library_from_metallib(
            device->device,
            descriptor->fragment_metallib_data,
            descriptor->fragment_metallib_size);

        if (fragmentLibrary == nullptr)
        {
            vertexLibrary->release();
            pool->release();
            return METAL_RESULT_COMPILE_FAILED;
        }
    }

    // ── 步骤 2：获取入口函数 ──
    const char* vertexFuncName = descriptor->vertex_function
        ? descriptor->vertex_function : "main";
    NS::String* vertexName = NS::String::string(vertexFuncName, NS::UTF8StringEncoding);
    MTL::Function* vertexFunction = vertexLibrary->newFunction(vertexName);

    if (vertexFunction == nullptr)
    {
        vertexLibrary->release();
        if (fragmentLibrary) fragmentLibrary->release();
        pool->release();
        return METAL_RESULT_COMPILE_FAILED;
    }

    MTL::Function* fragmentFunction = nullptr;
    if (fragmentLibrary)
    {
        const char* fragFuncName = descriptor->fragment_function
            ? descriptor->fragment_function : "main";
        NS::String* fragmentName = NS::String::string(fragFuncName, NS::UTF8StringEncoding);
        fragmentFunction = fragmentLibrary->newFunction(fragmentName);

        if (fragmentFunction == nullptr)
        {
            vertexFunction->release();
            vertexLibrary->release();
            fragmentLibrary->release();
            pool->release();
            return METAL_RESULT_COMPILE_FAILED;
        }
    }

    // ── 步骤 3：创建渲染管线描述符 ──
    MTL::RenderPipelineDescriptor* rpDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    rpDesc->setVertexFunction(vertexFunction);
    if (fragmentFunction)
    {
        rpDesc->setFragmentFunction(fragmentFunction);
    }

    MTL::VertexDescriptor* vertexDesc = create_vertex_descriptor(descriptor);
    if (vertexDesc != nullptr)
    {
        rpDesc->setVertexDescriptor(vertexDesc);
    }

    // 设置颜色附件格式
    MTL::PixelFormat colorFormat = MTL::PixelFormatBGRA8Unorm; // 默认
    if (descriptor->color_attachment_format != METAL_PIXEL_FORMAT_INVALID)
    {
        // 转换 metal_pixel_format 到 MTL::PixelFormat
        // TODO: P4.1.3 的格式映射表已有此转换，后续可复用
        colorFormat = static_cast<MTL::PixelFormat>(
            static_cast<uint32_t>(descriptor->color_attachment_format) +
            static_cast<uint32_t>(MTL::PixelFormatInvalid) - 1);
        // 注意：这个映射假设两种枚举值偏移一致，后续需用正式映射表
    }
    rpDesc->colorAttachments()->object(0)->setPixelFormat(colorFormat);

    // 应用混合状态（P4.3.9）
    if (descriptor->blend_attachments != nullptr && descriptor->blend_attachment_count > 0)
    {
        uint32_t blendCount = descriptor->blend_attachment_count;
        if (blendCount > METAL_MAX_COLOR_ATTACHMENTS)
        {
            blendCount = METAL_MAX_COLOR_ATTACHMENTS;
        }

        for (uint32_t i = 0; i < blendCount; i++)
        {
            const metal_blend_attachment_descriptor& blend = descriptor->blend_attachments[i];
            MTL::RenderPipelineColorAttachmentDescriptor* colorAtt = rpDesc->colorAttachments()->object(i);

            // 设置像素格式（与 colorAttachment(0) 一致）
            colorAtt->setPixelFormat(colorFormat);
            colorAtt->setBlendingEnabled(blend.blending_enabled != 0);

            if (blend.blending_enabled)
            {
                colorAtt->setSourceRGBBlendFactor(
                    static_cast<MTL::BlendFactor>(blend.src_rgb_factor));
                colorAtt->setDestinationRGBBlendFactor(
                    static_cast<MTL::BlendFactor>(blend.dst_rgb_factor));
                colorAtt->setRgbBlendOperation(
                    static_cast<MTL::BlendOperation>(blend.rgb_operation));
                colorAtt->setSourceAlphaBlendFactor(
                    static_cast<MTL::BlendFactor>(blend.src_alpha_factor));
                colorAtt->setDestinationAlphaBlendFactor(
                    static_cast<MTL::BlendFactor>(blend.dst_alpha_factor));
                colorAtt->setAlphaBlendOperation(
                    static_cast<MTL::BlendOperation>(blend.alpha_operation));
            }

            colorAtt->setWriteMask(blend.write_mask);
        }
    }

    // 设置深度模板格式
    if (descriptor->depth_stencil_format != METAL_PIXEL_FORMAT_INVALID)
    {
        MTL::PixelFormat dsFormat = static_cast<MTL::PixelFormat>(
            static_cast<uint32_t>(descriptor->depth_stencil_format) +
            static_cast<uint32_t>(MTL::PixelFormatInvalid) - 1);
        rpDesc->setDepthAttachmentPixelFormat(dsFormat);
    }

    // ── 步骤 4：创建管线状态 ──
    NS::Error* error = nullptr;
    // 同步 API：4 参数 (descriptor, options, reflection, error)
    // reflection 传 nullptr 表示不需要反射信息
    MTL::RenderPipelineState* pipelineState =
        device->device->newRenderPipelineState(rpDesc, options, nullptr, &error);

    // 释放中间对象
    vertexFunction->release();
    if (fragmentFunction) fragmentFunction->release();
    vertexLibrary->release();
    if (fragmentLibrary) fragmentLibrary->release();
    if (vertexDesc) vertexDesc->release();
    rpDesc->release();

    if (pipelineState == nullptr)
    {
        pool->release();
        return METAL_RESULT_COMPILE_FAILED;
    }

    // ── 步骤 5：创建 metal_render_pipeline 句柄 ──
    metal_render_pipeline* pipeline = new (std::nothrow) metal_render_pipeline();
    if (pipeline == nullptr)
    {
        pipelineState->release();
        pool->release();
        return METAL_RESULT_OUT_OF_MEMORY;
    }

    pipeline->base.type = METAL_HANDLE_TYPE_RENDER_PIPELINE;
    pipeline->base.abi_version = METAL_BRIDGE_ABI_VERSION;
    pipeline->pipeline_state = pipelineState;

    pool->release();
    *out_pipeline = pipeline;
    return METAL_RESULT_OK;
}

// ════════════════════════════════════════════════════════════════
// 深度/模板状态（P4.3.10）
// ════════════════════════════════════════════════════════════════

/// 内部辅助：将 metal_stencil_descriptor 转换为 MTL::StencilDescriptor
static void apply_stencil_descriptor(
    MTL::StencilDescriptor* dst,
    const metal_stencil_descriptor& src)
{
    dst->setStencilCompareFunction(
        static_cast<MTL::CompareFunction>(src.compare_function));
    dst->setStencilFailureOperation(
        static_cast<MTL::StencilOperation>(src.stencil_failure));
    dst->setDepthFailureOperation(
        static_cast<MTL::StencilOperation>(src.depth_failure));
    dst->setDepthStencilPassOperation(
        static_cast<MTL::StencilOperation>(src.depth_stencil_pass));
    dst->setReadMask(src.read_mask);
    dst->setWriteMask(src.write_mask);
}

metal_result metal_create_depth_stencil_state(
    metal_device* device,
    const metal_depth_stencil_descriptor* descriptor,
    metal_depth_stencil_state** out_state)
{
    if (device == nullptr || descriptor == nullptr || out_state == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (device->device == nullptr)
        return METAL_RESULT_RUNTIME_ERROR;

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    MTL::DepthStencilDescriptor* dsDesc = MTL::DepthStencilDescriptor::alloc()->init();

    // 深度比较函数
    dsDesc->setDepthCompareFunction(
        static_cast<MTL::CompareFunction>(descriptor->depth_compare_function));
    dsDesc->setDepthWriteEnabled(descriptor->depth_write_enabled != 0);

    // 模板状态
    if (descriptor->stencil_enabled)
    {
        MTL::StencilDescriptor* frontStencil = MTL::StencilDescriptor::alloc()->init();
        apply_stencil_descriptor(frontStencil, descriptor->front_face);
        dsDesc->setFrontFaceStencil(frontStencil);

        MTL::StencilDescriptor* backStencil = MTL::StencilDescriptor::alloc()->init();
        apply_stencil_descriptor(backStencil, descriptor->back_face);
        dsDesc->setBackFaceStencil(backStencil);
    }

    MTL::DepthStencilState* dsState = device->device->newDepthStencilState(dsDesc);

    // 释放描述符（状态对象已创建）
    dsDesc->release();

    if (dsState == nullptr)
    {
        pool->release();
        return METAL_RESULT_RUNTIME_ERROR;
    }

    metal_depth_stencil_state* state = new (std::nothrow) metal_depth_stencil_state();
    if (state == nullptr)
    {
        dsState->release();
        pool->release();
        return METAL_RESULT_OUT_OF_MEMORY;
    }

    state->base.type = METAL_HANDLE_TYPE_DEPTH_STENCIL_STATE;
    state->base.abi_version = METAL_BRIDGE_ABI_VERSION;
    state->depth_stencil_state = dsState;

    pool->release();
    *out_state = state;
    return METAL_RESULT_OK;
}

metal_result metal_render_encoder_set_depth_stencil_state(
    metal_render_encoder* encoder,
    metal_depth_stencil_state* state)
{
    if (encoder == nullptr || state == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (encoder->encoder == nullptr || state->depth_stencil_state == nullptr)
        return METAL_RESULT_RUNTIME_ERROR;

    encoder->encoder->setDepthStencilState(state->depth_stencil_state);
    return METAL_RESULT_OK;
}

metal_result metal_render_encoder_set_stencil_reference_value(
    metal_render_encoder* encoder,
    uint32_t front_value,
    uint32_t back_value)
{
    if (encoder == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (encoder->encoder == nullptr)
        return METAL_RESULT_RUNTIME_ERROR;

    // metal-cpp 使用 setStencilReferenceValues 同时设置正反面引用值
    encoder->encoder->setStencilReferenceValues(front_value, back_value);
    return METAL_RESULT_OK;
}

// ════════════════════════════════════════════════════════════════
// 视口与裁剪矩形（P4.3.11）
// ════════════════════════════════════════════════════════════════

metal_result metal_render_encoder_set_viewports(
    metal_render_encoder* encoder,
    const metal_viewport* viewports,
    uint32_t count)
{
    if (encoder == nullptr || viewports == nullptr || count == 0)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (encoder->encoder == nullptr)
        return METAL_RESULT_RUNTIME_ERROR;

    // 转换为 MTL::Viewport 数组
    // MTLViewport: originX, originY, width, height, znear, zfar (均为 double)
    if (count == 1)
    {
        MTL::Viewport vp;
        vp.originX = viewports[0].origin_x;
        vp.originY = viewports[0].origin_y;
        vp.width = viewports[0].width;
        vp.height = viewports[0].height;
        vp.znear = viewports[0].znear;
        vp.zfar = viewports[0].zfar;
        encoder->encoder->setViewport(vp);
    }
    else
    {
        // 多视口：构建 MTL::Viewport 数组并通过 setViewports(count, array) 设置
        std::vector<MTL::Viewport> vpArray(count);
        for (uint32_t i = 0; i < count; i++)
        {
            vpArray[i].originX = viewports[i].origin_x;
            vpArray[i].originY = viewports[i].origin_y;
            vpArray[i].width = viewports[i].width;
            vpArray[i].height = viewports[i].height;
            vpArray[i].znear = viewports[i].znear;
            vpArray[i].zfar = viewports[i].zfar;
        }
        encoder->encoder->setViewports(vpArray.data(), count);
    }
    return METAL_RESULT_OK;
}

metal_result metal_render_encoder_set_scissor_rects(
    metal_render_encoder* encoder,
    const metal_scissor_rect* rects,
    uint32_t count)
{
    if (encoder == nullptr || rects == nullptr || count == 0)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (encoder->encoder == nullptr)
        return METAL_RESULT_RUNTIME_ERROR;

    // MTLScissorRect: x, y, width, height (均为 NSUInteger)
    if (count == 1)
    {
        MTL::ScissorRect sr;
        sr.x = rects[0].x;
        sr.y = rects[0].y;
        sr.width = rects[0].width;
        sr.height = rects[0].height;
        encoder->encoder->setScissorRect(sr);
    }
    else
    {
        std::vector<MTL::ScissorRect> srArray(count);
        for (uint32_t i = 0; i < count; i++)
        {
            srArray[i].x = rects[i].x;
            srArray[i].y = rects[i].y;
            srArray[i].width = rects[i].width;
            srArray[i].height = rects[i].height;
        }
        encoder->encoder->setScissorRects(srArray.data(), count);
    }
    return METAL_RESULT_OK;
}
