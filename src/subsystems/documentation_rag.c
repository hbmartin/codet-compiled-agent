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
 * _get_home_dir @ 0x000279f4..0x00027a68
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Copy nonempty HOME, otherwise /tmp, into the caller's buffer.
 * Parameters/return (inferred): void _get_home_dir(char *param_1,int param_2)
 * Direct callers: _ensure_ppccode_dir@0x00027a68, _rag_init@0x00027ac8
 * Possible side effects (direct-call closure, incomplete for indirect calls): reads process environment
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000279f4_get_home_dir.s
 */
void get_home_dir(char *param_1,int param_2)

{
  char *text;
  
  text = getenv("HOME");
  if ((text == (char *)0x0) || (*text == '\0')) {
    text = "/tmp";
  }
  strncpy(param_1,text,param_2 - 1);
  param_1[param_2 + -1] = '\0';
  return;
}

/*
 * _ensure_ppccode_dir @ 0x00027a68..0x00027ac8
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in documentation_rag: ensure ppccode dir. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _ensure_ppccode_dir(void)
 * Direct callers: _rag_init@0x00027ac8
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; reads process environment; creates directories
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00027a68_ensure_ppccode_dir.s
 */
void ensure_ppccode_dir(void)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x830] __attribute__((aligned(16)));
#define auStack_818 (*((undefined1 (*)[1024])(app_frame + 0x8))) /* original SP - 0x818 */
#define acStack_418 (*((char (*)[1036])(app_frame + 0x408))) /* original SP - 0x418 */

  get_home_dir((char *)(auStack_818),0x400);
  app_format_truncated(acStack_418,0x400,"%s/.ppccode",auStack_818);
  mkdir(acStack_418,0x1ed);
  return;

#undef auStack_818
#undef acStack_418
}

/*
 * _rag_init @ 0x00027ac8..0x00027bf4
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Resolve/copy the documentation DB path, open SQLite and create rag_chunks/rag_meta if absent.
 * Parameters/return (inferred): int _rag_init(char *path_in_out,int path_capacity)
 * Direct callers: _rag_insert@0x00027bf4, _rag_search@0x000281ec, _rag_stats@0x000288f0
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; reads process environment; creates directories; executes SQL text; opens/creates a SQLite database
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00027ac8_rag_init.s
 */
int rag_init(char *path_in_out,int path_capacity)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x440] __attribute__((aligned(16)));
#define local_428 (*((sqlite3 * *)(app_frame + 0x8))) /* original SP - 0x428 */
#define auStack_424 (*((undefined1 (*)[1044])(app_frame + 0xc))) /* original SP - 0x424 */

  int iVar1;


  if ((path_in_out == (char *)0x0) || (*path_in_out == '\0')) {
    ensure_ppccode_dir();
    get_home_dir((char *)(auStack_424),0x400);
    app_format_truncated(&_g_rag_db_path,0x400,"%s/.ppccode/tiger_docs.db",auStack_424);
  }
  else {
    strncpy(&_g_rag_db_path,path_in_out,0x3ff);
    DAT_000395e3 = 0;
  }
  if (path_capacity - 1U < 0x3ff) {
    strncpy(path_in_out,&_g_rag_db_path,path_capacity - 1U);
    path_in_out[path_capacity + -1] = '\0';
  }
  iVar1 = sqlite3_open(&_g_rag_db_path,&local_428);
  if (iVar1 == 0) {
    sqlite3_exec(local_428,
                  "CREATE TABLE IF NOT EXISTS rag_chunks (id INTEGER PRIMARY KEY AUTOINCREMENT,source TEXT,title TEXT,chunk_text TEXT,embedding TEXT);"
                  ,NULL,(void *)0x0,(char **)0x0);
    sqlite3_exec(local_428,"CREATE TABLE IF NOT EXISTS rag_meta (key TEXT PRIMARY KEY,value TEXT);"
                  ,NULL,(void *)0x0,(char **)0x0);
    sqlite3_close(local_428);
    iVar1 = 0;
  }
  else {
    sqlite3_close(local_428);
    iVar1 = -1;
  }
  return iVar1;

#undef local_428
#undef auStack_424
}

