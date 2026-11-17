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
- Olhe `partida.situacao` a cada quadro. Com `PARTIDA_VEZ_DO_JOGADOR`, chame `partida_agir_jogador(&partida, acao)` (`partida_acoes_legais` diz o que pode). Com `PARTIDA_VEZ_DO_ADVERSARIO`, chame `partida_agir_adversario(&partida, NULL)` no ritmo que a UI quiser, por exemplo depois de uma pausa, para o adversário não responder de imediato. Com `PARTIDA_MAO_ENCERRADA` ou `PARTIDA_CONFRONTO_VENCIDO`, chame `partida_continuar(&partida)`. Com `PARTIDA_ROLETA`, chame `partida_puxar_gatilho(&partida)` quando o jogador pedir. `PARTIDA_JOGADOR_FALIDO` (sem recuperações sobrando), `PARTIDA_JOGADOR_MORTO` e `PARTIDA_CAMPEAO` encerram a partida.
- O resultado de cada mão já entra no torneio dentro da chamada que a encerra, e os eventos (3.3) ficam em `partida.eventos` para a UI consumir a cada quadro.
- O perfil da IA muda a cada confronto: Agressivo, Medroso e Desconfiado. `partida_nome_do_adversario` dá o nome para a tela. O modo embriagado fica em `partida.adversario_embriagado`, que as trapaças vão ligar; hoje é sempre falso.
- **Roleta:** quando o jogador zera, a partida vai para `PARTIDA_ROLETA` (com `partida.roleta_motivo == ROLETA_FALENCIA`) e espera o gatilho. `partida_puxar_gatilho` sorteia com um gerador da `Partida` (`estado_do_risco`), de modo que refazer a mão dá o mesmo resultado. Se o jogador morre, a situação vira `PARTIDA_JOGADOR_MORTO`. Se sobrevive, entra uma bala no tambor, o contador de usos das trapaças zera, o core chama `torneio_recuperar_jogador` e a situação volta a `PARTIDA_MAO_ENCERRADA`, para o jogador seguir com `partida_continuar`. Depois de `PARTIDA_RECUPERACOES_MAXIMAS` (3) recuperações, a próxima falência já é definitiva (`PARTIDA_JOGADOR_FALIDO`), sem roleta. Ao vencer um confronto, `partida_continuar` tira uma bala (mínimo 1). As regras estão na seção 4, e o risco (`SistemaRisco`) vive na `Partida`.
- **Trapaças:** na vez do jogador, `partida_trapacear(&partida, pedido, &resultado)` tenta uma trapaça sem gastar a vez dele. `partida_pode_trapacear`, `partida_custo_da_trapaca` e `partida_chance_de_descoberta` dizem à interface o que vale agora, quanto custa e qual o risco. O custo sai da pilha do jogador na hora, e o mesmo valor sai do total da mão, porque essas fichas saem de circulação (5.2). A recusa do dealer (30%, só no suborno) e a descoberta saem do gerador da `Partida` (`estado_do_risco`), nessa ordem. Ser descoberto leva à roleta (`ROLETA_DESCOBERTO`), e sobreviver devolve a vez ao jogador. O estado físico das trapaças fica em `partida.cheats` (`gameplay/trapacas.h`) e o risco em `partida.risco`; os módulos do Gabriel não sorteiam e não conhecem a `Partida`. A bebedeira do adversário (`partida.adversario_embriagado`) deriva de `cheats.maos_embriagado`.
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
| `EVENTO_ROLETA` | o jogador puxou o gatilho | se veio de falência ou de trapaça descoberta, se morreu, e as balas antes e depois |
| `EVENTO_TRAPACA` | uma trapaça foi tentada ou fez efeito | qual trapaça, resultado (sucesso, descoberta ou recusa do dealer), se veio na distribuição das cartas, custo, cartas envolvidas e, no cúmplice, a categoria da mão do adversário |

