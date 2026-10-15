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
 * _mcp_write_cb @ 0x0002afdc..0x0002b01c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in mcp: mcp write cb. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): uint _mcp_write_cb(char *data,uint size,uint count,RecoveredDString *output)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002afdc_mcp_write_cb.s
 */
uint mcp_write_cb(char *data,uint size,uint count,RecoveredDString *output)

{
  ds_appendn(output,data,size * count);
  return size * count;
}

/*
 * _mcp_http_post @ 0x0002b01c..0x0002b1fc
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): POST to a configured MCP endpoint with optional token, redirects and disabled peer verification.
 * Parameters/return (inferred): int _mcp_http_post(char *url,char *body,char *token,char *output,int output_size)
 * Direct callers: _tool_execute_mcp@0x0002c0c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002b01c_mcp_http_post.s
 * Preserved behavior/security: TLS-01: disables certificate-chain verification.
 */
int mcp_http_post(char *url,char *body,char *token,char *output,int output_size)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x260] __attribute__((aligned(16)));
#define local_248 (*((RecoveredDString *)(app_frame + 0x8))) /* original SP - 0x248 */
#define local_23c (*((int *)(app_frame + 0x14))) /* original SP - 0x23c */
#define acStack_238 (*((char (*)[532])(app_frame + 0x18))) /* original SP - 0x238 */

  int iVar1;
  void *handle;
  void *list;
  uint length;

  handle = curl_easy_init();
  iVar1 = -1;
  if (handle != (void *)0x0) {
    list = curl_slist_append((void *)0x0,"Content-Type: application/json");
    if ((token != (char *)0x0) && (*token != '\0')) {
      snprintf(acStack_238,0x200,"Authorization: Bearer %s",token);
      list = curl_slist_append(list,acStack_238);
    }
    ds_init(&local_248);
    curl_easy_setopt(handle,0x2712,url);
    curl_easy_setopt(handle,0x271f,body);
    curl_easy_setopt(handle,0x2727,list);
    curl_easy_setopt(handle,0x4e2b,mcp_write_cb);
    curl_easy_setopt(handle,0x2711,&local_248);
    curl_easy_setopt(handle,0xd,0x1e);
    curl_easy_setopt(handle,0x40,0);
    curl_easy_setopt(handle,0x34,1);
    iVar1 = curl_easy_perform(handle);
    local_23c = 0;
    curl_easy_getinfo(handle,0x200002,&local_23c);
    curl_easy_cleanup(handle);
    curl_slist_free_all(list);
    if (((iVar1 == 0) && (199 < local_23c)) && (local_23c < 300)) {
      length = output_size - 1U;
      if (local_248.length < output_size - 1U) {
        length = local_248.length;
      }
      if ((length != 0) && (local_248.data != (char *)0x0)) {
        memcpy(output,local_248.data,length);
      }
      output[length] = '\0';
      ds_free(&local_248);
      iVar1 = 0;
    }
    else {
      ds_free(&local_248);
      iVar1 = -1;
    }
  }
  return iVar1;

#undef local_248
#undef local_23c
#undef acStack_238
}

/*
 * _mcp_http_get @ 0x0002b1fc..0x0002b3bc
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): GET MCP discovery data with optional token, redirects and disabled peer verification.
 * Parameters/return (inferred): int _mcp_http_get(char *url,char *token,char *output,int output_size)
 * Direct callers: _mcp_discover_server@0x0002b980
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002b1fc_mcp_http_get.s
 * Preserved behavior/security: TLS-01: disables certificate-chain verification.
 */
int mcp_http_get(char *url,char *token,char *output,int output_size)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x250] __attribute__((aligned(16)));
#define local_238 (*((RecoveredDString *)(app_frame + 0x8))) /* original SP - 0x238 */
#define local_22c (*((int *)(app_frame + 0x14))) /* original SP - 0x22c */
#define acStack_228 (*((char (*)[524])(app_frame + 0x18))) /* original SP - 0x228 */

  int iVar1;
  void *handle;
  void *list;
  uint length;

  handle = curl_easy_init();
  iVar1 = -1;
  if (handle != (void *)0x0) {
    if ((token == (char *)0x0) || (*token == '\0')) {
      list = (void *)0x0;
    }
    else {
      snprintf(acStack_228,0x200,"Authorization: Bearer %s",token);
      list = curl_slist_append((void *)0x0,acStack_228);
    }
    ds_init(&local_238);
    curl_easy_setopt(handle,0x2712,url);
    curl_easy_setopt(handle,0x2727,list);
    curl_easy_setopt(handle,0x4e2b,mcp_write_cb);
    curl_easy_setopt(handle,0x2711,&local_238);
    curl_easy_setopt(handle,0xd,0x1e);
    curl_easy_setopt(handle,0x40,0);
    curl_easy_setopt(handle,0x34,1);
    iVar1 = curl_easy_perform(handle);
    local_22c = 0;
    curl_easy_getinfo(handle,0x200002,&local_22c);
    curl_easy_cleanup(handle);
    if (list != (void *)0x0) {
      curl_slist_free_all(list);
    }
    if ((iVar1 == 0) && (local_22c == 200)) {
      length = output_size - 1U;
      if (local_238.length < output_size - 1U) {
        length = local_238.length;
      }
      if ((length != 0) && (local_238.data != (char *)0x0)) {
        memcpy(output,local_238.data,length);
      }
      output[length] = '\0';
      ds_free(&local_238);
      iVar1 = 0;
    }
    else {
      ds_free(&local_238);
      iVar1 = -1;
    }
  }
  return iVar1;

