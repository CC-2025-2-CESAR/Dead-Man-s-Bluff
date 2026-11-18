# Dead Man's Bluff

Jogo de pôquer Texas Hold'em para um jogador, inspirado em **007 — Cassino Royale** e desenvolvido em **C com Raylib** para a disciplina de Algoritmos e Estruturas de Dados da CESAR School.

O objetivo é derrotar três adversários em confrontos de um contra um. Além das apostas e dos blefes, é possível marcar cartas, subornar o dealer e usar outras trapaças. Ser descoberto ou ficar sem fichas pode levar à roleta russa.

O projeto utiliza **listas simplesmente encadeadas** no gerenciamento das cartas e **Insertion Sort** na ordenação das mãos. Os adversários tomam decisões por regras e probabilidades implementadas em C.

## Equipe e contribuições

| Integrante | Contribuição |
| --- | --- |
| Arthur Filipe Silva dos Reis (afsr) | Núcleo do jogo, torneio, integração dos módulos, salvamento e áudio. |
| Gabriel Peixoto e Silva Ferreira (gpsf) | Trapaças, probabilidades de descoberta, suspeita e roleta russa. |
| Júlia Tenório Calado (jtc) | Regras do pôquer, avaliação das mãos, apostas e definição dos vencedores. |
| Matheus Assis de Souza Jácome (masj) | Interface com Raylib, menus, telas, cartas, fichas e animações. |
| Rafael Almeida Carrilho (rac4) | Baralho, listas encadeadas, ordenação das cartas e IA dos adversários. |

## Como executar

Requisitos: **compilador C11, GNU Make, Raylib 5.5 e pkg-config**. Execute os comandos na raiz do projeto, onde ficam o `Makefile` e a pasta `assets/`.

### Windows — MSYS2 UCRT64

No terminal **MSYS2 UCRT64**, instale as dependências uma vez:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-{gcc,make,raylib,pkgconf}
```

Depois, compile e abra o jogo:

```sh
mingw32-make jogo CC=gcc
./build/dead_mans_bluff.exe
```

### Linux / macOS

Com as dependências instaladas:

```sh
make jogo
./build/dead_mans_bluff
```

## Controles

Todas as ações também podem ser feitas com o **mouse**.

| Tecla | Ação |
| --- | --- |
| W/S ou setas e Enter | Navegar e confirmar nos menus |
| F / C / R / A | Desistir / passar ou pagar / apostar ou aumentar / all-in |
| ↑ / ↓ | Ajustar a aposta de uma em uma ficha; Shift usa passos de um big blind |
| T | Abrir as trapaças |
| Enter ou Espaço | Avançar após a mão e confirmar a roleta |
| Esc | Voltar |

O progresso é salvo automaticamente. Em **Configurações**, é possível alterar a resolução e os volumes Geral, SFX e Música de 0 a 100.

Para executar os testes automatizados: `make test` (ou `mingw32-make test CC=gcc` no Windows).
