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
 * _add_str_prop @ 0x0000ca04..0x0000ca7c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in provider_openai: add str prop. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _add_str_prop(RecoveredCJSON *properties,char *name,char *description)
 * Direct callers: _add_tool@0x0000cbd0
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000ca04_add_str_prop.s
 */
void add_str_prop(RecoveredCJSON *properties,char *name,char *description)

{
  RecoveredCJSON *object;
  
  object = cJSON_CreateObject();
  cJSON_AddStringToObject(object,"type","string");
  cJSON_AddStringToObject(object,"description",description);
  cJSON_AddItemToObject(properties,name,object);
  return;
}

/*
 * _add_required @ 0x0000ca7c..0x0000caf4
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in provider_openai: add required. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _add_required(RecoveredCJSON *required,char *name)
 * Direct callers: _add_tool@0x0000cbd0
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000ca7c_add_required.s
 */
void add_required(RecoveredCJSON *required,char *name)

{
  RecoveredCJSON *item;
  RecoveredCJSON *item_00;
  
  item = cJSON_GetObjectItem(required,"required");
  if (item == (RecoveredCJSON *)0x0) {
    item = cJSON_CreateArray();
    cJSON_AddItemToObject(required,"required",item);
  }
  item_00 = cJSON_CreateString(name);
  cJSON_AddItemToArray(item,item_00);
  return;
}

/*
 * _make_tool @ 0x0000caf4..0x0000cbd0
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in provider_openai: make tool. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): RecoveredCJSON * _make_tool(char *name,char *description)
 * Direct callers: _add_tool@0x0000cbd0
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000caf4_make_tool.s
 */
RecoveredCJSON * make_tool(char *name,char *description)

{
  RecoveredCJSON *object;
  RecoveredCJSON *object_00;
  RecoveredCJSON *object_01;
  RecoveredCJSON *item;
  
  object = cJSON_CreateObject();
  cJSON_AddStringToObject(object,"type","function");
  object_00 = cJSON_CreateObject();
  cJSON_AddStringToObject(object_00,"name",name);
  cJSON_AddStringToObject(object_00,"description",description);
  object_01 = cJSON_CreateObject();
  cJSON_AddStringToObject(object_01,"type","object");
  item = cJSON_CreateObject();
  cJSON_AddItemToObject(object_01,"properties",item);
  cJSON_AddItemToObject(object_00,"parameters",object_01);
  cJSON_AddItemToObject(object,"function",object_00);
  return object;
}

/*
 * _add_tool @ 0x0000cbd0..0x0000ccb8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in provider_openai: add tool. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): RecoveredCJSON * _add_tool(RecoveredCJSON *array,char *name,char *description,char **argument_names, char **argument_descriptions,int argument_count,char **required_names,int required_count)
 * Direct callers: _openai_build_tools_array@0x0000ccb8
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000cbd0_add_tool.s
 */
RecoveredCJSON *
add_tool(RecoveredCJSON *array,char *name,char *description,char **argument_names,
         char **argument_descriptions,int argument_count,char **required_names,int required_count)

{
  RecoveredCJSON *object;
  RecoveredCJSON *pRVar1;
  RecoveredCJSON *properties;
  int iVar2;
  int iVar3;
  
  object = make_tool(name,description);
  pRVar1 = cJSON_GetObjectItem(object,"function");
  pRVar1 = cJSON_GetObjectItem(pRVar1,"parameters");
  properties = cJSON_GetObjectItem(pRVar1,"properties");
  if (0 < argument_count) {
    iVar2 = 0;
    iVar3 = 0;
    do {
      add_str_prop(properties,*(char **)((int)argument_names + iVar3),
                    *(char **)((int)argument_descriptions + iVar3));
      iVar2 = iVar2 + 1;
      iVar3 = iVar3 + 4;
    } while (argument_count != iVar2);
  }
  if (0 < required_count) {
    iVar2 = 0;
    do {
      add_required(pRVar1,required_names[iVar2]);
      iVar2 = iVar2 + 1;
    } while (required_count != iVar2);
  }
  cJSON_AddItemToArray(array,object);
  return object;
}

/*
 * _openai_build_tools_array @ 0x0000ccb8..0x0000dd28
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Construct openai build tools array data; inspect the preserved body for exact field names and conditions.
 * Parameters/return (inferred): RecoveredCJSON * _openai_build_tools_array(RecoveredRoute *route,int *failures)
 * Direct callers: _ollama_build_request@0x0000bdf0, _openai_send@0x0000e5ec
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000ccb8_openai_build_tools_array.s
 */
