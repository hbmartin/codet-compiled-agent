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
 * _anthropic_build_tools_array @ 0x0000e7a4..0x0000f520
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Construct anthropic build tools array data; inspect the preserved body for exact field names and conditions.
 * Parameters/return (inferred): RecoveredCJSON * _anthropic_build_tools_array(RecoveredRoute *route,int *failures)
 * Direct callers: _anthropic_send@0x00010540
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000e7a4_anthropic_build_tools_array.s
 */
RecoveredCJSON * anthropic_build_tools_array(RecoveredRoute *route,int *failures)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0xb0] __attribute__((aligned(16)));
#define local_98 (*((char * (*)[4])(app_frame + 0x8))) /* original SP - 0x98 */
#define local_88 (*((char * *)(app_frame + 0x18))) /* original SP - 0x88 */
#define local_84 (*((char * *)(app_frame + 0x1c))) /* original SP - 0x84 */
#define local_80 (*((char * *)(app_frame + 0x20))) /* original SP - 0x80 */
#define local_7c (*((char * *)(app_frame + 0x24))) /* original SP - 0x7c */
#define local_78 (*((char * (*)[4])(app_frame + 0x28))) /* original SP - 0x78 */
#define local_68 (*((char * *)(app_frame + 0x38))) /* original SP - 0x68 */
#define local_64 (*((char * *)(app_frame + 0x3c))) /* original SP - 0x64 */
#define local_60 (*((char * *)(app_frame + 0x40))) /* original SP - 0x60 */
#define local_5c (*((char * *)(app_frame + 0x44))) /* original SP - 0x5c */
#define local_58 (*((char * *)(app_frame + 0x48))) /* original SP - 0x58 */
#define local_54 (*((char * *)(app_frame + 0x4c))) /* original SP - 0x54 */

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
  RecoveredMCPState *pRVar9;
  int iVar10;
  int iVar11;
  uint tool_type;
  char *pcVar12;
  int *piVar13;

  array = cJSON_CreateArray();
  local_98[0] = "read_file";
  local_98[1] = "write_file";
  local_98[2] = "edit_file";
  local_98[3] = "shell";
  local_88 = "glob";
  local_84 = "grep";
  local_80 = "search";
  local_7c = "read_and_edit";
  local_78[0] = 
  "Read the contents of one or more files. Multiple paths can be comma-separated. Example: path=\'src/main.c,src/config.c\'"
  ;
  local_78[1] = 
  "Write content to a file. Creates parent directories if needed. Overwrites existing files. Example: path=\'src/utils.c\', content=\'...\'"
  ;
  local_78[2] = 
  "Edit a file by replacing old text with new text. The oldText must match exactly (including whitespace). Only replaces the first occurrence. Example: path=\'src/main.c\', oldText=\'int x = 1;\', newText=\'int x = 2;\'"
  ;
  local_78[3] = 
  "Execute a shell command in the working directory. Use for listing files, running builds, checking git status, etc. Commands run with bash. Example: command=\'ls -la\'"
  ;
  local_68 = 
  "Find files matching a glob pattern in the working directory and subdirectories. Example: pattern=\'*.c\'"
  ;
  local_64 = 
  "Search file contents for a regex pattern. Searches recursively in the working directory. Example: pattern=\'TODO|FIXME\'"
  ;
  local_60 = 
  "Search the web for news, technical info, or answers requiring external knowledge. Use concise, specific queries. Example: query=\'latest PowerPC G5 news\'"
  ;
  local_5c = 
  "Read a file and then apply an edit in one step. Equivalent to read_file followed by edit_file. Args: path, oldText, newText."
  ;
  tool_type = 0;
  iVar10 = 0;
  local_58 = "ider /providers /profile /profiles /models /keys /key /url /save /clear";
  local_54 = "ider /providers /profile /profiles /models /keys /key /url /save /clear";
  do {
    if ((route == (RecoveredRoute *)0x0) ||
       ((iVar3 = router_should_send_tool
                           (tool_type,route->intent_mask,route->mentions_path,route->short_message),
        iVar3 != 0 && (iVar3 = trust_is_trusted(tool_type,failures), iVar3 != 0)))) {
      pRVar4 = cJSON_CreateObject();
      cJSON_AddStringToObject(pRVar4,"name",*(char **)((int)local_98 + iVar10));
      cJSON_AddStringToObject(pRVar4,"description",*(char **)((int)local_78 + iVar10));
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
          cJSON_AddStringToObject(pRVar7,(char *)(0x1e78 + (int)local_58),"Search query text");
          pcVar12 = "query";
        }
        else if (tool_type == 3) {
          pcVar12 = "command";
        }
        else {
          pcVar12 = (char *)(0x1de8 + (int)local_54);
        }
        cJSON_AddItemToObject(pRVar6,pcVar12,pRVar7);
        cJSON_AddItemToObject(pRVar5,"properties",pRVar6);
        pRVar6 = cJSON_CreateArray();
        pRVar7 = cJSON_CreateString(pcVar12);
        cJSON_AddItemToArray(pRVar6,pRVar7);
        cJSON_AddItemToObject(pRVar5,"required",pRVar6);
      }
      cJSON_AddItemToObject(pRVar4,"input_schema",pRVar5);
      cJSON_AddItemToArray(array,pRVar4);
    }
    iVar10 = iVar10 + 4;
    bVar1 = tool_type != 7;
    tool_type = tool_type + 1;
  } while (bVar1);
  if ((route == (RecoveredRoute *)0x0) ||
     ((iVar10 = router_should_send_tool
                          (7,route->intent_mask,route->mentions_path,route->short_message),
      iVar10 != 0 && (iVar10 = trust_is_trusted(7,failures), iVar10 != 0)))) {
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
    cJSON_AddItemToObject(pRVar4,"input_schema",pRVar5);
    cJSON_AddItemToArray(array,pRVar4);
    if (route != (RecoveredRoute *)0x0) goto LAB_0000ec54;
LAB_0000ec84:
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
    cJSON_AddItemToObject(pRVar4,"input_schema",pRVar5);
    cJSON_AddItemToArray(array,pRVar4);
    if (route != (RecoveredRoute *)0x0) goto LAB_0000ee94;
LAB_0000eec4:
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
    cJSON_AddItemToObject(pRVar4,"input_schema",pRVar5);
    cJSON_AddItemToArray(array,pRVar4);
  }
  else {
LAB_0000ec54:
    iVar10 = router_should_send_tool
                       (8,route->intent_mask,route->mentions_path,route->short_message);
    if ((iVar10 != 0) && (iVar10 = trust_is_trusted(8,failures), iVar10 != 0)) goto LAB_0000ec84;
LAB_0000ee94:
    iVar10 = router_should_send_tool
                       (0xc,route->intent_mask,route->mentions_path,route->short_message);
    if ((iVar10 != 0) && (iVar10 = trust_is_trusted(0xc,failures), iVar10 != 0)) goto LAB_0000eec4;
  }
  pRVar2 = PTR__g_mcp_000396b4;
  if (0 < PTR__g_mcp_000396b4->count) {
    iVar10 = 0;
    piVar13 = &PTR__g_mcp_000396b4->servers[0].tool_count;
    pRVar9 = PTR__g_mcp_000396b4;
    do {
      if (0 < *piVar13) {
        iVar3 = 0;
        iVar11 = 0;
        do {
          pcVar12 = pRVar9->servers[0].tools[0].name + iVar11;
          pRVar4 = cJSON_CreateObject();
          cJSON_AddStringToObject(pRVar4,"name",pcVar12);
          cJSON_AddStringToObject(pRVar4,"description",pcVar12 + 0x40);
          pRVar5 = cJSON_Parse(pcVar12 + 0x240);
          if (pRVar5 == (RecoveredCJSON *)0x0) {
            pRVar5 = cJSON_CreateObject();
          }
          cJSON_AddItemToObject(pRVar4,"input_schema",pRVar5);
          cJSON_AddItemToArray(array,pRVar4);
          iVar3 = iVar3 + 1;
          iVar11 = iVar11 + 0x1240;
        } while (iVar3 < *piVar13);
      }
      iVar10 = iVar10 + 1;
      piVar13 = piVar13 + 0x9358;
      pRVar9 = (RecoveredMCPState *)(pRVar9->servers + 1);
    } while (iVar10 < pRVar2->count);
  }
  if ((route == (RecoveredRoute *)0x0) ||
     ((iVar10 = router_should_send_tool
                          (0xd,route->intent_mask,route->mentions_path,route->short_message),
      iVar10 != 0 && (iVar10 = trust_is_trusted(0xd,failures), iVar10 != 0)))) {
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
    cJSON_AddItemToObject(pRVar4,"input_schema",pRVar5);
    cJSON_AddItemToArray(array,pRVar4);
    if (route != (RecoveredRoute *)0x0) goto LAB_0000f244;
LAB_0000f274:
    pRVar4 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar4,"name","tree");
    cJSON_AddStringToObject
              (pRVar4,"description",
               "Return a recursive directory tree with file sizes in ONE call. Use to discover all files. Skips .git, node_modules, build dirs."
              );
    pRVar5 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar5,"type","object");
    pRVar6 = cJSON_CreateObject();
    pRVar7 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar7,"type","string");
    cJSON_AddStringToObject
              (pRVar7,"description","Sub-path relative to working dir (default: project root)");
    cJSON_AddItemToObject(pRVar6,"path",pRVar7);
    pRVar7 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar7,"type","integer");
    cJSON_AddStringToObject(pRVar7,"description","Max recursion depth 1-8 (default 4)");
    cJSON_AddItemToObject(pRVar6,"max_depth",pRVar7);
    cJSON_AddItemToObject(pRVar5,"properties",pRVar6);
    cJSON_AddItemToObject(pRVar4,"input_schema",pRVar5);
    cJSON_AddItemToArray(array,pRVar4);
    if (route == (RecoveredRoute *)0x0) goto LAB_0000f3c0;
  }
  else {
LAB_0000f244:
    iVar10 = router_should_send_tool
                       (0xe,route->intent_mask,route->mentions_path,route->short_message);
    if ((iVar10 != 0) && (iVar10 = trust_is_trusted(0xe,failures), iVar10 != 0)) goto LAB_0000f274;
  }
  iVar10 = router_should_send_tool
                     (0xf,route->intent_mask,route->mentions_path,route->short_message);
  if (iVar10 == 0) {
    return array;
  }
  iVar10 = trust_is_trusted(0xf,failures);
  if (iVar10 == 0) {
    return array;
  }
