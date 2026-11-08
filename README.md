# Dead Man's Bluff

Roguelike de pôquer desenvolvido em C com Raylib para a disciplina de Algoritmos e Estruturas de Dados (AED).

> **Estado atual:** motor de pôquer heads-up implementado, com testes de console e demonstração Raylib. O jogo completo ainda está em desenvolvimento.

## Sobre o jogo

O jogador enfrenta adversários em diferentes mesas de pôquer, aumentando progressivamente as apostas (*stakes*) até alcançar o chefe final. O diferencial é a possibilidade de recorrer a ações ilegais para virar o jogo, ao custo de atrair suspeita e correr o risco de ser descoberto.

## Inspiração

A atmosfera é inspirada em **007 — Cassino Royale**: mesas de alto risco, tensão psicológica e blefe. O universo, os personagens e a história são próprios do projeto.

## Mecânicas planejadas

- Partidas de pôquer contra adversários com comportamentos diferentes.
- Aumento progressivo de stakes ao longo das mesas, até o chefe final.
- Ações ilegais: roubar fichas, marcar cartas, subornar o dealer, ameaçar e envenenar adversários.
- Resultado das ações ilegais dependente de probabilidades e das habilidades do jogador.
- Gerenciamento de suspeita.
- Progressão roguelike com melhorias de habilidades.

## Tecnologias e estruturas de dados previstas

- Linguagem **C**.
- Biblioteca **Raylib** para a parte gráfica.
- **Listas simplesmente encadeadas** para o gerenciamento das cartas.
- **Insertion Sort** para a ordenação das mãos.
- Algoritmos próprios para probabilidades e decisões dos adversários.

## Equipe e responsabilidades

| Integrante | Área | Responsabilidades |
| --- | --- | --- |
| Arthur | Game Engine e arquitetura | Loop principal, gerenciamento de estados, sistema de turnos, integração entre os módulos |
| Júlia | Poker Engine | Regras do pôquer, avaliação das mãos, sistema de apostas, comparação de combinações e determinação dos vencedores |
| Rafael | Cartas e estruturas de dados | Listas simplesmente encadeadas, gerenciamento do baralho, embaralhamento e distribuição, compra, descarte e ordenação das cartas |
| Peixoto | Mecânicas e progressão | Ações ilegais, sistema de probabilidades, gerenciamento de suspeita, adversários e progressão roguelike |
| Matheus | Interface gráfica | Interface com Raylib, menus e telas, renderização de cartas e fichas, interações e animações |

## Estrutura de pastas

```
Dead-Man-s-Bluff/
├── README.md
├── Makefile
├── tests/         # Testes automatizados do motor
├── examples/      # Demonstrações de console e Raylib
├── src/
│   ├── main.c
│   ├── core/       # Engine: loop, estados e turnos
│   ├── cards/      # Lista encadeada de cartas e baralho
│   ├── poker/      # Regras, avaliação de mãos e apostas
│   ├── gameplay/   # Trapaças, probabilidades, adversários e progressão
│   └── ui/         # Interface gráfica e animações (Raylib)
└── assets/
    ├── images/
    ├── sounds/
    └── fonts/
```

| Pasta | Conteúdo |
| --- | --- |
| `src/core/` | Núcleo do jogo e integração entre os módulos |
| `src/cards/` | Estruturas de dados das cartas e gerenciamento do baralho |
| `src/poker/` | Motor de pôquer e sistema de apostas |
| `src/gameplay/` | Mecânicas de trapaça, probabilidades, adversários e progressão |
| `src/ui/` | Telas, menus, renderização e animações |
| `assets/` | Imagens, sons e fontes |

O motor está em `src/poker/poker.c` (avaliação) e `src/poker/apostas.c` (apostas e pagamento). A API pública está em `src/poker/poker.h`; `apostas.h` inclui essa interface para consumidores do módulo de apostas. Os demais módulos ainda contêm comentários `TODO`.

## Estado atual

- [x] Planejamento do projeto e divisão de responsabilidades.
- [x] Estruturação inicial do repositório.
- [x] Motor de pôquer heads-up e demonstrações.
- [ ] Implementação dos demais módulos.
- [ ] Integração e testes.


## Compilação e testes do motor

Requer um compilador C11 e GNU Make. Raylib não é necessária para os testes ou para a demonstração de console.

```sh
make test
make console-demo
./build/console_demo
```

Os alvos usam `-std=c11 -Wall -Wextra -Wpedantic -Werror`. Os testes verificam vitória por par superior, all-in com stacks diferentes (ambos os assentos e vencedores), devolução de apostas não cobertas, empate, ficha ímpar, fold, conservação de fichas e rejeição de pagamento duplicado. A demonstração de console termina com saldos de `2300 / 1700` após raise para 300.

Para compilar sem Make:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Isrc/poker tests/test_poker.c src/poker/poker.c src/poker/apostas.c -o test_poker
./test_poker
```

### Windows / PowerShell sem Make

Se o GCC estiver em `C:\mingw64\bin`, execute na raiz do repositório:

```powershell
& "C:\mingw64\bin\gcc.exe" -std=c11 -Wall -Wextra -Wpedantic -Werror -Isrc/poker tests/test_poker.c src/poker/poker.c src/poker/apostas.c -o test_poker.exe
# Execute somente se a compilação terminar sem erros:
.\test_poker.exe
```

Para a demonstração de console:

```powershell
& "C:\mingw64\bin\gcc.exe" -std=c11 -Wall -Wextra -Wpedantic -Werror -Isrc/poker examples/console_demo.c src/poker/poker.c src/poker/apostas.c -o console_demo.exe
.\console_demo.exe
```

Ajuste o caminho do compilador caso sua instalação seja diferente.

### Demonstração Raylib

Com Raylib e seu arquivo `raylib.pc` instalados:

```sh
make raylib-demo
./build/raylib_demo
```

Em instalações sem `pkg-config`, informe as opções do seu ambiente, por exemplo no Windows/MinGW:

```sh
make raylib-demo RAYLIB_CFLAGS="-IC:/raylib/include" RAYLIB_LIBS="-LC:/raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm"
```

Controles: **C** passa/paga, **B** aposta/aumenta pelo mínimo, **A** faz all-in, **F** desiste e **N** avança a etapa ou paga o pote. A demonstração controla os dois jogadores e usa cartas fixas, sem IA nem embaralhamento. No pré-flop, use B e C; depois alterne N e dois C até o showdown. O último N paga o pote, com saldos de `2200 / 1800`.

### Integração em C11

`poker_settle` recebe as duas mãos separadamente para aceitar cartas constantes e mutáveis sem casts em C11:

```c
PokerCard hole[2][2]; /* preencher com cartas válidas */
PokerCard board[5];  /* preencher com o board completo */
/* table e result devem estar declarados; table deve estar em showdown. */
poker_settle(&table, hole[0], hole[1], board, &result);
```

Em vitória por fold, use `poker_settle(&table, NULL, NULL, NULL, &result)`.
