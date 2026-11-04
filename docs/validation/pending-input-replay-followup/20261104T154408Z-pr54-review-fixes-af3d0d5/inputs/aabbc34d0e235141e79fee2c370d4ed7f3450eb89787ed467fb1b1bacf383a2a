#include "codet/core.hpp"
#include "codet/process_control.hpp"

namespace codet {
bool fatal(const std::exception_ptr &failure) {
  if (!failure)
    return false;
  bool propagate = false;
  try {
    std::rethrow_exception(failure);
  } catch (const std::bad_alloc &) {
    propagate = true;
  } catch (const ProcessResourceError &) {
    propagate = true;
  } catch (const Error &error) {
    propagate = error.code == 130;
  } catch (const std::exception &) {
    propagate = false;
  } catch (...) {
    propagate = true;
  }
  return propagate;
}
} // namespace codet
