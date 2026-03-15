// ============================================================
//  Yield v1.2.1 — Parser implementation
//  Recursive-descent parser → AST
//
//  Key fix: keywords are only reserved in statement-starting
//  position. Anywhere a *name* is expected (func name, param
//  names, var/const name, assignment target, class name,
//  catch var, new class name, identifiers in expressions)
//  ANY token that isn't punctuation or an operator is accepted.
//  This lets users write:  var chance = 5
//                          func add(a, b):
//                          func str(x):
// ============================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "parser.h"

// ============================================================
//  Parser state
// ============================================================
typedef struct {
    const TokenArray *ta;
    int               pos;
} Parser;

// ---- Token navigation --------------------------------------
static Token *peek(Parser *p) {
    return &p->ta->tokens[p->pos];
}
static Token *peek2(Parser *p) {
    if (p->pos + 1 < p->ta->count) return &p->ta->tokens[p->pos + 1];
    return &p->ta->tokens[p->ta->count - 1];
}
static Token *advance(Parser *p) {
    Token *t = &p->ta->tokens[p->pos];
    if (p->pos < p->ta->count - 1) p->pos++;
    return t;
}
static int check(Parser *p, YieldTokenType t) {
    return peek(p)->type == t;
}
static int check2(Parser *p, YieldTokenType t) {
    return peek2(p)->type == t;
}
static Token *expect(Parser *p, YieldTokenType t, const char *ctx) {
    if (!check(p, t)) {
        fprintf(stderr, "Parse error at line %d: expected %s but got '%s' (%s)\n",
                peek(p)->line, token_type_name(t), peek(p)->value, ctx);
        exit(1);
    }
    return advance(p);
}
static void skip_newlines(Parser *p) {
    while (check(p, TOK_NEWLINE)) advance(p);
}
static void expect_end(Parser *p, const char *ctx) {
    skip_newlines(p);
    expect(p, TOK_END, ctx);
}

// ============================================================
//  tok_is_name — true for any token that can serve as a name.
//  Excludes only punctuation, operators, and structural tokens.
//  This is the core fix: keywords like add/sub/int/str/chance
//  are valid names when the grammar expects a name.
// ============================================================
static int tok_is_name(YieldTokenType t) {
    switch (t) {
        // Structural / punctuation — never a name
        case TOK_EOF:
        case TOK_NEWLINE:
        case TOK_COLON:
        case TOK_COMMA:
        case TOK_DOT:
        case TOK_LPAREN:
        case TOK_RPAREN:
        case TOK_LBRACKET:
        case TOK_RBRACKET:
        // Operators — never a name
        case TOK_EQ:
        case TOK_NEQ:
        case TOK_GT:
        case TOK_LT:
        case TOK_GTE:
        case TOK_LTE:
        case TOK_PLUS:
        case TOK_MINUS:
        case TOK_STAR:
        case TOK_SLASH:
        case TOK_PERCENT:
        // Literals — never a name
        case TOK_NUMBER:
        case TOK_STRING:
        case TOK_BOOL:
            return 0;
        default:
            return 1;   // everything else (keywords, TOK_IDENT) is a valid name
    }
}

// Consume the next token as a name, or error if it can't be one.
static Token *expect_name(Parser *p, const char *ctx) {
    if (!tok_is_name(peek(p)->type)) {
        fprintf(stderr, "Parse error at line %d: expected name but got '%s' (%s)\n",
                peek(p)->line, peek(p)->value, ctx);
        exit(1);
    }
    return advance(p);
}

// ============================================================
//  Node constructors
// ============================================================
static Node *node_new(NodeType type, int line) {
    Node *n = calloc(1, sizeof(Node));
    if (!n) { perror("calloc"); exit(1); }
    n->type = type;
    n->line = line;
    return n;
}
static void nodelist_push(NodeList *nl, Node *n) {
    if (nl->count >= nl->capacity) {
        nl->capacity = nl->capacity ? nl->capacity * 2 : 8;
        nl->items = realloc(nl->items, sizeof(Node *) * nl->capacity);
        if (!nl->items) { perror("realloc"); exit(1); }
    }
    nl->items[nl->count++] = n;
}
static void nodelist_free(NodeList *nl) {
    for (int i = 0; i < nl->count; i++) node_free(nl->items[i]);
    free(nl->items);
    nl->items = NULL; nl->count = nl->capacity = 0;
}

