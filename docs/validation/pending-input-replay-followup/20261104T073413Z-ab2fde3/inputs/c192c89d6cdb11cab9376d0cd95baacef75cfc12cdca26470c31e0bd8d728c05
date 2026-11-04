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
 * _ensure_curl_init @ 0x000165d0..0x0001661c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Initialize libcurl once using a process-global flag.
 * Parameters/return (inferred): void _ensure_curl_init(void)
 * Direct callers: _http_post_sse2@0x000169cc, _http_post_json@0x00016d48, _http_get@0x00016eb8
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000165d0_ensure_curl_init.s
 */
void ensure_curl_init(void)

{
  if (_curl_initialized == 0) {
    curl_global_init(3);
    _curl_initialized = 1;
  }
  return;
}

/*
 * _http_buf_init @ 0x0001661c..0x00016630
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage the HTTP response accumulation buffer: init. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _http_buf_init(RecoveredHttpBuffer *buffer)
 * Direct callers: _do_async_fetch@0x000087a8, _ollama_sync_model_metadata@0x0000bb9c, _http_buf_free@0x000166e8, _sse_buf_init@0x00016720, _sse2_adapter_cb@0x000167f8, _http_post_sse2@0x000169cc
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0001661c_http_buf_init.s
 */
void http_buf_init(RecoveredHttpBuffer *buffer)

{
  buffer->data = (char *)0x0;
  buffer->length = 0;
  buffer->capacity = 0;
  return;
}

/*
 * _http_buf_append @ 0x00016630..0x000166e8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage the HTTP response accumulation buffer: append. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _http_buf_append(RecoveredHttpBuffer *buffer,char *data,uint length)
 * Direct callers: _write_cb@0x000167b8, _sse2_adapter_cb@0x000167f8
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00016630_http_buf_append.s
 */
void http_buf_append(RecoveredHttpBuffer *buffer,char *data,uint length)

{
  uint uVar1;
  uint uVar2;
  char *pcVar3;
  uint capacity;
  
  uVar2 = length + buffer->length + 1;
  uVar1 = buffer->capacity;
  if (uVar1 < uVar2) {
    capacity = 0x1000;
    if (uVar1 != 0) {
      capacity = uVar1 << 1;
    }
    for (; capacity < uVar2; capacity = capacity << 1) {
    }
    pcVar3 = realloc(buffer->data,capacity);
    if (pcVar3 == (char *)0x0) {
      return;
    }
    buffer->data = pcVar3;
    buffer->capacity = capacity;
  }
  memcpy(buffer->data + buffer->length,data,length);
  uVar1 = length + buffer->length;
  buffer->length = uVar1;
  buffer->data[uVar1] = '\0';
  return;
}

/*
 * _http_buf_free @ 0x000166e8..0x00016720
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Manage the HTTP response accumulation buffer: free. Detailed semantics remain in the recovered body and call/reference evidence.
 * Parameters/return (inferred): void _http_buf_free(RecoveredHttpBuffer *buffer)
 * Direct callers: _do_async_fetch@0x000087a8, _ollama_sync_model_metadata@0x0000bb9c, _sse_buf_free@0x00016770, _sse2_adapter_cb@0x000167f8, _http_post_sse2@0x000169cc
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000166e8_http_buf_free.s
 */
void http_buf_free(RecoveredHttpBuffer *buffer)

{
  free(buffer->data);
  http_buf_init(buffer);
  return;
}

/*
 * _sse_buf_init @ 0x00016720..0x00016770
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in http: sse buf init. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _sse_buf_init(undefined4 *param_1)
 * Direct callers: _sse_buf_free@0x00016770
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00016720_sse_buf_init.s
 */
void sse_buf_init(undefined4 *param_1)

{
  http_buf_init((RecoveredHttpBuffer *)(param_1 + 5));
  http_buf_init((RecoveredHttpBuffer *)(param_1 + 8));
  *param_1 = 0;
  param_1[1] = 0;
  param_1[2] = 0;
  param_1[3] = 0;
  param_1[4] = 0;
  return;
}

/*
 * _sse_buf_free @ 0x00016770..0x000167b8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in http: sse buf free. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _sse_buf_free(undefined4 *param_1)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00016770_sse_buf_free.s
 */
void sse_buf_free(undefined4 *param_1)

{
  http_buf_free((RecoveredHttpBuffer *)(param_1 + 5));
  http_buf_free((RecoveredHttpBuffer *)(param_1 + 8));
  free((void *)*param_1);
  sse_buf_init(param_1);
  return;
}