#undef local_238
#undef local_22c
#undef acStack_228
}

/*
 * _mcp_stdio_connect @ 0x0002b3bc..0x0002b4f0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Fork and execlp the configured executable with inherited environment and pipe-connected standard streams.
 * Parameters/return (inferred): int _mcp_stdio_connect(RecoveredMCPServer *server)
 * Direct callers: _mcp_discover_server@0x0002b980, _tool_execute_mcp@0x0002c0c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): resolves/executes a program through PATH; creates a same-account child process
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002b3bc_mcp_stdio_connect.s
 */
int mcp_stdio_connect(RecoveredMCPServer *server)

{
  int iVar1;
  int iVar2;
  pid_t pVar3;
  
  iVar2 = pipe(server->request_pipe);
  iVar1 = -1;
  if (iVar2 == 0) {
    iVar1 = pipe(server->response_pipe);
    if (iVar1 == 0) {
      pVar3 = fork();
      if (pVar3 < 0) {
        close(server->request_pipe[0]);
        close(server->request_pipe[1]);
        close(server->response_pipe[0]);
        close(server->response_pipe[1]);
        iVar1 = -1;
      }
      else {
        if (pVar3 == 0) {
          close(server->request_pipe[0]);
          close(server->response_pipe[1]);
          dup2(server->response_pipe[0],0);
          dup2(server->request_pipe[1],1);
          dup2(server->request_pipe[1],2);
          close(server->response_pipe[0]);
          close(server->request_pipe[1]);
          execlp(server->command,server->command,(char *)NULL);
                    /* WARNING: Subroutine does not return */
          _exit(0x7f);
        }
        server->pid = pVar3;
        close(server->request_pipe[1]);
        close(server->response_pipe[0]);
        server->connected = 1;
        iVar1 = 0;
      }
    }
    else {
      close(server->request_pipe[0]);
      close(server->request_pipe[1]);
      iVar1 = -1;
    }
  }
  return iVar1;
}

/*
 * _mcp_stdio_rpc @ 0x0002b4f0..0x0002b6fc
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Send an RPC message over the stdio pipe and collect a bounded response with a timeout.
 * Parameters/return (inferred): int _mcp_stdio_rpc(RecoveredMCPServer *server,char *request,char *output,int output_size, int timeout_seconds)
 * Direct callers: _mcp_discover_server@0x0002b980, _tool_execute_mcp@0x0002c0c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): reads a descriptor or IPC channel; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002b4f0_mcp_stdio_rpc.s
 */
int mcp_stdio_rpc(RecoveredMCPServer *server,char *request,char *output,int output_size,
                  int timeout_seconds)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x10e0] __attribute__((aligned(16)));
