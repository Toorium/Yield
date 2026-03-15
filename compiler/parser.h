#ifndef PARSER_H
#define PARSER_H

// ============================================================
//  Yield v1.2.0 — Parser
//  Converts a TokenArray into an Abstract Syntax Tree (AST)
// ============================================================

#include "lexer.h"

// ---- AST Node Types ----------------------------------------
typedef enum {
    // Statements
    NODE_PROGRAM,       // root node — list of statements
    NODE_VAR_DECL,      // var x = expr
    NODE_CONST_DECL,    // const X = expr
    NODE_SET,           // set x expr
    NODE_ADD_ASSIGN,    // add x expr
    NODE_SUB_ASSIGN,    // sub x expr
    NODE_MUL_ASSIGN,    // mul x expr
    NODE_DIV_ASSIGN,    // div x expr
    NODE_MOD_ASSIGN,    // mod x expr
    NODE_OUT,           // out(...)
    NODE_INPUT,         // input(var, "prompt")
    NODE_IF,            // if/elseif/else
    NODE_RUN_TIMES,     // run(n):
    NODE_RUN_INDEX,     // run(i, n):
    NODE_RUN_LIST,      // run(i, list):
    NODE_RUN_WHILE,     // run(condition):
    NODE_RUN_FOREVER,   // run:
    NODE_STOP,          // stop
    NODE_SKIP,          // skip
    NODE_FUNC_DEF,      // func name(args): ... end
    NODE_RETURN,        // yield expr
    NODE_CLASS_DEF,     // class Name: ... end
    NODE_ERROR_THROW,   // error("msg")
    NODE_ERROR_BLOCK,   // error: ... end catch e: ... end
    NODE_LOAD,          // load "file"
    NODE_PLUGIN,        // plugin "lib"

    // Expressions
    NODE_NUMBER,        // 42  3.14
    NODE_STRING,        // "hello"
    NODE_BOOL,          // True  False
    NODE_IDENT,         // foo
    NODE_BINOP,         // a + b,  a = b,  a > b ...
    NODE_UNOP,          // not x
    NODE_CALL,          // func(args)  or  builtin(args)
    NODE_INDEX,         // list[0]
    NODE_MEMBER,        // obj.field
    NODE_METHOD_CALL,   // obj.method(args)
    NODE_LIST_LITERAL,  // [1, 2, 3]
    NODE_NEW,           // new ClassName(args)
    NODE_IN_RANGE,      // x in(a, b)
} NodeType;

// ---- Forward declaration -----------------------------------
typedef struct Node Node;

// ---- Node children array -----------------------------------
typedef struct {
    Node **items;
    int    count;
    int    capacity;
} NodeList;

// ---- If-branch (condition + body) --------------------------
typedef struct {
    Node    *condition;  // NULL for else branch
    NodeList body;
} IfBranch;

// ---- Main AST Node -----------------------------------------
struct Node {
    NodeType type;
    int      line;

    char    *sval;       // string value (ident name, string literal, op)
    double   nval;       // numeric value
    int      ival;       // integer value (bool: 1/0)

    Node    *left;       // binary op left, condition, target var
    Node    *right;      // binary op right, assigned value

    NodeList children;   // body of blocks, arg list for calls

    IfBranch *branches;  // array of if/elseif/else branches
    int       branch_count;

    char    *loop_var;   // name of loop variable (run(i, ...))

    char   **params;     // parameter name list
    int      param_count;
    char    *name;       // func/class name

    NodeList error_body;
    NodeList catch_body;
    char    *catch_var;  // variable name in catch e:
};

// ---- Public API --------------------------------------------
Node *parse(const TokenArray *ta);
void  node_free(Node *n);
void  node_print(const Node *n, int indent);

#endif // PARSER_H
