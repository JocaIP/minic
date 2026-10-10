float f(int n) {
    return n;
}
int principal() {
    float x;
    bool ok;
    x = f(3);
    ok = x >= 2 && 1 == 1.0;
    if (ok) return 1;
    return 0;
}
