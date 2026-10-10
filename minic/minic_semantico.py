# -*- coding: utf-8 -*-
"""
MINIC - Analisador semântico.

Entrada : AST (lista de declarações de topo, ver contrato em minic_parser.py).
Saída   : lista de erros [(linha, mensagem)] ordenada por linha (estável).
Este módulo não depende do scanner nem do parser: conhece apenas o formato
da AST.

Regras verificadas
  * escopos aninhados; redeclaração no mesmo escopo; variável não declarada;
    sombreamento em bloco interno é permitido;
  * tipos: int, float, bool, void; promoção implícita int -> float;
  * condições de if/while e operandos de &&, ||, ! devem ser bool;
  * comparações (<, >, <=, >=) exigem operandos numéricos; (==, !=) exigem
    ambos numéricos ou ambos bool;
  * vetores: índice int, indexação só de vetor, sem atribuição de vetor inteiro,
    parâmetro vetor exige vetor do mesmo tipo-base;
  * chamadas: função existente, nº e tipos dos argumentos; void só como comando;
  * return compatível com o tipo da função; função não-void deve retornar em
    todos os caminhos;
  * função 'principal' obrigatória, sem parâmetros, retornando int.
"""

ERRO = "erro"  # tipo especial usado para evitar erros em cascata


def nome_tipo(t):
    if isinstance(t, tuple):
        return nome_tipo(t[1]) + "[]"
    return t


def numerico(t):
    return t in ("int", "float")


def atribuivel(destino, origem):
    """int -> float é permitido (promoção numérica); demais: tipos iguais."""
    if destino == ERRO or origem == ERRO:
        return True
    return destino == origem or (destino == "float" and origem == "int")


