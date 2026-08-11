# ADR-0002 — Composição separada de product firmware e board model

**Estado:** Accepted

**Data:** 2026-08-11

**Decisores:** Arquiteto humano

**Especificações relacionadas:** `docs/specs/Firmware-Variants-Menuconfig.md`

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

## Consequências

- product firmware descreve comportamento e capabilities;
- board model descreve pinagem, polaridade e recursos físicos;
- componentes compartilhados não contêm símbolos de produto ou board;
- adicionar variante exige declarar produto, board compatível e seleção.

## Critério de reavaliação

Reavaliar se uma board precisar de descoberta dinâmica de recursos ou se a
compatibilidade deixar de ser expressável por requisitos estáticos.
