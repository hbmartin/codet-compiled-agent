/* Inferred Tiger PPC32 declarations; names and types are editorial. */
#ifndef RECOVERED_TYPES_H
#define RECOVERED_TYPES_H
typedef unsigned int uint; typedef unsigned char byte, uchar;
typedef unsigned short ushort; typedef unsigned long ulong;
typedef long long longlong; typedef unsigned long long ulonglong;
typedef unsigned char undefined, undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned long long undefined8;
typedef void (*RecoveredStreamCallback)(const char *, const char *, void *);
/* ABI assertions deliberately require a 32-bit target, not host pointers. */
_Static_assert(sizeof(void *) == 4, "requires PPC32 pointer layout");
#pragma pack(push, 4)
typedef struct RecoveredDString RecoveredDString;
typedef struct RecoveredCJSON RecoveredCJSON;
typedef struct RecoveredToolCall RecoveredToolCall;
typedef struct RecoveredMessage RecoveredMessage;
typedef struct RecoveredProfile RecoveredProfile;
typedef struct RecoveredConfig RecoveredConfig;
typedef struct RecoveredAppPrefix RecoveredAppPrefix;
typedef struct RecoveredConversation RecoveredConversation;
typedef struct RecoveredResponse RecoveredResponse;
typedef struct RecoveredHttpBuffer RecoveredHttpBuffer;
typedef struct RecoveredSSEAdapter RecoveredSSEAdapter;
typedef struct RecoveredProviderCallbackContext RecoveredProviderCallbackContext;
typedef struct RecoveredMCPTool RecoveredMCPTool;
typedef struct RecoveredMCPServer RecoveredMCPServer;
typedef struct RecoveredTigerStat RecoveredTigerStat;
typedef struct RecoveredTigerDirent RecoveredTigerDirent;
typedef struct RecoveredChildContext RecoveredChildContext;
typedef struct RecoveredHook RecoveredHook;
typedef struct RecoveredHooks RecoveredHooks;
typedef struct RecoveredThemePair RecoveredThemePair;
typedef struct RecoveredTheme RecoveredTheme;
typedef struct RecoveredTigerUtsname RecoveredTigerUtsname;
typedef struct RecoveredHistory RecoveredHistory;
typedef struct RecoveredFileContext RecoveredFileContext;
typedef struct RecoveredCacheEntry RecoveredCacheEntry;
typedef struct RecoveredQueue RecoveredQueue;
typedef struct RecoveredPlanStep RecoveredPlanStep;
typedef struct RecoveredPlan RecoveredPlan;
typedef struct RecoveredWorker RecoveredWorker;
typedef struct RecoveredSubagent RecoveredSubagent;
typedef struct RecoveredRoute RecoveredRoute;
typedef struct RecoveredAppState RecoveredAppState;
typedef struct RecoveredExecutor RecoveredExecutor;
typedef struct RecoveredModel RecoveredModel;
typedef struct RecoveredModelCache RecoveredModelCache;
typedef struct RecoveredMCPState RecoveredMCPState;
typedef struct RecoveredUnwindImage RecoveredUnwindImage;

/* size 0xc; offsets verified against static instruction evidence. */
struct RecoveredDString {
    char * data; /* +0x0, 4 bytes */
    uint length; /* +0x4, 4 bytes */
    uint capacity; /* +0x8, 4 bytes */
};
_Static_assert(sizeof(RecoveredDString) == 12, "RecoveredDString size");
_Static_assert(__builtin_offsetof(RecoveredDString, data) == 0, "RecoveredDString.data offset");
_Static_assert(__builtin_offsetof(RecoveredDString, length) == 4, "RecoveredDString.length offset");
_Static_assert(__builtin_offsetof(RecoveredDString, capacity) == 8, "RecoveredDString.capacity offset");

/* size 0x24; offsets verified against static instruction evidence. */
struct RecoveredCJSON {
    RecoveredCJSON * next; /* +0x0, 4 bytes */
    RecoveredCJSON * previous; /* +0x4, 4 bytes */
    RecoveredCJSON * child; /* +0x8, 4 bytes */
    int type_flags; /* +0xc, 4 bytes */
    char * value_string; /* +0x10, 4 bytes */
    int value_int; /* +0x14, 4 bytes */
    double value_double; /* +0x18, 8 bytes */
    char * member_name; /* +0x20, 4 bytes */
};
_Static_assert(sizeof(RecoveredCJSON) == 36, "RecoveredCJSON size");
_Static_assert(__builtin_offsetof(RecoveredCJSON, next) == 0, "RecoveredCJSON.next offset");
_Static_assert(__builtin_offsetof(RecoveredCJSON, previous) == 4, "RecoveredCJSON.previous offset");
_Static_assert(__builtin_offsetof(RecoveredCJSON, child) == 8, "RecoveredCJSON.child offset");
_Static_assert(__builtin_offsetof(RecoveredCJSON, type_flags) == 12, "RecoveredCJSON.type_flags offset");
_Static_assert(__builtin_offsetof(RecoveredCJSON, value_string) == 16, "RecoveredCJSON.value_string offset");
_Static_assert(__builtin_offsetof(RecoveredCJSON, value_int) == 20, "RecoveredCJSON.value_int offset");
_Static_assert(__builtin_offsetof(RecoveredCJSON, value_double) == 24, "RecoveredCJSON.value_double offset");
_Static_assert(__builtin_offsetof(RecoveredCJSON, member_name) == 32, "RecoveredCJSON.member_name offset");

/* size 0x51c; offsets verified against static instruction evidence. */
struct RecoveredToolCall {
    int tool_type; /* +0x0, 4 bytes */
    char mcp_name[32]; /* +0x4, 32 bytes */
    RecoveredDString arguments[99]; /* +0x24, 1188 bytes */
    int argument_count; /* +0x4c8, 4 bytes */
    char call_id[64]; /* +0x4cc, 64 bytes */
    int execution_status; /* +0x50c, 4 bytes */
    RecoveredDString result; /* +0x510, 12 bytes */
};
_Static_assert(sizeof(RecoveredToolCall) == 1308, "RecoveredToolCall size");
_Static_assert(__builtin_offsetof(RecoveredToolCall, tool_type) == 0, "RecoveredToolCall.tool_type offset");
_Static_assert(__builtin_offsetof(RecoveredToolCall, mcp_name) == 4, "RecoveredToolCall.mcp_name offset");
_Static_assert(__builtin_offsetof(RecoveredToolCall, arguments) == 36, "RecoveredToolCall.arguments offset");
_Static_assert(__builtin_offsetof(RecoveredToolCall, argument_count) == 1224, "RecoveredToolCall.argument_count offset");
_Static_assert(__builtin_offsetof(RecoveredToolCall, call_id) == 1228, "RecoveredToolCall.call_id offset");
_Static_assert(__builtin_offsetof(RecoveredToolCall, execution_status) == 1292, "RecoveredToolCall.execution_status offset");
_Static_assert(__builtin_offsetof(RecoveredToolCall, result) == 1296, "RecoveredToolCall.result offset");