LAB_0000f3c0:
  pRVar4 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar4,"name","read_project");
  cJSON_AddStringToObject
            (pRVar4,"description",
             "Atomically read every file matching a glob in ONE call. Use this to ingest a whole module or list of files without burning exploration steps. Each file is truncated to max_bytes_per_file."
            );
  pRVar5 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar5,"type","object");
  pRVar6 = cJSON_CreateObject();
  pRVar7 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar7,"type","string");
  cJSON_AddStringToObject
            (pRVar7,"description","Glob pattern relative to working dir (e.g. \'src/*.c\')");
  cJSON_AddItemToObject(pRVar6,"pattern",pRVar7);
  pRVar7 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar7,"type","integer");
  cJSON_AddStringToObject
            (pRVar7,"description","Max bytes per file before truncation (default 65536)");
  cJSON_AddItemToObject(pRVar6,"max_bytes_per_file",pRVar7);
  cJSON_AddItemToObject(pRVar5,"properties",pRVar6);
  pRVar6 = cJSON_CreateArray();
  pRVar7 = cJSON_CreateString("pattern");
  cJSON_AddItemToArray(pRVar6,pRVar7);
  cJSON_AddItemToObject(pRVar5,"required",pRVar6);
  cJSON_AddItemToObject(pRVar4,"input_schema",pRVar5);
  cJSON_AddItemToArray(array,pRVar4);
  return array;

