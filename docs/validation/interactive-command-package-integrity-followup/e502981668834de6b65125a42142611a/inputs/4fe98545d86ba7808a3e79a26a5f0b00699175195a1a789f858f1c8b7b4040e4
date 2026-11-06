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
 * _get_home @ 0x00008288..0x000082ec
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Resolve a home-directory value for this translation unit; inspect this body because fallback behavior differs between helpers with the same symbol name.
 * Parameters/return (inferred): char * _get_home(void)
 * Direct callers: _get_model_cache_path@0x000082ec
 * Possible side effects (direct-call closure, incomplete for indirect calls): reads process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00008288_get_home.s
 */
char * models_profiles_get_home(void)

{
  char *pcVar1;
  int iVar2;
  
  pcVar1 = getenv("HOME");
  if (pcVar1 == (char *)0x0) {
    iVar2 = (int)getpwuid(getuid());
    if (iVar2 == 0) goto LAB_000082d8;
    pcVar1 = *(char **)(iVar2 + 0x1c);
  }
  if (pcVar1 != (char *)0x0) {
    return pcVar1;
  }
LAB_000082d8:
  return ".";
}

/*
 * _get_model_cache_path @ 0x000082ec..0x00008340
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Build the resolved-home/.ppccode_models cache pathname.
 * Parameters/return (inferred): undefined * _get_model_cache_path(void)
 * Direct callers: _model_cache_load@0x00008340, _model_cache_save@0x00008540
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; reads process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000082ec_get_model_cache_path.s
 */
undefined * get_model_cache_path(void)

{
  undefined4 uVar1;
  
  uVar1 = (undefined4)(models_profiles_get_home());
  snprintf(&_path_3670,0x400,"%s/.ppccode_models",(char *)(uVar1));
  return (undefined *)(&_path_3670);
}

/*
 * _model_cache_load @ 0x00008340..0x00008540
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Parse persisted provider/model metadata into the model-cache state.
 * Parameters/return (inferred): void _model_cache_load(void)
 * Direct callers: _config_load@0x00005fb8, _ollama_refresh_poll@0x00008c00
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; opens/creates/truncates filesystem paths according to the supplied mode; reads bytes from a stream; reads process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00008340_model_cache_load.s
 */
void model_cache_load(void)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x50] __attribute__((aligned(16)));
#define auStack_38 (*((undefined1 (*)[8])(app_frame + 0x8))) /* original SP - 0x38 */
#define local_30 (*((uint *)(app_frame + 0x10))) /* original SP - 0x30 */
#define local_2c (*((int (*)[4])(app_frame + 0x14))) /* original SP - 0x2c */

  RecoveredModelCache *pRVar1;
  RecoveredModelCache *pRVar2;
  char *pcVar3;
  void *stream;
  uint uVar4;
  int iVar5;
  time_t tVar6;
  int *output;

  pcVar3 = (char *)get_model_cache_path();
  stream = fopen(pcVar3,"rb");
  if (stream != (void *)0x0) {
    uVar4 = fread(auStack_38,1,7,stream);
    if ((uVar4 == 7) && (iVar5 = memcmp(auStack_38,"CACHE01",7), iVar5 == 0)) {
      local_30 = 0;
      uVar4 = fread(&local_30,4,1,stream);
      pRVar1 = PTR__g_model_cache_000396ac;
      if (uVar4 == 1) {
        if (local_30 < 0x41) {
          PTR__g_model_cache_000396ac->count = local_30;
          pRVar2 = PTR__g_model_cache_000396ac;
          if (0 < (int)local_30) {
            iVar5 = 0;
            pcVar3 = pRVar1->models[0].name + 0x3f;
            do {
              uVar4 = fread(pRVar2->models + iVar5,1,0x40,stream);
              if (uVar4 != 0x40) {
LAB_000084a0:
                if (iVar5 < (int)local_30) {
                  PTR__g_model_cache_000396ac->count = iVar5;
                }
                break;
              }
              output = &pRVar2->models[iVar5].context_window;
              uVar4 = fread(output,4,1,stream);
              if ((uVar4 != 1) || (uVar4 = fread(output + 1,4,1,stream), uVar4 != 1))
              goto LAB_000084a0;
              *pcVar3 = '\0';
              iVar5 = iVar5 + 1;
              pcVar3 = pcVar3 + 0x48;
            } while (iVar5 < (int)local_30);
          }
          uVar4 = fread(local_2c,4,1,stream);
          if (uVar4 == 1) {
            PTR__g_model_cache_000396ac->updated_at = local_2c[0];
          }
          fclose(stream);
          pRVar1 = PTR__g_model_cache_000396ac;
          if ((0 < PTR__g_model_cache_000396ac->updated_at) &&
             (tVar6 = time((time_t *)0x0), 0x15180 < tVar6 - pRVar1->updated_at)) {
            pRVar1->count = 0;
            pRVar1->updated_at = 0;
          }
        }
        else {
          fclose(stream);
        }
      }
      else {
        fclose(stream);
      }
    }
    else {
      fclose(stream);
    }
  }
  return;

