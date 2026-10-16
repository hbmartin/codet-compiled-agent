#include "codet/tool_registry.hpp"
#include <algorithm>
#include <fnmatch.h>

namespace codet {
namespace {
Json field(const char *type) {
  return {{"type", type}};
}
Json object(const Json &properties, const Json &required = Json::array()) {
  Json s = {{"type", "object"}, {"properties", properties}, {"additionalProperties", false}};
  if (!required.empty())
    s["required"] = required;
  return s;
}
} // namespace
const std::vector<ToolDefinition> &tool_registry() {
  static const auto definitions = [] {
    std::vector<ToolDefinition> out;
    auto add = [&](const std::string &name, const std::string &description, const Json &properties,
                   const Json &required, ToolHandler handler, bool mutation = false,
                   const std::string &availability = "", const std::string &hook = "",
                   const std::string &event = "codet.tool") {
      out.push_back({name, description, object(properties, required), handler, mutation,
                     availability, hook.empty() ? name : hook, event});
    };
    add("exec_command",
        "Runs a shell command, returning output or a session ID for ongoing interaction. "
        "Application authorization only; Tiger has no OS shell sandbox. Wait defaults to 10000 ms "
        "(250-30000).",
        {{"cmd", field("string")},
         {"workdir", field("string")},
         {"shell", field("string")},
         {"login", field("boolean")},
         {"tty", field("boolean")},
         {"yield_time_ms", field("number")},
         {"max_output_tokens", field("number")},
         {"sandbox_permissions", field("string")},
         {"justification", field("string")},
         {"prefix_rule", {{"type", "array"}, {"items", field("string")}}}},
        {"cmd"}, ToolHandler::exec, true, "", "Bash", "command_execution");
    add("write_stdin",
        "Writes characters to an existing unified exec session and returns recent output. Empty "
        "chars polls. Nonempty input waits 250 ms by default; empty polls wait at least 5000 ms; "
        "cap 300000 ms.",
        {{"session_id", field("number")},
         {"chars", field("string")},
         {"yield_time_ms", field("number")},
         {"max_output_tokens", field("number")}},
        {"session_id"}, ToolHandler::stdin_write, false, "", "Bash", "command_execution");
    add("apply_patch",
        "Apply a *** Begin Patch envelope with Add/Delete/Update/Move. Supply raw patch text on "
        "custom-tool transports; use the patch property on function-only transports.",
        {{"patch", field("string")}}, {"patch"}, ToolHandler::patch, true, "", "apply_patch",
        "file_change");
    add("read_file",
        "Codet Plan-mode extension: read numbered text lines or list directory entries. Paths are "
        "workspace-relative or absolute. offset starts at 1; limit defaults to 2000.",
        {{"path", field("string")}, {"offset", field("integer")}, {"limit", field("integer")}},
        {"path"}, ToolHandler::read, false, "plan");
    add("view_image",
        "View a local image as pixels. high (default) resizes to 1280 pixels; original preserves "
        "dimensions within memory limits.",
        {{"path", field("string")},
         {"detail", {{"type", "string"}, {"enum", {"high", "original"}}}}},
        {"path"}, ToolHandler::image, false, "image");
    auto step = object(
        {{"step", field("string")},
         {"status", {{"type", "string"}, {"enum", {"pending", "in_progress", "completed"}}}}},
        {"step", "status"});
    add("update_plan",
        "Updates the task checklist. At most one step may be in_progress. Unavailable in Plan "
        "mode.",
        {{"explanation", field("string")}, {"plan", {{"type", "array"}, {"items", step}}}},
        {"plan"}, ToolHandler::plan, false, "default");
    auto option = object({{"label", field("string")}, {"description", field("string")}},
                         {"label", "description"});
    auto question = object({{"id", field("string")},
                            {"header", field("string")},
                            {"question", field("string")},
                            {"options", {{"type", "array"}, {"items", option}}}},
                           {"id", "header", "question", "options"});
    add("request_user_input",
        "Request one to three questions with two or three choices and a free-text answer. Waits "
        "for answers. Only available in interactive Plan mode.",
        {{"questions", {{"type", "array"}, {"items", question}}}}, {"questions"},
        ToolHandler::question, false, "question");
    auto async_question =
        object({{"title", field("string")},
                {"options", {{"type", "array"}, {"items", field("string")}, {"minItems", 1}}}},
               {"title"});
    add("request_user_input_async",
        "Ask questions during ongoing work. Returns immediately; replies arrive as new user "
        "messages. Suggested options never count as an answer until submitted.",
        {{"questions", {{"type", "array"}, {"items", async_question}, {"minItems", 1}}}},
        {"questions"}, ToolHandler::async_question, false, "interactive");
    add("send_message_to_user_async",
        "Send a concise message needing the user's attention during ongoing work; returns "
        "immediately and replies arrive as new user messages.",
        {{"message", field("string")}}, {"message"}, ToolHandler::message, false, "interactive");
    add("clock.curr_time", "Return the current time in UTC.", Json::object(), {},
        ToolHandler::time);
    add("clock.sleep",
        "Pause for duration_ms (1-43200000), waking on new input or cancellation. Returns elapsed "
        "wall time.",
        {{"duration_ms", field("integer")}}, {"duration_ms"}, ToolHandler::sleep);
    const auto str = field("string"), num = field("number");
    add("computer.get_state",
        "Observe applications, windows, focus, displays and a bounded accessibility tree. Node IDs "
        "belong to the returned snapshot_id.",
        {{"pid", field("integer")}}, {}, ToolHandler::computer, false, "computer");
    add("computer.screenshot",
        "Capture a display or visible region. Returns pixels and a screenshot_id. Coordinates for "
        "pointer tools refer to the returned image.",
        {{"display_id", field("integer")},
         {"window_id", str},
         {"x", num},
         {"y", num},
         {"width", num},
         {"height", num},
         {"detail", {{"type", "string"}, {"enum", {"high", "original"}}}}},
        {}, ToolHandler::computer, false, "computer_image");
    const Json point = {{"screenshot_id", str}, {"x", num}, {"y", num}};
    add("computer.move", "Move the pointer using coordinates in an observed screenshot.", point,
        {"screenshot_id", "x", "y"}, ToolHandler::computer, true, "computer");
    Json click = point;
    click["button"] = str;
    click["count"] = field("integer");
    add("computer.click",
        "Click at screenshot coordinates. button: left/right/middle; count: 1 or 2. Reports "
        "dispatch, not application success.",
        click, {"screenshot_id", "x", "y"}, ToolHandler::computer, true, "computer");
    Json drag = point;
    drag["to_x"] = num;
    drag["to_y"] = num;
    add("computer.drag", "Drag with the left mouse button between two screenshot coordinates.",
        drag, {"screenshot_id", "x", "y", "to_x", "to_y"}, ToolHandler::computer, true, "computer");
    Json scroll = point;
    scroll["delta_y"] = field("integer");
    scroll["delta_x"] = field("integer");
    add("computer.scroll",
        "Scroll wheel lines at screenshot coordinates. Positive delta_y scrolls up; positive "
        "delta_x scrolls left.",
        scroll, {"screenshot_id", "x", "y", "delta_y"}, ToolHandler::computer, true, "computer");
    add("computer.type_text",
        "Type Unicode text into the focused application without changing the clipboard.",
        {{"text", str}}, {"text"}, ToolHandler::computer, true, "computer");
    add("computer.key",
        "Dispatch an atomic key chord. key is a letter, digit or named navigation key; modifiers "
        "may be command, shift, option, control.",
        {{"key", str}, {"modifiers", {{"type", "array"}, {"items", str}}}}, {"key"},
        ToolHandler::computer, true, "computer");
    add("computer.perform_action",
        "Perform an advertised accessibility action on an observed node.",
        {{"snapshot_id", str}, {"element_id", str}, {"action", str}},
        {"snapshot_id", "element_id", "action"}, ToolHandler::computer, true, "computer");
    add("computer.set_value",
        "Set an advertised settable accessibility attribute (default AXValue).",
        {{"snapshot_id", str}, {"element_id", str}, {"attribute", str}, {"value", Json::object()}},
        {"snapshot_id", "element_id", "value"}, ToolHandler::computer, true, "computer");
    add("computer.activate_app", "Activate an observed application by PID.",
        {{"pid", field("integer")}}, {"pid"}, ToolHandler::computer, true, "computer");
    add("computer.open_app", "Open an application bundle by absolute path.", {{"path", str}},
        {"path"}, ToolHandler::computer, true, "computer");
    add("computer.open_url", "Open an HTTP(S) URL in the default browser.", {{"url", str}}, {"url"},
        ToolHandler::computer, true, "computer");
    add("computer.clipboard_read", "Read bounded plain text from the system clipboard.",
        Json::object(), {}, ToolHandler::computer, false, "computer");
    add("computer.clipboard_write", "Replace the system clipboard with plain text.",
        {{"text", str}}, {"text"}, ToolHandler::computer, true, "computer");
    add("computer.applescript",
        "Execute AppleScript through osascript with a bounded timeout. Scripts have the user's "
        "application privileges.",
        {{"script", str}, {"timeout_ms", field("integer")}}, {"script"}, ToolHandler::computer,
        true, "computer");
    return out;
  }();
  return definitions;
}
const ToolDefinition *tool_definition(const std::string &name) {
  for (const auto &d : tool_registry())
    if (d.name == name)
      return &d;
  return nullptr;
}
std::string tool_wire_name(std::string name) {
  const auto p = name.find('.');
  if (p != std::string::npos)
    name.replace(p, 1, "__");
  return name;
}
std::string tool_logical_name(std::string name) {
  for (const auto &d : tool_registry())
    if (tool_wire_name(d.name) == name)
      return d.name;
  return name;
}
void validate_tool_rules(const std::vector<std::string> &rules) {
  for (const auto &rule : rules) {
    const auto open = rule.find('(');
    const auto name = rule.substr(0, open);
    if (open != std::string::npos && (rule.empty() || rule.back() != ')'))
      throw Error("Malformed permission rule: " + rule, 2);
    const bool known =
        std::any_of(tool_registry().begin(), tool_registry().end(), [&](const ToolDefinition &d) {
          return fnmatch(name.c_str(), d.name.c_str(), 0) == 0;
        });
    if (!known)
      throw Error(
          "Unknown or removed tool in permission rule: " + name +
              ". Use exec_command, apply_patch, read_file, or qualified computer/clock names.",
          2);
  }
}
} // namespace codet
