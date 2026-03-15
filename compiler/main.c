// ============================================================
//  Yield v1.2.0 — main.c
//  Usage:
//    yield                    -- interactive REPL
//    yield myfile.yd          -- run a file
//    yield --ast myfile.yd    -- print AST only
//    yield --tokens myfile.yd -- print tokens only
// ============================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "interpreter.h"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open file: %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *buf = malloc(size + 1);
    if (!buf) { perror("malloc"); exit(1); }
    size_t bytes_read = fread(buf, 1, size, f);
    (void)bytes_read;
    buf[size] = '\0';
    fclose(f);
    return buf;
}

// ============================================================
//  REPL — interactive read-eval-print loop
//  Supports multi-line blocks: if a line ends with ':' or the
//  parser fails with an "unexpected EOF" style error we keep
//  reading until a standalone 'end' closes the block.
// ============================================================

// Simple heuristic: count open blocks by looking for block-opening
// keywords ending with ':' and 'end' tokens.
static int count_open_blocks(const char *src) {
    // Lex the source and count unmatched openers
    TokenArray ta = lex(src);
    int depth = 0;
    for (int i = 0; i < ta.count; i++) {
        YieldTokenType t = ta.tokens[i].type;
        if (t == TOK_IF || t == TOK_RUN || t == TOK_FUNC ||
            t == TOK_CLASS || t == TOK_ERROR) {
            // Check that next real token is COLON (block-opening form)
            // For simplicity just increment on these keywords
            depth++;
        } else if (t == TOK_END) {
            depth--;
        }
    }
    token_array_free(&ta);
    return depth;
}

static void run_repl(void) {
    printf("Yield v1.2.0 — Interactive REPL\n");
    printf("Type Yield code and press Enter. Multi-line blocks are supported.\n");
    printf("Type 'exit' or press Ctrl+C to quit.\n\n");

    // We maintain a persistent global environment across REPL lines
    // by re-interpreting a growing source buffer each time — simple
    // but functional for a beginner-oriented language.

    // A cleaner approach: accumulate complete statements and exec them.
    // We use a line buffer + block depth tracker.

    char line[4096];
    char block_buf[1024 * 64]; // accumulated multi-line block
    block_buf[0] = '\0';
    int in_block = 0;

    // We reuse a single Env by wrapping interpret() — but interpret()
    // creates its own env. For REPL we need to carry state forward.
    // Strategy: accumulate ALL successfully executed source in a
    // "history" buffer and re-run from scratch on each new statement.
    // This is simple and correct for beginner-length programs.
    char *history = strdup("");
    size_t history_len = 0;

    while (1) {
        // Print appropriate prompt
        if (in_block)
            printf("...  ");
        else
            printf(">>> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n"); break;
        }

        // Strip trailing newline
        int llen = (int)strlen(line);
        if (llen > 0 && line[llen - 1] == '\n') line[llen - 1] = '\0';

        // Exit command
        if (!in_block && (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0)) {
            printf("Goodbye!\n"); break;
        }

        // Accumulate into block buffer
        if (in_block) {
            strncat(block_buf, "\n", sizeof(block_buf) - strlen(block_buf) - 1);
            strncat(block_buf, line, sizeof(block_buf) - strlen(block_buf) - 1);
        } else {
            strncpy(block_buf, line, sizeof(block_buf) - 1);
            block_buf[sizeof(block_buf) - 1] = '\0';
        }

        // Check block depth to decide whether to execute or keep reading
        int depth = count_open_blocks(block_buf);
        if (depth > 0) {
            in_block = 1;
            continue;
        }
        in_block = 0;

        // Combine history + current block
        size_t blen = strlen(block_buf);
        char *full = malloc(history_len + blen + 4);
        memcpy(full, history, history_len);
        full[history_len] = '\n';
        memcpy(full + history_len + 1, block_buf, blen + 1);

        // Try to parse and run
        TokenArray ta  = lex(full);
        Node      *ast = parse(&ta);
        interpret(ast);
        node_free(ast);
        token_array_free(&ta);

        // Update history
        free(history);
        history = full;
        history_len = strlen(history);

        block_buf[0] = '\0';
    }

    free(history);
}

int main(int argc, char **argv) {
    // No arguments → REPL
    if (argc < 2) {
        run_repl();
        return 0;
    }

    int show_ast    = 0;
    int show_tokens = 0;
    const char *path = argv[1];

    if (strcmp(argv[1], "--ast") == 0)    { show_ast    = 1; path = argc > 2 ? argv[2] : NULL; }
    if (strcmp(argv[1], "--tokens") == 0) { show_tokens = 1; path = argc > 2 ? argv[2] : NULL; }

    if (!path) { fprintf(stderr, "No file specified.\n"); return 1; }
    size_t plen = strlen(path);
    if (plen < 3 || strcmp(path + plen - 3, ".yd") != 0) {
        fprintf(stderr, "Error: file must have .yd extension\n");
        return 1;
    }

    char      *source = read_file(path);
    TokenArray ta     = lex(source);

    if (show_tokens) {
        token_array_print(&ta);
        token_array_free(&ta);
        free(source);
        return 0;
    }

    Node *root = parse(&ta);

    if (show_ast) {
        node_print(root, 0);
        node_free(root);
        token_array_free(&ta);
        free(source);
        return 0;
    }

    interpret(root);

    node_free(root);
    token_array_free(&ta);
    free(source);
    return 0;
}
