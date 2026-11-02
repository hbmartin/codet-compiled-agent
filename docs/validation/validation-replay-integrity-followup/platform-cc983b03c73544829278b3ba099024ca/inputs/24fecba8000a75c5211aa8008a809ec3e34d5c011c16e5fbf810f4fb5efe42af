#include "app.h"
/* ANNOTATED C PORT OF STATIC DECOMPILATION -- NOT ORIGINAL SOURCE.
 * Target: 32-bit big-endian PowerPC, Mac OS X 10.4 Tiger.
 * Functions/strings/behavior are preserved as recovered; security flaws are NOT fixed.
 * Symbol spellings, addresses and bytes are evidence. Local names, structures,
 * signatures and subsystem filenames are inferred. undefinedN/DAT_/PTR_/stack
 * constructs remain where exact source types or expressions cannot be established.
 * Every entry has an exact assembly sidecar and machine-readable index record.
 * Build instructions and unverified runtime limitations: see README.md and BUILD-NOTES.md.
 */

/*
 * _ds_init @ 0x00016fec..0x00017008
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Initialize a 12-byte dynamic string to a null pointer and zero length/capacity.
 * Parameters/return (inferred): void _ds_init(RecoveredDString *str)
 * Direct callers: _simple_git_status@0x00003400, _handle_slash_command@0x0000362c, _simple_mode@0x00004e30, _config_init@0x00005da8, _tool_call_copy@0x00006cfc, _message_copy@0x00006eb0, _session_load@0x00007cb8, _provider_parse_fallback@0x000091c0, _provider_parse_tool_calls@0x00009f28, _provider_send@0x0000a39c, _anthropic_sse_cb@0x0000fad0, _anthropic_send@0x00010540, _google_sse_cb@0x000119fc, _sa_child_main@0x0001327c, _tool_read_file@0x00013e64, _tool_edit_file@0x000144a8, _tool_shell@0x00014870, _tool_grep@0x00014bd8, _cache_init@0x00014f10, _cache_store@0x00015010, _tool_read_and_edit@0x00015544, _tool_read_project@0x00016324, _ds_free@0x00017008, _diff_files@0x0001741c, _redraw_sidebar@0x00019a6c, _handle_cmd@0x0001b004, _tui_run@0x0001df44, _start_turn@0x000202dc, _reasoning_extract@0x00020aac, _execute_plan_steps@0x00022454, _worker_child_main@0x00022774, _worker_deserialize_out@0x00024b1c, _executor_run_step@0x00025704, _context_drop_oldest@0x00025d00, _context_auto_compact@0x000261fc, _context_enforce_hard_limit@0x000262b0, _fetch_url_internal@0x00026940, _db_tool_execute@0x000277dc, _fetch_embedding@0x00027fa0, _rag_search@0x000281ec, _fetch_bing_mobile@0x00028e20, _mcp_http_post@0x0002b01c, _mcp_http_get@0x0002b1fc, _mcp_get_tools_json@0x0002c004, _tool_execute_mcp@0x0002c0c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00016fec_ds_init.s
 */
/* Analyst reconstruction of _ds_init at 0x00016fec. See index/manual-review.json. */
void ds_init(RecoveredDString *str)
{
    if (str == 0) return;
    str->data = 0; str->length = 0; str->capacity = 0;
}

/*
 * _ds_free @ 0x00017008..0x00017044
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage heap-backed dynamic string state: free. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _ds_free(RecoveredDString *str)
 * Direct callers: _simple_git_status@0x00003400, _handle_slash_command@0x0000362c, _simple_mode@0x00004e30, _main@0x00005498, _message_free@0x00006de0, _message_copy@0x00006eb0, _session_load@0x00007cb8, _provider_parse_tool_calls@0x00009f28, _anthropic_send@0x00010540, _sa_child_main@0x0001327c, _tool_read_file@0x00013e64, _tool_edit_file@0x000144a8, _tool_shell@0x00014870, _tool_grep@0x00014bd8, _cache_store@0x00015010, _cache_clear@0x000150d0, _tool_read_and_edit@0x00015544, _tool_read_project@0x00016324, _diff_files@0x0001741c, _redraw_sidebar@0x00019a6c, _handle_cmd@0x0001b004, _tui_cleanup@0x0001de90, _reasoning_extract@0x00020aac, _execute_plan_steps@0x00022454, _worker_child_main@0x00022774, _executor_run_step@0x00025704, _context_auto_compact@0x000261fc, _context_enforce_hard_limit@0x000262b0, _fetch_url_internal@0x00026940, _db_tool_execute@0x000277dc, _fetch_embedding@0x00027fa0, _rag_search@0x000281ec, _fetch_bing_mobile@0x00028e20, _mcp_http_post@0x0002b01c, _mcp_http_get@0x0002b1fc, _mcp_get_tools_json@0x0002c004, _tool_execute_mcp@0x0002c0c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00017008_ds_free.s
 */
