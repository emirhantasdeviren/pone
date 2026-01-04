#include "pone_truetype.h"

#include "pone_assert.h"
#include "pone_math.h"
#include "pone_memory.h"
#include "pone_rect.h"
#include "pone_rect_pack.h"
#include "pone_string.h"

struct PoneSfntScanner {
    PoneTruetypeInput input;
    usize cursor;
};

static void pone_sfnt_scanner_read_string(PoneSfntScanner *scanner,
                                          PoneString *s, usize length) {
    pone_assert(scanner->cursor < scanner->input.length);

    s->buf = (u8 *)scanner->input.data + scanner->cursor;
    s->len = length;

    scanner->cursor += length;
}

static inline u32 pone_sfnt_scanner_read_be_u32(PoneSfntScanner *scanner) {
    pone_assert(scanner->cursor < scanner->input.length);

    u32 v = *(u32 *)((u8 *)scanner->input.data + scanner->cursor);
    scanner->cursor += 4;

    return (v >> 24) | ((v >> 8) & 0x0000FF00) | ((v << 8) & 0x00FF0000) |
           (v << 24);
}

static inline u16 pone_sfnt_scanner_read_be_u16(PoneSfntScanner *scanner) {
    pone_assert(scanner->cursor < scanner->input.length);

    u16 v = *(u16 *)((u8 *)scanner->input.data + scanner->cursor);
    scanner->cursor += 2;

    return (v << 8) | (v >> 8);
}

static inline i8 pone_sfnt_scanner_read_be_i8(PoneSfntScanner *scanner) {
    pone_assert(scanner->cursor < scanner->input.length);

    i8 v = *(i8 *)((u8 *)scanner->input.data + scanner->cursor);
    ++scanner->cursor;

    return v;
}

static inline i16 pone_sfnt_scanner_read_be_i16(PoneSfntScanner *scanner) {
    pone_assert(scanner->cursor < scanner->input.length);

    u16 v = *(u16 *)((u8 *)scanner->input.data + scanner->cursor);
    scanner->cursor += 2;

    v = (v << 8) | (v >> 8);
    return *(i16 *)&v;
}

static inline u8 pone_sfnt_scanner_read_u8(PoneSfntScanner *scanner) {
    pone_assert(scanner->cursor < scanner->input.length);

    u8 v = *((u8 *)scanner->input.data + scanner->cursor);
    ++scanner->cursor;
    return v;
}

static inline f32 pone_sfnt_scanner_read_f2dot14(PoneSfntScanner *scanner) {
    i16 v = pone_sfnt_scanner_read_be_i16(scanner);

    return (f32)v / (1 << 14);
}

struct PoneSfntFontDir {
    u32 scaler_type;
    u16 num_tables;
    u16 search_range;
    u16 entry_selector;
    u16 range_shift;
};

static void pone_sfnt_parse_font_dir(PoneSfntScanner *scanner,
                                     PoneSfntFontDir *font_dir) {
    font_dir->scaler_type = pone_sfnt_scanner_read_be_u32(scanner);
    font_dir->num_tables = pone_sfnt_scanner_read_be_u16(scanner);
    font_dir->search_range = pone_sfnt_scanner_read_be_u16(scanner);
    font_dir->entry_selector = pone_sfnt_scanner_read_be_u16(scanner);
    font_dir->range_shift = pone_sfnt_scanner_read_be_u16(scanner);
}

struct PoneSfntTableDirEntry {
    PoneString tag;
    u32 checksum;
    u32 offset;
    u32 length;
};

static void pone_sfnt_parse_table_entry(PoneSfntScanner *scanner,
                                        PoneSfntTableDirEntry *entry,
                                        Arena *arena) {
    pone_sfnt_scanner_read_string(scanner, &entry->tag, 4);
    entry->checksum = pone_sfnt_scanner_read_be_u32(scanner);
    entry->offset = pone_sfnt_scanner_read_be_u32(scanner);
    entry->length = pone_sfnt_scanner_read_be_u32(scanner);
}

static void pone_sfnt_parse_table_dir(PoneSfntScanner *scanner,
                                      PoneSfntTableDirEntry *entries,
                                      usize length, Arena *arena) {
    for (usize i = 0; i < length; ++i) {
        pone_sfnt_parse_table_entry(scanner, entries + i, arena);
    }
}

struct PoneSfntHead {
    u16 units_per_em;
    PoneSfntGlyphBbox global_bbox;
    i16 index_to_loc_format;
};

static void pone_sfnt_parse_head(PoneSfntScanner *scanner, PoneSfntHead *head) {
    scanner->cursor += 18;
    head->units_per_em = pone_sfnt_scanner_read_be_u16(scanner);
    scanner->cursor += 16;
    head->global_bbox.x_min = pone_sfnt_scanner_read_be_i16(scanner);
    head->global_bbox.y_min = pone_sfnt_scanner_read_be_i16(scanner);
    head->global_bbox.x_max = pone_sfnt_scanner_read_be_i16(scanner);
    head->global_bbox.y_max = pone_sfnt_scanner_read_be_i16(scanner);
    scanner->cursor += 6;
    head->index_to_loc_format = pone_sfnt_scanner_read_be_i16(scanner);
}

struct PoneSfntLoca {
    b8 format;
    void *offsets;
    usize offset_count;
};

static void pone_sfnt_parse_loca(PoneSfntScanner *scanner, b8 format,
                                 usize offset_count, PoneSfntLoca *loca,
                                 Arena *arena) {
    loca->format = format;

    if (format) {
        u32 *offsets = arena_alloc_array(arena, offset_count, u32);
        for (usize i = 0; i < offset_count; ++i) {
            offsets[i] = pone_sfnt_scanner_read_be_u32(scanner);
            pone_assert(offsets[i] < scanner->input.length);
        }
        loca->offsets = (void *)offsets;
    } else {
        u16 *offsets = arena_alloc_array(arena, offset_count, u16);
        for (usize i = 0; i < offset_count; ++i) {
            offsets[i] = pone_sfnt_scanner_read_be_u16(scanner);
            pone_assert(offsets[i] < scanner->input.length);
        }
        loca->offsets = (void *)offsets;
    }
}

struct PoneSfntMaxp {
    u16 num_glyphs;
};

static void pone_sfnt_parse_maxp(PoneSfntScanner *scanner, PoneSfntMaxp *maxp) {
    scanner->cursor += 4;
    maxp->num_glyphs = pone_sfnt_scanner_read_be_u16(scanner);
}

struct PoneSfntGlyphDescription {
    i16 number_of_contours;
    i16 x_min;
    i16 y_min;
    i16 x_max;
    i16 y_max;
};

static void
pone_sfnt_parse_glyph_description(PoneSfntScanner *scanner,
                                  PoneSfntGlyphDescription *glyph_desc) {
    glyph_desc->number_of_contours = pone_sfnt_scanner_read_be_i16(scanner);

    if (glyph_desc->number_of_contours != 0) {
        glyph_desc->x_min = pone_sfnt_scanner_read_be_i16(scanner);
        glyph_desc->y_min = pone_sfnt_scanner_read_be_i16(scanner);
        glyph_desc->x_max = pone_sfnt_scanner_read_be_i16(scanner);
        glyph_desc->y_max = pone_sfnt_scanner_read_be_i16(scanner);
    } else {
        glyph_desc->x_min = 0;
        glyph_desc->y_min = 0;
        glyph_desc->x_max = 0;
        glyph_desc->y_max = 0;
    }
}

struct PoneSfntCmapIndex {
    u16 version;
    u16 number_subtables;
};

static void pone_sfnt_scanner_parse_cmap_index(PoneSfntScanner *scanner,
                                               PoneSfntCmapIndex *index) {
    index->version = pone_sfnt_scanner_read_be_u16(scanner);
    index->number_subtables = pone_sfnt_scanner_read_be_u16(scanner);
}

struct PoneSfntCmapSubtable {
    u16 platform_id;
    u16 platform_specific_id;
    u32 offset;
};

static void
pone_sfnt_scanner_parse_cmap_subtable(PoneSfntScanner *scanner,
                                      PoneSfntCmapSubtable *subtable) {
    subtable->platform_id = pone_sfnt_scanner_read_be_u16(scanner);
    subtable->platform_specific_id = pone_sfnt_scanner_read_be_u16(scanner);
    subtable->offset = pone_sfnt_scanner_read_be_u32(scanner);
}

static void pone_sfnt_scanner_parse_cmap_format_12(PoneSfntScanner *scanner,
                                                   PoneSfntCmapFormat12 *format,
                                                   Arena *arena) {
    scanner->cursor += 2; // reserved
    format->length = pone_sfnt_scanner_read_be_u32(scanner);
    format->language = pone_sfnt_scanner_read_be_u32(scanner);
    format->group_count = pone_sfnt_scanner_read_be_u32(scanner);
    format->groups = arena_alloc_array(arena, format->group_count,
                                       PoneSfntSequentialMapGroup);
    for (usize group_index = 0; group_index < format->group_count;
         ++group_index) {
        PoneSfntSequentialMapGroup *group = format->groups + group_index;
        group->start_char = pone_sfnt_scanner_read_be_u32(scanner);
        group->end_char = pone_sfnt_scanner_read_be_u32(scanner);
        group->start_glyph_id = pone_sfnt_scanner_read_be_u32(scanner);
    }
}

static inline b8 pone_sfnt_outline_flags_on_curve(u8 flag) {
    // 0b00000001
    return flag & 0x1;
}

