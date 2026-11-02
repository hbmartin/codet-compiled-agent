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
 * _google_build_tools @ 0x00010768..0x00011474
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Construct google build tools data; inspect the preserved body for exact field names and conditions.
 * Parameters/return (inferred): RecoveredCJSON * _google_build_tools(RecoveredRoute *route,int *failures)
 * Direct callers: _google_send@0x00012238
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00010768_google_build_tools.s
 */
RecoveredCJSON * google_build_tools(RecoveredRoute *route,int *failures)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0xc0] __attribute__((aligned(16)));
#define local_a8 (*((char * (*)[4])(app_frame + 0x8))) /* original SP - 0xa8 */
#define local_98 (*((char * *)(app_frame + 0x18))) /* original SP - 0x98 */
#define local_94 (*((char * *)(app_frame + 0x1c))) /* original SP - 0x94 */
#define local_90 (*((char * *)(app_frame + 0x20))) /* original SP - 0x90 */
#define local_8c (*((char * *)(app_frame + 0x24))) /* original SP - 0x8c */
#define local_88 (*((char * (*)[4])(app_frame + 0x28))) /* original SP - 0x88 */
#define local_78 (*((char * *)(app_frame + 0x38))) /* original SP - 0x78 */
#define local_74 (*((char * *)(app_frame + 0x3c))) /* original SP - 0x74 */
#define local_70 (*((char * *)(app_frame + 0x40))) /* original SP - 0x70 */
#define local_6c (*((char * *)(app_frame + 0x44))) /* original SP - 0x6c */
#define local_68 (*((RecoveredCJSON * *)(app_frame + 0x48))) /* original SP - 0x68 */
#define local_64 (*((RecoveredCJSON * *)(app_frame + 0x4c))) /* original SP - 0x64 */
#define local_60 (*((char * *)(app_frame + 0x50))) /* original SP - 0x60 */
#define local_5c (*((char * *)(app_frame + 0x54))) /* original SP - 0x5c */

  bool bVar1;
  RecoveredMCPState *pRVar2;
  RecoveredCJSON *array;
  int iVar3;
  RecoveredCJSON *pRVar4;
  RecoveredCJSON *pRVar5;
  RecoveredCJSON *pRVar6;
  RecoveredCJSON *pRVar7;
  RecoveredCJSON *array_00;
  RecoveredCJSON *pRVar8;
  char *pcVar9;
  RecoveredMCPState *pRVar10;
  int *piVar11;
  int iVar12;
  int iVar13;
  uint tool_type;

  local_64 = cJSON_CreateArray();
  local_68 = cJSON_CreateObject();
  cJSON_AddStringToObject(local_68,"name","tigr_tools");
  array = cJSON_CreateArray();
  local_a8[0] = "read_file";
  local_a8[1] = "write_file";
  local_a8[2] = "edit_file";
  local_a8[3] = "shell";
  local_98 = "glob";
  local_94 = "grep";
  local_90 = "search";
  local_8c = "read_and_edit";
  local_88[0] = 
  "Read the contents of one or more files. Multiple paths can be comma-separated. Example: path=\'src/main.c,src/config.c\'"
  ;
  local_88[1] = 
  "Write content to a file. Creates parent directories if needed. Overwrites existing files. Example: path=\'src/utils.c\', content=\'...\'"
  ;
  local_88[2] = 
  "Edit a file by replacing old text with new text. The oldText must match exactly (including whitespace). Only replaces the first occurrence. Example: path=\'src/main.c\', oldText=\'int x = 1;\', newText=\'int x = 2;\'"
  ;
  local_88[3] = 
  "Execute a shell command in the working directory. Use for listing files, running builds, checking git status, etc. Commands run with bash. Example: command=\'ls -la\'"
  ;
  local_78 = 
  "Find files matching a glob pattern in the working directory and subdirectories. Example: pattern=\'*.c\'"
  ;
  local_74 = 
  "Search file contents for a regex pattern. Searches recursively in the working directory. Example: pattern=\'TODO|FIXME\'"
  ;
  local_70 = 
  "Search the web for news, technical info, or answers requiring external knowledge. Use concise, specific queries. Example: query=\'latest PowerPC G5 news\'"
  ;
  local_6c = 
  "Read a file and then apply an edit in one step. Equivalent to read_file followed by edit_file. Args: path, oldText, newText."
  ;
  tool_type = 0;
  iVar12 = 0;
  local_60 = "dable text.";
  local_5c = "dable text.";
  do {
    if (route == (RecoveredRoute *)0x0) {
LAB_000108bc:
      pRVar4 = cJSON_CreateObject();
      cJSON_AddStringToObject(pRVar4,"name",*(char **)((int)local_a8 + iVar12));
      cJSON_AddStringToObject(pRVar4,"description",*(char **)((int)local_88 + iVar12));
      pRVar5 = cJSON_CreateObject();
      cJSON_AddStringToObject(pRVar5,"type","object");
      pRVar6 = cJSON_CreateObject();
      if (tool_type == 7) {
        pRVar7 = cJSON_CreateObject();
        cJSON_AddStringToObject(pRVar7,"type","string");
        cJSON_AddStringToObject(pRVar7,"description","File path to read and edit");
        cJSON_AddItemToObject(pRVar6,"path",pRVar7);
        pRVar7 = cJSON_CreateObject();
        cJSON_AddStringToObject(pRVar7,"type","string");
        cJSON_AddStringToObject(pRVar7,"description","Exact text to replace");
        cJSON_AddItemToObject(pRVar6,"oldText",pRVar7);
        pRVar7 = cJSON_CreateObject();
        cJSON_AddStringToObject(pRVar7,"type","string");
        cJSON_AddStringToObject(pRVar7,"description","New replacement text");
        cJSON_AddItemToObject(pRVar6,"newText",pRVar7);
        cJSON_AddItemToObject(pRVar5,"properties",pRVar6);
        pRVar6 = cJSON_CreateArray();
        pRVar7 = cJSON_CreateString("path");
        cJSON_AddItemToArray(pRVar6,pRVar7);
        pRVar7 = cJSON_CreateString("oldText");
        cJSON_AddItemToArray(pRVar6,pRVar7);
        pRVar7 = cJSON_CreateString("newText");
        cJSON_AddItemToArray(pRVar6,pRVar7);
        cJSON_AddItemToObject(pRVar5,"required",pRVar6);
      }
      else {
        pRVar7 = cJSON_CreateObject();
        cJSON_AddStringToObject(pRVar7,"type","string");
        if (tool_type == 6) {
          cJSON_AddStringToObject(pRVar7,local_60 + -0x14c,"Search query text");
          pcVar9 = "query";
        }
        else if (tool_type == 3) {
          pcVar9 = "command";
        }
        else {
          pcVar9 = local_5c + -0x1dc;
        }
        cJSON_AddItemToObject(pRVar6,pcVar9,pRVar7);
        cJSON_AddItemToObject(pRVar5,"properties",pRVar6);
      }
      cJSON_AddItemToObject(pRVar4,"parameters",pRVar5);
      cJSON_AddItemToArray(array,pRVar4);
    }
    else {
      iVar3 = router_should_send_tool
                        (tool_type,route->intent_mask,route->mentions_path,route->short_message);
      if (iVar3 != 0) {
        iVar3 = trust_is_trusted(tool_type,failures);
        if (iVar3 != 0) goto LAB_000108bc;
      }
    }
    iVar12 = iVar12 + 4;
    bVar1 = tool_type != 7;
    tool_type = tool_type + 1;
  } while (bVar1);
  if (route == (RecoveredRoute *)0x0) {
LAB_00010b10:
    pRVar4 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar4,"name","fetch_url");
    cJSON_AddStringToObject
              (pRVar4,"description",
               "Fetch the text content of a web URL. Convert HTML to readable text.");
    pRVar5 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar5,"type","object");
    pRVar6 = cJSON_CreateObject();
    pRVar7 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar7,"type","string");
    cJSON_AddItemToObject(pRVar6,"url",pRVar7);
    cJSON_AddItemToObject(pRVar5,"properties",pRVar6);
    pRVar6 = cJSON_CreateArray();
    pRVar7 = cJSON_CreateString("url");
    cJSON_AddItemToArray(pRVar6,pRVar7);
    cJSON_AddItemToObject(pRVar5,"required",pRVar6);
    cJSON_AddItemToObject(pRVar4,"parameters",pRVar5);
    cJSON_AddItemToArray(array,pRVar4);
    if (route != (RecoveredRoute *)0x0) goto LAB_00010c08;
