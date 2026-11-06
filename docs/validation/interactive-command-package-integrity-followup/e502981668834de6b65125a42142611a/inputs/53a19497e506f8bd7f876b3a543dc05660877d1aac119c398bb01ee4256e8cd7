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
 * _hl_init @ 0x00020c2c..0x00020c94
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in highlighting: hl init. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _hl_init(void)
 * Direct callers: _tui_init@0x0001daac
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00020c2c_hl_init.s
 */
void hl_init(void)

{
  BOOL BVar1;
  
  BVar1 = has_colors();
  if (BVar1 != 0) {
    init_pair(0x16,6,-1);
    init_pair(0x17,2,-1);
    init_pair(0x18,3,-1);
    init_pair(0x19,0,-1);
  }
  return;
}

/*
 * _is_keyword @ 0x00020c94..0x00020d08
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in highlighting: is keyword. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): undefined4 _is_keyword(char *param_1)
 * Direct callers: _hl_line@0x00020d08
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00020c94_is_keyword.s
 */
undefined4 is_keyword(char *param_1)

{
  int iVar1;
  char *right;
  undefined **ppuVar2;
  
  if (_kw_3427 != NULL) {
    ppuVar2 = (undefined **)(&APP_PTR_00039134);
    right = _kw_3427;
    do {
      iVar1 = strcmp(param_1,right);
      if (iVar1 == 0) {
        return 1;
      }
      right = (char *)(*ppuVar2);
      ppuVar2 = ppuVar2 + 1;
    } while (right != (char *)0x0);
  }
  return 0;
}

/*
 * _hl_line @ 0x00020d08..0x000210a4
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Apply terminal highlighting to one source-code line.
 * Parameters/return (inferred): void _hl_line(WINDOW *param_1,byte *param_2,int param_3)
 * Direct callers: _tui_render_line@0x000182a0
 * Possible side effects (direct-call closure, incomplete for indirect calls): reads process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00020d08_hl_line.s
 */
void hl_line(WINDOW *param_1,byte *param_2,int param_3)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x80] __attribute__((aligned(16)));
#define local_68 (*((byte (*)[84])(app_frame + 0x8))) /* original SP - 0x68 */

  byte bVar1;
  byte bVar2;
  uint uVar3;
  char *pcVar4;
  int iVar5;
  int iVar6;

  if (_g_disable_hl == -1) {
    pcVar4 = getenv("SSH_CLIENT");
    if (pcVar4 == (char *)0x0) {
      pcVar4 = getenv("SSH_TTY");
      _g_disable_hl = 0;
      if (pcVar4 == (char *)0x0) goto LAB_00020d74;
    }
    _g_disable_hl = 1;
  }
LAB_00020d74:
  if ((param_3 == 0) || (_g_disable_hl != 0)) {
    wprintw(param_1,"%s",param_2);
  }
  else {
    while (bVar1 = *param_2, bVar1 != 0) {
      if ((bVar1 & 0x80) == 0) {
        uVar3 = *(uint *)(PTR_000396b0 + (uint)bVar1 * 4 + 0x34) >> 8 & 1;
      }
      else {
        iVar6 = __maskrune((uint)bVar1,0x100);
        uVar3 = (uint)(iVar6 != 0);
      }
      if ((uVar3 == 0) && (bVar1 = *param_2, bVar1 != 0x5f)) {
        if ((bVar1 == 0x22) || (bVar1 == 0x27)) {
          wattr_on(param_1,0x1700,(void *)0x0);
          waddch(param_1,(uint)bVar1);
          param_2 = param_2 + 1;
          bVar2 = *param_2;
          while (bVar2 != 0) {
            if (bVar1 == bVar2) goto LAB_00020f8c;
            waddch(param_1,(uint)bVar2);
            param_2 = param_2 + 1;
            bVar2 = *param_2;
          }
          if (bVar1 == 0) {
LAB_00020f8c:
            waddch(param_1,(uint)bVar1);
            param_2 = param_2 + 1;
          }
          wattr_off(param_1,0x1700,(void *)0x0);
        }
        else {
          uVar3 = (uint)bVar1;
          if ((uVar3 < 0x100) && ((*(uint *)(PTR_000396b0 + uVar3 * 4 + 0x34) & 0x400) != 0)) {
            wattr_on(param_1,0x1800,(void *)0x0);
            while( true ) {
              bVar1 = *param_2;
              uVar3 = (uint)bVar1;
              if ((((0xff < uVar3) || ((*(uint *)(PTR_000396b0 + uVar3 * 4 + 0x34) & 0x400) == 0))
                  && (bVar1 != 0x2e)) && (bVar1 != 0x78)) break;
              waddch(param_1,uVar3);
              param_2 = param_2 + 1;
            }
            wattr_off(param_1,0x1800,(void *)0x0);
          }
          else {
            if ((bVar1 == 0x2f) && (param_2[1] == 0x2f)) {
              wattr_on(param_1,0x101900,(void *)0x0);
              wprintw(param_1,"%s",param_2);
              wattr_off(param_1,0x101900,(void *)0x0);
              return;
            }
            param_2 = param_2 + 1;
            waddch(param_1,uVar3);
          }
        }
      }
      else {
        iVar6 = 0;
        while( true ) {
          bVar1 = *param_2;
          if ((bVar1 & 0x80) == 0) {
            uVar3 = -(*(uint *)(PTR_000396b0 + (uint)bVar1 * 4 + 0x34) & 0x500) >> 0x1f;
          }
          else {
            iVar5 = __maskrune((uint)bVar1,0x500);
            uVar3 = (uint)(iVar5 != 0);
          }
          if ((uVar3 == 0) && (*param_2 != 0x5f)) break;
          if (iVar6 < 0x3f) {
            local_68[iVar6] = *param_2;
            iVar6 = iVar6 + 1;
          }
          param_2 = param_2 + 1;
        }
        local_68[iVar6] = 0;
        iVar6 = is_keyword((char *)(local_68));
        if (iVar6 == 0) {
          wprintw(param_1,"%s",local_68);
        }
        else {
          wattr_on(param_1,0x1600,(void *)0x0);
          wprintw(param_1,"%s",local_68);
          wattr_off(param_1,0x1600,(void *)0x0);
        }
      }
    }
  }
  return;

#undef local_68
}