static inline b8 pone_sfnt_outline_flags_x_short_vector(u8 flag) {
    // 0b00000010
    return (flag & 0x02) != 0;
}

static inline b8 pone_sfnt_outline_flags_y_short_vector(u8 flag) {
    // 0b00000100
    return (flag & 0x04) != 0;
}

static inline b8 pone_sfnt_outline_flags_repeat(u8 flag) {
    // 0b00001000
    return (flag & 0x08) != 0;
}

static inline b8 pone_sfnt_outline_flags_x_is_same(u8 flag) {
    // 0b00010000
    return (flag & 0x10) != 0;
}

static inline b8 pone_sfnt_outline_flags_y_is_same(u8 flag) {
    // 0b00100000
    return (flag & 0x20) != 0;
}

static void pone_sfnt_scanner_read_coordinate(PoneSfntScanner *scanner,
                                              b8 is_short_vector, b8 is_same,
                                              i16 *coordinate) {
    if (is_short_vector) {
        i16 offset = (i16)pone_sfnt_scanner_read_u8(scanner);
        if (!is_same) {
            offset *= -1;
        }

        *coordinate += offset;
    } else if (!is_same) {
        i16 offset = pone_sfnt_scanner_read_be_i16(scanner);

        *coordinate += offset;
    }
}

static void pone_sfnt_parse_simple_glyph(PoneSfntScanner *scanner,
                                         PoneSfntGlyphDescription *desc,
                                         PoneSfntSimpleGlyph *glyph,
                                         Arena *arena, b8 debug_print) {
    usize end_points_of_contour_count = desc->number_of_contours;
    u16 *end_points_of_contours =
        arena_alloc_array(arena, end_points_of_contour_count, u16);
    for (usize i = 0; i < end_points_of_contour_count; ++i) {
        end_points_of_contours[i] = pone_sfnt_scanner_read_be_u16(scanner);
    }

    u16 instruction_count = pone_sfnt_scanner_read_be_u16(scanner);
    scanner->cursor += instruction_count;

    usize point_count;
    if (end_points_of_contour_count > 0) {
        point_count =
            end_points_of_contours[end_points_of_contour_count - 1] + 1;
    } else {
        point_count = 0;
    }
    PoneSfntGlyphPoint *points =
        arena_alloc_array(arena, point_count, PoneSfntGlyphPoint);
    usize x_size = 0;

    usize flag_index = 0;
    usize arena_temp_begin = arena->offset;
    u8 *flags = arena_alloc_array(arena, point_count, u8);
    while (flag_index < point_count) {
        usize x_size_increase = 0;
        flags[flag_index] = pone_sfnt_scanner_read_u8(scanner);
        b8 repeat = pone_sfnt_outline_flags_repeat(flags[flag_index]);
        b8 is_x_short_vector =
            pone_sfnt_outline_flags_x_short_vector(flags[flag_index]);
        b8 is_same_x = pone_sfnt_outline_flags_x_is_same(flags[flag_index]);

        if (is_x_short_vector || !is_same_x) {
            x_size_increase = is_x_short_vector ? 1 : 2;
            x_size += x_size_increase;
        }

        if (repeat) {
            u8 repeat_count = pone_sfnt_scanner_read_u8(scanner);
            x_size += repeat_count * x_size_increase;
            for (usize i = 0; i < repeat_count; ++i) {
                flags[flag_index + i + 1] = flags[flag_index];
            }
            flag_index += repeat_count;
        }

        ++flag_index;
    }

    i16 x = 0;
    i16 y = 0;
    PoneSfntScanner *x_coordinates = scanner;
    PoneSfntScanner y_coordinates = {
        .input = scanner->input,
        .cursor = scanner->cursor + x_size,
    };
    for (usize point_index = 0; point_index < point_count; ++point_index) {
        u8 flag = flags[point_index];
        b8 on_curve = pone_sfnt_outline_flags_on_curve(flag);
        b8 is_x_short_vector = pone_sfnt_outline_flags_x_short_vector(flag);
        b8 is_y_short_vector = pone_sfnt_outline_flags_y_short_vector(flag);
        b8 is_same_x = pone_sfnt_outline_flags_x_is_same(flag);
        b8 is_same_y = pone_sfnt_outline_flags_y_is_same(flag);

        pone_sfnt_scanner_read_coordinate(x_coordinates, is_x_short_vector,
                                          is_same_x, &x);
        pone_sfnt_scanner_read_coordinate(&y_coordinates, is_y_short_vector,
                                          is_same_y, &y);

        points[point_index] = {
            .on_curve = on_curve,
            .x = x,
            .y = y,
        };
    }
    *scanner = y_coordinates;
    arena->offset = arena_temp_begin;

    *glyph = {
        .end_points_of_contour_count = end_points_of_contour_count,
        .end_points_of_contours = end_points_of_contours,
        .point_count = point_count,
        .points = points,
    };
}

static inline b8 pone_sfnt_compound_glyph_args_are_x_y_values(u16 flags) {
    return flags & 0x2;
}

static inline b8 pone_sfnt_compound_glyph_arg1_and_arg2_are_words(u16 flags) {
    return flags & 0x1;
}

static inline b8 pone_sfnt_compound_glyph_we_have_a_scale(u16 flags) {
    return flags & 0x8;
}

static inline b8 pone_sfnt_component_glyph_more_components(u16 flags) {
    return flags & 0x20;
}

static inline b8 pone_sfnt_compound_glyph_we_have_an_x_and_y_scale(u16 flags) {
    return flags & 0x40;
}

static inline b8 pone_sfnt_compound_glyph_we_have_a_two_by_two(u16 flags) {
    return flags & 0x80;
}

static usize pone_sfnt_scanner_count_component_glyphs_in_compound_glyph(
    PoneSfntScanner *scanner) {
    usize count = 1;

    u16 flags = pone_sfnt_scanner_read_be_u16(scanner);
    while (pone_sfnt_component_glyph_more_components(flags)) {
        count++;

        scanner->cursor += 2;

        if (pone_sfnt_compound_glyph_arg1_and_arg2_are_words(flags)) {
            scanner->cursor += 4;
        } else {
            scanner->cursor += 2;
        }

        if (pone_sfnt_compound_glyph_we_have_a_scale(flags)) {
            scanner->cursor += 2;
        } else if (pone_sfnt_compound_glyph_we_have_an_x_and_y_scale(flags)) {
            scanner->cursor += 4;
        } else if (pone_sfnt_compound_glyph_we_have_a_two_by_two(flags)) {
            scanner->cursor += 8;
        }

        flags = pone_sfnt_scanner_read_be_u16(scanner);
    }

    return count;
}

static void
pone_sfnt_parse_component_glyph(PoneSfntScanner *scanner,
                                PoneSfntComponentGlyph *component_glyph) {

    component_glyph->flags = pone_sfnt_scanner_read_be_u16(scanner);
    component_glyph->glyph_index = pone_sfnt_scanner_read_be_u16(scanner);
    pone_assert(
        pone_sfnt_compound_glyph_args_are_x_y_values(component_glyph->flags));

    if (pone_sfnt_compound_glyph_arg1_and_arg2_are_words(
            component_glyph->flags)) {
        component_glyph->offset.x = (f32)pone_sfnt_scanner_read_be_i16(scanner);
        component_glyph->offset.y = (f32)pone_sfnt_scanner_read_be_i16(scanner);
    } else {
        component_glyph->offset.x = (f32)pone_sfnt_scanner_read_be_i8(scanner);
        component_glyph->offset.y = (f32)pone_sfnt_scanner_read_be_i8(scanner);
    }

    if (pone_sfnt_compound_glyph_we_have_a_scale(component_glyph->flags)) {
        component_glyph->transformation.data[0] =
            pone_sfnt_scanner_read_f2dot14(scanner);
        component_glyph->transformation.data[1] = 0.0f;
        component_glyph->transformation.data[2] = 0.0f;
        component_glyph->transformation.data[3] =
            component_glyph->transformation.data[0];
    } else if (pone_sfnt_compound_glyph_we_have_an_x_and_y_scale(
                   component_glyph->flags)) {
        component_glyph->transformation.data[0] =
            pone_sfnt_scanner_read_f2dot14(scanner);
        component_glyph->transformation.data[1] = 0.0f;
        component_glyph->transformation.data[2] = 0.0f;
        component_glyph->transformation.data[3] =
            pone_sfnt_scanner_read_f2dot14(scanner);
    } else if (pone_sfnt_compound_glyph_we_have_a_two_by_two(
                   component_glyph->flags)) {
        component_glyph->transformation.data[0] =
            pone_sfnt_scanner_read_f2dot14(scanner);
        component_glyph->transformation.data[1] =
            pone_sfnt_scanner_read_f2dot14(scanner);
        component_glyph->transformation.data[2] =
            pone_sfnt_scanner_read_f2dot14(scanner);
        component_glyph->transformation.data[3] =
            pone_sfnt_scanner_read_f2dot14(scanner);
    } else {
        component_glyph->transformation.data[0] = 1.0f;
        component_glyph->transformation.data[1] = 0.0f;
        component_glyph->transformation.data[2] = 0.0f;
        component_glyph->transformation.data[3] = 1.0f;
    }
}