LAB_00010c38:
    pRVar4 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar4,"name","db_query");
    cJSON_AddStringToObject
              (pRVar4,"description",
               "Query or modify a SQLite database. Supports SELECT, INSERT, UPDATE, DELETE, and PRAGMA."
              );
    pRVar5 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar5,"type","object");
    pRVar6 = cJSON_CreateObject();
    pRVar7 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar7,"type","string");
    cJSON_AddItemToObject(pRVar6,"db_path",pRVar7);
    pRVar7 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar7,"type","string");
    array_00 = cJSON_CreateArray();
    pRVar8 = cJSON_CreateString("query");
    cJSON_AddItemToArray(array_00,pRVar8);
    pRVar8 = cJSON_CreateString("tables");
    cJSON_AddItemToArray(array_00,pRVar8);
    pRVar8 = cJSON_CreateString("schema");
    cJSON_AddItemToArray(array_00,pRVar8);
    pRVar8 = cJSON_CreateString("insert");
    cJSON_AddItemToArray(array_00,pRVar8);
    pRVar8 = cJSON_CreateString("update");
    cJSON_AddItemToArray(array_00,pRVar8);
    pRVar8 = cJSON_CreateString("delete");
    cJSON_AddItemToArray(array_00,pRVar8);
    cJSON_AddItemToObject(pRVar7,"enum",array_00);
    cJSON_AddItemToObject(pRVar6,"operation",pRVar7);
    pRVar7 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar7,"type","string");
    cJSON_AddItemToObject(pRVar6,"sql",pRVar7);
    cJSON_AddItemToObject(pRVar5,"properties",pRVar6);
    pRVar6 = cJSON_CreateArray();
    pRVar7 = cJSON_CreateString("db_path");
    cJSON_AddItemToArray(pRVar6,pRVar7);
    pRVar7 = cJSON_CreateString("operation");
    cJSON_AddItemToArray(pRVar6,pRVar7);
    cJSON_AddItemToObject(pRVar5,"required",pRVar6);
    cJSON_AddItemToObject(pRVar4,"parameters",pRVar5);
    cJSON_AddItemToArray(array,pRVar4);
    if (route != (RecoveredRoute *)0x0) goto LAB_00010e48;
