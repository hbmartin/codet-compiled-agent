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
 * _wire_log @ 0x0000b62c..0x0000b6b8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Append formatted Ollama request/response diagnostics to fixed /tmp/ollama_wire.log without a verbose gate.
 * Parameters/return (inferred): void _wire_log(const char *format, ...)
 * Direct callers: _ollama_sse_cb@0x0000c3a0, _ollama_send@0x0000c70c
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; opens/creates/truncates filesystem paths according to the supplied mode
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/0000b62c_wire_log.s
 * Preserved behavior/security: TEMP-01: unconditional full conversation logging to a fixed shared temporary path.
 */
/* Analyst reconstruction of _wire_log at 0x0000b62c. See index/manual-review.json. */
void wire_log(const char *format, ...)
{
    void *stream = fopen("/tmp/ollama_wire.log", "a");
    if (stream != 0) {
        va_list arguments;
        va_start(arguments, format);
        vfprintf(stream, format, arguments);
        va_end(arguments);
        fputc('\n', stream);
        fclose(stream);
    }
}

/*
 * _model_context_hardcoded @ 0x0000b6b8..0x0000ba2c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in provider_ollama: model context hardcoded. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _model_context_hardcoded(char *model)
 * Direct callers: _model_context@0x0000ba2c
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000b6b8_model_context_hardcoded.s
 */
int model_context_hardcoded(char *model)

{
  char *pcVar1;
  
  if (model == (char *)0x0) {
    return 0x20000;
  }
  pcVar1 = strstr(model,"deepseek-v3");
  if (((pcVar1 == (char *)0x0) && (pcVar1 = strstr(model,"deepseek-r1"), pcVar1 == (char *)0x0)) &&
     (pcVar1 = strstr(model,"deepseek"), pcVar1 == (char *)0x0)) {
    pcVar1 = strstr(model,"gemini");
    if (pcVar1 != (char *)0x0) {
      return 0x100000;
    }
    pcVar1 = strstr(model,"nemotron");
    if (((pcVar1 == (char *)0x0) && (pcVar1 = strstr(model,"minimax"), pcVar1 == (char *)0x0)) &&
       (pcVar1 = strstr(model,"glm"), pcVar1 == (char *)0x0)) {
      pcVar1 = strstr(model,"kimi");
      if (pcVar1 != (char *)0x0) {
        return 0x40000;
      }
      pcVar1 = strstr(model,"qwen3");
      if (((pcVar1 == (char *)0x0) && (pcVar1 = strstr(model,"qwen2.5"), pcVar1 == (char *)0x0)) &&
         ((pcVar1 = strstr(model,"qwen"), pcVar1 == (char *)0x0 &&
          ((((pcVar1 = strstr(model,"gemma"), pcVar1 == (char *)0x0 &&
             (pcVar1 = strstr(model,"llama3.3"), pcVar1 == (char *)0x0)) &&
            (pcVar1 = strstr(model,"llama3.1"), pcVar1 == (char *)0x0)) &&
           (pcVar1 = strstr(model,"devstral"), pcVar1 == (char *)0x0)))))) {
        pcVar1 = strstr(model,"rnj");
        if (pcVar1 != (char *)0x0) {
          return 0x20000;
        }
        pcVar1 = strstr(model,"mistral");
        if (pcVar1 != (char *)0x0) {
          return 0x20000;
        }
        pcVar1 = strstr(model,"codestral");
        if (pcVar1 != (char *)0x0) {
          return 0x20000;
        }
        pcVar1 = strstr(model,"phi");
        if (pcVar1 != (char *)0x0) {
          return 0x20000;
        }
        pcVar1 = strstr(model,"solar");
        if (pcVar1 != (char *)0x0) {
          return 0x20000;
        }
        pcVar1 = strstr(model,"stable");
        if (pcVar1 != (char *)0x0) {
          return 0x20000;
        }
        pcVar1 = strstr(model,"yi");
        if (pcVar1 != (char *)0x0) {
          return 0x20000;
        }
        pcVar1 = strstr(model,"orca");
        if (pcVar1 != (char *)0x0) {
          return 0x20000;
        }
        pcVar1 = strstr(model,"starling");
        if (pcVar1 != (char *)0x0) {
          return 0x20000;
        }
        pcVar1 = strstr(model,"dolphin");
        if (pcVar1 != (char *)0x0) {
          return 0x20000;
        }
        pcVar1 = strstr(model,"mixtral");
        if (pcVar1 != (char *)0x0) {
          return 0x20000;
        }
        pcVar1 = strstr(model,"openchat");
        if (pcVar1 != (char *)0x0) {
          return 0x20000;
        }
        pcVar1 = strstr(model,"neural");
        if (pcVar1 != (char *)0x0) {
          return 0x20000;
        }
        pcVar1 = strstr(model,"vicuna");
        if (pcVar1 != (char *)0x0) {
          return 0x20000;
        }
        pcVar1 = strstr(model,"llava");
        if (pcVar1 != (char *)0x0) {
          return 0x20000;
        }
        pcVar1 = strstr(model,"tiny");
        if (pcVar1 == (char *)0x0) {
          pcVar1 = strstr(model,"small");
          if ((pcVar1 == (char *)0x0) && (pcVar1 = strstr(model,"mini"), pcVar1 == (char *)0x0)) {
            return 0x20000;
          }
          return 0x8000;
        }
        return 0x2000;
      }
    }
  }
  return 0x1f400;
}

