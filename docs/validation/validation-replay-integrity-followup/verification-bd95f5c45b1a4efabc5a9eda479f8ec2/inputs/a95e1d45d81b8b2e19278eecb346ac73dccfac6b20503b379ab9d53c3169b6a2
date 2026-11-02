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
 * _tool_call_copy @ 0x00006cfc..0x00006de0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Deep-copy a 0x51c-byte tool-call record and up to 99 dynamic-string arguments.
 * Parameters/return (inferred): void _tool_call_copy(RecoveredToolCall *destination,RecoveredToolCall *source)
 * Direct callers: _simple_mode@0x00004e30, _message_copy@0x00006eb0
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00006cfc_tool_call_copy.s
 */
void tool_call_copy(RecoveredToolCall *destination,RecoveredToolCall *source)

{
  int iVar1;
  RecoveredDString *str;
  int iVar2;
  
  if ((destination != (RecoveredToolCall *)0x0) && (source != (RecoveredToolCall *)0x0)) {
    memset(destination,0,0x51c);
    destination->tool_type = source->tool_type;
    app_copy_padded(destination->mcp_name,source->mcp_name,0x1f);
    destination->argument_count = source->argument_count;
    app_copy_padded(destination->call_id,source->call_id,0x3f);
    destination->execution_status = source->execution_status;
    ds_init(&destination->result);
    ds_copy(&destination->result,&source->result);
    if (0 < source->argument_count) {
      iVar1 = 0;
      iVar2 = 0;
      do {
        str = (RecoveredDString *)((int)&destination->arguments[0].data + iVar2);
        ds_init(str);
        ds_copy(str,(RecoveredDString *)((int)&source->arguments[0].data + iVar2));
        iVar1 = iVar1 + 1;
        if (source->argument_count <= iVar1) {
          return;
        }
        iVar2 = iVar2 + 0xc;
      } while (iVar1 != 99);
    }
  }
  return;
}

/*
 * _message_free @ 0x00006de0..0x00006eb0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Release dynamic strings and tool-call allocations owned by a conversation message.
 * Parameters/return (inferred): void _message_free(RecoveredMessage *message)
 * Direct callers: _handle_slash_command@0x0000362c, _main@0x00005498, _sa_child_main@0x0001327c, _handle_cmd@0x0001b004, _worker_child_main@0x00022774, _worker_deserialize_out@0x00024b1c, _context_drop_oldest@0x00025d00
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00006de0_message_free.s
 */
void message_free(RecoveredMessage *message)

{
  RecoveredToolCall *pRVar1;
  int iVar2;
  int iVar3;
  int iVar4;
  
  if (message != (RecoveredMessage *)0x0) {
    ds_free(&message->content);
    if (message->tool_calls != (RecoveredToolCall *)0x0) {
      if (0 < message->tool_call_count) {
        iVar2 = 0;
        iVar3 = 0;
        do {
          pRVar1 = message->tool_calls;
          ds_free((RecoveredDString *)(pRVar1->call_id + iVar3 + 0x44));
          if (0 < *(int *)(pRVar1->call_id + iVar3 + -4)) {
            iVar4 = 0;
            do {
              ds_free((RecoveredDString *)((int)&pRVar1->arguments[iVar4].data + iVar3));
              iVar4 = iVar4 + 1;
            } while (iVar4 < *(int *)(pRVar1->call_id + iVar3 + -4));
          }
          iVar2 = iVar2 + 1;
          iVar3 = iVar3 + 0x51c;
        } while (iVar2 < message->tool_call_count);
      }
      free(message->tool_calls);
      message->tool_calls = (RecoveredToolCall *)0x0;
    }
    message->tool_call_count = 0;
    message->tool_call_capacity = 0;
  }
  return;
}

/*
 * _message_copy @ 0x00006eb0..0x00007030
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Release destination-owned tool data, then deep-copy a 0x5c-byte conversation message and its tool calls.
 * Parameters/return (inferred): void _message_copy(RecoveredMessage *destination,RecoveredMessage *source)
 * Direct callers: _handle_slash_command@0x0000362c, _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00006eb0_message_copy.s
 */
void message_copy(RecoveredMessage *destination,RecoveredMessage *source)

