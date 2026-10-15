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
 * _queue_init @ 0x00017a00..0x00017a14
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage the fixed UI event queue: init. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _queue_init(RecoveredQueue *queue)
 * Direct callers: _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00017a00_queue_init.s
 */
/* Analyst reconstruction of _queue_init at 0x00017a00. See index/manual-review.json. */
void queue_init(RecoveredQueue *queue)
{
    queue->count = 0; queue->first = 0;
}

/*
 * _queue_empty @ 0x00017a14..0x00017a28
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage the fixed UI event queue: empty. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): int _queue_empty(RecoveredQueue *queue)
 * Direct callers: _queue_pop@0x00017ac8, _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00017a14_queue_empty.s
 */
/* Analyst reconstruction of _queue_empty at 0x00017a14. See index/manual-review.json. */
int queue_empty(RecoveredQueue *queue)
{
    return queue->count == 0;
}

/*
 * _queue_full @ 0x00017a28..0x00017a40
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage the fixed UI event queue: full. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): int _queue_full(RecoveredQueue *queue)
 * Direct callers: _queue_push@0x00017a40, _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00017a28_queue_full.s
 */
/* Analyst reconstruction of _queue_full at 0x00017a28. See index/manual-review.json. */
int queue_full(RecoveredQueue *queue)
{
    return queue->count > 7;
}

/*
 * _queue_push @ 0x00017a40..0x00017ac8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage the fixed UI event queue: push. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _queue_push(RecoveredQueue *queue, const char *text)
 * Direct callers: _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00017a40_queue_push.s
 */
/* Analyst reconstruction of _queue_push at 0x00017a40. See index/manual-review.json. */
void queue_push(RecoveredQueue *queue, const char *text)
{
    int index;
    if (queue_full(queue)) return;
    index = (queue->first + queue->count) % 8;
    strncpy(queue->entries[index], text, 262143);
    queue->entries[index][262143] = '\0';
    ++queue->count;
}

/*
 * _queue_pop @ 0x00017ac8..0x00017b38
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage the fixed UI event queue: pop. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): char *_queue_pop(RecoveredQueue *queue)
 * Direct callers: _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00017ac8_queue_pop.s
 */
/* Analyst reconstruction of _queue_pop at 0x00017ac8. See index/manual-review.json. */
char *queue_pop(RecoveredQueue *queue)
{
    char *text;
    if (queue_empty(queue)) return 0;
    text = queue->entries[queue->first];
    queue->first = (queue->first + 1) % 8;
    --queue->count;
    return text;
}

/*
 * _ascii_fixup @ 0x00017b38..0x00018240
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in terminal_ui: ascii fixup. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _ascii_fixup(char *input,char *output,uint capacity)
 * Direct callers: _tui_render_line@0x000182a0, _wprintw_safe@0x00018b48, _cprint@0x00018bec, _cprint_diff@0x00018ca4
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00017b38_ascii_fixup.s
 */
void ascii_fixup(char *input,char *output,uint capacity)

{
  byte bVar1;
  int iVar2;
  
  iVar2 = 0;
  do {
    while( true ) {
      while( true ) {
        while( true ) {
          while( true ) {
            while( true ) {
              bVar1 = *input;
              if ((bVar1 == 0) || (capacity <= iVar2 + 4U)) {
                output[iVar2] = '\0';
                return;
              }
              if ((((0x5e < (byte)(bVar1 - 0x20)) && (bVar1 != 10)) && (bVar1 != 0xd)) &&
                 (bVar1 != 9)) break;
              output[iVar2] = bVar1;
              iVar2 = iVar2 + 1;
              input = input + 1;
            }
            if ((bVar1 & 0x80) != 0) break;
            input = input + 1;
          }
          if (bVar1 != 0xc2) break;
          switch(input[1]) {
          case -0x60:
            output[iVar2] = ' ';
            iVar2 = iVar2 + 1;
            break;
          case -0x5f:
            output[iVar2] = '!';
            iVar2 = iVar2 + 1;
            break;
          default:
            output[iVar2] = '?';
            iVar2 = iVar2 + 1;
            break;
          case -0x57:
            output[iVar2] = '(';
            output[iVar2 + 1] = 'c';
            output[iVar2 + 2] = ')';
            iVar2 = iVar2 + 3;
            break;
          case -0x55:
            output[iVar2] = '\"';
            iVar2 = iVar2 + 1;
            break;
          case -0x53:
            output[iVar2] = '-';
            iVar2 = iVar2 + 1;
            break;
          case -0x52:
            output[iVar2] = '(';
            output[iVar2 + 1] = 'r';
            output[iVar2 + 2] = ')';
            iVar2 = iVar2 + 3;
            break;
          case -0x50:
            output[iVar2] = ' ';
            iVar2 = iVar2 + 1;
            break;
          case -0x49:
            output[iVar2] = '*';
            iVar2 = iVar2 + 1;
            break;
          case -0x45:
            output[iVar2] = '\"';
            iVar2 = iVar2 + 1;
          }
          input = input + 2;
        }
        if (bVar1 != 0xc3) break;
        bVar1 = input[1];
        if ((bVar1 ^ 0x80) < 7) {
          output[iVar2] = 'A';
        }
        else if (bVar1 == 0x87) {
          output[iVar2] = 'C';
        }
        else if ((byte)(bVar1 + 0x78) < 4) {
          output[iVar2] = 'E';
        }
        else if ((byte)(bVar1 + 0x74) < 4) {
          output[iVar2] = 'I';
        }
        else if (bVar1 == 0x90) {
          output[iVar2] = 'D';
        }
        else if (bVar1 == 0x91) {
          output[iVar2] = 'N';
        }
        else if ((byte)(bVar1 + 0x6e) < 5) {
          output[iVar2] = 'O';
        }
        else if (bVar1 == 0x98) {
          output[iVar2] = 'O';
        }
        else if ((byte)(bVar1 + 0x67) < 4) {
          output[iVar2] = 'U';
        }
        else if (bVar1 == 0x9d) {
          output[iVar2] = 'Y';
        }
        else if (bVar1 == 0x9e) {
          output[iVar2] = 'T';
        }
        else if (bVar1 == 0x9f) {
          output[iVar2] = 's';
        }
        else if ((byte)(bVar1 + 0x60) < 7) {
          output[iVar2] = 'a';
        }
        else if (bVar1 == 0xa7) {
          output[iVar2] = 'c';
        }
        else if ((byte)(bVar1 + 0x58) < 4) {
          output[iVar2] = 'e';
        }
        else if ((byte)(bVar1 + 0x54) < 4) {
          output[iVar2] = 'i';
        }
        else if (bVar1 == 0xb0) {
          output[iVar2] = 'd';
        }
        else if (bVar1 == 0xb1) {
          output[iVar2] = 'n';
        }
        else if ((byte)(bVar1 + 0x4e) < 5) {
          output[iVar2] = 'o';
        }
        else if (bVar1 == 0xb8) {
          output[iVar2] = 'o';
        }
        else if ((byte)(bVar1 + 0x47) < 4) {
          output[iVar2] = 'u';
        }
        else if (bVar1 == 0xbd) {
          output[iVar2] = 'y';
        }
        else if (bVar1 == 0xbe) {
          output[iVar2] = 't';
        }
        else if (bVar1 == 0xbf) {
          output[iVar2] = 'y';
        }
        else {
          output[iVar2] = '?';
        }
        iVar2 = iVar2 + 1;
        input = input + 2;
      }
      if (bVar1 == 0xe2) break;
      if ((bVar1 & 0xc0) == 0xc0) {
        input = input + 1;
        bVar1 = *input;
        while (((bVar1 & 0xc0) == 0x80 && (bVar1 != 0))) {
          input = input + 1;
          bVar1 = *input;
        }
      }
      else {
        input = input + 1;
      }
      output[iVar2] = '?';
      iVar2 = iVar2 + 1;
    }
    /* Original 0x17fbc/0x18140 use lbz, so these comparisons are unsigned
     * byte comparisons even though the surrounding string has char type. */
    if ((uint8_t)input[1] == 0x80) {
      switch(input[2]) {
      case -0x70:
        output[iVar2] = '-';
        iVar2 = iVar2 + 1;
        break;
      case -0x6f:
        output[iVar2] = '-';
        iVar2 = iVar2 + 1;
        break;
      default:
        output[iVar2] = '?';
        iVar2 = iVar2 + 1;
        break;
      case -0x6d:
        output[iVar2] = '-';
        iVar2 = iVar2 + 1;
        break;
      case -0x6c:
        output[iVar2] = '\'';
        iVar2 = iVar2 + 1;
        break;
      case -0x68:
        output[iVar2] = '\'';
        iVar2 = iVar2 + 1;
        break;
      case -0x67:
        output[iVar2] = '\"';
        iVar2 = iVar2 + 1;
        break;
      case -100:
        output[iVar2] = '\"';
        iVar2 = iVar2 + 1;
        break;
      case -0x5a:
        output[iVar2] = '.';
        iVar2 = iVar2 + 1;
        break;
      case -0x58:
        output[iVar2] = '.';
        iVar2 = iVar2 + 1;
        break;
      case -0x4d:
        output[iVar2] = '-';
        iVar2 = iVar2 + 1;
      }
    }
    else if ((uint8_t)input[1] == 0x86) {
      if ((uint8_t)input[2] == 0x92) {
        output[iVar2] = '-';
        output[iVar2 + 1] = '>';
        iVar2 = iVar2 + 2;
      }
      else if ((uint8_t)input[2] == 0x90) {
        output[iVar2] = '<';
        output[iVar2 + 1] = '-';
        iVar2 = iVar2 + 2;
      }
      else {
        if ((uint8_t)input[2] != 0x93) goto LAB_000181b8;
        output[iVar2] = '=';
        output[iVar2 + 1] = '>';
        iVar2 = iVar2 + 2;
      }
    }
    else {
LAB_000181b8:
      output[iVar2] = '?';
      iVar2 = iVar2 + 1;
    }
    input = input + 3;
  } while( true );
}

/*
 * _now_ms @ 0x00018240..0x000182a0
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in terminal_ui: now ms. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _now_ms(void)
 * Direct callers: _tui_run@0x0001df44, _start_turn@0x000202dc
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00018240_now_ms.s
 */
int now_ms(void)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x40] __attribute__((aligned(16)));
#define local_28 (*((struct timeval (*)[5])(app_frame + 0x8))) /* original SP - 0x28 */


  gettimeofday(local_28,(void *)0x0);
  return local_28[0].tv_sec * 1000 + local_28[0].tv_usec / 1000;

#undef local_28
}

/*
 * _tui_render_line @ 0x000182a0..0x000183dc
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in terminal_ui: tui render line. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _tui_render_line(char *param_1,int param_2)
 * Direct callers: _tui_print_token@0x000183dc
 * Possible side effects (direct-call closure, incomplete for indirect calls): reads process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000182a0_tui_render_line.s
 */
void tui_render_line(char *param_1,int param_2)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x10030] __attribute__((aligned(16)));
#define acStack_10018 (*((char (*)[65544])(app_frame + 0x8))) /* original SP - 0x10018 */

  char *pcVar1;
  bool bVar2;
  char cVar3;
  uint uVar4;

  cVar3 = *param_1;
  pcVar1 = param_1;
  uVar4 = _tui_in_code;
  while (cVar3 != '\0') {
    if (cVar3 == '`') {
      bVar2 = _tui_fence_bt == 2;
      _tui_fence_bt = _tui_fence_bt + 1;
      if (bVar2) {
        uVar4 = (uint)(uVar4 == 0);
        _tui_fence_bt = 0;
      }
    }
    else if (0 < _tui_fence_bt) {
      _tui_fence_bt = 0;
    }
    pcVar1 = pcVar1 + 1;
    cVar3 = *pcVar1;
  }
  _tui_in_code = uVar4;
  ascii_fixup(param_1,acStack_10018,0x10000);
  if (_pager_paused == 0) {
    if (param_2 != 0) {
      wattr_on(_w_chat,(*PTR__g_theme_000396c0)->pairs[0x12].attributes | 0x1300,(void *)0x0);
    }
    hl_line(_w_chat,(byte *)(acStack_10018),uVar4);
    if (param_2 != 0) {
      wattr_off(_w_chat,(*PTR__g_theme_000396c0)->pairs[0x12].attributes | 0x1300,(void *)0x0);
    }
  }
  else {
    _pager_lines = _pager_lines + 1;
  }
  return;

#undef acStack_10018
}

/*
 * _tui_print_token @ 0x000183dc..0x000189c0
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in terminal_ui: tui print token. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _tui_print_token(char *param_1)
 * Direct callers: _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): reads process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000183dc_tui_print_token.s
 */
/* WARNING: Removing unreachable block (ram,0x0001899c) */

void tui_print_token(char *param_1)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x11050] __attribute__((aligned(16)));
#define acStack_11038 (*((char (*)[65536])(app_frame + 0x8))) /* original SP - 0x11038 */
#define auStack_1038 (*((undefined1 (*)[4108])(app_frame + 0x10008))) /* original SP - 0x1038 */

  RecoveredTheme **ppRVar1;
  int iVar2;
  uint uVar3;
  char *pcVar4;
  char *pcVar5;
  int iVar6;
  undefined **ppuVar7;
  char *text;


  if ((param_1 != (char *)0x0) && (*param_1 != '\0')) {
    uVar3 = strlen(param_1);
    if (0xffff < uVar3) {
      uVar3 = 0xffff;
    }
    memcpy(acStack_11038,param_1,uVar3);
    acStack_11038[uVar3] = '\0';
    if (uVar3 != 0) {
      iVar2 = 0;
      do {
        if (acStack_11038[iVar2] == '\x01') {
          acStack_11038[iVar2] = '\n';
        }
        iVar2 = iVar2 + 1;
        uVar3 = uVar3 - 1;
      } while (uVar3 != 0);
    }
    text = acStack_11038;
    while (*text != '\0') {
      if (_tui_in_think == 0) {
        pcVar5 = "<thinking>";
        iVar6 = 0;
        pcVar4 = (char *)0x0;
        iVar2 = -1;
        ppuVar7 = (undefined **)(&APP_PTR_0003a644);
        do {
          pcVar5 = strstr(text,pcVar5);
          if ((pcVar5 != (char *)0x0) && ((pcVar4 == (char *)0x0 || (pcVar5 < pcVar4)))) {
            iVar2 = iVar6;
            pcVar4 = pcVar5;
          }
          iVar6 = iVar6 + 1;
          pcVar5 = (char *)(*ppuVar7);
          ppuVar7 = ppuVar7 + 4;
        } while (pcVar5 != (char *)0x0);
        if (pcVar4 == (char *)0x0) {
          do {
            pcVar4 = strchr(text,10);
            if (pcVar4 == (char *)0x0) {
              tui_render_line(text,0);
              uVar3 = strlen(text);
              text = text + uVar3;
            }
            else {
              uVar3 = (int)pcVar4 - (int)text;
              if (uVar3 < 0x1000) {
                if (uVar3 != 0) goto LAB_0001856c;
              }
              else {
                uVar3 = 0xfff;
LAB_0001856c:
                memcpy(auStack_1038,text,uVar3);
                auStack_1038[uVar3] = 0;
                tui_render_line((char *)(auStack_1038),0);
              }
              if (_pager_paused == 0) {
                wprintw(_w_chat,"\n");
              }
              text = pcVar4 + 1;
            }
          } while (*text != '\0');
        }
        else {
          if (text < pcVar4) {
            do {
              pcVar5 = strchr(text,10);
              if ((pcVar5 == (char *)0x0) || (pcVar4 <= pcVar5)) {
                uVar3 = (int)pcVar4 - (int)text;
                if (uVar3 != 0) {
                  if (0xfff < uVar3) {
                    uVar3 = 0xfff;
                  }
                  memcpy(auStack_1038,text,uVar3);
                  auStack_1038[uVar3] = 0;
                  tui_render_line((char *)(auStack_1038),0);
                }
                break;
              }
              uVar3 = (int)pcVar5 - (int)text;
              if (uVar3 < 0x1000) {
                if (uVar3 != 0) goto LAB_0001862c;
              }
              else {
                uVar3 = 0xfff;
LAB_0001862c:
                memcpy(auStack_1038,text,uVar3);
                auStack_1038[uVar3] = 0;
                tui_render_line((char *)(auStack_1038),0);
              }
              if (_pager_paused == 0) {
                wprintw(_w_chat,"\n");
              }
              text = pcVar5 + 1;
            } while (text < pcVar4);
          }
          ppRVar1 = PTR__g_theme_000396c0;
          _tui_in_think = 1;
          text = pcVar4 + (&DAT_0003a63c)[iVar2 * 4];
          _tui_think_tag_idx = iVar2;
          if (_pager_paused == 0) {
            wattr_on(_w_chat,(*PTR__g_theme_000396c0)->pairs[0x12].attributes | 0x1300,(void *)0x0)
            ;
            wprintw(_w_chat,"[thinking...]\n");
            wattr_off(_w_chat,(*ppRVar1)->pairs[0x12].attributes | 0x1300,(void *)0x0);
          }
          else {
            _pager_lines = _pager_lines + 1;
          }
        }
      }
      else {
        pcVar4 = strstr(text,*(char **)(&UNK_0003a638 + _tui_think_tag_idx * 0x10));
        if (pcVar4 == (char *)0x0) {
          do {
            pcVar4 = strchr(text,10);
            if (pcVar4 == (char *)0x0) {
              tui_render_line(text,1);
              uVar3 = strlen(text);
              text = text + uVar3;
            }
            else {
              uVar3 = (int)pcVar4 - (int)text;
              if (uVar3 < 0x1000) {
                if (uVar3 != 0) goto LAB_0001878c;
              }
              else {
                uVar3 = 0xfff;
LAB_0001878c:
                memcpy(auStack_1038,text,uVar3);
                auStack_1038[uVar3] = 0;
                tui_render_line((char *)(auStack_1038),1);
              }
              if (_pager_paused == 0) {
                wprintw(_w_chat,"\n");
              }
              text = pcVar4 + 1;
            }
          } while (*text != '\0');
        }
        else {
          if (text < pcVar4) {
            do {
              pcVar5 = strchr(text,10);
              if ((pcVar5 == (char *)0x0) || (pcVar4 <= pcVar5)) {
                uVar3 = (int)pcVar4 - (int)text;
                if (uVar3 != 0) {
                  if (0xfff < uVar3) {
                    uVar3 = 0xfff;
                  }
                  memcpy(auStack_1038,text,uVar3);
                  auStack_1038[uVar3] = 0;
                  tui_render_line((char *)(auStack_1038),1);
                }
                break;
              }
              uVar3 = (int)pcVar5 - (int)text;
              if (uVar3 < 0x1000) {
                if (uVar3 != 0) goto LAB_00018864;
              }
              else {
                uVar3 = 0xfff;
LAB_00018864:
                memcpy(auStack_1038,text,uVar3);
                auStack_1038[uVar3] = 0;
                tui_render_line((char *)(auStack_1038),1);
              }
              if (_pager_paused == 0) {
                wprintw(_w_chat,"\n");
              }
              text = pcVar5 + 1;
            } while (text < pcVar4);
          }
          if (_pager_paused == 0) {
            wattr_off(_w_chat,(*PTR__g_theme_000396c0)->pairs[0x12].attributes | 0x1300,(void *)0x0
                      );
          }
          else {
            _pager_lines = _pager_lines + 1;
          }
          _tui_in_think = 0;
          text = pcVar4 + *(int *)(&UNK_0003a640 + _tui_think_tag_idx * 0x10);
          _tui_think_tag_idx = -1;
        }
      }
    }
  }
  return;

#undef acStack_11038
#undef auStack_1038
}

/*
 * _chat_refresh @ 0x000189c0..0x00018b14
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in terminal_ui: chat refresh. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _chat_refresh(void)
 * Direct callers: _msg@0x00018f40, _handle_resize@0x00019fd8, _read_input@0x0001a74c, _read_paste@0x0001ae90, _handle_cmd@0x0001b004, _tui_run@0x0001df44, _start_turn@0x000202dc
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000189c0_chat_refresh.s
 */
void chat_refresh(void)

{
  int iVar1;
  int iVar2;
  int iVar3;
  int iVar4;
  
  iVar1 = _th + ((_streaming == 0) - 1);
  iVar4 = iVar1 + -3;
  iVar2 = _tw;
  if ((_g_app != 0) && (_g_app->sidebar_visible != 0)) {
    iVar2 = _tw + -0x1a;
  }
  if (iVar4 < 1) {
    iVar4 = 1;
  }
  iVar3 = 0;
  if (iVar4 <= _chat_y) {
    iVar3 = (_chat_y - iVar4) + 1;
  }
  if (_chat_user_scrolled != 0) {
    if (iVar3 < _chat_top) {
      _chat_user_scrolled = 0;
      _chat_top = iVar3;
    }
    if (-1 < _chat_top) goto LAB_00018acc;
    _chat_top = 0;
    _chat_user_scrolled = 0;
    iVar3 = _chat_top;
  }
  _chat_top = iVar3;
  if (_chat_top < 0) {
    _chat_top = 0;
  }
LAB_00018acc:
  iVar1 = iVar1 + -2;
  if (iVar1 < 2) {
    iVar1 = 2;
  }
  pnoutrefresh(_w_chat,_chat_top,0,2,0,iVar1,iVar2 + -1);
  return;
}

/*
 * _chat_advance @ 0x00018b14..0x00018b48
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in terminal_ui: chat advance. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _chat_advance(void)
 * Direct callers: _msg@0x00018f40, _read_input@0x0001a74c, _read_paste@0x0001ae90, _handle_cmd@0x0001b004, _tui_run@0x0001df44, _start_turn@0x000202dc
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00018b14_chat_advance.s
 */
void chat_advance(void)

{
  _chat_y = -1;
  if (_w_chat != NULL) {
    _chat_y = getcury(_w_chat);
  }
  return;
}

