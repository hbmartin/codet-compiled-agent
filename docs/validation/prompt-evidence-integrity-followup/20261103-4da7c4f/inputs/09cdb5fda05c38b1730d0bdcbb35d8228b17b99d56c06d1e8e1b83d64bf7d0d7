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
 * _usage @ 0x00003018..0x0000315c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Print supported CLI options, configuration/environment hints and slash commands.
 * Parameters/return (inferred): void _usage(char *program_name)
 * Direct callers: _main@0x00005498
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00003018_usage.s
 */
void usage(char *program_name)

{
  int iVar1;
  char *pcVar2;
  
  iVar1 = platform_is_mac();
  if (iVar1 == 0) {
    pcVar2 = "Unix-like systems";
  }
  else {
    pcVar2 = "Mac OS X";
  }
  printf("PowerPC Code v%s - AI coding agent for %s\n\n","0.8.0",pcVar2);
  printf("Platform: %s | Machine: %s\n\n",PTR__g_platform_name_0003969c,
                   PTR__g_platform_machine_000396a8);
  printf("Usage: %s [options]\n\n",program_name);
  puts("Options:");
  puts("  -p <provider>  Provider: ollama, openai, anthropic, google");
  puts("  -m <model>     Model name (default: gpt-4o-mini)");
  puts("  -k <key>       API key (fallback for all providers)");
  puts("  -u <url>       Base URL override");
  puts("  -d <dir>       Working directory (default: current)");
  puts("  -v             Verbose mode");
  puts("  -h             Show this help");
  puts("  --no-tui       Simple line mode (no ncurses)\n");
  puts("Environment variables:");
  puts("  OPENAI_API_KEY, ANTHROPIC_API_KEY, GOOGLE_API_KEY, OLLAMA_API_KEY\n");
  puts("Config file: ~/.ppccoderc");
  puts(
       "Commands: /help /model /provider /providers /profile /profiles /models /keys /key /url /save /clear"
       );
  puts(
       "          /save-session /load-session /compact /approve /git /context /paste /search-engine /effort"
       );
  puts("          /small-model /normal-model /undo /init /quit");
  return;
}

/*
 * _show_simple_http_error @ 0x0000315c..0x0000320c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Format provider HTTP errors for the non-curses interface.
 * Parameters/return (inferred): void _show_simple_http_error(int status,char *body)
 * Direct callers: _simple_mode@0x00004e30
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000315c_show_simple_http_error.s
 */
void show_simple_http_error(int status,char *body)

{
  RecoveredCJSON *object;
  RecoveredCJSON *pRVar1;
  
  printf("[ERROR] HTTP %d\n",status);
  if (body != (char *)0x0) {
    if (*body != '\0') {
      object = cJSON_Parse(body);
      if (object != (RecoveredCJSON *)0x0) {
        pRVar1 = cJSON_GetObjectItem(object,"error");
        if (pRVar1 != (RecoveredCJSON *)0x0) {
          pRVar1 = cJSON_GetObjectItem(pRVar1,"message");
          if ((pRVar1 != (RecoveredCJSON *)0x0) && (pRVar1->value_string != (char *)0x0)) {
            printf("  API: %s\n",pRVar1->value_string);
          }
        }
        cJSON_Delete(object);
      }
    }
  }
  return;
}

/*
 * _simple_ask_approve @ 0x0000320c..0x00003400
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Display a tool description and accept yes/no/always; always enables automatic approval for this session.
 * Parameters/return (inferred): int _simple_ask_approve(RecoveredAppState *app,RecoveredToolCall *call)
 * Direct callers: _simple_mode@0x00004e30
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; reads text from a stream
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000320c_simple_ask_approve.s
 */
int simple_ask_approve(RecoveredAppState *app,RecoveredToolCall *call)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x240] __attribute__((aligned(16)));
#define local_228 (*((char (*)[512])(app_frame + 0x8))) /* original SP - 0x228 */
#define local_28 (*((char (*)[28])(app_frame + 0x208))) /* original SP - 0x28 */

  undefined *stream;
  char *pcVar1;
  int iVar2;


  if ((app->config).auto_approve == 0) {
    local_228[0] = '\0';
    pcVar1 = ds_cstr(call->arguments);
    switch(call->tool_type) {
    case 0:
      snprintf(local_228,0x200,"read_file(%s)",pcVar1);
      break;
    case 1:
      snprintf(local_228,0x200,"write_file(%s)",pcVar1);
      break;
    case 2:
      snprintf(local_228,0x200,"edit_file(%s)",pcVar1);
      break;
    case 3:
      snprintf(local_228,0x200,"shell(%.80s)",pcVar1);
      break;
    case 4:
      snprintf(local_228,0x200,"glob(%s)",pcVar1);
      break;
    case 5:
      snprintf(local_228,0x200,"grep(%s)",pcVar1);
      break;
    case 6:
      snprintf(local_228,0x200,"search(%.80s)",pcVar1);
      break;
    case 0xb:
      snprintf(local_228,0x200,"read_and_edit(%s)",pcVar1);
    }
    printf("Run [%s]? (y/n/always): ",local_228);
    stream = (undefined *)(PTR_000396a4);
    fflush((FILE *)(PTR_000396a4 + 0x58));
    pcVar1 = fgets(local_28,8,(FILE *)(stream));
    if (pcVar1 != (char *)0x0) {
      if ((local_28[0] == 'y') || (local_28[0] == 'Y')) goto LAB_000033e0;
      if ((local_28[0] == 'a') || (local_28[0] == 'A')) {
        (app->config).auto_approve = 1;
        puts("Auto-approve enabled for this session.");
        return 1;
      }
    }
    iVar2 = 0;
  }
  else {
LAB_000033e0:
    iVar2 = 1;
  }
  return iVar2;

#undef local_228
#undef local_28
}

/*
 * _simple_git_status @ 0x00003400..0x000034a8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in cli: simple git status. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _simple_git_status(char *working_directory)
 * Direct callers: _handle_slash_command@0x0000362c
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; executes an argument vector through PATH; creates a same-account child process; reads a descriptor or IPC channel
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00003400_simple_git_status.s
 */
void simple_git_status(char *working_directory)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x50] __attribute__((aligned(16)));
#define local_38 (*((char * *)(app_frame + 0x8))) /* original SP - 0x38 */
#define local_34 (*((char * *)(app_frame + 0xc))) /* original SP - 0x34 */
#define local_30 (*((char * *)(app_frame + 0x10))) /* original SP - 0x30 */
#define local_2c (*((undefined4 *)(app_frame + 0x14))) /* original SP - 0x2c */
#define aRStack_28 (*((RecoveredDString (*)[2])(app_frame + 0x18))) /* original SP - 0x28 */

  int iVar1;
  char *pcVar2;

  local_38 = "git";
  local_34 = "status";
  local_30 = "--short";
  local_2c = 0;
  ds_init(aRStack_28);
  iVar1 = safe_exec_read(&local_38,working_directory,aRStack_28);
  if ((-1 < iVar1) && (aRStack_28[0].length != 0)) {
    pcVar2 = ds_cstr(aRStack_28);
    printf("\n[Git status]\n%s",pcVar2);
  }
  ds_free(aRStack_28);
  return;

#undef local_38
#undef local_34
#undef local_30
#undef local_2c
#undef aRStack_28
}

/*
 * _pick_model_simple @ 0x000034a8..0x0000362c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in cli: pick model simple. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _pick_model_simple(char *param_1,char *param_2,int param_3)
 * Direct callers: _handle_slash_command@0x0000362c
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; reads text from a stream
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000034a8_pick_model_simple.s
 */
