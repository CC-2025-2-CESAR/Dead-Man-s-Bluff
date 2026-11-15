#include "cards/deck.h"
#include "cards/lista_cartas.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

#define SEMENTES 300
#define TAMANHO_MAXIMO 12

static PokerCard carta(int valor, PokerSuit naipe)
{
    return (PokerCard){valor, naipe};
}

static bool mesma_carta(PokerCard a, PokerCard b)
{
    return a.rank == b.rank && a.suit == b.suit;
}

static unsigned proximo_aleatorio(unsigned *semente)
{
    *semente = *semente * 1664525u + 1013904223u;
    return *semente >> 8;
}

static void conferir(const ListaCartas *lista, const PokerCard *esperado, size_t quantidade)
{
    PokerCard copia[TAMANHO_MAXIMO + 1];

    CHECK(quantidade <= TAMANHO_MAXIMO);
    CHECK(lista->quantidade == quantidade);
    CHECK(lista_cartas_copiar(lista, copia, TAMANHO_MAXIMO + 1) == quantidade);

    for (size_t i = 0; i < quantidade; i++)
        CHECK(mesma_carta(copia[i], esperado[i]));

    if (quantidade == 0) {
        CHECK(lista->inicio == NULL);
        CHECK(lista->fim == NULL);
    } else {
        CHECK(lista->inicio != NULL);
        CHECK(lista->fim != NULL);
    }
}

static PokerCard carta_ausente(const PokerCard *cartas, size_t quantidade)
{
    for (int valor = 2; valor <= 14; valor++) {
        for (int naipe = POKER_CLUBS; naipe <= POKER_SPADES; naipe++) {
            PokerCard candidata = carta(valor, (PokerSuit)naipe);
            bool presente = false;

            for (size_t i = 0; i < quantidade; i++)
                presente = presente || mesma_carta(cartas[i], candidata);

            if (!presente)
                return candidata;
        }
    }

    CHECK(false);
    return carta(2, POKER_CLUBS);
}

static void conferir_fim(ListaCartas *lista, const PokerCard *esperado, size_t quantidade)
{
    PokerCard nova = carta_ausente(esperado, quantidade);
    PokerCard depois[TAMANHO_MAXIMO];

    CHECK(quantidade < TAMANHO_MAXIMO);
    for (size_t i = 0; i < quantidade; i++)
        depois[i] = esperado[i];
    depois[quantidade] = nova;

    CHECK(lista_cartas_inserir_fim(lista, nova));
    conferir(lista, depois, quantidade + 1);
    CHECK(lista_cartas_remover(lista, nova));
    conferir(lista, esperado, quantidade);
}

struct Contagem {
    size_t visitadas;
    int soma_dos_valores;
    PokerCard primeira;
    PokerCard ultima;
};

static void contar(const PokerCard *c, void *contexto)
{
    struct Contagem *contagem = contexto;

    if (contagem->visitadas == 0)
        contagem->primeira = *c;
    contagem->ultima = *c;
    contagem->visitadas++;
    contagem->soma_dos_valores += c->rank;
}

static void test_comparar_usa_valor_e_depois_naipe(void)
{
    CHECK(lista_cartas_comparar(carta(2, POKER_SPADES), carta(3, POKER_CLUBS)) < 0);
    CHECK(lista_cartas_comparar(carta(3, POKER_CLUBS), carta(2, POKER_SPADES)) > 0);
    CHECK(lista_cartas_comparar(carta(14, POKER_CLUBS), carta(13, POKER_SPADES)) > 0);
    CHECK(lista_cartas_comparar(carta(10, POKER_HEARTS), carta(10, POKER_HEARTS)) == 0);

    CHECK(lista_cartas_comparar(carta(9, POKER_CLUBS), carta(9, POKER_DIAMONDS)) < 0);
    CHECK(lista_cartas_comparar(carta(9, POKER_DIAMONDS), carta(9, POKER_HEARTS)) < 0);
    CHECK(lista_cartas_comparar(carta(9, POKER_HEARTS), carta(9, POKER_SPADES)) < 0);
    CHECK(lista_cartas_comparar(carta(9, POKER_SPADES), carta(9, POKER_CLUBS)) > 0);

    CHECK(lista_cartas_comparar(carta(INT_MIN, POKER_CLUBS), carta(INT_MAX, POKER_CLUBS)) < 0);
    CHECK(lista_cartas_comparar(carta(INT_MAX, POKER_CLUBS), carta(INT_MIN, POKER_CLUBS)) > 0);
}