#undef local_98
#undef local_88
#undef local_84
#undef local_80
#undef local_7c
#undef local_78
#undef local_68
#undef local_64
#undef local_60
#undef local_5c
#undef local_58
#undef local_54
}

/*
 * _anthropic_build_messages @ 0x0000f520..0x0000fad0
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Construct anthropic build messages data; inspect the preserved body for exact field names and conditions.
 * Parameters/return (inferred): RecoveredCJSON * _anthropic_build_messages(RecoveredConfig *config,RecoveredConversation *conversation)
 * Direct callers: _anthropic_send@0x00010540
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000f520_anthropic_build_messages.s
 */
RecoveredCJSON *
anthropic_build_messages(RecoveredConfig *config,RecoveredConversation *conversation)

{
  (void)config; /* Present in the recovered ABI; unused by this routine. */
  RecoveredToolCall *pRVar1;
  RecoveredCJSON *array;
  RecoveredCJSON *object;
  RecoveredCJSON *pRVar2;
  RecoveredCJSON *pRVar3;
  RecoveredCJSON *object_00;
  char *pcVar4;
  int iVar5;
  int iVar6;
  int iVar7;
  RecoveredMessage *pRVar8;
  
  array = cJSON_CreateArray();
  if (0 < conversation->message_count) {
    iVar6 = 0;
    do {
      pRVar8 = conversation->messages + iVar6;
      if (conversation->messages[iVar6].role != 0) {
        object = cJSON_CreateObject();
        if (conversation->messages[iVar6].role == 2) {
          pcVar4 = "assistant";
        }
        else {
          pcVar4 = "user";
        }
        cJSON_AddStringToObject(object,"role",pcVar4);
        if (pRVar8->role == 3) {
          pRVar2 = cJSON_CreateObject();
          cJSON_AddStringToObject(pRVar2,"type","tool_result");
          cJSON_AddStringToObject(pRVar2,"tool_use_id",pRVar8->call_id);
          pcVar4 = ds_cstr(&pRVar8->content);
          cJSON_AddStringToObject(pRVar2,"content",pcVar4);
          cJSON_AddItemToObject(object,"content",pRVar2);
        }
        else if (pRVar8->tool_call_count < 1) {
          pcVar4 = ds_cstr(&pRVar8->content);
          cJSON_AddStringToObject(object,"content",pcVar4);
        }
        else {
          pRVar2 = cJSON_CreateArray();
          if ((pRVar8->content).length != 0) {
            pRVar3 = cJSON_CreateObject();
            cJSON_AddStringToObject(pRVar3,"type","text");
            pcVar4 = ds_cstr(&pRVar8->content);
            cJSON_AddStringToObject(pRVar3,"text",pcVar4);
            cJSON_AddItemToArray(pRVar2,pRVar3);
          }
          if (0 < pRVar8->tool_call_count) {
            iVar5 = 0;
            iVar7 = 0;
            do {
              pRVar3 = cJSON_CreateObject();
              cJSON_AddStringToObject(pRVar3,"type","tool_use");
              cJSON_AddStringToObject(pRVar3,"id",pRVar8->tool_calls->call_id + iVar7);
              pcVar4 = tool_name(*(int *)(pRVar8->tool_calls->mcp_name + iVar7 + -4));
              cJSON_AddStringToObject(pRVar3,"name",pcVar4);
              object_00 = cJSON_CreateObject();
              pRVar1 = pRVar8->tool_calls;
              switch(*(undefined4 *)(pRVar1->mcp_name + iVar7 + -4)) {
              case 0:
                pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar1->arguments[0].data + iVar7));
                cJSON_AddStringToObject(object_00,"path",pcVar4);
                break;
              case 1:
                pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar1->arguments[0].data + iVar7));
                cJSON_AddStringToObject(object_00,"path",pcVar4);
                pcVar4 = ds_cstr((RecoveredDString *)
                                  ((int)&pRVar8->tool_calls->arguments[1].data + iVar7));
                cJSON_AddStringToObject(object_00,"content",pcVar4);
                break;
              case 2:
                pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar1->arguments[0].data + iVar7));
                cJSON_AddStringToObject(object_00,"path",pcVar4);
                pcVar4 = ds_cstr((RecoveredDString *)
                                  ((int)&pRVar8->tool_calls->arguments[1].data + iVar7));
                cJSON_AddStringToObject(object_00,"oldText",pcVar4);
                pcVar4 = ds_cstr((RecoveredDString *)
                                  ((int)&pRVar8->tool_calls->arguments[2].data + iVar7));
                cJSON_AddStringToObject(object_00,"newText",pcVar4);
                break;
              case 3:
                pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar1->arguments[0].data + iVar7));
                cJSON_AddStringToObject(object_00,"command",pcVar4);
                break;
              case 4:
                pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar1->arguments[0].data + iVar7));
                cJSON_AddStringToObject(object_00,"pattern",pcVar4);
                break;
              case 5:
                pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar1->arguments[0].data + iVar7));
                cJSON_AddStringToObject(object_00,"pattern",pcVar4);
                break;
              case 6:
                pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar1->arguments[0].data + iVar7));
                cJSON_AddStringToObject(object_00,"query",pcVar4);
                break;
              case 0xb:
                pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar1->arguments[0].data + iVar7));
                cJSON_AddStringToObject(object_00,"path",pcVar4);
                pcVar4 = ds_cstr((RecoveredDString *)
                                  ((int)&pRVar8->tool_calls->arguments[1].data + iVar7));
                cJSON_AddStringToObject(object_00,"oldText",pcVar4);
                pcVar4 = ds_cstr((RecoveredDString *)
                                  ((int)&pRVar8->tool_calls->arguments[2].data + iVar7));
                cJSON_AddStringToObject(object_00,"newText",pcVar4);
                break;
              case 0xc:
                pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar1->arguments[0].data + iVar7));
                cJSON_AddStringToObject(object_00,"prompt",pcVar4);
                if (1 < *(int *)(pRVar8->tool_calls->call_id + iVar7 + -4)) {
                  pcVar4 = ds_cstr((RecoveredDString *)
                                    ((int)&pRVar8->tool_calls->arguments[1].data + iVar7));
                  cJSON_AddStringToObject(object_00,"model",pcVar4);
                }
                break;
              case 0xd:
                pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar1->arguments[0].data + iVar7));
                cJSON_AddStringToObject(object_00,"query",pcVar4);
                break;
              case 0xe:
                if (**(char **)((int)&pRVar1->arguments[0].data + iVar7) != '\0') {
                  pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar1->arguments[0].data + iVar7));
                  cJSON_AddStringToObject(object_00,"path",pcVar4);
                }
                if (1 < *(int *)(pRVar8->tool_calls->call_id + iVar7 + -4)) {
                  pcVar4 = ds_cstr((RecoveredDString *)
                                    ((int)&pRVar8->tool_calls->arguments[1].data + iVar7));
                  cJSON_AddStringToObject(object_00,"max_depth",pcVar4);
                }
                break;
              case 0xf:
                pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar1->arguments[0].data + iVar7));
                cJSON_AddStringToObject(object_00,"pattern",pcVar4);
                if (1 < *(int *)(pRVar8->tool_calls->call_id + iVar7 + -4)) {
                  pcVar4 = ds_cstr((RecoveredDString *)
                                    ((int)&pRVar8->tool_calls->arguments[1].data + iVar7));
                  cJSON_AddStringToObject(object_00,"max_bytes_per_file",pcVar4);
                }
              }
              cJSON_AddItemToObject(pRVar3,"input",object_00);
              cJSON_AddItemToArray(pRVar2,pRVar3);
              iVar5 = iVar5 + 1;
              iVar7 = iVar7 + 0x51c;
            } while (iVar5 < pRVar8->tool_call_count);
          }
          cJSON_AddItemToObject(object,"content",pRVar2);
        }
        cJSON_AddItemToArray(array,object);
      }
      iVar6 = iVar6 + 1;
    } while (iVar6 < conversation->message_count);
  }
  return array;
}