/* size 0x5c; offsets verified against static instruction evidence. */
struct RecoveredMessage {
    int role; /* +0x0, 4 bytes */
    RecoveredDString content; /* +0x4, 12 bytes */
    RecoveredToolCall * tool_calls; /* +0x10, 4 bytes */
    int tool_call_count; /* +0x14, 4 bytes */
    int tool_call_capacity; /* +0x18, 4 bytes */
    char call_id[64]; /* +0x1c, 64 bytes */
};
_Static_assert(sizeof(RecoveredMessage) == 92, "RecoveredMessage size");
_Static_assert(__builtin_offsetof(RecoveredMessage, role) == 0, "RecoveredMessage.role offset");
_Static_assert(__builtin_offsetof(RecoveredMessage, content) == 4, "RecoveredMessage.content offset");
_Static_assert(__builtin_offsetof(RecoveredMessage, tool_calls) == 16, "RecoveredMessage.tool_calls offset");
_Static_assert(__builtin_offsetof(RecoveredMessage, tool_call_count) == 20, "RecoveredMessage.tool_call_count offset");
_Static_assert(__builtin_offsetof(RecoveredMessage, tool_call_capacity) == 24, "RecoveredMessage.tool_call_capacity offset");
_Static_assert(__builtin_offsetof(RecoveredMessage, call_id) == 28, "RecoveredMessage.call_id offset");

/* size 0x3c8; offsets verified against static instruction evidence. */
struct RecoveredProfile {
    char name[64]; /* +0x0, 64 bytes */
    int provider; /* +0x40, 4 bytes */
    char model[128]; /* +0x44, 128 bytes */
    char api_key[256]; /* +0xc4, 256 bytes */
    char base_url[512]; /* +0x1c4, 512 bytes */
    int context_window; /* +0x3c4, 4 bytes */
};
_Static_assert(sizeof(RecoveredProfile) == 968, "RecoveredProfile size");
_Static_assert(__builtin_offsetof(RecoveredProfile, name) == 0, "RecoveredProfile.name offset");
_Static_assert(__builtin_offsetof(RecoveredProfile, provider) == 64, "RecoveredProfile.provider offset");
_Static_assert(__builtin_offsetof(RecoveredProfile, model) == 68, "RecoveredProfile.model offset");
_Static_assert(__builtin_offsetof(RecoveredProfile, api_key) == 196, "RecoveredProfile.api_key offset");
_Static_assert(__builtin_offsetof(RecoveredProfile, base_url) == 452, "RecoveredProfile.base_url offset");
_Static_assert(__builtin_offsetof(RecoveredProfile, context_window) == 964, "RecoveredProfile.context_window offset");

/* size 0x3668; offsets verified against static instruction evidence. */
struct RecoveredConfig {
    int provider; /* +0x0, 4 bytes */
    char active_profile[64]; /* +0x4, 64 bytes */
    char model[128]; /* +0x44, 128 bytes */
    char api_key[256]; /* +0xc4, 256 bytes */
    char openai_key[256]; /* +0x1c4, 256 bytes */
    char anthropic_key[256]; /* +0x2c4, 256 bytes */
    char google_key[256]; /* +0x3c4, 256 bytes */
    char ollama_key[256]; /* +0x4c4, 256 bytes */
    char base_url[512]; /* +0x5c4, 512 bytes */
    char ollama_url[512]; /* +0x7c4, 512 bytes */
    char search_url[512]; /* +0x9c4, 512 bytes */
    char working_directory[1024]; /* +0xbc4, 1024 bytes */
    RecoveredDString system_prompt; /* +0xfc4, 12 bytes */
    int max_tokens; /* +0xfd0, 4 bytes */
    int context_window; /* +0xfd4, 4 bytes */
    float temperature; /* +0xfd8, 4 bytes */
    int verbose; /* +0xfdc, 4 bytes */
    int auto_approve; /* +0xfe0, 4 bytes */
    int auto_compact; /* +0xfe4, 4 bytes */
    int auto_save_session; /* +0xfe8, 4 bytes */
    int small_model_mode; /* +0xfec, 4 bytes */
    int unknown_0ff0; /* +0xff0, 4 bytes */
    int effort; /* +0xff4, 4 bytes */
    int think_budget; /* +0xff8, 4 bytes */
    int write_guard_enforce; /* +0xffc, 4 bytes */
    int sidebar_depth; /* +0x1000, 4 bytes */
    char theme[32]; /* +0x1004, 32 bytes */
    char mcp_servers[4][512]; /* +0x1024, 2048 bytes */
    RecoveredProfile profiles[8]; /* +0x1824, 7744 bytes */
    int profile_count; /* +0x3664, 4 bytes */
};
_Static_assert(sizeof(RecoveredConfig) == 13928, "RecoveredConfig size");
_Static_assert(__builtin_offsetof(RecoveredConfig, provider) == 0, "RecoveredConfig.provider offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, active_profile) == 4, "RecoveredConfig.active_profile offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, model) == 68, "RecoveredConfig.model offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, api_key) == 196, "RecoveredConfig.api_key offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, openai_key) == 452, "RecoveredConfig.openai_key offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, anthropic_key) == 708, "RecoveredConfig.anthropic_key offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, google_key) == 964, "RecoveredConfig.google_key offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, ollama_key) == 1220, "RecoveredConfig.ollama_key offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, base_url) == 1476, "RecoveredConfig.base_url offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, ollama_url) == 1988, "RecoveredConfig.ollama_url offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, search_url) == 2500, "RecoveredConfig.search_url offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, working_directory) == 3012, "RecoveredConfig.working_directory offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, system_prompt) == 4036, "RecoveredConfig.system_prompt offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, max_tokens) == 4048, "RecoveredConfig.max_tokens offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, context_window) == 4052, "RecoveredConfig.context_window offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, temperature) == 4056, "RecoveredConfig.temperature offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, verbose) == 4060, "RecoveredConfig.verbose offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, auto_approve) == 4064, "RecoveredConfig.auto_approve offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, auto_compact) == 4068, "RecoveredConfig.auto_compact offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, auto_save_session) == 4072, "RecoveredConfig.auto_save_session offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, small_model_mode) == 4076, "RecoveredConfig.small_model_mode offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, unknown_0ff0) == 4080, "RecoveredConfig.unknown_0ff0 offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, effort) == 4084, "RecoveredConfig.effort offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, think_budget) == 4088, "RecoveredConfig.think_budget offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, write_guard_enforce) == 4092, "RecoveredConfig.write_guard_enforce offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, sidebar_depth) == 4096, "RecoveredConfig.sidebar_depth offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, theme) == 4100, "RecoveredConfig.theme offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, mcp_servers) == 4132, "RecoveredConfig.mcp_servers offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, profiles) == 6180, "RecoveredConfig.profiles offset");
_Static_assert(__builtin_offsetof(RecoveredConfig, profile_count) == 13924, "RecoveredConfig.profile_count offset");