static void
pone_sfnt_parse_compound_glyph(PoneSfntScanner *scanner,
                               PoneSfntCompoundGlyph *compound_glyph,
                               Arena *arena) {
    usize begin_cursor = scanner->cursor;
    compound_glyph->component_glyph_count =
        pone_sfnt_scanner_count_component_glyphs_in_compound_glyph(scanner);
    scanner->cursor = begin_cursor;

    compound_glyph->component_glyphs = arena_alloc_array(
        arena, compound_glyph->component_glyph_count, PoneSfntComponentGlyph);
    for (usize i = 0; i < compound_glyph->component_glyph_count; ++i) {
        PoneSfntComponentGlyph *component_glyph =
            compound_glyph->component_glyphs + i;
        pone_sfnt_parse_component_glyph(scanner, component_glyph);
    }
}

static void pone_sfnt_parse_glyph(PoneSfntScanner *scanner,
                                  PoneSfntGlyph *glyph, Arena *arena,
                                  b8 debug_print) {
    PoneSfntGlyphDescription glyph_desc;
    pone_sfnt_parse_glyph_description(scanner, &glyph_desc);
    glyph->bbox = {
        .x_min = glyph_desc.x_min,
        .y_min = glyph_desc.y_min,
        .x_max = glyph_desc.x_max,
        .y_max = glyph_desc.y_max,
    };

    if (glyph_desc.number_of_contours > 0) {
        glyph->type = PONE_SFNT_GLYPH_TYPE_SIMPLE;
        pone_sfnt_parse_simple_glyph(scanner, &glyph_desc, &glyph->simple,
                                     arena, debug_print);
    } else if (glyph_desc.number_of_contours < 0) {
        glyph->type = PONE_SFNT_GLYPH_TYPE_COMPOUND;
        pone_sfnt_parse_compound_glyph(scanner, &glyph->compound, arena);
    }
}

static inline void pone_sfnt_glyph_bbox_rect(PoneSfntGlyphBbox *bbox,
                                             PoneRectF32 *rect) {
    *rect = (PoneRectF32){
        .p_min =
            {
                .x = (f32)bbox->x_min,
                .y = (f32)bbox->y_min,
            },
        .p_max =
            {
                .x = (f32)bbox->x_max,
                .y = (f32)bbox->y_max,
            },
    };
}

PoneTrueTypeFont *pone_truetype_parse(PoneTruetypeInput input, Arena *arena) {
    PoneSfntScanner scanner = {
        .input = input,
        .cursor = 0,
    };
    PoneSfntFontDir font_dir;
    pone_sfnt_parse_font_dir(&scanner, &font_dir);

    usize entry_count = font_dir.num_tables;
    PoneSfntTableDirEntry *entries =
        arena_alloc_array(arena, entry_count, PoneSfntTableDirEntry);
    pone_sfnt_parse_table_dir(&scanner, entries, entry_count, arena);
    PoneSfntTableDirEntry *glyph_entry;
    PoneSfntTableDirEntry *head_entry;
    PoneSfntTableDirEntry *loca_entry;
    PoneSfntTableDirEntry *maxp_entry;
    PoneSfntTableDirEntry *cmap_entry;
    for (usize i = 0; i < entry_count; ++i) {
        if (pone_string_eq(entries[i].tag, {.buf = (u8 *)"glyf", .len = 4})) {
            glyph_entry = entries + i;
        } else if (pone_string_eq(entries[i].tag,
                                  {.buf = (u8 *)"head", .len = 4})) {
            head_entry = entries + i;
        } else if (pone_string_eq(entries[i].tag,
                                  {.buf = (u8 *)"loca", .len = 4})) {
            loca_entry = entries + i;
        } else if (pone_string_eq(entries[i].tag,
                                  {.buf = (u8 *)"loca", .len = 4})) {
            loca_entry = entries + i;
        } else if (pone_string_eq(entries[i].tag,
                                  {.buf = (u8 *)"maxp", .len = 4})) {
            maxp_entry = entries + i;
        } else if (pone_string_eq(entries[i].tag,
                                  {.buf = (u8 *)"cmap", .len = 4})) {
            cmap_entry = entries + i;
        }
    }
    pone_assert(head_entry);
    pone_assert(maxp_entry);
    pone_assert(loca_entry);
    pone_assert(glyph_entry);
    pone_assert(cmap_entry);
    scanner.cursor = cmap_entry->offset;
    PoneSfntCmapIndex cmap_index;
    pone_sfnt_scanner_parse_cmap_index(&scanner, &cmap_index);
    b8 found_unicode_subtable = 0;
    for (usize cmap_subtable_index = 0;
         cmap_subtable_index < cmap_index.number_subtables;
         ++cmap_subtable_index) {
        PoneSfntCmapSubtable subtable;
        pone_sfnt_scanner_parse_cmap_subtable(&scanner, &subtable);
        if (subtable.platform_id == 0 && subtable.platform_specific_id == 4) {
            scanner.cursor = cmap_entry->offset + subtable.offset;
            found_unicode_subtable = 1;
            break;
        }
    }
    pone_assert(found_unicode_subtable);
    u16 format_id = pone_sfnt_scanner_read_be_u16(&scanner);
    pone_assert(format_id == 12);
    PoneSfntCmapFormat12 format_12;
    pone_sfnt_scanner_parse_cmap_format_12(&scanner, &format_12, arena);

    scanner.cursor = head_entry->offset;
    PoneSfntHead head;
    pone_sfnt_parse_head(&scanner, &head);

    scanner.cursor = maxp_entry->offset;
    PoneSfntMaxp maxp;
    pone_sfnt_parse_maxp(&scanner, &maxp);

    scanner.cursor = loca_entry->offset;
    PoneSfntLoca loca;
    pone_sfnt_parse_loca(&scanner, (b8)head.index_to_loc_format,
                         (usize)maxp.num_glyphs, &loca, arena);

    PoneTrueTypeFont *font =
        (PoneTrueTypeFont *)arena_alloc(arena, sizeof(PoneTrueTypeFont));
    font->format_12 = format_12;
    font->units_per_em = head.units_per_em;
    font->global_bbox = head.global_bbox;
    font->glyph_count = maxp.num_glyphs;
    font->glyphs = arena_alloc_array(arena, font->glyph_count, PoneSfntGlyph);
    for (usize glyph_index = 0; glyph_index < font->glyph_count;
         ++glyph_index) {
        usize glyph_offset;
        if (loca.format) {
            glyph_offset = (usize) * ((u32 *)loca.offsets + glyph_index);
        } else {
            glyph_offset = (usize) * ((u16 *)loca.offsets + glyph_index);
        }

        scanner.cursor = glyph_entry->offset + glyph_offset;
        b8 debug_print = glyph_index < 5;
        pone_sfnt_parse_glyph(&scanner, font->glyphs + glyph_index, arena,
                              debug_print);
    }

    return font;
}

static inline Vec2 pone_linear_interp(Vec2 p0, Vec2 p1, f32 t) {
    return (Vec2){
        .x = p0.x * (1.0f - t) + p1.x * t,
        .y = p0.y * (1.0f - t) + p1.y * t,
    };
}

static Vec2 pone_quadratic_bezier_curve(Vec2 p0, Vec2 p1, Vec2 p2, f32 t) {
    return (Vec2){
        .x = p1.x + (1.0f - t) * (1.0f - t) * (p0.x - p1.x) +
             t * t * (p2.x - p1.x),
        .y = p1.y + (1.0f - t) * (1.0f - t) * (p0.y - p1.y) +
             t * t * (p2.y - p1.y),
    };
}

static Vec2 pone_line_segment_derivative(Vec2 *p) {
    return pone_vec2_sub(p[1], p[0]);
}

static Vec2 pone_quadratic_bezier_curve_derivative(Vec2 *p, f32 t) {
    return pone_vec2_add(
        pone_vec2_mul_scalar(2.0f, pone_vec2_sub(p[1], p[0])),
        pone_vec2_mul_scalar(
            2.0f * t,
            pone_vec2_add(
                pone_vec2_add(p[2], pone_vec2_mul_scalar(-2.0f, p[1])), p[0])));
}

struct PoneTrueTypeEdgeSegment {
    Vec2 points[3];
    usize point_count;
};

static Vec2 pone_truetype_edge_segment_at_t(PoneTrueTypeEdgeSegment *edge,
                                            f32 t) {
    switch (edge->point_count) {
    case 2: {
        return pone_linear_interp(edge->points[0], edge->points[1], t);
    } break;
    case 3: {
        return pone_quadratic_bezier_curve(edge->points[0], edge->points[1],
                                           edge->points[2], t);
    } break;
    default: {
        pone_assert(0);
        return (Vec2){};
    } break;
    }
}