static void test_insercao_no_inicio_e_no_fim(void)
{
    ListaCartas lista = {0};
    PokerCard as = carta(14, POKER_SPADES);
    PokerCard dama = carta(12, POKER_HEARTS);
    PokerCard dois = carta(2, POKER_CLUBS);
    PokerCard sete = carta(7, POKER_DIAMONDS);

    conferir(&lista, NULL, 0);

    CHECK(lista_cartas_inserir_inicio(&lista, dama));
    {
        PokerCard esperado[] = {dama};
        conferir(&lista, esperado, 1);
        CHECK(lista.inicio == lista.fim);
    }

    CHECK(lista_cartas_inserir_fim(&lista, dois));
    CHECK(lista_cartas_inserir_inicio(&lista, as));
    CHECK(lista_cartas_inserir_fim(&lista, sete));
    {
        PokerCard esperado[] = {as, dama, dois, sete};
        conferir(&lista, esperado, 4);
        conferir_fim(&lista, esperado, 4);
    }

    lista_cartas_liberar(&lista);

    CHECK(lista_cartas_inserir_fim(&lista, dois));
    {
        PokerCard esperado[] = {dois};
        conferir(&lista, esperado, 1);
        CHECK(lista.inicio == lista.fim);
    }
    lista_cartas_liberar(&lista);
}

static void test_cartas_invalidas_e_argumentos_nulos_sao_recusados(void)
{
    ListaCartas lista = {0};
    PokerCard base = carta(5, POKER_HEARTS);
    PokerCard esperado[] = {base};
    PokerCard invalidas[] = {
        carta(1, POKER_CLUBS),
        carta(0, POKER_CLUBS),
        carta(15, POKER_CLUBS),
        carta(-3, POKER_CLUBS),
        carta(INT_MAX, POKER_CLUBS),
        carta(8, (PokerSuit)4),
        carta(8, (PokerSuit)99)
    };

    CHECK(lista_cartas_inserir_fim(&lista, base));

    for (size_t i = 0; i < sizeof invalidas / sizeof invalidas[0]; i++) {
        CHECK(!lista_cartas_inserir_fim(&lista, invalidas[i]));
        CHECK(!lista_cartas_inserir_inicio(&lista, invalidas[i]));
        conferir(&lista, esperado, 1);
    }

    CHECK(lista_cartas_inserir_inicio(&lista, carta(2, POKER_CLUBS)));
    CHECK(lista_cartas_inserir_fim(&lista, carta(14, POKER_SPADES)));
    lista_cartas_liberar(&lista);

    CHECK(!lista_cartas_inserir_inicio(NULL, base));
    CHECK(!lista_cartas_inserir_fim(NULL, base));
    CHECK(!lista_cartas_inserir_vetor(NULL, esperado, 1));
    CHECK(!lista_cartas_remover(NULL, base));
    CHECK(!lista_cartas_contem(NULL, base));
    CHECK(lista_cartas_copiar(NULL, esperado, 1) == 0);
    lista_cartas_percorrer(NULL, contar, NULL);
    lista_cartas_ordenar(NULL, ORDEM_CRESCENTE);
    lista_cartas_liberar(NULL);
}