int pick_model_simple(char *param_1,char *param_2,int param_3)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x60] __attribute__((aligned(16)));
#define acStack_48 (*((char (*)[32])(app_frame + 0x8))) /* original SP - 0x48 */

  bool bVar1;
  undefined *puVar2;
  undefined **ppuVar3;
  int iVar4;
  char *pcVar5;
  int iVar6;
  int iVar7;
  byte bVar8;

  if (_ollama_models != NULL) {
    iVar7 = 0;
    ppuVar3 = (undefined **)(&APP_PTR_00039018);
    do {
      iVar7 = iVar7 + 1;
      puVar2 = *ppuVar3;
      ppuVar3 = ppuVar3 + 1;
    } while (puVar2 != (undefined *)0x0);
    bVar8 = (0 < iVar7) << 2;
    if (iVar7 != 0) {
      puts("Models:");
      if ((bool)(bVar8 >> 2 & 1)) {
        iVar6 = 0;
        ppuVar3 = (undefined **)(&_ollama_models);
        bVar8 = (param_1 == (char *)0x0) << 1;
        do {
          if ((bool)(bVar8 >> 1 & 1)) {
LAB_00003554:
            bVar1 = false;
          }
          else {
            iVar4 = strcmp((const char *)(*ppuVar3),param_1);
            bVar1 = true;
            if (iVar4 != 0) goto LAB_00003554;
          }
          iVar6 = iVar6 + 1;
          if (bVar1) {
            pcVar5 = " [active]";
          }
          else {
            pcVar5 = "";
          }
          printf("  %2d. %s%s\n",iVar6,*ppuVar3,pcVar5);
          ppuVar3 = ppuVar3 + 1;
        } while (iVar6 != iVar7);
      }
      printf("Pick 1-%d (0 to cancel): ",iVar7);
      puVar2 = (undefined *)(PTR_000396a4);
      fflush((FILE *)(PTR_000396a4 + 0x58));
      pcVar5 = fgets(acStack_48,8,(FILE *)(puVar2));
      if (pcVar5 != (char *)0x0) {
        iVar6 = atoi(acStack_48);
        iVar6 = iVar6 + -1;
        if ((-1 < iVar6) && (iVar6 < iVar7)) {
          strncpy(param_2,(&_ollama_models)[iVar6],param_3 - 1);
          param_2[param_3 + -1] = '\0';
          return iVar6;
        }
      }
    }
  }
  return -1;

#undef acStack_48
}

/*
 * _handle_slash_command @ 0x0000362c..0x00004e30
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Dispatch simple-mode slash commands, including configuration, sessions, reference search and explicit project initialization.
 * Parameters/return (inferred): undefined4 _handle_slash_command(undefined4 *param_1,char *param_2)
 * Direct callers: _simple_mode@0x00004e30
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; performs network/protocol I/O; executes an argument vector through PATH; reads text from a stream; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; reads bytes from a stream; writes bytes to a stream; reads process environment; creates directories; opens a directory for enumeration; reads a descriptor or IPC channel; reads directory entries; reads filesystem metadata; removes a filesystem entry
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Refined type propagation failed; the earlier C view remains primary and the new warning-bearing view is retained separately.
 * Exact assembly: assembly/functions/0000362c_handle_slash_command.s
 * Uncertainty: Second-pass structure propagation did not settle; primary C retains the earlier less-typed view with the same exact assembly evidence.
 */
