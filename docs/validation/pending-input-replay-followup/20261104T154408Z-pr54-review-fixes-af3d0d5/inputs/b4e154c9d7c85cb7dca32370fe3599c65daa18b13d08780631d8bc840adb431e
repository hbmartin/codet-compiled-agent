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
 * _cJSON_malloc @ 0x000291e4..0x00029204
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: malloc. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void * _cJSON_malloc(uint bytes)
 * Direct callers: _cJSON_New_Item@0x00029240, _cJSON_strdup@0x000292f8, _parse_string@0x000293e4, _print_string_ptr@0x0002a098, _print_value@0x0002a320, _print_array@0x0002a4bc, _print_object@0x0002a6d0
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000291e4_cJSON_malloc.s
 */
void * cJSON_malloc(uint bytes)

{
  void *pvVar1;
  
  pvVar1 = malloc(bytes);
  return pvVar1;
}

/*
 * _cJSON_free @ 0x00029204..0x00029224
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: free. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _cJSON_free(void *allocation)
 * Direct callers: _cJSON_Delete@0x00029284, _print_array@0x0002a4bc, _print_object@0x0002a6d0, _cJSON_AddItemToObject@0x0002adb0, _cJSON_AddItemReferenceToObject@0x0002af44
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00029204_cJSON_free.s
 */
void cJSON_free(void *allocation)

{
  free(allocation);
  return;
}

/*
 * _cJSON_GetErrorPtr @ 0x00029224..0x00029240
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: GetErrorPtr. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): char * _cJSON_GetErrorPtr(void)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00029224_cJSON_GetErrorPtr.s
 */
char * cJSON_GetErrorPtr(void)

{
  return _ep;
}

/*
 * _cJSON_New_Item @ 0x00029240..0x00029284
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Allocate and zero a 36-byte JSON node.
 * Parameters/return (inferred): RecoveredCJSON * _cJSON_New_Item(void)
 * Direct callers: _parse_array@0x00029d94, _parse_object@0x00029e94, _cJSON_Parse@0x0002a02c, _cJSON_CreateObject@0x0002abb0, _cJSON_CreateArray@0x0002abe0, _cJSON_CreateString@0x0002ac10, _cJSON_CreateNumber@0x0002ac5c, _cJSON_CreateBool@0x0002acac, _cJSON_CreateNull@0x0002acf4, _cJSON_CreateStringReference@0x0002ad24
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00029240_cJSON_New_Item.s
 */
RecoveredCJSON * cJSON_New_Item(void)

{
  RecoveredCJSON *output;
  
  output = cJSON_malloc(0x24);
  if (output != (RecoveredCJSON *)0x0) {
    memset(output,0,0x24);
  }
  return output;
}

/*
 * _cJSON_Delete @ 0x00029284..0x000292f8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Walk a JSON sibling chain, recursively release children and free owned strings/nodes as this bundled implementation does.
 * Parameters/return (inferred): void _cJSON_Delete(RecoveredCJSON *item)
 * Direct callers: _show_simple_http_error@0x0000315c, _session_load@0x00007cb8, _do_async_fetch@0x000087a8, _provider_parse_any@0x0000983c, _provider_parse_tool_calls@0x00009f28, _ollama_sync_model_metadata@0x0000bb9c, _ollama_sse_cb@0x0000c3a0, _ollama_send@0x0000c70c, _openai_sse_cb@0x0000e388, _openai_send@0x0000e5ec, _anthropic_sse_cb@0x0000fad0, _anthropic_send@0x00010540, _google_sse_cb@0x000119fc, _google_send@0x00012238, _write_in_json@0x00013000, _read_result_json@0x00013140, _sa_child_main@0x0001327c, _child_write_out_json@0x00022270, _worker_child_main@0x00022774, _worker_serialize_in@0x00024740, _worker_deserialize_out@0x00024b1c, _parse_embedding_json@0x00027dfc, _cJSON_Delete@0x00029284, _cJSON_Parse@0x0002a02c, _mcp_discover_server@0x0002b980, _tool_execute_mcp@0x0002c0c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00029284_cJSON_Delete.s
 */
void cJSON_Delete(RecoveredCJSON *item)

{
  RecoveredCJSON *pRVar1;
  
  while (item != (RecoveredCJSON *)0x0) {
    pRVar1 = item->next;
    if (item->child != (RecoveredCJSON *)0x0) {
      cJSON_Delete(item->child);
    }
    if (item->value_string != (char *)0x0) {
      cJSON_free(item->value_string);
    }
    if (item->member_name != (char *)0x0) {
      cJSON_free(item->member_name);
    }
    cJSON_free(item);
    item = pRVar1;
  }
  return;
}

/*
 * _cJSON_strdup @ 0x000292f8..0x00029348
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: strdup. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): char * _cJSON_strdup(char *text)
 * Direct callers: _print_string_ptr@0x0002a098, _print_value@0x0002a320, _cJSON_CreateString@0x0002ac10, _cJSON_AddItemToObject@0x0002adb0, _cJSON_AddItemReferenceToObject@0x0002af44
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000292f8_cJSON_strdup.s
 */
char * cJSON_strdup(char *text)

{
  uint uVar1;
  char *output;
  
  uVar1 = strlen(text);
  output = cJSON_malloc(uVar1 + 1);
  if (output != (char *)0x0) {
    memcpy(output,text,uVar1 + 1);
  }
  return output;
}

/*
 * _cJSON_strcasecmp @ 0x00029348..0x000293e4
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: strcasecmp. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): int _cJSON_strcasecmp(char *left,char *right)
 * Direct callers: _cJSON_GetObjectItem@0x0002aad4
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00029348_cJSON_strcasecmp.s
 */
int cJSON_strcasecmp(char *left,char *right)

{
  uint uVar1;
  __darwin_ct_rune_t _Var2;
  __darwin_ct_rune_t _Var3;
  
  if (left == (char *)0x0) {
    uVar1 = (uint)(right != (char *)0x0);
  }
  else {
    uVar1 = 1;
    if (right != (char *)0x0) {
      while( true ) {
        _Var2 = __tolower((uint)(byte)*left);
        _Var3 = __tolower((uint)(byte)*right);
        if (_Var2 != _Var3) break;
        if (*left == 0) {
          return 0;
        }
        left = left + 1;
        right = right + 1;
      }
      _Var2 = __tolower((uint)(byte)*left);
      _Var3 = __tolower((uint)(byte)*right);
      uVar1 = _Var2 - _Var3;
    }
  }
  return uVar1;
}

/*
 * _parse_string @ 0x000293e4..0x00029800
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in bundled_cjson: parse string. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): char * _parse_string(RecoveredCJSON *item,char *text)
 * Direct callers: _parse_value@0x00029ad4, _parse_object@0x00029e94
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000293e4_parse_string.s
 */
char * parse_string(RecoveredCJSON *item,char *text)

