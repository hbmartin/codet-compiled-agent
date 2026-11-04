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
 * _diff_files @ 0x0001741c..0x000174ec
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Execute diff -u with a separate argument vector and append its captured output.
 * Parameters/return (inferred): int _diff_files(char *old_path,char *new_path,RecoveredDString *output)
 * Direct callers: _diff_string_vs_file@0x000174ec
 * Possible side effects (direct-call closure, incomplete for indirect calls): executes an argument vector through PATH; creates a same-account child process; reads a descriptor or IPC channel
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0001741c_diff_files.s
 */
int diff_files(char *old_path,char *new_path,RecoveredDString *output)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x50] __attribute__((aligned(16)));
#define local_38 (*((char * *)(app_frame + 0x8))) /* original SP - 0x38 */
#define local_34 (*((char * *)(app_frame + 0xc))) /* original SP - 0x34 */
#define local_30 (*((char * *)(app_frame + 0x10))) /* original SP - 0x30 */
#define local_2c (*((char * *)(app_frame + 0x14))) /* original SP - 0x2c */
#define local_28 (*((undefined4 *)(app_frame + 0x18))) /* original SP - 0x28 */
#define RStack_24 (*((RecoveredDString *)(app_frame + 0x1c))) /* original SP - 0x24 */

  int iVar1;
  char *text;

  local_38 = "diff";
  local_34 = "-u";
  local_28 = 0;
  local_30 = old_path;
  local_2c = new_path;
  ds_init(&RStack_24);
  iVar1 = safe_exec_read(&local_38,(char *)0x0,&RStack_24);
  if (iVar1 < 0) {
    ds_free(&RStack_24);
    iVar1 = -1;
  }
  else {
    text = ds_cstr(&RStack_24);
    ds_append(output,text);
    if ((RStack_24.length == 0) && (iVar1 == 0)) {
      ds_append(output,"(no changes)\n");
    }
    ds_free(&RStack_24);
    iVar1 = 0;
  }
  return iVar1;

#undef local_38
#undef local_34
#undef local_30
#undef local_2c
#undef local_28
#undef RStack_24
}

/*
 * _diff_string_vs_file @ 0x000174ec..0x000175ac
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Write the supplied text through mkstemp, compare with the target file and unlink the temporary file.
 * Parameters/return (inferred): int _diff_string_vs_file(char *text,char *path,RecoveredDString *output)
 * Direct callers: _tool_edit_file@0x000144a8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; executes an argument vector through PATH; creates a same-account child process; securely creates a temporary file; reads a descriptor or IPC channel; removes a filesystem entry; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000174ec_diff_string_vs_file.s
 */
int diff_string_vs_file(char *text,char *path,RecoveredDString *output)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x440] __attribute__((aligned(16)));
#define acStack_428 (*((char (*)[1036])(app_frame + 0x8))) /* original SP - 0x428 */

  int descriptor;
  uint length;
  uint uVar1;
  int iVar2;

  snprintf(acStack_428,0x400,"/tmp/ppcdiff_XXXXXX");
  descriptor = mkstemp(acStack_428);
  iVar2 = -1;
  if (-1 < descriptor) {
    length = strlen(text);
    uVar1 = write(descriptor,text,length);
    close(descriptor);
    if (length == uVar1) {
      iVar2 = diff_files(acStack_428,path,output);
      unlink(acStack_428);
    }
    else {
      unlink(acStack_428);
      iVar2 = -1;
    }
  }
  return iVar2;

#undef acStack_428
}

/*
 * _do_exec_read @ 0x000175ac..0x00017720
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Fork, connect output pipes and exec an argument vector, collecting child output.
 * Parameters/return (inferred): int _do_exec_read(char **arguments,char *working_directory,RecoveredDString *output, int include_stderr)
 * Direct callers: _safe_exec_read@0x00017720, _safe_exec_read_both@0x00017744
 * Possible side effects (direct-call closure, incomplete for indirect calls): executes an argument vector through PATH; creates a same-account child process; reads a descriptor or IPC channel
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000175ac_do_exec_read.s
 */
int do_exec_read(char **arguments,char *working_directory,RecoveredDString *output,
                 int include_stderr)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x1050] __attribute__((aligned(16)));