/* size 0xc078; offsets verified against static instruction evidence. */
struct RecoveredAppPrefix {
    char unknown_prefix[8]; /* +0x0, 8 bytes */
    RecoveredMessage messages[384]; /* +0x8, 35328 bytes */
    int message_count; /* +0x8a08, 4 bytes */
    uint unknown_accounting; /* +0x8a0c, 4 bytes */
    RecoveredConfig config; /* +0x8a10, 13928 bytes */
};
_Static_assert(sizeof(RecoveredAppPrefix) == 49272, "RecoveredAppPrefix size");
_Static_assert(__builtin_offsetof(RecoveredAppPrefix, unknown_prefix) == 0, "RecoveredAppPrefix.unknown_prefix offset");
_Static_assert(__builtin_offsetof(RecoveredAppPrefix, messages) == 8, "RecoveredAppPrefix.messages offset");
_Static_assert(__builtin_offsetof(RecoveredAppPrefix, message_count) == 35336, "RecoveredAppPrefix.message_count offset");
_Static_assert(__builtin_offsetof(RecoveredAppPrefix, unknown_accounting) == 35340, "RecoveredAppPrefix.unknown_accounting offset");
_Static_assert(__builtin_offsetof(RecoveredAppPrefix, config) == 35344, "RecoveredAppPrefix.config offset");

/* size 0x8a08; offsets verified against static instruction evidence. */
struct RecoveredConversation {
    RecoveredMessage messages[384]; /* +0x0, 35328 bytes */
    int message_count; /* +0x8a00, 4 bytes */
    uint estimated_tokens; /* +0x8a04, 4 bytes */
};
_Static_assert(sizeof(RecoveredConversation) == 35336, "RecoveredConversation size");
_Static_assert(__builtin_offsetof(RecoveredConversation, messages) == 0, "RecoveredConversation.messages offset");
_Static_assert(__builtin_offsetof(RecoveredConversation, message_count) == 35328, "RecoveredConversation.message_count offset");
_Static_assert(__builtin_offsetof(RecoveredConversation, estimated_tokens) == 35332, "RecoveredConversation.estimated_tokens offset");

/* size 0x1fa0c; offsets verified against static instruction evidence. */
struct RecoveredResponse {
    RecoveredDString content; /* +0x0, 12 bytes */
    RecoveredDString error_or_reasoning; /* +0xc, 12 bytes */
    uint unknown_18; /* +0x18, 4 bytes */
    RecoveredToolCall tool_calls[99]; /* +0x1c, 129492 bytes */
    int tool_call_count; /* +0x1f9f0, 4 bytes */
    int completion_flag; /* +0x1f9f4, 4 bytes */
    int finish_reason; /* +0x1f9f8, 4 bytes */
    int input_tokens; /* +0x1f9fc, 4 bytes */
    int output_tokens; /* +0x1fa00, 4 bytes */
    int total_tokens; /* +0x1fa04, 4 bytes */
    int usage_known; /* +0x1fa08, 4 bytes */
};
_Static_assert(sizeof(RecoveredResponse) == 129548, "RecoveredResponse size");
_Static_assert(__builtin_offsetof(RecoveredResponse, content) == 0, "RecoveredResponse.content offset");
_Static_assert(__builtin_offsetof(RecoveredResponse, error_or_reasoning) == 12, "RecoveredResponse.error_or_reasoning offset");
_Static_assert(__builtin_offsetof(RecoveredResponse, unknown_18) == 24, "RecoveredResponse.unknown_18 offset");
_Static_assert(__builtin_offsetof(RecoveredResponse, tool_calls) == 28, "RecoveredResponse.tool_calls offset");
_Static_assert(__builtin_offsetof(RecoveredResponse, tool_call_count) == 129520, "RecoveredResponse.tool_call_count offset");
_Static_assert(__builtin_offsetof(RecoveredResponse, completion_flag) == 129524, "RecoveredResponse.completion_flag offset");
_Static_assert(__builtin_offsetof(RecoveredResponse, finish_reason) == 129528, "RecoveredResponse.finish_reason offset");
_Static_assert(__builtin_offsetof(RecoveredResponse, input_tokens) == 129532, "RecoveredResponse.input_tokens offset");
_Static_assert(__builtin_offsetof(RecoveredResponse, output_tokens) == 129536, "RecoveredResponse.output_tokens offset");
_Static_assert(__builtin_offsetof(RecoveredResponse, total_tokens) == 129540, "RecoveredResponse.total_tokens offset");
_Static_assert(__builtin_offsetof(RecoveredResponse, usage_known) == 129544, "RecoveredResponse.usage_known offset");

/* size 0xc; offsets verified against static instruction evidence. */
struct RecoveredHttpBuffer {
    char * data; /* +0x0, 4 bytes */
    uint length; /* +0x4, 4 bytes */
    uint capacity; /* +0x8, 4 bytes */
};
_Static_assert(sizeof(RecoveredHttpBuffer) == 12, "RecoveredHttpBuffer size");
_Static_assert(__builtin_offsetof(RecoveredHttpBuffer, data) == 0, "RecoveredHttpBuffer.data offset");
_Static_assert(__builtin_offsetof(RecoveredHttpBuffer, length) == 4, "RecoveredHttpBuffer.length offset");
_Static_assert(__builtin_offsetof(RecoveredHttpBuffer, capacity) == 8, "RecoveredHttpBuffer.capacity offset");

/* size 0x14; offsets verified against static instruction evidence. */
struct RecoveredSSEAdapter {
    RecoveredStreamCallback callback; /* +0x0, 4 bytes */
    void * context; /* +0x4, 4 bytes */
    RecoveredHttpBuffer raw_response; /* +0x8, 12 bytes */
};
_Static_assert(sizeof(RecoveredSSEAdapter) == 20, "RecoveredSSEAdapter size");
_Static_assert(__builtin_offsetof(RecoveredSSEAdapter, callback) == 0, "RecoveredSSEAdapter.callback offset");
_Static_assert(__builtin_offsetof(RecoveredSSEAdapter, context) == 4, "RecoveredSSEAdapter.context offset");
_Static_assert(__builtin_offsetof(RecoveredSSEAdapter, raw_response) == 8, "RecoveredSSEAdapter.raw_response offset");

