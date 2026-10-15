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
 * _write_in_json @ 0x00013000..0x00013140
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Write delegated prompt, effective API key, provider, model, URL and directory as temporary JSON.
 * Parameters/return (inferred): void _write_in_json(char *param_1,char *param_2,char *param_3,char *param_4,char *param_5, char *param_6,uint param_7)
 * Direct callers: _subagent_start@0x000139bc
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; opens/creates/truncates filesystem paths according to the supplied mode; writes text to a stream
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/00013000_write_in_json.s
 * Preserved behavior/security: TEMP-01: selected API credential is written into shared temporary JSON.
 */
void write_in_json(char *param_1,char *param_2,char *param_3,char *param_4,char *param_5,
                   char *param_6,uint param_7)

{
  RecoveredCJSON *object;
  RecoveredCJSON *object_00;
  char *text;
  void *stream;
  
  object = cJSON_CreateObject();
  cJSON_AddStringToObject(object,"prompt",param_2);
  object_00 = cJSON_CreateObject();
  cJSON_AddStringToObject(object_00,"model",param_3);
  cJSON_AddStringToObject(object_00,"working_dir",param_4);
  cJSON_AddStringToObject(object_00,"api_key",param_5);
  cJSON_AddStringToObject(object_00,"base_url",param_6);
  cJSON_AddNumberToObject
            (object_00,"provider",
             app_bits_double(CONCAT44(0x43300000,param_7 ^ 0x80000000)) - 4503601774854144.0);
  cJSON_AddItemToObject(object,"config",object_00);
  text = cJSON_PrintUnformatted(object);
  stream = fopen(param_1,"w");
  if (stream != (void *)0x0) {
    fputs(text,stream);
    fclose(stream);
  }
  free(text);
  cJSON_Delete(object);
  return;
}

/*
 * _read_result_json @ 0x00013140..0x0001327c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Read bounded-size delegated-result JSON and copy its result text.
 * Parameters/return (inferred): undefined4 _read_result_json(char *param_1,char *param_2,uint param_3)
 * Direct callers: _subagent_result@0x00013ba0
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; opens/creates/truncates filesystem paths according to the supplied mode; reads bytes from a stream
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00013140_read_result_json.s
 */
undefined4 read_result_json(char *param_1,char *param_2,uint param_3)

