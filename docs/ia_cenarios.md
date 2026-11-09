# IA probabilistica heads-up (primeira integracao)

Implementacao de decisao de adversarios do **Dead Man's Bluff**. Nao altera o motor da Julia, nao conhece Raylib, nao usa historico nem embriaguez. O core consulta `poker_ai_choose_action`, recebe uma `PokerAction`, e chama `poker_act` depois.

## Arquivos

- `src/gameplay/poker_ai.c` / `.h`: classificacao pre-flop, draws (metadados), equidade Monte Carlo pos-flop, escolha ponderada, legais e aposta relativa ao pote.
- `src/gameplay/poker_ai_config.c` / `.h`: **editar aqui** os 240 cenarios e os parametros de aposta e de short stack.
- `tests/test_poker_ai.c`: suite de selecao estatistica adaptada da revisao anterior do Claude (9030 checagens).
- `tests/test_ai_integration.c`: regressoes da integracao, incluindo pote disputavel, short stack, RNG e cartas invalidas.
- `tests/test_tabelas_config.c`: valida os 240 cenarios e os 48 `FREE`.

## Editar porcentagens

Cada celula segue `[PERFIL][STREET][FORCA][PRESSAO]` e tem `{fold, passiva, raise, raise_big}`; soma 100.

- `FREE`: `{0, check, bet, big_bet}`. Fold deve ser ZERO na propria tabela, para as chances de check nao crescerem secretamente.
- `SMALL`, `MEDIUM`, `LARGE`, `OVERBET`: `{fold, call, raise, raise_big}`.
- Se uma acao nao for permitida pelo motor (stack ou limite de aposta), seu peso sera eliminado e os **pesos das acoes ainda legais serao renormalizados**. `PokerAIDecisionInfo` mostra pesos configurados e pesos efetivos.

Exemplo: `/* AGGRESSIVE, FLOP, WEAK, FREE */ {{0,58,30,12}}` significa 58% CHECK, 30% BET, 12% BIG BET quando as apostas sao legais. Os percentuais ainda sao *provisorios*, nao uma estrategia testada em partidas reais.

## Apostas e personalidade

`poker_ai_bet_ranges[perfil][normal/big]` regula as faixas de aposta em relacao ao pote. Agressivo tem faixas ligeiramente maiores, Medroso menores. Ao fazer raise o codigo soma um incremento sobre o valor do call; a estrutura `PokerAction.total` usa o total da rodada. As faixas sao truncadas pelos limites `legal.min_total/max_total` e, portanto, BIG pode coincidir com NORMAL em stacks pequenos.

## Short stack: caso especial **explicito e configuravel**

O motor devolve a parte do adversario que nao foi coberta. Portanto, a pot odds efetiva usa somente o pote DISPUTAVEL depois do call: `call_cost / (2 * min(committed_da_IA + call_cost, committed_do_adversario))`.

Quando `call_cost < to_call` (call capado pelo stack), classificamos a pressao usando `call_cost / (2*committed_da_IA)` (se ja existe pote disputavel). A pressao nominal original tambem fica no debug. Exemplo com 100 fichas comprometidas por jogador, adversario aposta mais 100 e a IA tem 5: pot odds = `5/210 = 2,38%`, classificado como SMALL apesar de a aposta nominal ser 50% do pote anterior.

A regra especial esta visivel em `poker_ai_cheap_all_in_odds_max` (padrao **0.03**). Se o call e capado e custa no maximo 3% do pote disputavel, o peso `FOLD` e transferido para `CALL`. No pos-flop isso so ocorre se a equity estimada e pelo menos igual as pot odds. `info.cheap_all_in_override` informa a aplicacao. Coloque `0.0` para desligar. **Isso e uma excecao declarada e nao um novo cenario oculto.**

## API por parametro

`PokerAIContext` reside em `poker_ai_config.h` e tem `style`, `table` (pode ser `NULL` para usar `poker_ai_scenario_table`) e dois RNGs separados (`action_rng`, `equity_rng`). O core cria e inicializa ambos com `poker_ai_rng_seed`, preserva os estados entre turnos e passa o endereco do contexto. No pos-flop os RNGs devem ser objetos diferentes. Nao existe tabela global mutavel nem alteracao de `PokerTable` pela IA.

```c
#include "gameplay/poker_ai_config.h"
PokerAIRng action_rng, equity_rng;
poker_ai_rng_seed(&action_rng, 12345);
poker_ai_rng_seed(&equity_rng, 67890);
PokerAIContext ctx = {POKER_AI_AGGRESSIVE, NULL, &action_rng, &equity_rng};
PokerAIDecisionInfo info;
PokerAction next;
/* table, seat, hole, board e board_count devem vir do core. */
PokerStatus status = poker_ai_choose_action(&table, seat, hole, board,
                                           board_count, &ctx, &info, &next);
if (status == POKER_OK)
    status = poker_act(&table, seat, next);
```

## Testes (a partir da raiz do repositorio)

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 \
  -Isrc/poker -Isrc/gameplay \
  src/gameplay/poker_ai.c src/gameplay/poker_ai_config.c \
  src/poker/poker.c src/poker/apostas.c tests/test_poker_ai.c \
  -o test_poker_ai -lm
./test_poker_ai

cc -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 \
  -Isrc/poker -Isrc/gameplay \
  src/gameplay/poker_ai.c src/gameplay/poker_ai_config.c \
  src/poker/poker.c src/poker/apostas.c tests/test_ai_integration.c \
  -o test_ai_integration -lm
./test_ai_integration

cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Isrc/poker -Isrc/gameplay \
  src/gameplay/poker_ai_config.c tests/test_tabelas_config.c \
  -o test_tabelas_config
./test_tabelas_config
```

`make test` nao foi alterado: o Arthur pode integrar os testes posteriormente. A UI, torneio, trapaças, embriaguez e aprendizagem ficam fora deste commit. Monte Carlo usa 400 iteracoes contra mao aleatoria; nao conhece o range do adversario.
