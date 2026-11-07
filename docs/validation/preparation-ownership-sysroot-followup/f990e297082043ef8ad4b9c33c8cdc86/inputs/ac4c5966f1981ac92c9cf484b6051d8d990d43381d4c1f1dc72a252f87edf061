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
 * _hook_slot_name @ 0x000123a8..0x00012400
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in hooks: hook slot name. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): char * _hook_slot_name(int param_1)
 * Direct callers: _hook_run@0x000127b0
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000123a8_hook_slot_name.s
 */
char * hook_slot_name(int param_1)

{
  if (param_1 == 1) {
    return "post-turn";
  }
  if (param_1 != 0) {
    if (param_1 == 2) {
      return "on-error";
    }
    return "unknown";
  }
  return "pre-turn";
}

/*
 * _hook_init_dir @ 0x00012400..0x00012470
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Choose nonempty HOME or /tmp, then form .ppccode/hooks.
 * Parameters/return (inferred): void _hook_init_dir(char *param_1,uint param_2)
 * Direct callers: _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; reads process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00012400_hook_init_dir.s
 */
void hook_init_dir(char *param_1,uint param_2)

{
  char *pcVar1;
  
  pcVar1 = getenv("HOME");
  if ((pcVar1 == (char *)0x0) || (*pcVar1 == '\0')) {
    pcVar1 = "/tmp";
  }
  snprintf(param_1,param_2,"%s/.ppccode/hooks",pcVar1);
  return;
}

/*
 * _hook_discover @ 0x00012470..0x000125cc
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Discover exact pre-turn.sh, post-turn.sh and on-error.sh hook filenames.
 * Parameters/return (inferred): int _hook_discover(RecoveredHooks *hooks,char *directory)
 * Direct callers: _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; opens a directory for enumeration; reads directory entries
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00012470_hook_discover.s
 */
int hook_discover(RecoveredHooks *hooks,char *directory)

{
  RecoveredHook *pRVar1;
  void *directory_00;
  RecoveredTigerDirent *pRVar2;
  char *left;
  int iVar3;
  
  iVar3 = 3;
  pRVar1 = hooks->slots;
  do {
    pRVar1->enabled = 0;
    ((RecoveredHook *)pRVar1->path)->path[0] = '\0';
    pRVar1 = pRVar1 + 1;
    iVar3 = iVar3 + -1;
  } while (iVar3 != 0);
  if (directory != (char *)0x0) {
    if (*directory != '\0') {
      directory_00 = opendir(directory);
      if (directory_00 != (void *)0x0) {
        while( true ) {
          pRVar2 = (RecoveredTigerDirent *)(readdir(directory_00));
          if (pRVar2 == (RecoveredTigerDirent *)0x0) break;
          if (pRVar2->name[0] != '.') {
            left = pRVar2->name;
            iVar3 = strcmp(left,"pre-turn.sh");
            if (iVar3 == 0) {
              snprintf((char *)hooks,0x400,"%s/%s",directory,left);
              hooks->slots[0].enabled = 1;
            }
            else {
              iVar3 = strcmp(left,"post-turn.sh");
              if (iVar3 == 0) {
                snprintf(hooks->slots[1].path,0x400,"%s/%s",directory,left);
                hooks->slots[1].enabled = 1;
              }
              else {
                iVar3 = strcmp(left,"on-error.sh");
                if (iVar3 == 0) {
                  snprintf(hooks->slots[2].path,0x400,"%s/%s",directory,left);
                  hooks->slots[2].enabled = 1;
                }
              }
            }
          }
        }
        closedir(directory_00);
      }
    }
  }
  return 0;
}

/*
 * _hook_close_all_fds @ 0x000125cc..0x00012624
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Close inherited descriptors except the explicitly retained communication descriptor.
 * Parameters/return (inferred): void _hook_close_all_fds(int param_1)
 * Direct callers: _hook_run@0x000127b0, _worker_start@0x000211b8
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000125cc_hook_close_all_fds.s
 */
void hook_close_all_fds(int param_1)

{
  int iVar1;
  int descriptor;
  
  iVar1 = getdtablesize();
  if (3 < iVar1) {
    descriptor = 3;
    do {
      if (descriptor != param_1) {
        close(descriptor);
      }
      descriptor = descriptor + 1;
    } while (iVar1 != descriptor);
  }
  return;
}

/*
 * _hook_build_state_json @ 0x00012624..0x000127b0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Write fallback hook metadata JSON using a PID-derived temporary pathname.
 * Parameters/return (inferred): undefined4 _hook_build_state_json (char *param_1,uint param_2,char *param_3,undefined4 param_4,int param_5,char *param_6, undefined4 param_7,char *param_8)
 * Direct callers: _hook_run@0x000127b0
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; opens/creates/truncates filesystem paths according to the supplied mode; writes bytes to a stream
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00012624_hook_build_state_json.s
 */
undefined4
hook_build_state_json
          (char *param_1,uint param_2,char *param_3,undefined4 param_4,int param_5,char *param_6,
          undefined4 param_7,char *param_8)

