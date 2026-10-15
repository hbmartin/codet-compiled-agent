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
 * _get_home @ 0x000078a8..0x0000790c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Resolve a home-directory value for this translation unit; inspect this body because fallback behavior differs between helpers with the same symbol name.
 * Parameters/return (inferred): char * _get_home(void)
 * Direct callers: _ensure_session_dir@0x0000790c, _session_get_path@0x00007980
 * Possible side effects (direct-call closure, incomplete for indirect calls): reads process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000078a8_get_home.s
 */
char * sessions_history_get_home(void)

{
  char *pcVar1;
  int iVar2;
  
  pcVar1 = getenv("HOME");
  if (pcVar1 == (char *)0x0) {
    iVar2 = (int)getpwuid(getuid());
    if (iVar2 == 0) goto LAB_000078f8;
    pcVar1 = *(char **)(iVar2 + 0x1c);
  }
  if (pcVar1 != (char *)0x0) {
    return pcVar1;
  }
LAB_000078f8:
  return ".";
}

/*
 * _ensure_session_dir @ 0x0000790c..0x00007980
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Create the resolved-home/.ppccode_sessions directory with mode 0755.
 * Parameters/return (inferred): undefined4 _ensure_session_dir(void)
 * Direct callers: _session_save@0x000079f0
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; reads process environment; creates directories
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000790c_ensure_session_dir.s
 */
undefined4 ensure_session_dir(void)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x430] __attribute__((aligned(16)));
#define acStack_418 (*((char (*)[1044])(app_frame + 0x8))) /* original SP - 0x418 */

  undefined4 uVar1;
  int iVar2;
  int *piVar3;

  uVar1 = (undefined4)(sessions_history_get_home());
  snprintf(acStack_418,0x400,"%s/.ppccode_sessions",(char *)(uVar1));
  iVar2 = mkdir(acStack_418,0x1ed);
  if ((iVar2 != 0) && (piVar3 = __error(), *piVar3 != 0x11)) {
    return 0xffffffff;
  }
  return 0;

#undef acStack_418
}

/*
 * _session_get_path @ 0x00007980..0x000079f0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Build a session pathname, using session as the empty-name default and a 190-character name limit.
 * Parameters/return (inferred): void _session_get_path(char *output,uint capacity,char *name)
 * Direct callers: _handle_slash_command@0x0000362c, _handle_cmd@0x0001b004, _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; reads process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00007980_session_get_path.s
 */
void session_get_path(char *output,uint capacity,char *name)

{
  undefined4 uVar1;
  
  if ((name == (char *)0x0) || (*name == '\0')) {
    name = "session";
  }
  uVar1 = (undefined4)(sessions_history_get_home());
  snprintf(output,capacity,"%s/.ppccode_sessions/%.190s.json",(char *)(uVar1),name);
  return;
}

/*
 * _session_save @ 0x000079f0..0x00007cb8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Write working-directory and message role/content JSON for a saved conversation.
 * Parameters/return (inferred): int _session_save(RecoveredConversation *conversation,char *name,char *working_directory)
 * Direct callers: _handle_slash_command@0x0000362c, _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; opens/creates/truncates filesystem paths according to the supplied mode; writes bytes to a stream; reads process environment; creates directories
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000079f0_session_save.s
 */
int session_save(RecoveredConversation *conversation,char *name,char *working_directory)

