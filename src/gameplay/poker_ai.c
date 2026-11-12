#include "poker_ai.h"
#include "poker_ai_config.h"

#include <string.h>
#include <math.h>
#include <limits.h>

/* =======================================================================
 * Helpers de carta (iguais ao rascunho original).
 * ======================================================================= */

static bool valid_card(PokerCard card)
{
    return card.rank >= 2 && card.rank <= 14 &&
           card.suit >= POKER_CLUBS && card.suit <= POKER_SPADES;
}

static bool same_card(PokerCard a, PokerCard b)
{
    return a.rank == b.rank && a.suit == b.suit;
}

static bool valid_hole(const PokerCard hole[2])
{
    return hole && valid_card(hole[0]) && valid_card(hole[1]) &&
           !same_card(hole[0], hole[1]);
}

/* Confere que um conjunto de cartas (hole + board) nao tem repetidas e
 * que todas sao fisicamente validas. Usado por poker_ai_evaluate_draws
 * e poker_ai_estimate_equity. */
static PokerStatus validate_cards(const PokerCard *cards, size_t count)
{
    /* Primeiro cartas invalidas, depois duplicadas: erros distintos. */
    for (size_t i = 0; i < count; ++i)
        if (!valid_card(cards[i]))
            return POKER_ERR_CARD;
    for (size_t i = 0; i < count; ++i)
        for (size_t j = i + 1; j < count; ++j)
            if (same_card(cards[i], cards[j]))
                return POKER_ERR_DUPLICATE_CARD;
    return POKER_OK;
}

/* =======================================================================
 * RNG proprio da IA -- xorshift64*. Determinístico, rapido, nunca chama
 * rand()/srand(). O estado e so um uint64_t; seed 0 e proibida no
 * algoritmo (fica presa em 0), por isso remapeamos para uma constante.
 * ======================================================================= */

void poker_ai_rng_seed(PokerAIRng *rng, uint64_t seed)
{
    if (!rng)
        return;
    rng->state = seed ? seed : 0x9E3779B97F4A7C15ULL;
}

static uint64_t rng_next_u64(PokerAIRng *rng)
{
    /* xorshift64* (Vigna). Periodo 2^64-1, passa testes estatisticos
     * basicos o suficiente para sorteio de acoes/tamanhos de aposta. */
    uint64_t x = rng->state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng->state = x;
    return x * 0x2545F4914F6CDD1DULL;
}

double poker_ai_rng_uniform(PokerAIRng *rng)
{
    if (!rng || rng->state == 0)
        return NAN; /* O chamador precisa inicializar com poker_ai_rng_seed. */
    /* Usa os 53 bits mais significativos para um double uniforme em
     * [0, 1), igual a tecnica padrao de geradores de ponto flutuante. */
    uint64_t bits = rng_next_u64(rng) >> 11;
    return (double)bits * (1.0 / 9007199254740992.0); /* / 2^53 */
}

/* =======================================================================
 * Forca da mao pre-flop (heuristica de range -- sem equidade calculada,
 * porque ainda nao existe board).
 * ======================================================================= */

PokerStatus poker_ai_evaluate_preflop(
    const PokerCard hole[2],
    PokerAIHandStrength *out
)
{
    if (!hole || !out)
        return POKER_ERR_ARGUMENT;
    if (!valid_hole(hole))
        return POKER_ERR_CARD;

    int high = hole[0].rank > hole[1].rank ? hole[0].rank : hole[1].rank;
    int low = hole[0].rank < hole[1].rank ? hole[0].rank : hole[1].rank;
    bool suited = hole[0].suit == hole[1].suit;
    bool pair = high == low;

    /* Ranges iniciais heuristicos para heads-up; mesmos cortes do
     * rascunho original (ainda nao sao resultado de simulacao propria
     * deste projeto -- ajustar por testes/observacao de jogo real e um
     * trabalho futuro, nao coberto aqui). */
    if ((pair && high >= 11) || (high == 14 && low == 13)) {
        *out = POKER_AI_HAND_PREMIUM;             /* JJ+ e AK */
    } else if ((pair && high >= 7) ||
               (high == 14 && low == 12) ||      /* AQ */
               (high == 14 && low == 11 && suited) ||
               (high == 13 && low == 12 && suited)) {
        *out = POKER_AI_HAND_STRONG;
    } else if (pair ||
               (high == 14 && (suited || low >= 10)) ||
               (high == 13 && low >= 11) ||
               (high == 12 && low == 11) ||
               (suited && high >= 9 && high - low == 1)) {
        *out = POKER_AI_HAND_MEDIUM;
    } else {
        *out = POKER_AI_HAND_WEAK;
    }

    return POKER_OK;
}

