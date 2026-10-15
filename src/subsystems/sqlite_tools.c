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
 * _db_query @ 0x00026c50..0x0002728c
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Prepare and execute a supplied SQL query and format result columns/rows.
 * Parameters/return (inferred): int _db_query(void *database,char *query,RecoveredDString *output)
 * Direct callers: _db_tool_execute@0x000277dc
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; prepares SQL text
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00026c50_db_query.s
 */
int db_query(void *database,char *query,RecoveredDString *output)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x70] __attribute__((aligned(16)));
#define local_58 (*((sqlite3_stmt * (*)[6])(app_frame + 0x8))) /* original SP - 0x58 */

  bool bVar1;
  int iVar2;
  char *pcVar3;
  uint count;
  void *pointer;
  int *pointer_00;
  undefined4 *puVar4;
  undefined4 *puVar5;
  void *pvVar6;
  int iVar7;
  uint uVar8;
  char *pcVar9;
  int iVar10;
  uint uVar11;
  int *piVar12;
  int iVar13;
  int iVar14;

  if ((query == (char *)0x0) || (*query == '\0')) {
    ds_append(output,"Error: no SQL query provided\n");
    iVar2 = -1;
  }
  else {
    ds_appendf(output,"Query: %s\n",query);
    iVar2 = sqlite3_prepare(database,query,-1,local_58,(void *)0x0);
    if (iVar2 == 0) {
      count = sqlite3_column_count(local_58[0]);
      if (count == 0) {
        ds_append(output,"0 columns in result set\n");
        sqlite3_finalize(local_58[0]);
        iVar2 = 0;
      }
      else {
        pointer = calloc(count,4);
        pointer_00 = calloc(count,4);
        if ((pointer == (void *)0x0) || (pointer_00 == (int *)0x0)) {
          ds_append(output,"Error: out of memory\n");
          free(pointer);
          free(pointer_00);
          sqlite3_finalize(local_58[0]);
          iVar2 = -1;
        }
        else {
          if (0 < (int)count) {
            uVar11 = 0;
            iVar2 = 0;
            do {
              pcVar3 = (char *)(sqlite3_column_name(local_58[0],uVar11));
              if (pcVar3 == (char *)0x0) {
                pcVar3 = "";
              }
              pcVar3 = strdup(pcVar3);
              *(char **)((int)pointer + iVar2) = pcVar3;
              if (pcVar3 == (char *)0x0) {
                ds_append(output,"Error: out of memory\n");
                free(pointer);
                free(pointer_00);
                sqlite3_finalize(local_58[0]);
                return -1;
              }
              uVar8 = strlen(pcVar3);
              *(uint *)((int)pointer_00 + iVar2) = uVar8;
              uVar11 = uVar11 + 1;
              iVar2 = iVar2 + 4;
            } while (count != uVar11);
          }
          puVar4 = calloc(0x40,4);
          if (puVar4 == (undefined4 *)0x0) {
            ds_append(output,"Error: out of memory\n");
            if (0 < (int)count) {
              uVar11 = 0;
              do {
                free(*(void **)((int)pointer + uVar11 * 4));
                uVar11 = uVar11 + 1;
              } while (count != uVar11);
            }
            free(pointer);
            free(pointer_00);
            sqlite3_finalize(local_58[0]);
            iVar2 = -1;
          }
          else {
            iVar10 = 0;
            iVar2 = 0x40;
            while (iVar7 = sqlite3_step(local_58[0]), puVar5 = puVar4, iVar7 == 100) {
              if (iVar10 == 5000) {
                bVar1 = true;
                goto LAB_00026fac;
              }
              iVar7 = iVar2;
              if (iVar2 <= iVar10) {
                iVar7 = iVar2 << 1;
                puVar5 = realloc(puVar4,iVar2 << 3);
                if (puVar5 == (undefined4 *)0x0) {
                  ds_append(output,"Error: out of memory during result fetch\n");
                  bVar1 = false;
                  puVar5 = puVar4;
                  goto LAB_00026fac;
                }
              }
              piVar12 = (int *)(puVar5 + iVar10);
              pvVar6 = calloc(count,4);
              puVar5[iVar10] = (undefined4)(pvVar6);
              if (pvVar6 == (void *)0x0) {
                ds_append(output,"Error: out of memory allocating row\n");
                bVar1 = false;
                goto LAB_00026fac;
              }
              if (0 < (int)count) {
                uVar11 = 0;
                iVar2 = 0;
                do {
                  pcVar3 = (char *)(sqlite3_column_text(local_58[0],uVar11));
                  if (pcVar3 == (char *)0x0) {
                    iVar14 = *piVar12;
                    pcVar3 = strdup("NULL");
                    *(char **)(iVar2 + iVar14) = pcVar3;
                  }
                  else {
                    iVar14 = *piVar12;
                    pcVar3 = strdup(pcVar3);
                    *(char **)(iVar2 + iVar14) = pcVar3;
                  }
                  if (*(char **)(*piVar12 + iVar2) == (char *)0x0) {
                    ds_append(output,"Error: out of memory on cell value\n");
                    break;
                  }
                  uVar8 = strlen(*(char **)(*piVar12 + iVar2));
                  if (*(int *)((int)pointer_00 + iVar2) < (int)uVar8) {
                    *(uint *)((int)pointer_00 + iVar2) = uVar8;
                  }
                  uVar11 = uVar11 + 1;
                  iVar2 = iVar2 + 4;
                } while (count != uVar11);
              }
              iVar10 = iVar10 + 1;
              iVar2 = iVar7;
              puVar4 = puVar5;
            }
            bVar1 = false;
LAB_00026fac:
            sqlite3_finalize(local_58[0]);
            if (0 < (int)count) {
              uVar11 = 0;
              iVar2 = 0;
              while( true ) {
                ds_append(output,*(char **)((int)pointer + iVar2));
                uVar8 = strlen(*(char **)((int)pointer + iVar2));
                iVar7 = *(int *)((int)pointer_00 + iVar2) - uVar8;
                if (0 < iVar7) {
                  iVar14 = 0;
                  do {
                    ds_append(output," ");
                    iVar14 = iVar14 + 1;
                  } while (iVar7 != iVar14);
                }
                uVar11 = uVar11 + 1;
                if (count == uVar11) break;
                if (0 < (int)uVar11) {
                  ds_append(output," | ");
                }
                iVar2 = iVar2 + 4;
              }
            }
            ds_append(output,"\n");
            if (0 < (int)count) {
              uVar11 = 0;
              piVar12 = pointer_00;
              while( true ) {
                if (0 < *piVar12) {
                  iVar2 = 0;
                  do {
                    ds_append(output,"-");
                    iVar2 = iVar2 + 1;
                  } while (iVar2 < *piVar12);
                }
                uVar11 = uVar11 + 1;
                if (count == uVar11) break;
                if (0 < (int)uVar11) {
                  ds_append(output,"|");
                }
                piVar12 = piVar12 + 1;
              }
            }
            ds_append(output,"\n");
            iVar2 = 0;
            if (0 < iVar10) {
              do {
                if (0 < (int)count) {
                  uVar11 = 0;
                  iVar7 = 0;
                  while( true ) {
                    pcVar3 = *(char **)(puVar5[iVar2] + iVar7);
                    if (pcVar3 == (char *)0x0) {
                      pcVar3 = "";
                    }
                    ds_append(output,pcVar3);
                    uVar8 = strlen(pcVar3);
                    iVar14 = *(int *)((int)pointer_00 + iVar7) - uVar8;
                    if (0 < iVar14) {
                      iVar13 = 0;
                      do {
                        ds_append(output," ");
                        iVar13 = iVar13 + 1;
                      } while (iVar14 != iVar13);
                    }
                    uVar11 = uVar11 + 1;
                    if (count == uVar11) break;
                    if (0 < (int)uVar11) {
                      ds_append(output," | ");
                    }
                    iVar7 = iVar7 + 4;
                  }
                }
                ds_append(output,"\n");
                iVar2 = iVar2 + 1;
              } while (iVar2 != iVar10);
            }
            if (iVar10 == 1) {
              pcVar3 = "";
            }
            else {
              pcVar3 = "s";
            }
            if (bVar1) {
              pcVar9 = " (truncated after 5000 rows)";
            }
            else {
              pcVar9 = "";
            }
            ds_appendf(output,"%d row%s in set%s\n",iVar10,pcVar3,pcVar9);
            if (0 < iVar10) {
              iVar2 = 0;
              iVar7 = 0;
              puVar4 = puVar5;
              do {
                if (0 < (int)count) {
                  uVar11 = 0;
                  do {
                    free(*(void **)(uVar11 * 4 + *(int *)((int)puVar5 + iVar7)));
                    uVar11 = uVar11 + 1;
                  } while (count != uVar11);
                }
                free((void *)*puVar4);
                iVar2 = iVar2 + 1;
                puVar4 = puVar4 + 1;
                iVar7 = iVar7 + 4;
              } while (iVar2 != iVar10);
            }
            free(puVar5);
            free(pointer);
            free(pointer_00);
            iVar2 = 0;
          }
        }
      }
    }
    else {
      pcVar3 = (char *)(sqlite3_errmsg(database));
      ds_appendf(output,"Error: %s\n",pcVar3);
      iVar2 = -1;
    }
  }
  return iVar2;