RecoveredCJSON * openai_build_tools_array(RecoveredRoute *route,int *failures)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0xa0] __attribute__((aligned(16)));
#define local_88 (*((char * *)(app_frame + 0x8))) /* original SP - 0x88 */
#define local_84 (*((char * *)(app_frame + 0xc))) /* original SP - 0x84 */
#define local_80 (*((char * *)(app_frame + 0x10))) /* original SP - 0x80 */
#define local_7c (*((char * *)(app_frame + 0x14))) /* original SP - 0x7c */
#define local_78 (*((char * *)(app_frame + 0x18))) /* original SP - 0x78 */
#define local_74 (*((char * *)(app_frame + 0x1c))) /* original SP - 0x74 */
#define local_70 (*((char * *)(app_frame + 0x20))) /* original SP - 0x70 */
#define local_6c (*((char * *)(app_frame + 0x24))) /* original SP - 0x6c */
#define local_68 (*((char * *)(app_frame + 0x28))) /* original SP - 0x68 */
#define local_58 (*((int *)(app_frame + 0x38))) /* original SP - 0x58 */
#define local_54 (*((int *)(app_frame + 0x3c))) /* original SP - 0x54 */

  RecoveredMCPState *pRVar1;
  int iVar2;
  int iVar3;
  RecoveredCJSON *array;
  int iVar4;
  RecoveredCJSON *pRVar5;
  RecoveredCJSON *pRVar6;
  RecoveredCJSON *pRVar7;
  RecoveredCJSON *pRVar8;
  RecoveredCJSON *pRVar9;
  RecoveredCJSON *array_00;
  RecoveredCJSON *pRVar10;
  int *piVar11;
  int iVar12;
  int iVar13;
  char *value;
  RecoveredMCPState *pRVar14;

  array = cJSON_CreateArray();
  local_88 = "path";
  local_70 = "File path to read";
  local_7c = "path";
  if (route == (RecoveredRoute *)0x0) {
LAB_0000cd34:
    add_tool(array,"read_file","Read the contents of a file. Example: path=\'src/main.c\'",
              &local_88,&local_70,1,&local_7c,1);
  }
  else {
    iVar4 = router_should_send_tool(0,route->intent_mask,route->mentions_path,route->short_message)
    ;
    if (iVar4 != 0) {
      iVar4 = trust_is_trusted(0,failures);
      if (iVar4 != 0) goto LAB_0000cd34;
    }
  }
  local_88 = "path";
  local_84 = "content";
  local_70 = "File path to write";
  local_6c = "Content to write to the file";
  local_7c = "path";
  local_78 = "content";
  if (route == (RecoveredRoute *)0x0) {
LAB_0000cde0:
    add_tool(array,"write_file",
              "Write content to a file. Creates parent directories if needed. Overwrites if file exists."
              ,&local_88,&local_70,2,&local_7c,2);
  }
  else {
    iVar4 = router_should_send_tool(1,route->intent_mask,route->mentions_path,route->short_message)
    ;
    if (iVar4 != 0) {
      iVar4 = trust_is_trusted(1,failures);
      if (iVar4 != 0) goto LAB_0000cde0;
    }
  }
  local_7c = "path";
  local_78 = "oldText";
  local_74 = "newText";
  local_70 = "File path to edit";
  local_6c = "Exact text to replace (must match whitespace exactly)";
  local_68 = "New replacement text";
  local_88 = "path";
  local_84 = "oldText";
  local_80 = "newText";
  if (route == (RecoveredRoute *)0x0) {
LAB_0000cea4:
    add_tool(array,"edit_file",
              "Edit a file by replacing old text with new text. Only replaces the first occurrence."
              ,&local_7c,&local_70,3,&local_88,3);
  }
  else {
    iVar4 = router_should_send_tool(2,route->intent_mask,route->mentions_path,route->short_message)
    ;
    if (iVar4 != 0) {
      iVar4 = trust_is_trusted(2,failures);
      if (iVar4 != 0) goto LAB_0000cea4;
    }
  }
  local_88 = "command";
  local_70 = "Command to run in the shell";
  local_7c = "command";
  if (route == (RecoveredRoute *)0x0) {
LAB_0000cf24:
    add_tool(array,"shell",
              "Execute a shell command. Use for listing files, running builds, git, etc.",&local_88,
              &local_70,1,&local_7c,1);
  }
  else {
    iVar4 = router_should_send_tool(3,route->intent_mask,route->mentions_path,route->short_message)
    ;
    if (iVar4 != 0) {
      iVar4 = trust_is_trusted(3,failures);
      if (iVar4 != 0) goto LAB_0000cf24;
    }
  }
  local_7c = "pattern";
  local_70 = "Glob pattern (e.g. \'*.c\' or \'src/**/*.h\')";
  local_88 = "pattern";
  if (route == (RecoveredRoute *)0x0) {
LAB_0000cfa4:
    add_tool(array,"glob","Find files matching a glob pattern recursively.",&local_7c,&local_70,1,
              &local_88,1);
  }
  else {
    iVar4 = router_should_send_tool(4,route->intent_mask,route->mentions_path,route->short_message)
    ;
    if (iVar4 != 0) {
      iVar4 = trust_is_trusted(4,failures);
      if (iVar4 != 0) goto LAB_0000cfa4;
    }
  }
  local_88 = "pattern";
  local_70 = "Regex pattern to search for in file contents";
  local_7c = "pattern";
  if (route == (RecoveredRoute *)0x0) {
LAB_0000d024:
    add_tool(array,"grep","Search file contents for a regex pattern recursively.",&local_88,
              &local_70,1,&local_7c,1);
  }
  else {
    iVar4 = router_should_send_tool(5,route->intent_mask,route->mentions_path,route->short_message)
    ;
    if (iVar4 != 0) {
      iVar4 = trust_is_trusted(5,failures);
      if (iVar4 != 0) goto LAB_0000d024;
    }
  }
  local_7c = "query";
  local_70 = "Search query text";
  local_88 = "query";
  if (route == (RecoveredRoute *)0x0) {
LAB_0000d0a4:
    add_tool(array,"search","Search the web for news, technical info, or external knowledge.",
              &local_7c,&local_70,1,&local_88,1);
  }
  else {
    iVar4 = router_should_send_tool(6,route->intent_mask,route->mentions_path,route->short_message)
    ;
    if (iVar4 != 0) {
      iVar4 = trust_is_trusted(6,failures);
      if (iVar4 != 0) goto LAB_0000d0a4;
    }
  }
  local_88 = "path";
  local_84 = "oldText";
  local_80 = "newText";
  local_70 = "File path";
  local_6c = "Exact text to replace";
  local_68 = "New replacement text";
  local_7c = "path";
  local_78 = "oldText";
  local_74 = "newText";
  if (route == (RecoveredRoute *)0x0) {
LAB_0000d168:
    add_tool(array,"read_and_edit","Read a file then apply an edit in one step.",&local_88,
              &local_70,3,&local_7c,3);
    if (route != (RecoveredRoute *)0x0) goto LAB_0000d19c;
LAB_0000d1cc:
    pRVar5 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar5,"type","function");
    pRVar6 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar6,"name","fetch_url");
    cJSON_AddStringToObject
              (pRVar6,"description",
               "Fetch the text content of a web URL. Convert HTML to readable text.");
    pRVar7 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar7,"type","object");
    pRVar8 = cJSON_CreateObject();
    pRVar9 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar9,"type","string");
    cJSON_AddStringToObject(pRVar9,"description","Full URL to fetch");
    cJSON_AddItemToObject(pRVar8,"url",pRVar9);
    cJSON_AddItemToObject(pRVar7,"properties",pRVar8);
    pRVar8 = cJSON_CreateArray();
    pRVar9 = cJSON_CreateString("url");
    cJSON_AddItemToArray(pRVar8,pRVar9);
    cJSON_AddItemToObject(pRVar7,"required",pRVar8);
    cJSON_AddItemToObject(pRVar6,"parameters",pRVar7);
    cJSON_AddItemToObject(pRVar5,"function",pRVar6);
    cJSON_AddItemToArray(array,pRVar5);
    if (route != (RecoveredRoute *)0x0) goto LAB_0000d308;