{
  undefined4 uVar1;
  void *stream;
  uint uVar2;
  RecoveredCJSON *object;
  RecoveredCJSON *pRVar3;
  uint capacity;
  
  stream = fopen(param_1,"r");
  if (stream == (void *)0x0) {
    snprintf(param_2,param_3,"Error: cannot read subagent result");
    uVar1 = 0xffffffff;
  }
  else {
    fseek(stream,0,2);
    uVar2 = ftell(stream);
    fseek(stream,0,0);
    if (((int)uVar2 < 1) || ((int)(param_3 - 1) < (int)uVar2)) {
      uVar2 = param_3 - 1;
    }
    capacity = param_3 - 1;
    uVar2 = fread(param_2,1,uVar2,stream);
    if (capacity < uVar2) {
      uVar2 = capacity;
    }
    param_2[uVar2] = '\0';
    fclose(stream);
    object = cJSON_Parse(param_2);
    uVar1 = 0;
    if (object != (RecoveredCJSON *)0x0) {
      pRVar3 = cJSON_GetObjectItem(object,"result");
      if ((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) {
        strncpy(param_2,pRVar3->value_string,capacity);
        param_2[param_3 - 1] = '\0';
      }
      cJSON_Delete(object);
      uVar1 = 0;
    }
  }
  return uVar1;
}

/*
 * _sa_child_main @ 0x0001327c..0x000139bc
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Run up to five delegated provider/tool turns with automatic approval forced on, then write result JSON.
 * Parameters/return (inferred): void _sa_child_main(char *input_path,char *output_path)
 * Direct callers: _subagent_start@0x000139bc
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; performs network/protocol I/O; resolves/executes a program through PATH; executes an argument vector through PATH; reads text from a stream; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; writes text to a stream; reads bytes from a stream; writes bytes to a stream; reads the process working directory; reads process environment; signals a process; creates directories; securely creates a temporary file; opens a directory for enumeration; executes a shell command and opens a pipe; reads a descriptor or IPC channel; reads directory entries; resolves a filesystem path; renames/replaces filesystem entries; executes SQL text; opens/creates a SQLite database; prepares SQL text; reads filesystem metadata; removes a filesystem entry; writes a descriptor
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/0001327c_sa_child_main.s
 * Preserved behavior/security: APPROVAL-01: forces automatic approval and dispatches child tools without the parent approval broker.
 */
void sa_child_main(char *input_path,char *output_path)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x2bb10] __attribute__((aligned(16)));
#define RStack_2baf8 (*((RecoveredDString *)(app_frame + 0x8))) /* original SP - 0x2baf8 */
#define RStack_2baec (*((RecoveredConfig *)(app_frame + 0x14))) /* original SP - 0x2baec */
#define RStack_28484 (*((RecoveredConversation *)(app_frame + 0x367c))) /* original SP - 0x28484 */
#define RStack_1fa7c (*((RecoveredDString *)(app_frame + 0xc084))) /* original SP - 0x1fa7c */
#define RStack_1fa70 (*((RecoveredResponse *)(app_frame + 0xc090))) /* original SP - 0x1fa70 */

  bool bVar1;
  void *pvVar2;
  uint uVar3;
  char *pcVar4;
  RecoveredCJSON *object;
  RecoveredCJSON *pRVar5;
  RecoveredCJSON *pRVar6;
  RecoveredCJSON *pRVar7;
  RecoveredAppState *app_state;
  char *pcVar8;
  int iVar9;
  int iVar10;
  RecoveredToolCall *tool_call;
  RecoveredToolCall *pRVar11;
  RecoveredDString *pRVar12;
  RecoveredMessage *pRVar13;
  int iVar14;
  RecoveredConversation *message;

  pvVar2 = fopen(input_path,"r");
  if (pvVar2 == (void *)0x0) {
                    /* WARNING: Subroutine does not return */
    _exit(1);
  }
  fseek(pvVar2,0,2);
  uVar3 = ftell(pvVar2);
  fseek(pvVar2,0,0);
  if (0x1ffffff < uVar3 - 1) {
    fclose(pvVar2);
                    /* WARNING: Subroutine does not return */
    _exit(1);
  }
  pcVar4 = malloc(uVar3 + 1);
  if (pcVar4 == (char *)0x0) {
    fclose(pvVar2);
                    /* WARNING: Subroutine does not return */
    _exit(1);
  }
  uVar3 = fread(pcVar4,1,uVar3,pvVar2);
  pcVar4[uVar3] = '\0';
  fclose(pvVar2);
  object = cJSON_Parse(pcVar4);
  free(pcVar4);
  if (object == (RecoveredCJSON *)0x0) {
                    /* WARNING: Subroutine does not return */
    _exit(1);
  }
  pRVar5 = cJSON_GetObjectItem(object,"prompt");
  pRVar6 = cJSON_GetObjectItem(object,"config");
  if ((pRVar5 == (RecoveredCJSON *)0x0) || (pRVar6 == (RecoveredCJSON *)0x0)) {
    cJSON_Delete(object);
                    /* WARNING: Subroutine does not return */
    _exit(1);
  }
  ds_init(&RStack_2baf8);
  ds_appendf(&RStack_2baf8,
              "You are a focused sub-agent. Complete the assigned task and return a concise result. Use tools as needed. Do not ask follow-up questions."
             );
  config_init(&RStack_2baec);
  pRVar7 = cJSON_GetObjectItem(pRVar6,"provider");
  if (pRVar7 != (RecoveredCJSON *)0x0) {
    RStack_2baec.provider = pRVar7->value_int;
  }
  pRVar7 = cJSON_GetObjectItem(pRVar6,"model");
  if (pRVar7 != (RecoveredCJSON *)0x0) {
    strncpy(RStack_2baec.model,pRVar7->value_string,0x7f);
  }
  pRVar7 = cJSON_GetObjectItem(pRVar6,"working_dir");
  if (pRVar7 != (RecoveredCJSON *)0x0) {
    strncpy(RStack_2baec.working_directory,pRVar7->value_string,0x3ff);
  }
  pRVar7 = cJSON_GetObjectItem(pRVar6,"api_key");
  if (pRVar7 != (RecoveredCJSON *)0x0) {
    strncpy(RStack_2baec.api_key,pRVar7->value_string,0xff);
  }
  pRVar6 = cJSON_GetObjectItem(pRVar6,"base_url");
  if (pRVar6 != (RecoveredCJSON *)0x0) {
    strncpy(RStack_2baec.base_url,pRVar6->value_string,0x1ff);
  }
  RStack_2baec.max_tokens = 0x1000;
  RStack_2baec.temperature = 0.3;
  RStack_2baec.auto_approve = 1;
  RStack_2baec.auto_compact = 0;
  memset(&RStack_28484,0,0x8a08);
  iVar9 = RStack_28484.message_count;
  pRVar13 = RStack_28484.messages + RStack_28484.message_count;
  RStack_28484.message_count = RStack_28484.message_count + 1;
  memset(pRVar13,0,0x5c);
  RStack_28484.messages[iVar9].role = 0;
  pRVar12 = &RStack_28484.messages[iVar9].content;
  ds_init(pRVar12);
  ds_copy(pRVar12,&RStack_2baf8);
  iVar9 = RStack_28484.message_count;
  pRVar13 = RStack_28484.messages + RStack_28484.message_count;
  RStack_28484.message_count = RStack_28484.message_count + 1;
  memset(pRVar13,0,0x5c);
  RStack_28484.messages[iVar9].role = 1;
  pRVar12 = &RStack_28484.messages[iVar9].content;
  ds_init(pRVar12);
  ds_append(pRVar12,pRVar5->value_string);
  ds_init(&RStack_1fa7c);
  iVar9 = 0;
  do {
    if (0x175 < RStack_28484.message_count) {
      context_auto_compact(&RStack_28484,&RStack_2baec);
    }
    if (0x17d < RStack_28484.message_count) break;
    memset(&RStack_1fa70,0,0x1fa0c);
    ds_init(&RStack_1fa70.content);
    pRVar12 = &RStack_1fa70.error_or_reasoning;
    ds_init(pRVar12);
    uVar3 = provider_send(&RStack_2baec,&RStack_28484,&RStack_1fa70,(RecoveredStreamCallback)0x0,
                           (void *)0x0,(RecoveredRoute *)0x0,(int *)0x0);
    iVar10 = RStack_28484.message_count;
    if (399 < uVar3) {
      ds_append(&RStack_1fa7c,"Subagent API error: ");
      pcVar4 = ds_cstr(pRVar12);
      ds_append(&RStack_1fa7c,pcVar4);
      ds_free(&RStack_1fa70.content);
      ds_free(pRVar12);
      break;
    }
    pRVar13 = RStack_28484.messages + RStack_28484.message_count;
    RStack_28484.message_count = RStack_28484.message_count + 1;
    memset(pRVar13,0,0x5c);
    RStack_28484.messages[iVar10].role = 2;
    pRVar12 = &RStack_28484.messages[iVar10].content;
    ds_init(pRVar12);
    ds_copy(pRVar12,&RStack_1fa70.content);
    if (RStack_1fa70.tool_call_count == 0) {
      ds_copy(&RStack_1fa7c,&RStack_1fa70.content);
      ds_free(&RStack_1fa70.content);
      ds_free(&RStack_1fa70.error_or_reasoning);
      break;
    }
    if (iVar9 == 0) {
      ds_append(&RStack_1fa7c,"(Subagent used tools)\n---\n");
    }
    if ((0 < RStack_1fa70.tool_call_count) && (RStack_28484.message_count < 0x17f)) {
      iVar10 = 0;
      tool_call = RStack_1fa70.tool_calls;
      pRVar11 = tool_call;
      do {
        app_state = malloc(0x8d08c4);
        if (app_state != (RecoveredAppState *)0x0) {
          memset(app_state,0,0x8d08c4);
          memcpy(&app_state->config,&RStack_2baec,0x3668);
          tool_execute(tool_call,app_state);
          free(app_state);
        }
        iVar14 = RStack_28484.message_count;
        pRVar13 = RStack_28484.messages + RStack_28484.message_count;
        RStack_28484.message_count = RStack_28484.message_count + 1;
        memset(pRVar13,0,0x5c);
        RStack_28484.messages[iVar14].role = 3;
        pRVar12 = &RStack_28484.messages[iVar14].content;
        ds_init(pRVar12);
        ds_copy(pRVar12,&tool_call->result);
        strncpy(RStack_28484.messages[iVar14].call_id,tool_call->call_id,0x3f);
        pcVar4 = tool_call->mcp_name;
        if (pRVar11->mcp_name[0] == '\0') {
          pcVar4 = tool_name(pRVar11->tool_type);
        }
        pcVar8 = ds_cstr(&tool_call->result);
        ds_appendf(&RStack_1fa7c,"Tool %s: %s\n",pcVar4,pcVar8);
        iVar10 = iVar10 + 1;
        if (RStack_1fa70.tool_call_count <= iVar10) break;
        tool_call = tool_call + 1;
        pRVar11 = pRVar11 + 1;
      } while (RStack_28484.message_count < 0x17f);
    }
    ds_free(&RStack_1fa70.content);
    ds_free(&RStack_1fa70.error_or_reasoning);
    if (0 < RStack_1fa70.tool_call_count) {
      iVar10 = 0;
      do {
        ds_free(&RStack_1fa70.tool_calls[iVar10].result);
        iVar14 = 0;
        do {
          ds_free((RecoveredDString *)
                   ((int)&RStack_1fa70.tool_calls[iVar10].arguments[0].data + iVar14));
          bVar1 = iVar14 != 0x498;
          iVar14 = iVar14 + 0xc;
        } while (bVar1);
        iVar10 = iVar10 + 1;
      } while (iVar10 < RStack_1fa70.tool_call_count);
    }
    bVar1 = iVar9 != 4;
    iVar9 = iVar9 + 1;
  } while (bVar1);
  if (RStack_1fa7c.length == 0) {
    ds_append(&RStack_1fa7c,"Subagent produced no output.");
  }
  pRVar5 = cJSON_CreateObject();
  pcVar4 = ds_cstr(&RStack_1fa7c);
  cJSON_AddStringToObject(pRVar5,"result",pcVar4);
  pcVar4 = cJSON_PrintUnformatted(pRVar5);
  pvVar2 = fopen(output_path,"w");
  if (pvVar2 != (void *)0x0) {
    fputs(pcVar4,pvVar2);
    fclose(pvVar2);
  }
  free(pcVar4);
  cJSON_Delete(pRVar5);
  ds_free(&RStack_1fa7c);
  ds_free(&RStack_2baf8);
  if (0 < RStack_28484.message_count) {
    iVar9 = 0;
    message = &RStack_28484;
    do {
      message_free(message->messages);
      iVar9 = iVar9 + 1;
      message = (RecoveredConversation *)(message->messages + 1);
    } while (iVar9 < RStack_28484.message_count);
  }
  ds_free(&RStack_2baec.system_prompt);
  cJSON_Delete(object);
                    /* WARNING: Subroutine does not return */
  _exit(0);