{
  undefined4 uVar1;
  pid_t pVar2;
  void *stream;
  int iVar3;
  
  pVar2 = getpid();
  snprintf(param_1,param_2,"/tmp/ppccode_hook_%d.json",pVar2);
  stream = fopen(param_1,"w");
  uVar1 = 0xffffffff;
  if (stream != (void *)0x0) {
    fwrite("{\n",1,2,stream);
    if (param_3 == (char *)0x0) {
      param_3 = "";
    }
    fprintf(stream,"  \"working_dir\": \"%s\",\n",param_3);
    fprintf(stream,"  \"message_count\": %d,\n",param_4);
    fprintf(stream,"  \"estimated_tokens\": %d,\n",param_5);
    if (param_6 == (char *)0x0) {
      param_6 = "";
    }
    fprintf(stream,"  \"model\": \"%s\",\n",param_6);
    iVar3 = 0x2000;
    if (0 < param_5) {
      iVar3 = param_5 << 1;
    }
    fprintf(stream,"  \"context_window\": %d,\n",iVar3);
    fprintf(stream,"  \"small_model_mode\": %d,\n",param_7);
    if (param_8 == (char *)0x0) {
      param_8 = "";
    }
    fprintf(stream,"  \"slot\": \"%s\"\n",param_8);
    fwrite("}\n",1,2,stream);
    fclose(stream);
    uVar1 = 0;
  }
  return uVar1;
}

/*
 * _hook_run @ 0x000127b0..0x00012a60
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Fork and invoke the configured hook through /bin/sh with metadata, descriptor cleanup and an alarm.
 * Parameters/return (inferred): int _hook_run(uint slot,char *input_path,char *working_directory,int message_count,int tokens, char *model,int provider,RecoveredHooks *hooks)
 * Direct callers: _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; replaces a process with an executable; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; writes bytes to a stream; changes process environment; removes a filesystem entry
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000127b0_hook_run.s
 */
int hook_run(uint slot,char *input_path,char *working_directory,int message_count,int tokens,
             char *model,int provider,RecoveredHooks *hooks)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x1a0] __attribute__((aligned(16)));
#define local_188 (*((char (*)[256])(app_frame + 0x8))) /* original SP - 0x188 */
#define local_88 (*((uint *)(app_frame + 0x108))) /* original SP - 0x88 */
#define acStack_84 (*((char (*)[92])(app_frame + 0x10c))) /* original SP - 0x84 */

  undefined4 uVar1;
  int iVar2;
  pid_t pVar3;
  char *pcVar4;
  char *pcVar5;

  if (2 < slot) {
    return -1;
  }
  if ((hooks->slots[slot].enabled == 0) || (hooks->slots[slot].path[0] == '\0')) {
    return 0;
  }
  local_188[0] = '\0';
  if ((input_path == (char *)0x0) || (*input_path == '\0')) {
    uVar1 = (undefined4)(hook_slot_name(slot));
    iVar2 = hook_build_state_json
                      (local_188,0x100,working_directory,message_count,tokens,model,provider,(char *)(uVar1));
    pcVar5 = local_188;
    if (iVar2 == 0) goto LAB_0001286c;
  }
  pcVar5 = input_path;
LAB_0001286c:
  pVar3 = fork();
  if (-1 < pVar3) {
    if (pVar3 == 0) {
      hook_close_all_fds(0xffffffff);
      unsetenv("DYLD_LIBRARY_PATH");
      unsetenv("DYLD_INSERT_LIBRARIES");
      if ((working_directory != (char *)0x0) && (*working_directory != '\0')) {
        setenv("PPPCODE_WORKING_DIR",working_directory,1);
      }
      snprintf(acStack_84,0x40,"%d",message_count);
      setenv("PPPCODE_MESSAGE_COUNT",acStack_84,1);
      snprintf(acStack_84,0x40,"%d",tokens);
      setenv("PPPCODE_ESTIMATED_TOKENS",acStack_84,1);
      if ((model != (char *)0x0) && (*model != '\0')) {
        setenv("PPPCODE_MODEL",model,1);
      }
      snprintf(acStack_84,0x40,"%d",provider);
      setenv("PPPCODE_SMALL_MODE",acStack_84,1);
      pcVar4 = (char *)hook_slot_name(slot);
      setenv("PPPCODE_SLOT",pcVar4,1);
      alarm(0x1e);
      if (pcVar5 == (char *)0x0) {
        pcVar5 = "";
      }
      execl("/bin/sh","sh",(char *)(hooks->slots + slot),pcVar5,(char *)NULL);
                    /* WARNING: Subroutine does not return */
      _exit(0x7f);
    }
    alarm(0x23);
    pVar3 = waitpid(pVar3,(int *)&local_88,0);
    alarm(0);
    if ((local_188[0] != '\0') && (local_188 == pcVar5)) {
      unlink(local_188);
    }
    if (-1 < pVar3) {
      if (((local_88 & 0x7f) != 0x7f) && ((local_88 & 0x7f) != 0)) {
        return -2;
      }
      return (int)local_88 >> 8;
    }
  }
  return -1;

#undef local_188
#undef local_88
#undef acStack_84
}

