// ============================================================
//  Yield v1.2.0 — Interpreter implementation
// ============================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>
#include <setjmp.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>      // _kbhit(), _getch()
#else
#include <unistd.h>
#include <termios.h>    // non-blocking key input on Linux/macOS
#include <fcntl.h>
#endif

#include "interpreter.h"

// ============================================================
//  Cross-platform keyboard helpers
//  On Windows we use <conio.h>.  On Linux/macOS we implement
//  the same interface using termios raw mode.
// ============================================================
#ifndef _WIN32
static int _kbhit(void) {
    struct termios oldt, newt;
    int ch, oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if (ch != EOF) { ungetc(ch, stdin); return 1; }
    return 0;
}
static int _getch(void) {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}
#endif

// Compile-time check: if parser.h and interpreter.c are from different versions,
// this assertion will fail at compile time with a clear message rather than a
// cryptic "unhandled node N" at runtime.
_Static_assert(NODE_RUN_FOREVER == 16,
    "Build mismatch: parser.h and interpreter.c are from different versions. "
    "Recompile ALL files: gcc -Wall -O2 -o yield main.c lexer.c parser.c interpreter.c -lm");

// ============================================================
//  Catchable error
// ============================================================
static jmp_buf  g_error_jump;
static char     g_error_msg[1024];
static int      g_in_error_block = 0;

static void throw_error(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(g_error_msg, sizeof(g_error_msg), fmt, ap);
    va_end(ap);
    longjmp(g_error_jump, 1);
}

static void runtime_error(int line, const char *fmt, ...) {
    char msg[1024];
    va_list ap; va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    if (g_in_error_block > 0) { throw_error("%s", msg); }
    fprintf(stderr, "Runtime error (line %d): %s\n", line, msg);
    exit(1);
}

// ============================================================
//  Control-flow signals
// ============================================================
typedef enum { SIG_NONE, SIG_STOP, SIG_SKIP, SIG_RETURN } Signal;
typedef struct { Signal signal; Value *value; } ExecResult;

static ExecResult make_ok()          { ExecResult r = {SIG_NONE,   NULL}; return r; }
static ExecResult make_stop()        { ExecResult r = {SIG_STOP,   NULL}; return r; }
static ExecResult make_skip()        { ExecResult r = {SIG_SKIP,   NULL}; return r; }
static ExecResult make_return(Value *v) { ExecResult r = {SIG_RETURN, v}; return r; }

// ============================================================
//  Value constructors / ref counting
// ============================================================
static Value *val_new(ValueType t) {
    Value *v = calloc(1, sizeof(Value));
    if (!v) { perror("calloc"); exit(1); }
    v->type = t; v->ref_count = 1;
    return v;
}
static Value *val_null()          { return val_new(VAL_NULL); }
static Value *val_number(double n){ Value *v = val_new(VAL_NUMBER); v->number = n; return v; }
static Value *val_string(const char *s){ Value *v = val_new(VAL_STRING); v->string = strdup(s); return v; }
static Value *val_bool(int b)     { Value *v = val_new(VAL_BOOL); v->boolean = b ? 1 : 0; return v; }
static Value *val_list(void) {
    Value *v = val_new(VAL_LIST);
    v->list = calloc(1, sizeof(YieldList));
    v->list->capacity = 4;
    v->list->items    = malloc(sizeof(Value *) * 4);
    return v;
}

static void val_retain(Value *v)  { if (v) v->ref_count++; }
static void val_release(Value *v);

static void list_release(YieldList *l) {
    if (!l) return;
    for (int i = 0; i < l->count; i++) val_release(l->items[i]);
    free(l->items); free(l);
}

static void val_release(Value *v) {
    if (!v) return;
    if (--v->ref_count > 0) return;
    free(v->string);
    free(v->class_name);
    for (int i = 0; i < v->param_count; i++) free(v->params[i]);
    free(v->params);
    list_release(v->list);
    if (v->object) { free(v->object->class_name); free(v->object); }
    free(v);
}

// ============================================================
//  Environment
// ============================================================
static Env *env_new(Env *parent) {
    Env *e = calloc(1, sizeof(Env));
    e->parent = parent; e->capacity = 8;
    e->keys = malloc(sizeof(char *) * 8);
    e->vals = malloc(sizeof(Value *) * 8);
    return e;
}
static void env_free(Env *e) {
    if (!e) return;
    for (int i = 0; i < e->count; i++) { free(e->keys[i]); val_release(e->vals[i]); }
    free(e->keys); free(e->vals); free(e);
}
static Value *env_get_local(Env *e, const char *k) {
    for (int i = 0; i < e->count; i++) if (strcmp(e->keys[i], k) == 0) return e->vals[i];
    return NULL;
}
static Value *env_get(Env *e, const char *k) {
    for (Env *c = e; c; c = c->parent) { Value *v = env_get_local(c, k); if (v) return v; }
    return NULL;
}
static void env_grow(Env *e) {
    if (e->count < e->capacity) return;
    e->capacity *= 2;
    e->keys = realloc(e->keys, sizeof(char *) * e->capacity);
    e->vals = realloc(e->vals, sizeof(Value *) * e->capacity);
}
static void env_set(Env *e, const char *k, Value *val) {
    for (Env *c = e; c; c = c->parent)
        for (int i = 0; i < c->count; i++)
            if (strcmp(c->keys[i], k) == 0) {
                val_release(c->vals[i]); val_retain(val); c->vals[i] = val; return;
            }
    env_grow(e);
    e->keys[e->count] = strdup(k); val_retain(val); e->vals[e->count] = val; e->count++;
}
static void env_define(Env *e, const char *k, Value *val) {
    for (int i = 0; i < e->count; i++)
        if (strcmp(e->keys[i], k) == 0) {
            val_release(e->vals[i]); val_retain(val); e->vals[i] = val; return;
        }
    env_grow(e);
    e->keys[e->count] = strdup(k); val_retain(val); e->vals[e->count] = val; e->count++;
}

