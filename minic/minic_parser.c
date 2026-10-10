/*
 * MINIC - Parser (análise sintática, descida recursiva).
 * Equivalente a minic_parser.py (mesma gramática, mesmas mensagens).
 */
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minic_parser.h"

typedef struct {
    const Token *t;
    int n;
    int p;
    jmp_buf jb;
    ErroFront *err;
} P;

typedef No *(*Regra)(P *);

static char *dup_str(const char *s) {
    size_t n = strlen(s);
    char *r = (char *)malloc(n + 1);
    if (!r) abort();
    memcpy(r, s, n + 1);
    return r;
}

static void falha(P *ps, int linha, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    ps->err->linha = linha;
    vsnprintf(ps->err->msg, sizeof(ps->err->msg), fmt, ap);
    va_end(ap);
    longjmp(ps->jb, 1);
}

static const Token *peek(P *ps, int k) {
    int i = ps->p + k;
    if (i > ps->n - 1) i = ps->n - 1;
    return &ps->t[i];
}

static const Token *avancar(P *ps) {
    const Token *tok = &ps->t[ps->p];
    if (tok->kind != TK_EOF) ps->p++;
    return tok;
}

static int eh_k(P *ps, const char *valor, int k) {
    const Token *tok = peek(ps, k);
    return (tok->kind == TK_OP || tok->kind == TK_KW) && strcmp(tok->val, valor) == 0;
}

static int eh(P *ps, const char *valor) { return eh_k(ps, valor, 0); }

static const Token *aceitar(P *ps, const char *valor) {
    return eh(ps, valor) ? avancar(ps) : NULL;
}

/* "fim do arquivo" ou 'lexema' */
static void achado(const Token *tok, char *buf, size_t n) {
    if (tok->kind == TK_EOF) snprintf(buf, n, "fim do arquivo");
    else snprintf(buf, n, "'%s'", tok->val);
}

static const Token *esperar(P *ps, const char *valor) {
    if (eh(ps, valor)) return avancar(ps);
    const Token *tok = peek(ps, 0);
    char a[256];
    achado(tok, a, sizeof a);
    falha(ps, tok->linha, "esperado '%s', encontrado %s", valor, a);
    return NULL;
}

static const Token *esperar_id(P *ps) {
    const Token *tok = peek(ps, 0);
    if (tok->kind != TK_ID) {
        char a[256];
        achado(tok, a, sizeof a);
        falha(ps, tok->linha, "esperado identificador, encontrado %s", a);
    }
    return avancar(ps);
}

static int eh_tipo(P *ps) {
    const Token *tok = peek(ps, 0);
    return tok->kind == TK_KW &&
           (!strcmp(tok->val, "int") || !strcmp(tok->val, "float") ||
            !strcmp(tok->val, "bool") || !strcmp(tok->val, "void"));
}

static Base base_de(const char *s) {
    if (!strcmp(s, "int")) return T_INT;
    if (!strcmp(s, "float")) return T_FLOAT;
    if (!strcmp(s, "bool")) return T_BOOL;
    return T_VOID;
}

static No *expressao(P *ps);
static No *comando(P *ps);
static No *bloco(P *ps);

/* declaração de variáveis: int a, b[3], c;  -> nós N_VAR adicionados a pai */
static void resto_var(P *ps, Base base, const Token *nome_tok, No *pai) {
    for (;;) {
        No *v = no_novo(N_VAR, nome_tok->linha);
        v->nome = dup_str(nome_tok->val);
        v->tipo.base = base;
        if (aceitar(ps, "[")) {
            const Token *tok = peek(ps, 0);
            if (tok->kind != TK_INT)
                falha(ps, tok->linha, "tamanho do vetor deve ser uma constante inteira");
            /* saturação evita overflow em literais enormes */
            long long val = 0;
            for (const char *s = avancar(ps)->val; *s; s++) {
                if (val < 100000000000000000LL) val = val * 10 + (*s - '0');
            }
            v->tam = val;
            v->tipo.vetor = 1;
            esperar(ps, "]");
        }
        no_add(pai, v);
        if (aceitar(ps, ",")) {
            nome_tok = esperar_id(ps);
            continue;
        }
        break;
    }
    esperar(ps, ";");
}