static void pone_truetype_edge_segment_bbox(PoneTrueTypeEdgeSegment *edge,
                                            PoneRectF32 *rect) {
    switch (edge->point_count) {
    case 2: {
        Vec2 a = pone_truetype_edge_segment_at_t(edge, 0.0f);
        Vec2 b = pone_truetype_edge_segment_at_t(edge, 1.0f);

        rect->p_min.x = PONE_MIN(a.x, b.x);
        rect->p_min.y = PONE_MIN(a.y, b.y);
        rect->p_max.x = PONE_MAX(a.x, b.x);
        rect->p_max.y = PONE_MAX(a.y, b.y);
    } break;
    case 3: {
        Vec2 p1 = pone_vec2_sub(edge->points[1], edge->points[0]);
        Vec2 p2 = pone_vec2_add(
            pone_vec2_add(edge->points[2],
                          pone_vec2_mul_scalar(-2.0f, edge->points[1])),
            edge->points[0]);

        Vec2 a = pone_truetype_edge_segment_at_t(edge, 0.0f);
        Vec2 b = pone_truetype_edge_segment_at_t(edge, 1.0f);

        rect->p_min.x = PONE_MIN(a.x, b.x);
        rect->p_min.y = PONE_MIN(a.y, b.y);
        rect->p_max.x = PONE_MAX(a.x, b.x);
        rect->p_max.y = PONE_MAX(a.y, b.y);

        if (p2.x > PONE_EPSILON || p2.x < -PONE_EPSILON) {
            f32 t_x = PONE_CLAMP(p1.x / p2.x, 0.0f, 1.0f);
            Vec2 c = pone_truetype_edge_segment_at_t(edge, t_x);

            rect->p_min.x = PONE_MIN(rect->p_min.x, c.x);
            rect->p_max.x = PONE_MAX(rect->p_max.x, c.x);
        }
        if (p2.y > PONE_EPSILON || p2.y < -PONE_EPSILON) {
            f32 t_y = PONE_CLAMP(p1.y / p2.y, 0.0f, 1.0f);
            Vec2 d = pone_truetype_edge_segment_at_t(edge, t_y);

            rect->p_min.y = PONE_MIN(rect->p_min.y, d.y);
            rect->p_max.y = PONE_MAX(rect->p_max.y, d.y);
        }
    } break;
    default: {
        pone_assert(0);
    } break;
    }
}

static inline b8 pone_truetype_between_closed_open(Vec2 p, PoneRectF32 *rect) {
    return p.y >= rect->p_min.y - PONE_EPSILON &&
           p.y < rect->p_max.y - PONE_EPSILON;
}

#if 0
static Vec2 pone_truetype_edge_segment_derivative(PoneTrueTypeEdgeSegment *edge,
                                                  f32 t) {
    switch (edge->point_count) {
    case 2: {
        return pone_line_segment_derivative(edge->points);
    } break;
    case 3: {
        return pone_quadratic_bezier_curve_derivative(edge->points, t);
    } break;
    default: {
        pone_assert(0);
        return (Vec2){};
    } break;
    }
}

#endif

static f32 pone_solve_linear_equation(f32 a, f32 b) { return -b / a; }

static void pone_solve_quadratic_equation(f32 a, f32 b, f32 c, f32 *z) {
    f32 d = b * b - 4 * a * c;
    pone_assert(pone_is_sign_positive(d));
    f32 d_sqrt = pone_sqrt(d);
    z[0] = (-b + d_sqrt) / (2 * a);
    z[1] = (-b - d_sqrt) / (2 * a);
}

static f32 pone_truetype_quadratic_bezier_segment_find_distance(
    PoneTrueTypeEdgeSegment *edge, Vec2 point, f32 *t) {
    Vec2 p = pone_vec2_sub(point, edge->points[0]);
    Vec2 p1 = pone_vec2_sub(edge->points[1], edge->points[0]);
    Vec2 p2 = pone_vec2_add(
        pone_vec2_add(edge->points[2],
                      pone_vec2_mul_scalar(-2.0f, edge->points[1])),
        edge->points[0]);
    f32 a[4] = {
        pone_vec2_dot(pone_vec2_neg(p1), p),
        pone_vec2_dot(pone_vec2_mul_scalar(2.0f, p1), p1) -
            pone_vec2_dot(p2, p),
        3.0f * pone_vec2_dot(p1, p2),
        pone_vec2_dot(p2, p2),
    };

    if (pone_abs(a[3]) < PONE_EPSILON) {
        if (pone_abs(a[2]) < PONE_EPSILON) {
            if (pone_abs(a[1]) < PONE_EPSILON) {
                return a[0];
            }

            f32 z = pone_solve_linear_equation(a[1], a[0]);
            f32 t = PONE_CLAMP(z, 0.0f, 1.0f);
            Vec2 closest_point = pone_quadratic_bezier_curve(
                edge->points[0], edge->points[1], edge->points[2], t);
            f32 dist = pone_sqrt(
                pone_vec2_len_squared(pone_vec2_sub(closest_point, point)));

            return dist;
        }

        f32 z[2];
        pone_solve_quadratic_equation(a[2], a[1], a[0], z);

        f32 dist = PONE_F32_MAX;
        for (usize i = 0; i < 2; ++i) {
            f32 t_candidate = PONE_CLAMP(z[i], 0.0f, 1.0f);
            Vec2 closest_point = pone_quadratic_bezier_curve(
                edge->points[0], edge->points[1], edge->points[2], t_candidate);
            f32 candidate_dist = pone_sqrt(
                pone_vec2_len_squared(pone_vec2_sub(closest_point, point)));

            if (candidate_dist < dist) {
                dist = candidate_dist;
                *t = t_candidate;
            }
        }

        return dist;
    }

    for (usize i = 0; i < 3; ++i) {
        a[i] /= a[3];
    }
    a[3] = 1.0f;

    f32 q = (3.0f * a[1] - a[2] * a[2]) / 9.0f;
    f32 r =
        (9.0f * a[2] * a[1] - 27.0f * a[0] - 2.0f * a[2] * a[2] * a[2]) / 54.0f;

    f32 d = q * q * q + r * r;
    if (pone_abs(d) < PONE_EPSILON) {
        f32 big_s = pone_cbrt(r);

        f32 z[2] = {
            (-1.0f / 3.0f) * a[2] + 2 * big_s,
            (-1.0f / 3.0f) * a[2] - big_s,
        };

        f32 dist = PONE_F32_MAX;
        for (usize i = 0; i < 2; ++i) {
            f32 t_candidate = PONE_CLAMP(z[i], 0.0f, 1.0f);
            Vec2 closest_point = pone_quadratic_bezier_curve(
                edge->points[0], edge->points[1], edge->points[2], t_candidate);
            f32 candidate_dist = pone_sqrt(
                pone_vec2_len_squared(pone_vec2_sub(closest_point, point)));

            if (candidate_dist < dist) {
                dist = candidate_dist;
                *t = t_candidate;
            }
        }

        return dist;
    } else if (pone_is_sign_negative(d)) {
        f32 theta = pone_acos(r / pone_sqrt(-q * q * q));

        f32 z[3] = {
            2.0f * pone_sqrt(-q) * pone_cos(theta / 3.0f) -
                (1.0f / 3.0f) * a[2],
            2.0f * pone_sqrt(-q) * pone_cos((theta + 2.0f * PONE_PI) / 3.0f) -
                (1.0f / 3.0f) * a[2],
            2.0f * pone_sqrt(-q) * pone_cos((theta + 4.0f * PONE_PI) / 3.0f) -
                (1.0f / 3.0f) * a[2],
        };

        f32 dist = PONE_F32_MAX;
        for (usize i = 0; i < 3; ++i) {
            f32 t_candidate = PONE_CLAMP(z[i], 0.0f, 1.0f);
            Vec2 closest_point = pone_quadratic_bezier_curve(
                edge->points[0], edge->points[1], edge->points[2], t_candidate);
            f32 candidate_dist = pone_sqrt(
                pone_vec2_len_squared(pone_vec2_sub(closest_point, point)));

            if (candidate_dist < dist) {
                dist = candidate_dist;
                *t = t_candidate;
            }
        }

        return dist;
    } else {
        f32 big_s = pone_cbrt(r + pone_sqrt(d));
        f32 big_t = pone_cbrt(r - pone_sqrt(d));

        f32 z = (-1.0f / 3.0f) * a[2] + (big_s + big_t);
        *t = PONE_CLAMP(z, 0.0f, 1.0f);

        Vec2 closest_point = pone_quadratic_bezier_curve(
            edge->points[0], edge->points[1], edge->points[2], *t);
        return pone_sqrt(
            pone_vec2_len_squared(pone_vec2_sub(closest_point, point)));
    }
}

