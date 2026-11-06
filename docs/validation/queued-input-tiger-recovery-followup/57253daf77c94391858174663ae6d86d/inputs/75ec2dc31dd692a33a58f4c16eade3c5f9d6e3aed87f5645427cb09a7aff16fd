/* ANNOTATED STATIC DECOMPILATION -- NOT ORIGINAL SOURCE, NOT BUILDABLE C.
 * Target: 32-bit big-endian PowerPC, Mac OS X 10.4 Tiger.
 * Functions/strings/behavior are preserved as recovered; security flaws are NOT fixed.
 * Symbol spellings, addresses and bytes are evidence. Local names, structures,
 * signatures and subsystem filenames are inferred. undefinedN/DAT_/PTR_/stack
 * constructs remain where exact source types or expressions cannot be established.
 * Every entry has an exact assembly sidecar and machine-readable index record.
 * Do not compile/run this archive as an application. See README.md and REPORT.md.
 */

/*
 * start @ 0x000026dc..0x0000271c
 * Status: assembly_only; symbol: retained.
 * Purpose (body-and-symbol): Mach-O process entry: establish the initial stack/argv state and transfer to __start; not an ordinary C-callable function.
 * Parameters/return (inferred): void entry(void)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; performs network/protocol I/O; replaces a process with an executable; resolves/executes a program through PATH; executes an argument vector through PATH; reads text from a stream; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; writes text to a stream; reads bytes from a stream; writes bytes to a stream; reads the process working directory; reads process environment; signals a process; creates directories; securely creates a temporary file; opens a directory for enumeration; executes a shell command and opens a pipe; reads a descriptor or IPC channel; reads directory entries; resolves a filesystem path; renames/replaces filesystem entries; changes process environment; executes SQL text; opens/creates a SQLite database; prepares SQL text; reads filesystem metadata; removes a filesystem entry; writes a descriptor
 * C confidence: assembly-only; C semantics unresolved; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000026dc_start.s
 * Uncertainty: Decompiler retains implicit register/stack variables or hidden return storage; exact signature/expression recovery is incomplete.
 * Uncertainty: Process entry consumes the initial stack/register state; an ordinary C-callable signature would be misleading.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * __start @ 0x0000271c..0x00002890
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): C runtime initialization: publish argc/argv/environment, initialize dynamic-loader state and transfer to application main.
 * Parameters/return (inferred): void __start(int param_1,char **param_2,int *param_3)
 * Direct callers: start@0x000026dc
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; performs network/protocol I/O; replaces a process with an executable; resolves/executes a program through PATH; executes an argument vector through PATH; reads text from a stream; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; writes text to a stream; reads bytes from a stream; writes bytes to a stream; reads the process working directory; reads process environment; signals a process; creates directories; securely creates a temporary file; opens a directory for enumeration; executes a shell command and opens a pipe; reads a descriptor or IPC channel; reads directory entries; resolves a filesystem path; renames/replaces filesystem entries; changes process environment; executes SQL text; opens/creates a SQLite database; prepares SQL text; reads filesystem metadata; removes a filesystem entry; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0000271c_start.s
 * Uncertainty: Indirect dispatch sites are retained with raw assembly and references; possible target sets are not assumed complete.
 */
void __start(int param_1,char **param_2,int *param_3)

{
  char cVar2;
  int iVar1;
  char *pcVar3;
  char *pcVar4;
  void *local_28 [6];
  
  __dyld_init_check();
  _NXArgc = param_1;
  _NXArgv = param_2;
  _environ = param_3;
  if (*(code **)PTR_0003968c != (code *)0x0) {
    (**(code **)PTR_0003968c)();
  }
  if (*(code **)PTR_00039694 != (code *)0x0) {
    (**(code **)PTR_00039694)();
  }
  ___keymgr_dwarf2_register_sections();
  if (*(int *)_pointer_to__darwin_gcc3_preregister_frame_info != 0) {
    (*(code *)_pointer_to__darwin_gcc3_preregister_frame_info)();
  }
  __call_mod_init_funcs();
  __call_objcInit();
  __dyld_func_lookup("__dyld_mod_term_funcs",local_28);
  if (local_28[0] != (void *)0x0) {
    _atexit(local_28[0]);
  }
  *(undefined4 *)PTR_00039690 = 0;
  pcVar4 = *param_2;
  if ((pcVar4 != (char *)0x0) && (cVar2 = *pcVar4, ___progname = pcVar4, cVar2 != '\0')) {
    pcVar3 = (char *)0x0;
    do {
      if (cVar2 == '/') {
        pcVar3 = pcVar4;
      }
      cVar2 = pcVar4[1];
      pcVar4 = pcVar4 + 1;
    } while (cVar2 != '\0');
    if (pcVar3 != (char *)0x0) {
      ___progname = pcVar3 + 1;
    }
  }
  iVar1 = *param_3;
  while (iVar1 != 0) {
    param_3 = param_3 + 1;
    iVar1 = *param_3;
  }
  iVar1 = _main(param_1,param_2);
                    /* WARNING: Subroutine does not return */
  _exit(iVar1);
}

