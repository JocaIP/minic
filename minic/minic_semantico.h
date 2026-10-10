/*
 * MINIC - Analisador semântico, interface.
 * Consome a AST. Não depende do scanner nem do parser.
 */
#ifndef MINIC_SEMANTICO_H
#define MINIC_SEMANTICO_H

#include "minic_ast.h"

typedef struct {
    char **msgs; /* mensagens já ordenadas por linha (ordenação estável) */
    int n;
} Erros;

Erros analisar(const No *programa);

#endif