/*
 * _wprintw_safe @ 0x00018b48..0x00018bec
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in terminal_ui: wprintw safe. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _wprintw_safe(void *window, const char *format, ...)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00018b48_wprintw_safe.s
 */
/* Analyst reconstruction of _wprintw_safe at 0x00018b48. See index/manual-review.json. */
void wprintw_safe(void *window, const char *format, ...)
{
    char formatted[65536], ascii[65536];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(formatted, 65536, format, arguments);
    va_end(arguments);
    ascii_fixup(formatted, ascii, 65536);
    wprintw(window, "%s", ascii);
}

/*
 * _cprint @ 0x00018bec..0x00018ca4
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in terminal_ui: cprint. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _cprint(void *window,int color,char *text)
 * Direct callers: _cprintf@0x00018ed8, _msg@0x00018f40, _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00018bec_cprint.s
 */
void cprint(void *window,int color,char *text)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x10040] __attribute__((aligned(16)));
#define acStack_10028 (*((char (*)[65556])(app_frame + 0x8))) /* original SP - 0x10028 */

  uint uVar1;
  RecoveredTheme **ppRVar2;

  if (text == (char *)0x0) {
    text = "";
  }
  ascii_fixup(text,acStack_10028,0x10000);
  ppRVar2 = PTR__g_theme_000396c0;
  uVar1 = (color + 1) * 0x100;
  wattr_on(window,uVar1 | (*PTR__g_theme_000396c0)->pairs[color].attributes,(void *)0x0);
  wprintw(window,"%s",acStack_10028);
  wattr_off(window,uVar1 | (*ppRVar2)->pairs[color].attributes,(void *)0x0);
  return;

#undef acStack_10028
}

/*
 * _cprint_diff @ 0x00018ca4..0x00018ed8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in terminal_ui: cprint diff. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _cprint_diff(void *window,char *text)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00018ca4_cprint_diff.s
 */
void cprint_diff(void *window,char *text)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x11040] __attribute__((aligned(16)));
#define acStack_11028 (*((char (*)[2])(app_frame + 0x8))) /* original SP - 0x11028 */
#define acStack_11026 (*((char (*)[65534])(app_frame + 0xa))) /* original SP - 0x11026 */
#define local_1028 (*((char (*)[4116])(app_frame + 0x10008))) /* original SP - 0x1028 */

  char cVar1;
  RecoveredTheme **ppRVar2;
  int iVar3;
  char cVar4;
  int iVar5;
  char *pcVar6;

  if (text == (char *)0x0) {
    text = "";
  }
  ascii_fixup(text,acStack_11028,0x10000);
  pcVar6 = acStack_11028;
  do {
    ppRVar2 = PTR__g_theme_000396c0;
    cVar1 = *pcVar6;
    if (cVar1 == '\0') {
      return;
    }
    iVar5 = 0;
    if (cVar1 != '\n') {
      pcVar6 = pcVar6 + 1;
      cVar4 = *pcVar6;
      iVar5 = 1;
      local_1028[0] = cVar1;
      if (cVar4 != '\0') {
        iVar5 = 1;
        iVar3 = 0xfff;
        do {
          if ((cVar4 == '\n') || (iVar3 = iVar3 + -1, iVar3 == 0)) break;
          local_1028[iVar5] = cVar4;
          iVar5 = iVar5 + 1;
          pcVar6 = pcVar6 + 1;
          cVar4 = *pcVar6;
        } while (cVar4 != '\0');
      }
    }
    local_1028[iVar5] = '\0';
    if (*pcVar6 == '\n') {
      pcVar6 = pcVar6 + 1;
    }
    if (local_1028[0] == '+') {
      wattr_on(window,(*PTR__g_theme_000396c0)->pairs[0xb].attributes | 0xc00,(void *)0x0);
      wprintw(window,"%s\n",local_1028);
      wattr_off(window,(*ppRVar2)->pairs[0xb].attributes | 0xc00,(void *)0x0);
    }
    else if (local_1028[0] == '-') {
      wattr_on(window,(*PTR__g_theme_000396c0)->pairs[9].attributes | 0xa00,(void *)0x0);
      wprintw(window,"%s\n",local_1028);
      wattr_off(window,(*ppRVar2)->pairs[9].attributes | 0xa00,(void *)0x0);
    }
    else if (local_1028[0] == '@') {
      wattr_on(window,(*PTR__g_theme_000396c0)->pairs[0xc].attributes | 0xd00,(void *)0x0);
      wprintw(window,"%s\n",local_1028);
      wattr_off(window,(*ppRVar2)->pairs[0xc].attributes | 0xd00,(void *)0x0);
    }
    else {
      wprintw(window,"%s\n",local_1028);
    }
  } while( true );

#undef acStack_11028
#undef acStack_11026
#undef local_1028
}

/*
 * _cprintf @ 0x00018ed8..0x00018f40
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in terminal_ui: cprintf. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _cprintf(void *window, int color, const char *format, ...)
 * Direct callers: _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00018ed8_cprintf.s
 */
/* Analyst reconstruction of _cprintf at 0x00018ed8. See index/manual-review.json. */
void cprintf(void *window, int color, const char *format, ...)
{
    char text[4096];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(text, 4096, format, arguments);
    va_end(arguments);
    cprint(window, color, text);
}

/*
 * _msg @ 0x00018f40..0x00019018
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in terminal_ui: msg. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _msg(int color,char *label,char *text)
 * Direct callers: _msgf@0x00019018, _pick_model@0x0001a620, _handle_cmd@0x0001b004, _tui_run@0x0001df44, _start_turn@0x000202dc
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00018f40_msg.s
 */
void msg(int color,char *label,char *text)

{
  uint uVar1;
  RecoveredTheme **ppRVar2;
  
  ppRVar2 = PTR__g_theme_000396c0;
  uVar1 = (color + 1) * 0x100;
  wattr_on(_w_chat,uVar1 | (*PTR__g_theme_000396c0)->pairs[color].attributes | 0x40000,(void *)0x0)
  ;
  wprintw(_w_chat," %s ",label);
  wattr_off(_w_chat,uVar1 | (*ppRVar2)->pairs[color].attributes | 0x40000,(void *)0x0);
  wprintw(_w_chat," ");
  cprint(_w_chat,3,text);
  wprintw(_w_chat,"\n");
  chat_advance();
  chat_refresh();
  return;
}

/*
 * _msgf @ 0x00019018..0x00019080
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in terminal_ui: msgf. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _msgf(int color, const char *label, const char *format, ...)
 * Direct callers: _handle_cmd@0x0001b004, _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00019018_msgf.s
 */
/* Analyst reconstruction of _msgf at 0x00019018. See index/manual-review.json. */
void msgf(int color, const char *label, const char *format, ...)
{
    char text[4096];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(text, 4096, format, arguments);
    va_end(arguments);
    msg(color, (char *)(label), text);
}

/*
 * _redraw_title @ 0x00019080..0x000192f8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Render the indicated curses UI region: title. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _redraw_title(RecoveredAppState *app)
 * Direct callers: _handle_resize@0x00019fd8, _handle_cmd@0x0001b004, _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00019080_redraw_title.s
 */
void redraw_title(RecoveredAppState *app)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x140] __attribute__((aligned(16)));
#define local_128 (*((char (*)[276])(app_frame + 0x8))) /* original SP - 0x128 */

  RecoveredTheme **ppRVar1;
  int iVar2;
  char *pcVar3;
  uint uVar4;
  char *text;
  int iVar5;

  werase(_w_title);
  wattr_on(_w_title,(*PTR__g_theme_000396c0)->pairs[5].attributes | 0x600,(void *)0x0);
  if (0 < _tw) {
    iVar5 = 0;
    do {
      iVar2 = wmove(_w_title,0,iVar5);
      if (iVar2 != -1) {
        waddch(_w_title,0x20);
      }
      iVar5 = iVar5 + 1;
    } while (iVar5 < _tw);
  }
  ppRVar1 = PTR__g_theme_000396c0;
  wattr_off(_w_title,(*PTR__g_theme_000396c0)->pairs[5].attributes | 0x600,(void *)0x0);
  wattr_on(_w_title,(*ppRVar1)->pairs[0].attributes | 0x100,(void *)0x0);
  mvwprintw(_w_title,0,1,"* PowerPC Code");
  wattr_off(_w_title,(*ppRVar1)->pairs[0].attributes | 0x100,(void *)0x0);
  text = (app->config).working_directory;
  pcVar3 = strrchr(text,0x2f);
  if (pcVar3 != (char *)0x0) {
    text = pcVar3 + 1;
  }
  app_format_truncated(local_128,0x100,"[%s]  %s",(app->config).model,text);
  iVar5 = _tw;
  uVar4 = strlen(local_128);
  iVar2 = (iVar5 - uVar4) + -1;
  if (iVar2 < 0x14) {
    iVar2 = 0x14;
  }
  if (iVar5 <= (int)(uVar4 + 1)) {
    local_128[0] = '\0';
  }
  wattr_on(_w_title,(*PTR__g_theme_000396c0)->pairs[4].attributes | 0x500,(void *)0x0);
  if (local_128[0] != '\0') {
    mvwprintw(_w_title,0,iVar2,"%s",local_128);
  }
  ppRVar1 = PTR__g_theme_000396c0;
  wattr_off(_w_title,(*PTR__g_theme_000396c0)->pairs[4].attributes | 0x500,(void *)0x0);
  if ((_streaming != 0) && (0x15 < _tw)) {
    wattr_on(_w_title,(*ppRVar1)->pairs[0].attributes | 0x100,(void *)0x0);
    mvwprintw(_w_title,0,_tw + -0xc,"[thinking]");
    wattr_off(_w_title,(*ppRVar1)->pairs[0].attributes | 0x100,(void *)0x0);
  }
  wnoutrefresh(_w_title);
  return;

#undef local_128
}

/*
 * _redraw_sep @ 0x000192f8..0x000193cc
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Render the indicated curses UI region: sep. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _redraw_sep(void)
 * Direct callers: _handle_resize@0x00019fd8, _tui_init@0x0001daac, _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000192f8_redraw_sep.s
 */
void redraw_sep(void)

{
  int iVar1;
  int iVar2;
  
  werase(_w_sep);
  wattr_on(_w_sep,(*PTR__g_theme_000396c0)->pairs[0x12].attributes | 0x1300,(void *)0x0);
  if (0 < _tw) {
    iVar2 = 0;
    do {
      iVar1 = wmove(_w_sep,0,iVar2);
      if (iVar1 != -1) {
        waddch(_w_sep,0x2d);
      }
      iVar2 = iVar2 + 1;
    } while (iVar2 < _tw);
  }
  wattr_off(_w_sep,(*PTR__g_theme_000396c0)->pairs[0x12].attributes | 0x1300,(void *)0x0);
  wnoutrefresh(_w_sep);
  return;
}

/*
 * _redraw_status @ 0x000193cc..0x00019930
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Render the indicated curses UI region: status. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _redraw_status(RecoveredAppState *app)
 * Direct callers: _handle_resize@0x00019fd8, _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000193cc_redraw_status.s
 */
void redraw_status(RecoveredAppState *app)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x1c0] __attribute__((aligned(16)));
#define acStack_1a8 (*((char (*)[64])(app_frame + 0x8))) /* original SP - 0x1a8 */
#define acStack_168 (*((char (*)[62])(app_frame + 0x48))) /* original SP - 0x168 */
#define acStack_12a (*((char (*)[274])(app_frame + 0x86))) /* original SP - 0x12a */

  RecoveredTheme **ppRVar1;
  int iVar2;
  int *piVar3;
  time_t tVar4;
  uint uVar5;
  uint uVar6;
  char *pcVar7;
  char *pcVar8;
  char *pcVar9;
  int iVar10;

  werase(_w_status);
  ppRVar1 = PTR__g_theme_000396c0;
  if ((app->plan_present == 0) || (iVar2 = (app->plan).count, iVar2 < 1)) {
    if (app->retry_remaining < 1) {
      if (_streaming == 0) {
        if (0 < app->retry_count) {
          wattr_on(_w_status,(*PTR__g_theme_000396c0)->pairs[0xb].attributes | 0xc00,(void *)0x0);
          mvwprintw(_w_status,0,1,"[done]");
          wattr_off(_w_status,(*ppRVar1)->pairs[0xb].attributes | 0xc00,(void *)0x0);
        }
      }
      else {
        wattr_on(_w_status,(*PTR__g_theme_000396c0)->pairs[0xb].attributes | 0xc00,(void *)0x0);
        uVar6 = app->spinner;
        iVar2 = uVar6 + (((int)uVar6 >> 2) + (uint)((int)uVar6 < 0 && (uVar6 & 3) != 0)) * -4;
        tVar4 = time((time_t *)0x0);
        if (iVar2 < 1) {
          pcVar8 = "";
          pcVar7 = pcVar8;
          pcVar9 = pcVar8;
        }
        else if (iVar2 < 2) {
          pcVar8 = ".";
          pcVar7 = "";
          pcVar9 = "";
        }
        else if (iVar2 < 3) {
          pcVar8 = ".";
          pcVar7 = pcVar8;
          pcVar9 = "";
        }
        else {
          pcVar8 = ".";
          pcVar7 = ".";
          pcVar9 = pcVar8;
        }
        snprintf(acStack_1a8,0x40,"* thinking%s%s%s%s (%ds)",pcVar8,pcVar7,pcVar9,"",
                           (int)(tVar4 - app->turn_start_time));
        mvwprintw(_w_status,0,1,"%s",acStack_1a8);
        wattr_off(_w_status,(*PTR__g_theme_000396c0)->pairs[0xb].attributes | 0xc00,(void *)0x0);
      }
    }
    else {
      wattr_on(_w_status,(*PTR__g_theme_000396c0)->pairs[10].attributes | 0xb00,(void *)0x0);
      mvwprintw(_w_status,0,1,"* retrying in %ds",app->retry_remaining / 1000);
      wattr_off(_w_status,(*ppRVar1)->pairs[10].attributes | 0xb00,(void *)0x0);
    }
  }
  else {
    piVar3 = &(app->plan).steps[0].status;
    iVar10 = 0;
    do {
      if (*piVar3 == 2) {
        iVar10 = iVar10 + 1;
      }
      piVar3 = piVar3 + 0xa02;
      iVar2 = iVar2 + -1;
    } while (iVar2 != 0);
    wattr_on(_w_status,(*PTR__g_theme_000396c0)->pairs[0xb].attributes | 0xc00,(void *)0x0);
    tVar4 = time((time_t *)0x0);
    mvwprintw(_w_status,0,1,"PLAN: %d/%d (%ds)",iVar10,(app->plan).count,
               tVar4 - app->turn_start_time);
    wattr_off(_w_status,(*ppRVar1)->pairs[0xb].attributes | 0xc00,(void *)0x0);
  }
  ppRVar1 = PTR__g_theme_000396c0;
  if ((_chat_user_scrolled != 0) && (0x1d < (int)_tw)) {
    iVar2 = _th + (uint)(_streaming == 0) + -4;
    if (iVar2 < 1) {
      iVar2 = 1;
    }
    if ((_chat_y < iVar2) || (iVar2 = (_chat_y - iVar2) + 1, iVar2 < 1)) {
      iVar2 = 0;
    }
    else {
      iVar2 = (_chat_top * 100) / iVar2;
    }
    wattr_on(_w_status,(*PTR__g_theme_000396c0)->pairs[10].attributes | 0xb00,(void *)0x0);
    mvwprintw(_w_status,0,((int)_tw >> 1) + (uint)((int)_tw < 0 && (_tw & 1) != 0) + -8,
               "scroll %d%% (pgup/pgdn)",iVar2);
    wattr_off(_w_status,(*ppRVar1)->pairs[10].attributes | 0xb00,(void *)0x0);
  }
  if ((app->usage_known == 0) || ((int)_tw < 0x3c)) {
    iVar2 = 0;
  }
  else {
    iVar2 = snprintf(acStack_12a + 2,0x100,"%d in | %d out | %d KTok",app->input_tokens,
                               app->output_tokens,app->total_tokens / 1000);
    iVar10 = snprintf(acStack_12a + 2 + iVar2,0x100 - iVar2,"  ");
    iVar2 = iVar2 + iVar10;
  }
  token_budget_str(acStack_168,0x40,&app->conversation,&app->config);
  conversation_mem_str(acStack_1a8,0x20,&app->conversation);
  app_format_truncated(acStack_12a + 2 + iVar2,0x100 - iVar2,"%s | %s",acStack_1a8,acStack_168);
  uVar6 = _tw;
  iVar2 = _tw - 2;
  uVar5 = strlen(acStack_12a + 2);
  if (iVar2 <= (int)uVar5) {
    acStack_12a[uVar6] = '\0';
  }
  uVar6 = strlen(acStack_12a + 2);
  ppRVar1 = PTR__g_theme_000396c0;
  wattr_on(_w_status,(*PTR__g_theme_000396c0)->pairs[4].attributes | 0x500,(void *)0x0);
  mvwprintw(_w_status,0,(_tw - uVar6) + -1,"%s",acStack_12a + 2);
  wattr_off(_w_status,(*ppRVar1)->pairs[4].attributes | 0x500,(void *)0x0);
  wnoutrefresh(_w_status);
  return;

#undef acStack_1a8
#undef acStack_168
#undef acStack_12a
}

/*
 * _redraw_input @ 0x00019930..0x00019a6c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Render the indicated curses UI region: input. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _redraw_input(char *param_1)
 * Direct callers: _read_input@0x0001a74c, _tui_init@0x0001daac, _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00019930_redraw_input.s
 */
void redraw_input(char *param_1)

{
  RecoveredTheme **ppRVar1;
  uint uVar2;
  int iVar3;
  
  werase(_w_input);
  ppRVar1 = PTR__g_theme_000396c0;
  wattr_on(_w_input,(*PTR__g_theme_000396c0)->pairs[0].attributes | 0x100,(void *)0x0);
  mvwprintw(_w_input,0,0,"> ");
  wattr_off(_w_input,(*ppRVar1)->pairs[0].attributes | 0x100,(void *)0x0);
  if ((param_1 != (char *)0x0) && (*param_1 != '\0')) {
    wattr_on(_w_input,(*ppRVar1)->pairs[3].attributes | 0x400,(void *)0x0);
    iVar3 = _tw + -4;
    uVar2 = strlen(param_1);
    if (iVar3 < (int)uVar2) {
      wprintw(_w_input,"...%s",param_1 + (uVar2 - iVar3) + 3);
    }
    else {
      wprintw(_w_input,"%s",param_1);
    }
    wattr_off(_w_input,(*PTR__g_theme_000396c0)->pairs[3].attributes | 0x400,(void *)0x0);
  }
  wnoutrefresh(_w_input);
  return;
}

/*
 * _redraw_sidebar @ 0x00019a6c..0x00019fb8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Render the indicated curses UI region: sidebar. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _redraw_sidebar(RecoveredAppState *app)
 * Direct callers: _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; executes an argument vector through PATH; creates a same-account child process; reads a descriptor or IPC channel
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00019a6c_redraw_sidebar.s
 */
