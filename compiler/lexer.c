// ============================================================
//  Yield v1.2.0 — Lexer implementation
// ============================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"

// ---- Internal helper macros --------------------------------
#define INITIAL_CAPACITY 256

// ---- Keyword table -----------------------------------------
typedef struct { const char *word; YieldTokenType type; } Keyword;

static const Keyword KEYWORDS[] = {
    { "var",     TOK_VAR     },
    { "const",   TOK_CONST   },
    { "set",     TOK_SET     },
    { "add",     TOK_ADD     },
    { "sub",     TOK_SUB     },
    { "mul",     TOK_MUL     },
    { "div",     TOK_DIV     },
    { "mod",     TOK_MOD     },
    { "out",     TOK_OUT     },
    { "input",   TOK_INPUT   },
    { "if",      TOK_IF      },
    { "elseif",  TOK_ELSEIF  },
    { "else",    TOK_ELSE    },
    { "end",     TOK_END     },
    { "run",     TOK_RUN     },
    { "stop",    TOK_STOP    },
    { "skip",    TOK_SKIP    },
    { "func",    TOK_FUNC    },
    { "yield",   TOK_YIELD   },
    { "class",   TOK_CLASS   },
    { "new",     TOK_NEW     },
    { "self",    TOK_SELF    },
    { "error",   TOK_ERROR   },
    { "catch",   TOK_CATCH   },
    { "load",    TOK_LOAD    },
    { "plugin",  TOK_PLUGIN  },
    { "wait",    TOK_WAIT    },
    { "chance",  TOK_CHANCE  },
    { "upper",   TOK_UPPER   },
    { "lower",   TOK_LOWER   },
    { "length",  TOK_LENGTH  },
    { "reverse", TOK_REVERSE },
    { "int",     TOK_INT     },
    { "float",   TOK_FLOAT   },
    { "str",     TOK_STR     },
    { "and",     TOK_AND     },
    { "or",      TOK_OR      },
    { "not",     TOK_NOT     },
    { "in",      TOK_IN      },
    { "fire",    TOK_FIRE    },
    { "True",    TOK_BOOL    },
    { "False",   TOK_BOOL    },
    { NULL,      TOK_EOF     }   // sentinel
};

// ---- TokenArray helpers ------------------------------------
static void ta_init(TokenArray *ta) {
    ta->capacity = INITIAL_CAPACITY;
    ta->count    = 0;
    ta->tokens   = malloc(sizeof(Token) * ta->capacity);
    if (!ta->tokens) { perror("malloc"); exit(1); }
}

static void ta_push(TokenArray *ta, YieldTokenType type, const char *value, int line) {
    if (ta->count >= ta->capacity) {
        ta->capacity *= 2;
        ta->tokens = realloc(ta->tokens, sizeof(Token) * ta->capacity);
        if (!ta->tokens) { perror("realloc"); exit(1); }
    }
    ta->tokens[ta->count].type  = type;
    ta->tokens[ta->count].value = strdup(value);
    ta->tokens[ta->count].line  = line;
    ta->count++;
}

void token_array_free(TokenArray *ta) {
    for (int i = 0; i < ta->count; i++) free(ta->tokens[i].value);
    free(ta->tokens);
    ta->tokens   = NULL;
    ta->count    = 0;
    ta->capacity = 0;
}

// ---- Classify an identifier as keyword or TOK_IDENT --------
static YieldTokenType classify(const char *word) {
    for (int i = 0; KEYWORDS[i].word != NULL; i++) {
        if (strcmp(KEYWORDS[i].word, word) == 0) return KEYWORDS[i].type;
    }
    return TOK_IDENT;
}

// ---- Temp string buffer ------------------------------------
#define BUF_SIZE 4096
static char buf[BUF_SIZE];