#define afStack_10c8 (*((fd_set (*)[1])(app_frame + 0x8))) /* original SP - 0x10c8 */
#define local_1048 (*((struct timeval *)(app_frame + 0x88))) /* original SP - 0x1048 */
#define local_1040 (*((char *)(app_frame + 0x90))) /* original SP - 0x1040 */
#define acStack_103f (*((char (*)[4123])(app_frame + 0x91))) /* original SP - 0x103f */

  char cVar1;
  uint uVar2;
  void *output_00;
  uint uVar3;
  int iVar4;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  int iVar8;

  iVar8 = server->response_pipe[1];
  if ((-1 < iVar8) && (-1 < server->request_pipe[0])) {
    uVar2 = strlen(request);
    if ((uVar2 == 0) || (request[uVar2 - 1] == '\n')) {
      write(iVar8,request,uVar2);
    }
    else {
      output_00 = malloc(uVar2 + 2);
      if (output_00 == (void *)0x0) {
        return -1;
      }
      memcpy(output_00,request,uVar2);
      *(undefined1 *)((int)output_00 + uVar2) = 10;
      *(undefined1 *)((int)output_00 + uVar2 + 1) = 0;
      write(server->response_pipe[1],output_00,uVar2 + 1);
      free(output_00);
    }
    uVar7 = output_size - 1;
    uVar2 = 0;
    while( true ) {
      while( true ) {
        uVar6 = uVar2;
        if ((uVar7 <= uVar2) || (timeout_seconds < 0)) goto LAB_0002b6b4;
        memset(afStack_10c8,0,0x80);
        uVar3 = server->request_pipe[0];
        uVar5 = uVar3 >> 3 & 0x1ffffffc;
        *(uint *)(((char *)afStack_10c8) + uVar5) =
             1 << (uVar3 & 0x1f) | *(uint *)(((char *)afStack_10c8) + uVar5);
        local_1048.tv_sec = 1;
        local_1048.tv_usec = 0;
        iVar8 = select(uVar3 + 1,afStack_10c8,(fd_set *)0x0,(fd_set *)0x0,&local_1048);
        if (iVar8 < 0) goto LAB_0002b6b4;
        if (iVar8 != 0) break;
        timeout_seconds = timeout_seconds + -1;
      }
      iVar8 = read(server->request_pipe[0],&local_1040,0x1000);
      if (iVar8 < 1) break;
      if (local_1040 == '\n') {
LAB_0002b670:
        output[uVar6] = '\0';
        return 0;
      }
      iVar4 = 0;
      cVar1 = local_1040;
      while( true ) {
        output[uVar2 + iVar4] = cVar1;
        uVar6 = uVar6 + 1;
        iVar8 = iVar8 + -1;
        if (iVar8 == 0) break;
        if (uVar7 - uVar2 == (uint)(iVar4 + 1)) goto LAB_0002b6b4;
        cVar1 = acStack_103f[iVar4];
        iVar4 = iVar4 + 1;
        if (cVar1 == '\n') goto LAB_0002b670;
      }
      uVar2 = uVar6;
      if (uVar7 <= uVar6) break;
    }
LAB_0002b6b4:
    output[uVar6] = '\0';
  }
  return -1;

#undef afStack_10c8
#undef local_1048
#undef local_1040
#undef acStack_103f
}

/*
 * _mcp_init @ 0x0002b6fc..0x0002b77c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in mcp: mcp init. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _mcp_init(void)
 * Direct callers: _main@0x00005498
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002b6fc_mcp_init.s
 */
void mcp_init(void)

{
  RecoveredMCPState *pRVar1;
  int *piVar2;
  int iVar3;
  
  pRVar1 = PTR__g_mcp_000396b4;
  memset(PTR__g_mcp_000396b4,0,0x93584);
  piVar2 = pRVar1->servers[0].response_pipe;
  iVar3 = 4;
  do {
    *piVar2 = -1;
    piVar2[1] = -1;
    piVar2[-2] = -1;
    piVar2[-1] = -1;
    piVar2[-3] = -1;
    piVar2 = piVar2 + 0x9358;
    iVar3 = iVar3 + -1;
  } while (iVar3 != 0);
  return;
}

/*
 * _mcp_shutdown @ 0x0002b77c..0x0002b79c
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in mcp: mcp shutdown. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _mcp_shutdown(void)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): signals a process
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002b77c_mcp_shutdown.s
 */
void mcp_shutdown(void)

{
  mcp_disconnect_all();
  return;
}

/*
 * _mcp_register @ 0x0002b79c..0x0002b980
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Parse name|transport|endpoint-or-command|token into one of four configured MCP server records.
 * Parameters/return (inferred): int _mcp_register(char *configuration)
 * Direct callers: _main@0x00005498
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002b79c_mcp_register.s
 */
