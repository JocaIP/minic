# -*- coding: utf-8 -*-
"""
MINIC - Scanner (análise léxica).

Contrato de saída: lista de tokens (tipo, lexema, linha), onde tipo é um de
'int', 'float', 'id', 'kw', 'op' ou 'eof' (o último token é sempre 'eof').
Este módulo não depende do parser nem do analisador semântico.
"""
import re

TIPOS = {"int", "float", "bool", "void"}
RESERVADAS = TIPOS | {"if", "else", "while", "return", "true", "false"}

TOKEN_RE = re.compile(
    r"""
    (?P<ws>[ \t\r\n]+)
  | (?P<lc>//[^\n]*)
  | (?P<bc>/\*.*?\*/)
  | (?P<float>\d+\.\d+)
  | (?P<int>\d+)
  | (?P<id>[A-Za-z_][A-Za-z_0-9]*)
  | (?P<op>&&|\|\||==|!=|<=|>=|[-+*/%<>=!(){}\[\];,])
    """,
    re.X | re.S,
)


class ErroLexico(Exception):
    def __init__(self, linha, msg):
        super().__init__(msg)
        self.linha = linha
        self.msg = msg


def tokenizar(texto):
    tokens = []
    pos = 0
    linha = 1
    while pos < len(texto):
        m = TOKEN_RE.match(texto, pos)
        if not m:
            raise ErroLexico(linha, "caractere inválido '%s'" % texto[pos])
        tipo = m.lastgroup
        lexema = m.group()
        if tipo in ("ws", "lc", "bc"):
            pass
        elif tipo == "id":
            tokens.append(("kw" if lexema in RESERVADAS else "id", lexema, linha))
        else:
            tokens.append((tipo, lexema, linha))
        linha += lexema.count("\n")
        pos = m.end()
    tokens.append(("eof", "", linha))
    return tokens