LAB_0000d338:
    pRVar5 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar5,"type","function");
    pRVar6 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar6,"name","tree");
    cJSON_AddStringToObject
              (pRVar6,"description",
               "Return a recursive directory tree of the project (or a sub-path) with file sizes in ONE call. Use this to discover all files before deciding what to read. Skips .git, node_modules, build dirs."
              );
    pRVar7 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar7,"type","object");
    pRVar8 = cJSON_CreateObject();
    pRVar9 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar9,"type","string");
    cJSON_AddStringToObject
              (pRVar9,"description","Sub-path relative to working dir (default: project root)");
    cJSON_AddItemToObject(pRVar8,"path",pRVar9);
    pRVar9 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar9,"type","integer");
    cJSON_AddStringToObject(pRVar9,"description","Max recursion depth 1-8 (default 4)");
    cJSON_AddItemToObject(pRVar8,"max_depth",pRVar9);
    cJSON_AddItemToObject(pRVar7,"properties",pRVar8);
    cJSON_AddItemToObject(pRVar6,"parameters",pRVar7);
    cJSON_AddItemToObject(pRVar5,"function",pRVar6);
    cJSON_AddItemToArray(array,pRVar5);
    if (route != (RecoveredRoute *)0x0) goto LAB_0000d480;
LAB_0000d4b0:
    pRVar5 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar5,"type","function");
    pRVar6 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar6,"name","read_project");
    cJSON_AddStringToObject
              (pRVar6,"description",
               "Atomically read every file matching a glob (e.g. \'src/*.c\', \'**/*.h\') and return their full contents in ONE response. Use this to ingest a whole module, codebase section, or list of files without burning exploration steps. Each file is truncated to max_bytes_per_file."
              );
    pRVar7 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar7,"type","object");
    pRVar8 = cJSON_CreateObject();
    pRVar9 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar9,"type","string");
    cJSON_AddStringToObject
              (pRVar9,"description",
               "Glob pattern relative to working dir (e.g. \'src/*.c\', \'**/*.h\')");
    cJSON_AddItemToObject(pRVar8,"pattern",pRVar9);
    pRVar9 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar9,"type","integer");
    cJSON_AddStringToObject
              (pRVar9,"description","Max bytes to read per file before truncation (default 65536)");
    cJSON_AddItemToObject(pRVar8,"max_bytes_per_file",pRVar9);
    cJSON_AddItemToObject(pRVar7,"properties",pRVar8);
    pRVar8 = cJSON_CreateArray();
    pRVar9 = cJSON_CreateString("pattern");
    cJSON_AddItemToArray(pRVar8,pRVar9);
    cJSON_AddItemToObject(pRVar7,"required",pRVar8);
    cJSON_AddItemToObject(pRVar6,"parameters",pRVar7);
    cJSON_AddItemToObject(pRVar5,"function",pRVar6);
    cJSON_AddItemToArray(array,pRVar5);
    if (route != (RecoveredRoute *)0x0) goto LAB_0000d62c;
