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
 * _url_encode @ 0x00028a68..0x00028b80
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in web_search: url encode. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _url_encode(byte *param_1,int param_2,uint param_3)
 * Direct callers: _fetch_bing_mobile@0x00028e20
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00028a68_url_encode.s
 */
void url_encode(byte *param_1,int param_2,uint param_3)

{
  undefined *puVar1;
  uint uVar2;
  int iVar3;
  int iVar4;
  
  puVar1 = (undefined *)(PTR_000396b0);
  uVar2 = (uint)(char)*param_1;
  if ((uVar2 == 0) || (param_3 < 5)) {
    iVar4 = 0;
  }
  else {
    iVar4 = 0;
    do {
      if ((uVar2 & 0x80) == 0) {
        uVar2 = -(*(uint *)(puVar1 + (uVar2 & 0xff) * 4 + 0x34) & 0x500) >> 0x1f;
      }
      else {
        iVar3 = __maskrune(uVar2 & 0xff,0x500);
        uVar2 = (uint)(iVar3 != 0);
      }
      if (uVar2 == 0) {
        if (*param_1 == 0x20) {
          *(undefined1 *)(param_2 + iVar4) = 0x2b;
          iVar4 = iVar4 + 1;
        }
        else {
          iVar3 = sprintf((char *)(param_2 + iVar4),"%%%02X",(uint)*param_1);
          iVar4 = iVar4 + iVar3;
        }
      }
      else {
        *(byte *)(param_2 + iVar4) = *param_1;
        iVar4 = iVar4 + 1;
      }
      param_1 = param_1 + 1;
      uVar2 = (uint)(char)*param_1;
    } while ((uVar2 != 0) && (iVar4 + 4U < param_3));
  }
  *(undefined1 *)(param_2 + iVar4) = 0;
  return;
}

/*
 * _strip_entity @ 0x00028b80..0x00028d34
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in web_search: strip entity. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _strip_entity(char *param_1)
 * Direct callers: _fetch_bing_mobile@0x00028e20
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00028b80_strip_entity.s
 */
void strip_entity(char *param_1)

{
  char cVar1;
  int iVar2;
  char *left;
  
  left = param_1;
  while( true ) {
    cVar1 = *left;
    if (cVar1 == '\0') break;
    if (cVar1 == '&') {
      iVar2 = strncmp(left,"&quot;",6);
      if (iVar2 == 0) {
        *param_1 = '\"';
        param_1 = param_1 + 1;
        left = left + 6;
      }
      else {
        iVar2 = strncmp(left,"&amp;",5);
        if (iVar2 == 0) {
          *param_1 = '&';
          param_1 = param_1 + 1;
          left = left + 5;
        }
        else {
          iVar2 = strncmp(left,"&lt;",4);
          if (iVar2 == 0) {
            *param_1 = '<';
            param_1 = param_1 + 1;
            left = left + 4;
          }
          else {
            iVar2 = strncmp(left,"&gt;",4);
            if (iVar2 == 0) {
              *param_1 = '>';
              param_1 = param_1 + 1;
              left = left + 4;
            }
            else {
              iVar2 = strncmp(left,"&#39;",5);
              if (iVar2 == 0) {
                *param_1 = '\'';
                param_1 = param_1 + 1;
                left = left + 5;
              }
              else {
                iVar2 = strncmp(left,"&nbsp;",6);
                if (iVar2 == 0) {
                  *param_1 = ' ';
                  param_1 = param_1 + 1;
                  left = left + 6;
                }
                else {
                  iVar2 = strncmp(left,"&#0183;",6);
                  if (iVar2 == 0) {
                    *param_1 = '-';
                    param_1 = param_1 + 1;
                    left = left + 6;
                  }
                  else {
                    left = left + 1;
                  }
                }
              }
            }
          }
        }
      }
    }
    else {
      *param_1 = cVar1;
      param_1 = param_1 + 1;
      left = left + 1;
    }
  }
  *param_1 = '\0';
  return;
}

