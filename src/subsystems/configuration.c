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
 * _provider_name @ 0x00005958..0x000059c4
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Map provider enum 0..3 to ollama/openai/anthropic/google labels.
 * Parameters/return (inferred): char * _provider_name(int provider)
 * Direct callers: _handle_slash_command@0x0000362c, _simple_mode@0x00004e30, _config_save@0x00006820, _profile_get_name@0x00008dc4, _handle_cmd@0x0001b004, _tui_run@0x0001df44, _worker_serialize_in@0x00024740
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00005958_provider_name.s
 */
char * provider_name(int provider)

{
  if (provider == 1) {
    return "openai";
  }
  if (provider == 0) {
    return "ollama";
  }
  if (provider == 2) {
    return "anthropic";
  }
  if (provider == 3) {
    return "google";
  }
  return "unknown";
}

/*
 * _provider_from_name @ 0x000059c4..0x00005ad8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Parse provider names and supported aliases into the provider enum.
 * Parameters/return (inferred): int _provider_from_name(char *name)
 * Direct callers: _handle_slash_command@0x0000362c, _main@0x00005498, _config_load@0x00005fb8, _handle_cmd@0x0001b004, _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000059c4_provider_from_name.s
 */
int provider_from_name(char *name)

{
  int iVar1;
  
  if (name != (char *)0x0) {
    iVar1 = strcasecmp(name,"ollama");
    if (iVar1 == 0) {
      return 0;
    }
    iVar1 = strcasecmp(name,"openai");
    if (((iVar1 == 0) || (iVar1 = strcasecmp(name,"chatgpt"), iVar1 == 0)) ||
       (iVar1 = strcasecmp(name,"gpt"), iVar1 == 0)) {
      return 1;
    }
    iVar1 = strcasecmp(name,"anthropic");
    if ((iVar1 == 0) || (iVar1 = strcasecmp(name,"claude"), iVar1 == 0)) {
      return 2;
    }
    iVar1 = strcasecmp(name,"google");
    if ((iVar1 == 0) || (iVar1 = strcasecmp(name,"gemini"), iVar1 == 0)) {
      return 3;
    }
  }
  return -1;
}

/*
 * _tool_name @ 0x00005ad8..0x00005c18
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Map tool enum 0..15 to retained tool names; other values fall back to unknown.
 * Parameters/return (inferred): char * _tool_name(int tool_type)
 * Direct callers: _simple_mode@0x00004e30, _ollama_build_request@0x0000bdf0, _openai_build_messages@0x0000dd28, _anthropic_build_messages@0x0000f520, _google_build_contents@0x00011474, _sa_child_main@0x0001327c, _trust_warning@0x00015214, _execute_plan_steps@0x00022454, _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00005ad8_tool_name.s
 */
char * tool_name(int tool_type)

{
  switch(tool_type) {
  case 0:
    return "read_file";
  case 1:
    return "write_file";
  case 2:
    return "edit_file";
  case 3:
    return "shell";
  case 4:
    return "glob";
  case 5:
    return "grep";
  case 6:
    return "search";
  case 7:
    return "fetch_url";
  case 8:
    return "db_query";
  case 9:
    return "memory_remember";
  case 10:
    return "memory_load";
  case 0xb:
    return "read_and_edit";
  case 0xc:
    return "subagent";
  case 0xd:
    return "reference";
  case 0xe:
    return "tree";
  case 0xf:
    return "read_project";
  default:
    return "unknown";
  }
}

/*
 * _config_get_key @ 0x00005c18..0x00005ca8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Select a nonempty provider-specific credential, otherwise the fallback api_key.
 * Parameters/return (inferred): char * _config_get_key(RecoveredConfig *config)
 * Direct callers: _profile_get_key@0x00008d0c, _ollama_send@0x0000c70c, _openai_send@0x0000e5ec, _anthropic_send@0x00010540, _google_send@0x00012238, _tool_subagent@0x00015748
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00005c18_config_get_key.s
 */
char * config_get_key(RecoveredConfig *config)