- Para ligar a fila, use `mao_iniciar_com_eventos(&mao, &torneio, &fila)`. `mao_iniciar` continua valendo e não registra nada.
- Os eventos saem na ordem em que acontecem. No all-in antes do river vêm `EVENTO_ACAO`, os três `EVENTO_BOARD_REVELADO`, `EVENTO_SHOWDOWN` e `EVENTO_POTE_PAGO`, todos dentro da mesma chamada de `mao_agir`; o ritmo da animação é da UI.
- A fila é circular, com 256 eventos. A UI deve consumi-la a cada quadro com `fila_eventos_retirar`. Se ela encher, o evento novo é descartado e `fila.perdidos` aumenta; nesse caso a UI redesenha a mesa a partir do estado da mão.
- O que uma trapaça revela chega por `EVENTO_TRAPACA` (a categoria da mão do adversário no cúmplice, as duas cartas dele no suborno de ver, a carta marcada reconhecida na mão dele), e a interface decide como mostrar. Os efeitos que só acontecem quando as cartas são dadas vêm com `na_distribuicao`: a carta do suborno entrando na mesa ou na mão, ou a recusa do dealer quando ela já saiu do baralho.

### 3.4 Contrato da interface (Matheus)

**[Decidido]** O `jogo.c` liga a partida à tela. A interface só implementa as funções de `src/ui/interface.h`. O que existe hoje é um **texto cru de teste, sem design**, só para o jogo poder ser jogado de ponta a ponta; o Matheus o substitui sem mexer no resto:

| Função | Para quê |
| --- | --- |
| `interface_mesa_ler_entrada()` | Traduz teclas ou cliques em uma `EntradaMesa`: comando (`COMANDO_DESISTIR`, `COMANDO_PASSAR_OU_PAGAR`, `COMANDO_APOSTAR`, `COMANDO_ALL_IN`), ajuste da aposta (-1 ou 1), continuar, voltar e o pedido de trapaça (`PedidoTrapaca`, ou `PEDIDO_NENHUM`). Hoje: F, C, R, A, setas, ENTER ou ESPAÇO, ESC e T (menu das trapaças) |
| `interface_mesa_registrar_evento(evento)` | Recebe cada evento da seção 3.3, na ordem em que aconteceu. É aqui que entram as animações |
| `interface_mesa_limpar_eventos()` | Chamada ao começar uma partida nova |
| `interface_mesa_desenhar(partida, aposta_escolhida)` | Desenha a mesa lendo a `Partida`, só para leitura |
| `interface_tempo_do_quadro()` | Duração do quadro, que o `jogo.c` usa para contar a pausa do adversário |
| `interface_configuracoes_ler_entrada()` e `interface_configuracoes_desenhar(configuracoes, selecionada)` | Tela de configurações, hoje só com a lista de resoluções |
| `interface_resolucao_cabe(resolucao)` e `interface_aplicar_resolucao(resolucao)` | Dizem se a resolução cabe no monitor e passam a janela para ela, ou para a tela cheia sem bordas |
| `interface_menu_atualizar(opcao, continuar_disponivel)` e `interface_menu_desenhar(opcao, continuar_disponivel, aviso)` | Menu inicial com Continuar, Novo jogo, Configurações e Sair; Continuar fica apagado e é pulado quando não há jogo salvo |
| `interface_confirmacao_ler_resposta()` e `interface_confirmacao_desenhar(pergunta, detalhe)` | Pergunta de sim ou não, usada ao começar um jogo novo por cima de um salvo |

- **Trapaças:** o jogador abre o menu das trapaças com T (ou clicando em `[T] trapaças`), só na vez dele. O menu lista as sete opções com o custo (`partida_custo_da_trapaca`), o risco (`partida_chance_de_descoberta`) e se valem agora (`partida_pode_trapacear`); as que pedem uma carta da mão ou uma carta ao dealer abrem um seletor. A interface só devolve o pedido pronto em `EntradaMesa.pedido`, quando o jogador confirma, e o `jogo.c` chama `partida_trapacear`. O estado do menu é só da interface e some quando a vez deixa de ser do jogador. A mesa de teste também mostra a carta da manga, as cartas marcadas (`is_card_marked`), a bebedeira do adversário e as cartas que um suborno revelou; esta última a interface guarda a partir do `EVENTO_TRAPACA`, até a mão seguinte. O design definitivo do menu é do Matheus, mas deve manter o contrato: tudo pelo teclado e por clique, e nada de decidir regras na interface.
- **Mouse:** tudo o que o teclado faz também se faz com o mouse, e as funções acima já devolvem o resultado dos cliques como se fossem teclas (por exemplo, clicar em `[C] passar ou pagar` devolve `COMANDO_PASSAR_OU_PAGAR` em `EntradaMesa`). Na tela de configurações, `EntradaConfiguracoes.apontada` diz qual opção está sob o mouse, para o `jogo.c` destacá-la. A interface registra as áreas clicáveis ao desenhar, nas coordenadas de `LARGURA_TELA` por `ALTURA_TELA`, e a leitura do quadro seguinte consulta essas áreas; as telas novas do Matheus devem manter essa equivalência entre clique e tecla.
- **Resolução:** as telas desenham sempre em `LARGURA_TELA` por `ALTURA_TELA` (1280x720), e a interface escala o desenho para a janela real, com faixas nas bordas se a proporção for outra. Telas novas usam essas coordenadas e não precisam saber o tamanho da janela. A lista de resoluções (960x540, 1280x720, 1600x900, 1920x1080 e tela cheia) e a escolha do jogador ficam em `src/core/configuracoes.*`. Uma janela só é oferecida se couber no monitor com a barra de título.

