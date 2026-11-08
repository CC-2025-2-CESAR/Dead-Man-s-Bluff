# Decisões técnicas e divisão de trabalho

> Documento vivo do **Dead Man's Bluff**. Serve para o grupo saber o que já foi decidido, o que ainda está em aberto e o que cada pessoa precisa fazer.
>
> **Última atualização:** 08/11/2026.
> **Fontes:** documento de conceito v0.2 (fixado no grupo), PR #1 (motor de pôquer), PR #2 (baralho) e a conversa do grupo.
>
> Legenda: **[Decidido]** já está no código ou foi combinado · **[Proposta]** sugestão que o grupo precisa confirmar · **[Pendente]** ainda não há decisão.
>
> Quando uma decisão mudar, atualize este arquivo no mesmo PR da mudança.

## 1. Visão geral

- Pôquer **Texas Hold'em single-player, 1 contra 1 (heads-up)**, em que trapacear faz parte da estratégia.
- O jogador enfrenta **3 adversários em um torneio**. Ganhar a mão importa, mas o desafio é sobreviver às consequências: **suspeita** e **roleta russa**.
- Prazo informado no conceito: **13 dias**.
- Foco da primeira versão (conceito v0.2, seção 9), nesta ordem:
  1. Texas Hold'em funcional, com apostas e avaliação de mãos.
  2. Três perfis de IA usando um motor compartilhado.
  3. Torneio com progressão de fichas e blinds.
  4. Duas ou três trapaças implementadas.
  5. Suspeita individual por adversário.
  6. Roleta russa integrada às punições e à falência.
  7. Interface básica e efeitos visuais de tensão.
- Fica para depois (visão futura): elementos roguelike, novos adversários, eventos aleatórios, mais trapaças e sistemas de progressão.

**[Pendente]** O `README.md` ainda descreve o jogo como roguelike com stakes crescentes até um chefe final. No conceito v0.2 o roguelike é visão futura e o jogo é um torneio de 3 confrontos. O README precisa ser atualizado (responsável: Arthur).

## 2. Decisões já tomadas

| Tema | Decisão | Onde está |
| --- | --- | --- |
| Linguagem | C, com Raylib na parte gráfica (a escolha Raylib x SDL ainda está aberta, ver seção 6) | — |
| Formato da partida | Heads-up (2 jogadores), no-limit, blinds configuráveis | `PokerTable.player[2]` em `src/poker/poker.h` |
| Fichas | `int64_t` (`PokerChips`), com limite `POKER_MAX_STACK` | `src/poker/poker.h` |
| Carta | `PokerCard { rank 2..14, suit }`, definida pelo motor e **usada também pelo baralho** (fonte única) | `src/poker/poker.h`, `src/cards/deck.h` |
| Baralho | Lista encadeada de 52 cartas (`head`/`tail`), embaralhamento Fisher-Yates, compra, queima e distribuição heads-up, com flop/turn/river | PR #2, `src/cards/deck.*` |
| Avaliação de mãos | Aceita 5, 6 ou 7 cartas, não depende da ordem das cartas, com desempate completo | `poker_evaluate`, `poker_compare` |
| Pote | Devolve a parte da aposta que o adversário não cobriu, divide em empate e paga uma única vez | `poker_settle` |
| Estrutura de pastas | `src/<módulo>/`, `tests/` para testes, `examples/` para demos, `assets/` | README |
| Testes | Um `tests/test_*.c` por área, rodando com `make test` | PR #1 |
| Branches | `main` e `develop` protegidas: só entram por PR, sem force-push e sem apagar. Trabalho novo sai de `develop` e volta por PR para `develop`; a `main` só recebe da `develop` | Settings do repositório |
| Commits | Em português, no presente ("Adiciona...", "Corrige..."), sem assinaturas ou créditos de ferramentas | Combinado no início |
| Revisão | O Arthur revisa os PRs, decide a ordem dos merges e resolve conflitos | Combinado no grupo |

**[Proposta]** Padronizar em **C11 com `-Wall -Wextra -Wpedantic`**, como o Makefile da Júlia já faz (os testes dela usam `-Werror`).

## 3. Como as partes se encaixam

### 3.1 Fluxo de uma mão

