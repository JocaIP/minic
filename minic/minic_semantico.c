/*
 * MINIC - Analisador semântico. Equivalente a minic_semantico.py
 * (mesmas regras, mesma ordem de verificação, mesmas mensagens).
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minic_semantico.h"

/* ---- tipos ------------------------------------------------------------ */
static Tipo T(Base b) {
    Tipo t;
    t.base = b;
    t.vetor = 0;
    return t;
}

static int igual(Tipo a, Tipo b) { return a.base == b.base && a.vetor == b.vetor; }
static int eh_erro(Tipo t) { return t.base == T_ERRO && !t.vetor; }
static int eh_t(Tipo t, Base b) { return t.base == b && !t.vetor; }
static int numerico(Tipo t) { return !t.vetor && (t.base == T_INT || t.base == T_FLOAT); }

static int atribuivel(Tipo destino, Tipo origem) {
    if (eh_erro(destino) || eh_erro(origem)) return 1;
    return igual(destino, origem) || (eh_t(destino, T_FLOAT) && eh_t(origem, T_INT));
}

static const char *nome_base(Base b) {
    switch (b) {
    case T_INT: return "int";
    case T_FLOAT: return "float";
    case T_BOOL: return "bool";
    case T_VOID: return "void";
    default: return "erro";
    }
}

/* buffers rotativos: permite várias chamadas na mesma mensagem */
static const char *nome_tipo(Tipo t) {
    static char buf[4][32];
    static int i = 0;
    i = (i + 1) % 4;
    snprintf(buf[i], sizeof buf[i], "%s%s", nome_base(t.base), t.vetor ? "[]" : "");
    return buf[i];
}

/* ---- estado ----------------------------------------------------------- */
typedef struct {
    int linha;
    char *msg;
} ErroItem;

typedef struct Escopo {
    char **nomes;
    Tipo *tipos;
    int n, cap;
    struct Escopo *pai;
} Escopo;

typedef struct {
    const char *nome;
    Tipo ret;
    int np;
    Tipo *params;
    int linha;
} Func;

typedef struct {
    ErroItem *erros;
    int nerros, caperros;
    Escopo *topo;
    Func *funcs;
    int nfuncs, capfuncs;
    const char *func_nome;
    Tipo func_ret;
} An;

