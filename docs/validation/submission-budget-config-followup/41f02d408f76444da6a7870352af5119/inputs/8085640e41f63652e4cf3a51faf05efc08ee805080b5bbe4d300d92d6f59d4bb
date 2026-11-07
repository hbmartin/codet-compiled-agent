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
 * _count_lines_in_buf @ 0x00013c7c..0x00013cb8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Count newline-delimited lines in a byte buffer.
 * Parameters/return (inferred): int _count_lines_in_buf(char *text,uint length)
 * Direct callers: _smart_truncate_content@0x00013cb8, _tool_read_file@0x00013e64
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00013c7c_count_lines_in_buf.s
 */
int count_lines_in_buf(char *text,uint length)

{
  int iVar1;
  int iVar2;
  
  iVar2 = 0;
  if (length != 0) {
    iVar1 = 0;
    iVar2 = 0;
    do {
      if (text[iVar1] == '\n') {
        iVar2 = iVar2 + 1;
      }
      iVar1 = iVar1 + 1;
      length = length - 1;
    } while (length != 0);
  }
  return iVar2;
}

/*
 * _smart_truncate_content @ 0x00013cb8..0x00013de0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Choose a bounded representation of long file content for model context.
 * Parameters/return (inferred): void _smart_truncate_content(RecoveredDString *destination,RecoveredDString *source)
 * Direct callers: _tool_read_file@0x00013e64
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00013cb8_smart_truncate_content.s
 */
void smart_truncate_content(RecoveredDString *destination,RecoveredDString *source)

{
  char *text;
  int iVar1;
  uint length;
  int iVar2;
  uint length_00;
  uint uVar3;
  
  text = ds_cstr(source);
  length_00 = source->length;
  iVar1 = count_lines_in_buf(text,length_00);
  if (iVar1 < 0x98) {
    ds_append(destination,text);
  }
  else {
    uVar3 = 0;
    if (length_00 != 0) {
      iVar2 = 0;
      length = 0;
      uVar3 = 0;
      do {
        if (text[uVar3] == '\n') {
          iVar2 = iVar2 + 1;
        }
        if ((iVar2 == 100) && (length == 0)) {
          length = uVar3 + 1;
        }
        uVar3 = uVar3 + 1;
        if (iVar2 == iVar1 + -0x32) goto LAB_00013d60;
      } while (uVar3 < length_00);
      uVar3 = 0;
LAB_00013d60:
      if (length != 0) {
        ds_appendn(destination,text,length);
      }
    }
    ds_appendf(destination,"\n... [truncated %d lines; file has %d total lines] ...\n\n",
                iVar1 + -0x96,iVar1);
    if ((uVar3 != 0) && (uVar3 < length_00)) {
      ds_append(destination,text + uVar3);
    }
  }
  return;
}

/*
 * _read_file_into_dstring @ 0x00013de0..0x00013e64
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Read file bytes into a heap-backed dynamic string.
 * Parameters/return (inferred): int _read_file_into_dstring(char *path,RecoveredDString *output)
 * Direct callers: _tool_read_file@0x00013e64
 * Possible side effects (direct-call closure, incomplete for indirect calls): opens/creates/truncates filesystem paths according to the supplied mode; reads bytes from a stream
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00013de0_read_file_into_dstring.s
 */
int read_file_into_dstring(char *path,RecoveredDString *output)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x1030] __attribute__((aligned(16)));
#define acStack_1018 (*((char (*)[4108])(app_frame + 0x8))) /* original SP - 0x1018 */

  int iVar1;
  void *stream;
  uint length;

  stream = fopen(path,"r");
  iVar1 = -1;
  if (stream != (void *)0x0) {
    while (length = fread(acStack_1018,1,0x1000,stream), length != 0) {
      ds_appendn(output,acStack_1018,length);
    }
    fclose(stream);
    iVar1 = 0;
  }
  return iVar1;

#undef acStack_1018
}

/*
 * _tool_read_file @ 0x00013e64..0x00014134
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Read comma-separated paths, prepend working_dir for relative names and update file-context metadata.
 * Parameters/return (inferred): int _tool_read_file(RecoveredToolCall *tool_call,RecoveredAppState *app_state)
 * Direct callers: _tool_read_and_edit@0x00015544, _tool_execute@0x000158c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; opens/creates/truncates filesystem paths according to the supplied mode; reads bytes from a stream
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00013e64_tool_read_file.s
 */
int tool_read_file(RecoveredToolCall *tool_call,RecoveredAppState *app_state)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x450] __attribute__((aligned(16)));
#define local_438 (*((char * *)(app_frame + 0x8))) /* original SP - 0x438 */
#define RStack_434 (*((RecoveredDString *)(app_frame + 0xc))) /* original SP - 0x434 */
#define acStack_428 (*((char (*)[1023])(app_frame + 0x18))) /* original SP - 0x428 */
#define local_29 (*((undefined1 *)(app_frame + 0x417))) /* original SP - 0x29 */

  bool bVar1;
  char cVar2;
  char *pcVar3;
  char *pcVar4;
  uint uVar5;
  char *output;
  int iVar6;
  int *piVar7;
  RecoveredFileContext *pRVar8;
  int last_message;
  int iVar9;
  RecoveredDString *str;

  str = &tool_call->result;
  ds_clear(str);
  pcVar4 = ds_cstr(tool_call->arguments);
  uVar5 = strlen(pcVar4);
  output = malloc(uVar5 + 1);
  if (output == (char *)0x0) {
    ds_appendf(str,"Error: out of memory processing paths\n");
    iVar6 = -1;
  }
  else {
    memcpy(output,pcVar4,uVar5 + 1);
    local_438 = (char *)0x0;
    pcVar4 = strtok_r(output,",",&local_438);
    iVar9 = 0;
    while (pcVar4 != (char *)0x0) {
      cVar2 = *pcVar4;
      while (cVar2 == ' ') {
        pcVar4 = pcVar4 + 1;
        cVar2 = *pcVar4;
      }
      uVar5 = strlen(pcVar4);
      pcVar3 = pcVar4 + (uVar5 - 1);
      if ((pcVar4 < pcVar3) && (pcVar4[uVar5 - 1] == ' ')) {
        iVar6 = (int)pcVar3 - (int)pcVar4;
        if (pcVar4 + (uVar5 - 2) < pcVar4) {
          iVar6 = 1;
        }
        do {
          *pcVar3 = '\0';
          pcVar3 = pcVar3 + -1;
          iVar6 = iVar6 + -1;
          if (iVar6 == 0) break;
        } while (*pcVar3 == ' ');
      }
      if ((*pcVar4 == '/') || (*pcVar4 == '~')) {
        strncpy(acStack_428,pcVar4,0x3ff);
        local_29 = 0;
      }
      else {
        app_format_truncated(acStack_428,0x400,"%s/%s",(app_state->config).working_directory,pcVar4);
      }
      ds_init(&RStack_434);
      iVar6 = read_file_into_dstring(acStack_428,&RStack_434);
      if (iVar6 == 0) {
        pcVar4 = ds_cstr(&RStack_434);
        iVar6 = count_lines_in_buf(pcVar4,RStack_434.length);
        bVar1 = 0x97 < iVar6;
        ds_appendf(str,"=== %s ===\n",acStack_428);
        pRVar8 = file_ctx_find(app_state,acStack_428);
        if (((pRVar8 != (RecoveredFileContext *)0x0) && ((pRVar8->flags & 0xc0000000) == 0x40000000)
            ) && (bVar1)) {
          ds_append(str,
                     "\n[Note: previously showed truncated version; use /read full PATH for complete file]\n\n"
                    );
        }
        smart_truncate_content(str,&RStack_434);
        ds_append(str,"\n");
        last_message = iVar6;
        if (bVar1) {
          last_message = 0x96;
        }
        file_ctx_update(app_state,acStack_428,RStack_434.length,iVar6,last_message,(uint)bVar1);
      }
      else {
        piVar7 = __error();
        pcVar4 = strerror(*piVar7);
        ds_appendf(str,"Error: Could not read \'%s\': %s\n",acStack_428,pcVar4);
        iVar9 = iVar9 + 1;
      }
      ds_free(&RStack_434);
      pcVar4 = strtok_r((char *)0x0,",",&local_438);
    }
    free(output);
    iVar6 = -1;
    if (iVar9 < 1) {
      iVar6 = 0;
    }
  }
  return iVar6;

#undef local_438
#undef RStack_434
#undef acStack_428
#undef local_29
}

/*
 * _guard_file_has_been_read @ 0x00014134..0x00014170
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Determine whether a path has a read record in the current file-context state.
 * Parameters/return (inferred): int _guard_file_has_been_read(RecoveredAppState *app,char *path)
 * Direct callers: _guard_refuse_write@0x00014170
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00014134_guard_file_has_been_read.s
 */
int guard_file_has_been_read(RecoveredAppState *app,char *path)

{
  RecoveredFileContext *pRVar1;
  int iVar2;
  
  pRVar1 = file_ctx_find(app,path);
  if ((pRVar1 == (RecoveredFileContext *)0x0) || (iVar2 = 1, pRVar1->first_message < 1)) {
    iVar2 = 0;
  }
  return iVar2;
}