Quem chama o quê, em ordem:

1. **core** define o dealer da mão e os stacks de cada um. Custos de trapaça que valem "por mão" são descontados dos stacks aqui (ver 5.2).
2. **cards**: `deck_create`, `deck_shuffle`, `deck_deal(deck, dealer, hole)`.
3. **poker**: `poker_start_hand(&table, stacks, dealer, small_blind, big_blind)`. Atenção: se um stack for menor ou igual ao blind, a mão já volta em `POKER_SHOWDOWN`, sem ninguém agir.
4. **Rodada de apostas**: enquanto `table.phase == POKER_ACTING`, age quem está em `table.actor`.
   - Humano: a UI devolve a ação escolhida. IA: `adversarios` devolve a ação.
   - `poker_legal_actions` diz o que é permitido e `poker_act` aplica a ação.
5. Em `POKER_STREET_FINISHED`: `deck_deal_board(deck, street, board, &n)` e depois `poker_next_street`.
6. Em `POKER_SHOWDOWN` ou `POKER_UNCONTESTED`: o **core completa o board** e chama `poker_settle(&table, hole0, hole1, board, &result)`. Isso vale também quando o all-in acontece antes do river, porque o motor exige as 5 cartas do board no showdown.
7. **core** lê `result` (vencedor, prêmio, devoluções) e atualiza o torneio: saldo, blinds, falência e roleta. A UI anima.

### 3.2 Fronteiras de responsabilidade

- `poker` não conhece o baralho, a UI, a IA nem as trapaças. Só recebe cartas e ações.
- `ui` é o único módulo que conhece a Raylib e **não decide regra**.
- `core` orquestra os demais e guarda o estado do torneio.
- Rotação do dealer entre mãos é responsabilidade de quem chama o motor (`core`).
- `result.value[]` só vale quando `result.showdown` é verdadeiro.
- Stack 0 em `poker_start_hand` devolve erro: a falência precisa ser tratada **antes** de iniciar uma nova mão.

### 3.3 Eventos para a UI

**[Proposta]** O Matheus precisa saber *o que* animar. Sugestão: o `core` registra uma fila de eventos (distribuir cartas, apostar, desistir, nova rodada, showdown, pagar pote, trapaça aplicada, trapaça descoberta, roleta) e a UI consome essa fila. Assim a UI não precisa ler o estado do motor para descobrir o que mudou. O formato dos eventos é o Arthur quem define, junto com o Matheus.

## 4. Regras do jogo (resumo do conceito v0.2)

### Torneio

| Confronto | Adversário | Fichas iniciais dele |
| --- | --- | --- |
| 1 | Agressivo | 2.000 |
| 2 | Medroso | 4.000 |
| 3 | Desconfiado (final) | 8.000 |

- O jogador começa com 2.000 fichas e **leva o saldo acumulado** para a etapa seguinte. Exemplo do conceito: vencer o primeiro sem gastar nada em trapaças chega ao segundo com 4.000; gastando 600, chega com 3.400.
- Os blinds sobem a cada determinado número de mãos, para evitar partidas intermináveis. **Ao iniciar um novo confronto, os blinds reiniciam** em valores adequados à etapa.

### Adversários e IA

| Perfil | Estilo | Quando embriagado |
| --- | --- | --- |
| Agressivo (loose-aggressive) | Joga muitas mãos, aposta e blefa bastante | Fica impulsivo e erra mais |
| Medroso (tight) | Range restrito, desiste diante de apostas grandes; se insiste ou aumenta, provavelmente tem mão forte | Perde cautela |
| Desconfiado (regular) | Equilibrado e muito atento a trapaças | Perde parte da capacidade de detectar trapaças |

- **Um único motor de decisão** para os três, com parâmetros diferentes: cautela, agressividade, frequência de blefe e vigilância.
- Pré-flop: tabelas de ranges. A partir do flop: um analisador avalia a força da mão (categoria, força relativa ao board e draws).
- A decisão final combina avaliação da mão, tamanho da aposta, personalidade e sorteio (RNG).
- Embriaguez é um **estado binário** (sóbrio ou embriagado), com modificadores fixos e duração limitada em mãos.
- Segundo o Rafael, que está cuidando da IA, ela é feita do zero, como regras de comportamento (por exemplo, "mão fraca diante de aposta alta foge em boa parte das vezes"), sem treinar modelos.