/*
 * _write_cb @ 0x000167b8..0x000167f8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in http: write cb. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): uint _write_cb(char *data,uint size,uint count,RecoveredHttpBuffer *buffer)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000167b8_write_cb.s
 */
uint write_cb(char *data,uint size,uint count,RecoveredHttpBuffer *buffer)

{
  http_buf_append(buffer,data,size * count);
  return size * count;
}

/*
 * _sse2_adapter_cb @ 0x000167f8..0x000169cc
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Accumulate curl chunks, split server-sent-event lines and invoke the supplied callback.
 * Parameters/return (inferred): uint _sse2_adapter_cb(char *data,uint size,uint count,RecoveredSSEAdapter *context)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000167f8_sse2_adapter_cb.s
 * Uncertainty: Indirect dispatch sites are retained with raw assembly and references; possible target sets are not assumed complete.
 */
uint sse2_adapter_cb(char *data,uint size,uint count,RecoveredSSEAdapter *context)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x60] __attribute__((aligned(16)));
#define local_48 (*((RecoveredHttpBuffer (*)[3])(app_frame + 0x8))) /* original SP - 0x48 */

  int iVar1;
  char *pcVar2;
  uint length;
  uint length_00;
  char *pcVar3;

  length = size * count;
  http_buf_append(&context->raw_response,data,length);
  http_buf_init(local_48);
  pcVar2 = data + length;
  for (pcVar3 = data; pcVar3 < pcVar2; pcVar3 = pcVar3 + 1) {
    if (*pcVar3 == '\n') {
      length_00 = (int)pcVar3 - (int)data;
      if ((length_00 == 0) ||
         ((data[length_00 - 1] == '\r' && (length_00 = length_00 - 1, length_00 == 0)))) {
        if (local_48[0].length != 0) {
          local_48[0].data[local_48[0].length - (local_48[0].data[local_48[0].length - 1] == '\n')]
               = '\0';
          (*context->callback)((char *)0x0,local_48[0].data,context->context);
        }
        local_48[0].length = 0;
        if (local_48[0].data != (char *)0x0) {
          *local_48[0].data = '\0';
        }
      }
      else if ((length_00 < 7) || (iVar1 = strncmp(data,"data: ",6), iVar1 != 0)) {
        if (*data == '{') {
          http_buf_append(local_48,data,length_00);
          http_buf_append(local_48,"\n",1);
          if (local_48[0].length != 0) {
            local_48[0].data[local_48[0].length - 1] = '\0';
            (*context->callback)((char *)0x0,local_48[0].data,context->context);
            local_48[0].length = 0;
            if (local_48[0].data != (char *)0x0) {
              *local_48[0].data = '\0';
            }
          }
        }
      }
      else {
        http_buf_append(local_48,data + 6,length_00 - 6);
        http_buf_append(local_48,"\n",1);
      }
      data = pcVar3 + 1;
    }
  }
  http_buf_free(local_48);
  return length;

#undef local_48
}

/*
 * _http_post_sse2 @ 0x000169cc..0x00016bd0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Create an authenticated streaming JSON POST, disable TLS checks, invoke the stream adapter and return HTTP status/error text.
 * Parameters/return (inferred): int _http_post_sse2(char *url,char *body,char *auth_header,RecoveredStreamCallback callback, void *callback_data,char *error_text,int error_size)
 * Direct callers: _http_post_sse@0x00016bd0, _http_post_with_retry2@0x00016bf8
 * Possible side effects (direct-call closure, incomplete for indirect calls): performs network/protocol I/O
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000169cc_http_post_sse2.s
 * Preserved behavior/security: TLS-01: explicitly disables peer and hostname certificate verification.
 */
int http_post_sse2(char *url,char *body,char *auth_header,RecoveredStreamCallback callback,
                   void *callback_data,char *error_text,int error_size)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x70] __attribute__((aligned(16)));