/*
 * _extract_text @ 0x00028d34..0x00028e20
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in web_search: extract text. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _extract_text(char *param_1,char *param_2,int param_3,uint param_4)
 * Direct callers: _fetch_bing_mobile@0x00028e20
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00028d34_extract_text.s
 */
void extract_text(char *param_1,char *param_2,int param_3,uint param_4)

{
  char cVar1;
  bool bVar2;
  uint uVar3;
  int iVar4;
  uint uVar5;
  uint uVar6;
  
  uVar3 = strlen(param_2);
  if ((*param_1 == '\0') || (param_4 < 2)) {
    uVar5 = 0;
  }
  else {
    bVar2 = false;
    uVar6 = 1;
    uVar5 = 0;
    while (iVar4 = strncasecmp(param_1,param_2,uVar3), iVar4 != 0) {
      cVar1 = *param_1;
      if (cVar1 == '<') {
        bVar2 = true;
      }
      else if (cVar1 == '>') {
        bVar2 = false;
      }
      else if (!bVar2) {
        *(char *)(param_3 + uVar5) = cVar1;
        uVar5 = uVar6;
      }
      param_1 = param_1 + 1;
      if ((*param_1 == '\0') || (uVar6 = uVar5 + 1, param_4 <= uVar6)) break;
    }
  }
  *(undefined1 *)(param_3 + uVar5) = 0;
  return;
}

/*
 * _fetch_bing_mobile @ 0x00028e20..0x00029168
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Fetch a Bing mobile search page and extract bounded result text.
 * Parameters/return (inferred): undefined4 _fetch_bing_mobile(undefined4 param_1,char *param_2,int param_3)
 * Direct callers: _search_web@0x00029168
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00028e20_fetch_bing_mobile.s
 */
