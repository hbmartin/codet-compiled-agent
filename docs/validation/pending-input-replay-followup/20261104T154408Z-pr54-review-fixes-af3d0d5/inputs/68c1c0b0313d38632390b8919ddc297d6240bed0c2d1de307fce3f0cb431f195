/* INFERRED PPC32 MEMORY LAYOUTS. NOT ORIGINAL HEADERS. */
/* Pointer-valued fields use ppc_addr32 so offsets remain meaningful on a 64-bit host.
 * These documentary layouts are not drop-in definitions for the pseudocode. */
#include <stdint.h>
typedef uint32_t ppc_addr32;

enum RecoveredProvider { PROVIDER_OLLAMA=0, PROVIDER_OPENAI=1, PROVIDER_ANTHROPIC=2, PROVIDER_GOOGLE=3 };
enum RecoveredRole { ROLE_SYSTEM=0, ROLE_USER=1, ROLE_ASSISTANT=2, ROLE_TOOL=3 };
enum RecoveredTool { TOOL_READ_FILE=0, TOOL_WRITE_FILE=1, TOOL_EDIT_FILE=2, TOOL_SHELL=3, TOOL_GLOB=4, TOOL_GREP=5, TOOL_SEARCH=6, TOOL_FETCH_URL=7, TOOL_DB=8, TOOL_MEMORY_REMEMBER=9, TOOL_MEMORY_LOAD=10, TOOL_READ_AND_EDIT=11, TOOL_SUBAGENT=12, TOOL_REFERENCE=13, TOOL_TREE=14, TOOL_READ_PROJECT=15, TOOL_MCP_BASE=100 };

/* RecoveredDString: exact observed size/offset model 0xc bytes. */
struct RecoveredDString_layout {
    uint8_t data_bytes[4]; /* +0x0000, char * */
    uint8_t length_bytes[4]; /* +0x0004, uint */
    uint8_t capacity_bytes[4]; /* +0x0008, uint */
};

/* RecoveredCJSON: exact observed size/offset model 0x24 bytes. */
struct RecoveredCJSON_layout {
    uint8_t next_bytes[4]; /* +0x0000, RecoveredCJSON * */
    uint8_t previous_bytes[4]; /* +0x0004, RecoveredCJSON * */
    uint8_t child_bytes[4]; /* +0x0008, RecoveredCJSON * */
    uint8_t type_flags_bytes[4]; /* +0x000c, int */
    uint8_t value_string_bytes[4]; /* +0x0010, char * */
    uint8_t value_int_bytes[4]; /* +0x0014, int */
    uint8_t value_double_bytes[8]; /* +0x0018, double */
    uint8_t member_name_bytes[4]; /* +0x0020, char * */
};

/* RecoveredToolCall: exact observed size/offset model 0x51c bytes. */
struct RecoveredToolCall_layout {
    uint8_t tool_type_bytes[4]; /* +0x0000, int */
    uint8_t mcp_name_bytes[32]; /* +0x0004, char[32] */
    uint8_t arguments_bytes[1188]; /* +0x0024, RecoveredDString[99] */
    uint8_t argument_count_bytes[4]; /* +0x04c8, int */
    uint8_t call_id_bytes[64]; /* +0x04cc, char[64] */
    uint8_t execution_status_bytes[4]; /* +0x050c, int */
    uint8_t result_bytes[12]; /* +0x0510, RecoveredDString */
};

/* RecoveredMessage: exact observed size/offset model 0x5c bytes. */
struct RecoveredMessage_layout {
    uint8_t role_bytes[4]; /* +0x0000, int */
    uint8_t content_bytes[12]; /* +0x0004, RecoveredDString */
    uint8_t tool_calls_bytes[4]; /* +0x0010, RecoveredToolCall * */
    uint8_t tool_call_count_bytes[4]; /* +0x0014, int */
    uint8_t tool_call_capacity_bytes[4]; /* +0x0018, int */
    uint8_t call_id_bytes[64]; /* +0x001c, char[64] */
};

