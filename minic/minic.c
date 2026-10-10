/*
 * MINIC - Driver (versão C). Equivalente a minic.py.
 *
 * Uso: ./minic arquivo.c
 *
 * Integra, em sequência: scanner -> parser -> analisador semântico.
 * Cada componente tem sua interface (.h) e sua implementação (.c) próprias.
 * Os .c são incluídos aqui apenas para que o comando de compilação do
 * professor (gcc minic.c) continue sendo um único arquivo-fonte:
 *     gcc -Wall -Wextra minic.c -o minic
 *
 * Saída (stdout), sem quebra de linha final:
 *   - sem erros: "Análise semântica concluída: 0 erros; programa aceito."
 *   - com erros: uma linha por erro e, no final,
 *     "Análise semântica concluída: N erros; programa rejeitado."
 * Código de saída: 0 (aceito), 1 (rejeitado), 2 (uso/arquivo).
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minic_scanner.c"
#include "minic_parser.c"
#include "minic_semantico.c"

/* Lê o arquivo inteiro e normaliza \r\n e \r para \n (como o modo texto do Python). */
static char *ler_arquivo(const char *caminho) {
    FILE *f = fopen(caminho, "rb");
    if (!f) return NULL;
    size_t cap = 4096, n = 0;
    char *bruto = (char *)malloc(cap);
    if (!bruto) abort();
    size_t r;
    while ((r = fread(bruto + n, 1, cap - n, f)) > 0) {
        n += r;
        if (n == cap) {
            cap *= 2;
            bruto = (char *)realloc(bruto, cap);
            if (!bruto) abort();
        }
    }
    fclose(f);
    char *txt = (char *)malloc(n + 1);
    if (!txt) abort();
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        if (bruto[i] == '\r') {
            txt[m++] = '\n';
            if (i + 1 < n && bruto[i + 1] == '\n') i++;
        } else {
            txt[m++] = bruto[i];
        }
    }
    txt[m] = '\0';
    free(bruto);
    return txt;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s arquivo.c\n", argv[0]);
        return 2;
    }
    char *texto = ler_arquivo(argv[1]);
    if (!texto) {
        fprintf(stderr, "Erro: não foi possível abrir '%s': %s\n", argv[1], strerror(errno));
        return 2;
    }

    ErroFront ef;
    Tokens toks;
    if (!tokenizar(texto, &toks, &ef)) {
        printf("Erro léxico (linha %d): %s", ef.linha, ef.msg);
        return 1;
    }
    No *prog = parsear(&toks, &ef);
    if (!prog) {
        printf("Erro sintático (linha %d): %s", ef.linha, ef.msg);
        return 1;
    }

    Erros erros = analisar(prog);
    if (erros.n == 0) {
        printf("Análise semântica concluída: 0 erros; programa aceito.");
        return 0;
    }
    for (int i = 0; i < erros.n; i++) printf("%s\n", erros.msgs[i]);
    printf("Análise semântica concluída: %d %s; programa rejeitado.", erros.n,
           erros.n == 1 ? "erro" : "erros");
    return 1;
}