int mcp_register(char *configuration)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x440] __attribute__((aligned(16)));
#define local_428 (*((char (*)[1023])(app_frame + 0x8))) /* original SP - 0x428 */
#define local_29 (*((undefined1 *)(app_frame + 0x407))) /* original SP - 0x29 */
#define local_28 (*((char * (*)[8])(app_frame + 0x408))) /* original SP - 0x28 */

  char *pcVar1;
  int iVar2;
  RecoveredMCPServer *output;

  if (((configuration == (char *)0x0) || (*configuration == '\0')) ||
     (3 < PTR__g_mcp_000396b4->count)) {
    iVar2 = -1;
  }
  else {
    strncpy(local_428,configuration,0x3ff);
    local_29 = 0;
    local_28[1] = (char *)0x0;
    local_28[2] = (char *)0x0;
    local_28[3] = (char *)0x0;
    pcVar1 = local_428;
    local_28[0] = pcVar1;
    if (local_428[0] != '\0') {
      iVar2 = 1;
      do {
        if (local_428[0] == '|') {
          *pcVar1 = '\0';
          local_28[iVar2] = pcVar1 + 1;
          iVar2 = iVar2 + 1;
        }
        pcVar1 = pcVar1 + 1;
        local_428[0] = *pcVar1;
      } while ((local_428[0] != '\0') && (iVar2 < 4));
    }
    output = PTR__g_mcp_000396b4->servers + PTR__g_mcp_000396b4->count;
    PTR__g_mcp_000396b4->count = PTR__g_mcp_000396b4->count + 1;
    memset(output,0,0x24d60);
    output->response_pipe[0] = -1;
    output->response_pipe[1] = -1;
    output->request_pipe[0] = -1;
    output->request_pipe[1] = -1;
    output->pid = -1;
    if (local_28[0] != (char *)0x0) {
      app_copy_padded(output->name,local_28[0],0x3f);
    }
    if ((local_28[1] == (char *)0x0) || (iVar2 = strcmp(local_28[1],"stdio"), iVar2 != 0)) {
      output->transport = 0;
    }
    else {
      output->transport = 1;
    }
    if (local_28[2] != (char *)0x0) {
      if (output->transport == 0) {
        strncpy(output->base_url,local_28[2],0x1ff);
      }
      else {
        strncpy(output->command,local_28[2],0x1ff);
      }
    }
    iVar2 = 0;
    if (local_28[3] != (char *)0x0) {
      strncpy(output->token,local_28[3],0xff);
      iVar2 = 0;
    }
  }
  return iVar2;

#undef local_428
#undef local_29
#undef local_28
}

/*
 * _mcp_discover_server @ 0x0002b980..0x0002bc28
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Fetch HTTP tool schemas or issue tools/list over a configured stdio server and populate tool metadata.
 * Parameters/return (inferred): int _mcp_discover_server(RecoveredMCPServer *server)
 * Direct callers: _mcp_discover_all@0x0002bc28
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O; resolves/executes a program through PATH; creates a same-account child process; reads a descriptor or IPC channel; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002b980_mcp_discover_server.s
 */
int mcp_discover_server(RecoveredMCPServer *server)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x11050] __attribute__((aligned(16)));
#define cStack_11038 (*((char *)(app_frame + 0x8))) /* original SP - 0x11038 */
#define auStack_11037 (*((undefined1 (*)[65535])(app_frame + 0x9))) /* original SP - 0x11037 */
#define acStack_1038 (*((char (*)[4108])(app_frame + 0x10008))) /* original SP - 0x1038 */

  int iVar1;
  RecoveredCJSON *object;
  RecoveredCJSON *pRVar2;
  RecoveredCJSON *pRVar3;
  RecoveredCJSON *pRVar4;
  RecoveredCJSON *pRVar5;
  char *text;
  int index;
  RecoveredMCPTool *output;

  cStack_11038 = '\0';
  memset(auStack_11037,0,0xffff);
  if (server->transport == 0) {
    snprintf(acStack_1038,0x400,"%s/mcp/v1/tools",server->base_url);
    iVar1 = mcp_http_get(acStack_1038,server->token,&cStack_11038,0x10000);
  }
  else {
    if ((server->connected == 0) && (iVar1 = mcp_stdio_connect(server), iVar1 != 0)) {
      return -1;
    }
    snprintf(acStack_1038,0x1000,
                       "{\"jsonrpc\":\"2.0\",\"method\":\"tools/list\",\"id\":1}");
    iVar1 = mcp_stdio_rpc(server,acStack_1038,&cStack_11038,0x10000,10);
  }
  if ((iVar1 != 0) || (object = cJSON_Parse(&cStack_11038), object == (RecoveredCJSON *)0x0)) {
    return -1;
  }
  if (server->transport == 0) {
    pRVar2 = cJSON_GetObjectItem(object,"tools");
  }
  else {
    pRVar2 = cJSON_GetObjectItem(object,"result");
    if (pRVar2 == (RecoveredCJSON *)0x0) goto LAB_0002bacc;
    pRVar2 = cJSON_GetObjectItem(pRVar2,"tools");
  }
  if (pRVar2 != (RecoveredCJSON *)0x0) {
    iVar1 = cJSON_GetArraySize(pRVar2);
    if ((0 < iVar1) && (server->tool_count < 0x20)) {
      index = 0;
      do {
        pRVar3 = cJSON_GetArrayItem(pRVar2,index);
        if (pRVar3 != (RecoveredCJSON *)0x0) {
          pRVar4 = cJSON_GetObjectItem(pRVar3,"name");
          pRVar5 = cJSON_GetObjectItem(pRVar3,"description");
          pRVar3 = cJSON_GetObjectItem(pRVar3,"parameters");
          output = server->tools + server->tool_count;
          server->tool_count = server->tool_count + 1;
          memset(output,0,0x1240);
          if ((pRVar4 != (RecoveredCJSON *)0x0) && (pRVar4->value_string != (char *)0x0)) {
            strncpy(output->name,pRVar4->value_string,0x3f);
          }
          if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0)) {
            strncpy(output->description,pRVar5->value_string,0x1ff);
          }
          if ((pRVar3 != (RecoveredCJSON *)0x0) &&
             (text = cJSON_Print(pRVar3), text != (char *)0x0)) {
            strncpy(output->parameters_json,text,0xfff);
            free(text);
          }
        }
        index = index + 1;
      } while ((iVar1 != index) && (server->tool_count < 0x20));
    }
    cJSON_Delete(object);
    return 0;
  }
