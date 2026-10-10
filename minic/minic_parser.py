# -*- coding: utf-8 -*-
"""
MINIC - Parser (análise sintática, descida recursiva).

Entrada : lista de tokens produzida pelo scanner.
Saída   : AST como lista de declarações de topo (tuplas; o 2º elemento é
          sempre a linha). Este é o contrato com o analisador semântico:

  ('func', linha, ret, nome, params, corpo)   params: [(tipo, nome, linha)]
  ('var', linha, tipo, nome, tamanho)         tipo já inclui ('arr', T)
  ('block', linha, itens)
  ('if', linha, cond, entao, senao)
  ('while', linha, cond, corpo)
  ('return', linha, expr|None)
  ('expr', linha, expr)
  ('empty', linha)
  expressões:
  ('num', linha, 'int'|'float')   ('bool', linha)   ('id', linha, nome)
  ('index', linha, base, idx)     ('call', linha, nome, args)
  ('un', linha, op, e)            ('bin', linha, op, l, r)
  ('assign', linha, alvo, valor)

Gramática (simplificada):
  programa  := (tipo ID ( '(' params ')' bloco | decl_var ))*
  bloco     := '{' ( decl_var_local | comando )* '}'
  comando   := bloco | if | while | return | expr ';' | ';'
  expr      := ou [ '=' expr ]            (atribuição, associativa à direita)
  ou > e > igualdade > relacional > soma > termo > unário > posfixo > primário

Este módulo conhece apenas o contrato de tokens do scanner (não importa
o analisador semântico).
"""
from minic_scanner import TIPOS


class ErroSintatico(Exception):
    def __init__(self, linha, msg):
        super().__init__(msg)
        self.linha = linha
        self.msg = msg