static void test_remocao_em_cada_posicao(void)
{
    ListaCartas lista = {0};
    PokerCard a = carta(3, POKER_CLUBS);
    PokerCard b = carta(8, POKER_DIAMONDS);
    PokerCard c = carta(11, POKER_HEARTS);
    PokerCard d = carta(13, POKER_SPADES);
    PokerCard todas[] = {a, b, c, d};

    CHECK(lista_cartas_inserir_vetor(&lista, todas, 4));

    CHECK(lista_cartas_remover(&lista, b));
    {
        PokerCard esperado[] = {a, c, d};
        conferir(&lista, esperado, 3);
        conferir_fim(&lista, esperado, 3);
    }

    CHECK(lista_cartas_remover(&lista, a));
    {
        PokerCard esperado[] = {c, d};
        conferir(&lista, esperado, 2);
        conferir_fim(&lista, esperado, 2);
    }

    CHECK(lista_cartas_remover(&lista, d));
    {
        PokerCard esperado[] = {c};
        conferir(&lista, esperado, 1);
        CHECK(lista.inicio == lista.fim);
        conferir_fim(&lista, esperado, 1);
    }

    CHECK(!lista_cartas_remover(&lista, d));
    CHECK(!lista_cartas_remover(&lista, carta(11, POKER_CLUBS)));
    CHECK(!lista_cartas_remover(&lista, carta(10, POKER_HEARTS)));

    CHECK(lista_cartas_remover(&lista, c));
    conferir(&lista, NULL, 0);
    CHECK(!lista_cartas_remover(&lista, c));

    CHECK(lista_cartas_inserir_fim(&lista, a));
    CHECK(lista_cartas_inserir_inicio(&lista, b));
    {
        PokerCard esperado[] = {b, a};
        conferir(&lista, esperado, 2);
    }
    lista_cartas_liberar(&lista);
}

static void test_remocao_tira_so_a_primeira_repeticao(void)
{
    ListaCartas lista = {0};
    PokerCard rei = carta(13, POKER_HEARTS);
    PokerCard sete = carta(7, POKER_CLUBS);
    PokerCard inicial[] = {rei, sete, rei, rei};

    CHECK(lista_cartas_inserir_vetor(&lista, inicial, 4));

    CHECK(lista_cartas_remover(&lista, rei));
    {
        PokerCard esperado[] = {sete, rei, rei};
        conferir(&lista, esperado, 3);
    }

    CHECK(lista_cartas_remover(&lista, rei));
    CHECK(lista_cartas_remover(&lista, rei));
    {
        PokerCard esperado[] = {sete};
        conferir(&lista, esperado, 1);
        conferir_fim(&lista, esperado, 1);
    }
    lista_cartas_liberar(&lista);
}

static void test_contem_distingue_valor_e_naipe(void)
{
    ListaCartas lista = {0};
    PokerCard cartas[] = {carta(4, POKER_CLUBS), carta(4, POKER_HEARTS), carta(12, POKER_SPADES)};

    CHECK(!lista_cartas_contem(&lista, cartas[0]));
    CHECK(lista_cartas_inserir_vetor(&lista, cartas, 3));

    CHECK(lista_cartas_contem(&lista, carta(4, POKER_CLUBS)));
    CHECK(lista_cartas_contem(&lista, carta(4, POKER_HEARTS)));
    CHECK(lista_cartas_contem(&lista, carta(12, POKER_SPADES)));
    CHECK(!lista_cartas_contem(&lista, carta(4, POKER_DIAMONDS)));
    CHECK(!lista_cartas_contem(&lista, carta(12, POKER_CLUBS)));
    CHECK(!lista_cartas_contem(&lista, carta(5, POKER_CLUBS)));
    lista_cartas_liberar(&lista);
}

