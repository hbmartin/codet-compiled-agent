#include "codet/core.hpp"
#include "codet/diagnostics.hpp"
#include "codet/output.hpp"
#include "codet/process_control.hpp"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <curl/curl.h>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/event.h>
#include <util.h>
#else
#include <pty.h>
#endif
#include <thread>

namespace codet {
namespace {
std::string system_error(const std::string &what) {
  return what + ": " + std::strerror(errno);
}
struct Fd {
  int n;
  explicit Fd(int fd) : n(fd) {}
  ~Fd() {
    if (n >= 0)
      ::close(n);
  }
  Fd(const Fd &) = delete;
  Fd &operator=(const Fd &) = delete;
};
struct CurlContext {
  std::string body, error;
  std::exception_ptr callback_failure;
  const Cancellation &cancel;
  const ChunkSink &sink;
  size_t max;
  int idle_seconds;
  int status = 200;
  CurlContext(const Cancellation &c, const ChunkSink &s, size_t maximum, int idle)
      : cancel(c), sink(s), max(maximum), idle_seconds(idle) {}
  std::chrono::steady_clock::time_point activity = std::chrono::steady_clock::now();
};
size_t curl_header(char *p, size_t s, size_t n, void *data) {
  if (s != 0 && n > SIZE_MAX / s)
    return 0;
  const size_t bytes = s * n;
  if (bytes >= 5 && std::string(p, 5) == "HTTP/") {
    std::istringstream line(std::string(p, bytes));
    std::string version;
    line >> version >> static_cast<CurlContext *>(data)->status;
  }
  return bytes;
}
size_t curl_write(char *p, size_t s, size_t n, void *data) {
  auto &c = *static_cast<CurlContext *>(data);
  if (s != 0 && n > SIZE_MAX / s)
    return 0;
  const size_t bytes = s * n;
  if (c.cancel.requested || bytes > c.max - c.body.size()) {
    c.error = "HTTP response limit exceeded";
    return 0;
  }
  try {
    if (bytes)
      c.activity = std::chrono::steady_clock::now();
    c.body.append(p, bytes);
    if (c.sink && c.status >= 200 && c.status < 300)
      c.sink(std::string(p, bytes));
  } catch (...) {
    c.callback_failure = std::current_exception();
    return 0;
  }
  return bytes;
}
int curl_progress(void *data, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
  auto &context = *static_cast<CurlContext *>(data);
  if (context.cancel.requested)
    return 1;
  if (context.idle_seconds > 0 && std::chrono::steady_clock::now() - context.activity >=
                                      std::chrono::seconds(context.idle_seconds)) {
    context.error = "HTTP stream idle timeout";
    return 1;
  }
  return 0;
}
} // namespace
namespace {
bool repair_text(const std::string &text, std::string &result) {
  size_t copied = 0;
  bool repaired = false;
  for (size_t i = 0; i < text.size();) {
    const auto lead = static_cast<unsigned char>(text[i]);
    if (lead < 0x80) {
      ++i;
      continue;
    }
    const size_t width = lead >= 0xc2 && lead <= 0xdf   ? 2
                         : lead >= 0xe0 && lead <= 0xef ? 3
                         : lead >= 0xf0 && lead <= 0xf4 ? 4
                                                        : 0;
    size_t accepted = 1;
    while (width && accepted < width && i + accepted < text.size()) {
      const auto byte = static_cast<unsigned char>(text[i + accepted]);
      const unsigned char low = accepted == 1 && lead == 0xe0   ? 0xa0
                                : accepted == 1 && lead == 0xf0 ? 0x90
                                                                : 0x80;
      const unsigned char high = accepted == 1 && lead == 0xed   ? 0x9f
                                 : accepted == 1 && lead == 0xf4 ? 0x8f
                                                                 : 0xbf;
      if (byte < low || byte > high)
        break;
      ++accepted;
    }
    if (width && accepted == width) {
      i += width;
      continue;
    }
    repaired = true;
    result.append(text, copied, i - copied);
    result += "\xef\xbf\xbd";
    i += accepted;
    copied = i;
  }
  if (repaired)
    result.append(text, copied, text.size() - copied);
  return repaired;
}
} // namespace
std::string safe_text(const std::string &text) {
  std::string result;
  if (!repair_text(text, result))
    return text;
  return result;
}
Json safe_json(const Json &value) {
  if (value.is_string())
    return safe_text(value.get_ref<const std::string &>());
  if (value.is_array()) {
    Json result = Json::array();
    result.get_ref<Json::array_t &>().reserve(value.size());
    for (const auto &item : value)
      result.push_back(safe_json(item));
    return result;
  }
  if (value.is_object()) {
    Json result = Json::object();
    for (auto it = value.begin(); it != value.end(); ++it)
      result[safe_text(it.key())] = safe_json(it.value());
    return result;
  }
  if (value.is_number_float() && !std::isfinite(value.get<double>()))
    return nullptr;
  // Binary/discarded values are not provider JSON. Retain the previous JSON representation.
  if (value.is_binary() || value.is_discarded())
    return Json::parse(value.dump(-1, ' ', false, Json::error_handler_t::replace));
  return value;
}
Json safe_json(Json &&value) {
  if (value.is_string()) {
    auto &text = value.get_ref<std::string &>();
    std::string repaired;
    if (repair_text(text, repaired))
      text.swap(repaired);
  } else if (value.is_array()) {
    for (auto &item : value)
      item = safe_json(std::move(item));
  } else if (value.is_object()) {
    bool rebuild = false;
    for (auto it = value.begin(); it != value.end(); ++it) {
      std::string repaired;
      if (repair_text(it.key(), repaired)) {
        rebuild = true;
        break;
      }
    }
    if (rebuild) {
      Json result = Json::object();
      // Original key order determines which value survives a repaired-key collision.
      for (auto it = value.begin(); it != value.end(); ++it)
        result[safe_text(it.key())] = safe_json(std::move(it.value()));
      return result;
    }
    for (auto &item : value)
      item = safe_json(std::move(item));
  } else if (value.is_number_float() && !std::isfinite(value.get<double>()))
    return nullptr;
  else if (value.is_binary() || value.is_discarded())
    return safe_json(static_cast<const Json &>(value));
  return std::move(value);
}
void report_diagnostic(const DiagnosticSink &sink, const std::string &message) {
  const auto text = bounded_text(message, size_t{1024} * 1024);
  if (sink) {
    try {
      sink(text);
      return;
    } catch (const std::bad_alloc &) {
      throw;
    } catch (...) { // A failed diagnostic renderer must not change a completed action.
      std::cerr << text << '\n';
      return;
    }
  }
  std::cerr << text << '\n';
}
const char *FileNotice::label() const noexcept {
  if (kind == FileNoticeKind::repair_backup)
    return confirmed ? "Session repair backup: "
                     : "Session repair backup candidate (publication unconfirmed): ";
  return confirmed ? "Uncommitted tool result preserved at "
                   : "Tool result receipt candidate (publication unconfirmed): ";
}
namespace {
template <class Write> bool escaped_file_path(const std::string &path, Write write) {
  constexpr char hex[] = "0123456789ABCDEF";
  char buffer[512];
  size_t used = 0;
  for (const auto character : path) {
    const auto byte = static_cast<unsigned int>(static_cast<unsigned char>(character));
    if (used + 4 > sizeof(buffer)) {
      if (!write(buffer, used))
        return false;
      used = 0;
    }
    if (byte >= 0x20 && byte <= 0x7e && byte != '\\')
      buffer[used++] = character;
    else {
      buffer[used++] = '\\';
      buffer[used++] = 'x';
      buffer[used++] = hex[byte >> 4u];
      buffer[used++] = hex[byte & 15u];
    }
  }
  return write(buffer, used);
}
} // namespace
std::string FileNotice::text() const {
  std::string repaired;
  const bool escape =
      repair_text(path, repaired) || std::any_of(path.begin(), path.end(), [](unsigned char byte) {
        return byte < 0x20 || byte == 0x7f || byte == '\\';
      });
  std::string result(label());
  if (escape)
    (void)escaped_file_path(path, [&](const char *bytes, size_t size) {
      result.append(bytes, size);
      return true;
    });
  else
    result += path;
  return result;
}
void emergency_file_diagnostic(const FileNotice &notice) noexcept {
  const auto write_all = [](const char *bytes, size_t size) noexcept {
    while (size) {
      const auto written = ::write(STDERR_FILENO, bytes, size);
      if (written < 0 && errno == EINTR)
        continue;
      if (written <= 0)
        return false;
      bytes += written;
      size -= static_cast<size_t>(written);
    }
    return true;
  };
  const auto *label = notice.label();
  if (!write_all(label, std::strlen(label)))
    return;
  if (escaped_file_path(notice.path, write_all))
    (void)write_all("\n", 1);
}
void FileDiagnosticQueue::submit(std::unique_ptr<FileNotice> notice) noexcept {
  auto *node = notice.release();
  node->next = pending_.load(std::memory_order_relaxed);
  while (!pending_.compare_exchange_weak(node->next, node, std::memory_order_release,
                                         std::memory_order_relaxed)) {
  }
}
FileNotice *FileDiagnosticQueue::take() noexcept {
  auto *pending = pending_.exchange(nullptr, std::memory_order_acquire);
  FileNotice *ordered = nullptr;
  while (pending) {
    auto *next = pending->next;
    pending->next = ordered;
    ordered = pending;
    pending = next;
  }
  return ordered;
}
void FileDiagnosticQueue::deliver(const DiagnosticSink &sink) noexcept {
  render(sink, false);
}
void FileDiagnosticQueue::preview(const DiagnosticSink &sink) noexcept {
  render(sink, true);
}
void FileDiagnosticQueue::retain(std::unique_ptr<FileNotice> notice) noexcept {
  if (deferred_tail_)
    deferred_tail_->next = notice.get();
  else
    deferred_ = notice.get();
  deferred_tail_ = notice.release();
}
void FileDiagnosticQueue::render(const DiagnosticSink &sink, bool preview) noexcept {
  auto *pending = take();
  while (pending) {
    std::unique_ptr<FileNotice> notice(pending);
    pending = pending->next;
    notice->next = nullptr;
    if (!sink) {
      retain(std::move(notice));
      continue;
    }
    try {
      sink(notice->text());
    } catch (...) {
      notice->rendering_failed = true;
    }
    if (preview || notice->rendering_failed)
      retain(std::move(notice));
  }
}
FileNotice *FileDiagnosticQueue::take_all() noexcept {
  auto *pending = take();
  if (deferred_tail_)
    deferred_tail_->next = pending;
  else
    deferred_ = pending;
  auto *all = deferred_;
  deferred_ = nullptr;
  deferred_tail_ = nullptr;
  return all;
}
void FileDiagnosticQueue::finish(const DiagnosticSink &sink) noexcept {
  auto *pending = take_all();
  while (pending) {
    std::unique_ptr<FileNotice> notice(pending);
    pending = notice->next;
    if (!notice->rendering_failed && sink)
      try {
        sink(notice->text());
        continue;
      } catch (...) {
        // Preserve the held bytes even if the final renderer partially emitted them.
        notice->rendering_failed = true;
      }
    emergency_file_diagnostic(*notice);
  }
}
void FileDiagnosticQueue::flush() noexcept {
  auto *pending = take_all();
  while (pending) {
    std::unique_ptr<FileNotice> notice(pending);
    pending = notice->next;
    emergency_file_diagnostic(*notice);
  }
}
FileDiagnosticQueue::~FileDiagnosticQueue() {
  flush();
}
std::string bounded_text(const std::string &text, size_t limit) {
  auto clean = safe_text(text);
  if (clean.size() > limit) {
    size_t end = limit;
    while (end > 0 && (static_cast<unsigned char>(clean[end]) & 0xc0) == 0x80)
      --end;
    clean.resize(end);
  }
  return clean;
}
int ProcessControl::wait_ready(int fd, int timeout_ms) {
  struct pollfd ready {
    fd, POLLIN, 0
  };
  const int result = ::poll(&ready, 1, timeout_ms);
  if (result < 0 && errno != EINTR)
    throw Error(system_error("wait for child setup"));
  return result;
}
int ProcessControl::read_setup(int fd, int &error) {
  return static_cast<int>(::read(fd, &error, sizeof error));
}
int ProcessControl::release(int fd) {
  const char token = 1;
#ifdef __APPLE__
  const int flags = 0;
#else
  const int flags = MSG_NOSIGNAL;
#endif
  return static_cast<int>(::send(fd, &token, 1, flags));
}
int64_t SystemClock::seconds() const {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}
void SystemClock::sleep_ms(int ms, const Cancellation &c) {
  for (int elapsed = 0; elapsed < ms; elapsed += 50) {
    c.check();
    std::this_thread::sleep_for(std::chrono::milliseconds(std::min(50, ms - elapsed)));
  }
}
std::string env(const char *name) {
  const char *v = std::getenv(name);
  return v ? v : "";
}
std::string unique_id() {
  unsigned char bytes[16];
  Fd fd(::open("/dev/urandom", O_RDONLY));
  if (fd.n < 0)
    throw Error(system_error("open random device"));
  size_t at = 0;
  while (at < sizeof bytes) {
    ssize_t n = ::read(fd.n, bytes + at, sizeof bytes - at);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      throw Error("Cannot read random device");
    at += static_cast<size_t>(n);
  }
  bytes[6] = (bytes[6] & 15) | 64;
  bytes[8] = (bytes[8] & 63) | 128;
  constexpr char hex[] = "0123456789abcdef";
  std::string out;
  for (size_t i = 0; i < 16; ++i) {
    if (i == 4 || i == 6 || i == 8 || i == 10)
      out += '-';
    out += hex[bytes[i] >> 4];
    out += hex[bytes[i] & 15];
  }
  return out;
}
bool within(const fs::path &child, const fs::path &root) {
  auto a = child.begin(), b = root.begin();
  for (; b != root.end(); ++b, ++a)
    if (a == child.end() || *a != *b)
      return false;
  return true;
}
std::string url_encode(const std::string &value) {
  static const char hex[] = "0123456789ABCDEF";
  std::string out;
  for (char raw : value) {
    const auto c = static_cast<unsigned char>(raw);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
        c == '_' || c == '.' || c == '~')
      out += static_cast<char>(c);
    else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 15];
    }
  }
  return out;
}
HttpResponse CurlTransport::perform(const HttpRequest &r, const Cancellation &cancel,
                                    const ChunkSink &sink) {
  cancel.check();
  static const int initialized = [] { return curl_global_init(CURL_GLOBAL_DEFAULT); }();
  if (initialized != 0)
    throw Error("Cannot initialize curl");
  CURL *raw = curl_easy_init();
  if (!raw)
    throw Error("Cannot allocate HTTP client");
  std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> handle(raw, curl_easy_cleanup);
  curl_slist *headers = nullptr;
  for (const auto &h : r.headers) {
    if (h.first.find_first_of("\r\n") != std::string::npos ||
        h.second.find_first_of("\r\n") != std::string::npos) {
      curl_slist_free_all(headers);
      throw Error("Invalid HTTP header");
    }
    auto *next = curl_slist_append(headers, (h.first + ": " + h.second).c_str());
    if (!next) {
      curl_slist_free_all(headers);
      throw Error("Cannot allocate HTTP headers");
    }
    headers = next;
  }
  std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> header_guard(headers,
                                                                           curl_slist_free_all);
  CurlContext c(cancel, sink, r.max_bytes, r.idle_timeout_seconds);
  char error[CURL_ERROR_SIZE] = {0};
  curl_easy_setopt(raw, CURLOPT_URL, r.url.c_str());
  curl_easy_setopt(raw, CURLOPT_PROTOCOLS_STR, "http,https");
  curl_easy_setopt(raw, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
  curl_easy_setopt(raw, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(raw, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(raw, CURLOPT_CONNECTTIMEOUT, static_cast<long>(r.connect_timeout_seconds));
  curl_easy_setopt(raw, CURLOPT_TIMEOUT, static_cast<long>(r.timeout_seconds));
  curl_easy_setopt(raw, CURLOPT_ERRORBUFFER, error);
  curl_easy_setopt(raw, CURLOPT_WRITEFUNCTION, curl_write);
  curl_easy_setopt(raw, CURLOPT_WRITEDATA, &c);
  curl_easy_setopt(raw, CURLOPT_HEADERFUNCTION, curl_header);
  curl_easy_setopt(raw, CURLOPT_HEADERDATA, &c);
  curl_easy_setopt(raw, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(raw, CURLOPT_XFERINFOFUNCTION, curl_progress);
  curl_easy_setopt(raw, CURLOPT_XFERINFODATA, &c);
  curl_easy_setopt(raw, CURLOPT_ACCEPT_ENCODING, "");
  const std::string user_agent = std::string("codet/") + CODET_VERSION;
  curl_easy_setopt(raw, CURLOPT_USERAGENT, user_agent.c_str());
  if (r.method == "GET") {
    curl_easy_setopt(raw, CURLOPT_FOLLOWLOCATION, r.headers.empty() ? 1L : 0L);
    curl_easy_setopt(raw, CURLOPT_MAXREDIRS, 5L);
  } else {
    curl_easy_setopt(raw, CURLOPT_CUSTOMREQUEST, r.method.c_str());
    curl_easy_setopt(raw, CURLOPT_POSTFIELDS, r.body.c_str());
    curl_easy_setopt(raw, CURLOPT_POSTFIELDSIZE, static_cast<long>(r.body.size()));
  }
  const CURLcode result = curl_easy_perform(raw);
  cancel.check();
  if (c.callback_failure)
    std::rethrow_exception(c.callback_failure);
  if (result != CURLE_OK)
    throw Error(c.error.empty() ? std::string("HTTP transport: ") +
                                      (error[0] ? error : curl_easy_strerror(result))
                                : c.error);
  long status = 0;
  curl_easy_getinfo(raw, CURLINFO_RESPONSE_CODE, &status);
  return {static_cast<int>(status), std::move(c.body)};
}
namespace {
std::string read_file_bytes(const fs::path &path, size_t limit, bool prefix) {
  Fd fd(::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK));
  if (fd.n < 0)
    throw Error(system_error("Read " + path.string()));
  struct stat status {};
  if (::fstat(fd.n, &status) != 0 || !S_ISREG(status.st_mode))
    throw Error("Expected regular file: " + path.string());
  if (!prefix && (status.st_size < 0 || static_cast<uint64_t>(status.st_size) > limit))
    throw Error("File exceeds read limit: " + path.string());
  std::string result;
  char buffer[8192];
  for (;;) {
    const auto remaining = limit - result.size();
    const auto request = remaining >= sizeof buffer ? sizeof buffer : remaining + 1;
    const auto count = ::read(fd.n, buffer, request);
    if (count < 0 && errno == EINTR)
      continue;
    if (count < 0)
      throw Error(system_error("Read " + path.string()));
    if (!count)
      break;
    if (static_cast<size_t>(count) > remaining) {
      if (!prefix)
        throw Error("File grew beyond read limit");
      result.append(buffer, static_cast<size_t>(count));
      break;
    }
    result.append(buffer, static_cast<size_t>(count));
  }
  return result;
}
} // namespace
std::string NativeFileSystem::read(const fs::path &path, size_t limit) const {
  return read_file_bytes(path, limit, false);
}
std::string NativeFileSystem::read_prefix(const fs::path &path, size_t limit,
                                          bool &truncated) const {
  const auto bytes = read_file_bytes(path, limit, true);
  const auto text = safe_text(bytes);
  truncated = bytes.size() > limit || text.size() > limit;
  return bounded_text(text, limit);
}
void FileSystem::atomic_stream(const fs::path &p,
                               const std::function<void(const Writer &)> &produce,
                               bool private_file) {
  std::string text;
  produce([&](const std::string &chunk) { text += chunk; });
  atomic_write(p, text, private_file);
}
void NativeFileSystem::atomic_write(const fs::path &p, const std::string &text, bool private_file) {
  atomic_stream(p, [&](const Writer &write) { write(text); }, private_file);
}
void NativeFileSystem::atomic_stream(const fs::path &p,
                                     const std::function<void(const Writer &)> &produce,
                                     bool private_file) {
  if (!fs::exists(p.parent_path()))
    throw Error("Parent directory does not exist: " + p.parent_path().string());
  struct stat old {};
  const bool exists = ::lstat(p.c_str(), &old) == 0;
  if (exists && !S_ISREG(old.st_mode))
    throw Error("Refusing to replace non-regular file: " + p.string());
  std::string pattern = (p.parent_path() / ".codet-write-XXXXXX").string();
  std::vector<char> name(pattern.begin(), pattern.end());
  name.push_back(0);
  Fd fd(::mkstemp(name.data()));
  if (fd.n < 0)
    throw Error(system_error("Create atomic file"));
  try {
    const mode_t mode = private_file ? 0600 : (exists ? (old.st_mode & 0777) : 0644);
    if (fchmod(fd.n, mode) != 0)
      throw Error(system_error("Set file permissions"));
    std::string pending;
    const auto flush = [&] {
      size_t offset = 0;
      while (offset < pending.size()) {
        const auto n = ::write(fd.n, pending.data() + offset, pending.size() - offset);
        if (n < 0 && errno == EINTR)
          continue;
        if (n <= 0)
          throw Error(system_error("Write atomic file"));
        offset += static_cast<size_t>(n);
      }
      pending.clear();
    };
    produce([&](const std::string &chunk) {
      for (size_t offset = 0; offset < chunk.size();) {
        const auto n = std::min<size_t>(65536 - pending.size(), chunk.size() - offset);
        pending.append(chunk, offset, n);
        offset += n;
        if (pending.size() == 65536)
          flush();
      }
    });
    flush();
    if (::fsync(fd.n) != 0)
      throw Error(system_error("Sync file"));
    struct stat current {};
    const bool now = ::lstat(p.c_str(), &current) == 0;
    if (now != exists || (now && (old.st_dev != current.st_dev || old.st_ino != current.st_ino)))
      throw Error("File changed during atomic write");
    if (::rename(name.data(), p.c_str()) != 0)
      throw Error(system_error("Replace atomic file"));
    Fd dir(::open(p.parent_path().c_str(), O_RDONLY));
    if (dir.n >= 0)
      (void)::fsync(dir.n);
  } catch (...) {
    ::unlink(name.data());
    throw;
  }
}
void NativeFileSystem::remove(const fs::path &p) {
  struct stat st {};
  if (lstat(p.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
    throw Error("Expected regular file for deletion");
  if (::unlink(p.c_str()) != 0)
    throw Error(system_error("Remove " + p.string()));
}
fs::path NativeFileSystem::canonical(const fs::path &p) const {
  return fs::weakly_canonical(fs::absolute(p));
}
namespace {
[[noreturn]] void process_error(const char *operation) {
  const auto code = errno;
  const auto message = system_error(operation);
  if (code == ENOMEM || code == EMFILE || code == ENFILE || code == ENOBUFS || code == EAGAIN)
    throw ProcessResourceError(message);
  throw Error(message);
}
class NativeProcessControl final : public ProcessControl {
public:
  int watch(pid_t pid) override {
#ifdef __APPLE__
    const int fd = ::kqueue();
    if (fd < 0)
      process_error("kqueue");
    struct kevent event;
    EV_SET(&event, static_cast<uintptr_t>(pid), EVFILT_PROC, EV_ADD | EV_ENABLE | EV_ONESHOT,
           NOTE_EXIT, 0, nullptr);
    if (::kevent(fd, &event, 1, nullptr, 0, nullptr) < 0) {
      const auto code = errno;
      ::close(fd);
      errno = code;
      process_error("register process exit");
    }
    (void)::fcntl(fd, F_SETFD, FD_CLOEXEC);
    return fd;
#else
    (void)pid;
    return -1;
#endif
  }
  bool exited(pid_t pid, int observer) override {
#ifdef __APPLE__
    (void)pid;
    struct kevent event;
    struct timespec immediate {};
    const int count = ::kevent(observer, nullptr, 0, &event, 1, &immediate);
    if (count < 0 && errno == EINTR)
      return false;
    if (count < 0)
      process_error("observe owned process exit");
    if (count && (event.flags & EV_ERROR)) {
      errno = static_cast<int>(event.data);
      process_error("observe owned process exit");
    }
    return count > 0 && (event.fflags & NOTE_EXIT);
#else
    (void)observer;
    siginfo_t info{};
    if (::waitid(P_PID, static_cast<id_t>(pid), &info, WEXITED | WNOHANG | WNOWAIT) < 0) {
      if (errno == EINTR)
        return false;
      if (errno == ECHILD)
        throw ProcessOwnershipLost(system_error("observe child"));
      process_error("observe child");
    }
    return info.si_pid == pid;
#endif
  }
  void signal(pid_t target, int sig) override {
    if (::kill(target, sig) < 0 && errno != ESRCH) {
#ifdef __APPLE__
      // Darwin returns EPERM when an owned group contains only its zombie leader.
      if (target < 0 && errno == EPERM)
        return;
#endif
      process_error("signal owned process");
    }
  }
  int reap(pid_t pid) override {
    int status = 0;
    pid_t result;
    do {
      result = ::waitpid(pid, &status, 0);
    } while (result < 0 && errno == EINTR);
    if (result < 0 && errno == ECHILD)
      throw ProcessOwnershipLost("Owned child is no longer available for reaping");
    if (result != pid)
      process_error("reap child");
    return status;
  }
  std::optional<int> try_reap(pid_t pid) override {
    int status = 0;
    const auto result = ::waitpid(pid, &status, WNOHANG);
    if (result < 0 && errno == EINTR)
      return std::nullopt;
    if (result < 0 && errno == ECHILD)
      throw ProcessOwnershipLost("Owned child is no longer available for reaping");
    if (result < 0)
      process_error("reap child without waiting");
    return result == pid ? std::optional<int>(status) : std::nullopt;
  }
};
} // namespace
ssize_t ProcessControl::read_output(int fd, void *buffer, size_t count) {
  return ::read(fd, buffer, count);
}
std::shared_ptr<ProcessControl> native_process_control() {
  return std::make_shared<NativeProcessControl>();
}
struct NativeProcesses::Job {
  Job() = default;
  Job(const Job &) = delete;
  Job &operator=(const Job &) = delete;
  pid_t pid = -1;
  int fd = -1, stderr_fd = -1, observer = -1, input_fd = -1;
  uint64_t completed_order = 0;
  ProcessSpec spec;
  ProcessResult result;
  std::chrono::steady_clock::time_point began = std::chrono::steady_clock::now();
  bool done = false, retain = true, exit_observed = false;
  bool cleanup_required = false;
  bool cleanup_kill_sent = false;
  std::exception_ptr failure;
  std::optional<int> termination_status;
  std::string error, reported_error, pending_utf8;
  ~Job() {
    if (input_fd >= 0)
      ::close(input_fd);
    if (fd >= 0)
      ::close(fd);
    if (stderr_fd >= 0)
      ::close(stderr_fd);
    if (observer >= 0)
      ::close(observer);
  }
};
NativeProcesses::NativeProcesses() : NativeProcesses(native_process_control()) {}
NativeProcesses::NativeProcesses(std::shared_ptr<ProcessControl> control,
                                 DiagnosticSink diagnostics)
    : control_(std::move(control)), diagnostics_(std::move(diagnostics)) {}
NativeProcesses::~NativeProcesses() {
  shutdown();
}
void NativeProcesses::shutdown() {
  for (auto &j : jobs_) {
    try {
      stop_owned(*j.second);
    } catch (...) {
      try {
        std::string detail = "Unknown exception";
        try {
          throw;
        } catch (const std::exception &e) {
          detail = e.what();
        } catch (...) { // NOLINT(bugprone-empty-catch): Report nonstandard cleanup failures too.
        }
        report_diagnostic(diagnostics_, "Process cleanup " + j.first + ": " + detail);
      } catch (...) { // NOLINT(bugprone-empty-catch): Cleanup must continue even if diagnostic
                      // allocation fails.
      }
    }
  }
}
std::string NativeProcesses::start(const ProcessSpec &spec) {
  Cancellation cancel;
  return start(spec, cancel);
}
std::string NativeProcesses::start(const ProcessSpec &spec, const Cancellation &cancel) {
  const auto began = control_->now();
  const auto deadline = began + std::chrono::milliseconds(spec.timeout_ms);
  const auto setup_deadline = std::min(deadline, began + std::chrono::seconds(5));
  cancel.check();
  refresh();
  const auto active =
      std::count_if(jobs_.begin(), jobs_.end(), [](const auto &j) { return !j.second->done; });
  if (active >= 64)
    throw Error("Process job limit reached");
  if (spec.argv.empty())
    throw Error("Empty process command");
  if (spec.spool_limit > size_t{8} * 1024 * 1024 || spec.spool_quota > size_t{64} * 1024 * 1024 ||
      spec.spool_records > 64)
    throw Error("Output capture exceeds supported storage limits");
  std::vector<char *> argv;
  argv.reserve(spec.argv.size() + 1);
  for (const auto &a : spec.argv)
    argv.push_back(const_cast<char *>(a.c_str()));
  argv.push_back(nullptr);
  int ready_pipe[2], release_pipe[2], output_pipe[2];
  if (::pipe(ready_pipe) < 0)
    process_error("startup pipe");
  Fd ready_read(ready_pipe[0]), ready_write(ready_pipe[1]);
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, release_pipe) < 0)
    process_error("startup gate");
  Fd release_read(release_pipe[0]), release_write(release_pipe[1]);
#ifdef __APPLE__
  const int no_sigpipe = 1;
  if (::setsockopt(release_write.n, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof no_sigpipe) < 0)
    process_error("startup gate SIGPIPE protection");
#endif
  int master = -1, slave = -1;
  if (spec.pty) {
    if (::openpty(&master, &slave, nullptr, nullptr, nullptr) < 0)
      process_error("openpty");
  } else {
    if (::pipe(output_pipe) < 0)
      process_error("pipe");
    master = output_pipe[0];
    slave = output_pipe[1];
  }
  Fd parent_output(master), child_output(slave);
  Fd parent_input(-1), child_input(-1);
  if (!spec.pty && spec.interactive_stdin) {
    int input[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, input) < 0)
      process_error("stdin socket");
    parent_input.n = input[0];
    child_input.n = input[1];
#ifdef __APPLE__
    if (::setsockopt(parent_input.n, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof no_sigpipe) < 0)
      process_error("stdin SIGPIPE protection");
#endif
  }
  Fd stderr_read(-1), stderr_write(-1);
  if (spec.separate_stderr) {
    if (spec.pty)
      throw Error("Separate stderr is unavailable for PTYs");
    int errors[2];
    if (::pipe(errors) < 0)
      process_error("stderr pipe");
    stderr_read.n = errors[0];
    stderr_write.n = errors[1];
  }
  cancel.check();
  if (control_->now() >= deadline)
    throw ProcessTimeout();
  const pid_t pid = ::fork();
  if (pid < 0)
    process_error("fork");
  if (pid == 0) {
    ::close(ready_read.n);
    ::close(release_write.n);
    struct sigaction disposition {};
    disposition.sa_handler = SIG_DFL;
    sigemptyset(&disposition.sa_mask);
    for (int sig = 1; sig < NSIG; ++sig)
      if (sig != SIGKILL && sig != SIGSTOP)
        (void)::sigaction(sig, &disposition, nullptr);
    sigset_t empty;
    sigemptyset(&empty);
    (void)::sigprocmask(SIG_SETMASK, &empty, nullptr);
    const int setup_error = ::setsid() < 0 ? errno : 0;
    ssize_t n;
    do {
      n = ::write(ready_write.n, &setup_error, sizeof setup_error);
    } while (n < 0 && errno == EINTR);
    if (n != static_cast<ssize_t>(sizeof setup_error) || setup_error)
      ::_exit(126);
    char token;
    do {
      n = ::read(release_read.n, &token, 1);
    } while (n < 0 && errno == EINTR);
    if (n != 1)
      ::_exit(126);
    ::close(master);
    if (spec.pty) {
      if (::ioctl(slave, TIOCSCTTY, 0) < 0)
        ::_exit(126);
      (void)::dup2(slave, 0);
    } else if (spec.interactive_stdin) {
      (void)::dup2(child_input.n, 0);
    } else {
      const int nullfd = ::open("/dev/null", O_RDONLY);
      if (nullfd < 0)
        ::_exit(126);
      (void)::dup2(nullfd, 0);
      if (nullfd > 2)
        ::close(nullfd);
    }
    (void)::dup2(slave, 1);
    (void)::dup2(spec.separate_stderr ? stderr_write.n : slave, 2);
    for (int fd = 3; fd < 1024; ++fd)
      ::close(fd);
    if (!spec.cwd.empty() && ::chdir(spec.cwd.c_str()) != 0)
      ::_exit(126);
    for (const auto &e : spec.env)
      (void)::setenv(e.first.c_str(), e.second.c_str(), 1);
    ::execvp(argv[0], argv.data());
    ::_exit(127);
  }
  ::close(ready_write.n);
  ready_write.n = -1;
  ::close(release_read.n);
  release_read.n = -1;
  ::close(child_output.n);
  child_output.n = -1;
  if (child_input.n >= 0) {
    ::close(child_input.n);
    child_input.n = -1;
  }
  if (stderr_write.n >= 0) {
    ::close(stderr_write.n);
    stderr_write.n = -1;
  }
  bool group_ready = false;
  try {
    int result = 0;
    while (result <= 0) {
      cancel.check();
      const auto now = control_->now();
      if (now >= deadline)
        throw ProcessTimeout();
      if (now >= setup_deadline)
        throw Error("Child process setup timed out");
      const auto remaining =
          std::chrono::duration_cast<std::chrono::milliseconds>(setup_deadline - now).count();
      result = control_->wait_ready(
          ready_read.n, static_cast<int>(std::max<int64_t>(1, std::min<int64_t>(50, remaining))));
    }
    const auto check_startup = [&] {
      cancel.check();
      const auto now = control_->now();
      if (now >= deadline)
        throw ProcessTimeout();
      if (now >= setup_deadline)
        throw Error("Child process setup timed out");
    };
    int setup_error = 0;
    int received;
    do {
      check_startup();
      received = control_->read_setup(ready_read.n, setup_error);
    } while (received < 0 && errno == EINTR);
    check_startup();
    if (received != static_cast<int>(sizeof setup_error) || setup_error)
      throw Error("Child process setup failed");
    group_ready = true;
    auto job = std::make_unique<Job>();
    job->pid = pid;
    job->spec = spec;
    if (!spec.separate_stderr) {
      auto store = spec.capture_store;
      if (!store && !spec.spool_directory.empty())
        store = std::make_shared<CaptureStore>(spec.spool_directory, spec.spool_limit,
                                               spec.spool_quota, spec.spool_records, diagnostics_);
      job->spec.capture_store = store;
      job->result.capture = std::make_shared<OutputCapture>(store, spec.output_limit);
    }
    job->began = began;
    // The child cannot exit through exec before this watcher has been installed.
    try {
      job->observer = control_->watch(pid);
    } catch (...) {
      check_startup();
      throw;
    }
    int released;
    do {
      check_startup();
      released = control_->release(release_write.n);
    } while (released < 0 && errno == EINTR);
    if (released != 1) {
      check_startup();
      throw Error("Cannot release child process");
    }
    job->fd = parent_output.n;
    parent_output.n = -1;
    job->stderr_fd = stderr_read.n;
    stderr_read.n = -1;
    job->input_fd = parent_input.n;
    parent_input.n = -1;
    if (job->input_fd >= 0) {
      (void)::fcntl(job->input_fd, F_SETFL, O_NONBLOCK);
      (void)::fcntl(job->input_fd, F_SETFD, FD_CLOEXEC);
    }
    if (job->stderr_fd >= 0) {
      (void)::fcntl(job->stderr_fd, F_SETFL, O_NONBLOCK);
      (void)::fcntl(job->stderr_fd, F_SETFD, FD_CLOEXEC);
    }
    (void)::fcntl(job->fd, F_SETFL, O_NONBLOCK);
    (void)::fcntl(job->fd, F_SETFD, FD_CLOEXEC);
    const std::string id = "job-" + std::to_string(++next_);
    jobs_.emplace(id, std::move(job));
    return id;
  } catch (...) {
    try {
      control_->signal(group_ready ? -pid : pid, SIGKILL);
    } catch (...) { // NOLINT(bugprone-empty-catch): Attempt reaping even if signaling fails.
    }
    try {
      (void)control_->reap(pid);
    } catch (...) { // NOLINT(bugprone-empty-catch): Preserve the original child-start failure.
    }
    throw;
  }
}
void NativeProcesses::drain(Job &job) {
  const auto drain = [&](int fd, std::string &output, bool stderr_stream) {
    char buffer[8192];
    size_t drained = 0;
    while (fd >= 0 && drained < size_t{1024} * 1024) {
      const auto n = control_->read_output(fd, buffer, sizeof buffer);
      if (n < 0 && errno == EINTR)
        continue;
      if (n <= 0)
        break;
      const auto count = static_cast<size_t>(n);
      drained += count;
      job.result.total_output_bytes += count;
      if (job.spec.separate_stderr) {
        const auto limit = job.spec.output_limit;
        const auto keep = std::min(count, limit - output.size());
        output.append(buffer, keep);
        if (stderr_stream)
          job.result.stderr_truncated |= keep < count;
        else
          job.result.stdout_truncated |= keep < count;
        job.result.truncated = job.result.stdout_truncated || job.result.stderr_truncated;
      } else {
        auto chunk = job.pending_utf8 + std::string(buffer, count);
        job.pending_utf8.clear();
        // Keep only a valid, incomplete trailing UTF-8 sequence across publications.
        if (job.spec.interactive_stdin && !job.done) {
          size_t lead = chunk.size();
          while (lead && (static_cast<unsigned char>(chunk[lead - 1]) & 0xc0) == 0x80)
            --lead;
          if (lead) {
            --lead;
            const auto byte = static_cast<unsigned char>(chunk[lead]);
            const size_t width = byte >= 0xc2 && byte <= 0xdf   ? 2
                                 : byte >= 0xe0 && byte <= 0xef ? 3
                                 : byte >= 0xf0 && byte <= 0xf4 ? 4
                                                                : 0;
            const auto available = chunk.size() - lead;
            if (width && available < width) {
              bool valid = true;
              if (available > 1) {
                const auto next = static_cast<unsigned char>(chunk[lead + 1]);
                valid = !(byte == 0xe0 && next < 0xa0) && !(byte == 0xed && next > 0x9f) &&
                        !(byte == 0xf0 && next < 0x90) && !(byte == 0xf4 && next > 0x8f);
              }
              if (valid) {
                job.pending_utf8 = chunk.substr(lead);
                chunk.resize(lead);
              }
            }
          }
        }
        job.result.capture->append(chunk);
      }
    }
  };
  drain(job.fd, job.result.output, false);
  drain(job.stderr_fd, job.result.stderr_output, true);
  if (job.done && !job.pending_utf8.empty()) {
    job.result.capture->append(job.pending_utf8);
    job.pending_utf8.clear();
  }
}
void NativeProcesses::collect(Job &job) {
  // Observation is independent of capture: stop_owned can still terminate after either fails.
  if (!job.done) {
    try {
      job.exit_observed = job.exit_observed || control_->exited(job.pid, job.observer);
    } catch (const ProcessOwnershipLost &error) {
      job.result.capture_error = error.what();
      finish(job, true);
    }
    if (job.exit_observed && !job.done)
      finish(job);
  }
  if (!job.done)
    drain(job);
  job.result.running = !job.done;
}
size_t NativeProcesses::job_bytes(const Job &job) {
  return job.result.output.capacity() + job.result.stderr_output.capacity() +
         (job.result.capture ? job.result.capture->memory_bytes() : 0);
}
size_t NativeProcesses::retained_output_bytes() const {
  size_t bytes = 0;
  for (const auto &entry : jobs_)
    if (entry.second->done && entry.second->retain)
      bytes += job_bytes(*entry.second);
  return bytes;
}
void NativeProcesses::prune() {
  size_t count = 0, bytes = 0;
  for (const auto &entry : jobs_)
    if (entry.second->done && entry.second->retain) {
      ++count;
      bytes += job_bytes(*entry.second);
    }
  while (count > 64 || bytes > size_t{4} * size_t{1024} * 1024) {
    auto oldest = jobs_.end();
    for (auto it = jobs_.begin(); it != jobs_.end(); ++it)
      if (it->second->done && it->second->retain &&
          (oldest == jobs_.end() || it->second->completed_order < oldest->second->completed_order))
        oldest = it;
    if (oldest == jobs_.end())
      break;
    bytes -= job_bytes(*oldest->second);
    --count;
    jobs_.erase(oldest);
  }
}
void NativeProcesses::refresh() {
  std::vector<std::string> ids;
  for (const auto &job : jobs_)
    if (!job.second->done)
      ids.push_back(job.first);
  for (const auto &id : ids) {
    auto &job = *jobs_.at(id);
    try {
      if (job.cleanup_required)
        advance_cleanup(job);
      else
        (void)observe(id);
    } catch (const std::exception &) {
      record_error(id, job, std::current_exception());
      if (job.reported_error != job.error) {
        job.reported_error = job.error;
        report_diagnostic(diagnostics_, job.error);
      }
    }
  }
  for (auto it = jobs_.begin(); it != jobs_.end();) {
    if (it->second->done && !it->second->retain)
      it = jobs_.erase(it);
    else
      ++it;
  }
  prune();
}
std::vector<ProcessStatus> NativeProcesses::list() {
  refresh();
  std::vector<ProcessStatus> out;
  for (const auto &job : jobs_)
    if (job.second->retain)
      out.push_back(
          {job.first, !job.second->done, job.second->result.exit_code, job.second->error});
  return out;
}
void NativeProcesses::finish(Job &job, bool ownership_lost, std::optional<int> collected_status) {
  if (job.done)
    return;
  // Keep the leader unreaped until every signal to this group has been issued.
  int status = collected_status.value_or(0);
  if (!ownership_lost && !collected_status) {
    control_->signal(-job.pid, SIGKILL);
    try {
      status = control_->reap(job.pid);
    } catch (const ProcessOwnershipLost &error) {
      ownership_lost = true;
      job.result.capture_error = error.what();
    }
  }
  job.pid = -1;
  job.done = true;
  job.completed_order = ++completed_;
  if (job.observer >= 0) {
    ::close(job.observer);
    job.observer = -1;
  }
  job.result.exit_code =
      ownership_lost ? 1
                     : job.termination_status.value_or(WIFEXITED(status) ? WEXITSTATUS(status)
                                                                         : 128 + WTERMSIG(status));
  job.result.running = false;
  // Ownership is already invalidated. Capture failure cannot leave descriptors open.
  std::exception_ptr capture_failure;
  bool secondary_capture_failure = false;
  try {
    drain(job);
  } catch (...) {
    capture_failure = std::current_exception();
  }
  try {
    if (job.result.capture)
      job.result.capture->finish();
  } catch (...) {
    if (!capture_failure)
      capture_failure = std::current_exception();
    else
      secondary_capture_failure = true;
  }
  if (job.input_fd >= 0) {
    ::close(job.input_fd);
    job.input_fd = -1;
  }
  if (job.fd >= 0) {
    ::close(job.fd);
    job.fd = -1;
  }
  if (job.stderr_fd >= 0) {
    ::close(job.stderr_fd);
    job.stderr_fd = -1;
  }
  if (secondary_capture_failure) {
    try {
      report_diagnostic(diagnostics_, "Output capture finalization also failed");
    } catch (...) {
      std::fputs("Output capture finalization also failed\n", stderr);
    }
  }
  if (capture_failure)
    std::rethrow_exception(capture_failure);
}
void NativeProcesses::terminate(Job &job, int status) {
  job.termination_status = status;
  finish(job);
}
void NativeProcesses::clear_error(Job &job) {
  job.error.clear();
  job.reported_error.clear();
  job.failure = nullptr;
}
void NativeProcesses::record_error(const std::string &id, Job &job,
                                   const std::exception_ptr &error) {
  if (fatal(error))
    std::rethrow_exception(error);
  try {
    std::rethrow_exception(error);
  } catch (const std::exception &e) {
    job.error = bounded_text("Job " + id + ": " + e.what(), 2048);
  }
  job.failure = error;
}
[[noreturn]] void NativeProcesses::throw_failure(const Job &job) const {
  try {
    std::rethrow_exception(job.failure);
  } catch (const Error &e) {
    throw Error(job.error, e.code);
  } catch (const std::exception &) {
    throw Error(job.error);
  }
}
NativeProcesses::Job &NativeProcesses::observe(const std::string &id) {
  auto it = jobs_.find(id);
  if (it == jobs_.end())
    throw Error("Unknown or expired job: " + id);
  auto &job = *it->second;
  try {
    collect(job);
    if (!job.done && control_->now() >= job.began + std::chrono::milliseconds(job.spec.timeout_ms))
      terminate(job, 124);
    clear_error(job);
  } catch (const std::exception &) {
    record_error(id, job, std::current_exception());
    // Internal callers retain original exception types; public boundaries add handle context.
    std::rethrow_exception(job.failure);
  }
  return job;
}
ProcessResult NativeProcesses::publish(Job &job, bool render) {
  auto result = job.result;
  if (result.capture) {
    result.capture->flush();
    result.preview = result.capture->snapshot();
    if (render) {
      const auto rendered = result.capture->render(job.spec.output_limit);
      result.output = rendered.text;
      result.truncated = rendered.omitted != 0;
    }
    result.capture->metadata(result, result.preview->total);
  }
  return result;
}
ProcessResult NativeProcesses::poll(const std::string &id) {
  auto &job = retained_job(id);
  try {
    auto result = publish(observe(id));
    prune();
    return result;
  } catch (const std::exception &) {
    record_error(id, job, std::current_exception());
    throw_failure(job);
  }
}
bool NativeProcesses::observe_cleanup(Job &job, std::exception_ptr &failure) {
  try {
    collect(job);
  } catch (...) {
    auto current = std::current_exception();
    if (!failure || fatal(current))
      failure = current;
  }
  return job.done;
}
void NativeProcesses::stop_owned(Job &job) {
  if (job.done) {
    clear_error(job);
    return;
  }
  std::exception_ptr observation_failure;
  observe_cleanup(job, observation_failure);
  if (!job.done) {
    job.cleanup_required = true;
    const auto deadline = job.began + std::chrono::milliseconds(job.spec.timeout_ms);
    if (control_->now() < deadline) {
      control_->signal(-job.pid, SIGTERM);
      for (int i = 0; i < 10 && !job.done && control_->now() < deadline; ++i) {
        if (observe_cleanup(job, observation_failure))
          break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      observe_cleanup(job, observation_failure);
    }
    if (!job.done) {
      if (control_->now() >= deadline)
        terminate(job, 124);
      else
        finish(job);
    }
  }
  clear_error(job);
  if (observation_failure) {
    if (fatal(observation_failure))
      std::rethrow_exception(observation_failure);
    try {
      std::rethrow_exception(observation_failure);
    } catch (const std::exception &e) {
      report_diagnostic(diagnostics_,
                        std::string("Process observation during cleanup: ") + e.what());
    }
  }
}
ProcessStatus NativeProcesses::status(const std::string &id) {
  auto &job = retained_job(id);
  try {
    observe(id);
    return {id, !job.done, job.result.exit_code, job.error};
  } catch (const std::exception &) {
    record_error(id, job, std::current_exception());
    throw_failure(job);
  }
}
NativeProcesses::Job &NativeProcesses::retained_job(const std::string &id) {
  const auto it = jobs_.find(id);
  if (it == jobs_.end() || !it->second->retain)
    throw Error("Unknown or expired job: " + id);
  return *it->second;
}
void NativeProcesses::advance_cleanup(Job &job) {
  std::exception_ptr failure;
  observe_cleanup(job, failure);
  if (!job.done && !job.cleanup_kill_sent) {
    control_->signal(-job.pid, SIGKILL);
    job.cleanup_kill_sent = true;
  }
  if (!job.done) {
    try {
      if (const auto status = control_->try_reap(job.pid))
        finish(job, false, status);
    } catch (const ProcessOwnershipLost &error) {
      job.result.capture_error = error.what();
      finish(job, true);
    } catch (...) {
      const auto current = std::current_exception();
      if (!failure || fatal(current))
        failure = current;
    }
  }
  if (fatal(failure))
    std::rethrow_exception(failure);
}
ProcessResult NativeProcesses::read_output(const std::string &id) {
  return consume(id, true);
}
ProcessResult NativeProcesses::take_output(const std::string &id) {
  return consume(id, false);
}
ProcessResult NativeProcesses::consume(const std::string &id, bool render) {
  auto &job = retained_job(id);
  try {
    auto result = publish(observe(id), render);
    if (result.capture) {
      result.capture->finish();
      job.result = ProcessResult{};
      job.result.running = !job.done;
      job.result.exit_code = result.exit_code;
      job.result.capture =
          std::make_shared<OutputCapture>(job.spec.capture_store, job.spec.output_limit);
      if (job.done)
        job.result.capture->finish();
    } else {
      job.result.output.clear();
      job.result.stderr_output.clear();
      job.result.truncated = false;
    }
    prune();
    return result;
  } catch (const std::exception &) {
    record_error(id, job, std::current_exception());
    throw_failure(job);
  }
}
void NativeProcesses::stop(const std::string &id) {
  auto &job = retained_job(id);
  try {
    stop_owned(job);
  } catch (const std::exception &) {
    record_error(id, job, std::current_exception());
    throw_failure(job);
  }
}
void NativeProcesses::close_stdin(const std::string &id) {
  auto &job = retained_job(id);
  if (job.spec.pty || !job.spec.interactive_stdin)
    throw Error("close_stdin requires a pipe session");
  if (job.input_fd >= 0) {
    ::close(job.input_fd);
    job.input_fd = -1;
  }
}
void NativeProcesses::write(const std::string &id, const std::string &input) {
  const auto it = jobs_.find(id);
  if (it == jobs_.end() || !it->second->retain || it->second->done ||
      (!it->second->spec.pty && it->second->input_fd < 0))
    throw Error("Input requires a running interactive job");
  if (input.size() > 65536)
    throw Error("PTY input limit exceeded");
  size_t at = 0;
  while (at < input.size()) {
    ssize_t n;
    if (it->second->spec.pty)
      n = ::write(it->second->fd, input.data() + at, input.size() - at);
    else {
#ifdef __APPLE__
      const int flags = 0;
#else
      const int flags = MSG_NOSIGNAL;
#endif
      n = ::send(it->second->input_fd, input.data() + at, input.size() - at, flags);
    }
    if (n < 0 && errno == EINTR)
      continue;
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
      throw Error("PTY input buffer is full; " + std::to_string(at) +
                  " bytes were already sent. Do not resend those bytes.");
    if (n <= 0)
      throw Error("Cannot write PTY input");
    at += static_cast<size_t>(n);
  }
}
ProcessResult NativeProcesses::run(const ProcessSpec &spec, const Cancellation &cancel) {
  std::string id;
  try {
    id = start(spec, cancel);
  } catch (const ProcessTimeout &) {
    return {124, "Command timed out during startup"};
  }
  jobs_.at(id)->retain = false;
  try {
    while (true) {
      cancel.check();
      auto &job = observe(id);
      if (job.done) {
        auto result = publish(job);
        jobs_.erase(id);
        return result;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  } catch (...) {
    try {
      if (jobs_.count(id))
        stop_owned(*jobs_.at(id));
    } catch (...) { // NOLINT(bugprone-empty-catch): Preserve the original run failure.
    }
    const auto found = jobs_.find(id);
    if (found != jobs_.end()) {
      if (found->second->done)
        jobs_.erase(found);
      // Failed internal cleanup remains private and owned for subsequent retries.
    }
    throw;
  }
}
void to_json(Json &j, const ToolCall &t) {
  j = {{"id", t.id}, {"name", t.name}, {"arguments", t.arguments}};
}
void from_json(const Json &j, ToolCall &t) {
  t.id = j.at("id");
  t.name = j.at("name");
  t.arguments = j.at("arguments");
}
void to_json(Json &j, const AssistantText &m) {
  j = {{"id", m.id}, {"text", m.text}};
  if (!m.phase.empty())
    j["phase"] = m.phase;
}
void from_json(const Json &j, AssistantText &m) {
  m.id = j.value("id", "");
  m.text = j.value("text", "");
  m.phase = j.value("phase", "");
}
void to_json(Json &j, const Message &m) {
  j = {{"role", m.role},   {"text", m.text},     {"tool_call_id", m.tool_call_id},
       {"calls", m.calls}, {"replay", m.replay}, {"replay_provider", m.replay_provider}};
  if (!m.assistant_items.empty())
    j["assistant_items"] = m.assistant_items;
  if (!m.checkpoint_state.is_null())
    j["checkpoint_state"] = m.checkpoint_state;
  if (!m.images.empty())
    j["images"] = m.images;
  if (!m.origin.empty())
    j["origin"] = m.origin;
  if (!m.artifact_index.empty())
    j["artifact_index"] = m.artifact_index;
  if (!m.message_id.empty())
    j["message_id"] = m.message_id;
  if (!m.result_file.empty())
    j["result_file"] = m.result_file;
  if (m.previewed)
    j["previewed"] = true;
  if (m.images_archived)
    j["images_archived"] = true;
}
void from_json(const Json &j, Message &m) {
  m.checkpoint_state = j.value("checkpoint_state", Json());
  m.assistant_items = j.value("assistant_items", std::vector<AssistantText>{});
  m.role = j.at("role");
  m.text = j.value("text", "");
  m.tool_call_id = j.value("tool_call_id", "");
  m.calls = j.value("calls", std::vector<ToolCall>{});
  m.replay = j.value("replay", Json::array());
  m.replay_provider = j.value("replay_provider", "");
  m.images = j.value("images", std::vector<ImageArtifact>{});
  m.origin = j.value("origin", "");
  m.artifact_index = j.value("artifact_index", "");
  m.message_id = j.value("message_id", "");
  m.result_file = j.value("result_file", "");
  m.previewed = j.value("previewed", false);
  m.images_archived = j.value("images_archived", false);
}
} // namespace codet