/* size 0xc; offsets verified against static instruction evidence. */
struct RecoveredProviderCallbackContext {
    RecoveredResponse * response; /* +0x0, 4 bytes */
    RecoveredStreamCallback callback; /* +0x4, 4 bytes */
    void * context; /* +0x8, 4 bytes */
};
_Static_assert(sizeof(RecoveredProviderCallbackContext) == 12, "RecoveredProviderCallbackContext size");
_Static_assert(__builtin_offsetof(RecoveredProviderCallbackContext, response) == 0, "RecoveredProviderCallbackContext.response offset");
_Static_assert(__builtin_offsetof(RecoveredProviderCallbackContext, callback) == 4, "RecoveredProviderCallbackContext.callback offset");
_Static_assert(__builtin_offsetof(RecoveredProviderCallbackContext, context) == 8, "RecoveredProviderCallbackContext.context offset");

/* size 0x1240; offsets verified against static instruction evidence. */
struct RecoveredMCPTool {
    char name[64]; /* +0x0, 64 bytes */
    char description[512]; /* +0x40, 512 bytes */
    char parameters_json[4096]; /* +0x240, 4096 bytes */
};
_Static_assert(sizeof(RecoveredMCPTool) == 4672, "RecoveredMCPTool size");
_Static_assert(__builtin_offsetof(RecoveredMCPTool, name) == 0, "RecoveredMCPTool.name offset");
_Static_assert(__builtin_offsetof(RecoveredMCPTool, description) == 64, "RecoveredMCPTool.description offset");
_Static_assert(__builtin_offsetof(RecoveredMCPTool, parameters_json) == 576, "RecoveredMCPTool.parameters_json offset");

/* size 0x24d60; offsets verified against static instruction evidence. */
struct RecoveredMCPServer {
    char name[64]; /* +0x0, 64 bytes */
    int transport; /* +0x40, 4 bytes */
    char base_url[512]; /* +0x44, 512 bytes */
    char command[512]; /* +0x244, 512 bytes */
    char token[256]; /* +0x444, 256 bytes */
    RecoveredMCPTool tools[32]; /* +0x544, 149504 bytes */
    int tool_count; /* +0x24d44, 4 bytes */
    int connected; /* +0x24d48, 4 bytes */
    int pid; /* +0x24d4c, 4 bytes */
    int request_pipe[2]; /* +0x24d50, 8 bytes */
    int response_pipe[2]; /* +0x24d58, 8 bytes */
};
_Static_assert(sizeof(RecoveredMCPServer) == 150880, "RecoveredMCPServer size");
_Static_assert(__builtin_offsetof(RecoveredMCPServer, name) == 0, "RecoveredMCPServer.name offset");
_Static_assert(__builtin_offsetof(RecoveredMCPServer, transport) == 64, "RecoveredMCPServer.transport offset");
_Static_assert(__builtin_offsetof(RecoveredMCPServer, base_url) == 68, "RecoveredMCPServer.base_url offset");
_Static_assert(__builtin_offsetof(RecoveredMCPServer, command) == 580, "RecoveredMCPServer.command offset");
_Static_assert(__builtin_offsetof(RecoveredMCPServer, token) == 1092, "RecoveredMCPServer.token offset");
_Static_assert(__builtin_offsetof(RecoveredMCPServer, tools) == 1348, "RecoveredMCPServer.tools offset");
_Static_assert(__builtin_offsetof(RecoveredMCPServer, tool_count) == 150852, "RecoveredMCPServer.tool_count offset");
_Static_assert(__builtin_offsetof(RecoveredMCPServer, connected) == 150856, "RecoveredMCPServer.connected offset");
_Static_assert(__builtin_offsetof(RecoveredMCPServer, pid) == 150860, "RecoveredMCPServer.pid offset");
_Static_assert(__builtin_offsetof(RecoveredMCPServer, request_pipe) == 150864, "RecoveredMCPServer.request_pipe offset");
_Static_assert(__builtin_offsetof(RecoveredMCPServer, response_pipe) == 150872, "RecoveredMCPServer.response_pipe offset");

/* size 0x60; offsets verified against static instruction evidence. */
struct RecoveredTigerStat {
    int device; /* +0x0, 4 bytes */
    uint inode; /* +0x4, 4 bytes */
    ushort mode; /* +0x8, 2 bytes */
    ushort link_count; /* +0xa, 2 bytes */
    uint uid; /* +0xc, 4 bytes */
    uint gid; /* +0x10, 4 bytes */
    int rdev; /* +0x14, 4 bytes */
    int atime_seconds; /* +0x18, 4 bytes */
    int atime_nanoseconds; /* +0x1c, 4 bytes */
    int mtime_seconds; /* +0x20, 4 bytes */
    int mtime_nanoseconds; /* +0x24, 4 bytes */
    int ctime_seconds; /* +0x28, 4 bytes */
    int ctime_nanoseconds; /* +0x2c, 4 bytes */
    longlong size; /* +0x30, 8 bytes */
    longlong blocks; /* +0x38, 8 bytes */
    int block_size; /* +0x40, 4 bytes */
    uint flags; /* +0x44, 4 bytes */
    uint generation; /* +0x48, 4 bytes */
    int reserved_4c; /* +0x4c, 4 bytes */
    char reserved_50[16]; /* +0x50, 16 bytes */
};
_Static_assert(sizeof(RecoveredTigerStat) == 96, "RecoveredTigerStat size");
_Static_assert(__builtin_offsetof(RecoveredTigerStat, device) == 0, "RecoveredTigerStat.device offset");
_Static_assert(__builtin_offsetof(RecoveredTigerStat, inode) == 4, "RecoveredTigerStat.inode offset");
_Static_assert(__builtin_offsetof(RecoveredTigerStat, mode) == 8, "RecoveredTigerStat.mode offset");
_Static_assert(__builtin_offsetof(RecoveredTigerStat, link_count) == 10, "RecoveredTigerStat.link_count offset");
_Static_assert(__builtin_offsetof(RecoveredTigerStat, uid) == 12, "RecoveredTigerStat.uid offset");
_Static_assert(__builtin_offsetof(RecoveredTigerStat, gid) == 16, "RecoveredTigerStat.gid offset");
_Static_assert(__builtin_offsetof(RecoveredTigerStat, rdev) == 20, "RecoveredTigerStat.rdev offset");
_Static_assert(__builtin_offsetof(RecoveredTigerStat, atime_seconds) == 24, "RecoveredTigerStat.atime_seconds offset");
_Static_assert(__builtin_offsetof(RecoveredTigerStat, atime_nanoseconds) == 28, "RecoveredTigerStat.atime_nanoseconds offset");
_Static_assert(__builtin_offsetof(RecoveredTigerStat, mtime_seconds) == 32, "RecoveredTigerStat.mtime_seconds offset");
_Static_assert(__builtin_offsetof(RecoveredTigerStat, mtime_nanoseconds) == 36, "RecoveredTigerStat.mtime_nanoseconds offset");
_Static_assert(__builtin_offsetof(RecoveredTigerStat, ctime_seconds) == 40, "RecoveredTigerStat.ctime_seconds offset");
_Static_assert(__builtin_offsetof(RecoveredTigerStat, ctime_nanoseconds) == 44, "RecoveredTigerStat.ctime_nanoseconds offset");
_Static_assert(__builtin_offsetof(RecoveredTigerStat, size) == 48, "RecoveredTigerStat.size offset");
_Static_assert(__builtin_offsetof(RecoveredTigerStat, blocks) == 56, "RecoveredTigerStat.blocks offset");
_Static_assert(__builtin_offsetof(RecoveredTigerStat, block_size) == 64, "RecoveredTigerStat.block_size offset");
_Static_assert(__builtin_offsetof(RecoveredTigerStat, flags) == 68, "RecoveredTigerStat.flags offset");
_Static_assert(__builtin_offsetof(RecoveredTigerStat, generation) == 72, "RecoveredTigerStat.generation offset");
_Static_assert(__builtin_offsetof(RecoveredTigerStat, reserved_4c) == 76, "RecoveredTigerStat.reserved_4c offset");
_Static_assert(__builtin_offsetof(RecoveredTigerStat, reserved_50) == 80, "RecoveredTigerStat.reserved_50 offset");