class Parser:
    def __init__(self, tokens):
        self.t = tokens
        self.p = 0

    def peek(self, k=0):
        return self.t[min(self.p + k, len(self.t) - 1)]

    def avancar(self):
        tok = self.t[self.p]
        if tok[0] != "eof":
            self.p += 1
        return tok

    def eh(self, valor, k=0):
        tok = self.peek(k)
        return tok[0] in ("op", "kw") and tok[1] == valor

    def aceitar(self, valor):
        if self.eh(valor):
            return self.avancar()
        return None

    def esperar(self, valor):
        if self.eh(valor):
            return self.avancar()
        tok = self.peek()
        achado = "fim do arquivo" if tok[0] == "eof" else "'%s'" % tok[1]
        raise ErroSintatico(tok[2], "esperado '%s', encontrado %s" % (valor, achado))

    def esperar_id(self):
        tok = self.peek()
        if tok[0] != "id":
            achado = "fim do arquivo" if tok[0] == "eof" else "'%s'" % tok[1]
            raise ErroSintatico(tok[2], "esperado identificador, encontrado %s" % achado)
        return self.avancar()

    def eh_tipo(self, k=0):
        tok = self.peek(k)
        return tok[0] == "kw" and tok[1] in TIPOS

    # programa -------------------------------------------------------------
    def programa(self):
        decls = []
        while self.peek()[0] != "eof":
            decls.extend(self.declaracao_topo())
        return decls

    def declaracao_topo(self):
        if not self.eh_tipo():
            tok = self.peek()
            raise ErroSintatico(tok[2], "esperado tipo, encontrado '%s'" % tok[1])
        tipo = self.avancar()[1]
        nome = self.esperar_id()
        if self.eh("("):
            return [self.funcao(tipo, nome)]
        return self.resto_var(tipo, nome)

    def funcao(self, ret, nome_tok):
        self.esperar("(")
        params = []
        if self.eh("void") and self.eh(")", 1):
            self.avancar()
        elif not self.eh(")"):
            params.append(self.parametro())
            while self.aceitar(","):
                params.append(self.parametro())
        self.esperar(")")
        corpo = self.bloco()
        return ("func", nome_tok[2], ret, nome_tok[1], params, corpo)

    def parametro(self):
        if not self.eh_tipo():
            tok = self.peek()
            raise ErroSintatico(tok[2], "esperado tipo de parâmetro, encontrado '%s'" % tok[1])
        tipo = self.avancar()[1]
        nome = self.esperar_id()
        if self.aceitar("["):
            self.esperar("]")
            tipo = ("arr", tipo)
        return (tipo, nome[1], nome[2])

    # declaração de variáveis: int a, b[3], c;
    def resto_var(self, tipo, nome_tok):
        decls = []
        while True:
            tam = None
            t = tipo
            if self.aceitar("["):
                tok = self.peek()
                if tok[0] != "int":
                    raise ErroSintatico(tok[2], "tamanho do vetor deve ser uma constante inteira")
                tam = int(self.avancar()[1])
                self.esperar("]")
                t = ("arr", tipo)
            decls.append(("var", nome_tok[2], t, nome_tok[1], tam))
            if self.aceitar(","):
                nome_tok = self.esperar_id()
                continue
            break
        self.esperar(";")
        return decls

    # comandos -------------------------------------------------------------
    def bloco(self):
        ab = self.esperar("{")
        itens = []
        while not self.eh("}"):
            if self.peek()[0] == "eof":
                raise ErroSintatico(self.peek()[2], "bloco não fechado: esperado '}'")
            if self.eh_tipo():
                tipo = self.avancar()[1]
                nome = self.esperar_id()
                itens.extend(self.resto_var(tipo, nome))
            else:
                itens.append(self.comando())
        self.esperar("}")
        return ("block", ab[2], itens)

    def comando(self):
        tok = self.peek()
        linha = tok[2]
        if self.eh("{"):
            return self.bloco()
        if self.eh("if"):
            self.avancar()
            self.esperar("(")
            cond = self.expressao()
            self.esperar(")")
            entao = self.comando()
            senao = self.comando() if self.aceitar("else") else None
            return ("if", linha, cond, entao, senao)
        if self.eh("while"):
            self.avancar()
            self.esperar("(")
            cond = self.expressao()
            self.esperar(")")
            return ("while", linha, cond, self.comando())
        if self.eh("return"):
            self.avancar()
            e = None if self.eh(";") else self.expressao()
            self.esperar(";")
            return ("return", linha, e)
        if self.aceitar(";"):
            return ("empty", linha)
        e = self.expressao()
        self.esperar(";")
        return ("expr", linha, e)

    # expressões -----------------------------------------------------------
    def expressao(self):
        esq = self.ou()
        if self.eh("="):
            op = self.avancar()
            valor = self.expressao()  # associatividade à direita
            return ("assign", op[2], esq, valor)
        return esq

    def _binario(self, ops, proximo):
        esq = proximo()
        while self.peek()[0] == "op" and self.peek()[1] in ops:
            op = self.avancar()
            dir_ = proximo()
            esq = ("bin", op[2], op[1], esq, dir_)
        return esq

    def ou(self):
        return self._binario({"||"}, self.e_logico)

    def e_logico(self):
        return self._binario({"&&"}, self.igualdade)

    def igualdade(self):
        return self._binario({"==", "!="}, self.relacional)

    def relacional(self):
        return self._binario({"<", ">", "<=", ">="}, self.soma)

    def soma(self):
        return self._binario({"+", "-"}, self.termo)

    def termo(self):
        return self._binario({"*", "/", "%"}, self.unario)

    def unario(self):
        if self.peek()[0] == "op" and self.peek()[1] in ("!", "-"):
            op = self.avancar()
            return ("un", op[2], op[1], self.unario())
        return self.posfixo()

    def posfixo(self):
        e = self.primario()
        while self.eh("["):
            ab = self.avancar()
            idx = self.expressao()
            self.esperar("]")
            e = ("index", ab[2], e, idx)
        return e

    def primario(self):
        tok = self.peek()
        k, v, linha = tok
        if k == "int":
            self.avancar()
            return ("num", linha, "int")
        if k == "float":
            self.avancar()
            return ("num", linha, "float")
        if k == "kw" and v in ("true", "false"):
            self.avancar()
            return ("bool", linha)
        if k == "id":
            self.avancar()
            if self.eh("("):
                self.avancar()
                args = []
                if not self.eh(")"):
                    args.append(self.expressao())
                    while self.aceitar(","):
                        args.append(self.expressao())
                self.esperar(")")
                return ("call", linha, v, args)
            return ("id", linha, v)
        if self.aceitar("("):
            e = self.expressao()
            self.esperar(")")
            return e
        achado = "fim do arquivo" if k == "eof" else "'%s'" % v
        raise ErroSintatico(linha, "expressão inválida: encontrado %s" % achado)


def parsear(tokens):
    """Ponto de entrada: tokens -> AST (lista de declarações de topo)."""
    return Parser(tokens).programa()