/* RecoveredProfile: exact observed size/offset model 0x3c8 bytes. */
struct RecoveredProfile_layout {
    uint8_t name_bytes[64]; /* +0x0000, char[64] */
    uint8_t provider_bytes[4]; /* +0x0040, int */
    uint8_t model_bytes[128]; /* +0x0044, char[128] */
    uint8_t api_key_bytes[256]; /* +0x00c4, char[256] */
    uint8_t base_url_bytes[512]; /* +0x01c4, char[512] */
    uint8_t context_window_bytes[4]; /* +0x03c4, int */
};

/* RecoveredConfig: exact observed size/offset model 0x3668 bytes. */
struct RecoveredConfig_layout {
    uint8_t provider_bytes[4]; /* +0x0000, int */
    uint8_t active_profile_bytes[64]; /* +0x0004, char[64] */
    uint8_t model_bytes[128]; /* +0x0044, char[128] */
    uint8_t api_key_bytes[256]; /* +0x00c4, char[256] */
    uint8_t openai_key_bytes[256]; /* +0x01c4, char[256] */
    uint8_t anthropic_key_bytes[256]; /* +0x02c4, char[256] */
    uint8_t google_key_bytes[256]; /* +0x03c4, char[256] */
    uint8_t ollama_key_bytes[256]; /* +0x04c4, char[256] */
    uint8_t base_url_bytes[512]; /* +0x05c4, char[512] */
    uint8_t ollama_url_bytes[512]; /* +0x07c4, char[512] */
    uint8_t search_url_bytes[512]; /* +0x09c4, char[512] */
    uint8_t working_directory_bytes[1024]; /* +0x0bc4, char[1024] */
    uint8_t system_prompt_bytes[12]; /* +0x0fc4, RecoveredDString */
    uint8_t max_tokens_bytes[4]; /* +0x0fd0, int */
    uint8_t context_window_bytes[4]; /* +0x0fd4, int */
    uint8_t temperature_bytes[4]; /* +0x0fd8, float */
    uint8_t verbose_bytes[4]; /* +0x0fdc, int */
    uint8_t auto_approve_bytes[4]; /* +0x0fe0, int */
    uint8_t auto_compact_bytes[4]; /* +0x0fe4, int */
    uint8_t auto_save_session_bytes[4]; /* +0x0fe8, int */
    uint8_t small_model_mode_bytes[4]; /* +0x0fec, int */
    uint8_t unknown_0ff0_bytes[4]; /* +0x0ff0, int */
    uint8_t effort_bytes[4]; /* +0x0ff4, int */
    uint8_t think_budget_bytes[4]; /* +0x0ff8, int */
    uint8_t write_guard_enforce_bytes[4]; /* +0x0ffc, int */
    uint8_t sidebar_depth_bytes[4]; /* +0x1000, int */
    uint8_t theme_bytes[32]; /* +0x1004, char[32] */
    uint8_t mcp_servers_bytes[2048]; /* +0x1024, char[4][512] */
    uint8_t profiles_bytes[7744]; /* +0x1824, RecoveredProfile[8] */
    uint8_t profile_count_bytes[4]; /* +0x3664, int */
};

/* RecoveredAppPrefix: exact observed size/offset model 0xc078 bytes. */
struct RecoveredAppPrefix_layout {
    uint8_t unknown_prefix_bytes[8]; /* +0x0000, char[8] */
    uint8_t messages_bytes[35328]; /* +0x0008, RecoveredMessage[384] */
    uint8_t message_count_bytes[4]; /* +0x8a08, int */
    uint8_t unknown_accounting_bytes[4]; /* +0x8a0c, uint */
    uint8_t config_bytes[13928]; /* +0x8a10, RecoveredConfig */
};

/* RecoveredConversation: exact observed size/offset model 0x8a08 bytes. */
struct RecoveredConversation_layout {
    uint8_t messages_bytes[35328]; /* +0x0000, RecoveredMessage[384] */
    uint8_t message_count_bytes[4]; /* +0x8a00, int */
    uint8_t estimated_tokens_bytes[4]; /* +0x8a04, uint */
};