/* size 0x108; offsets verified against static instruction evidence. */
struct RecoveredTigerDirent {
    uint inode; /* +0x0, 4 bytes */
    ushort record_length; /* +0x4, 2 bytes */
    uchar type; /* +0x6, 1 bytes */
    uchar name_length; /* +0x7, 1 bytes */
    char name[256]; /* +0x8, 256 bytes */
};
_Static_assert(sizeof(RecoveredTigerDirent) == 264, "RecoveredTigerDirent size");
_Static_assert(__builtin_offsetof(RecoveredTigerDirent, inode) == 0, "RecoveredTigerDirent.inode offset");
_Static_assert(__builtin_offsetof(RecoveredTigerDirent, record_length) == 4, "RecoveredTigerDirent.record_length offset");
_Static_assert(__builtin_offsetof(RecoveredTigerDirent, type) == 6, "RecoveredTigerDirent.type offset");
_Static_assert(__builtin_offsetof(RecoveredTigerDirent, name_length) == 7, "RecoveredTigerDirent.name_length offset");
_Static_assert(__builtin_offsetof(RecoveredTigerDirent, name) == 8, "RecoveredTigerDirent.name offset");

/* size 0x10; offsets verified against static instruction evidence. */
struct RecoveredChildContext {
    int event_fd; /* +0x0, 4 bytes */
    int approval_fd; /* +0x4, 4 bytes */
    int opaque_8; /* +0x8, 4 bytes */
    int auto_approve; /* +0xc, 4 bytes */
};
_Static_assert(sizeof(RecoveredChildContext) == 16, "RecoveredChildContext size");
_Static_assert(__builtin_offsetof(RecoveredChildContext, event_fd) == 0, "RecoveredChildContext.event_fd offset");
_Static_assert(__builtin_offsetof(RecoveredChildContext, approval_fd) == 4, "RecoveredChildContext.approval_fd offset");
_Static_assert(__builtin_offsetof(RecoveredChildContext, opaque_8) == 8, "RecoveredChildContext.opaque_8 offset");
_Static_assert(__builtin_offsetof(RecoveredChildContext, auto_approve) == 12, "RecoveredChildContext.auto_approve offset");

/* size 0x404; offsets verified against static instruction evidence. */
struct RecoveredHook {
    char path[1024]; /* +0x0, 1024 bytes */
    int enabled; /* +0x400, 4 bytes */
};
_Static_assert(sizeof(RecoveredHook) == 1028, "RecoveredHook size");
_Static_assert(__builtin_offsetof(RecoveredHook, path) == 0, "RecoveredHook.path offset");
_Static_assert(__builtin_offsetof(RecoveredHook, enabled) == 1024, "RecoveredHook.enabled offset");

/* size 0xc0c; offsets verified against static instruction evidence. */
struct RecoveredHooks {
    RecoveredHook slots[3]; /* +0x0, 3084 bytes */
};
_Static_assert(sizeof(RecoveredHooks) == 3084, "RecoveredHooks size");
_Static_assert(__builtin_offsetof(RecoveredHooks, slots) == 0, "RecoveredHooks.slots offset");

/* size 0x8; offsets verified against static instruction evidence. */
struct RecoveredThemePair {
    short foreground; /* +0x0, 2 bytes */
    short background; /* +0x2, 2 bytes */
    uint attributes; /* +0x4, 4 bytes */
};
_Static_assert(sizeof(RecoveredThemePair) == 8, "RecoveredThemePair size");
_Static_assert(__builtin_offsetof(RecoveredThemePair, foreground) == 0, "RecoveredThemePair.foreground offset");
_Static_assert(__builtin_offsetof(RecoveredThemePair, background) == 2, "RecoveredThemePair.background offset");
_Static_assert(__builtin_offsetof(RecoveredThemePair, attributes) == 4, "RecoveredThemePair.attributes offset");

/* size 0xa8; offsets verified against static instruction evidence. */
struct RecoveredTheme {
    char * name; /* +0x0, 4 bytes */
    char * description; /* +0x4, 4 bytes */
    RecoveredThemePair pairs[20]; /* +0x8, 160 bytes */
};
_Static_assert(sizeof(RecoveredTheme) == 168, "RecoveredTheme size");
_Static_assert(__builtin_offsetof(RecoveredTheme, name) == 0, "RecoveredTheme.name offset");
_Static_assert(__builtin_offsetof(RecoveredTheme, description) == 4, "RecoveredTheme.description offset");
_Static_assert(__builtin_offsetof(RecoveredTheme, pairs) == 8, "RecoveredTheme.pairs offset");

/* size 0x500; offsets verified against static instruction evidence. */
struct RecoveredTigerUtsname {
    char opaque_sysname[256]; /* +0x0, 256 bytes */
    char opaque_nodename[256]; /* +0x100, 256 bytes */
    char release[256]; /* +0x200, 256 bytes */
    char opaque_version[256]; /* +0x300, 256 bytes */
    char machine[256]; /* +0x400, 256 bytes */
};
_Static_assert(sizeof(RecoveredTigerUtsname) == 1280, "RecoveredTigerUtsname size");
_Static_assert(__builtin_offsetof(RecoveredTigerUtsname, opaque_sysname) == 0, "RecoveredTigerUtsname.opaque_sysname offset");
_Static_assert(__builtin_offsetof(RecoveredTigerUtsname, opaque_nodename) == 256, "RecoveredTigerUtsname.opaque_nodename offset");
_Static_assert(__builtin_offsetof(RecoveredTigerUtsname, release) == 512, "RecoveredTigerUtsname.release offset");
_Static_assert(__builtin_offsetof(RecoveredTigerUtsname, opaque_version) == 768, "RecoveredTigerUtsname.opaque_version offset");
_Static_assert(__builtin_offsetof(RecoveredTigerUtsname, machine) == 1024, "RecoveredTigerUtsname.machine offset");