/*
 * _model_context @ 0x0000ba2c..0x0000bb9c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in provider_ollama: model context. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _model_context(char *model)
 * Direct callers: _config_context_window@0x0000722c, _do_async_fetch@0x000087a8, _ollama_sync_model_metadata@0x0000bb9c, _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000ba2c_model_context.s
 */
int model_context(char *model)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x240] __attribute__((aligned(16)));
#define acStack_228 (*((char (*)[255])(app_frame + 0x8))) /* original SP - 0x228 */
#define local_129 (*((undefined1 *)(app_frame + 0x107))) /* original SP - 0x129 */
#define acStack_128 (*((char (*)[255])(app_frame + 0x108))) /* original SP - 0x128 */
#define local_29 (*((undefined1 *)(app_frame + 0x207))) /* original SP - 0x29 */

  RecoveredModelCache *pRVar1;
  int iVar2;
  int iVar3;
  char *pcVar4;
  int *piVar5;
  int iVar6;

  pRVar1 = PTR__g_model_cache_000396ac;
  iVar2 = 0x8000;
  if (model != (char *)0x0) {
    iVar2 = PTR__g_model_cache_000396ac->count;
    if (0 < iVar2) {
      iVar6 = 0;
      piVar5 = &PTR__g_model_cache_000396ac->models[0].context_window;
      do {
        iVar3 = strcmp(pRVar1->models[iVar6].name,model);
        if ((iVar3 == 0) && (0 < *piVar5)) {
          return *piVar5;
        }
        iVar6 = iVar6 + 1;
        piVar5 = piVar5 + 0x12;
      } while (iVar2 != iVar6);
      strncpy(acStack_228,model,0xff);
      local_129 = 0;
      pcVar4 = strchr(acStack_228,0x3a);
      if (pcVar4 != (char *)0x0) {
        *pcVar4 = '\0';
      }
      pRVar1 = PTR__g_model_cache_000396ac;
      if (0 < PTR__g_model_cache_000396ac->count) {
        iVar2 = 0;
        piVar5 = &PTR__g_model_cache_000396ac->models[0].context_window;
        do {
          app_copy_padded(acStack_128,pRVar1->models[iVar2].name,0xff);
          local_29 = 0;
          pcVar4 = strchr(acStack_128,0x3a);
          if (pcVar4 != (char *)0x0) {
            *pcVar4 = '\0';
          }
          iVar6 = strcmp(acStack_228,acStack_128);
          if ((iVar6 == 0) && (0 < *piVar5)) {
            return *piVar5;
          }
          iVar2 = iVar2 + 1;
          piVar5 = piVar5 + 0x12;
        } while (iVar2 < pRVar1->count);
      }
    }
    iVar2 = model_context_hardcoded(model);
  }
  return iVar2;

#undef acStack_228
#undef local_129
#undef acStack_128
#undef local_29
}

/*
 * _ollama_sync_model_metadata @ 0x0000bb9c..0x0000bdf0
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in provider_ollama: ollama sync model metadata. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _ollama_sync_model_metadata(RecoveredConfig *config)
 * Direct callers: _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000bb9c_ollama_sync_model_metadata.s
 */