{
  int iVar1;
  
  iVar1 = config->provider;
  if (iVar1 == 1) {
    if (config->openai_key[0] != '\0') {
      return config->openai_key;
    }
    return config->api_key;
  }
  if (iVar1 == 0) {
    if (config->ollama_key[0] != '\0') {
      return config->ollama_key;
    }
    return config->api_key;
  }
  if (iVar1 == 2) {
    if (config->anthropic_key[0] != '\0') {
      return config->anthropic_key;
    }
    return config->api_key;
  }
  if (iVar1 != 3) {
    return config->api_key;
  }
  if (config->google_key[0] != '\0') {
    return config->google_key;
  }
  return config->api_key;
}

/*
 * _config_get_url @ 0x00005ca8..0x00005d44
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Select Ollama-specific URL, generic base URL or the chosen provider's built-in endpoint.
 * Parameters/return (inferred): char * _config_get_url(RecoveredConfig *config)
 * Direct callers: _handle_slash_command@0x0000362c, _do_async_fetch@0x000087a8, _profile_get_url@0x00008d68, _ollama_sync_model_metadata@0x0000bb9c, _ollama_send@0x0000c70c, _openai_send@0x0000e5ec, _anthropic_send@0x00010540, _google_send@0x00012238, _tool_subagent@0x00015748, _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00005ca8_config_get_url.s
 */
char * config_get_url(RecoveredConfig *config)

{
  int iVar1;
  
  iVar1 = config->provider;
  if (iVar1 == 0) {
    if (config->ollama_url[0] != '\0') {
      return config->ollama_url;
    }
  }
  if (config->base_url[0] != '\0') {
    return config->base_url;
  }
  if (iVar1 == 1) {
    return "https://api.openai.com";
  }
  if (iVar1 != 0) {
    if (iVar1 == 2) {
      return "https://api.anthropic.com";
    }
    if (iVar1 != 3) {
      return "";
    }
    return "https://generativelanguage.googleapis.com";
  }
  return "https://api.ollama.com";
}

/*
 * _get_home @ 0x00005d44..0x00005da8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Resolve a home-directory value for this translation unit; inspect this body because fallback behavior differs between helpers with the same symbol name.
 * Parameters/return (inferred): char * _get_home(void)
 * Direct callers: _get_config_path@0x00005ee8
 * Possible side effects (direct-call closure, incomplete for indirect calls): reads process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00005d44_get_home.s
 */
char * configuration_get_home(void)

{
  char *pcVar1;
  int iVar2;
  
  pcVar1 = getenv("HOME");
  if (pcVar1 == (char *)0x0) {
    iVar2 = (int)getpwuid(getuid());
    if (iVar2 == 0) goto LAB_00005d94;
    pcVar1 = *(char **)(iVar2 + 0x1c);
  }
  if (pcVar1 != (char *)0x0) {
    return pcVar1;
  }
LAB_00005d94:
  return ".";
}

/*
 * _config_init @ 0x00005da8..0x00005ee8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Zero configuration, initialize strings and set defaults, including OpenAI/gpt-4o-mini, auto_approve=0 and write_guard_enforce=0.
 * Parameters/return (inferred): void _config_init(RecoveredConfig *config)
 * Direct callers: _main@0x00005498, _sa_child_main@0x0001327c, _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): reads the process working directory
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/00005da8_config_init.s
 * Preserved behavior/security: GUARD-NOTE: unread-file guard defaults to zero despite example configuration setting it to one.
 */
void config_init(RecoveredConfig *config)