/*
 * _anthropic_sse_cb @ 0x0000fad0..0x00010540
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Decode provider streaming events, accumulate response/tool-call state and invoke callbacks.
 * Parameters/return (inferred): void _anthropic_sse_cb(char *event_kind,char *data,RecoveredProviderCallbackContext *context)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000fad0_anthropic_sse_cb.s
 * Uncertainty: Indirect dispatch sites are retained with raw assembly and references; possible target sets are not assumed complete.
 */
void anthropic_sse_cb(char *event_kind,char *data,RecoveredProviderCallbackContext *context)

{
  (void)event_kind; /* Present in the recovered ABI; unused by this routine. */
  bool bVar1;
  RecoveredResponse *pRVar2;
  int iVar3;
  RecoveredCJSON *object;
  RecoveredCJSON *pRVar4;
  RecoveredCJSON *pRVar5;
  RecoveredToolCall *output;
  RecoveredDString *str;
  char *pcVar6;
  void **text;
  
  if ((data == (char *)0x0) || (iVar3 = strcmp(data,"[DONE]"), iVar3 == 0)) {
    context->response->completion_flag = 1;
    return;
  }
  object = cJSON_Parse(data);
  if (object == (RecoveredCJSON *)0x0) {
    return;
  }
  pRVar4 = cJSON_GetObjectItem(object,"type");
  if ((pRVar4 == (RecoveredCJSON *)0x0) || (pcVar6 = pRVar4->value_string, pcVar6 == (char *)0x0)) {
    cJSON_Delete(object);
    return;
  }
  iVar3 = strcmp(pcVar6,"content_block_start");
  if (iVar3 == 0) {
    pRVar4 = cJSON_GetObjectItem(object,"content_block");
    if ((((pRVar4 != (RecoveredCJSON *)0x0) &&
         (pRVar5 = cJSON_GetObjectItem(pRVar4,"type"), pRVar5 != (RecoveredCJSON *)0x0)) &&
        (pRVar5->value_string != (char *)0x0)) &&
       (iVar3 = strcmp(pRVar5->value_string,"tool_use"), iVar3 == 0)) {
      context[1].response = (RecoveredResponse *)0x1;
      pRVar5 = cJSON_GetObjectItem(pRVar4,"id");
      pRVar4 = cJSON_GetObjectItem(pRVar4,"name");
      if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
        strncpy(((char *)context + 16),pRVar5->value_string,0x3f);
      }
      if ((pRVar4 != (RecoveredCJSON *)0x0) && (pRVar4->value_string != (char *)0x0)) {
        strncpy(((char *)context + 80),pRVar4->value_string,0x1f);
      }
      ds_clear((RecoveredDString *)&context[9].callback);
    }
    goto LAB_00010524;
  }
  iVar3 = strcmp(pcVar6,"content_block_delta");
  if (iVar3 == 0) {
    pRVar4 = cJSON_GetObjectItem(object,"delta");
    if (((pRVar4 != (RecoveredCJSON *)0x0) &&
        (pRVar5 = cJSON_GetObjectItem(pRVar4,"type"), pRVar5 != (RecoveredCJSON *)0x0)) &&
       (pcVar6 = pRVar5->value_string, pcVar6 != (char *)0x0)) {
      iVar3 = strcmp(pcVar6,"text_delta");
      if (iVar3 == 0) {
        pRVar4 = cJSON_GetObjectItem(pRVar4,"text");
        if ((pRVar4 != (RecoveredCJSON *)0x0) && (pRVar4->value_string != (char *)0x0)) {
          ds_append(&context->response->content,pRVar4->value_string);
          if (context->callback != (RecoveredStreamCallback)0x0) {
            (*context->callback)((char *)0x0,pRVar4->value_string,context->context);
          }
        }
      }
      else {
        iVar3 = strcmp(pcVar6,"input_json_delta");
        if (((iVar3 == 0) &&
            (pRVar4 = cJSON_GetObjectItem(pRVar4,"partial_json"), pRVar4 != (RecoveredCJSON *)0x0))
           && (pRVar4->value_string != (char *)0x0)) {
          ds_append((RecoveredDString *)&context[9].callback,pRVar4->value_string);
        }
      }
    }
    goto LAB_00010524;
  }
  iVar3 = strcmp(pcVar6,"content_block_stop");
  if (iVar3 != 0) {
    iVar3 = strcmp(pcVar6,"message_delta");
    if (iVar3 == 0) {
      pRVar4 = cJSON_GetObjectItem(object,"delta");
      if (((pRVar4 != (RecoveredCJSON *)0x0) &&
          (pRVar4 = cJSON_GetObjectItem(pRVar4,"stop_reason"), pRVar4 != (RecoveredCJSON *)0x0)) &&
         (pcVar6 = pRVar4->value_string, pcVar6 != (char *)0x0)) {
        iVar3 = strcmp(pcVar6,"tool_use");
        if (iVar3 == 0) {
          context->response->finish_reason = 1;
        }
        else {
          iVar3 = strcmp(pcVar6,"max_tokens");
          if (iVar3 == 0) {
            context->response->finish_reason = 2;
          }
        }
      }
    }
    else {
      iVar3 = strcmp(pcVar6,"message_stop");
      if (iVar3 == 0) {
        context->response->completion_flag = 1;
        pRVar4 = cJSON_GetObjectItem(object,"usage");
        if (pRVar4 != (RecoveredCJSON *)0x0) {
          pRVar5 = cJSON_GetObjectItem(pRVar4,"input_tokens");
          pRVar4 = cJSON_GetObjectItem(pRVar4,"output_tokens");
          if (pRVar5 != (RecoveredCJSON *)0x0) {
            context->response->input_tokens = pRVar5->value_int;
          }
          if (pRVar4 != (RecoveredCJSON *)0x0) {
            context->response->output_tokens = pRVar4->value_int;
          }
          pRVar2 = context->response;
          pRVar2->total_tokens = pRVar2->input_tokens + pRVar2->output_tokens;
          context->response->usage_known = 1;
        }
      }
    }
    goto LAB_00010524;
  }
  if (context[1].response == (RecoveredResponse *)0x0) goto LAB_00010524;
  iVar3 = context->response->tool_call_count;
  if (iVar3 < 99) {
    output = context->response->tool_calls + iVar3;
    memset(output,0,0x51c);
    ds_init(&output->result);
    iVar3 = 0;
    str = output->arguments;
    do {
      ds_init((RecoveredDString *)((int)&str->data + iVar3));
      bVar1 = iVar3 != 0x498;
      iVar3 = iVar3 + 0xc;
    } while (bVar1);
    strncpy(output->call_id,((char *)context + 16),0x3f);
    text = &context[6].context;
    strncpy(output->mcp_name,(char *)text,0x1f);
    iVar3 = strcmp((char *)text,"read_file");
    if (iVar3 == 0) {
      output->tool_type = 0;
    }
    else {
      iVar3 = strcmp((char *)text,"write_file");
      if (iVar3 == 0) {
        output->tool_type = 1;
      }
      else {
        iVar3 = strcmp((char *)text,"edit_file");
        if (iVar3 == 0) {
          output->tool_type = 2;
        }
        else {
          iVar3 = strcmp((char *)text,"shell");
          if (iVar3 == 0) {
            output->tool_type = 3;
          }
          else {
            iVar3 = strcmp((char *)text,"glob");
            if (iVar3 == 0) {
              output->tool_type = 4;
            }
            else {
              iVar3 = strcmp((char *)text,"grep");
              if (iVar3 == 0) {
                output->tool_type = 5;
              }
              else {
                iVar3 = strcmp((char *)text,"search");
                if (iVar3 == 0) {
                  output->tool_type = 6;
                }
                else {
                  iVar3 = strcmp((char *)text,"read_and_edit");
                  if (iVar3 == 0) {
                    output->tool_type = 0xb;
                  }
                  else {
                    iVar3 = strcmp((char *)text,"reference");
                    if (iVar3 == 0) {
                      output->tool_type = 0xd;
                    }
                    else {
                      iVar3 = strcmp((char *)text,"tree");
                      if (iVar3 == 0) {
                        output->tool_type = 0xe;
                      }
                      else {
                        iVar3 = strcmp((char *)text,"read_project");
                        if (iVar3 == 0) {
                          output->tool_type = 0xf;
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
    if (context[9].context != (void *)0x0) {
      pcVar6 = ds_cstr((RecoveredDString *)&context[9].callback);
      pRVar4 = cJSON_Parse(pcVar6);
      if (pRVar4 != (RecoveredCJSON *)0x0) {
        switch(output->tool_type) {
        case 0:
          pRVar5 = cJSON_GetObjectItem(pRVar4,"path");
          if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
            ds_append(str,pRVar5->value_string);
          }
          output->argument_count = 1;
          break;
        case 1:
          pRVar5 = cJSON_GetObjectItem(pRVar4,"path");
          if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
            ds_append(str,pRVar5->value_string);
          }
          pRVar5 = cJSON_GetObjectItem(pRVar4,"content");
          if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
            ds_append(output->arguments + 1,pRVar5->value_string);
          }
          output->argument_count = 2;
          break;
        case 2:
          pRVar5 = cJSON_GetObjectItem(pRVar4,"path");
          if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
            ds_append(str,pRVar5->value_string);
          }
          pRVar5 = cJSON_GetObjectItem(pRVar4,"oldText");
          if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
            ds_append(output->arguments + 1,pRVar5->value_string);
          }
          pRVar5 = cJSON_GetObjectItem(pRVar4,"newText");
          if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
            ds_append(output->arguments + 2,pRVar5->value_string);
          }
          output->argument_count = 3;
          break;
        case 3:
          pRVar5 = cJSON_GetObjectItem(pRVar4,"command");
          if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
            ds_append(str,pRVar5->value_string);
          }
          output->argument_count = 1;
          break;
        case 4:
          pRVar5 = cJSON_GetObjectItem(pRVar4,"pattern");
          if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
            ds_append(str,pRVar5->value_string);
          }
          output->argument_count = 1;
          break;
        case 5:
          pRVar5 = cJSON_GetObjectItem(pRVar4,"pattern");
          if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
            ds_append(str,pRVar5->value_string);
          }
          output->argument_count = 1;
          break;
        case 6:
          pRVar5 = cJSON_GetObjectItem(pRVar4,"query");
          if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
            ds_append(str,pRVar5->value_string);
          }
          output->argument_count = 1;
          break;
        case 0xb:
          pRVar5 = cJSON_GetObjectItem(pRVar4,"path");
          if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
            ds_append(str,pRVar5->value_string);
          }
          pRVar5 = cJSON_GetObjectItem(pRVar4,"oldText");
          if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
            ds_append(output->arguments + 1,pRVar5->value_string);
          }
          pRVar5 = cJSON_GetObjectItem(pRVar4,"newText");
          if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
            ds_append(output->arguments + 2,pRVar5->value_string);
          }
          output->argument_count = 3;
          break;
        case 0xd:
          pRVar5 = cJSON_GetObjectItem(pRVar4,"query");
          if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
            ds_append(str,pRVar5->value_string);
          }
          output->argument_count = 1;
          break;
        case 0xe:
          pRVar5 = cJSON_GetObjectItem(pRVar4,"path");
          if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
            ds_append(str,pRVar5->value_string);
          }
          pRVar5 = cJSON_GetObjectItem(pRVar4,"max_depth");
          if ((pRVar5 == (RecoveredCJSON *)0x0) || (pRVar5->value_string == (char *)0x0)) {
LAB_00010320:
            iVar3 = 1;
          }
          else {
            ds_append(output->arguments + 1,pRVar5->value_string);
            iVar3 = 2;
            if (pRVar5->value_string == (char *)0x0) goto LAB_00010320;
          }
          output->argument_count = iVar3;
          break;
        case 0xf:
          pRVar5 = cJSON_GetObjectItem(pRVar4,"pattern");
          if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
            ds_append(str,pRVar5->value_string);
          }
          pRVar5 = cJSON_GetObjectItem(pRVar4,"max_bytes_per_file");
          if ((pRVar5 == (RecoveredCJSON *)0x0) || (pRVar5->value_string == (char *)0x0)) {
LAB_00010394:
            iVar3 = 1;
          }
          else {
            ds_append(output->arguments + 1,pRVar5->value_string);
            iVar3 = 2;
            if (pRVar5->value_string == (char *)0x0) goto LAB_00010394;
          }
          output->argument_count = iVar3;
        }
        cJSON_Delete(pRVar4);
      }
    }
    context->response->tool_call_count = context->response->tool_call_count + 1;
  }
  context[1].response = (RecoveredResponse *)0x0;