// ============================================================
//  Forward declarations
// ============================================================
static Node    *parse_stmt(Parser *p);
static Node    *parse_expr(Parser *p);
static Node    *parse_comparison(Parser *p);
static Node    *parse_additive(Parser *p);
static Node    *parse_multiplicative(Parser *p);
static Node    *parse_unary(Parser *p);
static Node    *parse_postfix(Parser *p);
static Node    *parse_primary(Parser *p);
static Node    *parse_assignment_target(Parser *p);
static NodeList parse_block(Parser *p);
static NodeList parse_arg_list(Parser *p);

// ============================================================
//  Block parser
// ============================================================
static int is_block_end(Parser *p) {
    YieldTokenType t = peek(p)->type;
    return t == TOK_END || t == TOK_ELSEIF || t == TOK_ELSE ||
           t == TOK_CATCH || t == TOK_EOF;
}

static NodeList parse_block(Parser *p) {
    NodeList body = {0};
    skip_newlines(p);
    while (!is_block_end(p)) {
        Node *s = parse_stmt(p);
        if (s) nodelist_push(&body, s);
        skip_newlines(p);
    }
    return body;
}

// ============================================================
//  Argument list  ( expr, expr, ... )
// ============================================================
static NodeList parse_arg_list(Parser *p) {
    NodeList args = {0};
    expect(p, TOK_LPAREN, "arg list");
    if (!check(p, TOK_RPAREN)) {
        nodelist_push(&args, parse_expr(p));
        while (check(p, TOK_COMMA)) {
            advance(p);
            nodelist_push(&args, parse_expr(p));
        }
    }
    expect(p, TOK_RPAREN, "arg list close");
    return args;
}

// ============================================================
//  parse_assignment_target
//  Parses the LHS of set/add/sub/mul/div/mod.
//  Accepts any name token (including keywords used as vars).
//  Supports: name, name.field, name[index]
// ============================================================
static Node *parse_assignment_target(Parser *p) {
    Token *t = peek(p);
    if (!tok_is_name(t->type)) {
        fprintf(stderr, "Parse error at line %d: expected assignment target, got '%s'\n",
                t->line, t->value);
        exit(1);
    }
    int line = t->line;
    advance(p);
    Node *left = node_new(NODE_IDENT, line);
    left->sval = strdup(t->value);

    // Chain .field or [non-empty index] accessors
    while (check(p, TOK_DOT) || (check(p, TOK_LBRACKET) && !check2(p, TOK_RBRACKET))) {
        if (check(p, TOK_DOT)) {
            int ln = peek(p)->line;
            advance(p); // .
            Token *field = peek(p);
            advance(p); // field name (any token)
            Node *n = node_new(NODE_MEMBER, ln);
            n->left = left;
            n->sval = strdup(field->value);
            left = n;
        } else {
            // [index]
            int ln = peek(p)->line;
            advance(p); // [
            Node *idx = parse_expr(p);
            expect(p, TOK_RBRACKET, "]");
            Node *n = node_new(NODE_INDEX, ln);
            n->left  = left;
            n->right = idx;
            left = n;
        }
    }
    return left;
}

