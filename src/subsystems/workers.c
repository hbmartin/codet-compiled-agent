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
 * _worker_init @ 0x000210a4..0x000210fc
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage TUI worker process/IPC state: init. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _worker_init(RecoveredWorker *worker)
 * Direct callers: _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000210a4_worker_init.s
 */
void worker_init(RecoveredWorker *worker)

{
  memset(worker,0,0x220);
  worker->state = 0;
  worker->pid = -1;
  worker->event_pipe[1] = -1;
  worker->event_pipe[0] = -1;
  worker->approval_pipe[1] = -1;
  worker->approval_pipe[0] = -1;
  worker->unknown_1c = -1;
  return;
}

/*
 * _worker_cleanup @ 0x000210fc..0x000211b8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage TUI worker process/IPC state: cleanup. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _worker_cleanup(RecoveredWorker *worker)
 * Direct callers: _tui_run@0x0001df44, _worker_start@0x000211b8
 * Possible side effects (direct-call closure, incomplete for indirect calls): signals a process
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000210fc_worker_cleanup.s
 */
void worker_cleanup(RecoveredWorker *worker)

{
  if (0 < worker->pid) {
    kill(worker->pid,0xf);
    waitpid(worker->pid,(int *)0x0,0);
    worker->pid = -1;
  }
  if (-1 < worker->event_pipe[0]) {
    close(worker->event_pipe[0]);
    worker->event_pipe[0] = -1;
  }
  if (-1 < worker->event_pipe[1]) {
    close(worker->event_pipe[1]);
    worker->event_pipe[1] = -1;
  }
  if (-1 < worker->approval_pipe[0]) {
    close(worker->approval_pipe[0]);
    worker->approval_pipe[0] = -1;
  }
  if (-1 < worker->approval_pipe[1]) {
    close(worker->approval_pipe[1]);
    worker->approval_pipe[1] = -1;
  }
  worker->state = 0;
  return;
}

/*
 * _worker_start @ 0x000211b8..0x00021300
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Create IPC pipes, fork the TUI child and retain the parent's polling/approval endpoints.
 * Parameters/return (inferred): int _worker_start(RecoveredWorker *worker,char *input_path,char *output_path)
 * Direct callers: _start_turn@0x000202dc
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; performs network/protocol I/O; replaces a process with an executable; resolves/executes a program through PATH; executes an argument vector through PATH; reads text from a stream; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; writes text to a stream; reads bytes from a stream; writes bytes to a stream; reads the process working directory; reads process environment; signals a process; creates directories; securely creates a temporary file; opens a directory for enumeration; executes a shell command and opens a pipe; reads a descriptor or IPC channel; reads directory entries; resolves a filesystem path; renames/replaces filesystem entries; changes process environment; executes SQL text; opens/creates a SQLite database; prepares SQL text; reads filesystem metadata; removes a filesystem entry; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000211b8_worker_start.s
 */
int worker_start(RecoveredWorker *worker,char *input_path,char *output_path)

{
  int iVar1;
  int iVar2;
  pid_t pVar3;
  
  worker_cleanup(worker);
  iVar2 = pipe(worker->event_pipe);
  iVar1 = -1;
  if (iVar2 == 0) {
    iVar1 = pipe(worker->approval_pipe);
    if (iVar1 == 0) {
      pVar3 = fork();
      if (pVar3 < 0) {
        close(worker->event_pipe[0]);
        close(worker->event_pipe[1]);
        close(worker->approval_pipe[0]);
        close(worker->approval_pipe[1]);
        iVar1 = -1;
      }
      else {
        if (pVar3 == 0) {
          close(worker->event_pipe[0]);
          close(worker->approval_pipe[1]);
          iVar1 = open("/dev/null",1);
          if (-1 < iVar1) {
            dup2(iVar1,2);
            close(iVar1);
          }
          hook_close_all_fds(worker->event_pipe[1]);
          worker_child_main(input_path,output_path,worker->event_pipe[1],worker->approval_pipe[0]);
                    /* WARNING: Subroutine does not return */
          _exit(1);
        }
        close(worker->event_pipe[1]);
        close(worker->approval_pipe[0]);
        fcntl(worker->event_pipe[0],4,4);
        worker_reset_linebuf();
        worker->pid = pVar3;
        worker->state = 1;
        iVar1 = 0;
      }
    }
    else {
      close(worker->event_pipe[0]);
      close(worker->event_pipe[1]);
      iVar1 = -1;
    }
  }
  return iVar1;
}

/*
 * _worker_reset_linebuf @ 0x00021300..0x00021350
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage TUI worker process/IPC state: reset linebuf. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _worker_reset_linebuf(void)
 * Direct callers: _worker_start@0x000211b8
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00021300_worker_reset_linebuf.s
 */
void worker_reset_linebuf(void)

{
  memset(&_linebuf,0,0x40400);
  _linepos = 0;
  return;
}

/*
 * _worker_poll @ 0x00021350..0x000215c0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Read framed child messages without blocking the TUI event loop.
 * Parameters/return (inferred): int _worker_poll(RecoveredWorker *worker,char *event_kind,char *body,int capacity)
 * Direct callers: _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): reads a descriptor or IPC channel
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00021350_worker_poll.s
 */
int worker_poll(RecoveredWorker *worker,char *event_kind,char *body,int capacity)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0xd0] __attribute__((aligned(16)));
#define afStack_b8 (*((fd_set (*)[1])(app_frame + 0x8))) /* original SP - 0xb8 */
#define local_38 (*((struct timeval *)(app_frame + 0x88))) /* original SP - 0x38 */
#define local_30 (*((char (*)[20])(app_frame + 0x90))) /* original SP - 0x30 */

  char *length;
  uint uVar1;
  int iVar2;
  int *piVar3;
  char *pcVar4;
  uint uVar5;

  if (worker->state - 1U < 2) {
    memset(afStack_b8,0,0x80);
    uVar1 = worker->event_pipe[0];
    uVar5 = uVar1 >> 3 & 0x1ffffffc;
    *(uint *)(((char *)afStack_b8) + uVar5) =
         1 << (uVar1 & 0x1f) | *(uint *)(((char *)afStack_b8) + uVar5);
    local_38.tv_sec = 0;
    local_38.tv_usec = 50000;
    iVar2 = select(uVar1 + 1,afStack_b8,(fd_set *)0x0,(fd_set *)0x0,&local_38);
    if (iVar2 == 0) {
      return -2;
    }
    if (iVar2 < 0) {
      piVar3 = __error();
      if (*piVar3 == 4) {
        return -2;
      }
    }
    else {
      while (iVar2 = read(worker->event_pipe[0],local_30,1), -1 < iVar2) {
        if (iVar2 == 0) {
          iVar2 = worker_reap(worker);
          if (iVar2 != 0) {
            return -1;
          }
          return 0;
        }
        if (local_30[0] == '\n') {
          (&_linebuf)[_linepos] = 0;
          _linepos = 0;
          pcVar4 = strchr(&_linebuf,0x3a);
          if (pcVar4 == (char *)0x0) {
            uVar1 = strlen(&_linebuf);
            pcVar4 = &_linebuf + uVar1;
          }
          length = pcVar4 + -0x2cc444;
          if (0xf < (int)length) {
            length = (char *)0xf;
          }
          memcpy(event_kind,&_linebuf,(uint)length);
          event_kind[(int)length] = '\0';
          uVar1 = strlen(pcVar4 + 1);
          if (capacity <= (int)uVar1) {
            uVar1 = capacity - 1;
          }
          memcpy(body,pcVar4 + 1,uVar1);
          body[uVar1] = '\0';
          iVar2 = strcmp(event_kind,"AR");
          if (iVar2 == 0) {
            worker->state = 2;
          }
          iVar2 = strcmp(event_kind,"D");
          if (iVar2 == 0) {
            worker->state = 3;
            return 1;
          }
          return 1;
        }
        if (_linepos < 0x403ff) {
          (&_linebuf)[_linepos] = local_30[0];
          _linepos = _linepos + 1;
        }
      }
      piVar3 = __error();
      if ((*piVar3 == 0x23) || (piVar3 = __error(), *piVar3 == 0x23)) {
        return 0;
      }
    }
  }
  return -1;

#undef afStack_b8
#undef local_38
#undef local_30
}

/*
 * _worker_send_approve @ 0x000215c0..0x00021650
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Send the user's approval response to the active worker.
 * Parameters/return (inferred): void _worker_send_approve(RecoveredWorker *worker,int request_id,int approved)
 * Direct callers: _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000215c0_worker_send_approve.s
 */
void worker_send_approve(RecoveredWorker *worker,int request_id,int approved)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0xb0] __attribute__((aligned(16)));
#define acStack_98 (*((char (*)[140])(app_frame + 0x8))) /* original SP - 0x98 */

  uint length;
  undefined4 uVar1;
  int descriptor;

  if (-1 < worker->approval_pipe[1]) {
    uVar1 = 0x79;
    if (approved == 0) {
      uVar1 = 0x6e;
    }
    snprintf(acStack_98,0x80,"%s:%d:%c\n","A",request_id,uVar1);
    descriptor = worker->approval_pipe[1];
    length = strlen(acStack_98);
    write(descriptor,acStack_98,length);
    worker->state = 1;
  }
  return;

#undef acStack_98
}

/*
 * _worker_reap @ 0x00021650..0x000216d0
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage TUI worker process/IPC state: reap. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): int _worker_reap(RecoveredWorker *worker)
 * Direct callers: _worker_poll@0x00021350
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00021650_worker_reap.s
 */
int worker_reap(RecoveredWorker *worker)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x40] __attribute__((aligned(16)));
#define local_28 (*((uint (*)[8])(app_frame + 0x8))) /* original SP - 0x28 */

  int iVar1;
  pid_t pVar2;

  iVar1 = 1;
  if (-1 < worker->pid) {
    pVar2 = waitpid(worker->pid,(int *)local_28,1);
    iVar1 = 0;
    if (pVar2 != 0) {
      iVar1 = -1;
      if ((local_28[0] & 0x7f) == 0) {
        iVar1 = (int)local_28[0] >> 8;
      }
      worker->exit_status = iVar1;
      worker->pid = -1;
      worker->state = 3;
      iVar1 = 1;
    }
  }
  return iVar1;

#undef local_28
}

/*
 * _worker_kill @ 0x000216d0..0x00021788
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage TUI worker process/IPC state: kill. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _worker_kill(RecoveredWorker *worker)
 * Direct callers: _handle_cmd@0x0001b004, _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): signals a process
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000216d0_worker_kill.s
 */
void worker_kill(RecoveredWorker *worker)

{
  if (-1 < worker->pid) {
    kill(worker->pid,9);
    waitpid(worker->pid,(int *)0x0,0);
    worker->pid = -1;
    worker->state = 0;
    if (-1 < worker->event_pipe[0]) {
      close(worker->event_pipe[0]);
      worker->event_pipe[0] = -1;
    }
    if (-1 < worker->event_pipe[1]) {
      close(worker->event_pipe[1]);
      worker->event_pipe[1] = -1;
    }
    if (-1 < worker->approval_pipe[0]) {
      close(worker->approval_pipe[0]);
      worker->approval_pipe[0] = -1;
    }
    if (-1 < worker->approval_pipe[1]) {
      close(worker->approval_pipe[1]);
      worker->approval_pipe[1] = -1;
    }
  }
  return;
}

/*
 * _debug_open @ 0x00021788..0x000217d0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Open the fixed worker debug log for append.
 * Parameters/return (inferred): void _debug_open(void)
 * Direct callers: _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): opens/creates/truncates filesystem paths according to the supplied mode
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00021788_debug_open.s
 */
void debug_open(void)

{
  _debug_log = fopen("/tmp/ppccode_worker_debug.log","a");
  return;
}

