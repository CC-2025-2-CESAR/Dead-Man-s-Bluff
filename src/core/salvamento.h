#ifndef SALVAMENTO_H
#define SALVAMENTO_H

#include <stdbool.h>
#include <stddef.h>

#include "core/partida.h"

#define SALVAMENTO_ARQUIVO "progresso.sav"
#define SALVAMENTO_TAMANHO_MAXIMO 32768

/**
 * Escreve os dados guardados de uma partida em texto. O formato tem uma
 * linha de cabeçalho com a versão, uma linha por grupo de dados (torneio,
 * mão, IA, cartas, board, fichas) e uma linha por ação, e termina em "fim".
 *
 * @param salva dados a escrever.
 * @param saida recebe o texto, terminado em '\0'.
 * @param capacidade tamanho de saida em bytes; SALVAMENTO_TAMANHO_MAXIMO
 * sempre basta.
 * @return quantos caracteres foram escritos, sem contar o '\0'; 0 se os
 * dados forem NULL ou o texto não couber.
 */
size_t salvamento_formatar(const PartidaSalva *salva, char *saida, size_t capacidade);

/**
 * Lê o texto escrito por salvamento_formatar e confere cada campo: versão,
 * ordem das linhas, números inteiros dentro dos limites, cartas e ações
 * válidas e nada depois do "fim". Texto cortado, alterado ou de outra versão
 * é recusado, e nunca derruba o jogo.
 *
 * @param texto texto a ler; não precisa terminar em '\0'.
 * @param tamanho quantos caracteres de texto ler.
 * @param salva recebe os dados; só vale se a função devolver true.
 * @return true se o texto é um salvamento válido.
 */
bool salvamento_interpretar(const char *texto, size_t tamanho, PartidaSalva *salva);

/**
 * Grava os dados num arquivo. Escreve primeiro num arquivo temporário e só
 * depois troca pelo definitivo, para que uma falha no meio da gravação não
 * destrua o salvamento anterior.
 *
 * @param salva dados a gravar.
 * @param caminho arquivo de destino.
 * @return true se o arquivo foi gravado.
 */
bool salvamento_gravar(const PartidaSalva *salva, const char *caminho);

/**
 * Lê um arquivo gravado por salvamento_gravar.
 *
 * @param caminho arquivo a ler.
 * @param salva recebe os dados; só vale se a função devolver true.
 * @return true se o arquivo existe, cabe em SALVAMENTO_TAMANHO_MAXIMO e é um
 * salvamento válido.
 */
bool salvamento_ler(const char *caminho, PartidaSalva *salva);

/**
 * Diz se há um salvamento no caminho, olhando só o cabeçalho. Um arquivo com
 * o cabeçalho certo e o resto estragado conta como existente; quem o carrega
 * descobre o defeito em salvamento_ler.
 *
 * @param caminho arquivo a conferir.
 * @return true se o arquivo existe e começa com o cabeçalho do salvamento.
 */
bool salvamento_existe(const char *caminho);

/**
 * Apaga o salvamento, se existir. Não faz nada se não existir.
 *
 * @param caminho arquivo a apagar.
 */
void salvamento_apagar(const char *caminho);

#endif
