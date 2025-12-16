#include "pone_json.h"

#include "pone_arena.h"
#include "pone_assert.h"
#include "pone_string.h"

PoneJsonValue *pone_json_scanner_parse_value(PoneJsonScanner *s, Arena *arena);
static void _pone_json_scanner_parse_value(PoneJsonScanner *s, Arena *arena,
                                           PoneJsonValue *value);

static u8 pone_json_scanner_peek(PoneJsonScanner *s) {
    return s->input.buf[s->cursor];
}

static b8 pone_json_scanner_is_eof(PoneJsonScanner *s) {
    return s->cursor >= s->input.len;
}

static void pone_json_scanner_skip_whitespace(PoneJsonScanner *s) {
    u8 b;
    while (!pone_json_scanner_is_eof(s) &&
           ((b = pone_json_scanner_peek(s)) == '\t' || b == '\n' || b == '\r' ||
            b == ' ')) {
        s->cursor++;
    }
}

static void pone_json_scanner_parse_string(PoneJsonScanner *s, Arena *arena,
                                           PoneString *string) {
    string->buf = 0;
    string->len = 0;
    usize start = ++s->cursor;
    while (!pone_json_scanner_is_eof(s) && pone_json_scanner_peek(s) != '"') {
        s->cursor++;
    }
    usize len = s->cursor - start;
    if (len == 0) {
        return;
    }
    // u8 *buf = (u8 *)arena_alloc(arena, len);
    // PONE_ASSERT(buf);
    // pone_memcpy((void *)buf, (void *)((usize)s->input.buf + start), len);
    string->buf = s->input.buf + start;
    string->len = len;

    ++s->cursor;
}

static void pone_json_scanner_parse_number(PoneJsonScanner *s, b8 is_positive,
                                           f64 *number) {
    PONE_ASSERT(number);
    *number = 0.0;
    b8 is_fraction = 0;
    b8 is_exponent = 0;
    u32 fraction_digit_count = 0;

    for (;;) {
        PONE_ASSERT(!pone_json_scanner_is_eof(s));
        u8 b = pone_json_scanner_peek(s);
        if (b >= '0' && b <= '9') {
            if (!is_fraction) {
                *number = (*number * 10.0) + (f64)(b - '0');
                ++s->cursor;
            } else if (!is_exponent) {
                ++fraction_digit_count;
                f64 frac = 1.0;
                for (u32 i = 0; i < fraction_digit_count; i++) {
                    frac /= 10.0;
                }
                *number += (f64)(b - '0') * frac;
                ++s->cursor;
            } else {
                PONE_ASSERT(0);
            }
        } else if (b == '.') {
            PONE_ASSERT(!is_fraction);
            is_fraction = 1;
            ++s->cursor;
        } else if (is_fraction && (b == 'e' || b == 'E')) {
            is_exponent = 1;
            ++s->cursor;
        } else {
            break;
        }
    }

    if (!is_positive) {
        *number = -(*number);
    }
}

static void pone_json_scanner_parse_object(PoneJsonScanner *s, Arena *arena,
                                           PoneJsonObject *object) {
    ++s->cursor;
    pone_json_scanner_skip_whitespace(s);
    u8 b;
    PoneJsonPair **pair = &object->pairs;
    while (!pone_json_scanner_is_eof(s) &&
           (b = pone_json_scanner_peek(s)) != '}') {
        if (b == ',') {
            ++s->cursor;
            pone_json_scanner_skip_whitespace(s);
        }
        *pair = (PoneJsonPair *)arena_alloc(arena, sizeof(PoneJsonPair));
        PONE_ASSERT(*pair);
        pone_json_scanner_parse_string(s, arena, &(*pair)->name);
        PONE_ASSERT(pone_json_scanner_peek(s) == ':');
        ++s->cursor;
        (*pair)->value =
            (PoneJsonValue *)arena_alloc(arena, sizeof(PoneJsonValue));
        PONE_ASSERT((*pair)->value);
        pone_json_scanner_skip_whitespace(s);
        _pone_json_scanner_parse_value(s, arena, (*pair)->value);
        pone_json_scanner_skip_whitespace(s);
        pair = &(*pair)->next;
        ++object->count;
    }

    ++s->cursor;
}

static void pone_json_scanner_parse_array(PoneJsonScanner *s, Arena *arena,
                                          PoneJsonArray *array) {
    ++s->cursor;
    pone_json_scanner_skip_whitespace(s);
    u8 b;
    PoneJsonArrayValue **array_value = &array->values;
    while (!pone_json_scanner_is_eof(s) &&
           (b = pone_json_scanner_peek(s)) != ']') {
        if (b == ',') {
            ++s->cursor;
            pone_json_scanner_skip_whitespace(s);
        }
        *array_value = (PoneJsonArrayValue *)arena_alloc(
            arena, sizeof(PoneJsonArrayValue));
        PONE_ASSERT(*array_value);
        (*array_value)->value =
            (PoneJsonValue *)arena_alloc(arena, sizeof(PoneJsonValue));
        PONE_ASSERT((*array_value)->value);
        (*array_value)->next = 0;
        _pone_json_scanner_parse_value(s, arena, (*array_value)->value);
        pone_json_scanner_skip_whitespace(s);
        array_value = &(*array_value)->next;
        ++array->count;
    }

    ++s->cursor;
}

