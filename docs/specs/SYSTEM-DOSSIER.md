# Dossiê do Sistema — IoTSmartLink15.4

**Tipo:** Informativo com decisões confirmadas identificadas

**Estado da fonte:** Vigente [`Active`]

**Última auditoria:** 2026-08-11

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
| Single smart plug e Door sensor | Implementados | Fato observado | Especificação de variantes |

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
implementações nos dois alvos, mas ainda não tem especificação dedicada.

## 7. Integrações e protocolos

- ISSP sobre IEEE 802.15.4 entre client e coordenador;
- JSON-lines sobre UART entre coordenador e host;
- Kconfig/CMake para seleção estática de produto e board.

## 8. Falhas, segurança e recuperação

Commissioning e factory reset tratam recuperação do client. Persistência do
registry trata reinicialização do coordenador. A confiabilidade residual de
ACK/retry permanece em `EKM-GAP-0006`.

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

## 11. Riscos, legado e preparação futura

O projeto ESP-IDF na raiz ainda é um protótipo não classificado. O protocolo
wire e a confiabilidade do enlace confirmado continuam incompletamente
especificados. Registros da EKM 1.x são históricos.

## 12. Questões abertas

| ID | Questão | Impacto | Destino |
|---|---|---|---|
| `EKM-GAP-0002` | Contrato wire dedicado ausente | Compatibilidade entre alvos | Especificação futura |
| `EKM-GAP-0003` | Matriz requisito–evidência incompleta | Navegação de validação | Recorte futuro |
| `EKM-GAP-0006` | ACK/retry residual | Confiabilidade operacional | Especificação futura |

## Regra de manutenção

Este dossiê oferece visão factual e navegação. Contratos detalhados permanecem
nas especificações e ADRs apontadas pelo mapa.
