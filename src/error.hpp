#ifndef ASRYX_ERROR_HPP
#define ASRYX_ERROR_HPP

#include <concepts>
#include <expected>
#include <string>
#include <type_traits>
#include <utility>

namespace yx {

struct Error
{
  std::string message;
};

template <typename T> using Result = std::expected<T, Error>;

[[nodiscard]] inline std::unexpected<Error> fail(std::string message)
{
  return std::unexpected(Error{
      .message = std::move(message),
  });
}

[[nodiscard]] inline std::unexpected<Error> fail(Error error)
{
  return std::unexpected(std::move(error));
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

#endif // ASRYX_ERROR_HPP