### Trapaças

Custam as mesmas fichas das apostas. Pagamentos a cúmplices e ao dealer **saem de circulação**. Proposta de implementação do Gabriel para cada uma:

| Trapaça | O que faz | Proposta de implementação |
| --- | --- | --- |
| Spike his drink | Embriaga o adversário temporariamente, se não for descoberta | O efeito depende da personalidade. Risco relativamente baixo, serve para preparar outras ações |
| Espelho / cúmplice | Revela informação sobre a mão do adversário | Paga fichas por uma mão. O valor sobe com o blind da mesa e com quantas vezes já foi usado naquele blind |
| Ás na manga | Troca uma carta da mão por uma carta escondida | O jogador começa com um 2 na manga e pode trocar com uma carta da mão para guardá-la para depois, na mesma mesa ou na próxima |
| Suborno do dealer | Obtém informação ou manipula cartas | Custo por ação, fixo por mesa e crescente com o valor da mesa, com chance de o dealer recusar. Ações: pedir uma carta específica para a mesa, receber uma carta específica, saber as cartas do outro jogador. Uma ação de suborno por mão |
| Marcar carta | Marca uma carta da própria mão para reconhecê-la depois, na mão do adversário ou na mesa | A chance de ser pego aumenta a cada carta marcada. A marca dura cerca de 4 mãos (ajustável). Só uma carta por turno |

Os custos e as chances de ser descoberto ainda não têm valores: serão ajustados testando.

### Suspeita

- A suspeita é **do adversário atual**, não do torneio inteiro. Cada personagem começa com uma suspeita-base definida pela sua vigilância.
- A chance de descoberta considera: risco-base da trapaça, suspeita acumulada, vigilância do adversário e modificadores temporários, como a embriaguez.
- Primeiro calcula-se a chance de descoberta. Se a trapaça não for descoberta, a suspeita sobe para as próximas tentativas.
- Ao vencer um confronto, a suspeita reinicia para o novo adversário.

### Roleta russa

- Acionada em duas situações: quando o jogador é **descoberto trapaceando** e quando **perde todas as fichas**.
- Revólver de 6 câmaras. O número de balas é o risco acumulado e **persiste durante o torneio**.

| Balas | 1 | 2 | 3 | 4 | 5 | 6 |
| --- | --- | --- | --- | --- | --- | --- |
| Chance de morte | 16,7% | 33,3% | 50% | 66,7% | 83,3% | 100% |

- Se sobreviver: continua, a suspeita do adversário atual diminui bastante (sem zerar) e as balas **não** são removidas automaticamente.
- Se a roleta veio de falência: o jogador recebe **metade do valor de referência da etapa** para tentar continuar (no confronto de 4.000, volta com 2.000), com um limite de recuperações para evitar exploração.
- Ao vencer um confronto, o jogador remove balas do revólver.

### Economia

Três recursos ligados: **fichas** (apostar, trapacear e continuar), **suspeita** (individual por adversário) e **balas** (perigo acumulado, removíveis como recompensa por vitória).

## 5. Trapaças e motor de pôquer

Pontos em que as trapaças esbarram no que já existe. Precisam ser resolvidos **antes** da implementação.

### 5.1 Cartas repetidas

O baralho gera 52 cartas únicas e o `poker_settle` **recusa** o showdown se houver carta repetida (`POKER_ERR_DUPLICATE_CARD`). Se uma trapaça colocar a mesma carta em dois lugares, a mão não consegue ser paga.

- A ideia "copiar carta", levantada na conversa, cria repetidas. Seria preciso retirar as travas do baralho e do `poker_settle`, e o Rafael considera a trapaça forte demais.
- A alternativa proposta pelo Rafael é **trocar sempre com uma carta real do baralho** (o jogador devolve uma e pega outra que já existe), e assim nunca repete.
- No "ás na manga", a carta guardada na manga **precisa sair do baralho** quando é separada, para continuar valendo "52 cartas únicas".

**[Pendente]** Decisão do Gabriel, que vai implementar. Recomendação: manter a regra de cartas únicas.

### 5.2 Custos em fichas