int ollama_sync_model_metadata(RecoveredConfig *config)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x650] __attribute__((aligned(16)));
#define acStack_638 (*((char (*)[1024])(app_frame + 0x8))) /* original SP - 0x638 */
#define local_238 (*((char *)(app_frame + 0x408))) /* original SP - 0x238 */
#define auStack_237 (*((undefined1 (*)[511])(app_frame + 0x409))) /* original SP - 0x237 */
#define local_38 (*((RecoveredHttpBuffer (*)[2])(app_frame + 0x608))) /* original SP - 0x38 */

  char *pcVar1;
  RecoveredCJSON *pRVar2;
  int iVar3;
  RecoveredCJSON *pRVar4;
  uint uVar5;
  char *pcVar6;

  pcVar1 = config_get_url(config);
  if ((pcVar1 == (char *)0x0) || (*pcVar1 == '\0')) {
    pcVar1 = "https://api.ollama.com";
  }
  snprintf(acStack_638,0x400,"%s/api/show",pcVar1);
  pRVar2 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar2,"name",config->model);
  pcVar1 = cJSON_PrintUnformatted(pRVar2);
  cJSON_Delete(pRVar2);
  pcVar6 = config->ollama_key;
  if (config->ollama_key[0] == '\0') {
    pcVar6 = config->api_key;
  }
  local_238 = '\0';
  memset(auStack_237,0,0x1ff);
  if ((pcVar6 != (char *)0x0) && (*pcVar6 != '\0')) {
    snprintf(&local_238,0x200,"Authorization: Bearer %s",pcVar6);
  }
  http_buf_init(local_38);
  pcVar6 = &local_238;
  if (local_238 == '\0') {
    pcVar6 = (char *)0x0;
  }
  iVar3 = http_post_json(acStack_638,pcVar1,pcVar6,local_38);
  free(pcVar1);
  if (((iVar3 == 200) && (local_38[0].data != (char *)0x0)) &&
     (pRVar2 = cJSON_Parse(local_38[0].data), pRVar2 != (RecoveredCJSON *)0x0)) {
    pRVar4 = cJSON_GetObjectItem(pRVar2,"model_info");
    if (pRVar4 != (RecoveredCJSON *)0x0) {
      for (pRVar4 = pRVar4->child; pRVar4 != (RecoveredCJSON *)0x0; pRVar4 = pRVar4->next) {
        pcVar1 = pRVar4->member_name;
        if (pcVar1 != (char *)0x0) {
          uVar5 = strlen(pcVar1);
          if ((uVar5 < 0xf) ||
             (iVar3 = strcmp(pcVar1 + (uVar5 - 0xf),".context_length"), iVar3 != 0)) {
            iVar3 = strcmp(pcVar1,"context_length");
            if (iVar3 != 0) goto LAB_0000bd58;
            iVar3 = pRVar4->value_int;
          }
          else {
            iVar3 = pRVar4->value_int;
          }
          if (0 < iVar3) goto LAB_0000bd98;
          goto LAB_0000bd68;
        }
LAB_0000bd58:
      }
    }
    iVar3 = 0;
LAB_0000bd68:
    pRVar4 = cJSON_GetObjectItem(pRVar2,"options");
    if ((pRVar4 != (RecoveredCJSON *)0x0) &&
       (pRVar4 = cJSON_GetObjectItem(pRVar4,"num_ctx"), pRVar4 != (RecoveredCJSON *)0x0)) {
      iVar3 = pRVar4->value_int;
    }
LAB_0000bd98:
    cJSON_Delete(pRVar2);
  }
  else {
    iVar3 = 0;
  }
  http_buf_free(local_38);
  if (config->context_window == 0) {
    if (iVar3 < 1) {
      iVar3 = model_context(config->model);
      config->context_window = iVar3;
    }
    else {
      config->context_window = iVar3;
    }
  }
  return 0;

#undef acStack_638
#undef local_238
#undef auStack_237
#undef local_38
}

/*
 * _ollama_build_request @ 0x0000bdf0..0x0000c3a0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Build Ollama chat JSON, including messages and available tool descriptions.
 * Parameters/return (inferred): RecoveredCJSON * _ollama_build_request (RecoveredConfig *config,RecoveredConversation *conversation,RecoveredRoute *route, int *failures)
 * Direct callers: _ollama_send@0x0000c70c
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/0000bdf0_ollama_build_request.s
 */