- A interface não decide regra: ela não chama funções de `poker`, `mao` ou `partida` que mudem o estado. Quem faz isso é o `jogo.c`.
- O que cada comando significa e qual aposta é válida é do core: `partida_comando_jogador` traduz o comando e `partida_ajustar_aposta` mantém o valor dentro dos limites, em passos de um big blind.
- O adversário espera 0,8 s (`PAUSA_DO_ADVERSARIO`, em `jogo.c`) antes de agir. Se uma animação precisar de mais tempo, combine com o Arthur em vez de travar o quadro.

### 3.5 Salvar e continuar

**[Decidido]** O jogo guarda a partida em `progresso.sav` (pasta de onde o jogo foi aberto, fora do git) e o menu oferece Continuar. O salvamento é automático: a cada jogada e a cada mão nova, ao voltar ao menu e ao sair. Quando a partida acaba, por falência definitiva, morte na roleta ou título, o arquivo é apagado. Começar um jogo novo com um salvo pede confirmação.

Para continuar **no meio de uma mão**, o que se guarda não é uma cópia da memória, e sim o suficiente para **refazer a mão**:

- `partida_exportar` junta numa `PartidaSalva` o torneio **no início da mão atual**, a semente do baralho da mão, o estado do gerador das sementes, os estados dos geradores da IA, o risco (balas e usos das trapaças) e o gerador do risco **no início da mão**, o estado das trapaças (`cheats`: bebedeira, carta da manga, marcas e suborno à espera) também no início da mão, e o **registro da mão**: tudo o que aconteceu nela, na ordem (cada ação do motor, do jogador ou da IA, cada pedido de trapaça e cada gatilho da roleta), até `PARTIDA_REGISTRO_MAXIMO` entradas.
- `partida_restaurar` volta o torneio e o risco ao início da mão, embaralha com a mesma semente, reaplica o registro (`mao_agir` para as ações, `partida_puxar_gatilho` para o gatilho) e confere se cartas, fichas e situação batem com os guardados. A roleta também pode ser guardada: na situação `PARTIDA_ROLETA` o jogo continua esperando o gatilho. Se não baterem, recusa, e o jogo avisa que não foi possível carregar.
- Cada mão sorteia a própria semente com um gerador da `Partida` (não com o `rand` global) e chama `srand` com ela logo antes de embaralhar. Por isso duas partidas no mesmo programa não se atrapalham, e o baralho de uma mão sai igual ao ser refeito.
- O formato do arquivo é texto simples, com versão (`DMBSAVE 3`), e o leitor confere cada campo; arquivo cortado, alterado ou de outra versão é recusado sem derrubar o jogo (`src/core/salvamento.*`).
- O baralho de uma mão só se refaz igual no mesmo sistema e na mesma compilação, porque depende do `rand` da biblioteca C. Um salvamento levado a outro computador pode ser recusado pela conferência.

**Regra para tudo o que mudar o estado do jogo fora das ações do motor** (carta trocada ou tirada do baralho, carta marcada, custo pago, duração da embriaguez, uma trapaça descoberta): precisa virar uma entrada do registro da mão e ser refeita por `partida_restaurar` na mesma ordem, e o estado que ela muda precisa estar no que se guarda no início da mão (`PartidaSalva`). Os sorteios dessas mecânicas saem de geradores da `Partida`, nunca do `rand` global. O teste `tests/test_salvamento.c` refaz partidas salvas e as compara com as originais passo a passo, e vai acusar a diferença. A roleta e as trapaças já seguem essa regra: cada pedido de trapaça é uma entrada do registro (código 7 no arquivo) e `PartidaSalva.cheats` guarda o estado delas no início da mão.

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
- Ao zerar o jogador, a situação do torneio vira `JOGADOR_FALIDO` e a partida vai para a roleta (3.1). Se ele sobreviver, o core chama `torneio_recuperar_jogador`: o jogador volta com **metade das fichas iniciais do adversário daquela etapa** (2.000 no confronto de 4.000), o adversário mantém suas fichas e o nível dos blinds não muda. O contador `recuperacoes` limita isso a `PARTIDA_RECUPERACOES_MAXIMAS` (3) por partida.

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