PoneJsonValue *pone_json_scanner_parse_value(PoneJsonScanner *s, Arena *arena) {
    PoneJsonValue *value =
        (PoneJsonValue *)arena_alloc(arena, sizeof(PoneJsonValue));
    PONE_ASSERT(value);
    _pone_json_scanner_parse_value(s, arena, value);

    return value;
}

// PoneJsonValue *pone_json_scanner_parse_value(PoneJsonScanner *s, Arena
// *arena) {
//     usize start = arena->offset;
//     PoneJsonValue *value =
//         (PoneJsonValue *)arena_alloc(arena, sizeof(PoneJsonValue));
//
//     for (;;) {
//         if (pone_json_scanner_is_eof(s)) {
//             break;
//         }
//         b8 b = pone_json_scanner_peek(s);
//         switch (b) {
//         case '"': {
//             value->type = PONE_JSON_TYPE_STRING;
//             pone_json_scanner_parse_string(s, arena, &value->string);
//
//             return value;
//         } break;
//         case '0':
//         case '1':
//         case '2':
//         case '3':
//         case '4':
//         case '5':
//         case '6':
//         case '7':
//         case '8':
//         case '9': {
//             value->type = PONE_JSON_TYPE_NUMBER;
//             pone_json_scanner_parse_number(s, 1, &value->number);
//
//             return value;
//         } break;
//         case '-': {
//             ++s->cursor;
//             value->type = PONE_JSON_TYPE_NUMBER;
//             pone_json_scanner_parse_number(s, 1, &value->number);
//
//             return value;
//         } break;
//         case '{': {
//             value->type = PONE_JSON_TYPE_OBJECT;
//             pone_json_scanner_parse_object(s, arena, &value->object);
//
//             return value;
//         } break;
//         case '[': {
//             value->type = PONE_JSON_TYPE_ARRAY;
//             pone_json_scanner_parse_array(s, arena, &value->array);
//
//             return value;
//         } break;
//         case 't': {
//             if (s->input.buf[++s->cursor] != 'r' ||
//                 s->input.buf[++s->cursor] != 'u' ||
//                 s->input.buf[++s->cursor] != 'e') {
//                 PONE_ASSERT(0 && "Invalid boolean");
//             }
//             value->type = PONE_JSON_TYPE_TRUE;
//             ++s->cursor;
//
//             return value;
//         } break;
//         case 'f': {
//             if (s->input.buf[++s->cursor] != 'a' ||
//                 s->input.buf[++s->cursor] != 'l' ||
//                 s->input.buf[++s->cursor] != 's' ||
//                 s->input.buf[++s->cursor] != 'e') {
//                 PONE_ASSERT(0 && "Invalid boolean");
//             }
//             value->type = PONE_JSON_TYPE_FALSE;
//             ++s->cursor;
//
//             return value;
//         } break;
//         case 'n': {
//             if (s->input.buf[++s->cursor] != 'u' ||
//                 s->input.buf[++s->cursor] != 'l' ||
//                 s->input.buf[++s->cursor] != 'l') {
//                 PONE_ASSERT(0 && "Invalid null");
//             }
//             value->type = PONE_JSON_TYPE_NULL;
//             ++s->cursor;
//
//             return value;
//         } break;
//         }
//
//         ++s->cursor;
//     }
//
//     arena->offset = start;
//     return 0;
// }

static void _pone_json_scanner_parse_value(PoneJsonScanner *s, Arena *arena,
                                           PoneJsonValue *value) {
    PONE_ASSERT(!pone_json_scanner_is_eof(s));
    b8 b = pone_json_scanner_peek(s);
    switch (b) {
    case '"': {
        value->type = PONE_JSON_TYPE_STRING;
        pone_json_scanner_parse_string(s, arena, &value->string);
    } break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': {
        value->type = PONE_JSON_TYPE_NUMBER;
        pone_json_scanner_parse_number(s, 1, &value->number);
    } break;
    case '-': {
        ++s->cursor;
        value->type = PONE_JSON_TYPE_NUMBER;
        pone_json_scanner_parse_number(s, 0, &value->number);
    } break;
    case '{': {
        value->type = PONE_JSON_TYPE_OBJECT;
        pone_json_scanner_parse_object(s, arena, &value->object);
    } break;
    case '[': {
        value->type = PONE_JSON_TYPE_ARRAY;
        pone_json_scanner_parse_array(s, arena, &value->array);
    } break;
    case 't': {
        if (s->input.buf[++s->cursor] != 'r' ||
            s->input.buf[++s->cursor] != 'u' ||
            s->input.buf[++s->cursor] != 'e') {
            PONE_ASSERT(0 && "Invalid boolean");
        }
        value->type = PONE_JSON_TYPE_TRUE;
        ++s->cursor;
    } break;
    case 'f': {
        if (s->input.buf[++s->cursor] != 'a' ||
            s->input.buf[++s->cursor] != 'l' ||
            s->input.buf[++s->cursor] != 's' ||
            s->input.buf[++s->cursor] != 'e') {
            PONE_ASSERT(0 && "Invalid boolean");
        }
        value->type = PONE_JSON_TYPE_FALSE;
        ++s->cursor;
    } break;
    case 'n': {
        if (s->input.buf[++s->cursor] != 'u' ||
            s->input.buf[++s->cursor] != 'l' ||
            s->input.buf[++s->cursor] != 'l') {
            PONE_ASSERT(0 && "Invalid null");
        }
        value->type = PONE_JSON_TYPE_NULL;
        ++s->cursor;
    } break;
    }
}