LAB_00010e78:
    pRVar4 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar4,"name","subagent");
    cJSON_AddStringToObject
              (pRVar4,"description",
               "Launch a sub-agent to handle a complex multi-step task. The sub-agent runs in a separate process with up to 5 tool-calling turns and returns a summarized result. Use for focused subtasks."
              );
    pRVar5 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar5,"type","object");
    pRVar6 = cJSON_CreateObject();
    pRVar7 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar7,"type","string");
    cJSON_AddStringToObject(pRVar7,"description","Task description for the sub-agent.");
    cJSON_AddItemToObject(pRVar6,"prompt",pRVar7);
    pRVar7 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar7,"type","string");
    cJSON_AddStringToObject(pRVar7,"description","Model name (optional).");
    cJSON_AddItemToObject(pRVar6,"model",pRVar7);
    cJSON_AddItemToObject(pRVar5,"properties",pRVar6);
    pRVar6 = cJSON_CreateArray();
    pRVar7 = cJSON_CreateString("prompt");
    cJSON_AddItemToArray(pRVar6,pRVar7);
    cJSON_AddItemToObject(pRVar5,"required",pRVar6);
    cJSON_AddItemToObject(pRVar4,"parameters",pRVar5);
    cJSON_AddItemToArray(array,pRVar4);
  }
  else {
    iVar12 = router_should_send_tool
                       (7,route->intent_mask,route->mentions_path,route->short_message);
    if (iVar12 != 0) {
      iVar12 = trust_is_trusted(7,failures);
      if (iVar12 != 0) goto LAB_00010b10;
    }
LAB_00010c08:
    iVar12 = router_should_send_tool
                       (8,route->intent_mask,route->mentions_path,route->short_message);
    if (iVar12 != 0) {
      iVar12 = trust_is_trusted(8,failures);
      if (iVar12 != 0) goto LAB_00010c38;
    }
LAB_00010e48:
    iVar12 = router_should_send_tool
                       (0xc,route->intent_mask,route->mentions_path,route->short_message);
    if (iVar12 != 0) {
      iVar12 = trust_is_trusted(0xc,failures);
      if (iVar12 != 0) goto LAB_00010e78;
    }
  }
  cJSON_AddItemToObject(local_68,"functionDeclarations",array);
  cJSON_AddItemToArray(local_64,local_68);
  pRVar2 = PTR__g_mcp_000396b4;
  if (0 < PTR__g_mcp_000396b4->count) {
    iVar12 = 0;
    piVar11 = &PTR__g_mcp_000396b4->servers[0].tool_count;
    pRVar10 = PTR__g_mcp_000396b4;
    do {
      if (0 < *piVar11) {
        iVar3 = 0;
        iVar13 = 0;
        do {
          pcVar9 = pRVar10->servers[0].tools[0].name + iVar13;
          pRVar4 = cJSON_CreateObject();
          cJSON_AddStringToObject(pRVar4,"name",pcVar9);
          cJSON_AddStringToObject(pRVar4,"description",pcVar9 + 0x40);
          pRVar5 = cJSON_Parse(pcVar9 + 0x240);
          if (pRVar5 == (RecoveredCJSON *)0x0) {
            pRVar5 = cJSON_CreateObject();
          }
          cJSON_AddItemToObject(pRVar4,"parameters",pRVar5);
          cJSON_AddItemToArray(array,pRVar4);
          iVar3 = iVar3 + 1;
          iVar13 = iVar13 + 0x1240;
        } while (iVar3 < *piVar11);
      }
      iVar12 = iVar12 + 1;
      piVar11 = piVar11 + 0x9358;
      pRVar10 = (RecoveredMCPState *)(pRVar10->servers + 1);
    } while (iVar12 < pRVar2->count);
  }
  if (route != (RecoveredRoute *)0x0) {
    iVar12 = router_should_send_tool
                       (0xd,route->intent_mask,route->mentions_path,route->short_message);
    if (iVar12 == 0) goto LAB_00011210;
    iVar12 = trust_is_trusted(0xd,failures);
    if (iVar12 == 0) goto LAB_00011210;
  }
  pRVar4 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar4,"name","reference");
  cJSON_AddStringToObject
            (pRVar4,"description",
             "Search the Tiger OS X documentation database for API references, function signatures, and programming guides. Use when user asks about Carbon, CoreFoundation, IOKit, or other Mac-specific APIs."
            );
  pRVar5 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar5,"type","object");
  pRVar6 = cJSON_CreateObject();
  pRVar7 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar7,"type","string");
  cJSON_AddStringToObject
            (pRVar7,"description",
             "What to search for (e.g., \'CFPreferences\', \'NSWindow setTitle\', \'IOKit matching\')"
            );
  cJSON_AddItemToObject(pRVar6,"query",pRVar7);
  cJSON_AddItemToObject(pRVar5,"properties",pRVar6);
  pRVar6 = cJSON_CreateArray();
  pRVar7 = cJSON_CreateString("query");
  cJSON_AddItemToArray(pRVar6,pRVar7);
  cJSON_AddItemToObject(pRVar5,"required",pRVar6);
  cJSON_AddItemToObject(pRVar4,"parameters",pRVar5);
  cJSON_AddItemToArray(array,pRVar4);
