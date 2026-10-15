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
 * _curl_write_cb @ 0x000263b0..0x00026424
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in web_fetch: curl write cb. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): uint _curl_write_cb(char *param_1,int param_2,int param_3,RecoveredDString *param_4)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000263b0_curl_write_cb.s
 */
uint curl_write_cb(char *param_1,int param_2,int param_3,RecoveredDString *param_4)

{
  uint length;
  
  length = 0;
  if (param_4[1].length == 0) {
    length = param_2 * param_3;
    if (param_4[1].data < (char *)(length + param_4->length)) {
      param_4[1].length = 1;
      length = 0;
    }
    else {
      ds_appendn(param_4,param_1,length);
    }
  }
  return length;
}

/*
 * _clean_str @ 0x00026424..0x000264dc
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in web_fetch: clean str. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _clean_str(char *param_1)
 * Direct callers: _strip_html@0x000264dc
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00026424_clean_str.s
 */
void clean_str(char *param_1)

{
  char cVar1;
  char cVar2;
  char *pcVar3;
  
  pcVar3 = param_1;
  do {
    cVar2 = *param_1;
    if (cVar2 == '\0') {
      *pcVar3 = '\0';
      return;
    }
    if ((cVar2 == -0x1e) && (param_1[1] == -0x80)) {
      cVar1 = param_1[2];
      if ((byte)(cVar1 + 0x68U) < 2) {
        *pcVar3 = '\'';
        param_1 = param_1 + 3;
      }
      else if ((byte)(cVar1 + 100U) < 2) {
        *pcVar3 = '\"';
        param_1 = param_1 + 3;
      }
      else {
        if (1 < (byte)(cVar1 + 0x6dU)) goto LAB_000264bc;
        *pcVar3 = '-';
        param_1 = param_1 + 3;
      }
    }
    else {
LAB_000264bc:
      *pcVar3 = cVar2;
      param_1 = param_1 + 1;
    }
    pcVar3 = pcVar3 + 1;
  } while( true );
}

/*
 * _strip_html @ 0x000264dc..0x0002684c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Remove markup from fetched text using the implementation's string-processing rules.
 * Parameters/return (inferred): void _strip_html(byte *param_1,RecoveredDString *param_2)
 * Direct callers: _fetch_url_internal@0x00026940
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000264dc_strip_html.s
 */