/*
 * _guard_refuse_write @ 0x00014170..0x0001420c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): When write_guard_enforce is enabled, refuse an existing file lacking a read record.
 * Parameters/return (inferred): int _guard_refuse_write(RecoveredToolCall *call,RecoveredAppState *app,char *path)
 * Direct callers: _tool_write_file@0x0001420c, _tool_edit_file@0x000144a8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; reads filesystem metadata
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00014170_guard_refuse_write.s
 */
int guard_refuse_write(RecoveredToolCall *call,RecoveredAppState *app,char *path)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x90] __attribute__((aligned(16)));
#define RStack_78 (*((RecoveredTigerStat *)(app_frame + 0x8))) /* original SP - 0x78 */

  int iVar1;

  if ((((app->config).write_guard_enforce != 0) && (iVar1 = stat(path,(struct stat *)(&RStack_78)), iVar1 == 0)) &&
     (iVar1 = guard_file_has_been_read(app,path), iVar1 == 0)) {
    ds_clear(&call->result);
    ds_appendf(&call->result,
                "[GUARD] File \'%s\' not read this session. Use read_file first to avoid overwriting unknown content."
                ,path);
    return 1;
  }
  return 0;

#undef RStack_78
}

/*
 * _tool_write_file @ 0x0001420c..0x000144a8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Resolve a tool path, apply the optional read guard, create parent directories and replace file contents.
 * Parameters/return (inferred): int _tool_write_file(RecoveredToolCall *tool_call,RecoveredAppState *app_state)
 * Direct callers: _tool_execute@0x000158c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; opens/creates/truncates filesystem paths according to the supplied mode; writes text to a stream; creates directories; reads filesystem metadata
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0001420c_tool_write_file.s
 */
int tool_write_file(RecoveredToolCall *tool_call,RecoveredAppState *app_state)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x8a0] __attribute__((aligned(16)));
#define acStack_888 (*((char (*)[1023])(app_frame + 0x8))) /* original SP - 0x888 */
#define local_489 (*((undefined1 *)(app_frame + 0x407))) /* original SP - 0x489 */
#define RStack_488 (*((RecoveredTigerStat *)(app_frame + 0x408))) /* original SP - 0x488 */
#define local_428 (*((char *)(app_frame + 0x468))) /* original SP - 0x428 */
#define local_427 (*((char (*)[1022])(app_frame + 0x469))) /* original SP - 0x427 */
#define local_29 (*((undefined1 *)(app_frame + 0x867))) /* original SP - 0x29 */

  char cVar1;
  int iVar2;
  char *pcVar3;
  uint uVar4;
  int iVar5;
  void *stream;
  int *piVar6;

  pcVar3 = ds_cstr(tool_call->arguments);
  cVar1 = *pcVar3;
  if (cVar1 == '\0') {
    ds_clear(&tool_call->result);
    ds_appendf(&tool_call->result,"Error: write_file called with empty path");
    iVar2 = -1;
  }
  else {
    uVar4 = strlen(pcVar3);
    if (pcVar3[uVar4 - 1] == '/') {
      ds_clear(&tool_call->result);
      ds_appendf(&tool_call->result,"Error: write_file path '%s' ends with '/' \u2014 must be a file, not a directory",pcVar3);
      iVar2 = -1;
    }
    else {
      if ((cVar1 == '/') || (cVar1 == '~')) {
        strncpy(acStack_888,pcVar3,0x3ff);
        local_489 = 0;
      }
      else {
        app_format_truncated(acStack_888,0x400,"%s/%s",(app_state->config).working_directory,pcVar3);
      }
      iVar2 = stat(acStack_888,(struct stat *)(&RStack_488));
      if ((iVar2 == 0) && ((RStack_488.mode & 0xf000) == 0x4000)) {
        ds_clear(&tool_call->result);
        ds_appendf(&tool_call->result,"Error: Could not write \'%s\': Is a directory",acStack_888);
        iVar2 = -1;
      }
      else {
        iVar5 = guard_refuse_write(tool_call,app_state,acStack_888);
        iVar2 = -1;
        if (iVar5 == 0) {
          app_copy_padded(&local_428,acStack_888,0x3ff);
          local_29 = 0;
          pcVar3 = strrchr(&local_428,0x2f);
          if (pcVar3 != (char *)0x0) {
            *pcVar3 = '\0';
            if (local_427[0] != '\0') {
              pcVar3 = local_427;
              do {
                if (local_427[0] == '/') {
                  *pcVar3 = '\0';
                  mkdir(&local_428,0x1ed);
                  *pcVar3 = '/';
                }
                pcVar3 = pcVar3 + 1;
                local_427[0] = *pcVar3;
              } while (local_427[0] != '\0');
            }
            if (local_428 != '\0') {
              mkdir(&local_428,0x1ed);
            }
          }
          stream = fopen(acStack_888,"w");
          if (stream == (void *)0x0) {
            ds_clear(&tool_call->result);
            piVar6 = __error();
            pcVar3 = strerror(*piVar6);
            ds_appendf(&tool_call->result,"Error: Could not write \'%s\': %s",acStack_888,pcVar3);
            iVar2 = -1;
          }
          else {
            pcVar3 = ds_cstr(tool_call->arguments + 1);
            fputs(pcVar3,stream);
            fclose(stream);
            ds_clear(&tool_call->result);
            pcVar3 = ds_cstr(tool_call->arguments + 1);
            uVar4 = strlen(pcVar3);
            ds_appendf(&tool_call->result,"Successfully wrote %zu bytes to %s",(size_t)(uVar4),acStack_888);
            file_ctx_mark_stale(app_state,acStack_888);
            iVar2 = 0;
          }
        }
      }
    }
  }
  return iVar2;

#undef acStack_888
#undef local_489
#undef RStack_488
#undef local_428
#undef local_427
#undef local_29
}

/*
 * _tool_edit_file @ 0x000144a8..0x00014870
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Read an existing file, replace a matching text occurrence, write it back and append a diff result.
 * Parameters/return (inferred): int _tool_edit_file(RecoveredToolCall *tool_call,RecoveredAppState *app_state)
 * Direct callers: _tool_read_and_edit@0x00015544, _tool_execute@0x000158c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; executes an argument vector through PATH; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; reads bytes from a stream; writes bytes to a stream; securely creates a temporary file; reads a descriptor or IPC channel; reads filesystem metadata; removes a filesystem entry; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000144a8_tool_edit_file.s
 */
