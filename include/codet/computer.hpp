#pragma once
#include "codet/core.hpp"

namespace codet {
constexpr size_t image_file_limit = size_t{8} * 1024 * 1024;
constexpr size_t image_buffer_limit = size_t{32} * 1024 * 1024;
struct ImageData {
  std::string bytes, mime = "image/png";
  size_t width = 0, height = 0;
};
class ImageBackend {
public:
  virtual ~ImageBackend() = default;
  virtual bool available() const = 0;
  virtual ImageData load(const fs::path &, const std::string &detail) = 0;
  virtual ImageData load_bytes(const std::string &, const std::string &) {
    throw Error("Image backend does not support decoding validated bytes");
  }
};
struct DesktopResult {
  Json data = Json::object();
  std::optional<ImageData> image;
};
class ComputerBackend {
public:
  virtual ~ComputerBackend() = default;
  virtual Json capabilities() const = 0;
  virtual DesktopResult invoke(const std::string &, const Json &, const Cancellation &) = 0;
};
struct CaptureRect {
  double x, y, width, height;
};
struct CaptureArea {
  size_t display = 0;
  CaptureRect bounds{};
  std::vector<std::string> warnings = {};
};
CaptureArea capture_area(const std::vector<CaptureRect> &, const Json &,
                         const std::optional<CaptureRect> &window = std::nullopt);
std::pair<double, double> desktop_point(double, double, size_t, size_t, double, double, double,
                                        double);
std::shared_ptr<ImageBackend> native_images();
std::shared_ptr<ComputerBackend> native_computer();
fs::path artifact_directory(const Config &);
ImageArtifact persist_image(const Config &, FileSystem &, const ImageData &, const std::string &);
std::string image_data_url(const ImageArtifact &, const Config &);
std::vector<ImageArtifact> input_images(const Config &, FileSystem &, ImageBackend &,
                                        const std::vector<fs::path> &, const Cancellation &);
std::string base64_encode(const std::string &);
ImageData encode_rgb(const std::string &, size_t, size_t, const std::string &);
// Converts tightly bounded Tiger framebuffer rows to RGB, without alignment assumptions.
std::string framebuffer_rgb(const unsigned char *, size_t width, size_t height, size_t stride,
                            unsigned bits, bool big_endian);
} // namespace codet
