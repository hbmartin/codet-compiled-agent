#pragma once
#include <memory>
#include <sys/types.h>

namespace codet {
// Internal syscall seam. Only the owning NativeProcesses instance may reap its children.
class ProcessControl {
public:
  virtual ~ProcessControl() = default;
  virtual int watch(pid_t) = 0;
  virtual bool exited(pid_t, int observer) = 0;
  virtual void signal(pid_t target, int signal) = 0;
  virtual int reap(pid_t) = 0;
};
std::shared_ptr<ProcessControl> native_process_control();
} // namespace codet