/* RecoveredResponse: exact observed size/offset model 0x1fa0c bytes. */
struct RecoveredResponse_layout {
    uint8_t content_bytes[12]; /* +0x0000, RecoveredDString */
    uint8_t error_or_reasoning_bytes[12]; /* +0x000c, RecoveredDString */
    uint8_t unknown_18_bytes[4]; /* +0x0018, uint */
    uint8_t tool_calls_bytes[129492]; /* +0x001c, RecoveredToolCall[99] */
    uint8_t tool_call_count_bytes[4]; /* +0x1f9f0, int */
    uint8_t completion_flag_bytes[4]; /* +0x1f9f4, int */
    uint8_t finish_reason_bytes[4]; /* +0x1f9f8, int */
    uint8_t input_tokens_bytes[4]; /* +0x1f9fc, int */
    uint8_t output_tokens_bytes[4]; /* +0x1fa00, int */
    uint8_t total_tokens_bytes[4]; /* +0x1fa04, int */
    uint8_t usage_known_bytes[4]; /* +0x1fa08, int */
};

/* RecoveredHttpBuffer: exact observed size/offset model 0xc bytes. */
struct RecoveredHttpBuffer_layout {
    uint8_t data_bytes[4]; /* +0x0000, char * */
    uint8_t length_bytes[4]; /* +0x0004, uint */
    uint8_t capacity_bytes[4]; /* +0x0008, uint */
};

/* RecoveredSSEAdapter: exact observed size/offset model 0x14 bytes. */
struct RecoveredSSEAdapter_layout {
    uint8_t callback_bytes[4]; /* +0x0000, RecoveredStreamCallback */
    uint8_t context_bytes[4]; /* +0x0004, void * */
    uint8_t raw_response_bytes[12]; /* +0x0008, RecoveredHttpBuffer */
};

/* RecoveredProviderCallbackContext: exact observed size/offset model 0xc bytes. */
struct RecoveredProviderCallbackContext_layout {
    uint8_t response_bytes[4]; /* +0x0000, RecoveredResponse * */
    uint8_t callback_bytes[4]; /* +0x0004, RecoveredStreamCallback */
    uint8_t context_bytes[4]; /* +0x0008, void * */
};

/* RecoveredMCPTool: exact observed size/offset model 0x1240 bytes. */
struct RecoveredMCPTool_layout {
    uint8_t name_bytes[64]; /* +0x0000, char[64] */
    uint8_t description_bytes[512]; /* +0x0040, char[512] */
    uint8_t parameters_json_bytes[4096]; /* +0x0240, char[4096] */
};

/* RecoveredMCPServer: exact observed size/offset model 0x24d60 bytes. */
struct RecoveredMCPServer_layout {
    uint8_t name_bytes[64]; /* +0x0000, char[64] */
    uint8_t transport_bytes[4]; /* +0x0040, int */
    uint8_t base_url_bytes[512]; /* +0x0044, char[512] */
    uint8_t command_bytes[512]; /* +0x0244, char[512] */
    uint8_t token_bytes[256]; /* +0x0444, char[256] */
    uint8_t tools_bytes[149504]; /* +0x0544, RecoveredMCPTool[32] */
    uint8_t tool_count_bytes[4]; /* +0x24d44, int */
    uint8_t connected_bytes[4]; /* +0x24d48, int */
    uint8_t pid_bytes[4]; /* +0x24d4c, int */
    uint8_t request_pipe_bytes[8]; /* +0x24d50, int[2] */
    uint8_t response_pipe_bytes[8]; /* +0x24d58, int[2] */
};

/* RecoveredTigerStat: exact observed size/offset model 0x60 bytes. */
struct RecoveredTigerStat_layout {
    uint8_t device_bytes[4]; /* +0x0000, int */
    uint8_t inode_bytes[4]; /* +0x0004, uint */
    uint8_t mode_bytes[2]; /* +0x0008, ushort */
    uint8_t link_count_bytes[2]; /* +0x000a, ushort */
    uint8_t uid_bytes[4]; /* +0x000c, uint */
    uint8_t gid_bytes[4]; /* +0x0010, uint */
    uint8_t rdev_bytes[4]; /* +0x0014, int */
    uint8_t atime_seconds_bytes[4]; /* +0x0018, int */
    uint8_t atime_nanoseconds_bytes[4]; /* +0x001c, int */
    uint8_t mtime_seconds_bytes[4]; /* +0x0020, int */
    uint8_t mtime_nanoseconds_bytes[4]; /* +0x0024, int */
    uint8_t ctime_seconds_bytes[4]; /* +0x0028, int */
    uint8_t ctime_nanoseconds_bytes[4]; /* +0x002c, int */
    uint8_t size_bytes[8]; /* +0x0030, longlong */
    uint8_t blocks_bytes[8]; /* +0x0038, longlong */
    uint8_t block_size_bytes[4]; /* +0x0040, int */
    uint8_t flags_bytes[4]; /* +0x0044, uint */
    uint8_t generation_bytes[4]; /* +0x0048, uint */
    uint8_t reserved_4c_bytes[4]; /* +0x004c, int */
    uint8_t reserved_50_bytes[16]; /* +0x0050, char[16] */
};