static No *parametro(P *ps) {
    if (!eh_tipo(ps)) {
        const Token *tok = peek(ps, 0);
        falha(ps, tok->linha, "esperado tipo de parâmetro, encontrado '%s'", tok->val);
    }
    Base base = base_de(avancar(ps)->val);
    const Token *nome = esperar_id(ps);
    No *v = no_novo(N_VAR, nome->linha);
    v->nome = dup_str(nome->val);
    v->tipo.base = base;
    if (aceitar(ps, "[")) {
        esperar(ps, "]");
        v->tipo.vetor = 1;
    }
    return v;
}

static No *funcao(P *ps, Base ret, const Token *nome_tok) {
    No *f = no_novo(N_FUNC, nome_tok->linha);
    f->nome = dup_str(nome_tok->val);
    f->tipo.base = ret;
    esperar(ps, "(");
    if (eh(ps, "void") && eh_k(ps, ")", 1)) {
        avancar(ps);
    } else if (!eh(ps, ")")) {
        no_add(f, parametro(ps));
        while (aceitar(ps, ",")) no_add(f, parametro(ps));
    }
    esperar(ps, ")");
    f->a = bloco(ps);
    return f;
}

static void declaracao_topo(P *ps, No *prog) {
    if (!eh_tipo(ps)) {
        const Token *tok = peek(ps, 0);
        falha(ps, tok->linha, "esperado tipo, encontrado '%s'", tok->val);
    }
    Base base = base_de(avancar(ps)->val);
    const Token *nome = esperar_id(ps);
    if (eh(ps, "(")) no_add(prog, funcao(ps, base, nome));
    else resto_var(ps, base, nome, prog);
}

static No *bloco(P *ps) {
    const Token *ab = esperar(ps, "{");
    No *b = no_novo(N_BLOCK, ab->linha);
    while (!eh(ps, "}")) {
        if (peek(ps, 0)->kind == TK_EOF)
            falha(ps, peek(ps, 0)->linha, "bloco não fechado: esperado '}'");
        if (eh_tipo(ps)) {
            Base base = base_de(avancar(ps)->val);
            const Token *nome = esperar_id(ps);
            resto_var(ps, base, nome, b);
        } else {
            no_add(b, comando(ps));
        }
    }
    esperar(ps, "}");
    return b;
}

static No *comando(P *ps) {
    int linha = peek(ps, 0)->linha;
    if (eh(ps, "{")) return bloco(ps);
    if (eh(ps, "if")) {
        avancar(ps);
        esperar(ps, "(");
        No *n = no_novo(N_IF, linha);
        n->a = expressao(ps);
        esperar(ps, ")");
        n->b = comando(ps);
        n->c = aceitar(ps, "else") ? comando(ps) : NULL;
        return n;
    }
    if (eh(ps, "while")) {
        avancar(ps);
        esperar(ps, "(");
        No *n = no_novo(N_WHILE, linha);
        n->a = expressao(ps);
        esperar(ps, ")");
        n->b = comando(ps);
        return n;
    }
    if (eh(ps, "return")) {
        avancar(ps);
        No *n = no_novo(N_RETURN, linha);
        n->a = eh(ps, ";") ? NULL : expressao(ps);
        esperar(ps, ";");
        return n;
    }
    if (aceitar(ps, ";")) return no_novo(N_EMPTY, linha);
    No *n = no_novo(N_EXPR, linha);
    n->a = expressao(ps);
    esperar(ps, ";");
    return n;
}

/* ---- expressões ------------------------------------------------------- */
static No *binario(P *ps, const char *const *ops, Regra proximo) {
    No *esq = proximo(ps);
    for (;;) {
        const Token *tok = peek(ps, 0);
        int achou = 0;
        if (tok->kind == TK_OP)
            for (int i = 0; ops[i]; i++)
                if (strcmp(tok->val, ops[i]) == 0) { achou = 1; break; }
        if (!achou) break;
        const Token *op = avancar(ps);
        No *dir = proximo(ps);
        No *n = no_novo(N_BIN, op->linha);
        snprintf(n->op, sizeof n->op, "%s", op->val);
        n->a = esq;
        n->b = dir;
        esq = n;
    }
    return esq;
}

