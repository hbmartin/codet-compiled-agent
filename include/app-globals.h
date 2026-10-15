#ifndef PPCCODE_APP_GLOBALS_H
#define PPCCODE_APP_GLOBALS_H
/* Typed views preserve overlapping recovered globals and observed offsets. */
extern unsigned char recovered_section_TEXT___cstring[34028];
extern unsigned char recovered_section_TEXT___const[9916];
extern unsigned char recovered_section_TEXT___literal4[36];
extern unsigned char recovered_section_TEXT___literal8[56];
extern unsigned char recovered_section_DATA___data[1676];
extern unsigned char recovered_section_DATA___const[3780];
extern unsigned char recovered_section_DATA___bss[2957624];
extern unsigned char recovered_section_DATA___common[9851812];
#define _pointer_to__darwin_gcc3_preregister_frame_info (*(uint32_t *)(recovered_section_DATA___data + 16)) /* original 0x00039010 */
#define error_message (*(char *)(recovered_section_TEXT___cstring + 224)) /* original 0x0002e4c0 */
#define _C_35_4094 (*(char *)(recovered_section_DATA___const + 0)) /* original 0x00039964 */
#define _ollama_models (*(char * *)(recovered_section_DATA___data + 20)) /* original 0x00039014 */
#define _C_78_4394 (*(char *)(recovered_section_DATA___const + 16)) /* original 0x00039974 */
#define _C_68_4346 (*(char *)(recovered_section_TEXT___const + 0)) /* original 0x000368cc */
#define _path_4008 (*(char *)(recovered_section_DATA___bss + 0)) /* original 0x0003a828 */
#define _buf_5343 (*(char *)(recovered_section_DATA___bss + 1024)) /* original 0x0003ac28 */
#define _g_refresh_pid (*(int32_t *)(recovered_section_DATA___bss + 1088)) /* original 0x0003ac68 */
#define _path_3670 (*(char *)(recovered_section_DATA___bss + 1092)) /* original 0x0003ac6c */
#define _ollama_sse_call_count (*(uint32_t *)(recovered_section_DATA___bss + 2116)) /* original 0x0003b06c */
#define _C_52_3320 (*(char *)(recovered_section_DATA___const + 32)) /* original 0x00039984 */
#define _C_51_3319 (*(char *)(recovered_section_DATA___const + 44)) /* original 0x00039990 */
#define _C_50_3318 (*(char *)(recovered_section_DATA___const + 56)) /* original 0x0003999c */
#define _C_49_3297 (*(char *)(recovered_section_DATA___const + 68)) /* original 0x000399a8 */
#define _C_48_3296 (*(char *)(recovered_section_DATA___const + 80)) /* original 0x000399b4 */
#define _C_47_3295 (*(char *)(recovered_section_DATA___const + 92)) /* original 0x000399c0 */
#define _C_46_3290 (*(char *)(recovered_section_DATA___const + 104)) /* original 0x000399cc */
#define _C_45_3289 (*(char *)(recovered_section_DATA___const + 112)) /* original 0x000399d4 */
#define _C_44_3288 (*(char *)(recovered_section_DATA___const + 120)) /* original 0x000399dc */
#define _C_25_3151 (*(char *)(recovered_section_DATA___const + 128)) /* original 0x000399e4 */
#define _C_24_3150 (*(char *)(recovered_section_DATA___const + 160)) /* original 0x00039a04 */
#define _C_25_3153 (*(char *)(recovered_section_DATA___const + 192)) /* original 0x00039a24 */
#define _C_24_3152 (*(char *)(recovered_section_DATA___const + 224)) /* original 0x00039a44 */
#define _subagent_idx (*(uint32_t *)(recovered_section_DATA___bss + 2120)) /* original 0x0003b070 */
#define _next_4649 (*(uint32_t *)(recovered_section_DATA___bss + 2124)) /* original 0x0003b074 */
#define _buf_4749 (*(char *)(recovered_section_DATA___bss + 2128)) /* original 0x0003b078 */
#define _g_loop_breaker (*(uint32_t *)(recovered_section_DATA___bss + 2384)) /* original 0x0003b178 */
#define _g_tree_visited_count (*(int32_t *)(recovered_section_DATA___bss + 4448)) /* original 0x0003b988 */
#define _visited_5254 ((char (*)[1024])(recovered_section_DATA___bss + 4452))
#define _curl_initialized (*(uint32_t *)(recovered_section_DATA___bss + 69988)) /* original 0x0004b98c */
#define _delays_ms_3185 (*(int *)(recovered_section_TEXT___const + 128))
#define _theme_light (*(RecoveredTheme *)(recovered_section_DATA___const + 256)) /* original 0x00039a64 */
#define _theme_dark (*(RecoveredTheme *)(recovered_section_DATA___const + 424)) /* original 0x00039b0c */
#define _theme_catppuccin (*(RecoveredTheme *)(recovered_section_DATA___const + 592)) /* original 0x00039bb4 */
#define _theme_catppuccin_light (*(RecoveredTheme *)(recovered_section_DATA___const + 760)) /* original 0x00039c5c */
#define _theme_tokyonight (*(RecoveredTheme *)(recovered_section_DATA___const + 928)) /* original 0x00039d04 */
#define _theme_nord (*(RecoveredTheme *)(recovered_section_DATA___const + 1096)) /* original 0x00039dac */
#define _theme_matrix (*(RecoveredTheme *)(recovered_section_DATA___const + 1264)) /* original 0x00039e54 */
#define _theme_gruvbox (*(RecoveredTheme *)(recovered_section_DATA___const + 1432)) /* original 0x00039efc */
#define _theme_kanagawa (*(RecoveredTheme *)(recovered_section_DATA___const + 1600)) /* original 0x00039fa4 */
#define _draft_len (*(uint32_t *)(recovered_section_DATA___bss + 69992)) /* original 0x0004b990 */
#define _draft_cursor (*(uint32_t *)(recovered_section_DATA___bss + 69996)) /* original 0x0004b994 */
#define _w_title (*(WINDOW * *)(recovered_section_DATA___bss + 70000)) /* original 0x0004b998 */
#define _w_sep (*(WINDOW * *)(recovered_section_DATA___bss + 70004)) /* original 0x0004b99c */
#define _w_chat (*(WINDOW * *)(recovered_section_DATA___bss + 70008)) /* original 0x0004b9a0 */
#define _w_status (*(WINDOW * *)(recovered_section_DATA___bss + 70012)) /* original 0x0004b9a4 */
#define _w_input (*(WINDOW * *)(recovered_section_DATA___bss + 70016)) /* original 0x0004b9a8 */
#define _w_sidebar (*(WINDOW * *)(recovered_section_DATA___bss + 70020)) /* original 0x0004b9ac */
#define _sidebar_content (*(RecoveredDString *)(recovered_section_DATA___bss + 70024)) /* original 0x0004b9b0 */
#define _th (*(int32_t *)(recovered_section_DATA___bss + 70036)) /* original 0x0004b9bc */
#define _tw (*(int32_t *)(recovered_section_DATA___bss + 70040)) /* original 0x0004b9c0 */
#define _chat_y (*(int32_t *)(recovered_section_DATA___bss + 70044)) /* original 0x0004b9c4 */
#define _chat_top (*(int32_t *)(recovered_section_DATA___bss + 70048)) /* original 0x0004b9c8 */
#define _streaming (*(uint32_t *)(recovered_section_DATA___bss + 70052)) /* original 0x0004b9cc */
#define _spinner_idx (*(uint32_t *)(recovered_section_DATA___bss + 70056)) /* original 0x0004b9d0 */
#define _g_app (*(RecoveredAppState * *)(recovered_section_DATA___bss + 70060)) /* original 0x0004b9d4 */
#define _tui_in_think (*(uint32_t *)(recovered_section_DATA___bss + 70064)) /* original 0x0004b9d8 */
#define _tui_think_tag_idx (*(uint32_t *)(recovered_section_DATA___data + 124)) /* original 0x0003907c */
#define _tui_think_tags (*(char *)(recovered_section_DATA___const + 3280)) /* original 0x0003a634 */
#define _tui_in_code (*(uint32_t *)(recovered_section_DATA___bss + 70068)) /* original 0x0004b9dc */
#define _tui_fence_bt (*(uint32_t *)(recovered_section_DATA___bss + 70072)) /* original 0x0004b9e0 */
#define _pager_lines (*(uint32_t *)(recovered_section_DATA___bss + 70076)) /* original 0x0004b9e4 */
#define _pager_paused (*(uint32_t *)(recovered_section_DATA___bss + 70080)) /* original 0x0004b9e8 */
#define _chat_user_scrolled (*(uint32_t *)(recovered_section_DATA___bss + 70084)) /* original 0x0004b9ec */
#define _last_tree_6744 (*(uint32_t *)(recovered_section_DATA___bss + 70088)) /* original 0x0004b9f0 */
#define _resize_pending (*(uint32_t *)(recovered_section_DATA___bss + 70092)) /* original 0x0004b9f4 */
#define _default_models (*(char * *)(recovered_section_DATA___data + 128)) /* original 0x00039080 */
#define _slash_cmds (*(char * *)(recovered_section_DATA___data + 152)) /* original 0x00039098 */
#define _tab_cycle_7399 (*(int32_t *)(recovered_section_DATA___bss + 70096)) /* original 0x0004b9f8 */
#define _mcycle_7388 (*(int32_t *)(recovered_section_DATA___bss + 70100)) /* original 0x0004b9fc */
#define _C_679_7402 (*(char *)(recovered_section_TEXT___const + 140)) /* original 0x00036958 */
#define _C_878_8332 (*(char *)(recovered_section_DATA___const + 3376)) /* original 0x0003a694 */
#define _C_877_8310 (*(char *)(recovered_section_DATA___const + 3392)) /* original 0x0003a6a4 */
#define _C_846_8140 (*(char *)(recovered_section_DATA___const + 3408)) /* original 0x0003a6b4 */
#define _C_841_8129 (*(char *)(recovered_section_DATA___const + 3432)) /* original 0x0003a6cc */
#define _C_837_8113 (*(char *)(recovered_section_DATA___const + 3456)) /* original 0x0003a6e4 */
#define _C_836_8108 (*(char *)(recovered_section_DATA___const + 3472)) /* original 0x0003a6f4 */
#define _C_833_8101 (*(char *)(recovered_section_DATA___const + 3500)) /* original 0x0003a710 */
#define _C_821_8024 (*(char *)(recovered_section_DATA___const + 3520)) /* original 0x0003a724 */
#define _C_819_8020 (*(char *)(recovered_section_DATA___const + 3548)) /* original 0x0003a740 */
#define _g_worker (*(RecoveredWorker *)(recovered_section_DATA___bss + 2430432)) /* original 0x0028be08 */
#define _pkt_body_9543 (*(char *)(recovered_section_DATA___bss + 70104)) /* original 0x0004ba00 */
#define _C_1167_9747 (*(char *)(recovered_section_TEXT___const + 4236)) /* original 0x00037958 */
#define _C_1165_9738 (*(char *)(recovered_section_TEXT___const + 4492)) /* original 0x00037a58 */
#define _C_1160_9704 (*(char *)(recovered_section_TEXT___const + 4748)) /* original 0x00037b58 */
#define _C_1159_9703 (*(char *)(recovered_section_TEXT___const + 5260)) /* original 0x00037d58 */
#define _token_batch (*(RecoveredDString *)(recovered_section_DATA___bss + 2693120)) /* original 0x002cc028 */
#define _g_queue (*(RecoveredQueue *)(recovered_section_DATA___bss + 333272)) /* original 0x0008be00 */
#define _draft_buf (*(char *)(recovered_section_DATA___bss + 2430976)) /* original 0x0028c028 */
#define _report_buf (*(char *)(recovered_section_DATA___bss + 2693132)) /* original 0x002cc034 */
#define _think_tags (*(char *)(recovered_section_DATA___const + 3684)) /* original 0x0003a7c8 */
#define _g_disable_hl (*(int32_t *)(recovered_section_DATA___data + 300)) /* original 0x0003912c */
#define _kw_3427 (*(char * *)(recovered_section_DATA___data + 304)) /* original 0x00039130 */
#define _g_verbose (*(uint32_t *)(recovered_section_DATA___bss + 2694156)) /* original 0x002cc434 */
#define _linepos (*(uint32_t *)(recovered_section_DATA___bss + 2694160)) /* original 0x002cc438 */
#define _linebuf (*(char *)(recovered_section_DATA___bss + 2694172)) /* original 0x002cc444 */
#define _debug_log (*(FILE * *)(recovered_section_DATA___bss + 2694164)) /* original 0x002cc43c */
#define _in_think_block (*(uint32_t *)(recovered_section_DATA___bss + 2694168)) /* original 0x002cc440 */
#define _markers_3047 (*(char * *)(recovered_section_DATA___data + 440)) /* original 0x000391b8 */
#define _C_50_3155 (*(char *)(recovered_section_TEXT___const + 5292)) /* original 0x00037d78 */
#define _C_49_3154 (*(char *)(recovered_section_TEXT___const + 6316)) /* original 0x00038178 */
#define _g_rag_db_path (*(char *)(recovered_section_DATA___data + 484)) /* original 0x000391e4 */
#define _C_67_3336 (*(char *)(recovered_section_TEXT___const + 7340)) /* original 0x00038578 */
#define _C_66_3334 (*(char *)(recovered_section_TEXT___const + 8364)) /* original 0x00038978 */
#define _C_62_3329 (*(char *)(recovered_section_TEXT___const + 8876)) /* original 0x00038b78 */
#define _ep (*(char * *)(recovered_section_DATA___data + 1508)) /* original 0x000395e4 */
#define _C_89_4654 (*(char *)(recovered_section_TEXT___const + 9900)) /* original 0x00038f78 */
#define _buf_2670 (*(char *)(recovered_section_DATA___bss + 2957340)) /* original 0x0030c844 */
#define _funcptr (*(uint32_t *)(recovered_section_DATA___bss + 2957596)) /* original 0x0030c944 */
#define _NXArgc (*(uint32_t *)(recovered_section_DATA___data + 0)) /* original 0x00039000 */
#define _NXArgv (*(uint32_t *)(recovered_section_DATA___data + 4)) /* original 0x00039004 */
#define ___progname (*(uint32_t *)(recovered_section_DATA___data + 12)) /* original 0x0003900c */
#define _all_themes ((RecoveredTheme **)(recovered_section_DATA___data + 84)) /* original 0x00039054 */
#define _catch_exception_raise (*(uint32_t *)(recovered_section_DATA___common + 0)) /* original 0x0030c960 */
#define _catch_exception_raise_state (*(uint32_t *)(recovered_section_DATA___common + 4)) /* original 0x0030c964 */
#define _catch_exception_raise_state_identity (*(uint32_t *)(recovered_section_DATA___common + 8)) /* original 0x0030c968 */
#define _clock_alarm_reply (*(uint32_t *)(recovered_section_DATA___common + 12)) /* original 0x0030c96c */
#define _do_mach_notify_dead_name (*(uint32_t *)(recovered_section_DATA___common + 16)) /* original 0x0030c970 */
#define _do_mach_notify_no_senders (*(uint32_t *)(recovered_section_DATA___common + 20)) /* original 0x0030c974 */
#define _do_mach_notify_port_deleted (*(uint32_t *)(recovered_section_DATA___common + 24)) /* original 0x0030c978 */
#define _do_mach_notify_send_once (*(uint32_t *)(recovered_section_DATA___common + 28)) /* original 0x0030c97c */
#define _do_seqnos_mach_notify_dead_name (*(uint32_t *)(recovered_section_DATA___common + 32)) /* original 0x0030c980 */
#define _do_seqnos_mach_notify_no_senders (*(uint32_t *)(recovered_section_DATA___common + 36)) /* original 0x0030c984 */
#define _do_seqnos_mach_notify_port_deleted (*(uint32_t *)(recovered_section_DATA___common + 40)) /* original 0x0030c988 */
#define _do_seqnos_mach_notify_send_once (*(uint32_t *)(recovered_section_DATA___common + 44)) /* original 0x0030c98c */
#define _environ (*(uint32_t *)(recovered_section_DATA___data + 8)) /* original 0x00039008 */
#define _g_mcp (*(RecoveredMCPState *)(recovered_section_DATA___common + 9248288)) /* original 0x00bde780 */
#define _g_model_cache (*(RecoveredModelCache *)(recovered_section_DATA___common + 9242896)) /* original 0x00bdd270 */
#define _g_platform (*(uint32_t *)(recovered_section_DATA___data + 1512)) /* original 0x000395e8 */
#define _g_platform_machine ((char *)(recovered_section_DATA___data + 1612)) /* original 0x0003964c */
#define _g_platform_name ((char *)(recovered_section_DATA___data + 1516)) /* original 0x000395ec */
#define _g_platform_version (*(char *)(recovered_section_DATA___data + 1580)) /* original 0x0003962c */
#define _g_state (*(RecoveredAppState *)(recovered_section_DATA___common + 64)) /* original 0x0030c9a0 */
#define _g_theme (*(RecoveredTheme * *)(recovered_section_DATA___data + 80)) /* original 0x00039050 */
#define _receive_samples (*(char *)(recovered_section_DATA___common + 48)) /* original 0x0030c990 */
#define DAT_000395e3 (*(char *)(recovered_section_DATA___data + 1507))
#define DAT_0003964b (*(char *)(recovered_section_DATA___data + 1611))
#define DAT_0003968b (*(char *)(recovered_section_DATA___data + 1675))
#define DAT_0003a63c (*(uint32_t *)(recovered_section_DATA___const + 3288))
#define DAT_0003a7d0 (*(uint32_t *)(recovered_section_DATA___const + 3692))
#define DAT_0003a7d4 (*(uint32_t *)(recovered_section_DATA___const + 3696))
#define DAT_0003b17c (*(uint32_t *)(recovered_section_DATA___bss + 2388))
#define DAT_0003b180 (*(uint32_t *)(recovered_section_DATA___bss + 2392))
#define DAT_0003b980 (*(uint32_t *)(recovered_section_DATA___bss + 4440))
#define DAT_0003b984 (*(uint32_t *)(recovered_section_DATA___bss + 4444))
#define DAT_0004c67c (*(uint32_t *)(recovered_section_DATA___bss + 73300))
#define DAT_0028be04 (*(uint32_t *)(recovered_section_DATA___bss + 2430428))
#define DAT_0028c029 (*(char *)(recovered_section_DATA___bss + 2430977))
#define DAT_002cc02c (*(uint32_t *)(recovered_section_DATA___bss + 2693124))
#define PTR_s_char_00039134 (*(char * *)(recovered_section_DATA___data + 308))
#define PTR_s_deepseek_v4_pro_00039018 (*(char * *)(recovered_section_DATA___data + 24))
#define PTR_s_steps__000391bc (*(char * *)(recovered_section_DATA___data + 444))
#define PTR__g_state_000396a0 (&_g_state)
#define PTR__g_model_cache_000396ac (&_g_model_cache)
#define PTR__g_mcp_000396b4 (&_g_mcp)
#define PTR__g_theme_000396c0 (&_g_theme)
#define PTR__g_platform_000396b8 (&_g_platform)
#define PTR__g_platform_name_0003969c _g_platform_name
#define PTR__g_platform_machine_000396a8 _g_platform_machine
#define PTR_000396a4 ((char *)__sF)
#define PTR_000396b0 ((char *)&_DefaultRuneLocale)
#define PTR_000396bc (&stdscr)
void app_storage_init(void);
#define APP_PTR_00039018 (*(char * *)(recovered_section_DATA___data + 24))
#define APP_PTR_00039134 (*(char * *)(recovered_section_DATA___data + 308))
#define APP_PTR_000391bc (*(char * *)(recovered_section_DATA___data + 444))
#define APP_PTR_0003a644 (*(char * *)(recovered_section_DATA___const + 3296))
#define APP_PTR_0003a7cc (*(char * *)(recovered_section_DATA___const + 3688))
#define APP_PTR_0003a7d8 (*(char * *)(recovered_section_DATA___const + 3700))
#define UNK_0003a638 (*(char *)(recovered_section_DATA___const + 3284))
#define UNK_0003a640 (*(char *)(recovered_section_DATA___const + 3292))
#endif