{
  byte *pbVar1;
  byte bVar2;
  byte *pbVar3;
  int iVar4;
  int iVar5;
  byte *pbVar6;
  uint uVar7;
  uint uVar8;
  byte *pbVar9;
  byte *pbVar10;
  
  pbVar10 = (byte *)(text + 1);
  if (*text == '\"') {
    bVar2 = *pbVar10;
    pbVar9 = pbVar10;
    if (bVar2 == 0x22) {
      iVar5 = 0;
LAB_00029498:
      pbVar6 = cJSON_malloc(iVar5 + 1);
      pbVar1 = (byte *)0x0;
      pbVar3 = pbVar6;
      if (pbVar6 != (byte *)0x0) {
        while (pbVar10 < pbVar9) {
          if (*pbVar10 == 0x5c) {
            pbVar1 = pbVar10 + 1;
            switch(pbVar10[1]) {
            case 0x62:
              *pbVar3 = 8;
              pbVar3 = pbVar3 + 1;
              break;
            default:
              *pbVar3 = pbVar10[1];
              pbVar3 = pbVar3 + 1;
              break;
            case 0x66:
              *pbVar3 = 0xc;
              pbVar3 = pbVar3 + 1;
              break;
            case 0x6e:
              *pbVar3 = 10;
              pbVar3 = pbVar3 + 1;
              break;
            case 0x72:
              *pbVar3 = 0xd;
              pbVar3 = pbVar3 + 1;
              break;
            case 0x74:
              *pbVar3 = 9;
              pbVar3 = pbVar3 + 1;
              break;
            case 0x75:
              pbVar10 = pbVar10 + 2;
              uVar8 = 0;
              iVar5 = 4;
              do {
                iVar4 = (int)(char)*pbVar10;
                if (iVar4 == 0) break;
                uVar8 = uVar8 * 0x10;
                if (((iVar4 - 0x30U) & 0xff) < 10) {
                  uVar8 = (uVar8 + iVar4) - 0x30;
                }
                else if (((iVar4 - 0x61U) & 0xff) < 6) {
                  uVar8 = (uVar8 + iVar4) - 0x57;
                }
                else if (((iVar4 - 0x41U) & 0xff) < 6) {
                  uVar8 = (uVar8 + iVar4) - 0x37;
                }
                pbVar10 = pbVar10 + 1;
                iVar5 = iVar5 + -1;
              } while (iVar5 != 0);
              if (((uVar8 - 0xd800 < 0x400) && (pbVar10[1] == 0x5c)) && (pbVar10[2] == 0x75)) {
                pbVar10 = pbVar10 + 3;
                uVar7 = 0;
                iVar5 = 4;
                do {
                  iVar4 = (int)(char)*pbVar10;
                  if (iVar4 == 0) break;
                  uVar7 = uVar7 * 0x10;
                  if (((iVar4 - 0x30U) & 0xff) < 10) {
                    uVar7 = (uVar7 + iVar4) - 0x30;
                  }
                  else if (((iVar4 - 0x61U) & 0xff) < 6) {
                    uVar7 = (uVar7 + iVar4) - 0x57;
                  }
                  else if (((iVar4 - 0x41U) & 0xff) < 6) {
                    uVar7 = (uVar7 + iVar4) - 0x37;
                  }
                  pbVar10 = pbVar10 + 1;
                  iVar5 = iVar5 + -1;
                } while (iVar5 != 0);
                uVar8 = ((uVar8 & 0x3ff) * 0x400 + 0x10000) | (uVar7 & 0x3ff);
              }
              if ((int)uVar8 < 0x80) {
                uVar7 = 1;
                iVar5 = 1;
              }
              else if ((int)uVar8 < 0x800) {
                uVar7 = 2;
                iVar5 = 2;
              }
              else {
                uVar7 = 4;
                iVar5 = 4;
              }
              pbVar3 = pbVar3 + iVar5;
              if (uVar7 == 2) {
LAB_00029764:
                pbVar3 = pbVar3 + -1;
                *pbVar3 = ((byte)uVar8 & 0xbf) | 0x80;
                uVar8 = (int)uVar8 >> 6;
LAB_00029778:
                pbVar3 = pbVar3 + -1;
                *pbVar3 = (byte)uVar8;
              }
              else {
                if (2 < uVar7) {
                  if (uVar7 != 3) {
                    if (uVar7 != 4) goto LAB_0002977c;
                    pbVar3 = pbVar3 + -1;
                    *pbVar3 = ((byte)uVar8 & 0xbf) | 0x80;
                    uVar8 = (int)uVar8 >> 6;
                  }
                  pbVar3 = pbVar3 + -1;
                  *pbVar3 = ((byte)uVar8 & 0xbf) | 0x80;
                  uVar8 = (int)uVar8 >> 6;
                  goto LAB_00029764;
                }
                if (uVar7 == 1) goto LAB_00029778;
              }
LAB_0002977c:
              pbVar3 = pbVar3 + iVar5;
              pbVar1 = pbVar10 + -1;
            }
          }
          else {
            *pbVar3 = *pbVar10;
            pbVar3 = pbVar3 + 1;
            pbVar1 = pbVar10;
          }
          pbVar10 = pbVar1 + 1;
        }
        *pbVar3 = 0;
        item->value_string = (char *)pbVar6;
        item->type_flags = 5;
        pbVar1 = pbVar10 + 1;
      }
    }
    else {
      if (bVar2 != 0) {
        iVar5 = 0;
        do {
          pbVar3 = pbVar9;
          if ((bVar2 == 0x5c) && (pbVar3 = pbVar9 + 1, pbVar9[1] == 0)) {
            _ep = (char *)(pbVar9);
            return (char *)0x0;
          }
          iVar5 = iVar5 + 1;
          pbVar9 = pbVar3 + 1;
          bVar2 = pbVar3[1];
          if (bVar2 == 0x22) goto LAB_00029498;
        } while (bVar2 != 0);
      }
      pbVar1 = (byte *)0x0;
      _ep = text;
    }
  }
  else {
    pbVar1 = (byte *)0x0;
    _ep = text;
  }
  return (char *)pbVar1;
}

/*
 * _skip @ 0x00029800..0x00029854
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in bundled_cjson: skip. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): char * _skip(char *text)
 * Direct callers: _parse_value@0x00029ad4, _parse_array@0x00029d94, _parse_object@0x00029e94, _cJSON_Parse@0x0002a02c
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00029800_skip.s
 */
char * skip(char *text)

{
  int iVar1;
  
  if (text == (char *)0x0) {
    return (char *)0x0;
  }
  if (*text == 0) {
    return text;
  }
  iVar1 = -(int)text;
  if ((byte)*text < 0x21) {
    while( true ) {
      text = text + 1;
      iVar1 = iVar1 + -1;
      if (iVar1 == 0) break;
      if (*text == 0) {
        return text;
      }
      if (0x20 < (byte)*text) {
        return text;
      }
    }
  }
  return text;
}

/*
 * _parse_number @ 0x00029854..0x00029ad4
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in bundled_cjson: parse number. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): char * _parse_number(RecoveredCJSON *item,char *text)
 * Direct callers: _parse_value@0x00029ad4
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/00029854_parse_number.s
 */
char * parse_number(RecoveredCJSON *item,char *text)

{
  int iVar1;
  int iVar2;
  char *pcVar3;
  uint uVar4;
  char *pcVar5;
  int iVar6;
  int iVar7;
  double dVar8;
  double dVar9;
  
  uVar4 = 1;
  if (*text == '-') {
    text = text + 1;
    uVar4 = 0xffffffff;
  }
  if (*text == '0') {
    text = text + 1;
  }
  if ((byte)(*text - 0x31U) < 9) {
    dVar8 = 0.0;
    do {
      dVar8 = dVar8 * 10.0 +
              (app_bits_double((CONCAT44(0x43300000,((int)*text - 0x30U) ^ 0x80000000))) - 4503601774854144.0);
      text = text + 1;
    } while ((byte)(*text - 0x30U) < 10);
  }
  else {
    dVar8 = 0.0;
  }
  if (*text == '.') {
    pcVar5 = text + 1;
    if (('/' < text[1]) && (pcVar3 = pcVar5, text[1] < ':')) {
      do {
        dVar8 = dVar8 * 10.0 +
                (app_bits_double((CONCAT44(0x43300000,((int)*pcVar3 - 0x30U) ^ 0x80000000))) - 4503601774854144.0
                );
        text = pcVar3 + 1;
        pcVar3 = text;
      } while ((byte)(*text - 0x30U) < 10);
      iVar7 = (int)text - (int)pcVar5;
      goto LAB_00029994;
    }
  }
  iVar7 = 0;
LAB_00029994:
  if ((*text == 'e') || (*text == 'E')) {
    if (text[1] == '+') {
      iVar1 = 1;
      pcVar5 = text + 2;
    }
    else {
      iVar1 = 1;
      pcVar5 = text + 1;
      if (text[1] == '-') {
        iVar1 = -1;
        pcVar5 = text + 2;
      }
    }
    text = pcVar5;
    iVar6 = (int)*text;
    iVar2 = 0;
    if (((iVar6 - 0x30U) & 0xff) < 10) {
      iVar2 = 0;
      do {
        iVar2 = iVar2 * 10 + iVar6 + -0x30;
        text = text + 1;
        iVar6 = (int)*text;
      } while (((iVar6 - 0x30U) & 0xff) < 10);
      iVar2 = iVar2 * iVar1;
    }
  }
  else {
    iVar2 = 0;
  }
  iVar1 = 0;
  if (iVar7 != 0) {
    iVar1 = -iVar7;
  }
  dVar9 = (double)pow(10.0,app_bits_double((CONCAT44(0x43300000,(iVar1 + iVar2) ^ 0x80000000))) -
                            4503601774854144.0);
  dVar9 = dVar8 * (app_bits_double(CONCAT44(0x43300000,uVar4 ^ 0x80000000)) - 4503601774854144.0) * dVar9;
  item->value_double = dVar9;
  item->value_int = (int)dVar9;
  item->type_flags = 4;
  return text;
}