static f32 pone_truetype_find_quadratic_bezier_curve_distance(
    Vec2 point, PoneTrueTypeEdgeSegment *edge, f32 *t) {
    Vec2 p = pone_vec2_sub(point, edge->points[0]);
    Vec2 p1 = pone_vec2_sub(edge->points[1], edge->points[0]);
    Vec2 p2 = pone_vec2_add(
        pone_vec2_add(edge->points[2],
                      pone_vec2_mul_scalar(-2.0f, edge->points[1])),
        edge->points[0]);
    f32 a[4] = {
        pone_vec2_dot(pone_vec2_neg(p1), p),
        pone_vec2_dot(pone_vec2_mul_scalar(2.0f, p1), p1) -
            pone_vec2_dot(p2, p),
        3.0f * pone_vec2_dot(p1, p2),
        pone_vec2_dot(p2, p2),
    };
    for (usize i = 0; i < 3; ++i) {
        a[i] /= a[3];
    }
    a[3] = 1.0f;

    f32 q = (3.0f * a[1] - a[2] * a[2]) / 9.0f;
    f32 r =
        (9.0f * a[2] * a[1] - 27.0f * a[0] - 2.0f * a[2] * a[2] * a[2]) / 54.0f;

    f32 d = q * q * q + r * r;
    if (pone_abs(d) < PONE_EPSILON) {
        f32 big_s = pone_cbrt(r);

        f32 z[2] = {
            (-1.0f / 3.0f) * a[2] + 2 * big_s,
            (-1.0f / 3.0f) * a[2] - big_s,
        };

        f32 dist = PONE_F32_MAX;
        for (usize i = 0; i < 2; ++i) {
            f32 t_candidate = PONE_CLAMP(z[i], 0.0f, 1.0f);
            Vec2 closest_point = pone_quadratic_bezier_curve(
                edge->points[0], edge->points[1], edge->points[2], t_candidate);
            f32 candidate_dist = pone_sqrt(
                pone_vec2_len_squared(pone_vec2_sub(closest_point, point)));

            if (candidate_dist < dist) {
                dist = candidate_dist;
                *t = t_candidate;
            }
        }

        return dist;
    } else if (pone_is_sign_negative(d)) {
        f32 theta = pone_acos(r / pone_sqrt(-q * q * q));

        f32 z[3] = {
            2.0f * pone_sqrt(-q) * pone_cos(theta / 3.0f) -
                (1.0f / 3.0f) * a[2],
            2.0f * pone_sqrt(-q) * pone_cos((theta + 2.0f * PONE_PI) / 3.0f) -
                (1.0f / 3.0f) * a[2],
            2.0f * pone_sqrt(-q) * pone_cos((theta + 4.0f * PONE_PI) / 3.0f) -
                (1.0f / 3.0f) * a[2],
        };

        f32 dist = PONE_F32_MAX;
        for (usize i = 0; i < 3; ++i) {
            f32 t_candidate = PONE_CLAMP(z[i], 0.0f, 1.0f);
            Vec2 closest_point = pone_quadratic_bezier_curve(
                edge->points[0], edge->points[1], edge->points[2], t_candidate);
            f32 candidate_dist = pone_sqrt(
                pone_vec2_len_squared(pone_vec2_sub(closest_point, point)));

            if (candidate_dist < dist) {
                dist = candidate_dist;
                *t = t_candidate;
            }
        }

        return dist;
    } else {
        f32 big_s = pone_cbrt(r + pone_sqrt(d));
        f32 big_t = pone_cbrt(r - pone_sqrt(d));

        f32 z = (-1.0f / 3.0f) * a[2] + (big_s + big_t);
        *t = PONE_CLAMP(z, 0.0f, 1.0f);

        Vec2 closest_point = pone_quadratic_bezier_curve(
            edge->points[0], edge->points[1], edge->points[2], *t);
        return pone_sqrt(
            pone_vec2_len_squared(pone_vec2_sub(closest_point, point)));
    }
}

static f32
pone_truetype_line_segment_find_nearest_t(PoneTrueTypeEdgeSegment *edge,
                                          Vec2 p) {
    Vec2 p_p0 = pone_vec2_sub(p, edge->points[0]);
    Vec2 p1_p0 = pone_vec2_sub(edge->points[1], edge->points[0]);

    f32 t = pone_vec2_dot(p_p0, p1_p0) / pone_vec2_dot(p1_p0, p1_p0);
    t = PONE_CLAMP(t, 0.0f, 1.0f);

    return t;
}

static f32
pone_truetype_line_segment_find_distance(PoneTrueTypeEdgeSegment *edge, Vec2 p,
                                         f32 *t) {
    *t = pone_truetype_line_segment_find_nearest_t(edge, p);
    Vec2 bt = pone_linear_interp(edge->points[0], edge->points[1], *t);

    return pone_sqrt(pone_vec2_len_squared(pone_vec2_sub(bt, p)));
}

static b8 pone_truetype_distance_sign(Vec2 p, PoneTrueTypeEdgeSegment *edge,
                                      f32 t) {
    Vec2 d_bt;
    Vec2 bt;
    switch (edge->point_count) {
    case 2: {
        d_bt = pone_line_segment_derivative(edge->points);
        bt = pone_linear_interp(edge->points[0], edge->points[1], t);
    } break;
    case 3: {
        d_bt = pone_quadratic_bezier_curve_derivative(edge->points, t);
        bt = pone_quadratic_bezier_curve(edge->points[0], edge->points[1],
                                         edge->points[2], t);
    } break;
    default: {
        pone_assert(0 && "point count not 2 or 3");
    } break;
    }

    return pone_is_sign_negative(
        pone_vec2_perp_dot(d_bt, pone_vec2_sub(bt, p)));
}

static b8
pone_truetype_edge_segment_calculate_side(PoneTrueTypeEdgeSegment *edge, Vec2 p,
                                          f32 t) {
    Vec2 d_bt;
    Vec2 bt;
    switch (edge->point_count) {
    case 2: {
        d_bt = pone_line_segment_derivative(edge->points);
        bt = pone_linear_interp(edge->points[0], edge->points[1], t);
    } break;
    case 3: {
        d_bt = pone_quadratic_bezier_curve_derivative(edge->points, t);
        bt = pone_quadratic_bezier_curve(edge->points[0], edge->points[1],
                                         edge->points[2], t);
    } break;
    default: {
        pone_assert(0 && "point count not 2 or 3");
    } break;
    }

    return pone_is_sign_negative(
        pone_vec2_perp_dot(d_bt, pone_vec2_sub(bt, p)));
}

static f32
pone_truetype_edge_segment_calculate_distance(PoneTrueTypeEdgeSegment *edge,
                                              Vec2 p, b8 *side) {
    f32 d;
    f32 t;
    switch (edge->point_count) {
    case 2: {
        d = pone_truetype_line_segment_find_distance(edge, p, &t);
    } break;
    case 3: {
        d = pone_truetype_quadratic_bezier_segment_find_distance(edge, p, &t);
    } break;
    default: {
        t = 0.0f;
        d = 0.0f;
        pone_assert(0);
    } break;
    }

    *side = pone_truetype_edge_segment_calculate_side(edge, p, t);
    return d;
}

static b8 pone_truetype_orthogonality_test(Vec2 point,
                                           PoneTrueTypeEdgeSegment *edge_0,
                                           PoneTrueTypeEdgeSegment *edge_1,
                                           f32 d_min, f32 d) {
    if (pone_abs(d_min) - pone_abs(d) > PONE_EPSILON ||
        pone_abs(d_min) - pone_abs(d) < -PONE_EPSILON ||
        !(pone_f32_sign_bit(d_min) ^ pone_f32_sign_bit(d))) {
        return 1;
    }

    PoneTrueTypeEdgeSegment *edges[2] = {edge_0, edge_1};
    f32 ortho[2];

    for (usize i = 0; i < 2; ++i) {
        f32 t;
        if (i == 0) {
            t = 1.0f;
        } else {
            t = 0.0f;
        }
        Vec2 d_bt;
        Vec2 bt;
        switch (edges[i]->point_count) {
        case 2: {
            d_bt =
                pone_vec2_norm(pone_line_segment_derivative(edges[i]->points));
            bt =
                pone_linear_interp(edges[i]->points[0], edges[i]->points[1], t);
        } break;
        case 3: {
            d_bt = pone_vec2_norm(
                pone_quadratic_bezier_curve_derivative(edges[i]->points, t));
            bt = pone_quadratic_bezier_curve(edges[i]->points[0],
                                             edges[i]->points[1],
                                             edges[i]->points[2], t);
        } break;
        default: {
            pone_assert(0 && "point count not 2 or 3");
        } break;
        }

        Vec2 p_bt = pone_vec2_norm(pone_vec2_sub(point, bt));

        ortho[i] = pone_vec2_perp_dot(d_bt, p_bt);
    }

    return pone_abs(ortho[0]) > pone_abs(ortho[1]);
}

struct PoneSfntGlyphPointTransformation {
    Vec2 offset;
    Mat2 scale;
};

static void pone_sfnt_glyph_point_apply_transformation(
    Vec2 *glyph_point, PoneSfntGlyphPointTransformation *transform) {
    Vec2 result = (Vec2){
        .x = (transform->scale.data[0] * glyph_point->x +
              transform->scale.data[1] * glyph_point->y) +
             transform->offset.x,
        .y = (transform->scale.data[2] * glyph_point->x +
              transform->scale.data[3] * glyph_point->y) +
             transform->offset.y,
    };

    *glyph_point = result;
}

struct PoneSfntGlyphPointRangeMap {
    f32 scale;
    PoneRectF32 *range_a;
    PoneRectF32 *range_b;
    b8 invert_y;
};

struct PoneSfntGlyphPointMapConstants {
    PoneSfntGlyphPointRangeMap *range_map;
    PoneSfntGlyphPointTransformation *transform;
};

static void
pone_sfnt_glyph_point_map(Vec2 *glyph_point,
                          PoneSfntGlyphPointMapConstants *map_constants) {
    if (map_constants->transform) {
        pone_sfnt_glyph_point_apply_transformation(glyph_point,
                                                   map_constants->transform);
    }

    if (map_constants->range_map) {
        glyph_point->x = glyph_point->x * map_constants->range_map->scale -
                         map_constants->range_map->range_a->p_min.x;
        if (!map_constants->range_map->invert_y) {
            glyph_point->y = glyph_point->y * map_constants->range_map->scale -
                             map_constants->range_map->range_a->p_min.y;
        } else {
            glyph_point->y =
                map_constants->range_map->range_b->p_max.y +
                (-glyph_point->y * map_constants->range_map->scale +
                 map_constants->range_map->range_a->p_min.y);
        }
    }
}

static Vec2 pone_sfnt_glyph_point_vec2(PoneSfntGlyphPoint *glyph_point) {
    return (Vec2){
        .x = (f32)glyph_point->x,
        .y = (f32)glyph_point->y,
    };
}

static Vec2 pone_sfnt_glyph_point_mid_vec2(PoneSfntGlyphPoint *p1,
                                           PoneSfntGlyphPoint *p2) {
    return (Vec2){
        .x = (p1->x + p2->x) / 2.0f,
        .y = (p1->y + p2->y) / 2.0f,
    };
}

struct PoneSdfData {
    usize width;
    usize height;
    u32 *sdf_buf;
    f32 *d_mins;
    i8 *delta_windings;
    u32 d_pad;
    u32 d_max;
};