/* =======================================================================
 * Draws no flop/turn -- META-DADO informativo (ver poker_ai.h). A
 * decisao em si nao usa isto como entrada numerica: equidade de Monte
 * Carlo ja reflete draws corretamente, porque simula as cartas que
 * faltam.
 *
 * CORRECAO em relacao ao rascunho original: um flush draw (e o backdoor
 * de flush) agora EXIGE que pelo menos uma hole card seja do naipe que
 * esta "quase fechando". No rascunho antigo, quatro cartas do mesmo
 * naipe caidas inteiramente no board (ex.: board com 4 copas e as hole
 * cards de outro naipe) eram contadas como vantagem da IA -- mas essa
 * vantagem e visivel para os dois jogadores igualmente, entao nao e
 * "nossa". A mesma logica NAO se aplica a straight draws: uma sequencia
 * que falta uma carta nao tem "naipe do jogador" para checar, e o
 * requisito do projeto so pede essa checagem explicitamente para flush.
 * ======================================================================= */

PokerStatus poker_ai_evaluate_draws(
    const PokerCard hole[2],
    const PokerCard *board,
    size_t board_count,
    PokerAIDrawInfo *out
)
{
    if (!hole || !board || !out || board_count < 3 || board_count > 5)
        return POKER_ERR_ARGUMENT;
    if (!valid_hole(hole))
        return POKER_ERR_CARD;

    PokerCard cards[7];
    cards[0] = hole[0];
    cards[1] = hole[1];
    for (size_t i = 0; i < board_count; ++i)
        cards[i + 2] = board[i];

    size_t count = board_count + 2;
    PokerStatus card_status = validate_cards(cards, count);
    if (card_status != POKER_OK)
        return card_status;

    PokerAIDrawInfo result = {0};
    if (board_count == 5) {
        *out = result; /* river: nao ha mais compra futura */
        return POKER_OK;
    }

    unsigned suit_count[4] = {0};
    bool ranks[15] = {false};
    for (size_t i = 0; i < count; ++i) {
        ++suit_count[cards[i].suit];
        ranks[cards[i].rank] = true;
    }
    ranks[1] = ranks[14]; /* As tambem pode ser baixo na sequencia A-2-3-4-5. */

    bool made_flush = false;
    for (int suit = POKER_CLUBS; suit <= POKER_SPADES; ++suit) {
        bool hole_has_suit = hole[0].suit == (PokerSuit)suit ||
                              hole[1].suit == (PokerSuit)suit;
        if (suit_count[suit] >= 5) {
            made_flush = true;
        } else if (suit_count[suit] == 4 && hole_has_suit) {
            /* Draw PESSOAL: exige hole card do naipe (correcao). */
            result.flush_draw = true;
        } else if (board_count == 3 && suit_count[suit] == 3 && hole_has_suit) {
            result.backdoor_flush = true;
        }
    }
    if (made_flush) {
        result.flush_draw = false;
        result.backdoor_flush = false;
    }

    bool missing_ranks[15] = {false};
    bool made_straight = false;
    /* Janela com 5 ranks: 1..5 (wheel) ate 10..14 (broadway). */
    for (int start = 1; start <= 10; ++start) {
        int present = 0;
        int missing = 0;
        for (int rank = start; rank < start + 5; ++rank) {
            if (ranks[rank])
                ++present;
            else
                missing = (rank == 1) ? 14 : rank;
        }
        if (present == 5)
            made_straight = true;
        else if (present == 4)
            missing_ranks[missing] = true;
    }

    if (!made_straight) {
        int straight_completion_ranks = 0;
        for (int rank = 2; rank <= 14; ++rank)
            straight_completion_ranks += missing_ranks[rank] ? 1 : 0;
        result.open_ended = straight_completion_ranks >= 2;
        result.gutshot = straight_completion_ranks == 1;
    }

    /* Outs NOMINAIS: alguns podem estar sujos ou coincidir entre draws.
     * Nao sao probabilidade de ganhar (ver poker_ai.h). */
    if (result.flush_draw && result.open_ended) {
        result.strength = POKER_AI_DRAW_COMBO;
        result.nominal_outs = 15;
    } else if (result.flush_draw) {
        result.strength = POKER_AI_DRAW_FLUSH;
        result.nominal_outs = result.gutshot ? 12 : 9;
    } else if (result.open_ended) {
        result.strength = POKER_AI_DRAW_OPEN_ENDED;
        result.nominal_outs = 8;
    } else if (result.gutshot) {
        result.strength = POKER_AI_DRAW_GUTSHOT;
        result.nominal_outs = 4;
    } else if (result.backdoor_flush) {
        result.strength = POKER_AI_DRAW_BACKDOOR;
        result.nominal_outs = 0; /* Requer DUAS cartas; nao equivale a 4 outs. */
    }

    *out = result;
    return POKER_OK;
}