/* Analyst reconstruction of _ds_free at 0x00017008. See index/manual-review.json. */
void ds_free(RecoveredDString *str)
{
    if (str == 0) return;
    free(str->data);
    ds_init(str);
}

/*
 * _ds_clear @ 0x00017044..0x0001706c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage heap-backed dynamic string state: clear. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _ds_clear(RecoveredDString *str)
 * Direct callers: _simple_mode@0x00004e30, _config_load@0x00005fb8, _anthropic_sse_cb@0x0000fad0, _tool_read_file@0x00013e64, _guard_refuse_write@0x00014170, _tool_write_file@0x0001420c, _tool_edit_file@0x000144a8, _tool_shell@0x00014870, _tool_glob@0x00014b50, _tool_grep@0x00014bd8, _tool_search@0x00014cf8, _tool_fetch_url@0x00014d94, _tool_db@0x0001513c, _tool_memory_remember@0x000152b4, _tool_memory_load@0x00015410, _tool_read_and_edit@0x00015544, _tool_subagent@0x00015748, _tool_execute@0x000158c8, _tool_tree@0x00016158, _tool_read_project@0x00016324, _ds_copy@0x000173c8, _do_exec_read@0x000175ac, _redraw_sidebar@0x00019a6c, _tui_run@0x0001df44, _execute_plan_steps@0x00022454, _worker_child_main@0x00022774, _context_build@0x00025bec
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00017044_ds_clear.s
 */
/* Analyst reconstruction of _ds_clear at 0x00017044. See index/manual-review.json. */
void ds_clear(RecoveredDString *str)
{
    if (str == 0) return;
    if (str->data != 0) str->data[0] = '\0';
    str->length = 0;
}

/*
 * _ds_reserve @ 0x0001706c..0x00017100
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Grow capacity by doubling; allocation failure frees and clears the prior storage.
 * Parameters/return (inferred): void _ds_reserve(RecoveredDString *str, unsigned int requested)
 * Direct callers: _ds_appendn@0x00017150, _ds_appendf@0x000171dc
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/0001706c_ds_reserve.s
 */
/* Analyst reconstruction of _ds_reserve at 0x0001706c. See index/manual-review.json. */
void ds_reserve(RecoveredDString *str, unsigned int requested)
{
    unsigned int capacity;
    char *allocation;
    if (str == 0 || requested == 0 || requested <= str->capacity) return;
    capacity = str->capacity;
    if (capacity == 0) capacity = 8;
    /* Unsigned 32-bit doubling; no overflow guard exists in the binary. */
    while (capacity < requested) capacity <<= 1;
    allocation = realloc(str->data, capacity);
    if (allocation == 0) {
        /* The binary discards the old buffer on allocation failure. */
        free(str->data);
        str->data = 0; str->capacity = 0; str->length = 0;
    } else {
        str->data = allocation; str->capacity = capacity;
    }
}

/*
 * _ds_append @ 0x00017100..0x00017150
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Append a NUL-terminated string to the dynamic string.
 * Parameters/return (inferred): void _ds_append(RecoveredDString *str, const char *text)
 * Direct callers: _handle_slash_command@0x0000362c, _simple_mode@0x00004e30, _config_init@0x00005da8, _config_load@0x00005fb8, _session_load@0x00007cb8, _provider_parse_tool_calls_args@0x000098bc, _ollama_sse_cb@0x0000c3a0, _ollama_send@0x0000c70c, _openai_sse_cb@0x0000e388, _anthropic_sse_cb@0x0000fad0, _anthropic_send@0x00010540, _google_sse_cb@0x000119fc, _sa_child_main@0x0001327c, _smart_truncate_content@0x00013cb8, _tool_read_file@0x00013e64, _tool_edit_file@0x000144a8, _tool_shell@0x00014870, _tool_glob@0x00014b50, _tool_grep@0x00014bd8, _tool_search@0x00014cf8, _tool_fetch_url@0x00014d94, _cache_store@0x00015010, _tool_db@0x0001513c, _tool_memory_load@0x00015410, _tool_read_and_edit@0x00015544, _tool_subagent@0x00015748, _tool_execute@0x000158c8, _tool_read_project@0x00016324, _diff_files@0x0001741c, _redraw_sidebar@0x00019a6c, _handle_cmd@0x0001b004, _tui_run@0x0001df44, _reasoning_extract@0x00020aac, _execute_plan_steps@0x00022454, _worker_child_main@0x00022774, _worker_deserialize_out@0x00024b1c, _executor_run_step@0x00025704, _gen_tree_recursive@0x00025a80, _context_build@0x00025bec, _context_drop_oldest@0x00025d00, _strip_html@0x000264dc, _extract_links@0x0002684c, _db_query@0x00026c50, _db_list_tables@0x0002728c, _db_schema_single@0x00027398, _db_schema@0x000275c4, _db_exec@0x000276bc, _fetch_embedding@0x00027fa0, _fetch_bing_mobile@0x00028e20, _mcp_build_tool_schemas@0x0002bed8, _tool_execute_mcp@0x0002c0c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00017100_ds_append.s
 */