{
  char *pcVar1;
  int iVar2;
  RecoveredDString *str;
  
  memset(config,0,0x3668);
  str = &config->system_prompt;
  ds_init(str);
  config->provider = 1;
  strncpy(config->model,"gpt-4o-mini",0x7f);
  config->max_tokens = 0x10000;
  config->temperature = 0.7;
  config->verbose = 0;
  config->auto_approve = 0;
  config->auto_compact = 0;
  config->auto_save_session = 1;
  config->small_model_mode = 0;
  config->effort = 1;
  config->sidebar_depth = 1;
  config->think_budget = 0;
  config->write_guard_enforce = 0;
  config->search_url[0] = '\0';
  pcVar1 = getcwd(config->working_directory,0x3ff);
  if (pcVar1 == (char *)0x0) {
    strncpy(config->working_directory,".",0x3ff);
  }
  strncpy(config->theme,"light",0x1f);
  ds_append(str,
             "You are PowerPC Code, an AI coding assistant for vintage Mac OS X Tiger. Running on ")
  ;
  ds_append(str,PTR__g_platform_name_0003969c);
  ds_append(str,". ");
  iVar2 = platform_is_ppc();
  if (iVar2 != 0) {
    ds_append(str,"Architecture: PowerPC G3/G4/G5. ");
  }
  ds_append(str,
             "Be helpful and conversational. Complete the user\'s request using your available tools. Always take action: read files, write code, run builds. You have full access to read, write, edit, shell, glob, grep, search, and web fetch tools. Use them proactively to accomplish the task.\n### Task Protocol:\n1. **Analyze**: Use \'ls\' or \'read_file\' briefly to understand the codebase. **ANTI-LOOP**: Never read the same file twice.\n2. **Plan**: If complex, outline steps with a markdown checklist (- [ ] task).\n3. **Write**: Move to \'write_file\' as soon as possible to deliver code.\n4. **Verify**: Run builds or tests to confirm your changes work.\n### Guidelines:\n- **Read before edit**: Always read a file before writing or editing it.\n- **Docs**: Use \'reference\' for Carbon/Cocoa API lookups.\n- **Constraint**: Use efficient Objective-C/C for 2005 hardware.\n- **Never ask for permission**: Just do the work. Fix errors immediately."
            );
  return;
}

/*
 * _get_config_path @ 0x00005ee8..0x00005f3c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Construct the resolved-home/.ppccoderc configuration pathname.
 * Parameters/return (inferred): undefined * _get_config_path(void)
 * Direct callers: _config_load@0x00005fb8, _config_save@0x00006820
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; reads process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00005ee8_get_config_path.s
 */
undefined * get_config_path(void)

{
  undefined4 uVar1;
  
  uVar1 = (undefined4)(configuration_get_home());
  snprintf(&_path_4008,0x400,"%s/.ppccoderc",(char *)(uVar1));
  return (undefined *)(&_path_4008);
}

/*
 * _strtrim @ 0x00005f3c..0x00005fb8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in configuration: strtrim. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _strtrim(char *text)
 * Direct callers: _config_load@0x00005fb8
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00005f3c_strtrim.s
 */
void strtrim(char *text)

{
  char cVar1;
  uint uVar2;
  char *pcVar3;
  
  uVar2 = strlen(text);
  for (pcVar3 = text + (uVar2 - 1);
      (text <= pcVar3 &&
      ((((cVar1 = *pcVar3, cVar1 == ' ' || (cVar1 == '\t')) || (cVar1 == '\n')) || (cVar1 == '\r')))
      ); pcVar3 = pcVar3 + -1) {
    *pcVar3 = '\0';
  }
  return;
}

/*
 * _config_load @ 0x00005fb8..0x00006820
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Read configuration properties and model-cache/profile state into the configuration record.
 * Parameters/return (inferred): int _config_load(RecoveredConfig *config)
 * Direct callers: _main@0x00005498
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; reads text from a stream; opens/creates/truncates filesystem paths according to the supplied mode; reads bytes from a stream; reads process environment
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/00005fb8_config_load.s
 */