/*
 * candidate_00002890 @ 0x00002890..0x000028b0
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to _exit; original C name/signature unavailable.
 * Parameters/return (inferred): void FUN_00002890(int param_1)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00002890_candidate_00002890.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * candidate_000028b0 @ 0x000028b0..0x000028d0
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to _main; original C name/signature unavailable.
 * Parameters/return (inferred): Unresolved; inspect assembly and both analyzer profiles.
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; performs network/protocol I/O; replaces a process with an executable; resolves/executes a program through PATH; executes an argument vector through PATH; reads text from a stream; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; writes text to a stream; reads bytes from a stream; writes bytes to a stream; reads the process working directory; reads process environment; signals a process; creates directories; securely creates a temporary file; opens a directory for enumeration; executes a shell command and opens a pipe; reads a descriptor or IPC channel; reads directory entries; resolves a filesystem path; renames/replaces filesystem entries; changes process environment; executes SQL text; opens/creates a SQLite database; prepares SQL text; reads filesystem metadata; removes a filesystem entry; writes a descriptor
 * C confidence: assembly-only; C semantics unresolved; profile: default.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Only the other profile produced a body.
 * Exact assembly: assembly/functions/000028b0_candidate_000028b0.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * candidate_000028d0 @ 0x000028d0..0x000028f0
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to _atexit; original C name/signature unavailable.
 * Parameters/return (inferred): Unresolved; inspect assembly and both analyzer profiles.
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: default.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Only the other profile produced a body.
 * Exact assembly: assembly/functions/000028d0_candidate_000028d0.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * candidate_000028f0 @ 0x000028f0..0x00002910
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to __dyld_func_lookup; original C name/signature unavailable.
 * Parameters/return (inferred): Unresolved; inspect assembly and both analyzer profiles.
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: default.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Only the other profile produced a body.
 * Exact assembly: assembly/functions/000028f0_candidate_000028f0.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * candidate_00002910 @ 0x00002910..0x00002930
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to __call_objcInit; original C name/signature unavailable.
 * Parameters/return (inferred): Unresolved; inspect assembly and both analyzer profiles.
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: default.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Only the other profile produced a body.
 * Exact assembly: assembly/functions/00002910_candidate_00002910.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * candidate_00002930 @ 0x00002930..0x00002950
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to __call_mod_init_funcs; original C name/signature unavailable.
 * Parameters/return (inferred): Unresolved; inspect assembly and both analyzer profiles.
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: default.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Only the other profile produced a body.
 * Exact assembly: assembly/functions/00002930_candidate_00002930.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * candidate_00002950 @ 0x00002950..0x00002970
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to ___keymgr_dwarf2_register_sections; original C name/signature unavailable.
 * Parameters/return (inferred): Unresolved; inspect assembly and both analyzer profiles.
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: default.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Only the other profile produced a body.
 * Exact assembly: assembly/functions/00002950_candidate_00002950.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * candidate_00002970 @ 0x00002970..0x00002990
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to __dyld_init_check; original C name/signature unavailable.
 * Parameters/return (inferred): Unresolved; inspect assembly and both analyzer profiles.
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: default.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Only the other profile produced a body.
 * Exact assembly: assembly/functions/00002970_candidate_00002970.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * __call_mod_init_funcs @ 0x00002990..0x000029d8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Run initializer function pointers registered by loaded Mach-O images.
 * Parameters/return (inferred): void __call_mod_init_funcs(void)
 * Direct callers: __start@0x0000271c, candidate_00002930@0x00002930
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00002990_call_mod_init_funcs.s
 * Uncertainty: Indirect dispatch sites are retained with raw assembly and references; possible target sets are not assumed complete.
 */
void __call_mod_init_funcs(void)

{
  code *local_28 [9];
  
  __dyld_func_lookup("__dyld_make_delayed_module_initializer_calls",local_28);
  (*local_28[0])();
  return;
}