void strip_html(byte *param_1,RecoveredDString *param_2)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x50] __attribute__((aligned(16)));
#define local_38 (*((byte (*)[32])(app_frame + 0x8))) /* original SP - 0x38 */

  byte bVar1;
  bool bVar2;
  bool bVar3;
  bool bVar4;
  bool bVar5;
  uint uVar6;
  char *pcVar7;
  char *pcVar8;
  int iVar9;

  pcVar7 = strcasestr((char *)param_1,"<title>");
  if (pcVar7 != (char *)0x0) {
    pcVar7 = pcVar7 + 7;
    pcVar8 = strcasestr(pcVar7,"</title>");
    if (pcVar8 != (char *)0x0) {
      ds_append(param_2,"TITLE: ");
      ds_appendn(param_2,pcVar7,(int)pcVar8 - (int)pcVar7);
      ds_append(param_2,"\n\n");
    }
  }
  bVar2 = false;
  bVar3 = false;
  bVar4 = false;
  bVar5 = true;
  while ((*param_1 != 0 && (param_2->length < 0xfe00))) {
    if (bVar2) {
      if (*param_1 == 0x3e) {
        bVar2 = false;
      }
      param_1 = param_1 + 1;
    }
    else {
      iVar9 = strncasecmp((char *)param_1,"<script",7);
      if (iVar9 == 0) {
        param_1 = param_1 + 7;
        bVar2 = false;
        bVar3 = true;
      }
      else if (bVar3) {
        iVar9 = strncasecmp((char *)param_1,"</script",8);
        if (iVar9 == 0) {
          param_1 = param_1 + 8;
          bVar2 = false;
          bVar3 = false;
        }
        else {
          param_1 = param_1 + 1;
          bVar2 = false;
        }
      }
      else {
        iVar9 = strncasecmp((char *)param_1,"<style",6);
        if (iVar9 == 0) {
          param_1 = param_1 + 6;
          bVar2 = false;
          bVar3 = false;
          bVar4 = true;
        }
        else if (bVar4) {
          iVar9 = strncasecmp((char *)param_1,"</style",7);
          if (iVar9 == 0) {
            param_1 = param_1 + 7;
            bVar2 = false;
            bVar3 = false;
            bVar4 = false;
          }
          else {
            param_1 = param_1 + 1;
            bVar2 = false;
            bVar3 = false;
          }
        }
        else {
          bVar1 = *param_1;
          if (bVar1 == 0x3c) {
            iVar9 = strncasecmp((char *)param_1,"<p",2);
            if (((((iVar9 == 0) || (iVar9 = strncasecmp((char *)param_1,"<br",3), iVar9 == 0)) ||
                 (iVar9 = strncasecmp((char *)param_1,"<li",3), iVar9 == 0)) ||
                ((iVar9 = strncasecmp((char *)param_1,"<tr",3), iVar9 == 0 ||
                 (((param_1[1] == 0x68 && true /* uint8_t is always below 256 */) &&
                  ((*(uint *)(PTR_000396b0 + (uint)param_1[2] * 4 + 0x34) & 0x400) != 0)))))) &&
               (!bVar5)) {
              ds_append(param_2,"\n");
              bVar5 = true;
            }
            param_1 = param_1 + 1;
            bVar2 = true;
            bVar3 = false;
            bVar4 = false;
          }
          else {
            if ((bVar1 & 0x80) == 0) {
              uVar6 = *(uint *)(PTR_000396b0 + (uint)bVar1 * 4 + 0x34) >> 0xe & 1;
            }
            else {
              iVar9 = __maskrune((uint)bVar1,0x4000);
              uVar6 = (uint)(iVar9 != 0);
            }
            if (uVar6 == 0) {
              local_38[0] = *param_1;
              local_38[1] = 0;
              ds_append(param_2,(char *)local_38);
              param_1 = param_1 + 1;
              bVar2 = false;
              bVar3 = false;
              bVar4 = false;
              bVar5 = false;
            }
            else if (bVar5) {
              param_1 = param_1 + 1;
              bVar2 = false;
              bVar3 = false;
              bVar4 = false;
            }
            else {
              ds_append(param_2," ");
              param_1 = param_1 + 1;
              bVar2 = false;
              bVar3 = false;
              bVar4 = false;
            }
          }
        }
      }
    }
  }
  clean_str(ds_cstr(param_2));
  return;

#undef local_38
}

/*
 * _extract_links @ 0x0002684c..0x00026940
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Extract link references from fetched markup.
 * Parameters/return (inferred): void _extract_links(char *param_1,RecoveredDString *param_2)
 * Direct callers: _fetch_url_internal@0x00026940
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002684c_extract_links.s
 */
void extract_links(char *param_1,RecoveredDString *param_2)

{
  char *pcVar1;
  char *pcVar2;
  int character;
  int iVar3;
  
  iVar3 = 0;
  while( true ) {
    param_1 = strcasestr(param_1,"<a");
    if ((param_1 == (char *)0x0) || (0xfeff < param_2->length)) break;
    param_1 = param_1 + 2;
    pcVar1 = strcasestr(param_1,"href=");
    if ((pcVar1 != (char *)0x0) &&
       ((character = (int)pcVar1[5], character == 0x22 || (character == 0x27)))) {
      pcVar1 = pcVar1 + 6;
      pcVar2 = strchr(pcVar1,character);
      if (pcVar2 != (char *)0x0) {
        ds_append(param_2,"- ");
        ds_appendn(param_2,pcVar1,(int)pcVar2 - (int)pcVar1);
        ds_append(param_2,"\n");
        iVar3 = iVar3 + 1;
      }
    }
  }
  if (iVar3 == 0) {
    ds_append(param_2,"(No links found)\n");
  }
  return;
}

/*
 * _fetch_url_internal @ 0x00026940..0x00026c0c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Fetch a URL with redirects, a 1MB response cap and disabled certificate-chain checking; format text/links/HTML output.
 * Parameters/return (inferred): undefined4 _fetch_url_internal(char *param_1,char *param_2,char *param_3,uint param_4,char *param_5)
 * Direct callers: _fetch_url@0x00026c0c, _fetch_url_custom_ua@0x00026c30
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00026940_fetch_url_internal.s
 * Preserved behavior/security: TLS-01: disables certificate-chain verification.
 */