LAB_0000d65c:
    pRVar5 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar5,"type","function");
    pRVar6 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar6,"name","db_query");
    cJSON_AddStringToObject
              (pRVar6,"description",
               "Query or modify a SQLite database. Supports SELECT, INSERT, UPDATE, DELETE, and PRAGMA."
              );
    pRVar7 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar7,"type","object");
    pRVar8 = cJSON_CreateObject();
    pRVar9 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar9,"type","string");
    cJSON_AddStringToObject(pRVar9,"description","Absolute path to .sqlite or .db file");
    cJSON_AddItemToObject(pRVar8,"db_path",pRVar9);
    pRVar9 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar9,"type","string");
    array_00 = cJSON_CreateArray();
    pRVar10 = cJSON_CreateString("query");
    cJSON_AddItemToArray(array_00,pRVar10);
    pRVar10 = cJSON_CreateString("tables");
    cJSON_AddItemToArray(array_00,pRVar10);
    pRVar10 = cJSON_CreateString("schema");
    cJSON_AddItemToArray(array_00,pRVar10);
    pRVar10 = cJSON_CreateString("insert");
    cJSON_AddItemToArray(array_00,pRVar10);
    pRVar10 = cJSON_CreateString("update");
    cJSON_AddItemToArray(array_00,pRVar10);
    pRVar10 = cJSON_CreateString("delete");
    cJSON_AddItemToArray(array_00,pRVar10);
    cJSON_AddItemToObject(pRVar9,"enum",array_00);
    cJSON_AddStringToObject(pRVar9,"description","Operation type");
    cJSON_AddItemToObject(pRVar8,"operation",pRVar9);
    pRVar9 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar9,"type","string");
    cJSON_AddStringToObject
              (pRVar9,"description","SQL statement (required for query/insert/update/delete/schema)"
              );
    cJSON_AddItemToObject(pRVar8,"sql",pRVar9);
    cJSON_AddItemToObject(pRVar7,"properties",pRVar8);
    pRVar8 = cJSON_CreateArray();
    pRVar9 = cJSON_CreateString("db_path");
    cJSON_AddItemToArray(pRVar8,pRVar9);
    pRVar9 = cJSON_CreateString("operation");
    cJSON_AddItemToArray(pRVar8,pRVar9);
    cJSON_AddItemToObject(pRVar7,"required",pRVar8);
    cJSON_AddItemToObject(pRVar6,"parameters",pRVar7);
    cJSON_AddItemToObject(pRVar5,"function",pRVar6);
    cJSON_AddItemToArray(array,pRVar5);
    if (route != (RecoveredRoute *)0x0) goto LAB_0000d8d8;