#undef auStack_38
#undef local_30
#undef local_2c
}

/*
 * _model_cache_save @ 0x00008540..0x0000864c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Serialize model metadata to the user model-cache file.
 * Parameters/return (inferred): void _model_cache_save(void)
 * Direct callers: _model_cache_clear@0x0000864c, _model_cache_sync@0x00008690, _do_async_fetch@0x000087a8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; opens/creates/truncates filesystem paths according to the supplied mode; writes bytes to a stream; reads process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00008540_model_cache_save.s
 */
void model_cache_save(void)

{
  RecoveredModelCache *pRVar1;
  RecoveredModelCache *pRVar2;
  char *path;
  void *stream;
  int iVar3;
  int *input;
  
  path = (char *)get_model_cache_path();
  stream = fopen(path,"wb");
  if (stream != (void *)0x0) {
    fwrite("CACHE01",1,7,stream);
    pRVar1 = PTR__g_model_cache_000396ac;
    fwrite(&PTR__g_model_cache_000396ac->count,4,1,stream);
    pRVar2 = PTR__g_model_cache_000396ac;
    if (0 < pRVar1->count) {
      iVar3 = 0;
      do {
        fwrite(pRVar2->models + iVar3,1,0x40,stream);
        input = &pRVar2->models[iVar3].context_window;
        fwrite(input,4,1,stream);
        fwrite(input + 1,4,1,stream);
        iVar3 = iVar3 + 1;
      } while (iVar3 < pRVar2->count);
    }
    fwrite(&PTR__g_model_cache_000396ac->updated_at,4,1,stream);
    fclose(stream);
  }
  return;
}

/*
 * _model_cache_clear @ 0x0000864c..0x00008690
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in models_profiles: model cache clear. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _model_cache_clear(void)
 * Direct callers: _profile_select@0x00008fe0
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; opens/creates/truncates filesystem paths according to the supplied mode; writes bytes to a stream; reads process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000864c_model_cache_clear.s
 */
void model_cache_clear(void)

{
  RecoveredModelCache *pRVar1;
  
  pRVar1 = PTR__g_model_cache_000396ac;
  PTR__g_model_cache_000396ac->count = 0;
  pRVar1->updated_at = 0;
  model_cache_save();
  return;
}

/*
 * _model_cache_sync @ 0x00008690..0x000086d4
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in models_profiles: model cache sync. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _model_cache_sync(RecoveredConfig *config)
 * Direct callers: _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; opens/creates/truncates filesystem paths according to the supplied mode; writes bytes to a stream; reads process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00008690_model_cache_sync.s
 */
void model_cache_sync(RecoveredConfig *config)

