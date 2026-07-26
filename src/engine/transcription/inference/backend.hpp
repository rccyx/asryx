#ifndef ASRYX_ENGINE_TRANSCRIPTION_INFERENCE_BACKEND_HPP
#define ASRYX_ENGINE_TRANSCRIPTION_INFERENCE_BACKEND_HPP

#include <cstdint>

namespace engine::transcription::inference {

enum class CompiledBackend : std::uint8_t
{
  Cpu,
  Cuda,
  Vulkan,
};

bool uses_gpu() noexcept;
int resolve_threads() noexcept;

} // namespace engine::transcription::inference

#endif // ASRYX_ENGINE_TRANSCRIPTION_INFERENCE_BACKEND_HPP