#undef RStack_2baf8
#undef RStack_2baec
#undef RStack_28484
#undef RStack_1fa7c
#undef RStack_1fa70
}

/*
 * _subagent_start @ 0x000139bc..0x00013aec
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Create counter-based input/output filenames, serialize delegated state and fork the child.
 * Parameters/return (inferred): int _subagent_start(RecoveredSubagent *agent,char *task,char *model,char *working_directory, char *key,char *url,int provider)
 * Direct callers: _tool_subagent@0x00015748
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; performs network/protocol I/O; resolves/executes a program through PATH; executes an argument vector through PATH; reads text from a stream; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; writes text to a stream; reads bytes from a stream; writes bytes to a stream; reads the process working directory; reads process environment; signals a process; creates directories; securely creates a temporary file; opens a directory for enumeration; executes a shell command and opens a pipe; reads a descriptor or IPC channel; reads directory entries; resolves a filesystem path; renames/replaces filesystem entries; executes SQL text; opens/creates a SQLite database; prepares SQL text; reads filesystem metadata; removes a filesystem entry; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000139bc_subagent_start.s
 */
int subagent_start(RecoveredSubagent *agent,char *task,char *model,char *working_directory,
                   char *key,char *url,int provider)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x450] __attribute__((aligned(16)));
#define acStack_438 (*((char (*)[1040])(app_frame + 0x8))) /* original SP - 0x438 */

  int iVar1;
  pid_t pVar2;

  memset(agent,0,0x4410);
  strncpy(agent->task,task,0x3fff);
  iVar1 = _subagent_idx;
  _subagent_idx = _subagent_idx + 1;
  snprintf(agent->output_path,0x400,"/tmp/ppccode_subagent_%d_out.json",iVar1);
  snprintf(acStack_438,0x400,"/tmp/ppccode_subagent_%d_in.json",iVar1);
  write_in_json(acStack_438,task,model,working_directory,key,url,provider);
  pVar2 = fork();
  iVar1 = -1;
  if (-1 < pVar2) {
    if (pVar2 == 0) {
      iVar1 = open("/dev/null",1);
      if (-1 < iVar1) {
        dup2(iVar1,2);
        close(iVar1);
      }
      sa_child_main(acStack_438,agent->output_path);
                    /* WARNING: Subroutine does not return */
      _exit(1);
    }
    agent->pid = pVar2;
    agent->running = 1;
    iVar1 = 0;
  }
  return iVar1;