{
  char *pcVar1;
  int iVar2;
  RecoveredToolCall *pRVar3;
  int iVar4;
  int iVar5;
  
  if ((destination != (RecoveredMessage *)0x0) && (source != (RecoveredMessage *)0x0)) {
    if (destination->tool_calls != (RecoveredToolCall *)0x0) {
      if (0 < destination->tool_call_count) {
        iVar2 = 0;
        iVar5 = 0;
        do {
          ds_free((RecoveredDString *)(destination->tool_calls->call_id + iVar5 + 0x44));
          pcVar1 = destination->tool_calls->mcp_name + iVar5 + -4;
          if (0 < *(int *)(pcVar1 + 0x4c8)) {
            iVar4 = 0;
            do {
              ds_free((RecoveredDString *)(pcVar1 + iVar4 * 0xc + 0x24));
              iVar4 = iVar4 + 1;
              pcVar1 = destination->tool_calls->mcp_name + iVar5 + -4;
            } while (iVar4 < *(int *)(pcVar1 + 0x4c8));
          }
          iVar2 = iVar2 + 1;
          iVar5 = iVar5 + 0x51c;
        } while (iVar2 < destination->tool_call_count);
      }
      free(destination->tool_calls);
    }
    memset(destination,0,0x5c);
    destination->role = source->role;
    destination->tool_call_count = source->tool_call_count;
    app_copy_padded(destination->call_id,source->call_id,0x3f);
    ds_init(&destination->content);
    ds_copy(&destination->content,&source->content);
    iVar2 = source->tool_call_count;
    if ((0 < iVar2) && (source->tool_calls != (RecoveredToolCall *)0x0)) {
      destination->tool_call_capacity = iVar2;
      pRVar3 = malloc(iVar2 * 0x51c);
      destination->tool_calls = pRVar3;
      if (0 < source->tool_call_count) {
        iVar2 = 0;
        iVar5 = 0;
        do {
          tool_call_copy((RecoveredToolCall *)(destination->tool_calls->mcp_name + iVar5 + -4),
                          (RecoveredToolCall *)(source->tool_calls->mcp_name + iVar5 + -4));
          iVar2 = iVar2 + 1;
          iVar5 = iVar5 + 0x51c;
        } while (iVar2 < source->tool_call_count);
      }
    }
  }
  return;
}

/*
 * _file_ctx_find @ 0x00007030..0x000070b0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Find a cached per-file context record for the supplied path.
 * Parameters/return (inferred): RecoveredFileContext * _file_ctx_find(RecoveredAppState *app,char *path)
 * Direct callers: _file_ctx_update@0x000070b0, _file_ctx_mark_stale@0x00007164, _tool_read_file@0x00013e64, _guard_file_has_been_read@0x00014134
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00007030_file_ctx_find.s
 */
RecoveredFileContext * file_ctx_find(RecoveredAppState *app,char *path)

{
  int iVar1;
  int iVar2;
  int iVar3;
  
  iVar2 = app->file_context_count;
  if (0 < iVar2) {
    iVar3 = 0;
    do {
      iVar1 = strcmp(_g_state.history.entries[0] +
                      (int)((app->history).entries[0x9c] + iVar3 * 0x418 + 0x7614),path);
      if (iVar1 == 0) {
        return app->file_contexts + iVar3;
      }
      iVar3 = iVar3 + 1;
    } while (iVar3 != iVar2);
  }
  return (RecoveredFileContext *)0x0;
}

/*
 * _file_ctx_update @ 0x000070b0..0x00007164
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Update file-context size/line-count/truncation metadata after a read.
 * Parameters/return (inferred): void _file_ctx_update(RecoveredAppState *app,char *path,uint bytes_read,int first_message, int last_message,int partial)
 * Direct callers: _handle_slash_command@0x0000362c, _tool_read_file@0x00013e64, _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000070b0_file_ctx_update.s
 */
void file_ctx_update(RecoveredAppState *app,char *path,uint bytes_read,int first_message,
                     int last_message,int partial)

{
  RecoveredFileContext *output;
  int iVar1;
  
  output = file_ctx_find(app,path);
  if (output == (RecoveredFileContext *)0x0) {
    iVar1 = app->file_context_count;
    if (199 < iVar1) {
      return;
    }
    output = app->file_contexts + iVar1;
    app->file_context_count = iVar1 + 1;
    memset(output,0,0x418);
    strncpy(output->path,path,0x3ff);
  }
  output->bytes_read = bytes_read;
  output->first_message = first_message;
  output->last_message = last_message;
  output->flags = (uint)(partial != 0) * 0x40000000 | (output->flags & 0x3fffffff);
  return;
}