LAB_00011210:
  pRVar4 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar4,"name","tree");
  cJSON_AddStringToObject
            (pRVar4,"description","Return a recursive directory tree with file sizes in ONE call.");
  pRVar5 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar5,"type","object");
  pRVar6 = cJSON_CreateObject();
  pRVar7 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar7,"type","string");
  cJSON_AddStringToObject(pRVar7,"description","Sub-path relative to working dir");
  cJSON_AddItemToObject(pRVar6,"path",pRVar7);
  pRVar7 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar7,"type","integer");
  cJSON_AddStringToObject(pRVar7,"description","Max depth 1-8 (default 4)");
  cJSON_AddItemToObject(pRVar6,"max_depth",pRVar7);
  cJSON_AddItemToObject(pRVar5,"properties",pRVar6);
  cJSON_AddItemToObject(pRVar4,"parameters",pRVar5);
  cJSON_AddItemToArray(array,pRVar4);
  pRVar4 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar4,"name","read_project");
  cJSON_AddStringToObject
            (pRVar4,"description",
             "Atomically read every file matching a glob in ONE call. Each file is truncated to max_bytes_per_file."
            );
  pRVar5 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar5,"type","object");
  pRVar6 = cJSON_CreateObject();
  pRVar7 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar7,"type","string");
  cJSON_AddStringToObject(pRVar7,"description","Glob pattern relative to working dir");
  cJSON_AddItemToObject(pRVar6,"pattern",pRVar7);
  pRVar7 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar7,"type","integer");
  cJSON_AddStringToObject(pRVar7,"description","Max bytes per file (default 65536)");
  cJSON_AddItemToObject(pRVar6,"max_bytes_per_file",pRVar7);
  cJSON_AddItemToObject(pRVar5,"properties",pRVar6);
  pRVar6 = cJSON_CreateArray();
  pRVar7 = cJSON_CreateString("pattern");
  cJSON_AddItemToArray(pRVar6,pRVar7);
  cJSON_AddItemToObject(pRVar5,"required",pRVar6);
  cJSON_AddItemToObject(pRVar4,"parameters",pRVar5);
  cJSON_AddItemToArray(array,pRVar4);

  return local_64;

#undef local_a8
#undef local_98
#undef local_94
#undef local_90
#undef local_8c
#undef local_88
#undef local_78
#undef local_74
#undef local_70
#undef local_6c
#undef local_68
#undef local_64
#undef local_60
#undef local_5c
}

/*
 * _google_build_contents @ 0x00011474..0x000119fc
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Construct google build contents data; inspect the preserved body for exact field names and conditions.
 * Parameters/return (inferred): RecoveredCJSON * _google_build_contents(RecoveredConfig *config,RecoveredConversation *conversation)
 * Direct callers: _google_send@0x00012238
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00011474_google_build_contents.s
 */
RecoveredCJSON * google_build_contents(RecoveredConfig *config,RecoveredConversation *conversation)