/*
 * _parse_value @ 0x00029ad4..0x00029d94
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in bundled_cjson: parse value. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): char * _parse_value(RecoveredCJSON *item,char *text)
 * Direct callers: _parse_array@0x00029d94, _parse_object@0x00029e94, _cJSON_Parse@0x0002a02c
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00029ad4_parse_value.s
 */
char * parse_value(RecoveredCJSON *item,char *text)

{
  char *pcVar1;
  int iVar2;
  
  if (text == (char *)0x0) {
    return (char *)0x0;
  }
  pcVar1 = skip(text);
  switch(*pcVar1) {
  case '\"':
    pcVar1 = parse_string(item,pcVar1);
    return pcVar1;
  case '-':
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    pcVar1 = parse_number(item,pcVar1);
    return pcVar1;
  case '[':
    pcVar1 = parse_array(item,pcVar1);
    return pcVar1;
  case 'f':
    iVar2 = strncmp(pcVar1,"false",4);
    if (iVar2 == 0) {
      item->type_flags = 1;
      item->value_int = 0;
      return pcVar1 + 5;
    }
    break;
  case 'n':
    iVar2 = strncmp(pcVar1,"null",4);
    if (iVar2 == 0) {
      item->type_flags = 3;
      return pcVar1 + 4;
    }
    break;
  case 't':
    iVar2 = strncmp(pcVar1,"true",4);
    if (iVar2 == 0) {
      item->type_flags = 2;
      item->value_int = 1;
      return pcVar1 + 4;
    }
    break;
  case '{':
    pcVar1 = parse_object(item,pcVar1);
    return pcVar1;
  }
  _ep = pcVar1;
  return (char *)0x0;
}

/*
 * _parse_array @ 0x00029d94..0x00029e94
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in bundled_cjson: parse array. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): char * _parse_array(RecoveredCJSON *item,char *text)
 * Direct callers: _parse_value@0x00029ad4
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00029d94_parse_array.s
 */
char * parse_array(RecoveredCJSON *item,char *text)

{
  char *pcVar1;
  RecoveredCJSON *item_00;
  RecoveredCJSON *item_01;
  
  pcVar1 = skip(text + 1);
  if (*pcVar1 == ']') {
    item->type_flags = 6;
    pcVar1 = pcVar1 + 1;
  }
  else {
    item_00 = cJSON_New_Item();
    item->child = item_00;
    if (item_00 != (RecoveredCJSON *)0x0) {
      pcVar1 = skip(pcVar1);
      pcVar1 = parse_value(item_00,pcVar1);
      pcVar1 = skip(pcVar1);
      if (pcVar1 != (char *)0x0) {
        item->type_flags = 6;
        do {
          if (*pcVar1 != ',') {
            if (*pcVar1 == ']') {
              return pcVar1 + 1;
            }
            _ep = pcVar1;
            return (char *)0x0;
          }
          item_01 = cJSON_New_Item();
          if (item_01 == (RecoveredCJSON *)0x0) break;
          item_00->next = item_01;
          item_01->previous = item_00;
          pcVar1 = skip(pcVar1 + 1);
          pcVar1 = parse_value(item_01,pcVar1);
          pcVar1 = skip(pcVar1);
          item_00 = item_01;
        } while (pcVar1 != (char *)0x0);
      }
    }
    pcVar1 = (char *)0x0;
  }
  return pcVar1;
}

/*
 * _parse_object @ 0x00029e94..0x0002a02c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in bundled_cjson: parse object. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): char * _parse_object(RecoveredCJSON *item,char *text)
 * Direct callers: _parse_value@0x00029ad4
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00029e94_parse_object.s
 */
char * parse_object(RecoveredCJSON *item,char *text)

{
  char *pcVar1;
  RecoveredCJSON *item_00;
  RecoveredCJSON *item_01;
  
  pcVar1 = skip(text + 1);
  if (*pcVar1 == '}') {
    item->type_flags = 7;
    pcVar1 = pcVar1 + 1;
  }
  else {
    item_00 = cJSON_New_Item();
    item->child = item_00;
    if (item_00 != (RecoveredCJSON *)0x0) {
      pcVar1 = skip(pcVar1);
      pcVar1 = parse_string(item_00,pcVar1);
      pcVar1 = skip(pcVar1);
      if (pcVar1 != (char *)0x0) {
        item_00->member_name = item_00->value_string;
        item_00->value_string = (char *)0x0;
        if (*pcVar1 != ':') {
          _ep = pcVar1;
          return (char *)0x0;
        }
        pcVar1 = skip(pcVar1 + 1);
        pcVar1 = parse_value(item_00,pcVar1);
        pcVar1 = skip(pcVar1);
        if (pcVar1 != (char *)0x0) {
          item->type_flags = 7;
          do {
            if (*pcVar1 != ',') {
              if (*pcVar1 == '}') {
                return pcVar1 + 1;
              }
              _ep = pcVar1;
              return (char *)0x0;
            }
            item_01 = cJSON_New_Item();
            if (item_01 == (RecoveredCJSON *)0x0) break;
            item_00->next = item_01;
            item_01->previous = item_00;
            pcVar1 = skip(pcVar1 + 1);
            pcVar1 = parse_string(item_01,pcVar1);
            pcVar1 = skip(pcVar1);
            if (pcVar1 == (char *)0x0) break;
            item_01->member_name = item_01->value_string;
            item_01->value_string = (char *)0x0;
            if (*pcVar1 != ':') {
              _ep = pcVar1;
              return (char *)0x0;
            }
            pcVar1 = skip(pcVar1 + 1);
            pcVar1 = parse_value(item_01,pcVar1);
            pcVar1 = skip(pcVar1);
            item_00 = item_01;
          } while (pcVar1 != (char *)0x0);
        }
      }
    }
    pcVar1 = (char *)0x0;
  }
  return pcVar1;
}

/*
 * _cJSON_Parse @ 0x0002a02c..0x0002a098
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Allocate and recursively parse a JSON value tree using the bundled implementation.
 * Parameters/return (inferred): RecoveredCJSON * _cJSON_Parse(char *text)
 * Direct callers: _show_simple_http_error@0x0000315c, _session_load@0x00007cb8, _do_async_fetch@0x000087a8, _provider_parse_any@0x0000983c, _provider_parse_tool_calls@0x00009f28, _ollama_sync_model_metadata@0x0000bb9c, _ollama_sse_cb@0x0000c3a0, _openai_build_tools_array@0x0000ccb8, _openai_sse_cb@0x0000e388, _anthropic_build_tools_array@0x0000e7a4, _anthropic_sse_cb@0x0000fad0, _google_build_tools@0x00010768, _google_sse_cb@0x000119fc, _read_result_json@0x00013140, _sa_child_main@0x0001327c, _worker_child_main@0x00022774, _worker_deserialize_out@0x00024b1c, _parse_embedding_json@0x00027dfc, _mcp_discover_server@0x0002b980, _mcp_get_tools_json@0x0002c004, _tool_execute_mcp@0x0002c0c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002a02c_cJSON_Parse.s
 */
