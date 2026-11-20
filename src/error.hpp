#ifndef ASRYX_ERROR_HPP
#define ASRYX_ERROR_HPP

#include <concepts>
#include <expected>
#include <string>
#include <type_traits>
#include <utility>

namespace yx {

enum class ErrorCode
{
  invalid_argument,
  filesystem,
  process,
  audio_read,
  audio_format,
  recorder,
  clipboard,
  pipe_target,
  transcription,
  cancelled,
  notification,
};

struct Error
{
  ErrorCode code;
  std::string message;
};

template <typename T> using Result = std::expected<T, Error>;

[[nodiscard]] inline std::unexpected<Error> fail(ErrorCode code, std::string message)
{
  return std::unexpected(Error{
      .code = code,
      .message = std::move(message),
  });
}

[[nodiscard]] inline Result<void> ok()
{
  return Result<void>{};
}

template <typename T> [[nodiscard]] Result<std::decay_t<T>> ok(T&& value)
{
  using Value = std::decay_t<T>;

  return Result<Value>(std::in_place, std::forward<T>(value));
}

template <typename T, typename... Args>
  requires std::constructible_from<T, Args...>
[[nodiscard]] Result<T> ok(Args&&... args)
{
  return Result<T>(std::in_place, std::forward<Args>(args)...);
}

template <typename T> void ignore_failure(const Result<T>&) noexcept {}

} // namespace yx

// Value bindings live in the caller's scope; brace conditional invocations.
#define YX_DETAIL_TRY_VALUE(name, expression) \
  auto name##_expected = (expression); \
  if (!name##_expected) \
    return std::unexpected(std::move(name##_expected).error()); \
  auto name = *std::move(name##_expected)

#define YX_DETAIL_TRY_VOID(expression) \
  do { \
    auto yx_expected = (expression); \
    if (!yx_expected) \
      return std::unexpected(std::move(yx_expected).error()); \
  } while (false)

#define YX_DETAIL_TRY_SELECT(_1, _2, selected, ...) selected
#define YX_TRY(...) \
  YX_DETAIL_TRY_SELECT(__VA_ARGS__, YX_DETAIL_TRY_VALUE, YX_DETAIL_TRY_VOID)(__VA_ARGS__)

#endif // ASRYX_ERROR_HPP