{
  char cVar1;
  int iVar2;
  int iVar3;
  void *stream;
  char *pcVar4;
  RecoveredConversation *pRVar5;
  
  iVar3 = ensure_session_dir();
  if ((iVar3 == 0) && (stream = fopen(name,"w"), stream != (void *)0x0)) {
    fwrite("{\n  \"working_dir\": \"",1,0x14,stream);
    if (working_directory == (char *)0x0) {
      working_directory = "";
    }
    cVar1 = *working_directory;
    while (iVar3 = (int)cVar1, iVar3 != 0) {
      if ((iVar3 == 0x5c) || (iVar3 == 0x22)) {
        fputc(0x5c,stream);
      }
      else if (iVar3 == 10) {
        fwrite("\\n",1,2,stream);
      }
      else {
        fputc(iVar3,stream);
      }
      working_directory = working_directory + 1;
      cVar1 = *working_directory;
    }
    fwrite("\",\n  \"messages\": [\n",1,0x13,stream);
    if (0 < conversation->message_count) {
      iVar3 = 0;
      pRVar5 = conversation;
      do {
        iVar2 = pRVar5->messages[0].role;
        if (iVar2 == 1) {
          pcVar4 = "user";
        }
        else if (iVar2 == 0) {
          pcVar4 = "system";
        }
        else if (iVar2 == 2) {
          pcVar4 = "assistant";
        }
        else if (iVar2 == 3) {
          pcVar4 = "tool";
        }
        else {
          pcVar4 = "unknown";
        }
        fprintf(stream,"    {\"role\": \"%s\", \"content\": \"",pcVar4);
        pcVar4 = ds_cstr(&conversation->messages[iVar3].content);
        cVar1 = *pcVar4;
        while (iVar2 = (int)cVar1, iVar2 != 0) {
          if ((iVar2 == 0x5c) || (iVar2 == 0x22)) {
            fputc(0x5c,stream);
          }
          else if (iVar2 == 10) {
            fwrite("\\n",1,2,stream);
          }
          else if (iVar2 == 0xd) {
            fwrite("\\r",1,2,stream);
          }
          else if (iVar2 == 9) {
            fwrite("\\t",1,2,stream);
          }
          else {
            fputc(iVar2,stream);
          }
          pcVar4 = pcVar4 + 1;
          cVar1 = *pcVar4;
        }
        iVar3 = iVar3 + 1;
        if (iVar3 < conversation->message_count) {
          pcVar4 = ",";
        }
        else {
          pcVar4 = "";
        }
        fprintf(stream,"\"}%s\n",pcVar4);
        pRVar5 = (RecoveredConversation *)(pRVar5->messages + 1);
      } while (iVar3 < conversation->message_count);
    }
    fwrite("  ]\n}\n",1,6,stream);
    fclose(stream);
    iVar3 = 0;
  }
  else {
    iVar3 = -1;
  }
  return iVar3;
}

/*
 * _session_load @ 0x00007cb8..0x00008064
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Load bounded-size session JSON, check a supplied working-directory constraint and restore messages.
 * Parameters/return (inferred): int _session_load(RecoveredConversation *conversation,char *name,char *working_directory)
 * Direct callers: _handle_slash_command@0x0000362c, _handle_cmd@0x0001b004, _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): opens/creates/truncates filesystem paths according to the supplied mode; reads bytes from a stream
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00007cb8_session_load.s
 */
int session_load(RecoveredConversation *conversation,char *name,char *working_directory)