/* size 0x80000c; offsets verified against static instruction evidence. */
struct RecoveredHistory {
    char entries[256][32768]; /* +0x0, 8388608 bytes */
    int first; /* +0x800000, 4 bytes */
    int count; /* +0x800004, 4 bytes */
    int cursor; /* +0x800008, 4 bytes */
};
_Static_assert(sizeof(RecoveredHistory) == 8388620, "RecoveredHistory size");
_Static_assert(__builtin_offsetof(RecoveredHistory, entries) == 0, "RecoveredHistory.entries offset");
_Static_assert(__builtin_offsetof(RecoveredHistory, first) == 8388608, "RecoveredHistory.first offset");
_Static_assert(__builtin_offsetof(RecoveredHistory, count) == 8388612, "RecoveredHistory.count offset");
_Static_assert(__builtin_offsetof(RecoveredHistory, cursor) == 8388616, "RecoveredHistory.cursor offset");

/* size 0x418; offsets verified against static instruction evidence. */
struct RecoveredFileContext {
    char path[1024]; /* +0x0, 1024 bytes */
    uint timestamp_high; /* +0x400, 4 bytes */
    uint timestamp_low; /* +0x404, 4 bytes */
    uint bytes_read; /* +0x408, 4 bytes */
    int first_message; /* +0x40c, 4 bytes */
    int last_message; /* +0x410, 4 bytes */
    uint flags; /* +0x414, 4 bytes */
};
_Static_assert(sizeof(RecoveredFileContext) == 1048, "RecoveredFileContext size");
_Static_assert(__builtin_offsetof(RecoveredFileContext, path) == 0, "RecoveredFileContext.path offset");
_Static_assert(__builtin_offsetof(RecoveredFileContext, timestamp_high) == 1024, "RecoveredFileContext.timestamp_high offset");
_Static_assert(__builtin_offsetof(RecoveredFileContext, timestamp_low) == 1028, "RecoveredFileContext.timestamp_low offset");
_Static_assert(__builtin_offsetof(RecoveredFileContext, bytes_read) == 1032, "RecoveredFileContext.bytes_read offset");
_Static_assert(__builtin_offsetof(RecoveredFileContext, first_message) == 1036, "RecoveredFileContext.first_message offset");
_Static_assert(__builtin_offsetof(RecoveredFileContext, last_message) == 1040, "RecoveredFileContext.last_message offset");
_Static_assert(__builtin_offsetof(RecoveredFileContext, flags) == 1044, "RecoveredFileContext.flags offset");

/* size 0x18; offsets verified against static instruction evidence. */
struct RecoveredCacheEntry {
    uint key; /* +0x0, 4 bytes */
    int tool_type; /* +0x4, 4 bytes */
    int valid; /* +0x8, 4 bytes */
    RecoveredDString result; /* +0xc, 12 bytes */
};
_Static_assert(sizeof(RecoveredCacheEntry) == 24, "RecoveredCacheEntry size");
_Static_assert(__builtin_offsetof(RecoveredCacheEntry, key) == 0, "RecoveredCacheEntry.key offset");
_Static_assert(__builtin_offsetof(RecoveredCacheEntry, tool_type) == 4, "RecoveredCacheEntry.tool_type offset");
_Static_assert(__builtin_offsetof(RecoveredCacheEntry, valid) == 8, "RecoveredCacheEntry.valid offset");
_Static_assert(__builtin_offsetof(RecoveredCacheEntry, result) == 12, "RecoveredCacheEntry.result offset");

/* size 0x200008; offsets verified against static instruction evidence. */
struct RecoveredQueue {
    char entries[8][262144]; /* +0x0, 2097152 bytes */
    int first; /* +0x200000, 4 bytes */
    int count; /* +0x200004, 4 bytes */
};
_Static_assert(sizeof(RecoveredQueue) == 2097160, "RecoveredQueue size");
_Static_assert(__builtin_offsetof(RecoveredQueue, entries) == 0, "RecoveredQueue.entries offset");
_Static_assert(__builtin_offsetof(RecoveredQueue, first) == 2097152, "RecoveredQueue.first offset");
_Static_assert(__builtin_offsetof(RecoveredQueue, count) == 2097156, "RecoveredQueue.count offset");

/* size 0x2808; offsets verified against static instruction evidence. */
struct RecoveredPlanStep {
    char description[4096]; /* +0x0, 4096 bytes */
    int tool_type; /* +0x1000, 4 bytes */
    char argument0[1024]; /* +0x1004, 1024 bytes */
    char argument1[1024]; /* +0x1404, 1024 bytes */
    int status; /* +0x1804, 4 bytes */
    char result[4096]; /* +0x1808, 4096 bytes */
};
_Static_assert(sizeof(RecoveredPlanStep) == 10248, "RecoveredPlanStep size");
_Static_assert(__builtin_offsetof(RecoveredPlanStep, description) == 0, "RecoveredPlanStep.description offset");
_Static_assert(__builtin_offsetof(RecoveredPlanStep, tool_type) == 4096, "RecoveredPlanStep.tool_type offset");
_Static_assert(__builtin_offsetof(RecoveredPlanStep, argument0) == 4100, "RecoveredPlanStep.argument0 offset");
_Static_assert(__builtin_offsetof(RecoveredPlanStep, argument1) == 5124, "RecoveredPlanStep.argument1 offset");
_Static_assert(__builtin_offsetof(RecoveredPlanStep, status) == 6148, "RecoveredPlanStep.status offset");
_Static_assert(__builtin_offsetof(RecoveredPlanStep, result) == 6152, "RecoveredPlanStep.result offset");

/* size 0x5020c; offsets verified against static instruction evidence. */
struct RecoveredPlan {
    char title[256]; /* +0x0, 256 bytes */
    RecoveredPlanStep steps[32]; /* +0x100, 327936 bytes */
    int count; /* +0x50200, 4 bytes */
    int id; /* +0x50204, 4 bytes */
    int unknown_50208; /* +0x50208, 4 bytes */
};
_Static_assert(sizeof(RecoveredPlan) == 328204, "RecoveredPlan size");
_Static_assert(__builtin_offsetof(RecoveredPlan, title) == 0, "RecoveredPlan.title offset");
_Static_assert(__builtin_offsetof(RecoveredPlan, steps) == 256, "RecoveredPlan.steps offset");
_Static_assert(__builtin_offsetof(RecoveredPlan, count) == 328192, "RecoveredPlan.count offset");
_Static_assert(__builtin_offsetof(RecoveredPlan, id) == 328196, "RecoveredPlan.id offset");
_Static_assert(__builtin_offsetof(RecoveredPlan, unknown_50208) == 328200, "RecoveredPlan.unknown_50208 offset");

