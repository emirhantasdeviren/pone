#include "pone_rect_pack.h"

#include "pone_assert.h"
#include "pone_math.h"
#include "pone_memory.h"

static void _pone_rect_pack_sort_by_area_merge(PoneRectPackItem *left,
                                               PoneRectPackItem *mid,
                                               PoneRectPackItem *right,
                                               Arena *arena) {
    usize left_count = (usize)(mid - left);
    usize right_count = (usize)(right - mid + 1);
    usize arena_tmp_begin = arena->offset;
    PoneRectPackItem *left_tmp =
        arena_alloc_array(arena, left_count, PoneRectPackItem);
    PoneRectPackItem *right_tmp =
        arena_alloc_array(arena, right_count, PoneRectPackItem);

    pone_memcpy((void *)left_tmp, (void *)left,
                sizeof(PoneRectPackItem) * left_count);
    pone_memcpy((void *)right_tmp, (void *)mid,
                sizeof(PoneRectPackItem) * right_count);

    usize i = 0;
    usize j = 0;
    usize k = 0;
    while (i < left_count && j < right_count) {
        u32 left_area = pone_rect_u32_width(&left_tmp[i].rect) *
                        pone_rect_u32_height(&left_tmp[i].rect);
        u32 right_area = pone_rect_u32_width(&right_tmp[j].rect) *
                         pone_rect_u32_height(&right_tmp[j].rect);

        if (left_area > right_area) {
            left[k] = left_tmp[i];
            i++;
        } else {
            left[k] = right_tmp[j];
            j++;
        }
        k++;
    }

    while (i < left_count) {
        left[k] = left_tmp[i];
        i++;
        k++;
    }

    while (j < right_count) {
        left[k] = right_tmp[j];
        j++;
        k++;
    }
    arena->offset = arena_tmp_begin;
}

void pone_rect_pack_item_sort_by_area(PoneRectPackItem *items, usize item_count,
                                      Arena *arena) {
    if (item_count != 1) {
        usize half_rect_count = item_count / 2;

        pone_rect_pack_item_sort_by_area(items, half_rect_count, arena);
        pone_rect_pack_item_sort_by_area(items + half_rect_count,
                                         item_count - half_rect_count, arena);
        _pone_rect_pack_sort_by_area_merge(items, items + half_rect_count,
                                           items + item_count - 1, arena);
    }
}

u32 pone_rect_pack_item_calculate_total_area(PoneRectPackItem *items,
                                             usize item_count) {
    u32 sum = 0;
    for (usize i = 0; i < item_count; i++) {
        sum += pone_rect_u32_width(&items[i].rect) *
               pone_rect_u32_height(&items[i].rect);
    }

    return sum;
}

static b8 pone_rect_node_is_leaf(PoneRectPackNode *node) {
    return !(node->child[0] || node->child[1]);
}

PoneRectPackNode *pone_rect_pack_node_insert(PoneRectPackNode *node,
                                             PoneRectPackItem *item,
                                             Arena *arena) {
    if (!pone_rect_node_is_leaf(node)) {
        PoneRectPackNode *new_node =
            pone_rect_pack_node_insert(node->child[0], item, arena);
        if (!new_node) {
            new_node = pone_rect_pack_node_insert(node->child[1], item, arena);
        }

        return new_node;
    } else {
        if (node->item) {
            return 0;
        }

        u32 node_rect_width = pone_rect_u32_width(&node->rect);
        u32 node_rect_height = pone_rect_u32_height(&node->rect);

        u32 item_rect_width = pone_rect_u32_width(&item->rect);
        u32 item_rect_height = pone_rect_u32_height(&item->rect);

        if (node_rect_width < item_rect_width ||
            node_rect_height < item_rect_height) {
            return 0;
        }

        u32 dw = node_rect_width - item_rect_width;
        u32 dh = node_rect_height - item_rect_height;

        if (dw == 0 && dh == 0) {
            node->item = item;
            return node;
        }

        node->child[0] =
            (PoneRectPackNode *)arena_alloc(arena, sizeof(PoneRectPackNode));
        node->child[1] =
            (PoneRectPackNode *)arena_alloc(arena, sizeof(PoneRectPackNode));
        pone_memset((void *)node->child[0], 0, sizeof(PoneRectPackNode));
        pone_memset((void *)node->child[1], 0, sizeof(PoneRectPackNode));

        if (dw > dh) {
            node->child[0]->rect = (PoneRectU32){
                .x_min = node->rect.x_min,
                .y_min = node->rect.y_min,
                .x_max = node->rect.x_min + item_rect_width,
                .y_max = node->rect.y_max,
            };

            node->child[1]->rect = (PoneRectU32){
                .x_min = node->rect.x_min + item_rect_width,
                .y_min = node->rect.y_min,
                .x_max = node->rect.x_max,
                .y_max = node->rect.y_max,
            };
        } else {
            node->child[0]->rect = (PoneRectU32){
                .x_min = node->rect.x_min,
                .y_min = node->rect.y_min,
                .x_max = node->rect.x_max,
                .y_max = node->rect.y_min + item_rect_height,
            };

            node->child[1]->rect = (PoneRectU32){
                .x_min = node->rect.x_min,
                .y_min = node->rect.y_min + item_rect_height,
                .x_max = node->rect.x_max,
                .y_max = node->rect.y_max,
            };
        }

        return pone_rect_pack_node_insert(node->child[0], item, arena);
    }
}