undefined4 fetch_bing_mobile(undefined4 param_1,char *param_2,int param_3)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x1660] __attribute__((aligned(16)));
#define auStack_1648 (*((undefined1 (*)[1024])(app_frame + 0x8))) /* original SP - 0x1648 */
#define acStack_1248 (*((char (*)[2048])(app_frame + 0x408))) /* original SP - 0x1248 */
#define RStack_a48 (*((RecoveredDString *)(app_frame + 0xc08))) /* original SP - 0xa48 */
#define local_a3c (*((char (*)[512])(app_frame + 0xc14))) /* original SP - 0xa3c */
#define auStack_83c (*((undefined1 (*)[1024])(app_frame + 0xe14))) /* original SP - 0x83c */
#define local_43c (*((char (*)[1036])(app_frame + 0x1214))) /* original SP - 0x43c */

  char *output;
  int iVar1;
  char *pcVar2;
  char *pcVar3;
  char *pcVar4;
  char *pcVar5;
  uint length;

  url_encode((byte *)(param_1),(int)(auStack_1648),0x400);
  app_format_truncated(acStack_1248,0x800,"http://www.bing.com/search?q=%s",auStack_1648);
  output = malloc(0x40000);
  if (output != (char *)0x0) {
    iVar1 = fetch_url_custom_ua
                      (acStack_1248,"raw",output,0x40000,
                       "Nokia6600/1.0 (4.03.24) SymbianOS/7.0s Series60/2.0 Profile/MIDP-2.0 Configuration/CLDC-1.0"
                      );
    if (iVar1 != 0) {
      free(output);
      return 0xffffffff;
    }
    ds_init(&RStack_a48);
    ds_append(&RStack_a48,"--- WEB SEARCH RESULTS (via Bing Mobile) ---\n");
    iVar1 = 0;
    pcVar5 = output;
    while (pcVar4 = strstr(pcVar5,"<li class=\"b_algo\""), pcVar4 != (char *)0x0) {
      if (9 < iVar1) goto LAB_00029110;
      pcVar2 = strstr(pcVar4,"<a href=\"");
      pcVar5 = pcVar4 + 0x12;
      if (pcVar2 != (char *)0x0) {
        pcVar2 = pcVar2 + 9;
        pcVar3 = strchr(pcVar2,0x22);
        pcVar5 = pcVar4 + 0x12;
        if (pcVar3 != (char *)0x0) {
          memcpy(auStack_83c,&_C_62_3329,0x400);
          length = (int)pcVar3 - (int)pcVar2;
          if (0x3ff < length) {
            length = 0x3ff;
          }
          memcpy(auStack_83c,pcVar2,length);
          auStack_83c[length] = 0;
          pcVar5 = strstr(pcVar4,"<h2");
          memcpy(local_a3c,&_C_66_3334,0x200);
          if ((pcVar5 != (char *)0x0) && (pcVar5 = strchr(pcVar5,0x3e), pcVar5 != (char *)0x0)) {
            extract_text(pcVar5 + 1,"</h2>",(int)(local_a3c),0x200);
          }
          pcVar5 = strstr(pcVar4,"<div class=\"b_caption\">");
          memcpy(local_43c,&_C_67_3336,0x400);
          if (((pcVar5 != (char *)0x0) && (pcVar5 = strstr(pcVar5,"<p"), pcVar5 != (char *)0x0)) &&
             (pcVar5 = strchr(pcVar5,0x3e), pcVar5 != (char *)0x0)) {
            extract_text(pcVar5 + 1,"</p>",(int)(local_43c),0x400);
          }
          strip_entity(local_a3c);
          strip_entity(local_43c);
          pcVar5 = pcVar3;
          if ((local_a3c[0] != '\0') || (local_43c[0] != '\0')) {
            iVar1 = iVar1 + 1;
            ds_appendf(&RStack_a48,"RESULT %d:\n",iVar1);
            pcVar4 = local_a3c;
            if (local_a3c[0] == '\0') {
              pcVar4 = "(no title)";
            }
            ds_appendf(&RStack_a48,"TITLE: %s\n",pcVar4);
            pcVar4 = local_43c;
            if (local_43c[0] == '\0') {
              pcVar4 = "(no description)";
            }
            ds_appendf(&RStack_a48,"SUMMARY: %s\n",pcVar4);
            ds_appendf(&RStack_a48,"SOURCE: %s\n",auStack_83c);
            ds_append(&RStack_a48,"---\n");
          }
        }
      }
    }
    if (0 < iVar1) {
LAB_00029110:
      pcVar5 = ds_cstr(&RStack_a48);
      strncpy(param_2,pcVar5,param_3 - 1);
      param_2[param_3 + -1] = '\0';
    }
    ds_free(&RStack_a48);
    free(output);
    if (0 < iVar1) {
      return 0;
    }
  }
  return 0xffffffff;

#undef auStack_1648
#undef acStack_1248
#undef RStack_a48
#undef local_a3c
#undef auStack_83c
#undef local_43c
}

/*
 * _search_web @ 0x00029168..0x000291e4
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Dispatch web searching and return formatted results.
 * Parameters/return (inferred): undefined4 _search_web(char *param_1,char *param_2,uint param_3)
 * Direct callers: _tool_search@0x00014cf8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00029168_search_web.s
 */
undefined4 search_web(char *param_1,char *param_2,uint param_3)

{
  undefined4 uVar1;
  int iVar2;
  
  if ((param_1 == (char *)0x0) || (*param_1 == '\0')) {
    uVar1 = 0xffffffff;
  }
  else {
    iVar2 = fetch_bing_mobile((undefined4)param_1,param_2,param_3);
    uVar1 = 0;
    if (iVar2 != 0) {
      snprintf(param_2,param_3,
                         "Error: Search failed. All providers returned no results or timed out.");
      uVar1 = 0xffffffff;
    }
  }
  return uVar1;
}

