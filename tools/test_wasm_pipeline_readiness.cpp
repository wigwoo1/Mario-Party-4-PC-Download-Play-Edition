#include "../extern/aurora/lib/gfx/pipeline_cache.hpp"
#include "../extern/aurora/lib/gfx/clear.hpp"
#include "../extern/aurora/lib/webgpu/browser_request.hpp"
#include <array>
#include <cassert>
#include <cstdio>

int main() {
    auto instance = wgpu::CreateInstance();
    wgpu::RequestAdapterOptions options{};
    auto adapter = aurora::webgpu::browser_request<wgpu::Adapter>([&](auto callback) {
        instance.RequestAdapter(&options, wgpu::CallbackMode::AllowSpontaneous, callback);
    });
    assert(adapter);
    wgpu::DeviceDescriptor deviceDescriptor{};
    auto device = aurora::webgpu::browser_request<wgpu::Device>([&](auto callback) {
        adapter.RequestDevice(&deviceDescriptor, wgpu::CallbackMode::AllowSpontaneous, callback);
    });
    assert(device);
    wgpu::ShaderSourceWGSL source{};
    source.code = R"(
        @vertex fn vs(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
            let p = array<vec2f, 3>(vec2f(-1,-1), vec2f(3,-1), vec2f(-1,3));
            return vec4f(p[i], 0, 1);
        }
        @fragment fn fs() -> @location(0) vec4f { return vec4f(1,1,1,1); }
    )";
    wgpu::ShaderModuleDescriptor shaderDescriptor{};
    shaderDescriptor.nextInChain = &source;
    auto shader = device.CreateShaderModule(&shaderDescriptor);
    unsigned created = 0;
    auto create = [&] {
        wgpu::ColorTargetState target{};
        target.format = wgpu::TextureFormat::RGBA8Unorm;
        wgpu::FragmentState fragment{};
        fragment.module = shader;
        fragment.entryPoint = "fs";
        fragment.targetCount = 1;
        fragment.targets = &target;
        wgpu::RenderPipelineDescriptor descriptor{};
        descriptor.vertex.module = shader;
        descriptor.vertex.entryPoint = "vs";
        descriptor.fragment = &fragment;
        ++created;
        return device.CreateRenderPipeline(&descriptor);
    };
    // Eight valid distinct keys exceed both release (5) and debug (1) budgets.
    // The callback uses a simple real GPU pipeline to isolate cache scheduling.
    std::array<aurora::gfx::PipelineRef, 8> refs{};
    aurora::gfx::begin_pipeline_frame();
    for (unsigned i = 0; i < refs.size(); ++i) {
        aurora::gfx::clear::PipelineConfig config{};
        config.clearColor = (i & 1) != 0;
        config.clearAlpha = (i & 2) != 0;
        config.clearDepth = (i & 4) != 0;
        refs[i] = aurora::gfx::find_pipeline(aurora::gfx::ShaderType::Clear, config, create);
    }
    aurora::gfx::end_pipeline_frame();
    unsigned unavailable = 0;
    for (const auto ref : refs) {
        wgpu::RenderPipeline pipeline;
        if (!aurora::gfx::get_pipeline(ref, pipeline)) ++unavailable;
    }
    const auto pendingAtDraw = aurora::gfx::pending_pipeline_count();
    aurora::gfx::begin_pipeline_frame();
    aurora::gfx::end_pipeline_frame();
    unsigned unavailableNextFrame = 0;
    for (const auto ref : refs) {
        wgpu::RenderPipeline pipeline;
        if (!aurora::gfx::get_pipeline(ref, pipeline)) ++unavailableNextFrame;
    }
    for (unsigned i = 0; i < refs.size(); ++i) {
        aurora::gfx::begin_pipeline_frame();
        aurora::gfx::end_pipeline_frame();
    }
    unsigned unavailableAfterDrain = 0;
    for (const auto ref : refs) {
        wgpu::RenderPipeline pipeline;
        if (!aurora::gfx::get_pipeline(ref, pipeline)) ++unavailableAfterDrain;
    }
    printf("PIPELINE_READINESS {\"requested\":8,\"created\":%u,\"unavailableAtDraw\":%u,\"pendingAtDraw\":%zu,\"unavailableNextFrame\":%u,\"unavailableAfterDrain\":%u}\n",
           created, unavailable, pendingAtDraw, unavailableNextFrame, unavailableAfterDrain);
    return 0;
}