static void test_percurso_e_copia(void)
{
    ListaCartas lista = {0};
    PokerCard cartas[] = {
        carta(9, POKER_CLUBS), carta(2, POKER_HEARTS), carta(14, POKER_SPADES),
        carta(6, POKER_DIAMONDS), carta(10, POKER_CLUBS)
    };
    struct Contagem contagem = {0};
    PokerCard saida[8];

    lista_cartas_percorrer(&lista, contar, &contagem);
    CHECK(contagem.visitadas == 0);
    CHECK(lista_cartas_copiar(&lista, saida, 8) == 0);

    CHECK(lista_cartas_inserir_vetor(&lista, cartas, 5));

    lista_cartas_percorrer(&lista, contar, &contagem);
    CHECK(contagem.visitadas == 5);
    CHECK(contagem.soma_dos_valores == 9 + 2 + 14 + 6 + 10);
    CHECK(mesma_carta(contagem.primeira, cartas[0]));
    CHECK(mesma_carta(contagem.ultima, cartas[4]));

    lista_cartas_percorrer(&lista, NULL, &contagem);
    CHECK(contagem.visitadas == 5);

    CHECK(lista_cartas_copiar(&lista, saida, 8) == 5);
    for (size_t i = 0; i < 5; i++)
        CHECK(mesma_carta(saida[i], cartas[i]));

    CHECK(lista_cartas_copiar(&lista, saida, 5) == 5);
    CHECK(lista_cartas_copiar(&lista, saida, 3) == 3);
    CHECK(mesma_carta(saida[2], cartas[2]));
    CHECK(lista_cartas_copiar(&lista, saida, 0) == 0);
    CHECK(lista_cartas_copiar(&lista, NULL, 5) == 0);

    lista_cartas_liberar(&lista);
}

static void test_insercao_de_vetor_e_recusa_sem_efeito_colateral(void)
{
    ListaCartas lista = {0};
    PokerCard base[] = {carta(2, POKER_CLUBS), carta(3, POKER_CLUBS)};
    PokerCard boas[] = {carta(10, POKER_HEARTS), carta(11, POKER_HEARTS), carta(12, POKER_HEARTS)};
    PokerCard ruins[] = {carta(5, POKER_SPADES), carta(6, POKER_SPADES), carta(1, POKER_SPADES), carta(7, POKER_SPADES)};

    CHECK(lista_cartas_inserir_vetor(&lista, NULL, 0));
    conferir(&lista, NULL, 0);
    CHECK(!lista_cartas_inserir_vetor(&lista, NULL, 2));
    conferir(&lista, NULL, 0);

    CHECK(!lista_cartas_inserir_vetor(&lista, ruins, 4));
    conferir(&lista, NULL, 0);
    CHECK(lista_cartas_inserir_fim(&lista, base[0]));
    lista_cartas_liberar(&lista);

    CHECK(lista_cartas_inserir_vetor(&lista, base, 2));
    CHECK(lista_cartas_inserir_vetor(&lista, boas, 3));
    {
        PokerCard esperado[] = {base[0], base[1], boas[0], boas[1], boas[2]};
        conferir(&lista, esperado, 5);
        conferir_fim(&lista, esperado, 5);
    }

    CHECK(!lista_cartas_inserir_vetor(&lista, ruins, 4));
    {
        PokerCard esperado[] = {base[0], base[1], boas[0], boas[1], boas[2]};
        conferir(&lista, esperado, 5);
        conferir_fim(&lista, esperado, 5);
    }

    CHECK(lista_cartas_inserir_vetor(&lista, ruins, 2));
    {
        PokerCard esperado[] = {base[0], base[1], boas[0], boas[1], boas[2], ruins[0], ruins[1]};
        conferir(&lista, esperado, 7);
    }
    lista_cartas_liberar(&lista);
}