{
  (void)config; /* Present in the recovered ABI; unused by this routine. */
  RecoveredModelCache *pRVar1;
  
  pRVar1 = PTR__g_model_cache_000396ac;
  PTR__g_model_cache_000396ac->count = 0;
  pRVar1->updated_at = 0;
  model_cache_save();
  return;
}

/*
 * _ollama_model_add @ 0x000086d4..0x000087a8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in models_profiles: ollama model add. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _ollama_model_add(char *name,int context_window,int size_class)
 * Direct callers: _do_async_fetch@0x000087a8
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000086d4_ollama_model_add.s
 */
void ollama_model_add(char *name,int context_window,int size_class)

{
  RecoveredModelCache *pRVar1;
  int iVar2;
  int iVar3;
  int iVar4;
  RecoveredModel *pRVar5;
  
  pRVar1 = PTR__g_model_cache_000396ac;
  if (((name != (char *)0x0) && (*name != '\0')) &&
     (iVar3 = PTR__g_model_cache_000396ac->count, iVar3 < 0x40)) {
    if (0 < iVar3) {
      iVar4 = 0;
      do {
        pRVar5 = pRVar1->models + iVar4;
        iVar2 = strcmp(pRVar5->name,name);
        if (iVar2 == 0) {
          pRVar5->context_window = context_window;
          pRVar5->size_class = size_class;
          return;
        }
        iVar4 = iVar4 + 1;
      } while (iVar3 != iVar4);
    }
    pRVar1 = PTR__g_model_cache_000396ac;
    PTR__g_model_cache_000396ac->count = iVar3 + 1;
    pRVar5 = pRVar1->models + iVar3;
    strncpy(pRVar5->name,name,0x3f);
    pRVar5->context_window = context_window;
    pRVar5->size_class = size_class;
  }
  return;
}

/*
 * _do_async_fetch @ 0x000087a8..0x00008b9c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in models_profiles: do async fetch. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _do_async_fetch(RecoveredConfig *config)
 * Direct callers: _ollama_refresh_models@0x00008b9c
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O; opens/creates/truncates filesystem paths according to the supplied mode; writes bytes to a stream; reads process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000087a8_do_async_fetch.s
 */
