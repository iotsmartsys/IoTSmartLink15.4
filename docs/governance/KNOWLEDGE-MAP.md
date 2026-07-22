# EKM — Mapa das Fontes de Verdade

**Tipo:** Normativo
**Status:** Active
**Versão:** 1.2
**Responsável:** Marcelo Miranda
**Última atualização:** 22/07/2026
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
| Diretrizes EKM | `docs/governance/EKM-GUIDELINES.md` | Normativo | Active v1.2; baseline e reconciliação obrigatórias em `EKM-CHG-0005` |
| Mapa de conhecimento | `docs/governance/KNOWLEDGE-MAP.md` | Normativo | Active |
| Histórico de mudanças EKM | `docs/governance/EKM-CHANGELOG.md` | Operacional | Active |
| Instruções do Copilot | `.github/copilot-instructions.md` | Adaptador | Active |

Os arquivos de instrução de ferramentas são adaptadores. Em caso de diferença,
`EKM-GUIDELINES.md` é a fonte canônica das regras EKM.

---

## 3. ISSP client e IEEE 802.15.4

| Área | Fonte normativa | Implementação principal | Evidência atual | Estado |
|---|---|---|---|---|
| Arquitetura ISSP | `client_154/docs/ISSP-Architecture.md` | `components/issp_*` e `client_154/main/main.cpp` | Builds, validação em hardware, consumidor mínimo e auditoria documental | Active; reutilização concluída e reauditada em `EKM-CHG-0004` |
| Commissioning | `client_154/docs/ISSP-Commissioning.md` | `Issp154NetworkManager`, transporte e coordenador | Cenários registrados na própria especificação e testes em hardware | Implemented and validated |
| Consolidação | `client_154/docs/ISSP-Consolidation.md` | Client e coordenador | Relatório de execução e auditoria posterior | Implemented; correções de conformidade pendentes |
| Componentes reutilizáveis | `client_154/docs/ISSP-Reusable-Components.md` | `components/issp_*` | Dois consumidores compilando; equivalência das cinco alterações preexistentes comprovada | Implemented and validated; `EKM-CHG-0004` Closed |
| Protocolo ISSP | Arquitetura e contratos em `issp_core` | `issp_protocol.*` | Build e testes existentes | Necessita especificação wire dedicada para reconstruibilidade completa |
| Transporte IEEE 802.15.4 | Arquitetura e commissioning | `issp_transport_154` | Build e testes em hardware | Implemented |
| Behaviors | Arquitetura ISSP | `issp_behaviors` | Build e comportamento em hardware | Implemented |
| Factory reset | Commissioning e arquitetura ISSP | `client_154/main/reset/` | Teste de pressão por 10 segundos e redescoberta | Implemented and validated |
| Fluxo de comandos | Arquitetura ISSP e comportamento validado | `IsspDevice`, behavior e coordenador | ON/OFF/TOGGLE e ACK em hardware | Implemented and validated |
| Reports confirmados | Arquitetura ISSP | `IsspDevice`, `Issp154ReportExecutor` e coordenador | Report inicial, ACK e retries em hardware | Implemented and validated |

---

## 4. Documentos históricos e do método

| Documento | Tipo | Autoridade atual |
|---|---|---|
| `client_154/docs/ai-assisted-engineering/PILOT-ISSP.md` | Histórico/experimental | Registra o piloto; não substitui especificações técnicas |
| `client_154/docs/rfc/MAN-0001.md` | Conforme metadados internos | Consultar antes de alterar o método correspondente |

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

Não remover uma entrada sem indicar o destino do conhecimento correspondente.