static void test_ordenacao_de_casos_pequenos(void)
{
    ListaCartas lista = {0};
    PokerCard a = carta(2, POKER_CLUBS);
    PokerCard b = carta(2, POKER_SPADES);
    PokerCard c = carta(9, POKER_HEARTS);
    PokerCard d = carta(14, POKER_DIAMONDS);

    lista_cartas_ordenar(&lista, ORDEM_CRESCENTE);
    lista_cartas_ordenar(&lista, ORDEM_DECRESCENTE);
    conferir(&lista, NULL, 0);

    CHECK(lista_cartas_inserir_fim(&lista, c));
    lista_cartas_ordenar(&lista, ORDEM_CRESCENTE);
    lista_cartas_ordenar(&lista, ORDEM_DECRESCENTE);
    {
        PokerCard esperado[] = {c};
        conferir(&lista, esperado, 1);
        CHECK(lista.inicio == lista.fim);
    }
    lista_cartas_liberar(&lista);

    {
        PokerCard entrada[] = {d, c, b, a};
        PokerCard crescente[] = {a, b, c, d};
        PokerCard decrescente[] = {d, c, b, a};

        CHECK(lista_cartas_inserir_vetor(&lista, entrada, 4));
        lista_cartas_ordenar(&lista, ORDEM_CRESCENTE);
        conferir(&lista, crescente, 4);
        conferir_fim(&lista, crescente, 4);

        lista_cartas_ordenar(&lista, ORDEM_CRESCENTE);
        conferir(&lista, crescente, 4);

        lista_cartas_ordenar(&lista, ORDEM_DECRESCENTE);
        conferir(&lista, decrescente, 4);
        conferir_fim(&lista, decrescente, 4);

        lista_cartas_ordenar(&lista, ORDEM_DECRESCENTE);
        conferir(&lista, decrescente, 4);
    }
    lista_cartas_liberar(&lista);

    {
        PokerCard entrada[] = {c, c, a, c, a};
        PokerCard crescente[] = {a, a, c, c, c};

        CHECK(lista_cartas_inserir_vetor(&lista, entrada, 5));
        lista_cartas_ordenar(&lista, ORDEM_CRESCENTE);
        conferir(&lista, crescente, 5);
        conferir_fim(&lista, crescente, 5);
    }
    lista_cartas_liberar(&lista);
}

static void test_naipe_desempata_cartas_de_mesmo_valor(void)
{
    ListaCartas lista = {0};
    PokerCard entrada[] = {
        carta(8, POKER_SPADES), carta(8, POKER_CLUBS),
        carta(8, POKER_HEARTS), carta(8, POKER_DIAMONDS)
    };
    PokerCard crescente[] = {
        carta(8, POKER_CLUBS), carta(8, POKER_DIAMONDS),
        carta(8, POKER_HEARTS), carta(8, POKER_SPADES)
    };
    PokerCard decrescente[] = {
        carta(8, POKER_SPADES), carta(8, POKER_HEARTS),
        carta(8, POKER_DIAMONDS), carta(8, POKER_CLUBS)
    };

    CHECK(lista_cartas_inserir_vetor(&lista, entrada, 4));
    lista_cartas_ordenar(&lista, ORDEM_CRESCENTE);
    conferir(&lista, crescente, 4);
    lista_cartas_ordenar(&lista, ORDEM_DECRESCENTE);
    conferir(&lista, decrescente, 4);
    lista_cartas_liberar(&lista);
}

static int comparar_para_qsort(const void *a, const void *b)
{
    return lista_cartas_comparar(*(const PokerCard *)a, *(const PokerCard *)b);
}

static int comparar_para_qsort_invertido(const void *a, const void *b)
{
    return lista_cartas_comparar(*(const PokerCard *)b, *(const PokerCard *)a);
}

static void test_ordenacao_confere_com_qsort_em_listas_aleatorias(void)
{
    for (unsigned semente = 1; semente <= SEMENTES; semente++) {
        unsigned estado = semente * 7919u;
        size_t tamanho = proximo_aleatorio(&estado) % (TAMANHO_MAXIMO + 1);
        PokerCard entrada[TAMANHO_MAXIMO];
        PokerCard referencia[TAMANHO_MAXIMO];
        ListaCartas lista = {0};

        for (size_t i = 0; i < tamanho; i++) {
            int valor = 2 + (int)(proximo_aleatorio(&estado) % 13);
            PokerSuit naipe = (PokerSuit)(proximo_aleatorio(&estado) % 4);

            entrada[i] = carta(valor, naipe);
        }

        for (int sentido = 0; sentido < 2; sentido++) {
            OrdemCartas ordem = sentido == 0 ? ORDEM_CRESCENTE : ORDEM_DECRESCENTE;

            for (size_t i = 0; i < tamanho; i++)
                referencia[i] = entrada[i];
            qsort(referencia, tamanho,
                  sizeof referencia[0],
                  sentido == 0 ? comparar_para_qsort : comparar_para_qsort_invertido);

            CHECK(lista_cartas_inserir_vetor(&lista, entrada, tamanho));
            lista_cartas_ordenar(&lista, ordem);
            conferir(&lista, referencia, tamanho);
            if (tamanho < TAMANHO_MAXIMO)
                conferir_fim(&lista, referencia, tamanho);

            for (size_t i = 0; i < tamanho; i++)
                CHECK(lista_cartas_contem(&lista, entrada[i]));

            lista_cartas_liberar(&lista);
        }
    }
}