// ============================================================
//  Statement parser
// ============================================================
static Node *parse_stmt(Parser *p) {
    while (check(p, TOK_NEWLINE)) advance(p);
    if (check(p, TOK_EOF)) return NULL;

    Token *t = peek(p);
    int line = t->line;

    // ---- var name = expr -----------------------------------
    if (t->type == TOK_VAR) {
        advance(p);
        Token *name = expect_name(p, "var name");
        expect(p, TOK_EQ, "var =");
        Node *val = parse_expr(p);
        Node *n = node_new(NODE_VAR_DECL, line);
        n->name  = strdup(name->value);
        n->right = val;
        return n;
    }

    // ---- const name = expr ---------------------------------
    if (t->type == TOK_CONST) {
        advance(p);
        Token *name = expect_name(p, "const name");
        expect(p, TOK_EQ, "const =");
        Node *val = parse_expr(p);
        Node *n = node_new(NODE_CONST_DECL, line);
        n->name  = strdup(name->value);
        n->right = val;
        return n;
    }

    // ---- set target expr -----------------------------------
    if (t->type == TOK_SET) {
        advance(p);
        Node *target = parse_assignment_target(p);
        Node *val    = parse_expr(p);
        Node *n = node_new(NODE_SET, line);
        n->left  = target;
        n->right = val;
        return n;
    }

    // ---- add / sub / mul / div / mod target expr -----------
    if (t->type == TOK_ADD || t->type == TOK_SUB ||
        t->type == TOK_MUL || t->type == TOK_DIV ||
        t->type == TOK_MOD) {
        YieldTokenType op = t->type;
        advance(p);
        Node *target = parse_assignment_target(p);
        Node *val    = parse_expr(p);
        NodeType nt = (op == TOK_ADD) ? NODE_ADD_ASSIGN :
                      (op == TOK_SUB) ? NODE_SUB_ASSIGN :
                      (op == TOK_MUL) ? NODE_MUL_ASSIGN :
                      (op == TOK_DIV) ? NODE_DIV_ASSIGN : NODE_MOD_ASSIGN;
        Node *n = node_new(nt, line);
        n->left  = target;
        n->right = val;
        return n;
    }

    // ---- out( ... ) ----------------------------------------
    if (t->type == TOK_OUT) {
        advance(p);
        Node *n = node_new(NODE_OUT, line);
        n->children = parse_arg_list(p);
        return n;
    }

    // ---- input(var) / input(var, "prompt") ----------------
    if (t->type == TOK_INPUT) {
        advance(p);
        Node *n = node_new(NODE_INPUT, line);
        n->children = parse_arg_list(p);
        return n;
    }

    // ---- if ------------------------------------------------
    if (t->type == TOK_IF) {
        advance(p);
        Node *n = node_new(NODE_IF, line);
        int cap = 4;
        n->branches = malloc(sizeof(IfBranch) * cap);
        n->branch_count = 0;

        IfBranch b0 = {0};
        b0.condition = parse_expr(p);
        expect(p, TOK_COLON, "if :");
        b0.body = parse_block(p);
        n->branches[n->branch_count++] = b0;

        while (check(p, TOK_ELSEIF) || check(p, TOK_ELSE)) {
            if (n->branch_count >= cap) {
                cap *= 2;
                n->branches = realloc(n->branches, sizeof(IfBranch) * cap);
            }
            IfBranch b = {0};
            if (check(p, TOK_ELSEIF)) {
                advance(p);
                b.condition = parse_expr(p);
            } else {
                advance(p); // else
                b.condition = NULL;
            }
            expect(p, TOK_COLON, "elseif/else :");
            b.body = parse_block(p);
            n->branches[n->branch_count++] = b;
        }
        expect_end(p, "if end");
        return n;
    }

    // ---- run -----------------------------------------------
    if (t->type == TOK_RUN) {
        advance(p);

        // run:  (forever)
        if (check(p, TOK_COLON)) {
            advance(p);
            Node *n = node_new(NODE_RUN_FOREVER, line);
            n->children = parse_block(p);
            expect_end(p, "run end");
            return n;
        }

        expect(p, TOK_LPAREN, "run (");

        // run(i, x): — ident/name-token followed by comma → index/list loop
        // run(n):    — single expr → RUN_TIMES or RUN_WHILE
        //
        // We detect the two-var form by checking: first token is a name AND
        // second token is a comma. We must use tok_is_name here so keyword
        // variable names like "run(count, list):" work.
        int is_two_var = (tok_is_name(peek(p)->type) && check2(p, TOK_COMMA));

        if (is_two_var) {
            Token *var = advance(p); // loop variable name
            advance(p);              // comma
            Node *range = parse_expr(p);
            expect(p, TOK_RPAREN, "run )");
            expect(p, TOK_COLON,  "run :");

            NodeType nt = (range->type == NODE_NUMBER) ? NODE_RUN_INDEX : NODE_RUN_LIST;
            Node *n = node_new(nt, line);
            n->loop_var = strdup(var->value);
            n->left     = range;
            n->children = parse_block(p);
            expect_end(p, "run end");
            return n;
        } else {
            Node *expr = parse_expr(p);
            expect(p, TOK_RPAREN, "run )");
            expect(p, TOK_COLON,  "run :");

            NodeType nt;
            if (expr->type == NODE_NUMBER) {
                nt = NODE_RUN_TIMES;        // run(5):
            } else if (expr->type == NODE_IDENT) {
                nt = NODE_RUN_TIMES;        // run(myVar): — evaluated at runtime
            } else {
                nt = NODE_RUN_WHILE;        // run(x < 10): — condition
            }
            Node *n = node_new(nt, line);
            n->left     = expr;
            n->children = parse_block(p);
            expect_end(p, "run end");
            return n;
        }
    }

    // ---- stop / skip ---------------------------------------
    if (t->type == TOK_STOP) { advance(p); return node_new(NODE_STOP, line); }
    if (t->type == TOK_SKIP) { advance(p); return node_new(NODE_SKIP, line); }

    // ---- func name(params): ... end ------------------------
    if (t->type == TOK_FUNC) {
        advance(p);
        // Accept any name token as function name (fixes: func add, func str, etc.)
        Token *name_tok = expect_name(p, "function name");
        Node *n = node_new(NODE_FUNC_DEF, line);
        n->name = strdup(name_tok->value);

        expect(p, TOK_LPAREN, "func params (");
        int pcap = 8;
        n->params = malloc(sizeof(char *) * pcap);
        n->param_count = 0;
        while (!check(p, TOK_RPAREN)) {
            // Accept any name token as parameter name (fixes: func f(add, sub, str):)
            Token *param = expect_name(p, "param name");
            if (n->param_count >= pcap) {
                pcap *= 2;
                n->params = realloc(n->params, sizeof(char *) * pcap);
            }
            n->params[n->param_count++] = strdup(param->value);
            if (check(p, TOK_COMMA)) advance(p);
        }
        expect(p, TOK_RPAREN, "func params )");
        expect(p, TOK_COLON,  "func :");
        n->children = parse_block(p);
        expect_end(p, "func end");
        return n;
    }

    // ---- yield expr ----------------------------------------
    if (t->type == TOK_YIELD) {
        advance(p);
        Node *n = node_new(NODE_RETURN, line);
        n->right = parse_expr(p);
        return n;
    }

    // ---- class Name: ... end -------------------------------
    if (t->type == TOK_CLASS) {
        advance(p);
        // Accept any name as class name
        Token *cname = expect_name(p, "class name");
        expect(p, TOK_COLON, "class :");
        Node *n = node_new(NODE_CLASS_DEF, line);
        n->name     = strdup(cname->value);
        n->children = parse_block(p);
        expect_end(p, "class end");
        return n;
    }

    // ---- error("msg")  or  error: ... catch e: ... end -----
    if (t->type == TOK_ERROR) {
        advance(p);
        if (check(p, TOK_LPAREN)) {
            Node *n = node_new(NODE_ERROR_THROW, line);
            n->children = parse_arg_list(p);
            return n;
        }
        expect(p, TOK_COLON, "error :");
        Node *n = node_new(NODE_ERROR_BLOCK, line);
        n->error_body = parse_block(p);
        expect_end(p, "error end");
        skip_newlines(p);
        expect(p, TOK_CATCH, "catch");
        // Accept any name as the catch variable
        Token *cv = expect_name(p, "catch var");
        n->catch_var = strdup(cv->value);
        expect(p, TOK_COLON, "catch :");
        n->catch_body = parse_block(p);
        expect_end(p, "catch end");
        return n;
    }

    // ---- load "file" ---------------------------------------
    if (t->type == TOK_LOAD) {
        advance(p);
        Token *s = expect(p, TOK_STRING, "load file");
        Node *n = node_new(NODE_LOAD, line);
        n->sval = strdup(s->value);
        return n;
    }

    // ---- plugin "lib" --------------------------------------
    if (t->type == TOK_PLUGIN) {
        advance(p);
        Token *s = expect(p, TOK_STRING, "plugin lib");
        Node *n = node_new(NODE_PLUGIN, line);
        n->sval = strdup(s->value);
        return n;
    }

    // ---- Expression statement (function call, method call, etc.)
    Node *expr = parse_expr(p);
    return expr;
}

