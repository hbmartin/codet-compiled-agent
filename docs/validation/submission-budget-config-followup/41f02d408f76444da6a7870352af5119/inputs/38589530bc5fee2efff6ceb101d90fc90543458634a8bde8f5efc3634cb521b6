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
 * _plan_init @ 0x00024e34..0x00024e60
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage internal plan-step state: init. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _plan_init(RecoveredPlan *plan)
 * Direct callers: _handle_cmd@0x0001b004, _tui_run@0x0001df44, _worker_child_main@0x00022774, _plan_reset@0x00024e60
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00024e34_plan_init.s
 */
void plan_init(RecoveredPlan *plan)

{
  memset(plan,0,0x5020c);
  return;
}

/*
 * _plan_reset @ 0x00024e60..0x00024e80
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage internal plan-step state: reset. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _plan_reset(RecoveredPlan *plan)
 * Direct callers: _execute_plan_steps@0x00022454, _worker_child_main@0x00022774, _plan_parse@0x00024fa4
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00024e60_plan_reset.s
 */
void plan_reset(RecoveredPlan *plan)

{
  plan_init(plan);
  return;
}

/*
 * _plan_add_step @ 0x00024e80..0x00024f2c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage internal plan-step state: add step. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): int _plan_add_step(RecoveredPlan *plan,char *description,int tool_type,char *argument0, char *argument1)
 * Direct callers: _plan_parse@0x00024fa4
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00024e80_plan_add_step.s
 */
int plan_add_step(RecoveredPlan *plan,char *description,int tool_type,char *argument0,
                  char *argument1)

{
  int iVar1;
  int iVar2;
  RecoveredPlanStep *output;
  
  iVar2 = plan->count;
  iVar1 = -1;
  if (iVar2 < 0x20) {
    output = plan->steps + iVar2;
    plan->count = iVar2 + 1;
    strncpy(output->description,description,0xfff);
    output->tool_type = tool_type;
    if (argument0 != (char *)0x0) {
      strncpy(output->argument0,argument0,0x3ff);
    }
    if (argument1 != (char *)0x0) {
      strncpy(output->argument1,argument1,0x3ff);
    }
    *(undefined4 *)(0x1804 + (int)output) = 0;
    iVar1 = 0;
  }
  return iVar1;
}

/*
 * _plan_is_present @ 0x00024f2c..0x00024fa4
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage internal plan-step state: is present. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): int _plan_is_present(char *text)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00024f2c_plan_is_present.s
 */
int plan_is_present(char *text)

{
  char *pcVar1;
  undefined **ppuVar2;
  
  if ((text != (char *)0x0) && (_markers_3047 != NULL)) {
    ppuVar2 = (undefined **)(&APP_PTR_000391bc);
    pcVar1 = _markers_3047;
    do {
      pcVar1 = strcasestr(text,pcVar1);
      if (pcVar1 != (char *)0x0) {
        return 1;
      }
      pcVar1 = (char *)(*ppuVar2);
      ppuVar2 = ppuVar2 + 1;
    } while (pcVar1 != (char *)0x0);
  }
  return 0;
}

/*
 * _plan_parse @ 0x00024fa4..0x0002543c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Parse a model/user textual checklist into internal plan steps and statuses.
 * Parameters/return (inferred): int _plan_parse(RecoveredPlan *plan,char *text)
 * Direct callers: _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00024fa4_plan_parse.s
 */