int tool_edit_file(RecoveredToolCall *tool_call,RecoveredAppState *app_state)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x14d0] __attribute__((aligned(16)));
#define acStack_14b8 (*((char (*)[1023])(app_frame + 0x8))) /* original SP - 0x14b8 */
#define local_10b9 (*((undefined1 *)(app_frame + 0x407))) /* original SP - 0x10b9 */
#define RStack_10b8 (*((RecoveredTigerStat *)(app_frame + 0x408))) /* original SP - 0x10b8 */
#define local_1058 (*((RecoveredDString *)(app_frame + 0x468))) /* original SP - 0x1058 */
#define acStack_104c (*((char (*)[4096])(app_frame + 0x474))) /* original SP - 0x104c */
#define local_4c (*((RecoveredDString *)(app_frame + 0x1474))) /* original SP - 0x4c */
#define aRStack_40 (*((RecoveredDString (*)[3])(app_frame + 0x1480))) /* original SP - 0x40 */

  char cVar1;
  int iVar2;
  char *pcVar3;
  uint uVar4;
  int iVar5;
  void *pvVar6;
  int *piVar7;
  char *text;
  char *pcVar8;
  uint uVar9;
  RecoveredDString *str;

  pcVar3 = ds_cstr(tool_call->arguments);
  cVar1 = *pcVar3;
  if (cVar1 == '\0') {
    ds_clear(&tool_call->result);
    ds_appendf(&tool_call->result,"Error: edit_file called with empty path");
    iVar2 = -1;
  }
  else {
    uVar4 = strlen(pcVar3);
    if (pcVar3[uVar4 - 1] == '/') {
      ds_clear(&tool_call->result);
      ds_appendf(&tool_call->result,"Error: edit_file path '%s' ends with '/' \u2014 must be a file, not a directory",pcVar3);
      iVar2 = -1;
    }
    else {
      if ((cVar1 == '/') || (cVar1 == '~')) {
        strncpy(acStack_14b8,pcVar3,0x3ff);
        local_10b9 = 0;
      }
      else {
        app_format_truncated(acStack_14b8,0x400,"%s/%s",(app_state->config).working_directory,pcVar3);
      }
      iVar2 = stat(acStack_14b8,(struct stat *)(&RStack_10b8));
      if ((iVar2 == 0) && ((RStack_10b8.mode & 0xf000) == 0x4000)) {
        ds_clear(&tool_call->result);
        ds_appendf(&tool_call->result,"Error: Could not edit \'%s\': Is a directory",acStack_14b8);
        iVar2 = -1;
      }
      else {
        iVar5 = guard_refuse_write(tool_call,app_state,acStack_14b8);
        iVar2 = -1;
        if (iVar5 == 0) {
          pvVar6 = fopen(acStack_14b8,"r");
          if (pvVar6 == (void *)0x0) {
            ds_clear(&tool_call->result);
            piVar7 = __error();
            pcVar3 = strerror(*piVar7);
            ds_appendf(&tool_call->result,"Error: Could not read \'%s\': %s",acStack_14b8,pcVar3);
            iVar2 = -1;
          }
          else {
            ds_init(&local_1058);
            while( true ) {
              uVar4 = fread(acStack_104c,1,0x1000,pvVar6);
              if (uVar4 == 0) break;
              ds_appendn(&local_1058,acStack_104c,uVar4);
            }
            fclose(pvVar6);
            pcVar3 = ds_cstr(tool_call->arguments + 1);
            text = ds_cstr(tool_call->arguments + 2);
            pcVar8 = ds_cstr(&local_1058);
            pcVar8 = strstr(pcVar8,pcVar3);
            if (pcVar8 == (char *)0x0) {
              ds_clear(&tool_call->result);
              ds_appendf(&tool_call->result,"Error: Could not find text to replace in \'%s\'",
                          acStack_14b8);
              ds_free(&local_1058);
              iVar2 = -1;
            }
            else {
              ds_init(&local_4c);
              ds_appendn(&local_4c,local_1058.data,(int)pcVar8 - (int)local_1058.data);
              ds_append(&local_4c,text);
              uVar4 = strlen(pcVar3);
              ds_append(&local_4c,pcVar8 + uVar4);
              pvVar6 = fopen(acStack_14b8,"w");
              if (pvVar6 == (void *)0x0) {
                ds_clear(&tool_call->result);
                piVar7 = __error();
                pcVar3 = strerror(*piVar7);
                ds_appendf(&tool_call->result,"Error: Could not write \'%s\': %s",acStack_14b8,
                            pcVar3);
                ds_free(&local_1058);
                ds_free(&local_4c);
                iVar2 = -1;
              }
              else {
                fwrite(local_4c.data,1,local_4c.length,pvVar6);
                fclose(pvVar6);
                str = &tool_call->result;
                ds_clear(str);
                uVar4 = strlen(pcVar3);
                uVar9 = strlen(text);
                ds_appendf(str,"Successfully edited %s (%lu chars -> %lu chars)\n",acStack_14b8,
                            (long unsigned int)(uVar4),(long unsigned int)(uVar9));
                ds_init(aRStack_40);
                pcVar3 = ds_cstr(&local_1058);
                iVar2 = diff_string_vs_file(pcVar3,acStack_14b8,aRStack_40);
                if ((iVar2 == 0) && (aRStack_40[0].length != 0)) {
                  pcVar3 = ds_cstr(aRStack_40);
                  ds_appendf(str,"\nDiff:\n%s",pcVar3);
                }
                ds_free(aRStack_40);
                file_ctx_mark_stale(app_state,acStack_14b8);
                ds_free(&local_1058);
                ds_free(&local_4c);
                iVar2 = 0;
              }
            }
          }
        }
      }
    }
  }
  return iVar2;

#undef acStack_14b8
#undef local_10b9
#undef RStack_10b8
#undef local_1058
#undef acStack_104c
#undef local_4c
#undef aRStack_40
}

/*
 * _tool_shell @ 0x00014870..0x000149e8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Build cd '<working_dir>' && <command> 2>&1 and capture popen output as the tool result.
 * Parameters/return (inferred): int _tool_shell(RecoveredToolCall *tool_call,RecoveredAppState *app_state)
 * Direct callers: _tool_execute@0x000158c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; reads text from a stream; executes a shell command and opens a pipe
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00014870_tool_shell.s
 */
int tool_shell(RecoveredToolCall *tool_call,RecoveredAppState *app_state)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x1050] __attribute__((aligned(16)));
#define RStack_1038 (*((RecoveredDString *)(app_frame + 0x8))) /* original SP - 0x1038 */
#define acStack_102c (*((char (*)[4116])(app_frame + 0x14))) /* original SP - 0x102c */

  uint uVar1;
  char *pcVar2;
  void *stream;
  int *piVar3;
  uint length;
  RecoveredDString *str;
  int iVar4;


  ds_init(&RStack_1038);
  pcVar2 = ds_cstr(tool_call->arguments);
  ds_appendf(&RStack_1038,"cd \'%s\' && %s 2>&1",(app_state->config).working_directory,pcVar2);
  pcVar2 = ds_cstr(&RStack_1038);
  stream = popen(pcVar2,"r");
  ds_free(&RStack_1038);
  str = &tool_call->result;
  ds_clear(str);
  iVar4 = 0;
  if (stream == (void *)0x0) {
    piVar3 = __error();
    pcVar2 = strerror(*piVar3);
    ds_appendf(str,"Error: Could not execute: %s",pcVar2);
    iVar4 = -1;
  }
  else {
    while (pcVar2 = fgets(acStack_102c,0x1000,stream), pcVar2 != (char *)0x0) {
      if ((iVar4 == 5000) || (uVar1 = (tool_call->result).length, 0xffff < uVar1))
      goto LAB_00014990;
      uVar1 = 0x10000 - uVar1;
      length = strlen(acStack_102c);
      if (uVar1 < length) {
        length = uVar1;
      }
      ds_appendn(str,acStack_102c,length);
      iVar4 = iVar4 + 1;
    }
    if ((4999 < iVar4) || (0xffff < (tool_call->result).length)) {
LAB_00014990:
      ds_append(str,"\n... [output truncated after ~64KB] ...\n");
    }
    uVar1 = pclose(stream);
    if (((uVar1 & 0x7f) == 0) && ((int)uVar1 >> 8 != 0)) {
      ds_appendf(str,"\n[exit code: %d]",(int)uVar1 >> 8);
      iVar4 = 0;
    }
    else {
      iVar4 = 0;
    }
  }
  return iVar4;

#undef RStack_1038
#undef acStack_102c
}

/*
 * _glob_walk @ 0x000149e8..0x00014b50
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Recursively match filesystem entries against a supplied glob pattern.
 * Parameters/return (inferred): void _glob_walk(char *param_1,undefined4 param_2,RecoveredDString *param_3,char *param_4,int param_5 )
 * Direct callers: _glob_walk@0x000149e8, _tool_glob@0x00014b50, _tool_read_project@0x00016324
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; opens a directory for enumeration; reads directory entries; reads filesystem metadata
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000149e8_glob_walk.s
 */
void glob_walk(char *param_1,undefined4 param_2,RecoveredDString *param_3,char *param_4,int param_5
               )

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x4b0] __attribute__((aligned(16)));
#define RStack_498 (*((RecoveredTigerStat *)(app_frame + 0x8))) /* original SP - 0x498 */
#define acStack_438 (*((char (*)[1036])(app_frame + 0x68))) /* original SP - 0x438 */

  void *directory;
  int iVar1;
  uint length;
  RecoveredTigerDirent *pRVar2;
  char *pcVar3;


  if (param_5 < 0x11) {
    directory = opendir(param_1);
    if (directory != (void *)0x0) {
      while( true ) {
        pRVar2 = (RecoveredTigerDirent *)(readdir(directory));
        if (pRVar2 == (RecoveredTigerDirent *)0x0) break;
        if ((pRVar2->name[0] != '\0') && (pRVar2->name[0] != '.')) {
          snprintf(acStack_438,0x400,"%s/%s",param_1,pRVar2->name);
          iVar1 = stat(acStack_438,(struct stat *)(&RStack_498));
          if (iVar1 == 0) {
            if ((RStack_498.mode & 0xf000) == 0x4000) {
              glob_walk(acStack_438,param_2,param_3,param_4,param_5 + 1);
            }
            else if ((RStack_498.mode & 0xf000) == 0x8000) {
              length = strlen(param_4);
              iVar1 = strncmp(acStack_438,param_4,length);
              pcVar3 = acStack_438;
              if (iVar1 == 0) {
                pcVar3 = acStack_438;
                if (acStack_438[length] == '/') {
                  pcVar3 = acStack_438 + length + 1;
                }
              }
              iVar1 = fnmatch((const char *)(param_2),pcVar3,0);
              if (iVar1 == 0) {
                ds_appendf(param_3,"%s\n",pcVar3);
              }
            }
          }
        }
      }
      closedir(directory);
    }
  }
  return;

#undef RStack_498
#undef acStack_438
}

/*
 * _tool_glob @ 0x00014b50..0x00014bd8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Run recursive glob matching rooted at the configured working directory.
 * Parameters/return (inferred): int _tool_glob(RecoveredToolCall *tool_call,RecoveredAppState *app_state)
 * Direct callers: _tool_execute@0x000158c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; opens a directory for enumeration; reads directory entries; reads filesystem metadata
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00014b50_tool_glob.s
 */
int tool_glob(RecoveredToolCall *tool_call,RecoveredAppState *app_state)

{
  char *pcVar1;
  char *pcVar2;
  RecoveredDString *str;
  
  str = &tool_call->result;
  ds_clear(str);
  pcVar1 = ds_cstr(tool_call->arguments);
  pcVar2 = (app_state->config).working_directory;
  glob_walk(pcVar2,(undefined4)(pcVar1),str,pcVar2,0);
  if ((tool_call->result).length == 0) {
    ds_append(str,"No files matched the pattern.\n");
  }
  return 0;
}

