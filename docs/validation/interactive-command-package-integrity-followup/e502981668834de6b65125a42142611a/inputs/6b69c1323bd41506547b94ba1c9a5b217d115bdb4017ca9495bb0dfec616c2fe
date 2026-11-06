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
 * _file_exists @ 0x000204b4..0x000204e0
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in project_initialization: file exists. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): bool _file_exists(char *param_1)
 * Direct callers: _detect_project@0x00020564, _init_project@0x00020760
 * Possible side effects (direct-call closure, incomplete for indirect calls): reads filesystem metadata
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000204b4_file_exists.s
 */
bool file_exists(char *param_1)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x90] __attribute__((aligned(16)));
#define RStack_78 (*((RecoveredTigerStat *)(app_frame + 0x8))) /* original SP - 0x78 */

  int iVar1;

  iVar1 = stat(param_1,(struct stat *)(&RStack_78));
  return iVar1 == 0;

#undef RStack_78
}

/*
 * _write_file @ 0x000204e0..0x00020564
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in project_initialization: write file. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): undefined4 _write_file(char *param_1,undefined4 param_2)
 * Direct callers: _init_project@0x00020760
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; opens/creates/truncates filesystem paths according to the supplied mode
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000204e0_write_file.s
 */
undefined4 write_file(char *param_1,undefined4 param_2)

{
  undefined4 uVar1;
  void *stream;
  int iVar2;
  
  stream = fopen(param_1,"w");
  uVar1 = 0xffffffff;
  if (stream != (void *)0x0) {
    iVar2 = fprintf(stream,"%s",(char *)(param_2));
    if (iVar2 < 0) {
      fclose(stream);
      uVar1 = 0xffffffff;
    }
    else {
      fclose(stream);
      uVar1 = 0;
    }
  }
  return uVar1;
}

/*
 * _detect_project @ 0x00020564..0x000206a4
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Identify project kind from representative build/package files.
 * Parameters/return (inferred): undefined4 _detect_project(undefined4 param_1)
 * Direct callers: _init_project@0x00020760
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; reads filesystem metadata
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00020564_detect_project.s
 */
undefined4 detect_project(undefined4 param_1)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x430] __attribute__((aligned(16)));
#define acStack_418 (*((char (*)[1040])(app_frame + 0x8))) /* original SP - 0x418 */

  undefined4 uVar1;
  int iVar2;

  snprintf(acStack_418,0x400,"%s/package.json",(char *)(param_1));
  iVar2 = file_exists(acStack_418);
  uVar1 = 0;
  if (iVar2 == 0) {
    snprintf(acStack_418,0x400,"%s/Cargo.toml",(char *)(param_1));
    iVar2 = file_exists(acStack_418);
    uVar1 = 1;
    if (iVar2 == 0) {
      snprintf(acStack_418,0x400,"%s/setup.py",(char *)(param_1));
      iVar2 = file_exists(acStack_418);
      if (iVar2 == 0) {
        snprintf(acStack_418,0x400,"%s/requirements.txt",(char *)(param_1));
        iVar2 = file_exists(acStack_418);
        if (iVar2 == 0) {
          snprintf(acStack_418,0x400,"%s/Makefile",(char *)(param_1));
          iVar2 = file_exists(acStack_418);
          if (iVar2 == 0) {
            snprintf(acStack_418,0x400,"%s/CMakeLists.txt",(char *)(param_1));
            iVar2 = file_exists(acStack_418);
            if (iVar2 == 0) {
              return 4;
            }
          }
          return 3;
        }
      }
      uVar1 = 2;
    }
  }
  return uVar1;

#undef acStack_418
}

/*
 * _type_name @ 0x000206a4..0x00020710
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in project_initialization: type name. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): char * _type_name(int param_1)
 * Direct callers: _init_project@0x00020760
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000206a4_type_name.s
 */
char * type_name(int param_1)

{
  if (param_1 == 1) {
    return "Rust";
  }
  if (param_1 == 0) {
    return "Node.js";
  }
  if (param_1 == 2) {
    return "Python";
  }
  if (param_1 == 3) {
    return "C/C++";
  }
  return "Generic";
}

/*
 * _basename_dir @ 0x00020710..0x00020760
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in project_initialization: basename dir. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): char * _basename_dir(char *param_1)
 * Direct callers: _init_project@0x00020760
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00020710_basename_dir.s
 */
char * basename_dir(char *param_1)

{
  char *pcVar1;
  
  pcVar1 = strrchr(param_1,0x2f);
  if (pcVar1 != (char *)0x0) {
    if (pcVar1[1] != '\0') {
      param_1 = pcVar1 + 1;
    }
  }
  return param_1;
}

/*
 * _init_project @ 0x00020760..0x00020a90
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): On explicit /init, create missing .gitignore/README and a generic Makefile under working_dir.
 * Parameters/return (inferred): int _init_project(RecoveredAppState *app)
 * Direct callers: _handle_slash_command@0x0000362c, _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; opens/creates/truncates filesystem paths according to the supplied mode; reads filesystem metadata
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00020760_init_project.s
 */