int plan_parse(RecoveredPlan *plan,char *text)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x1840] __attribute__((aligned(16)));
#define acStack_1829 (*((char (*)[1025])(app_frame + 0x7))) /* original SP - 0x1829 */
#define acStack_1428 (*((char (*)[1024])(app_frame + 0x408))) /* original SP - 0x1428 */
#define acStack_1028 (*((char (*)[4112])(app_frame + 0x808))) /* original SP - 0x1028 */

  char cVar1;
  char *pcVar2;
  uint uVar3;
  char *pcVar4;
  int iVar5;

  if ((text == (char *)0x0) || (*text == '\0')) {
    iVar5 = 0;
  }
  else {
    plan_reset(plan);
    pcVar2 = strchr(text,10);
    if (pcVar2 == (char *)0x0) {
      uVar3 = strlen(text);
      pcVar2 = text + uVar3;
    }
    uVar3 = (int)pcVar2 - (int)text;
    if (0xff < uVar3) {
      uVar3 = 0xff;
    }
    memcpy(plan,text,uVar3);
    plan->title[uVar3] = '\0';
    if (*text != '\0') {
      while( true ) {
        for (; (cVar1 = *text, cVar1 == ' ' || (cVar1 == '\t')); text = text + 1) {
        }
        if (cVar1 == '\0') break;
        if (cVar1 == '-') {
          if (((text[1] != ' ') || (text[2] != '[')) ||
             (((text[3] != ' ' && (text[3] != 'x')) || (text[4] != ']')))) {
LAB_000250e4:
            if ((cVar1 != '[') || (((text[1] != ' ' && (text[1] != 'x')) || (text[2] != ']'))))
            goto LAB_000253e4;
          }
LAB_00025110:
          if ((cVar1 == '-') || (pcVar2 = text, cVar1 == '*')) {
            pcVar2 = text + 1;
          }
          for (; (cVar1 = *pcVar2, cVar1 == ' ' || (cVar1 == '\t')); pcVar2 = pcVar2 + 1) {
          }
          if (cVar1 == '[') {
            for (pcVar2 = pcVar2 + 3; (*pcVar2 == ' ' || (*pcVar2 == '\t')); pcVar2 = pcVar2 + 1) {
            }
          }
          pcVar4 = strchr(pcVar2,10);
          if (pcVar4 == (char *)0x0) {
            uVar3 = strlen(pcVar2);
            pcVar4 = pcVar2 + uVar3;
          }
          uVar3 = (int)pcVar4 - (int)pcVar2;
          if (0xfff < uVar3) {
            uVar3 = 0xfff;
          }
          memcpy(acStack_1028,pcVar2,uVar3);
          acStack_1028[uVar3] = '\0';
          memcpy(acStack_1829 + 1,&_C_49_3154,0x400);
          memcpy(acStack_1428,&_C_50_3155,0x400);
          pcVar2 = strcasestr(acStack_1028,"read file");
          if ((pcVar2 == (char *)0x0) &&
             (pcVar2 = strcasestr(acStack_1028,"read "), pcVar2 == (char *)0x0)) {
            pcVar2 = strcasestr(acStack_1028,"write file");
            iVar5 = 1;
            if (pcVar2 == (char *)0x0) {
              pcVar2 = strcasestr(acStack_1028,"edit file");
              iVar5 = 2;
              if (pcVar2 == (char *)0x0) {
                pcVar2 = strcasestr(acStack_1028,"shell");
                if (((pcVar2 == (char *)0x0) &&
                    (pcVar2 = strcasestr(acStack_1028,"run "), pcVar2 == (char *)0x0)) &&
                   (pcVar2 = strcasestr(acStack_1028,"execute "), pcVar2 == (char *)0x0)) {
                  pcVar2 = strcasestr(acStack_1028,"glob");
                  if ((pcVar2 == (char *)0x0) &&
                     (pcVar2 = strcasestr(acStack_1028,"find "), pcVar2 == (char *)0x0)) {
                    pcVar2 = strcasestr(acStack_1028,"grep");
                    iVar5 = 5;
                    if (pcVar2 == (char *)0x0) {
                      pcVar2 = strcasestr(acStack_1028,"web search");
                      if (((pcVar2 == (char *)0x0) &&
                          (pcVar2 = strcasestr(acStack_1028,"duckduckgo"), pcVar2 == (char *)0x0))
                         && (pcVar2 = strcasestr(acStack_1028,"search the web"),
                            pcVar2 == (char *)0x0)) goto LAB_000253b4;
                      iVar5 = 6;
                    }
                  }
                  else {
                    iVar5 = 4;
                  }
                }
                else {
                  iVar5 = 3;
                }
              }
            }
          }
          else {
            pcVar2 = strchr(acStack_1028,0x27);
            if ((pcVar2 != (char *)0x0) ||
               (pcVar2 = strchr(acStack_1028,0x22), pcVar2 != (char *)0x0)) {
              pcVar4 = strchr(pcVar2 + 1,(int)*pcVar2);
              if ((pcVar4 != (char *)0x0) &&
                 (pcVar4 = pcVar4 + (-1 - (int)pcVar2), pcVar4 < (char *)0x400)) {
                memcpy(acStack_1829 + 1,pcVar2 + 1,(uint)pcVar4);
                (acStack_1829 + 1)[(int)pcVar4] = '\0';
                iVar5 = 0;
                goto LAB_000253d0;
              }
            }
LAB_000253b4:
            iVar5 = 0;
          }
LAB_000253d0:
          plan_add_step(plan,acStack_1028,iVar5,acStack_1829 + 1,acStack_1428);
        }
        else {
          if (cVar1 != '*') goto LAB_000250e4;
          if ((((text[1] == ' ') && (text[2] == '[')) && ((text[3] == ' ' || (text[3] == 'x')))) &&
             (text[4] == ']')) goto LAB_00025110;
        }
LAB_000253e4:
        pcVar2 = strchr(text,10);
        if ((pcVar2 == (char *)0x0) || (text = pcVar2 + 1, pcVar2[1] == '\0')) break;
      }
    }
    iVar5 = 1;
    if (plan->count < 1) {
      iVar5 = 0;
    }
  }
  return iVar5;

