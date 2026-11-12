#include "core/mao_ia.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void test_uma_mao(PokerAIStyle estilo, bool bebado, unsigned seed)
{
    Torneio torneio;
    Mao mao = {0};
    torneio_iniciar(&torneio);
    srand(seed);
    PokerAIRng acao, equidade;
    poker_ai_rng_seed(&acao, 50000u + seed);
    poker_ai_rng_seed(&equidade, 90000u + seed);
    PokerAIContext ia = {estilo, NULL, &acao, &equidade};
    assert(mao_iniciar(&mao, &torneio));
    PokerChips total = torneio.fichas_jogador + torneio.fichas_adversario;
    int acoes_ia = 0;
    for (int i = 0; i < 150 && mao.situacao == MAO_EM_ANDAMENTO; ++i) {
        PokerStatus st;
        if (mao.mesa.actor == TORNEIO_ASSENTO_ADVERSARIO) {
            PokerAIDecisionInfo info;
            st = mao_ia_agir(&mao, &ia, bebado, &info);
            assert(st == POKER_OK);
            assert(info.profile == estilo);
            ++acoes_ia;
        } else {
            PokerLegalActions legal;
            assert(poker_legal_actions(&mao.mesa,
                       TORNEIO_ASSENTO_JOGADOR, &legal) == POKER_OK);
            PokerAction human = {legal.check ? POKER_CHECK : POKER_CALL, 0};
            st = mao_agir(&mao, human);
            assert(st == POKER_OK);
        }
    }
    assert(mao.situacao == MAO_ENCERRADA);
    assert(acoes_ia > 0);
    assert(mao.mesa.player[0].chips + mao.mesa.player[1].chips == total);
    assert(mao_registrar_no_torneio(&mao, &torneio));
    assert(torneio.fichas_jogador + torneio.fichas_adversario == total);
    mao_liberar(&mao);
}

static void test_recusas(void)
{
    Torneio torneio;
    Mao mao = {0};
    PokerAIRng acao, eq;
    poker_ai_rng_seed(&acao, 1);
    poker_ai_rng_seed(&eq, 2);
    PokerAIContext ia = {POKER_AI_CAUTIOUS, NULL, &acao, &eq};
    torneio_iniciar(&torneio);
    srand(2);
    assert(mao_ia_agir(NULL, &ia, false, NULL) == POKER_ERR_ARGUMENT);
    assert(mao_ia_agir(&mao, NULL, false, NULL) == POKER_ERR_ARGUMENT);
    assert(mao_ia_agir(&mao, &ia, false, NULL) == POKER_ERR_STATE);
    assert(mao_iniciar(&mao, &torneio));
    assert(mao.mesa.actor == TORNEIO_ASSENTO_JOGADOR);
    assert(mao_ia_agir(&mao, &ia, false, NULL) == POKER_ERR_STATE);
    mao_liberar(&mao);
}

int main(void)
{
    test_recusas();
    for (int p = POKER_AI_CAUTIOUS; p <= POKER_AI_AGGRESSIVE; ++p)
        for (int bebado = 0; bebado <= 1; ++bebado)
            for (unsigned seed = 1; seed <= 20; ++seed)
                test_uma_mao((PokerAIStyle)p, bebado != 0, seed);
    puts("OK: 120 maos completas Mao + IA, sobria e embriagada.");
    return 0;
}