/*
 * candidate_000029d8 @ 0x000029d8..0x000029f8
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to __dyld_func_lookup; original C name/signature unavailable.
 * Parameters/return (inferred): void FUN_000029d8(void)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000029d8_candidate_000029d8.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * _crt_basename @ 0x000029f8..0x00002a34
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in runtime_startup: crt basename. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): char * _crt_basename(char *param_1)
 * Direct callers: __call_objcInit@0x00002a90, candidate_00002c0c@0x00002c0c
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000029f8_crt_basename.s
 */
char * _crt_basename(char *param_1)

{
  char cVar1;
  char *pcVar2;
  
  cVar1 = *param_1;
  pcVar2 = param_1;
  if (cVar1 != '\0') {
    do {
      if (cVar1 == '/') {
        pcVar2 = param_1 + 1;
      }
      param_1 = param_1 + 1;
      cVar1 = *param_1;
    } while (cVar1 != '\0');
    return pcVar2;
  }
  return param_1;
}

/*
 * _crt_strbeginswith @ 0x00002a34..0x00002a90
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in runtime_startup: crt strbeginswith. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): undefined4 _crt_strbeginswith(char *param_1,char *param_2)
 * Direct callers: __call_objcInit@0x00002a90, candidate_00002bec@0x00002bec
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00002a34_crt_strbeginswith.s
 */
undefined4 _crt_strbeginswith(char *param_1,char *param_2)

{
  int iVar1;
  
  if (*param_2 == '\0') {
    return 1;
  }
  if (*param_1 == *param_2) {
    iVar1 = 0;
    do {
      iVar1 = iVar1 + 1;
      if (param_2[iVar1] == '\0') {
        return 1;
      }
    } while (param_1[iVar1] == param_2[iVar1]);
  }
  return 0;
}

/*
 * __call_objcInit @ 0x00002a90..0x00002bec
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Runtime support for Objective-C initialization; presence of this CRT routine does not make the application Objective-C.
 * Parameters/return (inferred): void __call_objcInit(void)
 * Direct callers: __start@0x0000271c, candidate_00002910@0x00002910
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00002a90_call_objcInit.s
 * Uncertainty: Indirect dispatch sites are retained with raw assembly and references; possible target sets are not assumed complete.
 */
void __call_objcInit(void)

{
  int iVar1;
  undefined4 uVar2;
  int iVar3;
  code *pcVar4;
  int iVar5;
  code *local_48;
  code *local_44;
  code *local_40;
  code *local_3c;
  code *local_38 [9];
  
  __dyld_func_lookup("__dyld_image_count",&local_48);
  __dyld_func_lookup("__dyld_get_image_name",&local_44);
  __dyld_func_lookup("__dyld_get_image_header",&local_40);
  __dyld_func_lookup("__dyld_NSLookupSymbolInImage",&local_3c);
  __dyld_func_lookup("__dyld_NSAddressOfSymbol",local_38);
  if (((((local_48 != (code *)0x0) && (local_44 != (code *)0x0)) && (local_40 != (code *)0x0)) &&
      ((local_3c != (code *)0x0 && (local_38[0] != (code *)0x0)))) &&
     (iVar1 = (*local_48)(), iVar1 != 0)) {
    iVar5 = 0;
    do {
      (*local_44)(iVar5);
      uVar2 = _crt_basename();
      iVar3 = _crt_strbeginswith(uVar2,"libobjc");
      if (((iVar3 != 0) && (iVar3 = (*local_40)(iVar5), iVar3 != 0)) &&
         ((iVar3 = (*local_3c)(iVar3,"__objcInit",4), iVar3 != 0 &&
          (pcVar4 = (code *)(*local_38[0])(), pcVar4 != (code *)0x0)))) {
        (*pcVar4)();
        return;
      }
      iVar5 = iVar5 + 1;
    } while (iVar1 != iVar5);
  }
  return;
}

/*
 * candidate_00002bec @ 0x00002bec..0x00002c0c
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to _crt_strbeginswith; original C name/signature unavailable.
 * Parameters/return (inferred): undefined4 FUN_00002bec(char *param_1,char *param_2)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00002bec_candidate_00002bec.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * candidate_00002c0c @ 0x00002c0c..0x00002c2c
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to _crt_basename; original C name/signature unavailable.
 * Parameters/return (inferred): Unresolved; inspect assembly and both analyzer profiles.
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: default.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Only the other profile produced a body.
 * Exact assembly: assembly/functions/00002c0c_candidate_00002c0c.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * candidate_00002c2c @ 0x00002c2c..0x00002c4c
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to __dyld_func_lookup; original C name/signature unavailable.
 * Parameters/return (inferred): Unresolved; inspect assembly and both analyzer profiles.
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: default.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Only the other profile produced a body.
 * Exact assembly: assembly/functions/00002c2c_candidate_00002c2c.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * __dyld_init_check @ 0x00002c4c..0x00002c8c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Check/initialize the process dynamic-loader function binding interface.
 * Parameters/return (inferred): void __dyld_init_check(void)
 * Direct callers: __start@0x0000271c, candidate_00002970@0x00002970
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00002c4c_dyld_init_check.s
 */
