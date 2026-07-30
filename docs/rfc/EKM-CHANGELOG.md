# EKM — Histórico de Mudanças do Conhecimento

**Tipo:** Operacional
**Status:** Active
**Versão:** 1.3
**Responsável:** Marcelo Miranda
**Última atualização:** 23/07/2026
**Escopo:** Todo o repositório

---

## 1. Objetivo

Registrar o ciclo de mudanças relevantes no conhecimento do projeto, incluindo
seu estado, ativos afetados, critérios de encerramento e evidências.

Este histórico não substitui Git, especificações, RFCs, ADRs ou o mapa de
conhecimento. Ele indica se a transação de conhecimento correspondente está
aberta, encerrada, bloqueada ou substituída.

---

## 2. Estados

- `Open`: mudança iniciada e ainda incompleta.
- `Closed`: critérios atendidos, evidências registradas e dependentes
  consistentes.
- `Blocked`: depende de decisão, autoridade ou evidência externa.
- `Superseded`: substituída por outro registro indicado explicitamente.

Somente uma transação que satisfaça a Definition of Done EKM pode ser marcada
como `Closed`.

---

## EKM-CHG-0001 — Instituição da governança EKM

**Status:** `Closed`
**Tipo:** Criação de governança
**Aberta em:** 21/07/2026
**Encerrada em:** 21/07/2026

### Motivação

Separar especificações de comportamento das diretrizes permanentes de
implementação e preservação do conhecimento.

### Ativos afetados

- `AGENTS.md`;
- `.github/copilot-instructions.md`;
- `docs/governance/EKM-GUIDELINES.md`;
- `docs/governance/KNOWLEDGE-MAP.md`.

### Critérios de encerramento

- instruções canônicas disponíveis no repositório;
- especificação preservada como unidade principal de implementação;
- fontes de verdade classificadas e mapeadas;
- proteção e relatório de mudanças normativas definidos.

### Evidências

- arquivos de governança criados e relidos integralmente;
- `git diff --check` aprovado.

---

## EKM-CHG-0002 — Restauração da arquitetura ISSP

**Status:** `Closed`
**Tipo:** Correção de conhecimento
**Aberta em:** 21/07/2026
**Encerrada em:** 21/07/2026

### Motivação

A consolidação removeu conhecimento normativo de
`client_154/docs/ISSP-Architecture.md` sem tornar essa perda visível no
relatório de execução.

### Ativos afetados

- `client_154/docs/ISSP-Architecture.md`;
- `docs/governance/KNOWLEDGE-MAP.md`;
- este histórico.

### Critérios de encerramento

- conhecimento arquitetural vigente restaurado;
- atualizações legítimas da consolidação preservadas;
- conteúdo validado estaticamente contra a implementação;
- lacuna `EKM-GAP-0001` encerrada no mapa;
- relatório EKM auditado.

### Evidências

- `ISSP-Architecture.md` v1.1;
- comparação com a versão anterior no Git e com
  `ISSP-Consolidation.md`;
- auditoria contra componentes e composição atuais;
- `git diff --check` aprovado.

---

## EKM-CHG-0003 — Consistência global e ciclo das mudanças EKM

**Status:** `Closed`
**Tipo:** Evolução da governança
**Aberta em:** 21/07/2026
**Encerrada em:** 21/07/2026

### Motivação

A primeira restauração orientada pela EKM corrigiu o documento principal, mas
não atualizou o mapa nem encerrou sua lacuna. A governança precisava exigir
consistência entre ativos dependentes, não apenas conformidade local.

### Ativos afetados

- `AGENTS.md`;
- `.github/copilot-instructions.md`;
- `docs/governance/EKM-GUIDELINES.md`;
- `docs/governance/KNOWLEDGE-MAP.md`;
- este histórico.

### Critérios de encerramento

- análise de impacto documental definida;
- transação de conhecimento definida;
- Definition of Done EKM definida;
- mudanças e lacunas possuem IDs e estados;
- instruções de assistentes e mapa atualizados;
- restauração arquitetural refletida de forma consistente.

### Evidências

- EKM Guidelines v1.1;
- Knowledge Map v1.1;
- histórico EKM criado;
- adaptadores de Codex e Copilot atualizados;
- revisão do diff documental e `git diff --check` aprovados.

---

## EKM-CHG-0004 — Componentes ISSP reutilizáveis

**Status:** `Closed`
**Tipo:** Evolução arquitetural e empacotamento
**Aberta em:** 21/07/2026
**Encerrada em:** 21/07/2026

### Motivação

Comprovar o objetivo original da refatoração: permitir que firmwares ESP-IDF
consumam a stack ISSP sem copiar o firmware `client_154` ou manter variantes do
mesmo projeto.

### Ativos afetados previstos