RecoveredCJSON * cJSON_Parse(char *text)

{
  RecoveredCJSON *item;
  char *pcVar1;
  
  if ((text != (char *)0x0) && (item = cJSON_New_Item(), item != (RecoveredCJSON *)0x0)) {
    pcVar1 = skip(text);
    pcVar1 = parse_value(item,pcVar1);
    if (pcVar1 != (char *)0x0) {
      return item;
    }
    cJSON_Delete(item);
    return (RecoveredCJSON *)0x0;
  }
  return (RecoveredCJSON *)0x0;
}

/*
 * _print_string_ptr @ 0x0002a098..0x0002a320
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in bundled_cjson: print string ptr. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): char * _print_string_ptr(char *text)
 * Direct callers: _print_value@0x0002a320, _print_object@0x0002a6d0
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002a098_print_string_ptr.s
 */
char * print_string_ptr(char *text)

{
  byte bVar1;
  bool bVar2;
  int iVar3;
  byte *pbVar4;
  char *pcVar5;
  int iVar6;
  uint uVar7;
  uint uVar8;
  byte *pbVar9;
  
  if (text == (char *)0x0) {
    pcVar5 = cJSON_strdup("");
    return pcVar5;
  }
  uVar7 = 3;
  uVar8 = (uint)*text;
  if (uVar8 != 0) {
    bVar2 = false;
    uVar7 = uVar8;
    iVar3 = 0;
    do {
      iVar6 = iVar3;
      if ((((uVar7 & 0xff) < 0x20) || (uVar7 == 0x22)) || (uVar7 == 0x5c)) {
        bVar2 = true;
      }
      uVar7 = (uint)text[iVar6 + 1];
      iVar3 = iVar6 + 1;
    } while (uVar7 != 0);
    if (bVar2) {
      uVar7 = iVar6 + 4;
      pcVar5 = text;
      do {
        if ((int)uVar8 < 0xe) {
          if (((int)uVar8 < 0xc) && (2 < uVar8 - 8)) {
LAB_0002a18c:
            if ((uVar8 & 0xff) < 0x20) {
              uVar7 = uVar7 + 6;
            }
          }
          else {
            uVar7 = uVar7 + 2;
          }
        }
        else {
          if ((uVar8 != 0x22) && (uVar8 != 0x5c)) goto LAB_0002a18c;
          uVar7 = uVar7 + 2;
        }
        pcVar5 = pcVar5 + 1;
        uVar8 = (uint)*pcVar5;
      } while (uVar8 != 0);
      pcVar5 = cJSON_malloc(uVar7);
      if (pcVar5 == (char *)0x0) {
        return (char *)0x0;
      }
      *pcVar5 = '\"';
      pbVar9 = (byte *)(pcVar5 + 1);
      bVar1 = *text;
      do {
        if (bVar1 == 0) {
          *pbVar9 = 0x22;
          pbVar9[1] = 0;
          return pcVar5;
        }
        if (((bVar1 < 0x20) || (bVar1 == 0x22)) || (bVar1 == 0x5c)) {
          *pbVar9 = 0x5c;
          pbVar4 = pbVar9 + 1;
          bVar1 = *text;
          if (bVar1 == 0xc) {
            *pbVar4 = 0x66;
            pbVar9 = pbVar9 + 2;
          }
          else if ((char)bVar1 < '\r') {
            if (bVar1 == 9) {
              *pbVar4 = 0x74;
              pbVar9 = pbVar9 + 2;
            }
            else if (bVar1 == 10) {
              *pbVar4 = 0x6e;
              pbVar9 = pbVar9 + 2;
            }
            else if (bVar1 == 8) {
              *pbVar4 = 0x62;
              pbVar9 = pbVar9 + 2;
            }
            else {
LAB_0002a2b4:
              sprintf((char *)(pbVar9 + 2),"u%04x",(uint)bVar1);
              pbVar9 = pbVar9 + 6;
              *pbVar9 = 0x5c;
            }
          }
          else if (bVar1 == 0x22) {
            *pbVar4 = 0x22;
            pbVar9 = pbVar9 + 2;
          }
          else if (bVar1 == 0x5c) {
            *pbVar4 = 0x5c;
            pbVar9 = pbVar9 + 2;
          }
          else {
            if (bVar1 != 0xd) goto LAB_0002a2b4;
            *pbVar4 = 0x72;
            pbVar9 = pbVar9 + 2;
          }
        }
        else {
          *pbVar9 = bVar1;
          pbVar9 = pbVar9 + 1;
        }
        text = text + 1;
        bVar1 = *text;
      } while( true );
    }
    uVar7 = iVar6 + 4;
  }
  pcVar5 = cJSON_malloc(uVar7);
  if (pcVar5 == (char *)0x0) {
    return (char *)0x0;
  }
  *pcVar5 = '\"';
  strcpy(pcVar5 + 1,text);
  uVar7 = strlen(pcVar5);
  (pcVar5 + uVar7)[0] = '\"';
  (pcVar5 + uVar7)[1] = '\0';
  return pcVar5;
}

/*
 * _print_value @ 0x0002a320..0x0002a4bc
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in bundled_cjson: print value. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): char * _print_value(RecoveredCJSON *item,int depth,int formatted)
 * Direct callers: _print_array@0x0002a4bc, _print_object@0x0002a6d0, _cJSON_Print@0x0002aa84, _cJSON_PrintUnformatted@0x0002aaac
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/0002a320_print_value.s
 */
char * print_value(RecoveredCJSON *item,int depth,int formatted)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x30] __attribute__((aligned(16)));
#define local_20 (*((undefined4 *)(app_frame + 0x0))) /* original SP - 0x20 */
#define uStack_1c (*((undefined4 *)(app_frame + 0x4))) /* original SP - 0x1c */

  char *output;
  double dVar1;


  if (item == (RecoveredCJSON *)0x0) {
switchD_0002a364_caseD_0:
    output = (char *)0x0;
  }
  else {
    switch(item->type_flags) {
    default:
      goto switchD_0002a364_caseD_0;
    case 1:
      output = cJSON_strdup("false");
      break;
    case 2:
      output = cJSON_strdup("true");
      break;
    case 3:
      output = cJSON_strdup("null");
      break;
    case 4:
      output = cJSON_malloc(0x40);
      if (output != (char *)0x0) {
        dVar1 = item->value_double;
        if (((2.220446049250313e-16 <
              ABS((app_bits_double(CONCAT44(0x43300000,item->value_int ^ 0x80000000)) - 4503601774854144.0) -
                  dVar1)) || (2147483647.0 < dVar1)) || (dVar1 < -2147483648.0)) {
          local_20 = (undefined4)(APP_BITS(dVar1) >> 0x20);
          uStack_1c = SUB84(dVar1,0);
          sprintf(output,"%.17g",dVar1);
        }
        else {
          sprintf(output,"%d",item->value_int);
        }
      }
      break;
    case 5:
      output = print_string_ptr(item->value_string);
      break;
    case 6:
      output = print_array(item,depth,formatted);
      break;
    case 7:
      output = print_object(item,depth,formatted);
    }
  }
  return output;

#undef local_20
#undef uStack_1c
}

/*
 * _print_array @ 0x0002a4bc..0x0002a6d0
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in bundled_cjson: print array. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): char * _print_array(RecoveredCJSON *item,int depth,int formatted)
 * Direct callers: _print_value@0x0002a320
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002a4bc_print_array.s
 */