undefined4 handle_slash_command(undefined4 *param_1,char *param_2)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x1fff0] __attribute__((aligned(16)));
#define iStack_1ffd8 (*((int *)(app_frame + 0x8))) /* original SP - 0x1ffd8 */
#define RStack_1ffd4 (*((RecoveredDString *)(app_frame + 0xc))) /* original SP - 0x1ffd4 */
#define apcStack_1ffc8 (*((char * (*)[32])(app_frame + 0x18))) /* original SP - 0x1ffc8 */
#define acStack_1ff48 (*((char (*)[256])(app_frame + 0x98))) /* original SP - 0x1ff48 */
#define acStack_1fe48 (*((char (*)[1023])(app_frame + 0x198))) /* original SP - 0x1fe48 */
#define uStack_1fa49 (*((undefined1 *)(app_frame + 0x597))) /* original SP - 0x1fa49 */
#define RStack_1fa48 (*((RecoveredResponse *)(app_frame + 0x598))) /* original SP - 0x1fa48 */

  char cVar1;
  bool bVar2;
  undefined *puVar3;
  int iVar4;
  char *pcVar5;
  int iVar6;
  uint uVar7;
  int iVar8;
  undefined4 *puVar9;
  RecoveredMessage *pointer;
  char *pcVar10;
  RecoveredResponse *pRVar11;
  char *pcVar12;
  char *pcVar13;
  undefined4 *puVar14;
  RecoveredDString *str;
  char *pcVar15;
  RecoveredDString *str_00;
  RecoveredMessage *pRVar16;
  FILE *pFVar17;
  int iVar18;
  RecoveredMessage *pRVar19;
  char **ppcVar20;

  iVar4 = strcmp(param_2,"/paste");
  if (iVar4 == 0) {
    puts("Paste mode: Enter lines, type \'.\' on blank line to finish.");
    *param_2 = '\0';
    puVar3 = (undefined *)(PTR_000396a4);
    iVar4 = 0;
    while( true ) {
      printf("+ ");
      fflush((FILE *)(puVar3 + 0x58));
      pcVar5 = fgets((char *)&RStack_1fa48,0x2000,(FILE *)((RecoveredDString *)puVar3));
      if (pcVar5 == (char *)0x0) break;
      pcVar5 = strchr((char *)&RStack_1fa48,10);
      if (pcVar5 != (char *)0x0) {
        *pcVar5 = '\0';
      }
      iVar6 = strcmp((char *)&RStack_1fa48,".");
      if (iVar6 == 0) goto LAB_000036bc;
      if (iVar4 < 1) {
        pcVar5 = "";
      }
      else {
        pcVar5 = "\n";
      }
      iVar6 = snprintf(param_2 + iVar4,0x3ffff - iVar4,"%s%s",pcVar5,(char *)(&RStack_1fa48));
      if (0 < iVar6) {
        iVar4 = iVar4 + iVar6;
      }
    }
    *param_2 = '\0';
LAB_000036bc:
    if (*param_2 == '\0') {
      return 1;
    }
  }
  else if (*param_2 == '/') {
    iVar4 = strcmp(param_2,"/quit");
    if ((iVar4 == 0) || (iVar4 = strcmp(param_2,"/q"), iVar4 == 0)) {
      *param_1 = 0;
      return 1;
    }
    iVar4 = strcmp(param_2,"/help");
    if ((iVar4 == 0) || (iVar4 = strcmp(param_2,"/h"), iVar4 == 0)) {
      puts("Commands: /quit /help /model /provider /models /keys /key /url");
      puts("         /save /save-session /load-session /compact /approve");
      puts("         /git /context /paste /clear /config /undo /init");
      puts("         /small-model /normal-model");
      return 1;
    }
    iVar4 = strncmp(param_2,"/model ",7);
    if (iVar4 == 0) {
      pcVar15 = (char *)(param_1 + 0x2295);
      strncpy(pcVar15,param_2 + 7,0x7f);
      uVar7 = strlen(pcVar15);
      for (pcVar5 = pcVar15 + (uVar7 - 1);
          (pcVar15 <= pcVar5 &&
          ((((cVar1 = *pcVar5, cVar1 == ' ' || (cVar1 == '\t')) || (cVar1 == '\r')) ||
           (cVar1 == '\n')))); pcVar5 = pcVar5 + -1) {
        *pcVar5 = '\0';
      }
      printf("Model: %s\n",pcVar15);
      config_save((RecoveredConfig *)(param_1 + 0x2284));
      return 1;
    }
    iVar4 = strcmp(param_2,"/model");
    if (iVar4 == 0) {
      memcpy(apcStack_1ffc8,&_C_68_4346,0x80);
      pcVar5 = (char *)(param_1 + 0x2295);
      iVar4 = pick_model_simple(pcVar5,(char *)(apcStack_1ffc8),0x80);
      if (iVar4 < 0) {
        return 1;
      }
      app_copy_padded(pcVar5,(char *)apcStack_1ffc8,0x7f);
      printf("Model: %s\n",pcVar5);
      config_save((RecoveredConfig *)(param_1 + 0x2284));
      return 1;
    }
    iVar4 = strcmp(param_2,"/provider");
    if (((iVar4 != 0) && (iVar4 = strcmp(param_2,"/providers"), iVar4 != 0)) &&
       ((iVar4 = strcmp(param_2,"/profile"), iVar4 != 0 &&
        (iVar4 = strcmp(param_2,"/profiles"), iVar4 != 0)))) {
      iVar4 = strcmp(param_2,"/models");
      if (iVar4 == 0) {
        puts("Models:");
        puts("  OpenAI:    gpt-4o, gpt-4o-mini, gpt-4-turbo");
        puts("  Anthropic: claude-sonnet-4-20250514, claude-3-5-sonnet");
        puts("  Google:    gemini-2.0-flash, gemini-1.5-pro");
        puts("  Ollama:    list from local server");
        return 1;
      }
      iVar4 = strcmp(param_2,"/keys");
      if (iVar4 == 0) {
        if (*(char *)(param_1 + 0x22f5) == '\0') {
          pcVar5 = "(not set)";
        }
        else {
          pcVar5 = "***set***";
        }
        if (*(char *)(param_1 + 0x2335) == '\0') {
          pcVar15 = "(not set)";
        }
        else {
          pcVar15 = "***set***";
        }
        if (*(char *)(param_1 + 0x2375) == '\0') {
          pcVar10 = "(not set)";
        }
        else {
          pcVar10 = "***set***";
        }
        if (*(char *)(param_1 + 0x23b5) == '\0') {
          pcVar12 = "(not set)";
        }
        else {
          pcVar12 = "***set***";
        }
        if (*(char *)(param_1 + 0x22b5) == '\0') {
          pcVar13 = "(not set)";
        }
        else {
          pcVar13 = "***set***";
        }
        printf("Keys: openai=%s anthropic=%s google=%s ollama=%s fallback=%s\n",pcVar5,
                         pcVar15,pcVar10,pcVar12,pcVar13);
        return 1;
      }
      iVar4 = strncmp(param_2,"/key ",5);
      if (iVar4 == 0) {
        iVar4 = sscanf(param_2 + 5,"%31s %255s",(char *)(apcStack_1ffc8),acStack_1fe48);
        if (iVar4 != 2) {
          puts("Usage: /key <provider> <key>");
          return 1;
        }
        iVar4 = provider_from_name((char *)apcStack_1ffc8);
        if (iVar4 == 1) {
          app_copy_padded((char *)(param_1 + 0x22f5),acStack_1fe48,0xff);
          puts("OpenAI key set.");
        }
        else if (iVar4 == 2) {
          app_copy_padded((char *)(param_1 + 0x2335),acStack_1fe48,0xff);
          puts("Anthropic key set.");
        }
        else if (iVar4 == 3) {
          app_copy_padded((char *)(param_1 + 0x2375),acStack_1fe48,0xff);
          puts("Google key set.");
        }
        else if (iVar4 == 0) {
          app_copy_padded((char *)(param_1 + 0x23b5),acStack_1fe48,0xff);
          puts("Ollama key set.");
        }
        else {
          puts("Unknown provider.");
        }
        config_save((RecoveredConfig *)(param_1 + 0x2284));
        return 1;
      }
      iVar4 = strncmp(param_2,"/url ",5);
      if (iVar4 == 0) {
        iVar4 = sscanf(param_2 + 5,"%31s %511s",(char *)(apcStack_1ffc8),acStack_1fe48);
        if (iVar4 != 2) {
          return 1;
        }
        iVar4 = provider_from_name((char *)apcStack_1ffc8);
        if (iVar4 == 0) {
          app_copy_padded((char *)(param_1 + 0x2475),acStack_1fe48,0x1ff);
          printf("Ollama URL: %s\n",acStack_1fe48);
        }
        else {
          app_copy_padded((char *)(param_1 + 0x23f5),acStack_1fe48,0x1ff);
          printf("Base URL: %s\n",acStack_1fe48);
        }
        config_save((RecoveredConfig *)(param_1 + 0x2284));
        return 1;
      }
      iVar4 = strncmp(param_2,"/read full ",0xb);
      if (iVar4 == 0) {
        if ((param_2[0xb] == '/') || (param_2[0xb] == '~')) {
          strncpy(acStack_1fe48,param_2 + 0xb,0x3ff);
          uStack_1fa49 = 0;
        }
        else {
          snprintf(acStack_1fe48,0x400,"%s/%s",(char *)(param_1 + 0x2575),param_2 + 0xb);
        }
        ds_init(&RStack_1ffd4);
        iVar4 = open(acStack_1fe48,0);
        if (-1 < iVar4) {
          while (uVar7 = read(iVar4,&RStack_1fa48,0x1000), 0 < (int)uVar7) {
            ds_appendn(&RStack_1ffd4,(char *)&RStack_1fa48,uVar7);
          }
          close(iVar4);
        }
        if (RStack_1ffd4.length == 0) {
          printf("Could not read %s\n",acStack_1fe48);
        }
        else {
          iVar4 = param_1[0x2282];
          puVar14 = param_1 + iVar4 * 0x17 + 2;
          param_1[0x2282] = iVar4 + 1;
          memset(puVar14,0,0x5c);
          param_1[iVar4 * 0x17 + 2] = 1;
          ds_init((RecoveredDString *)(puVar14 + 1));
          pcVar5 = ds_cstr(&RStack_1ffd4);
          ds_appendf((RecoveredDString *)(puVar14 + 1),"Full contents of %s:\n%s",acStack_1fe48,
                      pcVar5);
          pcVar5 = ds_cstr(&RStack_1ffd4);
          iVar4 = 0;
          if (RStack_1ffd4.length != 0) {
            iVar6 = 0;
            iVar4 = 0;
            uVar7 = RStack_1ffd4.length;
            do {
              if (pcVar5[iVar6] == '\n') {
                iVar4 = iVar4 + 1;
              }
              iVar6 = iVar6 + 1;
              uVar7 = uVar7 - 1;
            } while (uVar7 != 0);
          }
          printf("Loaded %zu bytes (%d lines) from %s into context.\n",(size_t)(RStack_1ffd4.length)
                           ,iVar4,acStack_1fe48);
          file_ctx_update((RecoveredAppState *)(param_1),acStack_1fe48,RStack_1ffd4.length,iVar4,iVar4,0);
        }
        ds_free(&RStack_1ffd4);
        return 1;
      }
      iVar4 = strcmp(param_2,"/clear");
      if (iVar4 == 0) {
        if (0 < (int)param_1[0x2282]) {
          iVar4 = 0;
          do {
            message_free((RecoveredMessage *)(param_1 + iVar4 * 0x17 + 2));
            iVar4 = iVar4 + 1;
          } while (iVar4 < (int)param_1[0x2282]);
        }
        param_1[0x2282] = 0;
        param_1[0x2283] = 0;
        trust_reset((RecoveredAppState *)(param_1));
        cache_clear((RecoveredAppState *)(param_1));
        puts("Cleared.");
        return 1;
      }
      iVar4 = strcmp(param_2,"/small-model");
      if (iVar4 == 0) {
        param_1[0x267f] = 1;
        puts("Small model mode enabled.");
        return 1;
      }
      iVar4 = strcmp(param_2,"/normal-model");
      if (iVar4 == 0) {
        param_1[0x267f] = 0;
        puts("Normal model mode enabled.");
        return 1;
      }
      iVar4 = strcmp(param_2,"/config");
      if (iVar4 == 0) {
        pcVar5 = provider_name(param_1[0x2284]);
        pcVar15 = config_get_url((RecoveredConfig *)(param_1 + 0x2284));
        if (param_1[0x267c] == 0) {
          pcVar10 = "no";
        }
        else {
          pcVar10 = "yes";
        }
        printf("Provider: %s\nModel: %s\nURL: %s\nMaxTokens: %d\nDir: %s\nAuto-approve: %s\n"
                         ,pcVar5,(char *)(param_1 + 0x2295),pcVar15,param_1[0x2678],(char *)(param_1 + 0x2575),pcVar10);
        return 1;
      }
      iVar4 = strcmp(param_2,"/save");
      if (iVar4 == 0) {
        iVar4 = config_save((RecoveredConfig *)(param_1 + 0x2284));
        if (iVar4 == 0) {
          pcVar5 = "Config saved.\n";
        }
        else {
          pcVar5 = "Could not save config.\n";
        }
        printf("%s",pcVar5); /* Both branches select fixed, percent-free messages. */
        return 1;
      }
      iVar4 = strncmp(param_2,"/save-session",0xd);
      if (iVar4 == 0) {
        pcVar5 = param_2 + 0xe;
        if (param_2[0xd] != ' ') {
          pcVar5 = "session";
        }
        session_get_path(acStack_1fe48,0x400,pcVar5);
        iVar4 = session_save((RecoveredConversation *)(param_1 + 2),acStack_1fe48,(char *)(param_1 + 0x2575));
        if (iVar4 != 0) {
          puts("Could not save session.");
          return 1;
        }
        puts("Session saved.");
        return 1;
      }
      iVar4 = strncmp(param_2,"/load-session",0xd);
      if (iVar4 == 0) {
        pcVar5 = param_2 + 0xe;
        if (param_2[0xd] != ' ') {
          pcVar5 = "session";
        }
        session_get_path(acStack_1fe48,0x400,pcVar5);
        iVar4 = session_load((RecoveredConversation *)(param_1 + 2),acStack_1fe48,(char *)(param_1 + 0x2575));
        if (iVar4 != 0) {
          if (iVar4 != -2) {
            puts("Could not load session.");
            return 1;
          }
          puts("Session from different directory. Starting fresh.");
          unlink(acStack_1fe48);
          return 1;
        }
        printf("Session loaded (%d messages).\n",param_1[0x2282]);
        return 1;
      }
      iVar4 = strcmp(param_2,"/compact");
      if (iVar4 == 0) {
        iVar4 = param_1[0x2282];
        if (iVar4 < 2) {
          puts("Nothing to compact.");
          return 1;
        }
        pointer = malloc(iVar4 * 0x5c);
        if (pointer != (RecoveredMessage *)0x0) {
          iVar6 = 0;
          iVar18 = 0;
          pRVar16 = (RecoveredMessage *)(param_1 + 2);
          do {
            message_copy((RecoveredMessage *)(pointer->call_id + iVar18 + -0x1c),
                          (RecoveredMessage *)(pRVar16->call_id + iVar18 + -0x1c));
            message_free((RecoveredMessage *)(pRVar16->call_id + iVar18 + -0x1c));
            iVar6 = iVar6 + 1;
            iVar18 = iVar18 + 0x5c;
          } while (iVar4 != iVar6);
          param_1[0x2282] = 1;
          memset(pRVar16,0,0x5c);
          param_1[2] = 0;
          str = (RecoveredDString *)(param_1 + 3);
          ds_init(str);
          ds_append(str,
                     "Summarize the previous conversation into a concise summary. Capture key decisions, code changes, and remaining tasks."
                    );
          iVar6 = param_1[0x2282];
          param_1[0x2282] = iVar6 + 1;
          memset(pRVar16 + iVar6,0,0x5c);
          pRVar16[iVar6].role = 1;
          str_00 = &pRVar16[iVar6].content;
          ds_init(str_00);
          iVar6 = 0;
          pRVar19 = pointer;
          do {
            iVar18 = pRVar19->role;
            pcVar5 = ds_cstr(&pRVar19->content);
            ds_appendf(str_00,"[%d] %s\n",iVar18,pcVar5);
            iVar6 = iVar6 + 1;
            pRVar19 = pRVar19 + 1;
          } while (iVar4 != iVar6);
          memset(&RStack_1fa48,0,0x1fa0c);
          iVar6 = provider_send((RecoveredConfig *)(param_1 + 0x2284),
                                 (RecoveredConversation *)(param_1 + 2),&RStack_1fa48,
                                 (RecoveredStreamCallback)0x0,(void *)0x0,(void *)0x0,(void *)0x0);
          if ((iVar6 - 200U < 100) && (RStack_1fa48.content.length != 0)) {
            param_1[0x2282] = 1;
            memset(pRVar16,0,0x5c);
            pRVar16->role = 0;
            ds_init(str);
            pcVar5 = ds_cstr(&RStack_1fa48.content);
            ds_appendf(str,"Previous conversation summary: %s",pcVar5);
            puts("Compacted to summary.");
          }
          else {
            param_1[0x2282] = 0;
            iVar18 = 0;
            pRVar19 = pointer;
            do {
              message_copy(pRVar16,pRVar19);
              iVar18 = iVar18 + 1;
              pRVar16 = pRVar16 + 1;
              pRVar19 = pRVar19 + 1;
            } while (iVar4 != iVar18);
            param_1[0x2282] = iVar4;
            printf("Compact failed (HTTP %d). Conversation unchanged.\n",iVar6);
          }
          iVar6 = 0;
          pRVar16 = pointer;
          do {
            message_free(pRVar16);
            iVar6 = iVar6 + 1;
            pRVar16 = pRVar16 + 1;
          } while (iVar4 != iVar6);
          free(pointer);
          return 1;
        }
        puts("Out of memory.");
        return 1;
      }
      iVar4 = strcmp(param_2,"/approve");
      if (iVar4 == 0) {
        param_1[0x267c] = (uint)(param_1[0x267c] == 0);
        config_save((RecoveredConfig *)(param_1 + 0x2284));
        if (param_1[0x267c] == 0) {
          pcVar5 = "disabled";
        }
        else {
          pcVar5 = "enabled";
        }
        printf("Auto-approve: %s\n",pcVar5);
        return 1;
      }
      iVar4 = strcmp(param_2,"/git");
      if (iVar4 == 0) {
        simple_git_status((char *)(param_1 + 0x2575));
        return 1;
      }
      iVar4 = strcmp(param_2,"/context");
      if (iVar4 == 0) {
        context_build((RecoveredDString *)(param_1 + 0x203025),(char *)(param_1 + 0x2575));
        param_1[0x203028] = 1;
        printf("Project context loaded (%lu bytes).\n",(long unsigned int)(param_1[0x203026]));
        return 1;
      }
      iVar4 = strcmp(param_2,"/undo");
      if (iVar4 != 0) {
        iVar4 = strcmp(param_2,"/init");
        if (iVar4 == 0) {
          iVar4 = init_project((RecoveredAppState *)(param_1));
          if (iVar4 != 0) {
            puts("Failed to initialize project.");
            return 1;
          }
          pcVar5 = (char *)init_project_report();
          puts(pcVar5);
          return 1;
        }
        iVar4 = strcmp(param_2,"/search-engine");
        if (iVar4 == 0) {
          puts("Search Engine Selection:");
          if (*(char *)(param_1 + 0x2475) == '\0') {
            pcVar5 = "";
          }
          else {
            pcVar5 = " [active]";
          }
          printf("  1. Ollama Cloud/Local API%s\n",pcVar5);
          if (*(char *)(param_1 + 0x2475) == '\0') {
            pcVar5 = " [active]";
          }
          else {
            pcVar5 = "";
          }
          printf("  2. Native (DuckDuckGo + Strip)%s\n",pcVar5);
          printf("Pick 1-2 (0 to cancel): ");
          puVar3 = (undefined *)(PTR_000396a4);
          pFVar17 = (FILE *)(PTR_000396a4 + 0x58);
          fflush(pFVar17);
          pcVar5 = fgets((char *)&RStack_1ffd4,8,(FILE *)((RecoveredDString *)puVar3));
          if (pcVar5 == (char *)0x0) {
            return 1;
          }
          iVar4 = atoi((char *)&RStack_1ffd4);
          if (iVar4 == 1) {
            printf("Enter Ollama Search URL (blank for default cloud): ");
            fflush(pFVar17);
            pcVar5 = fgets(acStack_1ff48,0x100,(FILE *)((RecoveredDString *)puVar3));
            if (pcVar5 != (char *)0x0) {
              pcVar5 = strchr(acStack_1ff48,10);
              if (pcVar5 != (char *)0x0) {
                *pcVar5 = '\0';
              }
              if (acStack_1ff48[0] == '\0') {
                strncpy((char *)(param_1 + 0x2475),"https://api.ollama.com",0x1ff);
              }
              else {
                app_copy_padded((char *)(param_1 + 0x2475),acStack_1ff48,0x1ff);
              }
              printf("Ollama search enabled: %s\n",(char *)(param_1 + 0x2475));
            }
          }
          else if (iVar4 == 2) {
            *(undefined1 *)(param_1 + 0x2475) = 0;
            puts("Native stripped search enabled.");
          }
          config_save((RecoveredConfig *)(param_1 + 0x2284));
          return 1;
        }
        printf("Unknown command: %s (type /help)\n",param_2);
        return 1;
      }
      if ((int)param_1[0x2282] < 2) {
        puts("Nothing to undo.");
        return 1;
      }
      message_free((RecoveredMessage *)(param_1 + param_1[0x2282] * 0x17 + -0x15));
      message_free((RecoveredMessage *)(param_1 + param_1[0x2282] * 0x17 + -0x2c));
      param_1[0x2282] = param_1[0x2282] + -2;
      puts("Undone.");
      return 1;
    }
    puts("Provider Manager:");
    puts("  1. Manage / Switch Profiles");
    puts("  2. Create New Profile");
    puts("  3. Quick Start (No Profile)");
    printf("Pick 1-3 (0 to cancel): ");
    puVar3 = (undefined *)(PTR_000396a4);
    fflush((FILE *)(PTR_000396a4 + 0x58));
    pcVar5 = fgets((char *)&RStack_1ffd4,8,(FILE *)((RecoveredDString *)puVar3));
    if (pcVar5 == (char *)0x0) {
      return 1;
    }
    iVar4 = atoi((char *)&RStack_1ffd4);
    if (iVar4 == 1) {
      iStack_1ffd8 = 0;
      puVar14 = param_1 + 0x2284;
      profile_list((RecoveredConfig *)(puVar14),apcStack_1ffc8,8,&iStack_1ffd8);
      if (iStack_1ffd8 == 0) {
        puts("No profiles yet.");
        return 1;
      }
      puts("Select Profile:");
      if (0 < iStack_1ffd8) {
        iVar4 = 0;
        ppcVar20 = apcStack_1ffc8;
        do {
          iVar4 = iVar4 + 1;
          printf("  %d. %s\n",iVar4,*ppcVar20);
          ppcVar20 = ppcVar20 + 1;
        } while (iVar4 < iStack_1ffd8);
      }
      printf("Pick 1-%d: ",iStack_1ffd8);
      puVar3 = (undefined *)(PTR_000396a4);
      pFVar17 = (FILE *)(PTR_000396a4 + 0x58);
      fflush(pFVar17);
      pcVar5 = fgets((char *)&RStack_1ffd4,8,(FILE *)((RecoveredDString *)puVar3));
      if (pcVar5 != (char *)0x0) {
        iVar4 = atoi((char *)&RStack_1ffd4);
        iVar4 = iVar4 + -1;
        if ((-1 < iVar4) && (iVar4 < iStack_1ffd8)) {
          pcVar15 = apcStack_1ffc8[iVar4];
          printf("Manage \'%s\':\n",pcVar15);
          puts("  1. Activate\n  2. Edit Model\n  3. Edit API Key\n  4. Edit URL\n  5. Delete");
          printf("Pick 1-5: ");
          fflush(pFVar17);
          pcVar5 = fgets((char *)&RStack_1ffd4,8,(FILE *)((RecoveredDString *)puVar3));
          if (pcVar5 != (char *)0x0) {
            iVar4 = atoi((char *)&RStack_1ffd4);
            if (iVar4 == 1) {
              profile_select((RecoveredConfig *)(puVar14),pcVar15);
              puts("Activated.");
            }
            else if (iVar4 - 2U < 3) {
              printf("Enter New Value: ");
              puVar3 = (undefined *)(PTR_000396a4);
              fflush((FILE *)(PTR_000396a4 + 0x58));
              pcVar5 = fgets(acStack_1ff48,0x100,(FILE *)((RecoveredDString *)puVar3));
              if (pcVar5 != (char *)0x0) {
                pcVar5 = strchr(acStack_1ff48,10);
                if (pcVar5 != (char *)0x0) {
                  *pcVar5 = '\0';
                }
                iVar6 = param_1[0x301d];
                if (0 < iVar6) {
                  iVar18 = 0;
                  do {
                    iVar8 = strcmp((char *)(param_1 + iVar18 * 0xf2 + 0x288d),pcVar15);
                    if (iVar8 == 0) {
                      puVar9 = param_1 + iVar18 * 0xf2 + 0x288d;
                      if ((puVar9 != (undefined4 *)0x0) && (acStack_1ff48[0] != '\0')) {
                        if (iVar4 == 2) {
                          app_copy_padded((char *)(puVar9 + 0x11),acStack_1ff48,0x7f);
                        }
                        else if (iVar4 == 3) {
                          app_copy_padded((char *)(puVar9 + 0x31),acStack_1ff48,0xff);
                        }
                        else if (iVar4 == 4) {
                          app_copy_padded((char *)(puVar9 + 0x71),acStack_1ff48,0x1ff);
                        }
                        iVar4 = strcmp((char *)(param_1 + 0x2285),pcVar15);
                        if (iVar4 == 0) {
                          profile_select((RecoveredConfig *)(puVar14),pcVar15);
                        }
                        puts("Updated.");
                      }
                      break;
                    }
                    iVar18 = iVar18 + 1;
                  } while (iVar18 != iVar6);
                }
              }
            }
            else if (iVar4 == 5) {
              profile_remove((RecoveredConfig *)(puVar14),pcVar15);
              puts("Deleted.");
            }
          }
        }
      }
    }
    else if (iVar4 == 2) {
      printf("Provider Type (openai, anthropic, google, ollama-cloud, ollama-local): ");
      puVar3 = (undefined *)(PTR_000396a4);
      fflush((FILE *)(PTR_000396a4 + 0x58));
      pcVar5 = fgets((char *)apcStack_1ffc8,0x40,(FILE *)((RecoveredDString *)puVar3));
      if (pcVar5 != (char *)0x0) {
        pcVar5 = strchr((char *)apcStack_1ffc8,10);
        if (pcVar5 != (char *)0x0) {
          *pcVar5 = '\0';
        }
        iVar4 = strcmp((char *)apcStack_1ffc8,"ollama-cloud");
        if (iVar4 == 0) {
          iVar4 = 0;
          bVar2 = true;
        }
        else {
          iVar4 = strcmp((char *)apcStack_1ffc8,"ollama-local");
          if (iVar4 == 0) {
            iVar4 = 0;
            bVar2 = false;
          }
          else {
            iVar4 = provider_from_name((char *)apcStack_1ffc8);
            if (iVar4 < 0) {
              puts("Invalid type.");
              return 1;
            }
            bVar2 = false;
          }
        }
        printf("Profile Name: ");
        puVar3 = (undefined *)(PTR_000396a4);
        fflush((FILE *)(PTR_000396a4 + 0x58));
        pcVar5 = fgets((char *)apcStack_1ffc8,0x40,(FILE *)((RecoveredDString *)puVar3));
        if (pcVar5 != (char *)0x0) {
          pcVar5 = strchr((char *)apcStack_1ffc8,10);
          if (pcVar5 != (char *)0x0) {
            *pcVar5 = '\0';
          }
          printf("Model: ");
          puVar3 = (undefined *)(PTR_000396a4);
          fflush((FILE *)(PTR_000396a4 + 0x58));
          fgets(acStack_1ff48,0x80,(FILE *)((RecoveredDString *)puVar3));
          pcVar5 = strchr(acStack_1ff48,10);
          if (pcVar5 != (char *)0x0) {
            *pcVar5 = '\0';
          }
          if (bVar2) {
            strncpy(acStack_1fe48,"https://api.ollama.com",0xff);
            printf("Ollama Cloud API Key: ");
          }
          else {
            printf("API Key: ");
          }
          puVar3 = (undefined *)(PTR_000396a4);
          fflush((FILE *)(PTR_000396a4 + 0x58));
          fgets((char *)&RStack_1fa48,0x100,(FILE *)((RecoveredDString *)puVar3));
          pcVar5 = strchr((char *)&RStack_1fa48,10);
          if (pcVar5 != (char *)0x0) {
            *pcVar5 = '\0';
          }
          if (!bVar2) {
            printf("Base URL: ");
            puVar3 = (undefined *)(PTR_000396a4);
            fflush((FILE *)(PTR_000396a4 + 0x58));
            fgets(acStack_1fe48,0x100,(FILE *)((RecoveredDString *)puVar3));
            pcVar5 = strchr(acStack_1fe48,10);
            if (pcVar5 != (char *)0x0) {
              *pcVar5 = '\0';
            }
          }
          puVar14 = param_1 + 0x2284;
          pcVar5 = acStack_1ff48;
          if (acStack_1ff48[0] == '\0') {
            pcVar5 = (char *)0x0;
          }
          pRVar11 = &RStack_1fa48;
          if (APP_FIELD8(RStack_1fa48.content.data, 0) == '\0') {
            pRVar11 = (RecoveredResponse *)0x0;
          }
          pcVar15 = acStack_1fe48;
          if (acStack_1fe48[0] == '\0') {
            pcVar15 = (char *)0x0;
          }
          profile_add((RecoveredConfig *)(puVar14),(char *)(apcStack_1ffc8),iVar4,pcVar5,(char *)(pRVar11),pcVar15);
          profile_select((RecoveredConfig *)(puVar14),(char *)(apcStack_1ffc8));
          printf("Profile \'%s\' created and active.\n",(char *)(apcStack_1ffc8));
          if (iVar4 == 0) {
            ollama_refresh_models((RecoveredConfig *)(puVar14));
          }
        }
      }
    }
    else if (iVar4 == 3) {
      apcStack_1ffc8[0] = "ollama";
      apcStack_1ffc8[1] = "openai";
      apcStack_1ffc8[2] = "anthropic";
      apcStack_1ffc8[3] = "google";
      printf(
                      "Select Direct Provider (1-4):\n 1. ollama\n 2. openai\n 3. anthropic\n 4. google\nPick: "
                      );
      puVar3 = (undefined *)(PTR_000396a4);
      fflush((FILE *)(PTR_000396a4 + 0x58));
      pcVar5 = fgets((char *)&RStack_1ffd4,8,(FILE *)((RecoveredDString *)puVar3));
      if (pcVar5 != (char *)0x0) {
        iVar4 = atoi((char *)&RStack_1ffd4);
        uVar7 = iVar4 - 1;
        if (uVar7 < 4) {
          param_1[0x2284] = uVar7;
          *(undefined1 *)(param_1 + 0x2285) = 0;
          printf("Direct %s mode active.\n",apcStack_1ffc8[uVar7]);
        }
      }
    }
    config_save((RecoveredConfig *)(param_1 + 0x2284));
    return 1;
  }
  return 0;

