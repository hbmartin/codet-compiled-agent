#include "codet/computer.hpp"
#include <algorithm>
#include <openssl/sha.h>
#include <sys/stat.h>
#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#if !defined(__ppc__)
#include <ImageIO/ImageIO.h>
#endif
#endif

namespace codet {
namespace {
std::string image_hash(const std::string &bytes) {
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char *>(bytes.data()), bytes.size(), hash);
  std::string out;
  const char *hex = "0123456789abcdef";
  for (auto ch : hash) {
    out += hex[ch >> 4];
    out += hex[ch & 15];
  }
  return out;
}
void dimensions(size_t w, size_t h) {
  if (!w || !h || w > image_buffer_limit / 4 || h > image_buffer_limit / (w * 4))
    throw Error("Image decoded buffer exceeds 32 MiB or has invalid dimensions");
}
#ifdef __APPLE__
template <class T> class Owned {
  T value_;

public:
  explicit Owned(T value) : value_(value) {}
  ~Owned() {
    if (value_)
      CFRelease(value_);
  }
  operator T() const {
    return value_;
  }
  Owned(const Owned &) = delete;
  Owned &operator=(const Owned &) = delete;
};
ImageData encode_image(CGImageRef image, const std::string &detail) {
  if (!image)
    throw Error("Invalid image data");
  size_t width = CGImageGetWidth(image), height = CGImageGetHeight(image);
  dimensions(width, height);
  const double scale = detail == "original" ? 1.0 : std::min(1.0, 1280.0 / std::max(width, height));
  const size_t w = std::max(size_t{1}, static_cast<size_t>(width * scale)),
               h = std::max(size_t{1}, static_cast<size_t>(height * scale));
  Owned<CGColorSpaceRef> colors(CGColorSpaceCreateDeviceRGB());
  std::vector<unsigned char> pixels(w * h * 4);
  Owned<CGContextRef> context(
      CGBitmapContextCreate(pixels.data(), w, h, 8, w * 4, colors, kCGImageAlphaPremultipliedLast));
  if (!static_cast<CGContextRef>(context))
    throw Error("Cannot allocate image context");
  CGContextDrawImage(context, CGRectMake(0, 0, w, h), image);
  Owned<CGImageRef> resized(CGBitmapContextCreateImage(context));
  Owned<CFMutableDataRef> data(CFDataCreateMutable(nullptr, 0));
  Owned<CGImageDestinationRef> destination(
      CGImageDestinationCreateWithData(data, CFSTR("public.png"), 1, nullptr));
  if (!static_cast<CGImageDestinationRef>(destination))
    throw Error("PNG encoding unavailable");
  CGImageDestinationAddImage(destination, resized, nullptr);
  if (!CGImageDestinationFinalize(destination))
    throw Error("PNG encoding failed");
  const auto count = static_cast<size_t>(CFDataGetLength(data));
  if (count > image_file_limit)
    throw Error("Encoded image exceeds 8 MiB");
  return {std::string(reinterpret_cast<const char *>(CFDataGetBytePtr(data)), count), "image/png",
          w, h};
}
class NativeImages final : public ImageBackend {
public:
  bool available() const override {
    return true;
  }
  ImageData load(const fs::path &path, const std::string &detail) override {
    if (detail != "high" && detail != "original")
      throw Error("detail must be high or original");
    if (!fs::is_regular_file(path) || fs::file_size(path) > image_file_limit)
      throw Error("Image file exceeds 8 MiB or is not a regular file");
    const auto filename = path.string();
    Owned<CFURLRef> url(CFURLCreateFromFileSystemRepresentation(
        nullptr, reinterpret_cast<const UInt8 *>(filename.data()), filename.size(), false));
    Owned<CGImageSourceRef> source(CGImageSourceCreateWithURL(url, nullptr));
    if (!static_cast<CGImageSourceRef>(source))
      throw Error("Invalid or unsupported image data");
    Owned<CFDictionaryRef> props(CGImageSourceCopyPropertiesAtIndex(source, 0, nullptr));
    if (!static_cast<CFDictionaryRef>(props))
      throw Error("Missing image dimensions");
    size_t w = 0, h = 0;
    long width = 0, height = 0;
    const auto pw =
        static_cast<CFNumberRef>(CFDictionaryGetValue(props, kCGImagePropertyPixelWidth));
    const auto ph =
        static_cast<CFNumberRef>(CFDictionaryGetValue(props, kCGImagePropertyPixelHeight));
    if (!pw || !ph || !CFNumberGetValue(pw, kCFNumberLongType, &width) ||
        !CFNumberGetValue(ph, kCFNumberLongType, &height) || width <= 0 || height <= 0)
      throw Error("Invalid image dimensions");
    w = static_cast<size_t>(width);
    h = static_cast<size_t>(height);
    dimensions(w, h);
    Owned<CGImageRef> image(CGImageSourceCreateImageAtIndex(source, 0, nullptr));
    return encode_image(image, detail);
  }
};
#else
class NativeImages final : public ImageBackend {
public:
  bool available() const override {
    return false;
  }
  ImageData load(const fs::path &, const std::string &) override {
    throw Error("Native ImageIO is unavailable on this host");
  }
};
#endif
} // namespace
std::shared_ptr<ImageBackend> native_images() {
  return std::make_shared<NativeImages>();
}
ImageData encode_rgb(const std::string &rgb, size_t width, size_t height,
                     const std::string &detail) {
  dimensions(width, height);
  if (rgb.size() != width * height * 3)
    throw Error("Invalid RGB buffer");
#ifdef __APPLE__
  Owned<CGColorSpaceRef> colors(CGColorSpaceCreateDeviceRGB());
  Owned<CGDataProviderRef> provider(
      CGDataProviderCreateWithData(nullptr, rgb.data(), rgb.size(), nullptr));
  Owned<CGImageRef> image(CGImageCreate(width, height, 8, 24, width * 3, colors, kCGImageAlphaNone,
                                        provider, nullptr, false, kCGRenderingIntentDefault));
  return encode_image(image, detail);
#else
  (void)detail;
  throw Error("Native ImageIO is unavailable on this host");
#endif
}
std::string framebuffer_rgb(const unsigned char *source, size_t width, size_t height, size_t stride,
                            unsigned bits, bool big) {
  dimensions(width, height);
  if (!source || (bits != 16 && bits != 32) || stride < width * (bits / 8) ||
      stride > image_buffer_limit / height)
    throw Error("Unsupported or invalid framebuffer format");
  std::string out(width * height * 3, '\0');
  for (size_t y = 0; y < height; ++y)
    for (size_t x = 0; x < width; ++x) {
      const auto *p = source + y * stride + x * (bits / 8);
      auto *dest = &out[(y * width + x) * 3];
      if (bits == 32) {
        dest[0] = static_cast<char>(p[big ? 1 : 2]);
        dest[1] = static_cast<char>(p[big ? 2 : 1]);
        dest[2] = static_cast<char>(p[big ? 3 : 0]);
      } else {
        const unsigned value = big ? (unsigned(p[0]) << 8) | p[1] : (unsigned(p[1]) << 8) | p[0];
        for (unsigned c = 0; c < 3; ++c) {
          const unsigned v = (value >> (10 - c * 5)) & 31;
          dest[c] = static_cast<char>((v * 255 + 15) / 31);
        }
      }
    }
  return out;
}
void to_json(Json &j, const ImageArtifact &a) {
  j = {{"path", a.path},     {"mime", a.mime},   {"sha256", a.sha256},
       {"detail", a.detail}, {"width", a.width}, {"height", a.height}};
}
void from_json(const Json &j, ImageArtifact &a) {
  a.path = j.at("path");
  a.mime = j.at("mime");
  a.sha256 = j.at("sha256");
  a.detail = j.value("detail", "high");
  a.width = j.at("width");
  a.height = j.at("height");
}
ImageArtifact persist_image(const Config &c, FileSystem &files, const ImageData &image,
                            const std::string &detail) {
  dimensions(image.width, image.height);
  if (image.bytes.empty() || image.bytes.size() > image_file_limit ||
      (image.mime != "image/png" && image.mime != "image/jpeg"))
    throw Error("Invalid image artifact");
  const auto hash = image_hash(image.bytes);
  const auto dir = c.home / "artifacts";
  if (fs::is_symlink(dir))
    throw Error("Artifact directory must not be a symlink");
  fs::create_directories(dir);
  if (chmod(dir.c_str(), 0700) != 0)
    throw Error("Cannot protect artifact directory");
  const auto p = dir / (hash + (image.mime == "image/png" ? ".png" : ".jpg"));
  if (fs::is_symlink(p))
    throw Error("Artifact must not be a symlink");
  if (fs::exists(p)) {
    if (image_hash(files.read(p, image_file_limit)) != hash)
      throw Error("Corrupt immutable artifact");
  } else
    files.atomic_write(p, image.bytes, true);
  return {p.string(), image.mime, hash, detail, image.width, image.height};
}
std::string base64_encode(const std::string &data) {
  const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve((data.size() + 2) / 3 * 4);
  for (size_t i = 0; i < data.size(); i += 3) {
    const unsigned v =
        (unsigned(static_cast<unsigned char>(data[i])) << 16) |
        (i + 1 < data.size() ? unsigned(static_cast<unsigned char>(data[i + 1])) << 8 : 0) |
        (i + 2 < data.size() ? unsigned(static_cast<unsigned char>(data[i + 2])) : 0);
    out += alphabet[(v >> 18) & 63];
    out += alphabet[(v >> 12) & 63];
    out += i + 1 < data.size() ? alphabet[(v >> 6) & 63] : '=';
    out += i + 2 < data.size() ? alphabet[v & 63] : '=';
  }
  return out;
}
std::string image_data_url(const ImageArtifact &a, const Config &c) {
  dimensions(a.width, a.height);
  if (a.mime != "image/png" && a.mime != "image/jpeg")
    throw Error("Unsupported artifact MIME type");
  NativeFileSystem files;
  const auto root = files.canonical(c.home / "artifacts");
  const auto p = files.canonical(a.path);
  if (!within(p, root) || fs::is_symlink(a.path))
    throw Error("Image artifact is outside private storage");
  const auto bytes = files.read(p, image_file_limit);
  if (image_hash(bytes) != a.sha256)
    throw Error("Image artifact checksum mismatch");
  return "data:" + a.mime + ";base64," + base64_encode(bytes);
}
} // namespace codet