#define local_1038 (*((int *)(app_frame + 0x8))) /* original SP - 0x1038 */
#define local_1034 (*((int *)(app_frame + 0xc))) /* original SP - 0x1034 */
#define local_1030 (*((uint *)(app_frame + 0x10))) /* original SP - 0x1030 */
#define acStack_102c (*((char (*)[4116])(app_frame + 0x14))) /* original SP - 0x102c */

  bool bVar1;
  int iVar2;
  pid_t pVar3;
  uint length;
  int *piVar4;
  pid_t pVar5;

  if ((((arguments != (char **)0x0) && (*arguments != (char *)0x0)) &&
      (output != (RecoveredDString *)0x0)) && (iVar2 = pipe(&local_1038), -1 < iVar2)) {
    pVar3 = fork();
    if (pVar3 < 0) {
      close(local_1038);
      close(local_1034);
      return -1;
    }
    if (pVar3 == 0) {
      close(local_1038);
      dup2(local_1034,1);
      if (include_stderr != 0) {
        dup2(local_1034,2);
      }
      close(local_1034);
      if (working_directory != (char *)0x0) {
        chdir(working_directory);
      }
      iVar2 = 3;
      do {
        close(iVar2);
        bVar1 = iVar2 != 0xff;
        iVar2 = iVar2 + 1;
      } while (bVar1);
      execvp(*arguments,arguments);
                    /* WARNING: Subroutine does not return */
      _exit(0x7f);
    }
    close(local_1034);
    ds_clear(output);
    while (length = read(local_1038,acStack_102c,0x1000), 0 < (int)length) {
      ds_appendn(output,acStack_102c,length);
    }
    close(local_1038);
    do {
      pVar5 = waitpid(pVar3,(int *)&local_1030,0);
      if (-1 < pVar5) break;
      piVar4 = __error();
    } while (*piVar4 == 4);
    if ((local_1030 & 0x7f) == 0) {
      return (int)local_1030 >> 8;
    }
  }
  return -1;

#undef local_1038
#undef local_1034
#undef local_1030
#undef acStack_102c
}

/*
 * _safe_exec_read @ 0x00017720..0x00017744
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Capture standard output from an argument-vector subprocess.
 * Parameters/return (inferred): int _safe_exec_read(char **arguments,char *working_directory,RecoveredDString *output)
 * Direct callers: _simple_git_status@0x00003400, _diff_files@0x0001741c, _redraw_sidebar@0x00019a6c, _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): executes an argument vector through PATH; creates a same-account child process; reads a descriptor or IPC channel
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00017720_safe_exec_read.s
 */
int safe_exec_read(char **arguments,char *working_directory,RecoveredDString *output)

{
  int iVar1;
  
  iVar1 = do_exec_read(arguments,working_directory,output,0);
  return iVar1;
}

/*
 * _safe_exec_read_both @ 0x00017744..0x00017768
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Capture standard output and standard error from an argument-vector subprocess.
 * Parameters/return (inferred): int _safe_exec_read_both(char **arguments,char *working_directory,RecoveredDString *output)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): executes an argument vector through PATH; creates a same-account child process; reads a descriptor or IPC channel
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00017744_safe_exec_read_both.s
 */
int safe_exec_read_both(char **arguments,char *working_directory,RecoveredDString *output)

{
  int iVar1;
  
  iVar1 = do_exec_read(arguments,working_directory,output,1);
  return iVar1;
}

/*
 * _safe_exec_run @ 0x00017768..0x0001782c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Run an argument-vector subprocess and return its completion status.
 * Parameters/return (inferred): int _safe_exec_run(char **arguments,char *working_directory)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): executes an argument vector through PATH; creates a same-account child process
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00017768_safe_exec_run.s
 */
int safe_exec_run(char **arguments,char *working_directory)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x40] __attribute__((aligned(16)));
#define local_28 (*((uint (*)[6])(app_frame + 0x8))) /* original SP - 0x28 */

  bool bVar1;
  pid_t pVar2;
  int *piVar3;
  pid_t pVar4;
  int descriptor;

  if ((arguments != (char **)0x0) && (*arguments != (char *)0x0)) {
    pVar2 = fork();
    if (-1 < pVar2) {
      if (pVar2 == 0) {
        if (working_directory != (char *)0x0) {
          chdir(working_directory);
        }
        descriptor = 3;
        do {
          close(descriptor);
          bVar1 = descriptor != 0xff;
          descriptor = descriptor + 1;
        } while (bVar1);
        execvp(*arguments,arguments);
                    /* WARNING: Subroutine does not return */
        _exit(0x7f);
      }
      do {
        pVar4 = waitpid(pVar2,(int *)local_28,0);
        if (-1 < pVar4) break;
        piVar3 = __error();
      } while (*piVar3 == 4);
      if ((local_28[0] & 0x7f) == 0) {
        return (int)local_28[0] >> 8;
      }
    }
  }
  return -1;

#undef local_28
}

