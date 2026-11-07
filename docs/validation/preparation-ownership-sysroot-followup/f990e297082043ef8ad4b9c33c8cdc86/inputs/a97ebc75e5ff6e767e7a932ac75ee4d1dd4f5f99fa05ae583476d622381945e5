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
 * _gen_tree_recursive @ 0x00025a80..0x00025bec
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in context: gen tree recursive. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): void _gen_tree_recursive(RecoveredDString *output,char *directory,int depth,int maximum_depth)
 * Direct callers: _gen_tree_recursive@0x00025a80, _context_build@0x00025bec
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; opens a directory for enumeration; reads directory entries; reads filesystem metadata
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00025a80_gen_tree_recursive.s
 */
void gen_tree_recursive(RecoveredDString *output,char *directory,int depth,int maximum_depth)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x4b0] __attribute__((aligned(16)));
#define RStack_498 (*((RecoveredTigerStat *)(app_frame + 0x8))) /* original SP - 0x498 */
#define acStack_438 (*((char (*)[1036])(app_frame + 0x68))) /* original SP - 0x438 */

  void *directory_00;
  int iVar1;
  RecoveredTigerDirent *pRVar2;
  char *left;


  if (depth <= maximum_depth) {
    directory_00 = opendir(directory);
    if (directory_00 != (void *)0x0) {
      while( true ) {
        pRVar2 = (RecoveredTigerDirent *)(readdir(directory_00));
        if (pRVar2 == (RecoveredTigerDirent *)0x0) break;
        if (pRVar2->name[0] != '.') {
          left = pRVar2->name;
          iVar1 = strcmp(left,"node_modules");
          if (iVar1 != 0) {
            iVar1 = strcmp(left,"target");
            if (iVar1 != 0) {
              if (0 < depth) {
                iVar1 = 0;
                do {
                  ds_append(output,"  ");
                  iVar1 = iVar1 + 1;
                } while (depth != iVar1);
              }
              ds_appendf(output,"- %s",left);
              snprintf(acStack_438,0x400,"%s/%s",directory,left);
              iVar1 = stat(acStack_438,(struct stat *)(&RStack_498));
              if ((iVar1 == 0) && ((RStack_498.mode & 0xf000) == 0x4000)) {
                ds_append(output,"/\n");
                gen_tree_recursive(output,acStack_438,depth + 1,maximum_depth);
              }
              else {
                ds_append(output,"\n");
              }
            }
          }
        }
      }
      closedir(directory_00);
    }
  }
  return;

#undef RStack_498
#undef acStack_438
}

/*
 * _context_build @ 0x00025bec..0x00025c58
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Generate the current project tree for inclusion in model context.
 * Parameters/return (inferred): void _context_build(RecoveredDString *output,char *directory)
 * Direct callers: _handle_slash_command@0x0000362c, _handle_cmd@0x0001b004, _start_turn@0x000202dc
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol; closes a directory stream; opens a directory for enumeration; reads directory entries; reads filesystem metadata
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00025bec_context_build.s
 */
void context_build(RecoveredDString *output,char *directory)

{
  ds_clear(output);
  ds_append(output,"Current Project Tree:\n");
  gen_tree_recursive(output,directory,0,5);
  ds_append(output,"\n");
  return;
}

/*
 * _context_over_budget @ 0x00025c58..0x00025d00
 * Status: annotated_c; symbol: retained.
 * Purpose (symbol-and-call-graph): Helper in context: context over budget. Purpose inferred from the retained symbol; exact control flow and side effects are preserved below.
 * Parameters/return (inferred): int _context_over_budget (RecoveredConversation *conversation,RecoveredConfig *config,float threshold)
 * Direct callers: _context_auto_compact@0x000261fc
 * Possible side effects (direct-call closure, incomplete for indirect calls): No sensitive imported operation proven by direct-call closure.
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/00025c58_context_over_budget.s
 */
int context_over_budget
              (RecoveredConversation *conversation,RecoveredConfig *config,float threshold)