LAB_0000d908:
    pRVar5 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar5,"type","function");
    pRVar6 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar6,"name","subagent");
    cJSON_AddStringToObject
              (pRVar6,"description",
               "Launch a sub-agent to handle a multi-step task. The sub-agent will run in a separate process with up to 5 tool-calling turns and return a summarized result. Use for complex subtasks that benefit from focused iteration. Returns the final text result."
              );
    pRVar7 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar7,"type","object");
    pRVar8 = cJSON_CreateObject();
    pRVar9 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar9,"type","string");
    cJSON_AddStringToObject(pRVar9,"description","Task description for the sub-agent.");
    cJSON_AddItemToObject(pRVar8,"prompt",pRVar9);
    pRVar9 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar9,"type","string");
    cJSON_AddStringToObject(pRVar9,"description","Model name (optional).");
    cJSON_AddItemToObject(pRVar8,"model",pRVar9);
    cJSON_AddItemToObject(pRVar7,"properties",pRVar8);
    pRVar8 = cJSON_CreateArray();
    pRVar9 = cJSON_CreateString("prompt");
    cJSON_AddItemToArray(pRVar8,pRVar9);
    cJSON_AddItemToObject(pRVar7,"required",pRVar8);
    cJSON_AddItemToObject(pRVar6,"parameters",pRVar7);
    cJSON_AddItemToObject(pRVar5,"function",pRVar6);
    cJSON_AddItemToArray(array,pRVar5);
    if (route != (RecoveredRoute *)0x0) goto LAB_0000da84;
  }
  else {
    iVar4 = router_should_send_tool
                      (0xb,route->intent_mask,route->mentions_path,route->short_message);
    if (iVar4 != 0) {
      iVar4 = trust_is_trusted(0xb,failures);
      if (iVar4 != 0) goto LAB_0000d168;
    }
LAB_0000d19c:
    iVar4 = router_should_send_tool(7,route->intent_mask,route->mentions_path,route->short_message)
    ;
    if (iVar4 != 0) {
      iVar4 = trust_is_trusted(7,failures);
      if (iVar4 != 0) goto LAB_0000d1cc;
    }
LAB_0000d308:
    iVar4 = router_should_send_tool
                      (0xe,route->intent_mask,route->mentions_path,route->short_message);
    if (iVar4 != 0) {
      iVar4 = trust_is_trusted(0xe,failures);
      if (iVar4 != 0) goto LAB_0000d338;
    }
LAB_0000d480:
    iVar4 = router_should_send_tool
                      (0xf,route->intent_mask,route->mentions_path,route->short_message);
    if (iVar4 != 0) {
      iVar4 = trust_is_trusted(0xf,failures);
      if (iVar4 != 0) goto LAB_0000d4b0;
    }
LAB_0000d62c:
    iVar4 = router_should_send_tool(8,route->intent_mask,route->mentions_path,route->short_message)
    ;
    if (iVar4 != 0) {
      iVar4 = trust_is_trusted(8,failures);
      if (iVar4 != 0) goto LAB_0000d65c;
    }
LAB_0000d8d8:
    iVar4 = router_should_send_tool
                      (0xc,route->intent_mask,route->mentions_path,route->short_message);
    if (iVar4 != 0) {
      iVar4 = trust_is_trusted(0xc,failures);
      if (iVar4 != 0) goto LAB_0000d908;
    }
LAB_0000da84:
    iVar4 = router_should_send_tool
                      (0xd,route->intent_mask,route->mentions_path,route->short_message);
    if (iVar4 == 0) goto LAB_0000dbe8;
    iVar4 = trust_is_trusted(0xd,failures);
    if (iVar4 == 0) goto LAB_0000dbe8;
  }
  pRVar5 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar5,"type","function");
  pRVar6 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar6,"name","reference");
  cJSON_AddStringToObject
            (pRVar6,"description",
             "Search the Tiger OS X documentation database for API references, function signatures, and programming guides. Use when user asks about Carbon, CoreFoundation, IOKit, or other Mac-specific APIs."
            );
  pRVar7 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar7,"type","object");
  pRVar8 = cJSON_CreateObject();
  pRVar9 = cJSON_CreateObject();
  cJSON_AddStringToObject(pRVar9,"type","string");
  cJSON_AddStringToObject
            (pRVar9,"description",
             "What to search for (e.g., \'CFPreferences\', \'NSWindow setTitle\', \'IOKit matching\')"
            );
  cJSON_AddItemToObject(pRVar8,"query",pRVar9);
  cJSON_AddItemToObject(pRVar7,"properties",pRVar8);
  pRVar8 = cJSON_CreateArray();
  pRVar9 = cJSON_CreateString("query");
  cJSON_AddItemToArray(pRVar8,pRVar9);
  cJSON_AddItemToObject(pRVar7,"required",pRVar8);
  cJSON_AddItemToObject(pRVar6,"parameters",pRVar7);
  cJSON_AddItemToObject(pRVar5,"function",pRVar6);
  cJSON_AddItemToArray(array,pRVar5);
LAB_0000dbe8:
  pRVar1 = PTR__g_mcp_000396b4;
  if (0 < PTR__g_mcp_000396b4->count) {
    iVar4 = 0;
    piVar11 = &PTR__g_mcp_000396b4->servers[0].tool_count;
    local_58 = 0x2cccc;
    local_54 = 0x2cccc;
    pRVar14 = PTR__g_mcp_000396b4;
    do {
      iVar3 = local_54;
      iVar2 = local_58;
      if (0 < *piVar11) {
        iVar12 = 0;
        iVar13 = 0;
        do {
          value = pRVar14->servers[0].tools[0].name + iVar13;
          pRVar5 = cJSON_CreateObject();
          cJSON_AddStringToObject(pRVar5,"type","function");
          pRVar6 = cJSON_CreateObject();
          cJSON_AddStringToObject(pRVar6,(char *)(iVar2 + 0x37e0),value);
          cJSON_AddStringToObject(pRVar6,(char *)(iVar3 + 0x3964),value + 0x40);
          pRVar7 = cJSON_Parse(value + 0x240);
          if (pRVar7 == (RecoveredCJSON *)0x0) {
            pRVar7 = cJSON_CreateObject();
          }
          cJSON_AddItemToObject(pRVar6,"parameters",pRVar7);
          cJSON_AddItemToObject(pRVar5,"function",pRVar6);
          cJSON_AddItemToArray(array,pRVar5);
          iVar12 = iVar12 + 1;
          iVar13 = iVar13 + 0x1240;
        } while (iVar12 < *piVar11);
      }
      iVar4 = iVar4 + 1;
      piVar11 = piVar11 + 0x9358;
      pRVar14 = (RecoveredMCPState *)(pRVar14->servers + 1);
    } while (iVar4 < pRVar1->count);
  }
  return array;

#undef local_88
#undef local_84
#undef local_80
#undef local_7c
#undef local_78
#undef local_74
#undef local_70
#undef local_6c
#undef local_68
#undef local_58
#undef local_54
}

