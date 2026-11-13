# Dead Man's Bluff

Pôquer Texas Hold'em single-player em que trapacear faz parte da estratégia, desenvolvido em C com Raylib para a disciplina de Algoritmos e Estruturas de Dados (AED).

> **Estado atual:** motor de pôquer heads-up implementado, com testes de console e demonstração Raylib, tela inicial do jogo com o loop principal, lógica do torneio, loop da mão, IA dos adversários e uma mesa jogável de ponta a ponta com interface de teste. Trapaças, roleta e a interface gráfica definitiva ainda estão em desenvolvimento.

## Sobre o jogo

O jogo é Texas Hold'em **1 contra 1 (heads-up)**. O jogador enfrenta três adversários em um **torneio** de três confrontos: o Agressivo, o Medroso e o Desconfiado. Ele começa com 2.000 fichas e leva o saldo de um confronto para o seguinte, enquanto os adversários entram com 2.000, 4.000 e 8.000 fichas. Os blinds sobem a cada poucas mãos para a partida não se arrastar e recomeçam em cada confronto.

Ganhar as mãos importa, mas o desafio é sobreviver às consequências de trapacear: a **suspeita** de cada adversário e a **roleta russa**, acionada quando o jogador é descoberto ou perde todas as fichas.

## Inspiração

A atmosfera é inspirada em **007 — Cassino Royale**: mesas de alto risco, tensão psicológica e blefe. O universo, os personagens e a história são próprios do projeto.

## Mecânicas

**Já implementadas**

- Texas Hold'em heads-up com apostas, avaliação de mãos e pote.
- Torneio de três confrontos, com saldo acumulado, blinds crescentes e recuperação após falência.
- Três adversários com comportamentos diferentes, que usam um único motor de decisão com parâmetros próprios e podem ficar embriagados.

**Em desenvolvimento**

- Ações ilegais, que custam fichas e saem de circulação: embriagar o adversário (*spike his drink*), espelho ou cúmplice, ás na manga, suborno do dealer e marcar carta.
- Chance de ser descoberto, que depende do risco da ação, da suspeita acumulada e da vigilância do adversário.
- Suspeita individual por adversário.
- Roleta russa com revólver de seis câmaras, cujas balas persistem durante o torneio.
- Interface gráfica definitiva, com mesa, cartas, fichas e animações.

**Visão futura, fora da primeira versão**

- Elementos roguelike, novos adversários, eventos aleatórios, mais trapaças e sistemas de progressão.

## Tecnologias e estruturas de dados previstas

- Linguagem **C**.
- Biblioteca **Raylib** para a parte gráfica.
- **Listas simplesmente encadeadas** para o gerenciamento das cartas, com o baralho em `src/cards/deck.c`.
- **Insertion Sort** para a ordenação das mãos.
- Algoritmos próprios para as probabilidades e para as decisões dos adversários, como a estimativa de equidade por Monte Carlo.

## Equipe e responsabilidades

| Integrante | Área | Responsabilidades |
| --- | --- | --- |
| Arthur | Game Engine e arquitetura | Loop principal, estados, torneio, loop da mão, fila de eventos, integração entre os módulos e revisão dos PRs |
| Júlia | Poker Engine | Regras do pôquer, avaliação das mãos, sistema de apostas, comparação de combinações e determinação dos vencedores |
| Rafael | Cartas, estruturas de dados e IA | Listas simplesmente encadeadas, gerenciamento do baralho, embaralhamento e distribuição, compra, descarte e ordenação das cartas, e a IA dos três adversários |
| Peixoto | Mecânicas | Ações ilegais, probabilidades de descoberta, suspeita e roleta russa |
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
│   ├── core/       # Engine: loop, torneio, mão, partida e eventos
│   ├── cards/      # Lista encadeada de cartas e baralho
│   ├── poker/      # Regras, avaliação de mãos e apostas
│   ├── gameplay/   # IA dos adversários, trapaças, probabilidades e suspeita
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
| `src/gameplay/` | IA dos adversários, mecânicas de trapaça, probabilidades e suspeita |
| `src/ui/` | Telas, menus, renderização e animações |
| `assets/` | Imagens, sons e fontes |

