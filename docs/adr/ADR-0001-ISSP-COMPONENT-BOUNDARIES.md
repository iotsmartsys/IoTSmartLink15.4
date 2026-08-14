# ADR-0001 — Fronteiras dos componentes ISSP compartilhados

**Estado:** Accepted

**Data:** 2026-08-11

**Decisores:** Arquiteto humano

**Especificações relacionadas:** `docs/specs/ISSP-Architecture.md`,
`docs/specs/ISSP-Reusable-Components.md` e
`docs/specs/ISSP-Configurable-Bootstrap.md`

**Substitui:** Nenhuma

## Contexto

O client precisa reutilizar protocolo, transporte, behaviors, bootstrap e
factory reset em mais de um produto sem duplicar o firmware nem expor detalhes
do IEEE 802.15.4 na API da aplicação.

## Alternativas consideradas

- manter toda a composição em cada `main`, com duplicação e acoplamento;
- expor componentes técnicos diretamente a todos os produtos;
- oferecer componentes especializados e uma fachada pública fina.

## Decisão

`issp_core`, `issp_transport_154` e `issp_behaviors` mantêm responsabilidades
técnicas separadas. `issp_app_154` oferece a fachada pública `SmartSysApp` e
compõe os detalhes por delegação e armazenamento opaco. Aplicações podem usar a
fachada ou, quando necessário, consumir os componentes técnicos explicitamente.

## Consequências

- produtos não precisam conhecer transporte, commissioning ou executor;
- contratos públicos e dependências privadas precisam permanecer separados;
- mudanças wire continuam exigindo coordenação entre client e coordenador;
- o tamanho dos armazenamentos opacos permanece restrição explícita e validada
  por `static_assert`.

## Critério de reavaliação

Reavaliar se outro transporte, outra plataforma ou uma capability exigir
romper a direção de dependência ou expor tipos técnicos na fachada pública.

## Nota de reavaliação — tipos de driver na fachada (14/08/2026)

A capability de nível de bateria de `EKOM-BATTERY-001` acionou este critério ao
levar unidade, canal e atenuação de ADC à configuração pública. O Arquiteto
decidiu que **tipos de driver do ESP-IDF podem aparecer na fachada**, pelo
mesmo precedente já vigente de `gpio_num_t` em `SwitchConfig` e
`PushButtonConfig`, com a dependência correspondente declarada pública.

A decisão não altera a direção de dependência nem autoriza expor tipo de
protocolo, transporte ou commissioning na fachada, que permanecem privados. A
decisão vale para tipos de periférico entregues pelo board model ao produto.