/*
 * _openai_build_messages @ 0x0000dd28..0x0000e388
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Construct openai build messages data; inspect the preserved body for exact field names and conditions.
 * Parameters/return (inferred): RecoveredCJSON * _openai_build_messages(RecoveredConfig *config,RecoveredConversation *conversation)
 * Direct callers: _openai_send@0x0000e5ec
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/0000dd28_openai_build_messages.s
 */
RecoveredCJSON * openai_build_messages(RecoveredConfig *config,RecoveredConversation *conversation)

{
  int iVar1;
  RecoveredToolCall *pRVar2;
  RecoveredCJSON *array;
  RecoveredCJSON *pRVar3;
  char *pcVar4;
  RecoveredCJSON *array_00;
  RecoveredCJSON *object;
  RecoveredCJSON *object_00;
  RecoveredCJSON *object_01;
  uint uVar5;
  int iVar6;
  int iVar7;
  RecoveredMessage *pRVar8;
  
  array = cJSON_CreateArray();
  if ((config->system_prompt).length != 0) {
    pRVar3 = cJSON_CreateObject();
    cJSON_AddStringToObject(pRVar3,"role","system");
    pcVar4 = ds_cstr(&config->system_prompt);
    cJSON_AddStringToObject(pRVar3,"content",pcVar4);
    cJSON_AddItemToArray(array,pRVar3);
  }
  if (0 < conversation->message_count) {
    iVar6 = 0;
    do {
      pRVar8 = conversation->messages + iVar6;
      pRVar3 = cJSON_CreateObject();
      iVar1 = conversation->messages[iVar6].role;
      if (iVar1 == 2) {
        pcVar4 = "assistant";
      }
      else if (iVar1 == 3) {
        pcVar4 = "tool";
      }
      else if (iVar1 == 0) {
        pcVar4 = "system";
      }
      else {
        pcVar4 = "user";
      }
      cJSON_AddStringToObject(pRVar3,"role",pcVar4);
      if (pRVar8->role == 3) {
        pcVar4 = ds_cstr(&pRVar8->content);
        cJSON_AddStringToObject(pRVar3,"content",pcVar4);
        if (pRVar8->call_id[0] != '\0') {
          cJSON_AddStringToObject(pRVar3,"tool_call_id",pRVar8->call_id);
        }
      }
      else if (pRVar8->tool_call_count < 1) {
        pcVar4 = ds_cstr(&pRVar8->content);
        cJSON_AddStringToObject(pRVar3,"content",pcVar4);
      }
      else {
        if ((pRVar8->content).length != 0) {
          pcVar4 = ds_cstr(&pRVar8->content);
          cJSON_AddStringToObject(pRVar3,"content",pcVar4);
        }
        array_00 = cJSON_CreateArray();
        if (0 < pRVar8->tool_call_count) {
          iVar1 = 0;
          iVar7 = 0;
          do {
            object = cJSON_CreateObject();
            cJSON_AddStringToObject(object,"id",pRVar8->tool_calls->call_id + iVar7);
            cJSON_AddStringToObject(object,"type","function");
            object_00 = cJSON_CreateObject();
            pRVar2 = pRVar8->tool_calls;
            if ((*(uint *)(pRVar2->mcp_name + iVar7 + -4) < 100) ||
               (pRVar2->mcp_name[iVar7] == '\0')) {
              pcVar4 = tool_name(*(uint *)(pRVar2->mcp_name + iVar7 + -4));
              cJSON_AddStringToObject(object_00,"name",pcVar4);
            }
            else {
              cJSON_AddStringToObject(object_00,"name",pRVar2->mcp_name + iVar7);
            }
            object_01 = cJSON_CreateObject();
            pRVar2 = pRVar8->tool_calls;
            switch(*(undefined4 *)(pRVar2->mcp_name + iVar7 + -4)) {
            case 0:
              pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar7));
              cJSON_AddStringToObject(object_01,"path",pcVar4);
              break;
            case 1:
              pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar7));
              cJSON_AddStringToObject(object_01,"path",pcVar4);
              pcVar4 = ds_cstr((RecoveredDString *)
                                ((int)&pRVar8->tool_calls->arguments[1].data + iVar7));
              cJSON_AddStringToObject(object_01,"content",pcVar4);
              break;
            case 2:
              pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar7));
              cJSON_AddStringToObject(object_01,"path",pcVar4);
              pcVar4 = ds_cstr((RecoveredDString *)
                                ((int)&pRVar8->tool_calls->arguments[1].data + iVar7));
              cJSON_AddStringToObject(object_01,"oldText",pcVar4);
              pcVar4 = ds_cstr((RecoveredDString *)
                                ((int)&pRVar8->tool_calls->arguments[2].data + iVar7));
              cJSON_AddStringToObject(object_01,"newText",pcVar4);
              break;
            case 3:
              pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar7));
              cJSON_AddStringToObject(object_01,"command",pcVar4);
              break;
            case 4:
              pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar7));
              cJSON_AddStringToObject(object_01,"pattern",pcVar4);
              break;
            case 5:
              pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar7));
              cJSON_AddStringToObject(object_01,"pattern",pcVar4);
              break;
            case 6:
              pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar7));
              cJSON_AddStringToObject(object_01,"query",pcVar4);
              break;
            case 0xb:
              pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar7));
              cJSON_AddStringToObject(object_01,"path",pcVar4);
              pcVar4 = ds_cstr((RecoveredDString *)
                                ((int)&pRVar8->tool_calls->arguments[1].data + iVar7));
              cJSON_AddStringToObject(object_01,"oldText",pcVar4);
              pcVar4 = ds_cstr((RecoveredDString *)
                                ((int)&pRVar8->tool_calls->arguments[2].data + iVar7));
              cJSON_AddStringToObject(object_01,"newText",pcVar4);
              break;
            case 0xc:
              pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar7));
              cJSON_AddStringToObject(object_01,"prompt",pcVar4);
              if (1 < *(int *)(pRVar8->tool_calls->call_id + iVar7 + -4)) {
                pcVar4 = ds_cstr((RecoveredDString *)
                                  ((int)&pRVar8->tool_calls->arguments[1].data + iVar7));
                cJSON_AddStringToObject(object_01,"model",pcVar4);
              }
              break;
            case 0xd:
              pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar7));
              cJSON_AddStringToObject(object_01,"query",pcVar4);
              break;
            case 0xe:
              if (**(char **)((int)&pRVar2->arguments[0].data + iVar7) != '\0') {
                pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar7));
                cJSON_AddStringToObject(object_01,"path",pcVar4);
              }
              if (1 < *(int *)(pRVar8->tool_calls->call_id + iVar7 + -4)) {
                pcVar4 = ds_cstr((RecoveredDString *)
                                  ((int)&pRVar8->tool_calls->arguments[1].data + iVar7));
                uVar5 = atoi(pcVar4);
                cJSON_AddNumberToObject
                          (object_01,"max_depth",
                           app_bits_double(CONCAT44(0x43300000,uVar5 ^ 0x80000000)) - 4503601774854144.0);
              }
              break;
            case 0xf:
              pcVar4 = ds_cstr((RecoveredDString *)((int)&pRVar2->arguments[0].data + iVar7));
              cJSON_AddStringToObject(object_01,"pattern",pcVar4);
              if (1 < *(int *)(pRVar8->tool_calls->call_id + iVar7 + -4)) {
                pcVar4 = ds_cstr((RecoveredDString *)
                                  ((int)&pRVar8->tool_calls->arguments[1].data + iVar7));
                uVar5 = atoi(pcVar4);
                cJSON_AddNumberToObject
                          (object_01,"max_bytes_per_file",
                           app_bits_double(CONCAT44(0x43300000,uVar5 ^ 0x80000000)) - 4503601774854144.0);
              }
            }
            cJSON_AddItemToObject(object_00,"arguments",object_01);
            cJSON_AddItemToObject(object,"function",object_00);
            cJSON_AddItemToArray(array_00,object);
            iVar1 = iVar1 + 1;
            iVar7 = iVar7 + 0x51c;
          } while (iVar1 < pRVar8->tool_call_count);
        }
        cJSON_AddItemToObject(pRVar3,"tool_calls",array_00);
      }
      cJSON_AddItemToArray(array,pRVar3);
      iVar6 = iVar6 + 1;
    } while (iVar6 < conversation->message_count);
  }
  return array;
}

