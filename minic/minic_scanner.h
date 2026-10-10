/*
 * MINIC - Scanner (análise léxica), interface.
 * Produz um vetor de tokens terminado por TK_EOF. Não depende do parser.
 */
#ifndef MINIC_SCANNER_H
#define MINIC_SCANNER_H

#include "minic_erro.h"

typedef enum { TK_INT, TK_FLOAT, TK_ID, TK_KW, TK_OP, TK_EOF } TokKind;

typedef struct {
    TokKind kind;
    char *val;
    int linha;
} Token;

typedef struct {
    Token *v;
    int n;
} Tokens;

/* Retorna 1 em caso de sucesso; 0 e preenche *err em caso de erro léxico. */
int tokenizar(const char *texto, Tokens *out, ErroFront *err);

#endif