/*
 * _file_ctx_mark_stale @ 0x00007164..0x00007198
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Mark file-context metadata stale after a write or edit.
 * Parameters/return (inferred): void _file_ctx_mark_stale(RecoveredAppState *app,char *path)
 * Direct callers: _tool_write_file@0x0001420c, _tool_edit_file@0x000144a8
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00007164_file_ctx_mark_stale.s
 */
void file_ctx_mark_stale(RecoveredAppState *app,char *path)

{
  RecoveredFileContext *pRVar1;
  
  pRVar1 = file_ctx_find(app,path);
  if (pRVar1 != (RecoveredFileContext *)0x0) {
    pRVar1->flags = pRVar1->flags | 0x80000000;
  }
  return;
}

/*
 * _file_ctx_check_stale @ 0x00007198..0x0000722c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Check tracked file metadata for stale context.
 * Parameters/return (inferred): int _file_ctx_check_stale(RecoveredFileContext *entry)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): reads filesystem metadata
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00007198_file_ctx_check_stale.s
 */
/* Analyst reconstruction of _file_ctx_check_stale at 0x00007198. See index/manual-review.json. */
int file_ctx_check_stale(RecoveredFileContext *entry)
{
    unsigned char stat_bytes[96];
    unsigned int observed_low, observed_high;
    if (entry == 0 || entry->path[0] == '\0') return 0;
    if (stat(entry->path, (struct stat *)(stat_bytes)) != 0) return 1;
    /* Original stack+0x58 minus stat-base stack+0x38 = stat offset 0x20.
       Preserve this word, not the incompatible modern SDK field name Ghidra
       attached to it. Its signed extension is compared as an unsigned pair. */
    observed_low = PPC_BE32(stat_bytes + 0x20);
    observed_high = (observed_low & 0x80000000U) ? 0xffffffffU : 0;
    if (entry->timestamp_high < observed_high ||
        (entry->timestamp_high == observed_high && entry->timestamp_low < observed_low)) {
        entry->flags |= 0x80000000U;
        return 1;
    }
    return 0;
}

/*
 * _config_context_window @ 0x0000722c..0x00007390
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in conversation_state: config context window. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _config_context_window(RecoveredConfig *config)
 * Direct callers: _token_budget_str@0x00007434, _config_get_model_context@0x000077ac, _ollama_build_request@0x0000bdf0, _handle_cmd@0x0001b004, _context_over_budget@0x00025c58, _context_enforce_hard_limit@0x000262b0
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/0000722c_config_context_window.s
 */
/* Analyst reconstruction of _config_context_window at 0x0000722c. See index/manual-review.json. */
int config_context_window(RecoveredConfig *config)
{

    int index;
    if (config->context_window > 0) return config->context_window;
    if (config->active_profile[0] != '\0' && config->profile_count > 0) {
        for (index = 0; index < config->profile_count; ++index) {
            RecoveredProfile *profile = &config->profiles[index];
            if (strcmp(profile->name, config->active_profile) == 0) {
                if (profile != 0 && profile->context_window > 0)
                    return profile->context_window;
                break;
            }
        }
    }
    if (config->model[0] != '\0' && _g_model_cache.count > 0) {
        for (index = 0; index < _g_model_cache.count; ++index)
            if (strcmp(_g_model_cache.models[index].name, config->model) == 0 &&
                _g_model_cache.models[index].context_window > 0)
                return _g_model_cache.models[index].context_window;
    }
    switch (config->provider) {
    case 0: return model_context(config->model);
    case 1: return 128000;
    case 2: return 200000; /* 0x30d40 is an integer, not the string at that VA. */
    case 3: return 1048576;
    default: return 32768;
    }
}

/*
 * _token_budget_update @ 0x00007390..0x00007434
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in conversation_state: token budget update. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _token_budget_update(RecoveredConversation *conversation)
 * Direct callers: _token_budget_str@0x00007434, _context_over_budget@0x00025c58, _context_enforce_hard_limit@0x000262b0
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00007390_token_budget_update.s
 */
void token_budget_update(RecoveredConversation *conversation)

