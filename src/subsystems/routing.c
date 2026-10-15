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
 * _msg_has @ 0x00012a60..0x00012a8c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in routing: msg has. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): bool _msg_has(char *param_1,char *param_2)
 * Direct callers: _router_classify@0x00012a8c
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00012a60_msg_has.s
 */
bool msg_has(char *param_1,char *param_2)

{
  char *pcVar1;
  
  pcVar1 = strcasestr(param_1,param_2);
  return pcVar1 != (char *)0x0;
}

/*
 * _router_classify @ 0x00012a8c..0x00012f1c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Classify user intent using keyword/heuristic checks and populate routing state.
 * Parameters/return (inferred): uint * _router_classify(uint *param_1,char *param_2)
 * Direct callers: _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00012a8c_router_classify.s
 */
uint * router_classify(uint *param_1,char *param_2)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x40] __attribute__((aligned(16)));
#define local_28 (*((uint *)(app_frame + 0x8))) /* original SP - 0x28 */
#define local_24 (*((uint *)(app_frame + 0xc))) /* original SP - 0x24 */
#define local_20 (*((uint *)(app_frame + 0x10))) /* original SP - 0x20 */

  uint uVar1;
  int iVar2;

  local_28 = 0;
  local_24 = 0;
  if ((param_2 == (char *)0x0) || (*param_2 == '\0')) {
    *param_1 = 0x20;
    param_1[1] = 0;
    param_1[2] = 0;
  }
  else {
    uVar1 = strlen(param_2);
    local_20 = (uint)((int)uVar1 < 10);
    iVar2 = msg_has(param_2,"read");
    if (((((iVar2 != 0) || (iVar2 = msg_has(param_2,"show"), iVar2 != 0)) ||
         (iVar2 = msg_has(param_2,"display"), iVar2 != 0)) ||
        ((iVar2 = msg_has(param_2,"open"), iVar2 != 0 ||
         (iVar2 = msg_has(param_2,"view"), iVar2 != 0)))) ||
       (((iVar2 = msg_has(param_2,"inspect"), iVar2 != 0 ||
         ((iVar2 = msg_has(param_2,"look"), iVar2 != 0 ||
          (iVar2 = msg_has(param_2,"cat"), iVar2 != 0)))) ||
        (iVar2 = msg_has(param_2,"content"), iVar2 != 0)))) {
      local_28 = 1;
    }
    iVar2 = msg_has(param_2,"write");
    if ((((iVar2 != 0) || (iVar2 = msg_has(param_2,"create"), iVar2 != 0)) ||
        (iVar2 = msg_has(param_2,"add"), iVar2 != 0)) ||
       (((iVar2 = msg_has(param_2,"modify"), iVar2 != 0 ||
         (iVar2 = msg_has(param_2,"fix"), iVar2 != 0)) ||
        ((iVar2 = msg_has(param_2,"update"), iVar2 != 0 ||
         (iVar2 = msg_has(param_2,"change"), iVar2 != 0)))))) {
      local_28 = local_28 | 2;
    }
    iVar2 = msg_has(param_2,"search");
    if ((((iVar2 != 0) || (iVar2 = msg_has(param_2,"find"), iVar2 != 0)) ||
        ((iVar2 = msg_has(param_2,"google"), iVar2 != 0 ||
         (((iVar2 = msg_has(param_2,"look up"), iVar2 != 0 ||
           (iVar2 = msg_has(param_2,"web"), iVar2 != 0)) ||
          (iVar2 = msg_has(param_2,"fetch"), iVar2 != 0)))))) ||
       ((iVar2 = msg_has(param_2,"url"), iVar2 != 0 ||
        (iVar2 = msg_has(param_2,"download"), iVar2 != 0)))) {
      local_28 = local_28 | 4;
    }
    iVar2 = msg_has(param_2,"run");
    if (((iVar2 != 0) || (iVar2 = msg_has(param_2,"execute"), iVar2 != 0)) ||
       ((iVar2 = msg_has(param_2,"test"), iVar2 != 0 ||
        ((((iVar2 = msg_has(param_2,"build"), iVar2 != 0 ||
           (iVar2 = msg_has(param_2,"make"), iVar2 != 0)) ||
          (iVar2 = msg_has(param_2,"compile"), iVar2 != 0)) ||
         (iVar2 = msg_has(param_2,"check"), iVar2 != 0)))))) {
      local_28 = local_28 | 8;
    }
    iVar2 = msg_has(param_2,"plan");
    if (((iVar2 != 0) || (iVar2 = msg_has(param_2,"steps"), iVar2 != 0)) ||
       ((iVar2 = msg_has(param_2,"how to"), iVar2 != 0 ||
        (iVar2 = msg_has(param_2,"approach"), iVar2 != 0)))) {
      local_28 = local_28 | 0x10;
    }
    iVar2 = msg_has(param_2,"src/");
    if (((iVar2 != 0) || (iVar2 = msg_has(param_2,"include"), iVar2 != 0)) ||
       (iVar2 = msg_has(param_2,"path"), iVar2 != 0)) {
      local_24 = 1;
    }
    if (local_28 == 0) {
      local_28 = 0xf;
    }
    *param_1 = local_28;
    param_1[1] = local_24;
    param_1[2] = local_20;
  }
  return param_1;

#undef local_28
#undef local_24
#undef local_20
}

/*
 * _router_tool_mask @ 0x00012f1c..0x00012fc0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Convert request routing into the permitted/advertised tool mask.
 * Parameters/return (inferred): uint _router_tool_mask(int tool_type)
 * Direct callers: _router_should_send_tool@0x00012fc0
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00012f1c_router_tool_mask.s
 */
uint router_tool_mask(int tool_type)

{
  switch(tool_type) {
  case 0:
  case 4:
  case 5:
  case 0xe:
  case 0xf:
    return 1;
  case 1:
  case 2:
    return 2;
  case 3:
    return 8;
  case 6:
  case 7:
  case 0xd:
    return 4;
  case 8:
  case 0xb:
    return 3;
  default:
    return 0x20;
  }
}

/*
 * _router_should_send_tool @ 0x00012fc0..0x00013000
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in routing: router should send tool. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _router_should_send_tool(int tool_type,uint intent_mask,int mentions_path,int short_message)
 * Direct callers: _tools_schema_add_simple@0x0000a510, _tools_schema_add_read_and_edit@0x0000a68c, _tools_schema_add_fetch_url@0x0000a8ac, _tools_schema_add_db_query@0x0000aa20, _tools_schema_build_standard@0x0000acd4, _openai_build_tools_array@0x0000ccb8, _anthropic_build_tools_array@0x0000e7a4, _google_build_tools@0x00010768
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00012fc0_router_should_send_tool.s
 */
int router_should_send_tool(int tool_type,uint intent_mask,int mentions_path,int short_message)

{
  (void)mentions_path; /* Present in the recovered ABI; unused by this routine. */
  (void)short_message; /* Present in the recovered ABI; unused by this routine. */
  uint uVar1;
  
  uVar1 = router_tool_mask(tool_type);
  return (uint)((uVar1 & intent_mask) != 0);
}

