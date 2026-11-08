#pragma once
#include "codet/detail/request_input.hpp"
#include "codet/detail/submission_queue.hpp"

namespace codet::detail {
// The CLI, regressions and measurement harness use this same preparation path.
template <class Observe>
PreparationOutcome prepare_input_submission(const Config &config, FileSystem &files,
                                            const InputLine &line, Interaction *interaction,
                                            const Cancellation &cancel, Observe observe) {
  const bool pending = line.preparation_context == PreparationContext::pending;
  std::shared_ptr<const PreparedSubmission> payload;
  auto result = prepare_submission(cancel, [&] {
    std::optional<SubmissionPreparation::RawInput> borrowed;
    if (pending)
      borrowed.emplace(line.text());
    if (line.admission && (!pending || !interaction))
      throw Error("Pending admission has incompatible preparation context");
    if (line.admission_failure)
      std::rethrow_exception(line.admission_failure);
    if (pending && !line.admission)
      throw Error("Pending submission has no admission");
    std::optional<SubmissionPreparation> inspected;
    if (pending) {
      inspected.emplace(line.original_storage(), *borrowed);
      SubmissionAccess::charge(*interaction, line.admission, 0, inspected->retained_size());
    }
    observe(1);
    auto command = share_prepared(
        prepare_interactive_prompt(config, files, line.original_storage(), [&](size_t bytes) {
          if (pending)
            SubmissionAccess::charge(*interaction, line.admission, bytes,
                                     inspected->retained_size(bytes));
        }));
    observe(2);
    if (pending) {
      auto submission = inspected->finish(
          command, static_cast<bool>(command), [&](size_t bytes, size_t retained) {
            SubmissionAccess::charge(*interaction, line.admission, bytes, retained);
          });
      payload = std::make_shared<const PreparedSubmission>(std::move(submission));
    }
    return command;
  });
  if (result.state == PreparationState::ready)
    result.submission = std::move(payload);
  return result;
}
} // namespace codet::detail