#undef iStack_1ffd8
#undef RStack_1ffd4
#undef apcStack_1ffc8
#undef acStack_1ff48
#undef acStack_1fe48
#undef uStack_1fa49
#undef RStack_1fa48
}

/*
 * _simple_mode @ 0x00004e30..0x000053c8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Read user commands, call the selected provider, request approval for proposed tools and append results to conversation state.
 * Parameters/return (inferred): void _simple_mode(RecoveredAppState *app)
 * Direct callers: _main@0x00005498
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; performs network/protocol I/O; resolves/executes a program through PATH; executes an argument vector through PATH; reads text from a stream; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; writes text to a stream; reads bytes from a stream; writes bytes to a stream; reads the process working directory; reads process environment; signals a process; creates directories; securely creates a temporary file; opens a directory for enumeration; executes a shell command and opens a pipe; reads a descriptor or IPC channel; reads directory entries; resolves a filesystem path; renames/replaces filesystem entries; executes SQL text; opens/creates a SQLite database; prepares SQL text; reads filesystem metadata; removes a filesystem entry; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00004e30_simple_mode.s
 */
void simple_mode(RecoveredAppState *app)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x5fa70] __attribute__((aligned(16)));
#define acStack_5fa58 (*((char (*)[262144])(app_frame + 0x8))) /* original SP - 0x5fa58 */
#define RStack_1fa58 (*((RecoveredDString *)(app_frame + 0x40008))) /* original SP - 0x1fa58 */
#define RStack_1fa4c (*((RecoveredResponse *)(app_frame + 0x40014))) /* original SP - 0x1fa4c */

  bool bVar1;
  bool bVar2;
  undefined *stream;
  char *pcVar3;
  int iVar4;
  int iVar5;
  RecoveredToolCall *pRVar6;
  char *pcVar7;
  int iVar8;
  RecoveredDString *pRVar9;
  RecoveredMessage *pRVar10;

  printf("PowerPC Code v%s\n","0.8.0");
  pcVar3 = provider_name((app->config).provider);
  printf("%s/%s | Type /help for commands, /quit to exit.\n\n",pcVar3,(app->config).model)
  ;
  do {
    while( true ) {
      do {
        if (app->running == 0) {
          return;
        }
        printf("> ");
        stream = (undefined *)(PTR_000396a4);
        fflush((FILE *)(PTR_000396a4 + 0x58));
        pcVar3 = fgets(acStack_5fa58,0x40000,(FILE *)(stream));
        if (pcVar3 == (char *)0x0) {
          return;
        }
        pcVar3 = strchr(acStack_5fa58,10);
        if (pcVar3 != (char *)0x0) {
          *pcVar3 = '\0';
        }
        pcVar3 = strchr(acStack_5fa58,0xd);
        if (pcVar3 != (char *)0x0) {
          *pcVar3 = '\0';
        }
      } while ((acStack_5fa58[0] == '\0') ||
              (iVar4 = handle_slash_command((undefined4 *)(app),acStack_5fa58), iVar4 != 0));
      iVar4 = (app->conversation).message_count;
      if (iVar4 < 0x17e) break;
      puts("Conversation too long. Use /clear or /compact.");
    }
    if ((app->context_loaded != 0) && (iVar4 == 0)) {
      (app->conversation).message_count = 1;
      memset(&app->conversation,0,0x5c);
      (app->conversation).messages[0].role = 0;
      pRVar9 = &(app->conversation).messages[0].content;
      ds_init(pRVar9);
      pcVar3 = ds_cstr(&app->project_context);
      ds_appendf(pRVar9,"Project context: %s",pcVar3);
    }
    iVar4 = (app->conversation).message_count;
    pRVar10 = (app->conversation).messages + iVar4;
    (app->conversation).message_count = iVar4 + 1;
    memset(pRVar10,0,0x5c);
    (app->conversation).messages[iVar4].role = 1;
    pRVar9 = &pRVar10->content;
    ds_init(pRVar9);
    ds_append(pRVar9,acStack_5fa58);
    printf("\nYou: %s\n\nAssistant: ",acStack_5fa58);
    fflush((FILE *)(PTR_000396a4 + 0x58));
    iVar4 = 0x14;
    bVar2 = false;
    while (bVar1 = iVar4 != 0, iVar4 = iVar4 + -1, bVar1) {
      memset(&RStack_1fa4c,0,0x1fa0c);
      iVar5 = provider_send(&app->config,&app->conversation,&RStack_1fa4c,
                             (RecoveredStreamCallback)0x0,(void *)0x0,(RecoveredRoute *)0x0,
                             (int *)0x0);
      if (iVar5 < 0) {
        printf("[ERROR] HTTP request failed (%d)\n",iVar5);
        break;
      }
      if (399 < iVar5) {
        pcVar3 = ds_cstr(&RStack_1fa4c.error_or_reasoning);
        show_simple_http_error(iVar5,pcVar3);
        break;
      }
      ds_init(&RStack_1fa58);
      reasoning_extract((RecoveredDString *)(&RStack_1fa4c),&RStack_1fa58);
      pcVar3 = ds_cstr(&RStack_1fa4c.content);
      puts(pcVar3);
      if (RStack_1fa58.length != 0) {
        printf("[thinking: %zu chars]\n",(size_t)RStack_1fa58.length);
      }
      fflush((FILE *)(PTR_000396a4 + 0x58));
      ds_free(&RStack_1fa58);
      iVar5 = (app->conversation).message_count;
      if (0x17d < iVar5) break;
      pRVar10 = (app->conversation).messages + iVar5;
      (app->conversation).message_count = iVar5 + 1;
      memset(pRVar10,0,0x5c);
      (app->conversation).messages[iVar5].role = 2;
      ds_init(&pRVar10->content);
      ds_copy(&pRVar10->content,&RStack_1fa4c.content);
      if (0 < RStack_1fa4c.tool_call_count) {
        pRVar10->tool_call_capacity = RStack_1fa4c.tool_call_count;
        pRVar6 = malloc(RStack_1fa4c.tool_call_count * 0x51c);
        pRVar10->tool_calls = pRVar6;
        pRVar10->tool_call_count = RStack_1fa4c.tool_call_count;
        if (0 < RStack_1fa4c.tool_call_count) {
          iVar5 = 0;
          iVar8 = 0;
          pRVar6 = RStack_1fa4c.tool_calls;
          do {
            tool_call_copy((RecoveredToolCall *)(pRVar10->tool_calls->mcp_name + iVar8 + -4),pRVar6
                           );
            iVar5 = iVar5 + 1;
            iVar8 = iVar8 + 0x51c;
            pRVar6 = pRVar6 + 1;
          } while (iVar5 < RStack_1fa4c.tool_call_count);
        }
      }
      if (RStack_1fa4c.tool_call_count == 0) {
        if ((bVar2) || (iVar4 < 1)) break;
        iVar5 = (app->conversation).message_count;
        pRVar10 = (app->conversation).messages + iVar5;
        (app->conversation).message_count = iVar5 + 1;
        memset(pRVar10,0,0x5c);
        (app->conversation).messages[iVar5].role = 1;
        pRVar9 = &pRVar10->content;
        ds_init(pRVar9);
        ds_append(pRVar9,
                   "Continue. Use your tools to implement what you described. Do not repeat what you already wrote."
                  );
        printf("\n[Auto-continue: executing plan...]\n\nAssistant: ");
        fflush((FILE *)(PTR_000396a4 + 0x58));
        bVar2 = true;
      }
      else {
        if ((0 < RStack_1fa4c.tool_call_count) && ((app->conversation).message_count < 0x17f)) {
          iVar5 = 0;
          pRVar6 = RStack_1fa4c.tool_calls;
          do {
            iVar8 = simple_ask_approve(app,pRVar6);
            if (iVar8 == 0) {
              ds_clear(&pRVar6->result);
              ds_append(&pRVar6->result,"{\"error\": \"User rejected tool call\"}");
            }
            else {
              tool_execute(pRVar6,app);
            }
            pcVar3 = tool_name(pRVar6->tool_type);
            pcVar7 = ds_cstr(&pRVar6->result);
            printf("\n[tool:%s] %.200s\n",pcVar3,pcVar7);
            iVar8 = (app->conversation).message_count;
            pRVar10 = (app->conversation).messages + iVar8;
            (app->conversation).message_count = iVar8 + 1;
            memset(pRVar10,0,0x5c);
            (app->conversation).messages[iVar8].role = 3;
            ds_init(&pRVar10->content);
            ds_copy(&pRVar10->content,&pRVar6->result);
            strncpy(pRVar10->call_id,pRVar6->call_id,0x3f);
            iVar5 = iVar5 + 1;
          } while ((iVar5 < RStack_1fa4c.tool_call_count) &&
                  (pRVar6 = pRVar6 + 1, (app->conversation).message_count < 0x17f));
        }
        printf("\nAssistant: ");
        fflush((FILE *)(PTR_000396a4 + 0x58));
      }
    }
    putchar(10);
  } while( true );

