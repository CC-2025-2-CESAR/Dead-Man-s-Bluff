# Dead Man's Bluff

Roguelike de pôquer desenvolvido em C com Raylib para a disciplina de Algoritmos e Estruturas de Dados (AED).

> **Estado atual:** planejamento e estruturação inicial. O projeto ainda não é jogável nem compilável.

## Sobre o jogo

O jogador enfrenta adversários em diferentes mesas de pôquer, aumentando progressivamente as apostas (*stakes*) até alcançar o chefe final. O diferencial é a possibilidade de recorrer a ações ilegais para virar o jogo, ao custo de atrair suspeita e correr o risco de ser descoberto.

## Inspiração

A atmosfera é inspirada em **007 — Cassino Royale**: mesas de alto risco, tensão psicológica e blefe. O universo, os personagens e a história são próprios do projeto.

## Mecânicas planejadas

- Partidas de pôquer contra adversários com comportamentos diferentes.
- Aumento progressivo de stakes ao longo das mesas, até o chefe final.
- Ações ilegais: roubar fichas, marcar cartas, subornar o dealer, ameaçar e envenenar adversários.
- Resultado das ações ilegais dependente de probabilidades e das habilidades do jogador.
- Gerenciamento de suspeita.
- Progressão roguelike com melhorias de habilidades.

## Tecnologias e estruturas de dados previstas

- Linguagem **C**.
- Biblioteca **Raylib** para a parte gráfica.
- **Listas simplesmente encadeadas** para o gerenciamento das cartas.
- **Insertion Sort** para a ordenação das mãos.
- Algoritmos próprios para probabilidades e decisões dos adversários.

## Equipe e responsabilidades

| Integrante | Área | Responsabilidades |
| --- | --- | --- |
| Arthur | Game Engine e arquitetura | Loop principal, gerenciamento de estados, sistema de turnos, integração entre os módulos |
| Júlia | Poker Engine | Regras do pôquer, avaliação das mãos, sistema de apostas, comparação de combinações e determinação dos vencedores |
| Rafael | Cartas e estruturas de dados | Listas simplesmente encadeadas, gerenciamento do baralho, embaralhamento e distribuição, compra, descarte e ordenação das cartas |
| Peixoto | Mecânicas e progressão | Ações ilegais, sistema de probabilidades, gerenciamento de suspeita, adversários e progressão roguelike |
| Matheus | Interface gráfica | Interface com Raylib, menus e telas, renderização de cartas e fichas, interações e animações |

## Estrutura de pastas

```
Dead-Man-s-Bluff/
├── README.md
├── src/
│   ├── main.c
│   ├── core/       # Engine: loop, estados e turnos
│   ├── cards/      # Lista encadeada de cartas e baralho
│   ├── poker/      # Regras, avaliação de mãos e apostas
│   ├── gameplay/   # Trapaças, probabilidades, adversários e progressão
│   └── ui/         # Interface gráfica e animações (Raylib)
└── assets/
    ├── images/
    ├── sounds/
    └── fonts/
```

| Pasta | Conteúdo |
| --- | --- |
| `src/core/` | Núcleo do jogo e integração entre os módulos |
| `src/cards/` | Estruturas de dados das cartas e gerenciamento do baralho |
| `src/poker/` | Motor de pôquer e sistema de apostas |
| `src/gameplay/` | Mecânicas de trapaça, probabilidades, adversários e progressão |
| `src/ui/` | Telas, menus, renderização e animações |
| `assets/` | Imagens, sons e fontes |

Os arquivos `.c` e `.h` existentes contêm apenas comentários `TODO` com a responsabilidade de cada módulo. Nenhuma funcionalidade foi implementada ainda.

## Estado atual

- [x] Planejamento do projeto e divisão de responsabilidades.
- [x] Estruturação inicial do repositório.
- [ ] Implementação dos módulos.
- [ ] Integração e testes.
