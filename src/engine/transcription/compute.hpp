#pragma once // NOLINT(portability-avoid-pragma-once)

#include <cstdint>
#include <thread>

namespace engine::transcription::compute {

enum class CompiledBackend : std::uint8_t
{
  Cpu,
  Cuda,
  Vulkan,
};

#if (defined(ASRYX_BACKEND_CPU) + defined(ASRYX_BACKEND_CUDA) + defined(ASRYX_BACKEND_VULKAN)) != 1
#  error "exactly one Asryx inference backend must be selected"
#endif

#ifdef ASRYX_BACKEND_CPU
inline constexpr CompiledBackend kCompiledBackend = CompiledBackend::Cpu;
#elif defined(ASRYX_BACKEND_CUDA)
inline constexpr CompiledBackend kCompiledBackend = CompiledBackend::Cuda;
#elif defined(ASRYX_BACKEND_VULKAN)
inline constexpr CompiledBackend kCompiledBackend = CompiledBackend::Vulkan;
#endif

constexpr int resolve_cpu_threads(unsigned int logical_threads) noexcept
{
  if (logical_threads == 0) {
    return 4;
  }

  if (logical_threads <= 2) {
    return 1;
  }

  if (logical_threads <= 4) {
    return 2;
  }

  if (logical_threads <= 6) {
    return 3;
  }

  if (logical_threads <= 8) {
    return 4;
  }

  if (logical_threads <= 12) {
    return 6;
  }

  return 7;
}

constexpr int resolve_gpu_helper_threads(unsigned int logical_threads) noexcept
{
  if (logical_threads == 0) {
    return 4;
  }

  if (logical_threads <= 4) {
    return static_cast<int>(logical_threads);
  }

  return 4;
}

inline int resolve_threads() noexcept
{
  const auto logical_threads = std::thread::hardware_concurrency();

  if constexpr (kCompiledBackend == CompiledBackend::Cpu) {
    return resolve_cpu_threads(logical_threads);
  }

  return resolve_gpu_helper_threads(logical_threads);
}

static_assert(resolve_cpu_threads(0) == 4);
static_assert(resolve_cpu_threads(1) == 1);
static_assert(resolve_cpu_threads(2) == 1);
static_assert(resolve_cpu_threads(3) == 2);
static_assert(resolve_cpu_threads(4) == 2);
static_assert(resolve_cpu_threads(5) == 3);
static_assert(resolve_cpu_threads(6) == 3);
static_assert(resolve_cpu_threads(7) == 4);
static_assert(resolve_cpu_threads(8) == 4);
static_assert(resolve_cpu_threads(9) == 6);
static_assert(resolve_cpu_threads(12) == 6);
static_assert(resolve_cpu_threads(13) == 7);
static_assert(resolve_cpu_threads(20) == 7);
static_assert(resolve_cpu_threads(21) == 7);
static_assert(resolve_cpu_threads(64) == 7);

static_assert(resolve_gpu_helper_threads(0) == 4);
static_assert(resolve_gpu_helper_threads(1) == 1);
static_assert(resolve_gpu_helper_threads(2) == 2);
static_assert(resolve_gpu_helper_threads(3) == 3);
static_assert(resolve_gpu_helper_threads(4) == 4);
static_assert(resolve_gpu_helper_threads(64) == 4);

} // namespace engine::transcription::compute