Os custos e as chances de ser descoberto ainda não têm valores definitivos: serão ajustados testando.

**Como ficou no jogo (decidido, ajustável):**

- **Spike his drink:** sem custo; o adversário fica embriagado por 3 mãos (a mão atual conta, e a bebedeira passa a cada mão que termina). Risco-base 10%, mais 8% por uso.
- **Cúmplice:** custa o big blind vezes (usos naquele nível de blind + 1), e a conta recomeça a cada nível de blind. Revela a categoria da melhor mão do adversário com o que está à vista (par ou carta alta antes do flop). Risco 15%, mais 10% por uso.
- **Marcar carta:** sem custo; uma carta da própria mão por mão, em até 5 espaços; a marca dura 5 mãos contando a mão em que foi feita. No início de cada mão o jogo avisa se uma carta marcada caiu na mão do adversário. Risco 5%, mais 2,75% por uso.
- **Suborno do dealer:** uma ação por mão. O valor-base é o das fichas iniciais do adversário dividido por 20 (100, 200 ou 400), vezes 1 (a carta pedida será a primeira da próxima rua do board), 2 (a carta pedida vem para a sua mão na mão seguinte) ou 3 (ver as duas cartas do adversário). O dealer recusa 30% das vezes e fica com o dinheiro; ser descoberto também custa o valor. Se a carta pedida já tiver saído do baralho quando chegar a hora (queimada, ou dada a alguém), o dealer não a acha e o dinheiro se perde. Risco 20%, mais 7% por uso.
- **Ás na manga:** sem custo; troca uma carta da mão com a carta da manga, que começa como o 2 de paus. A manga fica fora do baralho e atravessa as mãos e os confrontos. Risco 25%, mais 5% por uso.
- Pagar uma trapaça nunca pode deixar o jogador sem fichas.
- Ao vencer um confronto, a bebedeira, as marcas e os usos do cúmplice recomeçam; a manga e o suborno que ainda não chegou à mão continuam.

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

- **[Decidido, ajustável]** Se sobreviver: **entra uma bala** no tambor (até 6) e a suspeita do adversário atual (os usos de cada trapaça) **recomeça do zero**. O conceito dizia que a suspeita cai bastante, sem zerar; quem balancear muda `prob_jogar_roleta_russa`.
- Se a roleta veio de falência: o jogador recebe **metade do valor de referência da etapa** para tentar continuar (no confronto de 4.000, volta com 2.000), com um limite de **3 recuperações** por partida (`PARTIDA_RECUPERACOES_MAXIMAS`) para evitar exploração; depois disso a falência é definitiva e não há roleta.
- **[Decidido, ajustável]** Ao vencer um confronto, o jogador remove **uma bala** do revólver (mínimo de 1) e a suspeita recomeça (`prob_avancar_mesa`).
- O sorteio do gatilho vem de um gerador da `Partida` (`estado_do_risco`), então o resultado é reproduzível: o salvamento e o jogo refeito dão o mesmo resultado.

### Economia

Três recursos ligados: **fichas** (apostar, trapacear e continuar), **suspeita** (individual por adversário) e **balas** (perigo acumulado, removíveis como recompensa por vitória).

## 5. Trapaças e motor de pôquer

Pontos em que as trapaças esbarram no que já existe. Precisam ser resolvidos **antes** da implementação.

### 5.1 Cartas repetidas

O baralho gera 52 cartas únicas e o `poker_settle` **recusa** o showdown se houver carta repetida (`POKER_ERR_DUPLICATE_CARD`). Se uma trapaça colocar a mesma carta em dois lugares, a mão não consegue ser paga.

- A ideia "copiar carta", levantada na conversa, cria repetidas. Seria preciso retirar as travas do baralho e do `poker_settle`, e o Rafael considera a trapaça forte demais.
- A alternativa proposta pelo Rafael é **trocar sempre com uma carta real do baralho** (o jogador devolve uma e pega outra que já existe), e assim nunca repete.
- No "ás na manga", a carta guardada na manga **precisa sair do baralho** quando é separada, para continuar valendo "52 cartas únicas".