#undef acStack_438
}

/*
 * _subagent_poll @ 0x00013aec..0x00013ba0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Poll the delegated child/result state without blocking for completion.
 * Parameters/return (inferred): int _subagent_poll(RecoveredSubagent *agent)
 * Direct callers: _tool_subagent@0x00015748
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00013aec_subagent_poll.s
 */
int subagent_poll(RecoveredSubagent *agent)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x40] __attribute__((aligned(16)));
#define local_28 (*((uint (*)[8])(app_frame + 0x8))) /* original SP - 0x28 */

  pid_t pVar1;

  if (agent->running == 0) {
    if (agent->finished != 0) {
      if (agent->succeeded != 0) {
        return 1;
      }
      return -1;
    }
  }
  else {
    pVar1 = waitpid(agent->pid,(int *)local_28,1);
    if (pVar1 != 0) {
      agent->running = 0;
      agent->finished = 1;
      if (((local_28[0] & 0x7f) == 0) && (local_28[0] >> 8 == 0)) {
        agent->succeeded = 1;
        return 1;
      }
      agent->succeeded = 0;
      return -1;
    }
  }
  return 0;

#undef local_28
}

/*
 * _subagent_result @ 0x00013ba0..0x00013c08
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Extract the delegated result into the caller's output buffer.
 * Parameters/return (inferred): int _subagent_result(RecoveredSubagent *agent,char *output,uint capacity)
 * Direct callers: _tool_subagent@0x00015748
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; opens/creates/truncates filesystem paths according to the supplied mode; reads bytes from a stream
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00013ba0_subagent_result.s
 */