void redraw_sidebar(RecoveredAppState *app)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0xb0] __attribute__((aligned(16)));
#define acStack_98 (*((char (*)[4])(app_frame + 0x8))) /* original SP - 0x98 */
#define local_94 (*((RecoveredDString (*)[2])(app_frame + 0xc))) /* original SP - 0x94 */
#define local_74 (*((char * *)(app_frame + 0x2c))) /* original SP - 0x74 */
#define local_70 (*((char * *)(app_frame + 0x30))) /* original SP - 0x70 */
#define local_6c (*((char * *)(app_frame + 0x34))) /* original SP - 0x6c */
#define local_68 (*((char * *)(app_frame + 0x38))) /* original SP - 0x68 */
#define local_64 (*((char * *)(app_frame + 0x3c))) /* original SP - 0x64 */
#define local_60 (*((char * *)(app_frame + 0x40))) /* original SP - 0x60 */
#define local_5c (*((char * *)(app_frame + 0x44))) /* original SP - 0x5c */
#define local_58 (*((char * *)(app_frame + 0x48))) /* original SP - 0x58 */
#define local_54 (*((char * *)(app_frame + 0x4c))) /* original SP - 0x54 */
#define local_50 (*((char * *)(app_frame + 0x50))) /* original SP - 0x50 */
#define local_4c (*((char * *)(app_frame + 0x54))) /* original SP - 0x4c */
#define local_48 (*((char * *)(app_frame + 0x58))) /* original SP - 0x48 */
#define local_44 (*((undefined4 *)(app_frame + 0x5c))) /* original SP - 0x44 */

  RecoveredTheme **ppRVar1;
  time_t tVar2;
  char *pcVar3;
  int iVar4;
  char *pcVar5;
  int iVar6;
  uint length;
  int iVar7;
  RecoveredDString *text;

  if (((_w_sidebar != (WINDOW *)0x0) && (app->sidebar_visible != 0)) && (0x2d < _tw)) {
    werase(_w_sidebar);
    tVar2 = time((time_t *)0x0);
    if ((_last_tree_6744 == 0) || (0x3c < tVar2 - _last_tree_6744)) {
      _last_tree_6744 = tVar2;
      ds_clear((RecoveredDString *)&_sidebar_content);
      iVar6 = (app->config).sidebar_depth;
      if (iVar6 < 1) {
        iVar6 = 1;
      }
      app_format_truncated(acStack_98,4,"%d",iVar6);
      local_74 = "find";
      local_70 = ".";
      local_6c = "-maxdepth";
      local_68 = acStack_98;
      local_64 = "-type";
      local_60 = "f";
      local_5c = "-not";
      local_58 = "-path";
      local_54 = "*/.git/*";
      local_50 = "-not";
      local_4c = "-path";
      local_48 = "*/.*";
      local_44 = 0;
      ds_init(local_94);
      iVar6 = safe_exec_read(&local_74,(app->config).working_directory,local_94);
      if ((-1 < iVar6) && (local_94[0].length != 0)) {
        pcVar3 = ds_cstr(local_94);
        ds_append((RecoveredDString *)&_sidebar_content,pcVar3);
      }
      ds_free(local_94);
    }
    wattr_on(_w_sidebar,(*PTR__g_theme_000396c0)->pairs[5].attributes | 0x600,(void *)0x0);
    if (0 < _th + -2) {
      iVar6 = 0;
      do {
        iVar4 = wmove(_w_sidebar,iVar6,0);
        if (iVar4 != -1) {
          waddch(_w_sidebar,0x20);
        }
        iVar6 = iVar6 + 1;
      } while (iVar6 < _th + -2);
    }
    ppRVar1 = PTR__g_theme_000396c0;
    wattr_off(_w_sidebar,(*PTR__g_theme_000396c0)->pairs[5].attributes | 0x600,(void *)0x0);
    wattr_on(_w_sidebar,(*ppRVar1)->pairs[0].attributes | 0x200100,(void *)0x0);
    mvwprintw(_w_sidebar,0,1,"Files:");
    wattr_off(_w_sidebar,(*ppRVar1)->pairs[0].attributes | 0x200100,(void *)0x0);
    pcVar3 = ds_cstr((RecoveredDString *)&_sidebar_content);
    ppRVar1 = PTR__g_theme_000396c0;
    if ((*pcVar3 != '\0') && (1 < _th + -4)) {
      iVar6 = 1;
      while( true ) {
        pcVar5 = strchr(pcVar3,10);
        if (pcVar5 == (char *)0x0) break;
        length = (int)pcVar5 - (int)pcVar3;
        if (0x1e < length) {
          length = 0x1e;
        }
        memcpy(local_94,pcVar3,length);
        *(undefined1 *)((int)&local_94[0].data + length) = 0;
        text = local_94;
        if ((APP_FIELD8(local_94[0].data, 0) == '.') &&
           (text = (RecoveredDString *)((int)&local_94[0].data + 2), APP_FIELD8(local_94[0].data, 1) != '/'))
        {
          text = local_94;
        }
        pcVar3 = strrchr((char *)text,0x2e);
        if (pcVar3 == (char *)0x0) {
LAB_00019e68:
          iVar7 = 4;
          iVar4 = 5;
          goto LAB_00019e88;
        }
        iVar4 = strcmp(pcVar3,".c");
        if (iVar4 == 0) {
LAB_00019e74:
          iVar7 = 0xd;
          iVar4 = 0xe;
        }
        else {
          iVar4 = strcmp(pcVar3,".h");
          if (iVar4 == 0) goto LAB_00019e74;
          iVar4 = strcmp(pcVar3,".py");
          if (iVar4 == 0) {
            iVar7 = 0xe;
            iVar4 = 0xf;
          }
          else {
            iVar4 = strcmp(pcVar3,".md");
            if (iVar4 != 0) {
              iVar4 = strcmp(pcVar3,".txt");
              if (iVar4 != 0) {
                iVar4 = strcmp(pcVar3,".sh");
                if (iVar4 == 0) {
                  iVar7 = 0xb;
                  iVar4 = 0xc;
                }
                else {
                  iVar4 = strcmp(pcVar3,".json");
                  if (iVar4 != 0) goto LAB_00019e68;
                  iVar7 = 10;
                  iVar4 = 0xb;
                }
                goto LAB_00019e88;
              }
            }
            iVar7 = 0xc;
            iVar4 = 0xd;
          }
        }
LAB_00019e88:
        wattr_on(_w_sidebar,iVar4 << 8 | (*ppRVar1)->pairs[iVar7].attributes,(void *)0x0);
        mvwprintw(_w_sidebar,iVar6,1,"%s",text);
        wattr_off(_w_sidebar,iVar4 << 8 | (*ppRVar1)->pairs[iVar7].attributes,(void *)0x0);
        pcVar3 = pcVar5 + 1;
        iVar6 = iVar6 + 1;
        if ((pcVar5[1] == '\0') || (_th + -4 <= iVar6)) break;
      }
    }
    ppRVar1 = PTR__g_theme_000396c0;
    wattr_on(_w_sidebar,(*PTR__g_theme_000396c0)->pairs[4].attributes | 0x500,(void *)0x0);
    mvwprintw(_w_sidebar,_th + -3,1,"Ctrl+L: toggle");
    wattr_off(_w_sidebar,(*ppRVar1)->pairs[4].attributes | 0x500,(void *)0x0);
    iVar6 = _tw + -0x1a;
    if (iVar6 < 0) {
      iVar6 = 0;
    }
    pnoutrefresh(_w_sidebar,0,0,2,iVar6,_th + -2,_tw + -1);
  }
  return;

#undef acStack_98
#undef local_94
#undef local_74
#undef local_70
#undef local_6c
#undef local_68
#undef local_64
#undef local_60
#undef local_5c
#undef local_58
#undef local_54
#undef local_50
#undef local_4c
#undef local_48
#undef local_44
}

/*
 * _sigwinch_h @ 0x00019fb8..0x00019fd8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in terminal_ui: sigwinch h. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _sigwinch_h(void)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00019fb8_sigwinch_h.s
 */
void sigwinch_h(void)

{
  _resize_pending = 1;
  return;
}

/*
 * _handle_resize @ 0x00019fd8..0x0001a254
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in terminal_ui: handle resize. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _handle_resize(void)
 * Direct callers: _tui_theme_set@0x0001a254, _handle_cmd@0x0001b004, _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00019fd8_handle_resize.s
 */
void handle_resize(void)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x40] __attribute__((aligned(16)));
#define local_28 (*((ushort *)(app_frame + 0x8))) /* original SP - 0x28 */
#define local_26 (*((ushort *)(app_frame + 0xa))) /* original SP - 0x26 */

  undefined *puVar1;
  int iVar2;
  int iVar3;


  iVar2 = ioctl(0,0x40087468,&local_28);
  if (((iVar2 == 0) && (local_28 != 0)) && (local_26 != 0)) {
    resizeterm((uint)local_28,(uint)local_26);
  }
  endwin();
  puVar1 = (undefined *)(PTR_000396bc);
  wrefresh(*(WINDOW **)PTR_000396bc);
  iVar2 = *(int *)puVar1;
  _th = -1;
  if (iVar2 != 0) {
    _th = *(short *)(iVar2 + 4) + 1;
  }
  _tw = -1;
  if (iVar2 != 0) {
    _tw = *(short *)(iVar2 + 6) + 1;
  }
  iVar2 = _tw;
  if (((_g_app != (RecoveredAppState *)0x0) && (_g_app->sidebar_visible != 0)) && (0x2e < _tw)) {
    iVar2 = _tw + -0x1a;
  }
  if (_w_title != (WINDOW *)0x0) {
    delwin(_w_title);
    _w_title = newwin(1,_tw,0,0);
  }
  if (_w_sep != (WINDOW *)0x0) {
    delwin(_w_sep);
    _w_sep = newwin(1,_tw,1,0);
  }
  if (_w_status != (WINDOW *)0x0) {
    delwin(_w_status);
    _w_status = newwin(1,_tw,_th + -2,0);
  }
  if (_w_input != (WINDOW *)0x0) {
    delwin(_w_input);
    _w_input = newwin(1,_tw,_th + -1,0);
    wtimeout(_w_input,0x32);
    keypad(_w_input,1);
  }
  if (_w_sidebar != (WINDOW *)0x0) {
    iVar3 = wresize(_w_sidebar,0x1000,0x1a);
    if (iVar3 != 0) {
      delwin(_w_sidebar);
      _w_sidebar = newpad(0x1000,0x1a);
    }
  }
  if (_w_chat != (WINDOW *)0x0) {
    iVar3 = wresize(_w_chat,0x1000,iVar2);
    if (iVar3 != 0) {
      delwin(_w_chat);
      _w_chat = newpad(0x1000,iVar2);
      scrollok(_w_chat,1);
    }
  }
  if (_g_app != (RecoveredAppState *)0x0) {
    _chat_top = 0;
    _chat_user_scrolled = 0;
    redraw_title(_g_app);
    redraw_sep();
    redraw_status(_g_app);
    chat_refresh();
  }
  return;

#undef local_28
#undef local_26
}

/*
 * _tui_theme_set @ 0x0001a254..0x0001a2cc
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in terminal_ui: tui theme set. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _tui_theme_set(char *name)
 * Direct callers: _main@0x00005498
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0001a254_tui_theme_set.s
 */
int tui_theme_set(char *name)

{
  int iVar1;
  RecoveredTheme *theme;
  
  theme = theme_find(name);
  iVar1 = -1;
  if (theme != (RecoveredTheme *)0x0) {
    iVar1 = platform_is_tiger();
    if (iVar1 != 0) {
      theme = &_theme_light;
    }
    theme_apply(theme);
    iVar1 = 0;
    if (_g_app != 0) {
      handle_resize();
      iVar1 = 0;
    }
  }
  return iVar1;
}

/*
 * _tui_theme_current @ 0x0001a2cc..0x0001a304
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in terminal_ui: tui theme current. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): char * _tui_theme_current(void)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0001a2cc_tui_theme_current.s
 */
char * tui_theme_current(void)

{
  if (*PTR__g_theme_000396c0 == (RecoveredTheme *)0x0) {
    return "default";
  }
  return (*PTR__g_theme_000396c0)->name;
}

/*
 * _tui_theme_list @ 0x0001a304..0x0001a324
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in terminal_ui: tui theme list. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _tui_theme_list(void)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0001a304_tui_theme_list.s
 */
void tui_theme_list(char *output, int capacity)

{
  theme_names_csv((undefined1 *)(output), capacity);
  return;
}

/*
 * _tui_menu_select @ 0x0001a324..0x0001a620
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in terminal_ui: tui menu select. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _tui_menu_select(char *title,char **items,int count,int selected)
 * Direct callers: _pick_model@0x0001a620, _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0001a324_tui_menu_select.s
 */
int tui_menu_select(char *title,char **items,int count,int selected)

{
  RecoveredTheme **ppRVar1;
  WINDOW *pWVar2;
  int iVar3;
  uint uVar4;
  uint uVar5;
  int iVar6;
  int iVar7;
  int iVar8;
  char **ppcVar9;
  
  iVar6 = count + 2;
  if (_th + -4 < count + 2) {
    iVar6 = _th + -4;
  }
  uVar4 = _th - iVar6;
  uVar5 = _tw - 0x28;
  pWVar2 = newwin(iVar6,0x28,((int)uVar4 >> 1) + (uint)((int)uVar4 < 0 && (uVar4 & 1) != 0),
                   ((int)uVar5 >> 1) + (uint)((int)uVar5 < 0 && (uVar5 & 1) != 0));
  keypad(pWVar2,1);
  wtimeout(pWVar2,-1);
  ppRVar1 = PTR__g_theme_000396c0;
  if ((selected < 0) || (count <= selected)) {
    selected = 0;
  }
  iVar7 = 0;
  while( true ) {
    while( true ) {
      while( true ) {
        werase(pWVar2);
        wattr_on(pWVar2,(*ppRVar1)->pairs[5].attributes | 0x600,(void *)0x0);
        wbkgd(pWVar2,(*ppRVar1)->pairs[5].attributes | 0x600);
        wborder(pWVar2,0,0,0,0,0,0,0,0);
        wattr_on(pWVar2,(*ppRVar1)->pairs[0].attributes | 0x200100,(void *)0x0);
        uVar4 = strlen(title);
        uVar4 = 0x26 - uVar4;
        mvwprintw(pWVar2,0,((int)uVar4 >> 1) + (uint)((int)uVar4 < 0 && (uVar4 & 1) != 0)," %s ",
                   title);
        wattr_off(pWVar2,(*ppRVar1)->pairs[0].attributes | 0x200100,(void *)0x0);
        ppcVar9 = items + iVar7;
        iVar8 = 0;
        for (iVar3 = iVar7; (iVar8 < iVar6 + -2 && (iVar3 < count)); iVar3 = iVar3 + 1) {
          if (selected == iVar3) {
            wattr_on(pWVar2,(*ppRVar1)->pairs[0].attributes | 0x40100,(void *)0x0);
          }
          iVar8 = iVar8 + 1;
          mvwprintw(pWVar2,iVar8,2," %-34.34s ",*ppcVar9);
          if (selected == iVar3) {
            wattr_off(pWVar2,(*ppRVar1)->pairs[0].attributes | 0x40100,(void *)0x0);
          }
          ppcVar9 = ppcVar9 + 1;
        }
        wrefresh(pWVar2);
        iVar3 = wgetch(pWVar2);
        if ((iVar3 != 0x103) && (iVar3 != 0x6b)) break;
        if (0 < selected) {
          selected = selected + -1;
        }
        if (selected < iVar7) {
          iVar7 = selected;
        }
      }
      if ((iVar3 != 0x102) && (iVar3 != 0x6a)) break;
      if (selected < count + -1) {
        selected = selected + 1;
      }
      if (iVar6 + iVar7 + -2 <= selected) {
        iVar7 = (selected - iVar6) + 3;
      }
    }
    if (((iVar3 == 10) || (iVar3 == 0xd)) || (iVar3 == 0x157)) break;
    if ((iVar3 == 0x1b) || (iVar3 == 0x71)) {
      delwin(pWVar2);
      wtimeout(_w_input,0x32);
      return -1;
    }
  }
  delwin(pWVar2);
  wtimeout(_w_input,0x32);
  return selected;
}

/*
 * _pick_model @ 0x0001a620..0x0001a74c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in terminal_ui: pick model. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _pick_model(undefined4 param_1,char *param_2)
 * Direct callers: _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0001a620_pick_model.s
 */
int pick_model(undefined4 param_1,char *param_2)

{
  (void)param_1; /* Present in the recovered ABI; unused by this routine. */
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x140] __attribute__((aligned(16)));
#define local_128 (*((char * (*)[69])(app_frame + 0x8))) /* original SP - 0x128 */

  int iVar1;
  int iVar2;
  int iVar3;
  byte bVar4;

  iVar3 = PTR__g_model_cache_000396ac->count;
  if (iVar3 == 0) {
    msg(10,"system","No models cached. Use /refresh-ollama-models.");
    iVar3 = -1;
  }
  else {
    if (0 < iVar3) {
      iVar2 = 0;
      iVar1 = iVar3;
      do {
        local_128[iVar2] = PTR__g_model_cache_000396ac->models[iVar2].name;
        iVar2 = iVar2 + 1;
        iVar1 = iVar1 + -1;
      } while (iVar1 != 0);
      iVar1 = 0;
      bVar4 = (param_2 == (char *)0x0) << 1;
      do {
        if ((!(bool)(bVar4 >> 1 & 1)) && (iVar2 = strcmp(local_128[iVar1],param_2), iVar2 == 0))
        goto LAB_0001a6e8;
        iVar1 = iVar1 + 1;
      } while (iVar3 != iVar1);
    }
    iVar1 = 0;
LAB_0001a6e8:
    iVar3 = tui_menu_select("Select Model",local_128,iVar3,iVar1);
    if (iVar3 < 0) {
      msg(10,"system","Cancelled");
      iVar3 = -1;
    }
  }
  return iVar3;

#undef local_128
}

/*
 * _read_input @ 0x0001a74c..0x0001ae90
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Handle terminal text editing, command history, completion and submission.
 * Parameters/return (inferred): undefined4 _read_input(char *param_1,uint param_2,RecoveredHistory *param_3)
 * Direct callers: _read_paste@0x0001ae90, _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0001a74c_read_input.s
 */
undefined4 read_input(char *param_1,uint param_2,RecoveredHistory *param_3)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x1120] __attribute__((aligned(16)));
#define acStack_1108 (*((char (*)[4096])(app_frame + 0x8))) /* original SP - 0x1108 */
#define local_108 (*((undefined4 (*)[16])(app_frame + 0x1008))) /* original SP - 0x108 */
#define local_c8 (*((char * (*)[34])(app_frame + 0x1048))) /* original SP - 0xc8 */

  bool bVar1;
  RecoveredModelCache *pRVar2;
  uint length;
  int iVar3;
  char *pcVar4;
  int iVar5;
  undefined **ppuVar6;
  int iVar7;
  int iVar8;
  uint capacity;
  uint uVar9;

  memcpy(acStack_1108,&_C_679_7402,0x1000);
  if (*param_1 == '\0') {
    *param_1 = '\0';
    length = 0;
  }
  else {
    length = strlen(param_1);
  }
  nodelay(_w_input,0);
  capacity = param_2 - 1;
  bVar1 = false;
  uVar9 = length;
LAB_0001ae0c:
  while( true ) {
    while( true ) {
      while( true ) {
        do {
          if ((int)capacity <= (int)length) {
            param_1[length] = '\0';
            wtimeout(_w_input,0x32);
            return 0;
          }
          redraw_input(param_1);
          iVar3 = wgetch(_w_input);
        } while (iVar3 == -1);
        if (iVar3 != 0xd) break;
        nodelay(_w_input,1);
        iVar3 = wgetch(_w_input);
        nodelay(_w_input,0);
        if (iVar3 != 10) {
          if (iVar3 == -1) {
            param_1[length] = '\0';
            wtimeout(_w_input,0x32);
            return 0;
          }
          ungetch(iVar3);
        }
        param_1[length] = '\n';
        length = length + 1;
        param_1[length] = '\0';
        uVar9 = length;
      }
      if ((iVar3 != 10) && (iVar3 != 0x157)) break;
      nodelay(_w_input,1);
      iVar3 = wgetch(_w_input);
      nodelay(_w_input,0);
      if (iVar3 == -1) {
        param_1[length] = '\0';
        wtimeout(_w_input,0x32);
        return 0;
      }
      ungetch(iVar3);
      param_1[length] = '\n';
      length = length + 1;
      param_1[length] = '\0';
      uVar9 = length;
    }
    if ((((iVar3 != 4) && (iVar3 != 0x7f)) && (iVar3 != 0x107)) && (iVar3 != 8)) break;
    if (0 < (int)uVar9) {
      memmove(param_1 + uVar9 + -1,param_1 + uVar9,(length - uVar9) + 1);
      length = length - 1;
      uVar9 = uVar9 - 1;
    }
  }
  if (iVar3 == 0x1b) {
    *param_1 = '\0';
    wtimeout(_w_input,0x32);
    return 0xffffffff;
  }
  if (iVar3 == 0x103) {
    if (param_3 == (RecoveredHistory *)0x0) goto LAB_0001adec;
    pcVar4 = history_prev(param_3);
    if (pcVar4 != (char *)0x0) {
      if (!bVar1) {
        strncpy(acStack_1108,param_1,0xfff);
        bVar1 = true;
      }
      strncpy(param_1,pcVar4,capacity);
      length = strlen(param_1);
      uVar9 = length;
    }
    goto LAB_0001ae0c;
  }
  if (iVar3 == 0x102) {
    if (param_3 == (RecoveredHistory *)0x0) {
LAB_0001adec:
      if (iVar3 - 0x20U < 0x5f) {
        param_1[length] = (char)iVar3;
        length = length + 1;
        param_1[length] = '\0';
        uVar9 = length;
      }
    }
    else {
      pcVar4 = history_next(param_3);
      if (pcVar4 == (char *)0x0) {
        if (bVar1) {
          strncpy(param_1,acStack_1108,capacity);
          length = strlen(param_1);
          uVar9 = length;
        }
        else {
          *param_1 = '\0';
          length = 0;
          uVar9 = 0;
        }
      }
      else {
        strncpy(param_1,pcVar4,capacity);
        length = strlen(param_1);
        uVar9 = length;
      }
    }
    goto LAB_0001ae0c;
  }
  if (((iVar3 != 9) || (*param_1 != '/')) || ((int)length < 1)) goto LAB_0001adec;
  iVar3 = strncmp(param_1,"/model ",7);
  pRVar2 = PTR__g_model_cache_000396ac;
  if ((iVar3 != 0) || ((int)length < 8)) {
    ppuVar6 = (undefined **)(&_slash_cmds);
    if (_slash_cmds != NULL) {
      iVar3 = 0;
      pcVar4 = _slash_cmds;
      do {
        ppuVar6 = ppuVar6 + 1;
        iVar7 = strncmp(pcVar4,param_1,length);
        if (iVar7 == 0) {
          local_c8[iVar3] = pcVar4;
          iVar3 = iVar3 + 1;
        }
        pcVar4 = (char *)(*ppuVar6);
        if (pcVar4 == (char *)0x0) {
          if (iVar3 == 1) {
            strncpy(param_1,local_c8[0],capacity);
            length = strlen(param_1);
            uVar9 = length;
            goto LAB_0001ae0c;
          }
          break;
        }
      } while (iVar3 < 0x20);
      if (1 < iVar3) {
        wattr_on(_w_chat,(*PTR__g_theme_000396c0)->pairs[4].attributes | 0x500,(void *)0x0);
        wprintw(_w_chat,"commands:");
        iVar7 = 0;
        do {
          wprintw(_w_chat," %s",local_c8[iVar7]);
          iVar7 = iVar7 + 1;
        } while (iVar7 != iVar3);
        wattr_off(_w_chat,(*PTR__g_theme_000396c0)->pairs[4].attributes | 0x500,(void *)0x0);
        wprintw(_w_chat,"\n");
        chat_advance();
        chat_refresh();
        if (_tab_cycle_7399 < iVar3) {
          strncpy(param_1,local_c8[_tab_cycle_7399],capacity);
          length = strlen(param_1);
          uVar9 = length;
        }
        _tab_cycle_7399 = _tab_cycle_7399 + 1;
        if (iVar3 <= _tab_cycle_7399) {
          _tab_cycle_7399 = 0;
        }
      }
    }
    goto LAB_0001ae0c;
  }
  iVar3 = PTR__g_model_cache_000396ac->count;
  if (0 < iVar3) {
    iVar7 = 0;
    iVar8 = 0;
    do {
      iVar5 = strncmp(pRVar2->models[iVar8].name,param_1 + 7,length - 7);
      if (iVar5 == 0) {
        local_108[iVar7] = (undefined4)(pRVar2->models + iVar8);
        iVar7 = iVar7 + 1;
      }
      iVar8 = iVar8 + 1;
      if (iVar3 == iVar8) goto LAB_0001ab48;
    } while (iVar7 < 0x10);
    goto LAB_0001aaf8;
  }
  ppuVar6 = (undefined **)(&_default_models);
  if (_default_models == NULL) goto LAB_0001ae0c;
  iVar7 = 0;
  pcVar4 = _default_models;
  do {
    ppuVar6 = ppuVar6 + 1;
    iVar3 = strncmp(pcVar4,param_1 + 7,length - 7);
    if (iVar3 == 0) {
      local_108[iVar7] = (undefined4)(pcVar4);
      iVar7 = iVar7 + 1;
    }
    pcVar4 = (char *)(*ppuVar6);
    if (pcVar4 == (char *)0x0) goto LAB_0001ab48;
  } while (iVar7 < 0x10);