static void erro(An *a, int linha, const char *fmt, ...) {
    char corpo[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(corpo, sizeof corpo, fmt, ap);
    va_end(ap);
    char *msg = (char *)malloc(strlen(corpo) + 64);
    if (!msg) abort();
    sprintf(msg, "Erro semântico (linha %d): %s", linha, corpo);
    if (a->nerros == a->caperros) {
        a->caperros = a->caperros ? a->caperros * 2 : 16;
        a->erros = (ErroItem *)realloc(a->erros, (size_t)a->caperros * sizeof(ErroItem));
        if (!a->erros) abort();
    }
    a->erros[a->nerros].linha = linha;
    a->erros[a->nerros].msg = msg;
    a->nerros++;
}

/* ---- escopos ---------------------------------------------------------- */
static void abrir(An *a) {
    Escopo *e = (Escopo *)calloc(1, sizeof(Escopo));
    if (!e) abort();
    e->pai = a->topo;
    a->topo = e;
}

static void fechar(An *a) { a->topo = a->topo->pai; }

static int no_escopo(const Escopo *e, const char *nome) {
    for (int i = 0; i < e->n; i++)
        if (strcmp(e->nomes[i], nome) == 0) return i;
    return -1;
}

static void declarar(An *a, const char *nome, Tipo tipo, int linha) {
    Escopo *e = a->topo;
    if (no_escopo(e, nome) >= 0) {
        erro(a, linha, "'%s' já declarado neste escopo", nome);
        return;
    }
    if (e->n == e->cap) {
        e->cap = e->cap ? e->cap * 2 : 8;
        e->nomes = (char **)realloc(e->nomes, (size_t)e->cap * sizeof(char *));
        e->tipos = (Tipo *)realloc(e->tipos, (size_t)e->cap * sizeof(Tipo));
        if (!e->nomes || !e->tipos) abort();
    }
    e->nomes[e->n] = (char *)nome;
    e->tipos[e->n] = tipo;
    e->n++;
}

static int buscar(An *a, const char *nome, Tipo *out) {
    for (Escopo *e = a->topo; e; e = e->pai) {
        int i = no_escopo(e, nome);
        if (i >= 0) {
            *out = e->tipos[i];
            return 1;
        }
    }
    return 0;
}

static Func *buscar_func(An *a, const char *nome) {
    for (int i = 0; i < a->nfuncs; i++)
        if (strcmp(a->funcs[i].nome, nome) == 0) return &a->funcs[i];
    return NULL;
}

/* ---- expressões ------------------------------------------------------- */
static Tipo expr(An *a, const No *e, int permite_void);

static Tipo chamada(An *a, const No *e) {
    int linha = e->linha;
    int na = e->n;
    Tipo *tipos = (Tipo *)malloc((size_t)(na ? na : 1) * sizeof(Tipo));
    if (!tipos) abort();
    for (int i = 0; i < na; i++) tipos[i] = expr(a, e->lista[i], 0);

    Func *f = buscar_func(a, e->nome);
    if (!f) {
        Tipo tmp;
        if (buscar(a, e->nome, &tmp)) erro(a, linha, "'%s' não é uma função", e->nome);
        else erro(a, linha, "função '%s' não declarada", e->nome);
        free(tipos);
        return T(T_ERRO);
    }
    if (na != f->np) {
        erro(a, linha, "função '%s' espera %d argumento(s), mas recebeu %d", e->nome, f->np, na);
        free(tipos);
        return f->ret;
    }
    for (int i = 0; i < na; i++) {
        Tipo pt = f->params[i], at = tipos[i];
        if (eh_erro(at)) continue;
        int ok;
        if (pt.vetor || at.vetor) ok = igual(pt, at);
        else ok = atribuivel(pt, at);
        if (!ok)
            erro(a, linha, "argumento %d da função '%s': esperado %s, encontrado %s",
                 i + 1, e->nome, nome_tipo(pt), nome_tipo(at));
    }
    free(tipos);
    return f->ret;
}

static Tipo sem_unario(An *a, const No *e) {
    Tipo t = expr(a, e->a, 0);
    if (eh_erro(t)) return T(T_ERRO);
    if (strcmp(e->op, "!") == 0) {
        if (!eh_t(t, T_BOOL)) {
            erro(a, e->linha, "operador '!' requer operando bool, mas recebeu %s", nome_tipo(t));
            return T(T_ERRO);
        }
        return T(T_BOOL);
    }
    if (!numerico(t)) {
        erro(a, e->linha, "operador '-' requer operando numérico, mas recebeu %s", nome_tipo(t));
        return T(T_ERRO);
    }
    return t;
}

static Tipo sem_binario(An *a, const No *e) {
    const char *op = e->op;
    Tipo tl = expr(a, e->a, 0);
    Tipo tr = expr(a, e->b, 0);
    int rel = !strcmp(op, "<") || !strcmp(op, ">") || !strcmp(op, "<=") || !strcmp(op, ">=");
    int eq = !strcmp(op, "==") || !strcmp(op, "!=");
    int logico = !strcmp(op, "&&") || !strcmp(op, "||");

    if (eh_erro(tl) || eh_erro(tr)) return (rel || eq || logico) ? T(T_BOOL) : T(T_ERRO);
    if (logico) {
        if (!eh_t(tl, T_BOOL) || !eh_t(tr, T_BOOL))
            erro(a, e->linha, "operador '%s' requer operandos bool, mas recebeu %s e %s",
                 op, nome_tipo(tl), nome_tipo(tr));
        return T(T_BOOL);
    }
    if (eq) {
        if (!((numerico(tl) && numerico(tr)) || (eh_t(tl, T_BOOL) && eh_t(tr, T_BOOL))))
            erro(a, e->linha, "operador '%s' não pode comparar %s com %s",
                 op, nome_tipo(tl), nome_tipo(tr));
        return T(T_BOOL);
    }
    if (rel) {
        if (!(numerico(tl) && numerico(tr)))
            erro(a, e->linha, "operador '%s' requer operandos numéricos, mas recebeu %s e %s",
                 op, nome_tipo(tl), nome_tipo(tr));
        return T(T_BOOL);
    }
    /* aritméticos */
    if (!(numerico(tl) && numerico(tr))) {
        erro(a, e->linha, "operador '%s' requer operandos numéricos, mas recebeu %s e %s",
             op, nome_tipo(tl), nome_tipo(tr));
        return T(T_ERRO);
    }
    if (!strcmp(op, "%") && (!eh_t(tl, T_INT) || !eh_t(tr, T_INT))) {
        erro(a, e->linha, "operador '%%' requer operandos int");
        return T(T_ERRO);
    }
    return (tl.base == T_FLOAT || tr.base == T_FLOAT) ? T(T_FLOAT) : T(T_INT);
}

static Tipo atribuicao(An *a, const No *e) {
    const No *alvo = e->a;
    if (alvo->kind != N_ID && alvo->kind != N_INDEX) {
        erro(a, e->linha, "lado esquerdo da atribuição não é uma variável");
        expr(a, e->b, 0);
        return T(T_ERRO);
    }
    Tipo ta = expr(a, alvo, 0);
    Tipo tv = expr(a, e->b, 0);
    if (eh_erro(ta)) return T(T_ERRO);
    if (ta.vetor) {
        erro(a, e->linha, "não é possível atribuir a um vetor inteiro");
        return T(T_ERRO);
    }
    if (!atribuivel(ta, tv))
        erro(a, e->linha, "atribuição incompatível: %s recebe %s", nome_tipo(ta), nome_tipo(tv));
    return ta;
}

static Tipo expr_(An *a, const No *e) {
    int linha = e->linha;
    switch (e->kind) {
    case N_NUM: return T(e->tipo.base);
    case N_BOOL: return T(T_BOOL);
    case N_ID: {
        Tipo t;
        if (!buscar(a, e->nome, &t)) {
            if (buscar_func(a, e->nome))
                erro(a, linha, "'%s' é uma função, não uma variável", e->nome);
            else
                erro(a, linha, "variável '%s' não declarada", e->nome);
            return T(T_ERRO);
        }
        return t;
    }
    case N_INDEX: {
        Tipo tb = expr(a, e->a, 0);
        Tipo ti = expr(a, e->b, 0);
        if (!eh_erro(ti) && !eh_t(ti, T_INT))
            erro(a, linha, "índice de vetor deve ser int, mas é %s", nome_tipo(ti));
        if (eh_erro(tb)) return T(T_ERRO);
        if (!tb.vetor) {
            erro(a, linha, "indexação de valor que não é vetor (tipo %s)", nome_tipo(tb));
            return T(T_ERRO);
        }
        return T(tb.base);
    }
    case N_CALL: return chamada(a, e);
    case N_UN: return sem_unario(a, e);
    case N_BIN: return sem_binario(a, e);
    case N_ASSIGN: return atribuicao(a, e);
    default: return T(T_ERRO);
    }
}

static Tipo expr(An *a, const No *e, int permite_void) {
    Tipo t = expr_(a, e);
    if (eh_t(t, T_VOID) && !permite_void) {
        erro(a, e->linha, "expressão do tipo void não pode ser usada como valor");
        return T(T_ERRO);
    }
    return t;
}

/* ---- comandos --------------------------------------------------------- */
static void decl_var(An *a, const No *d) {
    if (d->tipo.base == T_VOID)
        erro(a, d->linha, "variável '%s' não pode ser do tipo void", d->nome);
    if (d->tam != -1 && d->tam <= 0)
        erro(a, d->linha, "tamanho do vetor '%s' deve ser positivo", d->nome);
    declarar(a, d->nome, d->tipo, d->linha);
}

static void sem_comando(An *a, const No *n);

static void item(An *a, const No *n) {
    if (n->kind == N_VAR) decl_var(a, n);
    else sem_comando(a, n);
}

static void condicao(An *a, const No *e, const char *cmd) {
    Tipo t = expr(a, e, 0);
    if (!eh_erro(t) && !eh_t(t, T_BOOL))
        erro(a, e->linha, "condição do '%s' deve ser bool, mas é %s", cmd, nome_tipo(t));
}

static void retorno(An *a, const No *n) {
    const char *nome = a->func_nome;
    Tipo ret = a->func_ret;
    if (n->a == NULL) {
        if (!eh_t(ret, T_VOID))
            erro(a, n->linha, "função '%s' deve retornar %s", nome, nome_tipo(ret));
        return;
    }
    if (eh_t(ret, T_VOID)) {
        expr(a, n->a, 1);
        erro(a, n->linha, "função void '%s' não pode retornar valor", nome);
        return;
    }
    Tipo t = expr(a, n->a, 0);
    if (!atribuivel(ret, t))
        erro(a, n->linha, "retorno de tipo %s incompatível com o tipo %s da função '%s'",
             nome_tipo(t), nome_tipo(ret), nome);
}

static void sem_comando(An *a, const No *n) {
    switch (n->kind) {
    case N_BLOCK:
        abrir(a);
        for (int i = 0; i < n->n; i++) item(a, n->lista[i]);
        fechar(a);
        break;
    case N_IF:
        condicao(a, n->a, "if");
        sem_comando(a, n->b);
        if (n->c) sem_comando(a, n->c);
        break;
    case N_WHILE:
        condicao(a, n->a, "while");
        sem_comando(a, n->b);
        break;
    case N_RETURN: retorno(a, n); break;
    case N_EXPR: expr(a, n->a, 1); break;
    default: break; /* N_EMPTY */
    }
}

static int sempre_retorna(const No *n) {
    switch (n->kind) {
    case N_RETURN: return 1;
    case N_BLOCK:
        for (int i = 0; i < n->n; i++)
            if (sempre_retorna(n->lista[i])) return 1;
        return 0;
    case N_IF: return n->c != NULL && sempre_retorna(n->b) && sempre_retorna(n->c);
    default: return 0;
    }
}

static void sem_funcao(An *a, const No *f) {
    a->func_nome = f->nome;
    a->func_ret = f->tipo;
    abrir(a); /* parâmetros e corpo compartilham o mesmo escopo */
    for (int i = 0; i < f->n; i++) {
        const No *p = f->lista[i];
        if (p->tipo.base == T_VOID)
            erro(a, p->linha, "parâmetro '%s' não pode ser do tipo void", p->nome);
        declarar(a, p->nome, p->tipo, p->linha);
    }
    for (int i = 0; i < f->a->n; i++) item(a, f->a->lista[i]);
    fechar(a);
    if (!eh_t(f->tipo, T_VOID) && !sempre_retorna(f->a))
        erro(a, f->linha, "função '%s' pode terminar sem retornar valor", f->nome);
    a->func_nome = NULL;
}

static void registra_func(An *a, const No *d) {
    if (buscar_func(a, d->nome)) {
        erro(a, d->linha, "função '%s' já definida", d->nome);
        return;
    }
    if (a->nfuncs == a->capfuncs) {
        a->capfuncs = a->capfuncs ? a->capfuncs * 2 : 8;
        a->funcs = (Func *)realloc(a->funcs, (size_t)a->capfuncs * sizeof(Func));
        if (!a->funcs) abort();
    }
    Func *f = &a->funcs[a->nfuncs++];
    f->nome = d->nome;
    f->ret = d->tipo;
    f->np = d->n;
    f->params = (Tipo *)malloc((size_t)(d->n ? d->n : 1) * sizeof(Tipo));
    if (!f->params) abort();
    for (int i = 0; i < d->n; i++) f->params[i] = d->lista[i]->tipo;
    f->linha = d->linha;
}

Erros analisar(const No *prog) {
    An a;
    memset(&a, 0, sizeof a);
    abrir(&a); /* escopo global */

    /* 1ª passada: assinaturas (permite recursão e chamadas a funções adiante) */
    for (int i = 0; i < prog->n; i++)
        if (prog->lista[i]->kind == N_FUNC) registra_func(&a, prog->lista[i]);
    /* 2ª passada: variáveis globais e corpos das funções */
    for (int i = 0; i < prog->n; i++)
        if (prog->lista[i]->kind == N_VAR) decl_var(&a, prog->lista[i]);
    for (int i = 0; i < prog->n; i++)
        if (prog->lista[i]->kind == N_FUNC) sem_funcao(&a, prog->lista[i]);

    Func *pr = buscar_func(&a, "principal");
    if (!pr) {
        erro(&a, 1, "função 'principal' não definida");
    } else {
        if (pr->np > 0) erro(&a, pr->linha, "função 'principal' não deve ter parâmetros");
        if (!eh_t(pr->ret, T_INT)) erro(&a, pr->linha, "função 'principal' deve retornar int");
    }

    /* ordenação estável por linha (inserção) */
    for (int i = 1; i < a.nerros; i++) {
        ErroItem x = a.erros[i];
        int j = i - 1;
        while (j >= 0 && a.erros[j].linha > x.linha) {
            a.erros[j + 1] = a.erros[j];
            j--;
        }
        a.erros[j + 1] = x;
    }

    Erros r;
    r.n = a.nerros;
    r.msgs = (char **)malloc((size_t)(a.nerros ? a.nerros : 1) * sizeof(char *));
    if (!r.msgs) abort();
    for (int i = 0; i < a.nerros; i++) r.msgs[i] = a.erros[i].msg;
    return r;
}