#undef acStack_5fa58
#undef RStack_1fa58
#undef RStack_1fa4c
}

/*
 * _cleanup_orphan_workers @ 0x000053c8..0x00005498
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Scan temporary worker files and remove entries considered orphaned by the implementation.
 * Parameters/return (inferred): void _cleanup_orphan_workers(void)
 * Direct callers: _main@0x00005498
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; opens a directory for enumeration; reads directory entries; removes a filesystem entry
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000053c8_cleanup_orphan_workers.s
 */
void cleanup_orphan_workers(void)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x240] __attribute__((aligned(16)));
#define acStack_228 (*((char (*)[528])(app_frame + 0x8))) /* original SP - 0x228 */

  void *directory;
  int iVar1;
  RecoveredTigerDirent *pRVar2;
  int iVar3;

  directory = opendir("/tmp");
  if (directory != (void *)0x0) {
    iVar3 = 0;
    while( true ) {
      pRVar2 = (RecoveredTigerDirent *)(readdir(directory));
      if (pRVar2 == (RecoveredTigerDirent *)0x0) break;
      iVar1 = strncmp(pRVar2->name,"ppccode_worker_",0xf);
      if (iVar1 == 0) {
        snprintf(acStack_228,0x200,"/tmp/%s",pRVar2->name);
        iVar1 = unlink(acStack_228);
        if (iVar1 == 0) {
          iVar3 = iVar3 + 1;
        }
      }
    }
    closedir(directory);
    if (0 < iVar3) {
      fprintf((FILE * restrict)(PTR_000396a4 + 0xb0),"Cleaned up %d orphan worker files from /tmp\n",iVar3);
    }
  }
  return;

