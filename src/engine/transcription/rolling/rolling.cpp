#include "engine/transcription/rolling/rolling.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <utility>

namespace engine::transcription::rolling {

namespace {

constexpr std::uint64_t STEP = 5 * RATE;
constexpr std::uint64_t LIMIT = 26 * RATE;
constexpr std::uint64_t UNSTABLE_TAIL = RATE;
constexpr std::uint64_t TIMESTAMP_TOLERANCE = (3 * RATE) / 2;
constexpr std::size_t MAX_CONTEXT_CHARS = 220;

std::uint64_t _distance(std::uint64_t a, std::uint64_t b)
{
  return a > b ? a - b : b - a;
}

std::string _normalize_word(std::string_view text)
{
  std::string out;
  out.reserve(text.size());
  for (char ch : text) {
    const auto byte = static_cast<unsigned char>(ch);
    if (std::isalnum(byte) != 0 || byte >= 0x80U) {
      out.push_back(static_cast<char>(std::tolower(byte)));
    }
  }
  return out;
}

bool _words_match(std::string_view a, std::string_view b)
{
  if (a == b) {
    return true;
  }
  const std::string na = _normalize_word(a);
  const std::string nb = _normalize_word(b);
  return !na.empty() && na == nb;
}

} // namespace

Processor::Processor(Decode decode, std::string base_prompt)
    : _decode(std::move(decode))
    , _base_prompt(std::move(base_prompt))
{
}

std::string Processor::_build_prompt() const
{
  if (_committed.empty()) {
    return _base_prompt;
  }

  std::string_view tail(_committed);
  if (tail.size() > MAX_CONTEXT_CHARS) {
    tail = tail.substr(tail.size() - MAX_CONTEXT_CHARS);
    const auto space = tail.find(' ');
    if (space != std::string_view::npos && space + 1 < tail.size()) {
      tail = tail.substr(space + 1);
    }
  }

  if (_base_prompt.empty()) {
    return std::string(tail);
  }

  std::string combined = _base_prompt;
  if (!combined.empty() && combined.back() != ' ' && !tail.empty() && tail.front() != ' ') {
    combined.push_back(' ');
  }
  combined.append(tail);
  return combined;
}

void Processor::_confirm(const std::vector<Word>& current)
{
  std::size_t confirmed = 0;
  while (confirmed < _previous.size() && confirmed < current.size()) {
    const auto& old = _previous[confirmed];
    const auto& now = current[confirmed];
    if (!_words_match(old.text, now.text) || _distance(old.begin, now.begin) > TIMESTAMP_TOLERANCE ||
        now.end + UNSTABLE_TAIL > _received)
    {
      break;
    }
    _committed += now.text;
    ++confirmed;
  }

  if (confirmed != 0) {
    const auto cut = current[confirmed - 1].end;
    if (cut > _base) {
      const auto count = static_cast<std::ptrdiff_t>(cut - _base);
      _audio.erase(_audio.begin(), _audio.begin() + count);
      _base = cut;
    }
  }
  _previous.assign(current.begin() + static_cast<std::ptrdiff_t>(confirmed), current.end());
}

void Processor::_force_advance(const std::vector<Word>& current)
{
  const auto window_end = _base + static_cast<std::uint64_t>(_audio.size());
  const auto safe_horizon = window_end > UNSTABLE_TAIL ? window_end - UNSTABLE_TAIL : _base;

  std::uint64_t cut = _base;
  for (const auto& word : current) {
    if (word.end <= safe_horizon) {
      _committed += word.text;
      cut = word.end;
    }
  }

  if (cut <= _base) {
    const auto drop = std::min<std::size_t>(static_cast<std::size_t>(STEP), _audio.size());
    _audio.erase(_audio.begin(), _audio.begin() + static_cast<std::ptrdiff_t>(drop));
    _base += static_cast<std::uint64_t>(drop);
  }
  else {
    const auto count = static_cast<std::ptrdiff_t>(cut - _base);
    _audio.erase(_audio.begin(), _audio.begin() + count);
    _base = cut;
  }

  _previous.clear();
}

yx::Result<void> Processor::_process(bool final)
{
  if (_audio.empty()) {
    return yx::ok();
  }

  auto decoded = _decode(_audio, _build_prompt());
  if (!decoded) {
    return std::unexpected(decoded.error());
  }

  const auto limit = static_cast<std::uint64_t>(_audio.size());
  std::uint64_t prior_end = 0;
  for (auto& word : *decoded) {
    word.end = std::min(word.end, limit);
    word.begin = std::clamp(word.begin, prior_end, word.end);
    prior_end = word.end;
    word.begin += _base;
    word.end += _base;
  }
  std::erase_if(*decoded, [](const Word& word) { return word.text.empty(); });

  if (final) {
    for (const auto& word : *decoded) {
      _committed += word.text;
    }
    _audio.clear();
    _previous.clear();
    return yx::ok();
  }

  _confirm(*decoded);
  _decoded_at = _received;

  if (_audio.size() >= LIMIT) {
    _force_advance(*decoded);
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