/*
 * _openai_sse_cb @ 0x0000e388..0x0000e5ec
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Decode provider streaming events, accumulate response/tool-call state and invoke callbacks.
 * Parameters/return (inferred): void _openai_sse_cb(char *event_kind,char *data,RecoveredProviderCallbackContext *context)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000e388_openai_sse_cb.s
 * Uncertainty: Indirect dispatch sites are retained with raw assembly and references; possible target sets are not assumed complete.
 */
void openai_sse_cb(char *event_kind,char *data,RecoveredProviderCallbackContext *context)

{
  (void)event_kind; /* Present in the recovered ABI; unused by this routine. */
  int iVar1;
  RecoveredCJSON *object;
  RecoveredCJSON *pRVar2;
  RecoveredCJSON *pRVar3;
  RecoveredCJSON *pRVar4;
  char *left;
  
  if ((data == (char *)0x0) || (iVar1 = strcmp(data,"[DONE]"), iVar1 == 0)) {
    context->response->completion_flag = 1;
    return;
  }
  object = cJSON_Parse(data);
  if (object == (RecoveredCJSON *)0x0) {
    return;
  }
  pRVar2 = cJSON_GetObjectItem(object,"usage");
  if (pRVar2 != (RecoveredCJSON *)0x0) {
    pRVar3 = cJSON_GetObjectItem(pRVar2,"prompt_tokens");
    pRVar4 = cJSON_GetObjectItem(pRVar2,"completion_tokens");
    pRVar2 = cJSON_GetObjectItem(pRVar2,"total_tokens");
    if (pRVar3 != (RecoveredCJSON *)0x0) {
      context->response->input_tokens = pRVar3->value_int;
    }
    if (pRVar4 != (RecoveredCJSON *)0x0) {
      context->response->output_tokens = pRVar4->value_int;
    }
    if (pRVar2 != (RecoveredCJSON *)0x0) {
      context->response->total_tokens = pRVar2->value_int;
    }
    context->response->usage_known = 1;
  }
  pRVar2 = cJSON_GetObjectItem(object,"choices");
  if (pRVar2 == (RecoveredCJSON *)0x0) {
    cJSON_Delete(object);
    return;
  }
  pRVar2 = cJSON_GetArrayItem(pRVar2,0);
  if (pRVar2 == (RecoveredCJSON *)0x0) {
    cJSON_Delete(object);
    return;
  }
  pRVar3 = cJSON_GetObjectItem(pRVar2,"finish_reason");
  if ((pRVar3 != (RecoveredCJSON *)0x0) && (left = pRVar3->value_string, left != (char *)0x0)) {
    iVar1 = strcmp(left,"tool_calls");
    if (iVar1 == 0) {
      context->response->finish_reason = 1;
    }
    else {
      iVar1 = strcmp(left,"length");
      if (iVar1 == 0) {
        context->response->finish_reason = 2;
      }
    }
  }
  pRVar2 = cJSON_GetObjectItem(pRVar2,"delta");
  if (pRVar2 == (RecoveredCJSON *)0x0) {
    cJSON_Delete(object);
    return;
  }
  pRVar3 = cJSON_GetObjectItem(pRVar2,"content");
  if ((pRVar3 != (RecoveredCJSON *)0x0) && (pRVar3->value_string != (char *)0x0)) {
    ds_append(&context->response->content,pRVar3->value_string);
    if (context->callback != (RecoveredStreamCallback)0x0) {
      (*context->callback)((char *)0x0,pRVar3->value_string,context->context);
    }
  }
  pRVar2 = cJSON_GetObjectItem(pRVar2,"tool_calls");
  if (pRVar2 != (RecoveredCJSON *)0x0) {
    provider_parse_tool_calls(pRVar2,context->response);
  }
  cJSON_Delete(object);
  return;
}

