# Metal API 核心模式速查（Phase 4+）

> metal-cpp 关键 API 模式。实现 libmetal_bridge 时直接引用。

## 设备与队列

```cpp
NS::SharedPtr<MTL::Device> device = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
NS::SharedPtr<MTL::CommandQueue> queue = NS::TransferPtr(device->newCommandQueue());
```

## 缓冲区（M1 UMA 架构）

```cpp
// M1 是 UMA：Shared 和 Managed 行为相同；GPU-only 用 Private 性能最优
NS::SharedPtr<MTL::Buffer> buf = NS::TransferPtr(
    device->newBuffer(data, size, MTL::ResourceStorageModeShared)
);
```

## 纹理

```cpp
MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
desc->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
desc->setWidth(512); desc->setHeight(512);
desc->setStorageMode(MTL::StorageModePrivate);
desc->setUsage(MTL::TextureUsageShaderRead | MTL::TextureUsageRenderTarget);
NS::SharedPtr<MTL::Texture> tex = NS::TransferPtr(device->newTexture(desc));
```

## 从 metallib 加载着色器（Path A 输出）

```cpp
NS::SharedPtr<MTL::Library> lib = NS::TransferPtr(
    device->newLibrary(dispatch_data_create(metallibData, metallibSize, ...), &error)
);
NS::SharedPtr<MTL::Function> func = NS::TransferPtr(
    lib->newFunction(NS::String::string("vertexMain", NS::UTF8StringEncoding))
);
```

## 渲染管线与绘制

```cpp
MTL::RenderPipelineDescriptor* desc = MTL::RenderPipelineDescriptor::alloc()->init();
desc->setVertexFunction(vertexFunc);
desc->setFragmentFunction(fragmentFunc);
desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
NS::SharedPtr<MTL::RenderPipelineState> pso = NS::TransferPtr(
    device->newRenderPipelineState(desc, &error)
);

encoder->setRenderPipelineState(pso.get());
encoder->setVertexBuffer(vb.get(), 0, 0);
encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
encoder->endEncoding();
cmd->commit();
```

## 关键注意事项

- M1 UMA 架构：Shared 和 Managed 实际行为相同
- metal-cpp 用 `NS::SharedPtr` 管理生命周期
- `alloc()->init()` 对象需手动 release 或包装 SharedPtr
- 着色器加载失败时检查 `&error` 参数