int subagent_result(RecoveredSubagent *agent,char *output,uint capacity)

{
  int iVar1;
  
  if ((agent->finished == 0) || (agent->succeeded == 0)) {
    snprintf(output,capacity,"Subagent did not complete successfully.");
    iVar1 = -1;
  }
  else {
    iVar1 = read_result_json(agent->output_path,output,capacity);
  }
  return iVar1;
}

/*
 * _subagent_cleanup @ 0x00013c08..0x00013c7c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Kill/reap an active delegated child and unlink the output file; this routine does not remove its input file.
 * Parameters/return (inferred): void _subagent_cleanup(RecoveredSubagent *agent)
 * Direct callers: _tool_subagent@0x00015748
 * Possible side effects (direct-call closure, incomplete for indirect calls): signals a process; removes a filesystem entry
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00013c08_subagent_cleanup.s
 * Preserved behavior/security: TEMP-01: removes output but not credential-bearing input.
 */
void subagent_cleanup(RecoveredSubagent *agent)

{
  if ((agent->running != 0) && (0 < agent->pid)) {
    kill(agent->pid,9);
    waitpid(agent->pid,(int *)0x0,0);
  }
  agent->running = 0;
  agent->finished = 0;
  agent->pid = -1;
  unlink(agent->output_path);
  return;
}