LAB_0002bacc:
  cJSON_Delete(object);
  return -1;

#undef cStack_11038
#undef auStack_11037
#undef acStack_1038
}

/*
 * _mcp_discover_all @ 0x0002bc28..0x0002bcb4
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Discover tools for every configured MCP server.
 * Parameters/return (inferred): int _mcp_discover_all(void)
 * Direct callers: _main@0x00005498
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O; resolves/executes a program through PATH; creates a same-account child process; reads a descriptor or IPC channel; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002bc28_mcp_discover_all.s
 */
int mcp_discover_all(void)

{
  RecoveredMCPState *pRVar1;
  int iVar2;
  int iVar3;
  int iVar4;
  RecoveredMCPState *server;
  
  pRVar1 = PTR__g_mcp_000396b4;
  iVar3 = 0;
  if (0 < PTR__g_mcp_000396b4->count) {
    iVar4 = 0;
    iVar3 = 0;
    server = PTR__g_mcp_000396b4;
    do {
      iVar2 = mcp_discover_server(server->servers);
      if (iVar2 == 0) {
        iVar3 = iVar3 + 1;
      }
      iVar4 = iVar4 + 1;
      server = (RecoveredMCPState *)(server->servers + 1);
    } while (iVar4 < pRVar1->count);
  }
  return iVar3;
}

/*
 * _mcp_disconnect_all @ 0x0002bcb4..0x0002bdac
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in mcp: mcp disconnect all. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _mcp_disconnect_all(void)
 * Direct callers: _mcp_shutdown@0x0002b77c
 * Possible side effects (direct-call closure, incomplete for indirect calls): signals a process
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002bcb4_mcp_disconnect_all.s
 */
void mcp_disconnect_all(void)

{
  RecoveredMCPState *pRVar1;
  int iVar2;
  int *piVar3;
  
  pRVar1 = PTR__g_mcp_000396b4;
  if (0 < PTR__g_mcp_000396b4->count) {
    iVar2 = 0;
    piVar3 = &PTR__g_mcp_000396b4->servers[0].pid;
    do {
      if (0 < *piVar3) {
        kill(*piVar3,0xf);
        waitpid(*piVar3,(int *)0x0,0);
        *piVar3 = -1;
      }
      if (-1 < piVar3[3]) {
        close(piVar3[3]);
        piVar3[3] = -1;
      }
      if (-1 < piVar3[4]) {
        close(piVar3[4]);
        piVar3[4] = -1;
      }
      if (-1 < piVar3[1]) {
        close(piVar3[1]);
        piVar3[1] = -1;
      }
      if (-1 < piVar3[2]) {
        close(piVar3[2]);
        piVar3[2] = -1;
      }
      piVar3[-1] = 0;
      iVar2 = iVar2 + 1;
      piVar3 = piVar3 + 0x9358;
    } while (iVar2 < pRVar1->count);
  }
  return;
}

/*
 * _mcp_find_tool @ 0x0002bdac..0x0002bed8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in mcp: mcp find tool. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _mcp_find_tool(char *name,RecoveredMCPServer **server,RecoveredMCPTool **tool)
 * Direct callers: _tool_execute_mcp@0x0002c0c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002bdac_mcp_find_tool.s
 */
int mcp_find_tool(char *name,RecoveredMCPServer **server,RecoveredMCPTool **tool)