LAB_0001aaf8:
  if (1 < iVar7) {
    wattr_on(_w_chat,(*PTR__g_theme_000396c0)->pairs[4].attributes | 0x500,(void *)0x0);
    wprintw(_w_chat,"models:");
    iVar3 = 0;
    do {
      wprintw(_w_chat," %s",local_108[iVar3]);
      iVar3 = iVar3 + 1;
    } while (iVar3 != iVar7);
    wattr_off(_w_chat,(*PTR__g_theme_000396c0)->pairs[4].attributes | 0x500,(void *)0x0);
    wprintw(_w_chat,"\n");
    chat_advance();
    chat_refresh();
    if (_mcycle_7388 < iVar7) {
      snprintf(param_1,param_2,"/model %s",(char *)(local_108[_mcycle_7388]));
      length = strlen(param_1);
      uVar9 = length;
    }
    _mcycle_7388 = _mcycle_7388 + 1;
    if (iVar7 <= _mcycle_7388) {
      _mcycle_7388 = 0;
    }
  }
  goto LAB_0001ae0c;
LAB_0001ab48:
  if (iVar7 == 1) {
    snprintf(param_1,param_2,"/model %s",(char *)(local_108[0]));
    length = strlen(param_1);
    uVar9 = length;
    goto LAB_0001ae0c;
  }
  goto LAB_0001aaf8;

#undef acStack_1108
#undef local_108
#undef local_c8
}

/*
 * _read_paste @ 0x0001ae90..0x0001b004
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Collect pasted terminal input into the edit buffer.
 * Parameters/return (inferred): undefined4 _read_paste(char *param_1,int param_2)
 * Direct callers: _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0001ae90_read_paste.s
 */
undefined4 read_paste(char *param_1,int param_2)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x2040] __attribute__((aligned(16)));
#define acStack_2028 (*((char (*)[8204])(app_frame + 0x8))) /* original SP - 0x2028 */

  RecoveredTheme **ppRVar1;
  int iVar2;
  uint uVar3;
  int iVar4;

  ppRVar1 = PTR__g_theme_000396c0;
  wattr_on(_w_chat,(*PTR__g_theme_000396c0)->pairs[0x12].attributes | 0x1300,(void *)0x0);
  wprintw(_w_chat,"[Paste mode: type \'.\' on blank line to finish]\n");
  wattr_off(_w_chat,(*ppRVar1)->pairs[0x12].attributes | 0x1300,(void *)0x0);
  chat_advance();
  chat_refresh();
  *param_1 = '\0';
  iVar4 = 0;
  while( true ) {
    nodelay(_w_input,0);
    iVar2 = read_input(acStack_2028,0x2000,0);
    if (iVar2 < 0) {
      wtimeout(_w_input,0x32);
      *param_1 = '\0';
      return 0xffffffff;
    }
    iVar2 = strcmp(acStack_2028,".");
    if ((iVar2 == 0) || (uVar3 = strlen(acStack_2028), param_2 <= (int)(iVar4 + uVar3 + 1))) break;
    if (0 < iVar4) {
      strncat(param_1,"\n",(param_2 - iVar4) - 1);
      iVar4 = iVar4 + 1;
    }
    strncat(param_1,acStack_2028,(param_2 - iVar4) - 1);
    uVar3 = strlen(acStack_2028);
    iVar4 = iVar4 + uVar3;
  }
  wtimeout(_w_input,0x32);
  if (0 < iVar4) {
    return 0;
  }
  return 0xffffffff;

#undef acStack_2028
}

/*
 * _handle_cmd @ 0x0001b004..0x0001daac
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Dispatch the curses interface's slash commands and configuration interactions.
 * Parameters/return (inferred): int _handle_cmd(RecoveredAppState *app,char *command)
 * Direct callers: _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; performs network/protocol I/O; executes an argument vector through PATH; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; reads bytes from a stream; writes bytes to a stream; reads process environment; signals a process; creates directories; opens a directory for enumeration; executes a shell command and opens a pipe; reads a descriptor or IPC channel; reads directory entries; executes SQL text; opens/creates a SQLite database; prepares SQL text; reads filesystem metadata; removes a filesystem entry
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0001b004_handle_cmd.s
 */