#undef acStack_228
}

/*
 * _main @ 0x00005498..0x00005958
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Initialize configuration and MCP, parse CLI options/environment, then enter the simple or curses conversation interface.
 * Parameters/return (inferred): int _main(int argc,char **argv)
 * Direct callers: __start@0x0000271c, candidate_000028b0@0x000028b0
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; performs network/protocol I/O; replaces a process with an executable; resolves/executes a program through PATH; executes an argument vector through PATH; reads text from a stream; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; writes text to a stream; reads bytes from a stream; writes bytes to a stream; reads the process working directory; reads process environment; signals a process; creates directories; securely creates a temporary file; opens a directory for enumeration; executes a shell command and opens a pipe; reads a descriptor or IPC channel; reads directory entries; resolves a filesystem path; renames/replaces filesystem entries; changes process environment; executes SQL text; opens/creates a SQLite database; prepares SQL text; reads filesystem metadata; removes a filesystem entry; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00005498_main.s
 */
int main(int argc,char **argv)

{
  bool bVar1;
  RecoveredAppState *pRVar2;
  int iVar3;
  int iVar4;
  char (*configuration) [512];
  char *pcVar5;
  RecoveredConfig *pRVar6;
  RecoveredMessage *message;
  int iVar7;
  
  app_storage_init();
  cleanup_orphan_workers();
  signal(0xd,(void *)0x1);
  pRVar2 = PTR__g_state_000396a0;
  pRVar6 = &PTR__g_state_000396a0->config;
  config_init(pRVar6);
  config_load(pRVar6);
  pRVar2->running = 1;
  (pRVar2->conversation).message_count = 0;
  mcp_init();
  configuration = (pRVar2->config).mcp_servers;
  pRVar6 = &PTR__g_state_000396a0->config;
  do {
    if ((*configuration)[0] != '\0') {
      mcp_register(*configuration);
    }
    configuration = configuration + 1;
  } while (pRVar6->profiles != (RecoveredProfile *)configuration);
  mcp_discover_all();
  platform_detect();
  pRVar2 = PTR__g_state_000396a0;
  bVar1 = true;
  if (1 < argc) {
    iVar7 = 1;
    bVar1 = true;
    do {
      pcVar5 = argv[iVar7];
      iVar3 = strcmp(pcVar5,"-p");
      if ((iVar3 == 0) && (iVar3 = iVar7 + 1, iVar3 < argc)) {
        iVar4 = provider_from_name(argv[iVar3]);
        iVar7 = iVar3;
        if (-1 < iVar4) {
          (pRVar2->config).provider = iVar4;
        }
      }
      else {
        iVar3 = strcmp(pcVar5,"-m");
        if ((iVar3 == 0) && (iVar3 = iVar7 + 1, iVar3 < argc)) {
          strncpy((pRVar2->config).model,argv[iVar3],0x7f);
          iVar7 = iVar3;
        }
        else {
          iVar3 = strcmp(pcVar5,"-k");
          if ((iVar3 == 0) && (iVar3 = iVar7 + 1, iVar3 < argc)) {
            strncpy((pRVar2->config).api_key,argv[iVar3],0xff);
            iVar7 = iVar3;
          }
          else {
            iVar3 = strcmp(pcVar5,"-u");
            if ((iVar3 == 0) && (iVar3 = iVar7 + 1, iVar3 < argc)) {
              strncpy((pRVar2->config).base_url,argv[iVar3],0x1ff);
              iVar7 = iVar3;
            }
            else {
              iVar3 = strcmp(pcVar5,"-d");
              if ((iVar3 == 0) && (iVar3 = iVar7 + 1, iVar3 < argc)) {
                strncpy((pRVar2->config).working_directory,argv[iVar3],0x3ff);
                iVar7 = iVar3;
              }
              else {
                iVar3 = strcmp(pcVar5,"-v");
                if (iVar3 == 0) {
                  (pRVar2->config).verbose = 1;
                }
                else {
                  iVar3 = strcmp(pcVar5,"--no-tui");
                  if (iVar3 == 0) {
                    bVar1 = false;
                  }
                  else {
                    iVar3 = strcmp(pcVar5,"-h");
                    if ((iVar3 == 0) || (iVar3 = strcmp(pcVar5,"--help"), iVar3 == 0)) {
                      usage(*argv);
                      return 0;
                    }
                  }
                }
              }
            }
          }
        }
      }
      iVar7 = iVar7 + 1;
    } while (iVar7 < argc);
  }
  pRVar2 = PTR__g_state_000396a0;
  if ((((PTR__g_state_000396a0->config).openai_key[0] == '\0') &&
      (pcVar5 = getenv("OPENAI_API_KEY"), pcVar5 != (char *)0x0)) && (*pcVar5 != '\0')) {
    strncpy((pRVar2->config).openai_key,pcVar5,0xff);
  }
  pRVar2 = PTR__g_state_000396a0;
  if ((((PTR__g_state_000396a0->config).anthropic_key[0] == '\0') &&
      (pcVar5 = getenv("ANTHROPIC_API_KEY"), pcVar5 != (char *)0x0)) && (*pcVar5 != '\0')) {
    strncpy((pRVar2->config).anthropic_key,pcVar5,0xff);
  }
  pRVar2 = PTR__g_state_000396a0;
  if ((((PTR__g_state_000396a0->config).google_key[0] == '\0') &&
      (pcVar5 = getenv("GOOGLE_API_KEY"), pcVar5 != (char *)0x0)) && (*pcVar5 != '\0')) {
    strncpy((pRVar2->config).google_key,pcVar5,0xff);
  }
  pRVar2 = PTR__g_state_000396a0;
  if ((((PTR__g_state_000396a0->config).ollama_key[0] == '\0') &&
      (pcVar5 = getenv("OLLAMA_API_KEY"), pcVar5 != (char *)0x0)) && (*pcVar5 != '\0')) {
    strncpy((pRVar2->config).ollama_key,pcVar5,0xff);
  }
  if (bVar1) {
    tui_init();
    if ((PTR__g_state_000396a0->config).theme[0] != '\0') {
      tui_theme_set((PTR__g_state_000396a0->config).theme);
    }
    tui_run(PTR__g_state_000396a0);
    tui_cleanup();
  }
  else {
    simple_mode(PTR__g_state_000396a0);
  }
  pRVar2 = PTR__g_state_000396a0;
  if (0 < (PTR__g_state_000396a0->conversation).message_count) {
    iVar7 = 0;
    message = (PTR__g_state_000396a0->conversation).messages;
    do {
      message_free(message);
      iVar7 = iVar7 + 1;
      message = message + 1;
    } while (iVar7 < (pRVar2->conversation).message_count);
  }
  pRVar2 = PTR__g_state_000396a0;
  ds_free(&PTR__g_state_000396a0->project_context);
  ds_free(&(pRVar2->config).system_prompt);
  return 0;
}