int config_load(RecoveredConfig *config)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x8a0] __attribute__((aligned(16)));
#define local_888 (*((char (*)[8])(app_frame + 0x8))) /* original SP - 0x888 */
#define acStack_880 (*((char (*)[3])(app_frame + 0x10))) /* original SP - 0x880 */
#define acStack_87d (*((char (*)[2037])(app_frame + 0x13))) /* original SP - 0x87d */
#define acStack_88 (*((char (*)[80])(app_frame + 0x808))) /* original SP - 0x88 */

  char cVar1;
  bool bVar2;
  int iVar3;
  char *pcVar4;
  void *stream;
  uint uVar5;
  int iVar6;
  int iVar7;
  RecoveredProfile *output;
  char *text;
  double dVar8;

  pcVar4 = (char *)get_config_path();
  stream = fopen(pcVar4,"r");
  iVar3 = -1;
  if (stream != (void *)0x0) {
    bVar2 = false;
    while (pcVar4 = fgets(local_888,0x800,stream), pcVar4 != (char *)0x0) {
      pcVar4 = strchr(local_888,10);
      if (pcVar4 != (char *)0x0) {
        *pcVar4 = '\0';
      }
      pcVar4 = strchr(local_888,0xd);
      if (pcVar4 != (char *)0x0) {
        *pcVar4 = '\0';
      }
      if (((local_888[0] != '#') && (local_888[0] != '\0')) &&
         (pcVar4 = strchr(local_888,0x3d), pcVar4 != (char *)0x0)) {
        *pcVar4 = '\0';
        text = pcVar4 + 1;
        cVar1 = pcVar4[1];
        while (cVar1 == ' ') {
          text = text + 1;
          cVar1 = *text;
        }
        strtrim(local_888);
        strtrim(text);
        iVar3 = strcmp(local_888,"provider");
        if (iVar3 == 0) {
          iVar3 = provider_from_name(text);
          if (-1 < iVar3) {
            config->provider = iVar3;
          }
        }
        else {
          iVar3 = strcmp(local_888,"model");
          if (iVar3 == 0) {
            strncpy(config->model,text,0x7f);
            iVar3 = provider_detect_model_size(config->model);
            config->small_model_mode = iVar3;
          }
          else {
            iVar3 = strcmp(local_888,"api_key");
            if (iVar3 == 0) {
              strncpy(config->api_key,text,0xff);
            }
            else {
              iVar3 = strcmp(local_888,"openai_key");
              if ((iVar3 == 0) || (iVar3 = strcmp(local_888,"chatgpt_key"), iVar3 == 0)) {
                strncpy(config->openai_key,text,0xff);
              }
              else {
                iVar3 = strcmp(local_888,"anthropic_key");
                if ((iVar3 == 0) || (iVar3 = strcmp(local_888,"claude_key"), iVar3 == 0)) {
                  strncpy(config->anthropic_key,text,0xff);
                }
                else {
                  iVar3 = strcmp(local_888,"google_key");
                  if ((iVar3 == 0) || (iVar3 = strcmp(local_888,"gemini_key"), iVar3 == 0)) {
                    strncpy(config->google_key,text,0xff);
                  }
                  else {
                    iVar3 = strcmp(local_888,"ollama_key");
                    if (iVar3 == 0) {
                      strncpy(config->ollama_key,text,0xff);
                    }
                    else {
                      iVar3 = strcmp(local_888,"base_url");
                      if (iVar3 == 0) {
                        strncpy(config->base_url,text,0x1ff);
                      }
                      else {
                        iVar3 = strcmp(local_888,"ollama_url");
                        if (iVar3 == 0) {
                          strncpy(config->ollama_url,text,0x1ff);
                        }
                        else {
                          iVar3 = strcmp(local_888,"search_url");
                          if (iVar3 == 0) {
                            strncpy(config->search_url,text,0x1ff);
                          }
                          else {
                            iVar3 = strcmp(local_888,"max_tokens");
                            if (iVar3 == 0) {
                              iVar3 = atoi(text);
                              config->max_tokens = iVar3;
                              if (iVar3 < 1) {
                                config->max_tokens = 0x1000;
                              }
                            }
                            else {
                              iVar3 = strcmp(local_888,"context_window");
                              if (iVar3 == 0) {
                                iVar3 = atoi(text);
                                config->context_window = iVar3;
                              }
                              else {
                                iVar3 = strcmp(local_888,"temperature");
                                if (iVar3 == 0) {
                                  dVar8 = atof(text);
                                  config->temperature = (float)dVar8;
                                }
                                else {
                                  iVar3 = strcmp(local_888,"verbose");
                                  if (iVar3 == 0) {
                                    iVar3 = atoi(text);
                                    config->verbose = iVar3;
                                  }
                                  else {
                                    iVar3 = strcmp(local_888,"auto_approve");
                                    if (iVar3 == 0) {
                                      iVar3 = atoi(text);
                                      config->auto_approve = iVar3;
                                    }
                                    else {
                                      iVar3 = strcmp(local_888,"auto_compact");
                                      if (iVar3 == 0) {
                                        iVar3 = atoi(text);
                                        config->auto_compact = iVar3;
                                      }
                                      else {
                                        iVar3 = strcmp(local_888,"auto_save_session");
                                        if (iVar3 == 0) {
                                          iVar3 = atoi(text);
                                          config->auto_save_session = iVar3;
                                        }
                                        else {
                                          iVar3 = strcmp(local_888,"small_model_mode");
                                          if (iVar3 == 0) {
                                            iVar3 = atoi(text);
                                            config->small_model_mode = iVar3;
                                          }
                                          else {
                                            iVar3 = strcmp(local_888,"sidebar_depth");
                                            if (iVar3 == 0) {
                                              iVar3 = atoi(text);
                                              config->sidebar_depth = iVar3;
                                            }
                                            else {
                                              iVar3 = strcmp(local_888,"think_budget");
                                              if (iVar3 == 0) {
                                                iVar3 = atoi(text);
                                                config->think_budget = iVar3;
                                              }
                                              else {
                                                iVar3 = strcmp(local_888,"write_guard_enforce");
                                                if (iVar3 == 0) {
                                                  iVar3 = atoi(text);
                                                  config->write_guard_enforce = iVar3;
                                                }
                                                else {
                                                  iVar3 = strcmp(local_888,"effort");
                                                  if (iVar3 == 0) {
                                                    iVar3 = strcmp(text,"low");
                                                    if (iVar3 == 0) {
                                                      config->effort = 0;
                                                    }
                                                    else {
                                                      iVar3 = strcmp(text,"default");
                                                      if (iVar3 == 0) {
                                                        config->effort = 1;
                                                      }
                                                      else {
                                                        iVar3 = strcmp(text,"medium");
                                                        if (iVar3 == 0) {
                                                          config->effort = 2;
                                                        }
                                                        else {
                                                          iVar3 = strcmp(text,"high");
                                                          if (iVar3 == 0) {
                                                            config->effort = 3;
                                                          }
                                                          else {
                                                            iVar3 = strcmp(text,"max");
                                                            if (iVar3 == 0) {
                                                              config->effort = 4;
                                                            }
                                                          }
                                                        }
                                                      }
                                                    }
                                                  }
                                                  else {
                                                    iVar3 = strcmp(local_888,"theme");
                                                    if (iVar3 == 0) {
                                                      strncpy(config->theme,text,0x1f);
                                                    }
                                                    else {
                                                      iVar3 = strncmp(local_888,"mcp_server_",0xb);
                                                      if (iVar3 == 0) {
                                                        uVar5 = atoi(acStack_87d);
                                                        if (uVar5 < 4) {
                                                          strncpy(config->mcp_servers[uVar5],text,
                                                                   0x1ff);
                                                        }
                                                      }
                                                      else {
                                                        iVar3 = strcmp(local_888,"active_profile");
                                                        if (iVar3 == 0) {
                                                          strncpy(config->active_profile,text,0x3f)
                                                          ;
                                                        }
                                                        else {
                                                          iVar3 = strncmp(local_888,"profile_",8);
                                                          if (iVar3 == 0) {
                                                            pcVar4 = strchr(acStack_880,0x5f);
                                                            if ((pcVar4 != (char *)0x0) &&
                                                               (uVar5 = (int)pcVar4 -
                                                                        (int)acStack_880,
                                                               uVar5 - 1 < 0x3f)) {
                                                              memcpy(acStack_88,acStack_880,uVar5);
                                                              acStack_88[uVar5] = '\0';
                                                              pcVar4 = pcVar4 + 1;
                                                              iVar3 = config->profile_count;
                                                              if (0 < iVar3) {
                                                                iVar7 = 0;
                                                                do {
                                                                  iVar6 = strcmp((const char *)(0x1824 +
                                                                                  (int)(config->
                                                  active_profile + iVar7 * 0x3c8 + -4)),acStack_88);
                                                  if (iVar6 == 0) {
                                                    output = config->profiles + iVar7;
                                                    if (output != (RecoveredProfile *)0x0)
                                                    goto LAB_000066d4;
                                                    break;
                                                  }
                                                  iVar7 = iVar7 + 1;
                                                  } while (iVar3 != iVar7);
                                                  }
                                                  if (iVar3 < 8) {
                                                    output = config->profiles + iVar3;
                                                    config->profile_count = iVar3 + 1;
                                                    memset(output,0,0x3c8);
                                                    app_copy_padded(output->name,acStack_88,0x3f);
                                                    if (output != (RecoveredProfile *)0x0) {
LAB_000066d4:
                                                      iVar3 = strcmp(pcVar4,"provider");
                                                      if (iVar3 == 0) {
                                                        iVar3 = provider_from_name(text);
                                                        output->provider = iVar3;
                                                      }
                                                      else {
                                                        iVar3 = strcmp(pcVar4,"key");
                                                        if (iVar3 == 0) {
                                                          strncpy(output->api_key,text,0xff);
                                                        }
                                                        else {
                                                          iVar3 = strcmp(pcVar4,"model");
                                                          if (iVar3 == 0) {
                                                            strncpy(output->model,text,0x7f);
                                                          }
                                                          else {
                                                            iVar3 = strcmp(pcVar4,"url");
                                                            if (iVar3 == 0) {
                                                              strncpy(output->base_url,text,0x1ff);
                                                            }
                                                            else {
                                                              iVar3 = strcmp(pcVar4,
                                                  "context_window");
                                                  if (iVar3 == 0) {
                                                    iVar3 = atoi(text);
                                                    output->context_window = iVar3;
                                                  }
                                                  }
                                                  }
                                                  }
                                                  }
                                                  }
                                                  }
                                                  }
                                                  }
                                                  else {
                                                    iVar3 = strcmp(local_888,"system_prompt");
                                                    if (iVar3 == 0) {
                                                      if (!bVar2) {
                                                        ds_clear(&config->system_prompt);
                                                        bVar2 = true;
                                                      }
                                                      ds_append(&config->system_prompt,text);
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
    fclose(stream);
    model_cache_load();
    iVar3 = 0;
  }
  return iVar3;

#undef local_888
#undef acStack_880
#undef acStack_87d
#undef acStack_88
}

/*
 * _config_save @ 0x00006820..0x00006cfc
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Serialize populated configuration/profile properties, including credentials, to .ppccoderc.
 * Parameters/return (inferred): int _config_save(RecoveredConfig *config)
 * Direct callers: _handle_slash_command@0x0000362c, _handle_cmd@0x0001b004, _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; opens/creates/truncates filesystem paths according to the supplied mode; writes bytes to a stream; reads process environment
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/00006820_config_save.s
 */
int config_save(RecoveredConfig *config)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x40] __attribute__((aligned(16)));
#define local_30 (*((undefined4 *)(app_frame + 0x0))) /* original SP - 0x30 */
#define uStack_2c (*((undefined4 *)(app_frame + 0x4))) /* original SP - 0x2c */

  bool bVar1;
  int iVar2;
  char *pcVar3;
  void *stream;
  int *piVar4;
  char *pcVar5;
  int iVar6;
  RecoveredConfig *pRVar7;


  pcVar3 = (char *)get_config_path();
  stream = fopen(pcVar3,"w");
  if (stream == (void *)0x0) {
    piVar4 = __error();
    pcVar5 = strerror(*piVar4);
    fprintf((FILE * restrict)(PTR_000396a4 + 0xb0),"Error: Cannot write config to %s: %s\n",pcVar3,pcVar5);
    return -1;
  }
  fwrite("# PowerPC Code configuration\n",1,0x1d,stream);
  fwrite("# AI coding agent for Mac OS X Tiger PowerPC\n\n",1,0x2e,stream);
  pcVar3 = provider_name(config->provider);
  fprintf(stream,"provider = %s\n",pcVar3);
  fprintf(stream,"model = %s\n",config->model);
  if (config->api_key[0] != '\0') {
    fprintf(stream,"api_key = %s\n",config->api_key);
  }
  if (config->openai_key[0] != '\0') {
    fprintf(stream,"openai_key = %s\n",config->openai_key);
  }
  if (config->anthropic_key[0] != '\0') {
    fprintf(stream,"anthropic_key = %s\n",config->anthropic_key);
  }
  if (config->google_key[0] != '\0') {
    fprintf(stream,"google_key = %s\n",config->google_key);
  }
  if (config->ollama_key[0] != '\0') {
    fprintf(stream,"ollama_key = %s\n",config->ollama_key);
  }
  if (config->base_url[0] != '\0') {
    fprintf(stream,"base_url = %s\n",config->base_url);
  }
  if (config->ollama_url[0] != '\0') {
    fprintf(stream,"ollama_url = %s\n",config->ollama_url);
  }
  if (config->search_url[0] != '\0') {
    fprintf(stream,"search_url = %s\n",config->search_url);
  }
  fprintf(stream,"max_tokens = %d\n",config->max_tokens);
  fprintf(stream,"context_window = %d\n",config->context_window);
  local_30 = (undefined4)(APP_BITS((double)config->temperature) >> 0x20);
  uStack_2c = SUB84((double)config->temperature,0);
  fprintf(stream,"temperature = %.2f\n",(double)config->temperature);
  fprintf(stream,"verbose = %d\n",config->verbose);
  fprintf(stream,"auto_approve = %d\n",config->auto_approve);
  fprintf(stream,"auto_compact = %d\n",config->auto_compact);
  fprintf(stream,"auto_save_session = %d\n",config->auto_save_session);
  fprintf(stream,"small_model_mode = %d\n",config->small_model_mode);
  fprintf(stream,"sidebar_depth = %d\n",config->sidebar_depth);
  fprintf(stream,"think_budget = %d\n",config->think_budget);
  fprintf(stream,"write_guard_enforce = %d\n",config->write_guard_enforce);
  iVar2 = config->effort;
  if (iVar2 == 2) {
    pcVar3 = "medium";
    goto LAB_00006b34;
  }
  if (iVar2 < 3) {
    if (iVar2 == 0) {
      pcVar3 = "low";
      goto LAB_00006b34;
    }
  }
  else {
    if (iVar2 == 3) {
      pcVar3 = "high";
      goto LAB_00006b34;
    }
    if (iVar2 == 4) {
      pcVar3 = "max";
      goto LAB_00006b34;
    }
  }
  pcVar3 = "default";
LAB_00006b34:
  fprintf(stream,"effort = %s\n",pcVar3);
  if (config->theme[0] != '\0') {
    fprintf(stream,"theme = %s\n",config->theme);
  }
  iVar2 = 0;
  pRVar7 = config;
  do {
    if (pRVar7->mcp_servers[0][0] != '\0') {
      fprintf(stream,"mcp_server_%d = %s\n",iVar2,(char *)(config->mcp_servers + iVar2));
    }
    pRVar7 = (RecoveredConfig *)(pRVar7->openai_key + 0x3c);
    bVar1 = iVar2 != 3;
    iVar2 = iVar2 + 1;
  } while (bVar1);
  if (config->active_profile[0] != '\0') {
    fprintf(stream,"active_profile = %s\n",config->active_profile);
  }
  if (0 < config->profile_count) {
    iVar2 = 0;
    iVar6 = 0;
    do {
      pcVar5 = config->profiles[0].name + iVar6;
      pcVar3 = provider_name(*(int *)(pcVar5 + 0x40));
      fprintf(stream,"profile_%s_provider = %s\n",pcVar5,pcVar3);
      if (pcVar5[0xc4] != '\0') {
        fprintf(stream,"profile_%s_key = %s\n",pcVar5,pcVar5 + 0xc4);
      }
      if (pcVar5[0x44] != '\0') {
        fprintf(stream,"profile_%s_model = %s\n",pcVar5,pcVar5 + 0x44);
      }
      if (pcVar5[0x1c4] != '\0') {
        fprintf(stream,"profile_%s_url = %s\n",pcVar5,pcVar5 + 0x1c4);
      }
      if (0 < *(int *)(pcVar5 + 0x3c4)) {
        fprintf(stream,"profile_%s_context_window = %d\n",pcVar5,*(int *)(pcVar5 + 0x3c4));
      }
      iVar2 = iVar2 + 1;
      iVar6 = iVar6 + 0x3c8;
    } while (iVar2 < config->profile_count);
  }
  if (((config->system_prompt).data != (char *)0x0) && ((config->system_prompt).length != 0)) {
    pcVar3 = ds_cstr(&config->system_prompt);
    fprintf(stream,"system_prompt = %s\n",pcVar3);
  }
  fclose(stream);
  return 0;

#undef local_30
#undef uStack_2c
}