int handle_cmd(RecoveredAppState *app,char *command)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x20010] __attribute__((aligned(16)));
#define iStack_1fff8 (*((int *)(app_frame + 0x8))) /* original SP - 0x1fff8 */
#define pcStack_1fff4 (*((char * *)(app_frame + 0xc))) /* original SP - 0x1fff4 */
#define pcStack_1fff0 (*((char * *)(app_frame + 0x10))) /* original SP - 0x1fff0 */
#define pcStack_1ffec (*((char * *)(app_frame + 0x14))) /* original SP - 0x1ffec */
#define pcStack_1ffe8 (*((char * *)(app_frame + 0x18))) /* original SP - 0x1ffe8 */
#define uStack_1ffe4 (*((undefined4 *)(app_frame + 0x1c))) /* original SP - 0x1ffe4 */
#define RStack_1ffe0 (*((RecoveredDString *)(app_frame + 0x20))) /* original SP - 0x1ffe0 */
#define pcStack_1ffd4 (*((char * *)(app_frame + 0x2c))) /* original SP - 0x1ffd4 */
#define pcStack_1ffd0 (*((char * *)(app_frame + 0x30))) /* original SP - 0x1ffd0 */
#define pcStack_1ffcc (*((char * *)(app_frame + 0x34))) /* original SP - 0x1ffcc */
#define auStack_1ffc8 (*((undefined1 (*)[4])(app_frame + 0x38))) /* original SP - 0x1ffc8 */
#define apcStack_1ffc4 (*((char * (*)[15])(app_frame + 0x3c))) /* original SP - 0x1ffc4 */
#define uStack_1ff85 (*((undefined1 *)(app_frame + 0x7b))) /* original SP - 0x1ff85 */
#define uStack_1ff45 (*((undefined1 *)(app_frame + 0xbb))) /* original SP - 0x1ff45 */
#define pcStack_1ff44 (*((char * *)(app_frame + 0xbc))) /* original SP - 0x1ff44 */
#define pcStack_1ff40 (*((char * *)(app_frame + 0xc0))) /* original SP - 0x1ff40 */
#define pcStack_1ff3c (*((char * *)(app_frame + 0xc4))) /* original SP - 0x1ff3c */
#define pcStack_1ff38 (*((char * *)(app_frame + 0xc8))) /* original SP - 0x1ff38 */
#define pcStack_1ff34 (*((char * *)(app_frame + 0xcc))) /* original SP - 0x1ff34 */
#define pcStack_1ff30 (*((char * *)(app_frame + 0xd0))) /* original SP - 0x1ff30 */
#define uStack_1ff2c (*((undefined4 *)(app_frame + 0xd4))) /* original SP - 0x1ff2c */
#define acStack_1fe44 (*((char (*)[1023])(app_frame + 0x1bc))) /* original SP - 0x1fe44 */
#define uStack_1fa45 (*((undefined1 *)(app_frame + 0x5bb))) /* original SP - 0x1fa45 */
#define RStack_1fa44 (*((RecoveredResponse *)(app_frame + 0x5bc))) /* original SP - 0x1fa44 */

  char cVar1;
  uint *puVar2;
  RecoveredModelCache *pRVar3;
  RecoveredTheme **ppRVar4;
  int iVar5;
  char *pcVar6;
  int iVar7;
  uint uVar8;
  char **ppcVar9;
  int iVar10;
  RecoveredTheme *theme;
  void *stream;
  RecoveredMessage *pRVar11;
  undefined4 uVar12;
  int iVar13;
  RecoveredResponse *key;
  char *pcVar14;
  char *pcVar15;
  char *pcVar16;
  char *pcVar17;
  RecoveredConfig *pRVar18;
  RecoveredDString *str;
  RecoveredMessage *pRVar19;
  RecoveredMessage *pRVar20;
  RecoveredDString *pRVar21;
  int *piVar22;

  iVar5 = strcmp(command,"/quit");
  if ((iVar5 == 0) || (iVar5 = strcmp(command,"/q"), iVar5 == 0)) {
    if (((app->config).auto_save_session != 0) && (0 < (app->conversation).message_count)) {
      session_get_path(acStack_1fe44,0x400,"auto");
      iVar5 = session_save(&app->conversation,acStack_1fe44,(app->config).working_directory);
      if (iVar5 == 0) {
        msg(0xf,"system","Session auto-saved. Use /continue next time.");
      }
    }
    app->running = 0;
    return 1;
  }
  iVar5 = strcmp(command,"/help");
  if ((iVar5 == 0) || (iVar5 = strcmp(command,"\\?"), iVar5 == 0)) {
    memcpy(apcStack_1ffc4,&_C_819_8020,0x80);
    iVar5 = 0;
    if (apcStack_1ffc4[0] != (char *)0x0) {
      iVar5 = 0;
      do {
        iVar5 = iVar5 + 1;
      } while (apcStack_1ffc4[iVar5] != (char *)0x0);
    }
    tui_menu_select(((char *)(recovered_section_TEXT___cstring + 21492)),apcStack_1ffc4,iVar5,0);
    return 1;
  }
  iVar5 = strcmp(command,"/setup");
  if (iVar5 == 0) {
    RStack_1ffe0.data = "OpenAI (gpt-4o-mini)";
    RStack_1ffe0.length = (uint32_t)(recovered_section_TEXT___cstring + 0x4f64); /* relocated menu string */
    RStack_1ffe0.capacity = (uint32_t)(recovered_section_TEXT___cstring + 0x4f78); /* relocated menu string */
    pcStack_1ffd4 = "Ollama Cloud API";
    pcStack_1ffd0 = "Ollama (self-hosted LAN)";
    pcStack_1ffcc = "Cancel";
    memset(auStack_1ffc8, 0, sizeof(auStack_1ffc8));
    iVar5 = 0;
    do {
      iVar13 = iVar5 + 1;
      puVar2 = &RStack_1ffe0.length + iVar5;
      iVar5 = iVar13;
    } while (*puVar2 != 0);
    iVar5 = tui_menu_select("Setup: Choose Provider",&RStack_1ffe0.data,iVar13,0);
    if (iVar5 < 0) {
      return 1;
    }
    if (iVar5 == 5) {
      return 1;
    }
    switch(iVar5) {
    case 0:
      (app->config).provider = 1;
      strncpy((app->config).model,"gpt-4o-mini",0x7f);
      break;
    case 1:
      (app->config).provider = 2;
      strncpy((app->config).model,"claude-3-5-sonnet-latest",0x7f);
      break;
    case 2:
      (app->config).provider = 3;
      strncpy((app->config).model,"gemini-1.5-flash",0x7f);
      break;
    case 3:
      (app->config).provider = 0;
      strncpy((app->config).model,"qwen3",0x7f);
      strncpy((app->config).base_url,"https://api.ollama.com",0x1ff);
      strncpy((app->config).ollama_url,"https://api.ollama.com",0x1ff);
      msgf(0xc,"setup","Ollama Cloud: https://api.ollama.com");
      break;
    case 4:
      (app->config).provider = 0;
      strncpy((app->config).model,"qwen3",0x7f);
    }
    pcVar6 = provider_name((app->config).provider);
    msgf(0xb,"setup","Provider: %s",pcVar6);
    if (iVar5 < 3) {
      msgf(0xc,"setup","Enter API key with /key YOUR_KEY (not echoed):");
      nodelay(_w_input,0);
      memset(apcStack_1ffc4,0,0x80);
      ppRVar4 = PTR__g_theme_000396c0;
      wattr_on(_w_status,(*PTR__g_theme_000396c0)->pairs[0xc].attributes | 0xd00,(void *)0x0);
      mvwprintw(_w_status,0,1,"API key (not shown): ");
      wrefresh(_w_status);
      wattr_off(_w_status,(*ppRVar4)->pairs[0xc].attributes | 0xd00,(void *)0x0);
      iVar5 = 0;
      do {
        wtimeout(_w_input,-1);
        iVar13 = wgetch(_w_input);
        if (((iVar13 == 10) || (iVar13 == 0xd)) || (iVar13 == 0x157)) break;
        if (iVar13 == 0x1b) {
          apcStack_1ffc4[0] = (char *)((uint)apcStack_1ffc4[0] & 0xffffff);
          break;
        }
        if (iVar13 - 0x20U < 0x5f) {
          *(char *)((int)apcStack_1ffc4 + iVar5) = (char)iVar13;
          iVar5 = iVar5 + 1;
        }
      } while (iVar5 < 0x7f);
      wtimeout(_w_input,0x32);
      if (APP_FIELD8(apcStack_1ffc4[0], 0) != '\0') {
        app_format_truncated((char *)&pcStack_1ff44,0x100,"/key %s",(char *)(apcStack_1ffc4));
        iVar5 = (app->config).provider;
        if (iVar5 == 1) {
          app_copy_padded((app->config).openai_key,(char *)apcStack_1ffc4,0xff);
        }
        else if (iVar5 == 2) {
          app_copy_padded((app->config).anthropic_key,(char *)apcStack_1ffc4,0xff);
        }
        else if (iVar5 == 3) {
          app_copy_padded((app->config).google_key,(char *)apcStack_1ffc4,0xff);
        }
        msgf(0xb,"setup","API key set.");
      }
    }
    else if (iVar5 == 3) {
      msgf(0xc,"setup","Ollama Cloud API key (required):");
      nodelay(_w_input,0);
      memset(apcStack_1ffc4,0,0x80);
      ppRVar4 = PTR__g_theme_000396c0;
      wattr_on(_w_status,(*PTR__g_theme_000396c0)->pairs[0xc].attributes | 0xd00,(void *)0x0);
      mvwprintw(_w_status,0,1,"Key (not shown): ");
      wrefresh(_w_status);
      wattr_off(_w_status,(*ppRVar4)->pairs[0xc].attributes | 0xd00,(void *)0x0);
      iVar5 = 0;
      do {
        wtimeout(_w_input,-1);
        iVar13 = wgetch(_w_input);
        if (((iVar13 == 10) || (iVar13 == 0xd)) || (iVar13 == 0x157)) break;
        if (iVar13 == 0x1b) {
          apcStack_1ffc4[0] = (char *)((uint)apcStack_1ffc4[0] & 0xffffff);
          break;
        }
        if (iVar13 - 0x20U < 0x5f) {
          *(char *)((int)apcStack_1ffc4 + iVar5) = (char)iVar13;
          iVar5 = iVar5 + 1;
        }
      } while (iVar5 < 0x7f);
      wtimeout(_w_input,0x32);
      if (APP_FIELD8(apcStack_1ffc4[0], 0) != '\0') {
        app_copy_padded((app->config).ollama_key,(char *)apcStack_1ffc4,0xff);
        msgf(0xb,"setup","Ollama API key set.");
      }
    }
    else if (iVar5 == 4) {
      msgf(0xc,"setup","Enter Ollama LAN URL (e.g. http://host:11434):");
      nodelay(_w_input,0);
      wtimeout(_w_input,-1);
      memset(&pcStack_1ff44,0,0x100);
      iVar5 = 0;
      do {
        iVar13 = wgetch(_w_input);
        if (((iVar13 == 10) || (iVar13 == 0xd)) || (iVar13 == 0x157)) break;
        if (iVar13 == 0x1b) {
          pcStack_1ff44 = (char *)((uint)pcStack_1ff44 & 0xffffff);
          break;
        }
        if (iVar13 - 0x20U < 0x5f) {
          *(char *)((int)&pcStack_1ff44 + iVar5) = (char)iVar13;
          iVar5 = iVar5 + 1;
        }
      } while (iVar5 < 0xfe);
      wtimeout(_w_input,0x32);
      if (APP_FIELD8(pcStack_1ff44, 0) == '\0') {
        msgf(10,"setup","No URL entered. Set with /url later.");
      }
      else {
        app_copy_padded((app->config).base_url,(char *)&pcStack_1ff44,0x1ff);
        app_copy_padded((app->config).ollama_url,(char *)&pcStack_1ff44,0x1ff);
        msgf(0xb,"setup","URL: %s",(char *)(&pcStack_1ff44));
      }
      msgf(0xc,"setup","Ollama LAN key if any (ENTER to skip):");
      nodelay(_w_input,0);
      memset(apcStack_1ffc4,0,0x80);
      ppRVar4 = PTR__g_theme_000396c0;
      wattr_on(_w_status,(*PTR__g_theme_000396c0)->pairs[0xc].attributes | 0xd00,(void *)0x0);
      mvwprintw(_w_status,0,1,"Key (not shown): ");
      wrefresh(_w_status);
      wattr_off(_w_status,(*ppRVar4)->pairs[0xc].attributes | 0xd00,(void *)0x0);
      iVar5 = 0;
      do {
        wtimeout(_w_input,-1);
        iVar13 = wgetch(_w_input);
        if (((iVar13 == 10) || (iVar13 == 0xd)) || (iVar13 == 0x157)) break;
        if (iVar13 == 0x1b) {
          apcStack_1ffc4[0] = (char *)((uint)apcStack_1ffc4[0] & 0xffffff);
          break;
        }
        if (iVar13 - 0x20U < 0x5f) {
          *(char *)((int)apcStack_1ffc4 + iVar5) = (char)iVar13;
          iVar5 = iVar5 + 1;
        }
      } while (iVar5 < 0x7f);
      wtimeout(_w_input,0x32);
      if (APP_FIELD8(apcStack_1ffc4[0], 0) != '\0') {
        app_copy_padded((app->config).ollama_key,(char *)apcStack_1ffc4,0xff);
        msgf(0xb,"setup","Ollama LAN key set.");
      }
    }
    config_save(&app->config);
    redraw_title(app);
    msgf(0xb,"setup","Setup complete. Model: %s. /save to keep config.",(app->config).model);
    return 1;
  }
  iVar5 = strcmp(command,"/model");
  if (iVar5 == 0) {
    pRVar18 = &app->config;
    pcVar6 = (app->config).model;
    iVar5 = pick_model((undefined4)(pRVar18),pcVar6);
    if (iVar5 < 0) {
      return 1;
    }
    strncpy(pcVar6,PTR__g_model_cache_000396ac->models[iVar5].name,0x7f);
    if (((app->config).active_profile[0] != '\0') &&
       (iVar5 = (app->config).profile_count, 0 < iVar5)) {
      iVar13 = 0;
      do {
        iVar7 = strcmp((const char *)(0x1824 + (int)((app->config).active_profile + iVar13 * 0x3c8 + -4)),
                        (app->config).active_profile);
        if (iVar7 == 0) {
          app_copy_padded((char *)(0x1868 + (int)((app->config).active_profile + iVar13 * 0x3c8 + -4)),pcVar6,
                   0x7f);
          break;
        }
        iVar13 = iVar13 + 1;
      } while (iVar5 != iVar13);
    }
    if ((app->config).provider == 0) {
      msg(0xc,"system","Fetching model metadata...");
      chat_advance();
      chat_refresh();
      doupdate();
      ollama_sync_model_metadata(pRVar18);
    }
    iVar5 = config_context_window(pRVar18);
    msgf(0xb,"system","Model: %s (Ctx: %d)",pcVar6,iVar5);
    config_save(pRVar18);
    redraw_title(app);
    return 1;
  }
  iVar5 = strncmp(command,"/model ",7);
  if (iVar5 == 0) {
    strncpy((char *)apcStack_1ffc4,command + 7,0x7f);
    uStack_1ff45 = 0;
    uVar8 = strlen((char *)apcStack_1ffc4);
    ppcVar9 = (char **)((int)apcStack_1ffc4 + (uVar8 - 1));
    if (apcStack_1ffc4 <= ppcVar9) {
      do {
        cVar1 = *(char *)ppcVar9;
        if (((cVar1 != ' ') && (cVar1 != '\t')) && ((cVar1 != '\r' && (cVar1 != '\n')))) break;
        *(char *)ppcVar9 = '\0';
        ppcVar9 = (char **)((int)ppcVar9 + -1);
      } while (apcStack_1ffc4 <= ppcVar9);
    }
    pcVar6 = (app->config).model;
    app_copy_padded(pcVar6,(char *)apcStack_1ffc4,0x7f);
    if (((app->config).active_profile[0] != '\0') &&
       (iVar5 = (app->config).profile_count, 0 < iVar5)) {
      iVar13 = 0;
      do {
        iVar7 = strcmp((const char *)(0x1824 + (int)((app->config).active_profile + iVar13 * 0x3c8 + -4)),
                        (app->config).active_profile);
        if (iVar7 == 0) {
          app_copy_padded((char *)(0x1868 + (int)((app->config).active_profile + iVar13 * 0x3c8 + -4)),pcVar6,
                   0x7f);
          break;
        }
        iVar13 = iVar13 + 1;
      } while (iVar5 != iVar13);
    }
    if ((app->config).provider == 0) {
      ollama_sync_model_metadata(&app->config);
    }
    iVar5 = config_context_window(&app->config);
    msgf(0xf,"system","Model: %s (Ctx: %d)",pcVar6,iVar5);
    config_save(&app->config);
    redraw_title(app);
    return 1;
  }
  iVar5 = strcmp(command,"/provider");
  if (((iVar5 == 0) || (iVar5 = strcmp(command,"/providers"), iVar5 == 0)) ||
     (iVar5 = strcmp(command,"/profile"), iVar5 == 0)) {
LAB_0001bbec:
    pcStack_1fff4 = "Manage / Switch Profiles";
    pcStack_1fff0 = "Create New Profile";
    pcStack_1ffec = "Quick Start (No Profile)";
    pcStack_1ffe8 = "Back";
    uStack_1ffe4 = 0;
    iStack_1fff8 = tui_menu_select("Provider Manager",&pcStack_1fff4,4,0);
    if ((iStack_1fff8 != 3) && (-1 < iStack_1fff8)) {
      if (iStack_1fff8 == 0) {
        pRVar18 = &app->config;
        profile_list(pRVar18,apcStack_1ffc4,8,&iStack_1fff8);
        if (iStack_1fff8 == 0) {
          msg(10,"system","No profiles yet. Choose \'Create New Profile\' first.");
        }
        else {
          apcStack_1ffc4[iStack_1fff8] = (char *)0x0;
          iVar5 = tui_menu_select("Select Profile to Manage",apcStack_1ffc4,iStack_1fff8,0);
          if (-1 < iVar5) {
            pcVar6 = apcStack_1ffc4[iVar5];
            pcStack_1ff44 = "Activate";
            pcStack_1ff40 = "Edit Model";
            pcStack_1ff3c = "Edit API Key";
            pcStack_1ff38 = "Edit URL";
            pcStack_1ff34 = "Delete";
            pcStack_1ff30 = "Back";
            uStack_1ff2c = 0;
            iVar5 = tui_menu_select(pcVar6,&pcStack_1ff44,6,0);
            if (iVar5 == 0) {
              profile_select(pRVar18,pcVar6);
              if ((app->config).provider == 0) {
                msg(0xc,"system","Fetching model metadata...");
                chat_advance();
                chat_refresh();
                doupdate();
                ollama_sync_model_metadata(pRVar18);
              }
              iVar5 = config_context_window(pRVar18);
              msgf(0xb,"system","Profile \'%s\' activated (Ctx: %d).",pcVar6,iVar5);
              config_save(pRVar18);
              redraw_title(app);
            }
            else if (iVar5 - 1U < 3) {
              memset(&RStack_1fa44,0,0x100);
              RStack_1ffe0.data = "";
              RStack_1ffe0.length = (uint32_t)(recovered_section_TEXT___cstring + 0x4e84); /* relocated menu string */
              RStack_1ffe0.capacity = (uint32_t)(recovered_section_TEXT___cstring + 0x4e9c); /* relocated menu string */
              pcStack_1ffd4 = "Enter New Base URL:";
              msg(0xc,"setup",(&RStack_1ffe0.data)[iVar5]);
              chat_advance();
              chat_refresh();
              doupdate();
              iVar13 = read_input((char *)(&RStack_1fa44),0x100,0);
              if (((iVar13 == 0) && (APP_FIELD8(RStack_1fa44.content.data, 0) != '\0')) &&
                 (iVar13 = (app->config).profile_count, 0 < iVar13)) {
                iVar7 = 0;
                do {
                  iVar10 = strcmp((const char *)(0x1824 +
                                   (int)((app->config).active_profile + iVar7 * 0x3c8 + -4)),pcVar6);
                  if (iVar10 == 0) {
                    pcVar14 = (char *)&app->config + offsetof(RecoveredConfig,active_profile) + iVar7 * 0x3c8;
                    if (pcVar14 == (char *)0xffffe7e0) {
                      return 1;
                    }
                    if (iVar5 == 1) {
                      app_copy_padded(pcVar14 + 0x1864,(char *)&RStack_1fa44,0x7f);
                    }
                    else if (iVar5 == 2) {
                      app_copy_padded(pcVar14 + 0x18e4,(char *)&RStack_1fa44,0xff);
                    }
                    else if (iVar5 == 3) {
                      app_copy_padded(pcVar14 + 0x19e4,(char *)&RStack_1fa44,0x1ff);
                    }
                    msgf(0xb,"system","Profile \'%s\' updated.",pcVar6);
                    iVar5 = strcmp((app->config).active_profile,pcVar6);
                    if ((iVar5 == 0) &&
                       (profile_select(pRVar18,pcVar6), (app->config).provider == 0)) {
                      ollama_sync_model_metadata(pRVar18);
                    }
                    config_save(pRVar18);
                    redraw_title(app);
                    return 1;
                  }
                  iVar7 = iVar7 + 1;
                } while (iVar7 != iVar13);
              }
            }
            else if (iVar5 == 4) {
              profile_remove(pRVar18,pcVar6);
              msgf(0xb,"system","Profile \'%s\' deleted.",pcVar6);
              config_save(pRVar18);
            }
          }
        }
      }
      else if (iStack_1fff8 == 1) {
        RStack_1ffe0.data = "openai";
        RStack_1ffe0.length = (uint32_t)(recovered_section_TEXT___cstring + 0x6f4); /* relocated menu string */
        RStack_1ffe0.capacity = (uint32_t)(recovered_section_TEXT___cstring + 0x700); /* relocated menu string */
        pcStack_1ffd4 = "ollama cloud";
        pcStack_1ffd0 = "ollama local";
        pcStack_1ffcc = (char *)0x0;
        iVar5 = tui_menu_select("Base Provider Type",&RStack_1ffe0.data,5,0);
        if (-1 < iVar5) {
          flushinp();
          if (iVar5 == 0) {
            iVar13 = 1;
          }
          else {
            iVar13 = 2;
            if ((iVar5 != 1) && (iVar13 = 3, iVar5 != 2)) {
              iVar13 = 0;
            }
          }
          memset(apcStack_1ffc4,0,0x40);
          memset(&RStack_1fa44,0,0x100);
          memset(acStack_1fe44,0,0x100);
          memset(&pcStack_1ff44,0,0x80);
          while( true ) {
            msg(0xc,"setup","New Profile Name:");
            chat_advance();
            chat_refresh();
            doupdate();
            iVar7 = read_input((char *)(apcStack_1ffc4),0x40,0);
            if (iVar7 < 0) break;
            if (APP_FIELD8(apcStack_1ffc4[0], 0) != '\0') {
              msg(0xc,"setup","Model Name (optional):");
              chat_advance();
              chat_refresh();
              doupdate();
              read_input((char *)(&pcStack_1ff44),0x80,0);
              if (iVar5 == 3) {
                strncpy(acStack_1fe44,"https://api.ollama.com",0xff);
                msg(0xc,"setup","Ollama Cloud API Key:");
              }
              else {
                msg(0xc,"setup","API Key (optional):");
              }
              chat_advance();
              chat_refresh();
              doupdate();
              read_input((char *)(&RStack_1fa44),0x100,0);
              if (iVar5 == 4) {
                msg(0xc,"setup","Local Ollama URL (e.g. http://192.168.4.120:11434):");
                chat_advance();
                chat_refresh();
                doupdate();
                read_input(acStack_1fe44,0x100,0);
              }
              else if (iVar5 < 3) {
                msg(0xc,"setup","Base URL (optional):");
                chat_advance();
                chat_refresh();
                doupdate();
                read_input(acStack_1fe44,0x100,0);
              }
              pRVar18 = &app->config;
              ppcVar9 = &pcStack_1ff44;
              if (APP_FIELD8(pcStack_1ff44, 0) == '\0') {
                ppcVar9 = (char **)0x0;
              }
              key = &RStack_1fa44;
              if (APP_FIELD8(RStack_1fa44.content.data, 0) == '\0') {
                key = (RecoveredResponse *)0x0;
              }
              pcVar6 = acStack_1fe44;
              if (acStack_1fe44[0] == '\0') {
                pcVar6 = (char *)0x0;
              }
              profile_add(pRVar18,(char *)apcStack_1ffc4,iVar13,(char *)ppcVar9,(char *)key,pcVar6)
              ;
              profile_select(pRVar18,(char *)apcStack_1ffc4);
              msgf(0xb,"system","Profile \'%s\' created and activated.",(char *)(apcStack_1ffc4));
              if (iVar13 == 0) {
                msg(0xc,"models","Connecting and refreshing model list...");
                chat_advance();
                chat_refresh();
                doupdate();
                ollama_refresh_models(pRVar18);
              }
              config_save(pRVar18);
              return 1;
            }
            msg(10,"setup","Name is required.");
          }
        }
      }
      else if (iStack_1fff8 == 2) {
        RStack_1ffe0.data = "ollama";
        RStack_1ffe0.length = (uint32_t)(recovered_section_TEXT___cstring + 0x6ec); /* relocated menu string */
        RStack_1ffe0.capacity = (uint32_t)(recovered_section_TEXT___cstring + 0x6f4); /* relocated menu string */
        pcStack_1ffd4 = "google";
        pcStack_1ffd0 = "Back";
        pcStack_1ffcc = (char *)0x0;
        uVar8 = tui_menu_select("Select Direct Provider",&RStack_1ffe0.data,5,
                                 (app->config).provider);
        if (uVar8 < 4) {
          (app->config).provider = uVar8;
          (app->config).active_profile[0] = '\0';
          msgf(0xb,"system","Switched to direct %s mode.",(&RStack_1ffe0.data)[uVar8]);
          config_save(&app->config);
          redraw_title(app);
        }
      }
    }
    return 1;
  }
  iVar5 = strcmp(command,"/profiles");
  if (iVar5 == 0) goto LAB_0001bbec;
  iVar5 = strncmp(command,"/provider ",0xb);
  if (iVar5 == 0) {
    strncpy((char *)apcStack_1ffc4,command + 0xb,0x3f);
    uStack_1ff85 = 0;
    uVar8 = strlen((char *)apcStack_1ffc4);
    ppcVar9 = (char **)((int)apcStack_1ffc4 + (uVar8 - 1));
    if (apcStack_1ffc4 <= ppcVar9) {
      do {
        cVar1 = *(char *)ppcVar9;
        if (((cVar1 != ' ') && (cVar1 != '\t')) && ((cVar1 != '\r' && (cVar1 != '\n')))) break;
        *(char *)ppcVar9 = '\0';
        ppcVar9 = (char **)((int)ppcVar9 + -1);
      } while (apcStack_1ffc4 <= ppcVar9);
    }
    pRVar18 = &app->config;
    iVar5 = profile_select(pRVar18,(char *)apcStack_1ffc4);
    if (iVar5 == 0) {
      pcVar6 = profile_get_name(pRVar18);
      msgf(0xb,"system","Profile: %s",pcVar6);
      config_save(pRVar18);
      redraw_title(app);
      return 1;
    }
    iVar5 = provider_from_name((char *)apcStack_1ffc4);
    if (-1 < iVar5) {
      (app->config).provider = iVar5;
      pcVar6 = provider_name(iVar5);
      msgf(0xf,"system","Provider: %s",pcVar6);
      config_save(pRVar18);
      redraw_title(app);
      return 1;
    }
    msgf(9,"error","Unknown provider or profile: %s",(char *)(apcStack_1ffc4));
    return 1;
  }
  iVar5 = strncmp(command,"/theme ",7);
  if (iVar5 == 0) {
    strncpy((char *)apcStack_1ffc4,command + 7,0x3f);
    uStack_1ff85 = 0;
    uVar8 = strlen((char *)apcStack_1ffc4);
    ppcVar9 = (char **)((int)apcStack_1ffc4 + (uVar8 - 1));
    if (apcStack_1ffc4 <= ppcVar9) {
      do {
        cVar1 = *(char *)ppcVar9;
        if (((cVar1 != ' ') && (cVar1 != '\t')) && ((cVar1 != '\r' && (cVar1 != '\n')))) break;
        *(char *)ppcVar9 = '\0';
        ppcVar9 = (char **)((int)ppcVar9 + -1);
      } while (apcStack_1ffc4 <= ppcVar9);
    }
    theme = theme_find((char *)apcStack_1ffc4);
    if (theme == (RecoveredTheme *)0x0) {
      theme_names_csv((undefined1 *)(acStack_1fe44),0x100);
      msgf(9,"error","Unknown theme \'%s\'. Available:%s",(char *)(apcStack_1ffc4),acStack_1fe44);
      return 1;
    }
    theme_apply(theme);
    strncpy((app->config).theme,theme->name,0x1f);
    config_save(&app->config);
    msgf(0xf,"system","Theme: %s - %s",theme->name,theme->description);
    if (_g_app == 0) {
      return 1;
    }
    handle_resize();
    return 1;
  }
  iVar5 = strncmp(command,"/reference ",0xb);
  pcVar6 = command + 0xb;
  if (iVar5 == 0) {
    for (; (cVar1 = *pcVar6, cVar1 == ' ' || (cVar1 == '\t')); pcVar6 = pcVar6 + 1) {
    }
    if (cVar1 == '\0') {
      msg(10,"system","Usage: /reference <your query>");
      return 1;
    }
    msgf(0xc,"rag","Searching RAG DB for: %s",pcVar6);
    pcVar14 = (char *)0x0;
    if ((app->config).ollama_url[0] != '\0') {
      pcVar14 = (app->config).ollama_url;
    }
    iVar5 = rag_search(pcVar6,(char *)&RStack_1fa44,0x10000,pcVar14);
    if (iVar5 == 0) {
      msg(3,"rag",(char *)&RStack_1fa44);
      msg(0xb,"rag","Search complete.");
      return 1;
    }
    msgf(9,"rag","Search failed: %s",(char *)(&RStack_1fa44));
    return 1;
  }
  iVar5 = strcmp(command,"/refresh-ollama-models");
  if (iVar5 == 0) {
    msg(0xc,"models","Refreshing model list from Ollama Cloud...");
    model_cache_sync(&app->config);
    pRVar3 = PTR__g_model_cache_000396ac;
    PTR__g_model_cache_000396ac->count = 0;
    pRVar3->updated_at = 0;
    ollama_refresh_models(&app->config);
    msgf(0xc,"models","Done \u2014 %d model(s) cached.",pRVar3->count);
    return 1;
  }
  iVar5 = strcmp(command,"/list-models");
  if (iVar5 == 0) {
    if (PTR__g_model_cache_000396ac->count == 0) {
      msg(10,"models","No models cached. Use /refresh-ollama-models to fetch.");
      return 1;
    }
    msgf(0xc,"models","%d cached model(s) from Ollama Cloud:",PTR__g_model_cache_000396ac->count);
    pRVar3 = PTR__g_model_cache_000396ac;
    if (PTR__g_model_cache_000396ac->count < 1) {
      return 1;
    }
    iVar5 = 0;
    piVar22 = &PTR__g_model_cache_000396ac->models[0].context_window;
    do {
      if (piVar22[1] == 0) {
        pcVar6 = " [no tools]";
      }
      else {
        pcVar6 = "";
      }
      msgf(3,"model","%s -- ctx: %d%s",(char *)(pRVar3->models + iVar5),*piVar22,pcVar6);
      iVar5 = iVar5 + 1;
      piVar22 = piVar22 + 0x12;
    } while (iVar5 < pRVar3->count);
    return 1;
  }
  iVar5 = strcmp(command,"/keys");
  if (iVar5 == 0) {
    if ((app->config).openai_key[0] == '\0') {
      pcVar6 = "(not set)";
    }
    else {
      pcVar6 = "***";
    }
    if ((app->config).anthropic_key[0] == '\0') {
      pcVar14 = "(not set)";
    }
    else {
      pcVar14 = "***";
    }
    if ((app->config).google_key[0] == '\0') {
      pcVar15 = "(not set)";
    }
    else {
      pcVar15 = "***";
    }
    if ((app->config).ollama_key[0] == '\0') {
      pcVar16 = "(not set)";
    }
    else {
      pcVar16 = "***";
    }
    if ((app->config).api_key[0] == '\0') {
      pcVar17 = "(not set)";
    }
    else {
      pcVar17 = "***";
    }
    msgf(3,"keys","openai=%s anthropic=%s google=%s ollama=%s fallback=%s",pcVar6,pcVar14,pcVar15,
          pcVar16,pcVar17);
    return 1;
  }
  iVar5 = strncmp(command,"/key ",5);
  if (iVar5 == 0) {
    iVar5 = sscanf(command + 5,"%31s %255s",(char *)(&pcStack_1ff44),acStack_1fe44);
    if (iVar5 != 2) {
      return 1;
    }
    iVar5 = provider_from_name((char *)&pcStack_1ff44);
    if (iVar5 == 1) {
      app_copy_padded((app->config).openai_key,acStack_1fe44,0xff);
      msg(0xf,"system","OpenAI key set");
    }
    else if (iVar5 == 2) {
      app_copy_padded((app->config).anthropic_key,acStack_1fe44,0xff);
      msg(0xf,"system","Anthropic key set");
    }
    else if (iVar5 == 3) {
      app_copy_padded((app->config).google_key,acStack_1fe44,0xff);
      msg(0xf,"system","Google key set");
    }
    else if (iVar5 == 0) {
      app_copy_padded((app->config).ollama_key,acStack_1fe44,0xff);
      msg(0xf,"system","Ollama key set");
    }
    else {
      msg(9,"error","Unknown provider");
    }
    config_save(&app->config);
    return 1;
  }
  iVar5 = strncmp(command,"/url ",5);
  if (iVar5 == 0) {
    iVar5 = sscanf(command + 5,"%31s %511s",(char *)(&pcStack_1ff44),acStack_1fe44);
    if (iVar5 != 2) {
      return 1;
    }
    iVar5 = provider_from_name((char *)&pcStack_1ff44);
    if (iVar5 == 0) {
      app_copy_padded((app->config).ollama_url,acStack_1fe44,0x1ff);
      msgf(0xf,"system","Ollama URL: %s",acStack_1fe44);
    }
    else {
      app_copy_padded((app->config).base_url,acStack_1fe44,0x1ff);
      msgf(0xf,"system","Base URL: %s",acStack_1fe44);
    }
    config_save(&app->config);
    return 1;
  }
  iVar5 = strncmp(command,"/read full ",0xb);
  if (iVar5 == 0) {
    pcVar6 = command + 0xb;
    cVar1 = command[0xb];
    while (cVar1 == ' ') {
      pcVar6 = pcVar6 + 1;
      cVar1 = *pcVar6;
    }
    if ((cVar1 == '/') || (cVar1 == '~')) {
      strncpy(acStack_1fe44,pcVar6,0x3ff);
      uStack_1fa45 = 0;
    }
    else {
      app_format_truncated(acStack_1fe44,0x400,"%s/%s",(app->config).working_directory,pcVar6);
    }
    ds_init(&RStack_1ffe0);
    stream = fopen(acStack_1fe44,"r");
    if (stream != (void *)0x0) {
      while (uVar8 = fread(&RStack_1fa44,1,0x1000,stream), uVar8 != 0) {
        ds_appendn(&RStack_1ffe0,(char *)&RStack_1fa44,uVar8);
      }
      fclose(stream);
    }
    if (RStack_1ffe0.length == 0) {
      msgf(9,"error","Could not read %s",acStack_1fe44);
    }
    else {
      iVar5 = (app->conversation).message_count;
      pRVar11 = (app->conversation).messages + iVar5;
      (app->conversation).message_count = iVar5 + 1;
      memset(pRVar11,0,0x5c);
      (app->conversation).messages[iVar5].role = 1;
      pRVar21 = &pRVar11->content;
      ds_init(pRVar21);
      pcVar6 = ds_cstr(&RStack_1ffe0);
      ds_appendf(pRVar21,"Full contents of %s:\n%s",acStack_1fe44,pcVar6);
      pcVar6 = ds_cstr(&RStack_1ffe0);
      iVar5 = 0;
      if (RStack_1ffe0.length != 0) {
        iVar13 = 0;
        iVar5 = 0;
        uVar8 = RStack_1ffe0.length;
        do {
          if (pcVar6[iVar13] == '\n') {
            iVar5 = iVar5 + 1;
          }
          iVar13 = iVar13 + 1;
          uVar8 = uVar8 - 1;
        } while (uVar8 != 0);
      }
      msgf(0xf,"system","Loaded %zu bytes (%d lines) from %s",(size_t)(RStack_1ffe0.length),iVar5,
            acStack_1fe44);
      file_ctx_update(app,acStack_1fe44,RStack_1ffe0.length,iVar5,iVar5,0);
    }
    ds_free(&RStack_1ffe0);
    return 1;
  }
  iVar5 = strcmp(command,"/small-model");
  if (iVar5 == 0) {
    (app->config).small_model_mode = 1;
    msg(0xf,"system","Small model mode enabled.");
    return 1;
  }
  iVar5 = strcmp(command,"/normal-model");
  if (iVar5 == 0) {
    (app->config).small_model_mode = 0;
    msg(0xf,"system","Normal model mode enabled.");
    return 1;
  }
  iVar5 = strcmp(command,"/clear");
  if (iVar5 == 0) {
    if (0 < (app->conversation).message_count) {
      iVar5 = 0;
      do {
        message_free((app->conversation).messages + iVar5);
        iVar5 = iVar5 + 1;
      } while (iVar5 < (app->conversation).message_count);
    }
    (app->conversation).message_count = 0;
    (app->conversation).estimated_tokens = 0;
    trust_reset(app);
    cache_clear(app);
    app->plan_present = 0;
    plan_init(&app->plan);
    if (_g_worker.state != 0) {
      worker_kill(&_g_worker);
    }
    werase(_w_chat);
    _chat_y = 0;
    _chat_top = 0;
    _chat_user_scrolled = 0;
    msg(0xf,"system","Conversation cleared");
    return 1;
  }
  iVar5 = strcmp(command,"/config");
  if (iVar5 == 0) {
    pcVar15 = (app->config).model;
    int model_limit = model_context(pcVar15);
    pcVar6 = provider_name((app->config).provider);
    pcVar14 = config_get_url(&app->config);
    if (*PTR__g_theme_000396c0 == (RecoveredTheme *)0x0) {
      pcVar16 = "default";
    }
    else {
      pcVar16 = (*PTR__g_theme_000396c0)->name;
    }
    msgf(3,"config","Provider: %s  Model: %s  Dir: %s  URL: %s  Theme: %s  Ctx: %d",pcVar6,pcVar15,
          (app->config).working_directory,pcVar14,pcVar16,model_limit);
    return 1;
  }
  iVar5 = strcmp(command,"/save");
  if (iVar5 == 0) {
    iVar5 = config_save(&app->config);
    if (iVar5 == 0) {
      pcVar6 = "Config saved";
    }
    else {
      pcVar6 = "Could not save config";
    }
    msg(0xf,"system",pcVar6);
    return 1;
  }
  iVar5 = strncmp(command,"/save-session",0xd);
  if (iVar5 == 0) {
    pcVar6 = command + 0xe;
    if (command[0xd] != ' ') {
      pcVar6 = "session";
    }
    session_get_path(acStack_1fe44,0x400,pcVar6);
    iVar5 = session_save(&app->conversation,acStack_1fe44,(app->config).working_directory);
    if (iVar5 == 0) {
      pcVar6 = "Session saved";
    }
    else {
      pcVar6 = "Could not save session";
    }
    msg(0xf,"system",pcVar6);
    return 1;
  }
  iVar5 = strncmp(command,"/load-session",0xd);
  if ((iVar5 == 0) || (iVar5 = strcmp(command,"/continue"), iVar5 == 0)) {
    if ((*command == '/') && (command[1] == 'l')) {
      pcVar6 = command + 0xe;
      if (command[0xd] != ' ') {
        pcVar6 = "session";
      }
    }
    else {
      pcVar6 = "auto";
    }
    session_get_path(acStack_1fe44,0x400,pcVar6);
    iVar5 = session_load(&app->conversation,acStack_1fe44,(app->config).working_directory);
    if (iVar5 == -2) {
      msg(0xf,"system","Session was from a different directory. Starting fresh.");
      unlink(acStack_1fe44);
      return 1;
    }
    if (iVar5 == 0) {
      pcVar6 = "Session loaded";
    }
    else {
      pcVar6 = "Could not load session";
    }
    msg(0xf,"system",pcVar6);
    return 1;
  }
  iVar5 = strcmp(command,"/compact");
  if (iVar5 == 0) {
    iVar5 = (app->conversation).message_count;
    if (iVar5 < 2) {
      msg(0xf,"system","Nothing to compact.");
      return 1;
    }
    pRVar11 = malloc(iVar5 * 0x5c);
    if (pRVar11 == (RecoveredMessage *)0x0) {
      msg(9,"error","Out of memory");
      return 1;
    }
    iVar13 = 0;
    iVar7 = 0;
    pRVar19 = (app->conversation).messages;
    do {
      pRVar20 = (RecoveredMessage *)
                (((RecoveredConversation *)pRVar19)->messages[0].call_id + iVar7 + -0x1c);
      message_copy((RecoveredMessage *)(pRVar11->call_id + iVar7 + -0x1c),pRVar20);
      message_free(pRVar20);
      iVar13 = iVar13 + 1;
      iVar7 = iVar7 + 0x5c;
    } while (iVar5 != iVar13);
    (app->conversation).message_count = 1;
    memset(pRVar19,0,0x5c);
    (app->conversation).messages[0].role = 0;
    pRVar21 = &(app->conversation).messages[0].content;
    ds_init(pRVar21);
    ds_append(pRVar21,"Summarize conversation into a concise summary.");
    iVar13 = (app->conversation).message_count;
    (app->conversation).message_count = iVar13 + 1;
    memset(((RecoveredConversation *)pRVar19)->messages + iVar13,0,0x5c);
    ((RecoveredConversation *)pRVar19)->messages[iVar13].role = 1;
    str = &((RecoveredConversation *)pRVar19)->messages[iVar13].content;
    ds_init(str);
    iVar13 = 0;
    pRVar20 = pRVar11;
    do {
      iVar7 = pRVar20->role;
      pcVar6 = ds_cstr(&pRVar20->content);
      ds_appendf(str,"[%d] %s\n",iVar7,pcVar6);
      iVar13 = iVar13 + 1;
      pRVar20 = pRVar20 + 1;
    } while (iVar5 != iVar13);
    memset(&RStack_1fa44,0,0x1fa0c);
    iVar13 = provider_send(&app->config,&app->conversation,&RStack_1fa44,
                            (RecoveredStreamCallback)0x0,(void *)0x0,(RecoveredRoute *)0x0,
                            (int *)0x0);
    if ((iVar13 - 200U < 100) && (RStack_1fa44.content.length != 0)) {
      (app->conversation).message_count = 1;
      memset(pRVar19,0,0x5c);
      ((RecoveredConversation *)pRVar19)->messages[0].role = 0;
      ds_init(pRVar21);
      pcVar6 = ds_cstr(&RStack_1fa44.content);
      ds_appendf(pRVar21,"Summary: %s",pcVar6);
      msg(0xf,"system","Compacted to summary.");
    }
    else {
      (app->conversation).message_count = 0;
      iVar7 = 0;
      pRVar20 = pRVar11;
      do {
        message_copy(pRVar19,pRVar20);
        iVar7 = iVar7 + 1;
        pRVar19 = pRVar19 + 1;
        pRVar20 = pRVar20 + 1;
      } while (iVar5 != iVar7);
      (app->conversation).message_count = iVar5;
      msgf(9,"error","Compact failed (HTTP %d).",iVar13);
    }
    iVar13 = 0;
    pRVar19 = pRVar11;
    do {
      message_free(pRVar19);
      iVar13 = iVar13 + 1;
      pRVar19 = pRVar19 + 1;
    } while (iVar5 != iVar13);
    free(pRVar11);
    return 1;
  }
  iVar5 = strcmp(command,"/approve");
  if (iVar5 == 0) {
    (app->config).auto_approve = (uint)((app->config).auto_approve == 0);
    config_save(&app->config);
    if ((app->config).auto_approve == 0) {
      pcVar6 = "OFF";
    }
    else {
      pcVar6 = "ON";
    }
    msgf(0xf,"system","Auto-approve: %s",pcVar6);
    return 1;
  }
  iVar5 = strcmp(command,"/verbose");
  if (iVar5 == 0) {
    (app->config).verbose = (uint)((app->config).verbose == 0);
    config_save(&app->config);
    if ((app->config).verbose == 0) {
      pcVar6 = "OFF";
    }
    else {
      pcVar6 = "ON";
    }
    msgf(0xf,"system","Verbose logging: %s",pcVar6);
    return 1;
  }
  iVar5 = strncmp(command,"/effort ",8);
  if (iVar5 == 0) {
    pcVar6 = command + 8;
    cVar1 = command[8];
    while (cVar1 == ' ') {
      pcVar6 = pcVar6 + 1;
      cVar1 = *pcVar6;
    }
    iVar5 = strcmp(pcVar6,"low");
    if (iVar5 == 0) {
      iVar5 = 0;
      pcVar14 = "Quick one-shot, no validation";
    }
    else {
      iVar5 = strcmp(pcVar6,"default");
      if (iVar5 == 0) {
        iVar5 = 1;
        pcVar14 = "Standard generation";
      }
      else {
        iVar5 = strcmp(pcVar6,"medium");
        if (iVar5 == 0) {
          iVar5 = 2;
          pcVar14 = "Generate + validate (small model review)";
        }
        else {
          iVar5 = strcmp(pcVar6,"high");
          if (iVar5 == 0) {
            iVar5 = 3;
            pcVar14 = "Generate -> validate -> auto-fix (up to 3 iterations)";
          }
          else {
            iVar5 = strcmp(pcVar6,"max");
            if (iVar5 != 0) {
              msgf(9,"error","Unknown effort: %s. Use low|default|medium|high|max",pcVar6);
              return 1;
            }
            iVar5 = 4;
            pcVar14 = "Generate -> validate -> fix (up to 5 iterations)";
          }
        }
      }
    }
    (app->config).effort = iVar5;
    config_save(&app->config);
    msgf(0xf,"system","Effort: %s -- %s",pcVar6,pcVar14);
    return 1;
  }
  iVar5 = strcmp(command,"/effort");
  if (iVar5 != 0) {
    iVar5 = strcmp(command,"/git");
    if (iVar5 == 0) {
      pcStack_1fff4 = "git";
      pcStack_1fff0 = "status";
      pcStack_1ffec = "--short";
      pcStack_1ffe8 = (char *)0x0;
      ds_init(&RStack_1ffe0);
      iVar5 = safe_exec_read(&pcStack_1fff4,(app->config).working_directory,&RStack_1ffe0);
      if ((iVar5 < 0) || (RStack_1ffe0.length == 0)) {
        msg(0x12,"git","(no changes)");
      }
      else {
        pcVar6 = ds_cstr(&RStack_1ffe0);
        msg(0xb,"git",pcVar6);
      }
      ds_free(&RStack_1ffe0);
      return 1;
    }
    iVar5 = strcmp(command,"/context");
    if (iVar5 == 0) {
      context_build(&app->project_context,(app->config).working_directory);
      app->context_loaded = 1;
      msgf(0xf,"system","Project context loaded (%lu bytes).",(long unsigned int)((app->project_context).length));
      return 1;
    }
    iVar5 = strcmp(command,"/init");
    if (iVar5 == 0) {
      iVar5 = init_project(app);
      if (iVar5 == 0) {
        uVar12 = (undefined4)(init_project_report());
        msgf(0xb,"init","%s",(char *)(uVar12));
        return 1;
      }
      msgf(9,"init","Failed to initialize project.");
      return 1;
    }
    iVar5 = strcmp(command,"/undo");
    if (iVar5 == 0) {
      iVar5 = (app->conversation).message_count;
      if (1 < iVar5) {
        message_free((app->conversation).messages + iVar5 + -1);
        message_free((RecoveredMessage *)
                      ((int)app + (app->conversation).message_count * 0x5c + -0xb0));
        (app->conversation).message_count = (app->conversation).message_count + -2;
        msg(0xf,"system","Undone.");
        return 1;
      }
      msg(0xf,"system","Nothing to undo.");
      return 1;
    }
    iVar5 = strncmp(command,"/sidebar depth ",0xf);
    if (iVar5 == 0) {
      iVar5 = atoi(command + 0xf);
      if (iVar5 < 1) {
        iVar5 = 1;
      }
      else if (3 < iVar5) {
        iVar5 = 3;
      }
      (app->config).sidebar_depth = iVar5;
      config_save(&app->config);
      msgf(0xf,"system","Sidebar depth: %d",iVar5);
      return 1;
    }
    iVar5 = strcmp(command,"/search-engine");
    if (iVar5 == 0) {
      RStack_1ffe0.data = "Ollama Cloud/Local API";
      RStack_1ffe0.length = (uint32_t)(recovered_section_TEXT___cstring + 0x4e40); /* relocated menu string */
      RStack_1ffe0.capacity = (uint32_t)(recovered_section_TEXT___cstring + 0x4e5c); /* relocated menu string */
      pcStack_1ffd4 = (char *)0x0;
      iVar5 = tui_menu_select("Search Engine",&RStack_1ffe0.data,3,
                               (uint)((app->config).search_url[0] == '\0'));
      if (iVar5 < 0) {
        return 1;
      }
      if (iVar5 == 2) {
        return 1;
      }
      if (iVar5 == 0) {
        msg(0xf,"system","Enter Ollama Search URL (blank for default cloud):");
        chat_advance();
        chat_refresh();
        memset(&pcStack_1ff44,0,0x100);
        iVar5 = read_input((char *)(&pcStack_1ff44),0x100,0);
        if (iVar5 == 0) {
          if (APP_FIELD8(pcStack_1ff44, 0) == '\0') {
            strncpy((app->config).search_url,"https://api.ollama.com",0x1ff);
          }
          else {
            app_copy_padded((app->config).search_url,(char *)&pcStack_1ff44,0x1ff);
          }
          msgf(0xb,"system","Ollama search enabled: %s",(app->config).search_url);
        }
      }
      else {
        (app->config).search_url[0] = '\0';
        msg(0xb,"system","Native stripped search enabled (fallback mode).");
      }
      config_save(&app->config);
      return 1;
    }
    msgf(9,"error","Unknown command: %s",command);
    return 1;
  }
  iVar5 = (app->config).effort;
  if (iVar5 == 2) {
    pcVar6 = "medium";
    pcVar14 = "Generate + validate (small model review)";
    goto LAB_0001d63c;
  }
  if (iVar5 < 3) {
    if (iVar5 == 0) {
      pcVar6 = "low";
      pcVar14 = "Quick one-shot, no validation";
      goto LAB_0001d63c;
    }
  }
  else {
    if (iVar5 == 3) {
      pcVar6 = "high";
      pcVar14 = "Generate -> validate -> auto-fix (up to 3 iterations)";
      goto LAB_0001d63c;
    }
    if (iVar5 == 4) {
      pcVar6 = "max";
      pcVar14 = "Generate -> validate -> fix (up to 5 iterations)";
      goto LAB_0001d63c;
    }
  }
  pcVar6 = "default";
  pcVar14 = "Standard generation";