/*
 * _rag_insert @ 0x00027bf4..0x00027d34
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Insert a documentation record and optional embedding into SQLite.
 * Parameters/return (inferred): int _rag_insert(char *param_1,char *param_2,char *param_3,char *param_4)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; reads process environment; creates directories; executes SQL text; opens/creates a SQLite database; prepares SQL text
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00027bf4_rag_insert.s
 */
int rag_insert(char *param_1,char *param_2,char *param_3,char *param_4)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x50] __attribute__((aligned(16)));
#define local_38 (*((void * *)(app_frame + 0x8))) /* original SP - 0x38 */
#define local_34 (*((void * (*)[8])(app_frame + 0xc))) /* original SP - 0x34 */

  int iVar1;


  if (_g_rag_db_path == '\0') {
    rag_init((char *)0x0,0);
  }
  iVar1 = sqlite3_open(&_g_rag_db_path,(sqlite3 **)(&local_38));
  if (iVar1 == 0) {
    iVar1 = sqlite3_prepare(local_38,
                             "INSERT INTO rag_chunks (source, title, chunk_text, embedding) VALUES (?, ?, ?, ?);"
                             ,-1,(sqlite3_stmt **)(local_34),(void *)0x0);
    if (iVar1 == 0) {
      sqlite3_bind_text(local_34[0],1,param_1,-1,(void *)0xffffffff);
      sqlite3_bind_text(local_34[0],2,param_2,-1,(void *)0xffffffff);
      sqlite3_bind_text(local_34[0],3,param_3,-1,(void *)0xffffffff);
      sqlite3_bind_text(local_34[0],4,param_4,-1,(void *)0xffffffff);
      iVar1 = sqlite3_step(local_34[0]);
      sqlite3_finalize(local_34[0]);
      sqlite3_close(local_38);
      iVar1 = -(uint)(iVar1 != 0x65);
    }
    else {
      sqlite3_close(local_38);
      iVar1 = -1;
    }
  }
  else {
    sqlite3_close(local_38);
    iVar1 = -1;
  }
  return iVar1;

#undef local_38
#undef local_34
}

/*
 * _parse_embedding_csv @ 0x00027d34..0x00027dfc
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Decode comma-separated embedding values up to the provided dimension limit.
 * Parameters/return (inferred): int _parse_embedding_csv(char *text,float *output,int capacity)
 * Direct callers: _rag_search@0x000281ec
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/00027d34_parse_embedding_csv.s
 */
int parse_embedding_csv(char *text,float *output,int capacity)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x50] __attribute__((aligned(16)));
#define local_38 (*((char * (*)[9])(app_frame + 0x8))) /* original SP - 0x38 */

  char cVar1;
  int iVar2;
  double dVar3;

  iVar2 = 0;
  if (0 < capacity) {
    do {
      for (; (cVar1 = *text, cVar1 == ' ' || (cVar1 == '\t')); text = text + 1) {
      }
      if (cVar1 == '\0') {
        return iVar2;
      }
      dVar3 = strtod(text,local_38);
      output[iVar2] = (float)dVar3;
      if (text == local_38[0]) {
        return iVar2;
      }
      iVar2 = iVar2 + 1;
      text = local_38[0];
      if (*local_38[0] == ',') {
        text = local_38[0] + 1;
      }
      for (; (*text == ' ' || (*text == '\t')); text = text + 1) {
      }
    } while (capacity != iVar2);
  }
  return iVar2;

#undef local_38
}

/*
 * _parse_embedding_json @ 0x00027dfc..0x00027ee0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Extract embedding dimensions from JSON into the caller's float buffer.
 * Parameters/return (inferred): int _parse_embedding_json(char *text,float *output,int capacity)
 * Direct callers: _fetch_embedding@0x00027fa0
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/00027dfc_parse_embedding_json.s
 */
int parse_embedding_json(char *text,float *output,int capacity)