/*
 * _tool_grep @ 0x00014bd8..0x00014cf8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Interpolate the regex into a single-quoted grep pipeline, capture output and truncate matches.
 * Parameters/return (inferred): int _tool_grep(RecoveredToolCall *tool_call,RecoveredAppState *app_state)
 * Direct callers: _tool_execute@0x000158c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; reads text from a stream; executes a shell command and opens a pipe
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00014bd8_tool_grep.s
 * Preserved behavior/security: SHELL-01: pattern apostrophes can escape the generated shell command; normal outer tool approval remains a prerequisite.
 */
int tool_grep(RecoveredToolCall *tool_call,RecoveredAppState *app_state)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x1040] __attribute__((aligned(16)));
#define RStack_1028 (*((RecoveredDString *)(app_frame + 0x8))) /* original SP - 0x1028 */
#define acStack_101c (*((char (*)[4108])(app_frame + 0x14))) /* original SP - 0x101c */

  char *pcVar1;
  void *stream;
  int iVar2;
  RecoveredDString *str;
  int iVar3;


  ds_init(&RStack_1028);
  pcVar1 = ds_cstr(tool_call->arguments);
  ds_appendf(&RStack_1028,
              "cd \'%s\' && grep -rn --include=\'*\' -E \'%s\' . 2>/dev/null | head -%d",
              (app_state->config).working_directory,pcVar1,0x65);
  pcVar1 = ds_cstr(&RStack_1028);
  stream = popen(pcVar1,"r");
  ds_free(&RStack_1028);
  str = &tool_call->result;
  ds_clear(str);
  iVar3 = 0;
  if (stream == (void *)0x0) {
    ds_append(str,"Error: Could not execute grep.\n");
    iVar2 = -1;
  }
  else {
    while (pcVar1 = fgets(acStack_101c,0x1000,stream), pcVar1 != (char *)0x0) {
      if (iVar3 == 100) {
        ds_append(str,"\n... [truncated after 100 matches] ...\n");
        break;
      }
      ds_append(str,acStack_101c);
      iVar3 = iVar3 + 1;
    }
    pclose(stream);
    iVar2 = 0;
    if (iVar3 == 0) {
      ds_append(str,"No matches found.\n");
      iVar2 = 0;
    }
  }
  return iVar2;

#undef RStack_1028
#undef acStack_101c
}

/*
 * _tool_search @ 0x00014cf8..0x00014d94
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Invoke the configured web-search implementation and return text to the model.
 * Parameters/return (inferred): int _tool_search(RecoveredToolCall *tool_call,RecoveredAppState *app_state)
 * Direct callers: _tool_execute@0x000158c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00014cf8_tool_search.s
 */
int tool_search(RecoveredToolCall *tool_call,RecoveredAppState *app_state)

{
  (void)app_state; /* Present in the recovered ABI; unused by this routine. */
  char *text;
  char *pcVar1;
  int iVar2;
  RecoveredDString *str;
  
  str = &tool_call->result;
  ds_clear(str);
  text = malloc(0x10000);
  if (text == (char *)0x0) {
    ds_appendf(str,"Error: out of memory in search tool\n");
    iVar2 = -1;
  }
  else {
    pcVar1 = ds_cstr(tool_call->arguments);
    iVar2 = search_web(pcVar1,text,0x10000);
    ds_append(str,text);
    free(text);
  }
  return iVar2;
}

/*
 * _tool_fetch_url @ 0x00014d94..0x00014e08
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Fetch the supplied URL as text into a bounded output buffer.
 * Parameters/return (inferred): int _tool_fetch_url(RecoveredToolCall *tool_call,RecoveredAppState *app_state)
 * Direct callers: _tool_execute@0x000158c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00014d94_tool_fetch_url.s
 */
int tool_fetch_url(RecoveredToolCall *tool_call,RecoveredAppState *app_state)

{
  (void)app_state; /* Present in the recovered ABI; unused by this routine. */
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x10030] __attribute__((aligned(16)));
#define acStack_10018 (*((char (*)[65544])(app_frame + 0x8))) /* original SP - 0x10018 */

  char *url;
  int iVar1;

  ds_clear(&tool_call->result);
  url = ds_cstr(tool_call->arguments);
  iVar1 = fetch_url(url,"text",acStack_10018,0x10000);
  ds_append(&tool_call->result,acStack_10018);
  return iVar1;

#undef acStack_10018
}

/*
 * _djb2_hash @ 0x00014e08..0x00014e48
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in tools: djb2 hash. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): uint _djb2_hash(char *text)
 * Direct callers: _cache_key@0x00014e48
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00014e08_djb2_hash.s
 */
uint djb2_hash(char *text)

{
  uint uVar1;
  char *pcVar2;
  
  uVar1 = (uint)(byte)*text;
  pcVar2 = "usr/lib/dyld";
  if (uVar1 != 0) {
    pcVar2 = "usr/lib/dyld";
    do {
      text = text + 1;
      pcVar2 = (char *)((int)pcVar2 * 0x21 + uVar1);
      uVar1 = (uint)(byte)*text;
    } while (uVar1 != 0);
  }
  return (uint)pcVar2;
}

/*
 * _cache_key @ 0x00014e48..0x00014ec0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Build the tool-result cache lookup key.
 * Parameters/return (inferred): uint _cache_key(int tool_type,RecoveredDString *arguments,int argument_count)
 * Direct callers: _cache_find@0x00014f78, _cache_store@0x00015010
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00014e48_cache_key.s
 */
uint cache_key(int tool_type,RecoveredDString *arguments,int argument_count)

{
  char *text;
  uint uVar1;
  int iVar2;
  
  if (0 < argument_count) {
    iVar2 = 0;
    do {
      text = ds_cstr(arguments);
      if (text != (char *)0x0) {
        uVar1 = djb2_hash(text);
        tool_type = tool_type * 0x21 + uVar1;
      }
      iVar2 = iVar2 + 1;
    } while ((argument_count != iVar2) && (arguments = arguments + 1, iVar2 != 99));
  }
  return tool_type;
}

/*
 * _cache_is_readonly @ 0x00014ec0..0x00014f10
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Classify a tool operation for cache eligibility.
 * Parameters/return (inferred): int _cache_is_readonly(int tool_type)
 * Direct callers: _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00014ec0_cache_is_readonly.s
 */
int cache_is_readonly(int tool_type)

{
  int iVar1;
  
  if (((((tool_type == 0) || (tool_type == 4)) || (tool_type == 5)) ||
      ((tool_type == 6 || (tool_type == 7)))) ||
     ((tool_type == 8 || ((tool_type == 0xe || (iVar1 = 0, tool_type == 0xf)))))) {
    iVar1 = 1;
  }
  return iVar1;
}

/*
 * _cache_init @ 0x00014f10..0x00014f78
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage tool-result cache state: init. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _cache_init(RecoveredAppState *app)
 * Direct callers: _tool_execute@0x000158c8, _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00014f10_cache_init.s
 */
void cache_init(RecoveredAppState *app)

{
  bool bVar1;
  int iVar2;
  int *piVar3;
  
  piVar3 = &app->cache[0].valid;
  iVar2 = 0;
  do {
    *piVar3 = 0;
    ((RecoveredCacheEntry *)(piVar3 + -2))->key = 0;
    piVar3[-1] = 0;
    ds_init((RecoveredDString *)
             (_g_state.history.entries[0] +
             (int)((app->history).entries[0xb5] + iVar2 * 0x18 + 0x3b2c)));
    piVar3 = piVar3 + 6;
    bVar1 = iVar2 != 0xf;
    iVar2 = iVar2 + 1;
  } while (bVar1);
  return;
}

/*
 * _cache_find @ 0x00014f78..0x00015010
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage tool-result cache state: find. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): RecoveredCacheEntry * _cache_find(RecoveredAppState *app,int tool_type,RecoveredDString *arguments,int argument_count)
 * Direct callers: _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00014f78_cache_find.s
 */
RecoveredCacheEntry *
cache_find(RecoveredAppState *app,int tool_type,RecoveredDString *arguments,int argument_count)

{
  int *piVar1;
  uint uVar2;
  int iVar3;
  int iVar4;
  
  uVar2 = cache_key(tool_type,arguments,argument_count);
  piVar1 = &app->cache[0].valid;
  iVar3 = 0;
  iVar4 = 0x10;
  while (((*piVar1 == 0 || (uVar2 != ((RecoveredCacheEntry *)(piVar1 + -2))->key)) ||
         (tool_type != piVar1[-1]))) {
    iVar3 = iVar3 + 1;
    piVar1 = piVar1 + 6;
    iVar4 = iVar4 + -1;
    if (iVar4 == 0) {
      return (RecoveredCacheEntry *)0x0;
    }
  }
  return app->cache + iVar3;
}