struct PoneRectPackEmptySpace {
    PoneRectU32 rect;
    PoneRectPackEmptySpace *next;
};

struct PoneRectPackEmptySpacePool {
    PoneRectPackEmptySpace *full_list;
    PoneRectPackEmptySpace *free_list;
};

static void
pone_rect_pack_empty_space_pool_init(Arena *arena, usize capacity,
                                     PoneRectPackEmptySpacePool *pool) {
    PoneRectPackEmptySpace *empty_spaces =
        arena_alloc_array(arena, capacity, PoneRectPackEmptySpace);

    for (usize i = 0; i < capacity - 1; i++) {
        PoneRectPackEmptySpace *curr = empty_spaces + i;
        PoneRectPackEmptySpace *next = empty_spaces + i + 1;

        curr->next = next;
    }
    empty_spaces[capacity - 1].next = 0;

    pool->free_list = empty_spaces;
    pool->full_list = 0;
}

static void
pone_rect_pack_empty_space_pool_reset(PoneRectPackEmptySpacePool *pool) {
    PoneRectPackEmptySpace **curr = &pool->free_list;

    while (*curr) {
        curr = &(*curr)->next;
    }

    *curr = pool->full_list;
    pool->full_list = 0;
}

static void
pone_rect_pack_empty_space_pool_push(PoneRectPackEmptySpacePool *empty_spaces,
                                     PoneRectU32 *rect) {
    pone_assert(empty_spaces->free_list);

    PoneRectPackEmptySpace *free_empty_space = empty_spaces->free_list;
    empty_spaces->free_list = free_empty_space->next;

    free_empty_space->rect = *rect;
    free_empty_space->next = empty_spaces->full_list;

    empty_spaces->full_list = free_empty_space;
}

static void
pone_rect_pack_empty_space_pool_pop(PoneRectPackEmptySpacePool *empty_spaces,
                                    PoneRectPackEmptySpace *empty_space) {
    PoneRectPackEmptySpace *prev = 0;
    PoneRectPackEmptySpace *curr = empty_spaces->full_list;
    while (curr != empty_space) {
        pone_assert(curr);
        prev = curr;
        curr = curr->next;
    }
    pone_assert(curr);

    if (prev) {
        prev->next = curr->next;
    } else {
        empty_spaces->full_list = curr->next;
    }

    curr->next = empty_spaces->free_list;
    empty_spaces->free_list = curr;
}

enum PoneRectPackEmptySpaceFitResult {
    PONE_RECT_PACK_EMPTY_SPACE_FIT_PERFECT,
    PONE_RECT_PACK_EMPTY_SPACE_FIT_WIDTH_PERFECT,
    PONE_RECT_PACK_EMPTY_SPACE_FIT_HEIGHT_PERFECT,
    PONE_RECT_PACK_EMPTY_SPACE_FIT_OK,
    PONE_RECT_PACK_EMPTY_SPACE_FIT_DOES_NOT_FIT,
};