LAB_00010524:
  cJSON_Delete(object);
  return;
}

/*
 * _anthropic_send @ 0x00010540..0x00010768
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Send Anthropic messages/tools JSON with the configured x-api-key and streaming callback.
 * Parameters/return (inferred): int _anthropic_send(RecoveredConfig *config,RecoveredConversation *conversation, RecoveredResponse *response,RecoveredStreamCallback callback,void *context, RecoveredRoute *route,int *failures)
 * Direct callers: _provider_send@0x0000a39c
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/00010540_anthropic_send.s
 */
int anthropic_send(RecoveredConfig *config,RecoveredConversation *conversation,
                   RecoveredResponse *response,RecoveredStreamCallback callback,void *context,
                   RecoveredRoute *route,int *failures)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x16d0] __attribute__((aligned(16)));
#define acStack_16b8 (*((char (*)[1024])(app_frame + 0x8))) /* original SP - 0x16b8 */
#define acStack_12b8 (*((char (*)[512])(app_frame + 0x408))) /* original SP - 0x12b8 */
#define local_10b8 (*((RecoveredResponse * *)(app_frame + 0x608))) /* original SP - 0x10b8 */
#define local_10b4 (*((RecoveredStreamCallback *)(app_frame + 0x60c))) /* original SP - 0x10b4 */
#define local_10b0 (*((void * *)(app_frame + 0x610))) /* original SP - 0x10b0 */
#define RStack_1048 (*((RecoveredDString *)(app_frame + 0x678))) /* original SP - 0x1048 */
#define local_103c (*((char *)(app_frame + 0x684))) /* original SP - 0x103c */
#define auStack_103b (*((undefined1 (*)[4099])(app_frame + 0x685))) /* original SP - 0x103b */
#define local_38 (*((undefined4 *)(app_frame + 0x1688))) /* original SP - 0x38 */
#define uStack_34 (*((uint *)(app_frame + 0x168c))) /* original SP - 0x34 */

  char *pcVar1;
  RecoveredCJSON *object;
  RecoveredCJSON *pRVar2;
  char *pcVar3;
  uint uVar4;

  pcVar1 = config_get_url(config);
  snprintf(acStack_16b8,0x400,"%s/v1/messages",pcVar1);
  object = cJSON_CreateObject();
  cJSON_AddStringToObject(object,"model",config->model);
  uStack_34 = config->max_tokens ^ 0x80000000;
  local_38 = 0x43300000;
  cJSON_AddNumberToObject
            (object,"max_tokens",app_bits_double(CONCAT44(0x43300000,uStack_34)) - 4503601774854144.0);
  cJSON_AddNumberToObject(object,"temperature",(double)config->temperature);
  cJSON_AddBoolToObject(object,"stream",1);
  if ((config->system_prompt).length != 0) {
    pcVar1 = ds_cstr(&config->system_prompt);
    cJSON_AddStringToObject(object,"system",pcVar1);
  }
  pRVar2 = anthropic_build_messages(config,conversation);
  cJSON_AddItemToObject(object,"messages",pRVar2);
  pRVar2 = anthropic_build_tools_array(route,failures);
  cJSON_AddItemToObject(object,"tools",pRVar2);
  pcVar1 = cJSON_PrintUnformatted(object);
  cJSON_Delete(object);
  pcVar3 = config_get_key(config);
  snprintf(acStack_12b8,0x200,"x-api-key: %s\r\nanthropic-version: 2023-06-01",pcVar3);
  memset(&local_10b8,0,0x7c);
  ds_init(&RStack_1048);
  local_103c = '\0';
  local_10b8 = response;
  local_10b4 = callback;
  local_10b0 = context;
  memset(auStack_103b,0,0xfff);
  uVar4 = http_post_with_retry2
                    (acStack_16b8,pcVar1,acStack_12b8,(RecoveredStreamCallback)(anthropic_sse_cb),&local_10b8,&local_103c,
                     0x1000,(int *)0x0);
  free(pcVar1);
  ds_free(&RStack_1048);
  if ((399 < (int)uVar4) && (local_103c != '\0')) {
    ds_append(&response->error_or_reasoning,&local_103c);
    response->unknown_18 = uVar4;
  }
  return uVar4;

#undef acStack_16b8
#undef acStack_12b8
#undef local_10b8
#undef local_10b4
#undef local_10b0
#undef RStack_1048
#undef local_103c
#undef auStack_103b
#undef local_38
#undef uStack_34
}