#define local_58 (*((RecoveredStreamCallback *)(app_frame + 0x8))) /* original SP - 0x58 */
#define local_54 (*((void * *)(app_frame + 0xc))) /* original SP - 0x54 */
#define local_50 (*((RecoveredHttpBuffer *)(app_frame + 0x10))) /* original SP - 0x50 */
#define local_44 (*((int (*)[7])(app_frame + 0x1c))) /* original SP - 0x44 */

  void *handle;
  void *pvVar1;
  int iVar2;
  char *text;

  ensure_curl_init();
  handle = curl_easy_init();
  if (handle != (void *)0x0) {
    pvVar1 = curl_slist_append((void *)0x0,"Content-Type: application/json");
    pvVar1 = curl_slist_append(pvVar1,"Accept: text/event-stream");
    if ((auth_header != (char *)0x0) && (*auth_header != '\0')) {
      pvVar1 = curl_slist_append(pvVar1,auth_header);
    }
    local_50.data = (char *)0x0;
    local_50.length = 0;
    local_50.capacity = 0;
    local_58 = callback;
    local_54 = callback_data;
    http_buf_init(&local_50);
    curl_easy_setopt(handle,0x2712,url);
    curl_easy_setopt(handle,0x2f,1);
    curl_easy_setopt(handle,0x2727,pvVar1);
    curl_easy_setopt(handle,0x271f,body);
    curl_easy_setopt(handle,0x4e2b,sse2_adapter_cb);
    curl_easy_setopt(handle,0x2711,&local_58);
    curl_easy_setopt(handle,0x40,0);
    curl_easy_setopt(handle,0x51,0);
    curl_easy_setopt(handle,0x14,0xb4);
    curl_easy_setopt(handle,0x13,1);
    curl_easy_setopt(handle,0x4e,0x1e);
    iVar2 = curl_easy_perform(handle);
    local_44[0] = 0;
    curl_easy_getinfo(handle,0x200002,local_44);
    if (((error_text != (char *)0x0) && (0 < error_size)) && (399 < local_44[0])) {
      text = local_50.data;
      if (local_50.data == (char *)0x0) {
        text = "";
      }
      strncpy(error_text,text,error_size - 1);
      error_text[error_size + -1] = '\0';
    }
    curl_slist_free_all(pvVar1);
    curl_easy_cleanup(handle);
    http_buf_free(&local_50);
    if (iVar2 == 0) {
      return local_44[0];
    }
  }
  return -1;

#undef local_58
#undef local_54
#undef local_50
#undef local_44
}

/*
 * _http_post_sse @ 0x00016bd0..0x00016bf8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Adapt the simpler streaming POST interface to _http_post_sse2.
 * Parameters/return (inferred): int _http_post_sse(char *url,char *body,char *auth_header,RecoveredStreamCallback callback, void *callback_data)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): performs network/protocol I/O
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00016bd0_http_post_sse.s
 */
int http_post_sse(char *url,char *body,char *auth_header,RecoveredStreamCallback callback,
                  void *callback_data)

{
  int iVar1;
  
  iVar1 = http_post_sse2(url,body,auth_header,callback,callback_data,(char *)0x0,0);
  return iVar1;
}

/*
 * _http_post_with_retry2 @ 0x00016bf8..0x00016d1c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Retry selected transient HTTP/network statuses with fixed delays and report final status.
 * Parameters/return (inferred): int _http_post_with_retry2 (char *url,char *body,char *auth_header,RecoveredStreamCallback callback, void *callback_data,char *error_text,int error_size,int *http_status)
 * Direct callers: _ollama_send@0x0000c70c, _anthropic_send@0x00010540, _http_post_with_retry@0x00016d1c
 * Possible side effects (direct-call closure, incomplete for indirect calls): performs network/protocol I/O
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00016bf8_http_post_with_retry2.s
 */
int http_post_with_retry2
              (char *url,char *body,char *auth_header,RecoveredStreamCallback callback,
              void *callback_data,char *error_text,int error_size,int *http_status)

{
  bool bVar1;
  int iVar2;
  int *piVar3;
  int iVar4;
  byte bVar5;
  
  iVar4 = 0;
  piVar3 = &_delays_ms_3185;
  bVar5 = (http_status == (int *)0x0) << 1;
  do {
    iVar2 = http_post_sse2(url,body,auth_header,callback,callback_data,error_text,error_size);
    if (!(bool)(bVar5 >> 1 & 1)) {
      *http_status = iVar2;
    }
    if (iVar2 - 200U < 100) goto LAB_00016cf4;
    if (((((iVar2 != 0x1ad) && (iVar2 != 0x1f6)) && (iVar2 != 0x1f7)) &&
        ((iVar2 != 0x1f8 && (iVar2 != 0x20c)))) && (-1 < iVar2)) {
      return iVar2;
    }
    if (iVar4 < 3) {
      usleep(*piVar3 * 1000);
    }
    piVar3 = piVar3 + 1;
    bVar1 = iVar4 != 3;
    iVar4 = iVar4 + 1;
  } while (bVar1);
  if ((http_status == (int *)0x0) || (*http_status < 200)) {
    iVar4 = -1;
  }
  else {
LAB_00016cf4:
    iVar4 = 0;
  }
  return iVar4;
}

