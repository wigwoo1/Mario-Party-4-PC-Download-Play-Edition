#pragma once

#include "gpu.hpp"
#ifndef __EMSCRIPTEN__
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace aurora::webgpu {
// A validation request is consumed once. Capture the actual resolved game image,
// without introducing input, modifying game state, or capturing other windows.
inline wgpu::Buffer encode_frame_capture(const wgpu::CommandEncoder& encoder) {
  const char* request = std::getenv("PARTYBOARD_FRAME_CAPTURE_REQUEST");
  if (!request || !*request) return {};
  std::error_code error;
  if (!std::filesystem::exists(request, error) || error) return {};
  const auto& source = present_source();
  if (source.format != wgpu::TextureFormat::RGBA8Unorm &&
      source.format != wgpu::TextureFormat::RGBA8UnormSrgb &&
      source.format != wgpu::TextureFormat::BGRA8Unorm &&
      source.format != wgpu::TextureFormat::BGRA8UnormSrgb) return {};
  if (!std::filesystem::remove(request, error) || error) return {};
  const uint32_t pitch = (source.size.width * 4 + 255) & ~255u;
  const wgpu::BufferDescriptor descriptor{
      .label = "Validation frame capture",
      .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead,
      .size = uint64_t(pitch) * source.size.height,
  };
  auto buffer = g_device.CreateBuffer(&descriptor);
  const wgpu::TexelCopyTextureInfo from{.texture = source.texture};
  const wgpu::TexelCopyBufferInfo to{
      .layout = {.offset = 0, .bytesPerRow = pitch, .rowsPerImage = source.size.height},
      .buffer = buffer,
  };
  encoder.CopyTextureToBuffer(&from, &to, &source.size);
  return buffer;
}

inline void map_frame_capture(wgpu::Buffer buffer) {
  if (!buffer) return;
  const auto source = present_source();
  const uint32_t pitch = (source.size.width * 4 + 255) & ~255u;
  const std::string output = std::string(std::getenv("PARTYBOARD_FRAME_CAPTURE_REQUEST")) + ".ppm";
  buffer.MapAsync(wgpu::MapMode::Read, 0, uint64_t(pitch) * source.size.height,
      wgpu::CallbackMode::AllowSpontaneous,
      [buffer, size = source.size, format = source.format, pitch, output]
      (wgpu::MapAsyncStatus status, wgpu::StringView) {
        if (status != wgpu::MapAsyncStatus::Success) return;
        const auto* pixels = static_cast<const unsigned char*>(buffer.GetConstMappedRange());
        std::ofstream file(output, std::ios::binary);
        file << "P6\n" << size.width << ' ' << size.height << "\n255\n";
        const bool bgra = format == wgpu::TextureFormat::BGRA8Unorm ||
                          format == wgpu::TextureFormat::BGRA8UnormSrgb;
        for (uint32_t y = 0; y < size.height; ++y)
          for (uint32_t x = 0; x < size.width; ++x) {
            const auto* pixel = pixels + uint64_t(y) * pitch + x * 4;
            const char rgb[] = {char(pixel[bgra ? 2 : 0]), char(pixel[1]), char(pixel[bgra ? 0 : 2])};
            file.write(rgb, 3);
          }
        buffer.Unmap();
      });
}
} // namespace aurora::webgpu
#endif
