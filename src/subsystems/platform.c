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
 * _detect_macos @ 0x0002c5b8..0x0002c774
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in platform: detect macos. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _detect_macos(RecoveredTigerUtsname *system)
 * Direct callers: _platform_detect@0x0002c774
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002c5b8_detect_macos.s
 */
void detect_macos(RecoveredTigerUtsname *system)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x40] __attribute__((aligned(16)));
#define local_28 (*((int (*)[6])(app_frame + 0x8))) /* original SP - 0x28 */

  int iVar1;
  char *text;

  text = system->release;
  app_copy_padded(&_g_platform_version,text,0x1f);
  DAT_0003964b = 0;
  local_28[0] = 0;
  sscanf(text,"%d",local_28);
  if (local_28[0] == 8) {
    _g_platform = 2;
    app_format_truncated(_g_platform_name,0x40,"Mac OS X Tiger (Darwin %s)",text);
  }
  else if (local_28[0] == 9) {
    _g_platform = 3;
    app_format_truncated(_g_platform_name,0x40,"Mac OS X Leopard (Darwin %s)",text);
  }
  else if (local_28[0] == 7) {
    _g_platform = 1;
    app_format_truncated(_g_platform_name,0x40,"Mac OS X Panther (Darwin %s)",text);
  }
  else if (local_28[0] < 10) {
    _g_platform = 7;
    app_format_truncated(_g_platform_name,0x40,"Mac OS X/Darwin %s",text);
  }
  else {
    _g_platform = 4;
    app_format_truncated(_g_platform_name,0x40,"Mac OS X %d.x (Darwin %s)",local_28[0] + -4,text);
  }
  local_28[1] = 0x40;
  iVar1 = sysctlbyname("hw.machine",_g_platform_machine,(size_t *)(local_28 + 1),(void *)0x0,0);
  if (iVar1 != 0) {
    app_copy_padded(_g_platform_machine,system->machine,0x3f);
    DAT_0003968b = 0;
  }
  return;

#undef local_28
}

/*
 * _platform_detect @ 0x0002c774..0x0002c808
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Detect Darwin/architecture properties and populate global platform strings/flags.
 * Parameters/return (inferred): void _platform_detect(void)
 * Direct callers: _main@0x00005498
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002c774_platform_detect.s
 */
void platform_detect(void)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x530] __attribute__((aligned(16)));
#define RStack_518 (*((RecoveredTigerUtsname *)(app_frame + 0x8))) /* original SP - 0x518 */

  int iVar1;

  memset(&RStack_518,0,0x500);
  iVar1 = uname((struct utsname *)(&RStack_518));
  if (iVar1 < 0) {
    strncpy(_g_platform_name,"Unknown",0x3f);
  }
  else {
    app_copy_padded(&_g_platform_version,RStack_518.release,0x1f);
    DAT_0003964b = 0;
    detect_macos(&RStack_518);
  }
  return;

#undef RStack_518
}

/*
 * _platform_is_mac @ 0x0002c808..0x0002c834
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Test a recorded platform property: mac. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): bool _platform_is_mac(void)
 * Direct callers: _usage@0x00003018, _platform_prompt_string@0x0002c93c
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002c808_platform_is_mac.s
 */
bool platform_is_mac(void)

{
  return _g_platform - 1U < 4;
}

/*
 * _platform_is_tiger @ 0x0002c834..0x0002c85c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Test a recorded platform property: tiger. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): bool _platform_is_tiger(void)
 * Direct callers: _tui_theme_set@0x0001a254, _tui_init@0x0001daac, _platform_prompt_string@0x0002c93c
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002c834_platform_is_tiger.s
 */
bool platform_is_tiger(void)

{
  return _g_platform == 2;
}

/*
 * _platform_is_leopard @ 0x0002c85c..0x0002c884
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Test a recorded platform property: leopard. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): bool _platform_is_leopard(void)
 * Direct callers: _platform_prompt_string@0x0002c93c
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002c85c_platform_is_leopard.s
 */
bool platform_is_leopard(void)

{
  return _g_platform == 3;
}

/*
 * _platform_is_ppc @ 0x0002c884..0x0002c93c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Test a recorded platform property: ppc. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): undefined4 _platform_is_ppc(void)
 * Direct callers: _config_init@0x00005da8, _platform_prompt_string@0x0002c93c
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002c884_platform_is_ppc.s
 */
undefined4 platform_is_ppc(void)

{
  char *pcVar1;
  int iVar2;
  
  pcVar1 = strstr(_g_platform_machine,"ppc");
  if ((((pcVar1 == (char *)0x0) &&
       (pcVar1 = strstr(_g_platform_machine,"Power"), pcVar1 == (char *)0x0)) &&
      (iVar2 = strcmp(_g_platform_machine,"ppc7400"), iVar2 != 0)) &&
     ((iVar2 = strcmp(_g_platform_machine,"ppc7450"), iVar2 != 0 &&
      (iVar2 = strcmp(_g_platform_machine,"ppc64"), iVar2 != 0)))) {
    return 0;
  }
  return 1;
}

/*
 * _platform_prompt_string @ 0x0002c93c..0x0002ca60
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Format platform identification for the model system prompt.
 * Parameters/return (inferred): undefined * _platform_prompt_string(void)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002c93c_platform_prompt_string.s
 */
undefined * platform_prompt_string(void)

{
  int iVar1;
  char *pcVar2;
  char *pcVar3;
  
  iVar1 = platform_is_mac();
  if (iVar1 == 0) {
    if (_g_platform == 5) {
      pcVar3 = "Linux";
    }
    else {
      pcVar3 = "an operating system";
    }
  }
  else {
    iVar1 = platform_is_tiger();
    if (iVar1 == 0) {
      iVar1 = platform_is_leopard();
      if (iVar1 == 0) {
        if (_g_platform == 1) {
          pcVar3 = "Mac OS X Panther";
        }
        else {
          pcVar3 = "Mac OS X";
        }
      }
      else {
        pcVar3 = "Mac OS X Leopard";
      }
    }
    else {
      pcVar3 = "Mac OS X Tiger";
    }
  }
  iVar1 = platform_is_ppc();
  if (iVar1 == 0) {
    pcVar2 = "";
  }
  else {
    pcVar2 = "PowerPC";
  }
  if (*pcVar2 == '\0') {
    snprintf(&_buf_2670,0x100,
                       "You are PowerPC Code, an AI coding assistant running on %s. You have access to TOOLS. Use tools when the user asks for file operations, searches, shell commands, web searches, or database queries."
                       ,pcVar3);
  }
  else {
    snprintf(&_buf_2670,0x100,
                       "You are PowerPC Code, an AI coding assistant running on %s (%s). You have access to TOOLS. Use tools when the user asks for file operations, searches, shell commands, web searches, or database queries."
                       ,pcVar3,pcVar2);
  }
  return (undefined *)(&_buf_2670);
}