/*
 * _debug @ 0x000217d0..0x00021850
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Emit formatted worker diagnostics only when verbose is enabled and the log stream exists.
 * Parameters/return (inferred): void _debug(const char *format, ...)
 * Direct callers: _child_printf@0x00021850, _child_ask_approve@0x00021ed8, _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/000217d0_debug.s
 */
/* Analyst reconstruction of _debug at 0x000217d0. See index/manual-review.json. */
void debug(const char *format, ...)
{

    if (_debug_log != 0 && _g_verbose != 0) {
        va_list arguments;
        va_start(arguments, format);
        vfprintf(_debug_log, format, arguments);
        va_end(arguments);
        fflush(_debug_log);
    }
}

/*
 * _child_printf @ 0x00021850..0x00021908
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in workers: child printf. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _child_printf(RecoveredChildContext *context, const char *format, ...)
 * Direct callers: _child_send_think@0x00021908, _child_send_token@0x000219f0, _child_send_tool_run@0x00021ad8, _child_send_tool_call@0x00021b2c, _child_send_approve_req@0x00021ccc, _child_send_done@0x00021d14, _child_send_error@0x00021d70, _child_send_usage@0x00021db4, _child_stream_cb@0x00021dfc, _execute_plan_steps@0x00022454
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; writes a descriptor
 * C confidence: medium: algorithm reviewed against all original instructions; SDK source types remain unavailable; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56. Primary C is the manually reviewed instruction-based reconstruction.
 * Exact assembly: assembly/functions/00021850_child_printf.s
 */
/* Analyst reconstruction of _child_printf at 0x00021850. See index/manual-review.json. */
void child_printf(RecoveredChildContext *context, const char *format, ...)
{
    char text[65536];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(text, 65536, format, arguments);
    va_end(arguments);
    /* The first context word is the event pipe write descriptor. */
    write(context->event_fd, text, strlen(text));
    write(context->event_fd, "\n", 1);
    debug("SEND: %s\n", text);
    /* Partial writes, interrupted writes and formatting errors are ignored. */
}

/*
 * _child_send_think @ 0x00021908..0x000219f0
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Encode and emit a child-to-parent protocol message: think. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _child_send_think(RecoveredChildContext *context,char *text)
 * Direct callers: _child_stream_cb@0x00021dfc
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00021908_child_send_think.s
 */
void child_send_think(RecoveredChildContext *context,char *text)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x10040] __attribute__((aligned(16)));
#define acStack_10028 (*((char (*)[65556])(app_frame + 0x8))) /* original SP - 0x10028 */

  int iVar1;
  uint length;

  length = strlen(text);
  if (0xffff < (int)length) {
    length = 0xffff;
  }
  memcpy(acStack_10028,text,length);
  acStack_10028[length] = '\0';
  if (0 < (int)length) {
    iVar1 = 0;
    do {
      if (acStack_10028[iVar1] == '\n') {
        acStack_10028[iVar1] = '\x01';
      }
      else if (acStack_10028[iVar1] == '\r') {
        acStack_10028[iVar1] = ' ';
      }
      iVar1 = iVar1 + 1;
      length = length - 1;
    } while (length != 0);
  }
  child_printf(context,"%s:%s","TH",acStack_10028);
  return;

#undef acStack_10028
}

/*
 * _child_send_token @ 0x000219f0..0x00021ad8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Encode and emit a child-to-parent protocol message: token. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _child_send_token(RecoveredChildContext *context,char *text)
 * Direct callers: _child_stream_cb@0x00021dfc, _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000219f0_child_send_token.s
 */
void child_send_token(RecoveredChildContext *context,char *text)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x10040] __attribute__((aligned(16)));
#define acStack_10028 (*((char (*)[65556])(app_frame + 0x8))) /* original SP - 0x10028 */

  int iVar1;
  uint length;

  length = strlen(text);
  if (0xffff < (int)length) {
    length = 0xffff;
  }
  memcpy(acStack_10028,text,length);
  acStack_10028[length] = '\0';
  if (0 < (int)length) {
    iVar1 = 0;
    do {
      if (acStack_10028[iVar1] == '\n') {
        acStack_10028[iVar1] = '\x01';
      }
      else if (acStack_10028[iVar1] == '\r') {
        acStack_10028[iVar1] = ' ';
      }
      iVar1 = iVar1 + 1;
      length = length - 1;
    } while (length != 0);
  }
  child_printf(context,"%s:%s","T",acStack_10028);
  return;

#undef acStack_10028
}

/*
 * _child_send_tool_run @ 0x00021ad8..0x00021b2c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Encode and emit a child-to-parent protocol message: tool run. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _child_send_tool_run(RecoveredChildContext *context,int index,char *name)
 * Direct callers: _execute_plan_steps@0x00022454, _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00021ad8_child_send_tool_run.s
 */
void child_send_tool_run(RecoveredChildContext *context,int index,char *name)

{
  if (name == (char *)0x0) {
    name = "";
  }
  child_printf(context,"%s:%s:%s","TR",(char *)(index),name);
  return;
}

/*
 * _child_send_tool_call @ 0x00021b2c..0x00021ccc
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Encode and emit a child-to-parent protocol message: tool call. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _child_send_tool_call(RecoveredChildContext *context,int index,RecoveredToolCall *call)
 * Direct callers: _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00021b2c_child_send_tool_call.s
 */
void child_send_tool_call(RecoveredChildContext *context,int index,RecoveredToolCall *call)

{
  char *pcVar1;
  char *pcVar2;
  char *pcVar3;
  
  switch(call->tool_type) {
  case 0:
    pcVar3 = "read_file";
    break;
  case 1:
    pcVar3 = "write_file";
    break;
  case 2:
    pcVar3 = "edit_file";
    break;
  case 3:
    pcVar3 = "shell";
    break;
  case 4:
    pcVar3 = "glob";
    break;
  case 5:
    pcVar3 = "grep";
    break;
  case 6:
    pcVar3 = "search";
    break;
  case 7:
    pcVar3 = "fetch_url";
    break;
  case 8:
    pcVar3 = "db_query";
    break;
  default:
    pcVar3 = call->mcp_name;
    if (call->mcp_name[0] == '\0') {
      pcVar3 = "unknown";
    }
    break;
  case 0xb:
    pcVar3 = "read_and_edit";
    break;
  case 0xc:
    pcVar3 = "subagent";
    break;
  case 0xd:
    pcVar3 = "reference";
    break;
  case 0xe:
    pcVar3 = "tree";
    break;
  case 0xf:
    pcVar3 = "read_project";
  }
  pcVar1 = ds_cstr(call->arguments);
  pcVar2 = ds_cstr(call->arguments + 1);
  child_printf(context,"%s:%d:%s:%s:%s","TC",index,pcVar3,pcVar1,pcVar2);
  return;
}

/*
 * _child_send_approve_req @ 0x00021ccc..0x00021d14
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Encode and emit a child-to-parent protocol message: approve req. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _child_send_approve_req(RecoveredChildContext *context,int index,char *description)
 * Direct callers: _child_ask_approve@0x00021ed8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00021ccc_child_send_approve_req.s
 */
void child_send_approve_req(RecoveredChildContext *context,int index,char *description)

{
  child_printf(context,"%s:%d:%s","AR",index,description);
  return;
}

/*
 * _child_send_done @ 0x00021d14..0x00021d70
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Encode and emit a child-to-parent protocol message: done. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _child_send_done(RecoveredChildContext *context,int status)
 * Direct callers: _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00021d14_child_send_done.s
 */
void child_send_done(RecoveredChildContext *context,int status)

{
  char *pcVar1;
  
  if (status == 0) {
    pcVar1 = "err";
  }
  else {
    pcVar1 = "ok";
  }
  child_printf(context,"%s:%s","D",pcVar1);
  return;
}

/*
 * _child_send_error @ 0x00021d70..0x00021db4
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Encode and emit a child-to-parent protocol message: error. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _child_send_error(RecoveredChildContext *context,char *text)
 * Direct callers: _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00021d70_child_send_error.s
 */
void child_send_error(RecoveredChildContext *context,char *text)

{
  child_printf(context,"%s:%s","E",text);
  return;
}

/*
 * _child_send_usage @ 0x00021db4..0x00021dfc
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Encode and emit a child-to-parent protocol message: usage. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _child_send_usage(RecoveredChildContext *context,int input_tokens,int output_tokens)
 * Direct callers: _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00021db4_child_send_usage.s
 */
void child_send_usage(RecoveredChildContext *context,int input_tokens,int output_tokens)

{
  child_printf(context,"%s:%d:%d","U",input_tokens,output_tokens);
  return;
}

/*
 * _child_stream_cb @ 0x00021dfc..0x00021ed8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in workers: child stream cb. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _child_stream_cb(char *event_kind,char *data,void *context)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00021dfc_child_stream_cb.s
 */
void child_stream_cb(char *event_kind,char *data,void *context)

{
  int iVar1;
  
  if ((data != (char *)0x0) && (*data != '\0')) {
    if ((event_kind != (char *)0x0) && (iVar1 = strcmp(event_kind,"think"), iVar1 == 0)) {
      if (_in_think_block == 0) {
        child_printf(context,"%s","TS");
        _in_think_block = 1;
      }
      child_send_think(context,data);
      return;
    }
    if (_in_think_block != 0) {
      child_printf(context,"%s","TE");
      _in_think_block = 0;
    }
    child_send_token(context,data);
  }
  return;
}

/*
 * _child_ask_approve @ 0x00021ed8..0x00022270
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Send a descriptive tool approval request and wait for a reply; timeout defaults to denial.
 * Parameters/return (inferred): int _child_ask_approve(RecoveredChildContext *context,int index,RecoveredToolCall *call)
 * Direct callers: _execute_plan_steps@0x00022454, _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; reads a descriptor or IPC channel; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00021ed8_child_ask_approve.s
 */
int child_ask_approve(RecoveredChildContext *context,int index,RecoveredToolCall *call)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x2d0] __attribute__((aligned(16)));
#define local_2b8 (*((char (*)[512])(app_frame + 0x8))) /* original SP - 0x2b8 */
#define local_b8 (*((char (*)[128])(app_frame + 0x208))) /* original SP - 0xb8 */
#define local_38 (*((char (*)[32])(app_frame + 0x288))) /* original SP - 0x38 */

  int iVar1;
  char *pcVar2;
  time_t tVar3;
  time_t tVar4;
  int iVar5;
  char *pcVar6;

  iVar1 = 1;
  if (context->auto_approve == 0) {
    local_2b8[0] = '\0';
    pcVar2 = ds_cstr(call->arguments);
    switch(call->tool_type) {
    case 0:
      snprintf(local_2b8,0x200,"read_file(%s)",pcVar2);
      break;
    case 1:
      snprintf(local_2b8,0x200,"write_file(%s)",pcVar2);
      break;
    case 2:
      snprintf(local_2b8,0x200,"edit_file(%s)",pcVar2);
      break;
    case 3:
      snprintf(local_2b8,0x200,"shell(%.80s)",pcVar2);
      break;
    case 4:
      snprintf(local_2b8,0x200,"glob(%s)",pcVar2);
      break;
    case 5:
      snprintf(local_2b8,0x200,"grep(%s)",pcVar2);
      break;
    case 6:
      snprintf(local_2b8,0x200,"search(%.80s)",pcVar2);
      break;
    case 7:
      snprintf(local_2b8,0x200,"fetch_url(%.80s)",pcVar2);
      break;
    case 8:
      snprintf(local_2b8,0x200,"db_query(%s)",pcVar2);
      break;
    default:
      if ((99 < (uint)call->tool_type) && (call->mcp_name[0] != '\0')) {
        snprintf(local_2b8,0x200,"%s(%.80s)",call->mcp_name,pcVar2);
      }
      break;
    case 0xb:
      snprintf(local_2b8,0x200,"read_and_edit(%s)",pcVar2);
      break;
    case 0xc:
      if (call->argument_count < 2) {
        pcVar6 = "default";
      }
      else {
        pcVar6 = ds_cstr(call->arguments + 1);
      }
      snprintf(local_2b8,0x200,"subagent(%s:%s)",pcVar2,pcVar6);
      break;
    case 0xd:
      snprintf(local_2b8,0x200,"reference(%.80s)",pcVar2);
      break;
    case 0xe:
      snprintf(local_2b8,0x200,"tree(%s)",pcVar2);
      break;
    case 0xf:
      snprintf(local_2b8,0x200,"read_project(%s)",pcVar2);
    }
    child_send_approve_req(context,index,local_2b8);
    tVar3 = time((time_t *)0x0);
    iVar1 = 0;
    while (tVar4 = time((time_t *)0x0), tVar4 - tVar3 < 0x3d) {
      iVar5 = read(context->approval_fd,local_38,1);
      if (iVar5 < 1) {
        usleep(10000);
      }
      else if (local_38[0] == '\n') {
        local_b8[iVar1] = '\0';
        iVar1 = strncmp(local_b8,"A:",2);
        if (iVar1 == 0) {
          return (uint)(local_b8[3] == 'y');
        }
        iVar1 = strcmp(local_b8,"C");
        if (iVar1 == 0) {
          return 0;
        }
        iVar1 = 0;
      }
      else if (iVar1 < 0x7f) {
        local_b8[iVar1] = local_38[0];
        iVar1 = iVar1 + 1;
      }
    }
    debug("APPROVE TIMEOUT \u2014 defaulting to deny for %s",local_2b8);
    iVar1 = 0;
  }
  return iVar1;