char * print_array(RecoveredCJSON *item,int depth,int formatted)

{
  RecoveredCJSON *pRVar1;
  char *output;
  char *pcVar2;
  undefined4 *output_00;
  uint uVar3;
  uint uVar4;
  int iVar5;
  undefined4 *puVar6;
  char *pcVar7;
  int iVar8;
  int iVar9;
  byte bVar10;
  
  pRVar1 = item->child;
  if (pRVar1 != (RecoveredCJSON *)0x0) {
    iVar9 = 0;
    do {
      iVar8 = iVar9;
      iVar9 = iVar8 + 1;
      pRVar1 = pRVar1->next;
    } while (pRVar1 != (RecoveredCJSON *)0x0);
    if (iVar9 != 0) {
      output_00 = cJSON_malloc(iVar9 * 4);
      if (output_00 == (undefined4 *)0x0) {
        return (char *)0x0;
      }
      memset(output_00,0,iVar9 * 4);
      pRVar1 = item->child;
      uVar3 = 5;
      if (pRVar1 != (RecoveredCJSON *)0x0) {
        uVar3 = 5;
        puVar6 = output_00;
        do {
          pcVar2 = print_value(pRVar1,depth + 1,formatted);
          *puVar6 = (undefined4)(pcVar2);
          if (pcVar2 != (char *)0x0) {
            uVar4 = strlen(pcVar2);
            uVar3 = uVar4 + (formatted != 0) + uVar3 + 2;
          }
          pRVar1 = pRVar1->next;
          puVar6 = puVar6 + 1;
        } while (pRVar1 != (RecoveredCJSON *)0x0);
      }
      pcVar2 = cJSON_malloc(uVar3);
      if (pcVar2 != (char *)0x0) {
        *pcVar2 = '[';
        pcVar7 = pcVar2 + 1;
        if (0 < iVar9) {
          iVar5 = 0;
          bVar10 = (formatted == 0) << 1;
          puVar6 = output_00;
          output = pcVar7;
          do {
            if ((char *)*puVar6 != (char *)0x0) {
              strcpy(output,(char *)*puVar6);
              uVar3 = strlen((char *)*puVar6);
              output = output + uVar3;
            }
            pcVar7 = output;
            if (iVar5 != iVar8) {
              *output = ',';
              pcVar7 = output + 1;
              if (!(bool)(bVar10 >> 1 & 1)) {
                output[1] = ' ';
                pcVar7 = output + 2;
              }
            }
            cJSON_free((void *)*puVar6);
            iVar5 = iVar5 + 1;
            puVar6 = puVar6 + 1;
            output = pcVar7;
          } while (iVar5 != iVar9);
        }
        cJSON_free(output_00);
        *pcVar7 = ']';
        pcVar7[1] = '\0';
        return pcVar2;
      }
      if (0 < iVar9) {
        iVar8 = 0;
        do {
          cJSON_free((void *)output_00[iVar8]);
          iVar8 = iVar8 + 1;
        } while (iVar8 != iVar9);
      }
      cJSON_free(output_00);
      return (char *)0x0;
    }
  }
  pcVar2 = cJSON_malloc(3);
  if (pcVar2 != (char *)0x0) {
    *pcVar2 = '[';
    pcVar2[1] = ']';
    pcVar2[2] = '\0';
  }
  return pcVar2;
}

/*
 * _print_object @ 0x0002a6d0..0x0002aa84
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in bundled_cjson: print object. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): char * _print_object(RecoveredCJSON *item,int depth,int formatted)
 * Direct callers: _print_value@0x0002a320
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002a6d0_print_object.s
 */
char * print_object(RecoveredCJSON *item,int depth,int formatted)

{
  int iVar1;
  RecoveredCJSON *pRVar2;
  char *pcVar3;
  uint uVar4;
  char *pcVar5;
  void *output;
  void *output_00;
  uint uVar6;
  int iVar7;
  int iVar8;
  int iVar9;
  int iVar10;
  char *output_01;
  byte bVar11;
  byte bVar12;
  
  pRVar2 = item->child;
  if (pRVar2 != (RecoveredCJSON *)0x0) {
    iVar8 = 0;
    do {
      iVar10 = iVar8;
      iVar8 = iVar10 + 1;
      pRVar2 = pRVar2->next;
    } while (pRVar2 != (RecoveredCJSON *)0x0);
    if (iVar8 != 0) {
      uVar4 = iVar8 * 4;
      output = cJSON_malloc(uVar4);
      output_00 = cJSON_malloc(uVar4);
      if (output != (void *)0x0) {
        if (output_00 != (void *)0x0) {
          memset(output,0,uVar4);
          memset(output_00,0,uVar4);
          pRVar2 = item->child;
          iVar7 = 0;
          if (pRVar2 != (RecoveredCJSON *)0x0) {
            iVar7 = 0;
            iVar9 = 0;
            bVar11 = (formatted == 0) << 1;
            do {
              pcVar5 = print_string_ptr(pRVar2->member_name);
              *(char **)((int)output + iVar9) = pcVar5;
              pcVar5 = print_value(pRVar2,depth + 1,formatted);
              *(char **)((int)output_00 + iVar9) = pcVar5;
              if ((*(char **)((int)output + iVar9) != (char *)0x0) && (pcVar5 != (char *)0x0)) {
                uVar4 = strlen(*(char **)((int)output + iVar9));
                uVar6 = strlen(pcVar5);
                if ((bool)(bVar11 >> 1 & 1)) {
                  iVar1 = 0;
                }
                else {
                  iVar1 = depth + 2;
                }
                iVar7 = iVar1 + uVar4 + uVar6 + iVar7 + 2;
              }
              pRVar2 = pRVar2->next;
              iVar9 = iVar9 + 4;
            } while (pRVar2 != (RecoveredCJSON *)0x0);
          }
          iVar9 = 3;
          if (formatted != 0) {
            iVar9 = depth + 4;
          }
          pcVar5 = cJSON_malloc(iVar9 + iVar7);
          if (pcVar5 != (char *)0x0) {
            *pcVar5 = '{';
            pcVar3 = pcVar5 + 1;
            if (formatted != 0) {
              pcVar5[1] = '\n';
              pcVar3 = pcVar5 + 2;
            }
            if (0 < iVar8) {
              iVar7 = 0;
              iVar9 = 0;
              bVar12 = (formatted == 0) << 1;
              bVar11 = (0 < depth) << 2;
              do {
                if ((!(bool)(bVar12 >> 1 & 1)) && (iVar1 = depth, (bool)(bVar11 >> 2 & 1))) {
                  do {
                    *pcVar3 = '\t';
                    pcVar3 = pcVar3 + 1;
                    iVar1 = iVar1 + -1;
                  } while (iVar1 != 0);
                }
                strcpy(pcVar3,*(char **)((int)output + iVar9));
                uVar4 = strlen(*(char **)((int)output + iVar9));
                pcVar3[uVar4] = ':';
                iVar1 = uVar4 + 1;
                if (!(bool)(bVar12 >> 1 & 1)) {
                  pcVar3[uVar4 + 1] = '\t';
                  iVar1 = uVar4 + 2;
                }
                output_01 = pcVar3 + iVar1;
                strcpy(output_01,*(char **)((int)output_00 + iVar9));
                uVar4 = strlen(*(char **)((int)output_00 + iVar9));
                pcVar3 = output_01 + uVar4;
                if (iVar7 != iVar10) {
                  output_01[uVar4] = ',';
                  pcVar3 = pcVar3 + 1;
                }
                if (!(bool)(bVar12 >> 1 & 1)) {
                  *pcVar3 = '\n';
                  pcVar3 = pcVar3 + 1;
                }
                iVar7 = iVar7 + 1;
                iVar9 = iVar9 + 4;
              } while (iVar7 != iVar8);
            }
            if ((formatted != 0) && (iVar10 = depth + -1, 0 < iVar10)) {
              do {
                *pcVar3 = '\t';
                pcVar3 = pcVar3 + 1;
                iVar10 = iVar10 + -1;
              } while (iVar10 != 0);
            }
            *pcVar3 = '}';
            pcVar3[1] = '\0';
            if (0 < iVar8) {
              iVar10 = 0;
              iVar7 = 0;
              do {
                cJSON_free(*(void **)(iVar7 + (int)output));
                cJSON_free(*(void **)(iVar7 + (int)output_00));
                iVar10 = iVar10 + 1;
                iVar7 = iVar7 + 4;
              } while (iVar10 != iVar8);
            }
            cJSON_free(output);
            cJSON_free(output_00);
            return pcVar5;
          }
        }
        if (0 < iVar8) {
          iVar10 = 0;
          do {
            cJSON_free(*(void **)((int)output + iVar10 * 4));
            iVar10 = iVar10 + 1;
          } while (iVar10 != iVar8);
        }
        cJSON_free(output);
      }
      if (output_00 == (void *)0x0) {
        return (char *)0x0;
      }
      if (0 < iVar8) {
        iVar10 = 0;
        do {
          cJSON_free(*(void **)((int)output_00 + iVar10 * 4));
          iVar10 = iVar10 + 1;
        } while (iVar10 != iVar8);
      }
      cJSON_free(output_00);
      return (char *)0x0;
    }
  }
  uVar4 = 3;
  if (formatted != 0) {
    uVar4 = depth + 4;
  }
  pcVar5 = cJSON_malloc(uVar4);
  if (pcVar5 == (char *)0x0) {
    return (char *)0x0;
  }
  *pcVar5 = '{';
  pcVar3 = pcVar5 + 1;
  if (formatted != 0) {
    pcVar5[1] = '\n';
    pcVar3 = pcVar5 + 2;
    iVar8 = depth + -1;
    if (0 < iVar8) {
      do {
        *pcVar3 = '\t';
        pcVar3 = pcVar3 + 1;
        iVar8 = iVar8 + -1;
      } while (iVar8 != 0);
    }
  }
  *pcVar3 = '}';
  pcVar3[1] = '\0';
  return pcVar5;
}