void __dyld_init_check(void)

{
  if (dyld_lazy_symbol_binding_entry_point != 0) {
    return;
  }
  syscall(0);
  syscall(0);
  trapWord(0x1f,1,1);
  trapWord(0x1f,1,1);
  (*(code *)0x0)(0x3b,
                 "The kernel support for the dynamic linker is not present to run this program.\n",
                 0x4e);
  return;
}

/*
 * dyld_stub_binding_helper @ 0x00002c8c..0x00002ca4
 * Status: assembly_only; symbol: retained.
 * Purpose (body-and-symbol): Assembly-level dyld stub binding trampoline; preserve the assembly ABI rather than invent a C signature.
 * Parameters/return (inferred): Unresolved; inspect assembly and both analyzer profiles.
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: default.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Only the other profile produced a body.
 * Exact assembly: assembly/functions/00002c8c_dyld_stub_binding_helper.s
 * Uncertainty: Indirect dispatch sites are retained with raw assembly and references; possible target sets are not assumed complete.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * __dyld_func_lookup @ 0x00002ca4..0x00002cb4
 * Status: assembly_only; symbol: retained.
 * Purpose (body-and-symbol): Assembly-level dynamic-loader function lookup trampoline.
 * Parameters/return (inferred): void __dyld_func_lookup(void)
 * Direct callers: __start@0x0000271c, candidate_000028f0@0x000028f0, __call_mod_init_funcs@0x00002990, candidate_000029d8@0x000029d8, __call_objcInit@0x00002a90, candidate_00002c2c@0x00002c2c
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00002ca4_dyld_func_lookup.s
 * Uncertainty: Ghidra identifies an ABI thunk; its assembly is authoritative.
 * Uncertainty: / WARNING: Could not recover jumptable at 0x00002cb0. Too many branches /
 * Uncertainty: Indirect dispatch sites are retained with raw assembly and references; possible target sets are not assumed complete.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * _darwin_unwind_dyld_add_image_hook @ 0x00002cb4..0x00002d3c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in runtime_startup: darwin unwind dyld add image hook. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _darwin_unwind_dyld_add_image_hook(void *header, int slide)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: default.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Only the other profile produced a body. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00002cb4_darwin_unwind_dyld_add_image_hook.s
 */
/* Analyst reconstruction of _darwin_unwind_dyld_add_image_hook at 0x00002cb4. See index/manual-review.json. */
void _darwin_unwind_dyld_add_image_hook(void *header, int slide)
{
    RecoveredUnwindImage *image = _calloc(1, 40);
    /* No null check after calloc exists in the machine code. */
    image->header = header;
    image->slide = slide;
    image->size = 40;
    image->next = __keymgr_get_and_lock_processwide_ptr(301);
    __keymgr_set_and_unlock_processwide_ptr(301, image);
}

/*
 * candidate_00002d3c @ 0x00002d3c..0x00002d5c
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to __keymgr_set_and_unlock_processwide_ptr; original C name/signature unavailable.
 * Parameters/return (inferred): void FUN_00002d3c(uint param_1,void *param_2)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00002d3c_candidate_00002d3c.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * candidate_00002d5c @ 0x00002d5c..0x00002d7c
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to __keymgr_get_and_lock_processwide_ptr; original C name/signature unavailable.
 * Parameters/return (inferred): Unresolved; inspect assembly and both analyzer profiles.
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: default.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Only the other profile produced a body.
 * Exact assembly: assembly/functions/00002d5c_candidate_00002d5c.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * candidate_00002d7c @ 0x00002d7c..0x00002d9c
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to _calloc; original C name/signature unavailable.
 * Parameters/return (inferred): Unresolved; inspect assembly and both analyzer profiles.
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: default.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Only the other profile produced a body.
 * Exact assembly: assembly/functions/00002d7c_candidate_00002d7c.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * _darwin_unwind_dyld_remove_image_hook @ 0x00002d9c..0x00002ebc
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in runtime_startup: darwin unwind dyld remove image hook. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _darwin_unwind_dyld_remove_image_hook(void *header, int slide)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: default.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Only the other profile produced a body. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00002d9c_darwin_unwind_dyld_remove_image_hook.s
 * Uncertainty: Indirect dispatch sites are retained with raw assembly and references; possible target sets are not assumed complete.
 */