{
  uint uVar1;
  uint uVar2;
  
  uVar2 = config_context_window(config);
  uVar1 = 0;
  if (0 < (int)uVar2) {
    token_budget_update(conversation);
    uVar1 = (uint)((int)((float)(app_bits_double(CONCAT44(0x43300000,uVar2 ^ 0x80000000)) -
                                4503601774854144.0) * threshold) <=
                  (int)conversation->estimated_tokens);
  }
  return uVar1;
}

/*
 * _context_drop_oldest @ 0x00025d00..0x000261fc
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Remove older conversation material while managing associated allocations.
 * Parameters/return (inferred): int _context_drop_oldest (RecoveredConversation *conversation,int messages_to_keep,RecoveredDString *report)
 * Direct callers: _context_auto_compact@0x000261fc, _context_enforce_hard_limit@0x000262b0
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: macosx.
 * Profile selection: Mac OS X profile models the Darwin PPC stack-argument area at +56.
 * Exact assembly: assembly/functions/00025d00_context_drop_oldest.s
 */
int context_drop_oldest
              (RecoveredConversation *conversation,int messages_to_keep,RecoveredDString *report)

{
  bool bVar1;
  int iVar2;
  int iVar3;
  RecoveredConversation *pRVar4;
  int *piVar5;
  int iVar6;
  char *pcVar7;
  char *pcVar8;
  int iVar9;
  int iVar10;
  int iVar11;
  int iVar12;
  int iVar13;
  int iVar14;
  int iVar15;
  RecoveredDString *str;
  RecoveredMessage *pRVar16;
  int iVar17;
  
  if ((conversation != (RecoveredConversation *)0x0) || (iVar6 = -1, 0 < messages_to_keep)) {
    iVar6 = conversation->message_count;
    if (messages_to_keep + 1 < iVar6) {
      if ((iVar6 < 1) || (conversation->messages[0].role != 0)) {
        iVar14 = 0;
      }
      else {
        iVar14 = 0;
        pRVar4 = conversation;
        iVar17 = iVar6;
        do {
          iVar14 = iVar14 + 1;
          iVar17 = iVar17 + -1;
          if (iVar17 == 0) break;
          pRVar16 = pRVar4->messages;
          pRVar4 = (RecoveredConversation *)(pRVar4->messages + 1);
        } while (pRVar16[1].role == 0);
      }
      iVar17 = iVar6 - messages_to_keep;
      if (iVar14 < iVar17) {
        if (iVar17 < iVar6) {
          iVar9 = iVar17;
          if (*(int *)((int)conversation + iVar17 * 0x5c) != 1) {
            piVar5 = (int *)((int)conversation + iVar17 * 0x5c + 0x5c);
            iVar3 = iVar6 - iVar17;
            do {
              iVar9 = iVar9 + 1;
              iVar3 = iVar3 + -1;
              if (iVar3 == 0) goto LAB_00025db0;
              iVar2 = *piVar5;
              piVar5 = piVar5 + 0x17;
            } while (iVar2 != 1);
          }
          iVar17 = iVar14;
          if (iVar14 < iVar9) {
            iVar17 = iVar9;
          }
        }
LAB_00025db0:
        if (iVar17 < 1) {
          iVar13 = 0;
          iVar12 = 0;
          iVar11 = 0;
          iVar10 = 0;
          iVar2 = 0;
          iVar3 = 0;
          iVar9 = 0;
        }
        else {
          iVar15 = 0;
          iVar13 = 0;
          iVar12 = 0;
          iVar11 = 0;
          iVar10 = 0;
          iVar2 = 0;
          iVar3 = 0;
          iVar9 = 0;
          pRVar4 = conversation;
          do {
            if (pRVar4->messages[0].role == 3) {
              pcVar8 = ds_cstr(&conversation->messages[iVar15].content);
              pcVar7 = strstr(pcVar8,"read_file");
              if (pcVar7 == (char *)0x0) {
                pcVar7 = strstr(pcVar8,"write_file");
                if (pcVar7 == (char *)0x0) {
                  pcVar7 = strstr(pcVar8,"edit_file");
                  if (pcVar7 == (char *)0x0) {
                    pcVar7 = strstr(pcVar8,"shell");
                    if (pcVar7 == (char *)0x0) {
                      pcVar7 = strstr(pcVar8,"glob");
                      if (pcVar7 == (char *)0x0) {
                        pcVar7 = strstr(pcVar8,"grep");
                        if (pcVar7 == (char *)0x0) {
                          pcVar8 = strstr(pcVar8,"search");
                          if (pcVar8 != (char *)0x0) {
                            iVar9 = iVar9 + 1;
                          }
                        }
                        else {
                          iVar3 = iVar3 + 1;
                        }
                      }
                      else {
                        iVar2 = iVar2 + 1;
                      }
                    }
                    else {
                      iVar10 = iVar10 + 1;
                    }
                  }
                  else {
                    iVar11 = iVar11 + 1;
                  }
                }
                else {
                  iVar12 = iVar12 + 1;
                }
              }
              else {
                iVar13 = iVar13 + 1;
              }
            }
            iVar15 = iVar15 + 1;
            pRVar4 = (RecoveredConversation *)(pRVar4->messages + 1);
          } while (iVar17 != iVar15);
        }
        ds_appendf(report,"Turns 1-%d summarized: ",iVar17);
        bVar1 = iVar13 != 0;
        if (bVar1) {
          ds_appendf(report,"%d read_file",iVar13);
        }
        if (iVar12 != 0) {
          if (bVar1) {
            ds_append(report,", ");
          }
          ds_appendf(report,"%d write_file",iVar12);
          bVar1 = true;
        }
        if (iVar11 != 0) {
          if (bVar1) {
            ds_append(report,", ");
          }
          ds_appendf(report,"%d edit_file",iVar11);
          bVar1 = true;
        }
        if (iVar10 != 0) {
          if (bVar1) {
            ds_append(report,", ");
          }
          ds_appendf(report,"%d shell",iVar10);
          bVar1 = true;
        }
        if (iVar2 != 0) {
          if (bVar1) {
            ds_append(report,", ");
          }
          ds_appendf(report,"%d glob",iVar2);
          bVar1 = true;
        }
        if (iVar3 != 0) {
          if (bVar1) {
            ds_append(report,", ");
          }
          ds_appendf(report,"%d grep",iVar3);
          bVar1 = true;
        }
        if (iVar9 == 0) {
          if (!bVar1) {
            ds_append(report,"general discussion");
          }
        }
        else {
          if (bVar1) {
            ds_append(report,", ");
          }
          ds_appendf(report,"%d search",iVar9);
        }
        ds_append(report,"\n");
        if (iVar14 < iVar17) {
          iVar3 = iVar14 * 0x5c;
          iVar9 = 0;
          do {
            message_free((RecoveredMessage *)(conversation->messages[0].call_id + iVar3 + -0x1c));
            iVar9 = iVar9 + 1;
            iVar3 = iVar3 + 0x5c;
          } while (iVar17 - iVar14 != iVar9);
        }
        pRVar16 = conversation->messages + iVar14;
        memmove(pRVar16,conversation->messages + iVar17,(iVar6 - iVar17) * 0x5c);
        iVar6 = (iVar6 - iVar17) + iVar14;
        conversation->message_count = iVar6;
        if (iVar6 < 0x17f) {
          if (iVar14 < iVar6) {
            memmove(pRVar16 + 1,pRVar16,(iVar6 - iVar14) * 0x5c);
          }
          conversation->message_count = conversation->message_count + 1;
          memset(pRVar16,0,0x5c);
          conversation->messages[iVar14].role = 0;
          str = &conversation->messages[iVar14].content;
          ds_init(str);
          pcVar8 = ds_cstr(report);
          ds_appendf(str,"[Context preserved] %s",pcVar8);
          return 0;
        }
      }
    }
    iVar6 = 0;
  }
  return iVar6;
}

