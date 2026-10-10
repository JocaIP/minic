/* MINIC - Erro de front-end (léxico ou sintático), compartilhado por scanner e parser. */
#ifndef MINIC_ERRO_H
#define MINIC_ERRO_H

typedef struct {
    int linha;
    char msg[512];
} ErroFront;

#endif
