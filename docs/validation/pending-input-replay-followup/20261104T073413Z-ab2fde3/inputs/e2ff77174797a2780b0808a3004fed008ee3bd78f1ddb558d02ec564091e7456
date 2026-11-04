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
 * _reasoning_extract @ 0x00020aac..0x00020c2c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Separate model reasoning tags/text from visible response text.
 * Parameters/return (inferred): void _reasoning_extract(RecoveredDString *param_1,RecoveredDString *param_2)
 * Direct callers: _simple_mode@0x00004e30, _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00020aac_reasoning_extract.s
 */
/* WARNING: Removing unreachable block (ram,0x00020c14) */

void reasoning_extract(RecoveredDString *param_1,RecoveredDString *param_2)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x50] __attribute__((aligned(16)));
#define local_38 (*((RecoveredDString (*)[2])(app_frame + 0x8))) /* original SP - 0x38 */

  char cVar1;
  char *pcVar2;
  int iVar3;
  char *pcVar4;
  char *text;
  int iVar5;
  undefined **ppuVar6;

  if (((param_1 != (RecoveredDString *)0x0) && (param_1->data != (char *)0x0)) &&
     (param_1->length != 0)) {
    ds_init(local_38);
    text = param_1->data;
    cVar1 = *text;
    while (cVar1 != '\0') {
      pcVar2 = "<thinking>";
      iVar5 = 0;
      pcVar4 = (char *)0x0;
      iVar3 = -1;
      ppuVar6 = (undefined **)(&APP_PTR_0003a7d8);
      do {
        pcVar2 = strstr(text,pcVar2);
        if ((pcVar2 != (char *)0x0) && ((pcVar4 == (char *)0x0 || (pcVar2 < pcVar4)))) {
          iVar3 = iVar5;
          pcVar4 = pcVar2;
        }
        iVar5 = iVar5 + 1;
        pcVar2 = (char *)(*ppuVar6);
        ppuVar6 = ppuVar6 + 4;
      } while (pcVar2 != (char *)0x0);
      if (pcVar4 == (char *)0x0) {
        ds_append(local_38,text);
        break;
      }
      ds_appendn(local_38,text,(int)pcVar4 - (int)text);
      pcVar4 = pcVar4 + (&DAT_0003a7d0)[iVar3 * 4];
      pcVar2 = strstr(pcVar4,(&APP_PTR_0003a7cc)[iVar3 * 4]);
      if (pcVar2 == (char *)0x0) {
        ds_append(param_2,pcVar4);
        break;
      }
      ds_appendn(param_2,pcVar4,(int)pcVar2 - (int)pcVar4);
      text = pcVar2 + (&DAT_0003a7d4)[iVar3 * 4];
      cVar1 = pcVar2[(&DAT_0003a7d4)[iVar3 * 4]];
    }
    ds_free(param_1);
    param_1->data = local_38[0].data;
    param_1->length = local_38[0].length;
    param_1->capacity = local_38[0].capacity;
  }
  return;

#undef local_38
}