{
  int iVar1;
  RecoveredToolCall *pRVar2;
  int iVar3;
  RecoveredConversation *pRVar4;
  int iVar5;
  RecoveredToolCall *pRVar6;
  int iVar7;
  uint uVar8;
  
  uVar8 = 0;
  if (0 < conversation->message_count) {
    iVar3 = 0;
    uVar8 = 0;
    pRVar4 = conversation;
    do {
      uVar8 = uVar8 + 0x40 + (pRVar4->messages[0].content.length >> 2);
      iVar5 = pRVar4->messages[0].tool_call_count;
      if (0 < iVar5) {
        pRVar6 = pRVar4->messages[0].tool_calls;
        iVar7 = 0;
        do {
          uVar8 = uVar8 + ((pRVar6->result).length >> 2);
          iVar1 = pRVar6->argument_count;
          pRVar2 = pRVar6;
          if (0 < iVar1) {
            do {
              uVar8 = uVar8 + (pRVar2->arguments[0].length >> 2);
              iVar1 = iVar1 + -1;
              pRVar2 = (RecoveredToolCall *)(pRVar2->mcp_name + 8);
            } while (iVar1 != 0);
          }
          iVar7 = iVar7 + 1;
          pRVar6 = pRVar6 + 1;
        } while (iVar7 != iVar5);
      }
      iVar3 = iVar3 + 1;
      pRVar4 = (RecoveredConversation *)(pRVar4->messages + 1);
    } while (iVar3 != conversation->message_count);
  }
  conversation->estimated_tokens = uVar8;
  return;
}

/*
 * _token_budget_str @ 0x00007434..0x00007600
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in conversation_state: token budget str. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): char * _token_budget_str(char *output,uint capacity,RecoveredConversation *conversation, RecoveredConfig *config)
 * Direct callers: _redraw_status@0x000193cc
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/00007434_token_budget_str.s
 */
char * token_budget_str(char *output,uint capacity,RecoveredConversation *conversation,
                        RecoveredConfig *config)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x40] __attribute__((aligned(16)));
#define local_30 (*((undefined4 *)(app_frame + 0x0))) /* original SP - 0x30 */
#define uStack_2c (*((undefined4 *)(app_frame + 0x4))) /* original SP - 0x2c */

  uint uVar3;
  uint uVar4;
  char *pcVar5;
  double dVar6;
  double dVar7;


  token_budget_update(conversation);
  uVar3 = config_context_window(config);
  uVar4 = conversation->estimated_tokens;
  if ((int)uVar3 < 1) {
    dVar7 = (double)(float)(app_bits_double(CONCAT44(0x43300000,uVar4 ^ 0x80000000)) - 4503601774854144.0);
    dVar6 = 0.0;
  }
  else {
    dVar7 = (double)(float)(app_bits_double(CONCAT44(0x43300000,uVar4 ^ 0x80000000)) - 4503601774854144.0);
    dVar6 = (double)((float)(dVar7 * 100.0) /
                    (float)(app_bits_double(CONCAT44(0x43300000,uVar3 ^ 0x80000000)) - 4503601774854144.0));
  }
  if ((int)uVar4 < 1000000) {
    if ((int)uVar4 < 1000) {
      pcVar5 = "";
    }
    else {
      dVar7 = (double)(float)(dVar7 / 1000.0);
      pcVar5 = "k";
    }
  }
  else {
    dVar7 = (double)(float)(dVar7 / 1000000.0);
    pcVar5 = "M";
  }
  local_30 = (undefined4)(APP_BITS(dVar6) >> 0x20);

  uStack_2c = SUB84(dVar6,0);

  local_30 = (undefined4)(APP_BITS(dVar7) >> 0x20);
  uStack_2c = SUB84(dVar7,0);
  /* Original f3 is scaled independently and passed in the stack argument area. */
  double maximum = (float)(int32_t)uVar3;
  const char *maximum_suffix = "";
  if ((int32_t)uVar3 >= 1000000) {
    maximum = (float)(maximum / 1000000.0); maximum_suffix = "M";
  } else if ((int32_t)uVar3 >= 1000) {
    maximum = (float)(maximum / 1000.0); maximum_suffix = "k";
  }
  snprintf(output,capacity,"Ctx: %.0f%%  %.1f%s/%.0f%s",dVar6,dVar7,pcVar5,
           maximum,maximum_suffix);
  return output;

#undef local_30
#undef uStack_2c
}

/*
 * _conversation_bytes @ 0x00007600..0x00007694
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in conversation_state: conversation bytes. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): uint _conversation_bytes(RecoveredConversation *conversation)
 * Direct callers: _conversation_mem_str@0x00007694
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00007600_conversation_bytes.s
 */
uint conversation_bytes(RecoveredConversation *conversation)

