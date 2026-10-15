#include "codet/core.hpp"
#include "codet/output.hpp"
#include "codet/process_control.hpp"
#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <curl/curl.h>
#include <fcntl.h>
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
  const Cancellation &cancel;
  const ChunkSink &sink;
  size_t max;
  int idle_seconds;
  CurlContext(const Cancellation &c, const ChunkSink &s, size_t maximum, int idle)
      : cancel(c), sink(s), max(maximum), idle_seconds(idle) {}
  std::chrono::steady_clock::time_point activity = std::chrono::steady_clock::now();
};
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
    if (c.sink)
      c.sink(std::string(p, bytes));
  } catch (const std::exception &e) {
    c.error = e.what();
    return 0;
  } catch (...) {
    c.error = "HTTP stream callback failed";
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
std::string safe_text(const std::string &text) {
  return Json::parse(Json(text).dump(-1, ' ', false, Json::error_handler_t::replace))
      .get<std::string>();
}
Json safe_json(const Json &value) {
  return Json::parse(value.dump(-1, ' ', false, Json::error_handler_t::replace));
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
  for (unsigned char c : value) {
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
  curl_easy_setopt(raw, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(raw, CURLOPT_XFERINFOFUNCTION, curl_progress);
  curl_easy_setopt(raw, CURLOPT_XFERINFODATA, &c);
  curl_easy_setopt(raw, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(raw, CURLOPT_USERAGENT, "codet/" CODET_VERSION);
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
  if (result != CURLE_OK)
    throw Error(c.error.empty() ? std::string("HTTP transport: ") +
                                      (error[0] ? error : curl_easy_strerror(result))
                                : c.error);
  long status = 0;
  curl_easy_getinfo(raw, CURLINFO_RESPONSE_CODE, &status);
  return {static_cast<int>(status), std::move(c.body)};
}
std::string NativeFileSystem::read(const fs::path &p, size_t limit) const {
  Fd fd(::open(p.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK));
  if (fd.n < 0)
    throw Error(system_error("Read " + p.string()));
  struct stat st {};
  if (fstat(fd.n, &st) != 0 || !S_ISREG(st.st_mode))
    throw Error("Expected regular file: " + p.string());
  if (st.st_size < 0 || static_cast<uint64_t>(st.st_size) > limit)
    throw Error("File exceeds read limit: " + p.string());
  std::string result;
  char buf[8192];
  for (;;) {
    ssize_t n = ::read(fd.n, buf, sizeof buf);
    if (n < 0 && errno == EINTR)
      continue;
    if (n < 0)
      throw Error(system_error("Read " + p.string()));
    if (n == 0)
      break;
    if (static_cast<size_t>(n) > limit - result.size())
      throw Error("File grew beyond read limit");
    result.append(buf, static_cast<size_t>(n));
  }
  return result;
}
std::string NativeFileSystem::read_prefix(const fs::path &p, size_t limit, bool &truncated) const {
  Fd fd(::open(p.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK));
  if (fd.n < 0)
    throw Error(system_error("Read " + p.string()));
  struct stat st {};
  if (fstat(fd.n, &st) != 0 || !S_ISREG(st.st_mode))
    throw Error("Expected regular file: " + p.string());
  std::string text;
  char buffer[8192];
  while (text.size() <= limit) {
    const auto n = ::read(fd.n, buffer, std::min(sizeof buffer, limit + 1 - text.size()));
    if (n < 0 && errno == EINTR)
      continue;
    if (n < 0)
      throw Error(system_error("Read " + p.string()));
    if (!n)
      break;
    text.append(buffer, static_cast<size_t>(n));
  }
  const auto clean = safe_text(text);
  truncated = text.size() > limit || clean.size() > limit;
  return bounded_text(clean, limit);
}
void NativeFileSystem::atomic_write(const fs::path &p, const std::string &text, bool private_file) {
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
    size_t offset = 0;
    while (offset < text.size()) {
      ssize_t n = ::write(fd.n, text.data() + offset, text.size() - offset);
      if (n < 0 && errno == EINTR)
        continue;
      if (n <= 0)
        throw Error(system_error("Write atomic file"));
      offset += static_cast<size_t>(n);
    }
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
class NativeProcessControl final : public ProcessControl {
public:
  int watch(pid_t pid) override {
#ifdef __APPLE__
    const int fd = ::kqueue();
    if (fd < 0)
      throw Error(system_error("kqueue"));
    struct kevent event;
    EV_SET(&event, pid, EVFILT_PROC, EV_ADD | EV_ENABLE | EV_ONESHOT, NOTE_EXIT, 0, nullptr);
    if (::kevent(fd, &event, 1, nullptr, 0, nullptr) < 0) {
      const auto message = system_error("register process exit");
      ::close(fd);
      throw Error(message);
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
    if (count < 0 || (count && (event.flags & EV_ERROR)))
      throw Error("Cannot observe owned process exit");
    return count > 0 && (event.fflags & NOTE_EXIT);
#else
    (void)observer;
    siginfo_t info{};
    if (::waitid(P_PID, static_cast<id_t>(pid), &info, WEXITED | WNOHANG | WNOWAIT) < 0) {
      if (errno == EINTR)
        return false;
      if (errno == ECHILD)
        throw ProcessOwnershipLost(system_error("observe child"));
      throw Error(system_error("observe child"));
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
      throw Error(system_error("signal owned process"));
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
      throw Error(system_error("reap child"));
    return status;
  }
};
} // namespace
std::shared_ptr<ProcessControl> native_process_control() {
  return std::make_shared<NativeProcessControl>();
}
struct NativeProcesses::Job {
  pid_t pid = -1;
  int fd = -1, stderr_fd = -1, observer = -1;
  uint64_t completed_order = 0;
  ProcessSpec spec;
  ProcessResult result;
  std::string head, tail;
  std::unique_ptr<OutputSpool> spool;
  std::chrono::steady_clock::time_point began = std::chrono::steady_clock::now();
  bool done = false, retain = true, exit_observed = false;
  ~Job() {
    if (fd >= 0)
      ::close(fd);
    if (stderr_fd >= 0)
      ::close(stderr_fd);
    if (observer >= 0)
      ::close(observer);
  }
};
NativeProcesses::NativeProcesses() : NativeProcesses(native_process_control()) {}
NativeProcesses::NativeProcesses(std::shared_ptr<ProcessControl> control)
    : control_(std::move(control)) {}
NativeProcesses::~NativeProcesses() {
  for (auto &j : jobs_) {
    try {
      stop(j.first);
    } catch (...) { // NOLINT(bugprone-empty-catch): Destruction must continue stopping other jobs.
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
    throw Error(system_error("startup pipe"));
  Fd ready_read(ready_pipe[0]), ready_write(ready_pipe[1]);
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, release_pipe) < 0)
    throw Error(system_error("startup gate"));
  Fd release_read(release_pipe[0]), release_write(release_pipe[1]);
#ifdef __APPLE__
  const int no_sigpipe = 1;
  if (::setsockopt(release_write.n, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof no_sigpipe) < 0)
    throw Error(system_error("startup gate SIGPIPE protection"));
#endif
  int master = -1, slave = -1;
  if (spec.pty) {
    if (::openpty(&master, &slave, nullptr, nullptr, nullptr) < 0)
      throw Error(system_error("openpty"));
  } else {
    if (::pipe(output_pipe) < 0)
      throw Error(system_error("pipe"));
    master = output_pipe[0];
    slave = output_pipe[1];
  }
  Fd parent_output(master), child_output(slave);
  Fd stderr_read(-1), stderr_write(-1);
  if (spec.separate_stderr) {
    if (spec.pty)
      throw Error("Separate stderr is unavailable for PTYs");
    int errors[2];
    if (::pipe(errors) < 0)
      throw Error(system_error("stderr pipe"));
    stderr_read.n = errors[0];
    stderr_write.n = errors[1];
  }
  cancel.check();
  if (control_->now() >= deadline)
    throw ProcessTimeout();
  const pid_t pid = ::fork();
  if (pid < 0)
    throw Error(system_error("fork"));
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
void NativeProcesses::collect(Job &job) {
  const auto drain = [&](int fd, std::string &output) {
    char buffer[8192];
    size_t drained = 0;
    while (fd >= 0 && drained < size_t{1024} * 1024) {
      const auto n = ::read(fd, buffer, sizeof buffer);
      if (n < 0 && errno == EINTR)
        continue;
      if (n <= 0)
        break;
      const auto count = static_cast<size_t>(n);
      drained += count;
      job.result.total_output_bytes += count;
      if (job.spec.separate_stderr) {
        const auto used = job.result.output.size() + job.result.stderr_output.size();
        const auto keep = std::min(count, job.spec.output_limit - used);
        output.append(buffer, keep);
        job.result.truncated = job.result.truncated || keep < count;
      } else {
        if (!job.spec.spool_directory.empty()) {
          if (!job.spool) {
            job.spool =
                std::make_unique<OutputSpool>(job.spec.spool_directory, job.spec.spool_limit,
                                              job.spec.spool_quota, job.spec.spool_records);
            job.spool->append(job.head + job.tail);
          }
          if (job.spool)
            job.spool->append(std::string(buffer, count));
        }
        const auto head_limit = job.spec.output_limit / 2;
        const auto tail_limit = job.spec.output_limit - head_limit;
        const auto head_bytes = std::min(count, head_limit - job.head.size());
        job.head.append(buffer, head_bytes);
        const auto remaining = count - head_bytes;
        if (remaining >= tail_limit)
          job.tail.assign(buffer + count - tail_limit, tail_limit);
        else {
          const auto discard = job.tail.size() + remaining > tail_limit
                                   ? job.tail.size() + remaining - tail_limit
                                   : 0;
          job.tail.erase(0, discard);
          job.tail.append(buffer + head_bytes, remaining);
        }
        job.result.truncated = job.result.total_output_bytes > job.spec.output_limit;
      }
    }
  };
  drain(job.fd, job.result.output);
  drain(job.stderr_fd, job.result.stderr_output);
  if (!job.spec.separate_stderr)
    job.result.output =
        head_tail_text(job.head, job.tail, job.result.total_output_bytes, job.spec.output_limit);
  if (job.spool) {
    job.result.spill_path = job.spool->path().string();
    job.result.spill_bytes = job.spool->bytes();
    job.result.spill_truncated = job.spool->truncated();
    if (!job.spool->error().empty())
      job.result.capture_error = job.spool->error();
  }
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
  job.result.running = !job.done;
}
void NativeProcesses::prune() {
  size_t count = 0, bytes = 0;
  for (const auto &entry : jobs_)
    if (entry.second->done && entry.second->retain) {
      ++count;
      bytes += entry.second->result.output.size() + entry.second->result.stderr_output.size();
    }
  while (count > 64 || bytes > size_t{4} * size_t{1024} * 1024) {
    auto oldest = jobs_.end();
    for (auto it = jobs_.begin(); it != jobs_.end(); ++it)
      if (it->second->done && it->second->retain &&
          (oldest == jobs_.end() || it->second->completed_order < oldest->second->completed_order))
        oldest = it;
    if (oldest == jobs_.end())
      break;
    bytes -= oldest->second->result.output.size() + oldest->second->result.stderr_output.size();
    --count;
    jobs_.erase(oldest);
  }
}
void NativeProcesses::refresh() {
  std::vector<std::string> ids;
  ids.reserve(jobs_.size());
  for (const auto &job : jobs_)
    if (!job.second->done)
      ids.push_back(job.first);
  for (const auto &id : ids)
    (void)poll(id);
  prune();
}
std::vector<ProcessStatus> NativeProcesses::list() {
  refresh();
  std::vector<ProcessStatus> out;
  out.reserve(jobs_.size());
  for (const auto &job : jobs_)
    out.push_back({job.first, !job.second->done, job.second->result.exit_code});
  return out;
}
void NativeProcesses::finish(Job &job, bool ownership_lost) {
  if (job.done)
    return;
  // Keep the leader unreaped until every signal to this group has been issued.
  int status = 0;
  if (!ownership_lost) {
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
      ownership_lost ? 1 : (WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status));
  job.result.running = false;
  collect(job);
  if (job.spool) {
    job.spool->close();
    if (!job.spool->error().empty())
      job.result.capture_error = job.spool->error();
    job.result.spill_truncated = job.spool->truncated();
  }
  if (job.fd >= 0) {
    ::close(job.fd);
    job.fd = -1;
  }
  if (job.stderr_fd >= 0) {
    ::close(job.stderr_fd);
    job.stderr_fd = -1;
  }
}
ProcessResult NativeProcesses::poll(const std::string &id) {
  auto it = jobs_.find(id);
  if (it == jobs_.end())
    throw Error("Unknown or expired job: " + id);
  auto &job = *it->second;
  collect(job);
  if (!job.done &&
      std::chrono::duration_cast<std::chrono::milliseconds>(control_->now() - job.began).count() >
          job.spec.timeout_ms) {
    finish(job);
    job.result.exit_code = 124;
  }
  const auto result = job.result;
  prune();
  return result;
}
void NativeProcesses::stop(const std::string &id) {
  auto it = jobs_.find(id);
  if (it == jobs_.end())
    throw Error("Unknown or expired job: " + id);
  Job &job = *it->second;
  if (job.done)
    return;
  if (control_->now() >= job.began + std::chrono::milliseconds(job.spec.timeout_ms)) {
    finish(job);
    return;
  }
  control_->signal(-job.pid, SIGTERM);
  for (int i = 0; i < 10 && !job.done &&
                  control_->now() < job.began + std::chrono::milliseconds(job.spec.timeout_ms);
       ++i) {
    collect(job);
    if (!job.done)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  if (!job.done)
    finish(job);
}
void NativeProcesses::write(const std::string &id, const std::string &input) {
  const auto it = jobs_.find(id);
  if (it == jobs_.end() || it->second->done || !it->second->spec.pty)
    throw Error("Input requires a running PTY job");
  if (input.size() > 65536)
    throw Error("PTY input limit exceeded");
  size_t at = 0;
  while (at < input.size()) {
    const auto n = ::write(it->second->fd, input.data() + at, input.size() - at);
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
    ProcessResult result;
    do {
      cancel.check();
      result = poll(id);
      if (result.running)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (result.running);
    jobs_.erase(id);
    return result;
  } catch (...) {
    try {
      if (jobs_.count(id))
        stop(id);
    } catch (...) { // NOLINT(bugprone-empty-catch): Preserve the original run failure.
    }
    jobs_.erase(id);
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
void to_json(Json &j, const Message &m) {
  j = {{"role", m.role},   {"text", m.text},     {"tool_call_id", m.tool_call_id},
       {"calls", m.calls}, {"replay", m.replay}, {"replay_provider", m.replay_provider}};
}
void from_json(const Json &j, Message &m) {
  m.role = j.at("role");
  m.text = j.value("text", "");
  m.tool_call_id = j.value("tool_call_id", "");
  m.calls = j.value("calls", std::vector<ToolCall>{});
  m.replay = j.value("replay", Json::array());
  m.replay_provider = j.value("replay_provider", "");
}
} // namespace codet