/*
 * _cache_store @ 0x00015010..0x000150d0
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage tool-result cache state: store. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _cache_store(RecoveredAppState *app,int tool_type,RecoveredDString *arguments, int argument_count,RecoveredDString *result)
 * Direct callers: _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00015010_cache_store.s
 */
void cache_store(RecoveredAppState *app,int tool_type,RecoveredDString *arguments,
                 int argument_count,RecoveredDString *result)

{
  uint uVar1;
  int iVar2;
  char *text;
  RecoveredCacheEntry *pRVar3;
  
  uVar1 = _next_4649 + 1;
  iVar2 = uVar1 + (((int)uVar1 >> 4) + (uint)((int)uVar1 < 0 && (uVar1 & 0xf) != 0)) * -0x10;
  pRVar3 = app->cache + _next_4649;
  _next_4649 = iVar2;
  if (pRVar3->valid != 0) {
    ds_free(&pRVar3->result);
  }
  uVar1 = cache_key(tool_type,arguments,argument_count);
  pRVar3->key = uVar1;
  pRVar3->tool_type = tool_type;
  pRVar3->valid = 1;
  ds_init(&pRVar3->result);
  text = ds_cstr(result);
  ds_append(&pRVar3->result,text);
  return;
}

/*
 * _cache_clear @ 0x000150d0..0x0001513c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage tool-result cache state: clear. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _cache_clear(RecoveredAppState *app)
 * Direct callers: _handle_slash_command@0x0000362c, _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000150d0_cache_clear.s
 */
void cache_clear(RecoveredAppState *app)

{
  bool bVar1;
  int iVar2;
  int *piVar3;
  
  piVar3 = &app->cache[0].valid;
  iVar2 = 0;
  do {
    if (*piVar3 != 0) {
      ds_free((RecoveredDString *)
               (_g_state.history.entries[0] +
               (int)((app->history).entries[0xb5] + iVar2 * 0x18 + 0x3b2c)));
      *piVar3 = 0;
    }
    piVar3 = piVar3 + 6;
    bVar1 = iVar2 != 0xf;
    iVar2 = iVar2 + 1;
  } while (bVar1);
  return;
}

/*
 * _tool_db @ 0x0001513c..0x000151c4
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Forward database path, operation and query text to the SQLite tool dispatcher.
 * Parameters/return (inferred): int _tool_db(RecoveredToolCall *tool_call,RecoveredAppState *app_state)
 * Direct callers: _tool_execute@0x000158c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; executes SQL text; opens/creates a SQLite database; prepares SQL text
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0001513c_tool_db.s
 */
int tool_db(RecoveredToolCall *tool_call,RecoveredAppState *app_state)

{
  (void)app_state; /* Present in the recovered ABI; unused by this routine. */
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x10040] __attribute__((aligned(16)));
#define acStack_10028 (*((char (*)[65552])(app_frame + 0x8))) /* original SP - 0x10028 */

  char *pcVar1;
  char *pcVar2;
  char *pcVar3;
  int iVar4;

  ds_clear(&tool_call->result);
  pcVar1 = ds_cstr(tool_call->arguments);
  pcVar2 = ds_cstr(tool_call->arguments + 1);
  pcVar3 = ds_cstr(tool_call->arguments + 2);
  iVar4 = db_tool_execute(pcVar1,pcVar2,pcVar3,acStack_10028,0x10000);
  ds_append(&tool_call->result,acStack_10028);
  return iVar4;

#undef acStack_10028
}

/*
 * _trust_record_failure @ 0x000151c4..0x000151e8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage per-tool failure/trust heuristics: record failure. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _trust_record_failure(RecoveredAppState *app,uint tool_type)
 * Direct callers: _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000151c4_trust_record_failure.s
 */
void trust_record_failure(RecoveredAppState *app,uint tool_type)

{
  if (99 < tool_type) {
    return;
  }
  *(int *)(_g_state.history.entries[0] +
          (int)((app->history).entries[0xb5] + tool_type * 4 + 0x3ca0)) =
       *(int *)(_g_state.history.entries[0] +
               (int)((app->history).entries[0xb5] + tool_type * 4 + 0x3ca0)) + 1;
  return;
}

/*
 * _trust_is_trusted @ 0x000151e8..0x00015214
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage per-tool failure/trust heuristics: is trusted. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): int _trust_is_trusted(uint tool_type,int *failures)
 * Direct callers: _tools_schema_add_simple@0x0000a510, _tools_schema_add_read_and_edit@0x0000a68c, _tools_schema_add_fetch_url@0x0000a8ac, _tools_schema_add_db_query@0x0000aa20, _tools_schema_build_standard@0x0000acd4, _openai_build_tools_array@0x0000ccb8, _anthropic_build_tools_array@0x0000e7a4, _google_build_tools@0x00010768
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000151e8_trust_is_trusted.s
 */
int trust_is_trusted(uint tool_type,int *failures)

{
  if (99 < tool_type) {
    return 1;
  }
  return (uint)(failures[tool_type] < 5);
}

/*
 * _trust_warning @ 0x00015214..0x00015290
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage per-tool failure/trust heuristics: warning. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): char * _trust_warning(uint tool_type,int *failures)
 * Direct callers: _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00015214_trust_warning.s
 */
char * trust_warning(uint tool_type,int *failures)

{
  char *pcVar1;
  char *pcVar2;
  int iVar3;
  
  if ((tool_type < 100) && (iVar3 = failures[tool_type], iVar3 - 3U < 2)) {
    pcVar1 = tool_name(tool_type);
    pcVar2 = &_buf_4749;
    snprintf(&_buf_4749,0x100,"[WARNING: %s has failed %d times recently]",pcVar1,iVar3);
  }
  else {
    pcVar2 = (char *)0x0;
  }
  return pcVar2;
}

/*
 * _trust_reset @ 0x00015290..0x000152b4
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage per-tool failure/trust heuristics: reset. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _trust_reset(RecoveredAppState *app)
 * Direct callers: _handle_slash_command@0x0000362c, _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00015290_trust_reset.s
 */
void trust_reset(RecoveredAppState *app)

{
  int *piVar1;
  int iVar2;
  
  piVar1 = app->tool_failures;
  iVar2 = 100;
  do {
    *piVar1 = 0;
    piVar1 = piVar1 + 1;
    iVar2 = iVar2 + -1;
  } while (iVar2 != 0);
  return;
}

/*
 * _tool_memory_remember @ 0x000152b4..0x00015410
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Write tagged remembered text through a PID-suffixed temporary file and rename to memory.txt.
 * Parameters/return (inferred): int _tool_memory_remember(RecoveredToolCall *tool_call,RecoveredAppState *app_state)
 * Direct callers: _tool_execute@0x000158c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; opens/creates/truncates filesystem paths according to the supplied mode; reads process environment; creates directories; renames/replaces filesystem entries
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000152b4_tool_memory_remember.s
 */
int tool_memory_remember(RecoveredToolCall *tool_call,RecoveredAppState *app_state)

{
  (void)app_state; /* Present in the recovered ABI; unused by this routine. */
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0xc40] __attribute__((aligned(16)));
#define acStack_c28 (*((char (*)[1024])(app_frame + 0x8))) /* original SP - 0xc28 */
#define acStack_828 (*((char (*)[1024])(app_frame + 0x408))) /* original SP - 0x828 */
#define acStack_428 (*((char (*)[1040])(app_frame + 0x808))) /* original SP - 0x428 */

  char *pcVar1;
  char *pcVar2;
  char *pcVar3;
  pid_t pVar4;
  void *stream;
  int iVar5;

  pcVar1 = ds_cstr(tool_call->arguments);
  pcVar2 = ds_cstr(tool_call->arguments + 1);
  pcVar3 = getenv("HOME");
  if (pcVar3 != (char *)0x0) {
    if (*pcVar3 != '\0') goto LAB_00015310;
  }
  pcVar3 = "/tmp";
LAB_00015310:
  snprintf(acStack_c28,0x400,"%s/.ppccode",pcVar3);
  mkdir(acStack_c28,0x1ed);
  app_format_truncated(acStack_828,0x400,"%s/memory.txt",acStack_c28);
  pVar4 = getpid();
  app_format_truncated(acStack_428,0x400,"%s.tmp.%d",acStack_828,pVar4);
  stream = fopen(acStack_428,"a");
  if (stream == (void *)0x0) {
    ds_clear(&tool_call->result);
    ds_appendf(&tool_call->result,"Error: cannot open memory file");
    iVar5 = -1;
  }
  else {
    fprintf(stream,"[%s]\n%s\n\n",pcVar1,pcVar2);
    fclose(stream);
    rename(acStack_428,acStack_828);
    ds_clear(&tool_call->result);
    ds_appendf(&tool_call->result,"Remembered under tag \'%s\'",pcVar1);
    iVar5 = 0;
  }
  return iVar5;

#undef acStack_c28
#undef acStack_828
#undef acStack_428
}

/*
 * _tool_memory_load @ 0x00015410..0x00015544
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Load remembered text from the HOME-or-/tmp .ppccode memory store.
 * Parameters/return (inferred): int _tool_memory_load(RecoveredToolCall *tool_call,RecoveredAppState *app_state)
 * Direct callers: _tool_execute@0x000158c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; reads text from a stream; opens/creates/truncates filesystem paths according to the supplied mode; reads process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00015410_tool_memory_load.s
 */