{
  int iVar1;
  int iVar2;
  int iVar3;
  RecoveredMCPState *pRVar4;
  int iVar5;
  int iVar6;
  byte bVar7;
  byte bVar8;
  byte bVar9;
  
  if (((name != (char *)0x0) && (*name != '\0')) && (iVar2 = PTR__g_mcp_000396b4->count, 0 < iVar2))
  {
    iVar3 = 0;
    bVar7 = (server == (RecoveredMCPServer **)0x0) << 1;
    pRVar4 = PTR__g_mcp_000396b4;
    do {
      iVar5 = pRVar4->servers[0].tool_count;
      if (0 < iVar5) {
        iVar6 = 0;
        bVar8 = (byte)(((uint)(byte)((tool == (RecoveredMCPTool **)0x0) << 1) << 0x1c) >> 0x1c);
        bVar9 = bVar7;
        do {
          iVar1 = strcmp(pRVar4->servers[0].tools[iVar6].name,name);
          if (iVar1 == 0) {
            if (!(bool)(bVar9 >> 1 & 1)) {
              *server = pRVar4->servers;
            }
            if ((bool)(bVar8 >> 1 & 1)) {
              return 0;
            }
            *tool = pRVar4->servers[0].tools + iVar6;
            return 0;
          }
          iVar6 = iVar6 + 1;
        } while (iVar6 != iVar5);
      }
      iVar3 = iVar3 + 1;
      pRVar4 = (RecoveredMCPState *)(pRVar4->servers + 1);
    } while (iVar2 != iVar3);
  }
  return -1;
}

/*
 * _mcp_build_tool_schemas @ 0x0002bed8..0x0002c004
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Construct mcp build tool schemas data; inspect the preserved body for exact field names and conditions.
 * Parameters/return (inferred): void _mcp_build_tool_schemas(RecoveredDString *output)
 * Direct callers: _mcp_get_tools_json@0x0002c004
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002bed8_mcp_build_tool_schemas.s
 */
void mcp_build_tool_schemas(RecoveredDString *output)

{
  bool bVar1;
  RecoveredMCPState *pRVar2;
  char *pcVar3;
  int iVar4;
  RecoveredMCPState *pRVar5;
  int *piVar6;
  int iVar7;
  int iVar8;
  char *pcVar9;
  
  ds_append(output,"[");
  pRVar2 = PTR__g_mcp_000396b4;
  if (0 < PTR__g_mcp_000396b4->count) {
    bVar1 = true;
    iVar4 = 0;
    piVar6 = &PTR__g_mcp_000396b4->servers[0].tool_count;
    pRVar5 = PTR__g_mcp_000396b4;
    do {
      if (0 < *piVar6) {
        iVar7 = 0;
        iVar8 = 0;
        do {
          pcVar9 = pRVar5->servers[0].tools[0].name + iVar8;
          if (!bVar1) {
            ds_append(output,",");
          }
          pcVar3 = pcVar9 + 0x240;
          if (pcVar9[0x240] == '\0') {
            pcVar3 = "{}";
          }
          ds_appendf(output,
                      "{\"type\":\"function\",\"function\":{\"name\":\"%s\",\"description\":\"%s\",\"parameters\":%s}}"
                      ,pcVar9,pcVar9 + 0x40,pcVar3);
          iVar7 = iVar7 + 1;
          iVar8 = iVar8 + 0x1240;
          bVar1 = false;
        } while (iVar7 < *piVar6);
      }
      iVar4 = iVar4 + 1;
      piVar6 = piVar6 + 0x9358;
      pRVar5 = (RecoveredMCPState *)(pRVar5->servers + 1);
    } while (iVar4 < pRVar2->count);
  }
  ds_append(output,"]");
  return;
}

/*
 * _mcp_get_tools_json @ 0x0002c004..0x0002c0c8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in mcp: mcp get tools json. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): RecoveredCJSON * _mcp_get_tools_json(void)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002c004_mcp_get_tools_json.s
 */
RecoveredCJSON * mcp_get_tools_json(void)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x40] __attribute__((aligned(16)));
#define aRStack_28 (*((RecoveredDString (*)[2])(app_frame + 0x8))) /* original SP - 0x28 */

  int iVar1;
  int *piVar2;
  char *text;
  int iVar3;
  RecoveredCJSON *pRVar4;

  iVar3 = PTR__g_mcp_000396b4->count;
  if (iVar3 < 1) {
LAB_0002c034:
    pRVar4 = (RecoveredCJSON *)0x0;
  }
  else {
    if (PTR__g_mcp_000396b4->servers[0].tool_count < 1) {
      piVar2 = &PTR__g_mcp_000396b4->servers[1].tool_count;
      do {
        iVar3 = iVar3 + -1;
        if (iVar3 == 0) goto LAB_0002c034;
        iVar1 = *piVar2;
        piVar2 = piVar2 + 0x9358;
      } while (iVar1 < 1);
    }
    ds_init(aRStack_28);
    mcp_build_tool_schemas(aRStack_28);
    text = ds_cstr(aRStack_28);
    pRVar4 = cJSON_Parse(text);
    ds_free(aRStack_28);
  }
  return pRVar4;