void do_async_fetch(RecoveredConfig *config)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0xa80] __attribute__((aligned(16)));
#define local_a68 (*((RecoveredHttpBuffer *)(app_frame + 0x8))) /* original SP - 0xa68 */
#define local_a5c (*((RecoveredHttpBuffer *)(app_frame + 0x14))) /* original SP - 0xa5c */
#define local_a50 (*((char *)(app_frame + 0x20))) /* original SP - 0xa50 */
#define auStack_a4f (*((undefined1 (*)[511])(app_frame + 0x21))) /* original SP - 0xa4f */
#define acStack_850 (*((char (*)[1024])(app_frame + 0x220))) /* original SP - 0x850 */
#define acStack_450 (*((char (*)[1040])(app_frame + 0x620))) /* original SP - 0x450 */

  bool bVar1;
  RecoveredModelCache *pRVar2;
  char *pcVar3;
  int iVar4;
  RecoveredCJSON *object;
  RecoveredCJSON *array;
  RecoveredCJSON *pRVar5;
  RecoveredCJSON *pRVar6;
  int iVar7;
  RecoveredCJSON *pRVar8;
  uint uVar9;
  time_t tVar10;
  char *pcVar11;
  char *auth_header;
  int index;
  int context_window;

  PTR__g_model_cache_000396ac->count = 0;
  pcVar3 = config_get_url(config);
  if ((pcVar3 == (char *)0x0) || (*pcVar3 == '\0')) {
    pcVar3 = "https://api.ollama.com";
  }
  snprintf(acStack_850,0x400,"%s/api/tags",pcVar3);
  pcVar11 = config->ollama_key;
  if (config->ollama_key[0] == '\0') {
    pcVar11 = config->api_key;
  }
  local_a50 = '\0';
  memset(auStack_a4f,0,0x1ff);
  if ((pcVar11 != (char *)0x0) && (*pcVar11 != '\0')) {
    snprintf(&local_a50,0x200,"Authorization: Bearer %s",pcVar11);
  }
  http_buf_init(&local_a68);
  pcVar11 = &local_a50;
  if (local_a50 == '\0') {
    pcVar11 = (char *)0x0;
  }
  iVar4 = http_get(acStack_850,pcVar11,&local_a68);
  if ((iVar4 != 200) || (local_a68.data == (char *)0x0)) {
    http_buf_free(&local_a68);
                    /* WARNING: Subroutine does not return */
    _exit(1);
  }
  object = cJSON_Parse(local_a68.data);
  http_buf_free(&local_a68);
  if (object == (RecoveredCJSON *)0x0) {
                    /* WARNING: Subroutine does not return */
    _exit(1);
  }
  array = cJSON_GetObjectItem(object,"models");
  if (array == (RecoveredCJSON *)0x0) {
    cJSON_Delete(object);
                    /* WARNING: Subroutine does not return */
    _exit(1);
  }
  iVar4 = cJSON_GetArraySize(array);
  pRVar2 = PTR__g_model_cache_000396ac;
  if ((0 < iVar4) && (PTR__g_model_cache_000396ac->count < 0x40)) {
    index = 0;
    do {
      pRVar5 = cJSON_GetArrayItem(array,index);
      pRVar5 = cJSON_GetObjectItem(pRVar5,"name");
      if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
        snprintf(acStack_450,0x400,"%s/api/show",pcVar3);
        pRVar6 = cJSON_CreateObject();
        cJSON_AddStringToObject(pRVar6,"name",pRVar5->value_string);
        pcVar11 = cJSON_PrintUnformatted(pRVar6);
        cJSON_Delete(pRVar6);
        http_buf_init(&local_a5c);
        auth_header = &local_a50;
        if (local_a50 == '\0') {
          auth_header = (char *)0x0;
        }
        iVar7 = http_post_json(acStack_450,pcVar11,auth_header,&local_a5c);
        free(pcVar11);
        if (((iVar7 == 200) && (local_a5c.data != (char *)0x0)) &&
           (pRVar6 = cJSON_Parse(local_a5c.data), pRVar6 != (RecoveredCJSON *)0x0)) {
          pRVar8 = cJSON_GetObjectItem(pRVar6,"model_info");
          if (pRVar8 != (RecoveredCJSON *)0x0) {
            for (pRVar8 = pRVar8->child; pRVar8 != (RecoveredCJSON *)0x0; pRVar8 = pRVar8->next) {
              pcVar11 = pRVar8->member_name;
              if (pcVar11 != (char *)0x0) {
                uVar9 = strlen(pcVar11);
                if ((uVar9 < 0xf) ||
                   (iVar7 = strcmp(pcVar11 + (uVar9 - 0xf),".context_length"), iVar7 != 0)) {
                  iVar7 = strcmp(pcVar11,"context_length");
                  if (iVar7 != 0) goto LAB_00008a84;
                  context_window = pRVar8->value_int;
                }
                else {
                  context_window = pRVar8->value_int;
                }
                if (0 < context_window) goto LAB_00008ac4;
                goto LAB_00008a94;
              }
LAB_00008a84:
            }
          }
          context_window = 0;
LAB_00008a94:
          pRVar8 = cJSON_GetObjectItem(pRVar6,"options");
          if ((pRVar8 != (RecoveredCJSON *)0x0) &&
             (pRVar8 = cJSON_GetObjectItem(pRVar8,"num_ctx"), pRVar8 != (RecoveredCJSON *)0x0)) {
            context_window = pRVar8->value_int;
          }
LAB_00008ac4:
          pRVar8 = cJSON_GetObjectItem(pRVar6,"supports_tools");
          if (((pRVar8 == (RecoveredCJSON *)0x0) || (pRVar8->type_flags == 2)) ||
             (iVar7 = 0, pRVar8->value_int == 1)) {
            iVar7 = 1;
          }
          cJSON_Delete(pRVar6);
          if (context_window < 1) goto LAB_00008b14;
        }
        else {
          iVar7 = 1;
LAB_00008b14:
          context_window = model_context(pRVar5->value_string);
        }
        http_buf_free(&local_a5c);
        ollama_model_add(pRVar5->value_string,context_window,iVar7);
      }
      index = index + 1;
    } while ((iVar4 != index) && (pRVar2->count < 0x40));
  }
  cJSON_Delete(object);
  tVar10 = time((time_t *)0x0);
  PTR__g_model_cache_000396ac->updated_at = tVar10;
  model_cache_save();
  iVar4 = 3;
  do {
    close(iVar4);
    bVar1 = iVar4 != 0xff;
    iVar4 = iVar4 + 1;
  } while (bVar1);
  return;

