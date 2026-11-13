# Decisões técnicas e divisão de trabalho

> Documento vivo do **Dead Man's Bluff**. Serve para o grupo saber o que já foi decidido, o que ainda está em aberto e o que cada pessoa precisa fazer.
>
> **Última atualização:** 13/11/2026.
> **Fontes:** documento de conceito v0.2 (fixado no grupo), os PRs já mesclados na `develop` e a conversa do grupo.
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

## 2. Decisões já tomadas

| Tema | Decisão | Onde está |
| --- | --- | --- |
| Linguagem | C, com Raylib na parte gráfica (decisão 3 da seção 6) | `Makefile`, `src/ui/interface.c` |
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
   - Humano: a UI devolve a ação escolhida. IA: `mao_ia_agir` (`src/core/mao_ia.h`) obtém a ação de `poker_ai_choose_action` (`src/gameplay/poker_ai.h`) e a aplica.
   - `poker_legal_actions` diz o que é permitido e `poker_act` aplica a ação.
5. Em `POKER_STREET_FINISHED`: `deck_deal_board(deck, street, board, &n)` e depois `poker_next_street`.
6. Em `POKER_SHOWDOWN` ou `POKER_UNCONTESTED`: o **core completa o board** e chama `poker_settle(&table, hole0, hole1, board, &result)`. Isso vale também quando o all-in acontece antes do river, porque o motor exige as 5 cartas do board no showdown.
7. **core** lê `result` (vencedor, prêmio, devoluções) e atualiza o torneio: saldo, blinds, falência e roleta. A UI anima.

**[Decidido]** Esse passo a passo está em `src/core/mao.c` (sem Raylib), com testes em `tests/test_mao.c`:

- `Mao` guarda o baralho, a mesa do motor, as cartas de cada assento, o board, o resultado e a situação (`MAO_VAZIA`, `MAO_EM_ANDAMENTO`, `MAO_ENCERRADA`, `MAO_REGISTRADA`). Inicialize com `Mao mao = {0};`.
- `mao_iniciar(&mao, &torneio)` embaralha, distribui e inicia o motor com o dealer, os stacks e os blinds do torneio. Se um stack for menor ou igual ao blind, a mão já se resolve dentro da chamada (board completo e pote pago), então confira `mao.situacao` depois dela.
- Enquanto `mao.situacao == MAO_EM_ANDAMENTO`, age o assento `mao.mesa.actor`. Quem decide a ação (a UI, para o jogador, ou a IA, para o adversário) chama `mao_agir(&mao, acao)`, que devolve o status do motor. Ação recusada não altera a mão.
- `mao_agir` distribui o board a cada rodada, completa o board quando há all-in antes do river (sem chamar `poker_next_street`) e paga o pote com `poker_settle`. Ao fim, `mao.resultado` tem o resultado e o baralho já foi liberado.
- `mao_registrar_no_torneio(&mao, &torneio)` passa as fichas finais ao torneio, uma única vez por mão. `mao_liberar(&mao)` solta o baralho de uma mão abandonada.
- `mao_iniciar_com_eventos(&mao, &torneio, &fila)` faz o mesmo e registra os eventos da mão para a interface (ver 3.3).
- `mao_cartas_ordenadas(&mao, assento, saida)` devolve as cartas de um assento (as duas da mão mais o board já revelado) da maior para a menor, em até `MAO_CARTAS_ORDENADAS` posições. Monta a lista de `src/cards/lista_cartas.*` e ordena com Insertion Sort (pendência 5). A mesa de teste mostra a mão do jogador assim; quem desenha decide o que mostrar, e as cartas do adversário só aparecem no showdown.
- No turno do adversário, `mao_ia_agir(&mao, &ia, embriagado, &info)` (`src/core/mao_ia.*`, do Rafael) pede a ação à IA com as cartas do adversário e o board, e a aplica por `mao_agir`. O `PokerAIContext` (perfil e RNGs próprios da IA) precisa viver durante todo o confronto.
- O embaralhamento usa `rand()`: o jogo chama `srand` uma vez, antes da primeira mão.
- Os testes jogam desistência, mão até o river, all-in no pré-flop, no flop e no turn, stacks menores que o blind e 120 torneios inteiros com ações aleatórias. Conferem que as fichas se conservam, que as cartas da mesa são todas distintas e que o vencedor bate com uma avaliação independente.