// ============================================================
//  Main lex() function
// ============================================================
TokenArray lex(const char *source) {
    TokenArray ta;
    ta_init(&ta);

    int   pos  = 0;
    int   line = 1;
    int   len  = (int)strlen(source);

    int last_was_newline = 1;

    while (pos < len) {
        char c = source[pos];

        // ---- Comments: // to end of line -------------------
        if (c == '/' && pos + 1 < len && source[pos + 1] == '/') {
            while (pos < len && source[pos] != '\n') pos++;
            continue;
        }

        // ---- Newlines (significant) ------------------------
        if (c == '\n') {
            if (!last_was_newline) {
                ta_push(&ta, TOK_NEWLINE, "\\n", line);
                last_was_newline = 1;
            }
            line++;
            pos++;
            continue;
        }

        // ---- Whitespace (skip) -----------------------------
        if (c == ' ' || c == '\t' || c == '\r') {
            pos++;
            continue;
        }

        last_was_newline = 0;

        // ---- String literals "..." -------------------------
        if (c == '"') {
            pos++;
            int bpos = 0;
            while (pos < len && source[pos] != '"') {
                if (source[pos] == '\\' && pos + 1 < len) {
                    char esc = source[pos + 1];
                    if (esc == 'n')       buf[bpos++] = '\n';
                    else if (esc == 't')  buf[bpos++] = '\t';
                    else if (esc == '"')  buf[bpos++] = '"';
                    else if (esc == '\\') buf[bpos++] = '\\';
                    else { buf[bpos++] = '\\'; buf[bpos++] = esc; }
                    pos += 2;
                } else {
                    buf[bpos++] = source[pos++];
                }
                if (bpos >= BUF_SIZE - 1) { fprintf(stderr, "String too long at line %d\n", line); exit(1); }
            }
            buf[bpos] = '\0';
            if (pos < len) pos++;
            ta_push(&ta, TOK_STRING, buf, line);
            continue;
        }

        // ---- Numbers ---------------------------------------
        if (isdigit((unsigned char)c)) {
            int bpos = 0;
            while (pos < len && (isdigit((unsigned char)source[pos]) || source[pos] == '.')) {
                buf[bpos++] = source[pos++];
            }
            buf[bpos] = '\0';
            ta_push(&ta, TOK_NUMBER, buf, line);
            continue;
        }

        // ---- Identifiers and keywords ----------------------
        if (isalpha((unsigned char)c) || c == '_') {
            int bpos = 0;
            while (pos < len && (isalnum((unsigned char)source[pos]) || source[pos] == '_')) {
                buf[bpos++] = source[pos++];
            }
            buf[bpos] = '\0';

            YieldTokenType kw = classify(buf);
            // Special two-word operator: "not ="
            if (kw == TOK_NOT) {
                int peek = pos;
                while (peek < len && (source[peek] == ' ' || source[peek] == '\t')) peek++;
                if (peek < len && source[peek] == '=') {
                    pos = peek + 1;
                    ta_push(&ta, TOK_NEQ, "not =", line);
                    continue;
                }
            }
            ta_push(&ta, kw, buf, line);
            continue;
        }

        // ---- Operators -------------------------------------
        switch (c) {
            case '=':
                ta_push(&ta, TOK_EQ, "=", line);
                pos++;
                break;
            case '>':
                if (pos + 1 < len && source[pos + 1] == '=') {
                    ta_push(&ta, TOK_GTE, ">=", line); pos += 2;
                } else {
                    ta_push(&ta, TOK_GT, ">", line); pos++;
                }
                break;
            case '<':
                if (pos + 1 < len && source[pos + 1] == '=') {
                    ta_push(&ta, TOK_LTE, "<=", line); pos += 2;
                } else {
                    ta_push(&ta, TOK_LT, "<", line); pos++;
                }
                break;
            case '+': ta_push(&ta, TOK_PLUS,     "+",  line); pos++; break;
            case '-': ta_push(&ta, TOK_MINUS,    "-",  line); pos++; break;
            case '*': ta_push(&ta, TOK_STAR,     "*",  line); pos++; break;
            case '/': ta_push(&ta, TOK_SLASH,    "/",  line); pos++; break;
            case '%': ta_push(&ta, TOK_PERCENT,  "%",  line); pos++; break;
            case '(': ta_push(&ta, TOK_LPAREN,   "(",  line); pos++; break;
            case ')': ta_push(&ta, TOK_RPAREN,   ")",  line); pos++; break;
            case '[': ta_push(&ta, TOK_LBRACKET, "[",  line); pos++; break;
            case ']': ta_push(&ta, TOK_RBRACKET, "]",  line); pos++; break;
            case ',': ta_push(&ta, TOK_COMMA,    ",",  line); pos++; break;
            case '.': ta_push(&ta, TOK_DOT,      ".",  line); pos++; break;
            case ':': ta_push(&ta, TOK_COLON,    ":",  line); pos++; break;
            default:
                fprintf(stderr, "Lexer error: unexpected character '%c' at line %d\n", c, line);
                pos++;
                break;
        }
    }

    ta_push(&ta, TOK_EOF, "", line);
    return ta;
}