/*
 * _context_auto_compact @ 0x000261fc..0x000262b0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Summarize/compact conversation context when configured budget thresholds are exceeded.
 * Parameters/return (inferred): int _context_auto_compact(RecoveredConversation *conversation,RecoveredConfig *config)
 * Direct callers: _sa_child_main@0x0001327c, _worker_child_main@0x00022774
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/000261fc_context_auto_compact.s
 */
int context_auto_compact(RecoveredConversation *conversation,RecoveredConfig *config)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x40] __attribute__((aligned(16)));
#define aRStack_28 (*((RecoveredDString (*)[2])(app_frame + 0x8))) /* original SP - 0x28 */

  uint uVar1;
  int iVar2;

  if (config->auto_compact != 0) {
    iVar2 = context_over_budget(conversation,config,0.7);
    uVar1 = conversation->message_count;
    if ((iVar2 != 0) || (0x16b < (int)uVar1)) {
      iVar2 = 0x14;
      if (((int)uVar1 < 0x28) &&
         (iVar2 = ((int)uVar1 >> 1) + (uint)((int)uVar1 < 0 && (uVar1 & 1) != 0), iVar2 < 4)) {
        iVar2 = 4;
      }
      ds_init(aRStack_28);
      iVar2 = context_drop_oldest(conversation,iVar2,aRStack_28);
      ds_free(aRStack_28);
      return (uint)(iVar2 == 0);
    }
  }
  return 0;