#undef local_58
}

/*
 * _db_list_tables @ 0x0002728c..0x00027398
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in sqlite_tools: db list tables. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _db_list_tables(void *database,RecoveredDString *output)
 * Direct callers: _db_tool_execute@0x000277dc
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; prepares SQL text
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/0002728c_db_list_tables.s
 */
int db_list_tables(void *database,RecoveredDString *output)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x50] __attribute__((aligned(16)));
#define local_38 (*((void * (*)[9])(app_frame + 0x8))) /* original SP - 0x38 */

  int iVar1;
  char *pcVar2;
  int iVar3;

  iVar1 = sqlite3_prepare(database,
                           "SELECT name FROM sqlite_master WHERE type=\'table\' ORDER BY name",-1,
                           (sqlite3_stmt **)(local_38),(void *)0x0);
  if (iVar1 == 0) {
    ds_append(output,"Tables:\n");
    iVar1 = 0;
    while( true ) {
      iVar3 = sqlite3_step(local_38[0]);
      if (iVar3 != 100) break;
      pcVar2 = (char *)(sqlite3_column_text(local_38[0],0));
      if (pcVar2 == (char *)0x0) {
        pcVar2 = "";
      }
      ds_appendf(output,"  %s\n",pcVar2);
      iVar1 = iVar1 + 1;
    }
    sqlite3_finalize(local_38[0]);
    if (iVar1 == 1) {
      pcVar2 = "";
    }
    else {
      pcVar2 = "s";
    }
    ds_appendf(output,"%d table%s total\n",iVar1,pcVar2);
    iVar1 = 0;
  }
  else {
    pcVar2 = (char *)(sqlite3_errmsg(database));
    ds_appendf(output,"Error: %s\n",pcVar2);
    iVar1 = -1;
  }
  return iVar1;

