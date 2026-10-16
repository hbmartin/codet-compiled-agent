#include "codet/computer.hpp"
#include "codet/support.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__) && defined(__ppc__)
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#undef check
#endif

namespace codet {
CaptureArea capture_area(const std::vector<CaptureRect> &displays, const Json &args,
                         const std::optional<CaptureRect> &window) {
  const bool region =
      args.contains("x") || args.contains("y") || args.contains("width") || args.contains("height");
  if ((args.contains("display_id") && (region || args.contains("window_id"))) ||
      (region && args.contains("window_id")))
    throw Error("Choose one screenshot selector: display_id, window_id, or a global region");
  if (displays.empty())
    throw Error("No active displays");
  for (const auto &display : displays)
    if (!std::isfinite(display.x) || !std::isfinite(display.y) || !std::isfinite(display.width) ||
        !std::isfinite(display.height) || !std::isfinite(display.x + display.width) ||
        !std::isfinite(display.y + display.height) || display.width <= 0 || display.height <= 0)
      throw Error("Invalid display bounds; refresh computer.get_state");
  if (!region && !args.contains("window_id")) {
    const auto id = args.value("display_id", size_t{1});
    if (!id || id > displays.size())
      throw Error("Invalid display ID; use get_state display IDs");
    return {id - 1, displays[id - 1]};
  }
  CaptureRect requested{};
  if (region) {
    if (!args.contains("x") || !args.contains("y") || !args.contains("width") ||
        !args.contains("height"))
      throw Error("A region requires x, y, width and height");
    requested = {args.at("x"), args.at("y"), args.at("width"), args.at("height")};
  } else {
    if (!window)
      throw Error("Expired window ID; refresh computer.get_state");
    requested = *window;
  }
  if (!std::isfinite(requested.x) || !std::isfinite(requested.y) ||
      !std::isfinite(requested.width) || !std::isfinite(requested.height) ||
      !std::isfinite(requested.x + requested.width) ||
      !std::isfinite(requested.y + requested.height) || requested.width <= 0 ||
      requested.height <= 0)
    throw Error("Invalid capture region");
  std::optional<CaptureArea> result;
  for (size_t i = 0; i < displays.size(); ++i) {
    const auto &display = displays[i];
    const double x = std::max(requested.x, display.x), y = std::max(requested.y, display.y);
    const double right = std::min(requested.x + requested.width, display.x + display.width);
    const double bottom = std::min(requested.y + requested.height, display.y + display.height);
    if (right > x && bottom > y) {
      if (result)
        throw Error("Capture spans displays; capture each display separately or choose a region "
                    "inside one display");
      result = CaptureArea{i, {x, y, right - x, bottom - y}};
    }
  }
  if (!result)
    throw Error("Capture is outside active displays");
  return *result;
}
std::pair<double, double> desktop_point(double x, double y, size_t width, size_t height,
                                        double origin_x, double origin_y, double extent_x,
                                        double extent_y) {
  if (!std::isfinite(x) || !std::isfinite(y) || !width || !height || x < 0 || y < 0 ||
      x >= static_cast<double>(width) || y >= static_cast<double>(height) ||
      !std::isfinite(origin_x) || !std::isfinite(origin_y) || !std::isfinite(extent_x) ||
      !std::isfinite(extent_y) || extent_x <= 0 || extent_y <= 0)
    throw Error("Pointer coordinates are outside screenshot");
  return {origin_x + x * extent_x / static_cast<double>(width),
          origin_y + y * extent_y / static_cast<double>(height)};
}
#if defined(__APPLE__) && defined(__ppc__)
namespace {
std::string string_value(CFTypeRef value, CFIndex limit = 8192) {
  if (!value || CFGetTypeID(value) != CFStringGetTypeID())
    return {};
  auto text = static_cast<CFStringRef>(value);
  const auto n = CFStringGetLength(text);
  if (n > limit)
    return "[text exceeds observation limit]";
  std::vector<char> buffer(
      static_cast<size_t>(CFStringGetMaximumSizeForEncoding(n, kCFStringEncodingUTF8)) + 1);
  if (!CFStringGetCString(text, buffer.data(), buffer.size(), kCFStringEncodingUTF8))
    return {};
  return buffer.data();
}
CFTypeRef attribute(AXUIElementRef element, CFStringRef name) {
  CFTypeRef out = nullptr;
  if (AXUIElementCopyAttributeValue(element, name, &out) != kAXErrorSuccess)
    return nullptr;
  return out;
}
Json rectangle(CGRect r) {
  return {{"x", r.origin.x}, {"y", r.origin.y}, {"width", r.size.width}, {"height", r.size.height}};
}
CGRect bounds(AXUIElementRef node) {
  CF<CFTypeRef> pos(attribute(node, kAXPositionAttribute)), size(attribute(node, kAXSizeAttribute));
  CGPoint p{};
  CGSize s{};
  if (pos.value && CFGetTypeID(pos.value) == AXValueGetTypeID())
    AXValueGetValue(static_cast<AXValueRef>(pos.value), kAXValueCGPointType, &p);
  if (size.value && CFGetTypeID(size.value) == AXValueGetTypeID())
    AXValueGetValue(static_cast<AXValueRef>(size.value), kAXValueCGSizeType, &s);
  return CGRectMake(p.x, p.y, s.width, s.height);
}
class DesktopLock {
  int fd_ = -1;

public:
  explicit DesktopLock(const Cancellation &cancel) {
    const auto path = "/tmp/codet-computer-" + std::to_string(getuid()) + ".lock";
    fd_ = open(path.c_str(), O_RDWR | O_CREAT | O_NOFOLLOW, 0600);
    struct stat st{};
    if (fd_ < 0 || fstat(fd_, &st) != 0 || !S_ISREG(st.st_mode) || st.st_uid != getuid() ||
        (st.st_mode & 077) != 0) {
      if (fd_ >= 0)
        close(fd_);
      throw Error("Cannot acquire private desktop lock");
    }
    (void)fcntl(fd_, F_SETFD, FD_CLOEXEC);
    struct flock lock{};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    SystemClock clock;
    try {
      for (int tries = 0; fcntl(fd_, F_SETLK, &lock) < 0; ++tries) {
        cancel.check();
        if (tries >= 100)
          throw Error("Desktop is busy in another Codet process");
        clock.sleep_ms(50, cancel);
      }
    } catch (...) {
      close(fd_);
      fd_ = -1;
      throw;
    }
  }
  ~DesktopLock() {
    if (fd_ >= 0)
      close(fd_);
  }
};
void post_mouse(CGEventType type, CGPoint p, CGMouseButton button, int clicks = 1) {
  CF<CGEventRef> event(CGEventCreateMouseEvent(nullptr, type, p, button));
  if (!event.value)
    throw Error("Cannot create mouse event");
  CGEventSetIntegerValueField(event, kCGMouseEventClickState, clicks);
  CGEventPost(kCGSessionEventTap, event);
}
void post_key(CGKeyCode key, bool down, CGEventFlags flags = 0) {
  CF<CGEventRef> event(CGEventCreateKeyboardEvent(nullptr, key, down));
  if (!event.value)
    throw Error("Cannot create keyboard event");
  CGEventSetFlags(event, flags);
  CGEventPost(kCGSessionEventTap, event);
}
struct InputCleanup {
  std::vector<CGKeyCode> keys;
  std::optional<CGMouseButton> mouse;
  CGPoint position{};
  ~InputCleanup() {
    try {
      if (mouse)
        post_mouse(*mouse == kCGMouseButtonLeft    ? kCGEventLeftMouseUp
                   : *mouse == kCGMouseButtonRight ? kCGEventRightMouseUp
                                                   : kCGEventOtherMouseUp,
                   position, *mouse);
      for (auto it = keys.rbegin(); it != keys.rend(); ++it)
        post_key(*it, false);
    } catch (...) { /* Input release is best effort during failure. */
    }
  }
};
class TigerComputer final : public ComputerBackend {
  struct Shot {
    CGRect rect;
    size_t width, height;
    std::chrono::steady_clock::time_point created;
  };
  std::map<std::string, Shot> shots_;
  std::map<std::string, AXUIElementRef> nodes_;
  std::map<std::string, CGRect> windows_;
  std::map<int, ProcessSerialNumber> apps_;
  std::string snapshot_;
  std::chrono::steady_clock::time_point observed_{};
  bool ax_enabled() const {
    return AXAPIEnabled() || AXIsProcessTrusted();
  }
  void clear() {
    for (auto &node : nodes_)
      CFRelease(node.second);
    nodes_.clear();
    windows_.clear();
  }
  void require_snapshot(const Json &a) const {
    if (a.value("snapshot_id", "") != snapshot_ ||
        std::chrono::steady_clock::now() - observed_ > std::chrono::seconds(60))
      throw Error("Expired accessibility snapshot; call computer.get_state again");
  }
  AXUIElementRef node(const Json &a) const {
    require_snapshot(a);
    const auto it = nodes_.find(a.at("element_id"));
    if (it == nodes_.end())
      throw Error("Unknown accessibility node");
    return it->second;
  }
  CGPoint point(const Json &a, const char *x = "x", const char *y = "y") const {
    const auto it = shots_.find(a.at("screenshot_id"));
    if (it == shots_.end() ||
        std::chrono::steady_clock::now() - it->second.created > std::chrono::seconds(60))
      throw Error("Expired screenshot; capture again");
    const auto px = a.at(x).get<double>(), py = a.at(y).get<double>();
    const auto &s = it->second;
    const auto result = desktop_point(px, py, s.width, s.height, s.rect.origin.x, s.rect.origin.y,
                                      s.rect.size.width, s.rect.size.height);
    return CGPointMake(result.first, result.second);
  }
  Json tree(AXUIElementRef element, int depth, const Cancellation &cancel,
            std::chrono::steady_clock::time_point deadline) {
    cancel.check();
    if (depth > 12 || nodes_.size() >= 256 || std::chrono::steady_clock::now() >= deadline)
      return {{"truncated", true}};
    for (const auto &old : nodes_)
      if (CFEqual(old.second, element))
        return {{"element_id", old.first}};
    (void)AXUIElementSetMessagingTimeout(element, 0.05f);
    const auto id = "node-" + std::to_string(nodes_.size() + 1);
    nodes_[id] = static_cast<AXUIElementRef>(CFRetain(element));
    Json out = {{"element_id", id}, {"bounds", rectangle(bounds(element))}};
    CF<CFTypeRef> subrole(attribute(element, kAXSubroleAttribute));
    const bool secure = string_value(subrole) == "AXSecureTextField";
    const std::pair<CFStringRef, const char *> fields[] = {{kAXRoleAttribute, "role"},
                                                           {kAXTitleAttribute, "title"},
                                                           {kAXDescriptionAttribute, "description"},
                                                           {kAXValueAttribute, "value"}};
    for (const auto &f : fields) {
      cancel.check();
      if (std::chrono::steady_clock::now() >= deadline) {
        out["truncated"] = true;
        break;
      }
      if (secure && std::string(f.second) == "value")
        continue;
      CF<CFTypeRef> v(attribute(element, f.first));
      if (v.value) {
        if (CFGetTypeID(v.value) == CFStringGetTypeID())
          out[f.second] = string_value(v);
        else if (CFGetTypeID(v.value) == CFBooleanGetTypeID())
          out[f.second] = bool(CFBooleanGetValue(static_cast<CFBooleanRef>(v.value)));
        else if (CFGetTypeID(v.value) == CFNumberGetTypeID()) {
          double d = 0;
          if (CFNumberGetValue(static_cast<CFNumberRef>(v.value), kCFNumberDoubleType, &d))
            out[f.second] = d;
        }
      }
    }
    CFArrayRef actions = nullptr;
    if (AXUIElementCopyActionNames(element, &actions) == kAXErrorSuccess && actions) {
      CF<CFArrayRef> owned(actions);
      out["actions"] = Json::array();
      for (CFIndex i = 0; i < CFArrayGetCount(actions) && i < 32; ++i)
        out["actions"].push_back(string_value(CFArrayGetValueAtIndex(actions, i)));
    }
    Boolean settable = false;
    out["settable_attributes"] = Json::array();
    if (!secure &&
        AXUIElementIsAttributeSettable(element, kAXValueAttribute, &settable) == kAXErrorSuccess &&
        settable)
      out["settable_attributes"].push_back("AXValue");
    CFIndex count = 0;
    if (depth < 12 && nodes_.size() < 256 && std::chrono::steady_clock::now() < deadline &&
        AXUIElementGetAttributeValueCount(element, kAXChildrenAttribute, &count) ==
            kAXErrorSuccess) {
      CFArrayRef children = nullptr;
      if (AXUIElementCopyAttributeValues(element, kAXChildrenAttribute, 0,
                                         std::min<CFIndex>(count, 256 - nodes_.size()),
                                         &children) == kAXErrorSuccess &&
          children) {
        CF<CFArrayRef> owned(children);
        out["children"] = Json::array();
        for (CFIndex i = 0; i < CFArrayGetCount(children); ++i) {
          if (std::chrono::steady_clock::now() >= deadline) {
            out["truncated"] = true;
            break;
          }
          const auto child = static_cast<AXUIElementRef>(CFArrayGetValueAtIndex(children, i));
          if (CFGetTypeID(child) == AXUIElementGetTypeID())
            out["children"].push_back(tree(child, depth + 1, cancel, deadline));
        }
      }
    }
    return out;
  }
  DesktopResult state(const Json &a, const Cancellation &cancel) {
    clear();
    apps_.clear();
    snapshot_ = unique_id();
    observed_ = std::chrono::steady_clock::now();
    const auto deadline = observed_ + std::chrono::seconds(2);
    Json out = {{"snapshot_id", snapshot_},
                {"capabilities", capabilities()},
                {"applications", Json::array()},
                {"windows", Json::array()},
                {"displays", Json::array()}};
    CGDirectDisplayID displays[16];
    CGDisplayCount count = 0;
    if (CGGetActiveDisplayList(16, displays, &count) == kCGErrorSuccess)
      for (CGDisplayCount i = 0; i < count; ++i)
        out["displays"].push_back(
            {{"display_id", i + 1}, {"bounds", rectangle(CGDisplayBounds(displays[i]))}});
    ProcessSerialNumber front{};
    GetFrontProcess(&front);
    pid_t focus = 0;
    GetProcessPID(&front, &focus);
    out["focused_pid"] = focus;
    ProcessSerialNumber psn = {0, kNoProcess};
    while (apps_.size() < 128 && GetNextProcess(&psn) == noErr) {
      cancel.check();
      pid_t pid = 0;
      if (GetProcessPID(&psn, &pid) != noErr)
        continue;
      apps_[pid] = psn;
      CFStringRef name = nullptr;
      CopyProcessName(&psn, &name);
      CF<CFStringRef> owned(name);
      out["applications"].push_back({{"pid", pid}, {"name", string_value(name)}});
    }
    const auto target = a.value("pid", static_cast<int>(focus));
    if (ax_enabled() && target > 0) {
      CF<AXUIElementRef> app(AXUIElementCreateApplication(target));
      (void)AXUIElementSetMessagingTimeout(app, 0.05f);
      out["accessibility"] = tree(app, 0, cancel, deadline);
      CF<CFTypeRef> windows(attribute(app, kAXWindowsAttribute));
      if (windows.value && CFGetTypeID(windows.value) == CFArrayGetTypeID()) {
        auto list = static_cast<CFArrayRef>(windows.value);
        for (CFIndex i = 0;
             i < CFArrayGetCount(list) && i < 64 && std::chrono::steady_clock::now() < deadline;
             ++i) {
          const auto window = static_cast<AXUIElementRef>(CFArrayGetValueAtIndex(list, i));
          if (CFGetTypeID(window) != AXUIElementGetTypeID())
            continue;
          const auto id = snapshot_ + ":window-" + std::to_string(i + 1);
          const auto r = bounds(window);
          windows_[id] = r;
          CF<CFTypeRef> title(attribute(window, kAXTitleAttribute));
          out["windows"].push_back({{"window_id", id},
                                    {"pid", target},
                                    {"title", string_value(title)},
                                    {"bounds", rectangle(r)}});
        }
      }
    } else
      out["accessibility_error"] =
          "Enable access for assistive devices in System Preferences > Universal Access";
    out["truncated"] = nodes_.size() >= 256 || std::chrono::steady_clock::now() >= deadline;
    return {out, {}};
  }
  DesktopResult screenshot(const Json &a) {
    CGDirectDisplayID active[16];
    CGDisplayCount count = 0;
    if (CGGetActiveDisplayList(16, active, &count) != kCGErrorSuccess)
      throw Error("Cannot enumerate displays");
    std::vector<CaptureRect> displays;
    for (CGDisplayCount i = 0; i < count; ++i) {
      const auto bounds = CGDisplayBounds(active[i]);
      displays.push_back({bounds.origin.x, bounds.origin.y, bounds.size.width, bounds.size.height});
    }
    std::optional<CaptureRect> window;
    if (a.contains("window_id")) {
      const auto it = windows_.find(a.at("window_id"));
      if (it == windows_.end() ||
          std::chrono::steady_clock::now() - observed_ > std::chrono::seconds(60))
        throw Error("Expired window ID; refresh computer.get_state");
      const auto &r = it->second;
      window = CaptureRect{r.origin.x, r.origin.y, r.size.width, r.size.height};
    }
    const auto area = capture_area(displays, a, window);
    const auto display_id = area.display + 1;
    const auto display = active[area.display];
    const auto screen = CGDisplayBounds(display);
    CGRect rect = CGRectMake(area.bounds.x, area.bounds.y, area.bounds.width, area.bounds.height);
    rect.origin.x = std::floor(rect.origin.x);
    rect.origin.y = std::floor(rect.origin.y);
    rect.size.width = std::floor(rect.size.width);
    rect.size.height = std::floor(rect.size.height);
    const auto bits = CGDisplayBitsPerPixel(display), samples = CGDisplayBitsPerSample(display);
    if ((bits != 32 || samples != 8) && (bits != 16 || samples != 5))
      throw Error("Unsupported display pixel format");
    const auto *base = static_cast<const unsigned char *>(CGDisplayBaseAddress(display));
    if (!base)
      throw Error("Display framebuffer unavailable");
    const auto stride = CGDisplayBytesPerRow(display);
    const auto x = static_cast<size_t>(rect.origin.x - screen.origin.x),
               y = static_cast<size_t>(rect.origin.y - screen.origin.y);
    const auto width = static_cast<size_t>(rect.size.width),
               height = static_cast<size_t>(rect.size.height);
    auto rgb = framebuffer_rgb(base + y * stride + x * (bits / 8), width, height, stride,
                               static_cast<unsigned>(bits), true);
    auto image = encode_rgb(rgb, width, height, a.value("detail", std::string("high")));
    if (shots_.size() >= 8) {
      auto oldest =
          std::min_element(shots_.begin(), shots_.end(), [](const auto &l, const auto &r) {
            return l.second.created < r.second.created;
          });
      shots_.erase(oldest);
    }
    const auto id = unique_id();
    shots_[id] = {rect, image.width, image.height, std::chrono::steady_clock::now()};
    return {{{"screenshot_id", id},
             {"display_id", display_id},
             {"bounds", rectangle(rect)},
             {"width", image.width},
             {"height", image.height}},
            image};
  }

public:
  ~TigerComputer() override {
    clear();
  }
  Json capabilities() const override {
    CF<CFDictionaryRef> session(CGSessionCopyCurrentDictionary());
    bool active = false;
    if (session.value)
      active = CFDictionaryGetValue(session, kCGSessionOnConsoleKey) == kCFBooleanTrue &&
               CFDictionaryGetValue(session, kCGSessionLoginDoneKey) == kCFBooleanTrue;
    return {{"available", active}, {"backend", "tiger"}, {"accessibility", ax_enabled()},
            {"framebuffer", true}, {"clipboard", true},  {"applescript", true}};
  }
  DesktopResult invoke(const std::string &name, const Json &a,
                       const Cancellation &cancel) override {
    cancel.check();
    if (!capabilities().value("available", false))
      throw Error("An active Aqua console session is required");
    if (name == "get_state")
      return state(a, cancel);
    if (name == "screenshot")
      return screenshot(a);
    if (name == "clipboard_read") {
      PasteboardRef value = nullptr;
      if (PasteboardCreate(kPasteboardClipboard, &value) != noErr)
        throw Error("Clipboard unavailable");
      CF<PasteboardRef> board(value);
      PasteboardSynchronize(board);
      ItemCount count = 0;
      PasteboardGetItemCount(board, &count);
      if (count == 0)
        return {{{"text", ""}}, {}};
      PasteboardItemID item = nullptr;
      PasteboardGetItemIdentifier(board, 1, &item);
      for (const auto type :
           {CFSTR("public.utf8-plain-text"), CFSTR("public.utf16-external-plain-text")}) {
        CFDataRef data = nullptr;
        if (PasteboardCopyItemFlavorData(board, item, type, &data) == noErr && data) {
          CF<CFDataRef> owned(data);
          if (CFDataGetLength(data) > 65536)
            throw Error("Clipboard exceeds 64 KiB");
          CF<CFStringRef> text(CFStringCreateWithBytes(
              nullptr, CFDataGetBytePtr(data), CFDataGetLength(data),
              CFEqual(type, CFSTR("public.utf8-plain-text")) ? kCFStringEncodingUTF8
                                                             : kCFStringEncodingUTF16,
              true));
          auto result = string_value(text, 65536);
          if (result.size() > 65536)
            throw Error("UTF-8 clipboard text exceeds 64 KiB");
          return {{{"text", result}}, {}};
        }
      }
      throw Error("Clipboard has no supported plain text flavor");
    }
    DesktopLock lock(cancel);
    InputCleanup cleanup;
    if (name == "move" || name == "click" || name == "drag" || name == "scroll") {
      const auto p = point(a);
      cleanup.position = p;
      post_mouse(kCGEventMouseMoved, p, kCGMouseButtonLeft);
      if (name == "click") {
        const auto button = a.value("button", std::string("left"));
        const auto count = a.value("count", 1);
        if ((button != "left" && button != "right" && button != "middle") || count < 1 || count > 2)
          throw Error("Invalid button or click count");
        const auto b = button == "left"    ? kCGMouseButtonLeft
                       : button == "right" ? kCGMouseButtonRight
                                           : kCGMouseButtonCenter;
        const auto down = b == kCGMouseButtonLeft    ? kCGEventLeftMouseDown
                          : b == kCGMouseButtonRight ? kCGEventRightMouseDown
                                                     : kCGEventOtherMouseDown;
        const auto up = b == kCGMouseButtonLeft    ? kCGEventLeftMouseUp
                        : b == kCGMouseButtonRight ? kCGEventRightMouseUp
                                                   : kCGEventOtherMouseUp;
        for (int i = 1; i <= count; ++i) {
          cleanup.mouse = b;
          post_mouse(down, p, b, i);
          post_mouse(up, p, b, i);
          cleanup.mouse.reset();
        }
      } else if (name == "drag") {
        const auto end = point(a, "to_x", "to_y");
        cleanup.mouse = kCGMouseButtonLeft;
        post_mouse(kCGEventLeftMouseDown, p, kCGMouseButtonLeft);
        SystemClock clock;
        for (int i = 1; i <= 12; ++i) {
          cancel.check();
          cleanup.position =
              CGPointMake(p.x + (end.x - p.x) * i / 12, p.y + (end.y - p.y) * i / 12);
          post_mouse(kCGEventLeftMouseDragged, cleanup.position, kCGMouseButtonLeft);
          clock.sleep_ms(10, cancel);
        }
        post_mouse(kCGEventLeftMouseUp, end, kCGMouseButtonLeft);
        cleanup.mouse.reset();
      } else if (name == "scroll") {
        const auto dy = a.at("delta_y").get<int>(), dx = a.value("delta_x", 0);
        if (std::abs(static_cast<int64_t>(dy)) > 1000 || std::abs(static_cast<int64_t>(dx)) > 1000)
          throw Error("Scroll limit exceeded");
        if (CGPostScrollWheelEvent(2, dy, dx) != kCGErrorSuccess)
          throw Error("Scroll dispatch failed");
      }
    } else if (name == "type_text") {
      const auto text = a.at("text").get<std::string>();
      if (text.size() > 65536)
        throw Error("Text exceeds 64 KiB");
      CF<CFStringRef> value(CFStringCreateWithBytes(nullptr,
                                                    reinterpret_cast<const UInt8 *>(text.data()),
                                                    text.size(), kCFStringEncodingUTF8, false));
      if (!value.value)
        throw Error("Text must be valid UTF-8");
      std::vector<UniChar> units(static_cast<size_t>(CFStringGetLength(value)));
      CFStringGetCharacters(value, CFRangeMake(0, units.size()), units.data());
      for (size_t i = 0; i < units.size();) {
        cancel.check();
        size_t count = std::min(size_t{20}, units.size() - i);
        if (i + count < units.size() && units[i + count - 1] >= 0xd800 &&
            units[i + count - 1] <= 0xdbff)
          --count;
        for (bool down : {true, false}) {
          CF<CGEventRef> event(CGEventCreateKeyboardEvent(nullptr, 0, down));
          if (!event.value)
            throw Error("Cannot create text event");
          CGEventKeyboardSetUnicodeString(event, count, units.data() + i);
          CGEventPost(kCGSessionEventTap, event);
        }
        i += count;
      }
    } else if (name == "key") {
      static const std::map<std::string, CGKeyCode> codes = {
          {"a", 0},          {"s", 1},          {"d", 2},       {"f", 3},        {"h", 4},
          {"g", 5},          {"z", 6},          {"x", 7},       {"c", 8},        {"v", 9},
          {"b", 11},         {"q", 12},         {"w", 13},      {"e", 14},       {"r", 15},
          {"y", 16},         {"t", 17},         {"1", 18},      {"2", 19},       {"3", 20},
          {"4", 21},         {"6", 22},         {"5", 23},      {"9", 25},       {"7", 26},
          {"8", 28},         {"0", 29},         {"o", 31},      {"u", 32},       {"i", 34},
          {"p", 35},         {"enter", 36},     {"return", 36}, {"l", 37},       {"j", 38},
          {"k", 40},         {"n", 45},         {"m", 46},      {"tab", 48},     {"space", 49},
          {"backspace", 51}, {"escape", 53},    {"home", 115},  {"pageup", 116}, {"delete", 117},
          {"end", 119},      {"pagedown", 121}, {"left", 123},  {"right", 124},  {"down", 125},
          {"up", 126}};
      const auto key = a.at("key").get<std::string>();
      const auto it = codes.find(key);
      if (it == codes.end())
        throw Error("Unsupported key name");
      CGEventFlags flags = 0;
      for (const auto &m : a.value("modifiers", std::vector<std::string>{})) {
        CGKeyCode code = 0;
        if (m == "command") {
          code = 55;
          flags |= kCGEventFlagMaskCommand;
        } else if (m == "shift") {
          code = 56;
          flags |= kCGEventFlagMaskShift;
        } else if (m == "option") {
          code = 58;
          flags |= kCGEventFlagMaskAlternate;
        } else if (m == "control") {
          code = 59;
          flags |= kCGEventFlagMaskControl;
        } else
          throw Error("Invalid modifier");
        cleanup.keys.push_back(code);
        post_key(code, true, flags);
      }
      cleanup.keys.push_back(it->second);
      post_key(it->second, true, flags);
      post_key(it->second, false, flags);
      cleanup.keys.pop_back();
    } else if (name == "perform_action" || name == "set_value") {
      if (!ax_enabled())
        throw Error("Access for assistive devices is disabled");
      const auto element = node(a);
      if (name == "perform_action") {
        CFArrayRef actions = nullptr;
        if (AXUIElementCopyActionNames(element, &actions) != kAXErrorSuccess || !actions)
          throw Error("Node actions unavailable");
        CF<CFArrayRef> owned(actions);
        const auto text = a.at("action").get<std::string>();
        CF<CFStringRef> action(
            CFStringCreateWithCString(nullptr, text.c_str(), kCFStringEncodingUTF8));
        if (!CFArrayContainsValue(actions, CFRangeMake(0, CFArrayGetCount(actions)), action))
          throw Error("Unsupported node action");
        if (AXUIElementPerformAction(element, action) != kAXErrorSuccess)
          throw Error("Accessibility action failed; refresh state before retrying");
      } else {
        const auto attr = a.value("attribute", std::string("AXValue"));
        if (attr != "AXValue")
          throw Error("Attribute was not advertised as settable");
        Boolean settable = false;
        if (AXUIElementIsAttributeSettable(element, kAXValueAttribute, &settable) !=
                kAXErrorSuccess ||
            !settable)
          throw Error("Node value is not settable");
        CF<CFTypeRef> subrole(attribute(element, kAXSubroleAttribute));
        if (string_value(subrole) == "AXSecureTextField")
          throw Error("Secure field values are not exposed for semantic automation");
        CFTypeRef value = nullptr;
        if (a["value"].is_string()) {
          const auto text = a["value"].get<std::string>();
          if (text.size() > 65536)
            throw Error("Value limit exceeded");
          value = CFStringCreateWithBytes(nullptr, reinterpret_cast<const UInt8 *>(text.data()),
                                          text.size(), kCFStringEncodingUTF8, false);
        } else if (a["value"].is_boolean())
          value = CFRetain(a["value"].get<bool>() ? kCFBooleanTrue : kCFBooleanFalse);
        else if (a["value"].is_number()) {
          double d = a["value"];
          value = CFNumberCreate(nullptr, kCFNumberDoubleType, &d);
        }
        if (!value)
          throw Error("Unsupported accessibility value type");
        CF<CFTypeRef> owned(value);
        if (AXUIElementSetAttributeValue(element, kAXValueAttribute, value) != kAXErrorSuccess)
          throw Error("Setting value failed; refresh state before retrying");
      }
    } else if (name == "activate_app") {
      const auto pid = a.at("pid").get<int>();
      const auto it = apps_.find(pid);
      pid_t actual = 0;
      if (it == apps_.end() || GetProcessPID(&it->second, &actual) != noErr || actual != pid)
        throw Error("Application is no longer available; refresh state");
      if (SetFrontProcess(&it->second) != noErr)
        throw Error("Cannot activate application");
    } else if (name == "open_app" || name == "open_url") {
      CFURLRef raw = nullptr;
      if (name == "open_app") {
        const auto path = a.at("path").get<std::string>();
        if (path.empty() || path.front() != '/' || fs::path(path).extension() != ".app")
          throw Error("Expected absolute .app bundle path");
        raw = CFURLCreateFromFileSystemRepresentation(
            nullptr, reinterpret_cast<const UInt8 *>(path.data()), path.size(), true);
      } else {
        const auto url = a.at("url").get<std::string>();
        if (url.size() > 8192 || (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0))
          throw Error("Expected HTTP(S) URL");
        CF<CFStringRef> text(
            CFStringCreateWithCString(nullptr, url.c_str(), kCFStringEncodingUTF8));
        raw = CFURLCreateWithString(nullptr, text, nullptr);
      }
      CF<CFURLRef> url(raw);
      if (!raw || LSOpenCFURLRef(url, nullptr) != noErr)
        throw Error("LaunchServices open failed");
    } else if (name == "clipboard_write") {
      const auto text = a.at("text").get<std::string>();
      if (text.size() > 65536)
        throw Error("Clipboard exceeds 64 KiB");
      PasteboardRef raw = nullptr;
      if (PasteboardCreate(kPasteboardClipboard, &raw) != noErr)
        throw Error("Clipboard unavailable");
      CF<PasteboardRef> board(raw);
      CF<CFDataRef> data(
          CFDataCreate(nullptr, reinterpret_cast<const UInt8 *>(text.data()), text.size()));
      if (PasteboardClear(board) != noErr ||
          PasteboardPutItemFlavor(board, reinterpret_cast<PasteboardItemID>(1),
                                  CFSTR("public.utf8-plain-text"), data, 0) != noErr)
        throw Error("Clipboard write failed");
    } else if (name == "applescript") {
      const auto script = a.at("script").get<std::string>();
      const auto timeout = a.value("timeout_ms", 30000);
      if (script.empty() || script.size() > 65536 || timeout < 1 || timeout > 60000)
        throw Error("AppleScript limit exceeded");
      NativeProcesses processes;
      ProcessSpec spec;
      spec.argv = {"/usr/bin/osascript", "-e", script};
      spec.timeout_ms = timeout;
      spec.output_limit = 32768;
      const auto result = processes.run(spec, cancel);
      return {{{"exit_code", result.exit_code},
               {"output", safe_text(result.output)},
               {"truncated", result.truncated}},
              {}};
    } else
      throw Error("Unsupported desktop operation");
    cancel.check();
    return {{{"action_sent", true}, {"application_success_observed", false}}, {}};
  }
};
} // namespace
std::shared_ptr<ComputerBackend> native_computer() {
  return std::make_shared<TigerComputer>();
}
#else
namespace {
class UnavailableComputer final : public ComputerBackend {
public:
  Json capabilities() const override {
    return {
        {"available", false},
        {"backend", "unsupported"},
        {"reason", "Native desktop control requires PowerPC Tiger and an Aqua console session"}};
  }
  DesktopResult invoke(const std::string &, const Json &, const Cancellation &) override {
    throw Error("Native Tiger desktop control unavailable on this host");
  }
};
} // namespace
std::shared_ptr<ComputerBackend> native_computer() {
  return std::make_shared<UnavailableComputer>();
}
#endif
} // namespace codet