#undef aRStack_28
}

/*
 * _context_enforce_hard_limit @ 0x000262b0..0x000263b0
 * Status: annotated_c; symbol: retained.
 * Purpose (body-and-symbol): Apply the hard conversation-context limit before a provider request.
 * Parameters/return (inferred): int _context_enforce_hard_limit(RecoveredConversation *conversation,RecoveredConfig *config)
 * Direct callers: No resolved direct callers; may be entrypoint/callback/indirect.
 * Possible side effects (direct-call closure, incomplete for indirect calls): dynamically resolves a native symbol
 * C confidence: medium; profile: default.
 * Profile selection: Floating-point instructions or an FP interface require the default profile's FPR parameter/return model.
 * Exact assembly: assembly/functions/000262b0_context_enforce_hard_limit.s
 */
int context_enforce_hard_limit(RecoveredConversation *conversation,RecoveredConfig *config)

{
  /* Preserve observed adjacency/overlap of decompiled stack slots. */
  unsigned char app_frame[0x60] __attribute__((aligned(16)));
#define RStack_48 (*((RecoveredDString *)(app_frame + 0x8))) /* original SP - 0x48 */
#define local_38 (*((undefined4 *)(app_frame + 0x18))) /* original SP - 0x38 */
#define uStack_34 (*((uint *)(app_frame + 0x1c))) /* original SP - 0x34 */
#define local_30 (*((longlong *)(app_frame + 0x20))) /* original SP - 0x30 */

  int iVar1;
  int iVar2;

  uStack_34 = config_context_window(config);
  if (0 < (int)uStack_34) {
    uStack_34 = uStack_34 ^ 0x80000000;
    local_38 = 0x43300000;
    iVar1 = (int)((float)(app_bits_double(CONCAT44(0x43300000,uStack_34)) - 4503601774854144.0) * 0.95);
    local_30 = (longlong)iVar1;
    token_budget_update(conversation);
    if ((iVar1 < (int)conversation->estimated_tokens) && (4 < conversation->message_count)) {
      iVar2 = 0;
      while( true ) {
        ds_init(&RStack_48);
        context_drop_oldest(conversation,10,&RStack_48);
        ds_free(&RStack_48);
        token_budget_update(conversation);
        iVar2 = iVar2 + 1;
        if ((int)conversation->estimated_tokens <= iVar1) {
          return iVar2;
        }
        if (iVar2 == 0x14) break;
        if (conversation->message_count < 5) {
          return iVar2;
        }
      }
      return 0x14;
    }
  }
  return 0;

#undef RStack_48
#undef local_38
#undef uStack_34
#undef local_30
}