O motor não tem como cobrar uma taxa no meio da mão. Se o `core` alterar `table.player[i].chips` diretamente, a conta "fichas + pote = total inicial" deixa de fechar (e o conceito diz que esses pagamentos saem de circulação).

- Custos que valem **por mão** (cúmplice, suborno) podem ser descontados dos stacks **antes** de `poker_start_hand`, sem mudar o motor.
- Se algum custo precisar ser pago **durante** a mão, o motor precisa de uma função própria que registre a taxa fora do pote.

**[Pendente]** Decisão entre Júlia e Gabriel. Recomendação: cobrar entre mãos sempre que possível.

### 5.3 Novas operações no baralho

O baralho atual só sabe comprar a próxima carta, queimar, distribuir e distribuir o board. As trapaças pedem mais:

- Tirar uma carta **específica** do baralho (suborno: "receber carta específica", e a carta da manga).
- Espiar as próximas cartas.
- Devolver uma carta ao baralho (troca com carta do baralho).

**[Proposta]** O Rafael adiciona essas operações em `src/cards/` quando o Gabriel fechar o que precisa.

### 5.4 Informação revelada

Marcar carta, espelho e parte do suborno só **revelam informação** (cartas do adversário, cartas da mesa). Isso é estado de jogo mostrado pela UI e **não exige mudança no motor de pôquer**.

## 6. Decisões pendentes

| # | Decisão | Quem decide | Recomendação | Bloqueia |
| --- | --- | --- | --- | --- |
| 1 | Política de cartas repetidas nas trapaças (5.1) | Gabriel, com Rafael e Júlia | Manter cartas únicas; trocar com carta real do baralho | Trapaças |
| 2 | Custos de trapaça no meio da mão (5.2) | Júlia e Gabriel | Cobrar entre mãos; função própria no motor se for inevitável | Trapaças |
| 3 | Raylib ou SDL | Grupo (Arthur e Matheus) | Raylib: o protótipo da tela inicial e o demo da Júlia já usam | UI |
| 4 | Idioma dos nomes no código | Grupo | Escolher um critério e registrar aqui. Hoje: motor e baralho em inglês (`poker_*`, `deck_*`); o placeholder do baralho se chamava `baralho` | Todos |
| 5 | Dono do **Insertion Sort** das mãos e da lista encadeada de cartas (`src/cards/lista_cartas.*`) | Rafael e Arthur | O README prevê Insertion Sort para ordenar as mãos, e hoje ninguém o implementou | Requisito da disciplina |
| 6 | Dono de `adversarios.*` | Grupo | A conversa indica o Rafael; o README ainda lista adversários com o Peixoto. Atualizar o README | IA |
| 7 | 3 confrontos (conceito v0.2) ou 5 (ideia do Matheus no Discord) | Arthur e Rafael | Ficar com 3 na v1 | Torneio |
| 8 | Padrão C do projeto | Grupo | C11 com `-Wall -Wextra -Wpedantic` | Build |
| 9 | Makefile único (jogo + testes + demos) | Arthur | Um só `Makefile` na raiz, com alvo do jogo e alvos de teste | Build |
| 10 | Funcionamento definitivo do ás na manga | Gabriel | Ver 5.1 | Trapaças |
| 11 | Valores a balancear, listados no conceito v0.2 (seção 10): valores e frequência de aumento dos blinds, probabilidades-base de descoberta, suspeita acrescentada por tentativa, redução da suspeita após sobreviver à roleta, balas adicionadas por punição, balas removidas por vitória, penalidade financeira ao ser descoberto, limite de recuperações por falência, duração da embriaguez | Grupo | Deixar como constantes fáceis de ajustar e decidir testando | Balanceamento |

## 7. Quem faz o quê

**[Proposta]** Baseada na divisão inicial e no que foi dito no grupo. Confirmem ou ajustem.

