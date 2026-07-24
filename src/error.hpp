#ifndef ASRYX_ERROR_HPP
#define ASRYX_ERROR_HPP

#include <expected>
#include <string>
#include <utility>

namespace asryx {

struct Error
{
  std::string message;
};

inline std::unexpected<Error> fail(std::string message)
{
  return std::unexpected(Error{.message = std::move(message)});
}

template <typename T> void ignore_failure(const std::expected<T, Error>& result)
{
  (void)result.has_value();
}

} // namespace asryx

#endif // ASRYX_ERROR_HPP