// ============================================================
//  Debug helpers
// ============================================================
const char *token_type_name(YieldTokenType t) {
    switch (t) {
        case TOK_NUMBER:   return "NUMBER";
        case TOK_STRING:   return "STRING";
        case TOK_BOOL:     return "BOOL";
        case TOK_IDENT:    return "IDENT";
        case TOK_VAR:      return "VAR";
        case TOK_CONST:    return "CONST";
        case TOK_SET:      return "SET";
        case TOK_ADD:      return "ADD";
        case TOK_SUB:      return "SUB";
        case TOK_MUL:      return "MUL";
        case TOK_DIV:      return "DIV";
        case TOK_MOD:      return "MOD";
        case TOK_OUT:      return "OUT";
        case TOK_INPUT:    return "INPUT";
        case TOK_IF:       return "IF";
        case TOK_ELSEIF:   return "ELSEIF";
        case TOK_ELSE:     return "ELSE";
        case TOK_END:      return "END";
        case TOK_RUN:      return "RUN";
        case TOK_STOP:     return "STOP";
        case TOK_SKIP:     return "SKIP";
        case TOK_FUNC:     return "FUNC";
        case TOK_YIELD:    return "YIELD";
        case TOK_CLASS:    return "CLASS";
        case TOK_NEW:      return "NEW";
        case TOK_SELF:     return "SELF";
        case TOK_ERROR:    return "ERROR";
        case TOK_CATCH:    return "CATCH";
        case TOK_LOAD:     return "LOAD";
        case TOK_PLUGIN:   return "PLUGIN";
        case TOK_WAIT:     return "WAIT";
        case TOK_CHANCE:   return "CHANCE";
        case TOK_UPPER:    return "UPPER";
        case TOK_LOWER:    return "LOWER";
        case TOK_LENGTH:   return "LENGTH";
        case TOK_REVERSE:  return "REVERSE";
        case TOK_INT:      return "INT";
        case TOK_FLOAT:    return "FLOAT";
        case TOK_STR:      return "STR";
        case TOK_AND:      return "AND";
        case TOK_OR:       return "OR";
        case TOK_NOT:      return "NOT";
        case TOK_IN:       return "IN";
        case TOK_FIRE:     return "FIRE";
        case TOK_EQ:       return "EQ";
        case TOK_NEQ:      return "NEQ";
        case TOK_GT:       return "GT";
        case TOK_LT:       return "LT";
        case TOK_GTE:      return "GTE";
        case TOK_LTE:      return "LTE";
        case TOK_PLUS:     return "PLUS";
        case TOK_MINUS:    return "MINUS";
        case TOK_STAR:     return "STAR";
        case TOK_SLASH:    return "SLASH";
        case TOK_PERCENT:  return "PERCENT";
        case TOK_LPAREN:   return "LPAREN";
        case TOK_RPAREN:   return "RPAREN";
        case TOK_LBRACKET: return "LBRACKET";
        case TOK_RBRACKET: return "RBRACKET";
        case TOK_COMMA:    return "COMMA";
        case TOK_DOT:      return "DOT";
        case TOK_COLON:    return "COLON";
        case TOK_NEWLINE:  return "NEWLINE";
        case TOK_EOF:      return "EOF";
        default:           return "UNKNOWN";
    }
}

void token_array_print(const TokenArray *ta) {
    for (int i = 0; i < ta->count; i++) {
        printf("[%3d] %-10s  %s\n",
               ta->tokens[i].line,
               token_type_name(ta->tokens[i].type),
               ta->tokens[i].value);
    }
}