// ============================================================
//  Value utilities
// ============================================================
static char *val_to_string(Value *v) {
    if (!v) return strdup("null");
    char buf[64];
    switch (v->type) {
        case VAL_NULL:   return strdup("null");
        case VAL_BOOL:   return strdup(v->boolean ? "True" : "False");
        case VAL_STRING: return strdup(v->string);
        case VAL_NUMBER:
            if (v->number == (long long)v->number)
                 snprintf(buf, sizeof(buf), "%lld", (long long)v->number);
            else snprintf(buf, sizeof(buf), "%g", v->number);
            return strdup(buf);
        case VAL_LIST: {
            char *r = strdup("[");
            for (int i = 0; i < v->list->count; i++) {
                char *item = val_to_string(v->list->items[i]);
                r = realloc(r, strlen(r) + strlen(item) + 4);
                strcat(r, item); free(item);
                if (i < v->list->count - 1) strcat(r, ", ");
            }
            r = realloc(r, strlen(r) + 3); strcat(r, "]"); return r;
        }
        case VAL_FUNCTION: return strdup("<function>");
        case VAL_CLASS:    return strdup("<class>");
        case VAL_OBJECT:
            snprintf(buf, sizeof(buf), "<object:%s>", v->object->class_name);
            return strdup(buf);
        default: return strdup("?");
    }
}
static int val_truthy(Value *v) {
    if (!v) return 0;
    switch (v->type) {
        case VAL_NULL:   return 0;
        case VAL_BOOL:   return v->boolean;
        case VAL_NUMBER: return v->number != 0.0;
        case VAL_STRING: return strlen(v->string) > 0;
        case VAL_LIST:   return v->list->count > 0;
        default: return 1;
    }
}

// ============================================================
//  List helpers
// ============================================================
static void list_push(YieldList *l, Value *v) {
    if (l->count >= l->capacity) {
        l->capacity *= 2;
        l->items = realloc(l->items, sizeof(Value *) * l->capacity);
    }
    val_retain(v); l->items[l->count++] = v;
}
static void list_remove(YieldList *l, Value *v) {
    for (int i = 0; i < l->count; i++) {
        Value *it = l->items[i]; int eq = 0;
        if (it->type == VAL_NUMBER && v->type == VAL_NUMBER) eq = (it->number == v->number);
        else if (it->type == VAL_STRING && v->type == VAL_STRING) eq = (strcmp(it->string, v->string) == 0);
        else if (it->type == VAL_BOOL && v->type == VAL_BOOL) eq = (it->boolean == v->boolean);
        if (eq) {
            val_release(it);
            memmove(&l->items[i], &l->items[i+1], sizeof(Value *) * (l->count - i - 1));
            l->count--; return;
        }
    }
}

// ============================================================
//  Module registry for load "name"
//  Maps module name → file path relative to the interpreter exe
// ============================================================
typedef struct { const char *name; const char *path; } ModuleEntry;

static const ModuleEntry MODULE_REGISTRY[] = {
    { "math",    "lib/math.yd"    },
    { "strings", "lib/strings.yd" },
    { "lists",   "lib/lists.yd"   },
    { "io",      "lib/io.yd"      },
    { NULL, NULL }
};

// Already-loaded module names (to avoid re-loading)
#define MAX_LOADED 64
static char *g_loaded[MAX_LOADED];
static int   g_loaded_count = 0;

// Keep loaded ASTs alive — functions reference their body nodes
static Node      *g_loaded_asts[MAX_LOADED];
static TokenArray g_loaded_tas[MAX_LOADED];
static int        g_loaded_ast_count = 0;

static int module_already_loaded(const char *name) {
    for (int i = 0; i < g_loaded_count; i++)
        if (strcmp(g_loaded[i], name) == 0) return 1;
    return 0;
}
static void module_mark_loaded(const char *name) {
    if (g_loaded_count < MAX_LOADED)
        g_loaded[g_loaded_count++] = strdup(name);
}
static void module_registry_free(void) {
    for (int i = 0; i < g_loaded_count; i++) free(g_loaded[i]);
    g_loaded_count = 0;
    for (int i = 0; i < g_loaded_ast_count; i++) {
        node_free(g_loaded_asts[i]);
        token_array_free(&g_loaded_tas[i]);
    }
    g_loaded_ast_count = 0;
}

static char *read_file_contents(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f); rewind(f);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, sz, f); (void)n;
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

// Forward declarations
static Value     *eval(Node *n, Env *env);
static ExecResult exec(Node *n, Env *env);
static ExecResult exec_block(NodeList *body, Env *env);

static void load_module(const char *modname, Env *env, int line) {
    if (module_already_loaded(modname)) return;

    // 1. Look up in registry
    const char *fpath = NULL;
    for (int i = 0; MODULE_REGISTRY[i].name; i++) {
        if (strcmp(MODULE_REGISTRY[i].name, modname) == 0) {
            fpath = MODULE_REGISTRY[i].path;
            break;
        }
    }

    // 2. If not in registry, try modname.yd directly
    char pathbuf[512];
    if (!fpath) {
        snprintf(pathbuf, sizeof(pathbuf), "%s.yd", modname);
        fpath = pathbuf;
    }

    char *src = read_file_contents(fpath);
    if (!src) {
        // Try lib/ prefix as fallback
        snprintf(pathbuf, sizeof(pathbuf), "lib/%s.yd", modname);
        src = read_file_contents(pathbuf);
    }
    if (!src) {
        runtime_error(line, "Cannot load module '%s' (tried '%s')", modname, fpath);
        return;
    }

    module_mark_loaded(modname);

    TokenArray ta  = lex(src);
    Node      *ast = parse(&ta);
    exec_block(&ast->children, env);   // run in caller's env
    free(src);
    // Keep AST and tokens alive — function bodies reference them
    if (g_loaded_ast_count < MAX_LOADED) {
        g_loaded_asts[g_loaded_ast_count] = ast;
        g_loaded_tas[g_loaded_ast_count]  = ta;
        g_loaded_ast_count++;
    } else {
        node_free(ast);
        token_array_free(&ta);
    }
}