static inline PoneRectPackEmptySpaceFitResult
pone_rect_pack_empty_space_fits_item(PoneRectU32 *empty_space,
                                     PoneRectU32 *item) {
    u32 empty_space_width = pone_rect_u32_width(empty_space);
    u32 empty_space_height = pone_rect_u32_height(empty_space);
    u32 item_width = pone_rect_u32_width(item);
    u32 item_height = pone_rect_u32_height(item);

    if (item_width == empty_space_width && item_height == empty_space_height) {
        return PONE_RECT_PACK_EMPTY_SPACE_FIT_PERFECT;
    } else if (item_width == empty_space_width &&
               item_height < empty_space_height) {
        return PONE_RECT_PACK_EMPTY_SPACE_FIT_WIDTH_PERFECT;
    } else if (item_width < empty_space_width &&
               item_height == empty_space_height) {
        return PONE_RECT_PACK_EMPTY_SPACE_FIT_HEIGHT_PERFECT;
    } else if (item_width < empty_space_width &&
               item_height < empty_space_height) {
        return PONE_RECT_PACK_EMPTY_SPACE_FIT_OK;
    } else {
        return PONE_RECT_PACK_EMPTY_SPACE_FIT_DOES_NOT_FIT;
    }
}

b8 pone_rect_pack(PoneRectPackItem *items, usize item_count, u32 max_bin_side,
                  Arena *arena, u32 *side) {
    u32 max_area = max_bin_side * max_bin_side;
    u32 min_area = pone_rect_pack_item_calculate_total_area(items, item_count);
    u32 mid_side = (u32)pone_ceil(pone_sqrt((f32)(max_area + min_area) / 2.0f));

    usize arena_offset_begin = arena->offset;
    PoneRectPackItem *tmp_items =
        arena_alloc_array(arena, item_count, PoneRectPackItem);

    PoneRectPackEmptySpacePool empty_spaces;
    pone_rect_pack_empty_space_pool_init(arena, 1000, &empty_spaces);

    pone_rect_pack_item_sort_by_area(items, item_count, arena);
    for (usize item_index = 0; item_index < item_count; item_index++) {
        tmp_items[item_index].user_data = items[item_index].user_data;
    }

    b8 at_least_fit_once = 0;
    for (;;) {
        pone_rect_pack_empty_space_pool_reset(&empty_spaces);
        PoneRectU32 initial_space = {
            .x_min = 0,
            .y_min = 0,
            .x_max = mid_side,
            .y_max = mid_side,
        };
        pone_rect_pack_empty_space_pool_push(&empty_spaces, &initial_space);

        b8 all_fit = 1;
        for (usize item_index = 0; item_index < item_count; item_index++) {
            PoneRectU32 *item_rect = &items[item_index].rect;
            PoneRectU32 *tmp_item_rect = &tmp_items[item_index].rect;

            b8 item_fit = 0;
            for (PoneRectPackEmptySpace *empty_space = empty_spaces.full_list;
                 empty_space && !item_fit; empty_space = empty_space->next) {
                switch (pone_rect_pack_empty_space_fits_item(&empty_space->rect,
                                                             item_rect)) {
                case PONE_RECT_PACK_EMPTY_SPACE_FIT_PERFECT: {
                    *tmp_item_rect = empty_space->rect;
                    pone_rect_pack_empty_space_pool_pop(&empty_spaces,
                                                        empty_space);
                    item_fit = 1;
                } break;
                case PONE_RECT_PACK_EMPTY_SPACE_FIT_WIDTH_PERFECT: {
                    u32 item_height = pone_rect_u32_height(item_rect);
                    tmp_item_rect->x_min = empty_space->rect.x_min;
                    tmp_item_rect->y_min = empty_space->rect.y_min;
                    tmp_item_rect->x_max = empty_space->rect.x_max;
                    tmp_item_rect->y_max =
                        empty_space->rect.y_min + item_height;

                    PoneRectU32 split;
                    split.x_min = empty_space->rect.x_min;
                    split.y_min = empty_space->rect.y_min + item_height;
                    split.x_max = empty_space->rect.x_max;
                    split.y_max = empty_space->rect.y_max;

                    pone_rect_pack_empty_space_pool_pop(&empty_spaces,
                                                        empty_space);
                    pone_rect_pack_empty_space_pool_push(&empty_spaces, &split);
                    item_fit = 1;
                } break;
                case PONE_RECT_PACK_EMPTY_SPACE_FIT_HEIGHT_PERFECT: {
                    u32 item_width = pone_rect_u32_width(item_rect);
                    tmp_item_rect->x_min = empty_space->rect.x_min;
                    tmp_item_rect->y_min = empty_space->rect.y_min;
                    tmp_item_rect->x_max = empty_space->rect.x_min + item_width;
                    tmp_item_rect->y_max = empty_space->rect.y_max;

                    PoneRectU32 split;
                    split.x_min = empty_space->rect.x_min + item_width;
                    split.y_min = empty_space->rect.y_min;
                    split.x_max = empty_space->rect.x_max;
                    split.y_max = empty_space->rect.y_max;

                    pone_rect_pack_empty_space_pool_pop(&empty_spaces,
                                                        empty_space);
                    pone_rect_pack_empty_space_pool_push(&empty_spaces, &split);
                    item_fit = 1;
                } break;
                case PONE_RECT_PACK_EMPTY_SPACE_FIT_OK: {
                    u32 item_width = pone_rect_u32_width(item_rect);
                    u32 item_height = pone_rect_u32_height(item_rect);
                    u32 dw =
                        pone_rect_u32_width(&empty_space->rect) - item_width;
                    u32 dh =
                        pone_rect_u32_height(&empty_space->rect) - item_height;

                    tmp_item_rect->x_min = empty_space->rect.x_min;
                    tmp_item_rect->y_min = empty_space->rect.y_min;
                    tmp_item_rect->x_max = empty_space->rect.x_min + item_width;
                    tmp_item_rect->y_max =
                        empty_space->rect.y_min + item_height;
                    PoneRectU32 big_split;
                    PoneRectU32 small_split;
                    if (dw > dh) {
                        big_split.x_min = empty_space->rect.x_min + item_width;
                        big_split.y_min = empty_space->rect.y_min;
                        big_split.x_max = empty_space->rect.x_max;
                        big_split.y_max = empty_space->rect.y_max;

                        small_split.x_min = empty_space->rect.x_min;
                        small_split.y_min =
                            empty_space->rect.y_min + item_height;
                        small_split.x_max =
                            empty_space->rect.x_min + item_width;
                        small_split.y_max = empty_space->rect.y_max;
                    } else {

                        big_split.x_min = empty_space->rect.x_min;
                        big_split.y_min = empty_space->rect.y_min + item_height;
                        big_split.x_max = empty_space->rect.x_max;
                        big_split.y_max = empty_space->rect.y_max;

                        small_split.x_min =
                            empty_space->rect.x_min + item_width;
                        small_split.y_min = empty_space->rect.y_min;
                        small_split.x_max = empty_space->rect.x_max;
                        small_split.y_max =
                            empty_space->rect.y_min + item_height;
                    }
                    pone_rect_pack_empty_space_pool_pop(&empty_spaces,
                                                        empty_space);
                    pone_rect_pack_empty_space_pool_push(&empty_spaces,
                                                         &big_split);
                    pone_rect_pack_empty_space_pool_push(&empty_spaces,
                                                         &small_split);
                    item_fit = 1;
                } break;
                case PONE_RECT_PACK_EMPTY_SPACE_FIT_DOES_NOT_FIT: {
                } break;
                }
            }

            if (!item_fit) {
                all_fit = 0;
                break;
            }
        }

        if (all_fit) {
            *side = mid_side;
            pone_memcpy((void *)items, (void *)tmp_items,
                        item_count * sizeof(PoneRectPackItem));
            at_least_fit_once = 1;

            f32 mid_area = (f32)mid_side * (f32)mid_side;
            mid_side =
                (u32)pone_ceil(pone_sqrt((f32)(min_area + mid_area) / 2.0f));
        } else if (at_least_fit_once) {
            arena->offset = arena_offset_begin;
            return 1;
        } else {
            arena->offset = arena_offset_begin;
            return 0;
        }
    }
}