/* =======================================================================
 * Pot odds "classicas" (call_cost / (pote + call_cost)). Mantida por
 * compatibilidade/depuracao. A classificacao de pressao usada na
 * decisao e poker_ai_classify_pressure, que usa pote-ANTES, nao isto.
 * ======================================================================= */

PokerStatus poker_ai_pot_odds(
    const PokerTable *table,
    int seat,
    double *out
)
{
    if (!table || !out)
        return POKER_ERR_ARGUMENT;

    PokerLegalActions legal;
    PokerStatus status = poker_legal_actions(table, seat, &legal);
    if (status != POKER_OK)
        return status;

    /* So o pote DISPUTAVEL conta: excesso que nao conseguimos cobrir
     * volta para o adversario em poker_settle(). No heads-up o pote
     * disputavel depois do call e 2 * min(committed apos call). */
    double cost = (double)legal.call_cost;
    double mine = (double)table->player[seat].committed + cost;
    double theirs = (double)table->player[1 - seat].committed;
    double contested = 2.0 * (mine < theirs ? mine : theirs);
    *out = cost > 0.0 && contested > 0.0 ? cost / contested : 0.0;
    return POKER_OK;
}

/* =======================================================================
 * Classificacao de pressao (poker_ai_config.h).
 *
 * pote_antes = poker_pot(table) - legal.to_call
 * razao      = legal.to_call / pote_antes     (se to_call > 0)
 *
 * poker_pot(table) ja inclui o quanto o adversario colocou nesta rodada
 * de apostas, mesmo a parte que ainda nao respondemos -- por isso
 * SUBTRAIMOS legal.to_call para reconstruir o pote como ele estava
 * ANTES dessa aposta ser feita. Exemplo (igual ao do pedido): um
 * adversario aposta 10 BB num pote que tinha 100 BB antes da aposta ->
 * razao = 10/100 = 0.10 (pequena). O mesmo valor absoluto de 10 BB num
 * pote de 10 BB antes da aposta -> razao = 10/10 = 1.00 (bem mais
 * pressao) -- os dois casos NAO podem cair na mesma categoria, e e
 * exatamente isso que esta formula garante.
 *
 * Usamos legal.to_call (o valor TEORICO pedido), nao legal.call_cost (o
 * valor capado pelo stack em all-in). Isso e proposital: a PRESSAO que
 * o adversario esta exercendo e sobre o pote, independente de quanto
 * do nosso stack sobra para pagar; e a situacao de all-in acaba
 * aparecendo nos numeros de qualquer forma, porque um stack curto tende
 * a gerar to_call grande relativo ao pote-antes (classificando como
 * LARGE/OVERBET), que e o comportamento desejado.
 * ======================================================================= */

PokerAIPressure poker_ai_classify_pressure(
    const PokerLegalActions *legal,
    PokerChips pot_now
)
{
    if (!legal || legal->to_call <= 0)
        return POKER_AI_PRESSURE_FREE;

    PokerChips pot_before = pot_now - legal->to_call;
    double ratio;
    if (pot_before <= 0) {
        /* Protecao de pote zero/negativo: no motor atual isso so
         * aconteceria em estados degenerados (pote-antes nao deveria
         * ser <= 0 quando ha algo para pagar), mas tratamos como a
         * maior pressao possivel em vez de dividir por zero. */
        ratio = poker_ai_pressure_thresholds.large_max + 1.0;
    } else {
        ratio = (double)legal->to_call / (double)pot_before;
    }

    if (ratio <= poker_ai_pressure_thresholds.small_max)
        return POKER_AI_PRESSURE_SMALL;
    if (ratio <= poker_ai_pressure_thresholds.medium_max)
        return POKER_AI_PRESSURE_MEDIUM;
    if (ratio <= poker_ai_pressure_thresholds.large_max)
        return POKER_AI_PRESSURE_LARGE;
    return POKER_AI_PRESSURE_OVERBET;
}

/* Tabela escolhida explicitamente pelo controlador, sem estado global. */
static const PokerAIScenario *lookup_scenario(
    const PokerAIScenarioTable *table, PokerAIStyle style, PokerStreet street,
    PokerAIHandStrength strength, PokerAIPressure pressure)
{
    return &(*table)[style][street][strength][pressure];
}

/* =======================================================================
 * Monte Carlo: equidade contra uma mao oculta aleatoria.
 *
 * IMPORTANTE: equity contra mao aleatoria != equity contra o range real
 * do adversario (ver poker_ai.h). Isto e uma aproximacao deliberada.
 * ======================================================================= */

static int build_unseen_deck(const PokerCard *known, size_t known_count,
                              PokerCard out[52])
{
    int n = 0;
    for (int suit = POKER_CLUBS; suit <= POKER_SPADES; ++suit) {
        for (int rank = 2; rank <= 14; ++rank) {
            PokerCard candidate = { rank, (PokerSuit)suit };
            bool seen = false;
            for (size_t i = 0; i < known_count; ++i) {
                if (same_card(candidate, known[i])) {
                    seen = true;
                    break;
                }
            }
            if (!seen)
                out[n++] = candidate;
        }
    }
    return n;
}

