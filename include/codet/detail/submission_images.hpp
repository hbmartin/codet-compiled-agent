#pragma once
#include "codet/detail/prepared_prompt.hpp"
namespace codet::detail {
// Shared execution boundary: codecs validate attachments before admission; this
// boundary transfers their ownership to exactly this prepared provider turn.
template <class Run>
void execute_image_submission(const std::string &text, const PreparedPrompt *prompt,
                              std::vector<ImageArtifact> images, Run run) {
  run(text, true, prompt, std::move(images));
}
} // namespace codet::detail