{
  RecoveredToolCall *pRVar1;
  FILE *stream;
  uint count;
  char *pcVar2;
  RecoveredCJSON *object;
  RecoveredCJSON *pRVar3;
  int iVar4;
  RecoveredCJSON *pRVar5;
  RecoveredCJSON *pRVar6;
  RecoveredConversation *pRVar7;
  RecoveredToolCall **ppRVar8;
  int iVar9;
  int iVar10;
  RecoveredMessage *output;
  int iVar11;
  
  stream = fopen(name,"r");
  if (stream != (FILE *)0x0) {
    fseek(stream,0,2);
    count = ftell(stream);
    rewind(stream);
    if (0x7fffff < count - 1) {
      fclose(stream);
      return -1;
    }
    pcVar2 = malloc(count + 1);
    if (pcVar2 == (char *)0x0) {
      fclose(stream);
      return -1;
    }
    fread(pcVar2,1,count,stream);
    pcVar2[count] = '\0';
    fclose(stream);
    object = cJSON_Parse(pcVar2);
    free(pcVar2);
    if (object != (RecoveredCJSON *)0x0) {
      pRVar3 = cJSON_GetObjectItem(object,"working_dir");
      if ((((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) &&
          (working_directory != (char *)0x0)) &&
         ((*working_directory != '\0' &&
          (iVar4 = strcmp(pRVar3->value_string,working_directory), iVar4 != 0)))) {
        cJSON_Delete(object);
        return -2;
      }
      pRVar3 = cJSON_GetObjectItem(object,"messages");
      if (pRVar3 == (RecoveredCJSON *)0x0) {
        cJSON_Delete(object);
        return -1;
      }
      if (0 < conversation->message_count) {
        iVar4 = 0;
        pRVar7 = conversation;
        do {
          ds_free(&conversation->messages[iVar4].content);
          ppRVar8 = &pRVar7->messages[0].tool_calls;
          if (pRVar7->messages[0].tool_calls != (RecoveredToolCall *)0x0) {
            if (0 < pRVar7->messages[0].tool_call_count) {
              iVar9 = 0;
              iVar10 = 0;
              do {
                pRVar1 = *ppRVar8;
                if (0 < *(int *)(pRVar1->call_id + iVar10 + -4)) {
                  iVar11 = 0;
                  do {
                    ds_free((RecoveredDString *)((int)&pRVar1->arguments[iVar11].data + iVar10));
                    iVar11 = iVar11 + 1;
                  } while (iVar11 < *(int *)(pRVar1->call_id + iVar10 + -4));
                }
                ds_free((RecoveredDString *)(pRVar1->call_id + iVar10 + 0x44));
                iVar9 = iVar9 + 1;
                iVar10 = iVar10 + 0x51c;
              } while (iVar9 < pRVar7->messages[0].tool_call_count);
            }
            free(*ppRVar8);
            *ppRVar8 = (RecoveredToolCall *)0x0;
          }
          iVar4 = iVar4 + 1;
          pRVar7 = (RecoveredConversation *)(pRVar7->messages + 1);
        } while (iVar4 < conversation->message_count);
      }
      conversation->message_count = 0;
      iVar4 = cJSON_GetArraySize(pRVar3);
      if ((0 < iVar4) && (conversation->message_count < 0x180)) {
        iVar9 = 0;
        do {
          pRVar5 = cJSON_GetArrayItem(pRVar3,iVar9);
          pRVar6 = cJSON_GetObjectItem(pRVar5,"role");
          pRVar5 = cJSON_GetObjectItem(pRVar5,"content");
          if ((pRVar6 != (RecoveredCJSON *)0x0) && (pRVar5 != (RecoveredCJSON *)0x0)) {
            iVar10 = conversation->message_count;
            output = conversation->messages + iVar10;
            conversation->message_count = iVar10 + 1;
            memset(output,0,0x5c);
            ds_init(&output->content);
            ds_append(&output->content,pRVar5->value_string);
            pcVar2 = pRVar6->value_string;
            iVar11 = strcmp(pcVar2,"user");
            if (iVar11 == 0) {
              conversation->messages[iVar10].role = 1;
            }
            else {
              iVar10 = strcmp(pcVar2,"assistant");
              if (iVar10 == 0) {
                output->role = 2;
              }
              else {
                iVar10 = strcmp(pcVar2,"system");
                if (iVar10 == 0) {
                  output->role = 0;
                }
                else {
                  iVar10 = strcmp(pcVar2,"tool");
                  if (iVar10 == 0) {
                    output->role = 3;
                  }
                }
              }
            }
          }
          iVar9 = iVar9 + 1;
        } while ((iVar4 != iVar9) && (conversation->message_count < 0x180));
      }
      cJSON_Delete(object);
      return 0;
    }
  }
  return -1;
}

/*
 * _history_add @ 0x00008064..0x00008160
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage the interactive input-history state: add. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _history_add(RecoveredHistory *history, const char *text)
 * Direct callers: _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00008064_history_add.s
 */
/* Analyst reconstruction of _history_add at 0x00008064. See index/manual-review.json. */
void history_add(RecoveredHistory *history, const char *text)
{
    int index;
    if (text == 0 || text[0] == '\0') return;
    if (history->count > 0) {
        index = (history->count + history->first - 1) % 256;
        if (strcmp(history->entries[index], text) == 0) return;
    }
    index = (history->count + history->first) % 256;
    strncpy(history->entries[index], text, 32767);
    history->entries[index][32767] = '\0';
    if (history->count < 256) ++history->count;
    else history->first = (history->first + 1) % 256;
    history->cursor = -1;
}

/*
 * _history_reset @ 0x00008160..0x00008170
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage the interactive input-history state: reset. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _history_reset(RecoveredHistory *history)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00008160_history_reset.s
 */
/* Analyst reconstruction of _history_reset at 0x00008160. See index/manual-review.json. */
void history_reset(RecoveredHistory *history)
{
    /* Reset navigation only: stored history is intentionally preserved. */
    history->cursor = -1;
}

/*
 * _history_prev @ 0x00008170..0x000081f8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage the interactive input-history state: prev. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): char *_history_prev(RecoveredHistory *history)
 * Direct callers: _read_input@0x0001a74c, _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00008170_history_prev.s
 */
/* Analyst reconstruction of _history_prev at 0x00008170. See index/manual-review.json. */
char *history_prev(RecoveredHistory *history)
{
    int index;
    if (history->count == 0) return 0;
    if (history->cursor == -1)
        index = (history->count + history->first - 1) % 256;
    else
        index = (history->cursor + 255) % 256;
    if (index < 0) index += 256;
    if (history->count >= 256 || index < history->count) {
        history->cursor = index;
        return history->entries[index];
    }
    return 0;
}

/*
 * _history_next @ 0x000081f8..0x00008288
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage the interactive input-history state: next. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): char *_history_next(RecoveredHistory *history)
 * Direct callers: _read_input@0x0001a74c, _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/000081f8_history_next.s
 */
/* Analyst reconstruction of _history_next at 0x000081f8. See index/manual-review.json. */
char *history_next(RecoveredHistory *history)
{
    int index;
    if (history->count == 0 || history->cursor == -1) return 0;
    index = (history->cursor + 1) % 256;
    if (index != (history->count + history->first) % 256 &&
        history->entries[index][0] != '\0') {
        history->cursor = index;
        return history->entries[index];
    }
    history->cursor = -1;
    return 0;
}