/* RecoveredTigerDirent: exact observed size/offset model 0x108 bytes. */
struct RecoveredTigerDirent_layout {
    uint8_t inode_bytes[4]; /* +0x0000, uint */
    uint8_t record_length_bytes[2]; /* +0x0004, ushort */
    uint8_t type_bytes[1]; /* +0x0006, uchar */
    uint8_t name_length_bytes[1]; /* +0x0007, uchar */
    uint8_t name_bytes[256]; /* +0x0008, char[256] */
};

/* RecoveredChildContext: exact observed size/offset model 0x10 bytes. */
struct RecoveredChildContext_layout {
    uint8_t event_fd_bytes[4]; /* +0x0000, int */
    uint8_t approval_fd_bytes[4]; /* +0x0004, int */
    uint8_t opaque_8_bytes[4]; /* +0x0008, int */
    uint8_t auto_approve_bytes[4]; /* +0x000c, int */
};

/* RecoveredHook: exact observed size/offset model 0x404 bytes. */
struct RecoveredHook_layout {
    uint8_t path_bytes[1024]; /* +0x0000, char[1024] */
    uint8_t enabled_bytes[4]; /* +0x0400, int */
};

/* RecoveredHooks: exact observed size/offset model 0xc0c bytes. */
struct RecoveredHooks_layout {
    uint8_t slots_bytes[3084]; /* +0x0000, RecoveredHook[3] */
};

/* RecoveredThemePair: exact observed size/offset model 0x8 bytes. */
struct RecoveredThemePair_layout {
    uint8_t foreground_bytes[2]; /* +0x0000, short */
    uint8_t background_bytes[2]; /* +0x0002, short */
    uint8_t attributes_bytes[4]; /* +0x0004, uint */
};

/* RecoveredTheme: exact observed size/offset model 0xa8 bytes. */
struct RecoveredTheme_layout {
    uint8_t name_bytes[4]; /* +0x0000, char * */
    uint8_t description_bytes[4]; /* +0x0004, char * */
    uint8_t pairs_bytes[160]; /* +0x0008, RecoveredThemePair[20] */
};

/* RecoveredTigerUtsname: exact observed size/offset model 0x500 bytes. */
struct RecoveredTigerUtsname_layout {
    uint8_t opaque_sysname_bytes[256]; /* +0x0000, char[256] */
    uint8_t opaque_nodename_bytes[256]; /* +0x0100, char[256] */
    uint8_t release_bytes[256]; /* +0x0200, char[256] */
    uint8_t opaque_version_bytes[256]; /* +0x0300, char[256] */
    uint8_t machine_bytes[256]; /* +0x0400, char[256] */
};

/* RecoveredHistory: exact observed size/offset model 0x80000c bytes. */
struct RecoveredHistory_layout {
    uint8_t entries_bytes[8388608]; /* +0x0000, char[256][32768] */
    uint8_t first_bytes[4]; /* +0x800000, int */
    uint8_t count_bytes[4]; /* +0x800004, int */
    uint8_t cursor_bytes[4]; /* +0x800008, int */
};

/* RecoveredFileContext: exact observed size/offset model 0x418 bytes. */
struct RecoveredFileContext_layout {
    uint8_t path_bytes[1024]; /* +0x0000, char[1024] */
    uint8_t timestamp_high_bytes[4]; /* +0x0400, uint */
    uint8_t timestamp_low_bytes[4]; /* +0x0404, uint */
    uint8_t bytes_read_bytes[4]; /* +0x0408, uint */
    uint8_t first_message_bytes[4]; /* +0x040c, int */
    uint8_t last_message_bytes[4]; /* +0x0410, int */
    uint8_t flags_bytes[4]; /* +0x0414, uint */
};

