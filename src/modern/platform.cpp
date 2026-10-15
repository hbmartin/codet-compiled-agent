#include "codet/core.hpp"
#include "codet/process_control.hpp"
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
  return static_cast<const Cancellation *>(data)->requested.load() ? 1 : 0;
}
} // namespace
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
  CurlContext c{{}, {}, cancel, sink, r.max_bytes};
  char error[CURL_ERROR_SIZE] = {0};
  curl_easy_setopt(raw, CURLOPT_URL, r.url.c_str());
  curl_easy_setopt(raw, CURLOPT_PROTOCOLS_STR, "http,https");
  curl_easy_setopt(raw, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
  curl_easy_setopt(raw, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(raw, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(raw, CURLOPT_CONNECTTIMEOUT, 20L);
  curl_easy_setopt(raw, CURLOPT_TIMEOUT, static_cast<long>(r.timeout_seconds));
  curl_easy_setopt(raw, CURLOPT_ERRORBUFFER, error);
  curl_easy_setopt(raw, CURLOPT_WRITEFUNCTION, curl_write);
  curl_easy_setopt(raw, CURLOPT_WRITEDATA, &c);
  curl_easy_setopt(raw, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(raw, CURLOPT_XFERINFOFUNCTION, curl_progress);
  curl_easy_setopt(raw, CURLOPT_XFERINFODATA, &cancel);
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
  int fd = -1, observer = -1;
  ProcessSpec spec;
  ProcessResult result;
  std::chrono::steady_clock::time_point began = std::chrono::steady_clock::now();
  bool done = false;
  ~Job() {
    if (fd >= 0)
      ::close(fd);
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
    } catch (...) {
    }
  }
}
std::string NativeProcesses::start(const ProcessSpec &spec) {
  const auto began = std::chrono::steady_clock::now();
  if (jobs_.size() >= 64)
    throw Error("Process job limit reached");
  if (spec.argv.empty())
    throw Error("Empty process command");
  std::vector<char *> argv;
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
  const pid_t pid = ::fork();
  if (pid < 0)
    throw Error(system_error("fork"));
  if (pid == 0) {
    ::close(ready_read.n);
    ::close(release_write.n);
    ::signal(SIGPIPE, SIG_DFL);
    ::signal(SIGINT, SIG_DFL);
    ::signal(SIGTERM, SIG_DFL);
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
    (void)::dup2(slave, 2);
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
  bool group_ready = false;
  try {
    struct pollfd ready {
      ready_read.n, POLLIN, 0
    };
    int result;
    do {
      result = ::poll(&ready, 1, std::max(1, std::min(spec.timeout_ms, 5000)));
    } while (result < 0 && errno == EINTR);
    int setup_error = 0;
    if (result <= 0 ||
        ::read(ready_read.n, &setup_error, sizeof setup_error) !=
            static_cast<ssize_t>(sizeof setup_error) ||
        setup_error)
      throw Error("Child process setup failed");
    group_ready = true;
    auto job = std::make_unique<Job>();
    job->pid = pid;
    job->spec = spec;
    job->began = began;
    // The child cannot exit through exec before this watcher has been installed.
    job->observer = control_->watch(pid);
    const char token = 1;
#ifdef __APPLE__
    const int send_flags = 0;
#else
    const int send_flags = MSG_NOSIGNAL;
#endif
    if (::send(release_write.n, &token, 1, send_flags) != 1)
      throw Error("Cannot release child process");
    job->fd = parent_output.n;
    parent_output.n = -1;
    (void)::fcntl(job->fd, F_SETFL, O_NONBLOCK);
    (void)::fcntl(job->fd, F_SETFD, FD_CLOEXEC);
    const std::string id = "job-" + std::to_string(++next_);
    jobs_.emplace(id, std::move(job));
    return id;
  } catch (...) {
    const auto failure = std::current_exception();
    try {
      control_->signal(group_ready ? -pid : pid, SIGKILL);
    } catch (...) {
    }
    try {
      (void)control_->reap(pid);
    } catch (...) {
    }
    std::rethrow_exception(failure);
  }
}
void NativeProcesses::collect(Job &job) {
  char buf[8192];
  size_t drained = 0;
  while (drained < 1024 * 1024) {
    ssize_t n = ::read(job.fd, buf, sizeof buf);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      break;
    drained += static_cast<size_t>(n);
    size_t keep =
        std::min(static_cast<size_t>(n), job.spec.output_limit - job.result.output.size());
    job.result.output.append(buf, keep);
    if (keep < static_cast<size_t>(n))
      job.result.truncated = true;
  }
  if (!job.done && control_->exited(job.pid, job.observer))
    finish(job);
  job.result.running = !job.done;
}
void NativeProcesses::finish(Job &job) {
  if (job.done)
    return;
  // Keep the leader unreaped until every signal to this group has been issued.
  control_->signal(-job.pid, SIGKILL);
  const int status = control_->reap(job.pid);
  job.pid = -1;
  job.done = true;
  if (job.observer >= 0) {
    ::close(job.observer);
    job.observer = -1;
  }
  job.result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
  job.result.running = false;
  collect(job);
}
ProcessResult NativeProcesses::poll(const std::string &id) {
  auto it = jobs_.find(id);
  if (it == jobs_.end())
    throw Error("Unknown job: " + id);
  auto &job = *it->second;
  collect(job);
  if (!job.done && std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - job.began)
                           .count() > job.spec.timeout_ms) {
    stop(id);
    job.result.exit_code = 124;
  }
  return job.result;
}
void NativeProcesses::stop(const std::string &id) {
  auto it = jobs_.find(id);
  if (it == jobs_.end())
    throw Error("Unknown job: " + id);
  Job &job = *it->second;
  if (job.done)
    return;
  control_->signal(-job.pid, SIGTERM);
  for (int i = 0; i < 10 && !job.done; ++i) {
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
  const auto id = start(spec);
  try {
    ProcessResult result;
    do {
      cancel.check();
      result = poll(id);
      if (result.running)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (result.running);
    stop(id);
    jobs_.erase(id);
    return result;
  } catch (...) {
    stop(id);
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