class Analisador:
    def __init__(self):
        self.erros = []
        self.escopos = [{}]
        self.funcoes = {}
        self.func_atual = None

    def erro(self, linha, msg):
        self.erros.append((linha, "Erro semântico (linha %d): %s" % (linha, msg)))

    # escopos --------------------------------------------------------------
    def abrir(self):
        self.escopos.append({})

    def fechar(self):
        self.escopos.pop()

    def declarar(self, nome, tipo, linha):
        if nome in self.escopos[-1]:
            self.erro(linha, "'%s' já declarado neste escopo" % nome)
            return
        self.escopos[-1][nome] = tipo

    def buscar(self, nome):
        for esc in reversed(self.escopos):
            if nome in esc:
                return esc[nome]
        return None

    # programa -------------------------------------------------------------
    def analisar(self, decls):
        # 1ª passada: registra assinaturas (permite recursão e chamadas
        # a funções definidas mais adiante).
        for d in decls:
            if d[0] == "func":
                _, linha, ret, nome, params, _ = d
                if nome in self.funcoes:
                    self.erro(linha, "função '%s' já definida" % nome)
                else:
                    self.funcoes[nome] = (ret, [p[0] for p in params], linha)
        # 2ª passada: variáveis globais e corpos das funções.
        for d in decls:
            if d[0] == "var":
                self.decl_var(d)
        for d in decls:
            if d[0] == "func":
                self.funcao(d)

        if "principal" not in self.funcoes:
            self.erro(1, "função 'principal' não definida")
        else:
            ret, params, linha = self.funcoes["principal"]
            if params:
                self.erro(linha, "função 'principal' não deve ter parâmetros")
            if ret != "int":
                self.erro(linha, "função 'principal' deve retornar int")

    def decl_var(self, d):
        _, linha, tipo, nome, tam = d
        base = tipo[1] if isinstance(tipo, tuple) else tipo
        if base == "void":
            self.erro(linha, "variável '%s' não pode ser do tipo void" % nome)
        if tam is not None and tam <= 0:
            self.erro(linha, "tamanho do vetor '%s' deve ser positivo" % nome)
        self.declarar(nome, tipo, linha)

    def funcao(self, f):
        _, linha, ret, nome, params, corpo = f
        self.func_atual = (nome, ret)
        self.abrir()  # parâmetros e corpo compartilham o mesmo escopo
        for tipo, pnome, plinha in params:
            base = tipo[1] if isinstance(tipo, tuple) else tipo
            if base == "void":
                self.erro(plinha, "parâmetro '%s' não pode ser do tipo void" % pnome)
            self.declarar(pnome, tipo, plinha)
        for item in corpo[2]:
            self.item(item)
        self.fechar()
        if ret != "void" and not self.sempre_retorna(corpo):
            self.erro(linha, "função '%s' pode terminar sem retornar valor" % nome)
        self.func_atual = None

    def sempre_retorna(self, n):
        k = n[0]
        if k == "return":
            return True
        if k == "block":
            return any(self.sempre_retorna(i) for i in n[2])
        if k == "if":
            return (n[4] is not None
                    and self.sempre_retorna(n[3])
                    and self.sempre_retorna(n[4]))
        return False

    # comandos -------------------------------------------------------------
    def item(self, n):
        if n[0] == "var":
            self.decl_var(n)
        else:
            self.comando(n)

    def comando(self, n):
        k = n[0]
        if k == "block":
            self.abrir()
            for i in n[2]:
                self.item(i)
            self.fechar()
        elif k == "if":
            self.condicao(n[2], "if")
            self.comando(n[3])
            if n[4] is not None:
                self.comando(n[4])
        elif k == "while":
            self.condicao(n[2], "while")
            self.comando(n[3])
        elif k == "return":
            self.retorno(n)
        elif k == "expr":
            self.expr(n[2], permite_void=True)
        # 'empty': nada a fazer

    def condicao(self, e, comando):
        t = self.expr(e)
        if t != ERRO and t != "bool":
            self.erro(e[1], "condição do '%s' deve ser bool, mas é %s" % (comando, nome_tipo(t)))

    def retorno(self, n):
        _, linha, e = n
        nome, ret = self.func_atual
        if e is None:
            if ret != "void":
                self.erro(linha, "função '%s' deve retornar %s" % (nome, ret))
            return
        if ret == "void":
            self.expr(e, permite_void=True)
            self.erro(linha, "função void '%s' não pode retornar valor" % nome)
            return
        t = self.expr(e)
        if not atribuivel(ret, t):
            self.erro(linha, "retorno de tipo %s incompatível com o tipo %s da função '%s'"
                      % (nome_tipo(t), ret, nome))

    # expressões: retornam o tipo (ou ERRO) --------------------------------
    def expr(self, e, permite_void=False):
        t = self._expr(e)
        if t == "void" and not permite_void:
            self.erro(e[1], "expressão do tipo void não pode ser usada como valor")
            return ERRO
        return t

    def _expr(self, e):
        k = e[0]
        linha = e[1]
        if k == "num":
            return e[2]
        if k == "bool":
            return "bool"
        if k == "id":
            t = self.buscar(e[2])
            if t is None:
                if e[2] in self.funcoes:
                    self.erro(linha, "'%s' é uma função, não uma variável" % e[2])
                else:
                    self.erro(linha, "variável '%s' não declarada" % e[2])
                return ERRO
            return t
        if k == "index":
            tb = self.expr(e[2])
            ti = self.expr(e[3])
            if ti != ERRO and ti != "int":
                self.erro(linha, "índice de vetor deve ser int, mas é %s" % nome_tipo(ti))
            if tb == ERRO:
                return ERRO
            if not isinstance(tb, tuple):
                self.erro(linha, "indexação de valor que não é vetor (tipo %s)" % nome_tipo(tb))
                return ERRO
            return tb[1]
        if k == "call":
            return self.chamada(e)
        if k == "un":
            return self.unario(e)
        if k == "bin":
            return self.binario(e)
        if k == "assign":
            return self.atribuicao(e)
        return ERRO

    def chamada(self, e):
        _, linha, nome, args = e
        tipos = [self.expr(a) for a in args]
        if nome not in self.funcoes:
            if self.buscar(nome) is not None:
                self.erro(linha, "'%s' não é uma função" % nome)
            else:
                self.erro(linha, "função '%s' não declarada" % nome)
            return ERRO
        ret, params, _ = self.funcoes[nome]
        if len(args) != len(params):
            self.erro(linha, "função '%s' espera %d argumento(s), mas recebeu %d"
                      % (nome, len(params), len(args)))
            return ret
        for i, (pt, at) in enumerate(zip(params, tipos), 1):
            if at == ERRO:
                continue
            if isinstance(pt, tuple) or isinstance(at, tuple):
                ok = pt == at
            else:
                ok = atribuivel(pt, at)
            if not ok:
                self.erro(linha, "argumento %d da função '%s': esperado %s, encontrado %s"
                          % (i, nome, nome_tipo(pt), nome_tipo(at)))
        return ret

    def unario(self, e):
        _, linha, op, x = e
        t = self.expr(x)
        if t == ERRO:
            return ERRO
        if op == "!":
            if t != "bool":
                self.erro(linha, "operador '!' requer operando bool, mas recebeu %s" % nome_tipo(t))
                return ERRO
            return "bool"
        if not numerico(t):
            self.erro(linha, "operador '-' requer operando numérico, mas recebeu %s" % nome_tipo(t))
            return ERRO
        return t

    def binario(self, e):
        _, linha, op, l, r = e
        tl = self.expr(l)
        tr = self.expr(r)
        rel = op in ("<", ">", "<=", ">=")
        eq = op in ("==", "!=")
        logico = op in ("&&", "||")
        if tl == ERRO or tr == ERRO:
            return "bool" if (rel or eq or logico) else ERRO
        if logico:
            if tl != "bool" or tr != "bool":
                self.erro(linha, "operador '%s' requer operandos bool, mas recebeu %s e %s"
                          % (op, nome_tipo(tl), nome_tipo(tr)))
            return "bool"
        if eq:
            if not ((numerico(tl) and numerico(tr)) or (tl == "bool" and tr == "bool")):
                self.erro(linha, "operador '%s' não pode comparar %s com %s"
                          % (op, nome_tipo(tl), nome_tipo(tr)))
            return "bool"
        if rel:
            if not (numerico(tl) and numerico(tr)):
                self.erro(linha, "operador '%s' requer operandos numéricos, mas recebeu %s e %s"
                          % (op, nome_tipo(tl), nome_tipo(tr)))
            return "bool"
        # aritméticos
        if not (numerico(tl) and numerico(tr)):
            self.erro(linha, "operador '%s' requer operandos numéricos, mas recebeu %s e %s"
                      % (op, nome_tipo(tl), nome_tipo(tr)))
            return ERRO
        if op == "%" and (tl != "int" or tr != "int"):
            self.erro(linha, "operador '%' requer operandos int")
            return ERRO
        return "float" if "float" in (tl, tr) else "int"

    def atribuicao(self, e):
        _, linha, alvo, valor = e
        if alvo[0] not in ("id", "index"):
            self.erro(linha, "lado esquerdo da atribuição não é uma variável")
            self.expr(valor)
            return ERRO
        ta = self.expr(alvo)
        tv = self.expr(valor)
        if ta == ERRO:
            return ERRO
        if isinstance(ta, tuple):
            self.erro(linha, "não é possível atribuir a um vetor inteiro")
            return ERRO
        if not atribuivel(ta, tv):
            self.erro(linha, "atribuição incompatível: %s recebe %s" % (nome_tipo(ta), nome_tipo(tv)))
        return ta


def analisar(decls):
    """Ponto de entrada: AST -> lista de mensagens de erro ordenadas por linha."""
    an = Analisador()
    an.analisar(decls)
    return [msg for _, msg in sorted(an.erros, key=lambda x: x[0])]