#undef local_a68
#undef local_a5c
#undef local_a50
#undef auStack_a4f
#undef acStack_850
#undef acStack_450
}

/*
 * _ollama_refresh_models @ 0x00008b9c..0x00008c00
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in models_profiles: ollama refresh models. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _ollama_refresh_models(RecoveredConfig *config)
 * Direct callers: _handle_slash_command@0x0000362c, _profile_select@0x00008fe0, _handle_cmd@0x0001b004, _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; writes bytes to a stream; reads process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00008b9c_ollama_refresh_models.s
 */
void ollama_refresh_models(RecoveredConfig *config)

{
  _g_refresh_pid = fork();
  if ((-1 < _g_refresh_pid) && (_g_refresh_pid == 0)) {
    do_async_fetch(config);
                    /* WARNING: Subroutine does not return */
    _exit(0);
  }
  return;
}

/*
 * _ollama_refresh_poll @ 0x00008c00..0x00008c94
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in models_profiles: ollama refresh poll. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _ollama_refresh_poll(void)
 * Direct callers: _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; opens/creates/truncates filesystem paths according to the supplied mode; reads bytes from a stream; reads process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00008c00_ollama_refresh_poll.s
 */
int ollama_refresh_poll(void)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x40] __attribute__((aligned(16)));
#define local_28 (*((uint (*)[9])(app_frame + 0x8))) /* original SP - 0x28 */

  int iVar1;
  pid_t pVar2;

  iVar1 = 0;
  if (_g_refresh_pid != 0) {
    pVar2 = waitpid(_g_refresh_pid,(int *)local_28,1);
    iVar1 = 1;
    if (pVar2 != 0) {
      _g_refresh_pid = 0;
      if (((local_28[0] & 0x7f) == 0) && (local_28[0] >> 8 == 0)) {
        model_cache_load();
        iVar1 = 2;
      }
      else {
        iVar1 = -1;
      }
    }
  }
  return iVar1;

#undef local_28
}

/*
 * _profile_find @ 0x00008c94..0x00008d08
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage provider-profile state: find. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): RecoveredProfile * _profile_find(RecoveredConfig *config,char *name)
 * Direct callers: _profile_get_key@0x00008d0c, _profile_get_url@0x00008d68, _profile_add@0x00008e00, _profile_select@0x00008fe0
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00008c94_profile_find.s
 */
RecoveredProfile * profile_find(RecoveredConfig *config,char *name)

{
  int iVar1;
  int iVar2;
  int iVar3;
  
  iVar2 = config->profile_count;
  if (0 < iVar2) {
    iVar3 = 0;
    do {
      iVar1 = strcmp((const char *)(0x1824 + (int)(config->active_profile + iVar3 * 0x3c8 + -4)),name);
      if (iVar1 == 0) {
        return config->profiles + iVar3;
      }
      iVar3 = iVar3 + 1;
    } while (iVar3 != iVar2);
  }
  return (RecoveredProfile *)0x0;
}

