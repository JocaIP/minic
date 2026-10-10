/*
 * MINIC - Parser (análise sintática), interface.
 * Consome os tokens do scanner e produz a AST (ver minic_ast.h).
 */
#ifndef MINIC_PARSER_H
#define MINIC_PARSER_H

#include "minic_ast.h"
#include "minic_erro.h"
#include "minic_scanner.h"

/* Retorna a raiz (N_PROG) ou NULL com *err preenchido em caso de erro sintático. */
No *parsear(const Tokens *tokens, ErroFront *err);

#endif