// ============================================================
//  Built-in functions
// ============================================================
static Value *call_builtin(const char *name, Value **args, int argc, int line) {

    // ---- wait(seconds) ------------------------------------
    if (strcmp(name, "wait") == 0) {
        if (argc < 1) runtime_error(line, "wait() needs 1 argument");
        double s = args[0]->number;
#ifdef _WIN32
        Sleep((DWORD)(s * 1000));
#else
        usleep((useconds_t)(s * 1000000));
#endif
        return val_null();
    }

    // ---- clear() — clear the terminal screen --------------
    if (strcmp(name, "clear") == 0) {
#ifdef _WIN32
        // Use Win32 console API — no system() needed
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        DWORD count, written;
        if (GetConsoleScreenBufferInfo(h, &csbi)) {
            count = csbi.dwSize.X * csbi.dwSize.Y;
            COORD origin = {0, 0};
            FillConsoleOutputCharacter(h, ' ', count, origin, &written);
            FillConsoleOutputAttribute(h, csbi.wAttributes, count, origin, &written);
            SetConsoleCursorPosition(h, origin);
        }
#else
        // ANSI escape: clear screen + move cursor to top-left
        printf("\033[2J\033[H");
        fflush(stdout);
#endif
        return val_null();
    }

    // ---- chance(lo, hi) -----------------------------------
    if (strcmp(name, "chance") == 0) {
        if (argc < 2) runtime_error(line, "chance() needs 2 arguments");
        int lo = (int)args[0]->number, hi = (int)args[1]->number;
        return val_number(lo + rand() % (hi - lo + 1));
    }

    // ---- upper / lower ------------------------------------
    if (strcmp(name, "upper") == 0) {
        if (argc < 1) runtime_error(line, "upper() needs 1 argument");
        char *s = strdup(args[0]->string ? args[0]->string : "");
        for (int i = 0; s[i]; i++) s[i] = toupper((unsigned char)s[i]);
        Value *v = val_string(s); free(s); return v;
    }
    if (strcmp(name, "lower") == 0) {
        if (argc < 1) runtime_error(line, "lower() needs 1 argument");
        char *s = strdup(args[0]->string ? args[0]->string : "");
        for (int i = 0; s[i]; i++) s[i] = tolower((unsigned char)s[i]);
        Value *v = val_string(s); free(s); return v;
    }

    // ---- length -------------------------------------------
    if (strcmp(name, "length") == 0) {
        if (argc < 1) runtime_error(line, "length() needs 1 argument");
        if (args[0]->type == VAL_STRING)
            return val_number((double)strlen(args[0]->string));
        if (args[0]->type == VAL_LIST)
            return val_number((double)args[0]->list->count);
        runtime_error(line, "length(): unsupported type");
    }

    // ---- reverse ------------------------------------------
    if (strcmp(name, "reverse") == 0) {
        if (argc < 1) runtime_error(line, "reverse() needs 1 argument");
        char *src = args[0]->string ? args[0]->string : "";
        int len = (int)strlen(src);
        char *s = malloc(len + 1);
        for (int i = 0; i < len; i++) s[i] = src[len - 1 - i];
        s[len] = '\0';
        Value *v = val_string(s); free(s); return v;
    }

    // ---- int / float / str --------------------------------
    if (strcmp(name, "int") == 0) {
        if (argc < 1) runtime_error(line, "int() needs 1 argument");
        if (args[0]->type == VAL_NUMBER)
            return val_number((double)(long long)args[0]->number);
        if (args[0]->type == VAL_STRING) {
            char *end; double n = strtod(args[0]->string, &end);
            if (end == args[0]->string || *end != '\0')
                throw_error("Cannot convert \"%s\" to number", args[0]->string);
            return val_number((double)(long long)n);
        }
        throw_error("int(): unsupported type");
    }
    if (strcmp(name, "float") == 0) {
        if (argc < 1) runtime_error(line, "float() needs 1 argument");
        if (args[0]->type == VAL_NUMBER)  return val_number(args[0]->number);
        if (args[0]->type == VAL_STRING) {
            char *end; double n = strtod(args[0]->string, &end);
            if (end == args[0]->string) throw_error("Cannot convert \"%s\" to float", args[0]->string);
            return val_number(n);
        }
        throw_error("float(): unsupported type");
    }
    if (strcmp(name, "str") == 0) {
        if (argc < 1) runtime_error(line, "str() needs 1 argument");
        char *s = val_to_string(args[0]); Value *v = val_string(s); free(s); return v;
    }

    // ---- File I/O -----------------------------------------
    // read_file("path") → string contents or null on error
    if (strcmp(name, "read_file") == 0) {
        if (argc < 1) runtime_error(line, "read_file() needs 1 argument");
        const char *path = args[0]->string;
        if (!path) runtime_error(line, "read_file(): path must be a string");
        char *contents = read_file_contents(path);
        if (!contents) return val_null();
        Value *v = val_string(contents); free(contents); return v;
    }

    // write_file("path", "content") → True on success
    if (strcmp(name, "write_file") == 0) {
        if (argc < 2) runtime_error(line, "write_file() needs 2 arguments");
        const char *path = args[0]->string;
        if (!path) runtime_error(line, "write_file(): path must be a string");
        char *content = val_to_string(args[1]);
        FILE *f = fopen(path, "w");
        if (!f) { free(content); return val_bool(0); }
        fputs(content, f); fclose(f); free(content);
        return val_bool(1);
    }

    // append_file("path", "content") → True on success
    if (strcmp(name, "append_file") == 0) {
        if (argc < 2) runtime_error(line, "append_file() needs 2 arguments");
        const char *path = args[0]->string;
        if (!path) runtime_error(line, "append_file(): path must be a string");
        char *content = val_to_string(args[1]);
        FILE *f = fopen(path, "a");
        if (!f) { free(content); return val_bool(0); }
        fputs(content, f); fclose(f); free(content);
        return val_bool(1);
    }

    // file_exists("path") → True / False
    if (strcmp(name, "file_exists") == 0) {
        if (argc < 1) runtime_error(line, "file_exists() needs 1 argument");
        const char *path = args[0]->string;
        if (!path) runtime_error(line, "file_exists(): path must be a string");
        FILE *f = fopen(path, "r");
        if (f) { fclose(f); return val_bool(1); }
        return val_bool(0);
    }

    runtime_error(line, "Unknown built-in: %s", name);
    return val_null();
}

// ============================================================
//  Is-builtin check
// ============================================================
static int is_builtin_name(const char *name) {
    static const char *builtins[] = {
        "wait", "clear", "chance", "upper", "lower", "length", "reverse",
        "int", "float", "str",
        "read_file", "write_file", "append_file", "file_exists",
        NULL
    };
    for (int i = 0; builtins[i]; i++)
        if (strcmp(builtins[i], name) == 0) return 1;
    return 0;
}

// ============================================================
//  Call a user-defined function
// ============================================================
static Value *call_function(Value *fn, Value **args, int argc,
                             Value *self_obj, Env *call_env, int line) {
    if (fn->type != VAL_FUNCTION) runtime_error(line, "Not a function");
    Env *local = env_new(fn->closure ? fn->closure : call_env);
    int offset = 0;
    if (self_obj) { env_define(local, fn->params[0], self_obj); offset = 1; }
    for (int i = offset; i < fn->param_count; i++) {
        Value *v = (i - offset < argc) ? args[i - offset] : val_null();
        env_define(local, fn->params[i], v);
        if (i - offset >= argc) val_release(v);
    }
    ExecResult res = exec_block(&fn->body, local);
    env_free(local);
    if (res.signal == SIG_RETURN) return res.value;
    return val_null();
}

