#ifndef PONE_RECT_PACK_H
#define PONE_RECT_PACK_H

#include "pone_arena.h"
#include "pone_rect.h"

struct PoneRectPackItem {
    PoneRectU32 rect;
    void *user_data;
};

struct PoneRectPackNode {
    PoneRectPackNode *child[2];
    PoneRectU32 rect;
    PoneRectPackItem *item;
};

b8 pone_rect_pack(PoneRectPackItem *items, usize item_count, u32 max_bin_side,
                  Arena *arena, u32 *side);
PoneRectPackNode *pone_rect_pack_node_insert(PoneRectPackNode *node,
                                             PoneRectPackItem *item,
                                             Arena *arena);
void pone_rect_pack_item_sort_by_area(PoneRectPackItem *items, usize item_count,
                                      Arena *arena);
u32 pone_rect_pack_item_calculate_total_area(PoneRectPackItem *items,
                                             usize item_count);

#endif