// ============================================================
//  Expression parser  (precedence climbing)
//  Level 1: logical  (and / or)
//  Level 2: comparison  (= not= > < >= <= in)
//  Level 3: additive  (+ -)
//  Level 4: multiplicative  (* / %)
//  Level 5: unary  (not -)
//  Level 6: postfix  (. [] ())
//  Level 7: primary  (literals, idents, grouped)
// ============================================================

static Node *parse_expr(Parser *p) {
    Node *left = parse_comparison(p);
    while (check(p, TOK_AND) || check(p, TOK_OR)) {
        Token *op = advance(p);
        Node *right = parse_comparison(p);
        Node *n = node_new(NODE_BINOP, op->line);
        n->sval  = strdup(op->value);
        n->left  = left;
        n->right = right;
        left = n;
    }
    return left;
}

static Node *parse_comparison(Parser *p) {
    Node *left = parse_additive(p);
    YieldTokenType tt = peek(p)->type;
    if (tt == TOK_EQ || tt == TOK_NEQ || tt == TOK_GT ||
        tt == TOK_LT || tt == TOK_GTE || tt == TOK_LTE) {
        Token *op = advance(p);
        Node *right = parse_additive(p);
        Node *n = node_new(NODE_BINOP, op->line);
        n->sval  = strdup(op->value);
        n->left  = left;
        n->right = right;
        return n;
    }
    // x in(a, b)
    if (tt == TOK_IN) {
        int ln = peek(p)->line;
        advance(p);
        expect(p, TOK_LPAREN, "in(");
        Node *lo = parse_expr(p);
        expect(p, TOK_COMMA, "in ,");
        Node *hi = parse_expr(p);
        expect(p, TOK_RPAREN, "in )");
        Node *n = node_new(NODE_IN_RANGE, ln);
        n->left = left;
        NodeList args = {0};
        nodelist_push(&args, lo);
        nodelist_push(&args, hi);
        n->children = args;
        return n;
    }
    return left;
}