int tool_memory_load(RecoveredToolCall *tool_call,RecoveredAppState *app_state)

{
  (void)app_state; /* Present in the recovered ABI; unused by this routine. */
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x1440] __attribute__((aligned(16)));
#define acStack_1428 (*((char (*)[1024])(app_frame + 0x8))) /* original SP - 0x1428 */
#define local_1028 (*((char (*)[4112])(app_frame + 0x408))) /* original SP - 0x1028 */

  bool bVar1;
  char *pcVar2;
  char *pcVar3;
  void *stream;
  RecoveredDString *str;
  bool bVar4;


  pcVar2 = ds_cstr(tool_call->arguments);
  pcVar3 = getenv("HOME");
  if (pcVar3 != (char *)0x0) {
    if (*pcVar3 != '\0') goto LAB_00015460;
  }
  pcVar3 = "/tmp";
LAB_00015460:
  snprintf(acStack_1428,0x400,"%s/.ppccode/memory.txt",pcVar3);
  str = &tool_call->result;
  ds_clear(str);
  stream = fopen(acStack_1428,"r");
  if (stream == (void *)0x0) {
    ds_appendf(str,"No memory found (file does not exist).");
  }
  else {
    bVar4 = false;
    bVar1 = false;
    while( true ) {
      pcVar3 = fgets(local_1028,0x1000,stream);
      if (pcVar3 == (char *)0x0) break;
      if (local_1028[0] == '[') {
        pcVar3 = strcasestr(local_1028,pcVar2);
        bVar4 = pcVar3 != (char *)0x0;
      }
      if (bVar4) {
        ds_append(str,local_1028);
        bVar1 = true;
      }
    }
    fclose(stream);
    if (!bVar1) {
      ds_appendf(str,"No entries matching \'%s\'.",pcVar2);
    }
  }
  return 0;

#undef acStack_1428
#undef local_1028
}

/*
 * _tool_read_and_edit @ 0x00015544..0x00015748
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Combine a file read and an edit using temporary internal tool-call records.
 * Parameters/return (inferred): int _tool_read_and_edit(RecoveredToolCall *tool_call,RecoveredAppState *app_state)
 * Direct callers: _tool_execute@0x000158c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; executes an argument vector through PATH; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; reads bytes from a stream; writes bytes to a stream; securely creates a temporary file; reads a descriptor or IPC channel; reads filesystem metadata; removes a filesystem entry; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00015544_tool_read_and_edit.s
 */
int tool_read_and_edit(RecoveredToolCall *tool_call,RecoveredAppState *app_state)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0xa90] __attribute__((aligned(16)));
#define RStack_a78 (*((RecoveredToolCall *)(app_frame + 0x8))) /* original SP - 0xa78 */
#define RStack_55c (*((RecoveredToolCall *)(app_frame + 0x524))) /* original SP - 0x55c */

  char *pcVar1;
  int iVar2;
  int iVar3;
  RecoveredDString *str;
  RecoveredDString *str_00;
  RecoveredDString *str_01;
  RecoveredDString *str_02;
  RecoveredDString *str_03;
  RecoveredDString *str_04;


  memset(&RStack_a78,0,0x51c);
  ds_init(&RStack_a78.result);
  ds_init(RStack_a78.arguments);
  str_04 = tool_call->arguments;
  pcVar1 = ds_cstr(str_04);
  ds_append(RStack_a78.arguments,pcVar1);
  RStack_a78.argument_count = 1;
  iVar2 = tool_read_file(&RStack_a78,app_state);
  str = &tool_call->result;
  ds_clear(str);
  pcVar1 = ds_cstr(str_04);
  ds_appendf(str,"=== Read: %s ===\n",pcVar1);
  if (iVar2 == 0) {
    pcVar1 = ds_cstr(&RStack_a78.result);
    ds_append(str,pcVar1);
  }
  else {
    pcVar1 = ds_cstr(&RStack_a78.result);
    ds_appendf(str,"[Read failed: %s]\n",pcVar1);
  }
  ds_free(&RStack_a78.result);
  ds_free(RStack_a78.arguments);
  memset(&RStack_55c,0,0x51c);
  str_00 = &RStack_55c.result;
  ds_init(str_00);
  str_01 = RStack_55c.arguments;
  ds_init(str_01);
  pcVar1 = ds_cstr(str_04);
  ds_append(str_01,pcVar1);
  str_02 = RStack_55c.arguments + 1;
  ds_init(str_02);
  pcVar1 = ds_cstr(tool_call->arguments + 1);
  ds_append(str_02,pcVar1);
  str_03 = RStack_55c.arguments + 2;
  ds_init(str_03);
  pcVar1 = ds_cstr(tool_call->arguments + 2);
  ds_append(str_03,pcVar1);
  RStack_55c.argument_count = 3;
  iVar3 = tool_edit_file(&RStack_55c,app_state);
  pcVar1 = ds_cstr(str_04);
  ds_appendf(str,"\n=== Edit: %s ===\n",pcVar1);
  pcVar1 = ds_cstr(str_00);
  ds_append(str,pcVar1);
  ds_free(str_00);
  ds_free(str_01);
  ds_free(str_02);
  ds_free(str_03);
  if ((iVar2 != 0) || (iVar2 = 0, iVar3 != 0)) {
    iVar2 = -1;
  }
  return iVar2;

#undef RStack_a78
#undef RStack_55c
}

/*
 * _tool_subagent @ 0x00015748..0x000158c8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Pass effective provider/model/key/URL/directory to a delegated task, wait up to 300 seconds and collect its result.
 * Parameters/return (inferred): int _tool_subagent(RecoveredToolCall *tool_call,RecoveredAppState *app_state)
 * Direct callers: _tool_execute@0x000158c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; performs network/protocol I/O; resolves/executes a program through PATH; executes an argument vector through PATH; reads text from a stream; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; writes text to a stream; reads bytes from a stream; writes bytes to a stream; reads the process working directory; reads process environment; signals a process; creates directories; securely creates a temporary file; opens a directory for enumeration; executes a shell command and opens a pipe; reads a descriptor or IPC channel; reads directory entries; resolves a filesystem path; renames/replaces filesystem entries; executes SQL text; opens/creates a SQLite database; prepares SQL text; reads filesystem metadata; removes a filesystem entry; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00015748_tool_subagent.s
 */
int tool_subagent(RecoveredToolCall *tool_call,RecoveredAppState *app_state)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x14450] __attribute__((aligned(16)));
#define RStack_14438 (*((RecoveredSubagent *)(app_frame + 0x8))) /* original SP - 0x14438 */
#define acStack_10028 (*((char (*)[65548])(app_frame + 0x4418))) /* original SP - 0x10028 */

  char *task;
  char *model;
  char *key;
  char *url;
  int iVar1;
  time_t tVar2;
  time_t tVar3;
  RecoveredDString *str;


  str = &tool_call->result;
  ds_clear(str);
  task = ds_cstr(tool_call->arguments);
  if (tool_call->argument_count < 2) {
    model = (app_state->config).model;
  }
  else {
    model = ds_cstr(tool_call->arguments + 1);
  }
  key = config_get_key(&app_state->config);
  url = config_get_url(&app_state->config);
  iVar1 = subagent_start(&RStack_14438,task,model,(app_state->config).working_directory,key,url,
                          (app_state->config).provider);
  if (iVar1 == 0) {
    tVar2 = time((time_t *)0x0);
    do {
      iVar1 = subagent_poll(&RStack_14438);
      if (iVar1 != 0) {
        iVar1 = subagent_result(&RStack_14438,acStack_10028,0x10000);
        if (iVar1 == 0) {
          ds_append(str,acStack_10028);
        }
        else {
          ds_append(str,"Error: subagent failed");
        }
        subagent_cleanup(&RStack_14438);
        return -(uint)(RStack_14438.succeeded == 0);
      }
      usleep(100000);
      tVar3 = time((time_t *)0x0);
    } while (tVar3 - tVar2 < 0x12d);
    subagent_cleanup(&RStack_14438);
    ds_append(str,"Error: subagent timed out after 5 minutes");
  }
  else {
    ds_append(str,"Error: could not start subagent");
  }
  return -1;

#undef RStack_14438
#undef acStack_10028
}

/*
 * _tool_execute @ 0x000158c8..0x00015d30
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Dispatch standard tool types 0..15 or configured MCP types >=100, with exploration-loop and repeat-action tracking.
 * Parameters/return (inferred): int _tool_execute(RecoveredToolCall *tool_call,RecoveredAppState *app_state)
 * Direct callers: _simple_mode@0x00004e30, _sa_child_main@0x0001327c, _execute_plan_steps@0x00022454, _worker_child_main@0x00022774, _executor_run_step@0x00025704
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; performs network/protocol I/O; resolves/executes a program through PATH; executes an argument vector through PATH; reads text from a stream; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; writes text to a stream; reads bytes from a stream; writes bytes to a stream; reads the process working directory; reads process environment; signals a process; creates directories; securely creates a temporary file; opens a directory for enumeration; executes a shell command and opens a pipe; reads a descriptor or IPC channel; reads directory entries; resolves a filesystem path; renames/replaces filesystem entries; executes SQL text; opens/creates a SQLite database; prepares SQL text; reads filesystem metadata; removes a filesystem entry; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000158c8_tool_execute.s
 */