{
  int *piVar1;
  int iVar2;
  RecoveredToolCall *pRVar3;
  uint uVar4;
  int iVar5;
  RecoveredToolCall *pRVar6;
  int iVar7;
  int iVar8;
  
  piVar1 = &conversation->message_count;
  if (0 < *piVar1) {
    iVar5 = 0;
    uVar4 = 0;
    do {
      uVar4 = uVar4 + 0x5c + conversation->messages[0].content.length;
      iVar7 = conversation->messages[0].tool_call_count;
      if (0 < iVar7) {
        pRVar6 = conversation->messages[0].tool_calls;
        iVar8 = 0;
        do {
          uVar4 = uVar4 + (pRVar6->result).length;
          iVar2 = pRVar6->argument_count;
          pRVar3 = pRVar6;
          if (0 < iVar2) {
            do {
              uVar4 = uVar4 + pRVar3->arguments[0].length;
              iVar2 = iVar2 + -1;
              pRVar3 = (RecoveredToolCall *)(pRVar3->mcp_name + 8);
            } while (iVar2 != 0);
          }
          iVar8 = iVar8 + 1;
          pRVar6 = pRVar6 + 1;
        } while (iVar8 != iVar7);
      }
      iVar5 = iVar5 + 1;
      conversation = (RecoveredConversation *)(conversation->messages + 1);
    } while (iVar5 != *piVar1);
    return uVar4;
  }
  return 0;
}

/*
 * _conversation_mem_str @ 0x00007694..0x000077ac
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in conversation_state: conversation mem str. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): char * _conversation_mem_str(char *output,uint capacity,RecoveredConversation *conversation)
 * Direct callers: _redraw_status@0x000193cc
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/00007694_conversation_mem_str.s
 */
char * conversation_mem_str(char *output,uint capacity,RecoveredConversation *conversation)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x30] __attribute__((aligned(16)));
#define local_20 (*((undefined4 *)(app_frame + 0x0))) /* original SP - 0x20 */
#define uStack_1c (*((undefined4 *)(app_frame + 0x4))) /* original SP - 0x1c */

  uint uVar1;
  double dVar2;


  uVar1 = conversation_bytes(conversation);
  if ((int)uVar1 < 0x100000) {
    if ((int)uVar1 < 0x400) {
      snprintf(output,capacity,"%dB",uVar1);
    }
    else {
      dVar2 = (double)((float)(app_bits_double(CONCAT44(0x43300000,uVar1 ^ 0x80000000)) - 4503601774854144.0)
                      * 0.0009765625);
      local_20 = (undefined4)(APP_BITS(dVar2) >> 0x20);
      uStack_1c = SUB84(dVar2,0);
      snprintf(output,capacity,"%.1fkB",dVar2);
    }
  }
  else {
    dVar2 = (double)((float)(app_bits_double(CONCAT44(0x43300000,uVar1 ^ 0x80000000)) - 4503601774854144.0) *
                    9.536743e-07);
    local_20 = (undefined4)(APP_BITS(dVar2) >> 0x20);
    uStack_1c = SUB84(dVar2,0);
    snprintf(output,capacity,"%.1fMB",dVar2);
  }
  return output;

#undef local_20
#undef uStack_1c
}

/*
 * _config_get_model_context @ 0x000077ac..0x000078a8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in conversation_state: config get model context. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): char * _config_get_model_context(RecoveredConfig *config)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/000077ac_config_get_model_context.s
 */
char * config_get_model_context(RecoveredConfig *config)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x30] __attribute__((aligned(16)));
#define local_20 (*((undefined4 *)(app_frame + 0x0))) /* original SP - 0x20 */
#define uStack_1c (*((undefined4 *)(app_frame + 0x4))) /* original SP - 0x1c */

  uint uVar1;
  double dVar2;


  uVar1 = config_context_window(config);
  if ((int)uVar1 < 1000000) {
    if ((int)uVar1 < 1000) {
      snprintf(&_buf_5343,0x40,"%d",uVar1);
    }
    else {
      snprintf(&_buf_5343,0x40,"%dk",(int)uVar1 / 1000);
    }
  }
  else {
    dVar2 = (double)((float)(app_bits_double(CONCAT44(0x43300000,uVar1 ^ 0x80000000)) - 4503601774854144.0) /
                    1e+06);
    local_20 = (undefined4)(APP_BITS(dVar2) >> 0x20);
    uStack_1c = SUB84(dVar2,0);
    snprintf(&_buf_5343,0x40,"%.1fM",dVar2);
  }
  return &_buf_5343;

#undef local_20
#undef uStack_1c
}