static Node *parse_additive(Parser *p) {
    Node *left = parse_multiplicative(p);
    while (check(p, TOK_PLUS) || check(p, TOK_MINUS)) {
        Token *op = advance(p);
        Node *right = parse_multiplicative(p);
        Node *n = node_new(NODE_BINOP, op->line);
        n->sval  = strdup(op->value);
        n->left  = left;
        n->right = right;
        left = n;
    }
    return left;
}

static Node *parse_multiplicative(Parser *p) {
    Node *left = parse_unary(p);
    while (check(p, TOK_STAR) || check(p, TOK_SLASH) || check(p, TOK_PERCENT)) {
        Token *op = advance(p);
        Node *right = parse_unary(p);
        Node *n = node_new(NODE_BINOP, op->line);
        n->sval  = strdup(op->value);
        n->left  = left;
        n->right = right;
        left = n;
    }
    return left;
}

static Node *parse_unary(Parser *p) {
    if (check(p, TOK_NOT)) {
        Token *op = advance(p);
        Node *n = node_new(NODE_UNOP, op->line);
        n->sval  = strdup("not");
        n->right = parse_unary(p);
        return n;
    }
    if (check(p, TOK_MINUS)) {
        Token *op = advance(p);
        Node *n = node_new(NODE_UNOP, op->line);
        n->sval  = strdup("-");
        n->right = parse_unary(p);
        return n;
    }
    return parse_postfix(p);
}

static Node *parse_postfix(Parser *p) {
    Node *left = parse_primary(p);
    while (1) {
        // obj.field  or  obj.method(args)
        if (check(p, TOK_DOT)) {
            int ln = peek(p)->line;
            advance(p); // .
            Token *field = peek(p);
            advance(p); // field name (any token)
            if (check(p, TOK_LPAREN)) {
                Node *n = node_new(NODE_METHOD_CALL, ln);
                n->left     = left;
                n->sval     = strdup(field->value);
                n->children = parse_arg_list(p);
                left = n;
            } else {
                Node *n = node_new(NODE_MEMBER, ln);
                n->left = left;
                n->sval = strdup(field->value);
                left = n;
            }
            continue;
        }
        // list[index]
        if (check(p, TOK_LBRACKET)) {
            int ln = peek(p)->line;
            advance(p); // [
            Node *idx = parse_expr(p);
            expect(p, TOK_RBRACKET, "]");
            Node *n = node_new(NODE_INDEX, ln);
            n->left  = left;
            n->right = idx;
            left = n;
            continue;
        }
        break;
    }
    return left;
}

