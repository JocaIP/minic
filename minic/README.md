# MINIC — Analisador semântico

Compilador (front-end) para a linguagem MINIC, em **Python** e em **C**, com
os três componentes separados: **scanner**, **parser** e **analisador semântico**.

## Estrutura

| Componente           | Python               | C                                            |
|----------------------|----------------------|----------------------------------------------|
| Scanner              | `minic_scanner.py`   | `minic_scanner.h` / `minic_scanner.c`        |
| Parser               | `minic_parser.py`    | `minic_parser.h` / `minic_parser.c`          |
| Analisador semântico | `minic_semantico.py` | `minic_semantico.h` / `minic_semantico.c`    |
| Driver               | `minic.py`           | `minic.c` (+ `minic_ast.h`, `minic_erro.h`)  |

### Desacoplamento

Cada etapa só conhece o **contrato** da etapa anterior:

```
texto  --scanner-->  tokens  --parser-->  AST  --semântico-->  lista de erros
```

* O scanner não importa nada dos outros módulos.
* O parser conhece apenas o formato dos tokens.
* O analisador semântico conhece apenas o formato da AST (documentado no
  cabeçalho de `minic_parser.py` e em `minic_ast.h`); não importa scanner nem parser.
* O driver (`minic.py` / `minic.c`) é o único que liga as três etapas.

Em C, o driver inclui os `.c` dos componentes só para que o comando do professor
(`gcc minic.c`, um único arquivo-fonte) continue funcionando. Cada componente
mantém interface (`.h`) e implementação (`.c`) próprias.

### Equivalência C ↔ Python

As duas versões têm a mesma gramática, a mesma ordem de verificação e as mesmas
mensagens, e produzem **a mesma saída byte a byte e o mesmo código de saída**.
Isso foi verificado com um teste diferencial (casos de borda + 1500 programas
gerados por mutação dos testes): 0 diferenças. A versão C também foi executada
sem erros com AddressSanitizer/UBSan.

## Uso

```
python3 minic.py arquivo.c
gcc -Wall -Wextra minic.c -o minic && ./minic arquivo.c
```

Saída (sem quebra de linha final, conforme os gabaritos):

```
Análise semântica concluída: 0 erros; programa aceito.
```

ou, com erros, uma linha por erro (ordenadas por linha) e o resumo:

```
Erro semântico (linha 4): variável 'b' não declarada
Análise semântica concluída: 1 erro; programa rejeitado.
```

Erros léxicos e sintáticos interrompem a análise:
`Erro léxico (linha N): ...` / `Erro sintático (linha N): ...`.

Código de saída: `0` aceito, `1` rejeitado, `2` uso/arquivo inválido.

## Testes

```
./testes_semanticos_py.sh minic.py testes
./testes_semanticos_c.sh  minic.c  testes
```

`testes/` contém 35 casos, cada um com `.c` e `.gabarito`:

* **01–10**: casos fornecidos pelo professor (todos aceitos).
* **11–32**: casos de erro propostos, um por regra (variável não declarada,
  redeclaração, tipos incompatíveis, condição não-bool, operadores, chamadas,
  retornos, `void`, vetores, `principal`, escopo, erro léxico e sintático,
  múltiplos erros ordenados).
* **33–35**: casos aceitos adicionais (função global e chamada adiante,
  comentários/comando vazio, promoção `int → float`).

## Regras semânticas verificadas

* Escopos aninhados; redeclaração no mesmo escopo é erro; sombreamento em bloco
  interno é permitido; parâmetros e corpo da função compartilham o escopo.
* Tipos `int`, `float`, `bool`, `void`; única conversão implícita: `int → float`.
* `if`/`while` e `&&`, `||`, `!` exigem `bool`.
* `<`, `>`, `<=`, `>=` exigem numéricos; `==`, `!=` exigem ambos numéricos ou ambos `bool`;
  `%` exige `int`.
* Vetores: índice `int`; só vetores podem ser indexados; não se atribui vetor inteiro;
  parâmetro `T v[]` exige vetor de `T`; tamanho declarado deve ser positivo.
* Chamadas: função existente, quantidade e tipos de argumentos; `void` só como comando.
* `return` compatível com o tipo da função; função não-`void` deve retornar em todos
  os caminhos (`if` só conta se os dois ramos retornam; `while` não conta).
* Funções são registradas antes dos corpos (recursão e uso antes da definição).
* `principal` deve existir, sem parâmetros e retornando `int`.
* Variáveis `void` e parâmetros `void` são erros.

Após um erro, o tipo da expressão vira "erro" e não gera erros em cascata.