**[Decidido]** A partida inteira está em `src/core/partida.c` (sem Raylib), com testes em `tests/test_partida.c`. Quem desenha a tela só precisa deste módulo:

- `Partida` junta o torneio, a `Mao`, a fila de eventos e a IA do adversário. Zere com `Partida partida = {0};` e não copie a struct.
- `partida_iniciar(&partida, semente)` chama `srand` com a semente, prepara o torneio e a IA do primeiro adversário e inicia a primeira mão. A mesma semente com as mesmas ações reproduz a mesma partida.
- Olhe `partida.situacao` a cada quadro. Com `PARTIDA_VEZ_DO_JOGADOR`, chame `partida_agir_jogador(&partida, acao)` (`partida_acoes_legais` diz o que pode). Com `PARTIDA_VEZ_DO_ADVERSARIO`, chame `partida_agir_adversario(&partida, NULL)` no ritmo que a UI quiser, por exemplo depois de uma pausa, para o adversário não responder de imediato. Com `PARTIDA_MAO_ENCERRADA` ou `PARTIDA_CONFRONTO_VENCIDO`, chame `partida_continuar(&partida)`. `PARTIDA_JOGADOR_FALIDO` e `PARTIDA_CAMPEAO` encerram a partida.
- O resultado de cada mão já entra no torneio dentro da chamada que a encerra, e os eventos (3.3) ficam em `partida.eventos` para a UI consumir a cada quadro.
- O perfil da IA muda a cada confronto: Agressivo, Medroso e Desconfiado. `partida_nome_do_adversario` dá o nome para a tela. O modo embriagado fica em `partida.adversario_embriagado`, que as trapaças vão ligar; hoje é sempre falso.
- A falência hoje encerra a partida. A roleta (Gabriel) entra em `PARTIDA_JOGADOR_FALIDO`, chamando `torneio_recuperar_jogador` se o jogador sobreviver.
- Os testes jogam 120 partidas inteiras com três jogadores automáticos (passivo, all-in e aleatório), conferindo a conservação das fichas, o perfil da IA de cada confronto, um `EVENTO_POTE_PAGO` por mão e o mesmo resultado para a mesma semente.

### 3.2 Fronteiras de responsabilidade

- `poker` não conhece o baralho, a UI, a IA nem as trapaças. Só recebe cartas e ações.
- `ui` é o único módulo que conhece a Raylib e **não decide regra**.
- `core` orquestra os demais e guarda o estado do torneio.
- Rotação do dealer entre mãos é responsabilidade de quem chama o motor (`core`).
- `result.value[]` só vale quando `result.showdown` é verdadeiro.
- Stack 0 em `poker_start_hand` devolve erro: a falência precisa ser tratada **antes** de iniciar uma nova mão.

### 3.3 Eventos para a UI

**[Proposta]** O Matheus precisa saber *o que* animar. O `core` registra uma fila de eventos e a UI a consome, sem ler o estado do motor para descobrir o que mudou. O formato está em `src/core/eventos.h`, com testes em `tests/test_eventos.c`, para o Matheus confirmar ou ajustar:

| Evento | Quando | Dados principais |
| --- | --- | --- |
| `EVENTO_MAO_INICIADA` | nova mão | dealer, blinds nominais, fichas postadas e restantes de cada assento |
| `EVENTO_CARTAS_DISTRIBUIDAS` | uma vez por assento, a começar pelo dealer | assento e as duas cartas |
| `EVENTO_ACAO` | cada ação | assento, tipo (desistir, passar, pagar, apostar, aumentar, all-in), fichas colocadas, total na rua, fichas restantes e pote |
| `EVENTO_BOARD_REVELADO` | flop, turn e river, inclusive quando o all-in vem antes do river | rua e cartas novas |
| `EVENTO_SHOWDOWN` | só se ninguém desistiu | cartas e melhor mão de cada assento |
| `EVENTO_POTE_PAGO` | fim da mão | vencedor (ou empate), pote, prêmio, devolução e fichas finais |

- Para ligar a fila, use `mao_iniciar_com_eventos(&mao, &torneio, &fila)`. `mao_iniciar` continua valendo e não registra nada.
- Os eventos saem na ordem em que acontecem. No all-in antes do river vêm `EVENTO_ACAO`, os três `EVENTO_BOARD_REVELADO`, `EVENTO_SHOWDOWN` e `EVENTO_POTE_PAGO`, todos dentro da mesma chamada de `mao_agir`; o ritmo da animação é da UI.
- A fila é circular, com 256 eventos. A UI deve consumi-la a cada quadro com `fila_eventos_retirar`. Se ela encher, o evento novo é descartado e `fila.perdidos` aumenta; nesse caso a UI redesenha a mesa a partir do estado da mão.
- Ainda não existem eventos de trapaça aplicada, trapaça descoberta e roleta. Quem implementar essas mecânicas (Gabriel) acrescenta os tipos em `eventos.h`.

### 3.4 Contrato da interface (Matheus)

**[Decidido]** O `jogo.c` liga a partida à tela. A interface só implementa as funções de `src/ui/interface.h`. O que existe hoje é um **texto cru de teste, sem design**, só para o jogo poder ser jogado de ponta a ponta; o Matheus o substitui sem mexer no resto:

| Função | Para quê |
| --- | --- |
| `interface_mesa_ler_entrada()` | Traduz teclas ou cliques em uma `EntradaMesa`: comando (`COMANDO_DESISTIR`, `COMANDO_PASSAR_OU_PAGAR`, `COMANDO_APOSTAR`, `COMANDO_ALL_IN`), ajuste da aposta (-1 ou 1), continuar e voltar. Hoje: F, C, R, A, setas, ENTER ou ESPAÇO e ESC |
| `interface_mesa_registrar_evento(evento)` | Recebe cada evento da seção 3.3, na ordem em que aconteceu. É aqui que entram as animações |
| `interface_mesa_limpar_eventos()` | Chamada ao começar uma partida nova |
| `interface_mesa_desenhar(partida, aposta_escolhida)` | Desenha a mesa lendo a `Partida`, só para leitura |
| `interface_tempo_do_quadro()` | Duração do quadro, que o `jogo.c` usa para contar a pausa do adversário |

- A interface não decide regra: ela não chama funções de `poker`, `mao` ou `partida` que mudem o estado. Quem faz isso é o `jogo.c`.
- O que cada comando significa e qual aposta é válida é do core: `partida_comando_jogador` traduz o comando e `partida_ajustar_aposta` mantém o valor dentro dos limites, em passos de um big blind.
- O adversário espera 0,8 s (`PAUSA_DO_ADVERSARIO`, em `jogo.c`) antes de agir. Se uma animação precisar de mais tempo, combine com o Arthur em vez de travar o quadro.

## 4. Regras do jogo (resumo do conceito v0.2)

### Torneio

| Confronto | Adversário | Fichas iniciais dele |
| --- | --- | --- |
| 1 | Agressivo | 2.000 |
| 2 | Medroso | 4.000 |
| 3 | Desconfiado (final) | 8.000 |

- O jogador começa com 2.000 fichas e **leva o saldo acumulado** para a etapa seguinte. Exemplo do conceito: vencer o primeiro sem gastar nada em trapaças chega ao segundo com 4.000; gastando 600, chega com 3.400.
- Os blinds sobem a cada determinado número de mãos, para evitar partidas intermináveis. **Ao iniciar um novo confronto, os blinds reiniciam** em valores adequados à etapa.