LAB_0001d63c:
  msgf(0xf,"system","Effort: %s -- %s. Use /effort low|default|medium|high|max",pcVar6,pcVar14);
  return 1;

#undef iStack_1fff8
#undef pcStack_1fff4
#undef pcStack_1fff0
#undef pcStack_1ffec
#undef pcStack_1ffe8
#undef uStack_1ffe4
#undef RStack_1ffe0
#undef pcStack_1ffd4
#undef pcStack_1ffd0
#undef pcStack_1ffcc
#undef auStack_1ffc8
#undef apcStack_1ffc4
#undef uStack_1ff85
#undef uStack_1ff45
#undef pcStack_1ff44
#undef pcStack_1ff40
#undef pcStack_1ff3c
#undef pcStack_1ff38
#undef pcStack_1ff34
#undef pcStack_1ff30
#undef uStack_1ff2c
#undef acStack_1fe44
#undef uStack_1fa45
#undef RStack_1fa44
}

/*
 * _tui_init @ 0x0001daac..0x0001de90
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Initialize curses windows, colors, terminal state and UI dimensions.
 * Parameters/return (inferred): void _tui_init(void)
 * Direct callers: _main@0x00005498
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; writes bytes to a stream; reads process environment; changes process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0001daac_tui_init.s
 */
void tui_init(void)

{
  int iVar1;
  char *pcVar2;
  char *pcVar3;
  BOOL BVar4;
  int iVar5;
  int iVar6;
  
  setlocale(2,"");
  iVar1 = platform_is_tiger();
  if ((iVar1 != 0) || (*(int *)PTR__g_platform_000396b8 == 1)) {
    pcVar2 = getenv("TERM");
    if ((pcVar2 == (char *)0x0) || (*pcVar2 == '\0')) {
      fwrite("Warning: TERM not set; defaulting to xterm-color\n",1,0x31,(FILE * restrict)(PTR_000396a4 + 0xb0));
      setenv("TERM","xterm-color",1);
    }
    else {
      pcVar3 = strstr(pcVar2,"256color");
      if (((((pcVar3 != (char *)0x0) ||
            (pcVar3 = strstr(pcVar2,"alacritty"), pcVar3 != (char *)0x0)) ||
           (pcVar3 = strstr(pcVar2,"kitty"), pcVar3 != (char *)0x0)) ||
          ((pcVar3 = strstr(pcVar2,"tmux"), pcVar3 != (char *)0x0 ||
           (pcVar3 = strstr(pcVar2,"screen"), pcVar3 != (char *)0x0)))) ||
         (pcVar3 = strstr(pcVar2,"st-"), pcVar3 != (char *)0x0)) {
        fprintf((FILE * restrict)(PTR_000396a4 + 0xb0),
                          "Warning: TERM=%s not supported by Tiger ncurses 5.4; using xterm-color\n"
                          ,pcVar2);
        setenv("TERM","xterm-color",1);
      }
    }
  }
  pcVar2 = getenv("LINES");
  iVar1 = 0;
  if (pcVar2 != (char *)0x0) {
    iVar1 = atoi(pcVar2);
  }
  pcVar2 = getenv("COLUMNS");
  iVar6 = 0;
  if (pcVar2 != (char *)0x0) {
    iVar6 = atoi(pcVar2);
  }
  initscr();
  cbreak();
  noecho();
  keypad(*(WINDOW **)PTR_000396bc,1);
  curs_set(0);
  BVar4 = has_colors();
  if (BVar4 != 0) {
    start_color();
    use_default_colors();
  }
  theme_apply(&_theme_dark);
  hl_init();
  iVar5 = *(int *)PTR_000396bc;
  _th = -1;
  if (iVar5 != 0) {
    _th = *(short *)(iVar5 + 4) + 1;
  }
  _tw = -1;
  if (iVar5 != 0) {
    _tw = *(short *)(iVar5 + 6) + 1;
  }
  if ((_th < 1) || (_tw < 1)) {
    if ((iVar1 < 1) || (_th = iVar1, _tw = iVar6, iVar6 < 1)) {
      _th = 0x18;
      _tw = 0x50;
    }
    resize_term(_th,_tw);
  }
  if ((4 < _th) && (0x13 < _tw)) {
    _w_title = newwin(1,_tw,0,0);
    _w_sep = newwin(1,_tw,1,0);
    _w_chat = newpad(0x1000,_tw);
    scrollok(_w_chat,1);
    _w_sidebar = newpad(0x1000,0x1a);
    _w_status = newwin(1,_tw,_th + -2,0);
    _w_input = newwin(1,_tw,_th + -1,0);
    wtimeout(_w_input,0x32);
    keypad(_w_input,1);
    redraw_sep();
    redraw_input("");
    wclear(*(WINDOW **)PTR_000396bc);
    doupdate();
    return;
  }
  endwin();
  fprintf((FILE * restrict)(PTR_000396a4 + 0xb0),
                    "Terminal too small or not detected (%dx%d).\nTry: export TERM=xterm-color; export LINES=24; export COLUMNS=80\n"
                    ,_th,_tw);
                    /* WARNING: Subroutine does not return */
  exit(1);
}

/*
 * _tui_cleanup @ 0x0001de90..0x0001df44
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Release curses/UI state and restore terminal operation.
 * Parameters/return (inferred): void _tui_cleanup(void)
 * Direct callers: _main@0x00005498
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0001de90_tui_cleanup.s
 */
void tui_cleanup(void)

{
  if (_w_title != (WINDOW *)0x0) {
    delwin(_w_title);
  }
  if (_w_sep != (WINDOW *)0x0) {
    delwin(_w_sep);
  }
  if (_w_status != (WINDOW *)0x0) {
    delwin(_w_status);
  }
  if (_w_input != (WINDOW *)0x0) {
    delwin(_w_input);
  }
  if (_w_chat != (WINDOW *)0x0) {
    delwin(_w_chat);
  }
  if (_w_sidebar != (WINDOW *)0x0) {
    delwin(_w_sidebar);
  }
  ds_free((RecoveredDString *)&_sidebar_content);
  endwin();
  return;
}

/*
 * _tui_run @ 0x0001df44..0x000202dc
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Drive the curses event loop, input commands, worker messages, approvals, streaming and screen updates.
 * Parameters/return (inferred): void _tui_run(RecoveredAppState *app)
 * Direct callers: _main@0x00005498
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; performs network/protocol I/O; replaces a process with an executable; resolves/executes a program through PATH; executes an argument vector through PATH; reads text from a stream; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; writes text to a stream; reads bytes from a stream; writes bytes to a stream; reads the process working directory; reads process environment; signals a process; creates directories; securely creates a temporary file; opens a directory for enumeration; executes a shell command and opens a pipe; reads a descriptor or IPC channel; reads directory entries; resolves a filesystem path; renames/replaces filesystem entries; changes process environment; executes SQL text; opens/creates a SQLite database; prepares SQL text; reads filesystem metadata; removes a filesystem entry; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0001df44_tui_run.s
 */
/* WARNING: Restarted to delay deadcode elimination for space: ram */