/* size 0x220; offsets verified against static instruction evidence. */
struct RecoveredWorker {
    int state; /* +0x0, 4 bytes */
    int pid; /* +0x4, 4 bytes */
    int event_pipe[2]; /* +0x8, 8 bytes */
    int approval_pipe[2]; /* +0x10, 8 bytes */
    int exit_status; /* +0x18, 4 bytes */
    int unknown_1c; /* +0x1c, 4 bytes */
    char opaque_20[512]; /* +0x20, 512 bytes */
};
_Static_assert(sizeof(RecoveredWorker) == 544, "RecoveredWorker size");
_Static_assert(__builtin_offsetof(RecoveredWorker, state) == 0, "RecoveredWorker.state offset");
_Static_assert(__builtin_offsetof(RecoveredWorker, pid) == 4, "RecoveredWorker.pid offset");
_Static_assert(__builtin_offsetof(RecoveredWorker, event_pipe) == 8, "RecoveredWorker.event_pipe offset");
_Static_assert(__builtin_offsetof(RecoveredWorker, approval_pipe) == 16, "RecoveredWorker.approval_pipe offset");
_Static_assert(__builtin_offsetof(RecoveredWorker, exit_status) == 24, "RecoveredWorker.exit_status offset");
_Static_assert(__builtin_offsetof(RecoveredWorker, unknown_1c) == 28, "RecoveredWorker.unknown_1c offset");
_Static_assert(__builtin_offsetof(RecoveredWorker, opaque_20) == 32, "RecoveredWorker.opaque_20 offset");

/* size 0x4410; offsets verified against static instruction evidence. */
struct RecoveredSubagent {
    int running; /* +0x0, 4 bytes */
    int finished; /* +0x4, 4 bytes */
    int succeeded; /* +0x8, 4 bytes */
    int pid; /* +0xc, 4 bytes */
    char output_path[1024]; /* +0x10, 1024 bytes */
    char task[16384]; /* +0x410, 16384 bytes */
};
_Static_assert(sizeof(RecoveredSubagent) == 17424, "RecoveredSubagent size");
_Static_assert(__builtin_offsetof(RecoveredSubagent, running) == 0, "RecoveredSubagent.running offset");
_Static_assert(__builtin_offsetof(RecoveredSubagent, finished) == 4, "RecoveredSubagent.finished offset");
_Static_assert(__builtin_offsetof(RecoveredSubagent, succeeded) == 8, "RecoveredSubagent.succeeded offset");
_Static_assert(__builtin_offsetof(RecoveredSubagent, pid) == 12, "RecoveredSubagent.pid offset");
_Static_assert(__builtin_offsetof(RecoveredSubagent, output_path) == 16, "RecoveredSubagent.output_path offset");
_Static_assert(__builtin_offsetof(RecoveredSubagent, task) == 1040, "RecoveredSubagent.task offset");

/* size 0xc; offsets verified against static instruction evidence. */
struct RecoveredRoute {
    uint intent_mask; /* +0x0, 4 bytes */
    int mentions_path; /* +0x4, 4 bytes */
    int short_message; /* +0x8, 4 bytes */
};
_Static_assert(sizeof(RecoveredRoute) == 12, "RecoveredRoute size");
_Static_assert(__builtin_offsetof(RecoveredRoute, intent_mask) == 0, "RecoveredRoute.intent_mask offset");
_Static_assert(__builtin_offsetof(RecoveredRoute, mentions_path) == 4, "RecoveredRoute.mentions_path offset");
_Static_assert(__builtin_offsetof(RecoveredRoute, short_message) == 8, "RecoveredRoute.short_message offset");

/* size 0x8d08c4; offsets verified against static instruction evidence. */
struct RecoveredAppState {
    int running; /* +0x0, 4 bytes */
    uint opaque_4; /* +0x4, 4 bytes */
    RecoveredConversation conversation; /* +0x8, 35336 bytes */
    RecoveredConfig config; /* +0x8a10, 13928 bytes */
    RecoveredHistory history; /* +0xc078, 8388620 bytes */
    int input_tokens; /* +0x80c084, 4 bytes */
    int output_tokens; /* +0x80c088, 4 bytes */
    int total_tokens; /* +0x80c08c, 4 bytes */
    int usage_known; /* +0x80c090, 4 bytes */
    RecoveredDString project_context; /* +0x80c094, 12 bytes */
    int context_loaded; /* +0x80c0a0, 4 bytes */
    RecoveredFileContext file_contexts[200]; /* +0x80c0a4, 209600 bytes */
    int file_context_count; /* +0x83f364, 4 bytes */
    int retry_count; /* +0x83f368, 4 bytes */
    int retry_delay; /* +0x83f36c, 4 bytes */
    int retry_remaining; /* +0x83f370, 4 bytes */
    char last_input[262144]; /* +0x83f374, 262144 bytes */
    int worker_start_ms; /* +0x87f374, 4 bytes */
    int opaque_87f378; /* +0x87f378, 4 bytes */
    RecoveredPlan plan; /* +0x87f37c, 328204 bytes */
    int plan_present; /* +0x8cf588, 4 bytes */
    int worker_active; /* +0x8cf58c, 4 bytes */
    int spinner; /* +0x8cf590, 4 bytes */
    int turn_start_time; /* +0x8cf594, 4 bytes */
    int cancel_armed; /* +0x8cf598, 4 bytes */
    int cancel_time; /* +0x8cf59c, 4 bytes */
    char worker_input_path[1024]; /* +0x8cf5a0, 1024 bytes */
    char opaque_8cf9a0[3088]; /* +0x8cf9a0, 3088 bytes */
    RecoveredCacheEntry cache[16]; /* +0x8d05b0, 384 bytes */
    int tool_failures[100]; /* +0x8d0730, 400 bytes */
    int sidebar_visible; /* +0x8d08c0, 4 bytes */
};
_Static_assert(sizeof(RecoveredAppState) == 9242820, "RecoveredAppState size");
_Static_assert(__builtin_offsetof(RecoveredAppState, running) == 0, "RecoveredAppState.running offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, opaque_4) == 4, "RecoveredAppState.opaque_4 offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, conversation) == 8, "RecoveredAppState.conversation offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, config) == 35344, "RecoveredAppState.config offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, history) == 49272, "RecoveredAppState.history offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, input_tokens) == 8437892, "RecoveredAppState.input_tokens offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, output_tokens) == 8437896, "RecoveredAppState.output_tokens offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, total_tokens) == 8437900, "RecoveredAppState.total_tokens offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, usage_known) == 8437904, "RecoveredAppState.usage_known offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, project_context) == 8437908, "RecoveredAppState.project_context offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, context_loaded) == 8437920, "RecoveredAppState.context_loaded offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, file_contexts) == 8437924, "RecoveredAppState.file_contexts offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, file_context_count) == 8647524, "RecoveredAppState.file_context_count offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, retry_count) == 8647528, "RecoveredAppState.retry_count offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, retry_delay) == 8647532, "RecoveredAppState.retry_delay offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, retry_remaining) == 8647536, "RecoveredAppState.retry_remaining offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, last_input) == 8647540, "RecoveredAppState.last_input offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, worker_start_ms) == 8909684, "RecoveredAppState.worker_start_ms offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, opaque_87f378) == 8909688, "RecoveredAppState.opaque_87f378 offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, plan) == 8909692, "RecoveredAppState.plan offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, plan_present) == 9237896, "RecoveredAppState.plan_present offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, worker_active) == 9237900, "RecoveredAppState.worker_active offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, spinner) == 9237904, "RecoveredAppState.spinner offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, turn_start_time) == 9237908, "RecoveredAppState.turn_start_time offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, cancel_armed) == 9237912, "RecoveredAppState.cancel_armed offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, cancel_time) == 9237916, "RecoveredAppState.cancel_time offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, worker_input_path) == 9237920, "RecoveredAppState.worker_input_path offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, opaque_8cf9a0) == 9238944, "RecoveredAppState.opaque_8cf9a0 offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, cache) == 9242032, "RecoveredAppState.cache offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, tool_failures) == 9242416, "RecoveredAppState.tool_failures offset");
_Static_assert(__builtin_offsetof(RecoveredAppState, sidebar_visible) == 9242816, "RecoveredAppState.sidebar_visible offset");