/*
 * _profile_init_defaults @ 0x00008d08..0x00008d0c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage provider-profile state: init defaults. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _profile_init_defaults(void)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00008d08_profile_init_defaults.s
 */
void profile_init_defaults(void)

{
  return;
}

/*
 * _profile_get_key @ 0x00008d0c..0x00008d68
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage provider-profile state: get key. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): char * _profile_get_key(RecoveredConfig *config)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00008d0c_profile_get_key.s
 */
char * profile_get_key(RecoveredConfig *config)

{
  RecoveredProfile *pRVar1;
  char *pcVar2;
  
  if (((config->active_profile[0] != '\0') &&
      (pRVar1 = profile_find(config,config->active_profile), pRVar1 != (RecoveredProfile *)0x0)) &&
     (pRVar1->api_key[0] != '\0')) {
    return pRVar1->api_key;
  }
  pcVar2 = config_get_key(config);
  return pcVar2;
}

/*
 * _profile_get_url @ 0x00008d68..0x00008dc4
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage provider-profile state: get url. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): char * _profile_get_url(RecoveredConfig *config)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00008d68_profile_get_url.s
 */
char * profile_get_url(RecoveredConfig *config)

{
  RecoveredProfile *pRVar1;
  char *pcVar2;
  
  if (((config->active_profile[0] != '\0') &&
      (pRVar1 = profile_find(config,config->active_profile), pRVar1 != (RecoveredProfile *)0x0)) &&
     (pRVar1->base_url[0] != '\0')) {
    return pRVar1->base_url;
  }
  pcVar2 = config_get_url(config);
  return pcVar2;
}

/*
 * _profile_get_name @ 0x00008dc4..0x00008e00
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage provider-profile state: get name. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): char * _profile_get_name(RecoveredConfig *config)
 * Direct callers: _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00008dc4_profile_get_name.s
 */
char * profile_get_name(RecoveredConfig *config)

{
  char *pcVar1;
  
  pcVar1 = config->active_profile;
  if (config->active_profile[0] == '\0') {
    pcVar1 = provider_name(config->provider);
  }
  return pcVar1;
}

/*
 * _profile_add @ 0x00008e00..0x00008eec
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage provider-profile state: add. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _profile_add(RecoveredConfig *config,char *name,int provider,char *model,char *key,char *url)
 * Direct callers: _handle_slash_command@0x0000362c, _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00008e00_profile_add.s
 */
void profile_add(RecoveredConfig *config,char *name,int provider,char *model,char *key,char *url)

{
  RecoveredProfile *output;
  
  if ((config->profile_count < 8) && (name != (char *)0x0)) {
    if (*name != '\0') {
      output = profile_find(config,name);
      if (output == (RecoveredProfile *)0x0) {
        output = config->profiles + config->profile_count;
        config->profile_count = config->profile_count + 1;
      }
      memset(output,0,0x3c8);
      strncpy(output->name,name,0x3f);
      output->provider = provider;
      if (model != (char *)0x0) {
        strncpy(output->model,model,0x7f);
      }
      if (key != (char *)0x0) {
        strncpy(output->api_key,key,0xff);
      }
      if (url != (char *)0x0) {
        strncpy(output->base_url,url,0x1ff);
      }
    }
  }
  return;
}

/*
 * _profile_remove @ 0x00008eec..0x00008fe0
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage provider-profile state: remove. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): int _profile_remove(RecoveredConfig *config,char *name)
 * Direct callers: _handle_slash_command@0x0000362c, _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00008eec_profile_remove.s
 */
int profile_remove(RecoveredConfig *config,char *name)

