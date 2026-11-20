#include "engine/transcription/rolling/rolling.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace engine::transcription::rolling {

namespace {

constexpr std::uint64_t STEP = 6 * RATE;
constexpr std::uint64_t LIMIT = 28 * RATE;
constexpr std::uint64_t UNSTABLE_TAIL = RATE;
constexpr std::uint64_t TIMESTAMP_TOLERANCE = RATE;

std::uint64_t _distance(std::uint64_t a, std::uint64_t b)
{
  return a > b ? a - b : b - a;
}

} // namespace

Processor::Processor(Decode decode)
    : _decode(std::move(decode))
{
}

void Processor::_confirm(const std::vector<Segment>& current)
{
  std::size_t confirmed = 0;
  while (confirmed < _previous.size() && confirmed < current.size()) {
    const auto& old = _previous[confirmed];
    const auto& now = current[confirmed];
    if (old.text != now.text || _distance(old.begin, now.begin) > TIMESTAMP_TOLERANCE ||
        _distance(old.end, now.end) > TIMESTAMP_TOLERANCE || now.end + UNSTABLE_TAIL > _received)
    {
      break;
    }
    _committed += now.text;
    ++confirmed;
  }

  // A complete confirmed segment is the only authority to discard audio.
  if (confirmed != 0) {
    const auto cut = current[confirmed - 1].end;
    const auto count = static_cast<std::ptrdiff_t>(cut - _base);
    _audio.erase(_audio.begin(), _audio.begin() + count);
    _base = cut;
  }
  _previous.assign(current.begin() + static_cast<std::ptrdiff_t>(confirmed), current.end());
}

yx::Result<void> Processor::_process(bool final)
{
  if (_audio.empty()) {
    return yx::ok();
  }
  auto decoded = _decode(_audio);
  if (!decoded) {
    return std::unexpected(decoded.error());
  }
  std::uint64_t prior_end = 0;
  for (auto& segment : *decoded) {
    if (segment.begin < prior_end || segment.begin > segment.end || segment.end > _audio.size()) {
      return yx::fail(yx::ErrorCode::transcription, "invalid Whisper segment timestamps");
    }
    prior_end = segment.end;
    segment.begin += _base;
    segment.end += _base;
  }
  std::erase_if(*decoded, [](const Segment& segment) { return segment.text.empty(); });
  if (final) {
    for (const auto& segment : *decoded) {
      _committed += segment.text;
    }
    _audio.clear();
    _previous.clear();
    return yx::ok();
  }
  _confirm(*decoded);
  _decoded_at = _received;
  if (_audio.size() >= LIMIT) {
    return yx::fail(yx::ErrorCode::transcription, "rolling transcription has no confirmed boundary before 28 seconds");
  }
  return yx::ok();
}

yx::Result<void> Processor::append(std::span<const float> samples)
{
  while (!samples.empty()) {
    const auto room = static_cast<std::size_t>(LIMIT - _audio.size());
    const auto until_step = static_cast<std::size_t>(STEP - (_received - _decoded_at));
    const auto count = std::min({samples.size(), room, until_step});
    _audio.insert(_audio.end(), samples.begin(),
                  samples.begin() + static_cast<std::ptrdiff_t>(count));
    samples = samples.subspan(count);
    _received += count;
    if (_received - _decoded_at == STEP || _audio.size() == LIMIT) {
      const auto processed = _process(false);
      if (!processed) {
        return processed;
      }
    }
  }
  return yx::ok();
}

yx::Result<std::string> Processor::finish()
{
  const auto done = _process(true);
  if (!done) {
    return std::unexpected(done.error());
  }
  return yx::ok(std::move(_committed));
}

} // namespace engine::transcription::rolling