int init_project(RecoveredAppState *app)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x1450] __attribute__((aligned(16)));
#define acStack_1438 (*((char (*)[1024])(app_frame + 0x8))) /* original SP - 0x1438 */
#define acStack_1038 (*((char (*)[4116])(app_frame + 0x408))) /* original SP - 0x1038 */

  bool bVar1;
  int iVar2;
  undefined4 uVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  char *pcVar7;
  char *pcVar8;
  char *pcVar9;


  pcVar9 = (app->config).working_directory;
  iVar2 = detect_project((undefined4)(pcVar9));
  _report_buf = 0;
  uVar3 = (undefined4)(type_name(iVar2));
  iVar4 = snprintf(&_report_buf,0x400,"Detected %s project. Created: ",(char *)(uVar3));
  app_format_truncated(acStack_1438,0x400,"%s/.gitignore",pcVar9);
  iVar5 = file_exists(acStack_1438);
  if (iVar5 == 0) {
    if (iVar2 == 1) {
      pcVar7 = "target/\nCargo.lock\n*.rs.bk\n.DS_Store\n";
    }
    else if (iVar2 == 0) {
      pcVar7 = "node_modules/\n*.log\nnpm-debug.log*\n.DS_Store\n";
    }
    else if (iVar2 == 2) {
      pcVar7 = "__pycache__/\n*.py[cod]\n*.so\n*.egg-info/\ndist/\nbuild/\n.DS_Store\n";
    }
    else if (iVar2 == 3) {
      pcVar7 = "build/\n*.o\n*.a\n*.so\n*.dylib\n.DS_Store\n";
    }
    else {
      pcVar7 = "*.o\n*.a\n*.so\n*.dylib\nbuild/\ndist/\n.DS_Store\n";
    }
    iVar5 = write_file(acStack_1438,(undefined4)(pcVar7));
    if (iVar5 == 0) {
      iVar5 = snprintf(&_report_buf + iVar4,0x400 - iVar4,".gitignore");
      iVar4 = iVar4 + iVar5;
      bVar1 = true;
      iVar5 = 1;
    }
    else {
      bVar1 = false;
      iVar5 = 0;
    }
  }
  else {
    bVar1 = true;
    iVar5 = 0;
  }
  app_format_truncated(acStack_1438,0x400,"%s/README.md",pcVar9);
  iVar6 = file_exists(acStack_1438);
  if (iVar6 == 0) {
    if (iVar2 == 1) {
      pcVar7 = "cargo build";
      pcVar8 = "cargo run";
    }
    else if (iVar2 == 0) {
      pcVar7 = "npm install";
      pcVar8 = "npm start";
    }
    else if (iVar2 == 2) {
      pcVar7 = "pip install -r requirements.txt";
      pcVar8 = "python main.py";
    }
    else {
      pcVar7 = "make";
      pcVar8 = "./myapp";
    }
    uVar3 = (undefined4)(basename_dir(pcVar9));
    snprintf(acStack_1038,0x1000,
                       "# %s\n\nBrief description of %s.\n\n## Build\n\n```bash\n%s\n```\n\n## Run\n\n```bash\n%s\n```\n"
                       ,(char *)(uVar3),(char *)(uVar3),pcVar7,pcVar8);
    iVar6 = write_file(acStack_1438,(undefined4)(acStack_1038));
    if (iVar6 == 0) {
      if (iVar5 != 0) {
        iVar6 = snprintf(&_report_buf + iVar4,0x400 - iVar4,", ");
        iVar4 = iVar4 + iVar6;
      }
      iVar6 = snprintf(&_report_buf + iVar4,0x400 - iVar4,"README.md");
      iVar4 = iVar4 + iVar6;
      iVar5 = iVar5 + 1;
    }
    else {
      bVar1 = false;
    }
  }
  if (iVar2 == 4) {
    app_format_truncated(acStack_1438,0x400,"%s/Makefile",pcVar9);
    iVar6 = file_exists(acStack_1438);
    if (iVar6 == 0) {
      iVar6 = write_file(acStack_1438,
                          (undefined4)("CC = gcc\nCFLAGS = -O1 -Wall -Wextra -std=c99\nTARGET = myapp\nSRCS = main.c\nOBJS = $(SRCS:.c=.o)\n\nall: $(TARGET)\n\n$(TARGET): $(OBJS)\n\t$(CC) $(CFLAGS) -o $@ $^\n\nclean:\n\trm -f $(OBJS) $(TARGET)\n\ninstall: $(TARGET)\n\tcp $(TARGET) /usr/local/bin/\n\n.PHONY: all clean install\n")
                         );
      if (iVar6 == 0) {
        if (iVar5 != 0) {
          iVar6 = snprintf(&_report_buf + iVar4,0x400 - iVar4,", ");
          iVar4 = iVar4 + iVar6;
        }
        snprintf(&_report_buf + iVar4,0x400 - iVar4,"Makefile");
        iVar5 = iVar5 + 1;
      }
      else {
        bVar1 = false;
      }
    }
  }
  if (iVar5 == 0) {
    uVar3 = (undefined4)(type_name(iVar2));
    snprintf(&_report_buf,0x400,"All starter files already exist for %s project.",(char *)(uVar3));
  }
  return -(uint)!bVar1;

#undef acStack_1438
#undef acStack_1038
}

/*
 * _init_project_report @ 0x00020a90..0x00020aac
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in project_initialization: init project report. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): undefined1 * _init_project_report(void)
 * Direct callers: _handle_slash_command@0x0000362c, _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00020a90_init_project_report.s
 */
undefined1 * init_project_report(void)

{
  return (undefined1 *)(&_report_buf);
}

