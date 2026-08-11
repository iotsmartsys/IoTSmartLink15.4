# ADR-0003 — Targets físicos e execução deliberada de testes

**Estado:** Accepted

**Data:** 2026-08-11

**Decisores:** Arquiteto humano

**Especificações relacionadas:**
`docs/specs/Repository-Test-Execution-Policy.md`

**Substitui:** Nenhuma

## Contexto

O repositório usa IEEE 802.15.4 e chegou a carregar runners ESP32-C3 e QEMU que
não representavam o hardware suportado. A execução automática dessas suítes não
demonstrou benefício proporcional ao custo do experimento EKOM.

## Alternativas consideradas

- manter alvos genéricos e QEMU como conveniência;
- executar todas as suítes em cada atuação;
- restringir targets e exigir autorização explícita para execução.

## Decisão

Os únicos targets físicos são ESP32-H2 para o client e ESP32-C6 para o
coordenador. ESP32-C3 e QEMU não são suportados. Testes permanecem preservados,
mas coleta, flash, monitor, pytest ou execução dependem de ordem em
especificação futura. Testes host-native só usam toolchain de host puro.

## Consequências

- guards impedem configuração com target indevido;
- ausência de execução permanece registrada como `Not Executed`;
- build não é apresentado como evidência de comportamento executado;
- nova estratégia de execução exige nova decisão normativa.

## Critério de reavaliação

Reavaliar quando uma especificação futura demonstrar benefício material de
outra estratégia ou quando o conjunto de hardware suportado mudar.