struct PoneSfntGlyphContourEdgeIterator {
    PoneSfntSimpleGlyph *glyph;
    usize contour_index;
    usize point_index;
    Vec2 closing_point;
    b8 closing_point_is_end;
    b8 was_on_curve;
    PoneSfntGlyphPointMapConstants *map_constants;
};

static b8 pone_sfnt_glyph_contour_next_edge_segment(
    PoneSfntGlyphContourEdgeIterator *iter,
    PoneTrueTypeEdgeSegment *edge_segment) {
    usize first_point_index = 0;
    if (iter->contour_index > 0) {
        first_point_index =
            (usize)(iter->glyph
                        ->end_points_of_contours[iter->contour_index - 1]) +
            1;
    }
    usize contour_point_count =
        (usize)(iter->glyph->end_points_of_contours[iter->contour_index]) + 1 -
        first_point_index;

    if (iter->point_index >= contour_point_count + 1) {
        return 0;
    }

    if (iter->point_index == 0) {
        PoneSfntGlyphPoint *first_point =
            (iter->glyph->points + first_point_index);
        PoneSfntGlyphPoint *last_point =
            (iter->glyph->points + first_point_index + contour_point_count - 1);
        iter->was_on_curve = first_point->on_curve;
        if (first_point->on_curve) {
            iter->closing_point = pone_sfnt_glyph_point_vec2(first_point);
            if (iter->map_constants) {
                pone_sfnt_glyph_point_map(&iter->closing_point,
                                          iter->map_constants);
            }
        } else {
            if (last_point->on_curve) {
                iter->closing_point = pone_sfnt_glyph_point_vec2(last_point);
                if (iter->map_constants) {
                    pone_sfnt_glyph_point_map(&iter->closing_point,
                                              iter->map_constants);
                }
                iter->closing_point_is_end = 1;
            } else {
                iter->closing_point =
                    pone_sfnt_glyph_point_mid_vec2(first_point, last_point);
                if (iter->map_constants) {
                    pone_sfnt_glyph_point_map(&iter->closing_point,
                                              iter->map_constants);
                }
            }
            edge_segment->points[1] = pone_sfnt_glyph_point_vec2(first_point);
            if (iter->map_constants) {
                pone_sfnt_glyph_point_map(&edge_segment->points[1],
                                          iter->map_constants);
            }
        }
        edge_segment->points[0] = iter->closing_point;
        if (iter->closing_point_is_end) {
            pone_assert(0);
            contour_point_count--;
        }
        iter->point_index++;
    }

    if (iter->point_index > 1 && iter->point_index < contour_point_count) {
        PoneSfntGlyphPoint *point =
            iter->glyph->points + first_point_index + iter->point_index;
        Vec2 p = pone_sfnt_glyph_point_vec2(point);
        if (iter->map_constants) {
            pone_sfnt_glyph_point_map(&p, iter->map_constants);
        }

        edge_segment->points[0] =
            edge_segment->points[edge_segment->point_count - 1];
        if (!iter->was_on_curve && !point->on_curve &&
            iter->point_index < contour_point_count) {

            edge_segment->points[1] = p;

            iter->point_index++;
        } else {
            iter->was_on_curve = 1;
        }
    }

    while (iter->point_index < contour_point_count) {
        PoneSfntGlyphPoint *point =
            iter->glyph->points + first_point_index + iter->point_index;
        Vec2 p = pone_sfnt_glyph_point_vec2(point);
        if (iter->map_constants) {
            pone_sfnt_glyph_point_map(&p, iter->map_constants);
        }
        if (iter->was_on_curve && point->on_curve) {
            edge_segment->points[1] = p;
            edge_segment->point_count = 2;

            iter->point_index++;
            return 1;
        } else if (iter->was_on_curve && !point->on_curve) {
            edge_segment->points[1] = p;
            iter->was_on_curve = 0;

            iter->point_index++;
        } else if (!iter->was_on_curve && point->on_curve) {
            edge_segment->points[2] = p;
            edge_segment->point_count = 3;

            iter->was_on_curve = 1;
            iter->point_index++;
            return 1;
        } else {
            edge_segment->points[2] = pone_vec2_mul_scalar(
                0.5f, pone_vec2_add(edge_segment->points[1], p));
            edge_segment->point_count = 3;

            return 1;
        }
    }

    if (iter->was_on_curve) {
        edge_segment->points[1] = iter->closing_point;
        edge_segment->point_count = 2;

        iter->point_index++;
        return 1;
    } else {
        edge_segment->points[2] = iter->closing_point;
        edge_segment->point_count = 3;

        iter->point_index++;
        return 1;
    }
}

static void pone_sfnt_simple_glyph_bounding_box(
    PoneSfntSimpleGlyph *glyph, PoneSfntGlyphPointMapConstants *map_constants,
    PoneRectF32 *bbox) {
    *bbox = (PoneRectF32){
        .p_min =
            {
                .x = PONE_F32_MAX,
                .y = PONE_F32_MAX,
            },
        .p_max =
            {
                .x = PONE_F32_MIN,
                .y = PONE_F32_MIN,
            },
    };

    for (usize contour_index = 0;
         contour_index < glyph->end_points_of_contour_count; contour_index++) {
        PoneSfntGlyphContourEdgeIterator iter = {
            .glyph = glyph,
            .contour_index = contour_index,
            .map_constants = map_constants,
        };

        PoneTrueTypeEdgeSegment edge_segment;
        while (
            pone_sfnt_glyph_contour_next_edge_segment(&iter, &edge_segment)) {
            PoneRectF32 edge_segment_bbox;

            pone_truetype_edge_segment_bbox(&edge_segment, &edge_segment_bbox);
            bbox->p_min.x = PONE_MIN(bbox->p_min.x, edge_segment_bbox.p_min.x);
            bbox->p_min.y = PONE_MIN(bbox->p_min.y, edge_segment_bbox.p_min.y);
            bbox->p_max.x = PONE_MAX(bbox->p_max.x, edge_segment_bbox.p_max.x);
            bbox->p_max.y = PONE_MAX(bbox->p_max.y, edge_segment_bbox.p_max.y);
        }
    }
}

static void pone_sfnt_glyph_bounding_box(PoneTrueTypeFont *font,
                                         PoneSfntGlyph *glyph, PoneRectF32 *bbox) {
    switch (glyph->type) {
    case PONE_SFNT_GLYPH_TYPE_SIMPLE: {
        pone_sfnt_simple_glyph_bounding_box(&glyph->simple, 0, bbox);
    } break;
    case PONE_SFNT_GLYPH_TYPE_COMPOUND: {
        *bbox = {
            .p_min =
                {
                    .x = PONE_F32_MAX,
                    .y = PONE_F32_MAX,
                },
            .p_max =
                {
                    .x = PONE_F32_MIN,
                    .y = PONE_F32_MIN,
                },
        };
        for (usize glyph_component_index = 0;
             glyph_component_index < glyph->compound.component_glyph_count;
             ++glyph_component_index) {
            PoneSfntComponentGlyph *component_glyph_ref =
                glyph->compound.component_glyphs + glyph_component_index;
            PoneSfntGlyph *component_glyph =
                font->glyphs + component_glyph_ref->glyph_index;
            PoneSfntGlyphPointTransformation transform = {
                .offset = component_glyph_ref->offset,
                .scale = component_glyph_ref->transformation};
            PoneSfntGlyphPointMapConstants map_constants = {
                .range_map = 0,
                .transform = &transform,
            };
            pone_assert(component_glyph->type == PONE_SFNT_GLYPH_TYPE_SIMPLE);
            PoneRectF32 component_bbox;
            pone_sfnt_simple_glyph_bounding_box(
                &component_glyph->simple, &map_constants, &component_bbox);
            bbox->p_min.x = PONE_MIN(bbox->p_min.x, component_bbox.p_min.x);
            bbox->p_min.y = PONE_MIN(bbox->p_min.y, component_bbox.p_min.y);
            bbox->p_max.x = PONE_MAX(bbox->p_max.x, component_bbox.p_max.x);
            bbox->p_max.y = PONE_MAX(bbox->p_max.y, component_bbox.p_max.y);
        }
        break;
    }
    }
}