/* Analyst reconstruction of _ds_append at 0x00017100. See index/manual-review.json. */
void ds_append(RecoveredDString *str, const char *text)
{
    if (str != 0 && text != 0) ds_appendn(str, text, strlen(text));
}

/*
 * _ds_appendn @ 0x00017150..0x000171dc
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Append an explicit byte count and maintain the trailing terminator.
 * Parameters/return (inferred): void _ds_appendn(RecoveredDString *str, const char *text, unsigned int length)
 * Direct callers: _handle_slash_command@0x0000362c, _provider_parse_fallback@0x000091c0, _smart_truncate_content@0x00013cb8, _read_file_into_dstring@0x00013de0, _tool_edit_file@0x000144a8, _tool_shell@0x00014870, _tool_read_project@0x00016324, _ds_append@0x00017100, _ds_copy@0x000173c8, _do_exec_read@0x000175ac, _handle_cmd@0x0001b004, _reasoning_extract@0x00020aac, _curl_write_cb@0x000263b0, _strip_html@0x000264dc, _extract_links@0x0002684c, _fetch_embedding@0x00027fa0, _mcp_write_cb@0x0002afdc
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00017150_ds_appendn.s
 */
/* Analyst reconstruction of _ds_appendn at 0x00017150. See index/manual-review.json. */
void ds_appendn(RecoveredDString *str, const char *text, unsigned int length)
{
    if (str == 0 || text == 0 || length == 0) return;
    ds_reserve(str, str->length + length + 1);
    if (str->data == 0) return;
    memcpy(str->data + str->length, text, length);
    str->length += length;
    str->data[str->length] = '\0';
}

/*
 * _ds_appendf @ 0x000171dc..0x00017364
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Format variadic arguments and append the resulting text.
 * Parameters/return (inferred): void _ds_appendf(RecoveredDString *str, const char *format, ...)
 * Direct callers: _handle_slash_command@0x0000362c, _simple_mode@0x00004e30, _sa_child_main@0x0001327c, _smart_truncate_content@0x00013cb8, _tool_read_file@0x00013e64, _guard_refuse_write@0x00014170, _tool_write_file@0x0001420c, _tool_edit_file@0x000144a8, _tool_shell@0x00014870, _glob_walk@0x000149e8, _tool_grep@0x00014bd8, _tool_search@0x00014cf8, _tool_memory_remember@0x000152b4, _tool_memory_load@0x00015410, _tool_read_and_edit@0x00015544, _tree_walk@0x00015d9c, _tool_tree@0x00016158, _tool_read_project@0x00016324, _handle_cmd@0x0001b004, _start_turn@0x000202dc, _execute_plan_steps@0x00022454, _worker_child_main@0x00022774, _gen_tree_recursive@0x00025a80, _context_drop_oldest@0x00025d00, _db_query@0x00026c50, _db_list_tables@0x0002728c, _db_schema_single@0x00027398, _db_schema@0x000275c4, _db_exec@0x000276bc, _db_tool_execute@0x000277dc, _rag_search@0x000281ec, _fetch_bing_mobile@0x00028e20, _mcp_build_tool_schemas@0x0002bed8, _tool_execute_mcp@0x0002c0c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/000171dc_ds_appendf.s
 */
/* Analyst reconstruction of _ds_appendf at 0x000171dc. See index/manual-review.json. */
void ds_appendf(RecoveredDString *str, const char *format, ...)
{
    char small[256];
    char *buffer = 0;
    unsigned int capacity;
    int length;
    va_list original, attempt;
    if (str == 0 || format == 0) return;
    /* va_list denotes the Tiger PPC parameter-save-area cursor. The machine
       spills r5..r10 at original-SP+32..52 and passes that cursor in r6. */
    va_start(original, format);
    va_copy(attempt, original);
    length = vsnprintf(small, 256, format, attempt);
    va_end(attempt);
    if (length < 0) { va_end(original); return; }
    if ((unsigned int)length < 256) {
        ds_reserve(str, str->length + (unsigned int)length + 1);
        if (str->data != 0) {
            memcpy(str->data + str->length, small, (unsigned int)length + 1);
            str->length += (unsigned int)length;
        }
        va_end(original);
        return;
    }
    /* Preserve the observed retry from 256 and doubling, even though the
       first formatting result already reported a longer required length. */
    capacity = 256;
    for (;;) {
        char *resized = realloc(buffer, capacity);
        if (resized == 0) { free(buffer); break; }
        buffer = resized;
        va_copy(attempt, original);
        length = vsnprintf(buffer, capacity, format, attempt);
        va_end(attempt);
        if (length < 0) { free(buffer); break; }
        if ((unsigned int)length >= capacity) { capacity <<= 1; continue; }
        ds_reserve(str, str->length + (unsigned int)length + 1);
        if (str->data != 0) {
            memcpy(str->data + str->length, buffer, (unsigned int)length + 1);
            str->length += (unsigned int)length;
        }
        free(buffer);
        break;
    }
    va_end(original);
}