**[Decidido]** A lógica do torneio está em `src/core/torneio.c` (sem Raylib), com testes em `tests/test_torneio.c`:

- `Torneio` guarda o confronto atual (0 a 2), as fichas de cada lado, as mãos jogadas, o dealer, as recuperações usadas e a situação (`EM_ANDAMENTO`, `CONFRONTO_VENCIDO`, `JOGADOR_FALIDO`, `CAMPEAO`).
- O assento 0 é sempre o jogador e o 1 é o adversário, como o motor espera. O dealer alterna a cada mão; no primeiro confronto e a cada confronto novo, o jogador começa como dealer.
- Depois de `poker_settle`, o core chama `torneio_registrar_mao` com as fichas finais dos dois assentos. O torneio **recusa** o registro se alguém ficar negativo, se os dois zerarem ou se a soma das fichas **aumentar**: fichas só saem de circulação (custos de trapaça), nunca surgem.
- Ao zerar o adversário, a situação vira `CONFRONTO_VENCIDO` (ou `CAMPEAO` no último) e o jogo chama `torneio_avancar_confronto`: o jogador leva o saldo, o adversário entra com as fichas do seu confronto e os blinds reiniciam.
- Ao zerar o jogador, a situação vira `JOGADOR_FALIDO`. A roleta (do Gabriel) decide o que acontece; se ele sobreviver, o core chama `torneio_recuperar_jogador`: o jogador volta com **metade das fichas iniciais do adversário daquela etapa** (2.000 no confronto de 4.000), o adversário mantém suas fichas e o nível dos blinds não muda. O contador `recuperacoes` existe para o Gabriel aplicar o limite de recuperações.

**[Proposta]** Valores escolhidos só para ter o que testar; o grupo ajusta jogando (pendência 11). Ficam em `src/core/torneio.h` e `src/core/torneio.c`:

| Valor | Hoje |
| --- | --- |
| Big blind inicial (confrontos 1, 2 e 3) | 20, 40 e 80 (small blind = metade) |
| Mãos por nível de blind | 5 (`TORNEIO_MAOS_POR_NIVEL`) |
| Fator de aumento do blind | x2 (`TORNEIO_FATOR_BLIND`) |
| Teto do big blind | 1.000.000 (`TORNEIO_BLIND_MAXIMO`), só para evitar estouro |

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
| 3 | Raylib ou SDL | Grupo (Arthur e Matheus) | **[Decidido]** Raylib: o jogo, as demos e o contrato da interface (3.4) já usam. O Matheus avisa no PR se tiver motivo para mudar | — |
| 4 | Idioma dos nomes no código | Grupo | Escolher um critério e registrar aqui. Hoje: motor e baralho em inglês (`poker_*`, `deck_*`); o placeholder do baralho se chamava `baralho` | Todos |
| 5 | Dono do **Insertion Sort** das mãos e da lista encadeada de cartas (`src/cards/lista_cartas.*`) | Rafael e Arthur | **[Decidido]** O Arthur implementou a lista e o Insertion Sort em `src/cards/lista_cartas.*`, com testes. A lista guarda uma mão (as cartas do jogador e do board) e ordena por valor e naipe, nos dois sentidos, sem alocar memória. A `Mao` a usa em `mao_cartas_ordenadas` (3.1) e a mesa de teste mostra a mão ordenada. O baralho continua com a lista própria de `deck.*` | — |
| 6 | Dono de `adversarios.*` | Grupo | **[Decidido]** O Rafael é o dono da IA. Ela fica em `src/gameplay/poker_ai*` e a ponte com a mão em `src/core/mao_ia.*`; os placeholders `adversarios.*` foram removidos e o README já lista o Rafael na IA | — |
| 7 | 3 confrontos (conceito v0.2) ou 5 (ideia do Matheus no Discord) | Arthur e Rafael | **[Decidido]** 3 confrontos na v1, como no conceito v0.2 (`TORNEIO_CONFRONTOS`). Mais confrontos ficam para a visão futura | — |
| 8 | Padrão C do projeto | Grupo | C11 com `-Wall -Wextra -Wpedantic` | Build |
| 9 | Makefile único (jogo + testes + demos) | Arthur | **[Decidido]** Um só `Makefile` na raiz, com `make jogo`, `make test` e as demos | — |
| 10 | Funcionamento definitivo do ás na manga | Gabriel | Ver 5.1 | Trapaças |
| 11 | Valores a balancear, listados no conceito v0.2 (seção 10): valores e frequência de aumento dos blinds, probabilidades-base de descoberta, suspeita acrescentada por tentativa, redução da suspeita após sobreviver à roleta, balas adicionadas por punição, balas removidas por vitória, penalidade financeira ao ser descoberto, limite de recuperações por falência, duração da embriaguez | Grupo | Deixar como constantes fáceis de ajustar e decidir testando | Balanceamento |

