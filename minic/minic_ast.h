/*
 * MINIC - Contrato da AST (versão C).
 * Equivale às tuplas descritas em minic_parser.py. É o único ponto de
 * contato entre o parser (que produz) e o analisador semântico (que consome).
 */
#ifndef MINIC_AST_H
#define MINIC_AST_H

#include <stdlib.h>

typedef enum { T_INT, T_FLOAT, T_BOOL, T_VOID, T_ERRO } Base;

/* Tipo = tipo-base + indicador de vetor (equivale a ('arr', T) em Python). */
typedef struct {
    Base base;
    int vetor;
} Tipo;

typedef enum {
    N_PROG,   /* lista: declarações de topo (N_FUNC / N_VAR)                  */
    N_FUNC,   /* tipo=retorno, nome, lista=parâmetros (N_VAR), a=corpo (N_BLOCK) */
    N_VAR,    /* tipo, nome, tam (-1 se não tem tamanho)                      */
    N_BLOCK,  /* lista: itens (N_VAR ou comandos)                             */
    N_IF,     /* a=cond, b=então, c=senão (ou NULL)                           */
    N_WHILE,  /* a=cond, b=corpo                                              */
    N_RETURN, /* a=expressão (ou NULL)                                        */
    N_EXPR,   /* a=expressão                                                  */
    N_EMPTY,
    N_NUM,    /* tipo.base = T_INT ou T_FLOAT                                 */
    N_BOOL,
    N_ID,     /* nome                                                         */
    N_INDEX,  /* a=base, b=índice                                             */
    N_CALL,   /* nome, lista=argumentos                                       */
    N_UN,     /* op, a                                                        */
    N_BIN,    /* op, a, b                                                     */
    N_ASSIGN  /* a=alvo, b=valor                                              */
} Kind;

typedef struct No {
    Kind kind;
    int linha;
    char *nome;
    Tipo tipo;
    long long tam;
    char op[3];
    struct No *a, *b, *c;
    struct No **lista;
    int n, cap;
} No;

static inline No *no_novo(Kind kind, int linha) {
    No *x = (No *)calloc(1, sizeof(No));
    if (!x) { abort(); }
    x->kind = kind;
    x->linha = linha;
    x->tam = -1;
    return x;
}

static inline void no_add(No *pai, No *filho) {
    if (pai->n == pai->cap) {
        pai->cap = pai->cap ? pai->cap * 2 : 4;
        pai->lista = (No **)realloc(pai->lista, (size_t)pai->cap * sizeof(No *));
        if (!pai->lista) { abort(); }
    }
    pai->lista[pai->n++] = filho;
}

#endif
