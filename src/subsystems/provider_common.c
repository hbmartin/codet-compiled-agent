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
 * _provider_parse_fallback @ 0x000091c0..0x0000983c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Recover fallback textual tool descriptions from a provider response.
 * Parameters/return (inferred): int _provider_parse_fallback(char *text,RecoveredResponse *response)
 * Direct callers: _provider_parse_any@0x0000983c
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000091c0_provider_parse_fallback.s
 */
int provider_parse_fallback(char *text,RecoveredResponse *response)

{
  bool bVar1;
  char *pcVar2;
  char *pcVar3;
  uint capacity;
  RecoveredToolCall *output;
  RecoveredDString *str;
  int iVar4;
  
  if ((((text != (char *)0x0) && (*text != '\0')) && (iVar4 = response->tool_call_count, iVar4 < 99)
      ) && (((pcVar2 = strstr(text,"\"name\""), pcVar2 != (char *)0x0 ||
             (pcVar2 = strstr(text,"name="), pcVar2 != (char *)0x0)) &&
            (pcVar2 = strchr(pcVar2 + (*pcVar2 == '\"') + 5,0x22), pcVar2 != (char *)0x0)))) {
    pcVar2 = pcVar2 + 1;
    pcVar3 = strchr(pcVar2,0x22);
    if ((pcVar3 != (char *)0x0) && (capacity = (int)pcVar3 - (int)pcVar2, capacity - 1 < 0x1e)) {
      output = response->tool_calls + iVar4;
      memset(output,0,0x51c);
      ds_init(&output->result);
      iVar4 = 0;
      str = output->arguments;
      do {
        ds_init((RecoveredDString *)((int)&str->data + iVar4));
        bVar1 = iVar4 != 0x498;
        iVar4 = iVar4 + 0xc;
      } while (bVar1);
      pcVar3 = output->mcp_name;
      strncpy(pcVar3,pcVar2,capacity);
      *(undefined1 *)((int)output + capacity + 4) = 0;
      iVar4 = strcmp(pcVar3,"read_file");
      if (iVar4 == 0) {
        output->tool_type = 0;
      }
      else {
        iVar4 = strcmp(pcVar3,"write_file");
        if (iVar4 == 0) {
          output->tool_type = 1;
        }
        else {
          iVar4 = strcmp(pcVar3,"edit_file");
          if (iVar4 == 0) {
            output->tool_type = 2;
          }
          else {
            iVar4 = strcmp(pcVar3,"shell");
            if (iVar4 == 0) {
              output->tool_type = 3;
            }
            else {
              iVar4 = strcmp(pcVar3,"glob");
              if (iVar4 == 0) {
                output->tool_type = 4;
              }
              else {
                iVar4 = strcmp(pcVar3,"grep");
                if (iVar4 == 0) {
                  output->tool_type = 5;
                }
                else {
                  iVar4 = strcmp(pcVar3,"search");
                  if (iVar4 == 0) {
                    output->tool_type = 6;
                  }
                  else {
                    iVar4 = strcmp(pcVar3,"fetch_url");
                    if (iVar4 == 0) {
                      output->tool_type = 7;
                    }
                    else {
                      iVar4 = strcmp(pcVar3,"db_query");
                      if (iVar4 == 0) {
                        output->tool_type = 8;
                      }
                      else {
                        iVar4 = strcmp(pcVar3,"memory_remember");
                        if (iVar4 == 0) {
                          output->tool_type = 9;
                        }
                        else {
                          iVar4 = strcmp(pcVar3,"memory_load");
                          if (iVar4 == 0) {
                            output->tool_type = 10;
                          }
                          else {
                            iVar4 = strcmp(pcVar3,"read_and_edit");
                            if (iVar4 == 0) {
                              output->tool_type = 0xb;
                            }
                            else {
                              iVar4 = strcmp(pcVar3,"reference");
                              if (iVar4 == 0) {
                                output->tool_type = 0xd;
                              }
                              else {
                                iVar4 = strcmp(pcVar3,"subagent");
                                if (iVar4 == 0) {
                                  output->tool_type = 0xc;
                                }
                                else {
                                  iVar4 = strcmp(pcVar3,"tree");
                                  if (iVar4 == 0) {
                                    output->tool_type = 0xe;
                                  }
                                  else {
                                    iVar4 = strcmp(pcVar3,"read_project");
                                    if (iVar4 == 0) {
                                      output->tool_type = 0xf;
                                    }
                                    else {
                                      output->tool_type = 100;
                                      strncpy(pcVar3,pcVar2,capacity);
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
      pcVar2 = strstr(text,"\"path\"");
      if (((pcVar2 != (char *)0x0) || (pcVar2 = strstr(text,"path="), pcVar2 != (char *)0x0)) &&
         (pcVar2 = strchr(pcVar2 + (*pcVar2 == '\"') + 5,0x22), pcVar2 != (char *)0x0)) {
        pcVar2 = pcVar2 + 1;
        pcVar3 = strchr(pcVar2,0x22);
        if (pcVar3 != (char *)0x0) {
          if (((int)pcVar3 - (int)pcVar2) - 1U < 0x3ffff) {
            ds_appendn(str,pcVar2,(int)pcVar3 - (int)pcVar2);
          }
          output->argument_count = 1;
        }
      }
      pcVar2 = strstr(text,"\"content\"");
      if (((pcVar2 != (char *)0x0) || (pcVar2 = strstr(text,"content="), pcVar2 != (char *)0x0)) &&
         ((0 < output->argument_count &&
          (pcVar2 = strchr(pcVar2 + (*pcVar2 == '\"') + 8,0x22), pcVar2 != (char *)0x0)))) {
        pcVar2 = pcVar2 + 1;
        pcVar3 = strchr(pcVar2,0x22);
        if ((pcVar3 != (char *)0x0) && (((int)pcVar3 - (int)pcVar2) - 1U < 0x3ffff)) {
          ds_appendn(output->arguments + 1,pcVar2,(int)pcVar3 - (int)pcVar2);
        }
      }
      pcVar2 = strstr(text,"\"oldText\"");
      if (((pcVar2 != (char *)0x0) || (pcVar2 = strstr(text,"oldText="), pcVar2 != (char *)0x0)) &&
         (pcVar2 = strchr(pcVar2 + 8,0x22), pcVar2 != (char *)0x0)) {
        pcVar2 = pcVar2 + 1;
        pcVar3 = strchr(pcVar2,0x22);
        if ((pcVar3 != (char *)0x0) && (((int)pcVar3 - (int)pcVar2) - 1U < 0x3ffff)) {
          ds_appendn(output->arguments + 1,pcVar2,(int)pcVar3 - (int)pcVar2);
        }
      }
      pcVar2 = strstr(text,"\"newText\"");
      if (((pcVar2 != (char *)0x0) || (pcVar2 = strstr(text,"newText="), pcVar2 != (char *)0x0)) &&
         (pcVar2 = strchr(pcVar2 + 8,0x22), pcVar2 != (char *)0x0)) {
        pcVar2 = pcVar2 + 1;
        pcVar3 = strchr(pcVar2,0x22);
        if ((pcVar3 != (char *)0x0) && (((int)pcVar3 - (int)pcVar2) - 1U < 0x3ffff)) {
          ds_appendn(output->arguments + 2,pcVar2,(int)pcVar3 - (int)pcVar2);
        }
      }
      pcVar2 = strstr(text,"\"command\"");
      if (((pcVar2 != (char *)0x0) || (pcVar2 = strstr(text,"command="), pcVar2 != (char *)0x0)) &&
         (pcVar2 = strchr(pcVar2 + 8,0x22), pcVar2 != (char *)0x0)) {
        pcVar2 = pcVar2 + 1;
        pcVar3 = strchr(pcVar2,0x22);
        if ((pcVar3 != (char *)0x0) && (((int)pcVar3 - (int)pcVar2) - 1U < 0x3ffff)) {
          ds_appendn(str,pcVar2,(int)pcVar3 - (int)pcVar2);
        }
      }
      response->tool_call_count = response->tool_call_count + 1;
      snprintf(output->call_id,0x40,"tc_f%d",response->tool_call_count);
      return 1;
    }
  }
  return 0;
}

/*
 * _provider_parse_any @ 0x0000983c..0x000098bc
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in provider_common: provider parse any. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _provider_parse_any(char *text,RecoveredResponse *response)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000983c_provider_parse_any.s
 */
int provider_parse_any(char *text,RecoveredResponse *response)

{
  RecoveredCJSON *object;
  RecoveredCJSON *array;
  int iVar1;
  
  object = cJSON_Parse(text);
  if (object != (RecoveredCJSON *)0x0) {
    array = cJSON_GetObjectItem(object,"tool_calls");
    if (array != (RecoveredCJSON *)0x0) {
      provider_parse_tool_calls(array,response);
      cJSON_Delete(object);
      return 1;
    }
    cJSON_Delete(object);
  }
  iVar1 = provider_parse_fallback(text,response);
  return iVar1;
}

/*
 * _provider_parse_tool_calls_args @ 0x000098bc..0x00009f28
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Decode named JSON tool arguments into internal positional dynamic-string slots.
 * Parameters/return (inferred): void _provider_parse_tool_calls_args(RecoveredToolCall *call,RecoveredCJSON *arguments)
 * Direct callers: _provider_parse_tool_calls@0x00009f28, _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000098bc_provider_parse_tool_calls_args.s
 */
void provider_parse_tool_calls_args(RecoveredToolCall *call,RecoveredCJSON *arguments)

{
  int iVar1;
  RecoveredCJSON *pRVar2;
  RecoveredCJSON *pRVar3;
  RecoveredCJSON *pRVar4;
  
  if ((arguments != (RecoveredCJSON *)0x0) && (call != (RecoveredToolCall *)0x0)) {
    switch(call->tool_type) {
    case 0:
      pRVar3 = cJSON_GetObjectItem(arguments,"path");
      if ((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) {
        ds_append(call->arguments,pRVar3->value_string);
      }
      call->argument_count = 1;
      break;
    case 1:
      pRVar3 = cJSON_GetObjectItem(arguments,"path");
      pRVar4 = cJSON_GetObjectItem(arguments,"content");
      if ((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) {
        ds_append(call->arguments,pRVar3->value_string);
      }
      if ((pRVar4 != (RecoveredCJSON *)0x0) && (pRVar4->value_string != (char *)0x0)) {
        ds_append(call->arguments + 1,pRVar4->value_string);
      }
      call->argument_count = 2;
      break;
    case 2:
      pRVar3 = cJSON_GetObjectItem(arguments,"path");
      pRVar4 = cJSON_GetObjectItem(arguments,"oldText");
      pRVar2 = cJSON_GetObjectItem(arguments,"newText");
      if ((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) {
        ds_append(call->arguments,pRVar3->value_string);
      }
      if ((pRVar4 != (RecoveredCJSON *)0x0) && (pRVar4->value_string != (char *)0x0)) {
        ds_append(call->arguments + 1,pRVar4->value_string);
      }
      if ((pRVar2 != (RecoveredCJSON *)0x0) && (pRVar2->value_string != (char *)0x0)) {
        ds_append(call->arguments + 2,pRVar2->value_string);
      }
      call->argument_count = 3;
      break;
    case 3:
      pRVar3 = cJSON_GetObjectItem(arguments,"command");
      if ((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) {
        ds_append(call->arguments,pRVar3->value_string);
      }
      call->argument_count = 1;
      break;
    case 4:
      pRVar3 = cJSON_GetObjectItem(arguments,"pattern");
      if ((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) {
        ds_append(call->arguments,pRVar3->value_string);
      }
      call->argument_count = 1;
      break;
    case 5:
      pRVar3 = cJSON_GetObjectItem(arguments,"pattern");
      if ((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) {
        ds_append(call->arguments,pRVar3->value_string);
      }
      call->argument_count = 1;
      break;
    case 6:
      pRVar3 = cJSON_GetObjectItem(arguments,"query");
      if ((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) {
        ds_append(call->arguments,pRVar3->value_string);
      }
      call->argument_count = 1;
      break;
    case 7:
      pRVar3 = cJSON_GetObjectItem(arguments,"url");
      if ((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) {
        ds_append(call->arguments,pRVar3->value_string);
      }
      call->argument_count = 1;
      break;
    case 8:
      pRVar3 = cJSON_GetObjectItem(arguments,"db_path");
      pRVar4 = cJSON_GetObjectItem(arguments,"operation");
      pRVar2 = cJSON_GetObjectItem(arguments,"sql");
      if ((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) {
        ds_append(call->arguments,pRVar3->value_string);
      }
      if ((pRVar4 != (RecoveredCJSON *)0x0) && (pRVar4->value_string != (char *)0x0)) {
        ds_append(call->arguments + 1,pRVar4->value_string);
      }
      if ((pRVar2 != (RecoveredCJSON *)0x0) && (pRVar2->value_string != (char *)0x0)) {
        ds_append(call->arguments + 2,pRVar2->value_string);
      }
      call->argument_count = 3;
      break;
    case 9:
      pRVar3 = cJSON_GetObjectItem(arguments,"tag");
      pRVar4 = cJSON_GetObjectItem(arguments,"content");
      if ((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) {
        ds_append(call->arguments,pRVar3->value_string);
      }
      if ((pRVar4 != (RecoveredCJSON *)0x0) && (pRVar4->value_string != (char *)0x0)) {
        ds_append(call->arguments + 1,pRVar4->value_string);
      }
      call->argument_count = 2;
      break;
    case 10:
      pRVar3 = cJSON_GetObjectItem(arguments,"query");
      if ((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) {
        ds_append(call->arguments,pRVar3->value_string);
      }
      call->argument_count = 1;
      break;
    case 0xb:
      pRVar3 = cJSON_GetObjectItem(arguments,"path");
      pRVar4 = cJSON_GetObjectItem(arguments,"oldText");
      pRVar2 = cJSON_GetObjectItem(arguments,"newText");
      if ((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) {
        ds_append(call->arguments,pRVar3->value_string);
      }
      if ((pRVar4 != (RecoveredCJSON *)0x0) && (pRVar4->value_string != (char *)0x0)) {
        ds_append(call->arguments + 1,pRVar4->value_string);
      }
      if ((pRVar2 != (RecoveredCJSON *)0x0) && (pRVar2->value_string != (char *)0x0)) {
        ds_append(call->arguments + 2,pRVar2->value_string);
      }
      call->argument_count = 3;
      break;
    case 0xc:
      pRVar3 = cJSON_GetObjectItem(arguments,"prompt");
      pRVar4 = cJSON_GetObjectItem(arguments,"model");
      if ((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) {
        ds_append(call->arguments,pRVar3->value_string);
      }
      iVar1 = 1;
      if (pRVar4 != (RecoveredCJSON *)0x0) {
        iVar1 = 2;
        if (pRVar4->value_string != (char *)0x0) {
          ds_append(call->arguments + 1,pRVar4->value_string);
          iVar1 = 2;
        }
      }
      call->argument_count = iVar1;
      break;
    case 0xd:
      pRVar3 = cJSON_GetObjectItem(arguments,"query");
      if ((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) {
        ds_append(call->arguments,pRVar3->value_string);
      }
      call->argument_count = 1;
      break;
    case 0xe:
      pRVar3 = cJSON_GetObjectItem(arguments,"path");
      pRVar4 = cJSON_GetObjectItem(arguments,"max_depth");
      if ((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) {
        ds_append(call->arguments,pRVar3->value_string);
      }
      iVar1 = 1;
      if (pRVar4 != (RecoveredCJSON *)0x0) {
        iVar1 = 2;
        if (pRVar4->value_string != (char *)0x0) {
          ds_append(call->arguments + 1,pRVar4->value_string);
          iVar1 = 2;
        }
      }
      call->argument_count = iVar1;
      break;
    case 0xf:
      pRVar3 = cJSON_GetObjectItem(arguments,"pattern");
      pRVar4 = cJSON_GetObjectItem(arguments,"max_bytes_per_file");
      if ((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) {
        ds_append(call->arguments,pRVar3->value_string);
      }
      iVar1 = 1;
      if (pRVar4 != (RecoveredCJSON *)0x0) {
        iVar1 = 2;
        if (pRVar4->value_string != (char *)0x0) {
          ds_append(call->arguments + 1,pRVar4->value_string);
          iVar1 = 2;
        }
      }
      call->argument_count = iVar1;
    }
  }
  return;
}

/*
 * _provider_parse_tool_calls @ 0x00009f28..0x0000a39c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Decode provider tool-call objects and identifiers into internal tool-call records.
 * Parameters/return (inferred): void _provider_parse_tool_calls(RecoveredCJSON *array,RecoveredResponse *response)
 * Direct callers: _provider_parse_any@0x0000983c, _ollama_sse_cb@0x0000c3a0, _openai_sse_cb@0x0000e388
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00009f28_provider_parse_tool_calls.s
 */
void provider_parse_tool_calls(RecoveredCJSON *array,RecoveredResponse *response)

{
  bool bVar1;
  int iVar2;
  RecoveredCJSON *pRVar3;
  RecoveredCJSON *pRVar4;
  int index;
  RecoveredCJSON *pRVar5;
  RecoveredToolCall *call;
  int iVar6;
  char *left;
  RecoveredDString *str;
  
  if (((array != (RecoveredCJSON *)0x0) && (iVar2 = cJSON_GetArraySize(array), 0 < iVar2)) &&
     (response->tool_call_count < 99)) {
    index = 0;
    do {
      pRVar3 = cJSON_GetArrayItem(array,index);
      call = response->tool_calls + response->tool_call_count;
      memset(call,0,0x51c);
      ds_init(&call->result);
      iVar6 = 0;
      str = call->arguments;
      do {
        ds_init((RecoveredDString *)((int)&str->data + iVar6));
        bVar1 = iVar6 != 0x498;
        iVar6 = iVar6 + 0xc;
      } while (bVar1);
      call->argument_count = 0;
      pRVar4 = cJSON_GetObjectItem(pRVar3,"function");
      if (pRVar4 == (RecoveredCJSON *)0x0) {
        pRVar4 = (RecoveredCJSON *)0x0;
        pRVar5 = (RecoveredCJSON *)0x0;
      }
      else {
        pRVar5 = cJSON_GetObjectItem(pRVar4,"name");
        pRVar4 = cJSON_GetObjectItem(pRVar4,"arguments");
      }
      pRVar3 = cJSON_GetObjectItem(pRVar3,"id");
      if ((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) {
        strncpy(call->call_id,pRVar3->value_string,0x3f);
      }
      if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
        strncpy(call->mcp_name,pRVar5->value_string,0x1f);
        left = pRVar5->value_string;
        iVar6 = strcmp(left,"read_file");
        if (iVar6 == 0) {
          call->tool_type = 0;
        }
        else {
          iVar6 = strcmp(left,"write_file");
          if (iVar6 == 0) {
            call->tool_type = 1;
          }
          else {
            iVar6 = strcmp(left,"edit_file");
            if (iVar6 == 0) {
              call->tool_type = 2;
            }
            else {
              iVar6 = strcmp(left,"shell");
              if (iVar6 == 0) {
                call->tool_type = 3;
              }
              else {
                iVar6 = strcmp(left,"glob");
                if (iVar6 == 0) {
                  call->tool_type = 4;
                }
                else {
                  iVar6 = strcmp(left,"grep");
                  if (iVar6 == 0) {
                    call->tool_type = 5;
                  }
                  else {
                    iVar6 = strcmp(left,"search");
                    if (iVar6 == 0) {
                      call->tool_type = 6;
                    }
                    else {
                      iVar6 = strcmp(left,"fetch_url");
                      if (iVar6 == 0) {
                        call->tool_type = 7;
                      }
                      else {
                        iVar6 = strcmp(left,"db_query");
                        if (iVar6 == 0) {
                          call->tool_type = 8;
                        }
                        else {
                          iVar6 = strcmp(left,"memory_remember");
                          if (iVar6 == 0) {
                            call->tool_type = 9;
                          }
                          else {
                            iVar6 = strcmp(left,"memory_load");
                            if (iVar6 == 0) {
                              call->tool_type = 10;
                            }
                            else {
                              iVar6 = strcmp(left,"read_and_edit");
                              if (iVar6 == 0) {
                                call->tool_type = 0xb;
                              }
                              else {
                                iVar6 = strcmp(left,"subagent");
                                if (iVar6 == 0) {
                                  call->tool_type = 0xc;
                                }
                                else {
                                  iVar6 = strcmp(left,"reference");
                                  if (iVar6 == 0) {
                                    call->tool_type = 0xd;
                                  }
                                  else {
                                    iVar6 = strcmp(left,"tree");
                                    if (iVar6 == 0) {
                                      call->tool_type = 0xe;
                                    }
                                    else {
                                      iVar6 = strcmp(left,"read_project");
                                      if (iVar6 == 0) {
                                        call->tool_type = 0xf;
                                      }
                                      else {
                                        call->tool_type = 100;
                                        strncpy(call->mcp_name,pRVar5->value_string,0x1f);
                                      }
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
      if (pRVar4 != (RecoveredCJSON *)0x0) {
        if (pRVar4->value_string == (char *)0x0) {
          if (pRVar4->type_flags == 7) {
            provider_parse_tool_calls_args(call,pRVar4);
          }
        }
        else {
          pRVar3 = cJSON_Parse(pRVar4->value_string);
          if (pRVar3 != (RecoveredCJSON *)0x0) {
            provider_parse_tool_calls_args(call,pRVar3);
            cJSON_Delete(pRVar3);
          }
        }
      }
      if (((call->arguments[0].length == 0) && (call->tool_type != 0xe)) && (call->tool_type != 0xf)
         ) {
        ds_free(&call->result);
        iVar6 = 0;
        do {
          ds_free(str);
          str = str + 1;
          bVar1 = iVar6 != 0x62;
          iVar6 = iVar6 + 1;
        } while (bVar1);
      }
      else {
        response->tool_call_count = response->tool_call_count + 1;
      }
      index = index + 1;
    } while ((iVar2 != index) && (response->tool_call_count < 99));
  }
  return;
}

/*
 * _provider_send @ 0x0000a39c..0x0000a510
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Dispatch one request through the selected Ollama/OpenAI/Anthropic/Google implementation.
 * Parameters/return (inferred): int _provider_send(RecoveredConfig *config,RecoveredConversation *conversation, RecoveredResponse *response,RecoveredStreamCallback callback,void *context, RecoveredRoute *route,int *failures)
 * Direct callers: _handle_slash_command@0x0000362c, _simple_mode@0x00004e30, _sa_child_main@0x0001327c, _handle_cmd@0x0001b004, _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O; opens/creates/truncates filesystem paths according to the supplied mode
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000a39c_provider_send.s
 */
int provider_send(RecoveredConfig *config,RecoveredConversation *conversation,
                  RecoveredResponse *response,RecoveredStreamCallback callback,void *context,
                  RecoveredRoute *route,int *failures)

{
  bool bVar1;
  int iVar2;
  int iVar3;
  
  ds_init(&response->content);
  ds_init(&response->error_or_reasoning);
  response->unknown_18 = 0;
  response->tool_call_count = 0;
  response->completion_flag = 0;
  response->finish_reason = 0;
  response->input_tokens = 0;
  response->output_tokens = 0;
  response->total_tokens = 0;
  response->usage_known = 0;
  iVar2 = 0;
  do {
    ds_init(&response->tool_calls[iVar2].result);
    iVar3 = 0;
    do {
      ds_init((RecoveredDString *)((int)&response->tool_calls[iVar2].arguments[0].data + iVar3));
      bVar1 = iVar3 != 0x498;
      iVar3 = iVar3 + 0xc;
    } while (bVar1);
    bVar1 = iVar2 != 0x62;
    iVar2 = iVar2 + 1;
  } while (bVar1);
  iVar2 = config->provider;
  if (iVar2 == 1) {
    iVar3 = openai_send(config,conversation,response,callback,context,route,failures);
  }
  else if (iVar2 == 0) {
    iVar3 = ollama_send(config,conversation,response,callback,context,route,failures);
  }
  else if (iVar2 == 2) {
    iVar3 = anthropic_send(config,conversation,response,callback,context,route,failures);
  }
  else {
    iVar3 = -1;
    if (iVar2 == 3) {
      iVar3 = google_send(config,conversation,response,callback,context,route,failures);
    }
  }
  return iVar3;
}

/*
 * _tools_schema_add_simple @ 0x0000a510..0x0000a68c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in provider_common: tools schema add simple. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _tools_schema_add_simple (RecoveredCJSON *array,char *name,char *description,char *argument_name, char *argument_description,RecoveredRoute *route,int *failures,int tool_type)
 * Direct callers: _tools_schema_build_standard@0x0000acd4
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000a510_tools_schema_add_simple.s
 */
void tools_schema_add_simple
               (RecoveredCJSON *array,char *name,char *description,char *argument_name,
               char *argument_description,RecoveredRoute *route,int *failures,int tool_type)

{
  int iVar1;
  RecoveredCJSON *object;
  RecoveredCJSON *object_00;
  RecoveredCJSON *pRVar2;
  RecoveredCJSON *pRVar3;
  
  if (route != (RecoveredRoute *)0x0) {
    iVar1 = router_should_send_tool
                      (tool_type,route->intent_mask,route->mentions_path,route->short_message);
    if (iVar1 == 0) {
      return;
    }
    iVar1 = trust_is_trusted(tool_type,failures);
    if (iVar1 == 0) {
      return;
    }
  }
  object = cJSON_CreateObject();
  cJSON_AddStringToObject(object,"name",name);
  cJSON_AddStringToObject(object,"description",description);
  object_00 = cJSON_CreateObject();
  cJSON_AddStringToObject(object_00,"type","object");
  pRVar2 = cJSON_CreateObject();
  pRVar3 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar3,"type","string");
  if (argument_description != (char *)0x0) {
    cJSON_AddStringToObject(pRVar3,"description",argument_description);
  }
  cJSON_AddItemToObject(pRVar2,argument_name,pRVar3);
  cJSON_AddItemToObject(object_00,"properties",pRVar2);
  pRVar2 = cJSON_CreateArray();
  pRVar3 = cJSON_CreateString(argument_name);
  cJSON_AddItemToArray(pRVar2,pRVar3);
  cJSON_AddItemToObject(object_00,"required",pRVar2);
  cJSON_AddItemToObject(object,"parameters",object_00);
  cJSON_AddItemToArray(array,object);
  return;
}

/*
 * _tools_schema_add_read_and_edit @ 0x0000a68c..0x0000a8ac
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in provider_common: tools schema add read and edit. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _tools_schema_add_read_and_edit(RecoveredCJSON *array,RecoveredRoute *route,int *failures)
 * Direct callers: _tools_schema_build_standard@0x0000acd4
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000a68c_tools_schema_add_read_and_edit.s
 */
void tools_schema_add_read_and_edit(RecoveredCJSON *array,RecoveredRoute *route,int *failures)

{
  int iVar1;
  RecoveredCJSON *object;
  RecoveredCJSON *object_00;
  RecoveredCJSON *pRVar2;
  RecoveredCJSON *pRVar3;
  
  if (route != (RecoveredRoute *)0x0) {
    iVar1 = router_should_send_tool
                      (0xb,route->intent_mask,route->mentions_path,route->short_message);
    if (iVar1 == 0) {
      return;
    }
    iVar1 = trust_is_trusted(0xb,failures);
    if (iVar1 == 0) {
      return;
    }
  }
  object = cJSON_CreateObject();
  cJSON_AddStringToObject(object,"name","read_and_edit");
  cJSON_AddStringToObject
            (object,"description",
             "Read a file and then apply an edit in one step. Equivalent to read_file followed by edit_file. Args: path, oldText, newText."
            );
  object_00 = cJSON_CreateObject();
  cJSON_AddStringToObject(object_00,"type","object");
  pRVar2 = cJSON_CreateObject();
  pRVar3 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar3,"type","string");
  cJSON_AddStringToObject(pRVar3,"description","File path to read and edit");
  cJSON_AddItemToObject(pRVar2,"path",pRVar3);
  pRVar3 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar3,"type","string");
  cJSON_AddStringToObject(pRVar3,"description","Exact text to replace");
  cJSON_AddItemToObject(pRVar2,"oldText",pRVar3);
  pRVar3 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar3,"type","string");
  cJSON_AddStringToObject(pRVar3,"description","New replacement text");
  cJSON_AddItemToObject(pRVar2,"newText",pRVar3);
  cJSON_AddItemToObject(object_00,"properties",pRVar2);
  pRVar2 = cJSON_CreateArray();
  pRVar3 = cJSON_CreateString("path");
  cJSON_AddItemToArray(pRVar2,pRVar3);
  pRVar3 = cJSON_CreateString("oldText");
  cJSON_AddItemToArray(pRVar2,pRVar3);
  pRVar3 = cJSON_CreateString("newText");
  cJSON_AddItemToArray(pRVar2,pRVar3);
  cJSON_AddItemToObject(object_00,"required",pRVar2);
  cJSON_AddItemToObject(object,"parameters",object_00);
  cJSON_AddItemToArray(array,object);
  return;
}

/*
 * _tools_schema_add_fetch_url @ 0x0000a8ac..0x0000aa20
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in provider_common: tools schema add fetch url. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _tools_schema_add_fetch_url(RecoveredCJSON *array,RecoveredRoute *route,int *failures)
 * Direct callers: _tools_schema_build_standard@0x0000acd4
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000a8ac_tools_schema_add_fetch_url.s
 */
void tools_schema_add_fetch_url(RecoveredCJSON *array,RecoveredRoute *route,int *failures)

{
  int iVar1;
  RecoveredCJSON *object;
  RecoveredCJSON *object_00;
  RecoveredCJSON *pRVar2;
  RecoveredCJSON *pRVar3;
  
  if (route != (RecoveredRoute *)0x0) {
    iVar1 = router_should_send_tool(7,route->intent_mask,route->mentions_path,route->short_message)
    ;
    if (iVar1 == 0) {
      return;
    }
    iVar1 = trust_is_trusted(7,failures);
    if (iVar1 == 0) {
      return;
    }
  }
  object = cJSON_CreateObject();
  cJSON_AddStringToObject(object,"name","fetch_url");
  cJSON_AddStringToObject
            (object,"description",
             "Fetch the text content of a web URL. Convert HTML to readable text.");
  object_00 = cJSON_CreateObject();
  cJSON_AddStringToObject(object_00,"type","object");
  pRVar2 = cJSON_CreateObject();
  pRVar3 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar3,"type","string");
  cJSON_AddStringToObject(pRVar3,"description","Full URL to fetch");
  cJSON_AddItemToObject(pRVar2,"url",pRVar3);
  cJSON_AddItemToObject(object_00,"properties",pRVar2);
  pRVar2 = cJSON_CreateArray();
  pRVar3 = cJSON_CreateString("url");
  cJSON_AddItemToArray(pRVar2,pRVar3);
  cJSON_AddItemToObject(object_00,"required",pRVar2);
  cJSON_AddItemToObject(object,"parameters",object_00);
  cJSON_AddItemToArray(array,object);
  return;
}

/*
 * _tools_schema_add_db_query @ 0x0000aa20..0x0000acd4
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in provider_common: tools schema add db query. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _tools_schema_add_db_query(RecoveredCJSON *array,RecoveredRoute *route,int *failures)
 * Direct callers: _tools_schema_build_standard@0x0000acd4
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000aa20_tools_schema_add_db_query.s
 */
void tools_schema_add_db_query(RecoveredCJSON *array,RecoveredRoute *route,int *failures)

{
  int iVar1;
  RecoveredCJSON *object;
  RecoveredCJSON *object_00;
  RecoveredCJSON *pRVar2;
  RecoveredCJSON *pRVar3;
  RecoveredCJSON *array_00;
  RecoveredCJSON *pRVar4;
  
  if (route != (RecoveredRoute *)0x0) {
    iVar1 = router_should_send_tool(8,route->intent_mask,route->mentions_path,route->short_message)
    ;
    if (iVar1 == 0) {
      return;
    }
    iVar1 = trust_is_trusted(8,failures);
    if (iVar1 == 0) {
      return;
    }
  }
  object = cJSON_CreateObject();
  cJSON_AddStringToObject(object,"name","db_query");
  cJSON_AddStringToObject
            (object,"description",
             "Query or modify a SQLite database. Supports SELECT, INSERT, UPDATE, DELETE, and PRAGMA."
            );
  object_00 = cJSON_CreateObject();
  cJSON_AddStringToObject(object_00,"type","object");
  pRVar2 = cJSON_CreateObject();
  pRVar3 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar3,"type","string");
  cJSON_AddStringToObject(pRVar3,"description","Absolute path to .sqlite or .db file");
  cJSON_AddItemToObject(pRVar2,"db_path",pRVar3);
  pRVar3 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar3,"type","string");
  array_00 = cJSON_CreateArray();
  pRVar4 = cJSON_CreateString("query");
  cJSON_AddItemToArray(array_00,pRVar4);
  pRVar4 = cJSON_CreateString("tables");
  cJSON_AddItemToArray(array_00,pRVar4);
  pRVar4 = cJSON_CreateString("schema");
  cJSON_AddItemToArray(array_00,pRVar4);
  pRVar4 = cJSON_CreateString("insert");
  cJSON_AddItemToArray(array_00,pRVar4);
  pRVar4 = cJSON_CreateString("update");
  cJSON_AddItemToArray(array_00,pRVar4);
  pRVar4 = cJSON_CreateString("delete");
  cJSON_AddItemToArray(array_00,pRVar4);
  cJSON_AddItemToObject(pRVar3,"enum",array_00);
  cJSON_AddStringToObject(pRVar3,"description","Operation type");
  cJSON_AddItemToObject(pRVar2,"operation",pRVar3);
  pRVar3 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar3,"type","string");
  cJSON_AddStringToObject
            (pRVar3,"description","SQL statement (required for query/insert/update/delete/schema)");
  cJSON_AddItemToObject(pRVar2,"sql",pRVar3);
  cJSON_AddItemToObject(object_00,"properties",pRVar2);
  pRVar2 = cJSON_CreateArray();
  pRVar3 = cJSON_CreateString("db_path");
  cJSON_AddItemToArray(pRVar2,pRVar3);
  pRVar3 = cJSON_CreateString("operation");
  cJSON_AddItemToArray(pRVar2,pRVar3);
  cJSON_AddItemToObject(object_00,"required",pRVar2);
  cJSON_AddItemToObject(object,"parameters",object_00);
  cJSON_AddItemToArray(array,object);
  return;
}

/*
 * _tools_schema_build_standard @ 0x0000acd4..0x0000b2f0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Construct shared JSON schemas for the application's standard tools.
 * Parameters/return (inferred): RecoveredCJSON * _tools_schema_build_standard(RecoveredRoute *route,int *failures)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000acd4_tools_schema_build_standard.s
 */
RecoveredCJSON * tools_schema_build_standard(RecoveredRoute *route,int *failures)

{
  RecoveredCJSON *array;
  int iVar1;
  RecoveredCJSON *pRVar2;
  RecoveredCJSON *pRVar3;
  RecoveredCJSON *pRVar4;
  RecoveredCJSON *pRVar5;
  
  array = cJSON_CreateArray();
  tools_schema_add_simple
            (array,"read_file",
             "Read the contents of one or more files. Multiple paths can be comma-separated. Example: path=\'src/main.c,src/config.c\'"
             ,"path","File path",route,failures,0);
  tools_schema_add_simple
            (array,"write_file",
             "Write content to a file. Creates parent directories if needed. Overwrites existing files. Example: path=\'src/utils.c\', content=\'...\'"
             ,"path","File path",route,failures,1);
  tools_schema_add_simple
            (array,"edit_file",
             "Edit a file by replacing old text with new text. The oldText must match exactly (including whitespace). Only replaces the first occurrence. Example: path=\'src/main.c\', oldText=\'int x = 1;\', newText=\'int x = 2;\'"
             ,"path","File path",route,failures,2);
  tools_schema_add_simple
            (array,"shell",
             "Execute a shell command in the working directory. Use for listing files, running builds, checking git status, etc. Commands run with bash. Example: command=\'ls -la\'"
             ,"command","Command to run",route,failures,3);
  tools_schema_add_simple
            (array,"glob",
             "Find files matching a glob pattern in the working directory and subdirectories. Example: pattern=\'*.c\'"
             ,"pattern","Glob pattern",route,failures,4);
  tools_schema_add_simple
            (array,"grep",
             "Search file contents for a regex pattern. Searches recursively in the working directory. Example: pattern=\'TODO|FIXME\'"
             ,"pattern","Regex pattern",route,failures,5);
  tools_schema_add_simple
            (array,"search","Search the web via DuckDuckGo.","query","Search query text",route,
             failures,6);
  tools_schema_add_read_and_edit(array,route,failures);
  tools_schema_add_fetch_url(array,route,failures);
  tools_schema_add_db_query(array,route,failures);
  if ((route == (RecoveredRoute *)0x0) ||
     ((iVar1 = router_should_send_tool
                         (0xc,route->intent_mask,route->mentions_path,route->short_message),
      iVar1 != 0 && (iVar1 = trust_is_trusted(0xc,failures), iVar1 != 0)))) {
    pRVar2 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar2,"name","subagent");
    cJSON_AddStringToObject
              (pRVar2,"description",
               "Launch a sub-agent to handle a complex multi-step task. The sub-agent runs in a separate process with up to 5 tool-calling turns and returns a summarized result."
              );
    pRVar3 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar3,"type","object");
    pRVar4 = cJSON_CreateObject();
    pRVar5 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar5,"type","string");
    cJSON_AddStringToObject(pRVar5,"description","Task description for the sub-agent.");
    cJSON_AddItemToObject(pRVar4,"prompt",pRVar5);
    pRVar5 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar5,"type","string");
    cJSON_AddStringToObject(pRVar5,"description","Model name (optional).");
    cJSON_AddItemToObject(pRVar4,"model",pRVar5);
    cJSON_AddItemToObject(pRVar3,"properties",pRVar4);
    pRVar4 = cJSON_CreateArray();
    pRVar5 = cJSON_CreateString("prompt");
    cJSON_AddItemToArray(pRVar4,pRVar5);
    cJSON_AddItemToObject(pRVar3,"required",pRVar4);
    cJSON_AddItemToObject(pRVar2,"parameters",pRVar3);
    cJSON_AddItemToArray(array,pRVar2);
    if (route != (RecoveredRoute *)0x0) goto LAB_0000b014;
LAB_0000b044:
    pRVar2 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar2,"name","tree");
    cJSON_AddStringToObject
              (pRVar2,"description",
               "Return a recursive directory tree with file sizes in ONE call. Use to discover all files. Skips .git, node_modules, build dirs."
              );
    pRVar3 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar3,"type","object");
    pRVar4 = cJSON_CreateObject();
    pRVar5 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar5,"type","string");
    cJSON_AddStringToObject
              (pRVar5,"description","Sub-path relative to working dir (default: project root)");
    cJSON_AddItemToObject(pRVar4,"path",pRVar5);
    pRVar5 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar5,"type","integer");
    cJSON_AddStringToObject(pRVar5,"description","Max recursion depth 1-8 (default 4)");
    cJSON_AddItemToObject(pRVar4,"max_depth",pRVar5);
    cJSON_AddItemToObject(pRVar3,"properties",pRVar4);
    cJSON_AddItemToObject(pRVar2,"parameters",pRVar3);
    cJSON_AddItemToArray(array,pRVar2);
    if (route == (RecoveredRoute *)0x0) goto LAB_0000b190;
  }
  else {
LAB_0000b014:
    iVar1 = router_should_send_tool
                      (0xe,route->intent_mask,route->mentions_path,route->short_message);
    if ((iVar1 != 0) && (iVar1 = trust_is_trusted(0xe,failures), iVar1 != 0)) goto LAB_0000b044;
  }
  iVar1 = router_should_send_tool(0xf,route->intent_mask,route->mentions_path,route->short_message)
  ;
  if (iVar1 == 0) {
    return array;
  }
  iVar1 = trust_is_trusted(0xf,failures);
  if (iVar1 == 0) {
    return array;
  }
LAB_0000b190:
  pRVar2 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar2,"name","read_project");
  cJSON_AddStringToObject
            (pRVar2,"description",
             "Atomically read every file matching a glob in ONE call. Use this to ingest a whole module or list of files without burning exploration steps. Each file is truncated to max_bytes_per_file."
            );
  pRVar3 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar3,"type","object");
  pRVar4 = cJSON_CreateObject();
  pRVar5 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar5,"type","string");
  cJSON_AddStringToObject
            (pRVar5,"description","Glob pattern relative to working dir (e.g. \'src/*.c\')");
  cJSON_AddItemToObject(pRVar4,"pattern",pRVar5);
  pRVar5 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar5,"type","integer");
  cJSON_AddStringToObject(pRVar5,"description","Max bytes per file (default 65536)");
  cJSON_AddItemToObject(pRVar4,"max_bytes_per_file",pRVar5);
  cJSON_AddItemToObject(pRVar3,"properties",pRVar4);
  pRVar4 = cJSON_CreateArray();
  pRVar5 = cJSON_CreateString("pattern");
  cJSON_AddItemToArray(pRVar4,pRVar5);
  cJSON_AddItemToObject(pRVar3,"required",pRVar4);
  cJSON_AddItemToObject(pRVar2,"parameters",pRVar3);
  cJSON_AddItemToArray(array,pRVar2);
  return array;
}

/*
 * _kw_match @ 0x0000b2f0..0x0000b3f4
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in provider_common: kw match. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _kw_match(char *text,char *keyword)
 * Direct callers: _provider_detect_model_size@0x0000b3f4
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000b2f0_kw_match.s
 */
int kw_match(char *text,char *keyword)

{
  byte bVar1;
  bool bVar2;
  undefined *puVar3;
  uint uVar4;
  uint uVar5;
  int iVar6;
  char *pcVar7;
  
  uVar5 = strlen(keyword);
  puVar3 = (undefined *)(PTR_000396b0);
  pcVar7 = text;
  do {
    pcVar7 = strstr(pcVar7,keyword);
    if (pcVar7 == (char *)0x0) {
      return 0;
    }
    if (text == pcVar7) {
LAB_0000b378:
      bVar2 = true;
    }
    else {
      bVar1 = pcVar7[-1];
      if ((bVar1 & 0x80) == 0) {
        uVar4 = -(*(uint *)(puVar3 + (uint)bVar1 * 4 + 0x34) & 0x500) >> 0x1f;
      }
      else {
        iVar6 = __maskrune((uint)bVar1,0x500);
        uVar4 = (uint)(iVar6 != 0);
      }
      bVar2 = false;
      if (uVar4 == 0) goto LAB_0000b378;
    }
    bVar1 = pcVar7[uVar5];
    if ((bVar1 & 0x80) == 0) {
      uVar4 = -(*(uint *)(puVar3 + (uint)bVar1 * 4 + 0x34) & 0x500) >> 0x1f;
    }
    else {
      iVar6 = __maskrune((uint)bVar1,0x500);
      uVar4 = (uint)(iVar6 != 0);
    }
    if ((bVar2) && (uVar4 == 0)) {
      return 1;
    }
    pcVar7 = pcVar7 + 1;
  } while( true );
}

/*
 * _provider_detect_model_size @ 0x0000b3f4..0x0000b62c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in provider_common: provider detect model size. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _provider_detect_model_size(char *model)
 * Direct callers: _config_load@0x00005fb8
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000b3f4_provider_detect_model_size.s
 */
int provider_detect_model_size(char *model)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0xc0] __attribute__((aligned(16)));
#define local_a8 (*((char (*)[148])(app_frame + 0x8))) /* original SP - 0xa8 */

  bool bVar1;
  char *pcVar2;
  __darwin_ct_rune_t _Var3;
  int iVar4;

  if ((model != (char *)0x0) && (*model != '\0')) {
    pcVar2 = strstr(model,":1.5b");
    if (((pcVar2 != (char *)0x0) ||
        (((pcVar2 = strstr(model,":3b"), pcVar2 != (char *)0x0 ||
          (pcVar2 = strstr(model,":4b"), pcVar2 != (char *)0x0)) ||
         (pcVar2 = strstr(model,":7b"), pcVar2 != (char *)0x0)))) ||
       ((pcVar2 = strstr(model,":8b"), pcVar2 != (char *)0x0 ||
        (pcVar2 = strstr(model,":1b"), pcVar2 != (char *)0x0)))) {
      return 1;
    }
    pcVar2 = strstr(model,":27b");
    if (((pcVar2 == (char *)0x0) && (pcVar2 = strstr(model,":30b"), pcVar2 == (char *)0x0)) &&
       ((pcVar2 = strstr(model,":70b"), pcVar2 == (char *)0x0 &&
        ((pcVar2 = strstr(model,":72b"), pcVar2 == (char *)0x0 &&
         (pcVar2 = strstr(model,":8x7b"), pcVar2 == (char *)0x0)))))) {
      iVar4 = 0;
      do {
        if (model[iVar4] == 0) break;
        _Var3 = __tolower((uint)(byte)model[iVar4]);
        local_a8[iVar4] = (char)_Var3;
        bVar1 = iVar4 != 0x7e;
        iVar4 = iVar4 + 1;
      } while (bVar1);
      local_a8[iVar4] = '\0';
      iVar4 = kw_match(local_a8,"mini");
      if (iVar4 != 0) {
        return 1;
      }
      iVar4 = kw_match(local_a8,"nano");
      if (iVar4 != 0) {
        return 1;
      }
      iVar4 = kw_match(local_a8,"tiny");
      if (iVar4 != 0) {
        return 1;
      }
      iVar4 = kw_match(local_a8,"pro");
      if (((iVar4 == 0) && (iVar4 = kw_match(local_a8,"max"), iVar4 == 0)) &&
         (iVar4 = kw_match(local_a8,"opus"), iVar4 == 0)) {
        kw_match(local_a8,"large");
        return 0;
      }
    }
  }
  return 0;

#undef local_a8
}