/*
 * _cJSON_Print @ 0x0002aa84..0x0002aaac
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: Print. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): char * _cJSON_Print(RecoveredCJSON *item)
 * Direct callers: _mcp_discover_server@0x0002b980, _tool_execute_mcp@0x0002c0c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002aa84_cJSON_Print.s
 */
char * cJSON_Print(RecoveredCJSON *item)

{
  char *pcVar1;
  
  pcVar1 = print_value(item,0,1);
  return pcVar1;
}

/*
 * _cJSON_PrintUnformatted @ 0x0002aaac..0x0002aad4
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: PrintUnformatted. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): char * _cJSON_PrintUnformatted(RecoveredCJSON *item)
 * Direct callers: _do_async_fetch@0x000087a8, _ollama_sync_model_metadata@0x0000bb9c, _ollama_send@0x0000c70c, _openai_send@0x0000e5ec, _anthropic_send@0x00010540, _google_send@0x00012238, _write_in_json@0x00013000, _sa_child_main@0x0001327c, _child_write_out_json@0x00022270, _worker_serialize_in@0x00024740
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002aaac_cJSON_PrintUnformatted.s
 */
char * cJSON_PrintUnformatted(RecoveredCJSON *item)

{
  char *pcVar1;
  
  pcVar1 = print_value(item,0,0);
  return pcVar1;
}

/*
 * _cJSON_GetObjectItem @ 0x0002aad4..0x0002ab3c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: GetObjectItem. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): RecoveredCJSON * _cJSON_GetObjectItem(RecoveredCJSON *object,char *name)
 * Direct callers: _show_simple_http_error@0x0000315c, _session_load@0x00007cb8, _do_async_fetch@0x000087a8, _provider_parse_any@0x0000983c, _provider_parse_tool_calls_args@0x000098bc, _provider_parse_tool_calls@0x00009f28, _ollama_sync_model_metadata@0x0000bb9c, _ollama_sse_cb@0x0000c3a0, _add_required@0x0000ca7c, _add_tool@0x0000cbd0, _openai_sse_cb@0x0000e388, _anthropic_sse_cb@0x0000fad0, _google_sse_cb@0x000119fc, _read_result_json@0x00013140, _sa_child_main@0x0001327c, _worker_child_main@0x00022774, _worker_deserialize_out@0x00024b1c, _parse_embedding_json@0x00027dfc, _cJSON_HasObjectItem@0x0002afb0, _mcp_discover_server@0x0002b980, _tool_execute_mcp@0x0002c0c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002aad4_cJSON_GetObjectItem.s
 */
RecoveredCJSON * cJSON_GetObjectItem(RecoveredCJSON *object,char *name)

{
  int iVar1;
  RecoveredCJSON *pRVar2;
  
  pRVar2 = (RecoveredCJSON *)0x0;
  if (object != (RecoveredCJSON *)0x0) {
    pRVar2 = object->child;
    while ((pRVar2 != (RecoveredCJSON *)0x0 &&
           (iVar1 = cJSON_strcasecmp(pRVar2->member_name,name), iVar1 != 0))) {
      pRVar2 = pRVar2->next;
    }
  }
  return pRVar2;
}

/*
 * _cJSON_GetArrayItem @ 0x0002ab3c..0x0002ab78
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: GetArrayItem. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): RecoveredCJSON * _cJSON_GetArrayItem(RecoveredCJSON *array,int index)
 * Direct callers: _session_load@0x00007cb8, _do_async_fetch@0x000087a8, _provider_parse_tool_calls@0x00009f28, _ollama_sse_cb@0x0000c3a0, _openai_sse_cb@0x0000e388, _google_sse_cb@0x000119fc, _worker_child_main@0x00022774, _worker_deserialize_out@0x00024b1c, _parse_embedding_json@0x00027dfc, _mcp_discover_server@0x0002b980, _tool_execute_mcp@0x0002c0c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002ab3c_cJSON_GetArrayItem.s
 */
RecoveredCJSON * cJSON_GetArrayItem(RecoveredCJSON *array,int index)

{
  RecoveredCJSON *pRVar1;
  
  pRVar1 = (RecoveredCJSON *)0x0;
  if (((array != (RecoveredCJSON *)0x0) && (pRVar1 = array->child, pRVar1 != (RecoveredCJSON *)0x0))
     && (0 < index)) {
    do {
      pRVar1 = pRVar1->next;
      if (pRVar1 == (RecoveredCJSON *)0x0) {
        return (RecoveredCJSON *)0x0;
      }
      index = index + -1;
    } while (index != 0);
  }
  return pRVar1;
}

/*
 * _cJSON_GetArraySize @ 0x0002ab78..0x0002abb0
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: GetArraySize. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): int _cJSON_GetArraySize(RecoveredCJSON *array)
 * Direct callers: _session_load@0x00007cb8, _do_async_fetch@0x000087a8, _provider_parse_tool_calls@0x00009f28, _ollama_sse_cb@0x0000c3a0, _google_sse_cb@0x000119fc, _worker_child_main@0x00022774, _worker_deserialize_out@0x00024b1c, _parse_embedding_json@0x00027dfc, _mcp_discover_server@0x0002b980, _tool_execute_mcp@0x0002c0c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002ab78_cJSON_GetArraySize.s
 */
int cJSON_GetArraySize(RecoveredCJSON *array)