void tui_run(RecoveredAppState *app)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x40380] __attribute__((aligned(16)));
#define iStack_40368 (*((int *)(app_frame + 0x8))) /* original SP - 0x40368 */
#define iStack_40364 (*((int *)(app_frame + 0xc))) /* original SP - 0x40364 */
#define acStack_40360 (*((char (*)[256])(app_frame + 0x10))) /* original SP - 0x40360 */
#define aRStack_40260 (*((RecoveredTigerStat (*)[2])(app_frame + 0x110))) /* original SP - 0x40260 */
#define uStack_40161 (*((undefined1 *)(app_frame + 0x20f))) /* original SP - 0x40161 */
#define acStack_40060 (*((char (*)[262164])(app_frame + 0x310))) /* original SP - 0x40060 */

  char cVar1;
  undefined1 *puVar2;
  bool bVar3;
  RecoveredTheme **ppRVar4;
  WINDOW *window;
  uint uVar5;
  uint uVar6;
  undefined **ppuVar7;
  char *pcVar8;
  int iVar9;
  int iVar10;
  pid_t pVar11;
  uint uVar12;
  time_t tVar13;
  void *directory;
  RecoveredTigerDirent *pRVar14;
  char *pcVar15;
  uint length;

  _g_app = app;
  signal(0x1c,(void (*)(int))(sigwinch_h));
  redraw_title(app);
  redraw_sep();
  cprintf(_w_chat,0,"PowerPC Code v%s","0.8.0");
  wprintw(_w_chat,"\n");
  window = _w_chat;
  pcVar8 = provider_name((app->config).provider);
  cprintf(window,0x12,"Provider: %s  Model: %s  /help for commands",pcVar8,(app->config).model);
  wprintw(_w_chat,"\n\n");
  pcVar8 = getenv("HOME");
  if (pcVar8 == (char *)0x0) {
    pcVar8 = ".";
  }
  else {
    pcVar8 = getenv("HOME");
  }
  snprintf(acStack_40060,0x400,"%s/.ppccoderc",pcVar8);
  iVar9 = stat(acStack_40060,(struct stat *)(aRStack_40260));
  ppRVar4 = PTR__g_theme_000396c0;
  if (iVar9 != 0) {
    wattr_on(_w_chat,(*PTR__g_theme_000396c0)->pairs[0xc].attributes | 0xd00,(void *)0x0);
    wprintw(_w_chat,"=== FIRST RUN: Quick Setup ===\n");
    wprintw(_w_chat,"No config found. Set up a provider:\n");
    wattr_off(_w_chat,(*ppRVar4)->pairs[0xc].attributes | 0xd00,(void *)0x0);
    wattr_on(_w_chat,(*ppRVar4)->pairs[0xb].attributes | 0xc00,(void *)0x0);
    wprintw(_w_chat,((char *)(recovered_section_TEXT___cstring + 24784)));
    wprintw(_w_chat,"  /provider openai      (needs /key YOUR_API_KEY)\n");
    wprintw(_w_chat,"  /provider anthropic   (Claude models)\n");
    wprintw(_w_chat,"  /provider google      (Gemini models)\n");
    wattr_off(_w_chat,(*ppRVar4)->pairs[0xb].attributes | 0xc00,(void *)0x0);
    wattr_on(_w_chat,(*ppRVar4)->pairs[4].attributes | 0x500,(void *)0x0);
    wprintw(_w_chat,"Quick setup:\n");
    wprintw(_w_chat,"  /setup  (interactive wizard)\n");
    wprintw(_w_chat,"Or manual:\n");
    wprintw(_w_chat,"  /provider openai\n");
    wprintw(_w_chat,"  /key sk-your-key-here\n");
    wprintw(_w_chat,"  /save\n");
    wprintw(_w_chat,"  /list-models  (Ctrl+R to refresh model cache)\n");
    wprintw(_w_chat,"Type /help for all commands.\n");
    wattr_off(_w_chat,(*ppRVar4)->pairs[4].attributes | 0x500,(void *)0x0);
    wprintw(_w_chat,"\n");
  }
  chat_advance();
  chat_refresh();
  worker_init(&_g_worker);
  queue_init((RecoveredQueue *)&_g_queue);
  memset(&_draft_buf,0,0x40000);
  _draft_cursor = 0;
  _draft_len = 0;
  plan_init(&app->plan);
  app->plan_present = 0;
  ds_init((RecoveredDString *)&_token_batch);
  _tui_in_think = 0;
  session_get_path(acStack_40060,0x400,"auto");
  iVar9 = stat(acStack_40060,(struct stat *)(aRStack_40260));
  ppRVar4 = PTR__g_theme_000396c0;
  if ((iVar9 == 0) &&
     ((0 < APP_FIELD32(aRStack_40260[0].size, 0) ||
      ((APP_FIELD32(aRStack_40260[0].size, 0) == 0 && (APP_FIELD32(aRStack_40260[0].size, 4) != 0)))))) {
    wattr_on(_w_chat,(*PTR__g_theme_000396c0)->pairs[10].attributes | 0xb00,(void *)0x0);
    wprintw(_w_chat,"[Previous session found. Resume? (y/N)]\n");
    wattr_off(_w_chat,(*ppRVar4)->pairs[10].attributes | 0xb00,(void *)0x0);
    chat_advance();
    chat_refresh();
    nodelay(_w_input,0);
    iVar9 = wgetch(_w_input);
    wtimeout(_w_input,0x32);
    if ((iVar9 == 0x79) || (iVar9 == 0x59)) {
      iVar9 = session_load(&app->conversation,acStack_40060,(app->config).working_directory);
      ppRVar4 = PTR__g_theme_000396c0;
      if (iVar9 == 0) {
        wattr_on(_w_chat,(*PTR__g_theme_000396c0)->pairs[0xb].attributes | 0xc00,(void *)0x0);
        wprintw(_w_chat,"[Session restored. %d messages loaded.]\n",
                 (app->conversation).message_count);
        wattr_off(_w_chat,(*ppRVar4)->pairs[0xb].attributes | 0xc00,(void *)0x0);
      }
      else if (iVar9 == -2) {
        unlink(acStack_40060);
        ppRVar4 = PTR__g_theme_000396c0;
        wattr_on(_w_chat,(*PTR__g_theme_000396c0)->pairs[0x12].attributes | 0x1300,(void *)0x0);
        wprintw(_w_chat,"[Session was from a different directory. Starting fresh.]\n");
        wattr_off(_w_chat,(*ppRVar4)->pairs[0x12].attributes | 0x1300,(void *)0x0);
      }
      else {
        wattr_on(_w_chat,(*PTR__g_theme_000396c0)->pairs[9].attributes | 0xa00,(void *)0x0);
        wprintw(_w_chat,"[Could not load session.]\n");
        wattr_off(_w_chat,(*ppRVar4)->pairs[9].attributes | 0xa00,(void *)0x0);
      }
      chat_advance();
      chat_refresh();
    }
    else {
      unlink(acStack_40060);
      ppRVar4 = PTR__g_theme_000396c0;
      wattr_on(_w_chat,(*PTR__g_theme_000396c0)->pairs[0x12].attributes | 0x1300,(void *)0x0);
      wprintw(_w_chat,"[Previous session discarded. Starting fresh.]\n");
      wattr_off(_w_chat,(*ppRVar4)->pairs[0x12].attributes | 0x1300,(void *)0x0);
      chat_advance();
      chat_refresh();
    }
  }
