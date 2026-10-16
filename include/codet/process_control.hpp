#pragma once
#include <chrono>
#include <exception>
#include <memory>
#include <stdexcept>
#include <sys/types.h>

namespace codet {
struct ProcessResourceError : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct ProcessOwnershipLost : std::runtime_error {
  using std::runtime_error::runtime_error;
};
// Internal syscall seam. Only the owning NativeProcesses instance may reap its children.
class ProcessControl {
public:
  virtual ~ProcessControl() = default;
  virtual int watch(pid_t) = 0;
  virtual bool exited(pid_t, int observer) = 0;
  virtual void signal(pid_t target, int signal) = 0;
  virtual int reap(pid_t) = 0;
  virtual std::chrono::steady_clock::time_point now() const {
    return std::chrono::steady_clock::now();
  }
  // Return readiness, timeout, or EINTR without resetting the caller's deadline.
  virtual int wait_ready(int fd, int timeout_ms);
  virtual int read_setup(int fd, int &setup_error);
  virtual int release(int fd);
  virtual ssize_t read_output(int fd, void *buffer, size_t count);
};
std::shared_ptr<ProcessControl> native_process_control();
// Unknown exception types are fatal; ordinary job-local exceptions remain recoverable.
bool fatal(const std::exception_ptr &);
} // namespace codet