{
  int iVar1;
  int iVar2;
  int iVar3;
  undefined *input;
  
  iVar3 = config->profile_count;
  if (0 < iVar3) {
    iVar2 = 0;
    do {
      iVar1 = strcmp((const char *)(0x1824 + (int)(config->active_profile + iVar2 * 0x3c8 + -4)),name);
      if (iVar1 == 0) {
        iVar3 = iVar3 + -1;
        if (iVar2 < iVar3) {
          input = (undefined *)(0x1824 + (int)(config->google_key + iVar2 * 0x3c8 + 4));
          do {
            iVar2 = iVar2 + 1;
            memcpy(input + -0x3c8,input,0x3c8);
            iVar3 = config->profile_count + -1;
            input = input + 0x3c8;
          } while (iVar2 < iVar3);
        }
        config->profile_count = iVar3;
        iVar3 = strcmp(config->active_profile,name);
        if (iVar3 != 0) {
          return 0;
        }
        config->active_profile[0] = '\0';
        return 0;
      }
      iVar2 = iVar2 + 1;
    } while (iVar3 != iVar2);
  }
  return -1;
}

/*
 * _profile_select @ 0x00008fe0..0x00009168
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Copy populated fields of the selected profile into the active provider/configuration fields.
 * Parameters/return (inferred): int _profile_select(RecoveredConfig *config,char *name)
 * Direct callers: _handle_slash_command@0x0000362c, _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; writes bytes to a stream; reads process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00008fe0_profile_select.s
 */
int profile_select(RecoveredConfig *config,char *name)

{
  int iVar1;
  RecoveredProfile *pRVar2;
  
  pRVar2 = profile_find(config,name);
  iVar1 = -1;
  if (pRVar2 != (RecoveredProfile *)0x0) {
    config->provider = pRVar2->provider;
    strncpy(config->active_profile,name,0x3f);
    config->active_profile[0x3f] = '\0';
    if (pRVar2->model[0] != '\0') {
      strncpy(config->model,pRVar2->model,0x7f);
      config->model[0x7f] = '\0';
    }
    if (pRVar2->api_key[0] != '\0') {
      iVar1 = pRVar2->provider;
      if (iVar1 == 1) {
        strncpy(config->openai_key,pRVar2->api_key,0xff);
        config->openai_key[0xff] = '\0';
      }
      else if (iVar1 == 0) {
        strncpy(config->ollama_key,pRVar2->api_key,0xff);
        config->ollama_key[0xff] = '\0';
      }
      else if (iVar1 == 2) {
        strncpy(config->anthropic_key,pRVar2->api_key,0xff);
        config->anthropic_key[0xff] = '\0';
      }
      else if (iVar1 == 3) {
        strncpy(config->google_key,pRVar2->api_key,0xff);
        config->google_key[0xff] = '\0';
      }
    }
    if (pRVar2->base_url[0] != '\0') {
      if (pRVar2->provider == 0) {
        strncpy(config->ollama_url,pRVar2->base_url,0x1ff);
        config->ollama_url[0x1ff] = '\0';
      }
      else {
        strncpy(config->base_url,pRVar2->base_url,0x1ff);
        config->base_url[0x1ff] = '\0';
      }
    }
    model_cache_clear();
    iVar1 = 0;
    if (config->provider == 0) {
      ollama_refresh_models(config);
      iVar1 = 0;
    }
  }
  return iVar1;
}

/*
 * _profile_list @ 0x00009168..0x000091c0
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage provider-profile state: list. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _profile_list(RecoveredConfig *config,char **names,int capacity,int *count)
 * Direct callers: _handle_slash_command@0x0000362c, _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00009168_profile_list.s
 */
void profile_list(RecoveredConfig *config,char **names,int capacity,int *count)

{
  int iVar1;
  
  *count = 0;
  if (config->profile_count < 1) {
    return;
  }
  if (capacity < 1) {
    return;
  }
  iVar1 = 0;
  do {
    names[iVar1] = (char *)(0x1824 + (int)(config->active_profile + iVar1 * 0x3c8 + -4));
    iVar1 = iVar1 + 1;
    *count = iVar1;
    if (config->profile_count <= iVar1) {
      return;
    }
    capacity = capacity + -1;
  } while (capacity != 0);
  return;
}

