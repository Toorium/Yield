#ifndef LEXER_H
#define LEXER_H

// ============================================================
//  Yield v1.2.0 — Lexer
//  Tokenizes .yd source code into a flat array of Token structs
// ============================================================

// ----- Token types ------------------------------------------
typedef enum {
    // Literals
    TOK_NUMBER,         // 42  3.14
    TOK_STRING,         // "hello"
    TOK_BOOL,           // True  False

    // Identifiers & keywords
    TOK_IDENT,          // foo  myVar
    TOK_VAR,            // var
    TOK_CONST,          // const
    TOK_SET,            // set
    TOK_ADD,            // add
    TOK_SUB,            // sub
    TOK_MUL,            // mul
    TOK_DIV,            // div
    TOK_MOD,            // mod
    TOK_OUT,            // out
    TOK_INPUT,          // input
    TOK_IF,             // if
    TOK_ELSEIF,         // elseif
    TOK_ELSE,           // else
    TOK_END,            // end
    TOK_RUN,            // run
    TOK_STOP,           // stop
    TOK_SKIP,           // skip
    TOK_FUNC,           // func
    TOK_YIELD,          // yield
    TOK_CLASS,          // class
    TOK_NEW,            // new
    TOK_SELF,           // self
    TOK_ERROR,          // error
    TOK_CATCH,          // catch
    TOK_LOAD,           // load
    TOK_PLUGIN,         // plugin
    TOK_WAIT,           // wait
    TOK_CHANCE,         // chance
    TOK_UPPER,          // upper
    TOK_LOWER,          // lower
    TOK_LENGTH,         // length
    TOK_REVERSE,        // reverse
    TOK_INT,            // int
    TOK_FLOAT,          // float
    TOK_STR,            // str
    TOK_AND,            // and
    TOK_OR,             // or
    TOK_NOT,            // not
    TOK_IN,             // in
    TOK_FIRE,           // fire  (constructor name — reserved)

    // Operators & punctuation
    TOK_EQ,             // =
    TOK_NEQ,            // not =   (lexed as a pair)
    TOK_GT,             // >
    TOK_LT,             // <
    TOK_GTE,            // >=
    TOK_LTE,            // <=
    TOK_PLUS,           // +
    TOK_MINUS,          // -
    TOK_STAR,           // *
    TOK_SLASH,          // /
    TOK_PERCENT,        // %  (mod operator)
    TOK_LPAREN,         // (
    TOK_RPAREN,         // )
    TOK_LBRACKET,       // [
    TOK_RBRACKET,       // ]
    TOK_COMMA,          // ,
    TOK_DOT,            // .
    TOK_COLON,          // :
    TOK_NEWLINE,        // \n  (significant in Yield)

    TOK_EOF
} YieldTokenType;

// ----- Token struct -----------------------------------------
typedef struct {
    YieldTokenType type;
    char     *value;    // heap-allocated copy of the raw text
    int       line;     // 1-based source line
} Token;

// ----- Token array (dynamic) --------------------------------
typedef struct {
    Token *tokens;
    int    count;
    int    capacity;
} TokenArray;

// ----- Public API -------------------------------------------

// Lex an entire source string; returns filled TokenArray.
// Caller must free with token_array_free().
TokenArray lex(const char *source);

// Free all memory owned by a TokenArray.
void token_array_free(TokenArray *ta);

// Print all tokens to stdout (for debugging).
void token_array_print(const TokenArray *ta);

// Human-readable name for a YieldTokenType.
const char *token_type_name(YieldTokenType t);

#endif // LEXER_H
