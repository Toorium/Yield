#ifndef INTERPRETER_H
#define INTERPRETER_H

// ============================================================
//  Yield v1.2.0 — Interpreter
//  Tree-walk interpreter over the AST
// ============================================================

#include "parser.h"

// ---- Value types -------------------------------------------
typedef enum {
    VAL_NULL,
    VAL_NUMBER,
    VAL_STRING,
    VAL_BOOL,
    VAL_LIST,
    VAL_FUNCTION,
    VAL_CLASS,
    VAL_OBJECT,
} ValueType;

typedef struct Value Value;
typedef struct YieldList YieldList;
typedef struct Env Env;
typedef struct YieldObject YieldObject;

// ---- Dynamic list of Values --------------------------------
struct YieldList {
    Value **items;
    int     count;
    int     capacity;
};

// ---- Class instance ----------------------------------------
struct YieldObject {
    char  *class_name;
    Env   *fields;      // stores self.x values
};

// ---- Value -------------------------------------------------
struct Value {
    ValueType type;
    int       ref_count;

    double      number;
    char       *string;
    int         boolean;
    YieldList  *list;

    // Function / method
    char      **params;
    int         param_count;
    NodeList    body;
    Env        *closure;

    // Class definition
    char       *class_name;
    NodeList    class_body;

    // Object instance
    YieldObject *object;
};

// ---- Environment (variable scope) --------------------------
struct Env {
    char   **keys;
    Value  **vals;
    int      count;
    int      capacity;
    Env     *parent;
};

// ---- Public API --------------------------------------------
void interpret(Node *program);

#endif // INTERPRETER_H