#undef local_2b8
#undef local_b8
#undef local_38
}

/*
 * _child_write_out_json @ 0x00022270..0x00022454
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Serialize worker conversation/result state to its parent-designated output JSON path.
 * Parameters/return (inferred): void _child_write_out_json(char *param_1,int *param_2,uint param_3,uint param_4)
 * Direct callers: _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; opens/creates/truncates filesystem paths according to the supplied mode; writes text to a stream
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/00022270_child_write_out_json.s
 */
void child_write_out_json(char *param_1,int *param_2,uint param_3,uint param_4)

{
  int iVar1;
  RecoveredCJSON *object;
  RecoveredCJSON *pRVar2;
  RecoveredCJSON *object_00;
  void *stream;
  char *pcVar3;
  int *piVar4;
  int iVar5;
  
  object = cJSON_CreateObject();
  pRVar2 = cJSON_CreateArray();
  if (0 < param_2[0x2280]) {
    iVar5 = 0;
    piVar4 = param_2;
    do {
      object_00 = cJSON_CreateObject();
      iVar1 = *piVar4;
      if (iVar1 == 2) {
        pcVar3 = "assistant";
      }
      else if (iVar1 == 3) {
        pcVar3 = "tool";
      }
      else if (iVar1 == 1) {
        pcVar3 = "user";
      }
      else {
        pcVar3 = "system";
      }
      cJSON_AddStringToObject(object_00,"role",pcVar3);
      pcVar3 = ds_cstr((RecoveredDString *)(param_2 + iVar5 * 0x17 + 1));
      cJSON_AddStringToObject(object_00,"content",pcVar3);
      cJSON_AddItemToArray(pRVar2,object_00);
      iVar5 = iVar5 + 1;
      piVar4 = piVar4 + 0x17;
    } while (iVar5 < param_2[0x2280]);
  }
  cJSON_AddItemToObject(object,"messages",pRVar2);
  if (-1 < (int)param_3) {
    pRVar2 = cJSON_CreateObject();
    cJSON_AddNumberToObject
              (pRVar2,"prompt_tokens",
               app_bits_double(CONCAT44(0x43300000,param_3 ^ 0x80000000)) - 4503601774854144.0);
    cJSON_AddNumberToObject
              (pRVar2,"response_tokens",
               app_bits_double(CONCAT44(0x43300000,param_4 ^ 0x80000000)) - 4503601774854144.0);
    cJSON_AddItemToObject(object,"usage",pRVar2);
  }
  pcVar3 = cJSON_PrintUnformatted(object);
  stream = fopen(param_1,"w");
  if (stream != (void *)0x0) {
    fputs(pcVar3,stream);
    fclose(stream);
  }
  free(pcVar3);
  cJSON_Delete(object);
  return;
}

/*
 * _execute_plan_steps @ 0x00022454..0x00022774
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Run pending plan steps from the worker and update/emit progress state.
 * Parameters/return (inferred): int _execute_plan_steps(RecoveredChildContext *param_1,void *param_2,int param_3, RecoveredPlan *param_4)
 * Direct callers: _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; performs network/protocol I/O; resolves/executes a program through PATH; executes an argument vector through PATH; reads text from a stream; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; writes text to a stream; reads bytes from a stream; writes bytes to a stream; reads the process working directory; reads process environment; signals a process; creates directories; securely creates a temporary file; opens a directory for enumeration; executes a shell command and opens a pipe; reads a descriptor or IPC channel; reads directory entries; resolves a filesystem path; renames/replaces filesystem entries; executes SQL text; opens/creates a SQLite database; prepares SQL text; reads filesystem metadata; removes a filesystem entry; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00022454_execute_plan_steps.s
 */
int execute_plan_steps(RecoveredChildContext *param_1,void *param_2,int param_3,
                       RecoveredPlan *param_4)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x590] __attribute__((aligned(16)));
#define local_578 (*((undefined1 (*)[1296])(app_frame + 0x8))) /* original SP - 0x578 */
#define RStack_68 (*((RecoveredDString *)(app_frame + 0x518))) /* original SP - 0x68 */
#define local_58 (*((RecoveredDString * *)(app_frame + 0x528))) /* original SP - 0x58 */

  char *pcVar1;
  int iVar2;
  RecoveredAppState *app_state;
  char *name;
  int iVar3;
  int iVar4;
  int index;
  void *output;
  RecoveredDString *pRVar5;
  RecoveredDString *str;
  char *pcVar6;

  child_printf(param_1,"%s:%d:%s","PS",param_4->count,(char *)(param_4));
  if ((param_4->count < 1) || (0x17d < *(int *)(param_3 + 0x8a00))) {
    iVar3 = 0;
  }
  else {
    index = 0;
    iVar3 = 0;
    iVar4 = 0;
    local_58 = (RecoveredDString *)(local_578 + 0x4c8);
    do {
      pcVar6 = param_4->steps[0].description + iVar4;
      child_printf(param_1,"%s:%d:%d:%s","PI",index,1,pcVar6);
      if ((*(int *)(pcVar6 + 0x1000) == 0) &&
         (*(char *)(0x1004 + (int)pcVar6) == '\0')) {
        child_printf(param_1,"%s:%d:%d:%s","PI",index,2,pcVar6);
      }
      else {
        memset(local_578,0,0x51c);
        ds_init(&RStack_68);
        APP_FIELD32(local_578, 0) = *(undefined4 *)(pcVar6 + 0x1000);
        pcVar1 = tool_name(APP_FIELD32(local_578, 0));
        strncpy((char *)(local_578 + 4),pcVar1,0x1f);
        if (*(char *)(0x1004 + (int)pcVar6) != '\0') {
          ds_init((RecoveredDString *)(local_578 + 0x24));
          ds_append((RecoveredDString *)(local_578 + 0x24),
                     (char *)(0x1004 + (int)pcVar6));
        }
        if (pcVar6[0x1404] != '\0') {
          ds_init((RecoveredDString *)(local_578 + 0x30));
          ds_append((RecoveredDString *)(local_578 + 0x30),pcVar6 + 0x1404);
        }
        APP_FIELD32(local_578, 1224) = 2;
        iVar2 = child_ask_approve(param_1,index,(RecoveredToolCall *)local_578);
        if (iVar2 == 0) {
          ds_clear(&RStack_68);
          ds_append(&RStack_68,"User rejected plan step");
          child_printf(param_1,"%s:%d:%d:%s","PI",index,4,pcVar6);
        }
        else {
          pcVar1 = tool_name(*(int *)(pcVar6 + 0x1000));
          name = (char *)0x0;
          if (*(char *)(0x1004 + (int)pcVar6) != '\0') {
            name = (char *)(0x1004 + (int)pcVar6);
          }
          child_send_tool_run(param_1,(int)pcVar1,name);
          app_state = malloc(0x8d08c4);
          if (app_state != (RecoveredAppState *)0x0) {
            memset(app_state,0,0x8d08c4);
            memcpy(&app_state->config,param_2,0x3668);
            tool_execute((RecoveredToolCall *)local_578,app_state);
            free(app_state);
          }
          child_printf(param_1,"%s:%d:%d:%s","PI",index,2,pcVar6);
          iVar3 = iVar3 + 1;
        }
        iVar2 = *(int *)(param_3 + 0x8a00) * 0x5c;
        output = (void *)(param_3 + iVar2);
        *(int *)(param_3 + 0x8a00) = *(int *)(param_3 + 0x8a00) + 1;
        memset(output,0,0x5c);
        *(undefined4 *)(param_3 + iVar2) = 3;
        pRVar5 = (RecoveredDString *)((int)output + 4);
        ds_init(pRVar5);
        pcVar1 = ds_cstr(&RStack_68);
        ds_appendf(pRVar5,"Step: %s\nResult: %s",pcVar6,pcVar1);
        ds_free(&RStack_68);
        pRVar5 = local_58;
        str = (RecoveredDString *)(local_578 + 0x24);
        do {
          ds_free(str);
          str = str + 1;
        } while (str != pRVar5);
      }
      index = index + 1;
    } while ((index < param_4->count) &&
            (iVar4 = iVar4 + 0x2808, *(int *)(param_3 + 0x8a00) < 0x17e));
  }
  plan_reset(param_4);
  return iVar3;

#undef local_578
#undef RStack_68
#undef local_58
}

/*
 * _worker_child_main @ 0x00022774..0x00024740
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Deserialize parent state, run lifecycle hooks and provider/tool turns, request approvals and emit framed results.
 * Parameters/return (inferred): void _worker_child_main(char *input_path,char *output_path,int parent_write_fd,int approval_read_fd)
 * Direct callers: _worker_start@0x000211b8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; performs network/protocol I/O; replaces a process with an executable; resolves/executes a program through PATH; executes an argument vector through PATH; reads text from a stream; opens/creates/truncates filesystem paths according to the supplied mode; creates a same-account child process; writes text to a stream; reads bytes from a stream; writes bytes to a stream; reads the process working directory; reads process environment; signals a process; creates directories; securely creates a temporary file; opens a directory for enumeration; executes a shell command and opens a pipe; reads a descriptor or IPC channel; reads directory entries; resolves a filesystem path; renames/replaces filesystem entries; changes process environment; executes SQL text; opens/creates a SQLite database; prepares SQL text; reads filesystem metadata; removes a filesystem entry; writes a descriptor
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/00022774_worker_child_main.s
 * Uncertainty: Decompiler retains implicit register/stack variables or hidden return storage; exact signature/expression recovery is incomplete.
 */
