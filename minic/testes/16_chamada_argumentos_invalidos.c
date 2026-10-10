int soma(int a, int b) {
    return a + b;
}
int principal() {
    int r;
    r = soma(1);
    r = soma(1, 2, 3);
    r = soma(1, 2.5);
    r = soma(true, 2);
    return r;
}