/* Sorteia (sem reposicao) os primeiros `k` elementos de `deck[0..n)`,
 * embaralhando so o necessario (Fisher-Yates parcial). */
static void partial_shuffle(PokerCard *deck, int n, int k, PokerAIRng *rng)
{
    for (int i = 0; i < k; ++i) {
        int remaining = n - i;
        int j = i + (int)(poker_ai_rng_uniform(rng) * (double)remaining);
        if (j >= n) /* protege contra arredondamento de ponto flutuante */
            j = n - 1;
        PokerCard tmp = deck[i];
        deck[i] = deck[j];
        deck[j] = tmp;
    }
}

PokerStatus poker_ai_estimate_equity(
    const PokerCard hole[2],
    const PokerCard *board,
    size_t board_count,
    PokerAIRng *rng,
    int iterations,
    double *out_equity
)
{
    if (!hole || !rng || rng->state == 0 || !out_equity || iterations <= 0)
        return POKER_ERR_ARGUMENT;
    if (board_count > 5 || (board_count > 0 && !board))
        return POKER_ERR_ARGUMENT;
    if (!valid_hole(hole))
        return POKER_ERR_CARD;

    PokerCard known[7];
    known[0] = hole[0];
    known[1] = hole[1];
    for (size_t i = 0; i < board_count; ++i)
        known[i + 2] = board[i];
    size_t known_count = board_count + 2;
    PokerStatus card_status = validate_cards(known, known_count);
    if (card_status != POKER_OK)
        return card_status;

    PokerCard unseen[52];
    int unseen_count = build_unseen_deck(known, known_count, unseen);

    int missing_board = (int)(5 - board_count);
    int needed = 2 + missing_board; /* mao do oponente + resto do board */
    if (needed > unseen_count)
        return POKER_ERR_ARGUMENT;

    long wins = 0, ties = 0;
    PokerCard deck_copy[52];
    PokerCard full_board[5];
    for (size_t i = 0; i < board_count; ++i)
        full_board[i] = board[i];

    for (int iter = 0; iter < iterations; ++iter) {
        memcpy(deck_copy, unseen, (size_t)unseen_count * sizeof(PokerCard));
        partial_shuffle(deck_copy, unseen_count, needed, rng);

        PokerCard opponent_hole[2] = { deck_copy[0], deck_copy[1] };
        for (int i = 0; i < missing_board; ++i)
            full_board[board_count + (size_t)i] = deck_copy[2 + i];

        PokerHandValue our_value, opponent_value;
        PokerStatus status = poker_evaluate_holdem(hole, full_board, 5, &our_value);
        if (status != POKER_OK)
            return status;
        status = poker_evaluate_holdem(opponent_hole, full_board, 5, &opponent_value);
        if (status != POKER_OK)
            return status;

        int cmp = poker_compare(&our_value, &opponent_value);
        if (cmp > 0)
            ++wins;
        else if (cmp == 0)
            ++ties;
    }

    *out_equity = ((double)wins + 0.5 * (double)ties) / (double)iterations;
    return POKER_OK;
}

static PokerAIHandStrength classify_by_equity(double equity)
{
    if (equity <= poker_ai_equity_thresholds.weak_max)
        return POKER_AI_HAND_WEAK;
    if (equity <= poker_ai_equity_thresholds.medium_max)
        return POKER_AI_HAND_MEDIUM;
    if (equity <= poker_ai_equity_thresholds.strong_max)
        return POKER_AI_HAND_STRONG;
    return POKER_AI_HAND_PREMIUM;
}

/* =======================================================================
 * Sorteio ponderado de acao.
 *
 * Recebe os 4 pesos JA filtrados pela legalidade (pesos de acoes
 * ilegais chegam aqui como 0.0 -- ver build_effective_weights). Sortear
 * com pesos ja zerados automaticamente "renormaliza": a soma usada como
 * denominador e so dos pesos que sobraram, entao as acoes legais
 * dividem 100% do espaco de probabilidade entre si.
 * ======================================================================= */

static PokerAIActionSlot weighted_pick(const double weight[POKER_AI_ACTION_SLOT_COUNT],
                                        PokerAIRng *rng)
{
    double total = weight[0] + weight[1] + weight[2] + weight[3];
    double roll = poker_ai_rng_uniform(rng) * total;
    double acc = 0.0;
    for (int i = 0; i < POKER_AI_ACTION_SLOT_COUNT; ++i) {
        acc += weight[i];
        if (roll < acc)
            return (PokerAIActionSlot)i;
    }
    /* So chega aqui por arredondamento de ponto flutuante no limite
     * superior; devolve o ultimo slot com peso > 0. */
    for (int i = POKER_AI_ACTION_SLOT_COUNT - 1; i >= 0; --i)
        if (weight[i] > 0.0)
            return (PokerAIActionSlot)i;
    return POKER_AI_ACTION_FOLD;
}