void worker_child_main(char *input_path,char *output_path,int parent_write_fd,int approval_read_fd)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x9c940] __attribute__((aligned(16)));
#define RStack_9c928 (*((RecoveredChildContext *)(app_frame + 0x8))) /* original SP - 0x9c928 */
#define RStack_9c918 (*((RecoveredConfig *)(app_frame + 0x18))) /* original SP - 0x9c918 */
#define RStack_992b0 (*((RecoveredConversation *)(app_frame + 0x3680))) /* original SP - 0x992b0 */
#define uStack_908a8 (*((undefined4 *)(app_frame + 0xc088))) /* original SP - 0x908a8 */
#define uStack_908a4 (*((undefined4 *)(app_frame + 0xc08c))) /* original SP - 0x908a4 */
#define uStack_908a0 (*((undefined4 *)(app_frame + 0xc090))) /* original SP - 0x908a0 */
#define auStack_9089c (*((undefined4 (*)[100])(app_frame + 0xc094))) /* original SP - 0x9089c */
#define RStack_9070c (*((RecoveredHooks *)(app_frame + 0xc224))) /* original SP - 0x9070c */
#define acStack_8fb00 (*((char (*)[1024])(app_frame + 0xce30))) /* original SP - 0x8fb00 */
#define RStack_8f700 (*((RecoveredChildContext *)(app_frame + 0xd230))) /* original SP - 0x8f700 */
#define uStack_8f6ec (*((uint *)(app_frame + 0xd244))) /* original SP - 0x8f6ec */
#define RStack_8f6e0 (*((RecoveredDString *)(app_frame + 0xd250))) /* original SP - 0x8f6e0 */
#define aRStack_8f6d4 (*((RecoveredDString (*)[974])(app_frame + 0xd25c))) /* original SP - 0x8f6d4 */
#define auStack_8c928 (*((undefined1 (*)[117844])(app_frame + 0x10008))) /* original SP - 0x8c928 */
#define RStack_6fcd4 (*((RecoveredResponse *)(app_frame + 0x2cc5c))) /* original SP - 0x6fcd4 */
#define RStack_502c8 (*((RecoveredPlan *)(app_frame + 0x4c668))) /* original SP - 0x502c8 */
#define uStack_b0 (*((undefined4 *)(app_frame + 0x9c880))) /* original SP - 0xb0 */
#define uStack_ac (*((undefined4 *)(app_frame + 0x9c884))) /* original SP - 0xac */
#define uStack_a8 (*((undefined4 *)(app_frame + 0x9c888))) /* original SP - 0xa8 */
#define local_a0 (*((undefined1 * *)(app_frame + 0x9c890))) /* original SP - 0xa0 */
#define local_9c (*((RecoveredChildContext * *)(app_frame + 0x9c894))) /* original SP - 0x9c */
#define local_98 (*((RecoveredConversation * *)(app_frame + 0x9c898))) /* original SP - 0x98 */
#define local_94 (*((RecoveredDString * *)(app_frame + 0x9c89c))) /* original SP - 0x94 */
#define local_90 (*((int *)(app_frame + 0x9c8a0))) /* original SP - 0x90 */
#define local_8c (*((undefined * *)(app_frame + 0x9c8a4))) /* original SP - 0x8c */
#define local_88 (*((undefined * *)(app_frame + 0x9c8a8))) /* original SP - 0x88 */
#define local_84 (*((undefined1 * *)(app_frame + 0x9c8ac))) /* original SP - 0x84 */
#define local_80 (*((RecoveredDString * *)(app_frame + 0x9c8b0))) /* original SP - 0x80 */
#define local_7c (*((undefined1 * *)(app_frame + 0x9c8b4))) /* original SP - 0x7c */
#define local_78 (*((undefined1 * *)(app_frame + 0x9c8b8))) /* original SP - 0x78 */
#define local_74 (*((RecoveredDString * *)(app_frame + 0x9c8bc))) /* original SP - 0x74 */
#define local_70 (*((RecoveredPlan * *)(app_frame + 0x9c8c0))) /* original SP - 0x70 */
#define local_6c (*((RecoveredDString * *)(app_frame + 0x9c8c4))) /* original SP - 0x6c */
#define local_68 (*((undefined1 * *)(app_frame + 0x9c8c8))) /* original SP - 0x68 */
#define local_64 (*((RecoveredDString * *)(app_frame + 0x9c8cc))) /* original SP - 0x64 */
#define local_60 (*((RecoveredDString * *)(app_frame + 0x9c8d0))) /* original SP - 0x60 */
#define local_5c (*((undefined1 * *)(app_frame + 0x9c8d4))) /* original SP - 0x5c */
#define local_58 (*((undefined1 * *)(app_frame + 0x9c8d8))) /* original SP - 0x58 */
#define local_54 (*((RecoveredDString * *)(app_frame + 0x9c8dc))) /* original SP - 0x54 */

  bool bVar1;
  bool bVar2;
  undefined1 *puVar3;
  RecoveredPlan *plan;
  void *stream;
  uint uVar4;
  char *pcVar5;
  RecoveredCJSON *object;
  RecoveredCJSON *pRVar6;
  RecoveredCJSON *pRVar7;
  int iVar8;
  RecoveredCJSON *pRVar9;
  int iVar10;
  RecoveredToolCall *pRVar11;
  char *pcVar12;
  RecoveredCJSON *pRVar13;
  RecoveredCacheEntry *pRVar14;
  RecoveredAppState *pRVar15;
  RecoveredRoute *route;
  int *piVar16;
  RecoveredToolCall *call;
  int iVar17;
  int iVar18;
  RecoveredMessage *pRVar19;
  char *text;
  RecoveredChildContext *context;
  uint *puVar20;
  int iVar21;
  int iVar22;
  int iVar23;
  RecoveredDString *pRVar24;
  RecoveredResponse *response;
  RecoveredDString *str;
  RecoveredConversation *pRVar25;
  undefined4 *puVar26;

  debug_open();
  debug("CHILD started: in=%s out=%s\n",input_path,output_path);
  stream = fopen(input_path,"r");
  if (stream == (void *)0x0) {
    RStack_9c928.event_fd = parent_write_fd;
    RStack_9c928.approval_fd = approval_read_fd;
    RStack_9c928.opaque_8 = (int)stream;
    RStack_9c928.auto_approve = (int)stream;
    child_send_error(&RStack_9c928,"Cannot read input");
                    /* WARNING: Subroutine does not return */
    _exit(1);
  }
  fseek(stream,0,2);
  uVar4 = ftell(stream);
  fseek(stream,0,0);
  pcVar5 = malloc(uVar4 + 1);
  fread(pcVar5,1,uVar4,stream);
  pcVar5[uVar4] = '\0';
  fclose(stream);
  object = cJSON_Parse(pcVar5);
  free(pcVar5);
  if (object == (RecoveredCJSON *)0x0) {
                    /* WARNING: Subroutine does not return */
    _exit(1);
  }
  config_init(&RStack_9c918);
  pRVar6 = cJSON_GetObjectItem(object,"config");
  if (pRVar6 != (RecoveredCJSON *)0x0) {
    pRVar7 = cJSON_GetObjectItem(pRVar6,"provider");
    if ((pRVar7 != (RecoveredCJSON *)0x0) && (pRVar7->value_string != (char *)0x0)) {
      RStack_9c918.provider = provider_from_name(pRVar7->value_string);
    }
    pRVar7 = cJSON_GetObjectItem(pRVar6,"model");
    if (pRVar7 != (RecoveredCJSON *)0x0) {
      strncpy(RStack_9c918.model,pRVar7->value_string,0x7f);
    }
    pRVar7 = cJSON_GetObjectItem(pRVar6,"api_key");
    if (pRVar7 != (RecoveredCJSON *)0x0) {
      strncpy(RStack_9c918.api_key,pRVar7->value_string,0xff);
    }
    pRVar7 = cJSON_GetObjectItem(pRVar6,"openai_key");
    if (pRVar7 != (RecoveredCJSON *)0x0) {
      strncpy(RStack_9c918.openai_key,pRVar7->value_string,0xff);
    }
    pRVar7 = cJSON_GetObjectItem(pRVar6,"anthropic_key");
    if (pRVar7 != (RecoveredCJSON *)0x0) {
      strncpy(RStack_9c918.anthropic_key,pRVar7->value_string,0xff);
    }
    pRVar7 = cJSON_GetObjectItem(pRVar6,"google_key");
    if (pRVar7 != (RecoveredCJSON *)0x0) {
      strncpy(RStack_9c918.google_key,pRVar7->value_string,0xff);
    }
    pRVar7 = cJSON_GetObjectItem(pRVar6,"ollama_key");
    if (pRVar7 != (RecoveredCJSON *)0x0) {
      strncpy(RStack_9c918.ollama_key,pRVar7->value_string,0xff);
    }
    pRVar7 = cJSON_GetObjectItem(pRVar6,"base_url");
    if (pRVar7 != (RecoveredCJSON *)0x0) {
      strncpy(RStack_9c918.base_url,pRVar7->value_string,0x1ff);
    }
    pRVar7 = cJSON_GetObjectItem(pRVar6,"ollama_url");
    if (pRVar7 != (RecoveredCJSON *)0x0) {
      strncpy(RStack_9c918.ollama_url,pRVar7->value_string,0x1ff);
    }
    pRVar7 = cJSON_GetObjectItem(pRVar6,"working_dir");
    if (pRVar7 != (RecoveredCJSON *)0x0) {
      strncpy(RStack_9c918.working_directory,pRVar7->value_string,0x3ff);
    }
    pRVar7 = cJSON_GetObjectItem(pRVar6,"max_tokens");
    if (pRVar7 != (RecoveredCJSON *)0x0) {
      RStack_9c918.max_tokens = pRVar7->value_int;
    }
    pRVar7 = cJSON_GetObjectItem(pRVar6,"context_window");
    if (pRVar7 != (RecoveredCJSON *)0x0) {
      RStack_9c918.context_window = pRVar7->value_int;
    }
    pRVar7 = cJSON_GetObjectItem(pRVar6,"temperature");
    if (pRVar7 != (RecoveredCJSON *)0x0) {
      RStack_9c918.temperature = (float)pRVar7->value_double;
    }
    pRVar7 = cJSON_GetObjectItem(pRVar6,"auto_approve");
    if (pRVar7 != (RecoveredCJSON *)0x0) {
      RStack_9c918.auto_approve = pRVar7->value_int;
    }
    pRVar7 = cJSON_GetObjectItem(pRVar6,"auto_compact");
    if (pRVar7 != (RecoveredCJSON *)0x0) {
      RStack_9c918.auto_compact = pRVar7->value_int;
    }
    pRVar7 = cJSON_GetObjectItem(pRVar6,"effort");
    if (pRVar7 != (RecoveredCJSON *)0x0) {
      RStack_9c918.effort = pRVar7->value_int;
    }
    pRVar7 = cJSON_GetObjectItem(pRVar6,"verbose");
    if (pRVar7 != (RecoveredCJSON *)0x0) {
      _g_verbose = pRVar7->value_int;
      RStack_9c918.verbose = _g_verbose;
    }
    pRVar7 = cJSON_GetObjectItem(pRVar6,"small_model_mode");
    if (pRVar7 != (RecoveredCJSON *)0x0) {
      RStack_9c918.small_model_mode = pRVar7->value_int;
    }
    pRVar6 = cJSON_GetObjectItem(pRVar6,"system_prompt");
    if ((pRVar6 != (RecoveredCJSON *)0x0) && (pRVar6->value_string != (char *)0x0)) {
      ds_clear(&RStack_9c918.system_prompt);
      ds_append(&RStack_9c918.system_prompt,pRVar6->value_string);
    }
  }
  memset(&RStack_992b0,0,0x8a08);
  pRVar6 = cJSON_GetObjectItem(object,"messages");
  if ((pRVar6 != (RecoveredCJSON *)0x0) && (iVar8 = cJSON_GetArraySize(pRVar6), 0 < iVar8)) {
    iVar17 = 0;
    do {
      pRVar7 = cJSON_GetArrayItem(pRVar6,iVar17);
      if (pRVar7 != (RecoveredCJSON *)0x0) {
        pRVar9 = cJSON_GetObjectItem(pRVar7,"role");
        pRVar7 = cJSON_GetObjectItem(pRVar7,"content");
        iVar10 = RStack_992b0.message_count;
        pRVar19 = RStack_992b0.messages + RStack_992b0.message_count;
        RStack_992b0.message_count = RStack_992b0.message_count + 1;
        memset(pRVar19,0,0x5c);
        pRVar24 = &RStack_992b0.messages[iVar10].content;
        ds_init(pRVar24);
        if ((pRVar7 != (RecoveredCJSON *)0x0) && (pRVar7->value_string != (char *)0x0)) {
          ds_append(pRVar24,pRVar7->value_string);
        }
        if ((pRVar9 != (RecoveredCJSON *)0x0) &&
           (pcVar5 = pRVar9->value_string, pcVar5 != (char *)0x0)) {
          iVar10 = strcmp(pcVar5,"user");
          if (iVar10 == 0) {
            pRVar19->role = 1;
          }
          else {
            iVar10 = strcmp(pcVar5,"assistant");
            if (iVar10 == 0) {
              pRVar19->role = 2;
            }
            else {
              iVar10 = strcmp(pcVar5,"tool");
              if (iVar10 == 0) {
                pRVar19->role = 3;
              }
              else {
                pRVar19->role = 0;
              }
            }
          }
        }
      }
      iVar17 = iVar17 + 1;
    } while ((iVar8 != iVar17) && (iVar17 != 0x180));
  }
  pRVar6 = cJSON_GetObjectItem(object,"user_msg");
  if ((pRVar6 == (RecoveredCJSON *)0x0) || (pcVar5 = pRVar6->value_string, pcVar5 == (char *)0x0)) {
    pcVar5 = "";
  }
  router_classify((uint *)((RecoveredRoute *)&uStack_b0),pcVar5);
  iVar8 = RStack_9c918.small_model_mode;
  uStack_908a8 = uStack_b0;
  uStack_908a4 = uStack_ac;
  uStack_908a0 = uStack_a8;
  iVar17 = 0;
  iVar10 = 100;
  do {
    *(undefined4 *)((int)auStack_9089c + iVar17) = 0;
    iVar17 = iVar17 + 4;
    iVar10 = iVar10 + -1;
  } while (iVar10 != 0);
  hook_init_dir(acStack_8fb00,0x400);
  hook_discover(&RStack_9070c,acStack_8fb00);
  if (RStack_9c918.auto_compact != 0) {
    context_auto_compact(&RStack_992b0,&RStack_9c918);
  }
  iVar17 = RStack_992b0.message_count;
  RStack_8f700.opaque_8 = 0;
  RStack_8f700.auto_approve = RStack_9c918.auto_approve;
  pRVar19 = RStack_992b0.messages + RStack_992b0.message_count;
  RStack_992b0.message_count = RStack_992b0.message_count + 1;
  RStack_8f700.event_fd = parent_write_fd;
  RStack_8f700.approval_fd = approval_read_fd;
  memset(pRVar19,0,0x5c);
  RStack_992b0.messages[iVar17].role = 1;
  pRVar24 = &RStack_992b0.messages[iVar17].content;
  ds_init(pRVar24);
  ds_append(pRVar24,pcVar5);
  iVar17 = RStack_992b0.message_count;
  if (RStack_9c918.small_model_mode != 0) {
    pRVar19 = RStack_992b0.messages + RStack_992b0.message_count;
    RStack_992b0.message_count = RStack_992b0.message_count + 1;
    memset(pRVar19,0,0x5c);
    RStack_992b0.messages[iVar17].role = 0;
    pRVar24 = &RStack_992b0.messages[iVar17].content;
    ds_init(pRVar24);
    ds_append(pRVar24,
               "[WORKSPACE DISCOVERY -- small model assist]\nBefore editing ANY file, FIRST use read_file and glob to understand the project structure. Check for README.md, Makefile, setup instructions, or configuration files. Never overwrite a file you haven\'t read. Look for .docs/ directory with project-specific documentation."
              );
  }
  iVar17 = 0;
  iVar10 = 0;
  bVar2 = false;
  local_90 = 0xc0b0;
  local_a0 = auStack_8c928;
  local_80 = RStack_6fcd4.tool_calls[10].arguments + 7;
  local_8c = (undefined *)(&DAT_0004c67c);
  local_7c = auStack_8c928;
  local_9c = &RStack_8f700;
  local_98 = &RStack_992b0;
  local_78 = auStack_8c928;
  local_70 = &RStack_502c8;
  local_68 = auStack_8c928;
  local_94 = &RStack_8f6e0;
  local_60 = aRStack_8f6d4;
  local_5c = auStack_8c928;
  local_54 = &RStack_6fcd4.error_or_reasoning;
  local_84 = local_a0;
  local_74 = local_80;
  local_6c = local_80;
  local_64 = local_80;
  local_58 = local_5c;