#undef local_38
}

/*
 * _db_schema_single @ 0x00027398..0x000275c4
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in sqlite_tools: db schema single. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _db_schema_single(void *database,char *table,RecoveredDString *output)
 * Direct callers: _db_schema@0x000275c4
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; prepares SQL text
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00027398_db_schema_single.s
 */
int db_schema_single(void *database,char *table,RecoveredDString *output)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x560] __attribute__((aligned(16)));
#define local_548 (*((undefined1 (*)[256])(app_frame + 0x8))) /* original SP - 0x548 */
#define acStack_448 (*((char (*)[1024])(app_frame + 0x108))) /* original SP - 0x448 */
#define local_48 (*((void * (*)[8])(app_frame + 0x508))) /* original SP - 0x48 */

  int iVar1;
  char *pcVar2;
  char *pcVar3;
  char *pcVar4;
  int iVar5;
  int iVar6;
  int iVar7;

  ds_appendf(output,"Table: %s\n",table);
  iVar6 = 0;
  iVar1 = (int)*table;
  if (iVar1 != 0) {
    iVar6 = 0;
    iVar7 = 0xff;
    while ((((((iVar1 - 0x61U) & 0xff) < 0x1a || (((iVar1 - 0x41U) & 0xff) < 0x1a)) ||
            (((iVar1 - 0x30U) & 0xff) < 10)) || (iVar1 == 0x5f))) {
      local_548[iVar6] = (char)iVar1;
      iVar6 = iVar6 + 1;
      iVar1 = (int)table[iVar6];
      if ((iVar1 == 0) || (iVar7 = iVar7 + -1, iVar7 == 0)) break;
    }
  }
  local_548[iVar6] = 0;
  app_format_truncated(acStack_448,0x400,"PRAGMA table_info(%s)",local_548);
  iVar1 = sqlite3_prepare(database,acStack_448,-1,(sqlite3_stmt **)(local_48),(void *)0x0);
  iVar6 = 0;
  if (iVar1 == 0) {
    while( true ) {
      iVar1 = sqlite3_step(local_48[0]);
      if (iVar1 != 100) break;
      iVar1 = sqlite3_column_int(local_48[0],0);
      pcVar2 = (char *)(sqlite3_column_text(local_48[0],1));
      pcVar3 = (char *)(sqlite3_column_text(local_48[0],2));
      iVar7 = sqlite3_column_int(local_48[0],3);
      pcVar4 = (char *)(sqlite3_column_text(local_48[0],4));
      iVar5 = sqlite3_column_int(local_48[0],5);
      if (pcVar2 == (char *)0x0) {
        pcVar2 = "";
      }
      if (pcVar3 == (char *)0x0) {
        pcVar3 = "";
      }
      if (pcVar4 == (char *)0x0) {
        pcVar4 = "NULL";
      }
      ds_appendf(output,"  %d | %s | %s | %d | %s | %d\n",iVar1,pcVar2,pcVar3,iVar7,pcVar4,iVar5);
      iVar6 = iVar6 + 1;
    }
    sqlite3_finalize(local_48[0]);
    if (iVar6 == 0) {
      ds_append(output,"  (no columns found)\n");
    }
    ds_append(output,"\n");
    iVar1 = 0;
  }
  else {
    pcVar2 = (char *)(sqlite3_errmsg(database));
    ds_appendf(output,"Error: %s\n",pcVar2);
    iVar1 = -1;
  }
  return iVar1;