RecoveredCJSON *
ollama_build_request
          (RecoveredConfig *config,RecoveredConversation *conversation,RecoveredRoute *route,
          int *failures)

{
  int iVar1;
  RecoveredToolCall *pRVar2;
  RecoveredCJSON *object;
  RecoveredCJSON *pRVar3;
  RecoveredCJSON *pRVar4;
  char *pcVar5;
  RecoveredCJSON *array;
  RecoveredCJSON *object_00;
  RecoveredCJSON *object_01;
  RecoveredCJSON *object_02;
  uint uVar6;
  int iVar7;
  RecoveredToolCall **ppRVar8;
  RecoveredConversation *pRVar9;
  int iVar10;
  int *piStack00000024;
  
  piStack00000024 = failures;
  object = cJSON_CreateObject();
  cJSON_AddStringToObject(object,"model",config->model);
  cJSON_AddBoolToObject(object,"stream",1);
  pRVar3 = cJSON_CreateArray();
  if ((config->system_prompt).length != 0) {
    pRVar4 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar4,"role","system");
    pcVar5 = ds_cstr(&config->system_prompt);
    cJSON_AddStringToObject(pRVar4,"content",pcVar5);
    cJSON_AddItemToArray(pRVar3,pRVar4);
  }
  if (0 < conversation->message_count) {
    iVar7 = 0;
    pRVar9 = conversation;
    do {
      pRVar4 = cJSON_CreateObject();
      iVar1 = pRVar9->messages[0].role;
      if (iVar1 == 2) {
        pcVar5 = "assistant";
      }
      else if (iVar1 == 3) {
        pcVar5 = "tool";
      }
      else if (iVar1 == 0) {
        pcVar5 = "system";
      }
      else {
        pcVar5 = "user";
      }
      cJSON_AddStringToObject(pRVar4,"role",pcVar5);
      pcVar5 = ds_cstr(&conversation->messages[iVar7].content);
      cJSON_AddStringToObject(pRVar4,"content",pcVar5);
      if (0 < pRVar9->messages[0].tool_call_count) {
        array = cJSON_CreateArray();
        if (0 < pRVar9->messages[0].tool_call_count) {
          iVar1 = 0;
          iVar10 = 0;
          do {
            object_00 = cJSON_CreateObject();
            ppRVar8 = &pRVar9->messages[0].tool_calls;
            cJSON_AddStringToObject
                      (object_00,"id",(pRVar9->messages[0].tool_calls)->call_id + iVar10);
            cJSON_AddStringToObject(object_00,"type","function");
            object_01 = cJSON_CreateObject();
            pcVar5 = tool_name(*(int *)((pRVar9->messages[0].tool_calls)->mcp_name + iVar10 + -4));
            cJSON_AddStringToObject(object_01,"name",pcVar5);
            object_02 = cJSON_CreateObject();
            pRVar2 = pRVar9->messages[0].tool_calls;
            switch(*(undefined4 *)(pRVar2->mcp_name + iVar10 + -4)) {
            case 0:
              pcVar5 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar10));
              cJSON_AddStringToObject(object_02,"path",pcVar5);
              break;
            case 1:
              pcVar5 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar10));
              cJSON_AddStringToObject(object_02,"path",pcVar5);
              pcVar5 = ds_cstr((RecoveredDString *)((int)&(*ppRVar8)->arguments[1].data + iVar10));
              cJSON_AddStringToObject(object_02,"content",pcVar5);
              break;
            case 2:
              pcVar5 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar10));
              cJSON_AddStringToObject(object_02,"path",pcVar5);
              pcVar5 = ds_cstr((RecoveredDString *)((int)&(*ppRVar8)->arguments[1].data + iVar10));
              cJSON_AddStringToObject(object_02,"oldText",pcVar5);
              pcVar5 = ds_cstr((RecoveredDString *)((int)&(*ppRVar8)->arguments[2].data + iVar10));
              cJSON_AddStringToObject(object_02,"newText",pcVar5);
              break;
            case 3:
              pcVar5 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar10));
              cJSON_AddStringToObject(object_02,"command",pcVar5);
              break;
            case 4:
              pcVar5 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar10));
              cJSON_AddStringToObject(object_02,"pattern",pcVar5);
              break;
            case 5:
              pcVar5 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar10));
              cJSON_AddStringToObject(object_02,"pattern",pcVar5);
              break;
            case 6:
              pcVar5 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar10));
              cJSON_AddStringToObject(object_02,"query",pcVar5);
              break;
            case 0xb:
              pcVar5 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar10));
              cJSON_AddStringToObject(object_02,"path",pcVar5);
              pcVar5 = ds_cstr((RecoveredDString *)((int)&(*ppRVar8)->arguments[1].data + iVar10));
              cJSON_AddStringToObject(object_02,"oldText",pcVar5);
              pcVar5 = ds_cstr((RecoveredDString *)((int)&(*ppRVar8)->arguments[2].data + iVar10));
              cJSON_AddStringToObject(object_02,"newText",pcVar5);
              break;
            case 0xd:
              pcVar5 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar10));
              cJSON_AddStringToObject(object_02,"query",pcVar5);
            }
            cJSON_AddItemToObject(object_01,"arguments",object_02);
            cJSON_AddItemToObject(object_00,"function",object_01);
            cJSON_AddItemToArray(array,object_00);
            iVar1 = iVar1 + 1;
            iVar10 = iVar10 + 0x51c;
          } while (iVar1 < pRVar9->messages[0].tool_call_count);
        }
        cJSON_AddItemToObject(pRVar4,"tool_calls",array);
      }
      if ((pRVar9->messages[0].role == 3) && (pRVar9->messages[0].call_id[0] != '\0')) {
        cJSON_AddStringToObject(pRVar4,"tool_call_id",conversation->messages[iVar7].call_id);
      }
      cJSON_AddItemToArray(pRVar3,pRVar4);
      iVar7 = iVar7 + 1;
      pRVar9 = (RecoveredConversation *)(pRVar9->messages + 1);
    } while (iVar7 < conversation->message_count);
  }
  cJSON_AddItemToObject(object,"messages",pRVar3);
  pRVar3 = openai_build_tools_array(route,piStack00000024);
  cJSON_AddItemToObject(object,"tools",pRVar3);
  pRVar3 = cJSON_CreateObject();
  cJSON_AddNumberToObject
            (pRVar3,"num_predict",
             app_bits_double(CONCAT44(0x43300000,config->max_tokens ^ 0x80000000)) - 4503601774854144.0);
  uVar6 = config_context_window(config);
  cJSON_AddNumberToObject
            (pRVar3,"num_ctx",app_bits_double(CONCAT44(0x43300000,uVar6 ^ 0x80000000)) - 4503601774854144.0);
  cJSON_AddNumberToObject(pRVar3,"temperature",(double)config->temperature);
  cJSON_AddItemToObject(object,"options",pRVar3);
  return object;
}

