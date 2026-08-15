# ADR-0002 — Composição separada de product firmware e board model

**Estado:** Accepted

**Data:** 2026-08-11

**Decisores:** Arquiteto humano

**Especificações relacionadas:** `docs/specs/Firmware-Variants-Menuconfig.md`;
`docs/specs/Client-SDK-Configurable-Features.md`

**Emendada pelo Arquiteto em:** 15/08/2026

**Substitui:** Nenhuma

## Contexto

Um único projeto ESP-IDF produz firmwares diferentes para placas físicas com
pinagens e recursos próprios. Misturar seleção, lógica de produto e detalhes
físicos espalharia condicionais pelos componentes compartilhados.

## Alternativas consideradas

- um projeto por produto, duplicando runtime;
- um firmware monolítico controlado por condicionais internas;
- composição explícita de produto e placa no limite da aplicação.

## Decisão

Kconfig seleciona exatamente um product firmware e um board model. O produto
declara recursos de que depende; a board declara recursos físicos que oferece.
CMake inclui somente a composição selecionada e rejeita combinações
incompatíveis. Kconfig escolhe composição, não lógica interna de componentes.

Como exceção estreita, aceita pelo Arquiteto para
`EKOM-CLIENT-CONFIG-001`, Kconfig pode parametrizar o **número do GPIO do botão
de factory reset** no board selecionado. O board continua responsável por
oferecer o recurso, por sua polaridade e pelos demais fatos elétricos; o product
firmware continua recebendo o botão pelo contrato do board, sem ler diretamente
o símbolo. A exceção não autoriza parametrizar outros GPIOs por analogia nem
levar símbolos `CONFIG_*` aos componentes compartilhados.

## Consequências

- product firmware descreve comportamento e capabilities;
- board model descreve pinagem, polaridade e recursos físicos;
- o GPIO de factory reset é a única pinagem atualmente parametrizável no board
  selecionado e preserva GPIO 9 como default;
- componentes compartilhados não contêm símbolos de produto ou board;
- adicionar variante exige declarar produto, board compatível e seleção.

## Critério de reavaliação

Reavaliar se uma board precisar de descoberta dinâmica de recursos ou se a
compatibilidade deixar de ser expressável por requisitos estáticos. Reavaliar
também antes de tornar configurável qualquer outra pinagem, para impedir que a
exceção do factory reset dissolva a responsabilidade do board model.