/*
 * Filtra os 4 pesos configurados pela legalidade real da mesa. Em
 * cenarios FREE, a propria tabela ja traz FOLD=0. No short stack,
 * um override explicito posterior pode transferir fold para call. Isso NAO e
 * logica de "estrategia" -- e so a definicao do que "legal" significa
 * aqui: foldar de graca e estritamente dominado por check, e permitir
 * isso so adicionaria ruido sem nenhum efeito jogavel interessante.
 *
 * Se, depois do filtro, todos os pesos legais forem 0 (configuracao mal
 * feita: o usuario zerou todas as acoes permitidas), cai num fallback
 * de seguranca documentado: prefere a acao passiva, depois fold, depois
 * raise -- nesta ordem, so para garantir que SEMPRE devolvemos uma
 * acao legal. Isto e uma rede de seguranca para configs invalidas, nao
 * uma "estrategia escondida": com qualquer tabela minimamente sensata
 * (pelo menos um peso > 0 numa acao legal) ele nunca entra em jogo.
 */
static void build_effective_weights(
    const PokerAIScenario *scenario,
    const PokerLegalActions *legal,
    bool current_bet_zero,
    bool offensive_all_in_legal,
    double effective[POKER_AI_ACTION_SLOT_COUNT],
    bool legal_slot[POKER_AI_ACTION_SLOT_COUNT])
{
    bool can_raise = current_bet_zero ? legal->bet : legal->raise;

    legal_slot[POKER_AI_ACTION_FOLD] = legal->fold && !legal->check;
    legal_slot[POKER_AI_ACTION_PASSIVE] = legal->check || legal->call;
    legal_slot[POKER_AI_ACTION_RAISE] = can_raise;
    /* Permite all-in ofensivo curto mesmo sem raise minimo convencional. */
    legal_slot[POKER_AI_ACTION_RAISE_BIG] = can_raise || offensive_all_in_legal;

    for (int i = 0; i < POKER_AI_ACTION_SLOT_COUNT; ++i)
        effective[i] = legal_slot[i] ? scenario->weight[i] : 0.0;

    double total = effective[0] + effective[1] + effective[2] + effective[3];
    if (total <= 0.0) {
        if (legal_slot[POKER_AI_ACTION_PASSIVE])
            effective[POKER_AI_ACTION_PASSIVE] = 1.0;
        else if (legal_slot[POKER_AI_ACTION_FOLD])
            effective[POKER_AI_ACTION_FOLD] = 1.0;
        else if (legal_slot[POKER_AI_ACTION_RAISE])
            effective[POKER_AI_ACTION_RAISE] = 1.0;
        /* Se nem isso for legal, a mesa esta num estado que
         * poker_legal_actions ja deveria ter rejeitado antes de chegar
         * aqui; devolvemos todos os pesos em 0 e quem chamou trata o
         * erro (poker_ai_choose_action devolve POKER_ERR_ACTION). */
    }
}

/* =======================================================================
 * Tamanho de aposta/raise.
 *
 * BET (abertura, current_bet == 0): fracao sorteada em
 * poker_ai_bet_ranges aplicada sobre poker_pot(table).
 *
 * RAISE (current_bet > 0): "valor extra acima do call como percentual
 * do pote depois do call", exatamente como pedido. O pote depois do
 * call e poker_pot(table) + legal->to_call; o total final e
 * current_bet + extra (porque, apos pagar o call, o street_bet do
 * jogador fica igual a current_bet).
 *
 * Em ambos os casos o resultado e travado em [legal->min_total,
 * legal->max_total] antes de devolver -- nunca gera um PokerAction
 * ilegal, mesmo que a fracao sorteada seja extrema ou o stack seja
 * curto (all-in).
 * ======================================================================= */

static PokerChips clamp_chips(PokerChips value, PokerChips lo, PokerChips hi)
{
    if (value < lo)
        return lo;
    if (value > hi)
        return hi;
    return value;
}

static PokerChips choose_bet_total(
    PokerAIStyle style,
    PokerAIBetKind kind,
    bool opening_bet,
    const PokerTable *table,
    const PokerLegalActions *legal,
    PokerAIRng *rng)
{
    const PokerAIBetRange *range = &poker_ai_bet_ranges[style][kind];
    double fraction = range->min_fraction +
        poker_ai_rng_uniform(rng) * (range->max_fraction - range->min_fraction);

    /* Nunca converter um real fora do alcance de int64_t diretamente. */
    long double desired;
    if (opening_bet) {
        desired = (long double)poker_pot(table) * (long double)fraction;
    } else {
        long double pot_after_call = (long double)poker_pot(table) +
                                     (long double)legal->to_call;
        desired = (long double)table->current_bet +
                  pot_after_call * (long double)fraction;
    }
    if (desired <= (long double)legal->min_total)
        return legal->min_total;
    if (desired >= (long double)legal->max_total)
        return legal->max_total;
    return clamp_chips((PokerChips)desired, legal->min_total, legal->max_total);
}