// ============================================================
//  parse_primary
//
//  The big change: instead of a separate is_builtin() branch
//  that force-consumed built-in keyword tokens as function calls,
//  we now use tok_is_name() to unify ALL name-like tokens.
//  If the name is followed by '(' it becomes a NODE_CALL —
//  the interpreter decides whether it's a builtin or user func.
//  If NOT followed by '(' it becomes a NODE_IDENT (a variable).
//
//  This means:  var chance = 5  works (chance is a variable)
//               out(chance(1,6)) works (chance() is a call)
//               func add(a,b):  works (add is a function name)
// ============================================================
static Node *parse_primary(Parser *p) {
    Token *t = peek(p);
    int line = t->line;

    // Number literal
    if (t->type == TOK_NUMBER) {
        advance(p);
        Node *n = node_new(NODE_NUMBER, line);
        n->nval = atof(t->value);
        n->sval = strdup(t->value);
        return n;
    }

    // String literal
    if (t->type == TOK_STRING) {
        advance(p);
        Node *n = node_new(NODE_STRING, line);
        n->sval = strdup(t->value);
        return n;
    }

    // Boolean literal
    if (t->type == TOK_BOOL) {
        advance(p);
        Node *n = node_new(NODE_BOOL, line);
        n->ival = (strcmp(t->value, "True") == 0) ? 1 : 0;
        n->sval = strdup(t->value);
        return n;
    }

    // new ClassName(args)
    if (t->type == TOK_NEW) {
        advance(p);
        // Class name can be any name token
        Token *cname = expect_name(p, "new class name");
        Node *n = node_new(NODE_NEW, line);
        n->name     = strdup(cname->value);
        n->children = parse_arg_list(p);
        return n;
    }

    // List literal [...]
    if (t->type == TOK_LBRACKET) {
        advance(p);
        Node *n = node_new(NODE_LIST_LITERAL, line);
        if (!check(p, TOK_RBRACKET)) {
            nodelist_push(&n->children, parse_expr(p));
            while (check(p, TOK_COMMA)) {
                advance(p);
                nodelist_push(&n->children, parse_expr(p));
            }
        }
        expect(p, TOK_RBRACKET, "]");
        return n;
    }

    // Grouped expression (...)
    if (t->type == TOK_LPAREN) {
        advance(p);
        Node *n = parse_expr(p);
        expect(p, TOK_RPAREN, ")");
        return n;
    }

    // Any name token — identifier, builtin keyword, or any keyword used as a name.
    // If followed by '(' → function/builtin call.
    // Otherwise → variable reference.
    if (tok_is_name(t->type)) {
        advance(p);
        if (check(p, TOK_LPAREN)) {
            Node *n = node_new(NODE_CALL, line);
            n->sval     = strdup(t->value);
            n->children = parse_arg_list(p);
            return n;
        }
        Node *n = node_new(NODE_IDENT, line);
        n->sval = strdup(t->value);
        return n;
    }

    // Nothing matched
    fprintf(stderr, "Parse error at line %d: unexpected token '%s' (%s)\n",
            line, t->value, token_type_name(t->type));
    exit(1);
}

// ============================================================
//  Public: parse()
// ============================================================
Node *parse(const TokenArray *ta) {
    Parser p = { ta, 0 };
    Node *root = node_new(NODE_PROGRAM, 1);
    skip_newlines(&p);
    while (!check(&p, TOK_EOF)) {
        Node *s = parse_stmt(&p);
        if (s) nodelist_push(&root->children, s);
        skip_newlines(&p);
    }
    return root;
}

// ============================================================
//  node_free
// ============================================================
void node_free(Node *n) {
    if (!n) return;
    free(n->sval);
    free(n->name);
    free(n->loop_var);
    free(n->catch_var);
    for (int i = 0; i < n->param_count; i++) free(n->params[i]);
    free(n->params);
    node_free(n->left);
    node_free(n->right);
    nodelist_free(&n->children);
    nodelist_free(&n->error_body);
    nodelist_free(&n->catch_body);
    if (n->branches) {
        for (int i = 0; i < n->branch_count; i++) {
            node_free(n->branches[i].condition);
            nodelist_free(&n->branches[i].body);
        }
        free(n->branches);
    }
    free(n);
}

