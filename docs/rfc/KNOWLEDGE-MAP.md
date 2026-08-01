# EKM — Mapa das Fontes de Verdade

**Tipo:** Normativo
**Status:** Active
**Versão:** 1.13
**Responsável:** Marcelo Miranda
**Última atualização:** 01/08/2026
**Escopo:** Todo o repositório

---

## 1. Objetivo

Indicar onde está o conhecimento autoritativo de cada área do projeto e como
localizar sua implementação e suas evidências.

Este mapa não duplica o conteúdo das fontes. Ele aponta para elas e registra sua
classificação e estado.

---

## 2. Governança do repositório

| Área | Fonte | Tipo | Estado |
|---|---|---|---|
| Instruções para assistentes | `AGENTS.md` | Normativo | Active |
| Diretrizes EKM | `docs/rfc/EKM-GUIDELINES.md` | Normativo | Active v1.3; ciclo de vida das especificações definido em `EKM-CHG-0006` |
| Mapa de conhecimento | `docs/rfc/KNOWLEDGE-MAP.md` | Normativo | Active |
| Histórico de mudanças EKM | `docs/rfc/EKM-CHANGELOG.md` | Operacional | Active |
| Manual da IA Executora | `docs/rfc/MAN-0001.md` | Normativo complementar | Active |
| Instruções do Copilot | `.github/copilot-instructions.md` | Adaptador | Active |

Os arquivos de instrução de ferramentas são adaptadores. Em caso de diferença,
`EKM-GUIDELINES.md` é a fonte canônica das regras EKM.

---

## 3. Especificações ISSP e IEEE 802.15.4

| Área | Fonte normativa | Estado normativo | Estado da implementação | Implementação principal | Evidência atual |
|---|---|---|---|---|---|
| Arquitetura ISSP | `docs/specs/ISSP-Architecture.md` | Active | Validated | `components/issp_*` e `client_154/main/main.cpp` | Builds, hardware, consumidor mínimo e auditoria documental |
| Commissioning | `docs/specs/ISSP-Commissioning.md` | Active | Validated | `Issp154NetworkManager`, transporte e coordenador | Cenários da especificação e testes em hardware |
| Registry de devices pareados do coordenador | `docs/specs/ISSP-Coordinator-Paired-Device-Registry.md` | Proposed | In Progress; Not Ready; Implementable | `coordinator_154/main/device_registry.{h,c}` + `device_registry_nvs.c`, integrado em `main.c` | v0.3 analisada como `Implementable` em 01/08/2026: integridade obrigatória e mutações independentes fecham AC-007; G3-F atravessa o adaptador de produção sob falha de commit; baseline permanece não conforme e sem aceite |
| Consolidação | `docs/specs/ISSP-Consolidation.md` | Active | Validated | Client e coordenador | Relatório de execução e auditoria posterior |
| Componentes reutilizáveis | `docs/specs/ISSP-Reusable-Components.md` | Active | Validated | `components/issp_*` | Dois consumidores compilando e equivalência do worktree comprovada |
| API `SmartSysApp` e bootstrap configurável | `docs/specs/ISSP-Configurable-Bootstrap.md` | Active | Validated | `components/issp_app_154`; `client_154/main.cpp` e `examples/issp_minimal_client` migrados | Quatro builds sem warnings; 19/19 testes QEMU; validação e aceite humanos em hardware; risco de ACK/retry separado em `EKM-GAP-0006` |
| Protocolo wire ISSP | Especificação dedicada ainda inexistente | — | Blocked | `issp_protocol.*` | Lacuna `EKM-GAP-0002` |
| Factory reset | Requisitos distribuídos em commissioning e arquitetura | Active | Validated | `components/issp_app_154/{include,src}/reset/` (realocado de `client_154/main/reset/` por `EKM-CHG-0007`, sem mudança funcional) | Pressão por 10 segundos e redescoberta em hardware |
| Fluxo de comandos | `docs/specs/ISSP-Architecture.md` | Active | Validated | `IsspDevice`, behavior e coordenador | ON/OFF/TOGGLE funcionais; confiabilidade residual de ACK em `EKM-GAP-0006` |
| Reports confirmados | `docs/specs/ISSP-Architecture.md` | Active | Validated | `IsspDevice`, executor e coordenador | Report inicial funcional; turnaround, confirmação e sequência entre retries em `EKM-GAP-0006` |

---

## 4. Documentos históricos e do método

| Documento | Tipo | Autoridade atual |
|---|---|---|
| `client_154/docs/ai-assisted-engineering/PILOT-ISSP.md` | Histórico/experimental | Registra o piloto; não substitui especificações técnicas |
| `docs/rfc/MAN-0001.md` | Normativo complementar | Consultar antes de alterar o método correspondente |

Documentos históricos não devem ser atualizados para simular que decisões
passadas já refletiam o estado atual.

---

## 5. Lacunas conhecidas de conhecimento

| ID | Estado | Lacuna | Critério de encerramento | Evidência ou dependência |
|---|---|---|---|---|
| `EKM-GAP-0001` | `Closed` | Restaurar as decisões vigentes removidas de `ISSP-Architecture.md` durante a consolidação | Conteúdo restaurado, validado contra implementação e mapa atualizado | `ISSP-Architecture.md` v1.1 e `EKM-CHG-0002` |
| `EKM-GAP-0002` | `Open` | Criar especificação dedicada do protocolo wire ISSP | Layout, tipos, checksum, endianness e compatibilidade definidos e validados | Requer recorte próprio |
| `EKM-GAP-0003` | `Open` | Mapear requisitos estáveis para testes automatizados e de hardware | Matriz requisito–evidência vigente | Requer recorte próprio |
| `EKM-GAP-0004` | `Closed` | Registrar contratos públicos, provar reutilização local e comprovar preservação do worktree inicial | APIs, dependências e compatibilidade documentadas; segundo consumidor compilando; cinco alterações preexistentes recuperadas e equivalência comprovada contra o worktree inicial | `ISSP-Reusable-Components.md`, `components/README.md` e `EKM-CHG-0004` |
| `EKM-GAP-0005` | `Open` | Definir preservação dos relatórios de validação relevantes | Localização, retenção e autoridade definidas | Requer decisão operacional |
| `EKM-GAP-0006` | `Open` | Tornar confiável o enlace confirmado entre client e coordenador | Estado explícito `TX → RX pronto`; resultado do comando coerente com a atuação; ACKs confirmados sem timeout espúrio; mesma identidade lógica preservada entre retries; cenário repetido em hardware sem eventos duplicados para o host | Evidência observada no fechamento de `EKM-CHG-0007`; requer especificação própria de transporte/ACK |

Uma lacuna registrada não autoriza o assistente a preenchê-la por suposição.

---

## 6. Regra de manutenção

Atualizar este mapa quando:

- uma nova fonte normativa for criada;
- um documento for substituído, arquivado ou mudar de autoridade;
- um componente assumir ou perder responsabilidade;
- uma nova evidência se tornar obrigatória;
- uma lacuna de conhecimento for identificada ou encerrada.
- uma mudança EKM alterar o estado ou a autoridade de uma fonte.
- uma especificação mudar de estado normativo ou de estado da implementação.

Não remover uma entrada sem indicar o destino do conhecimento correspondente.