{
  RecoveredCJSON *object;
  RecoveredCJSON *array;
  int iVar1;
  RecoveredCJSON *pRVar2;
  int index;
  int iVar3;
  
  object = cJSON_Parse(text);
  iVar3 = -1;
  if (object != (RecoveredCJSON *)0x0) {
    array = cJSON_GetObjectItem(object,"embedding");
    if ((((array == (RecoveredCJSON *)0x0) &&
         (array = cJSON_GetObjectItem(object,"embeddings"), array == (RecoveredCJSON *)0x0)) ||
        (iVar1 = cJSON_GetArraySize(array), iVar1 < 1)) || (capacity < 1)) {
      iVar3 = 0;
    }
    else {
      iVar3 = 0;
      index = 0;
      do {
        pRVar2 = cJSON_GetArrayItem(array,index);
        if (pRVar2 != (RecoveredCJSON *)0x0) {
          output[iVar3] = (float)pRVar2->value_double;
          iVar3 = iVar3 + 1;
        }
        index = index + 1;
      } while ((iVar1 != index) && (iVar3 < capacity));
    }
    cJSON_Delete(object);
  }
  return iVar3;
}

/*
 * _cosine_similarity @ 0x00027ee0..0x00027fa0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Compute a float-vector dot product divided by vector magnitudes, rejecting near-zero magnitudes.
 * Parameters/return (inferred): float _cosine_similarity(float *left,float *right,int dimensions)
 * Direct callers: _rag_search@0x000281ec
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/00027ee0_cosine_similarity.s
 */
float cosine_similarity(float *left,float *right,int dimensions)

{
  int iVar1;
  double dVar2;
  double dVar3;
  double dVar4;
  double dVar5;
  double dVar6;
  
  if (0 < dimensions) {
    dVar4 = 0.0;
    iVar1 = 0;
    dVar5 = dVar4;
    dVar6 = dVar4;
    do {
      dVar3 = (double)*(float *)((int)left + iVar1);
      dVar2 = (double)*(float *)((int)right + iVar1);
      dVar5 = dVar3 * dVar2 + dVar5;
      dVar4 = dVar3 * dVar3 + dVar4;
      dVar6 = dVar2 * dVar2 + dVar6;
      iVar1 = iVar1 + 4;
      dimensions = dimensions + -1;
    } while (dimensions != 0);
    if ((1e-12 <= dVar4) && (1e-12 <= dVar6)) {
      dVar4 = sqrt(dVar4);
      dVar6 = sqrt(dVar6);
      return (float)(dVar5 / (dVar4 * dVar6));
    }
  }
  return 0.0;
}

/*
 * _fetch_embedding @ 0x00027fa0..0x000281ec
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): JSON-escape query text, interpolate it into a shell curl command and parse the returned embedding JSON.
 * Parameters/return (inferred): int _fetch_embedding(char *query,char *host,float *output,int maximum_dimensions)
 * Direct callers: _rag_search@0x000281ec
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; reads bytes from a stream; reads process environment; executes a shell command and opens a pipe
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/00027fa0_fetch_embedding.s
 * Preserved behavior/security: SHELL-01: JSON escaping leaves shell apostrophes unsafe; query can acquire shell syntax.
 */