#undef acStack_1829
#undef acStack_1428
#undef acStack_1028
}

/*
 * _plan_format @ 0x0002543c..0x00025630
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage internal plan-step state: format. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): char * _plan_format(RecoveredPlan *plan,char *output,uint capacity)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002543c_plan_format.s
 */
char * plan_format(RecoveredPlan *plan,char *output,uint capacity)

{
  int iVar1;
  uint uVar2;
  uint uVar3;
  char *pcVar4;
  int iVar5;
  RecoveredPlan *pRVar6;
  
  if (plan == (RecoveredPlan *)0x0) {
    if (capacity != 0) {
      *output = '\0';
    }
  }
  else if (capacity != 0) {
    uVar2 = snprintf(output,capacity,"## %s\n\n",(char *)(plan));
    if (((int)uVar2 < 0) || (capacity <= uVar2)) {
      output[capacity - 1] = '\0';
    }
    else if ((0 < plan->count) && (uVar2 + 0x40 < capacity)) {
      iVar5 = 0;
      pRVar6 = plan;
      do {
        iVar1 = pRVar6->steps[0].status;
        if (iVar1 == 3) {
          pcVar4 = "!";
        }
        else if (iVar1 == 4) {
          pcVar4 = "-";
        }
        else if (iVar1 == 2) {
          pcVar4 = "x";
        }
        else {
          pcVar4 = " ";
        }
        uVar3 = snprintf(output + uVar2,capacity - uVar2,"- [%s] %s\n",pcVar4,
                                   (char *)(plan->steps + iVar5));
        if (((int)uVar3 < 0) || (capacity - uVar2 <= uVar3)) {
          output[capacity - 1] = '\0';
          return output;
        }
        uVar2 = uVar2 + uVar3;
        if ((pRVar6->steps[0].result[0] != '\0') && (pRVar6->steps[0].status == 2)) {
          uVar3 = snprintf(output + uVar2,capacity - uVar2,"      %s\n",
                                     (char *)(0x1908 + (int)(plan->title + iVar5 * 0x2808)));
          if (((int)uVar3 < 0) || (capacity - uVar2 <= uVar3)) {
            output[capacity - 1] = '\0';
            return output;
          }
          uVar2 = uVar2 + uVar3;
        }
        iVar5 = iVar5 + 1;
        if (plan->count <= iVar5) {
          return output;
        }
        pRVar6 = (RecoveredPlan *)(pRVar6->steps[0].result + 0xf00);
      } while (uVar2 + 0x40 < capacity);
      return output;
    }
  }
  return output;
}

/*
 * _plan_set_status @ 0x00025630..0x0002568c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage internal plan-step state: set status. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _plan_set_status(RecoveredPlan *plan,int index,int status,char *result)
 * Direct callers: _executor_run_step@0x00025704
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00025630_plan_set_status.s
 */
void plan_set_status(RecoveredPlan *plan,int index,int status,char *result)

{
  if ((-1 < index) && (index < plan->count)) {
    *(int *)(0x1904 + (int)(plan->title + index * 0x2808)) = status;
    if (result != (char *)0x0) {
      strncpy((char *)(0x1908 + (int)(plan->title + index * 0x2808)),result,0xfff);
    }
  }
  return;
}

