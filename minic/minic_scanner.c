/*
 * MINIC - Scanner (análise léxica). Equivalente a minic_scanner.py.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minic_scanner.h"

static const char *const RESERVADAS[] = {
    "int", "float", "bool", "void", "if", "else", "while", "return", "true", "false", NULL
};

static int eh_reservada(const char *s) {
    for (int i = 0; RESERVADAS[i]; i++)
        if (strcmp(s, RESERVADAS[i]) == 0) return 1;
    return 0;
}

static char *copia(const char *s, size_t n) {
    char *r = (char *)malloc(n + 1);
    if (!r) abort();
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

static int eh_digito(char c) { return c >= '0' && c <= '9'; }
static int eh_inicio_id(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; }
static int eh_parte_id(char c) { return eh_inicio_id(c) || eh_digito(c); }

/* Tamanho (em bytes) do caractere UTF-8 que começa em c. */
static size_t tam_utf8(unsigned char c) {
    if (c < 0x80) return 1;
    if (c >= 0xC0 && c <= 0xDF) return 2;
    if (c >= 0xE0 && c <= 0xEF) return 3;
    if (c >= 0xF0 && c <= 0xF7) return 4;
    return 1;
}

static void empilha(Tokens *t, int *cap, TokKind k, const char *lex, size_t n, int linha) {
    if (t->n == *cap) {
        *cap = *cap ? *cap * 2 : 64;
        t->v = (Token *)realloc(t->v, (size_t)*cap * sizeof(Token));
        if (!t->v) abort();
    }
    t->v[t->n].kind = k;
    t->v[t->n].val = copia(lex, n);
    t->v[t->n].linha = linha;
    t->n++;
}

int tokenizar(const char *texto, Tokens *out, ErroFront *err) {
    size_t len = strlen(texto);
    size_t pos = 0;
    int linha = 1, cap = 0;
    out->v = NULL;
    out->n = 0;

    while (pos < len) {
        char c = texto[pos];
        size_t fim = pos;
        int tipo = 0; /* 0 = descartar; senão TokKind + 1 */

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            while (fim < len && (texto[fim] == ' ' || texto[fim] == '\t' ||
                                 texto[fim] == '\r' || texto[fim] == '\n'))
                fim++;
        } else if (c == '/' && pos + 1 < len && texto[pos + 1] == '/') {
            fim = pos + 2;
            while (fim < len && texto[fim] != '\n') fim++;
        } else if (c == '/' && pos + 1 < len && texto[pos + 1] == '*' &&
                   strstr(texto + pos + 2, "*/") != NULL) {
            /* comentário de bloco (só se estiver fechado; senão '/' é operador) */
            fim = (size_t)(strstr(texto + pos + 2, "*/") - texto) + 2;
        } else if (eh_digito(c)) {
            while (fim < len && eh_digito(texto[fim])) fim++;
            if (fim + 1 < len && texto[fim] == '.' && eh_digito(texto[fim + 1])) {
                fim++;
                while (fim < len && eh_digito(texto[fim])) fim++;
                tipo = TK_FLOAT + 1;
            } else {
                tipo = TK_INT + 1;
            }
        } else if (eh_inicio_id(c)) {
            while (fim < len && eh_parte_id(texto[fim])) fim++;
            char *lex = copia(texto + pos, fim - pos);
            tipo = (eh_reservada(lex) ? TK_KW : TK_ID) + 1;
            free(lex);
        } else {
            char d = (pos + 1 < len) ? texto[pos + 1] : '\0';
            if ((c == '&' && d == '&') || (c == '|' && d == '|') || (c == '=' && d == '=') ||
                (c == '!' && d == '=') || (c == '<' && d == '=') || (c == '>' && d == '=')) {
                fim = pos + 2;
                tipo = TK_OP + 1;
            } else if (strchr("-+*/%<>=!(){}[];,", c) != NULL) {
                fim = pos + 1;
                tipo = TK_OP + 1;
            } else {
                size_t n = tam_utf8((unsigned char)c);
                if (pos + n > len) n = len - pos;
                char *ch = copia(texto + pos, n);
                err->linha = linha;
                snprintf(err->msg, sizeof(err->msg), "caractere inválido '%s'", ch);
                free(ch);
                return 0;
            }
        }

        if (tipo) empilha(out, &cap, (TokKind)(tipo - 1), texto + pos, fim - pos, linha);
        for (size_t i = pos; i < fim; i++)
            if (texto[i] == '\n') linha++;
        pos = fim;
    }
    empilha(out, &cap, TK_EOF, "", 0, linha);
    return 1;
}
