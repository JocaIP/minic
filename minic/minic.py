#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MINIC - Driver (versão Python).

Uso: python3 minic.py arquivo.c

Integra, em sequência: scanner -> parser -> analisador semântico.
Cada componente fica em seu próprio módulo e só se comunica com o próximo
por um contrato simples (lista de tokens, AST, lista de erros).

Saída (stdout), sem quebra de linha final:
  * sem erros: "Análise semântica concluída: 0 erros; programa aceito."
  * com erros: uma linha por erro e, no final,
    "Análise semântica concluída: N erros; programa rejeitado."
Código de saída: 0 (aceito), 1 (rejeitado), 2 (uso/arquivo).
"""
import sys

from minic_scanner import tokenizar, ErroLexico
from minic_parser import parsear, ErroSintatico
from minic_semantico import analisar


def emitir(linhas):
    sys.stdout.buffer.write("\n".join(linhas).encode("utf-8"))
    sys.stdout.flush()


def main(argv):
    if len(argv) != 2:
        sys.stderr.write("Uso: %s arquivo.c\n" % argv[0])
        return 2
    try:
        with open(argv[1], "r", encoding="utf-8", errors="replace") as f:
            texto = f.read()
    except OSError as ex:
        sys.stderr.write("Erro: não foi possível abrir '%s': %s\n" % (argv[1], ex.strerror))
        return 2

    try:
        decls = parsear(tokenizar(texto))
    except ErroLexico as ex:
        emitir(["Erro léxico (linha %d): %s" % (ex.linha, ex.msg)])
        return 1
    except ErroSintatico as ex:
        emitir(["Erro sintático (linha %d): %s" % (ex.linha, ex.msg)])
        return 1

    erros = analisar(decls)
    if not erros:
        emitir(["Análise semântica concluída: 0 erros; programa aceito."])
        return 0

    n = len(erros)
    erros.append("Análise semântica concluída: %d %s; programa rejeitado."
                 % (n, "erro" if n == 1 else "erros"))
    emitir(erros)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
