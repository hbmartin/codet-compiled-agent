/* ANNOTATED STATIC DECOMPILATION -- NOT ORIGINAL SOURCE, NOT BUILDABLE C.
 * Target: 32-bit big-endian PowerPC, Mac OS X 10.4 Tiger.
 * Functions/strings/behavior are preserved as recovered; security flaws are NOT fixed.
 * Symbol spellings, addresses and bytes are evidence. Local names, structures,
 * signatures and subsystem filenames are inferred. undefinedN/DAT_/PTR_/stack
 * constructs remain where exact source types or expressions cannot be established.
 * Every entry has an exact assembly sidecar and machine-readable index record.
 * Do not compile/run this archive as an application. See README.md and REPORT.md.
 */

/*
 * _fprintf$LDBLStub @ 0x0002ca60..0x0002cb0c
 * Status: assembly_only; symbol: retained.
 * Purpose (assembly-and-symbol): Darwin long-double stdio ABI wrapper: preserve argument registers while resolving/forwarding the matching libc symbol.
 * Parameters/return (inferred): int _fprintf_LDBLStub(void *stream,char *format,...)
 * Direct callers: _cleanup_orphan_workers@0x000053c8, _config_save@0x00006820, _session_save@0x000079f0, _ollama_send@0x0000c70c, _hook_build_state_json@0x00012624, _tool_memory_remember@0x000152b4, _tui_init@0x0001daac, _write_file@0x000204e0
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: assembly-only; C semantics unresolved; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002ca60_fprintf_LDBLStub.s
 * Uncertainty: / WARNING: Could not recover jumptable at 0x0002cb08. Too many branches /
 * Uncertainty: Indirect dispatch sites are retained with raw assembly and references; possible target sets are not assumed complete.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * _printf$LDBLStub @ 0x0002cb0c..0x0002cbb8
 * Status: assembly_only; symbol: retained.
 * Purpose (assembly-and-symbol): Darwin long-double stdio ABI wrapper: preserve argument registers while resolving/forwarding the matching libc symbol.
 * Parameters/return (inferred): int _printf_LDBLStub(char *format,...)
 * Direct callers: _usage@0x00003018, _show_simple_http_error@0x0000315c, _simple_ask_approve@0x0000320c, _simple_git_status@0x00003400, _pick_model_simple@0x000034a8, _handle_slash_command@0x0000362c, _simple_mode@0x00004e30
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: assembly-only; C semantics unresolved; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002cb0c_printf_LDBLStub.s
 * Uncertainty: / WARNING: Could not recover jumptable at 0x0002cbb4. Too many branches /
 * Uncertainty: Indirect dispatch sites are retained with raw assembly and references; possible target sets are not assumed complete.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * _snprintf$LDBLStub @ 0x0002cbb8..0x0002cc64
 * Status: assembly_only; symbol: retained.
 * Purpose (assembly-and-symbol): Darwin long-double stdio ABI wrapper: preserve argument registers while resolving/forwarding the matching libc symbol.
 * Parameters/return (inferred): int _snprintf_LDBLStub(char *output,uint size,char *format,...)
 * Direct callers: _simple_ask_approve@0x0000320c, _handle_slash_command@0x0000362c, _cleanup_orphan_workers@0x000053c8, _get_config_path@0x00005ee8, _token_budget_str@0x00007434, _conversation_mem_str@0x00007694, _config_get_model_context@0x000077ac, _ensure_session_dir@0x0000790c, _session_get_path@0x00007980, _get_model_cache_path@0x000082ec, _do_async_fetch@0x000087a8, _provider_parse_fallback@0x000091c0, _ollama_sync_model_metadata@0x0000bb9c, _ollama_send@0x0000c70c, _openai_send@0x0000e5ec, _anthropic_send@0x00010540, _google_sse_cb@0x000119fc, _google_send@0x00012238, _hook_init_dir@0x00012400, _hook_discover@0x00012470, _hook_build_state_json@0x00012624, _hook_run@0x000127b0, _read_result_json@0x00013140, _subagent_start@0x000139bc, _subagent_result@0x00013ba0, _tool_read_file@0x00013e64, _tool_write_file@0x0001420c, _tool_edit_file@0x000144a8, _glob_walk@0x000149e8, _trust_warning@0x00015214, _tool_memory_remember@0x000152b4, _tool_memory_load@0x00015410, _tree_walk@0x00015d9c, _tool_tree@0x00016158, _tool_read_project@0x00016324, _diff_string_vs_file@0x000174ec, _theme_names_csv@0x00017968, _redraw_title@0x00019080, _redraw_status@0x000193cc, _redraw_sidebar@0x00019a6c, _read_input@0x0001a74c, _handle_cmd@0x0001b004, _tui_run@0x0001df44, _start_turn@0x000202dc, _detect_project@0x00020564, _init_project@0x00020760, _worker_send_approve@0x000215c0, _child_ask_approve@0x00021ed8, _worker_child_main@0x00022774, _worker_serialize_in@0x00024740, _plan_format@0x0002543c, _executor_run_step@0x00025704, _executor_run_all@0x00025970, _gen_tree_recursive@0x00025a80, _fetch_url_internal@0x00026940, _db_schema_single@0x00027398, _db_tool_execute@0x000277dc, _ensure_ppccode_dir@0x00027a68, _rag_init@0x00027ac8, _fetch_embedding@0x00027fa0, _rag_search@0x000281ec, _rag_stats@0x000288f0, _fetch_bing_mobile@0x00028e20, _search_web@0x00029168, _mcp_http_post@0x0002b01c, _mcp_http_get@0x0002b1fc, _mcp_discover_server@0x0002b980, _tool_execute_mcp@0x0002c0c8, _detect_macos@0x0002c5b8, _platform_prompt_string@0x0002c93c
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: assembly-only; C semantics unresolved; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002cbb8_snprintf_LDBLStub.s
 * Uncertainty: / WARNING: Could not recover jumptable at 0x0002cc60. Too many branches /
 * Uncertainty: Indirect dispatch sites are retained with raw assembly and references; possible target sets are not assumed complete.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * _sscanf$LDBLStub @ 0x0002cc64..0x0002cd10
 * Status: assembly_only; symbol: retained.
 * Purpose (assembly-and-symbol): Darwin long-double stdio ABI wrapper: preserve argument registers while resolving/forwarding the matching libc symbol.
 * Parameters/return (inferred): int _sscanf_LDBLStub(char *input,char *format,...)
 * Direct callers: _handle_slash_command@0x0000362c, _handle_cmd@0x0001b004, _tui_run@0x0001df44, _detect_macos@0x0002c5b8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: assembly-only; C semantics unresolved; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002cc64_sscanf_LDBLStub.s
 * Uncertainty: / WARNING: Could not recover jumptable at 0x0002cd0c. Too many branches /
 * Uncertainty: Indirect dispatch sites are retained with raw assembly and references; possible target sets are not assumed complete.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * _vfprintf$LDBLStub @ 0x0002cd10..0x0002cdbc
 * Status: assembly_only; symbol: retained.
 * Purpose (assembly-and-symbol): Darwin long-double stdio ABI wrapper: preserve argument registers while resolving/forwarding the matching libc symbol.
 * Parameters/return (inferred): int _vfprintf_LDBLStub(void *stream,char *format,void *arguments)
 * Direct callers: _wire_log@0x0000b62c, _debug@0x000217d0
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: assembly-only; C semantics unresolved; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002cd10_vfprintf_LDBLStub.s
 * Uncertainty: / WARNING: Could not recover jumptable at 0x0002cdb8. Too many branches /
 * Uncertainty: Indirect dispatch sites are retained with raw assembly and references; possible target sets are not assumed complete.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * _vsnprintf$LDBLStub @ 0x0002cdbc..0x0002ce68
 * Status: assembly_only; symbol: retained.
 * Purpose (assembly-and-symbol): Darwin long-double stdio ABI wrapper: preserve argument registers while resolving/forwarding the matching libc symbol.
 * Parameters/return (inferred): int _vsnprintf_LDBLStub(char *output,uint capacity,char *format,void *arguments)
 * Direct callers: _ds_appendf@0x000171dc, _wprintw_safe@0x00018b48, _cprintf@0x00018ed8, _msgf@0x00019018, _child_printf@0x00021850
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: assembly-only; C semantics unresolved; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002cdbc_vsnprintf_LDBLStub.s
 * Uncertainty: / WARNING: Could not recover jumptable at 0x0002ce64. Too many branches /
 * Uncertainty: Indirect dispatch sites are retained with raw assembly and references; possible target sets are not assumed complete.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * _sprintf$LDBLStub @ 0x0002ce68..0x0002cf14
 * Status: assembly_only; symbol: retained.
 * Purpose (assembly-and-symbol): Darwin long-double stdio ABI wrapper: preserve argument registers while resolving/forwarding the matching libc symbol.
 * Parameters/return (inferred): int _sprintf_LDBLStub(char *output,char *format,...)
 * Direct callers: _url_encode@0x00028a68, _print_string_ptr@0x0002a098, _print_value@0x0002a320
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: assembly-only; C semantics unresolved; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002ce68_sprintf_LDBLStub.s
 * Uncertainty: / WARNING: Could not recover jumptable at 0x0002cf10. Too many branches /
 * Uncertainty: Indirect dispatch sites are retained with raw assembly and references; possible target sets are not assumed complete.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * ___stub_getrealaddr @ 0x0002cf14..0x0002cfb8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Resolve Darwin's long-double stdio symbol variant through runtime dynamic symbol lookup.
 * Parameters/return (inferred): void *___stub_getrealaddr(const char *name)
 * Direct callers: _fprintf$LDBLStub@0x0002ca60, _printf$LDBLStub@0x0002cb0c, _snprintf$LDBLStub@0x0002cbb8, _sscanf$LDBLStub@0x0002cc64, _vfprintf$LDBLStub@0x0002cd10, _vsnprintf$LDBLStub@0x0002cdbc, _sprintf$LDBLStub@0x0002ce68
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/0002cf14_stub_getrealaddr.s
 */
/* Analyst reconstruction of ___stub_getrealaddr at 0x0002cf14. See index/manual-review.json. */
void *___stub_getrealaddr(const char *name)
{
    /* 72 bytes of available stack scratch before saved registers; the exact
       original array declaration is unavailable. There is no bounds check. */
    char symbol[72];
    unsigned int suffix_at;
    symbol[0] = '_';
    _strcpy(symbol + 1, name);
    suffix_at = _strlen(symbol);
    _memcpy(symbol + suffix_at, "$LDBL128", 9);
    if (!_NSIsSymbolNameDefinedWithHint(symbol, "libSystem."))
        symbol[suffix_at] = '\0';
    return _NSAddressOfSymbol(_NSLookupAndBindSymbolWithHint(symbol, "libSystem."));
}