static No *primario(P *ps);
static No *posfixo(P *ps);
static No *unario(P *ps);

static No *termo(P *ps) {
    static const char *const ops[] = {"*", "/", "%", NULL};
    return binario(ps, ops, unario);
}
static No *soma(P *ps) {
    static const char *const ops[] = {"+", "-", NULL};
    return binario(ps, ops, termo);
}
static No *relacional(P *ps) {
    static const char *const ops[] = {"<", ">", "<=", ">=", NULL};
    return binario(ps, ops, soma);
}
static No *igualdade(P *ps) {
    static const char *const ops[] = {"==", "!=", NULL};
    return binario(ps, ops, relacional);
}
static No *e_logico(P *ps) {
    static const char *const ops[] = {"&&", NULL};
    return binario(ps, ops, igualdade);
}
static No *ou(P *ps) {
    static const char *const ops[] = {"||", NULL};
    return binario(ps, ops, e_logico);
}

static No *expressao(P *ps) {
    No *esq = ou(ps);
    if (eh(ps, "=")) {
        const Token *op = avancar(ps);
        No *valor = expressao(ps); /* associatividade à direita */
        No *n = no_novo(N_ASSIGN, op->linha);
        n->a = esq;
        n->b = valor;
        return n;
    }
    return esq;
}

static No *unario(P *ps) {
    const Token *tok = peek(ps, 0);
    if (tok->kind == TK_OP && (!strcmp(tok->val, "!") || !strcmp(tok->val, "-"))) {
        const Token *op = avancar(ps);
        No *n = no_novo(N_UN, op->linha);
        snprintf(n->op, sizeof n->op, "%s", op->val);
        n->a = unario(ps);
        return n;
    }
    return posfixo(ps);
}

static No *posfixo(P *ps) {
    No *e = primario(ps);
    while (eh(ps, "[")) {
        const Token *ab = avancar(ps);
        No *idx = expressao(ps);
        esperar(ps, "]");
        No *n = no_novo(N_INDEX, ab->linha);
        n->a = e;
        n->b = idx;
        e = n;
    }
    return e;
}

static No *primario(P *ps) {
    const Token *tok = peek(ps, 0);
    int linha = tok->linha;
    if (tok->kind == TK_INT || tok->kind == TK_FLOAT) {
        avancar(ps);
        No *n = no_novo(N_NUM, linha);
        n->tipo.base = tok->kind == TK_INT ? T_INT : T_FLOAT;
        return n;
    }
    if (tok->kind == TK_KW && (!strcmp(tok->val, "true") || !strcmp(tok->val, "false"))) {
        avancar(ps);
        return no_novo(N_BOOL, linha);
    }
    if (tok->kind == TK_ID) {
        avancar(ps);
        if (eh(ps, "(")) {
            avancar(ps);
            No *n = no_novo(N_CALL, linha);
            n->nome = dup_str(tok->val);
            if (!eh(ps, ")")) {
                no_add(n, expressao(ps));
                while (aceitar(ps, ",")) no_add(n, expressao(ps));
            }
            esperar(ps, ")");
            return n;
        }
        No *n = no_novo(N_ID, linha);
        n->nome = dup_str(tok->val);
        return n;
    }
    if (aceitar(ps, "(")) {
        No *e = expressao(ps);
        esperar(ps, ")");
        return e;
    }
    char a[256];
    achado(tok, a, sizeof a);
    falha(ps, linha, "expressão inválida: encontrado %s", a);
    return NULL;
}

No *parsear(const Tokens *tokens, ErroFront *err) {
    P ps;
    ps.t = tokens->v;
    ps.n = tokens->n;
    ps.p = 0;
    ps.err = err;
    if (setjmp(ps.jb) != 0) return NULL;
    No *prog = no_novo(N_PROG, 1);
    while (peek(&ps, 0)->kind != TK_EOF) declaracao_topo(&ps, prog);
    return prog;
}