**[Decidido]** Cartas únicas, sem copiar. A carta da manga sai do baralho antes de embaralhar cada mão (`MaoAjustes.reservar_carta`), então fica fora do jogo até ser trocada; o suborno e a troca com a manga sempre trocam com uma carta real (`deck_exchange` põe a carta devolvida no lugar da que sai, na mesma posição). Se a carta pedida já saiu do baralho, a troca falha e nada muda. O `poker_settle` continua recusando repetidas, e os testes conferem que nunca há.

### 5.2 Custos em fichas

O motor não tem como cobrar uma taxa no meio da mão. Se o `core` alterar `table.player[i].chips` diretamente, a conta "fichas + pote = total inicial" deixa de fechar (e o conceito diz que esses pagamentos saem de circulação).

- Custos que valem **por mão** (cúmplice, suborno) podem ser descontados dos stacks **antes** de `poker_start_hand`, sem mudar o motor.
- Se algum custo precisar ser pago **durante** a mão, o motor precisa de uma função própria que registre a taxa fora do pote.

**[Decidido por enquanto, sem mudar o motor]** O custo sai da pilha do jogador **na hora do pedido**, mesmo no meio da mão, e o core tira o mesmo valor de `PokerTable.initial_total`, para a conta "fichas + pote = total inicial" continuar fechando; o torneio aceita o resultado porque a soma das fichas só diminui. O pedido é recusado se pagar deixasse o jogador com 0 fichas (viraria all-in sem ter apostado). Se a Júlia preferir uma função própria no motor, só `executar_pedido`, em `partida.c`, muda.

### 5.3 Novas operações no baralho

O baralho atual só sabe comprar a próxima carta, queimar, distribuir e distribuir o board. As trapaças pedem mais:

- Tirar uma carta **específica** do baralho (suborno: "receber carta específica", e a carta da manga).
- Espiar as próximas cartas.
- Devolver uma carta ao baralho (troca com carta do baralho).

**[Feito]** `deck_contains`, `deck_remove_card` (tira uma carta específica, usada para a manga) e `deck_exchange` (troca uma carta do baralho por outra, na mesma posição) estão em `src/cards/deck.*`, com testes. Espiar as próximas cartas não foi preciso.

### 5.4 Informação revelada

Marcar carta, espelho e parte do suborno só **revelam informação** (cartas do adversário, cartas da mesa). Isso é estado de jogo mostrado pela UI e **não exige mudança no motor de pôquer**.

## 6. Decisões pendentes

| # | Decisão | Quem decide | Recomendação | Bloqueia |
| --- | --- | --- | --- | --- |
| 1 | Política de cartas repetidas nas trapaças (5.1) | Gabriel, com Rafael e Júlia | **[Decidido]** Cartas únicas; trocar sempre com carta real do baralho | — |
| 2 | Custos de trapaça no meio da mão (5.2) | Júlia e Gabriel | **[Decidido por enquanto]** Cobrar na hora e baixar o total da mão no core; a Júlia pode pedir uma função no motor | — |
| 3 | Raylib ou SDL | Grupo (Arthur e Matheus) | **[Decidido]** Raylib: o jogo, as demos e o contrato da interface (3.4) já usam. O Matheus avisa no PR se tiver motivo para mudar | — |
| 4 | Idioma dos nomes no código | Grupo | Escolher um critério e registrar aqui. Hoje: motor e baralho em inglês (`poker_*`, `deck_*`); o placeholder do baralho se chamava `baralho` | Todos |
| 5 | Dono do **Insertion Sort** das mãos e da lista encadeada de cartas (`src/cards/lista_cartas.*`) | Rafael e Arthur | **[Decidido]** O Arthur implementou a lista e o Insertion Sort em `src/cards/lista_cartas.*`, com testes. A lista guarda uma mão (as cartas do jogador e do board) e ordena por valor e naipe, nos dois sentidos, sem alocar memória. A `Mao` a usa em `mao_cartas_ordenadas` (3.1) e a mesa de teste mostra a mão ordenada. O baralho continua com a lista própria de `deck.*` | — |
| 6 | Dono de `adversarios.*` | Grupo | **[Decidido]** O Rafael é o dono da IA. Ela fica em `src/gameplay/poker_ai*` e a ponte com a mão em `src/core/mao_ia.*`; os placeholders `adversarios.*` foram removidos e o README já lista o Rafael na IA | — |
| 7 | 3 confrontos (conceito v0.2) ou 5 (ideia do Matheus no Discord) | Arthur e Rafael | **[Decidido]** 3 confrontos na v1, como no conceito v0.2 (`TORNEIO_CONFRONTOS`). Mais confrontos ficam para a visão futura | — |
| 8 | Padrão C do projeto | Grupo | C11 com `-Wall -Wextra -Wpedantic` | Build |
| 9 | Makefile único (jogo + testes + demos) | Arthur | **[Decidido]** Um só `Makefile` na raiz, com `make jogo`, `make test` e as demos | — |
| 10 | Funcionamento definitivo do ás na manga | Gabriel | **[Decidido]** Troca uma carta da mão com a manga, que começa com o 2 de paus e fica fora do baralho (5.1) | — |
| 11 | Valores a balancear, listados no conceito v0.2 (seção 10): valores e frequência de aumento dos blinds, probabilidades-base de descoberta, suspeita acrescentada por tentativa, redução da suspeita após sobreviver à roleta, balas adicionadas por punição, balas removidas por vitória, penalidade financeira ao ser descoberto, limite de recuperações por falência, duração da embriaguez | Grupo | Deixar como constantes fáceis de ajustar e decidir testando. **[Decidido por enquanto]** Sobreviver à roleta soma 1 bala e zera a suspeita; vencer um confronto tira 1 bala; 3 recuperações por partida; os demais seguem em `probabilidade.c` e `trapacas.c` | Balanceamento |