/*
 * _http_post_with_retry @ 0x00016d1c..0x00016d48
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Adapt the retry interface without an error-text buffer.
 * Parameters/return (inferred): int _http_post_with_retry (char *url,char *body,char *auth_header,RecoveredStreamCallback callback, void *callback_data,int *http_status)
 * Direct callers: _openai_send@0x0000e5ec, _google_send@0x00012238
 * Possible side effects (direct-call closure, incomplete for indirect calls): performs network/protocol I/O
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00016d1c_http_post_with_retry.s
 */
int http_post_with_retry
              (char *url,char *body,char *auth_header,RecoveredStreamCallback callback,
              void *callback_data,int *http_status)

{
  int iVar1;
  
  iVar1 = http_post_with_retry2
                    (url,body,auth_header,callback,callback_data,(char *)0x0,0,http_status);
  return iVar1;
}

/*
 * _http_post_json @ 0x00016d48..0x00016eb8
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Send a JSON POST into a response buffer; peer and hostname verification are disabled.
 * Parameters/return (inferred): int _http_post_json(char *url,char *body,char *auth_header,RecoveredHttpBuffer *buffer)
 * Direct callers: _do_async_fetch@0x000087a8, _ollama_sync_model_metadata@0x0000bb9c
 * Possible side effects (direct-call closure, incomplete for indirect calls): performs network/protocol I/O
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00016d48_http_post_json.s
 * Preserved behavior/security: TLS-01: explicitly disables peer and hostname certificate verification.
 */
int http_post_json(char *url,char *body,char *auth_header,RecoveredHttpBuffer *buffer)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x50] __attribute__((aligned(16)));
#define local_38 (*((int (*)[7])(app_frame + 0x8))) /* original SP - 0x38 */

  void *handle;
  void *list;
  int iVar1;

  ensure_curl_init();
  handle = curl_easy_init();
  if (handle != (void *)0x0) {
    list = curl_slist_append((void *)0x0,"Content-Type: application/json");
    if ((auth_header != (char *)0x0) && (*auth_header != '\0')) {
      list = curl_slist_append(list,auth_header);
    }
    curl_easy_setopt(handle,0x2712,url);
    curl_easy_setopt(handle,0x2f,1);
    curl_easy_setopt(handle,0x2727,list);
    curl_easy_setopt(handle,0x271f,body);
    curl_easy_setopt(handle,0x4e2b,write_cb);
    curl_easy_setopt(handle,0x2711,buffer);
    curl_easy_setopt(handle,0x40,0);
    curl_easy_setopt(handle,0x51,0);
    curl_easy_setopt(handle,0xd,0x78);
    curl_easy_setopt(handle,0x4e,0x1e);
    iVar1 = curl_easy_perform(handle);
    local_38[0] = 0;
    curl_easy_getinfo(handle,0x200002,local_38);
    curl_slist_free_all(list);
    curl_easy_cleanup(handle);
    if (iVar1 == 0) {
      return local_38[0];
    }
  }
  return -1;

#undef local_38
}

/*
 * _http_get @ 0x00016eb8..0x00016fec
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Perform a GET with optional authentication; peer and hostname verification are disabled.
 * Parameters/return (inferred): int _http_get(char *url,char *auth_header,RecoveredHttpBuffer *buffer)
 * Direct callers: _do_async_fetch@0x000087a8
 * Possible side effects (direct-call closure, incomplete for indirect calls): performs network/protocol I/O
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00016eb8_http_get.s
 * Preserved behavior/security: TLS-01: explicitly disables peer and hostname certificate verification.
 */
int http_get(char *url,char *auth_header,RecoveredHttpBuffer *buffer)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x50] __attribute__((aligned(16)));
#define local_38 (*((int (*)[8])(app_frame + 0x8))) /* original SP - 0x38 */

  void *handle;
  void *list;
  int iVar1;

  ensure_curl_init();
  handle = curl_easy_init();
  if (handle != (void *)0x0) {
    if ((auth_header == (char *)0x0) || (*auth_header == '\0')) {
      list = (void *)0x0;
    }
    else {
      list = curl_slist_append((void *)0x0,auth_header);
    }
    curl_easy_setopt(handle,0x2712,url);
    curl_easy_setopt(handle,0x2727,list);
    curl_easy_setopt(handle,0x4e2b,write_cb);
    curl_easy_setopt(handle,0x2711,buffer);
    curl_easy_setopt(handle,0x40,0);
    curl_easy_setopt(handle,0x51,0);
    curl_easy_setopt(handle,0xd,0x1e);
    iVar1 = curl_easy_perform(handle);
    local_38[0] = 0;
    curl_easy_getinfo(handle,0x200002,local_38);
    curl_slist_free_all(list);
    curl_easy_cleanup(handle);
    if (iVar1 == 0) {
      return local_38[0];
    }
  }
  return -1;

#undef local_38
}