LAB_00020260:
  do {
    if (app->running == 0) {
      worker_cleanup(&_g_worker);
      return;
    }
    iVar9 = ollama_refresh_poll();
    if (iVar9 == 2) {
      msgf(0xb,"system","Models refreshed (%d loaded)",PTR__g_model_cache_000396ac->count);
    }
    else if (iVar9 == -1) {
      msgf(10,"system","Model refresh failed.");
    }
    if (_resize_pending != 0) {
      _resize_pending = 0;
      handle_resize();
    }
    redraw_title(app);
    redraw_sep();
    redraw_input(&_draft_buf);
    redraw_status(app);
    if (app->sidebar_visible != 0) {
      redraw_sidebar(app);
    }
    if (_streaming != 0) {
      uVar6 = app->spinner + 1;
      app->spinner = uVar6 + (((int)uVar6 >> 2) + (uint)((int)uVar6 < 0 && (uVar6 & 3) != 0)) * -4;
    }
    ppRVar4 = PTR__g_theme_000396c0;
    if (_g_worker.state - 1U < 2) {
      bVar3 = false;
      iVar9 = 0;
      do {
        iVar10 = worker_poll(&_g_worker,acStack_40360,&_pkt_body_9543,0x40400);
        if (iVar10 < -1) {
          if ((0 < app->worker_start_ms) &&
             (iVar10 = now_ms(), 3600000 < iVar10 - app->worker_start_ms)) {
            if (DAT_002cc02c != 0) {
              if (_pager_paused == 0) {
                tui_print_token(ds_cstr((RecoveredDString *)&_token_batch));
                _pager_lines = _pager_lines + 1;
              }
              ds_clear((RecoveredDString *)&_token_batch);
              chat_advance();
              if (_chat_user_scrolled == 0) {
                chat_refresh();
              }
              doupdate();
            }
            msgf(9,"error","Turn timed out after %d minutes - killing worker",0x3c);
            _streaming = 0;
            app->worker_active = 0;
            app->opaque_87f378 = 1;
            if (app->worker_input_path[0] != '\0') {
              unlink(app->worker_input_path);
              app->worker_input_path[0] = '\0';
            }
            worker_cleanup(&_g_worker);
            app->worker_start_ms = 0;
            break;
          }
        }
        else {
          if (iVar10 == -1) {
            if (DAT_002cc02c != 0) {
              if (_pager_paused == 0) {
                tui_print_token(ds_cstr((RecoveredDString *)&_token_batch));
                _pager_lines = _pager_lines + 1;
              }
              ds_clear((RecoveredDString *)&_token_batch);
              chat_advance();
              if (_chat_user_scrolled == 0) {
                chat_refresh();
              }
              doupdate();
            }
            msg(9,"error","Worker process died unexpectedly");
            _streaming = 0;
            app->worker_active = 0;
            if (app->worker_input_path[0] != '\0') {
              unlink(app->worker_input_path);
              app->worker_input_path[0] = '\0';
            }
            worker_cleanup(&_g_worker);
            iVar9 = queue_empty((RecoveredQueue *)&_g_queue);
            if (iVar9 == 0) {
              pcVar8 = queue_pop((RecoveredQueue *)&_g_queue);
              start_turn(app,pcVar8);
            }
            break;
          }
          if (iVar10 < 1) break;
          iVar10 = strcmp(acStack_40360,"TS");
          if (iVar10 == 0) {
            if (DAT_002cc02c != 0) {
              if (_pager_paused == 0) {
                tui_print_token(ds_cstr((RecoveredDString *)&_token_batch));
                _pager_lines = _pager_lines + 1;
              }
              ds_clear((RecoveredDString *)&_token_batch);
              chat_advance();
              if (_chat_user_scrolled == 0) {
                chat_refresh();
              }
              doupdate();
            }
            wattr_on(_w_chat,(*ppRVar4)->pairs[0x12].attributes | 0x1300,(void *)0x0);
            wprintw(_w_chat,"[thinking]\n");
            wattr_off(_w_chat,(*ppRVar4)->pairs[0x12].attributes | 0x1300,(void *)0x0);
            chat_advance();
            chat_refresh();
            doupdate();
          }
          else {
            iVar10 = strcmp(acStack_40360,"TH");
            if (iVar10 == 0) {
              uVar6 = strlen(&_pkt_body_9543);
              if (0xffff < uVar6) {
                uVar6 = 0xffff;
              }
              memcpy(acStack_40060,&_pkt_body_9543,uVar6);
              acStack_40060[uVar6] = '\0';
              if (uVar6 != 0) {
                iVar10 = 0;
                do {
                  if (acStack_40060[iVar10] == '\x01') {
                    acStack_40060[iVar10] = '\n';
                  }
                  iVar10 = iVar10 + 1;
                  uVar6 = uVar6 - 1;
                } while (uVar6 != 0);
              }
              wattr_on(_w_chat,(*ppRVar4)->pairs[0x12].attributes | 0x1300,(void *)0x0);
              wprintw(_w_chat,"%s",acStack_40060);
              wattr_off(_w_chat,(*ppRVar4)->pairs[0x12].attributes | 0x1300,(void *)0x0);
              chat_advance();
              chat_refresh();
              doupdate();
            }
            else {
              iVar10 = strcmp(acStack_40360,"TE");
              if (iVar10 == 0) {
                if (DAT_002cc02c != 0) {
                  if (_pager_paused == 0) {
                    tui_print_token(ds_cstr((RecoveredDString *)&_token_batch));
                    _pager_lines = _pager_lines + 1;
                  }
                  ds_clear((RecoveredDString *)&_token_batch);
                  chat_advance();
                  if (_chat_user_scrolled == 0) {
                    chat_refresh();
                  }
                  doupdate();
                }
                wprintw(_w_chat,"\n[/thinking]\n");
                chat_advance();
                chat_refresh();
                doupdate();
              }
              else {
                iVar10 = strcmp(acStack_40360,"T");
                if (iVar10 == 0) {
                  ds_append((RecoveredDString *)&_token_batch,&_pkt_body_9543);
                  _spinner_idx = _spinner_idx + 1;
                  if (3 < DAT_002cc02c) {
                    if (_pager_paused == 0) {
                      tui_print_token(ds_cstr((RecoveredDString *)&_token_batch));
                      _pager_lines = _pager_lines + 1;
                    }
                    ds_clear((RecoveredDString *)&_token_batch);
                    chat_advance();
                    if (_chat_user_scrolled == 0) {
                      chat_refresh();
                    }
                    doupdate();
                  }
                }
                else {
                  iVar10 = strcmp(acStack_40360,"O");
                  if (iVar10 == 0) {
                    if (DAT_002cc02c != 0) {
                      if (_pager_paused == 0) {
                        tui_print_token(ds_cstr((RecoveredDString *)&_token_batch));
                        _pager_lines = _pager_lines + 1;
                      }
                      ds_clear((RecoveredDString *)&_token_batch);
                      chat_advance();
                      if (_chat_user_scrolled == 0) {
                        chat_refresh();
                      }
                      doupdate();
                    }
                    pcVar8 = strchr(&_pkt_body_9543,0x3a);
                    if (pcVar8 != (char *)0x0) {
                      *pcVar8 = '\0';
                      msgf(0xc,&_pkt_body_9543,"%s",pcVar8 + 1);
                      *pcVar8 = ':';
                    }
                  }
                  else {
                    iVar10 = strcmp(acStack_40360,"TR");
                    if (iVar10 == 0) {
                      if (DAT_002cc02c != 0) {
                        if (_pager_paused == 0) {
                          tui_print_token(ds_cstr((RecoveredDString *)&_token_batch));
                          _pager_lines = _pager_lines + 1;
                        }
                        ds_clear((RecoveredDString *)&_token_batch);
                        chat_advance();
                        if (_chat_user_scrolled == 0) {
                          chat_refresh();
                        }
                        doupdate();
                      }
                      aRStack_40260[0].device = 0;
                      aRStack_40260[0].inode = 0;
                      aRStack_40260[0].mode = 0;
                      aRStack_40260[0].link_count = 0;
                      aRStack_40260[0].uid = 0;
                      aRStack_40260[0].gid = 0;
                      aRStack_40260[0].rdev = 0;
                      aRStack_40260[0].atime_seconds = 0;
                      aRStack_40260[0].atime_nanoseconds = 0;
                      memcpy(acStack_40060,&_C_1160_9704,0x200);
                      pcVar8 = strchr(&_pkt_body_9543,0x3a);
                      if (pcVar8 != (char *)0x0) {
                        pcVar15 = pcVar8 + -0x4ba00;
                        if ((char *)0x1f < pcVar15) {
                          pcVar15 = (char *)0x1f;
                        }
                        memcpy(aRStack_40260,&_pkt_body_9543,(uint)pcVar15);
                        uVar6 = strlen(pcVar8 + 1);
                        if (0x1ff < uVar6) {
                          uVar6 = 0x1ff;
                        }
                        memcpy(acStack_40060,pcVar8 + 1,uVar6);
                      }
                      if (APP_FIELD8(aRStack_40260[0].device, 0) != '\0') {
                        if (acStack_40060[0] == '\0') {
                          msgf(0xc,"running","Running %s...",(char *)(aRStack_40260));
                        }
                        else {
                          msgf(0xc,"running","Running %s(%s)...",(char *)(aRStack_40260),acStack_40060);
                        }
                      }
                    }
                    else {
                      iVar10 = strcmp(acStack_40360,"AR");
                      if (iVar10 == 0) {
                        if (DAT_002cc02c != 0) {
                          if (_pager_paused == 0) {
                            tui_print_token(ds_cstr((RecoveredDString *)&_token_batch));
                            _pager_lines = _pager_lines + 1;
                          }
                          ds_clear((RecoveredDString *)&_token_batch);
                          chat_advance();
                          if (_chat_user_scrolled == 0) {
                            chat_refresh();
                          }
                          doupdate();
                        }
                        iStack_40364 = 0;
                        iVar10 = sscanf(&_pkt_body_9543,"%d:%511[^\n]",&iStack_40364,
                                                  (char *)(aRStack_40260));
                        if (iVar10 == 2) {
                          _g_worker.unknown_1c = iStack_40364;
                          app_copy_padded(_g_worker.opaque_20,(char *)aRStack_40260,0x1ff);
                          msgf(0x10,"approve","Run [%s]? (y/n/always): ",(char *)(aRStack_40260));
                        }
                      }
                      else {
                        iVar10 = strcmp(acStack_40360,"D");
                        if (iVar10 == 0) {
                          if (DAT_002cc02c != 0) {
                            if (_pager_paused == 0) {
                              tui_print_token(ds_cstr((RecoveredDString *)&_token_batch));
                              _pager_lines = _pager_lines + 1;
                            }
                            ds_clear((RecoveredDString *)&_token_batch);
                            chat_advance();
                            if (_chat_user_scrolled == 0) {
                              chat_refresh();
                            }
                            doupdate();
                          }
                          iVar9 = strcmp(&_pkt_body_9543,"ok");
                          _streaming = 0;
                          app->worker_active = 0;
                          app->plan_present = 0;
                          _pager_paused = 0;
                          _pager_lines = 0;
                          if (iVar9 == 0) {
                            wattr_on(_w_chat,(*ppRVar4)->pairs[0xb].attributes | 0xc00,(void *)0x0)
                            ;
                            wprintw(_w_chat,"[done]\n");
                            wattr_off(_w_chat,(*ppRVar4)->pairs[0xb].attributes | 0xc00,(void *)0x0
                                      );
                          }
                          else if (!bVar3) {
                            msg(9,"error","Worker reported failure");
                          }
                          pVar11 = getpid();
                          snprintf((char *)aRStack_40260,0x100,
                                             "/tmp/ppccode_worker_%d_out.json",pVar11);
                          worker_deserialize_out(app,(char *)aRStack_40260);
                          unlink((char *)aRStack_40260);
                          worker_cleanup(&_g_worker);
                          chat_advance();
                          chat_refresh();
                          iVar9 = queue_empty((RecoveredQueue *)&_g_queue);
                          if (iVar9 == 0) {
                            pcVar8 = queue_pop((RecoveredQueue *)&_g_queue);
                            start_turn(app,pcVar8);
                          }
                          break;
                        }
                        iVar10 = strcmp(acStack_40360,"E");
                        if (iVar10 == 0) {
                          if (DAT_002cc02c != 0) {
                            if (_pager_paused == 0) {
                              tui_print_token(ds_cstr((RecoveredDString *)&_token_batch));
                              _pager_lines = _pager_lines + 1;
                            }
                            ds_clear((RecoveredDString *)&_token_batch);
                            chat_advance();
                            if (_chat_user_scrolled == 0) {
                              chat_refresh();
                            }
                            doupdate();
                          }
                          pcVar8 = strstr(&_pkt_body_9543,"busy");
                          if (((((pcVar8 != (char *)0x0) ||
                                (pcVar8 = strstr(&_pkt_body_9543,"overloaded"),
                                pcVar8 != (char *)0x0)) ||
                               (pcVar8 = strstr(&_pkt_body_9543,"429"), pcVar8 != (char *)0x0)) ||
                              (pcVar8 = strstr(&_pkt_body_9543,"throttled"), pcVar8 != (char *)0x0)
                              ) && (app->retry_count < 3)) {
                            app->retry_count = app->retry_count + 1;
                            app->retry_delay = 5000;
                            app->retry_remaining = 5000;
                            msgf(10,"info","Model busy (attempt %d/3), retrying in 5s...",app->retry_count);
                            _streaming = 0;
                            app->worker_active = 0;
                            if (app->worker_input_path[0] != '\0') {
                              unlink(app->worker_input_path);
                              app->worker_input_path[0] = '\0';
                            }
                            worker_cleanup(&_g_worker);
                            break;
                          }
                          msgf(9,"error","%s",&_pkt_body_9543);
                          app->retry_count = 0;
                          bVar3 = true;
                          if (app->worker_input_path[0] != '\0') {
                            unlink(app->worker_input_path);
                            app->worker_input_path[0] = '\0';
                            bVar3 = true;
                          }
                        }
                        else {
                          iVar10 = strcmp(acStack_40360,"PS");
                          if (iVar10 == 0) {
                            if (DAT_002cc02c != 0) {
                              if (_pager_paused == 0) {
                                tui_print_token(ds_cstr((RecoveredDString *)&_token_batch));
                                _pager_lines = _pager_lines + 1;
                              }
                              ds_clear((RecoveredDString *)&_token_batch);
                              chat_advance();
                              if (_chat_user_scrolled == 0) {
                                chat_refresh();
                              }
                              doupdate();
                            }
                            memcpy(aRStack_40260,&_C_1165_9738,0x100);
                            iVar10 = atoi(&_pkt_body_9543);
                            pcVar8 = strchr(&_pkt_body_9543,0x3a);
                            if (pcVar8 != (char *)0x0) {
                              strncpy((char *)aRStack_40260,pcVar8 + 1,0xff);
                              uStack_40161 = 0;
                            }
                            if (0 < iVar10) {
                              plan_init(&app->plan);
                              (app->plan).count = iVar10;
                              if (APP_FIELD8(aRStack_40260[0].device, 0) != '\0') {
                                app_copy_padded((app->plan).title,(char *)aRStack_40260,0xff);
                              }
                              app->plan_present = 1;
                              pcVar8 = (app->plan).title;
                              if ((app->plan).title[0] == '\0') {
                                pcVar8 = "(untitled)";
                              }
                              msgf(0xc,"plan","Plan: %s (%d steps)",pcVar8,iVar10);
                            }
                          }
                          else {
                            iVar10 = strcmp(acStack_40360,"PI");
                            if (iVar10 == 0) {
                              if (DAT_002cc02c != 0) {
                                if (_pager_paused == 0) {
                                  tui_print_token(ds_cstr((RecoveredDString *)&_token_batch));
                                  _pager_lines = _pager_lines + 1;
                                }
                                ds_clear((RecoveredDString *)&_token_batch);
                                chat_advance();
                                if (_chat_user_scrolled == 0) {
                                  chat_refresh();
                                }
                                doupdate();
                              }
                              memcpy(aRStack_40260,&_C_1167_9747,0x100);
                              pcVar8 = strchr(&_pkt_body_9543,0x3a);
                              if (pcVar8 == (char *)0x0) {
                                uVar6 = 0;
                                uVar12 = 0;
                                iVar10 = 1;
                              }
                              else {
                                uVar6 = atoi(&_pkt_body_9543);
                                pcVar15 = strchr(pcVar8 + 1,0x3a);
                                if (pcVar15 == (char *)0x0) {
                                  iVar10 = uVar6 + 1;
                                  uVar12 = 0;
                                }
                                else {
                                  uVar12 = atoi(pcVar8 + 1);
                                  strncpy((char *)aRStack_40260,pcVar15 + 1,0xff);
                                  uStack_40161 = 0;
                                  iVar10 = uVar6 + 1;
                                }
                              }
                              if ((uVar6 < 0x20) && ((int)uVar6 < (app->plan).count)) {
                                *(uint *)(_g_state.history.entries[0] +
                                         (int)((app->history).entries[0xab] +
                                              uVar6 * 0x2808 + 0x41f0)) = uVar12;
                                if (APP_FIELD8(aRStack_40260[0].device, 0) != '\0') {
                                  app_copy_padded(_g_state.history.entries[0] +
                                           (int)((app->history).entries[0xab] +
                                                uVar6 * 0x2808 + 0x29ec),(char *)aRStack_40260,0xfff
                                          );
                                }
                              }
                              if (uVar12 == 2) {
                                pcVar8 = "done";
                              }
                              else if (uVar12 < 3) {
                                if (uVar12 == 1) {
                                  pcVar8 = "running";
                                }
                                else {
LAB_0001f348:
                                  pcVar8 = "step";
                                }
                              }
                              else if (uVar12 == 3) {
                                pcVar8 = "failed";
                              }
                              else {
                                if (uVar12 != 4) goto LAB_0001f348;
                                pcVar8 = "cancelled";
                              }
                              pcVar15 = (char *)aRStack_40260;
                              if (APP_FIELD8(aRStack_40260[0].device, 0) == '\0') {
                                pcVar15 = "";
                              }
                              msgf(0xc,pcVar8,"%d/%d %s",iVar10,(app->plan).count,pcVar15);
                            }
                            else {
                              iVar10 = strcmp(acStack_40360,"U");
                              if (iVar10 == 0) {
                                if (DAT_002cc02c != 0) {
                                  if (_pager_paused == 0) {
                                    tui_print_token(ds_cstr((RecoveredDString *)&_token_batch));
                                    _pager_lines = _pager_lines + 1;
                                  }
                                  ds_clear((RecoveredDString *)&_token_batch);
                                  chat_advance();
                                  if (_chat_user_scrolled == 0) {
                                    chat_refresh();
                                  }
                                  doupdate();
                                }
                                iStack_40368 = 0;
                                iStack_40364 = 0;
                                sscanf(&_pkt_body_9543,"%d:%d",&iStack_40368,&iStack_40364
                                                );
                                app->input_tokens = iStack_40368;
                                app->output_tokens = iStack_40364;
                                app->total_tokens = iStack_40368 + iStack_40364;
                                app->usage_known = 1;
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
          iVar9 = iVar9 + 1;
          if (7 < iVar9) break;
        }
      } while (_g_worker.state - 1U < 2);
      if (DAT_002cc02c != 0) {
        if (_pager_paused == 0) {
          tui_print_token(ds_cstr((RecoveredDString *)&_token_batch));
          _pager_lines = _pager_lines + 1;
          if (999999 < _pager_lines) {
            _pager_paused = 1;
            msgf(10,"--more--","Press SPACE to continue, q to skip");
          }
        }
        ds_clear((RecoveredDString *)&_token_batch);
        chat_advance();
        if (_chat_user_scrolled == 0) {
          chat_refresh();
        }
      }
    }
    if (_g_worker.state == 3) {
      _streaming = 0;
      app->worker_active = 0;
      pVar11 = getpid();
      snprintf((char *)aRStack_40260,0x100,"/tmp/ppccode_worker_%d_out.json",pVar11);
      worker_deserialize_out(app,(char *)aRStack_40260);
      unlink((char *)aRStack_40260);
      worker_cleanup(&_g_worker);
      wprintw(_w_chat,"\n");
      chat_advance();
      chat_refresh();
      iVar9 = queue_empty((RecoveredQueue *)&_g_queue);
      if (iVar9 == 0) {
        pcVar8 = queue_pop((RecoveredQueue *)&_g_queue);
        start_turn(app,pcVar8);
      }
    }
    if (app->retry_remaining < 1) {
      doupdate();
      uVar12 = wgetch(_w_input);
      uVar6 = _draft_len;
      ppRVar4 = PTR__g_theme_000396c0;
      if (uVar12 != 0xffffffff) {
        if (_g_worker.state != 2) {
          if (_pager_paused != 0) {
            if ((uVar12 != 0x20) && (uVar12 != 0x71)) goto LAB_00020260;
            _pager_paused = 0;
            _pager_lines = 0;
            if (uVar12 == 0x71) goto LAB_00020260;
          }
          if (uVar12 == 0x1b) {
            if (_g_worker.state - 1U < 2) {
              tVar13 = time((time_t *)0x0);
              if ((app->cancel_armed == 0) || (5 < tVar13 - app->cancel_time)) {
                app->cancel_armed = 1;
                app->cancel_time = tVar13;
                msgf(10,"interrupt","Press ESC again within 5s to cancel");
              }
              else {
                app->cancel_armed = 0;
                msg(9,"interrupt","Interrupted");
                _streaming = 0;
                app->worker_active = 0;
                worker_kill(&_g_worker);
                wprintw(_w_chat,"\n");
                chat_advance();
                chat_refresh();
              }
            }
            else {
              memset(&_draft_buf,0,0x40000);
              _draft_cursor = 0;
              _draft_len = 0;
            }
          }
          else if (((uVar12 == 0xd) || (uVar12 == 10)) || (uVar12 == 0x157)) {
            nodelay(_w_input,1);
            iVar9 = wgetch(_w_input);
            if ((iVar9 != -1) && (ungetch(iVar9), (int)_draft_len < 0x3ffff)) {
              (&_draft_buf)[_draft_len] = '\n';
              _draft_cursor = _draft_len + 1;
              puVar2 = (undefined1 *)(&DAT_0028c029 + _draft_len);
              _draft_len = _draft_cursor;
              *puVar2 = 0;
            }
            wtimeout(_w_input,0x32);
            uVar6 = _draft_len;
            if ((iVar9 == -1) && ((&_draft_buf)[_draft_len] = '\0', uVar6 != 0)) {
              app_copy_padded(acStack_40060,&_draft_buf,0x3ffff);
              memset(&_draft_buf,0,0x40000);
              _draft_cursor = 0;
              _draft_len = 0;
              redraw_input(&_draft_buf);
              doupdate();
              iVar9 = strcmp(acStack_40060,"/paste");
              if ((iVar9 != 0) ||
                 ((iVar9 = read_paste(acStack_40060,0x40000), -1 < iVar9 &&
                  (acStack_40060[0] != '\0')))) {
                strncpy(app->last_input,acStack_40060,0x3ffff);
                app->last_input[0x3ffff] = '\0';
                app->retry_count = 0;
                if ((acStack_40060[0] != '/') ||
                   (iVar9 = handle_cmd(app,acStack_40060), iVar9 == 0)) {
                  if ((app->conversation).message_count < 0x17e) {
                    history_add(&app->history,acStack_40060);
                    ppRVar4 = PTR__g_theme_000396c0;
                    wattr_on(_w_chat,(*PTR__g_theme_000396c0)->pairs[0xd].attributes | 0xe00,
                              (void *)0x0);
                    wprintw(_w_chat,"[you] ");
                    wattr_off(_w_chat,(*ppRVar4)->pairs[0xd].attributes | 0xe00,(void *)0x0);
                    cprint(_w_chat,3,acStack_40060);
                    wprintw(_w_chat,"\n");
                    chat_advance();
                    _chat_user_scrolled = 0;
                    chat_refresh();
                    doupdate();
                    if (_g_worker.state - 1U < 2) {
                      iVar9 = queue_full((RecoveredQueue *)&_g_queue);
                      if (iVar9 == 0) {
                        queue_push((RecoveredQueue *)&_g_queue,acStack_40060);
                        msgf(0xc,"info","Queued (%d pending)",DAT_0028be04);
                      }
                      else {
                        msg(9,"error","Queue full (max 8). Wait for current turn.");
                      }
                    }
                    else {
                      start_turn(app,acStack_40060);
                    }
                  }
                  else {
                    msg(9,"error","Conversation full. Use /clear or /compact.");
                  }
                }
              }
            }
          }
          else if ((((uVar12 == 4) || (uVar12 == 0x7f)) || (uVar12 == 0x107)) || (uVar12 == 8)) {
            if (0 < (int)_draft_cursor) {
              memmove(_g_worker.opaque_20 + _draft_cursor + 0x1ff,&_draft_buf + _draft_cursor,
                       (_draft_len - _draft_cursor) + 1);
              _draft_cursor = _draft_cursor - 1;
              _draft_len = _draft_len - 1;
            }
          }
          else if (uVar12 == 0x104) {
            if (0 < (int)_draft_cursor) {
              _draft_cursor = _draft_cursor - 1;
            }
          }
          else if (uVar12 == 0x105) {
            if ((int)_draft_cursor < (int)_draft_len) {
              _draft_cursor = _draft_cursor + 1;
            }
          }
          else if (uVar12 == 0x153) {
            iVar9 = _th + -4;
            if (iVar9 < 1) {
              iVar9 = 1;
            }
            _chat_top = _chat_top - iVar9;
            if (_chat_top < 0) {
              _chat_top = 0;
            }
            _chat_user_scrolled = 1;
            chat_refresh();
          }
          else if (uVar12 == 0x152) {
            iVar9 = _th + -4;
            if (iVar9 < 1) {
              iVar9 = 1;
            }
            _chat_top = iVar9 + _chat_top;
            _chat_user_scrolled = 1;
            chat_refresh();
          }
          else if (uVar12 == 0x15) {
            iVar9 = 0;
            if (_th + -3 <= _chat_y) {
              iVar9 = (_chat_y - _th) + 4;
            }
            _chat_top = _chat_top + -1;
            if (_chat_top < 0) {
              _chat_top = 0;
            }
            if (iVar9 < _chat_top) {
              _chat_top = iVar9;
            }
            _chat_user_scrolled = 1;
            chat_refresh();
          }
          else if (uVar12 == 0x106) {
            _chat_top = 0;
            _chat_user_scrolled = 1;
            chat_refresh();
          }
          else {
            if (uVar12 != 0x168) {
              if (uVar12 == 0x103) {
                if (0 < (app->history).count) {
                  pcVar8 = history_prev(&app->history);
                  if (pcVar8 != (char *)0x0) {
                    strncpy(&_draft_buf,pcVar8,0x3ffff);
                    _draft_len = strlen(&_draft_buf);
                    _draft_cursor = _draft_len;
                  }
                  goto LAB_00020260;
                }
LAB_0002011c:
                if ((uVar12 == 0xc) && (_draft_len == 0)) {
                  app->sidebar_visible = (uint)(app->sidebar_visible == 0);
                  handle_resize();
                  if (app->sidebar_visible == 0) {
                    pcVar8 = "OFF";
                  }
                  else {
                    pcVar8 = "ON";
                  }
                  msgf(0xf,"system","File browser: %s",pcVar8);
                  goto LAB_00020260;
                }
              }
              else if (uVar12 == 0x102) {
                if (0 < (app->history).count) {
                  pcVar8 = history_next(&app->history);
                  if (pcVar8 == (char *)0x0) {
                    memset(&_draft_buf,0,0x40000);
                    _draft_cursor = 0;
                    _draft_len = 0;
                  }
                  else {
                    strncpy(&_draft_buf,pcVar8,0x3ffff);
                    _draft_len = strlen(&_draft_buf);
                    _draft_cursor = _draft_len;
                  }
                  goto LAB_00020260;
                }
              }
              else {
                if (uVar12 == 9) {
                  if (_draft_buf == '/') {
                    if ((0 < (int)_draft_len) && (_slash_cmds != NULL)) {
                      iVar9 = 0;
                      ppuVar7 = (undefined **)(&_slash_cmds);
                      pcVar8 = _slash_cmds;
                      do {
                        ppuVar7 = ppuVar7 + 1;
                        iVar10 = strncmp(pcVar8,&_draft_buf,uVar6);
                        if (iVar10 == 0) {
                          strncpy(&_draft_buf,(&_slash_cmds)[iVar9],0x3ffff);
                          _draft_len = strlen(&_draft_buf);
                          _draft_cursor = _draft_len;
                          break;
                        }
                        iVar9 = iVar9 + 1;
                        pcVar8 = (char *)(*ppuVar7);
                      } while (pcVar8 != (char *)0x0);
                    }
                  }
                  else if (0 < (int)_draft_len) {
                    if ((((int)(_draft_len - 1) < 0) ||
                        (((&_draft_buf)[_draft_len - 1]) == ' ')) ||
                       (((&_draft_buf)[_draft_len - 1]) == '\n')) {
                      length = 0;
                      uVar5 = 0xffffffff;
                    }
                    else {
                      pcVar8 = &_draft_buf + _draft_len - 2;
                      uVar6 = 0;
                      uVar12 = _draft_len;
                      do {
                        length = uVar6 + 1;
                        uVar12 = uVar12 - 1;
                        uVar5 = _draft_len - 1;
                        if ((uVar12 == 0) || (cVar1 = *pcVar8, uVar5 = uVar6, cVar1 == ' ')) break;
                        pcVar8 = pcVar8 + -1;
                        uVar6 = length;
                      } while (cVar1 != '\n');
                    }
                    if (uVar5 < 0xfe) {
                      memcpy(acStack_40360,&_draft_buf + (_draft_len - length),length);
                      acStack_40360[length] = '\0';
                      pcVar8 = (app->config).working_directory;
                      app_format_truncated(acStack_40060,0x200,"%s/%s*",pcVar8,acStack_40360);
                      directory = opendir(pcVar8);
                      if (directory != (void *)0x0) {
                        aRStack_40260[0].device = aRStack_40260[0].device & 0xffffff;
                        memset((void *)((int)&aRStack_40260[0].device + 1),0,0xff);
                        iVar9 = 0;
                        while (pRVar14 = (RecoveredTigerDirent *)(readdir(directory)), pRVar14 != (RecoveredTigerDirent *)0x0
                              ) {
                          if (pRVar14->name[0] != '.') {
                            iVar10 = strncmp(pRVar14->name,acStack_40360,length);
                            if (iVar10 == 0) {
                              if (iVar9 == 0) {
                                snprintf((char *)aRStack_40260,0x100,"%s",pRVar14->name);
                              }
                              iVar9 = iVar9 + 1;
                            }
                          }
                        }
                        closedir(directory);
                        uVar6 = _draft_len;
                        if ((iVar9 == 1) && (APP_FIELD8(aRStack_40260[0].device, 0) != '\0')) {
                          iVar9 = 0x3ffff - _draft_len;
                          uVar12 = strlen((char *)aRStack_40260);
                          uVar12 = uVar12 - length;
                          if ((0 < (int)uVar12) && ((int)uVar12 < iVar9)) {
                            memcpy(&_draft_buf + uVar6,
                                    aRStack_40260[0].reserved_50 + (length - 0x50),uVar12);
                            _draft_len = uVar12 + _draft_len;
                            _draft_cursor = _draft_len;
                            (&_draft_buf)[_draft_len] = '\0';
                          }
                        }
                      }
                    }
                  }
                  goto LAB_00020260;
                }
                if (uVar12 != 0x12) goto LAB_0002011c;
                if (_draft_len == 0) {
                  wattr_on(_w_status,(*PTR__g_theme_000396c0)->pairs[0xc].attributes | 0xd00,
                            (void *)0x0);
                  mvwprintw(_w_status,0,1,"Scanning Ollama models (check status bar)...");
                  wattr_off(_w_status,(*ppRVar4)->pairs[0xc].attributes | 0xd00,(void *)0x0);
                  wnoutrefresh(_w_status);
                  doupdate();
                  ollama_refresh_models(&app->config);
                  msgf(0xf,"system","Scanning models started...");
                  goto LAB_00020260;
                }
              }
              if ((uVar12 & 0x80) == 0) {
                uVar6 = *(uint *)(PTR_000396b0 + (uVar12 & 0xff) * 4 + 0x34) >> 0x12 & 1;
              }
              else {
                iVar9 = __maskrune(uVar12 & 0xff,0x40000);
                uVar6 = (uint)(iVar9 != 0);
              }
              if ((uVar6 != 0) && ((int)_draft_len < 0x3ffff)) {
                memmove(&DAT_0028c029 + _draft_cursor,&_draft_buf + _draft_cursor,
                         (_draft_len - _draft_cursor) + 1);
                (&_draft_buf)[_draft_cursor] = (char)uVar12;
                _draft_cursor = _draft_cursor + 1;
                _draft_len = _draft_len + 1;
              }
              goto LAB_00020260;
            }
            _chat_user_scrolled = 0;
            chat_refresh();
          }
          goto LAB_00020260;
        }
        if ((uVar12 == 0x79) || (uVar12 == 0x59)) {
          worker_send_approve(&_g_worker,_g_worker.unknown_1c,1);
          msg(0xb,"system","Approved");
        }
        else if ((uVar12 == 0x6e) || (uVar12 == 0x4e)) {
          worker_send_approve(&_g_worker,_g_worker.unknown_1c,0);
          msg(9,"system","Rejected");
        }
        else if ((uVar12 == 0x61) || (uVar12 == 0x41)) {
          (app->config).auto_approve = 1;
          config_save(&app->config);
          worker_send_approve(&_g_worker,_g_worker.unknown_1c,1);
          msg(0xb,"system","Auto-approve enabled");
        }
      }
      goto LAB_00020260;
    }
    iVar9 = app->retry_remaining + -1;
    app->retry_remaining = iVar9;
    if (iVar9 < 1) {
      app->retry_remaining = 0;
      msg(0xb,"system","Retrying...");
      start_turn(app,app->last_input);
    }
  } while( true );

#undef iStack_40368
#undef iStack_40364
#undef acStack_40360
#undef aRStack_40260
#undef uStack_40161
#undef acStack_40060
}

/*
 * _start_turn @ 0x000202dc..0x000204b4
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Build project context, serialize credentials/messages, start a worker and mark UI streaming state.
 * Parameters/return (inferred): void _start_turn(RecoveredAppState *app,char *message)
 * Direct callers: _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; performs network/protocol I/O; replaces a process with an executable; resolves/executes a program through PATH; executes an argument vector through PATH; reads text from a stream; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; writes text to a stream; reads bytes from a stream; writes bytes to a stream; reads the process working directory; reads process environment; signals a process; creates directories; securely creates a temporary file; opens a directory for enumeration; executes a shell command and opens a pipe; reads a descriptor or IPC channel; reads directory entries; resolves a filesystem path; renames/replaces filesystem entries; changes process environment; executes SQL text; opens/creates a SQLite database; prepares SQL text; reads filesystem metadata; removes a filesystem entry; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000202dc_start_turn.s
 */
void start_turn(RecoveredAppState *app,char *message)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x240] __attribute__((aligned(16)));
#define acStack_228 (*((char (*)[256])(app_frame + 0x8))) /* original SP - 0x228 */
#define acStack_128 (*((char (*)[272])(app_frame + 0x108))) /* original SP - 0x128 */

  RecoveredTheme **ppRVar1;
  char *pcVar2;
  pid_t pVar3;
  int iVar4;
  time_t tVar5;
  RecoveredDString *str;


  if (app->worker_input_path[0] != '\0') {
    unlink(app->worker_input_path);
    app->worker_input_path[0] = '\0';
  }
  context_build(&app->project_context,(app->config).working_directory);
  if ((app->conversation).message_count == 0) {
    (app->conversation).message_count = 1;
    memset(&app->conversation,0,0x5c);
    (app->conversation).messages[0].role = 0;
    str = &(app->conversation).messages[0].content;
    ds_init(str);
    pcVar2 = ds_cstr(&app->project_context);
    ds_appendf(str,"Current state: %s",pcVar2);
  }
  worker_serialize_in(app,message,acStack_228,0x100);
  pVar3 = getpid();
  snprintf(acStack_128,0x100,"/tmp/ppccode_worker_%d_out.json",pVar3);
  iVar4 = worker_start(&_g_worker,acStack_228,acStack_128);
  if (iVar4 == 0) {
    strncpy(app->worker_input_path,acStack_228,0x3ff);
    app->worker_input_path[0x3ff] = '\0';
    iVar4 = now_ms();
    app->worker_start_ms = iVar4;
    app->opaque_87f378 = 0;
    ppRVar1 = PTR__g_theme_000396c0;
    wattr_on(_w_chat,(*PTR__g_theme_000396c0)->pairs[0xe].attributes | 0xf00,(void *)0x0);
    wprintw(_w_chat,"[assistant] ");
    wattr_off(_w_chat,(*ppRVar1)->pairs[0xe].attributes | 0xf00,(void *)0x0);
    chat_advance();
    chat_refresh();
    doupdate();
    _streaming = 1;
    tVar5 = time((time_t *)0x0);
    app->turn_start_time = tVar5;
    app->worker_active = 2;
    app->spinner = 0;
    app->cancel_armed = 0;
  }
  else {
    msg(9,"error","Failed to start worker process");
  }
  return;

#undef acStack_228
#undef acStack_128
}