// ============================================================
//  eval()
// ============================================================
static Value *eval(Node *n, Env *env) {
    if (!n) return val_null();
    switch (n->type) {

    case NODE_NUMBER: return val_number(n->nval);
    case NODE_STRING: return val_string(n->sval);
    case NODE_BOOL:   return val_bool(n->ival);

    case NODE_IDENT: {
        Value *v = env_get(env, n->sval);
        if (!v) runtime_error(n->line, "Undefined variable: %s", n->sval);
        val_retain(v); return v;
    }

    case NODE_LIST_LITERAL: {
        Value *list = val_list();
        for (int i = 0; i < n->children.count; i++) {
            Value *item = eval(n->children.items[i], env);
            list_push(list->list, item); val_release(item);
        }
        return list;
    }

    case NODE_BINOP: {
        const char *op = n->sval;
        if (strcmp(op, "and") == 0) {
            Value *l = eval(n->left, env); int lt = val_truthy(l); val_release(l);
            if (!lt) return val_bool(0);
            Value *r = eval(n->right, env); int rt = val_truthy(r); val_release(r);
            return val_bool(rt);
        }
        if (strcmp(op, "or") == 0) {
            Value *l = eval(n->left, env); int lt = val_truthy(l); val_release(l);
            if (lt) return val_bool(1);
            Value *r = eval(n->right, env); int rt = val_truthy(r); val_release(r);
            return val_bool(rt);
        }
        Value *l = eval(n->left, env), *r = eval(n->right, env), *res = NULL;
        if (strcmp(op, "+") == 0) {
            if (l->type == VAL_STRING || r->type == VAL_STRING) {
                char *ls = val_to_string(l), *rs = val_to_string(r);
                char *cat = malloc(strlen(ls) + strlen(rs) + 1);
                strcpy(cat, ls); strcat(cat, rs);
                res = val_string(cat); free(ls); free(rs); free(cat);
            } else res = val_number(l->number + r->number);
        }
        else if (strcmp(op, "-")  == 0)  res = val_number(l->number - r->number);
        else if (strcmp(op, "*")  == 0)  res = val_number(l->number * r->number);
        else if (strcmp(op, "/")  == 0) {
            if (r->number == 0) runtime_error(n->line, "Division by zero");
            res = val_number(l->number / r->number);
        }
        else if (strcmp(op, "%")  == 0) {
            if (r->number == 0) runtime_error(n->line, "Modulo by zero");
            res = val_number(fmod(l->number, r->number));
        }
        else if (strcmp(op, "=")  == 0) {
            if (l->type == VAL_NUMBER && r->type == VAL_NUMBER)
                res = val_bool(l->number == r->number);
            else if (l->type == VAL_STRING && r->type == VAL_STRING)
                res = val_bool(strcmp(l->string, r->string) == 0);
            else if (l->type == VAL_BOOL && r->type == VAL_BOOL)
                res = val_bool(l->boolean == r->boolean);
            else res = val_bool(0);
        }
        else if (strcmp(op, "not =") == 0) {
            if (l->type == VAL_NUMBER && r->type == VAL_NUMBER)
                res = val_bool(l->number != r->number);
            else if (l->type == VAL_STRING && r->type == VAL_STRING)
                res = val_bool(strcmp(l->string, r->string) != 0);
            else res = val_bool(1);
        }
        else if (strcmp(op, ">")  == 0) res = val_bool(l->number >  r->number);
        else if (strcmp(op, "<")  == 0) res = val_bool(l->number <  r->number);
        else if (strcmp(op, ">=") == 0) res = val_bool(l->number >= r->number);
        else if (strcmp(op, "<=") == 0) res = val_bool(l->number <= r->number);
        else runtime_error(n->line, "Unknown operator: %s", op);
        val_release(l); val_release(r); return res;
    }

    case NODE_UNOP: {
        if (strcmp(n->sval, "not") == 0) {
            Value *v = eval(n->right, env); int t = val_truthy(v); val_release(v);
            return val_bool(!t);
        }
        if (strcmp(n->sval, "-") == 0) {
            Value *v = eval(n->right, env); double d = v->number; val_release(v);
            return val_number(-d);
        }
        runtime_error(n->line, "Unknown unary op: %s", n->sval);
        return val_null();
    }

    case NODE_IN_RANGE: {
        Value *val = eval(n->left, env);
        Value *lo  = eval(n->children.items[0], env);
        Value *hi  = eval(n->children.items[1], env);
        int r = (val->number >= lo->number && val->number <= hi->number);
        val_release(val); val_release(lo); val_release(hi);
        return val_bool(r);
    }

    case NODE_CALL: {
        const char *fname = n->sval;
        int argc = n->children.count;
        Value **args = malloc(sizeof(Value *) * (argc + 1));
        for (int i = 0; i < argc; i++) args[i] = eval(n->children.items[i], env);
        Value *result;
        // User-defined functions take priority over builtins — check env first.
        Value *fn = env_get(env, fname);
        if (fn && fn->type == VAL_FUNCTION) {
            result = call_function(fn, args, argc, NULL, env, n->line);
        } else if (is_builtin_name(fname)) {
            result = call_builtin(fname, args, argc, n->line);
        } else {
            if (!fn) runtime_error(n->line, "Undefined function: %s", fname);
            result = call_function(fn, args, argc, NULL, env, n->line);
        }
        for (int i = 0; i < argc; i++) val_release(args[i]);
        free(args);
        return result;
    }

    case NODE_NEW: {
        Value *cls = env_get(env, n->name);
        if (!cls || cls->type != VAL_CLASS)
            runtime_error(n->line, "Unknown class: %s", n->name);
        Value *obj = val_new(VAL_OBJECT);
        obj->object = calloc(1, sizeof(YieldObject));
        obj->object->class_name = strdup(n->name);
        obj->object->fields = env_new(NULL);
        Value *fire_fn = NULL;
        for (int i = 0; i < cls->class_body.count; i++) {
            Node *md = cls->class_body.items[i];
            if (md->type == NODE_FUNC_DEF && strcmp(md->name, "fire") == 0) {
                Value *fn = val_new(VAL_FUNCTION);
                fn->params = malloc(sizeof(char *) * md->param_count);
                fn->param_count = md->param_count;
                for (int j = 0; j < md->param_count; j++) fn->params[j] = strdup(md->params[j]);
                fn->body = md->children; fn->closure = env; fire_fn = fn; break;
            }
        }
        if (fire_fn) {
            int argc = n->children.count;
            Value **args = malloc(sizeof(Value *) * (argc + 1));
            for (int i = 0; i < argc; i++) args[i] = eval(n->children.items[i], env);
            call_function(fire_fn, args, argc, obj, env, n->line);
            for (int i = 0; i < argc; i++) val_release(args[i]);
            free(args); val_release(fire_fn);
        }
        return obj;
    }

    case NODE_MEMBER: {
        Value *obj = eval(n->left, env);
        if (obj->type != VAL_OBJECT) runtime_error(n->line, "Not an object (.%s)", n->sval);
        Value *field = env_get(obj->object->fields, n->sval);
        if (!field) runtime_error(n->line, "No field: %s", n->sval);
        val_retain(field); val_release(obj); return field;
    }

    case NODE_METHOD_CALL: {
        // ---- key namespace: key.pressed() / key.get() -----
        // Intercept before eval(n->left) since 'key' isn't a real variable.
        if (n->left->type == NODE_IDENT && strcmp(n->left->sval, "key") == 0) {
            const char *m = n->sval;
            if (strcmp(m, "pressed") == 0) {
                return val_bool(_kbhit() != 0);
            }
            if (strcmp(m, "get") == 0) {
                int ch = _getch();
                // On Windows/Linux, arrow keys send an escape sequence:
                //   Windows: 0x00 or 0xE0 followed by a scan code
                //   Linux:   0x1B 0x5B followed by 'A'/'B'/'C'/'D'
                if (ch == 0 || ch == 0xE0) {
                    // Windows extended key — read scan code
                    int scan = _getch();
                    switch (scan) {
                        case 72: return val_string("UP");
                        case 80: return val_string("DOWN");
                        case 75: return val_string("LEFT");
                        case 77: return val_string("RIGHT");
                        default: {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "\\x%02x", scan);
                            return val_string(buf);
                        }
                    }
                }
#ifndef _WIN32
                if (ch == 27) {
                    // Linux/macOS escape sequence for arrow keys
                    int ch2 = _getch();
                    if (ch2 == '[') {
                        int ch3 = _getch();
                        switch (ch3) {
                            case 'A': return val_string("UP");
                            case 'B': return val_string("DOWN");
                            case 'C': return val_string("RIGHT");
                            case 'D': return val_string("LEFT");
                        }
                    }
                    return val_string("ESC");
                }
#endif
                // Common named keys
                if (ch == 13 || ch == 10) return val_string("ENTER");
                if (ch == 32)             return val_string("SPACE");
                if (ch == 27)             return val_string("ESC");
                if (ch == 8 || ch == 127) return val_string("BACKSPACE");
                if (ch == 9)              return val_string("TAB");
                // Printable ASCII — return as single-char string
                if (ch >= 32 && ch <= 126) {
                    char s[2] = { (char)ch, '\0' };
                    return val_string(s);
                }
                // Anything else — return hex representation
                char buf[8];
                snprintf(buf, sizeof(buf), "\\x%02x", ch);
                return val_string(buf);
            }
            runtime_error(n->line, "key has no method '%s' (use key.pressed() or key.get())", m);
        }

        Value *obj = eval(n->left, env);

        // ---- List methods ----------------------------------
        if (obj->type == VAL_LIST) {
            const char *m = n->sval; Value *result = NULL;
            if (strcmp(m, "add") == 0) {
                Value *item = eval(n->children.items[0], env);
                list_push(obj->list, item); val_release(item); result = val_null();
            } else if (strcmp(m, "remove") == 0) {
                Value *item = eval(n->children.items[0], env);
                list_remove(obj->list, item); val_release(item); result = val_null();
            } else if (strcmp(m, "has") == 0) {
                Value *item = eval(n->children.items[0], env); int found = 0;
                for (int i = 0; i < obj->list->count; i++) {
                    Value *it = obj->list->items[i];
                    if (it->type == VAL_NUMBER  && item->type == VAL_NUMBER  && it->number == item->number) { found = 1; break; }
                    if (it->type == VAL_STRING  && item->type == VAL_STRING  && strcmp(it->string, item->string) == 0) { found = 1; break; }
                    if (it->type == VAL_BOOL    && item->type == VAL_BOOL    && it->boolean == item->boolean) { found = 1; break; }
                }
                val_release(item); result = val_bool(found);
            } else if (strcmp(m, "size")  == 0) result = val_number(obj->list->count);
            else if (strcmp(m, "first") == 0) {
                if (obj->list->count == 0) runtime_error(n->line, "list.first() on empty list");
                result = obj->list->items[0]; val_retain(result);
            } else if (strcmp(m, "last") == 0) {
                if (obj->list->count == 0) runtime_error(n->line, "list.last() on empty list");
                result = obj->list->items[obj->list->count - 1]; val_retain(result);
            } else if (strcmp(m, "pop") == 0) {
                if (obj->list->count == 0) runtime_error(n->line, "list.pop() on empty list");
                result = obj->list->items[obj->list->count - 1];
                val_retain(result);
                obj->list->count--;
                val_release(result); // release the list's reference, caller gets retain from above
                val_retain(result);  // net: caller holds 1 ref
            } else if (strcmp(m, "insert") == 0) {
                // insert(index, value)
                if (n->children.count < 2) runtime_error(n->line, "list.insert() needs 2 args");
                Value *idxv = eval(n->children.items[0], env);
                Value *item = eval(n->children.items[1], env);
                int idx = (int)idxv->number;
                if (idx < 0) idx = 0;
                if (idx > obj->list->count) idx = obj->list->count;
                if (obj->list->count >= obj->list->capacity) {
                    obj->list->capacity *= 2;
                    obj->list->items = realloc(obj->list->items, sizeof(Value *) * obj->list->capacity);
                }
                memmove(&obj->list->items[idx + 1], &obj->list->items[idx],
                        sizeof(Value *) * (obj->list->count - idx));
                val_retain(item);
                obj->list->items[idx] = item;
                obj->list->count++;
                val_release(idxv); val_release(item);
                result = val_null();
            } else if (strcmp(m, "index_of") == 0) {
                // index_of(value) → number or -1
                Value *item = eval(n->children.items[0], env); int found = -1;
                for (int i = 0; i < obj->list->count; i++) {
                    Value *it = obj->list->items[i];
                    if (it->type == VAL_NUMBER  && item->type == VAL_NUMBER  && it->number == item->number) { found = i; break; }
                    if (it->type == VAL_STRING  && item->type == VAL_STRING  && strcmp(it->string, item->string) == 0) { found = i; break; }
                    if (it->type == VAL_BOOL    && item->type == VAL_BOOL    && it->boolean == item->boolean) { found = i; break; }
                }
                val_release(item); result = val_number((double)found);
            } else runtime_error(n->line, "Unknown list method: %s", m);
            val_release(obj); return result;
        }

        // ---- String methods --------------------------------
        if (obj->type == VAL_STRING) {
            const char *m = n->sval; Value *result = NULL;
            const char *s = obj->string ? obj->string : "";
            if (strcmp(m, "size") == 0 || strcmp(m, "length") == 0) {
                result = val_number((double)strlen(s));
            } else if (strcmp(m, "upper") == 0) {
                char *r = strdup(s);
                for (int i = 0; r[i]; i++) r[i] = toupper((unsigned char)r[i]);
                result = val_string(r); free(r);
            } else if (strcmp(m, "lower") == 0) {
                char *r = strdup(s);
                for (int i = 0; r[i]; i++) r[i] = tolower((unsigned char)r[i]);
                result = val_string(r); free(r);
            } else if (strcmp(m, "contains") == 0) {
                Value *sub = eval(n->children.items[0], env);
                result = val_bool(strstr(s, sub->string ? sub->string : "") != NULL);
                val_release(sub);
            } else if (strcmp(m, "starts_with") == 0) {
                Value *sub = eval(n->children.items[0], env);
                const char *prefix = sub->string ? sub->string : "";
                result = val_bool(strncmp(s, prefix, strlen(prefix)) == 0);
                val_release(sub);
            } else if (strcmp(m, "ends_with") == 0) {
                Value *sub = eval(n->children.items[0], env);
                const char *suffix = sub->string ? sub->string : "";
                size_t sl = strlen(s), pl = strlen(suffix);
                result = val_bool(sl >= pl && strcmp(s + sl - pl, suffix) == 0);
                val_release(sub);
            } else if (strcmp(m, "trim") == 0) {
                const char *start = s;
                while (*start && isspace((unsigned char)*start)) start++;
                const char *end = s + strlen(s);
                while (end > start && isspace((unsigned char)*(end - 1))) end--;
                char *r = malloc(end - start + 1);
                memcpy(r, start, end - start); r[end - start] = '\0';
                result = val_string(r); free(r);
            } else if (strcmp(m, "split") == 0) {
                Value *delv = eval(n->children.items[0], env);
                const char *delim = delv->string ? delv->string : " ";
                result = val_list();
                char *copy = strdup(s), *tok, *rest = copy;
                size_t dlen = strlen(delim);
                if (dlen == 0) {
                    // split by character
                    for (int i = 0; copy[i]; i++) {
                        char ch[2] = { copy[i], 0 };
                        Value *vs = val_string(ch); list_push(result->list, vs); val_release(vs);
                    }
                } else {
                    while ((tok = strstr(rest, delim)) != NULL) {
                        *tok = '\0';
                        Value *vs = val_string(rest); list_push(result->list, vs); val_release(vs);
                        rest = tok + dlen;
                    }
                    Value *vs = val_string(rest); list_push(result->list, vs); val_release(vs);
                }
                free(copy); val_release(delv);
            } else if (strcmp(m, "replace") == 0) {
                if (n->children.count < 2) runtime_error(n->line, "str.replace() needs 2 args");
                Value *fromv = eval(n->children.items[0], env);
                Value *tov   = eval(n->children.items[1], env);
                const char *from = fromv->string ? fromv->string : "";
                const char *to   = tov->string   ? tov->string   : "";
                size_t flen = strlen(from), tlen = strlen(to);
                if (flen == 0) { result = val_string(s); }
                else {
                    size_t buflen = strlen(s) * 4 + tlen + 16;
                    char *out = malloc(buflen); out[0] = '\0';
                    const char *p = s; size_t used = 0;
                    while (*p) {
                        if (strncmp(p, from, flen) == 0) {
                            if (used + tlen + 1 >= buflen) { buflen *= 2; out = realloc(out, buflen); }
                            memcpy(out + used, to, tlen); used += tlen; p += flen;
                        } else {
                            if (used + 2 >= buflen) { buflen *= 2; out = realloc(out, buflen); }
                            out[used++] = *p++;
                        }
                    }
                    out[used] = '\0';
                    result = val_string(out); free(out);
                }
                val_release(fromv); val_release(tov);
            } else if (strcmp(m, "substr") == 0) {
                // substr(start [, len])
                Value *startv = eval(n->children.items[0], env);
                int start = (int)startv->number;
                int slen = (int)strlen(s);
                if (start < 0) start = 0;
                if (start > slen) start = slen;
                int take = slen - start;
                if (n->children.count >= 2) {
                    Value *lenv2 = eval(n->children.items[1], env);
                    take = (int)lenv2->number;
                    val_release(lenv2);
                }
                if (take < 0) take = 0;
                if (start + take > slen) take = slen - start;
                char *r = malloc(take + 1);
                memcpy(r, s + start, take); r[take] = '\0';
                result = val_string(r); free(r);
                val_release(startv);
            } else runtime_error(n->line, "Unknown string method: %s", m);
            val_release(obj); return result;
        }

        // ---- Object methods --------------------------------
        if (obj->type != VAL_OBJECT) runtime_error(n->line, "Not an object (.%s)", n->sval);
        Value *cls = env_get(env, obj->object->class_name);
        if (!cls) runtime_error(n->line, "Class not found: %s", obj->object->class_name);
        Value *method_fn = NULL;
        for (int i = 0; i < cls->class_body.count; i++) {
            Node *md = cls->class_body.items[i];
            if (md->type == NODE_FUNC_DEF && strcmp(md->name, n->sval) == 0) {
                Value *fn = val_new(VAL_FUNCTION);
                fn->params = malloc(sizeof(char *) * md->param_count);
                fn->param_count = md->param_count;
                for (int j = 0; j < md->param_count; j++) fn->params[j] = strdup(md->params[j]);
                fn->body = md->children; fn->closure = env; method_fn = fn; break;
            }
        }
        if (!method_fn) runtime_error(n->line, "No method: %s", n->sval);
        int argc = n->children.count;
        Value **args = malloc(sizeof(Value *) * (argc + 1));
        for (int i = 0; i < argc; i++) args[i] = eval(n->children.items[i], env);
        Value *result = call_function(method_fn, args, argc, obj, env, n->line);
        for (int i = 0; i < argc; i++) val_release(args[i]);
        free(args); val_release(method_fn); val_release(obj);
        return result;
    }

    case NODE_INDEX: {
        Value *obj = eval(n->left, env);
        Value *idx = eval(n->right, env);
        // List indexing
        if (obj->type == VAL_LIST) {
            int i = (int)idx->number;
            if (i < 0) i = obj->list->count + i;  // negative indexing
            if (i < 0 || i >= obj->list->count)
                runtime_error(n->line, "Index %d out of range (size %d)", i, obj->list->count);
            Value *item = obj->list->items[i]; val_retain(item);
            val_release(obj); val_release(idx); return item;
        }
        // String indexing: s[0] → single-char string
        if (obj->type == VAL_STRING) {
            const char *s = obj->string ? obj->string : "";
            int slen = (int)strlen(s);
            int i = (int)idx->number;
            if (i < 0) i = slen + i;
            if (i < 0 || i >= slen)
                runtime_error(n->line, "String index %d out of range (length %d)", i, slen);
            char ch[2] = { s[i], '\0' };
            Value *result = val_string(ch);
            val_release(obj); val_release(idx); return result;
        }
        runtime_error(n->line, "Cannot index into type %d", obj->type);
        val_release(obj); val_release(idx); return val_null();
    }

    default:
        runtime_error(n->line, "eval(): unhandled node type %d", n->type);
    }
    return val_null();
}