/*
 * _openai_send @ 0x0000e5ec..0x0000e7a4
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Send OpenAI chat-completions JSON with bearer authentication through shared streaming transport.
 * Parameters/return (inferred): int _openai_send(RecoveredConfig *config,RecoveredConversation *conversation, RecoveredResponse *response,RecoveredStreamCallback callback,void *context, RecoveredRoute *route,int *failures)
 * Direct callers: _provider_send@0x0000a39c
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/0000e5ec_openai_send.s
 */
int openai_send(RecoveredConfig *config,RecoveredConversation *conversation,
                RecoveredResponse *response,RecoveredStreamCallback callback,void *context,
                RecoveredRoute *route,int *failures)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x660] __attribute__((aligned(16)));
#define acStack_648 (*((char (*)[1024])(app_frame + 0x8))) /* original SP - 0x648 */
#define acStack_248 (*((char (*)[512])(app_frame + 0x408))) /* original SP - 0x248 */
#define local_48 (*((RecoveredResponse * *)(app_frame + 0x608))) /* original SP - 0x48 */
#define local_44 (*((RecoveredStreamCallback *)(app_frame + 0x60c))) /* original SP - 0x44 */
#define local_40 (*((void * *)(app_frame + 0x610))) /* original SP - 0x40 */
#define local_38 (*((undefined4 *)(app_frame + 0x618))) /* original SP - 0x38 */
#define uStack_34 (*((uint *)(app_frame + 0x61c))) /* original SP - 0x34 */

  char *pcVar1;
  RecoveredCJSON *object;
  RecoveredCJSON *pRVar2;
  char *pcVar3;
  int iVar4;

  pcVar1 = config_get_url(config);
  snprintf(acStack_648,0x400,"%s/v1/chat/completions",pcVar1);
  object = cJSON_CreateObject();
  cJSON_AddStringToObject(object,"model",config->model);
  cJSON_AddBoolToObject(object,"stream",1);
  uStack_34 = config->max_tokens ^ 0x80000000;
  local_38 = 0x43300000;
  cJSON_AddNumberToObject
            (object,"max_tokens",app_bits_double(CONCAT44(0x43300000,uStack_34)) - 4503601774854144.0);
  cJSON_AddNumberToObject(object,"temperature",(double)config->temperature);
  pRVar2 = openai_build_messages(config,conversation);
  cJSON_AddItemToObject(object,"messages",pRVar2);
  pRVar2 = openai_build_tools_array(route,failures);
  cJSON_AddItemToObject(object,"tools",pRVar2);
  pRVar2 = cJSON_CreateObject();
  cJSON_AddBoolToObject(pRVar2,"include_usage",1);
  cJSON_AddItemToObject(object,"stream_options",pRVar2);
  pcVar1 = cJSON_PrintUnformatted(object);
  cJSON_Delete(object);
  pcVar3 = config_get_key(config);
  snprintf(acStack_248,0x200,"Authorization: Bearer %s",pcVar3);
  local_48 = response;
  local_44 = callback;
  local_40 = context;
  iVar4 = http_post_with_retry(acStack_648,pcVar1,acStack_248,(RecoveredStreamCallback)(openai_sse_cb),&local_48,(int *)0x0);
  free(pcVar1);
  return iVar4;

#undef acStack_648
#undef acStack_248
#undef local_48
#undef local_44
#undef local_40
#undef local_38
#undef uStack_34
}