## 7. Quem faz o quê

**[Proposta]** Baseada na divisão inicial e no que foi dito no grupo. Confirmem ou ajustem.

| Pessoa | Área | Já feito | Próximos passos | Depende de |
| --- | --- | --- | --- | --- |
| **Arthur** | Engine e arquitetura (`src/core`) | Estrutura do repositório, regras de branch, tela inicial e loop principal com troca de estados (PR #3); lógica do torneio com testes (PR #5); revisão e merge dos PRs #1, #2 e #6; testes do baralho ligados ao `make test`; loop da mão com testes (`src/core/mao.*`); fila de eventos para a UI com testes (`src/core/eventos.*`); controlador da partida com testes (`src/core/partida.*`), com o perfil da IA de cada confronto; mesa jogável de ponta a ponta, com o `jogo.c` ligado à `Partida` e a interface em texto cru de teste (3.4); README atualizado e pendências 3, 6, 7 e 9 fechadas (PR #14); lista encadeada de cartas com Insertion Sort e testes (`src/cards/lista_cartas.*`) | Integrar as trapaças e a roleta à partida; revisar e mesclar os PRs | Peixoto, para as trapaças e a roleta |
| **Júlia** | Motor de pôquer (`src/poker`) | Avaliação, apostas e pote com testes; correções do review aplicadas (PR #1, já na `develop`); contrato do motor escrito em `poker.h`: fases, `poker_start_hand` e `poker_settle` (PR #6, já na `develop`) | Decidir com o Gabriel os custos no meio da mão (5.2); newline final em `src/poker/poker.c` | Decisões do Gabriel sobre custos |
| **Rafael** | Cartas e IA (`src/cards`, `src/gameplay/poker_ai*`) | Baralho, embaralhamento, distribuição e board com 3 arquivos de teste (PR #2, já na `develop`); motor de decisão da IA com os 3 perfis e testes, em `src/gameplay/poker_ai*` (PR #7, já na `develop`); ponte entre a `Mao` e a IA com teste, em `src/core/mao_ia.*` (PR #10, já na `develop`) | Operações novas do baralho para as trapaças (5.3); ajuste fino da IA jogando | Decisões do Gabriel sobre trapaças |
| **Gabriel** | Mecânicas (`src/gameplay/trapacas.*`, `probabilidade.*`, `progressao.*`) | Lista de mecânicas com proposta de implementação | Fechar as pendências 1, 2 e 10; implementar trapaças, chance de descoberta e suspeita individual; roleta russa e balas | Loop da mão do Arthur |
| **Matheus** | Interface (`src/ui`) | Ideias de mecânicas no grupo | Telas da Raylib no lugar do texto de teste (mesa, cartas, fichas), seguindo o contrato de 3.4; animações a partir dos eventos de 3.3 (distribuir, apostar, trapaça, roleta); efeitos de tensão; decidir Raylib x SDL | Confirmar a lista de eventos (3.3). A partida já roda, então não depende mais do Arthur |

**Caminho crítico:** o Matheus desenhar a mesa de verdade (cartas, fichas e animações) sobre o contrato de 3.4. O jogo já é jogável de ponta a ponta com a interface de teste: partida, torneio, mão, fila de eventos e IA estão prontos e testados em `src/core/partida.*`. O motor (PR #1, com o contrato no PR #6), o baralho (PR #2), a tela inicial e o loop principal (PR #3), o torneio (PR #5), o loop da mão, a IA (PR #7) e a ponte entre a mão e a IA (PR #10) já estão na `develop`, então o Gabriel já pode testar trapaças em cima de `Mao`. O Matheus depende da lista de eventos para animar.

## 8. Integração imediata

1. **PR #1 (Júlia)** já está na `develop`. As quatro correções pedidas no review foram feitas: motor em `src/poker/`, demos em `examples/`, `poker_settle` compatível com C11 e testes de all-in e empate.
2. **PR #2 (Rafael)** já está na `develop`: baralho em `src/cards/deck.*`, que usa o `PokerCard` do motor, com os testes `test_deck`, `test_edge_cases` e `test_integration`. O placeholder `src/cards/baralho.*` foi removido.
3. **PR #3 (Arthur)** já está na `develop`: tela inicial, loop principal e alvo `make jogo`.
4. **PR #5 (Arthur)** já está na `develop`: torneio em `src/core/torneio.*`, com `tests/test_torneio.c`.
5. **PR #6 (Júlia)** já está na `develop`: contrato do motor documentado em `poker.h`.
6. **Loop da mão (Arthur)** já está na `develop`: `src/core/mao.*`, com `tests/test_mao.c` (ver 3.1). Ainda não está ligado à tela: o `make jogo` continua mostrando a mesa provisória.
7. **Fila de eventos (Arthur)** já está na `develop`: `src/core/eventos.*`, com `tests/test_eventos.c` (ver 3.3). A `Mao` registra os eventos quando iniciada com `mao_iniciar_com_eventos`.
8. **PR #7 e PR #10 (Rafael)** já estão na `develop`: o motor de decisão da IA em `src/gameplay/poker_ai*` e a ponte `mao_ia_agir` em `src/core/mao_ia.*`, com testes. O primeiro workflow de CI do repositório (`.github/workflows/ci-c.yml`) veio no PR #7 e roda os testes em Linux nos PRs para a `develop`.
9. **Controlador da partida e mesa jogável (Arthur)** já estão na `develop`: `src/core/partida.*`, com `tests/test_partida.c` (ver 3.1), e o `jogo.c` ligado à `Partida` e à interface (ver 3.4). O `make jogo` abre um jogo completo contra os 3 adversários, com a interface em texto cru de teste para o Matheus substituir.
10. O `make test` roda os programas de teste do motor, do torneio, da mão, dos eventos, do baralho, da IA e da partida com `-std=c11 -Wall -Wextra -Wpedantic -Werror`.
11. **Em paralelo**, depois do loop da mão: trapaças (Gabriel) e telas (Matheus).

## 9. Como contribuir

- Crie a branch a partir da `develop` (`feature/<assunto>`) e abra o PR **para a `develop`**. Não dê push direto em `main` nem em `develop`.
- Um PR por assunto, com commits em português e no presente.
- Mexa só no seu módulo. Para `README.md`, `Makefile` e arquivos de outros módulos, combine antes.
- Rode os testes antes de abrir o PR. No Windows com MSYS2 UCRT64: `mingw32-make test`.
- Testes vão em `tests/` e demos em `examples/`. Nenhum desses arquivos pode virar o `src/main.c`, que é o único ponto de entrada do jogo.
- Decisão nova ou alterada: atualize este arquivo no mesmo PR.