static void pone_sfnt_simple_glyph_calculate_sdf(
    PoneSfntSimpleGlyph *glyph, PoneSfntGlyphPointMapConstants *map_constants,
    PoneSdfData *sdf_data) {
    usize contour_begin_point_index = 0;
    for (usize contour_index = 0;
         contour_index < glyph->end_points_of_contour_count; ++contour_index) {
        usize contour_point_count =
            (usize)glyph->end_points_of_contours[contour_index] + 1 -
            contour_begin_point_index;

        PoneTrueTypeEdgeSegment candidate_edge_segment;
        Vec2 closing_point;
        b8 closing_point_is_end = 0;

        PoneSfntGlyphPoint *contour_begin_point =
            (glyph->points + contour_begin_point_index);
        PoneSfntGlyphPoint *contour_end_point =
            (glyph->points + contour_begin_point_index + contour_point_count -
             1);

        b8 was_on_curve = contour_begin_point->on_curve;
        if (contour_begin_point->on_curve) {
            closing_point = pone_sfnt_glyph_point_vec2(contour_begin_point);
            pone_sfnt_glyph_point_map(&closing_point, map_constants);
        } else {
            if (contour_end_point->on_curve) {
                closing_point = pone_sfnt_glyph_point_vec2(contour_end_point);
                pone_sfnt_glyph_point_map(&closing_point, map_constants);
                closing_point_is_end = 1;
            } else {
                closing_point = pone_sfnt_glyph_point_mid_vec2(
                    contour_begin_point, contour_end_point);
                pone_sfnt_glyph_point_map(&closing_point, map_constants);
            }
            candidate_edge_segment.points[1] =
                pone_sfnt_glyph_point_vec2(contour_begin_point);
            pone_sfnt_glyph_point_map(&candidate_edge_segment.points[1],
                                      map_constants);
        }
        candidate_edge_segment.points[0] = closing_point;
        if (closing_point_is_end) {
            --contour_point_count;
        }

        for (usize contour_point_index = 1;
             contour_point_index < contour_point_count; ++contour_point_index) {
            PoneSfntGlyphPoint *contour_point =
                glyph->points + contour_begin_point_index + contour_point_index;
            Vec2 p_contour = pone_sfnt_glyph_point_vec2(contour_point);
            pone_sfnt_glyph_point_map(&p_contour, map_constants);

            if (was_on_curve && contour_point->on_curve) {
                candidate_edge_segment.points[1] = p_contour;
                candidate_edge_segment.point_count = 2;
            } else if (was_on_curve && !contour_point->on_curve) {
                candidate_edge_segment.points[1] = p_contour;

                was_on_curve = 0;
                continue;
            } else if (!was_on_curve && contour_point->on_curve) {
                candidate_edge_segment.points[2] = p_contour;
                candidate_edge_segment.point_count = 3;
            } else {
                candidate_edge_segment.points[2] = pone_vec2_mul_scalar(
                    0.5f,
                    pone_vec2_add(candidate_edge_segment.points[1], p_contour));
                candidate_edge_segment.point_count = 3;
            }

            PoneRectF32 edge_segment_bbox;
            pone_truetype_edge_segment_bbox(&candidate_edge_segment,
                                            &edge_segment_bbox);

            PoneRectF32 edge_segment_bbox_padded = {
                .p_min =
                    {
                        .x = edge_segment_bbox.p_min.x - sdf_data->d_pad,
                        .y = edge_segment_bbox.p_min.y - sdf_data->d_pad,
                    },
                .p_max =
                    {
                        .x = edge_segment_bbox.p_max.x + sdf_data->d_pad,
                        .y = edge_segment_bbox.p_max.y + sdf_data->d_pad,
                    },
            };
            u32 x_min_u32 = (u32)pone_floor(edge_segment_bbox_padded.p_min.x);
            u32 y_min_u32 = (u32)pone_floor(edge_segment_bbox_padded.p_min.y);
            u32 x_max_u32 = (u32)pone_ceil(edge_segment_bbox_padded.p_max.x -
                                           sdf_data->width * PONE_EPSILON);
            u32 y_max_u32 = (u32)pone_ceil(edge_segment_bbox_padded.p_max.y -
                                           sdf_data->height * PONE_EPSILON);
            pone_assert(x_min_u32 >= 0 && x_max_u32 <= sdf_data->width);
            pone_assert(y_min_u32 >= 0 && y_max_u32 <= sdf_data->height);

            for (u32 y = y_min_u32; y < y_max_u32; ++y) {
                f32 *d_mins_row = sdf_data->d_mins + (y * sdf_data->width);
                i8 *delta_windings_row =
                    sdf_data->delta_windings + (y * sdf_data->width);

                i8 prev_side = 0;
                for (u32 x = x_min_u32; x < x_max_u32; ++x) {
                    Vec2 p_px = {.x = x + 0.5f, .y = y + 0.5f};
                    f32 *d_min = d_mins_row + x;
                    i8 *delta_winding = delta_windings_row + x;
                    b8 side;
                    f32 d = pone_truetype_edge_segment_calculate_distance(
                        &candidate_edge_segment, p_px, &side);
                    if (pone_truetype_between_closed_open(p_px,
                                                          &edge_segment_bbox)) {
                        if (prev_side == -1 && !side) {
                            *delta_winding += 1;
                        } else if (prev_side == 1 && side) {
                            *delta_winding += -1;
                        }
                    }
                    prev_side = side ? -1 : 1;
                    if (pone_abs(d) < pone_abs(*d_min)) {
                        *d_min = d;
                    }
                }
            }

            candidate_edge_segment.points[0] =
                candidate_edge_segment
                    .points[candidate_edge_segment.point_count - 1];
            if (!was_on_curve && !contour_point->on_curve) {
                candidate_edge_segment.points[1] = p_contour;
            } else {
                was_on_curve = 1;
            }
        }

        if (was_on_curve) {
            candidate_edge_segment.points[1] = closing_point;
            candidate_edge_segment.point_count = 2;
        } else {
            candidate_edge_segment.points[2] = closing_point;
            candidate_edge_segment.point_count = 3;
        }

        PoneRectF32 edge_segment_bbox;
        pone_truetype_edge_segment_bbox(&candidate_edge_segment,
                                        &edge_segment_bbox);
        PoneRectF32 edge_segment_bbox_padded = {
            .p_min =
                {
                    .x = edge_segment_bbox.p_min.x - sdf_data->d_pad,
                    .y = edge_segment_bbox.p_min.y - sdf_data->d_pad,
                },
            .p_max =
                {
                    .x = edge_segment_bbox.p_max.x + sdf_data->d_pad,
                    .y = edge_segment_bbox.p_max.y + sdf_data->d_pad,
                },
        };
        u32 x_min_u32 = (u32)pone_floor(edge_segment_bbox_padded.p_min.x);
        u32 y_min_u32 = (u32)pone_floor(edge_segment_bbox_padded.p_min.y);
        u32 x_max_u32 = (u32)pone_ceil(edge_segment_bbox_padded.p_max.x -
                                       sdf_data->width * PONE_EPSILON);
        u32 y_max_u32 = (u32)pone_ceil(edge_segment_bbox_padded.p_max.y -
                                       sdf_data->height * PONE_EPSILON);
        pone_assert(x_min_u32 >= 0 && x_max_u32 <= sdf_data->width);
        pone_assert(y_min_u32 >= 0 && y_max_u32 <= sdf_data->height);

        for (u32 y = y_min_u32; y < y_max_u32; ++y) {
            f32 *d_mins_row = sdf_data->d_mins + (y * sdf_data->width);
            i8 *delta_windings_row =
                sdf_data->delta_windings + (y * sdf_data->width);

            i8 prev_side = 0;
            for (u32 x = x_min_u32; x < x_max_u32; ++x) {
                Vec2 p_px = {.x = x + 0.5f, .y = y + 0.5f};
                f32 *d_min = d_mins_row + x;
                i8 *delta_winding = delta_windings_row + x;
                b8 side;
                f32 d = pone_truetype_edge_segment_calculate_distance(
                    &candidate_edge_segment, p_px, &side);
                if (pone_truetype_between_closed_open(p_px,
                                                      &edge_segment_bbox)) {
                    if (prev_side == -1 && !side) {
                        *delta_winding += 1;
                    } else if (prev_side == 1 && side) {
                        *delta_winding += -1;
                    }
                }
                prev_side = side ? -1 : 1;
                if (pone_abs(d) < pone_abs(*d_min)) {
                    *d_min = d;
                }
            }
        }
        contour_begin_point_index =
            (usize)glyph->end_points_of_contours[contour_index] + 1;
    }
    for (u32 y = 0; y < sdf_data->height; ++y) {
        i8 winding_score = 0;
        for (u32 x = 0; x < sdf_data->width; ++x) {
            i8 delta_winding =
                sdf_data->delta_windings[(y * sdf_data->width) + x];
            f32 *d_min = &sdf_data->d_mins[(y * sdf_data->width) + x];
            u32 *sdf_pixel = &sdf_data->sdf_buf[(y * sdf_data->width) + x];

            winding_score += delta_winding;

            if (winding_score) {
                *d_min = pone_f32_copysign(*d_min, 1.0f);
            } else {
                *d_min = pone_f32_copysign(*d_min, -1.0f);
            }

            f32 d_min_z = (*d_min + sdf_data->d_max) / (2.0f * sdf_data->d_max);
            d_min_z = PONE_CLAMP(d_min_z, 0.0f, 1.0f);

            u8 gray_value = (u8)(d_min_z * 255.0f);
            *sdf_pixel = (u32)gray_value << 24 | (u32)gray_value << 16 |
                         (u32)gray_value << 8 | 0x000000FFu;
        }
    }
}