int fetch_embedding(char *query,char *host,float *output,int maximum_dimensions)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x3070] __attribute__((aligned(16)));
#define acStack_3058 (*((char (*)[8192])(app_frame + 0x8))) /* original SP - 0x3058 */
#define RStack_1058 (*((RecoveredDString *)(app_frame + 0x2008))) /* original SP - 0x1058 */
#define RStack_104c (*((RecoveredDString *)(app_frame + 0x2014))) /* original SP - 0x104c */
#define acStack_1040 (*((char (*)[4096])(app_frame + 0x2020))) /* original SP - 0x1040 */
#define local_40 (*((char *)(app_frame + 0x3020))) /* original SP - 0x40 */
#define local_3f (*((undefined1 *)(app_frame + 0x3021))) /* original SP - 0x3f */

  char cVar1;
  char cVar2;
  char *pcVar3;
  char *pcVar4;
  void *stream;
  uint length;
  int iVar5;

  if (((host == (char *)0x0) || (*host == '\0')) &&
     (host = getenv("OLLAMA_HOST"), host == (char *)0x0)) {
    host = "http://localhost:11434";
  }
  pcVar3 = getenv("RAG_EMBEDDING_MODEL");
  if (pcVar3 == (char *)0x0) {
    pcVar3 = "nomic-embed-text";
  }
  else if (*pcVar3 == '\0') {
    pcVar3 = "nomic-embed-text";
  }
  ds_init(&RStack_1058);
  cVar2 = *query;
  cVar1 = local_40;
  while (local_40 = cVar2, local_40 != '\0') {
    if (local_40 == '\\') {
      local_40 = cVar1;
      ds_append(&RStack_1058,"\\\\");
    }
    else if (local_40 == '\"') {
      local_40 = cVar1;
      ds_append(&RStack_1058,"\\\"");
    }
    else if (local_40 == '\n') {
      local_40 = cVar1;
      ds_append(&RStack_1058,"\\n");
    }
    else if (local_40 == '\r') {
      local_40 = cVar1;
      ds_append(&RStack_1058,"\\r");
    }
    else if (local_40 == '\t') {
      local_40 = cVar1;
      ds_append(&RStack_1058,"\\t");
    }
    else {
      local_3f = 0;
      ds_append(&RStack_1058,&local_40);
    }
    query = query + 1;
    cVar1 = local_40;
    cVar2 = *query;
  }
  local_40 = cVar1;
  pcVar4 = ds_cstr(&RStack_1058);
  snprintf(acStack_3058,0x2000,
                     "curl -s -X POST \'%s/api/embeddings\' -d \'{\"model\":\"%s\",\"prompt\":\"%s\"}\' --connect-timeout 10 --max-time 30 2>/dev/null"
                     ,host,pcVar3,pcVar4);
  ds_free(&RStack_1058);
  stream = popen(acStack_3058,"r");
  if (stream != (void *)0x0) {
    ds_init(&RStack_104c);
    while (length = fread(acStack_1040,1,0x1000,stream), length != 0) {
      ds_appendn(&RStack_104c,acStack_1040,length);
    }
    iVar5 = pclose(stream);
    if ((iVar5 != 0) || (RStack_104c.length == 0)) {
      ds_free(&RStack_104c);
      return -1;
    }
    pcVar3 = ds_cstr(&RStack_104c);
    iVar5 = parse_embedding_json(pcVar3,output,maximum_dimensions);
    ds_free(&RStack_104c);
    if (0 < iVar5) {
      return iVar5;
    }
  }
  return -1;

#undef acStack_3058
#undef RStack_1058
#undef RStack_104c
#undef acStack_1040
#undef local_40
#undef local_3f
}

/*
 * _rag_search @ 0x000281ec..0x000288f0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Try query embeddings, fall back to bound substring SQL on failure, and format up to five documentation results.
 * Parameters/return (inferred): int _rag_search(char *query,char *output,int output_size,char *host)
 * Direct callers: _tool_execute@0x000158c8, _handle_cmd@0x0001b004
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; reads bytes from a stream; reads process environment; creates directories; executes a shell command and opens a pipe; executes SQL text; opens/creates a SQLite database; prepares SQL text
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/000281ec_rag_search.s
 */