#undef local_548
#undef acStack_448
#undef local_48
}

/*
 * _db_schema @ 0x000275c4..0x000276bc
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in sqlite_tools: db schema. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _db_schema(void *database,char *table,RecoveredDString *output)
 * Direct callers: _db_tool_execute@0x000277dc
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; prepares SQL text
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000275c4_db_schema.s
 */
int db_schema(void *database,char *table,RecoveredDString *output)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x40] __attribute__((aligned(16)));
#define local_28 (*((void * (*)[6])(app_frame + 0x8))) /* original SP - 0x28 */

  int iVar1;
  char *pcVar2;
  int iVar3;

  if ((table != (char *)0x0) && (*table != '\0')) {
    iVar1 = db_schema_single(database,table,output);
    return iVar1;
  }
  iVar1 = sqlite3_prepare(database,
                           "SELECT name FROM sqlite_master WHERE type=\'table\' ORDER BY name",-1,
                           (sqlite3_stmt **)(local_28),(void *)0x0);
  iVar3 = 0;
  if (iVar1 == 0) {
    while (iVar1 = sqlite3_step(local_28[0]), iVar1 == 100) {
      pcVar2 = (char *)(sqlite3_column_text(local_28[0],0));
      if (pcVar2 != (char *)0x0) {
        db_schema_single(database,pcVar2,output);
        iVar3 = iVar3 + 1;
      }
    }
    sqlite3_finalize(local_28[0]);
    iVar1 = 0;
    if (iVar3 == 0) {
      ds_append(output,"No tables in database.\n");
      iVar1 = 0;
    }
  }
  else {
    pcVar2 = (char *)(sqlite3_errmsg(database));
    ds_appendf(output,"Error: %s\n",pcVar2);
    iVar1 = -1;
  }
  return iVar1;

#undef local_28
}

/*
 * _db_exec @ 0x000276bc..0x000277dc
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Execute a database mutation and format its affected-row/error result.
 * Parameters/return (inferred): undefined4 _db_exec(sqlite3 *param_1,undefined4 param_2,char *param_3,RecoveredDString *param_4)
 * Direct callers: _db_tool_execute@0x000277dc
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; executes SQL text
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000276bc_db_exec.s
 */