O motor está em `src/poker/poker.c` (avaliação) e `src/poker/apostas.c` (apostas e pagamento). A API pública está em `src/poker/poker.h`; `apostas.h` inclui essa interface para consumidores do módulo de apostas. O loop principal e os estados do jogo estão em `src/core/jogo.c`, a lógica do torneio (3 confrontos, saldo, blinds e recuperação após falência), em `src/core/torneio.c`, o loop de uma mão (baralho, motor e torneio juntos), em `src/core/mao.c`, a fila de eventos que a interface consome para animar a mão, em `src/core/eventos.c`, a IA dos adversários (3 perfis), em `src/gameplay/poker_ai.c`, a ponte entre a mão e a IA, em `src/core/mao_ia.c`, o controlador da partida (torneio, mão, eventos e IA juntos), em `src/core/partida.c`, e a tela inicial, em `src/ui/interface.c`. Os demais módulos ainda contêm comentários `TODO`.

## Estado atual

- [x] Planejamento do projeto e divisão de responsabilidades.
- [x] Estruturação inicial do repositório.
- [x] Motor de pôquer heads-up e demonstrações.
- [x] Tela inicial (Jogar/Sair) e loop principal com Raylib.
- [x] Lógica do torneio: 3 confrontos, saldo acumulado, blinds crescentes e recuperação após falência.
- [x] Loop da mão: embaralhar, distribuir, rodadas de apostas, showdown e registro no torneio.
- [x] Fila de eventos da mão para a interface animar (distribuição, ações, board, showdown e pagamento).
- [x] IA dos adversários com 3 perfis e ponte para a mão.
- [x] Controlador da partida: torneio, mão, fila de eventos e IA juntos, com o perfil de cada confronto.
- [x] Mesa jogável de ponta a ponta no `make jogo`, com interface de teste em texto cru (o design da mesa é do Matheus).
- [ ] Trapaças, suspeita e roleta russa.
- [ ] Interface gráfica definitiva (mesa, cartas, fichas e animações).
- [ ] Integração e testes.


## Compilação e testes do motor

Requer um compilador C11 e GNU Make. Raylib não é necessária para os testes ou para a demonstração de console.

```sh
make test
make console-demo
./build/console_demo
```

Os alvos usam `-std=c11 -Wall -Wextra -Wpedantic -Werror`. Os testes verificam vitória por par superior, all-in com stacks diferentes (ambos os assentos e vencedores), devolução de apostas não cobertas, empate, ficha ímpar, fold, conservação de fichas e rejeição de pagamento duplicado. A demonstração de console termina com saldos de `2300 / 1700` após raise para 300. O `make test` também roda `tests/test_torneio.c`, que joga confrontos completos com o motor e confere saldo, blinds, falência e recuperação, `tests/test_mao.c`, que joga mãos inteiras com baralho, motor e torneio (desistência, all-in em cada rua, stacks menores que o blind e torneios completos com ações aleatórias), `tests/test_eventos.c`, que confere a fila de eventos e a sequência de eventos de cada tipo de mão, `tests/test_partida.c`, que joga partidas inteiras contra a IA, e os três testes do baralho (`test_deck`, `test_edge_cases` e `test_integration`), que conferem as 52 cartas únicas, a distribuição heads-up e partidas completas com o motor.

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

### Jogo

Com Raylib instalada:

```sh
make jogo
./build/dead_mans_bluff
```

Abre a tela inicial: **W/S** ou as setas navegam e **ENTER** ou **ESPAÇO** confirmam. **Jogar** começa o torneio contra os três adversários (Agressivo, Medroso e Desconfiado), e **ESC** volta ao menu. **Sair**, ou fechar a janela, encerra o jogo.

A mesa ainda é um texto cru de teste, sem design, que o Matheus vai substituir. Na sua vez: **F** desiste, **C** passa ou paga, **R** aposta ou aumenta (as setas para cima e para baixo ajustam o valor, em passos de um big blind), **A** vai all-in. **ENTER** ou **ESPAÇO** seguem para a próxima mão. A falência encerra a partida, porque a roleta ainda não existe; **ENTER** recomeça.

No MSYS2 UCRT64, use `mingw32-make jogo` e mantenha `C:\msys64\ucrt64\bin` no `PATH`, pois o executável usa a `libraylib.dll` e a `glfw3.dll` dessa pasta.

### Integração em C11

`poker_settle` recebe as duas mãos separadamente para aceitar cartas constantes e mutáveis sem casts em C11:

```c
PokerCard hole[2][2]; /* preencher com cartas válidas */
PokerCard board[5];  /* preencher com o board completo */
/* table e result devem estar declarados; table deve estar em showdown. */
poker_settle(&table, hole[0], hole[1], board, &result);
```

Em vitória por fold, use `poker_settle(&table, NULL, NULL, NULL, &result)`.