{
  (void)config; /* Present in the recovered ABI; unused by this routine. */
  RecoveredCJSON *array;
  RecoveredCJSON *object;
  RecoveredCJSON *array_00;
  RecoveredCJSON *pRVar1;
  RecoveredCJSON *object_00;
  char *pcVar2;
  uint uVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  int iVar7;
  RecoveredMessage *pRVar8;
  
  array = cJSON_CreateArray();
  if (0 < conversation->message_count) {
    iVar5 = 0;
    do {
      pRVar8 = conversation->messages + iVar5;
      if (conversation->messages[iVar5].role != 0) {
        object = cJSON_CreateObject();
        if (conversation->messages[iVar5].role == 2) {
          pcVar2 = "model";
        }
        else {
          pcVar2 = "user";
        }
        cJSON_AddStringToObject(object,"role",pcVar2);
        array_00 = cJSON_CreateArray();
        if (pRVar8->role == 3) {
          pRVar1 = cJSON_CreateObject();
          pcVar2 = ds_cstr(&pRVar8->content);
          cJSON_AddStringToObject(pRVar1,"text",pcVar2);
          cJSON_AddItemToArray(array_00,pRVar1);
        }
        else if ((pRVar8->content).length != 0) {
          pRVar1 = cJSON_CreateObject();
          pcVar2 = ds_cstr(&pRVar8->content);
          cJSON_AddStringToObject(pRVar1,"text",pcVar2);
          cJSON_AddItemToArray(array_00,pRVar1);
        }
        if (0 < pRVar8->tool_call_count) {
          iVar4 = 0;
          iVar6 = 0;
          do {
            pRVar1 = cJSON_CreateObject();
            pcVar2 = tool_name(*(int *)(pRVar8->tool_calls->mcp_name + iVar6 + -4));
            cJSON_AddStringToObject(pRVar1,"functionCall.name",pcVar2);
            object_00 = cJSON_CreateObject();
            pcVar2 = pRVar8->tool_calls->mcp_name + iVar6 + -4;
            uVar3 = *(uint *)(pRVar8->tool_calls->mcp_name + iVar6 + -4);
            switch(uVar3) {
            case 0:
              pcVar2 = ds_cstr((RecoveredDString *)(pcVar2 + 0x24));
              cJSON_AddStringToObject(object_00,"path",pcVar2);
              break;
            case 1:
              pcVar2 = ds_cstr((RecoveredDString *)(pcVar2 + 0x24));
              cJSON_AddStringToObject(object_00,"path",pcVar2);
              pcVar2 = ds_cstr((RecoveredDString *)
                                ((int)&pRVar8->tool_calls->arguments[1].data + iVar6));
              cJSON_AddStringToObject(object_00,"content",pcVar2);
              break;
            case 2:
              pcVar2 = ds_cstr((RecoveredDString *)(pcVar2 + 0x24));
              cJSON_AddStringToObject(object_00,"path",pcVar2);
              pcVar2 = ds_cstr((RecoveredDString *)
                                ((int)&pRVar8->tool_calls->arguments[1].data + iVar6));
              cJSON_AddStringToObject(object_00,"oldText",pcVar2);
              pcVar2 = ds_cstr((RecoveredDString *)
                                ((int)&pRVar8->tool_calls->arguments[2].data + iVar6));
              cJSON_AddStringToObject(object_00,"newText",pcVar2);
              break;
            case 3:
              pcVar2 = ds_cstr((RecoveredDString *)(pcVar2 + 0x24));
              cJSON_AddStringToObject(object_00,"command",pcVar2);
              break;
            case 4:
              pcVar2 = ds_cstr((RecoveredDString *)(pcVar2 + 0x24));
              cJSON_AddStringToObject(object_00,"pattern",pcVar2);
              break;
            case 5:
              pcVar2 = ds_cstr((RecoveredDString *)(pcVar2 + 0x24));
              cJSON_AddStringToObject(object_00,"pattern",pcVar2);
              break;
            case 6:
              pcVar2 = ds_cstr((RecoveredDString *)(pcVar2 + 0x24));
              cJSON_AddStringToObject(object_00,"query",pcVar2);
              break;
            default:
              if ((99 < uVar3) && (0 < *(int *)(pcVar2 + 0x4c8))) {
                iVar7 = 0;
                do {
                  pcVar2 = ds_cstr((RecoveredDString *)(pcVar2 + iVar7 * 0xc + 0x24));
                  if ((pcVar2 != (char *)0x0) && (*pcVar2 != '\0')) {
                    cJSON_AddStringToObject(object_00,"arg",pcVar2);
                  }
                  iVar7 = iVar7 + 1;
                  pcVar2 = pRVar8->tool_calls->mcp_name + iVar6 + -4;
                } while ((iVar7 < *(int *)(pcVar2 + 0x4c8)) && (iVar7 != 99));
              }
              break;
            case 0xb:
              pcVar2 = ds_cstr((RecoveredDString *)(pcVar2 + 0x24));
              cJSON_AddStringToObject(object_00,"path",pcVar2);
              pcVar2 = ds_cstr((RecoveredDString *)
                                ((int)&pRVar8->tool_calls->arguments[1].data + iVar6));
              cJSON_AddStringToObject(object_00,"oldText",pcVar2);
              pcVar2 = ds_cstr((RecoveredDString *)
                                ((int)&pRVar8->tool_calls->arguments[2].data + iVar6));
              cJSON_AddStringToObject(object_00,"newText",pcVar2);
              break;
            case 0xc:
              pcVar2 = ds_cstr((RecoveredDString *)(pcVar2 + 0x24));
              cJSON_AddStringToObject(object_00,"prompt",pcVar2);
              if (1 < *(int *)(pRVar8->tool_calls->call_id + iVar6 + -4)) {
                pcVar2 = ds_cstr((RecoveredDString *)
                                  ((int)&pRVar8->tool_calls->arguments[1].data + iVar6));
                cJSON_AddStringToObject(object_00,"model",pcVar2);
              }
              break;
            case 0xd:
              pcVar2 = ds_cstr((RecoveredDString *)(pcVar2 + 0x24));
              cJSON_AddStringToObject(object_00,"query",pcVar2);
              break;
            case 0xe:
              if (**(char **)(pcVar2 + 0x24) != '\0') {
                pcVar2 = ds_cstr((RecoveredDString *)(pcVar2 + 0x24));
                cJSON_AddStringToObject(object_00,"path",pcVar2);
              }
              if (1 < *(int *)(pRVar8->tool_calls->call_id + iVar6 + -4)) {
                pcVar2 = ds_cstr((RecoveredDString *)
                                  ((int)&pRVar8->tool_calls->arguments[1].data + iVar6));
                cJSON_AddStringToObject(object_00,"max_depth",pcVar2);
              }
              break;
            case 0xf:
              pcVar2 = ds_cstr((RecoveredDString *)(pcVar2 + 0x24));
              cJSON_AddStringToObject(object_00,"pattern",pcVar2);
              if (1 < *(int *)(pRVar8->tool_calls->call_id + iVar6 + -4)) {
                pcVar2 = ds_cstr((RecoveredDString *)
                                  ((int)&pRVar8->tool_calls->arguments[1].data + iVar6));
                cJSON_AddStringToObject(object_00,"max_bytes_per_file",pcVar2);
              }
            }
            cJSON_AddItemToObject(pRVar1,"functionCall.args",object_00);
            cJSON_AddItemToArray(array_00,pRVar1);
            iVar4 = iVar4 + 1;
            iVar6 = iVar6 + 0x51c;
          } while (iVar4 < pRVar8->tool_call_count);
        }
        cJSON_AddItemToObject(object,"parts",array_00);
        cJSON_AddItemToArray(array,object);
      }
      iVar5 = iVar5 + 1;
    } while (iVar5 < conversation->message_count);
  }
  return array;
}