// ============================================================
//  Assignment helper
// ============================================================
static void assign_to(Node *target, Value *val, Env *env, int line) {
    if (target->type == NODE_IDENT) { env_set(env, target->sval, val); return; }
    if (target->type == NODE_MEMBER) {
        Value *obj = eval(target->left, env);
        if (obj->type != VAL_OBJECT) runtime_error(line, "Cannot set field on non-object");
        env_define(obj->object->fields, target->sval, val); val_release(obj); return;
    }
    if (target->type == NODE_INDEX) {
        Value *obj = eval(target->left, env), *idx = eval(target->right, env);
        if (obj->type == VAL_LIST) {
            int i = (int)idx->number;
            if (i < 0) i = obj->list->count + i;
            if (i < 0 || i >= obj->list->count) runtime_error(line, "Index %d out of range", i);
            val_release(obj->list->items[i]); val_retain(val); obj->list->items[i] = val;
        } else runtime_error(line, "Index assign on non-list");
        val_release(obj); val_release(idx); return;
    }
    runtime_error(line, "Invalid assignment target");
}

// ============================================================
//  exec()
// ============================================================
static ExecResult exec(Node *n, Env *env) {
    if (!n) return make_ok();
    switch (n->type) {

    case NODE_VAR_DECL: {
        Value *v = eval(n->right, env); env_define(env, n->name, v); val_release(v); return make_ok();
    }
    case NODE_CONST_DECL: {
        char *up = strdup(n->name);
        for (int i = 0; up[i]; i++) up[i] = toupper((unsigned char)up[i]);
        Value *v = eval(n->right, env); env_define(env, up, v); val_release(v); free(up); return make_ok();
    }
    case NODE_SET: {
        Value *v = eval(n->right, env); assign_to(n->left, v, env, n->line); val_release(v); return make_ok();
    }
    case NODE_ADD_ASSIGN: case NODE_SUB_ASSIGN:
    case NODE_MUL_ASSIGN: case NODE_DIV_ASSIGN:
    case NODE_MOD_ASSIGN: {
        Value *cur = eval(n->left, env), *rhs = eval(n->right, env); double res;
        if (n->type == NODE_ADD_ASSIGN) {
            // String concatenation support for add
            if (cur->type == VAL_STRING || rhs->type == VAL_STRING) {
                char *ls = val_to_string(cur), *rs = val_to_string(rhs);
                char *cat = malloc(strlen(ls) + strlen(rs) + 1);
                strcpy(cat, ls); strcat(cat, rs);
                Value *sv = val_string(cat); free(ls); free(rs); free(cat);
                val_release(cur); val_release(rhs);
                assign_to(n->left, sv, env, n->line); val_release(sv);
                return make_ok();
            }
            res = cur->number + rhs->number;
        }
        else if (n->type == NODE_SUB_ASSIGN) res = cur->number - rhs->number;
        else if (n->type == NODE_MUL_ASSIGN) res = cur->number * rhs->number;
        else if (n->type == NODE_DIV_ASSIGN) {
            if (rhs->number == 0) runtime_error(n->line, "Division by zero");
            res = cur->number / rhs->number;
        }
        else { // MOD_ASSIGN
            if (rhs->number == 0) runtime_error(n->line, "Modulo by zero");
            res = fmod(cur->number, rhs->number);
        }
        val_release(cur); val_release(rhs);
        Value *nv = val_number(res); assign_to(n->left, nv, env, n->line); val_release(nv);
        return make_ok();
    }
    case NODE_OUT: {
        for (int i = 0; i < n->children.count; i++) {
            Value *v = eval(n->children.items[i], env); char *s = val_to_string(v);
            if (i > 0) printf(" ");
            printf("%s", s); free(s); val_release(v);
        }
        printf("\n"); return make_ok();
    }
    case NODE_INPUT: {
        if (n->children.count >= 2) {
            Value *pr = eval(n->children.items[1], env); char *s = val_to_string(pr);
            printf("%s", s); fflush(stdout); free(s); val_release(pr);
        }
        char buf[1024]; if (!fgets(buf, sizeof(buf), stdin)) buf[0] = '\0';
        int len = (int)strlen(buf); if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
        Value *result = val_string(buf);
        assign_to(n->children.items[0], result, env, n->line); val_release(result);
        return make_ok();
    }
    case NODE_IF: {
        for (int i = 0; i < n->branch_count; i++) {
            IfBranch *b = &n->branches[i]; int run = 1;
            if (b->condition) { Value *c = eval(b->condition, env); run = val_truthy(c); val_release(c); }
            if (run) { Env *e = env_new(env); ExecResult r = exec_block(&b->body, e); env_free(e); return r; }
        }
        return make_ok();
    }
    case NODE_RUN_FOREVER: {
        while (1) {
            Env *e = env_new(env); ExecResult r = exec_block(&n->children, e); env_free(e);
            if (r.signal == SIG_STOP) break;
            if (r.signal == SIG_RETURN) return r;
            // SIG_SKIP → continue loop
        }
        return make_ok();
    }
    case NODE_RUN_TIMES: {
        Value *cv = eval(n->left, env); int count = (int)cv->number; val_release(cv);
        for (int i = 0; i < count; i++) {
            Env *e = env_new(env); ExecResult r = exec_block(&n->children, e); env_free(e);
            if (r.signal == SIG_STOP) break;
            if (r.signal == SIG_RETURN) return r;
        }
        return make_ok();
    }
    case NODE_RUN_INDEX: {
        Value *lv = eval(n->left, env); int lim = (int)lv->number; val_release(lv);
        for (int i = 0; i < lim; i++) {
            Env *e = env_new(env); Value *iv = val_number(i);
            env_define(e, n->loop_var, iv); val_release(iv);
            ExecResult r = exec_block(&n->children, e); env_free(e);
            if (r.signal == SIG_STOP) break;
            if (r.signal == SIG_RETURN) return r;
        }
        return make_ok();
    }
    case NODE_RUN_LIST: {
        // run(i, list):  — at runtime, if the value is a number treat as RUN_INDEX
        Value *listval = eval(n->left, env);
        if (listval->type == VAL_NUMBER) {
            // Treat as index loop: run(i, n)
            int lim = (int)listval->number; val_release(listval);
            for (int i = 0; i < lim; i++) {
                Env *e = env_new(env); Value *iv = val_number(i);
                env_define(e, n->loop_var, iv); val_release(iv);
                ExecResult r = exec_block(&n->children, e); env_free(e);
                if (r.signal == SIG_STOP) break;
                if (r.signal == SIG_RETURN) return r;
            }
            return make_ok();
        }
        if (listval->type == VAL_STRING) {
            // Iterate over characters
            const char *s = listval->string ? listval->string : "";
            int slen = (int)strlen(s);
            for (int i = 0; i < slen; i++) {
                char ch[2] = { s[i], '\0' };
                Env *e = env_new(env); Value *cv = val_string(ch);
                env_define(e, n->loop_var, cv); val_release(cv);
                ExecResult r = exec_block(&n->children, e); env_free(e);
                if (r.signal == SIG_STOP) break;
                if (r.signal == SIG_RETURN) { val_release(listval); return r; }
            }
            val_release(listval); return make_ok();
        }
        if (listval->type != VAL_LIST)
            runtime_error(n->line, "run(i, x): x must be a list, number, or string");
        for (int i = 0; i < listval->list->count; i++) {
            Env *e = env_new(env);
            env_define(e, n->loop_var, listval->list->items[i]);
            ExecResult r = exec_block(&n->children, e); env_free(e);
            if (r.signal == SIG_STOP) break;
            if (r.signal == SIG_RETURN) { val_release(listval); return r; }
        }
        val_release(listval); return make_ok();
    }
    case NODE_RUN_WHILE: {
        while (1) {
            Value *c = eval(n->left, env); int go = val_truthy(c); val_release(c);
            if (!go) break;
            Env *e = env_new(env); ExecResult r = exec_block(&n->children, e); env_free(e);
            if (r.signal == SIG_STOP) break;
            if (r.signal == SIG_RETURN) return r;
        }
        return make_ok();
    }
    case NODE_STOP:   return make_stop();
    case NODE_SKIP:   return make_skip();

    case NODE_FUNC_DEF: {
        Value *fn = val_new(VAL_FUNCTION);
        fn->params = malloc(sizeof(char *) * n->param_count);
        fn->param_count = n->param_count;
        for (int i = 0; i < n->param_count; i++) fn->params[i] = strdup(n->params[i]);
        fn->body = n->children; fn->closure = env;
        env_define(env, n->name, fn); val_release(fn); return make_ok();
    }
    case NODE_RETURN: {
        Value *v = eval(n->right, env); return make_return(v);
    }
    case NODE_CLASS_DEF: {
        Value *cls = val_new(VAL_CLASS);
        cls->class_name = strdup(n->name); cls->class_body = n->children;
        env_define(env, n->name, cls); val_release(cls); return make_ok();
    }
    case NODE_ERROR_THROW: {
        char msg[1024] = "";
        for (int i = 0; i < n->children.count; i++) {
            Value *v = eval(n->children.items[i], env); char *s = val_to_string(v);
            if (i > 0) strncat(msg, " ", sizeof(msg) - strlen(msg) - 1);
            strncat(msg, s, sizeof(msg) - strlen(msg) - 1);
            free(s); val_release(v);
        }
        if (g_in_error_block > 0) throw_error("%s", msg);
        fprintf(stderr, "Error: %s\n", msg); exit(1);
    }
    case NODE_ERROR_BLOCK: {
        jmp_buf saved; memcpy(saved, g_error_jump, sizeof(jmp_buf));
        g_in_error_block++;
        ExecResult result = make_ok();
        if (setjmp(g_error_jump) == 0) {
            Env *try_env = env_new(env);
            result = exec_block(&n->error_body, try_env);
            env_free(try_env);
        } else {
            Env *catch_env = env_new(env);
            if (n->catch_var) {
                Value *err = val_string(g_error_msg);
                env_define(catch_env, n->catch_var, err); val_release(err);
            }
            result = exec_block(&n->catch_body, catch_env);
            env_free(catch_env);
        }
        g_in_error_block--;
        memcpy(g_error_jump, saved, sizeof(jmp_buf));
        return result;
    }
    case NODE_LOAD: {
        load_module(n->sval, env, n->line);
        return make_ok();
    }
    case NODE_PLUGIN:
        // Python plugins not supported in C interpreter — silently ignore
        return make_ok();

    case NODE_CALL: case NODE_METHOD_CALL: {
        Value *v = eval(n, env); val_release(v); return make_ok();
    }

    default:
        runtime_error(n->line, "exec(): unhandled node %d", n->type);
    }
    return make_ok();
}

static ExecResult exec_block(NodeList *body, Env *env) {
    for (int i = 0; i < body->count; i++) {
        ExecResult r = exec(body->items[i], env);
        if (r.signal != SIG_NONE) return r;
    }
    return make_ok();
}

void interpret(Node *program) {
    srand((unsigned)time(NULL));
    Env *global = env_new(NULL);
    exec_block(&program->children, global);
    env_free(global);
    module_registry_free();
}