undefined4 fetch_url_internal(char *param_1,char *param_2,char *param_3,uint param_4,char *param_5)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x70] __attribute__((aligned(16)));
#define local_58 (*((RecoveredDString *)(app_frame + 0x8))) /* original SP - 0x58 */
#define local_4c (*((undefined4 *)(app_frame + 0x14))) /* original SP - 0x4c */
#define local_48 (*((int *)(app_frame + 0x18))) /* original SP - 0x48 */
#define local_44 (*((int *)(app_frame + 0x1c))) /* original SP - 0x44 */
#define local_40 (*((RecoveredDString (*)[3])(app_frame + 0x20))) /* original SP - 0x40 */

  void *handle;
  int iVar1;
  char *pcVar2;
  uint length;

  if ((param_1 == (char *)0x0) || (*param_1 == '\0')) {
    snprintf(param_3,param_4,"Error: no URL provided\n");
    return 0xffffffff;
  }
  handle = curl_easy_init();
  if (handle == (void *)0x0) {
    snprintf(param_3,param_4,"Error: could not initialize curl\n");
    return 0xffffffff;
  }
  local_58.data = (char *)0x0;
  local_58.length = 0;
  local_58.capacity = 0;
  local_4c = 0;
  local_48 = 0;
  ds_init(&local_58);
  local_4c = 0x100000;
  curl_easy_setopt(handle,0x2712,param_1);
  curl_easy_setopt(handle,0x4e2b,curl_write_cb);
  curl_easy_setopt(handle,0x2711,&local_58);
  curl_easy_setopt(handle,0x34,1);
  curl_easy_setopt(handle,0x44,10);
  curl_easy_setopt(handle,0xd,0x78);
  curl_easy_setopt(handle,0x40,0);
  if (param_5 == (char *)0x0) {
    param_5 = "Mozilla/5.0 (PowerPC; Mac OS X 10.4)";
  }
  curl_easy_setopt(handle,0x2722,param_5);
  iVar1 = curl_easy_perform(handle);
  if ((iVar1 == 0) && (local_48 == 0)) {
    local_44 = 0;
    curl_easy_getinfo(handle,0x200002,&local_44);
    curl_easy_cleanup(handle);
    if (99 < local_44 - 200U) {
      snprintf(param_3,param_4,"Error: HTTP %ld\n",(long)local_44);
      ds_free(&local_58);
      return 0xffffffff;
    }
    ds_init(local_40);
    iVar1 = strcmp(param_2,"strip");
    if (iVar1 == 0) {
      pcVar2 = ds_cstr(&local_58);
      strip_html((byte *)(pcVar2),local_40);
    }
    else {
      iVar1 = strcmp(param_2,"links");
      if (iVar1 == 0) {
        pcVar2 = ds_cstr(&local_58);
        extract_links(pcVar2,local_40);
      }
      else {
        ds_copy(local_40,&local_58);
      }
    }
    length = param_4 - 1;
    if (local_40[0].length < param_4 - 1) {
      length = local_40[0].length;
    }
    if ((length != 0) && (local_40[0].data != (char *)0x0)) {
      memcpy(param_3,local_40[0].data,length);
    }
    param_3[length] = '\0';
    ds_free(local_40);
    ds_free(&local_58);
    return 0;
  }
  pcVar2 = (char *)curl_easy_strerror((CURLcode)iVar1);
  if (local_48 != 0) {
    pcVar2 = "Download too large (max 1MB)";
  }
  snprintf(param_3,param_4,"Error: %s\n",pcVar2);
  ds_free(&local_58);
  curl_easy_cleanup(handle);
  return 0xffffffff;

#undef local_58
#undef local_4c
#undef local_48
#undef local_44
#undef local_40
}

/*
 * _fetch_url @ 0x00026c0c..0x00026c30
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in web_fetch: fetch url. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _fetch_url(char *url,char *mode,char *output,int output_size)
 * Direct callers: _tool_fetch_url@0x00014d94
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00026c0c_fetch_url.s
 */
int fetch_url(char *url,char *mode,char *output,int output_size)

{
  int iVar1;
  
  iVar1 = fetch_url_internal(url,mode,output,output_size,NULL);
  return iVar1;
}

/*
 * _fetch_url_custom_ua @ 0x00026c30..0x00026c50
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in web_fetch: fetch url custom ua. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _fetch_url_custom_ua(char *url,char *mode,char *output,int output_size,char *user_agent)
 * Direct callers: _fetch_bing_mobile@0x00028e20
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00026c30_fetch_url_custom_ua.s
 */
int fetch_url_custom_ua(char *url,char *mode,char *output,int output_size,char *user_agent)

{
  int iVar1;
  
  iVar1 = fetch_url_internal(url,mode,output,output_size,user_agent);
  return iVar1;
}

