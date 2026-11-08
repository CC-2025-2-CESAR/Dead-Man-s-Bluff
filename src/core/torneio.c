#include "core/torneio.h"

/** Fichas com que o adversário de cada confronto começa. */
static const PokerChips FICHAS_ADVERSARIO[TORNEIO_CONFRONTOS] = { 2000, 4000, 8000 };

/** Big blind do nível zero de cada confronto. */
static const PokerChips BIG_BLIND_INICIAL[TORNEIO_CONFRONTOS] = { 20, 40, 80 };

/**
 * Calcula o big blind de um confronto em um nível de blinds.
 *
 * @param confronto índice do confronto.
 * @param nivel quantas vezes o blind já foi multiplicado.
 * @return big blind do nível, limitado a TORNEIO_BLIND_MAXIMO.
 */
static PokerChips big_blind_do_nivel(int confronto, int nivel)
{
    PokerChips blind = BIG_BLIND_INICIAL[confronto];

    for (int i = 0; i < nivel && blind < TORNEIO_BLIND_MAXIMO; i++) {
        blind *= TORNEIO_FATOR_BLIND;
    }

    return blind < TORNEIO_BLIND_MAXIMO ? blind : TORNEIO_BLIND_MAXIMO;
}

void torneio_iniciar(Torneio *torneio)
{
    torneio->confronto = 0;
    torneio->fichas_jogador = TORNEIO_FICHAS_INICIAIS_JOGADOR;
    torneio->fichas_adversario = FICHAS_ADVERSARIO[0];
    torneio->maos_jogadas = 0;
    torneio->dealer = TORNEIO_ASSENTO_JOGADOR;
    torneio->recuperacoes = 0;
    torneio->situacao = TORNEIO_EM_ANDAMENTO;
}

PokerChips torneio_fichas_iniciais_adversario(int confronto)
{
    if (confronto < 0 || confronto >= TORNEIO_CONFRONTOS) {
        return 0;
    }

    return FICHAS_ADVERSARIO[confronto];
}

PokerChips torneio_big_blind(const Torneio *torneio)
{
    return big_blind_do_nivel(torneio->confronto, torneio->maos_jogadas / TORNEIO_MAOS_POR_NIVEL);
}

PokerChips torneio_small_blind(const Torneio *torneio)
{
    return torneio_big_blind(torneio) / 2;
}

void torneio_stacks(const Torneio *torneio, PokerChips stacks[2])
{
    stacks[TORNEIO_ASSENTO_JOGADOR] = torneio->fichas_jogador;
    stacks[TORNEIO_ASSENTO_ADVERSARIO] = torneio->fichas_adversario;
}

bool torneio_registrar_mao(
    Torneio *torneio,
    PokerChips fichas_jogador,
    PokerChips fichas_adversario
)
{
    PokerChips antes = torneio->fichas_jogador + torneio->fichas_adversario;

    if (torneio->situacao != TORNEIO_EM_ANDAMENTO) {
        return false;
    }

    if (fichas_jogador < 0 || fichas_adversario < 0) {
        return false;
    }

    if (fichas_jogador == 0 && fichas_adversario == 0) {
        return false;
    }

    if (fichas_jogador + fichas_adversario > antes) {
        return false;
    }

    torneio->fichas_jogador = fichas_jogador;
    torneio->fichas_adversario = fichas_adversario;
    torneio->maos_jogadas++;
    torneio->dealer = 1 - torneio->dealer;

    if (fichas_jogador == 0) {
        torneio->situacao = TORNEIO_JOGADOR_FALIDO;
    } else if (fichas_adversario == 0) {
        torneio->situacao = torneio->confronto == TORNEIO_CONFRONTOS - 1
            ? TORNEIO_CAMPEAO
            : TORNEIO_CONFRONTO_VENCIDO;
    }

    return true;
}

bool torneio_avancar_confronto(Torneio *torneio)
{
    if (torneio->situacao != TORNEIO_CONFRONTO_VENCIDO) {
        return false;
    }

    torneio->confronto++;
    torneio->fichas_adversario = FICHAS_ADVERSARIO[torneio->confronto];
    torneio->maos_jogadas = 0;
    torneio->dealer = TORNEIO_ASSENTO_JOGADOR;
    torneio->situacao = TORNEIO_EM_ANDAMENTO;

    return true;
}

bool torneio_recuperar_jogador(Torneio *torneio)
{
    if (torneio->situacao != TORNEIO_JOGADOR_FALIDO) {
        return false;
    }

    torneio->fichas_jogador = FICHAS_ADVERSARIO[torneio->confronto] / 2;
    torneio->recuperacoes++;
    torneio->situacao = TORNEIO_EM_ANDAMENTO;

    return true;
}