/*
 * _plan_next_pending @ 0x0002568c..0x000256e8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage internal plan-step state: next pending. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): int _plan_next_pending(RecoveredPlan *plan)
 * Direct callers: _executor_run_step@0x00025704, _executor_run_all@0x00025970
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002568c_plan_next_pending.s
 */
int plan_next_pending(RecoveredPlan *plan)

{
  int iVar1;
  int *piVar2;
  int iVar3;
  int iVar4;
  
  iVar4 = plan->count;
  if (0 < iVar4) {
    if (plan->steps[0].status == 0) {
      return 0;
    }
    piVar2 = &plan->steps[1].status;
    iVar3 = 0;
    while( true ) {
      iVar3 = iVar3 + 1;
      iVar4 = iVar4 + -1;
      if (iVar4 == 0) break;
      iVar1 = *piVar2;
      piVar2 = piVar2 + 0xa02;
      if (iVar1 == 0) {
        return iVar3;
      }
    }
  }
  return -1;
}

/*
 * _executor_init @ 0x000256e8..0x00025704
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in plans_executor: executor init. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _executor_init(RecoveredExecutor *executor,RecoveredConfig *config, RecoveredConversation *conversation,RecoveredPlan *plan)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000256e8_executor_init.s
 */
void executor_init(RecoveredExecutor *executor,RecoveredConfig *config,
                   RecoveredConversation *conversation,RecoveredPlan *plan)

{
  executor->config = config;
  executor->conversation = conversation;
  executor->plan = plan;
  executor->state = 0;
  executor->step_index = 0;
  return;
}

/*
 * _executor_run_step @ 0x00025704..0x00025970
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Execute a selected plan step through the configured execution state.
 * Parameters/return (inferred): int _executor_run_step(RecoveredExecutor *executor)
 * Direct callers: _executor_run_all@0x00025970
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; performs network/protocol I/O; resolves/executes a program through PATH; executes an argument vector through PATH; reads text from a stream; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; writes text to a stream; reads bytes from a stream; writes bytes to a stream; reads the process working directory; reads process environment; signals a process; creates directories; securely creates a temporary file; opens a directory for enumeration; executes a shell command and opens a pipe; reads a descriptor or IPC channel; reads directory entries; resolves a filesystem path; renames/replaces filesystem entries; executes SQL text; opens/creates a SQLite database; prepares SQL text; reads filesystem metadata; removes a filesystem entry; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00025704_executor_run_step.s
 */
int executor_run_step(RecoveredExecutor *executor)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x560] __attribute__((aligned(16)));
#define local_548 (*((undefined1 (*)[1228])(app_frame + 0x8))) /* original SP - 0x548 */
#define acStack_7c (*((char (*)[68])(app_frame + 0x4d4))) /* original SP - 0x7c */
#define aRStack_38 (*((RecoveredDString (*)[2])(app_frame + 0x518))) /* original SP - 0x38 */

  RecoveredPlan *pRVar1;
  int iVar2;
  RecoveredAppState *app_state;
  char *left;
  int iVar3;
  int status;
  RecoveredMessage *output;
  RecoveredDString *str;
  RecoveredConversation *pRVar4;

  if (executor->state == 6) {
    return 0;
  }
  pRVar1 = executor->plan;
  if ((pRVar1 == (RecoveredPlan *)0x0) || (pRVar1->count == 0)) {
    return -1;
  }
  iVar2 = plan_next_pending(pRVar1);
  if (iVar2 < 0) {
    return 0;
  }
  executor->step_index = iVar2;
  executor->state = 3;
  pRVar1 = executor->plan;
  plan_set_status(pRVar1,iVar2,1,(char *)0x0);
  memset(local_548,0,0x51c);
  APP_FIELD32(local_548, 0) = pRVar1->steps[iVar2].tool_type;
  ds_init(aRStack_38);
  if (pRVar1->steps[iVar2].argument0[0] != '\0') {
    ds_init((RecoveredDString *)(local_548 + 0x24));
    ds_append((RecoveredDString *)(local_548 + 0x24),pRVar1->steps[iVar2].argument0);
  }
  if (pRVar1->steps[iVar2].argument1[0] != '\0') {
    ds_init((RecoveredDString *)(local_548 + 0x30));
    ds_append((RecoveredDString *)(local_548 + 0x30),pRVar1->steps[iVar2].argument1);
  }
  snprintf(acStack_7c,0x40,"plan_%d_%d",executor->plan->id,iVar2);
  app_state = malloc(0x8d08c4);
  if (app_state == (RecoveredAppState *)0x0) {
    ds_append(aRStack_38,"Error: out of memory");
  }
  else {
    memset(app_state,0,0x8d08c4);
    memcpy(&app_state->config,executor->config,0x3668);
    tool_execute((RecoveredToolCall *)local_548,app_state);
    free(app_state);
  }
  left = ds_cstr(aRStack_38);
  pRVar1 = executor->plan;
  if (*left != '\0') {
    iVar3 = strncmp(left,"Error:",6);
    status = 3;
    if (iVar3 == 0) goto LAB_000258a8;
  }
  status = 2;
