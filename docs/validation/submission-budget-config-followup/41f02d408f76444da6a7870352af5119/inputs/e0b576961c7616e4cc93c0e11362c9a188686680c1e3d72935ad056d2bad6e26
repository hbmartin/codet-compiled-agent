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
 * _theme_apply @ 0x0001782c..0x00017894
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Look up/apply terminal theme state: apply. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _theme_apply(RecoveredTheme *theme)
 * Direct callers: _tui_theme_set@0x0001a254, _handle_cmd@0x0001b004, _tui_init@0x0001daac
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0001782c_theme_apply.s
 */
void theme_apply(RecoveredTheme *theme)

{
  bool bVar1;
  BOOL BVar2;
  int iVar3;
  
  _g_theme = theme;
  BVar2 = has_colors();
  if (BVar2 != 0) {
    iVar3 = 1;
    do {
      init_pair((short)iVar3,theme->pairs[0].foreground,theme->pairs[0].background);
      theme = (RecoveredTheme *)theme->pairs;
      bVar1 = iVar3 != 0x14;
      iVar3 = iVar3 + 1;
    } while (bVar1);
  }
  return;
}

/*
 * _theme_pair @ 0x00017894..0x000178a0
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Look up/apply terminal theme state: pair. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): int _theme_pair(int param_1)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00017894_theme_pair.s
 */
int theme_pair(int param_1)

{
  return (param_1 + 1) * 0x100;
}

/*
 * _theme_attr @ 0x000178a0..0x000178dc
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Look up/apply terminal theme state: attr. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): uint _theme_attr(int color_index)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000178a0_theme_attr.s
 */
uint theme_attr(int color_index)

{
  uint uVar1;
  
  uVar1 = 0;
  if (_g_theme != (RecoveredTheme *)0x0) {
    uVar1 = _g_theme->pairs[color_index].attributes;
  }
  return uVar1;
}

/*
 * _theme_find @ 0x000178dc..0x00017968
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Look up/apply terminal theme state: find. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): RecoveredTheme * _theme_find(char *name)
 * Direct callers: _tui_theme_set@0x0001a254, _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000178dc_theme_find.s
 */
RecoveredTheme * theme_find(char *name)

{
  int iVar1;
  RecoveredTheme *pRVar2;
  int iVar3;
  RecoveredTheme **ppRVar4;
  
  if (_all_themes[0] != (RecoveredTheme *)0x0) {
    iVar3 = 0;
    ppRVar4 = _all_themes;
    pRVar2 = _all_themes[0];
    do {
      ppRVar4 = ppRVar4 + 1;
      iVar1 = strcmp(pRVar2->name,name);
      if (iVar1 == 0) {
        return _all_themes[iVar3];
      }
      iVar3 = iVar3 + 1;
      pRVar2 = *ppRVar4;
    } while (pRVar2 != (RecoveredTheme *)0x0);
  }
  return (RecoveredTheme *)0x0;
}

/*
 * _theme_names_csv @ 0x00017968..0x00017a00
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Look up/apply terminal theme state: names csv. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): undefined1 * _theme_names_csv(undefined1 *param_1,int param_2)
 * Direct callers: _tui_theme_list@0x0001a304, _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00017968_theme_names_csv.s
 */
undefined1 * theme_names_csv(undefined1 *param_1,int param_2)

{
  int iVar1;
  RecoveredTheme *pRVar2;
  RecoveredTheme **ppRVar3;
  int iVar4;
  
  *param_1 = 0;
  if ((_all_themes[0] != (RecoveredTheme *)0x0) && (0 < param_2 + -0x20)) {
    iVar4 = 0;
    ppRVar3 = _all_themes;
    pRVar2 = _all_themes[0];
    do {
      ppRVar3 = ppRVar3 + 1;
      iVar1 = snprintf((char * restrict)(param_1 + iVar4),param_2 - iVar4," %s",pRVar2->name);
      iVar4 = iVar4 + iVar1;
      pRVar2 = *ppRVar3;
      if (pRVar2 == (RecoveredTheme *)0x0) {
        return param_1;
      }
    } while (iVar4 < param_2 + -0x20);
  }
  return param_1;
}

