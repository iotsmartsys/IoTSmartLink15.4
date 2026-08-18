# Validação e encerramento — EKOM-CLIENT-CONFIG-001

**Classe da fonte:** Relatório

**Papel:** Consultor de Arquitetura com confirmação do Arquiteto

**Especificação:** `docs/specs/Client-SDK-Configurable-Features.md`

**Revisão confrontada:** `dfbd6f5eeefe7cb757b33b461898595a45d6175a` e o
delta recuperado da execução `32091116616`

**Estado:** Concluído por decisão do Arquiteto

**Data:** 18/08/2026

---

## Confirmação do Arquiteto

O Arquiteto confirmou que a hierarquia do SDK Configuration Editor passou a
funcionar como esperado e determinou: renomear o menu do projeto para
`App Client`, atualizar o mapa de conhecimento e fechar a especificação v0.1.

Esta fonte registra a decisão humana e as evidências observadas. Ela não alega
revisão independente do trabalho do Consultor.

## Correções incorporadas

- escolha de product firmware encerrada antes das features;
- escolha de board model preservada como grupo separado;
- grupos `Firmware features` e `Board configuration` criados no local
  normativo;
- menu superior renomeado para `App Client`;
- ausência de chamada a `configureDeepSleep()` quando a feature está
  desabilitada;
- colisões do GPIO de factory reset com contato seco, wake LED e medição de
  bateria rejeitadas durante o build.

## Evidência de configuração

A árvore produzida pelo ESP-IDF 6.0.1 foi inspecionada mecanicamente e contém,
nesta ordem:

```text
App Client
├── Product firmware
├── Board model
├── Firmware features
└── Board configuration
```

O Arquiteto confirmou manualmente a apresentação esperada no SDK Configuration
Editor antes de ordenar o encerramento.

## Builds proporcionais

Todos os builds abaixo usaram ESP-IDF 6.0.1, target ESP32-H2,
`IDF_COMPONENT_MANAGER=0`, `sdkconfig` temporário e diretório de build isolado.

| Composição | Resultado terminal | Código de saída |
|---|---|---:|
| configuração default versionada | `Project build complete` | 0 |
| deep sleep desabilitado e bateria habilitada | `Project build complete` | 0 |
| bateria desabilitada | `Project build complete` | 0 |
| janela de 45 s, wakeup de 30 min e factory reset no GPIO 10 | `Project build complete` | 0 |

As configurações com factory reset nos GPIOs 14, 13 e 1 foram rejeitadas por
`static_assert`, respectivamente por colisão com `dry_contact_input`,
`wake_led` e `battery_measurement`. Nenhuma delas produziu binário.

## Operações não executadas e limites

- Testes: `Not Executed`.
- Flash: `Not Executed`.
- Monitor: `Not Executed`.
- Hardware: `Not Executed`.

A confirmação humana cobre a experiência do menu. Os builds comprovam
configuração e compilação, mas não comportamento físico, temporização real,
medição de bateria nem factory reset no dispositivo.

## Decisão

O Arquiteto considerou a evidência suficiente e determinou que a v0.1 seja
Concluída [`Done`] em 18/08/2026. As limitações acima permanecem preservadas e
não são convertidas em sucesso. Nova necessidade ou evidência material exige
decisão de reabertura.