// ============================================================
//  Debug print
// ============================================================
static const char *node_type_name(NodeType t) {
    switch (t) {
        case NODE_PROGRAM:      return "PROGRAM";
        case NODE_VAR_DECL:     return "VAR_DECL";
        case NODE_CONST_DECL:   return "CONST_DECL";
        case NODE_SET:          return "SET";
        case NODE_ADD_ASSIGN:   return "ADD_ASSIGN";
        case NODE_SUB_ASSIGN:   return "SUB_ASSIGN";
        case NODE_MUL_ASSIGN:   return "MUL_ASSIGN";
        case NODE_DIV_ASSIGN:   return "DIV_ASSIGN";
        case NODE_MOD_ASSIGN:   return "MOD_ASSIGN";
        case NODE_OUT:          return "OUT";
        case NODE_INPUT:        return "INPUT";
        case NODE_IF:           return "IF";
        case NODE_RUN_TIMES:    return "RUN_TIMES";
        case NODE_RUN_INDEX:    return "RUN_INDEX";
        case NODE_RUN_LIST:     return "RUN_LIST";
        case NODE_RUN_WHILE:    return "RUN_WHILE";
        case NODE_RUN_FOREVER:  return "RUN_FOREVER";
        case NODE_STOP:         return "STOP";
        case NODE_SKIP:         return "SKIP";
        case NODE_FUNC_DEF:     return "FUNC_DEF";
        case NODE_RETURN:       return "RETURN";
        case NODE_CLASS_DEF:    return "CLASS_DEF";
        case NODE_ERROR_THROW:  return "ERROR_THROW";
        case NODE_ERROR_BLOCK:  return "ERROR_BLOCK";
        case NODE_LOAD:         return "LOAD";
        case NODE_PLUGIN:       return "PLUGIN";
        case NODE_NUMBER:       return "NUMBER";
        case NODE_STRING:       return "STRING";
        case NODE_BOOL:         return "BOOL";
        case NODE_IDENT:        return "IDENT";
        case NODE_BINOP:        return "BINOP";
        case NODE_UNOP:         return "UNOP";
        case NODE_CALL:         return "CALL";
        case NODE_INDEX:        return "INDEX";
        case NODE_MEMBER:       return "MEMBER";
        case NODE_METHOD_CALL:  return "METHOD_CALL";
        case NODE_LIST_LITERAL: return "LIST_LITERAL";
        case NODE_NEW:          return "NEW";
        case NODE_IN_RANGE:     return "IN_RANGE";
        default:                return "UNKNOWN";
    }
}

static void indent_print(int indent) {
    for (int i = 0; i < indent; i++) printf("  ");
}

void node_print(const Node *n, int indent) {
    if (!n) return;
    indent_print(indent);
    printf("[%s]", node_type_name(n->type));
    if (n->name)     printf(" name=%s", n->name);
    if (n->sval)     printf(" sval=%s", n->sval);
    if (n->type == NODE_NUMBER) printf(" nval=%g", n->nval);
    if (n->type == NODE_BOOL)   printf(" ival=%d", n->ival);
    if (n->loop_var) printf(" loop_var=%s", n->loop_var);
    printf("\n");

    if (n->left)  { indent_print(indent+1); printf("(left)\n");  node_print(n->left,  indent+2); }
    if (n->right) { indent_print(indent+1); printf("(right)\n"); node_print(n->right, indent+2); }

    if (n->children.count > 0) {
        indent_print(indent+1); printf("(children: %d)\n", n->children.count);
        for (int i = 0; i < n->children.count; i++)
            node_print(n->children.items[i], indent+2);
    }

    if (n->branch_count > 0) {
        for (int i = 0; i < n->branch_count; i++) {
            indent_print(indent+1);
            printf("(branch %d)\n", i);
            if (n->branches[i].condition) node_print(n->branches[i].condition, indent+2);
            else { indent_print(indent+2); printf("[else]\n"); }
            for (int j = 0; j < n->branches[i].body.count; j++)
                node_print(n->branches[i].body.items[j], indent+2);
        }
    }

    if (n->error_body.count > 0) {
        indent_print(indent+1); printf("(error_body)\n");
        for (int i = 0; i < n->error_body.count; i++)
            node_print(n->error_body.items[i], indent+2);
    }
    if (n->catch_body.count > 0) {
        indent_print(indent+1); printf("(catch_body var=%s)\n", n->catch_var ? n->catch_var : "?");
        for (int i = 0; i < n->catch_body.count; i++)
            node_print(n->catch_body.items[i], indent+2);
    }
}