- componentes ISSP atualmente em `client_154/components`;
- configuração CMake do `client_154`;
- novo consumidor mínimo em `examples/issp_minimal_client`;
- `client_154/docs/ISSP-Reusable-Components.md`;
- `client_154/docs/ISSP-Architecture.md`;
- `docs/governance/KNOWLEDGE-MAP.md`;
- `components/README.md`;
- este histórico.

### Critérios de encerramento

- componentes movidos para localização compartilhada sem duplicação;
- client existente consumindo o pacote compartilhado;
- segundo consumidor independente compilando e fazendo link;
- contratos públicos, privados e dependências documentados;
- builds do client, exemplo e coordenador aprovados;
- `EKM-GAP-0004` encerrada ou escopo residual separado explicitamente;
- transação EKM e relatório completos.

### Evidências atuais

- componentes movidos para `components/` sem fontes duplicadas;
- `client_154` e `examples/issp_minimal_client` localizam os componentes por
  `EXTRA_COMPONENT_DIRS` e compilam para ESP32-H2 com ESP-IDF 6.0.1;
- `coordinator_154` compila para ESP32-C6 com ESP-IDF 6.0.1;
- contratos públicos, privados e dependências registrados em
  `components/README.md` e na especificação;
- integridade dos binários registrada por tamanho e SHA-256 no relatório de
  execução;
- buscas estruturais, comparação de conteúdo movido, revisão do diff e
  `git diff --check` aprovados;
- `EKM-GAP-0004` encerrada no mapa.

### Reabertura em 21/07/2026

A auditoria identificou que cinco fontes possuíam alterações preexistentes não
commitadas antes da movimentação, mas os arquivos compartilhados resultantes
ficaram idênticos ao `HEAD`. A comparação contra o `HEAD` não comprova
preservação do worktree real e tornou inválido o encerramento anterior.

O registro permanece `Open` até que as cinco alterações sejam recuperadas
exatamente, aplicadas na nova localização e comparadas com evidência do
worktree inicial. Os três builds e as demais validações devem ser repetidos
depois da recuperação. `EKM-GAP-0004` foi reaberta no mapa.

### Reencerramento em 21/07/2026

O histórico local da execução de consolidação identificou as cinco fontes e
preservou seus diffs. A cronologia comprovou que esses pós-diffs foram
registrados imediatamente antes do início da reutilização e que o primeiro
`git status` da tarefa estava limpo. A comparação dos objetos registrados com
os arquivos na nova localização produziu hashes idênticos para as cinco fontes,
sem usar o estado anterior à consolidação como baseline.

Depois da comprovação foram repetidos os builds de `client_154`,
`examples/issp_minimal_client` e `coordinator_154`, as buscas estruturais, a
revisão das referências em `ISSP-Consolidation.md` e `git diff --check`, todos
aprovados. `EKM-GAP-0004` foi encerrada novamente no mapa.

---

## EKM-CHG-0005 — Baseline e evidências não ambíguas

**Status:** `Closed`
**Tipo:** Evolução da governança
**Aberta em:** 22/07/2026
**Encerrada em:** 22/07/2026

### Motivação

A auditoria de `EKM-CHG-0004` mostrou que comparar arquivos somente com `HEAD`
não comprova preservação de alterações preexistentes. A reauditoria também usou
hashes de objetos Git e SHA-256 de arquivos e binários, exigindo identificação
explícita para evitar interpretações incorretas. Um diff editorial fora do
escopo reforçou que toda alteração final deve ser reconciliada e relatada.

### Ativos afetados

- `AGENTS.md`;
- `.github/copilot-instructions.md`;
- `docs/governance/EKM-GUIDELINES.md`;
- `docs/governance/KNOWLEDGE-MAP.md`;
- este histórico.

### Critérios de encerramento

- baseline do worktree definida como obrigatória antes de mutações;
- `HEAD` isolado explicitamente insuficiente para preservar estado local;
- reconciliação entre inventários inicial e final incorporada ao fluxo;
- alterações sem requisito impedem encerramento;
- hashes exigem objeto e algoritmo identificados;
- relatório e Definition of Done EKM atualizados;
- adaptadores de Codex e Copilot atualizados;
- documentos validados sem alterações de produto.

### Evidências

- EKM Guidelines v1.2;
- Knowledge Map v1.2;
- instruções de assistentes atualizadas;
- revisão cruzada das novas obrigações;
- `git diff --check` aprovado;
- nenhum código ou comportamento de produto alterado.

---

## EKM-CHG-0006 — Organização e ciclo de vida das especificações

**Status:** `Closed`
**Tipo:** Evolução da governança e organização documental
**Aberta em:** 22/07/2026
**Encerrada em:** 22/07/2026

### Motivação

Permitir que o sistema evolua por especificações funcionais independentes e
graduais, sem confundir a autoridade do documento com a situação atual de sua
implementação. Também reduzir a dispersão das fontes normativas no repositório.

### Ativos afetados