{
  int iVar1;
  RecoveredCJSON *pRVar2;
  
  if ((array == (RecoveredCJSON *)0x0) || (pRVar2 = array->child, pRVar2 == (RecoveredCJSON *)0x0))
  {
    iVar1 = 0;
  }
  else {
    iVar1 = 0;
    do {
      iVar1 = iVar1 + 1;
      pRVar2 = pRVar2->next;
    } while (pRVar2 != (RecoveredCJSON *)0x0);
  }
  return iVar1;
}

/*
 * _cJSON_CreateObject @ 0x0002abb0..0x0002abe0
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: CreateObject. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): RecoveredCJSON * _cJSON_CreateObject(void)
 * Direct callers: _do_async_fetch@0x000087a8, _tools_schema_add_simple@0x0000a510, _tools_schema_add_read_and_edit@0x0000a68c, _tools_schema_add_fetch_url@0x0000a8ac, _tools_schema_add_db_query@0x0000aa20, _tools_schema_build_standard@0x0000acd4, _ollama_sync_model_metadata@0x0000bb9c, _ollama_build_request@0x0000bdf0, _add_str_prop@0x0000ca04, _make_tool@0x0000caf4, _openai_build_tools_array@0x0000ccb8, _openai_build_messages@0x0000dd28, _openai_send@0x0000e5ec, _anthropic_build_tools_array@0x0000e7a4, _anthropic_build_messages@0x0000f520, _anthropic_send@0x00010540, _google_build_tools@0x00010768, _google_build_contents@0x00011474, _google_send@0x00012238, _write_in_json@0x00013000, _sa_child_main@0x0001327c, _child_write_out_json@0x00022270, _worker_serialize_in@0x00024740
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002abb0_cJSON_CreateObject.s
 */
RecoveredCJSON * cJSON_CreateObject(void)

{
  RecoveredCJSON *pRVar1;
  
  pRVar1 = cJSON_New_Item();
  if (pRVar1 != (RecoveredCJSON *)0x0) {
    pRVar1->type_flags = 7;
  }
  return pRVar1;
}

/*
 * _cJSON_CreateArray @ 0x0002abe0..0x0002ac10
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: CreateArray. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): RecoveredCJSON * _cJSON_CreateArray(void)
 * Direct callers: _tools_schema_add_simple@0x0000a510, _tools_schema_add_read_and_edit@0x0000a68c, _tools_schema_add_fetch_url@0x0000a8ac, _tools_schema_add_db_query@0x0000aa20, _tools_schema_build_standard@0x0000acd4, _ollama_build_request@0x0000bdf0, _add_required@0x0000ca7c, _openai_build_tools_array@0x0000ccb8, _openai_build_messages@0x0000dd28, _anthropic_build_tools_array@0x0000e7a4, _anthropic_build_messages@0x0000f520, _google_build_tools@0x00010768, _google_build_contents@0x00011474, _child_write_out_json@0x00022270, _worker_serialize_in@0x00024740
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002abe0_cJSON_CreateArray.s
 */
RecoveredCJSON * cJSON_CreateArray(void)

{
  RecoveredCJSON *pRVar1;
  
  pRVar1 = cJSON_New_Item();
  if (pRVar1 != (RecoveredCJSON *)0x0) {
    pRVar1->type_flags = 6;
  }
  return pRVar1;
}

/*
 * _cJSON_CreateString @ 0x0002ac10..0x0002ac5c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: CreateString. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): RecoveredCJSON * _cJSON_CreateString(char *value)
 * Direct callers: _tools_schema_add_simple@0x0000a510, _tools_schema_add_read_and_edit@0x0000a68c, _tools_schema_add_fetch_url@0x0000a8ac, _tools_schema_add_db_query@0x0000aa20, _tools_schema_build_standard@0x0000acd4, _add_required@0x0000ca7c, _openai_build_tools_array@0x0000ccb8, _anthropic_build_tools_array@0x0000e7a4, _google_build_tools@0x00010768, _cJSON_AddStringToObject@0x0002ae1c
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002ac10_cJSON_CreateString.s
 */
RecoveredCJSON * cJSON_CreateString(char *value)

{
  RecoveredCJSON *pRVar1;
  char *pcVar2;
  
  pRVar1 = cJSON_New_Item();
  if (pRVar1 != (RecoveredCJSON *)0x0) {
    pRVar1->type_flags = 5;
    pcVar2 = cJSON_strdup(value);
    pRVar1->value_string = pcVar2;
  }
  return pRVar1;
}

/*
 * _cJSON_CreateNumber @ 0x0002ac5c..0x0002acac
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: CreateNumber. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): RecoveredCJSON * _cJSON_CreateNumber(double value)
 * Direct callers: _cJSON_AddNumberToObject@0x0002ae68
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/0002ac5c_cJSON_CreateNumber.s
 */
RecoveredCJSON * cJSON_CreateNumber(double value)

{
  RecoveredCJSON *pRVar1;
  
  pRVar1 = cJSON_New_Item();
  if (pRVar1 != (RecoveredCJSON *)0x0) {
    pRVar1->type_flags = 4;
    pRVar1->value_double = value;
    pRVar1->value_int = (int)value;
  }
  return pRVar1;
}

/*
 * _cJSON_CreateBool @ 0x0002acac..0x0002acf4
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: CreateBool. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): RecoveredCJSON * _cJSON_CreateBool(int value)
 * Direct callers: _cJSON_AddBoolToObject@0x0002aeb0
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002acac_cJSON_CreateBool.s
 */
RecoveredCJSON * cJSON_CreateBool(int value)

{
  RecoveredCJSON *pRVar1;
  
  pRVar1 = cJSON_New_Item();
  if (pRVar1 != (RecoveredCJSON *)0x0) {
    pRVar1->type_flags = 2 - (uint)(value == 0);
    pRVar1->value_int = value;
  }
  return pRVar1;
}

/*
 * _cJSON_CreateNull @ 0x0002acf4..0x0002ad24
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: CreateNull. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): RecoveredCJSON * _cJSON_CreateNull(void)
 * Direct callers: _cJSON_AddNullToObject@0x0002aefc
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002acf4_cJSON_CreateNull.s
 */
RecoveredCJSON * cJSON_CreateNull(void)

{
  RecoveredCJSON *pRVar1;
  
  pRVar1 = cJSON_New_Item();
  if (pRVar1 != (RecoveredCJSON *)0x0) {
    pRVar1->type_flags = 3;
  }
  return pRVar1;
}

/*
 * _cJSON_CreateStringReference @ 0x0002ad24..0x0002ad64
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: CreateStringReference. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): RecoveredCJSON * _cJSON_CreateStringReference(char *value)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002ad24_cJSON_CreateStringReference.s
 */
RecoveredCJSON * cJSON_CreateStringReference(char *value)

{
  RecoveredCJSON *pRVar1;
  
  pRVar1 = cJSON_New_Item();
  if (pRVar1 != (RecoveredCJSON *)0x0) {
    pRVar1->type_flags = 5;
    pRVar1->value_string = value;
  }
  return pRVar1;
}

/*
 * _cJSON_AddItemToArray @ 0x0002ad64..0x0002adb0
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: AddItemToArray. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _cJSON_AddItemToArray(RecoveredCJSON *array,RecoveredCJSON *item)
 * Direct callers: _tools_schema_add_simple@0x0000a510, _tools_schema_add_read_and_edit@0x0000a68c, _tools_schema_add_fetch_url@0x0000a8ac, _tools_schema_add_db_query@0x0000aa20, _tools_schema_build_standard@0x0000acd4, _ollama_build_request@0x0000bdf0, _add_required@0x0000ca7c, _add_tool@0x0000cbd0, _openai_build_tools_array@0x0000ccb8, _openai_build_messages@0x0000dd28, _anthropic_build_tools_array@0x0000e7a4, _anthropic_build_messages@0x0000f520, _google_build_tools@0x00010768, _google_build_contents@0x00011474, _child_write_out_json@0x00022270, _worker_serialize_in@0x00024740, _cJSON_AddItemToObject@0x0002adb0, _cJSON_AddItemReferenceToObject@0x0002af44
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002ad64_cJSON_AddItemToArray.s
 */