#undef aRStack_28
}

/*
 * _tool_execute_mcp @ 0x0002c0c8..0x0002c5b8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Find the configured server/tool, encode arguments and invoke the HTTP or stdio call path.
 * Parameters/return (inferred): int _tool_execute_mcp(RecoveredToolCall *tool_call,RecoveredAppState *app_state)
 * Direct callers: _tool_execute@0x000158c8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; performs network/protocol I/O; resolves/executes a program through PATH; creates a same-account child process; reads a descriptor or IPC channel; writes a descriptor
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002c0c8_tool_execute_mcp.s
 */
int tool_execute_mcp(RecoveredToolCall *tool_call,RecoveredAppState *app_state)

{
  (void)app_state; /* Present in the recovered ABI; unused by this routine. */
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x10470] __attribute__((aligned(16)));
#define pRStack_10458 (*((RecoveredMCPServer * *)(app_frame + 0x8))) /* original SP - 0x10458 */
#define pRStack_10454 (*((RecoveredMCPTool * *)(app_frame + 0xc))) /* original SP - 0x10454 */
#define RStack_10450 (*((RecoveredDString *)(app_frame + 0x10))) /* original SP - 0x10450 */
#define RStack_10444 (*((RecoveredDString *)(app_frame + 0x1c))) /* original SP - 0x10444 */
#define cStack_10438 (*((char *)(app_frame + 0x28))) /* original SP - 0x10438 */
#define auStack_10437 (*((undefined1 (*)[65535])(app_frame + 0x29))) /* original SP - 0x10437 */
#define acStack_438 (*((char (*)[1040])(app_frame + 0x10028))) /* original SP - 0x438 */

  RecoveredMCPServer *server;
  int iVar1;
  char *pcVar2;
  char *pcVar3;
  RecoveredCJSON *object;
  RecoveredCJSON *pRVar4;
  RecoveredCJSON *pRVar5;
  char *pcVar6;
  RecoveredDString *str;
  int iVar7;

  if ((tool_call == (RecoveredToolCall *)0x0) || (tool_call->mcp_name[0] == '\0')) {
    ds_appendf(&tool_call->result,"MCP error: no tool name\n");
    iVar1 = -1;
  }
  else {
    pRStack_10458 = (RecoveredMCPServer *)0x0;
    pRStack_10454 = (RecoveredMCPTool *)0x0;
    pcVar6 = tool_call->mcp_name;
    iVar1 = mcp_find_tool(pcVar6,&pRStack_10458,&pRStack_10454);
    if (iVar1 == 0) {
      ds_init(&RStack_10450);
      ds_append(&RStack_10450,"{");
      if (0 < tool_call->argument_count) {
        iVar7 = 0;
        iVar1 = 0;
        while( true ) {
          if (iVar7 + 1 < tool_call->argument_count) {
            str = (RecoveredDString *)((int)&tool_call->arguments[0].data + iVar1);
            pcVar2 = ds_cstr(str);
            pcVar3 = ds_cstr(str + 1);
            ds_appendf(&RStack_10450,"\"%s\":\"%s\"",pcVar2,pcVar3);
          }
          iVar7 = iVar7 + 2;
          if (tool_call->argument_count <= iVar7) break;
          if (0 < iVar7) {
            ds_append(&RStack_10450,",");
          }
          iVar1 = iVar1 + 0x18;
        }
      }
      ds_append(&RStack_10450,"}");
      ds_init(&RStack_10444);
      cStack_10438 = '\0';
      memset(auStack_10437,0,0xffff);
      if (pRStack_10458->transport == 0) {
        pcVar2 = ds_cstr(&RStack_10450);
        ds_appendf(&RStack_10444,"{\"tool\":\"%s\",\"arguments\":%s}",pcVar6,pcVar2);
        snprintf(acStack_438,0x400,"%s/mcp/v1/call",pRStack_10458->base_url);
        pcVar2 = ds_cstr(&RStack_10444);
        iVar1 = mcp_http_post(acStack_438,pcVar2,pRStack_10458->token,&cStack_10438,0x10000);
      }
      else {
        if ((pRStack_10458->connected == 0) &&
           (iVar1 = mcp_stdio_connect(pRStack_10458), iVar1 != 0)) {
          ds_appendf(&tool_call->result,"MCP error: cannot connect to stdio server \'%s\'\n",
                      (char *)(pRStack_10458));
          ds_free(&RStack_10450);
          ds_free(&RStack_10444);
          return -1;
        }
        pcVar2 = ds_cstr(&RStack_10450);
        ds_appendf(&RStack_10444,
                    "{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"%s\",\"arguments\":%s},\"id\":1}"
                    ,pcVar6,pcVar2);
        server = pRStack_10458;
        pcVar2 = ds_cstr(&RStack_10444);
        iVar1 = mcp_stdio_rpc(server,pcVar2,&cStack_10438,0x10000,0x1e);
      }
      if (iVar1 == 0) {
        object = cJSON_Parse(&cStack_10438);
        if (object == (RecoveredCJSON *)0x0) {
          ds_append(&tool_call->result,&cStack_10438);
          ds_free(&RStack_10450);
          ds_free(&RStack_10444);
          iVar1 = 0;
        }
        else {
          if (pRStack_10458->transport == 0) {
            pRVar4 = cJSON_GetObjectItem(object,"result");
            pRVar5 = object;
            if ((pRVar4 == (RecoveredCJSON *)0x0) ||
               (pRVar5 = pRVar4, pRVar4->value_string == (char *)0x0)) {
              pcVar2 = cJSON_Print(pRVar5);
              pcVar6 = pcVar2;
              if (pcVar2 == (char *)0x0) {
                pcVar6 = "null";
              }
              ds_append(&tool_call->result,pcVar6);
              if (pcVar2 != (char *)0x0) {
                free(pcVar2);
              }
            }
            else {
              ds_append(&tool_call->result,pRVar4->value_string);
            }
          }
          else {
            pRVar5 = cJSON_GetObjectItem(object,"result");
            if (pRVar5 == (RecoveredCJSON *)0x0) {
              pRVar5 = cJSON_GetObjectItem(object,"error");
              if (pRVar5 == (RecoveredCJSON *)0x0) {
                pcVar2 = cJSON_Print(object);
                pcVar6 = pcVar2;
                if (pcVar2 == (char *)0x0) {
                  pcVar6 = "null";
                }
                ds_append(&tool_call->result,pcVar6);
                if (pcVar2 != (char *)0x0) {
                  free(pcVar2);
                }
              }
              else {
                pRVar5 = cJSON_GetObjectItem(pRVar5,"message");
                if ((pRVar5 == (RecoveredCJSON *)0x0) ||
                   (pcVar6 = pRVar5->value_string, pcVar6 == (char *)0x0)) {
                  pcVar6 = "unknown";
                }
                ds_appendf(&tool_call->result,"MCP error: %s\n",pcVar6);
              }
            }
            else {
              pRVar4 = cJSON_GetObjectItem(pRVar5,"content");
              if (pRVar4 == (RecoveredCJSON *)0x0) {
                pcVar2 = cJSON_Print(pRVar5);
                pcVar6 = pcVar2;
                if (pcVar2 == (char *)0x0) {
                  pcVar6 = "null";
                }
                ds_append(&tool_call->result,pcVar6);
                if (pcVar2 != (char *)0x0) {
                  free(pcVar2);
                }
              }
              else {
                iVar1 = cJSON_GetArraySize(pRVar4);
                if (0 < iVar1) {
                  iVar7 = 0;
                  do {
                    pRVar5 = cJSON_GetArrayItem(pRVar4,iVar7);
                    pRVar5 = cJSON_GetObjectItem(pRVar5,"text");
                    if ((pRVar5 != (RecoveredCJSON *)0x0) && (pRVar5->value_string != (char *)0x0))
                    {
                      ds_append(&tool_call->result,pRVar5->value_string);
                    }
                    iVar7 = iVar7 + 1;
                  } while (iVar1 != iVar7);
                }
              }
            }
          }
          cJSON_Delete(object);
          ds_free(&RStack_10450);
          ds_free(&RStack_10444);
          iVar1 = 0;
        }
      }
      else {
        ds_appendf(&tool_call->result,"MCP error: call to \'%s\' failed\n",pcVar6);
        ds_free(&RStack_10450);
        ds_free(&RStack_10444);
        iVar1 = -1;
      }
    }
    else {
      ds_appendf(&tool_call->result,"MCP error: tool \'%s\' not found in any server\n",pcVar6);
      iVar1 = -1;
    }
  }
  return iVar1;

#undef pRStack_10458
#undef pRStack_10454
#undef RStack_10450
#undef RStack_10444
#undef cStack_10438
#undef auStack_10437
#undef acStack_438
}