/* RecoveredCacheEntry: exact observed size/offset model 0x18 bytes. */
struct RecoveredCacheEntry_layout {
    uint8_t key_bytes[4]; /* +0x0000, uint */
    uint8_t tool_type_bytes[4]; /* +0x0004, int */
    uint8_t valid_bytes[4]; /* +0x0008, int */
    uint8_t result_bytes[12]; /* +0x000c, RecoveredDString */
};

/* RecoveredQueue: exact observed size/offset model 0x200008 bytes. */
struct RecoveredQueue_layout {
    uint8_t entries_bytes[2097152]; /* +0x0000, char[8][262144] */
    uint8_t first_bytes[4]; /* +0x200000, int */
    uint8_t count_bytes[4]; /* +0x200004, int */
};

/* RecoveredPlanStep: exact observed size/offset model 0x2808 bytes. */
struct RecoveredPlanStep_layout {
    uint8_t description_bytes[4096]; /* +0x0000, char[4096] */
    uint8_t tool_type_bytes[4]; /* +0x1000, int */
    uint8_t argument0_bytes[1024]; /* +0x1004, char[1024] */
    uint8_t argument1_bytes[1024]; /* +0x1404, char[1024] */
    uint8_t status_bytes[4]; /* +0x1804, int */
    uint8_t result_bytes[4096]; /* +0x1808, char[4096] */
};

/* RecoveredPlan: exact observed size/offset model 0x5020c bytes. */
struct RecoveredPlan_layout {
    uint8_t title_bytes[256]; /* +0x0000, char[256] */
    uint8_t steps_bytes[327936]; /* +0x0100, RecoveredPlanStep[32] */
    uint8_t count_bytes[4]; /* +0x50200, int */
    uint8_t id_bytes[4]; /* +0x50204, int */
    uint8_t unknown_50208_bytes[4]; /* +0x50208, int */
};

/* RecoveredWorker: exact observed size/offset model 0x220 bytes. */
struct RecoveredWorker_layout {
    uint8_t state_bytes[4]; /* +0x0000, int */
    uint8_t pid_bytes[4]; /* +0x0004, int */
    uint8_t event_pipe_bytes[8]; /* +0x0008, int[2] */
    uint8_t approval_pipe_bytes[8]; /* +0x0010, int[2] */
    uint8_t exit_status_bytes[4]; /* +0x0018, int */
    uint8_t unknown_1c_bytes[4]; /* +0x001c, int */
    uint8_t opaque_20_bytes[512]; /* +0x0020, char[512] */
};

/* RecoveredSubagent: exact observed size/offset model 0x4410 bytes. */
struct RecoveredSubagent_layout {
    uint8_t running_bytes[4]; /* +0x0000, int */
    uint8_t finished_bytes[4]; /* +0x0004, int */
    uint8_t succeeded_bytes[4]; /* +0x0008, int */
    uint8_t pid_bytes[4]; /* +0x000c, int */
    uint8_t output_path_bytes[1024]; /* +0x0010, char[1024] */
    uint8_t task_bytes[16384]; /* +0x0410, char[16384] */
};

/* RecoveredRoute: exact observed size/offset model 0xc bytes. */
struct RecoveredRoute_layout {
    uint8_t intent_mask_bytes[4]; /* +0x0000, uint */
    uint8_t mentions_path_bytes[4]; /* +0x0004, int */
    uint8_t short_message_bytes[4]; /* +0x0008, int */
};