- `AGENTS.md` e `.github/copilot-instructions.md`;
- `docs/rfc/EKM-GUIDELINES.md`;
- `docs/rfc/KNOWLEDGE-MAP.md`;
- `docs/rfc/EKM-CHANGELOG.md`;
- `docs/rfc/MAN-0001.md`;
- especificações movidas para `docs/specs/`;
- referências documentais dependentes.

### Decisões

- `AGENTS.md` permanece na raiz para descoberta automática;
- regras, mapa, histórico e manuais ficam centralizados em `docs/rfc/`;
- especificações funcionais e técnicas ficam centralizadas em `docs/specs/`;
- toda especificação possui estado normativo e estado da implementação
  independentes;
- `Open` e `Closed` continuam reservados a transações e lacunas EKM;
- comportamento que precise ser preservado ou reconstruído deve estar
  representado em especificação normativa.

### Critérios de encerramento

- estrutura documental aplicada sem perda de conteúdo;
- referências ativas atualizadas para os novos caminhos;
- estados e transições formalizados nas diretrizes;
- especificações ativas com os dois estados explícitos;
- mapa atualizado com autoridade, implementação e evidência;
- inventário final reconciliado e `git diff --check` aprovado.

### Evidências

- EKM Guidelines v1.3;
- Knowledge Map v1.3;
- quatro especificações ISSP sob `docs/specs/`;
- buscas por referências antigas e revisão dos diffs documentais;
- `git diff --check` aprovado;
- nenhum código ou comportamento de produto alterado.

---

## EKM-CHG-0007 — Bootstrap configurável do client ISSP

**Status:** `Open`
**Tipo:** Especificação e evolução arquitetural
**Aberta em:** 23/07/2026

### Motivação

Substituir, em uma implementação futura, a orquestração repetível concentrada
em `client_154/main/main.cpp` por uma fachada configurável e compartilhada,
inspirada no modelo de configuração antes de `setup()` da `IoTSmartSysCore`,
sem importar suas dependências nem reimplementar factory reset ou report
inicial.

### Ativos afetados nesta etapa

- nova `docs/specs/ISSP-Configurable-Bootstrap.md`;
- `docs/rfc/KNOWLEDGE-MAP.md`;
- este histórico.

### Decisões

- a especificação permanece `Proposed`, `Not Started`, `Not Ready` e
  `Pending Review`;
- o componente futuro, ainda inexistente, permanece planejado como
  `components/issp_app_154`, mas seu entrypoint público será
  `iotsmartsys::SmartSysApp`;
- a API comum configura firmware e capabilities sem expor nomes ISSP ou
  IEEE 802.15.4 em seus tipos de entrada;
- a primeira capability pública é `SwitchPlugCapability`, criada e possuída
  pela fachada a partir de `SwitchConfig`;
- `deviceId`, `endpointId` e `eventType` permanecem configuráveis;
- o short address `0x1001`, a leitura do endereço IEEE e a política NVS vigente
  ficam internos à fachada nesta versão;
- factory reset reutiliza os serviços existentes, com realocação permitida
  somente para eliminar dependência reversa;
- o report inicial continua pertencendo ao behavior e ao executor existentes;
- protocolo, persistência, commissioning e comportamento funcional permanecem
  inalterados;
- identidade automática, atribuição de short address e abstração integral do
  protocolo ficam explicitamente adiadas.

### Critérios de encerramento

- especificação aprovada pelo responsável arquitetural;
- componente e migração implementados conforme todos os requisitos;
- factory reset, report inicial, wire e persistência preservados;
- testes automatizados, três builds e checklist de hardware aprovados;
- arquitetura, componentes reutilizáveis, mapa e evidências reconciliados;
- Definition of Done EKM respondida integralmente.

### Evidências atuais

- inspeção da composição funcional em `client_154/main/main.cpp` e dos
  componentes `issp_*`;
- inspeção do modelo de `SmartSysApp`, `ConnectivityBootstrap` e
  `CORE-RUNTIME-LIFECYCLE.md` na `IoTSmartSysCore`;
- a validação arquitetural posterior identificou lacunas na máquina de estados,
  no lifetime da fachada, na classificação das dependências e na distinção
  entre solução proposta e implementação existente;
- especificação v1.2 corrigida pelo Autor com transição direta
  `Configuring → Failed`, duração estática obrigatória para toda fachada em que
  `setup()` seja chamado e contratos explícitos de destruição;
- classificação de `esp_driver_gpio` como dependência pública e das demais
  dependências como privadas registrada somente como proposta, pendente de
  validação independente pelo Engenheiro Analista;
- especificação novamente deixada como `Proposed`, `Not Started`, `Not Ready`
  e `Pending Review`; a implementação de `components/issp_app_154` permanece
  inexistente;
- decisões futuras de identidade, short address, endereçamento de capabilities
  e multiprotocolo preservadas fora do recorte;
- nenhuma implementação, factory reset, report inicial, wire ou persistência
  alterados nesta etapa.