LAB_000258a8:
  plan_set_status(pRVar1,iVar2,status,left);
  pRVar4 = executor->conversation;
  if ((pRVar4 != (RecoveredConversation *)0x0) && (iVar2 = pRVar4->message_count, iVar2 < 0x17f)) {
    output = pRVar4->messages + iVar2;
    pRVar4->message_count = iVar2 + 1;
    memset(output,0,0x5c);
    pRVar4->messages[iVar2].role = 3;
    ds_init(&output->content);
    ds_append(&output->content,left);
    app_copy_padded(output->call_id,acStack_7c,0x3f);
  }
  ds_free(aRStack_38);
  str = (RecoveredDString *)(local_548 + 0x24);
  do {
    ds_free(str);
    str = str + 1;
  } while (str != (RecoveredDString *)(local_548 + 0x4c8));
  return 1;

#undef local_548
#undef acStack_7c
#undef aRStack_38
}

/*
 * _executor_run_all @ 0x00025970..0x00025a74
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in plans_executor: executor run all. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): undefined4 _executor_run_all(RecoveredExecutor *param_1,code *param_2,undefined4 param_3)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; performs network/protocol I/O; resolves/executes a program through PATH; executes an argument vector through PATH; reads text from a stream; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; writes text to a stream; reads bytes from a stream; writes bytes to a stream; reads the process working directory; reads process environment; signals a process; creates directories; securely creates a temporary file; opens a directory for enumeration; executes a shell command and opens a pipe; reads a descriptor or IPC channel; reads directory entries; resolves a filesystem path; renames/replaces filesystem entries; executes SQL text; opens/creates a SQLite database; prepares SQL text; reads filesystem metadata; removes a filesystem entry; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00025970_executor_run_all.s
 * Uncertainty: Indirect dispatch sites are retained with raw assembly and references; possible target sets are not assumed complete.
 */
undefined4 executor_run_all(RecoveredExecutor *param_1,code *param_2,undefined4 param_3)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x240] __attribute__((aligned(16)));
#define acStack_228 (*((char (*)[528])(app_frame + 0x8))) /* original SP - 0x228 */

  int iVar1;
  byte bVar2;

  param_1->state = 3;
  bVar2 = (param_2 == (code *)0x0) << 1;
  while( true ) {
    if (param_1->state == 6) {
      return 0xffffffff;
    }
    iVar1 = plan_next_pending(param_1->plan);
    if (iVar1 < 0) break;
    if (!(bool)(bVar2 >> 1 & 1)) {
      snprintf(acStack_228,0x200,"Running step %d/%d: %s",iVar1 + 1,param_1->plan->count,
                         (char *)(param_1->plan->steps + iVar1));
      (*param_2)(param_1,iVar1,acStack_228,param_3);
    }
    iVar1 = executor_run_step(param_1);
    if (iVar1 < 0) {
      param_1->state = 5;
      return 0xffffffff;
    }
    if (param_1->state == 6) {
      return 0xffffffff;
    }
  }
  param_1->state = 4;
  return 0;

#undef acStack_228
}

/*
 * _executor_cancel @ 0x00025a74..0x00025a80
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in plans_executor: executor cancel. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _executor_cancel(RecoveredExecutor *executor)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00025a74_executor_cancel.s
 */
void executor_cancel(RecoveredExecutor *executor)

{
  executor->state = 6;
  return;
}