undefined4 db_exec(sqlite3 *param_1,undefined4 param_2,char *param_3,RecoveredDString *param_4)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x40] __attribute__((aligned(16)));
#define local_28 (*((char * (*)[6])(app_frame + 0x8))) /* original SP - 0x28 */

  undefined4 uVar1;
  int iVar2;
  char *pcVar3;

  if ((param_3 == (char *)0x0) || (*param_3 == '\0')) {
    ds_append(param_4,"Error: no SQL statement provided\n");
    uVar1 = 0xffffffff;
  }
  else {
    local_28[0] = (char *)0x0;
    iVar2 = sqlite3_exec(param_1,param_3,NULL,(void *)0x0,local_28);
    if (iVar2 == 0) {
      if (local_28[0] != (char *)0x0) {
        sqlite3_free(local_28[0]);
      }
      iVar2 = sqlite3_changes(param_1);
      if (iVar2 == 1) {
        pcVar3 = "";
      }
      else {
        pcVar3 = "s";
      }
      ds_appendf(param_4,"%s: OK, %d row%s affected\n",(char *)(param_2),iVar2,pcVar3);
      uVar1 = 0;
    }
    else {
      pcVar3 = local_28[0];
      if (local_28[0] == (char *)0x0) {
        pcVar3 = (char *)(sqlite3_errmsg(param_1));
      }
      ds_appendf(param_4,"Error: %s\n",pcVar3);
      uVar1 = 0xffffffff;
      if (local_28[0] != (char *)0x0) {
        sqlite3_free(local_28[0]);
        uVar1 = 0xffffffff;
      }
    }
  }
  return uVar1;

#undef local_28
}

/*
 * _db_tool_execute @ 0x000277dc..0x000279f4
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Open the supplied SQLite path and dispatch query/tables/schema/insert/update/delete operations.
 * Parameters/return (inferred): int _db_tool_execute(char *param_1,char *param_2,char *param_3,char *param_4,uint param_5)
 * Direct callers: _tool_db@0x0001513c
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; executes SQL text; opens/creates a SQLite database; prepares SQL text
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/000277dc_db_tool_execute.s
 */
int db_tool_execute(char *param_1,char *param_2,char *param_3,char *param_4,uint param_5)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x50] __attribute__((aligned(16)));
#define local_38 (*((void * *)(app_frame + 0x8))) /* original SP - 0x38 */
#define local_34 (*((RecoveredDString (*)[2])(app_frame + 0xc))) /* original SP - 0x34 */

  int iVar1;
  char *pcVar2;
  uint length;


  if ((param_1 != (char *)0x0) && (*param_1 != '\0')) {
    iVar1 = sqlite3_open(param_1,(sqlite3 **)(&local_38));
    if (iVar1 == 0) {
      ds_init(local_34);
      iVar1 = strcmp(param_2,"query");
      if (iVar1 == 0) {
        iVar1 = db_query(local_38,param_3,local_34);
      }
      else {
        iVar1 = strcmp(param_2,"tables");
        if (iVar1 == 0) {
          iVar1 = db_list_tables(local_38,local_34);
        }
        else {
          iVar1 = strcmp(param_2,"schema");
          if (iVar1 == 0) {
            iVar1 = db_schema(local_38,param_3,local_34);
          }
          else {
            iVar1 = strcmp(param_2,"insert");
            if (((iVar1 == 0) || (iVar1 = strcmp(param_2,"update"), iVar1 == 0)) ||
               (iVar1 = strcmp(param_2,"delete"), iVar1 == 0)) {
              iVar1 = db_exec(local_38,(undefined4)(param_2),param_3,local_34);
            }
            else {
              ds_appendf(local_34,"Error: unknown db operation \'%s\'\n",param_2);
              iVar1 = -1;
            }
          }
        }
      }
      length = param_5 - 1;
      if (local_34[0].length < param_5 - 1) {
        length = local_34[0].length;
      }
      if ((length != 0) && (local_34[0].data != (char *)0x0)) {
        memcpy(param_4,local_34[0].data,length);
      }
      param_4[length] = '\0';
      ds_free(local_34);
      sqlite3_close(local_38);
      return iVar1;
    }
    pcVar2 = (char *)(sqlite3_errmsg(local_38));
    snprintf(param_4,param_5,"Error: could not open database \'%s\': %s\n",param_1,pcVar2)
    ;
    sqlite3_close(local_38);
    return -1;
  }
  snprintf(param_4,param_5,"Error: no database path provided\n");
  return -1;

#undef local_38
#undef local_34
}