/* RecoveredAppState: exact observed size/offset model 0x8d08c4 bytes. */
struct RecoveredAppState_layout {
    uint8_t running_bytes[4]; /* +0x0000, int */
    uint8_t opaque_4_bytes[4]; /* +0x0004, uint */
    uint8_t conversation_bytes[35336]; /* +0x0008, RecoveredConversation */
    uint8_t config_bytes[13928]; /* +0x8a10, RecoveredConfig */
    uint8_t history_bytes[8388620]; /* +0xc078, RecoveredHistory */
    uint8_t input_tokens_bytes[4]; /* +0x80c084, int */
    uint8_t output_tokens_bytes[4]; /* +0x80c088, int */
    uint8_t total_tokens_bytes[4]; /* +0x80c08c, int */
    uint8_t usage_known_bytes[4]; /* +0x80c090, int */
    uint8_t project_context_bytes[12]; /* +0x80c094, RecoveredDString */
    uint8_t context_loaded_bytes[4]; /* +0x80c0a0, int */
    uint8_t file_contexts_bytes[209600]; /* +0x80c0a4, RecoveredFileContext[200] */
    uint8_t file_context_count_bytes[4]; /* +0x83f364, int */
    uint8_t retry_count_bytes[4]; /* +0x83f368, int */
    uint8_t retry_delay_bytes[4]; /* +0x83f36c, int */
    uint8_t retry_remaining_bytes[4]; /* +0x83f370, int */
    uint8_t last_input_bytes[262144]; /* +0x83f374, char[262144] */
    uint8_t worker_start_ms_bytes[4]; /* +0x87f374, int */
    uint8_t opaque_87f378_bytes[4]; /* +0x87f378, int */
    uint8_t plan_bytes[328204]; /* +0x87f37c, RecoveredPlan */
    uint8_t plan_present_bytes[4]; /* +0x8cf588, int */
    uint8_t worker_active_bytes[4]; /* +0x8cf58c, int */
    uint8_t spinner_bytes[4]; /* +0x8cf590, int */
    uint8_t turn_start_time_bytes[4]; /* +0x8cf594, int */
    uint8_t cancel_armed_bytes[4]; /* +0x8cf598, int */
    uint8_t cancel_time_bytes[4]; /* +0x8cf59c, int */
    uint8_t worker_input_path_bytes[1024]; /* +0x8cf5a0, char[1024] */
    uint8_t opaque_8cf9a0_bytes[3088]; /* +0x8cf9a0, char[3088] */
    uint8_t cache_bytes[384]; /* +0x8d05b0, RecoveredCacheEntry[16] */
    uint8_t tool_failures_bytes[400]; /* +0x8d0730, int[100] */
    uint8_t sidebar_visible_bytes[4]; /* +0x8d08c0, int */
};

/* RecoveredExecutor: exact observed size/offset model 0x14 bytes. */
struct RecoveredExecutor_layout {
    uint8_t config_bytes[4]; /* +0x0000, RecoveredConfig * */
    uint8_t conversation_bytes[4]; /* +0x0004, RecoveredConversation * */
    uint8_t plan_bytes[4]; /* +0x0008, RecoveredPlan * */
    uint8_t state_bytes[4]; /* +0x000c, int */
    uint8_t step_index_bytes[4]; /* +0x0010, int */
};

/* RecoveredModel: exact observed size/offset model 0x48 bytes. */
struct RecoveredModel_layout {
    uint8_t name_bytes[64]; /* +0x0000, char[64] */
    uint8_t context_window_bytes[4]; /* +0x0040, int */
    uint8_t size_class_bytes[4]; /* +0x0044, int */
};

/* RecoveredModelCache: exact observed size/offset model 0x1208 bytes. */
struct RecoveredModelCache_layout {
    uint8_t models_bytes[4608]; /* +0x0000, RecoveredModel[64] */
    uint8_t count_bytes[4]; /* +0x1200, int */
    uint8_t updated_at_bytes[4]; /* +0x1204, int */
};

/* RecoveredMCPState: exact observed size/offset model 0x93584 bytes. */
struct RecoveredMCPState_layout {
    uint8_t servers_bytes[603520]; /* +0x0000, RecoveredMCPServer[4] */
    uint8_t count_bytes[4]; /* +0x93580, int */
};

/* Evidence for layout reconstruction:
 * DString: _ds_init / _ds_reserve and 12-byte argument strides.
 * ToolCall: _tool_call_copy zeroes 0x51c and copies 99 argument slots.
 * Message: _message_copy zeroes 0x5c and owns tool-call pointer/count/capacity.
 * CJSON: _cJSON_New_Item allocates 0x24; parse/create/access offsets corroborate fields.
 * Config/Profile: _config_init zeroes 0x3668; _config_save key names and
 * profile selection/stride expose field identities and capacities.
 * AppPrefix: 8-byte prefix + 384 Message records + count/accounting + Config.
 * unknown_0ff0 and unknown_accounting remain deliberately opaque.
 */
