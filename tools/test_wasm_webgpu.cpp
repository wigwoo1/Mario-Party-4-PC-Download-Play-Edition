#include "../extern/aurora/lib/webgpu/browser_request.hpp"
#include <emscripten.h>
#include <cassert>
#include <cstdio>
#include <utility>
static wgpu::Adapter adapter;
static wgpu::Device device;
static bool ready;
static void wait_ready() {
    double deadline=emscripten_get_now()+15000;
    while(!ready && emscripten_get_now()<deadline) emscripten_sleep(10);
    assert(ready);
    ready=false;
}
int main() {
    auto instance=wgpu::CreateInstance();
    assert(instance);
    wgpu::RequestAdapterOptions options{};
    adapter=aurora::webgpu::browser_request<wgpu::Adapter>([&](auto callback) {
        instance.RequestAdapter(&options,wgpu::CallbackMode::AllowSpontaneous,callback);
    });
    assert(adapter);
    wgpu::DeviceDescriptor descriptor{};
    device=aurora::webgpu::browser_request<wgpu::Device>([&](auto callback) {
        adapter.RequestDevice(&descriptor,wgpu::CallbackMode::AllowSpontaneous,callback);
    });
    assert(device);
    auto queue=device.GetQueue(); assert(queue);
    wgpu::ShaderSourceWGSL wgsl{};
    wgsl.code=R"(
        @vertex fn vs(@builtin(vertex_index) i:u32)->@builtin(position) vec4f {
            var vertices=array<vec2f,3>(vec2f(-1,-1),vec2f(3,-1),vec2f(-1,3));
            return vec4f(vertices[i],0,1);
        }
        @fragment fn fs()->@location(0) vec4f { return vec4f(0,1,0,1); }
    )";
    wgpu::ShaderModuleDescriptor shader_desc{}; shader_desc.nextInChain=&wgsl;
    auto shader=device.CreateShaderModule(&shader_desc);
    wgpu::ColorTargetState target{}; target.format=wgpu::TextureFormat::RGBA8Unorm;
    wgpu::FragmentState fragment{}; fragment.module=shader; fragment.entryPoint="fs";
    fragment.targetCount=1; fragment.targets=&target;
    wgpu::RenderPipelineDescriptor pipeline_desc{};
    pipeline_desc.vertex.module=shader; pipeline_desc.vertex.entryPoint="vs";
    pipeline_desc.fragment=&fragment;
    auto pipeline=device.CreateRenderPipeline(&pipeline_desc); assert(pipeline);
    wgpu::TextureDescriptor texture_desc{};
    texture_desc.size={4,4,1};
    texture_desc.format=wgpu::TextureFormat::RGBA8Unorm;
    texture_desc.usage=wgpu::TextureUsage::RenderAttachment|wgpu::TextureUsage::CopySrc;
    auto texture=device.CreateTexture(&texture_desc);
    auto view=texture.CreateView();
    wgpu::BufferDescriptor buffer_desc{};
    buffer_desc.size=1024;
    buffer_desc.usage=wgpu::BufferUsage::MapRead|wgpu::BufferUsage::CopyDst;
    auto buffer=device.CreateBuffer(&buffer_desc);
    auto encoder=device.CreateCommandEncoder();
    wgpu::RenderPassColorAttachment color{};
    color.view=view;
    color.loadOp=wgpu::LoadOp::Clear;
    color.storeOp=wgpu::StoreOp::Store;
    color.clearValue={1,0,1,1};
    wgpu::RenderPassDescriptor pass_desc{};
    pass_desc.colorAttachmentCount=1;
    pass_desc.colorAttachments=&color;
    auto pass=encoder.BeginRenderPass(&pass_desc);
    pass.SetPipeline(pipeline);
    pass.SetScissorRect(0,0,2,4);
    pass.Draw(3);
    pass.End();
    wgpu::TexelCopyTextureInfo source{}; source.texture=texture;
    wgpu::TexelCopyBufferInfo destination{}; destination.buffer=buffer;
    destination.layout.bytesPerRow=256;
    destination.layout.rowsPerImage=4;
    wgpu::Extent3D extent{4,4,1};
    encoder.CopyTextureToBuffer(&source,&destination,&extent);
    auto commands=encoder.Finish();
    queue.Submit(1,&commands);
    static bool mapped;
    buffer.MapAsync(wgpu::MapMode::Read,0,1024,wgpu::CallbackMode::AllowSpontaneous,
        [](wgpu::MapAsyncStatus status,wgpu::StringView) {
            mapped=status==wgpu::MapAsyncStatus::Success; ready=true;
        });
    wait_ready(); assert(mapped);
    auto bytes=static_cast<const unsigned char*>(buffer.GetConstMappedRange(0,1024));
    assert(bytes);
    for(int y=0;y<4;++y) for(int x=0;x<4;++x) {
        auto pixel=bytes+y*256+x*4;
        if(x<2) assert(pixel[0]==0 && pixel[1]==255 && pixel[2]==0 && pixel[3]==255);
        else assert(pixel[0]==255 && pixel[1]==0 && pixel[2]==255 && pixel[3]==255);
    }
    buffer.Unmap(); buffer.Destroy(); texture.Destroy();
    puts("WebGPU render passed: 16 exact RGBA pixels after render-pass clear, submission and GPU readback.");
    puts("WebGPU shader passed: WGSL triangle draws eight green pixels; scissor preserves eight magenta pixels.");
    wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector canvas_source;
    canvas_source.selector="#canvas";
    wgpu::SurfaceDescriptor surface_desc{}; surface_desc.nextInChain=&canvas_source;
    auto surface=instance.CreateSurface(&surface_desc); assert(surface);
    wgpu::SurfaceCapabilities caps;
    assert(surface.GetCapabilities(adapter,&caps)==wgpu::Status::Success);
    assert(caps.formatCount>0);
    wgpu::SurfaceConfiguration config{};
    config.device=device; config.format=caps.formats[0];
    assert(config.format==wgpu::TextureFormat::RGBA8Unorm || config.format==wgpu::TextureFormat::BGRA8Unorm);
    config.width=640; config.height=480;
    config.usage=wgpu::TextureUsage::RenderAttachment|wgpu::TextureUsage::CopySrc;
    config.alphaMode=wgpu::CompositeAlphaMode::Opaque;
    config.presentMode=wgpu::PresentMode::Fifo;
    surface.Configure(&config);
    wgpu::SurfaceTexture frame;
    surface.GetCurrentTexture(&frame); assert(frame.texture);
    color.view=frame.texture.CreateView(); color.clearValue={0,1,0,1};
    encoder=device.CreateCommandEncoder();
    pass=encoder.BeginRenderPass(&pass_desc); pass.End();
    buffer=device.CreateBuffer(&buffer_desc);
    source.texture=frame.texture; destination.buffer=buffer;
    encoder.CopyTextureToBuffer(&source,&destination,&extent);
    commands=encoder.Finish(); queue.Submit(1,&commands);
    mapped=false;
    buffer.MapAsync(wgpu::MapMode::Read,0,1024,wgpu::CallbackMode::AllowSpontaneous,
        [](wgpu::MapAsyncStatus status,wgpu::StringView) {
            mapped=status==wgpu::MapAsyncStatus::Success; ready=true;
        });
    wait_ready(); assert(mapped);
    bytes=static_cast<const unsigned char*>(buffer.GetConstMappedRange(0,1024));
    assert(bytes);
    for(int y=0;y<4;++y) for(int x=0;x<4;++x) {
        auto pixel=bytes+y*256+x*4;
        assert(pixel[0]==0 && pixel[1]==255 && pixel[2]==0 && pixel[3]==255);
    }
    buffer.Unmap(); buffer.Destroy(); surface.Unconfigure();
    puts("WebGPU surface passed: canvas frame acquired, rendered and 16 pixels verified.");
    puts("WebGPU probe passed: asynchronous adapter, device and queue through Dawn C++ bindings.");
    device.Destroy(); device=nullptr; adapter=nullptr;
    return 0;
}