## 7. Quem faz o quê

**[Proposta]** Baseada na divisão inicial e no que foi dito no grupo. Confirmem ou ajustem.

| Pessoa | Área | Já feito | Próximos passos | Depende de |
| --- | --- | --- | --- | --- |
| **Arthur** | Engine e arquitetura (`src/core`) | Estrutura do repositório, regras de branch, tela inicial e loop principal com troca de estados (PR #3); lógica do torneio com testes (PR #5); revisão e merge dos PRs #1, #2 e #6; testes do baralho ligados ao `make test`; loop da mão com testes (`src/core/mao.*`); fila de eventos para a UI com testes (`src/core/eventos.*`); controlador da partida com testes (`src/core/partida.*`), com o perfil da IA de cada confronto; mesa jogável de ponta a ponta, com o `jogo.c` ligado à `Partida` e a interface em texto cru de teste (3.4); README atualizado e pendências 3, 6, 7 e 9 fechadas (PR #14); lista encadeada de cartas com Insertion Sort e testes (`src/cards/lista_cartas.*`); tela de configurações com a escolha de resolução (PR #18); salvar e continuar o jogo, inclusive no meio de uma mão (3.5); roleta russa e risco na partida (PR #23); trapaças ligadas ao core, com custo no meio da mão, suborno, manga e registro no salvamento (3.1, 3.5 e 5); menu das trapaças e seletor de carta na mesa de teste (3.4) | revisar e mesclar os PRs; ajudar o Matheus a trocar a mesa de teste pelo design, seguindo o contrato de 3.4 | — |
| **Júlia** | Motor de pôquer (`src/poker`) | Avaliação, apostas e pote com testes; correções do review aplicadas (PR #1, já na `develop`); contrato do motor escrito em `poker.h`: fases, `poker_start_hand` e `poker_settle` (PR #6, já na `develop`) | Decidir com o Gabriel os custos no meio da mão (5.2); newline final em `src/poker/poker.c` | Decisões do Gabriel sobre custos |
| **Rafael** | Cartas e IA (`src/cards`, `src/gameplay/poker_ai*`) | Baralho, embaralhamento, distribuição e board com 3 arquivos de teste (PR #2, já na `develop`); motor de decisão da IA com os 3 perfis e testes, em `src/gameplay/poker_ai*` (PR #7, já na `develop`); ponte entre a `Mao` e a IA com teste, em `src/core/mao_ia.*` (PR #10, já na `develop`) | Operações novas do baralho para as trapaças (5.3); ajuste fino da IA jogando | Decisões do Gabriel sobre trapaças |
| **Gabriel** | Mecânicas (`src/gameplay/trapacas.*`, `probabilidade.*`, `progressao.*`) | Lista de mecânicas; `trapacas.*` e `probabilidade.*` com as cinco trapaças, a chance de descoberta e a roleta (PRs #16 e #22, já na `develop`) | Balancear chances, custos e balas jogando; `progressao.*` | Interface das trapaças do Arthur |
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
