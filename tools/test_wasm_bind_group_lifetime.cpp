#include "../extern/aurora/lib/gfx/common.hpp"
#include "../extern/aurora/lib/webgpu/gpu.hpp"
#include "../extern/aurora/lib/webgpu/browser_request.hpp"
#include <emscripten.h>
#include <cassert>
#include <cstdio>
#include <unordered_set>

int main() {
    auto instance = wgpu::CreateInstance();
    wgpu::RequestAdapterOptions options{};
    auto adapter = aurora::webgpu::browser_request<wgpu::Adapter>([&](auto callback) {
        instance.RequestAdapter(&options, wgpu::CallbackMode::AllowSpontaneous, callback);
    });
    assert(adapter);
    wgpu::DeviceDescriptor descriptor{};
    auto device = aurora::webgpu::browser_request<wgpu::Device>([&](auto callback) {
        adapter.RequestDevice(&descriptor, wgpu::CallbackMode::AllowSpontaneous, callback);
    });
    assert(device);
    aurora::webgpu::g_device = device;
    wgpu::BindGroupLayoutEntry layoutEntry{};
    layoutEntry.binding = 0;
    layoutEntry.visibility = wgpu::ShaderStage::Fragment;
    layoutEntry.texture.sampleType = wgpu::TextureSampleType::Float;
    layoutEntry.texture.viewDimension = wgpu::TextureViewDimension::e2D;
    wgpu::BindGroupLayoutDescriptor layoutDescriptor{};
    layoutDescriptor.entryCount = 1;
    layoutDescriptor.entries = &layoutEntry;
    auto layout = device.CreateBindGroupLayout(&layoutDescriptor);
    std::unordered_set<WGPUTextureView> seen;
    unsigned recycled = 0;
    // Every group remains cached. A released API handle must not be reused in
    // its key for a different view while the old group can still be returned.
    for (unsigned i = 0; i < 256; ++i) {
        wgpu::TextureDescriptor textureDescriptor{};
        textureDescriptor.usage = wgpu::TextureUsage::TextureBinding;
        textureDescriptor.size = {1, 1, 1};
        textureDescriptor.format = wgpu::TextureFormat::RGBA8Unorm;
        auto texture = device.CreateTexture(&textureDescriptor);
        auto view = texture.CreateView();
        if (!seen.insert(view.Get()).second) ++recycled;
        WGPUBindGroupEntry entry{};
        entry.binding = 0;
        entry.textureView = view.Get();
        WGPUBindGroupDescriptor group{};
        group.layout = layout.Get();
        group.entryCount = 1;
        group.entries = &entry;
        const auto id = aurora::gfx::bind_group_ref(group);
        assert(aurora::gfx::find_bind_group(id));
    }
    printf("BIND_GROUP_LIFETIME {\"iterations\":256,\"recycledLiveCacheViewHandles\":%u}\n", recycled);
    assert(recycled == 0 && "A cached binding key reused a released texture-view handle");
    puts("PASS production bind-group resource lifetime");
    return 0;
}
