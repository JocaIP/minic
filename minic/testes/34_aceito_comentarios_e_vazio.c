// comentário de linha
int principal() {
    int v[2]; /* comentário
                 de bloco */
    ;
    v[0] = 1;
    v[1] = v[0] + 1;
    return v[1] % 2;
}