/*
 * _ollama_sse_cb @ 0x0000c3a0..0x0000c70c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Decode provider streaming events, accumulate response/tool-call state and invoke callbacks.
 * Parameters/return (inferred): void _ollama_sse_cb(char *event_kind,char *data,RecoveredProviderCallbackContext *context)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; opens/creates/truncates filesystem paths according to the supplied mode
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000c3a0_ollama_sse_cb.s
 * Uncertainty: Indirect dispatch sites are retained with raw assembly and references; possible target sets are not assumed complete.
 */
void ollama_sse_cb(char *event_kind,char *data,RecoveredProviderCallbackContext *context)

{
  (void)event_kind; /* Present in the recovered ABI; unused by this routine. */
  RecoveredResponse *pRVar1;
  int iVar2;
  RecoveredCJSON *object;
  RecoveredCJSON *pRVar3;
  RecoveredCJSON *pRVar4;
  int iVar5;
  char *text;
  
  _ollama_sse_call_count = _ollama_sse_call_count + 1;
  wire_log("--- SSE chunk #%d ---");
  if (data != (char *)0x0) {
    wire_log("data: %s",data);
    iVar2 = strcmp(data,"[DONE]");
    if (iVar2 != 0) {
      object = cJSON_Parse(data);
      if (object == (RecoveredCJSON *)0x0) {
        return;
      }
      pRVar3 = cJSON_GetObjectItem(object,"error");
      if ((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) {
        ds_append(&context->response->error_or_reasoning,pRVar3->value_string);
        context->response->finish_reason = 0;
        context->response->completion_flag = 1;
        cJSON_Delete(object);
        return;
      }
      pRVar3 = cJSON_GetObjectItem(object,"message");
      if (pRVar3 != (RecoveredCJSON *)0x0) {
        pRVar4 = cJSON_GetObjectItem(pRVar3,"thinking");
        if ((((pRVar4 != (RecoveredCJSON *)0x0) &&
             (text = pRVar4->value_string, text != (char *)0x0)) && (*text != '\0')) &&
           (context->callback != (RecoveredStreamCallback)0x0)) {
          (*context->callback)("think",text,context->context);
        }
        pRVar4 = cJSON_GetObjectItem(pRVar3,"content");
        if ((pRVar4 != (RecoveredCJSON *)0x0) && (pRVar4->value_string != (char *)0x0)) {
          ds_append(&context->response->content,pRVar4->value_string);
          if (context->callback != (RecoveredStreamCallback)0x0) {
            (*context->callback)((char *)0x0,pRVar4->value_string,context->context);
          }
        }
        pRVar3 = cJSON_GetObjectItem(pRVar3,"tool_calls");
        if ((pRVar3 != (RecoveredCJSON *)0x0) && (iVar2 = cJSON_GetArraySize(pRVar3), 0 < iVar2)) {
          for (iVar2 = 0; iVar5 = cJSON_GetArraySize(pRVar3), iVar2 < iVar5; iVar2 = iVar2 + 1) {
            pRVar4 = cJSON_GetArrayItem(pRVar3,iVar2);
            pRVar4 = cJSON_GetObjectItem(pRVar4,"function");
            if ((pRVar4 != (RecoveredCJSON *)0x0) &&
               (pRVar4 = cJSON_GetObjectItem(pRVar4,"arguments"), pRVar4 != (RecoveredCJSON *)0x0))
            {
              provider_parse_tool_calls(pRVar3,context->response);
              if (0 < context->response->tool_call_count) {
                context->response->finish_reason = 1;
              }
              break;
            }
          }
        }
      }
      pRVar3 = cJSON_GetObjectItem(object,"done");
      if ((pRVar3 != (RecoveredCJSON *)0x0) &&
         ((pRVar3->type_flags == 2 || (pRVar3->value_int == 1)))) {
        context->response->completion_flag = 1;
        pRVar3 = cJSON_GetObjectItem(object,"prompt_eval_count");
        pRVar4 = cJSON_GetObjectItem(object,"eval_count");
        if (pRVar3 != (RecoveredCJSON *)0x0) {
          context->response->input_tokens = pRVar3->value_int;
        }
        if (pRVar4 != (RecoveredCJSON *)0x0) {
          context->response->output_tokens = pRVar4->value_int;
        }
        pRVar1 = context->response;
        pRVar1->total_tokens = pRVar1->input_tokens + pRVar1->output_tokens;
        if ((pRVar3 != (RecoveredCJSON *)0x0) || (iVar2 = 0, pRVar4 != (RecoveredCJSON *)0x0)) {
          iVar2 = 1;
        }
        context->response->usage_known = iVar2;
        pRVar3 = cJSON_GetObjectItem(object,"done_reason");
        if (((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) &&
           (iVar2 = strcmp(pRVar3->value_string,"length"), iVar2 == 0)) {
          context->response->finish_reason = 3;
        }
      }
      cJSON_Delete(object);
      return;
    }
  }
  context->response->completion_flag = 1;
  return;
}

/*
 * _ollama_send @ 0x0000c70c..0x0000ca04
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Build/log an Ollama request, attach configured authentication and stream through the shared retry transport.
 * Parameters/return (inferred): int _ollama_send(RecoveredConfig *config,RecoveredConversation *conversation, RecoveredResponse *response,RecoveredStreamCallback callback,void *context, RecoveredRoute *route,int *failures)
 * Direct callers: _provider_send@0x0000a39c
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O; opens/creates/truncates filesystem paths according to the supplied mode
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000c70c_ollama_send.s
 */
int ollama_send(RecoveredConfig *config,RecoveredConversation *conversation,
                RecoveredResponse *response,RecoveredStreamCallback callback,void *context,
                RecoveredRoute *route,int *failures)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0xe60] __attribute__((aligned(16)));
#define acStack_e48 (*((char (*)[1024])(app_frame + 0x8))) /* original SP - 0xe48 */
#define local_a48 (*((char *)(app_frame + 0x408))) /* original SP - 0xa48 */
#define auStack_a47 (*((undefined1 (*)[511])(app_frame + 0x409))) /* original SP - 0xa47 */
#define local_848 (*((RecoveredResponse * *)(app_frame + 0x608))) /* original SP - 0x848 */
#define local_844 (*((RecoveredStreamCallback *)(app_frame + 0x60c))) /* original SP - 0x844 */
#define local_840 (*((void * *)(app_frame + 0x610))) /* original SP - 0x840 */
#define local_83c (*((char *)(app_frame + 0x614))) /* original SP - 0x83c */
#define auStack_83b (*((undefined1 (*)[2047])(app_frame + 0x615))) /* original SP - 0x83b */
#define local_3c (*((int (*)[3])(app_frame + 0xe14))) /* original SP - 0x3c */

  char *pcVar1;
  RecoveredCJSON *item;
  uint uVar2;
  char *pcVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  RecoveredResponse *pRVar7;

  pcVar1 = config_get_url(config);
  snprintf(acStack_e48,0x400,"%s/api/chat",pcVar1);
  pcVar3 = pcVar1;
  if (pcVar1 == (char *)0x0) {
    pcVar3 = "(null)";
  }
  fprintf((FILE * restrict)(PTR_000396a4 + 0xb0),"DIAG: ollama_send ENTERED base=\'%s\' url=\'%s\'\n",pcVar3,
                    acStack_e48);
  if (pcVar1 == (char *)0x0) {
    pcVar1 = "(null)";
  }
  wire_log("DIAG: ollama_send ENTERED base=\'%s\' url=\'%s\'",pcVar1,acStack_e48);
  _ollama_sse_call_count = 0;
  item = ollama_build_request(config,conversation,route,failures);
  pcVar3 = cJSON_PrintUnformatted(item);
  cJSON_Delete(item);
  wire_log("--- REQUEST to %s ---",acStack_e48);
  wire_log("%s",pcVar3);
  pcVar1 = config_get_key(config);
  local_a48 = '\0';
  memset(auStack_a47,0,0x1ff);
  if ((pcVar1 != (char *)0x0) && (*pcVar1 != '\0')) {
    snprintf(&local_a48,0x200,"Authorization: Bearer %s",pcVar1);
  }
  local_83c = '\0';
  local_848 = response;
  local_844 = callback;
  local_840 = context;
  memset(auStack_83b,0,0x7ff);
  local_3c[0] = 0;
  pcVar1 = &local_a48;
  if (local_a48 == '\0') {
    pcVar1 = (char *)0x0;
  }
  uVar2 = http_post_with_retry2
                    (acStack_e48,pcVar3,pcVar1,(RecoveredStreamCallback)(ollama_sse_cb),&local_848,&local_83c,0x800,local_3c);
  free(pcVar3);
  if ((399 < uVar2) && (local_83c != '\0')) {
    ds_append(&response->error_or_reasoning,&local_83c);
  }
  wire_log("--- RESPONSE SUMMARY ---");
  wire_log("http_code: %d",uVar2);
  wire_log("tool_call_count: %d",response->tool_call_count);
  wire_log("stop_reason: %d",response->finish_reason);
  wire_log("prompt_tokens: %d",response->input_tokens);
  wire_log("completion_tokens: %d",response->output_tokens);
  wire_log("total_SSE_chunks: %d",_ollama_sse_call_count);
  if (0 < response->tool_call_count) {
    iVar4 = 0;
    pRVar7 = response;
    do {
      iVar6 = pRVar7->tool_calls[0].tool_type;
      iVar5 = pRVar7->tool_calls[0].argument_count;
      pcVar3 = ds_cstr(response->tool_calls[iVar4].arguments);
      pcVar1 = ds_cstr(response->tool_calls[iVar4].arguments + 1);
      wire_log("tool[%d]: name=%s type=%d arg_count=%d args0=\'%s\' args1=\'%s\'",iVar4,
                response->tool_calls[iVar4].mcp_name,iVar6,iVar5,pcVar3,pcVar1);
      iVar4 = iVar4 + 1;
      if (response->tool_call_count <= iVar4) {
        return uVar2;
      }
      pRVar7 = (RecoveredResponse *)(pRVar7->tool_calls[0].call_id + 0x34);
    } while (iVar4 != 99);
  }
  return uVar2;

#undef acStack_e48
#undef local_a48
#undef auStack_a47
#undef local_848
#undef local_844
#undef local_840
#undef local_83c
#undef auStack_83b
#undef local_3c
}