/*
 * _ds_truncate @ 0x00017364..0x00017394
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage heap-backed dynamic string state: truncate. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _ds_truncate(RecoveredDString *str, unsigned int length)
 * Direct callers: _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00017364_ds_truncate.s
 */
/* Analyst reconstruction of _ds_truncate at 0x00017364. See index/manual-review.json. */
void ds_truncate(RecoveredDString *str, unsigned int length)
{
    if (str == 0 || str->length <= length) return;
    str->length = length;
    if (str->data != 0) str->data[length] = '\0';
}

/*
 * _ds_cstr @ 0x00017394..0x000173c8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Return the current string pointer or an empty-string fallback.
 * Parameters/return (inferred): char *_ds_cstr(RecoveredDString *str)
 * Direct callers: _simple_ask_approve@0x0000320c, _simple_git_status@0x00003400, _handle_slash_command@0x0000362c, _simple_mode@0x00004e30, _config_save@0x00006820, _session_save@0x000079f0, _ollama_build_request@0x0000bdf0, _ollama_send@0x0000c70c, _openai_build_messages@0x0000dd28, _anthropic_build_messages@0x0000f520, _anthropic_sse_cb@0x0000fad0, _anthropic_send@0x00010540, _google_build_contents@0x00011474, _sa_child_main@0x0001327c, _smart_truncate_content@0x00013cb8, _tool_read_file@0x00013e64, _tool_write_file@0x0001420c, _tool_edit_file@0x000144a8, _tool_shell@0x00014870, _tool_glob@0x00014b50, _tool_grep@0x00014bd8, _tool_search@0x00014cf8, _tool_fetch_url@0x00014d94, _cache_key@0x00014e48, _cache_store@0x00015010, _tool_db@0x0001513c, _tool_memory_remember@0x000152b4, _tool_memory_load@0x00015410, _tool_read_and_edit@0x00015544, _tool_subagent@0x00015748, _tool_execute@0x000158c8, _tool_tree@0x00016158, _tool_read_project@0x00016324, _diff_files@0x0001741c, _redraw_sidebar@0x00019a6c, _handle_cmd@0x0001b004, _tui_run@0x0001df44, _start_turn@0x000202dc, _child_send_tool_call@0x00021b2c, _child_ask_approve@0x00021ed8, _child_write_out_json@0x00022270, _execute_plan_steps@0x00022454, _worker_child_main@0x00022774, _worker_serialize_in@0x00024740, _executor_run_step@0x00025704, _context_drop_oldest@0x00025d00, _strip_html@0x000264dc, _fetch_url_internal@0x00026940, _fetch_embedding@0x00027fa0, _rag_search@0x000281ec, _fetch_bing_mobile@0x00028e20, _mcp_get_tools_json@0x0002c004, _tool_execute_mcp@0x0002c0c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00017394_ds_cstr.s
 */
/* Analyst reconstruction of _ds_cstr at 0x00017394. See index/manual-review.json. */
char *ds_cstr(RecoveredDString *str)
{
    return str != 0 && str->data != 0 ? str->data : "";
}

/*
 * _ds_copy @ 0x000173c8..0x0001741c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage heap-backed dynamic string state: copy. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _ds_copy(RecoveredDString *destination, RecoveredDString *source)
 * Direct callers: _simple_mode@0x00004e30, _tool_call_copy@0x00006cfc, _message_copy@0x00006eb0, _sa_child_main@0x0001327c, _worker_child_main@0x00022774, _fetch_url_internal@0x00026940
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/000173c8_ds_copy.s
 */
/* Analyst reconstruction of _ds_copy at 0x000173c8. See index/manual-review.json. */
void ds_copy(RecoveredDString *destination, RecoveredDString *source)
{
    if (destination == 0 || destination == source) return;
    ds_clear(destination);
    if (source != 0) ds_appendn(destination, source->data, source->length);
}