int rag_search(char *query,char *output,int output_size,char *host)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0xa4620] __attribute__((aligned(16)));
#define pvStack_a4608 (*((void * *)(app_frame + 0x8))) /* original SP - 0xa4608 */
#define pvStack_a4604 (*((void * *)(app_frame + 0xc))) /* original SP - 0xa4604 */
#define afStack_a4600 (*((float (*)[4096])(app_frame + 0x10))) /* original SP - 0xa4600 */
#define RStack_a0600 (*((RecoveredDString *)(app_frame + 0x4010))) /* original SP - 0xa0600 */
#define iStack_a05f4 (*((int *)(app_frame + 0x401c))) /* original SP - 0xa05f4 */
#define acStack_a05f0 (*((char (*)[255])(app_frame + 0x4020))) /* original SP - 0xa05f0 */
#define acStack_a04f1 (*((char (*)[12545])(app_frame + 0x411f))) /* original SP - 0xa04f1 */
#define afStack_9d3f0 (*((float (*)[156899])(app_frame + 0x7220))) /* original SP - 0x9d3f0 */
#define aRStack_4064 (*((RecoveredDString (*)[1367])(app_frame + 0xa05ac))) /* original SP - 0x4064 */
#define local_50 (*((undefined8 *)(app_frame + 0xa45c0))) /* original SP - 0x50 */

  void *statement;
  int iVar3;
  int iVar4;
  char *pcVar5;
  char *pcVar6;
  char *text;
  uint uVar7;
  int iVar8;
  int *input;
  int *input_00;
  float *pfVar9;
  double dVar10;
  float fVar11;

  if ((output == (char *)0x0) || (output_size < 1)) {
    iVar3 = -1;
  }
  else {
    if (_g_rag_db_path == '\0') {
      rag_init((char *)0x0,0);
    }
    iVar3 = sqlite3_open(&_g_rag_db_path,(sqlite3 **)(&pvStack_a4608));
    if (iVar3 == 0) {
      iVar3 = sqlite3_prepare(pvStack_a4608,"SELECT COUNT(*) FROM rag_chunks",-1,(sqlite3_stmt **)(&pvStack_a4604),
                               (void *)0x0);
      if ((iVar3 == 0) && (iVar3 = sqlite3_step(pvStack_a4604), iVar3 == 100)) {
        iVar3 = sqlite3_column_int(pvStack_a4604,0);
      }
      else {
        iVar3 = 0;
      }
      sqlite3_finalize(pvStack_a4604);
      if (iVar3 == 0) {
        snprintf(output,output_size,
                           "RAG database is empty. No Tiger documentation indexed.\nUse the ingest script to populate it from Tiger doc files."
                          );
        sqlite3_close(pvStack_a4608);
        iVar3 = 0;
      }
      else {
        iVar4 = fetch_embedding(query,host,afStack_a4600,0x1000);
        ds_init(&RStack_a0600);
        if (iVar4 < 1) {
          ds_appendf(&RStack_a0600,
                      "RAG database has %d docs. Embedding search unavailable.\nUsing substring match for query: %s\n\n"
                      ,iVar3,query);
          iVar3 = sqlite3_prepare(pvStack_a4608,
                                   "SELECT id, source, title, chunk_text FROM rag_chunks WHERE chunk_text LIKE ?1 OR title LIKE ?1 ORDER BY id LIMIT ?2"
                                   ,-1,(sqlite3_stmt **)(&pvStack_a4604),(void *)0x0);
          if (iVar3 == 0) {
            ds_init(aRStack_4064);
            ds_appendf(aRStack_4064,"%%%s%%",query);
            statement = pvStack_a4604;
            pcVar5 = ds_cstr(aRStack_4064);
            sqlite3_bind_text(statement,1,pcVar5,-1,(void *)0xffffffff);
            sqlite3_bind_int(pvStack_a4604,2,5);
            ds_free(aRStack_4064);
            while (iVar3 = sqlite3_step(pvStack_a4604), iVar3 == 100) {
              iVar3 = sqlite3_column_int(pvStack_a4604,0);
              pcVar5 = (char *)(sqlite3_column_text(pvStack_a4604,1));
              pcVar6 = (char *)(sqlite3_column_text(pvStack_a4604,2));
              text = (char *)(sqlite3_column_text(pvStack_a4604,3));
              ds_appendf(&RStack_a0600,"---\nDoc #%d\n",iVar3);
              if ((pcVar5 != (char *)0x0) && (*pcVar5 != '\0')) {
                ds_appendf(&RStack_a0600,"Source: %s\n",pcVar5);
              }
              if ((pcVar6 != (char *)0x0) && (*pcVar6 != '\0')) {
                ds_appendf(&RStack_a0600,"Title: %s\n",pcVar6);
              }
              if ((text != (char *)0x0) && (*text != '\0')) {
                uVar7 = strlen(text);
                if ((int)uVar7 < 0x7d1) {
                  pcVar5 = "";
                }
                else {
                  uVar7 = 2000;
                  pcVar5 = "...";
                }
                ds_appendf(&RStack_a0600,"Text: %.*s%s\n\n",uVar7,text,pcVar5);
              }
            }
            sqlite3_finalize(pvStack_a4604);
          }
          else {
            pcVar5 = (char *)(sqlite3_errmsg(pvStack_a4608));
            ds_appendf(&RStack_a0600,"DB error: %s\n",pcVar5);
          }
        }
        else {
          ds_appendf(&RStack_a0600,"RAG search results for: %s\n\n",query);
          iVar3 = sqlite3_prepare(pvStack_a4608,
                                   "SELECT id, source, title, chunk_text, embedding FROM rag_chunks"
                                   ,-1,(sqlite3_stmt **)(&pvStack_a4604),(void *)0x0);
          iVar8 = 0;
          if (iVar3 == 0) {
            while ((iVar3 = sqlite3_step(pvStack_a4604), iVar3 == 100 && (iVar8 < 0x32))) {
              pcVar5 = (char *)(sqlite3_column_text(pvStack_a4604,4));
              if ((pcVar5 != (char *)0x0) && (*pcVar5 != '\0')) {
                iVar3 = parse_embedding_csv(pcVar5,(float *)aRStack_4064,0x1000);
                if (iVar4 == iVar3) {
                  fVar11 = cosine_similarity(afStack_a4600,(float *)aRStack_4064,iVar4);
                  iVar3 = sqlite3_column_int(pvStack_a4604,0);
                  *(int *)(acStack_a05f0 + iVar8 * 0x3208 + -4) = iVar3;
                  pcVar5 = (char *)(sqlite3_column_text(pvStack_a4604,1));
                  if (pcVar5 == (char *)0x0) {
                    pcVar5 = "";
                  }
                  strncpy(acStack_a05f0 + iVar8 * 0x3208,pcVar5,0xff);
                  acStack_a04f1[iVar8 * 0x3208] = '\0';
                  pcVar5 = (char *)(sqlite3_column_text(pvStack_a4604,2));
                  if (pcVar5 == (char *)0x0) {
                    pcVar5 = "";
                  }
                  strncpy(acStack_a04f1 + iVar8 * 0x3208 + 1,pcVar5,0xff);
                  acStack_a04f1[iVar8 * 0x3208 + 0x100] = '\0';
                  pcVar5 = (char *)(sqlite3_column_text(pvStack_a4604,3));
                  if (pcVar5 == (char *)0x0) {
                    pcVar5 = "";
                  }
                  strncpy(acStack_a04f1 + iVar8 * 0x3208 + 0x101,pcVar5,0x1fff);
                  acStack_a04f1[(int)(0x2100 + iVar8 * 0x3208)] = '\0';
                  afStack_9d3f0[iVar8 * 0xc82] = fVar11;
                  iVar8 = iVar8 + 1;
                }
              }
            }
            sqlite3_finalize(pvStack_a4604);
            iVar3 = 0;
            input_00 = &iStack_a05f4;
            while (iVar3 < iVar8 + -1) {
              iVar3 = iVar3 + 1;
              if (iVar3 < iVar8) {
                input = &iStack_a05f4 + iVar3 * 0xc82;
                iVar4 = 0;
                do {
                  if ((float)input_00[0xc81] < (float)input[0xc81]) {
                    memcpy(aRStack_4064,input_00,0x3208);
                    memcpy(input_00,input,0x3208);
                    memcpy(input,aRStack_4064,0x3208);
                  }
                  iVar4 = iVar4 + 1;
                  input = input + 0xc82;
                } while (iVar4 != iVar8 - iVar3);
              }
              input_00 = input_00 + 0xc82;
            }
            if (5 < iVar8) {
              iVar8 = 5;
            }
            if (0 < iVar8) {
              pfVar9 = afStack_9d3f0;
              iVar3 = 0;
              do {
                iVar4 = iVar3 + 1;
                dVar10 = (double)*pfVar9;
                APP_FIELD32(local_50, 0) = (undefined4)(APP_BITS(dVar10) >> 0x20);

                APP_FIELD32(local_50, 4) = SUB84(dVar10,0);

                local_50 = dVar10;
                ds_appendf(&RStack_a0600,"---\nResult #%d (score: %.3f)\n",iVar4,dVar10);
                if (*(char *)(pfVar9 + -0xc80) != '\0') {
                  ds_appendf(&RStack_a0600,"Source: %s\n",acStack_a05f0 + iVar3 * 0x3208);
                }
                if (*(char *)(pfVar9 + -0xc40) != '\0') {
                  ds_appendf(&RStack_a0600,"Title: %s\n",acStack_a04f1 + iVar3 * 0x3208 + 1);
                }
                uVar7 = strlen(acStack_a04f1 + iVar3 * 0x3208 + 0x101);
                if ((int)uVar7 < 0x9c5) {
                  pcVar5 = "";
                }
                else {
                  uVar7 = 0x9c4;
                  pcVar5 = "...";
                }
                ds_appendf(&RStack_a0600,"Text: %.*s%s\n\n",uVar7,
                            acStack_a04f1 + iVar3 * 0x3208 + 0x101,pcVar5);
                pfVar9 = pfVar9 + 0xc82;
                iVar3 = iVar4;
              } while (iVar8 != iVar4);
            }
          }
          else {
            pcVar5 = (char *)(sqlite3_errmsg(pvStack_a4608));
            ds_appendf(&RStack_a0600,"DB error: %s\n",pcVar5);
          }
        }
        uVar7 = output_size - 1U;
        if (RStack_a0600.length < output_size - 1U) {
          uVar7 = RStack_a0600.length;
        }
        if ((uVar7 != 0) && (RStack_a0600.data != (char *)0x0)) {
          memcpy(output,RStack_a0600.data,uVar7);
        }
        output[uVar7] = '\0';
        ds_free(&RStack_a0600);
        sqlite3_close(pvStack_a4608);
        iVar3 = 0;
      }
    }
    else {
      snprintf(output,output_size,"RAG database error: cannot open %s",&_g_rag_db_path);
      sqlite3_close(pvStack_a4608);
      iVar3 = -1;
    }
  }
  return iVar3;