/* Analyst reconstruction of _darwin_unwind_dyld_remove_image_hook at 0x00002d9c. See index/manual-review.json. */
void _darwin_unwind_dyld_remove_image_hook(void *header, int slide)
{
    RecoveredUnwindImage *removed = 0;
    RecoveredUnwindImage *head = __keymgr_get_and_lock_processwide_ptr(301);
    RecoveredUnwindImage **link = &head;
    while (*link != 0) {
        if ((*link)->header == header && (*link)->slide == slide) {
            removed = *link;
            *link = removed->next;
            if (removed->size != 40) _abort();
            break;
        }
        link = &(*link)->next;
    }
    __keymgr_set_and_unlock_processwide_ptr(301, head);
    if (removed != 0) {
        if (removed->destroy != 0) removed->destroy(removed);
        _free(removed);
    }
}

/*
 * candidate_00002ebc @ 0x00002ebc..0x00002edc
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to _free; original C name/signature unavailable.
 * Parameters/return (inferred): void FUN_00002ebc(void *param_1)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00002ebc_candidate_00002ebc.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * candidate_00002edc @ 0x00002edc..0x00002efc
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to __keymgr_set_and_unlock_processwide_ptr; original C name/signature unavailable.
 * Parameters/return (inferred): Unresolved; inspect assembly and both analyzer profiles.
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: default.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Only the other profile produced a body.
 * Exact assembly: assembly/functions/00002edc_candidate_00002edc.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * candidate_00002efc @ 0x00002efc..0x00002f1c
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to _abort; original C name/signature unavailable.
 * Parameters/return (inferred): Unresolved; inspect assembly and both analyzer profiles.
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: default.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Only the other profile produced a body.
 * Exact assembly: assembly/functions/00002efc_candidate_00002efc.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * candidate_00002f1c @ 0x00002f1c..0x00002f3c
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to __keymgr_get_and_lock_processwide_ptr; original C name/signature unavailable.
 * Parameters/return (inferred): Unresolved; inspect assembly and both analyzer profiles.
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: default.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Only the other profile produced a body.
 * Exact assembly: assembly/functions/00002f1c_candidate_00002f1c.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * ___darwin_gcc3_preregister_frame_info @ 0x00002f3c..0x00002fb8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in runtime_startup: darwin gcc3 preregister frame info. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void ___darwin_gcc3_preregister_frame_info(void)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00002f3c_darwin_gcc3_preregister_frame_info.s
 */
void ___darwin_gcc3_preregister_frame_info(void)

{
  __init_keymgr();
  if ((*(int *)(PTR_00039698 + 8) == 0) || (*(ushort *)(*(int *)(PTR_00039698 + 8) + 4) < 3)) {
    __dyld_register_func_for_add_image(&_darwin_unwind_dyld_add_image_hook);
    __dyld_register_func_for_remove_image(&_darwin_unwind_dyld_remove_image_hook);
  }
  return;
}

/*
 * candidate_00002fb8 @ 0x00002fb8..0x00002fd8
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to __dyld_register_func_for_remove_image; original C name/signature unavailable.
 * Parameters/return (inferred): void FUN_00002fb8(void)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00002fb8_candidate_00002fb8.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * candidate_00002fd8 @ 0x00002fd8..0x00002ff8
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to __dyld_register_func_for_add_image; original C name/signature unavailable.
 * Parameters/return (inferred): Unresolved; inspect assembly and both analyzer profiles.
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: default.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Only the other profile produced a body.
 * Exact assembly: assembly/functions/00002fd8_candidate_00002fd8.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

/*
 * candidate_00002ff8 @ 0x00002ff8..0x00003018
 * Status: assembly_only; symbol: inferred candidate.
 * Purpose (original-instructions-and-constant-target): Register-preserving PIC tail trampoline to __init_keymgr; original C name/signature unavailable.
 * Parameters/return (inferred): Unresolved; inspect assembly and both analyzer profiles.
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: assembly-only; C semantics unresolved; profile: default.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Only the other profile produced a body.
 * Exact assembly: assembly/functions/00002ff8_candidate_00002ff8.s
 * Uncertainty: Unlabeled compiler/runtime PIC trampoline; destination is statically resolved, original name and C call signature unavailable.
 */
/* No trustworthy standalone C body selected. See assembly sidecar and raw/function-views. */