int tool_execute(RecoveredToolCall *tool_call,RecoveredAppState *app_state)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x10040] __attribute__((aligned(16)));
#define acStack_10028 (*((char (*)[65548])(app_frame + 0x8))) /* original SP - 0x10028 */

  int iVar1;
  int iVar2;
  char *pcVar3;
  char *pcVar4;

  iVar1 = tool_call->tool_type;
  if ((((iVar1 == 0) || (iVar1 == 4)) || (iVar1 == 3)) || (iVar1 == 6)) {
    _g_loop_breaker = _g_loop_breaker + 1;
    if (500 < _g_loop_breaker) {
      ds_clear(&tool_call->result);
      ds_append(&tool_call->result,
                 "Error: Too many exploration steps (500). Please stop searching/reading and summarize what you have found so far for the user."
                );
      return -1;
    }
  }
  else if ((iVar1 - 1U < 2) || (iVar1 == 0xb)) {
    _g_loop_breaker = 0;
  }
  if (((DAT_0003b980 != 0) && (DAT_0003b17c == (uint32_t)tool_call->tool_type)) && (DAT_0003b984 != 0)) {
    if (0 < tool_call->argument_count) {
      iVar1 = 0;
      pcVar4 = (char *)(&DAT_0003b180);
      do {
        pcVar3 = ds_cstr(tool_call->arguments + iVar1);
        iVar2 = strcmp(pcVar4,pcVar3);
        if (iVar2 != 0) goto LAB_000159a0;
        iVar1 = iVar1 + 1;
      } while ((iVar1 < tool_call->argument_count) && (pcVar4 = pcVar4 + 0x400, iVar1 != 2));
    }
    iVar1 = tool_call->tool_type;
    if ((iVar1 != 1) && ((iVar1 != 2 && (iVar1 != 0xb)))) {
      ds_clear(&tool_call->result);
      ds_append(&tool_call->result,
                 "Error: You have already performed this exact action. Repeating the same exploration tool call is forbidden. Move to the next step of your plan or finish the task."
                );
      return -1;
    }
  }
LAB_000159a0:
  DAT_0003b17c = tool_call->tool_type;
  if (0 < tool_call->argument_count) {
    iVar1 = 0;
    pcVar4 = (char *)(&DAT_0003b180);
    do {
      pcVar3 = ds_cstr(tool_call->arguments + iVar1);
      strncpy(pcVar4,pcVar3,0x3ff);
      pcVar4[0x3ff] = '\0';
      iVar1 = iVar1 + 1;
      if (tool_call->argument_count <= iVar1) break;
      pcVar4 = pcVar4 + 0x400;
    } while (iVar1 != 2);
  }
  DAT_0003b980 = 1;
  DAT_0003b984 = 1;
  cache_init(app_state);
  tool_call->execution_status = 1;
  switch(tool_call->tool_type) {
  case 0:
    iVar1 = tool_read_file(tool_call,app_state);
    break;
  case 1:
    iVar1 = tool_write_file(tool_call,app_state);
    break;
  case 2:
    iVar1 = tool_edit_file(tool_call,app_state);
    break;
  case 3:
    iVar1 = tool_shell(tool_call,app_state);
    break;
  case 4:
    iVar1 = tool_glob(tool_call,app_state);
    break;
  case 5:
    iVar1 = tool_grep(tool_call,app_state);
    break;
  case 6:
    iVar1 = tool_search(tool_call,app_state);
    break;
  case 7:
    iVar1 = tool_fetch_url(tool_call,app_state);
    break;
  case 8:
    iVar1 = tool_db(tool_call,app_state);
    break;
  case 9:
    iVar1 = tool_memory_remember(tool_call,app_state);
    break;
  case 10:
    iVar1 = tool_memory_load(tool_call,app_state);
    break;
  case 0xb:
    iVar1 = tool_read_and_edit(tool_call,app_state);
    break;
  case 0xc:
    iVar1 = tool_subagent(tool_call,app_state);
    break;
  case 0xd:
    ds_clear(&tool_call->result);
    if ((app_state == (RecoveredAppState *)0x0) ||
       (pcVar4 = (app_state->config).ollama_url, (app_state->config).ollama_url[0] == '\0')) {
      pcVar4 = (char *)0x0;
    }
    pcVar3 = ds_cstr(tool_call->arguments);
    iVar1 = rag_search(pcVar3,acStack_10028,0x10000,pcVar4);
    ds_append(&tool_call->result,acStack_10028);
    break;
  case 0xe:
    iVar1 = tool_tree(tool_call,app_state);
    break;
  case 0xf:
    iVar1 = tool_read_project(tool_call,app_state);
    break;
  default:
    if (99 < (uint)tool_call->tool_type) {
      iVar1 = tool_execute_mcp(tool_call,app_state);
      break;
    }
    ds_clear(&tool_call->result);
    ds_append(&tool_call->result,"Unknown tool type.\n");
    iVar1 = -1;
    goto LAB_00015cf4;
  }
  if (iVar1 != 0) {
LAB_00015cf4:
    DAT_0003b984 = 0;
  }
  return iVar1;

#undef acStack_10028
}

/*
 * _tree_should_skip @ 0x00015d30..0x00015d9c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in tools: tree should skip. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): undefined4 _tree_should_skip(undefined4 *param_1,char *param_2)
 * Direct callers: _tree_walk@0x00015d9c
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00015d30_tree_should_skip.s
 */
undefined4 tree_should_skip(undefined4 *param_1,char *param_2)

{
  int iVar1;
  int iVar2;
  int iVar3;
  
  iVar2 = param_1[0x10];
  if (0 < iVar2) {
    iVar3 = 0;
    do {
      iVar1 = strcmp(param_2,(char *)*param_1);
      if (iVar1 == 0) {
        return 1;
      }
      iVar3 = iVar3 + 1;
      param_1 = param_1 + 1;
    } while (iVar2 != iVar3);
  }
  return 0;
}

/*
 * _tree_walk @ 0x00015d9c..0x00016158
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in tools: tree walk. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _tree_walk(char *path, int depth, int maximum_depth, RecoveredDString *output, void *skip_rules)
 * Direct callers: _tree_walk@0x00015d9c, _tool_tree@0x00016158
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; opens a directory for enumeration; reads directory entries; resolves a filesystem path; reads filesystem metadata
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the separately documented manual assembly reconstruction, not the warning-bearing Ghidra body.
 * Exact assembly: assembly/functions/00015d9c_tree_walk.s
 * Uncertainty: Exact application SDK version is unknown; observed stat/dirent offsets are corroborated by archived Tiger-era XNU headers.
 * Uncertainty: Original Ghidra type-propagation warning was addressed by an explicit assembly review; both tentative tool views remain retained.
 */
/* Analyst reconstruction of _tree_walk, VA 0x00015d9c..0x00016158.
 * NOT original source. Reviewed against the complete 239-instruction listing.
 * PPC_BE16/PPC_BE32 mean target big-endian loads, not application helper calls.
 * Raw stat bytes avoid importing an incompatible modern struct stat layout.
 * The stat scratch area is 96 bytes: stack+0x4b8 to stack+0x518.
 * Mode is loaded at +8; the displayed size is the signed 32-bit word at +0x34.
 * No missing checks, sorting, symlink policy or truncation handling is added.
 */
int tree_walk(char *path, int depth, int maximum_depth,
               RecoveredDString *output, void *skip_rules)
{


    char canonical[1024];
    char child_path[1024];
    unsigned char tiger_stat_bytes[96];
    char indent[64];
    void *directory;
    void *entry;
    int pushed = 0;
    int printed_directory = 0;

    if (depth > maximum_depth || path == 0 || path[0] == '\0' || depth >= 65)
        return 0;

    if (realpath(path, canonical) == 0) {
        unsigned int length = strlen(path);
        if (length > 1023) length = 1023;
        memcpy(canonical, path, length);
        canonical[length] = '\0';
    }

    if (_g_tree_visited_count < 64) {
        int i;
        for (i = 0; i < _g_tree_visited_count; ++i)
            if (strcmp(_visited_5254[i], canonical) == 0)
                return 0;
        app_copy_padded(_visited_5254[_g_tree_visited_count], canonical, 1023);
        _visited_5254[_g_tree_visited_count][1023] = '\0';
        ++_g_tree_visited_count;
        pushed = 1;
    }

    directory = opendir(canonical);
    if (directory == 0) {
        if (pushed) --_g_tree_visited_count;
        return -1;
    }

    while ((entry = readdir(directory)) != 0) {
        /* This Tiger binary uses d_name at byte offset 8. */
        char *name = (char *)entry + 8;
        unsigned int mode;
        int n;

        if (name[0] == '.' || tree_should_skip(skip_rules, name))
            continue;
        app_format_truncated(child_path, 1024, "%s/%s", canonical, name);
        if (stat(child_path, (struct stat *)(tiger_stat_bytes)) != 0)
            continue;
        mode = PPC_BE16(tiger_stat_bytes + 8) & 0xf000;
        if (mode != 0x4000 && mode != 0x8000)
            continue;

        if (!printed_directory) {
            char *base = strrchr(canonical, '/');
            if (base != 0) ++base;
            else base = canonical;
            n = 0;
            while (n < 60 && n < depth * 2) indent[n++] = ' ';
            indent[n] = '\0';
            ds_appendf(output, "%s%s/\n", indent, base);
            printed_directory = 1;
        }

        if (mode == 0x4000) {
            /* Child return values are intentionally ignored, as in the binary. */
            tree_walk(child_path, depth + 1, maximum_depth, output, skip_rules);
        } else {
            n = 0;
            while (n < 60 && n < depth * 2 + 2) indent[n++] = ' ';
            indent[n] = '\0';
            ds_appendf(output, "%s%s\t%ld bytes\n", indent, name,
                        (long)(int32_t)PPC_BE32(tiger_stat_bytes + 0x34));
        }
    }
    closedir(directory);
    if (pushed) --_g_tree_visited_count;
    return 0;
}

