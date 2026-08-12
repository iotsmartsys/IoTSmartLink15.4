# Dossiê do Sistema — IoTSmartLink15.4

**Tipo:** Informativo com decisões confirmadas identificadas

**Estado da fonte:** Vigente [`Active`]

**Última auditoria:** 2026-08-12

## 1. Resumo executivo

O repositório contém os dois extremos de uma solução ISSP sobre IEEE 802.15.4:
firmwares client em ESP32-H2 e um coordenador em ESP32-C6. O coordenador também
oferece uma ponte JSON-lines por UART para o host.

## 2. Escopo e suporte vigente

| Item | Situação | Natureza | Fonte |
|---|---|---|---|
| `client_154` em ESP32-H2 | Suportado | Decisão | ADR-0003 |
| `coordinator_154` em ESP32-C6 | Suportado | Decisão | ADR-0003 |
| ESP32-C3 e QEMU | Não suportados | Decisão | ADR-0003 |
| Single smart plug e Door sensor battery H2 | Implementados | Fato observado | Especificação de variantes |

## 3. Arquitetura

`client_154/main/` compõe product firmware e board model. Os componentes
`issp_core`, `issp_behaviors`, `issp_transport_154` e `issp_app_154` formam a
plataforma compartilhada do client. `coordinator_154` mantém rádio, registry de
devices, comandos, reports e integração UART. Os alvos se conectam pelo
protocolo, não por dependência de código.

## 4. Entradas e ciclo de vida

O client inicia por `client_154/main/app_main.cpp`, configura `SmartSysApp`,
executa commissioning ou restaura a rede e entra em operação. O coordenador
inicia em `coordinator_154/main/main.c`, abre janela de ingresso, mantém o
registry e processa rádio e UART.

## 5. API pública e consumidores

`components/issp_app_154/include/SmartSysApp.h` é a fachada pública de produto.
Os headers `include/` dos demais componentes são contratos para consumidores
técnicos. `examples/issp_minimal_client` é o segundo consumidor local.

## 6. Dados e persistência

Client e coordenador usam NVS para rede e registry. O wire ISSP possui
implementações nos dois alvos. `ISSP-Report-Identity.md` especifica em Draft o
envelope v2 necessário à identidade de DATA/ACK; o protocolo integral ainda não
está consolidado em uma única fonte dedicada.

## 7. Integrações e protocolos

- ISSP sobre IEEE 802.15.4 entre client e coordenador;
- JSON-lines sobre UART entre coordenador e host;
- Kconfig/CMake para seleção estática de produto e board.

## 8. Falhas, segurança e recuperação

Commissioning e factory reset tratam recuperação do client. Persistência do
registry trata reinicialização do coordenador. A identidade de report entre
boots e sua deduplicação estão preparadas em `ISSP-Report-Identity.md`;
implementação e validação ainda permanecem em `EKM-GAP-0006`.

## 9. Build, testes e operação

Os projetos usam ESP-IDF 6.0.1. Guards vinculam client a H2 e coordenador a C6.
As suítes versionadas permanecem preservadas, mas não são executadas sem ordem
normativa futura. Hardware real é evidência material quando solicitado.

## 10. Domínios e especificações normativas

| Domínio | Fonte | Cobertura | Observação |
|---|---|---|---|
| Arquitetura ISSP | `ISSP-Architecture.md` | Especificado | Runtime client |
| Commissioning | `ISSP-Commissioning.md` | Especificado | Client e coordenador |
| Bootstrap | `ISSP-Configurable-Bootstrap.md` | Especificado | Fachada pública |
| Registry | `ISSP-Coordinator-Paired-Device-Registry.md` | Especificado | Coordenador |
| Variantes | `Firmware-Variants-Menuconfig.md` | Especificado | Produto e board |
| Targets e testes | `Repository-Test-Execution-Policy.md` | Especificado | Repositório inteiro |
| Deep sleep do client | `Client-Deep-Sleep.md` | v0.10 e v0.11 implementadas em código; build H2 executado na v0.11; AC-012 pendente de hardware | Client a bateria |
| Identidade de reports | `ISSP-Report-Identity.md`; ADR-0004 | v0.2 Proposed e Pronta; análise v0.2 Ready; autorização de implementação ausente | Client, protocolo, coordenador e ponte UART |

## 11. Riscos, legado e preparação futura

O projeto ESP-IDF na raiz ainda é um protótipo não classificado. O protocolo
wire integral continua incompletamente especificado; o recorte v2 para
identidade de report possui fonte própria em Draft. Registros da EKM 1.x são
históricos.

## 12. Questões abertas

| ID | Questão | Impacto | Destino |
|---|---|---|---|
| `EKM-GAP-0002` | Contrato wire integral ainda não consolidado; recorte v2 de report está em Draft | Compatibilidade entre alvos | `ISSP-Report-Identity.md` e recorte futuro integral |
| `EKM-GAP-0003` | Matriz requisito–evidência incompleta | Navegação de validação | Recorte futuro |
| `EKM-GAP-0006` | Identidade e ACK v2 ainda não implementados nem validados | Confiabilidade operacional | `ISSP-Report-Identity.md` |

## Regra de manutenção

Este dossiê oferece visão factual e navegação. Contratos detalhados permanecem
nas especificações e ADRs apontadas pelo mapa.