| Pessoa | Área | Já feito | Próximos passos | Depende de |
| --- | --- | --- | --- | --- |
| **Arthur** | Engine e arquitetura (`src/core`) | Estrutura do repositório, regras de branch, tela inicial e loop principal com troca de estados (PR #3) | Torneio (3 confrontos, saldo, blinds, falência); loop da mão (seção 3.1); definir os eventos para a UI; integrar IA, trapaças e UI; atualizar o README; revisar e mesclar os PRs | Merge do PR #2 (baralho) |
| **Júlia** | Motor de pôquer (`src/poker`) | Avaliação, apostas e pote com testes; correções do review aplicadas (PR #1, já na `develop`) | Deixar o contrato do motor escrito em `poker.h` (mão que volta em showdown, `value[]` só com showdown, board completo no settle); decidir com o Gabriel os custos no meio da mão (5.2); newline final nos arquivos | Decisões do Gabriel sobre custos |
| **Rafael** | Cartas e IA (`src/cards`, `src/gameplay/adversarios.*`) | Baralho, embaralhamento, distribuição e board com 3 arquivos de teste (PR #2, aberto) | Rebase do PR #2 sobre a `develop` (ver seção 8); operações novas do baralho para as trapaças (5.3); os 3 perfis de IA sobre um motor único; Insertion Sort das mãos (pendência 5) | Decisões do Gabriel sobre trapaças |
| **Gabriel** | Mecânicas (`src/gameplay/trapacas.*`, `probabilidade.*`, `progressao.*`) | Lista de mecânicas com proposta de implementação | Fechar as pendências 1, 2 e 10; implementar trapaças, chance de descoberta e suspeita individual; roleta russa e balas | Loop da mão do Arthur; baralho; motor |
| **Matheus** | Interface (`src/ui`) | Ideias de mecânicas no grupo | Telas da Raylib (mesa, cartas, fichas); animações (distribuir, apostar, trapaça, roleta); efeitos de tensão; decidir Raylib x SDL | Lista de eventos do Arthur; estado do jogo |

**Caminho crítico:** ajuste e merge do PR #2 (baralho) → loop da mão do Arthur. O PR #1 (motor) e o PR #3 (tela inicial e loop principal) já estão na `develop`. O Gabriel e o Matheus dependem do loop da mão para testar o que implementarem.

## 8. Integração imediata

1. **PR #1 (Júlia)** já está na `develop`. As quatro correções pedidas no review foram feitas: motor em `src/poker/`, demos em `examples/`, `poker_settle` compatível com C11 e testes de all-in e empate.
2. **PR #2 (Rafael)** foi criado em cima da versão antiga do motor (arquivos na raiz) e inclui os commits antigos da Júlia. Agora que o #1 entrou, precisa de **rebase** (ficando só os commits do baralho) e de três ajustes pequenos, que já foram testados:
   - `src/cards/deck.h`: trocar `#include "../../poker.h"` por `#include "../poker/poker.h"`.
   - `tests/test_edge_cases.c` e `tests/test_integration.c`: chamar `poker_settle(&table, hole[0], hole[1], board, &result)` (e `poker_settle(&table, NULL, NULL, NULL, &result)` no caso de fold).
   - Incluir os testes do baralho no Makefile.
   Com esses ajustes, os quatro programas de teste (`test_poker`, `test_deck`, `test_edge_cases`, `test_integration`) compilam com `-std=c11 -Wall -Wextra -Wpedantic -Werror` e passam.
3. **PR #3 (Arthur)** já está na `develop`: tela inicial, loop principal e alvo `make jogo`, com o `Makefile` e o `README.md` da Júlia preservados. O Rafael deve partir dessa `develop` ao refazer o #2, porque o `Makefile` e o `README.md` mudaram.
4. **Em paralelo**, depois do loop da mão: trapaças (Gabriel), IA (Rafael) e telas (Matheus).

## 9. Como contribuir

- Crie a branch a partir da `develop` (`feature/<assunto>`) e abra o PR **para a `develop`**. Não dê push direto em `main` nem em `develop`.
- Um PR por assunto, com commits em português e no presente.
- Mexa só no seu módulo. Para `README.md`, `Makefile` e arquivos de outros módulos, combine antes.
- Rode os testes antes de abrir o PR. No Windows com MSYS2 UCRT64: `mingw32-make test`.
- Testes vão em `tests/` e demos em `examples/`. Nenhum desses arquivos pode virar o `src/main.c`, que é o único ponto de entrada do jogo.
- Decisão nova ou alterada: atualize este arquivo no mesmo PR.