void pone_truetype_font_generate_sdf(PoneTrueTypeFont *font, u32 resolution,
                                     u32 d_pad, Arena *permanent_arena,
                                     Arena *transient_arena,
                                     PoneTrueTypeSdfAtlas *atlas) {
#if 0
    atlas->glyph_count = 1;
    u32 char_code = 89;
    u32 *char_codes = &char_code;
#else
    atlas->glyph_count = 106;
    u32 *char_codes =
        arena_alloc_array(transient_arena, atlas->glyph_count, u32);
    // ASCII chars
    for (usize i = 0; i < 94; ++i) {
        char_codes[i] = i + 33;
    }
    char_codes[94] = 0x87c3;  // Ç
    char_codes[95] = 0x96c3;  // Ö
    char_codes[96] = 0x9cc3;  // Ü
    char_codes[97] = 0xa7c3;  // ç
    char_codes[98] = 0xb6c3;  // ö
    char_codes[99] = 0xbcc3;  // ü
    char_codes[100] = 0x9ec4; // Ğ
    char_codes[101] = 0x9fc4; // ğ
    char_codes[102] = 0xb0c4; // İ
    char_codes[103] = 0xb1c4; // ı
    char_codes[104] = 0x9ec5; // Ş
    char_codes[105] = 0x9fc5; // ş
#endif
    atlas->glyph_rects =
        arena_alloc_array(permanent_arena, atlas->glyph_count, PoneRectU32);
    atlas->glyph_bboxes = arena_alloc_array(permanent_arena, atlas->glyph_count, PoneRectF32);

    u32 *glyph_ids =
        arena_alloc_array(transient_arena, atlas->glyph_count, u32);

    usize i = 0;
    usize j = 0;
    while (i < atlas->glyph_count && j < font->format_12.group_count) {
        u32 *utf8_char = char_codes + i;
        PoneSfntSequentialMapGroup *group = font->format_12.groups + j;

        u32 char_code = 0;
        u8 *utf8_byte = (u8 *)utf8_char;
        if (((*utf8_byte) & 0x80) == 0) {
            char_code = (u32)(*utf8_byte);
        } else if (((*utf8_byte) & 0xe0) == 0xc0) {
            char_code |= (*utf8_byte & 0x1f) << 6;
            utf8_byte++;
            char_code |= ((u32)(*utf8_byte) & 0x3f);
        } else if (((*utf8_byte) & 0xf0) == 0xe0) {
            pone_assert(0 && "unimplemented");
        } else if (((*utf8_byte) & 0xf8) == 0xf0) {
            pone_assert(0 && "unimplemented");
        } else {
            pone_assert(0 && "invalid utf8 char");
        }

        if (char_code >= group->start_char && char_code <= group->end_char) {
            glyph_ids[i] =
                group->start_glyph_id + (char_code - group->start_char);
            ++i;
            if (char_code == group->end_char) {
                ++j;
            }
        } else if (char_code > group->end_char) {
            ++j;
        } else {
            pone_assert(0 && "missed one");
        }
    }

    u32 d_max = d_pad;

    u32 content_bitmap_size = resolution - d_pad * 2;
    f32 pixels_per_funit = (f32)content_bitmap_size / (f32)font->units_per_em;

    PoneRectPackItem *sdf_bitmap_pack_items = arena_alloc_array(
        transient_arena, atlas->glyph_count, PoneRectPackItem);
    usize *pack_item_glyph_indices =
        arena_alloc_array(transient_arena, atlas->glyph_count, usize);
    for (usize glyph_index = 0; glyph_index < atlas->glyph_count;
         glyph_index++) {
        pack_item_glyph_indices[glyph_index] = glyph_index;
        sdf_bitmap_pack_items[glyph_index] = {
            .user_data = (void *)(pack_item_glyph_indices + glyph_index),
        };
    }

    PoneRectF32 *glyph_bboxes = arena_alloc_array(transient_arena, atlas->glyph_count, PoneRectF32);
    for (usize glyph_index = 0; glyph_index < atlas->glyph_count;
         ++glyph_index) {
        u32 glyph_id = glyph_ids[glyph_index];
        PoneSfntGlyph *glyph = font->glyphs + glyph_id;
        PoneRectF32 *glyph_bbox = glyph_bboxes + glyph_index;

        pone_sfnt_glyph_bounding_box(font, glyph, glyph_bbox);
        atlas->glyph_bboxes[glyph_index] = *glyph_bbox;

        glyph_bbox->p_min = pone_vec2_mul_scalar(pixels_per_funit, glyph_bbox->p_min);
        glyph_bbox->p_max = pone_vec2_mul_scalar(pixels_per_funit, glyph_bbox->p_max);
        glyph_bbox->p_min.x -= d_pad;
        glyph_bbox->p_min.y -= d_pad;
        glyph_bbox->p_max.x += d_pad;
        glyph_bbox->p_max.y += d_pad;

        u32 glyph_width = (u32)pone_ceil(glyph_bbox->p_max.x - glyph_bbox->p_min.x);
        u32 glyph_height = (u32)pone_ceil(glyph_bbox->p_max.y - glyph_bbox->p_min.y);
        sdf_bitmap_pack_items[glyph_index].rect = {
            .x_min = 0,
            .y_min = 0,
            .x_max = glyph_width,
            .y_max = glyph_height,
        };
    }
    u32 side;
    b8 packed = pone_rect_pack(sdf_bitmap_pack_items, atlas->glyph_count, 2048,
                               transient_arena, &side);
    pone_assert(packed);
    for (usize rect_pack_item_index = 0;
         rect_pack_item_index < atlas->glyph_count; rect_pack_item_index++) {
        PoneRectPackItem *rect_pack_item =
            sdf_bitmap_pack_items + rect_pack_item_index;
        usize glyph_index = *(usize *)rect_pack_item->user_data;
        atlas->glyph_rects[glyph_index] = rect_pack_item->rect;
    }

    atlas->width = side;
    atlas->height = side;
    atlas->buf =
        arena_alloc_array(permanent_arena, atlas->width * atlas->height, u32);
    u32 **sdf_bufs =
        arena_alloc_array(transient_arena, atlas->glyph_count, u32 *);
    for (usize glyph_index = 0; glyph_index < atlas->glyph_count;
         glyph_index++) {
        u32 **sdf_buf = sdf_bufs + glyph_index;
        PoneRectU32 *glyph_rect = atlas->glyph_rects + glyph_index;
        u32 glyph_width = pone_rect_u32_width(glyph_rect);
        u32 glyph_height = pone_rect_u32_height(glyph_rect);

        *sdf_buf = arena_alloc_array(
            transient_arena, (usize)glyph_width * (usize)glyph_height, u32);
    }

    for (usize glyph_id_index = 0; glyph_id_index < atlas->glyph_count;
         ++glyph_id_index) {
        u32 glyph_id = glyph_ids[glyph_id_index];
        PoneRectU32 *glyph_rect = atlas->glyph_rects + glyph_id_index;
        u32 glyph_width = pone_rect_u32_width(glyph_rect);
        u32 glyph_height = pone_rect_u32_height(glyph_rect);
        PoneSfntGlyph *glyph = font->glyphs + glyph_id;

        usize transient_arena_offset = transient_arena->offset;
        f32 *d_mins =
            arena_alloc_array(transient_arena, glyph_width * glyph_height, f32);
        for (usize i = 0; i < glyph_height; ++i) {
            for (usize j = 0; j < glyph_width; ++j) {
                d_mins[(i * glyph_width) + j] = -(f32)(d_pad * d_pad);
            }
        }
        i8 *delta_windings =
            arena_alloc_array(transient_arena, glyph_width * glyph_height, i8);
        pone_memset(delta_windings, 0, glyph_width * glyph_height * sizeof(i8));
        PoneRectF32 range_b = {
            .p_min =
                {
                    .x = 0.0f,
                    .y = 0.0f,
                },
            .p_max =
                {
                    .x = (f32)glyph_width,
                    .y = (f32)glyph_height,
                },
        };
        PoneSdfData sdf_data = {
            .width = glyph_width,
            .height = glyph_height,
            .sdf_buf = sdf_bufs[glyph_id_index],
            .d_mins = d_mins,
            .delta_windings = delta_windings,
            .d_pad = d_pad,
            .d_max = d_max,
        };
        PoneSfntGlyphPointRangeMap range_map = {
            .scale = pixels_per_funit,
            .range_a = glyph_bboxes + glyph_id_index,
            .range_b = &range_b,
            .invert_y = 1,
        };

        if (glyph->type == PONE_SFNT_GLYPH_TYPE_SIMPLE) {
            PoneSfntGlyphPointMapConstants map_constants = {
                .range_map = &range_map,
                .transform = 0,
            };
            pone_sfnt_simple_glyph_calculate_sdf(&glyph->simple, &map_constants,
                                                 &sdf_data);
        } else {
            for (usize glyph_component_index = 0;
                 glyph_component_index < glyph->compound.component_glyph_count;
                 ++glyph_component_index) {
                PoneSfntComponentGlyph *component_glyph_ref =
                    glyph->compound.component_glyphs + glyph_component_index;
                PoneSfntGlyph *component_glyph =
                    font->glyphs + component_glyph_ref->glyph_index;
                pone_assert(component_glyph->type ==
                            PONE_SFNT_GLYPH_TYPE_SIMPLE);
                PoneSfntGlyphPointTransformation transform = {
                    .offset = component_glyph_ref->offset,
                    .scale = component_glyph_ref->transformation};
                PoneSfntGlyphPointMapConstants map_constants = {
                    .range_map = &range_map,
                    .transform = &transform,
                };
                pone_sfnt_simple_glyph_calculate_sdf(&component_glyph->simple,
                                                     &map_constants, &sdf_data);
            }
        }
        transient_arena->offset = transient_arena_offset;
    }

    for (usize glyph_index = 0; glyph_index < atlas->glyph_count;
         glyph_index++) {
        PoneRectU32 *glyph_rect = atlas->glyph_rects + glyph_index;
        u32 glyph_width = pone_rect_u32_width(glyph_rect);
        u32 glyph_height = pone_rect_u32_height(glyph_rect);
        u32 *sdf_buf = sdf_bufs[glyph_index];

        for (usize y = 0; y < glyph_height; y++) {
            pone_memcpy((void *)(atlas->buf +
                                 ((glyph_rect->y_min + y) * atlas->width) +
                                 glyph_rect->x_min),
                        (void *)(sdf_buf + (y * glyph_width)),
                        glyph_width * sizeof(u32));
        }
    }
}
