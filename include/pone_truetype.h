#ifndef PONE_TRUETYPE_H
#define PONE_TRUETYPE_H

#include "pone_arena.h"
#include "pone_mat2.h"
#include "pone_rect.h"
#include "pone_types.h"
#include "pone_vec2.h"

struct PoneTruetypeInput {
    void *data;
    usize length;
};

enum PoneSfntGlyphType {
    PONE_SFNT_GLYPH_TYPE_SIMPLE,
    PONE_SFNT_GLYPH_TYPE_COMPOUND,
};

struct PoneSfntGlyphBbox {
    i16 x_min;
    i16 y_min;
    i16 x_max;
    i16 y_max;
};

i16 pone_sfnt_glyph_bbox_width(PoneSfntGlyphBbox *bbox);

i16 pone_sfnt_glyph_bbox_height(PoneSfntGlyphBbox *bbox);

struct PoneSfntGlyphPoint {
    b8 on_curve;
    i16 x;
    i16 y;
};

struct PoneSfntSimpleGlyph {
    usize end_points_of_contour_count;
    u16 *end_points_of_contours;
    usize point_count;
    PoneSfntGlyphPoint *points;
};

struct PoneSfntComponentGlyph {
    u16 flags;
    u16 glyph_index;
    Vec2 offset;
    Mat2 transformation;
};

struct PoneSfntCompoundGlyph {
    usize component_glyph_count;
    PoneSfntComponentGlyph *component_glyphs;
};

struct PoneSfntGlyph {
    PoneSfntGlyphType type;
    PoneSfntGlyphBbox bbox;
    union {
        PoneSfntSimpleGlyph simple;
        PoneSfntCompoundGlyph compound;
    };
};

struct PoneSfntSequentialMapGroup {
    u32 start_char;
    u32 end_char;
    u32 start_glyph_id;
};

struct PoneSfntCmapFormat12 {
    u32 length;
    u32 language;
    usize group_count;
    PoneSfntSequentialMapGroup *groups;
};

struct PoneTrueTypeFont {
    u16 units_per_em;
    usize glyph_count;
    PoneSfntGlyphBbox global_bbox;
    PoneSfntGlyph *glyphs;
    PoneSfntCmapFormat12 format_12;
};

struct PoneTrueTypeSdfAtlas {
    u32 *buf;
    usize width;
    usize height;
    usize glyph_count;
    PoneRectU32 *glyph_rects;
    PoneRectF32 *glyph_bboxes;
};

PoneTrueTypeFont *pone_truetype_parse(PoneTruetypeInput input, Arena *arena);
void pone_truetype_font_generate_sdf(PoneTrueTypeFont *font, u32 resolution,
                                     u32 d_pad, Arena *permanent_arena,
                                     Arena *transient_arena,
                                     PoneTrueTypeSdfAtlas *atlas);

#endif