LAB_00022f6c:
  if (0x16b < RStack_992b0.message_count) {
    context_auto_compact(&RStack_992b0,&RStack_9c918);
  }
  puVar3 = local_84;
  if (RStack_992b0.message_count < 0x17e) {
    hook_run(0,input_path,RStack_9c918.working_directory,RStack_992b0.message_count,
              RStack_992b0.estimated_tokens,RStack_9c918.model,RStack_9c918.small_model_mode,
              (RecoveredHooks *)(local_84 + -0x3de4));
    memset(&RStack_6fcd4,0,0x1fa0c);
    ds_init(&RStack_6fcd4.content);
    ds_init(&RStack_6fcd4.error_or_reasoning);
    if (iVar8 == 0) {
      piVar16 = (int *)0x0;
      route = (RecoveredRoute *)0x0;
    }
    else {
      piVar16 = (int *)(puVar3 + -0x3f74);
      route = (RecoveredRoute *)(puVar3 + -0x3f80);
    }
    uVar4 = provider_send(&RStack_9c918,&RStack_992b0,&RStack_6fcd4,(RecoveredStreamCallback)(child_stream_cb),
                           local_a0 + -0x2dd8,route,piVar16);
    pRVar24 = local_80;
    iVar17 = RStack_6fcd4.input_tokens;
    iVar10 = RStack_6fcd4.output_tokens;
    if (RStack_6fcd4.usage_known == 0) {
      iVar17 = 0;
      iVar10 = 0;
    }
    if (_g_verbose != 0) {
      pcVar5 = ds_cstr((RecoveredDString *)((int)(local_80 + -0x44f) + 8));
      debug("LLM Response (%d tokens):\n%s\n",RStack_6fcd4.output_tokens,pcVar5);
      if (0 < RStack_6fcd4.tool_call_count) {
        iVar21 = 0;
        piVar16 = (int *)(pRVar24 + -0x44c);
        do {
          pcVar5 = tool_name(*piVar16);
          debug("TOOL CALL: %s\n",pcVar5);
          iVar21 = iVar21 + 1;
          piVar16 = piVar16 + 0x147;
        } while (iVar21 < RStack_6fcd4.tool_call_count);
      }
    }
    puVar3 = local_7c;
    if (399 < uVar4) {
      hook_run(2,input_path,RStack_9c918.working_directory,RStack_992b0.message_count,
                RStack_992b0.estimated_tokens,RStack_9c918.model,RStack_9c918.small_model_mode,
                (RecoveredHooks *)(local_7c + -0x3de4));
      pcVar5 = ds_cstr(&RStack_6fcd4.error_or_reasoning);
      if ((pcVar5 == (char *)0x0) || (*pcVar5 == '\0')) {
        snprintf((char * restrict)(local_a0 + -0x2db8),0x400,
                           "HTTP request failed with status %d (empty body)",uVar4);
      }
      else {
        snprintf((char * restrict)(puVar3 + -0x2db8),0x400,"HTTP %d: %s",uVar4,pcVar5);
      }
      child_send_error(local_9c,(char *)(local_a0 + -0x2db8));
      pRVar25 = &RStack_992b0;
      child_write_out_json
                (output_path,(int *)(pRVar25),RStack_6fcd4.input_tokens,RStack_6fcd4.output_tokens);
      child_send_done(local_9c,0);
      if (0 < RStack_992b0.message_count) {
        iVar8 = 0;
        do {
          message_free(pRVar25->messages);
          iVar8 = iVar8 + 1;
          pRVar25 = (RecoveredConversation *)(pRVar25->messages + 1);
        } while (iVar8 < RStack_992b0.message_count);
      }
      ds_free(&RStack_9c918.system_prompt);
      cJSON_Delete(object);
                    /* WARNING: Subroutine does not return */
      _exit(1);
    }
    iVar21 = RStack_992b0.message_count;
    pRVar19 = local_98->messages + iVar21;
    RStack_992b0.message_count = iVar21 + 1;
    memset(pRVar19,0,0x5c);
    local_98->messages[iVar21].role = 2;
    ds_init(&pRVar19->content);
    puVar3 = local_78;
    pRVar24 = (RecoveredDString *)(local_78 + -0x2dc8);
    ds_init(pRVar24);
    reasoning_extract((RecoveredDString *)(&RStack_6fcd4),pRVar24);
    if ((0 < RStack_9c918.think_budget) && ((uint)(RStack_9c918.think_budget << 2) < uStack_8f6ec))
    {
      ds_clear(pRVar24);
      child_send_token((RecoveredChildContext *)(puVar3 + -0x2dd8),
                        "\n[Thinking budget exceeded. Stripped deliberation.]\n");
    }
    pRVar24 = local_74;
    ds_copy(&pRVar19->content,(RecoveredDString *)((int)(local_74 + -0x44f) + 8));
    if (0 < RStack_6fcd4.tool_call_count) {
      pRVar19->tool_call_capacity = RStack_6fcd4.tool_call_count;
      pRVar11 = malloc(RStack_6fcd4.tool_call_count * 0x51c);
      pRVar19->tool_calls = pRVar11;
      pRVar19->tool_call_count = RStack_6fcd4.tool_call_count;
      if (0 < RStack_6fcd4.tool_call_count) {
        iVar21 = 0;
        iVar18 = 0;
        puVar26 = (undefined4 *)(&pRVar24[-0x44c].data);
        do {
          memset(pRVar19->tool_calls->mcp_name + iVar18 + -4,0,0x51c);
          *(undefined4 *)(pRVar19->tool_calls->mcp_name + iVar18 + -4) = *puVar26;
          strncpy(pRVar19->tool_calls->call_id + iVar18,RStack_6fcd4.tool_calls[iVar21].call_id,
                   0x3f);
          strncpy(pRVar19->tool_calls->mcp_name + iVar18,RStack_6fcd4.tool_calls[iVar21].mcp_name,
                   0x1f);
          *(undefined4 *)(pRVar19->tool_calls->call_id + iVar18 + -4) = puVar26[0x132];
          if (0 < (int)puVar26[0x132]) {
            iVar22 = 0;
            do {
              ds_init((RecoveredDString *)
                       ((int)&pRVar19->tool_calls->arguments[iVar22].data + iVar18));
              ds_copy((RecoveredDString *)
                       ((int)&pRVar19->tool_calls->arguments[iVar22].data + iVar18),
                       RStack_6fcd4.tool_calls[iVar21].arguments + iVar22);
              iVar22 = iVar22 + 1;
            } while (iVar22 < (int)puVar26[0x132]);
          }
          iVar21 = iVar21 + 1;
          iVar18 = iVar18 + 0x51c;
          puVar26 = puVar26 + 0x147;
        } while (iVar21 < RStack_6fcd4.tool_call_count);
      }
    }
    if (pRVar19->tool_call_count == 0) {
      pcVar5 = ds_cstr(&RStack_6fcd4.content);
      pcVar12 = strstr(pcVar5,"```json");
      if ((pcVar12 != (char *)0x0) || (pcVar12 = strstr(pcVar5,"```"), pcVar12 != (char *)0x0)) {
        text = pcVar12 + 3;
        pcVar5 = strstr(text,"```");
        if (pcVar5 != (char *)0x0) {
          iVar21 = -3 - (int)pcVar12;
          if (pcVar12[3] == 'j') {
            text = pcVar12 + 7;
            iVar21 = -7 - (int)pcVar12;
          }
          pcVar5 = pcVar5 + iVar21;
          pcVar12 = malloc((uint)(pcVar5 + 1));
          if (pcVar12 != (char *)0x0) {
            memcpy(pcVar12,text,(uint)pcVar5);
            pcVar12[(int)pcVar5] = '\0';
            pRVar6 = cJSON_Parse(pcVar12);
            if (pRVar6 != (RecoveredCJSON *)0x0) {
              pRVar7 = cJSON_GetObjectItem(pRVar6,"tool_calls");
              if (pRVar7 == (RecoveredCJSON *)0x0) {
                pRVar7 = pRVar6;
              }
              iVar21 = cJSON_GetArraySize(pRVar7);
              if (0 < iVar21) {
                child_send_token((RecoveredChildContext *)(local_a0 + -0x2dd8),
                                  "\n[Repaired malformed tool calls from code fence]\n");
                local_88 = (undefined *)(&DAT_0004c67c);
                for (iVar21 = 0;
                    (iVar18 = cJSON_GetArraySize(pRVar7), iVar21 < iVar18 && (iVar21 != 99));
                    iVar21 = iVar21 + 1) {
                  pRVar13 = cJSON_GetArrayItem(pRVar7,iVar21);
                  pRVar9 = cJSON_GetObjectItem(pRVar13,"function");
                  if (pRVar9 == (RecoveredCJSON *)0x0) {
                    pRVar9 = pRVar13;
                  }
                  pRVar13 = cJSON_GetObjectItem(pRVar9,"name");
                  pRVar9 = cJSON_GetObjectItem(pRVar9,"arguments");
                  if ((pRVar13 != (RecoveredCJSON *)0x0) && (pRVar13->value_string != (char *)0x0))
                  {
                    iVar18 = RStack_6fcd4.tool_call_count;
                    pRVar11 = RStack_6fcd4.tool_calls + iVar18;
                    RStack_6fcd4.tool_call_count = iVar18 + 1;
                    memset(pRVar11,0,0x51c);
                    ds_init(&RStack_6fcd4.tool_calls[iVar18].result);
                    iVar22 = 0;
                    pRVar24 = RStack_6fcd4.tool_calls[iVar18].arguments;
                    do {
                      ds_init((RecoveredDString *)((int)&pRVar24->data + iVar22));
                      bVar1 = iVar22 != 0x498;
                      iVar22 = iVar22 + 0xc;
                    } while (bVar1);
                    snprintf(RStack_6fcd4.tool_calls[iVar18].mcp_name,0x20,"%s",
                                       pRVar13->value_string);
                    pcVar5 = pRVar13->value_string;
                    iVar22 = strcmp(pcVar5,"read_file");
                    if (iVar22 == 0) {
                      pRVar11->tool_type = 0;
                    }
                    else {
                      iVar22 = strcmp(pcVar5,"write_file");
                      if (iVar22 == 0) {
                        pRVar11->tool_type = 1;
                      }
                      else {
                        iVar22 = strcmp(pcVar5,"edit_file");
                        if (iVar22 == 0) {
                          pRVar11->tool_type = 2;
                        }
                        else {
                          iVar22 = strcmp(pcVar5,"shell");
                          if (iVar22 == 0) {
                            pRVar11->tool_type = 3;
                          }
                          else {
                            iVar22 = strcmp(pcVar5,"glob");
                            if (iVar22 == 0) {
                              pRVar11->tool_type = 4;
                            }
                            else {
                              iVar22 = strcmp(pcVar5,"grep");
                              if (iVar22 == 0) {
                                pRVar11->tool_type = 5;
                              }
                              else {
                                iVar22 = strcmp(pcVar5,"search");
                                if (iVar22 == 0) {
                                  pRVar11->tool_type = 6;
                                }
                                else {
                                  iVar22 = strcmp(pcVar5,"fetch_url");
                                  if (iVar22 == 0) {
                                    pRVar11->tool_type = 7;
                                  }
                                  else {
                                    iVar22 = strcmp(pcVar5,"db_query");
                                    if (iVar22 == 0) {
                                      pRVar11->tool_type = 8;
                                    }
                                    else {
                                      iVar22 = strcmp(pcVar5,"memory_remember");
                                      if (iVar22 == 0) {
                                        pRVar11->tool_type = 9;
                                      }
                                      else {
                                        iVar22 = strcmp(pcVar5,"memory_load");
                                        if (iVar22 == 0) {
                                          pRVar11->tool_type = 10;
                                        }
                                        else {
                                          iVar22 = strcmp(pcVar5,"read_and_edit");
                                          if (iVar22 == 0) {
                                            pRVar11->tool_type = 0xb;
                                          }
                                          else {
                                            iVar22 = strcmp(pcVar5,"subagent");
                                            if (iVar22 == 0) {
                                              pRVar11->tool_type = 0xc;
                                            }
                                            else {
                                              iVar22 = strcmp(pcVar5,"reference");
                                              if (iVar22 == 0) {
                                                pRVar11->tool_type = 0xd;
                                              }
                                              else {
                                                iVar22 = strcmp(pcVar5,"tree");
                                                if (iVar22 == 0) {
                                                  pRVar11->tool_type = 0xe;
                                                }
                                                else {
                                                  iVar22 = strcmp(pcVar5,"read_project");
                                                  if (iVar22 == 0) {
                                                    pRVar11->tool_type = 0xf;
                                                  }
                                                  else {
                                                    pRVar11->tool_type = 100;
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
                            }
                          }
                        }
                      }
                    }
                    if (pRVar9 != (RecoveredCJSON *)0x0) {
                      if (pRVar9->value_string == (char *)0x0) {
                        if (pRVar9->type_flags == 7) {
                          provider_parse_tool_calls_args(pRVar11,pRVar9);
                        }
                      }
                      else {
                        pRVar13 = cJSON_Parse(pRVar9->value_string);
                        if (pRVar13 == (RecoveredCJSON *)0x0) {
                          ds_append(pRVar24,pRVar9->value_string);
                          RStack_6fcd4.tool_calls[iVar18].argument_count = 1;
                        }
                        else {
                          provider_parse_tool_calls_args(pRVar11,pRVar13);
                          cJSON_Delete(pRVar13);
                        }
                      }
                    }
                    snprintf(RStack_6fcd4.tool_calls[iVar18].call_id,0x40,"tc_r%d",
                                       RStack_6fcd4.tool_call_count);
                  }
                }
              }
              cJSON_Delete(pRVar6);
            }
            free(pcVar12);
          }
        }
      }
    }
    plan = local_70;
    plan_init(local_70);
    pRVar24 = local_6c;
    pcVar5 = ds_cstr((RecoveredDString *)((int)(local_6c + -0x44f) + 8));
    iVar21 = plan_parse(plan,pcVar5);
    if ((iVar21 != 0) && (0 < RStack_502c8.count)) goto code_r0x0002395c;
    goto LAB_000239f8;
  }
LAB_00023f74:
  if (1 < RStack_9c918.effort) {
    iVar8 = RStack_992b0.message_count;
    iVar21 = 0;
    if (iVar8 < 0x17c) {
      while( true ) {
        pRVar19 = local_98->messages;
        RStack_992b0.message_count = iVar8 + 1;
        memset(pRVar19 + iVar8,0,0x5c);
        local_98->messages[iVar8].role = 0;
        pRVar24 = &pRVar19[iVar8].content;
        ds_init(pRVar24);
        ds_append(pRVar24,
                   "You just made changes to the codebase. Review your work for correctness:\n1. Check for syntax errors or missing includes\n2. Verify variable initialization and null checks\n3. Ensure file paths and function names are correct\n4. Report any issues found in this format:\n- [ISSUE] <description>\n- [OK] if no issues found"
                  );
        child_send_token(local_9c,"\n[Validation pass ");
        iVar21 = iVar21 + 1;
        pcVar5 = (char *)(local_a0 + -0x2dc8);
        snprintf(pcVar5,0x10,"%d",iVar21);
        child_send_token(local_9c,pcVar5);
        child_send_token(local_9c,"...]\n");
        response = (RecoveredResponse *)(local_a0 + -0x2db8);
        memset(response,0,0x1fa0c);
        ds_init((RecoveredDString *)response);
        pRVar24 = local_60;
        ds_init(local_60);
        uVar4 = provider_send(&RStack_9c918,local_98,response,(RecoveredStreamCallback)(child_stream_cb),local_9c,
                               (RecoveredRoute *)0x0,(int *)0x0);
        if (399 < uVar4) {
          ds_free((RecoveredDString *)response);
          ds_free(pRVar24);
          goto LAB_00024608;
        }
        iVar8 = RStack_992b0.message_count;
        pRVar19 = local_98->messages;
        RStack_992b0.message_count = iVar8 + 1;
        memset(pRVar19 + iVar8,0,0x5c);
        local_98->messages[iVar8].role = 2;
        str = &pRVar19[iVar8].content;
        ds_init(str);
        puVar3 = local_5c;
        pRVar24 = (RecoveredDString *)(local_5c + -0x2db8);
        ds_copy(str,pRVar24);
        pcVar5 = ds_cstr(pRVar24);
        pcVar12 = strstr(pcVar5,"[OK]");
        if ((pcVar12 != (char *)0x0) &&
           (pcVar12 = strstr(pcVar5,"[ISSUE]"), pcVar12 == (char *)0x0)) break;
        pcVar5 = strstr(pcVar5,"[ISSUE]");
        puVar3 = local_58;
        if (pcVar5 == (char *)0x0) {
          ds_free((RecoveredDString *)(local_a0 + -0x2db8));
          ds_free((RecoveredDString *)(local_a0 + -0x2dac));
          goto LAB_00024608;
        }
        context = (RecoveredChildContext *)(local_58 + -0x2dd8);
        child_send_token(context,"\n[Auto-fix iteration ");
        snprintf((char * restrict)(puVar3 + -0x2dc8),0x10,"%d",iVar21);
        child_send_token(context,(char *)(puVar3 + -0x2dc8));
        child_send_token(context,"...]\n");
        iVar8 = RStack_992b0.message_count;
        pRVar19 = local_98->messages;
        RStack_992b0.message_count = iVar8 + 1;
        memset(pRVar19 + iVar8,0,0x5c);
        local_98->messages[iVar8].role = 1;
        pRVar24 = &pRVar19[iVar8].content;
        ds_init(pRVar24);
        ds_append(pRVar24,"Fix the issues reported above. Take corrective action.");
        memset(&RStack_6fcd4,0,0x1fa0c);
        ds_init(&RStack_6fcd4.content);
        pRVar24 = local_54;
        ds_init(local_54);
        uVar4 = provider_send(&RStack_9c918,local_98,&RStack_6fcd4,(RecoveredStreamCallback)(child_stream_cb),context,
                               (RecoveredRoute *)0x0,(int *)0x0);
        if (399 < uVar4) {
          ds_free(&RStack_6fcd4.content);
          ds_free(pRVar24);
          ds_free((RecoveredDString *)(puVar3 + -0x2db8));
          ds_free((RecoveredDString *)(puVar3 + -0x2dac));
          goto LAB_00024608;
        }
        iVar8 = RStack_992b0.message_count;
        pRVar19 = local_98->messages;
        RStack_992b0.message_count = iVar8 + 1;
        memset(pRVar19 + iVar8,0,0x5c);
        local_98->messages[iVar8].role = 2;
        pRVar24 = &pRVar19[iVar8].content;
        ds_init(pRVar24);
        ds_copy(pRVar24,&RStack_6fcd4.content);
        ds_free(&RStack_6fcd4.content);
        ds_free(&RStack_6fcd4.error_or_reasoning);
        if (RStack_6fcd4.tool_call_count == 0) {
          ds_free((RecoveredDString *)(local_a0 + -0x2db8));
          ds_free((RecoveredDString *)(local_a0 + -0x2dac));
          goto LAB_00024608;
        }
        if (0 < RStack_6fcd4.tool_call_count) {
          if (RStack_992b0.message_count < 0x17f) {
            iVar8 = 0;
            pRVar11 = RStack_6fcd4.tool_calls;
            puVar20 = &RStack_6fcd4.tool_calls[0].result.length;
            do {
              iVar18 = child_ask_approve((RecoveredChildContext *)(local_a0 + -0x2dd8),iVar8,
                                          pRVar11);
              if (iVar18 == 0) {
                ds_clear(&pRVar11->result);
                ds_append(&pRVar11->result,"{\"error\": \"User rejected tool call\"}");
              }
              else {
                pRVar15 = malloc(0x8d08c4);
                if (pRVar15 == (RecoveredAppState *)0x0) {
                  ds_clear(&pRVar11->result);
                  ds_append(&pRVar11->result,"{\"error\": \"System out of memory\"}");
                }
                else {
                  memset(pRVar15,0,0x8d08c4);
                  memcpy(&pRVar15->config,&RStack_9c918,0x3668);
                  pcVar5 = pRVar11->mcp_name;
                  if (*(char *)(puVar20 + -0x144) == '\0') {
                    pcVar5 = tool_name(puVar20[-0x145]);
                  }
                  pcVar12 = ds_cstr(pRVar11->arguments);
                  child_send_tool_run
                            ((RecoveredChildContext *)(local_a0 + -0x2dd8),(int)pcVar5,pcVar12);
                  tool_execute(pRVar11,pRVar15);
                  free(pRVar15);
                }
              }
              if (0x10000 < *puVar20) {
                ds_truncate(&pRVar11->result,0x10000);
                ds_append(&pRVar11->result,"\n... [result truncated to ~64KB] ...\n");
              }
              iVar18 = RStack_992b0.message_count;
              pRVar19 = local_98->messages;
              RStack_992b0.message_count = iVar18 + 1;
              memset(pRVar19 + iVar18,0,0x5c);
              local_98->messages[iVar18].role = 3;
              pRVar24 = &pRVar19[iVar18].content;
              ds_init(pRVar24);
              ds_copy(pRVar24,&pRVar11->result);
              iVar8 = iVar8 + 1;
              if (RStack_6fcd4.tool_call_count <= iVar8) break;
              pRVar11 = pRVar11 + 1;
              puVar20 = puVar20 + 0x147;
            } while (RStack_992b0.message_count < 0x17f);
            if (RStack_6fcd4.tool_call_count < 1) goto LAB_000245e0;
          }
          iVar8 = 0;
          do {
            ds_free(&RStack_6fcd4.tool_calls[iVar8].result);
            iVar18 = 0;
            do {
              ds_free((RecoveredDString *)
                       ((int)&RStack_6fcd4.tool_calls[iVar8].arguments[0].data + iVar18));
              bVar2 = iVar18 != 0x498;
              iVar18 = iVar18 + 0xc;
            } while (bVar2);
            iVar8 = iVar8 + 1;
          } while (iVar8 < RStack_6fcd4.tool_call_count);
        }
LAB_000245e0:
        ds_free((RecoveredDString *)(local_a0 + -0x2db8));
        ds_free((RecoveredDString *)(local_a0 + -0x2dac));
        if ((iVar21 == 5) || (iVar8 = RStack_992b0.message_count, 0x17b < iVar8))
        goto LAB_00024608;
      }
      child_send_token((RecoveredChildContext *)(puVar3 + -0x2dd8),
                        "\n[Validation: no issues found]\n");
      ds_free(pRVar24);
      ds_free((RecoveredDString *)(puVar3 + -0x2dac));
    }
  }
LAB_00024608:
  child_send_usage(local_9c,iVar17,iVar10);
  pRVar25 = &RStack_992b0;
  child_write_out_json(output_path,(int *)(pRVar25),iVar17,iVar10);
  child_send_done(local_9c,1);
  hook_run(1,input_path,RStack_9c918.working_directory,RStack_992b0.message_count,
            RStack_992b0.estimated_tokens,RStack_9c918.model,RStack_9c918.small_model_mode,
            (RecoveredHooks *)(local_a0 + -0x3de4));
  if (0 < RStack_992b0.message_count) {
    iVar8 = 0;
    do {
      message_free(pRVar25->messages);
      iVar8 = iVar8 + 1;
      pRVar25 = (RecoveredConversation *)(pRVar25->messages + 1);
    } while (iVar8 < RStack_992b0.message_count);
  }
  ds_free(&RStack_9c918.system_prompt);
  cJSON_Delete(object);
                    /* WARNING: Subroutine does not return */
  _exit(0);
code_r0x0002395c:
  iVar21 = execute_plan_steps((RecoveredChildContext *)(local_a0 + -0x2dd8),&RStack_9c918,(int)(&RStack_992b0),plan);
  plan_reset(plan);
  if (0 < RStack_6fcd4.tool_call_count) {
    iVar18 = 0;
    iVar22 = (int)(pRVar24 + -0x44c);
    do {
      ds_free((RecoveredDString *)(iVar22 + 0x510));
      iVar23 = 0;
      do {
        ds_free((RecoveredDString *)(iVar22 + 0x24 + iVar23));
        bVar1 = iVar23 != 0x498;
        iVar23 = iVar23 + 0xc;
      } while (bVar1);
      iVar18 = iVar18 + 1;
      iVar22 = iVar22 + 0x51c;
    } while (iVar18 < RStack_6fcd4.tool_call_count);
  }
  ds_free(&RStack_6fcd4.content);
  ds_free(&RStack_6fcd4.error_or_reasoning);
  if (iVar21 < 1) {
LAB_000239f8:
    if (RStack_6fcd4.tool_call_count == 0) {
      if ((RStack_6fcd4.finish_reason == 3) && (RStack_992b0.message_count < 0x17e)) {
        child_send_token((RecoveredChildContext *)(local_a0 + -0x2dd8),
                          "\n[Auto-continuing: output was truncated by max_tokens limit]\n");
        iVar21 = RStack_992b0.message_count;
        pRVar19 = local_98->messages + RStack_992b0.message_count;
        RStack_992b0.message_count = RStack_992b0.message_count + 1;
        memset(pRVar19,0,0x5c);
        local_98->messages[iVar21].role = 1;
        pRVar24 = &pRVar19->content;
        ds_init(pRVar24);
        ds_append(pRVar24,"Continue from where you left off. Do not repeat what you already wrote."
                  );
      }
      else {
        if ((bVar2) || (0x17d < RStack_992b0.message_count)) goto LAB_00023f74;
        child_send_token((RecoveredChildContext *)(local_a0 + -0x2dd8),
                          "\n[Auto-continue: execute your plan using tools]\n");
        iVar21 = RStack_992b0.message_count;
        pRVar19 = local_98->messages + RStack_992b0.message_count;
        RStack_992b0.message_count = RStack_992b0.message_count + 1;
        memset(pRVar19,0,0x5c);
        local_98->messages[iVar21].role = 1;
        pRVar24 = &pRVar19->content;
        ds_init(pRVar24);
        ds_append(pRVar24,
                   "Now execute the plan using your tools. Do not repeat what you already wrote.");
        bVar2 = true;
      }
      ds_free(&RStack_6fcd4.content);
      ds_free(&RStack_6fcd4.error_or_reasoning);
      if (0 < RStack_6fcd4.tool_call_count) {
        iVar21 = 0;
        pRVar11 = RStack_6fcd4.tool_calls;
        do {
          ds_free(&pRVar11->result);
          iVar18 = 0;
          do {
            ds_free((RecoveredDString *)((int)&pRVar11->arguments[0].data + iVar18));
            bVar1 = iVar18 != 0x498;
            iVar18 = iVar18 + 0xc;
          } while (bVar1);
          iVar21 = iVar21 + 1;
          pRVar11 = pRVar11 + 1;
        } while (iVar21 < RStack_6fcd4.tool_call_count);
      }
    }
    else {
      if ((0 < RStack_6fcd4.tool_call_count) &&
         (RStack_992b0.message_count < 0x17f)) {
        iVar21 = 0;
        call = RStack_6fcd4.tool_calls;
        pRVar11 = call;
        do {
          child_send_tool_call(local_9c,iVar21,call);
          iVar18 = child_ask_approve(local_9c,iVar21,call);
          if (iVar18 == 0) {
            ds_clear(&call->result);
            ds_append(&call->result,"{\"error\": \"User rejected tool call\"}");
          }
          else {
            pRVar15 = malloc(0x8d08c4);
            if (pRVar15 == (RecoveredAppState *)0x0) {
              ds_clear(&call->result);
              ds_append(&call->result,"{\"error\": \"System out of memory\"}");
            }
            else {
              memset(pRVar15,0,0x8d08c4);
              memcpy(&pRVar15->config,&RStack_9c918,0x3668);
              cache_init(pRVar15);
              iVar18 = cache_is_readonly(pRVar11->tool_type);
              if (((iVar18 == 0) || (pRVar11->argument_count < 1)) ||
                 (pRVar14 = cache_find(pRVar15,pRVar11->tool_type,call->arguments,
                                        pRVar11->argument_count),
                 pRVar14 == (RecoveredCacheEntry *)0x0)) {
                pcVar5 = call->mcp_name;
                if (pRVar11->mcp_name[0] == '\0') {
                  pcVar5 = tool_name(pRVar11->tool_type);
                }
                pcVar12 = ds_cstr(call->arguments);
                puVar3 = local_68;
                child_send_tool_run
                          ((RecoveredChildContext *)(local_68 + -0x2dd8),(int)pcVar5,pcVar12);
                iVar18 = tool_execute(call,pRVar15);
                if (iVar18 < 0) {
                  trust_record_failure(pRVar15,pRVar11->tool_type);
                  hook_run(2,input_path,RStack_9c918.working_directory,
                            RStack_992b0.message_count,RStack_992b0.estimated_tokens,
                            RStack_9c918.model,RStack_9c918.small_model_mode,
                            (RecoveredHooks *)(puVar3 + -0x3de4));
                }
                else {
                  iVar18 = cache_is_readonly(pRVar11->tool_type);
                  if ((iVar18 != 0) && (0 < pRVar11->argument_count)) {
                    cache_store(pRVar15,pRVar11->tool_type,call->arguments,pRVar11->argument_count,
                                 &call->result);
                  }
                }
              }
              else {
                ds_clear(&call->result);
                pcVar5 = ds_cstr(&pRVar14->result);
                ds_append(&call->result,pcVar5);
              }
              pcVar5 = trust_warning(pRVar11->tool_type,pRVar15->tool_failures);
              if (pcVar5 != (char *)0x0) {
                ds_init(local_94);
                pRVar24 = &call->result;
                pcVar12 = ds_cstr(pRVar24);
                ds_appendf(local_94,"%s\n%s",pcVar5,pcVar12);
                ds_clear(pRVar24);
                pcVar5 = ds_cstr(local_94);
                ds_append(pRVar24,pcVar5);
                ds_free(local_94);
              }
              free(pRVar15);
            }
          }
          if (0x10000 < (pRVar11->result).length) {
            ds_truncate(&call->result,0x10000);
            ds_append(&call->result,"\n... [result truncated to ~64KB] ...\n");
          }
          iVar18 = RStack_992b0.message_count;
          pRVar19 = local_98->messages + iVar18;
          RStack_992b0.message_count = iVar18 + 1;
          memset(pRVar19,0,0x5c);
          local_98->messages[iVar18].role = 3;
          pRVar24 = &pRVar19->content;
          ds_init(pRVar24);
          ds_copy(pRVar24,&call->result);
          strncpy(pRVar19->call_id,call->call_id,0x3f);
          if (_g_verbose != 0) {
            pcVar5 = tool_name(pRVar11->tool_type);
            pcVar12 = ds_cstr(pRVar24);
            debug("TOOL RESULT (%s): %s\n",pcVar5,pcVar12);
          }
          iVar21 = iVar21 + 1;
          if (RStack_6fcd4.tool_call_count <= iVar21) break;
          call = call + 1;
          pRVar11 = pRVar11 + 1;
        } while (RStack_992b0.message_count < 0x17f);
      }
      pRVar24 = local_64;
      ds_free((RecoveredDString *)((int)(local_64 + -0x44f) + 8));
      ds_free((RecoveredDString *)((int)(pRVar24 + -0x44e) + 8));
      if (0 < RStack_6fcd4.tool_call_count) {
        iVar21 = 0;
        iVar18 = (int)(pRVar24 + -0x44c);
        do {
          ds_free((RecoveredDString *)(iVar18 + 0x510));
          iVar22 = 0;
          do {
            ds_free((RecoveredDString *)(iVar18 + 0x24 + iVar22));
            bVar1 = iVar22 != 0x498;
            iVar22 = iVar22 + 0xc;
          } while (bVar1);
          iVar21 = iVar21 + 1;
          iVar18 = iVar18 + 0x51c;
        } while (iVar21 < RStack_6fcd4.tool_call_count);
      }
    }
  }
  goto LAB_00022f6c;

#undef RStack_9c928
#undef RStack_9c918
#undef RStack_992b0
#undef uStack_908a8
#undef uStack_908a4
#undef uStack_908a0
#undef auStack_9089c
#undef RStack_9070c
#undef acStack_8fb00
#undef RStack_8f700
#undef uStack_8f6ec
#undef RStack_8f6e0
#undef aRStack_8f6d4
#undef auStack_8c928
#undef RStack_6fcd4
#undef RStack_502c8
#undef uStack_b0
#undef uStack_ac
#undef uStack_a8
#undef local_a0
#undef local_9c
#undef local_98
#undef local_94
#undef local_90
#undef local_8c
#undef local_88
#undef local_84
#undef local_80
#undef local_7c
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
}

/*
 * _worker_serialize_in @ 0x00024740..0x00024b1c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Write all configured keys, configuration and conversation to PID-named shared input JSON.
 * Parameters/return (inferred): void _worker_serialize_in(RecoveredAppState *app_state,char *message,char *path,uint path_size)
 * Direct callers: _start_turn@0x000202dc
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; opens/creates/truncates filesystem paths according to the supplied mode; writes text to a stream
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/00024740_worker_serialize_in.s
 * Preserved behavior/security: TEMP-01: all configured credentials and conversation are serialized into a predictable shared temporary file.
 * Preserved behavior/security: GUARD-NOTE: write_guard_enforce is not serialized to workers.
 */
void worker_serialize_in(RecoveredAppState *app_state,char *message,char *path,uint path_size)

{
  int iVar1;
  pid_t pVar2;
  RecoveredCJSON *object;
  RecoveredCJSON *pRVar3;
  char *pcVar4;
  RecoveredCJSON *object_00;
  void *stream;
  int iVar5;
  RecoveredAppState *pRVar6;
  
  pVar2 = getpid();
  snprintf(path,path_size,"/tmp/ppccode_worker_%d_in.json",pVar2);
  object = cJSON_CreateObject();
  cJSON_AddStringToObject(object,"user_msg",message);
  pRVar3 = cJSON_CreateObject();
  pcVar4 = provider_name((app_state->config).provider);
  cJSON_AddStringToObject(pRVar3,"provider",pcVar4);
  cJSON_AddStringToObject(pRVar3,"model",(app_state->config).model);
  cJSON_AddStringToObject(pRVar3,"api_key",(app_state->config).api_key);
  cJSON_AddStringToObject(pRVar3,"openai_key",(app_state->config).openai_key);
  cJSON_AddStringToObject(pRVar3,"anthropic_key",(app_state->config).anthropic_key);
  cJSON_AddStringToObject(pRVar3,"google_key",(app_state->config).google_key);
  cJSON_AddStringToObject(pRVar3,"ollama_key",(app_state->config).ollama_key);
  cJSON_AddStringToObject(pRVar3,"base_url",(app_state->config).base_url);
  cJSON_AddStringToObject(pRVar3,"ollama_url",(app_state->config).ollama_url);
  cJSON_AddStringToObject(pRVar3,"working_dir",(app_state->config).working_directory);
  cJSON_AddNumberToObject
            (pRVar3,"max_tokens",
             app_bits_double(CONCAT44(0x43300000,(app_state->config).max_tokens ^ 0x80000000)) -
             4503601774854144.0);
  cJSON_AddNumberToObject
            (pRVar3,"context_window",
             app_bits_double(CONCAT44(0x43300000,(app_state->config).context_window ^ 0x80000000)) -
             4503601774854144.0);
  cJSON_AddNumberToObject(pRVar3,"temperature",(double)(app_state->config).temperature);
  cJSON_AddNumberToObject
            (pRVar3,"auto_approve",
             app_bits_double(CONCAT44(0x43300000,(app_state->config).auto_approve ^ 0x80000000)) -
             4503601774854144.0);
  cJSON_AddNumberToObject
            (pRVar3,"auto_compact",
             app_bits_double(CONCAT44(0x43300000,(app_state->config).auto_compact ^ 0x80000000)) -
             4503601774854144.0);
  cJSON_AddNumberToObject
            (pRVar3,"effort",
             app_bits_double(CONCAT44(0x43300000,(app_state->config).effort ^ 0x80000000)) -
             4503601774854144.0);
  cJSON_AddNumberToObject
            (pRVar3,"verbose",
             app_bits_double(CONCAT44(0x43300000,(app_state->config).verbose ^ 0x80000000)) -
             4503601774854144.0);
  cJSON_AddNumberToObject
            (pRVar3,"small_model_mode",
             app_bits_double(CONCAT44(0x43300000,(app_state->config).small_model_mode ^ 0x80000000)) -
             4503601774854144.0);
  pcVar4 = ds_cstr(&(app_state->config).system_prompt);
  cJSON_AddStringToObject(pRVar3,"system_prompt",pcVar4);
  cJSON_AddItemToObject(object,"config",pRVar3);
  pRVar3 = cJSON_CreateArray();
  if (0 < (app_state->conversation).message_count) {
    iVar5 = 0;
    pRVar6 = app_state;
    do {
      object_00 = cJSON_CreateObject();
      iVar1 = (pRVar6->conversation).messages[0].role;
      if (iVar1 == 2) {
        pcVar4 = "assistant";
      }
      else if (iVar1 == 3) {
        pcVar4 = "tool";
      }
      else if (iVar1 == 1) {
        pcVar4 = "user";
      }
      else {
        pcVar4 = "system";
      }
      cJSON_AddStringToObject(object_00,"role",pcVar4);
      pcVar4 = ds_cstr(&(app_state->conversation).messages[iVar5].content);
      cJSON_AddStringToObject(object_00,"content",pcVar4);
      cJSON_AddItemToArray(pRVar3,object_00);
      iVar5 = iVar5 + 1;
      pRVar6 = (RecoveredAppState *)((pRVar6->conversation).messages[0].call_id + 0x38);
    } while (iVar5 < (app_state->conversation).message_count);
  }
  cJSON_AddItemToObject(object,"messages",pRVar3);
  pcVar4 = cJSON_PrintUnformatted(object);
  stream = fopen(path,"w");
  if (stream != (void *)0x0) {
    fputs(pcVar4,stream);
    fclose(stream);
  }
  free(pcVar4);
  cJSON_Delete(object);
  return;
}

/*
 * _worker_deserialize_out @ 0x00024b1c..0x00024e34
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Read worker output JSON and merge returned conversation/result state into the parent.
 * Parameters/return (inferred): int _worker_deserialize_out(RecoveredAppState *app,char *path)
 * Direct callers: _tui_run@0x0001df44
 * Possible side effects (direct-call closure, incomplete for indirect calls): opens/creates/truncates filesystem paths according to the supplied mode; reads bytes from a stream
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00024b1c_worker_deserialize_out.s
 */
int worker_deserialize_out(RecoveredAppState *app,char *path)

{
  int iVar1;
  void *stream;
  uint uVar2;
  char *pcVar3;
  RecoveredCJSON *object;
  RecoveredCJSON *pRVar4;
  RecoveredCJSON *pRVar5;
  RecoveredCJSON *pRVar6;
  int index;
  RecoveredMessage *output;
  int iVar7;
  
  stream = fopen(path,"r");
  if (stream != (void *)0x0) {
    fseek(stream,0,2);
    uVar2 = ftell(stream);
    fseek(stream,0,0);
    if (0x3ffffff < uVar2 - 1) {
      fclose(stream);
      return -1;
    }
    pcVar3 = malloc(uVar2 + 1);
    if (pcVar3 == (char *)0x0) {
      fclose(stream);
      return -1;
    }
    uVar2 = fread(pcVar3,1,uVar2,stream);
    pcVar3[uVar2] = '\0';
    fclose(stream);
    object = cJSON_Parse(pcVar3);
    free(pcVar3);
    if (object != (RecoveredCJSON *)0x0) {
      if (0 < (app->conversation).message_count) {
        iVar7 = 0;
        do {
          message_free((app->conversation).messages + iVar7);
          iVar7 = iVar7 + 1;
        } while (iVar7 < (app->conversation).message_count);
      }
      (app->conversation).message_count = 0;
      pRVar4 = cJSON_GetObjectItem(object,"messages");
      if ((pRVar4 != (RecoveredCJSON *)0x0) && (iVar7 = cJSON_GetArraySize(pRVar4), 0 < iVar7)) {
        index = 0;
        do {
          pRVar5 = cJSON_GetArrayItem(pRVar4,index);
          if (pRVar5 != (RecoveredCJSON *)0x0) {
            pRVar6 = cJSON_GetObjectItem(pRVar5,"role");
            pRVar5 = cJSON_GetObjectItem(pRVar5,"content");
            iVar1 = (app->conversation).message_count;
            output = (app->conversation).messages + iVar1;
            (app->conversation).message_count = iVar1 + 1;
            memset(output,0,0x5c);
            ds_init(&output->content);
            if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
              ds_append(&output->content,pRVar5->value_string);
            }
            if ((pRVar6 != (RecoveredCJSON *)0x0) &&
               (pcVar3 = pRVar6->value_string, pcVar3 != (char *)0x0)) {
              iVar1 = strcmp(pcVar3,"user");
              if (iVar1 == 0) {
                output->role = 1;
              }
              else {
                iVar1 = strcmp(pcVar3,"assistant");
                if (iVar1 == 0) {
                  output->role = 2;
                }
                else {
                  iVar1 = strcmp(pcVar3,"tool");
                  if (iVar1 == 0) {
                    output->role = 3;
                  }
                  else {
                    output->role = 0;
                  }
                }
              }
            }
          }
          index = index + 1;
        } while ((iVar7 != index) && (index != 0x180));
      }
      pRVar4 = cJSON_GetObjectItem(object,"usage");
      if (pRVar4 != (RecoveredCJSON *)0x0) {
        pRVar5 = cJSON_GetObjectItem(pRVar4,"prompt_tokens");
        pRVar4 = cJSON_GetObjectItem(pRVar4,"response_tokens");
        if (pRVar5 != (RecoveredCJSON *)0x0) {
          app->input_tokens = pRVar5->value_int;
        }
        if (pRVar4 != (RecoveredCJSON *)0x0) {
          app->output_tokens = pRVar4->value_int;
        }
        app->total_tokens = app->input_tokens + app->output_tokens;
        app->usage_known = 1;
      }
      cJSON_Delete(object);
      return 0;
    }
  }
  return -1;
}