/*
 * _google_sse_cb @ 0x000119fc..0x00012238
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Decode provider streaming events, accumulate response/tool-call state and invoke callbacks.
 * Parameters/return (inferred): void _google_sse_cb(char *event_kind,char *data,RecoveredProviderCallbackContext *context)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000119fc_google_sse_cb.s
 * Uncertainty: Indirect dispatch sites are retained with raw assembly and references; possible target sets are not assumed complete.
 */
void google_sse_cb(char *event_kind,char *data,RecoveredProviderCallbackContext *context)

{
  (void)event_kind; /* Present in the recovered ABI; unused by this routine. */
  bool bVar1;
  int iVar2;
  RecoveredCJSON *object;
  RecoveredCJSON *pRVar3;
  RecoveredCJSON *pRVar4;
  int iVar5;
  RecoveredCJSON *pRVar6;
  RecoveredCJSON *pRVar7;
  int index;
  RecoveredToolCall *output;
  RecoveredDString *str;
  char *left;
  
  if (((data != (char *)0x0) && (*data != '\0')) &&
     (object = cJSON_Parse(data), object != (RecoveredCJSON *)0x0)) {
    pRVar3 = cJSON_GetObjectItem(object,"candidates");
    if (pRVar3 == (RecoveredCJSON *)0x0) {
      cJSON_Delete(object);
    }
    else {
      pRVar3 = cJSON_GetArrayItem(pRVar3,0);
      if (pRVar3 == (RecoveredCJSON *)0x0) {
        cJSON_Delete(object);
      }
      else {
        pRVar4 = cJSON_GetObjectItem(pRVar3,"content");
        if (((pRVar4 != (RecoveredCJSON *)0x0) &&
            (pRVar4 = cJSON_GetObjectItem(pRVar4,"parts"), pRVar4 != (RecoveredCJSON *)0x0)) &&
           (iVar5 = cJSON_GetArraySize(pRVar4), 0 < iVar5)) {
          index = 0;
          do {
            pRVar6 = cJSON_GetArrayItem(pRVar4,index);
            pRVar7 = cJSON_GetObjectItem(pRVar6,"text");
            if ((pRVar7 != (RecoveredCJSON *)0x0) && (pRVar7->value_string != (char *)0x0)) {
              ds_append(&context->response->content,pRVar7->value_string);
              if (context->callback != (RecoveredStreamCallback)0x0) {
                (*context->callback)((char *)0x0,pRVar7->value_string,context->context);
              }
            }
            pRVar6 = cJSON_GetObjectItem(pRVar6,"functionCall");
            if (pRVar6 != (RecoveredCJSON *)0x0) {
              iVar2 = context->response->tool_call_count;
              if (iVar2 < 99) {
                output = context->response->tool_calls + iVar2;
                memset(output,0,0x51c);
                ds_init(&output->result);
                iVar2 = 0;
                str = output->arguments;
                do {
                  ds_init((RecoveredDString *)((int)&str->data + iVar2));
                  bVar1 = iVar2 != 0x498;
                  iVar2 = iVar2 + 0xc;
                } while (bVar1);
                pRVar7 = cJSON_GetObjectItem(pRVar6,"name");
                if ((pRVar7 != (RecoveredCJSON *)0x0) && (pRVar7->value_string != (char *)0x0)) {
                  strncpy(output->mcp_name,pRVar7->value_string,0x1f);
                  left = pRVar7->value_string;
                  iVar2 = strcmp(left,"read_file");
                  if (iVar2 == 0) {
                    output->tool_type = 0;
                  }
                  else {
                    iVar2 = strcmp(left,"write_file");
                    if (iVar2 == 0) {
                      output->tool_type = 1;
                    }
                    else {
                      iVar2 = strcmp(left,"edit_file");
                      if (iVar2 == 0) {
                        output->tool_type = 2;
                      }
                      else {
                        iVar2 = strcmp(left,"shell");
                        if (iVar2 == 0) {
                          output->tool_type = 3;
                        }
                        else {
                          iVar2 = strcmp(left,"glob");
                          if (iVar2 == 0) {
                            output->tool_type = 4;
                          }
                          else {
                            iVar2 = strcmp(left,"grep");
                            if (iVar2 == 0) {
                              output->tool_type = 5;
                            }
                            else {
                              iVar2 = strcmp(left,"search");
                              if (iVar2 == 0) {
                                output->tool_type = 6;
                              }
                              else {
                                iVar2 = strcmp(left,"read_and_edit");
                                if (iVar2 == 0) {
                                  output->tool_type = 0xb;
                                }
                                else {
                                  iVar2 = strcmp(left,"reference");
                                  if (iVar2 == 0) {
                                    output->tool_type = 0xd;
                                  }
                                  else {
                                    iVar2 = strcmp(left,"tree");
                                    if (iVar2 == 0) {
                                      output->tool_type = 0xe;
                                    }
                                    else {
                                      iVar2 = strcmp(left,"read_project");
                                      if (iVar2 == 0) {
                                        output->tool_type = 0xf;
                                      }
                                      else {
                                        output->tool_type = 100;
                                        strncpy(output->mcp_name,pRVar7->value_string,0x1f);
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
                pRVar6 = cJSON_GetObjectItem(pRVar6,"args");
                if (pRVar6 != (RecoveredCJSON *)0x0) {
                  switch(output->tool_type) {
                  case 0:
                    pRVar6 = cJSON_GetObjectItem(pRVar6,"path");
                    if ((pRVar6 != (RecoveredCJSON *)0x0) && (pRVar6->value_string != (char *)0x0))
                    {
                      ds_append(str,pRVar6->value_string);
                    }
                    output->argument_count = 1;
                    break;
                  case 1:
                    pRVar7 = cJSON_GetObjectItem(pRVar6,"path");
                    if ((pRVar7 != (RecoveredCJSON *)0x0) && (pRVar7->value_string != (char *)0x0))
                    {
                      ds_append(str,pRVar7->value_string);
                    }
                    pRVar6 = cJSON_GetObjectItem(pRVar6,"content");
                    if ((pRVar6 != (RecoveredCJSON *)0x0) && (pRVar6->value_string != (char *)0x0))
                    {
                      ds_append(output->arguments + 1,pRVar6->value_string);
                    }
                    output->argument_count = 2;
                    break;
                  case 2:
                    pRVar7 = cJSON_GetObjectItem(pRVar6,"path");
                    if ((pRVar7 != (RecoveredCJSON *)0x0) && (pRVar7->value_string != (char *)0x0))
                    {
                      ds_append(str,pRVar7->value_string);
                    }
                    pRVar7 = cJSON_GetObjectItem(pRVar6,"oldText");
                    if ((pRVar7 != (RecoveredCJSON *)0x0) && (pRVar7->value_string != (char *)0x0))
                    {
                      ds_append(output->arguments + 1,pRVar7->value_string);
                    }
                    pRVar6 = cJSON_GetObjectItem(pRVar6,"newText");
                    if ((pRVar6 != (RecoveredCJSON *)0x0) && (pRVar6->value_string != (char *)0x0))
                    {
                      ds_append(output->arguments + 2,pRVar6->value_string);
                    }
                    output->argument_count = 3;
                    break;
                  case 3:
                    pRVar6 = cJSON_GetObjectItem(pRVar6,"command");
                    if ((pRVar6 != (RecoveredCJSON *)0x0) && (pRVar6->value_string != (char *)0x0))
                    {
                      ds_append(str,pRVar6->value_string);
                    }
                    output->argument_count = 1;
                    break;
                  case 4:
                    pRVar6 = cJSON_GetObjectItem(pRVar6,"pattern");
                    if ((pRVar6 != (RecoveredCJSON *)0x0) && (pRVar6->value_string != (char *)0x0))
                    {
                      ds_append(str,pRVar6->value_string);
                    }
                    output->argument_count = 1;
                    break;
                  case 5:
                    pRVar6 = cJSON_GetObjectItem(pRVar6,"pattern");
                    if ((pRVar6 != (RecoveredCJSON *)0x0) && (pRVar6->value_string != (char *)0x0))
                    {
                      ds_append(str,pRVar6->value_string);
                    }
                    output->argument_count = 1;
                    break;
                  case 6:
                    pRVar6 = cJSON_GetObjectItem(pRVar6,"query");
                    if ((pRVar6 != (RecoveredCJSON *)0x0) && (pRVar6->value_string != (char *)0x0))
                    {
                      ds_append(str,pRVar6->value_string);
                    }
                    output->argument_count = 1;
                    break;
                  case 0xb:
                    pRVar7 = cJSON_GetObjectItem(pRVar6,"path");
                    if ((pRVar7 != (RecoveredCJSON *)0x0) && (pRVar7->value_string != (char *)0x0))
                    {
                      ds_append(str,pRVar7->value_string);
                    }
                    pRVar7 = cJSON_GetObjectItem(pRVar6,"oldText");
                    if ((pRVar7 != (RecoveredCJSON *)0x0) && (pRVar7->value_string != (char *)0x0))
                    {
                      ds_append(output->arguments + 1,pRVar7->value_string);
                    }
                    pRVar6 = cJSON_GetObjectItem(pRVar6,"newText");
                    if ((pRVar6 != (RecoveredCJSON *)0x0) && (pRVar6->value_string != (char *)0x0))
                    {
                      ds_append(output->arguments + 2,pRVar6->value_string);
                    }
                    output->argument_count = 3;
                    break;
                  case 0xd:
                    pRVar6 = cJSON_GetObjectItem(pRVar6,"query");
                    if ((pRVar6 != (RecoveredCJSON *)0x0) && (pRVar6->value_string != (char *)0x0))
                    {
                      ds_append(str,pRVar6->value_string);
                    }
                    output->argument_count = 1;
                    break;
                  case 0xe:
                    pRVar7 = cJSON_GetObjectItem(pRVar6,"path");
                    if ((pRVar7 != (RecoveredCJSON *)0x0) && (pRVar7->value_string != (char *)0x0))
                    {
                      ds_append(str,pRVar7->value_string);
                    }
                    pRVar6 = cJSON_GetObjectItem(pRVar6,"max_depth");
                    if ((pRVar6 == (RecoveredCJSON *)0x0) || (pRVar6->value_string == (char *)0x0))
                    {
LAB_00012108:
                      iVar2 = 1;
                    }
                    else {
                      ds_append(output->arguments + 1,pRVar6->value_string);
                      iVar2 = 2;
                      if (pRVar6->value_string == (char *)0x0) goto LAB_00012108;
                    }
                    output->argument_count = iVar2;
                    break;
                  case 0xf:
                    pRVar7 = cJSON_GetObjectItem(pRVar6,"pattern");
                    if ((pRVar7 != (RecoveredCJSON *)0x0) && (pRVar7->value_string != (char *)0x0))
                    {
                      ds_append(str,pRVar7->value_string);
                    }
                    pRVar6 = cJSON_GetObjectItem(pRVar6,"max_bytes_per_file");
                    if ((pRVar6 == (RecoveredCJSON *)0x0) || (pRVar6->value_string == (char *)0x0))
                    {
LAB_0001217c:
                      iVar2 = 1;
                    }
                    else {
                      ds_append(output->arguments + 1,pRVar6->value_string);
                      iVar2 = 2;
                      if (pRVar6->value_string == (char *)0x0) goto LAB_0001217c;
                    }
                    output->argument_count = iVar2;
                  }
                }
                snprintf(output->call_id,0x40,"tc_%d",context->response->tool_call_count);
                context->response->tool_call_count = context->response->tool_call_count + 1;
                context->response->finish_reason = 1;
              }
            }
            index = index + 1;
          } while (iVar5 != index);
        }
        pRVar3 = cJSON_GetObjectItem(pRVar3,"finishReason");
        if (((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) &&
           (iVar5 = strcmp(pRVar3->value_string,"STOP"), iVar5 == 0)) {
          context->response->completion_flag = 1;
        }
        cJSON_Delete(object);
      }
    }
  }
  return;
}

/*
 * _google_send @ 0x00012238..0x000123a8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Send Google streamGenerateContent JSON, with the selected key in the request URL.
 * Parameters/return (inferred): int _google_send(RecoveredConfig *config,RecoveredConversation *conversation, RecoveredResponse *response,RecoveredStreamCallback callback,void *context, RecoveredRoute *route,int *failures)
 * Direct callers: _provider_send@0x0000a39c
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/00012238_google_send.s
 */
int google_send(RecoveredConfig *config,RecoveredConversation *conversation,
                RecoveredResponse *response,RecoveredStreamCallback callback,void *context,
                RecoveredRoute *route,int *failures)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x460] __attribute__((aligned(16)));
#define acStack_448 (*((char (*)[1024])(app_frame + 0x8))) /* original SP - 0x448 */
#define local_48 (*((RecoveredResponse * *)(app_frame + 0x408))) /* original SP - 0x48 */
#define local_44 (*((RecoveredStreamCallback *)(app_frame + 0x40c))) /* original SP - 0x44 */
#define local_40 (*((void * *)(app_frame + 0x410))) /* original SP - 0x40 */
#define local_38 (*((undefined4 *)(app_frame + 0x418))) /* original SP - 0x38 */
#define uStack_34 (*((uint *)(app_frame + 0x41c))) /* original SP - 0x34 */

  char *pcVar1;
  char *pcVar2;
  RecoveredCJSON *object;
  RecoveredCJSON *pRVar3;
  int iVar4;

  pcVar1 = config_get_url(config);
  pcVar2 = config_get_key(config);
  snprintf(acStack_448,0x400,"%s/v1beta/models/%s:streamGenerateContent?alt=sse&key=%s",
                     pcVar1,config->model,pcVar2);
  object = cJSON_CreateObject();
  pRVar3 = google_build_contents(config,conversation);
  cJSON_AddItemToObject(object,"contents",pRVar3);
  pRVar3 = google_build_tools(route,failures);
  cJSON_AddItemToObject(object,"tools",pRVar3);
  pRVar3 = cJSON_CreateObject();
  uStack_34 = config->max_tokens ^ 0x80000000;
  local_38 = 0x43300000;
  cJSON_AddNumberToObject
            (pRVar3,"maxOutputTokens",app_bits_double(CONCAT44(0x43300000,uStack_34)) - 4503601774854144.0);
  cJSON_AddNumberToObject(pRVar3,"temperature",(double)config->temperature);
  cJSON_AddItemToObject(object,"generationConfig",pRVar3);
  pcVar1 = cJSON_PrintUnformatted(object);
  cJSON_Delete(object);
  local_48 = response;
  local_44 = callback;
  local_40 = context;
  iVar4 = http_post_with_retry(acStack_448,pcVar1,(char *)0x0,(RecoveredStreamCallback)(google_sse_cb),&local_48,(int *)0x0);
  free(pcVar1);
  return iVar4;

#undef acStack_448
#undef local_48
#undef local_44
#undef local_40
#undef local_38
#undef uStack_34
}