/* =======================================================================
 * Decisao principal.
 * ======================================================================= */

static void fill_info(
    PokerAIDecisionInfo *info,
    PokerAIStyle style, PokerStreet street, PokerAIHandStrength strength,
    PokerAIPressure pressure, double pressure_ratio,
    double nominal_pressure_ratio, double effective_pot_odds,
    bool cheap_all_in_override, double equity,
    double spr, bool capped_all_in_call,
    const double configured[POKER_AI_ACTION_SLOT_COUNT],
    const double effective[POKER_AI_ACTION_SLOT_COUNT],
    PokerAIActionSlot chosen)
{
    if (!info)
        return;
    info->profile = style;
    info->street = street;
    info->strength = strength;
    info->pressure = (int)pressure;
    info->pressure_ratio = pressure_ratio;
    info->nominal_pressure_ratio = nominal_pressure_ratio;
    info->effective_pot_odds = effective_pot_odds;
    info->cheap_all_in_override = cheap_all_in_override;
    info->equity = equity;
    info->spr = spr;
    info->capped_all_in_call = capped_all_in_call;
    memcpy(info->configured_weight, configured, sizeof info->configured_weight);
    memcpy(info->effective_weight, effective, sizeof info->effective_weight);
    info->chosen_slot = chosen;
}