static void test_ordenar_mao_de_sete_cartas_nao_muda_a_avaliacao(void)
{
    for (unsigned semente = 1; semente <= SEMENTES; semente++) {
        Deck baralho = {0};
        PokerCard mao[7];
        PokerCard ordenada[7];
        ListaCartas lista = {0};
        PokerHandValue antes;
        PokerHandValue depois;

        srand(semente);
        CHECK(deck_create(&baralho));
        deck_shuffle(&baralho);
        for (size_t i = 0; i < 7; i++)
            CHECK(deck_draw(&baralho, &mao[i]));
        deck_free(&baralho);

        CHECK(poker_evaluate(mao, 7, &antes) == POKER_OK);

        CHECK(lista_cartas_inserir_vetor(&lista, mao, 7));
        lista_cartas_ordenar(&lista, semente % 2 == 0 ? ORDEM_CRESCENTE : ORDEM_DECRESCENTE);
        CHECK(lista_cartas_copiar(&lista, ordenada, 7) == 7);
        lista_cartas_liberar(&lista);

        for (size_t i = 1; i < 7; i++) {
            int comparacao = lista_cartas_comparar(ordenada[i - 1], ordenada[i]);

            CHECK(semente % 2 == 0 ? comparacao < 0 : comparacao > 0);
        }

        CHECK(poker_evaluate(ordenada, 7, &depois) == POKER_OK);
        CHECK(poker_compare(&antes, &depois) == 0);
    }
}

static void test_liberar_deixa_a_lista_reutilizavel(void)
{
    ListaCartas lista = {0};
    PokerCard cartas[] = {carta(6, POKER_CLUBS), carta(7, POKER_CLUBS), carta(8, POKER_CLUBS)};

    lista_cartas_liberar(&lista);
    conferir(&lista, NULL, 0);

    CHECK(lista_cartas_inserir_vetor(&lista, cartas, 3));
    lista_cartas_liberar(&lista);
    conferir(&lista, NULL, 0);
    lista_cartas_liberar(&lista);
    conferir(&lista, NULL, 0);

    CHECK(lista_cartas_inserir_vetor(&lista, cartas, 3));
    conferir(&lista, cartas, 3);
    lista_cartas_liberar(&lista);
}

int main(void)
{
    test_comparar_usa_valor_e_depois_naipe();
    test_insercao_no_inicio_e_no_fim();
    test_cartas_invalidas_e_argumentos_nulos_sao_recusados();
    test_remocao_em_cada_posicao();
    test_remocao_tira_so_a_primeira_repeticao();
    test_contem_distingue_valor_e_naipe();
    test_percurso_e_copia();
    test_insercao_de_vetor_e_recusa_sem_efeito_colateral();
    test_ordenacao_de_casos_pequenos();
    test_naipe_desempata_cartas_de_mesmo_valor();
    test_ordenacao_confere_com_qsort_em_listas_aleatorias();
    test_ordenar_mao_de_sete_cartas_nao_muda_a_avaliacao();
    test_liberar_deixa_a_lista_reutilizavel();
    puts("OK: lista de cartas (insercao, remocao, percurso, copia, recusa de cartas invalidas) e Insertion Sort nos dois sentidos.");
    return EXIT_SUCCESS;
}
