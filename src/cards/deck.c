#include "deck.h"

#include <stdlib.h>

struct CardNode {
    PokerCard card;
    struct CardNode *next;
};

void deck_free(Deck *deck)
{
    if (deck == NULL)
        return;

    CardNode *atual = deck->head;
    while (atual != NULL) {
        CardNode *next = atual->next;
        free(atual);
        atual = next;
    }

    deck->head = NULL;
    deck->tail = NULL;
    deck->count = 0;
}

bool deck_create(Deck *deck)
{
    if (deck == NULL || deck->head != NULL ||
        deck->tail != NULL || deck->count != 0)
        return false;

    for (int suit = POKER_CLUBS; suit <= POKER_SPADES; suit++) {
        for (int rank = 2; rank <= 14; rank++) {
            CardNode *new_node = malloc(sizeof *new_node);
            if (new_node == NULL) {
                deck_free(deck);
                return false;
            }

            new_node->card = (PokerCard){rank, (PokerSuit)suit};
            new_node->next = NULL;

            if (deck->head == NULL) {
                deck->head = new_node;
                deck->tail = new_node;
            } else {
                deck->tail->next = new_node;
                deck->tail = new_node;
            }

            deck->count++;
        }
    }

    return true;
}

void deck_shuffle(Deck *deck)
{
    if (deck == NULL || deck->count < 2 || deck->count > 52)
        return;

    /* Guarda os enderecos dos nos, sem copiar ou realocar cartas. */
    CardNode *nodes[52];
    CardNode *atual = deck->head;

    for (size_t i = 0; i < deck->count; i++) {
        if (atual == NULL)
            return;

        nodes[i] = atual;
        atual = atual->next;
    }

    /* Se sobraram nos, count esta inconsistente com a lista. */
    if (atual != NULL)
        return;

    /* Fisher-Yates: troca somente o conteudo card de cada no. */
    for (size_t indice_atual = deck->count - 1;
         indice_atual > 0;
         indice_atual--) {
        size_t indice_sorteado = (size_t)rand() % (indice_atual + 1);
        PokerCard temp = nodes[indice_atual]->card;
        nodes[indice_atual]->card = nodes[indice_sorteado]->card;
        nodes[indice_sorteado]->card = temp;
    }
}

bool deck_draw(Deck *deck, PokerCard *card)
{
    if (deck == NULL || card == NULL ||
        deck->head == NULL || deck->count == 0)
        return false;

    CardNode *first = deck->head;
    *card = first->card;
    deck->head = first->next;

    if (deck->head == NULL)
        deck->tail = NULL;

    deck->count--;
    free(first);
    return true;
}

bool deck_burn(Deck *deck)
{
    PokerCard discarded;
    return deck_draw(deck, &discarded);
}

bool deck_deal(Deck *deck, int dealer, PokerCard hole[2][2])
{
    if (deck == NULL || hole == NULL ||
        (dealer != 0 && dealer != 1) || deck->count < 4)
        return false;

    int other = 1 - dealer;

    for (int i = 0; i < 2; i++) {
        deck_draw(deck, &hole[dealer][i]);
        deck_draw(deck, &hole[other][i]);
    }

    return true;
}

bool deck_deal_board(Deck *deck,
                     PokerStreet street,
                     PokerCard board[5],
                     size_t *board_count)
{
    if (deck == NULL || board == NULL || board_count == NULL)
        return false;

    size_t expected;
    size_t deal_count;

    switch (street) {
        case POKER_FLOP:
            expected = 0;
            deal_count = 3;
            break;
        case POKER_TURN:
            expected = 3;
            deal_count = 1;
            break;
        case POKER_RIVER:
            expected = 4;
            deal_count = 1;
            break;
        default:
            return false;
    }

    /* Uma carta queimada + as comunitarias que serao reveladas. */
    if (*board_count != expected || deck->count < deal_count + 1)
        return false;

    deck_burn(deck);
    for (size_t i = 0; i < deal_count; i++)
        deck_draw(deck, &board[expected + i]);

    *board_count += deal_count;
    return true;
}