PokerStatus poker_ai_choose_action_mode(
    const PokerTable *table,
    int seat,
    const PokerCard hole[2],
    const PokerCard *board,
    size_t board_count,
    const PokerAIContext *context,
    bool intoxicated,
    PokerAIDecisionInfo *info_out,
    PokerAction *out
)
{
    if (!table || !hole || !context || !out || seat < 0 || seat > 1 ||
        table->street < POKER_PREFLOP || table->street > POKER_RIVER ||
        context->style < POKER_AI_CAUTIOUS ||
        context->style > POKER_AI_AGGRESSIVE ||
        !context->action_rng || context->action_rng->state == 0 ||
        (table->street != POKER_PREFLOP &&
         (!context->equity_rng || context->equity_rng->state == 0 ||
          context->equity_rng == context->action_rng)))
        return POKER_ERR_ARGUMENT;

    PokerAIStyle style = context->style;
    PokerAIRng *action_rng = context->action_rng;
    const PokerAIScenarioTable *active_table = context->table
        ? context->table : &poker_ai_scenario_table;
    if (!isfinite(poker_ai_pressure_thresholds.small_max) ||
        !isfinite(poker_ai_pressure_thresholds.medium_max) ||
        !isfinite(poker_ai_pressure_thresholds.large_max) ||
        poker_ai_pressure_thresholds.small_max <= 0.0 ||
        poker_ai_pressure_thresholds.small_max >= poker_ai_pressure_thresholds.medium_max ||
        poker_ai_pressure_thresholds.medium_max >= poker_ai_pressure_thresholds.large_max ||
        !isfinite(poker_ai_cheap_all_in_odds_max) ||
        poker_ai_cheap_all_in_odds_max < 0.0 ||
        poker_ai_cheap_all_in_odds_max > 1.0 ||
        !isfinite(poker_ai_offensive_all_in_spr_max) ||
        poker_ai_offensive_all_in_spr_max < 0.0 ||
        poker_ai_monte_carlo_iterations <= 0)
        return POKER_ERR_ARGUMENT;
    for (int kind = 0; kind < POKER_AI_BET_KIND_COUNT; ++kind) {
        const PokerAIBetRange *range = &poker_ai_bet_ranges[style][kind];
        if (!isfinite(range->min_fraction) || !isfinite(range->max_fraction) ||
            range->min_fraction < 0.0 || range->min_fraction > range->max_fraction)
            return POKER_ERR_ARGUMENT;
    }
    if (!valid_hole(hole))
        return POKER_ERR_CARD;

    PokerLegalActions legal;
    PokerStatus status = poker_legal_actions(table, seat, &legal);
    if (status != POKER_OK)
        return status;

    PokerStreet street = table->street;
    PokerAIHandStrength strength;
    double equity = -1.0;

    if (street == POKER_PREFLOP) {
        if (board_count != 0)
            return POKER_ERR_ARGUMENT;
        status = poker_ai_evaluate_preflop(hole, &strength);
        if (status != POKER_OK)
            return status;
    } else {
        /* flop=3, turn=4, river=5 cartas de board -- conferimos que bate
         * com a street informada pela mesa, para nao calcular equidade
         * com um board inconsistente. */
        size_t expected = street == POKER_FLOP ? 3 : street == POKER_TURN ? 4 : 5;
        if (board_count != expected)
            return POKER_ERR_ARGUMENT;

        status = poker_ai_estimate_equity(hole, board, board_count, context->equity_rng,
                                           poker_ai_monte_carlo_iterations,
                                           &equity);
        if (status != POKER_OK)
            return status;
        strength = classify_by_equity(equity);
    }

    PokerChips pot_now = poker_pot(table);
    PokerAIPressure nominal_pressure = poker_ai_classify_pressure(&legal, pot_now);
    PokerAIPressure pressure = nominal_pressure;

    PokerChips pot_before = pot_now - legal.to_call;
    double nominal_pressure_ratio = legal.to_call <= 0 ? -1.0 :
        pot_before > 0 ? (double)legal.to_call / (double)pot_before :
        poker_ai_pressure_thresholds.large_max + 1.0;
    double pressure_ratio = nominal_pressure_ratio;

    double pot_odds = 0.0;
    status = poker_ai_pot_odds(table, seat, &pot_odds);
    if (status != POKER_OK)
        return status;

    bool capped_all_in_call = legal.call && legal.call_cost < legal.to_call;
    /* Ao receber um all-in capado, a aposta nominal nao representa o
     * quanto temos de arriscar. Usa custo REAL / pote contestado antes
     * de completar nosso call, para escolher a celula SMALL/MEDIUM/etc.
     * Mantemos nominal_pressure_ratio no debug para registrar a pressao. */
    if (capped_all_in_call) {
        double contested_before = 2.0 * (double)table->player[seat].committed;
        if (contested_before > 0.0) {
            pressure_ratio = (double)legal.call_cost / contested_before;
        } else {
            pressure_ratio = poker_ai_pressure_thresholds.large_max + 1.0;
        }
        PokerLegalActions effective_legal = legal;
        effective_legal.to_call = legal.call_cost;
        PokerChips base = table->player[seat].committed;
        /* Evita overflow; o classificatorio recebe apenas uma razao. */
        PokerChips effective_pot = base <= (INT64_MAX - legal.call_cost) / 2
            ? 2 * base + legal.call_cost : INT64_MAX;
        pressure = poker_ai_classify_pressure(&effective_legal, effective_pot);
    }

    double spr = pot_before > 0 ?
        (double)table->player[seat].chips / (double)pot_before : -1.0;

    const PokerAIScenario *scenario =
        lookup_scenario(active_table, style, street, strength, pressure);

    /* Configuracao invalida nao deve alterar uma jogada silenciosamente. */
    double config_sum = 0.0;
    for (int i = 0; i < POKER_AI_ACTION_SLOT_COUNT; ++i) {
        if (!isfinite(scenario->weight[i]) || scenario->weight[i] < 0.0)
            return POKER_ERR_ARGUMENT;
        config_sum += scenario->weight[i];
    }
    if (!isfinite(config_sum) || config_sum <= 0.0 ||
        (pressure == POKER_AI_PRESSURE_FREE &&
         scenario->weight[POKER_AI_ACTION_FOLD] != 0.0))
        return POKER_ERR_ARGUMENT;

    bool current_bet_zero = table->current_bet == 0;
    bool conventional_raise_legal = current_bet_zero ? legal.bet : legal.raise;
    /* Call all-in defensivo nao supera o valor da aposta atual. */
    bool offensive_all_in_legal = legal.all_in && legal.max_total > table->current_bet;
    double effective[POKER_AI_ACTION_SLOT_COUNT];
    bool legal_slot[POKER_AI_ACTION_SLOT_COUNT];
    build_effective_weights(scenario, &legal, current_bet_zero,
                            offensive_all_in_legal, effective, legal_slot);

    bool cheap_all_in_override = false;
    if (capped_all_in_call && legal.call &&
        poker_ai_cheap_all_in_odds_max > 0.0 &&
        pot_odds <= poker_ai_cheap_all_in_odds_max &&
        (street == POKER_PREFLOP || equity >= pot_odds)) {
        /* Excecao configuravel e auditavel: nao foldar quando e muito
         * barato disputar o pote possivel. Nao altera a tabela base. */
        effective[POKER_AI_ACTION_PASSIVE] += effective[POKER_AI_ACTION_FOLD];
        effective[POKER_AI_ACTION_FOLD] = 0.0;
        cheap_all_in_override = true;
    }

    /* Embriaguez V1: modifica pesos EFETIVOS, nunca a tabela-base.
     * CAUTIOUS e BALANCED: pagam mais calls que deveriam abandonar.
     * AGGRESSIVE: converte parte dos folds em apostas/raises.
     * Um check gratuito jamais e convertido em fold. */
    if (intoxicated) {
        if ((style == POKER_AI_CAUTIOUS || style == POKER_AI_BALANCED) &&
            legal.call && legal_slot[POKER_AI_ACTION_PASSIVE]) {
            double shift = effective[POKER_AI_ACTION_FOLD] *
                (style == POKER_AI_CAUTIOUS ? 0.40 : 0.35);
            effective[POKER_AI_ACTION_FOLD] -= shift;
            effective[POKER_AI_ACTION_PASSIVE] += shift;
        } else if (style == POKER_AI_AGGRESSIVE) {
            double shift = effective[POKER_AI_ACTION_FOLD] * 0.30;
            effective[POKER_AI_ACTION_FOLD] -= shift;
            if (legal_slot[POKER_AI_ACTION_RAISE_BIG])
                effective[POKER_AI_ACTION_RAISE_BIG] += shift;
            else if (legal_slot[POKER_AI_ACTION_RAISE])
                effective[POKER_AI_ACTION_RAISE] += shift;
            else
                effective[POKER_AI_ACTION_FOLD] += shift;
        }
    }

    double total_effective = effective[0] + effective[1] + effective[2] + effective[3];
    if (total_effective <= 0.0)
        return POKER_ERR_ACTION;

    PokerAIActionSlot chosen = weighted_pick(effective, action_rng);
    /* 12% por decisao elegivel, independentemente da forca da mao.
     * Usa o RNG da IA e exige um all-in OFENSIVO legal. */
    bool impulsive_shove = intoxicated && style == POKER_AI_AGGRESSIVE &&
        offensive_all_in_legal &&
        poker_ai_rng_uniform(action_rng) < 0.12;
    if (impulsive_shove)
        chosen = POKER_AI_ACTION_RAISE_BIG;

    PokerAction action;
    switch (chosen) {
    case POKER_AI_ACTION_FOLD:
        action.type = POKER_FOLD;
        action.total = 0;
        break;
    case POKER_AI_ACTION_PASSIVE:
        if (legal.check) {
            action.type = POKER_CHECK;
            action.total = 0;
        } else {
            action.type = POKER_CALL;
            action.total = 0;
        }
        break;
    case POKER_AI_ACTION_RAISE:
        action.type = current_bet_zero ? POKER_BET : POKER_RAISE;
        action.total = choose_bet_total(style, POKER_AI_BET_NORMAL, current_bet_zero,
                                         table, &legal, action_rng);
        /* Agressivo bebado alterna tamanhos pequenos e grandes. */
        if (intoxicated && style == POKER_AI_AGGRESSIVE &&
            poker_ai_rng_uniform(action_rng) < 0.50)
            action.total = choose_bet_total(style, POKER_AI_BET_BIG,
                                             current_bet_zero, table, &legal, action_rng);
        break;
    case POKER_AI_ACTION_RAISE_BIG:
    default:
        /* Os pesos de BIG continuam por personalidade. Com raise curto
         * permitido, BIG pode virar all-in; com SPR baixo, vira shove. */
        if (offensive_all_in_legal &&
            (impulsive_shove || !conventional_raise_legal ||
             (spr >= 0.0 && spr <= poker_ai_offensive_all_in_spr_max))) {
            action.type = POKER_ALL_IN;
            action.total = 0;
        } else {
            action.type = current_bet_zero ? POKER_BET : POKER_RAISE;
            PokerAIBetKind kind = POKER_AI_BET_BIG;
            if (intoxicated && style == POKER_AI_AGGRESSIVE &&
                poker_ai_rng_uniform(action_rng) < 0.25)
                kind = POKER_AI_BET_NORMAL;
            action.total = choose_bet_total(style, kind, current_bet_zero,
                                             table, &legal, action_rng);
        }
        break;
    }

    fill_info(info_out, style, street, strength, pressure, pressure_ratio,
              nominal_pressure_ratio, pot_odds, cheap_all_in_override,
              equity, spr, capped_all_in_call, scenario->weight, effective, chosen);

    *out = action;
    return POKER_OK;
}

/* A API antiga continua identica: todos os chamadores existentes seguem sobrios. */
PokerStatus poker_ai_choose_action(
    const PokerTable *table, int seat, const PokerCard hole[2],
    const PokerCard *board, size_t board_count,
    const PokerAIContext *context, PokerAIDecisionInfo *info_out,
    PokerAction *out)
{
    return poker_ai_choose_action_mode(table, seat, hole, board, board_count,
                                       context, false, info_out, out);
}


/* Multiplicador isolado para o futuro sistema de trapaça.
 * Nao sorteia deteccao nem aplica penalidade aqui. */
double poker_ai_detection_chance(PokerAIStyle style, bool intoxicated,
                                  double base_chance)
{
    if (!isfinite(base_chance) || base_chance < 0.0 || base_chance > 1.0)
        return NAN;
    if (intoxicated && style == POKER_AI_BALANCED)
        return base_chance * 0.55;
    return base_chance;
}