/* size 0x14; offsets verified against static instruction evidence. */
struct RecoveredExecutor {
    RecoveredConfig * config; /* +0x0, 4 bytes */
    RecoveredConversation * conversation; /* +0x4, 4 bytes */
    RecoveredPlan * plan; /* +0x8, 4 bytes */
    int state; /* +0xc, 4 bytes */
    int step_index; /* +0x10, 4 bytes */
};
_Static_assert(sizeof(RecoveredExecutor) == 20, "RecoveredExecutor size");
_Static_assert(__builtin_offsetof(RecoveredExecutor, config) == 0, "RecoveredExecutor.config offset");
_Static_assert(__builtin_offsetof(RecoveredExecutor, conversation) == 4, "RecoveredExecutor.conversation offset");
_Static_assert(__builtin_offsetof(RecoveredExecutor, plan) == 8, "RecoveredExecutor.plan offset");
_Static_assert(__builtin_offsetof(RecoveredExecutor, state) == 12, "RecoveredExecutor.state offset");
_Static_assert(__builtin_offsetof(RecoveredExecutor, step_index) == 16, "RecoveredExecutor.step_index offset");

/* size 0x48; offsets verified against static instruction evidence. */
struct RecoveredModel {
    char name[64]; /* +0x0, 64 bytes */
    int context_window; /* +0x40, 4 bytes */
    int size_class; /* +0x44, 4 bytes */
};
_Static_assert(sizeof(RecoveredModel) == 72, "RecoveredModel size");
_Static_assert(__builtin_offsetof(RecoveredModel, name) == 0, "RecoveredModel.name offset");
_Static_assert(__builtin_offsetof(RecoveredModel, context_window) == 64, "RecoveredModel.context_window offset");
_Static_assert(__builtin_offsetof(RecoveredModel, size_class) == 68, "RecoveredModel.size_class offset");

/* size 0x1208; offsets verified against static instruction evidence. */
struct RecoveredModelCache {
    RecoveredModel models[64]; /* +0x0, 4608 bytes */
    int count; /* +0x1200, 4 bytes */
    int updated_at; /* +0x1204, 4 bytes */
};
_Static_assert(sizeof(RecoveredModelCache) == 4616, "RecoveredModelCache size");
_Static_assert(__builtin_offsetof(RecoveredModelCache, models) == 0, "RecoveredModelCache.models offset");
_Static_assert(__builtin_offsetof(RecoveredModelCache, count) == 4608, "RecoveredModelCache.count offset");
_Static_assert(__builtin_offsetof(RecoveredModelCache, updated_at) == 4612, "RecoveredModelCache.updated_at offset");

/* size 0x93584; offsets verified against static instruction evidence. */
struct RecoveredMCPState {
    RecoveredMCPServer servers[4]; /* +0x0, 603520 bytes */
    int count; /* +0x93580, 4 bytes */
};
_Static_assert(sizeof(RecoveredMCPState) == 603524, "RecoveredMCPState size");
_Static_assert(__builtin_offsetof(RecoveredMCPState, servers) == 0, "RecoveredMCPState.servers offset");
_Static_assert(__builtin_offsetof(RecoveredMCPState, count) == 603520, "RecoveredMCPState.count offset");

/* size 0x28; offsets verified against static instruction evidence. */
struct RecoveredUnwindImage {
    uint size; /* +0x0, 4 bytes */
    void * header; /* +0x4, 4 bytes */
    int slide; /* +0x8, 4 bytes */
    void (*destroy)(struct RecoveredUnwindImage *); /* +0xc, 4 bytes */
    RecoveredUnwindImage * next; /* +0x10, 4 bytes */
    char opaque_14[20]; /* +0x14, 20 bytes */
};
_Static_assert(sizeof(RecoveredUnwindImage) == 40, "RecoveredUnwindImage size");
_Static_assert(__builtin_offsetof(RecoveredUnwindImage, size) == 0, "RecoveredUnwindImage.size offset");
_Static_assert(__builtin_offsetof(RecoveredUnwindImage, header) == 4, "RecoveredUnwindImage.header offset");
_Static_assert(__builtin_offsetof(RecoveredUnwindImage, slide) == 8, "RecoveredUnwindImage.slide offset");
_Static_assert(__builtin_offsetof(RecoveredUnwindImage, destroy) == 12, "RecoveredUnwindImage.destroy offset");
_Static_assert(__builtin_offsetof(RecoveredUnwindImage, next) == 16, "RecoveredUnwindImage.next offset");
_Static_assert(__builtin_offsetof(RecoveredUnwindImage, opaque_14) == 20, "RecoveredUnwindImage.opaque_14 offset");
#pragma pack(pop)
#endif