/*
 * _tool_tree @ 0x00016158..0x00016324
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Return a bounded directory-tree representation of the requested workspace.
 * Parameters/return (inferred): int _tool_tree(RecoveredToolCall *tool_call,RecoveredAppState *app_state)
 * Direct callers: _tool_execute@0x000158c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; opens a directory for enumeration; reads directory entries; resolves a filesystem path; reads filesystem metadata
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00016158_tool_tree.s
 */
int tool_tree(RecoveredToolCall *tool_call,RecoveredAppState *app_state)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x490] __attribute__((aligned(16)));
#define acStack_478 (*((char (*)[1023])(app_frame + 0x8))) /* original SP - 0x478 */
#define local_79 (*((undefined1 *)(app_frame + 0x407))) /* original SP - 0x79 */
#define local_78 (*((char * *)(app_frame + 0x408))) /* original SP - 0x78 */
#define local_74 (*((char * *)(app_frame + 0x40c))) /* original SP - 0x74 */
#define local_70 (*((char * *)(app_frame + 0x410))) /* original SP - 0x70 */
#define local_6c (*((char * *)(app_frame + 0x414))) /* original SP - 0x6c */
#define local_68 (*((char * *)(app_frame + 0x418))) /* original SP - 0x68 */
#define local_64 (*((char * *)(app_frame + 0x41c))) /* original SP - 0x64 */
#define local_60 (*((char * *)(app_frame + 0x420))) /* original SP - 0x60 */
#define local_5c (*((char * *)(app_frame + 0x424))) /* original SP - 0x5c */
#define local_58 (*((char * *)(app_frame + 0x428))) /* original SP - 0x58 */
#define local_54 (*((char * *)(app_frame + 0x42c))) /* original SP - 0x54 */
#define local_38 (*((undefined4 *)(app_frame + 0x448))) /* original SP - 0x38 */

  char *text;
  char *text_00;
  int iVar1;
  RecoveredDString *str;

  str = &tool_call->result;
  ds_clear(str);
  if ((tool_call->argument_count < 1) || (tool_call->arguments[0].length == 0)) {
    text = (app_state->config).working_directory;
  }
  else {
    text = ds_cstr(tool_call->arguments);
  }
  if ((tool_call->argument_count < 2) || (tool_call->arguments[1].length == 0)) {
    iVar1 = 4;
  }
  else {
    text_00 = ds_cstr(tool_call->arguments + 1);
    iVar1 = atoi(text_00);
    if (iVar1 < 1) {
      iVar1 = 1;
    }
    else if (8 < iVar1) {
      iVar1 = 8;
    }
  }
  if ((*text == '/') || (*text == '~')) {
    app_copy_padded(acStack_478,text,0x3ff);
    local_79 = 0;
  }
  else {
    app_format_truncated(acStack_478,0x400,"%s/%s",(app_state->config).working_directory,text);
  }
  local_78 = ".git";
  local_74 = ".svn";
  local_70 = "node_modules";
  local_6c = "build";
  local_68 = ".build";
  local_64 = "__pycache__";
  local_60 = ".cache";
  local_5c = "target";
  local_58 = "dist";
  local_54 = "vendor";
  local_38 = 10;
  ds_appendf(str,"=== Tree of %s (depth %d) ===\n",acStack_478,iVar1);
  _g_tree_visited_count = 0;
  tree_walk(acStack_478,0,iVar1,str,&local_78);
  return 0;

#undef acStack_478
#undef local_79
#undef local_78
#undef local_74
#undef local_70
#undef local_6c
#undef local_68
#undef local_64
#undef local_60
#undef local_5c
#undef local_58
#undef local_54
#undef local_38
}

/*
 * _tool_read_project @ 0x00016324..0x000165d0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Collect multiple project files/context for model consumption.
 * Parameters/return (inferred): int _tool_read_project(RecoveredToolCall *tool_call,RecoveredAppState *app_state)
 * Direct callers: _tool_execute@0x000158c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; opens/creates/truncates filesystem paths according to the supplied mode; reads bytes from a stream; opens a directory for enumeration; reads directory entries; reads filesystem metadata
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00016324_tool_read_project.s
 */
int tool_read_project(RecoveredToolCall *tool_call,RecoveredAppState *app_state)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x14c0] __attribute__((aligned(16)));
#define local_14a8 (*((RecoveredDString *)(app_frame + 0x8))) /* original SP - 0x14a8 */
#define local_149c (*((char * *)(app_frame + 0x14))) /* original SP - 0x149c */
#define RStack_1498 (*((RecoveredTigerStat *)(app_frame + 0x18))) /* original SP - 0x1498 */
#define acStack_1438 (*((char (*)[1024])(app_frame + 0x78))) /* original SP - 0x1438 */
#define acStack_1038 (*((char (*)[4104])(app_frame + 0x478))) /* original SP - 0x1038 */

  char *pcVar1;
  char *pcVar2;
  int iVar3;
  int iVar4;
  void *stream;
  uint length;
  int iVar5;
  int iVar6;
  RecoveredDString *str;

  str = &tool_call->result;
  ds_clear(str);
  pcVar1 = ds_cstr(tool_call->arguments);
  if ((pcVar1 == (char *)0x0) || (*pcVar1 == '\0')) {
    ds_append(str,"Error: \'pattern\' argument is required.\n");
    return -1;
  }
  if ((tool_call->argument_count < 2) || (tool_call->arguments[1].length == 0)) {
    iVar3 = 0x10000;
  }
  else {
    pcVar2 = ds_cstr(tool_call->arguments + 1);
    iVar3 = atoi(pcVar2);
    if (iVar3 < 0) {
      iVar3 = 0;
    }
    else if (0x1000000 < iVar3) {
      iVar3 = 0x1000000;
    }
  }
  ds_init(&local_14a8);
  pcVar2 = (app_state->config).working_directory;
  glob_walk(pcVar2,(undefined4)(pcVar1),&local_14a8,pcVar2,0);
  if (local_14a8.length == 0) {
    ds_append(str,"No files matched.\n");
    ds_free(&local_14a8);
    return -1;
  }
  local_149c = (char *)0x0;
  pcVar1 = strtok_r(local_14a8.data,"\n",&local_149c);
  if (pcVar1 == (char *)0x0) {
    iVar5 = 0;
    iVar6 = 0;
  }
  else {
    iVar5 = 0;
    iVar6 = 0;
    do {
      if (*pcVar1 == '\0') {
        pcVar1 = strtok_r((char *)0x0,"\n",&local_149c);
      }
      else {
        app_format_truncated(acStack_1438,0x400,"%s/%s",pcVar2,pcVar1);
        iVar4 = stat(acStack_1438,(struct stat *)(&RStack_1498));
        if ((iVar4 == 0) && ((RStack_1498.mode & 0xf000) == 0x8000)) {
          ds_appendf(str,"\n=== %s (%ld bytes) ===\n",pcVar1,(long int)(APP_FIELD32(RStack_1498.size, 4)));
          stream = fopen(acStack_1438,"r");
          if (stream != (void *)0x0) {
            iVar4 = 0;
            while (length = fread(acStack_1038,1,0x1000,stream), length != 0) {
              if (iVar3 < (int)(length + iVar4)) {
                ds_appendn(str,acStack_1038,iVar3 - iVar4);
                ds_append(str,"\n... [truncated] ...\n");
                break;
              }
              ds_appendn(str,acStack_1038,length);
              iVar6 = iVar6 + length;
              iVar4 = length + iVar4;
            }
            fclose(stream);
          }
          iVar5 = iVar5 + 1;
        }
        pcVar1 = strtok_r((char *)0x0,"\n",&local_149c);
      }
    } while (pcVar1 != (char *)0x0);
  }
  ds_free(&local_14a8);
  ds_appendf(str,"\n=== Read %d file(s), %d bytes total ===\n",iVar5,iVar6);
  return 0;

#undef local_14a8
#undef local_149c
#undef RStack_1498
#undef acStack_1438
#undef acStack_1038
}

