#ifndef PONE_JSON_H
#define PONE_JSON_H

#include "pone_arena.h"
#include "pone_string.h"

struct PoneJsonValue;

enum PoneJsonType {
    PONE_JSON_TYPE_OBJECT,
    PONE_JSON_TYPE_ARRAY,
    PONE_JSON_TYPE_STRING,
    PONE_JSON_TYPE_NUMBER,
    PONE_JSON_TYPE_TRUE,
    PONE_JSON_TYPE_FALSE,
    PONE_JSON_TYPE_NULL,
};

struct PoneJsonScanner {
    PoneString input;
    usize cursor;
};

struct PoneJsonArrayValue {
    PoneJsonValue *value;

    PoneJsonArrayValue *next;
};

struct PoneJsonArray {
    PoneJsonArrayValue *values;
    usize count;
};

struct PoneJsonPair {
    PoneString name;
    PoneJsonValue *value;

    PoneJsonPair *next;
};

struct PoneJsonObject {
    PoneJsonPair *pairs;
    usize count;
};

struct PoneJsonValue {
    PoneJsonType type;
    union {
        PoneJsonObject object;
        PoneJsonArray array;
        PoneString string;
        f64 number;
    };
};

PoneJsonValue *pone_json_scanner_parse_value(PoneJsonScanner *s, Arena *arena);

#endif