#undef pvStack_a4608
#undef pvStack_a4604
#undef afStack_a4600
#undef RStack_a0600
#undef iStack_a05f4
#undef acStack_a05f0
#undef acStack_a04f1
#undef afStack_9d3f0
#undef aRStack_4064
#undef local_50
}

/*
 * _rag_stats @ 0x000288f0..0x00028a68
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Format documentation database counts/metadata into the caller's output buffer.
 * Parameters/return (inferred): int _rag_stats(char *output,int output_size)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; reads process environment; creates directories; executes SQL text; opens/creates a SQLite database; prepares SQL text
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000288f0_rag_stats.s
 */
int rag_stats(char *output,int output_size)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x50] __attribute__((aligned(16)));
#define local_38 (*((void * *)(app_frame + 0x8))) /* original SP - 0x38 */
#define local_34 (*((void * (*)[8])(app_frame + 0xc))) /* original SP - 0x34 */

  int iVar1;
  int iVar2;


  if ((output == (char *)0x0) || (output_size < 1)) {
    return -1;
  }
  if (_g_rag_db_path == '\0') {
    rag_init((char *)0x0,0);
  }
  iVar1 = sqlite3_open(&_g_rag_db_path,(sqlite3 **)(&local_38));
  if (iVar1 == 0) {
    iVar1 = sqlite3_prepare(local_38,"SELECT COUNT(*) FROM rag_chunks",-1,(sqlite3_stmt **)(local_34),(void *)0x0);
    if ((iVar1 == 0) && (iVar1 = sqlite3_step(local_34[0]), iVar1 == 100)) {
      iVar1 = sqlite3_column_int(local_34[0],0);
    }
    else {
      iVar1 = 0;
    }
    sqlite3_finalize(local_34[0]);
    iVar2 = sqlite3_prepare(local_38,"SELECT COUNT(DISTINCT source) FROM rag_chunks",-1,(sqlite3_stmt **)(local_34),
                             (void *)0x0);
    if ((iVar2 == 0) && (iVar2 = sqlite3_step(local_34[0]), iVar2 == 100)) {
      iVar2 = sqlite3_column_int(local_34[0],0);
    }
    else {
      iVar2 = 0;
    }
    sqlite3_finalize(local_34[0]);
    sqlite3_close(local_38);
    snprintf(output,output_size,
                       "RAG database: %d chunks from %d source documents. Path: %s",iVar1,iVar2,
                       &_g_rag_db_path);
    return 0;
  }
  snprintf(output,output_size,"RAG database not available");
  sqlite3_close(local_38);
  return -1;

#undef local_38
#undef local_34
}