void cJSON_AddItemToArray(RecoveredCJSON *array,RecoveredCJSON *item)

{
  RecoveredCJSON *pRVar1;
  RecoveredCJSON *pRVar2;
  
  if (array == (RecoveredCJSON *)0x0) {
    return;
  }
  if (item != (RecoveredCJSON *)0x0) {
    item->next = (RecoveredCJSON *)0x0;
    item->previous = (RecoveredCJSON *)0x0;
    pRVar1 = array->child;
    if (array->child != (RecoveredCJSON *)0x0) {
      do {
        pRVar2 = pRVar1;
        pRVar1 = pRVar2->next;
      } while (pRVar2->next != (RecoveredCJSON *)0x0);
      pRVar2->next = item;
      item->previous = pRVar2;
      return;
    }
    array->child = item;
    return;
  }
  return;
}

/*
 * _cJSON_AddItemToObject @ 0x0002adb0..0x0002ae1c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: AddItemToObject. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _cJSON_AddItemToObject(RecoveredCJSON *object,char *name,RecoveredCJSON *item)
 * Direct callers: _tools_schema_add_simple@0x0000a510, _tools_schema_add_read_and_edit@0x0000a68c, _tools_schema_add_fetch_url@0x0000a8ac, _tools_schema_add_db_query@0x0000aa20, _tools_schema_build_standard@0x0000acd4, _ollama_build_request@0x0000bdf0, _add_str_prop@0x0000ca04, _add_required@0x0000ca7c, _make_tool@0x0000caf4, _openai_build_tools_array@0x0000ccb8, _openai_build_messages@0x0000dd28, _openai_send@0x0000e5ec, _anthropic_build_tools_array@0x0000e7a4, _anthropic_build_messages@0x0000f520, _anthropic_send@0x00010540, _google_build_tools@0x00010768, _google_build_contents@0x00011474, _google_send@0x00012238, _write_in_json@0x00013000, _child_write_out_json@0x00022270, _worker_serialize_in@0x00024740, _cJSON_AddStringToObject@0x0002ae1c, _cJSON_AddNumberToObject@0x0002ae68, _cJSON_AddBoolToObject@0x0002aeb0, _cJSON_AddNullToObject@0x0002aefc
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002adb0_cJSON_AddItemToObject.s
 */
void cJSON_AddItemToObject(RecoveredCJSON *object,char *name,RecoveredCJSON *item)

{
  char *pcVar1;
  
  if (((object != (RecoveredCJSON *)0x0) && (name != (char *)0x0)) &&
     (item != (RecoveredCJSON *)0x0)) {
    if (item->member_name != (char *)0x0) {
      cJSON_free(item->member_name);
    }
    pcVar1 = cJSON_strdup(name);
    item->member_name = pcVar1;
    cJSON_AddItemToArray(object,item);
  }
  return;
}

/*
 * _cJSON_AddStringToObject @ 0x0002ae1c..0x0002ae68
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: AddStringToObject. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _cJSON_AddStringToObject(RecoveredCJSON *object,char *name,char *value)
 * Direct callers: _do_async_fetch@0x000087a8, _tools_schema_add_simple@0x0000a510, _tools_schema_add_read_and_edit@0x0000a68c, _tools_schema_add_fetch_url@0x0000a8ac, _tools_schema_add_db_query@0x0000aa20, _tools_schema_build_standard@0x0000acd4, _ollama_sync_model_metadata@0x0000bb9c, _ollama_build_request@0x0000bdf0, _add_str_prop@0x0000ca04, _make_tool@0x0000caf4, _openai_build_tools_array@0x0000ccb8, _openai_build_messages@0x0000dd28, _openai_send@0x0000e5ec, _anthropic_build_tools_array@0x0000e7a4, _anthropic_build_messages@0x0000f520, _anthropic_send@0x00010540, _google_build_tools@0x00010768, _google_build_contents@0x00011474, _write_in_json@0x00013000, _sa_child_main@0x0001327c, _child_write_out_json@0x00022270, _worker_serialize_in@0x00024740
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002ae1c_cJSON_AddStringToObject.s
 */
void cJSON_AddStringToObject(RecoveredCJSON *object,char *name,char *value)

{
  RecoveredCJSON *item;
  
  item = cJSON_CreateString(value);
  cJSON_AddItemToObject(object,name,item);
  return;
}

/*
 * _cJSON_AddNumberToObject @ 0x0002ae68..0x0002aeb0
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: AddNumberToObject. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _cJSON_AddNumberToObject(RecoveredCJSON *object,char *name,double value)
 * Direct callers: _ollama_build_request@0x0000bdf0, _openai_build_messages@0x0000dd28, _openai_send@0x0000e5ec, _anthropic_send@0x00010540, _google_send@0x00012238, _write_in_json@0x00013000, _child_write_out_json@0x00022270, _worker_serialize_in@0x00024740
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/0002ae68_cJSON_AddNumberToObject.s
 */
void cJSON_AddNumberToObject(RecoveredCJSON *object,char *name,double value)

{
  RecoveredCJSON *item;
  
  item = cJSON_CreateNumber(value);
  cJSON_AddItemToObject(object,name,item);
  return;
}

/*
 * _cJSON_AddBoolToObject @ 0x0002aeb0..0x0002aefc
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: AddBoolToObject. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _cJSON_AddBoolToObject(RecoveredCJSON *object,char *name,int value)
 * Direct callers: _ollama_build_request@0x0000bdf0, _openai_send@0x0000e5ec, _anthropic_send@0x00010540
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002aeb0_cJSON_AddBoolToObject.s
 */
void cJSON_AddBoolToObject(RecoveredCJSON *object,char *name,int value)

{
  RecoveredCJSON *item;
  
  item = cJSON_CreateBool(value);
  cJSON_AddItemToObject(object,name,item);
  return;
}

/*
 * _cJSON_AddNullToObject @ 0x0002aefc..0x0002af44
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: AddNullToObject. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _cJSON_AddNullToObject(RecoveredCJSON *object,char *name)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002aefc_cJSON_AddNullToObject.s
 */
void cJSON_AddNullToObject(RecoveredCJSON *object,char *name)

{
  RecoveredCJSON *item;
  
  item = cJSON_CreateNull();
  cJSON_AddItemToObject(object,name,item);
  return;
}

/*
 * _cJSON_AddItemReferenceToObject @ 0x0002af44..0x0002afb0
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: AddItemReferenceToObject. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _cJSON_AddItemReferenceToObject(RecoveredCJSON *object,char *name,RecoveredCJSON *item)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002af44_cJSON_AddItemReferenceToObject.s
 */
void cJSON_AddItemReferenceToObject(RecoveredCJSON *object,char *name,RecoveredCJSON *item)

{
  char *pcVar1;
  
  if (((object != (RecoveredCJSON *)0x0) && (name != (char *)0x0)) &&
     (item != (RecoveredCJSON *)0x0)) {
    if (item->member_name != (char *)0x0) {
      cJSON_free(item->member_name);
    }
    pcVar1 = cJSON_strdup(name);
    item->member_name = pcVar1;
    cJSON_AddItemToArray(object,item);
  }
  return;
}

/*
 * _cJSON_HasObjectItem @ 0x0002afb0..0x0002afdc
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Bundled JSON-tree helper: HasObjectItem. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): int _cJSON_HasObjectItem(RecoveredCJSON *object,char *name)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002afb0_cJSON_HasObjectItem.s
 */
int cJSON_HasObjectItem(RecoveredCJSON *object,char *name)

{
  RecoveredCJSON *pRVar1;
  
  pRVar1 = cJSON_GetObjectItem(object,name);
  return (uint)(pRVar1 != (RecoveredCJSON *)0x0);
}

