# EKM — Histórico de Mudanças do Conhecimento

**Tipo:** Operacional
**Status:** Active
**Versão:** 1.8
**Responsável:** Marcelo Miranda
**Última atualização:** 01/08/2026
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

**Status:** `Closed`
**Tipo:** Especificação e evolução arquitetural
**Aberta em:** 23/07/2026
**Encerrada em:** 30/07/2026

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

- a especificação permanece `Proposed` e `Not Started`; a revisão de
  implementabilidade avança de `Pending Review` para `Implementable` e a
  prontidão permanece `Not Ready` até autorização humana para implementar;
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
  alterados nesta etapa;
- Engenheiro Analista confrontou a especificação v1.2 com o estado atual do
  repositório: `components/issp_app_154` continua inexistente;
  `client_154/main/main.cpp`, `client_154/main/CMakeLists.txt`,
  `client_154/main/reset/*`, `components/issp_core`,
  `components/issp_behaviors`, `components/issp_transport_154`,
  `examples/issp_minimal_client` e `coordinator_154` foram inspecionados
  diretamente; nenhuma dependência reversa preexistente foi encontrada em
  `coordinator_154` ou `examples/issp_minimal_client`;
- a classificação pública/privada da seção 15 foi confirmada como correta
  contra os headers e `CMakeLists.txt` reais; `esp_driver_gpio` é o único
  requisito público, coerente com o precedente já aplicado em
  `components/issp_behaviors`; a ausência de `ieee802154` na tabela foi
  validada como correta, pois nenhuma fonte da fachada o inclui diretamente;
- o mapeamento `IsspResult → AppResult` da seção 8.6 foi confirmado como
  completo contra os cinco valores existentes em
  `components/issp_core/include/issp_types.hpp`;
- a decisão de criar `components/issp_app_154` como nova camada está
  fundamentada por decisão explícita do Arquiteto na seção 2, identificando o
  padrão atual (composição direta em `main.cpp`), a mudança (fachada
  compartilhada), o alcance (somente configuração e composição, sem novo
  protocolo ou lifecycle) e a justificativa (reuso sem repetir composição
  técnica), satisfazendo a exceção de preservação arquitetural das regras
  comuns;
- nenhuma decisão normativa, de produto ou de arquitetura ausente foi
  encontrada no recorte analisado; a especificação v1.3 é promovida para
  `Implementable`, preservando `Not Started` e `Not Ready`;
- observação não bloqueante registrada para a implementação futura: a
  atualização mínima de `examples/issp_minimal_client` (item 6 da seção 16)
  deve preservar a ausência de efeitos colaterais de rádio/NVS já
  deliberada nesse exemplo, comprovando apenas compilação e link da fachada;
- nenhuma implementação, factory reset, report inicial, wire ou persistência
  alterados nesta etapa; esta atuação não autoriza início de implementação;
- 30/07/2026: Arquiteto autoriza explicitamente o início da implementação da
  especificação v1.3 (`Implementable`), com resultado, recorte e requisitos
  descritos na ordem, mantendo hardware (`SMARTAPP-AC-022`) fora do recorte;
- Engenheiro Implementador cria `components/issp_app_154`
  (`iotsmartsys::SmartSysApp`), compondo por delegação `issp_core`,
  `issp_behaviors` e `issp_transport_154`, e possuindo o factory reset
  realocado de `client_154/main/reset/`; `client_154/main.cpp`,
  `client_154/main/CMakeLists.txt` e
  `examples/issp_minimal_client/main/{main.cpp,CMakeLists.txt}` migrados
  conforme a seção 8/9 da especificação; `client_154/main/reset/` removido
  (relocação sem mudança funcional); `components/README.md` atualizado;
- três builds obrigatórios (`client_154`, `examples/issp_minimal_client`,
  `coordinator_154`) compilam sem warnings em ESP-IDF v6.0.1-dirty,
  alvo `esp32h2`; tamanhos e SHA-256 registrados na especificação (seção 22.6);
- testes automatizados de configuração escritos e compilados em
  `components/issp_app_154/test_apps/smart_sys_app_test`; execução requer
  hardware ou QEMU com rádio IEEE 802.15.4, indisponíveis neste ambiente —
  permanecem não executados, evidência registrada como limitação real, não
  como validação aprovada;
- desvio sintático registrado: o exemplo normativo da seção 9 (`static
  SmartSysApp app(...)` com `using namespace iotsmartsys::app;`) não compila
  por ambiguidade entre a variável `app` e o namespace `iotsmartsys::app`; a
  implementação usa `smartSysApp` como nome de instância e não importa o
  namespace `app` via `using namespace`, preservando a intenção;
- `SMARTAPP-AC-004C` e `SMARTAPP-AC-022` permanecem pendentes, dependentes de
  hardware, conforme instrução explícita do Arquiteto (sem flash nem teste em
  hardware nesta etapa);
- estado da implementação da especificação atualizado para `In Progress`;
  `EKM-CHG-0007` permanece `Open` — fechamento, promoção a `Validated`/`Done`
  e validação em hardware não foram executados nem autorizados nesta etapa;
- 30/07/2026 (mesmo dia, correção): Arquiteto determina remover toda
  exposição de tipos/headers `issp::*` de `SmartSysApp.h`, tornar privadas as
  dependências ISSP/NVS/reset, implementar e executar testes automatizados de
  estados/ordem/`setup()` repetido/falhas/rollback sem classificá-los como
  dependentes de hardware, e adicionar build de `coordinator_154` em
  ESP32-C6;
- `SmartSysApp.h` reescrito: passa a incluir somente `<cstddef>`,
  `<cstdint>` e `driver/gpio.h`; `SmartSysApp` guarda todo o estado atrás de
  `struct Impl` incompleto, materializado por placement-new num buffer fixo
  (`SmartSysApp::kImplStorageBytes`, sem alocação dinâmica);
  `core::SwitchPlugCapability::state()` passa a usar um par
  função-ponteiro/contexto opaco, mesmo padrão já usado por
  `IsspDevice::CommandHandler`;
- `Impl` dividido em `src/smart_sys_app.cpp` (máquina de estados de
  `setup()`, sem dependência de rádio) e `src/smart_sys_app_hardware.cpp`
  (NVS, transporte, network manager, device, executor e reset reais),
  compartilhando `src/smart_sys_app_impl.hpp` (privado);
  `components/issp_app_154/CMakeLists.txt` só compila
  `smart_sys_app_hardware.cpp`/`src/reset/*.cpp` e só requer
  `issp_transport_154`/`nvs_flash`/`esp_timer`/`esp_hw_support` quando o alvo
  é `esp32h2` ou `esp32c6`; em outros alvos `issp_app_154` só oferece o
  construtor de dois argumentos com `SmartSysApp::SetupHooks` (seam de
  teste, aditivo e não normativo);
- 19 testes automatizados (configuração, estados, ordem de inicialização,
  `setup()` repetido, falhas injetadas em cada etapa e rollback) escritos em
  `components/issp_app_154/test_apps/smart_sys_app_test`, usando apenas
  `SmartSysApp::SetupHooks` com fakes — nenhum toca NVS, GPIO real ou
  rádio — e **executados** sob QEMU (`qemu-riscv32`
  `esp_develop_9.2.2_20250817`, alvo `esp32c3`, instalado via
  `idf_tools.py install qemu-riscv32`; dependências de biblioteca dinâmica
  do macOS instaladas via Homebrew neste ambiente de desenvolvimento):
  **19/19 `PASS`, 0 `FAIL`**;
- quatro builds sem warnings: `client_154` (esp32h2),
  `examples/issp_minimal_client` (esp32h2), `coordinator_154` (esp32c6, novo
  nesta correção), app de teste (esp32c3); nenhum sdkconfig versionado de
  `client_154`/`coordinator_154` foi alterado (builds isolados via
  `-DSDKCONFIG`); tamanhos e SHA-256 registrados na especificação
  (seção 22.6);
- `SMARTAPP-AC-001` agora satisfeito por inspeção direta do header público;
  `SMARTAPP-AC-006` a `AC-013` e a parte hardware-independente de
  `SMARTAPP-AC-020` passam de "não executado" para "executado, 19/19 PASS";
- `SMARTAPP-AC-004C` e `SMARTAPP-AC-022` permanecem pendentes (hardware
  físico), conforme instrução explícita do Arquiteto ("não execute testes em
  hardware");
- `EKM-CHG-0007` permanece `Open` — fechamento, promoção a `Validated`/`Done`
  e validação em hardware físico não foram executados nem autorizados nesta
  etapa.
- em atuação humana posterior, o Arquiteto validou o firmware em hardware com
  `client_154` no ESP32-H2 e `coordinator_154` no ESP32-C6, declarou a
  implementação funcional e autorizou o fechamento da especificação;
- os logs de hardware comprovam carregamento de rede persistida, transição para
  `Running`, criação e recepção do report inicial e atuação por comandos;
- os mesmos logs revelam perda intermitente de ACK nos dois sentidos e criação
  de nova sequência para retries externos do mesmo report. O Arquiteto aceitou
  esse risco como preexistente e fora da fachada, sem declará-lo resolvido;
- a confiabilidade de turnaround, ACK e identidade de retries foi separada em
  `EKM-GAP-0006`, com critério de encerramento próprio;
- `ISSP-Configurable-Bootstrap.md` foi promovida para `Active`, `Validated` e
  `Ready`; o mapa foi reconciliado e a Definition of Done EKM respondida;
- `EKM-CHG-0007` foi encerrada por decisão do Arquiteto. O encerramento cobre a
  fachada configurável e não constitui validação de uma correção do transporte.

---

## EKM-CHG-0008 — Registry persistente de devices pareados do coordenador

**Status:** `Open`
**Tipo:** Especificação funcional
**Aberta em:** 31/07/2026

### Motivação

O coordenador mantém até oito devices somente em RAM, perde os destinos de
comando ao reiniciar e permite que `DATA` de origem desconhecida alimente essa
tabela mesmo depois do fechamento da janela. A especificação de commissioning
já exige preservar e atender devices registrados, mas não define seu registry.

### Ativos afetados nesta etapa

- nova `docs/specs/ISSP-Coordinator-Paired-Device-Registry.md`;
- `docs/rfc/KNOWLEDGE-MAP.md`;
- este histórico.

### Decisões confirmadas pelo Arquiteto

- pareamento decorre de discovery válido durante a janela e só é respondido
  como sucesso depois da confirmação NVS;
- endereço IEEE é a identidade primária e `device_id` também é persistido;
- o limite permanece em oito, sem eviction;
- mesmo endereço atualiza `device_id`; repetição idêntica é idempotente;
- `last_seq` permanece volátil;
- tráfego operacional depois do fechamento exige device persistido;
- falha de persistência não conclui pareamento;
- ausência ou schema incompatível inicia registry vazio; corrupção e erro real
  não apagam automaticamente toda a NVS;
- remoção, reset do coordenador, autenticação e migração automática ficam fora
  deste recorte.

### Resultado da autoria

- schema lógico, estados de carga, transação atômica, capacidade, falhas,
  autorização de tráfego e compatibilidade de implantação especificados;
- treze requisitos mapeados para oito critérios falsificáveis;
- efeitos de rádio, protocolo, janela e deduplicação preservados fora da
  mudança necessária;
- nenhuma implementação ou validação funcional executada;
- especificação deixada como `Proposed`, `Not Started`, `Not Ready` e
  `Pending Review`.

### Revisão de implementabilidade (Engenheiro Analista, 31/07/2026)

- confrontados os treze requisitos, as fontes arquiteturais locais
  (`ISSP-Commissioning.md`, `ISSP-Architecture.md`) e o estado real de
  `coordinator_154/main/main.c` e `iot154_packet.h`; nenhuma divergência
  material encontrada entre os fatos descritos na especificação e o firmware
  atual;
- nenhuma decisão normativa, de produto ou de arquitetura ausente identificada;
  a solução proposta não introduz nova camada de domínio e delimita a
  abstração interna de NVS necessária para testes;
- observação registrada para o Implementador: `coordinator_154` ainda não tem
  componente nem `test_apps` próprios; o precedente mais próximo para o gate
  automatizado com NVS substituível é `components/issp_app_154`
  (`SetupHooks` + `test_apps` sob QEMU);
- resultado: `Implementable`. Detalhe completo em
  `docs/specs/ISSP-Coordinator-Paired-Device-Registry.md` seção 16.1.

### Critérios de encerramento da transação

- revisão independente promove a especificação para `Implementable` — **satisfeito** em 01/08/2026 para v0.3; registros de v0.1/v0.2 permanecem históricos;
- Arquiteto autoriza implementação;
- requisitos COORD-REG-001 a COORD-REG-013 são implementados;
- gates automatizados e cenários de hardware AC-001 a AC-008 terminam com
  evidência aprovada;
- mapa, especificações relacionadas e transação são reconciliados;
- implantação em ambiente real permanece sujeita a ordem própria.

### Registro do Engenheiro Implementador (31/07/2026)

- Arquiteto autoriza implementação com recorte de escopo completo (COORD-REG-001
  a 013) nesta etapa;
- criados `coordinator_154/main/device_registry.h`/`.c` (schema versionado,
  validação de carga, transação de pareamento create/update/known/rejected,
  seam de storage isolando NVS da lógica de pareamento) e
  `device_registry_nvs.c` (implementação real do seam sobre `nvs.h`, namespace
  próprio `coord_reg`, chave `devices`, `nvs_set_blob`+`nvs_commit` para
  substituição atômica do blob, seguindo o precedente de
  `issp154_network_manager.cpp`);
- `coordinator_154/main/main.c` integrado: `device_registry_load()` executa
  antes de `iot154_radio_start_rx()` (seção 7); a tabela volátil `s_devices[8]`
  e `is_duplicate()`/`find_device_by_ext_addr()` (que aprendiam qualquer
  origem, inclusive por `DATA`, com eviction cega do slot 0) foram removidas;
  `DISCOVERY_REQ` passa a chamar `device_registry_pair()` e só responde sucesso
  após o commit; `DATA` de origem desconhecida com janela fechada é descartado
  sem ACK/evento/gravação (COORD-REG-007/008); `ACK` de comando passa a exigir
  também que a origem seja conhecida e o `device_id` corresponda à entrada
  persistida; comando do host passa a resolver destino via
  `device_registry_find()`; deduplicação de `last_seq` continua volátil, agora
  em cache por slot do registry, nunca no blob persistido (COORD-REG-009);
- comportamento de `DATA` de origem desconhecida com janela **aberta**
  permanece fora do recorte da especificação (seção 9); a implementação
  preserva o processamento anterior (evento + ACK) sem criar entrada
  persistente, por não haver decisão normativa para alterá-lo;
- logs de `DEVICE_REGISTRY:` (seção 11) implementados; `discovery
  ignored reason=registry_unavailable` e `pairing result=failed
  reason=registry_unavailable` foram adicionados além do vocabulário mínimo da
  especificação, para tornar o estado indisponível observável (seção 7), sem
  reduzir os tokens exigidos;
- testes automatizados escritos em
  `coordinator_154/test_apps/device_registry_test` (Unity, alvo `esp32c3`,
  execução prevista via QEMU `idf.py qemu`, mesmo precedente de
  `components/issp_app_154/test_apps/smart_sys_app_test`), com um substituto de
  storage em memória que injeta falha de leitura/escrita e corrupção
  estrutural. Cobrem COORD-REG-AC-002 (falha de commit preserva a visão
  anterior e sobrevive a um "reboot" do módulo), AC-003 (capacidade cheia
  rejeita sem eviction), AC-004 (repetição idêntica não grava; mudança de
  `device_id` grava exatamente uma vez) e AC-006 (o blob serializado nunca
  contém `last_seq`), além dos estados de carga do AC-001/AC-007 (ausente,
  schema incompatível, blob truncado, contagem inválida, checksum inválido,
  erro real de leitura);
- **não executados nesta etapa**: build ESP-IDF (`esp32c6`) e os testes acima
  sob QEMU — este ambiente não tem `idf.py`/toolchain ESP-IDF instalado.
  Nenhuma evidência de compilação ou execução é reivindicada; a limitação é
  registrada como real, não como validação aprovada;
- **fora do gate automatizado desta etapa**: a parte de AC-007 que exige NVS
  real com namespace sentinela isolado (o teste escrito usa apenas o
  substituto em memória, que não exercita `device_registry_nvs.c` nem
  partição NVS real) e a totalidade de AC-001/AC-008, que a própria
  especificação (seção 13) exige como execução terminal em hardware real —
  nenhuma das duas foi executada nem simulada;
- estado de implementação da especificação atualizado para `In Progress`
  (código e testes escritos; evidência de build/execução ausente bloqueia
  `Implemented` conforme regras comuns §3.2 e perfil do Implementador);
  `EKM-CHG-0008` permanece `Open`; promoção a `Implemented`, `Validated` ou
  `Done` não ocorreu nem foi autorizada nesta etapa.

### Revisão técnica (Engenheiro Revisor, 01/08/2026)

- revisão estática do commit `2cc600c` confrontada com COORD-REG-001 a 013 e
  AC-001 a AC-008;
- build de produção ESP-IDF 6.0.1/ESP32-C6 concluído com código 0 e sem warnings
  do compilador; build do app de teste ESP32-C3 concluído com código 0; QEMU
  terminou com `10 Tests 0 Failures 0 Ignored`; `git diff --check` sem erro;
- achado alto: `DATA` é encaminhado ao host e recebe ACK com janela aberta
  quando o registry está indisponível, violando o fail-closed de COORD-REG-011
  e AC-007;
- achado alto: `init_nvs()` ainda executa `nvs_flash_erase()` em duas falhas de
  inicialização, podendo apagar namespaces não relacionados e violando
  COORD-REG-010/AC-007;
- achado alto de evidência: faltam o gate de integração AC-005, o cenário
  completo de reboot/deduplicação de AC-006 e fakes que preservem staging,
  commit e isolamento de namespaces para AC-002/AC-007; os dez testes aprovam
  somente os cenários presentes;
- registro anterior superestimava cobertura ao mencionar contagem inválida,
  que não integra os dez testes executados; a limitação de toolchain deixou de
  se aplicar ao ambiente observado nesta revisão;
- AC-001 e AC-008 continuam sem execução terminal em hardware real; nenhuma
  validação humana nem aprovação do Arquiteto foi recebida;
- recomendação: não aceitar nem promover. `EKM-CHG-0008` permanece `Open` e a
  especificação permanece `Proposed`, `In Progress` e `Not Ready` até correção
  e nova revisão.

### Encerramento da revisão e retorno à autoria (01/08/2026)

- o Arquiteto decide preservar o escopo funcional completo e abrir uma nova
  rodada de autoria para tornar a especificação mais validável antes de
  corrigir a implementação;
- a atuação do Engenheiro Revisor é encerrada sem aceite, promoção normativa ou
  correção de código;
- estado normativo transita de `Proposed` para `Draft`; implementação permanece
  `In Progress`; prontidão permanece `Not Ready`; revisão de implementabilidade
  retorna de `Implementable` para `Pending Review`;
- a revisão de implementabilidade de 31/07/2026 permanece como registro
  histórico da versão então avaliada, mas não antecipa a análise independente
  do conteúdo que será revisado;
- achados e evidências da revisão técnica permanecem como entrada factual para
  o Autor da Especificação; `EKM-CHG-0008` permanece `Open`.

### Reautoria v0.2 (Autor da Especificação, 01/08/2026)

- Arquiteto ordena revisão integral sem redução do escopo funcional e preserva
  COORD-REG-001 a 013 e AC-001 a AC-008;
- baseline anterior, implementação corrente e contrato normativo passam a ser
  distinguidos explicitamente;
- adicionadas precedência normativa de estados e matriz de decisão cruzando
  registry, janela, identidade, mensagem e efeitos observáveis;
- falhas de inicialização NVS e operações destrutivas preexistentes passam a
  integrar explicitamente COORD-REG-010/011 e AC-007;
- contrato persistente distingue staging, durable, commit e reboot;
- gates G1 a G5 separam política integrada, backend fiel, QEMU com NVS real,
  build ESP32-C6 e hardware; cada AC declara evidência terminal mínima;
- substitutos devem modelar namespaces, sentinela, falhas por etapa e descarte
  de staging no reboot; fake parcial não pode sustentar AC completo;
- manifesto AC–teste–gate–resultado, diagnóstico ambiental e varreduras de
  conformidade tornam-se saída obrigatória da implementação;
- nenhum código ou teste funcional alterado ou executado nesta autoria;
  especificação v0.2 fica `Proposed`, implementação `In Progress`, prontidão
  `Not Ready` e revisão de implementabilidade `Pending Review`;
- `EKM-CHG-0008` permanece `Open`; próxima etapa é análise independente de
  implementabilidade da versão 0.2.

### Revisão de implementabilidade v0.2 (Engenheiro Analista, 01/08/2026)

- confrontados COORD-REG-001 a 013, AC-001 a AC-008, a matriz 9.1, G1–G5 e o
  contrato de substitutos da v0.2 com `ISSP-Commissioning.md`,
  `ISSP-Architecture.md` e o baseline real de `coordinator_154`;
- nenhum requisito obrigatório sem oráculo; a célula aberta de `DATA`
  desconhecido com janela aberta em `Ready` já delimita dualidade de aceite e
  proíbe persistência;
- seção 2.4 autoriza apenas abstrações locais de política/NVS; precedentes
  `device_registry*`, `test_apps/device_registry_test` e
  `components/issp_app_154/test_apps` bastam sem nova camada arquitetural;
- desvios do baseline (`nvs_flash_erase`, fail-closed incompleto de `DATA`,
  gates parciais) são débitos de implementação `In Progress`, não decisões
  ausentes;
- resultado: `Implementable` (detalhe na seção 16.6 da especificação);
- estados preservados: normativo `Proposed`, implementação `In Progress`,
  prontidão `Not Ready`; `EKM-CHG-0008` permanece `Open`;
- esta promoção não autoriza programar; correção e nova evidência dependem de
  ordem própria do Arquiteto.

### Encerramento corretivo da análise v0.2 (01/08/2026)

- revisão adversarial posterior identifica inconsistência entre o caráter
  opcional de checksum/marcador na seção 6 e sua corrupção obrigatória em
  AC-007;
- AC-002 e sua matriz exigem falha de commit apenas em G1+G2, permitindo que um
  fake correto aprove enquanto o adaptador NVS de produção trata
  `nvs_commit()` incorretamente;
- a promoção `Implementable` registrada anteriormente para v0.2 fica sem
  efeito; resultado corrente da análise: `Needs Clarification`;
- especificação devolvida ao Autor para alinhar o contrato de integridade e
  fechar o gate de falha de commit no adaptador de produção;
- COORD-REG-001 a 013, AC-001 a AC-008 e o escopo funcional permanecem
  inalterados; implementação `In Progress`, prontidão `Not Ready` e
  `EKM-CHG-0008` `Open`;
- nenhuma alteração ou execução de código, testes ou configuração ocorreu
  nesta correção; a etapa de análise fica encerrada.

### Revisão de implementabilidade independente v0.2 (Engenheiro Analista, 01/08/2026)

- ordem: Engenheiro Analista sobre a especificação v0.2 corrente; branch
  `gap0006-radio-diagnostics`, árvore limpa; conclusões 16.1/16.6/16.7 tratadas
  apenas como histórico;
- confrontados de novo o texto normativo completo, `ISSP-Commissioning.md`,
  `ISSP-Architecture.md`, baseline de `coordinator_154/main/*device_registry*`
  e `main.c`, `test_apps/device_registry_test` e o precedente QEMU de
  `issp_app_154`;
- requisitos 001–013, matriz 9.1, precedência 5.1 e seção 2.4 continuam
  suficientes para o “o quê” e o alcance arquitetural local; desvios do
  baseline (`nvs_flash_erase`, fail-closed incompleto, gates parciais) seguem
  como débito de implementação `In Progress`, não como decisão nova;
- bloqueio 1 confirmado: seção 6 torna checksum/marcador opcional, enquanto
  seção 10 e AC-007 classe 5 exigem reprovar integridade inválida;
- bloqueio 2 confirmado: AC-002/G1+G2 não obrigam atravessar
  `device_registry_nvs.c` sob falha de `nvs_commit()` após staging;
- resultado: `Needs Clarification` (detalhe na seção 16.8 da especificação);
  estados preservados `Proposed` / `In Progress` / `Not Ready`;
  `EKM-CHG-0008` permanece `Open`;
- especificação permanece com o Autor; nenhuma autorização de implementação;
  nenhum código, teste ou configuração de implementação alterado ou executado.

### Reautoria v0.3 (Autor da Especificação, 01/08/2026)

- Arquiteto ordena aplicar os dois ajustes devolvidos pela análise v0.2 sem
  reduzir COORD-REG-001 a 013, AC-001 a AC-008 ou o escopo funcional;
- schema passa a exigir valor determinístico de integridade cobrindo versão,
  contagem e todos os bytes das entradas; marcador constante isolado não
  satisfaz o contrato;
- seção 10 e AC-007 passam a reprovar ausência, truncamento ou divergência do
  valor obrigatório de integridade; mutações independentes de endereço e
  `device_id` comprovam cobertura do conteúdo funcional;
- G3 é explicitado como G3-N para NVS real nominal e G3-F para executar o
  próprio `device_registry_nvs.c` sob falha controlada de primitiva;
- AC-002 passa a exigir G1+G2+G3-F, com `nvs_set_blob()` bem-sucedido seguido
  de erro de `nvs_commit()`, propagação do erro, nenhuma resposta/publicação,
  durable anterior após reabertura/reboot e sentinela preservada;
- nenhum código, teste ou configuração de implementação alterado ou executado
  nesta autoria;
- versão 0.3 fica `Proposed`, implementação existente `In Progress`, prontidão
  `Not Ready`, revisão `Pending Review` e `EKM-CHG-0008` `Open`; próxima etapa
  é nova análise independente de implementabilidade.

### Revisão de implementabilidade v0.3 (Engenheiro Analista, 01/08/2026)

- confrontados COORD-REG-001 a 013, AC-001 a AC-008, matrizes, fontes de
  commissioning/arquitetura, baseline do coordenador e precedentes de hooks e
  QEMU;
- integridade obrigatória e corrupções independentes de endereço/`device_id`
  impedem aprovação por marcador constante ou cobertura parcial;
- AC-002/G3-F executa o próprio `device_registry_nvs.c` e observa staging,
  falha de commit, propagação do erro, ausência de resposta/publicação,
  durable anterior e sentinela; G3-N preserva a prova nominal com NVS real;
- seção 2.4 delimita padrão atual, mudança local, alcance e justificativa do
  seam, sem nova camada transversal ou conflito com as especificações vigentes;
- resultado: `Implementable`; nenhuma decisão normativa, de produto ou
  arquitetura ausente para o recorte completo;
- implementação permanece `In Progress` e não aceita; normativo `Proposed`,
  prontidão `Not Ready`, `EKM-CHG-0008` `Open`;
- nenhuma implementação ou teste alterado ou executado; nova ordem do Arquiteto
  é necessária antes de programar.

### Atuação corretiva do Engenheiro Implementador v0.3 (01/08/2026)

- removido o apagamento global automático da NVS em `init_nvs()`; falhas de
  inicialização impedem o início do rádio e do tráfego de devices;
- a precedência `RegistryUnavailable` foi aplicada a discovery, DATA, ACK e
  comando do host; os caminhos indisponíveis não confirmam pareamento, não
  emitem ACK/evento nem transmitem comando;
- `device_registry_nvs.c` passou a ser exercitado por hook restrito ao build de
  teste, incluindo `nvs_set_blob()` bem-sucedido seguido de falha controlada de
  `nvs_commit()`;
- Unity/QEMU ESP32-C3 terminou com `13 Tests 0 Failures 0 Ignored`; o teste
  observa durable anterior, staging descartado e sentinela inalterada em
  falhas de set/commit, inclusive através do adaptador de produção;
- build limpo temporário ESP-IDF 6.0.1/ESP32-C6 terminou com código 0 e gerou
  `central_154.bin`; a varredura de `coordinator_154/main` não encontrou
  `nvs_flash_erase()` nem outra operação NVS de apagamento;
- os resultados são parciais: faltam G1, G3-N e G5 e os critérios que exigem
  rádio/host ou hardware não foram aprovados. A especificação permanece
  `Proposed`/`In Progress`/`Not Ready` e esta transação permanece `Open`.

### Revisão técnica da correção v0.3 (Engenheiro Revisor, 01/08/2026)

- revisado integralmente o commit `a739be0` contra COORD-REG-001 a 013,
  AC-001 a AC-008 e gates G1 a G5;
- build limpo ESP-IDF 6.0.1/ESP32-C6 terminou com código 0 e gerou
  `central_154.bin` de `0x45b00` bytes; build ESP32-C3 e QEMU terminaram com
  código 0 e `13 Tests 0 Failures 0 Ignored`;
- confirmado que não há mais operação de apagamento NVS em
  `coordinator_154/main` e que os cenários escritos de core/adaptador passam;
- achado alto: permanecem ausentes G1, G3-N e G5; AC-005 não possui cenário e
  AC-003/004/006/007 mantêm cobertura parcial relevante;
- achado médio: `start_host_command()` ainda converte indisponibilidade em
  alvo desconhecido e não preserva a precedência normativa de
  `RegistryUnavailable`, embora impeça a transmissão;
- achado médio: quatro testes parciais usam os rótulos integrais `[AC-002]`,
  `[AC-003]`, `[AC-004]` e `[AC-006]`, contrariando o manifesto de evidências;
- G3-F atravessa o adaptador e comprova persistência sob falha, mas permanece
  parcial porque não observa ausência de resposta/publicação em G1;
- recomendação: não aceitar nem promover. Estados permanecem `Proposed`,
  `In Progress` e `Not Ready`; `EKM-CHG-0008` permanece `Open`.

### Revisão de implementabilidade v0.4 (Engenheiro Analista, 01/08/2026)

- confrontados integralmente COORD-REG-001 a 013, AC-001 a AC-008, matriz de
  decisão, gates G1 a G5, substitutos, manifesto de evidências, arquitetura,
  commissioning, baseline técnico e política transversal de testes;
- confirmado que a retirada de QEMU preserva todos os cenários e oráculos:
  G1/G2 usam host-native fiel ou placa, G3-N exige NVS real em ESP32-C3/C6,
  G3-F conserva o adaptador de produção, G4 continua build e G5 hardware real;
- a abstração local autorizada na seção 2.4 é suficiente para extrair a
  política integrada usada por `main.c`, sem componente transversal ou lógica
  paralela de teste;
- G1 ausente, G3-N/G5 pendentes, AC-005 sem caso integrado, distinção do
  comando sob `RegistryUnavailable` e rótulos parciais são débitos objetivos
  da implementação, não decisões ausentes;
- resultado da versão 0.4: `Implementable`; normativo `Proposed`, implementação
  funcional e migração de validação `In Progress`, prontidão `Not Ready` e
  `EKM-CHG-0008` `Open`;
- nenhuma implementação ou execução foi realizada; nova ordem do Arquiteto é
  necessária para iniciar a atuação de Engenheiro Implementador.

### Registro corretivo de implementação v0.4 (Engenheiro Implementador, 01/08/2026)

- corrigidos dois achados Médios da revisão técnica anterior: `main.c`
  (`start_host_command()`) passou a consultar `device_registry_state()`
  explicitamente e a propagar um motivo distinto de `RegistryUnavailable`
  até o host, em vez de reportar tudo como `"target not known"`
  (COORD-REG-011/012); quatro `TEST_CASE` de
  `device_registry_test/main/test_device_registry.c` que usavam rótulo
  integral (`[AC-002]`, `[AC-003]`, `[AC-004]`, `[AC-006]`) apesar de
  exercitarem somente `device_registry.c` isolado passaram a usar sufixo
  `-partial-core`/`-partial-schema`, conforme o contrato de rótulos da
  seção 13;
- ampliada a cobertura estrutural de AC-007 em G2 (sem hardware): quatro
  `TEST_CASE` novos cobrem contagem acima de oito, endereço nulo, endereço
  broadcast e endereço duplicado em blob corretamente checksumado, todos
  rotulados `[AC-007-partial-...]`;
- build limpo de produção ESP32-C6 (`central_154.bin`, `0x45bc0` bytes) e do
  app `device_registry_test` ESP32-C3 (`0x24110` bytes) com ESP-IDF 6.0.1,
  código 0, zero warnings — evidência G4 apenas;
- nenhum caso Unity foi executado nesta atuação: sem placa ESP32-C3/C6
  conectada nesta sessão e com QEMU proibido por `TESTEXEC-001`, G4
  (compilação) não substitui evidência comportamental; flash em placa também
  depende de ordem explícita do Arquiteto;
- débitos preservados sem alteração: G1 (política integrada de `main.c`)
  continua inexistente, G3-N e G5 continuam não executados por ausência de
  hardware, AC-005 continua sem qualquer caso, e as classes de AC-007
  dependentes de erro de inicialização NVS ou de sentinela sob NVS real
  continuam fora do escopo desta correção;
- estado resultante: implementação `In Progress`, migração de validação
  `In Progress`, prontidão `Not Ready`, `EKM-CHG-0008` permanece `Open`;
  nenhum AC promovido a `Approved`, implementação não promovida a
  `Implemented`. Recomenda-se atuação futura dedicada a extrair a política
  de decisão de `main.c` para forma testável (G1) e a obter placa física
  ESP32-C3/C6 para fechar G3-N/G5.

### Revisão técnica da implementação v0.4 (Engenheiro Revisor, 01/08/2026)

- revisado integralmente o resultado até `135aef1` contra COORD-REG-001 a 013,
  AC-001 a AC-008, matriz de decisão, gates G1 a G5, substitutos e manifesto;
- builds independentes ESP-IDF 6.0.1 terminaram com código 0: app ESP32-C3
  `0x24110` bytes e produção ESP32-C6 `0x45bc0` bytes; nenhum caso foi
  executado e G4 não foi convertido em evidência comportamental;
- confirmado que os rótulos parciais foram corrigidos e que os quatro novos
  casos estruturais de AC-007 compilam, sem alegação de AC integral;
- achado alto: a fonte agora contém 17 casos, mas o runner físico ainda exige
  resumo terminal de 13; portanto ele não consegue concluir com sucesso para
  a suíte atual;
- achado alto: G1 continua inexistente, G3-N/G5 não foram executados, AC-005
  não possui caso, AC-007 permanece sem inicialização NVS/sentinela real e
  G3-F continua parcial; nenhum AC está integralmente aprovado;
- achado médio: `start_host_command()` ainda verifica comando pendente antes
  do estado do registry; a combinação pending + unavailable não preserva a
  precedência normativa de `RegistryUnavailable`, embora não transmita;
- recomendação: não aceitar nem promover; corrigir runner e precedência,
  completar G1 e depois executar os gates físicos autorizados. Estados
  permanecem `Proposed`, `In Progress`, `Not Ready`, `Implementable` e
  `EKM-CHG-0008` `Open`.

### Implementação corretiva de política e runner v0.4 (Engenheiro Implementador, 01/08/2026)

- criada política local compartilhada por produção e testes para discovery,
  DATA, ACK e comando do host; `main.c` passou a consumir suas decisões;
- corrigida a precedência pending + unavailable no comando: após validar o
  endereço, indisponibilidade agora precede correlações e identidade;
- runner físico atualizado de 13 para 24 casos, correspondendo à fonte atual;
- adicionado runner host-native da política real; execução terminal aprovou
  `7 Tests 0 Failures 0 Ignored` com código 0;
- builds ESP-IDF 6.0.1 aprovados: app ESP32-C3 `0x24a30` e produção ESP32-C6
  `0x45c60`; os 24 casos Unity foram compilados, mas não executados;
- reconstruções terminais dos artefatos finais aprovaram após ativação
  explícita do ambiente Python 3.14; tentativa C3 anterior foi bloqueada pelo
  sandbox em consulta `psutil`, antes da compilação;
- nenhuma porta física foi detectada, flash/monitor não foram autorizados e
  QEMU não foi usado; G3-N, G5, sentinela real e oráculos integrados restantes
  continuam pendentes;
- implementação e migração permanecem `In Progress`, prontidão `Not Ready` e
  `EKM-CHG-0008` `Open`; nenhum AC integralmente promovido.

---

## EKM-CHG-0009 — Retirada transversal de QEMU

**Status:** `Closed`
**Tipo:** Mudança de estratégia de validação
**Aberta em:** 01/08/2026

### Decisão do Arquiteto

QEMU deixa de ser usado em todo o repositório como estratégia de validação ou
execução de testes. A decisão não reduz requisitos funcionais, cenários,
falhas, oráculos nem quantidade de casos.

### Ativos afetados

- `docs/specs/Repository-Test-Execution-Policy.md`;
- `docs/specs/ISSP-Configurable-Bootstrap.md`;
- `docs/specs/ISSP-Coordinator-Paired-Device-Registry.md`;
- `components/README.md`;
- `pytest_hello_world.py` e os dois test apps ESP-IDF, como artefatos técnicos
  candidatos à migração posterior;
- `docs/rfc/KNOWLEDGE-MAP.md` e este histórico.

### Resultado da autoria

- criada política normativa transversal com requisitos TESTEXEC-001 a 007 e
  critérios TESTEXEC-AC-001 a 007;
- definido runner host-native somente quando preservada toda a semântica
  material; demais testes executam em placa física suportada;
- G3-N do registry passa a exigir adaptador de produção e NVS real em
  ESP32-C3 ou ESP32-C6 físico; G3-F pode executar host-native fiel ou físico;
- os dezenove cenários `SmartSysApp` permanecem obrigatórios e devem migrar
  para host-native fiel ou ESP32-C3 físico;
- resultados QEMU anteriores permanecem auditáveis como fatos históricos, mas
  não podem aprovar versões ou revisões posteriores;
- inventariados imports, markers, runners, comentários, test apps, diretórios
  `build_qemu_c3`, imagens e ferramenta externa candidatos à remoção ou
  migração; nada foi excluído nesta autoria;
- nenhum código, teste, configuração ou runner foi alterado ou executado.

### Estados e próxima etapa

- política transversal: `Proposed`, `Not Started`, `Not Ready`,
  `Pending Review`;
- registry v0.4: funcional `In Progress`, migração `Not Started`, `Not Ready`,
  `Pending Review`; `EKM-CHG-0008` permanece `Open`;
- bootstrap v1.5: baseline funcional v1.4 historicamente `Validated`, migração
  `Not Started`, versão `Proposed`, `Not Ready`, `Pending Review`;
- próxima etapa: análise independente de implementabilidade antes de qualquer
  migração ou exclusão técnica.

### Critérios de encerramento

- política promovida a `Implementable` por análise independente;
- runners substitutos implementados e executados com evidência terminal;
- artefatos QEMU versionados e locais removidos sem perda de cobertura;
- especificações, documentação e mapa reconciliados;
- revisão confirma ausência de QEMU como estratégia vigente.

### Revisão de implementabilidade (Engenheiro Analista, 01/08/2026)

- confrontados integralmente TESTEXEC-001 a 007, TESTEXEC-AC-001 a 007,
  matriz de substituição, inventário técnico, Bootstrap v1.5 e Registry v0.4;
- confirmado precedente host-native no runner raiz e execução física viável
  para os dois test apps ESP-IDF; quando a fidelidade host-native não puder ser
  demonstrada, o fallback físico é obrigatório;
- a retirada de QEMU não elimina cenário, falha, oráculo ou quantidade de
  casos e não transforma build em evidência comportamental;
- migração limitada a runners, imports, markers, comentários, configurações,
  documentação e artefatos inventariados, sem nova camada de produção;
- resultado da política v0.1: `Implementable`; normativo `Proposed`,
  implementação `Not Started`, prontidão `Not Ready`, transação `Open`;
- Bootstrap v1.5 e Registry v0.4 permanecem `Pending Review` em seus ciclos
  integrais; nenhuma implementação, remoção, teste ou flash foi executado.

### Implementação da retirada técnica (Engenheiro Implementador, 01/08/2026)

- removidos do runner raiz imports, tipos, marker e caso específicos do
  emulador; a verificação de SHA-256 foi preservada no teste físico genérico;
- adicionados runners pytest físicos ESP32-C3 para os 20 casos SmartSysApp e
  13 casos do registry, com oráculos sobre resumo Unity e `OK` terminal;
- comentários e configurações versionadas migrados; varredura técnica não
  encontra dependência, marker ou runner proibido fora de política/histórico;
- removidos os dois diretórios locais regeneráveis `build_qemu_c3`; fontes de
  teste preservadas; ferramenta externa mantida por estar fora do escopo;
- `py_compile` aprovou os três runners; builds ESP-IDF 6.0.1/ESP32-C3 geraram
  `smart_sys_app_test.bin` (138272 bytes) e `device_registry_test.bin` (146320
  bytes), ambos com código 0;
- a coleta não iniciou porque o ambiente ESP-IDF não possui `pytest`; nenhum
  teste comportamental foi executado;
- flash e monitor não foram iniciados por ausência de ordem explícita;
- TESTEXEC-AC-001/002/003/004/006/007 têm evidência estática aprovada;
  TESTEXEC-AC-005 permanece `Not Executed`;
- estado da política e das migrações dependentes: `In Progress`; prontidão
  `Not Ready`; `EKM-CHG-0009` permanece `Open`.

### Revisão técnica da retirada (Engenheiro Revisor, 01/08/2026)

- revisado integralmente o commit `c2e6c41` contra TESTEXEC-001 a 007,
  TESTEXEC-AC-001 a 007, matriz de substituição e inventário técnico;
- confirmado por inspeção e varredura que imports, tipos, marker, runner e
  comandos QEMU não permanecem como ativos técnicos vigentes; os dois
  diretórios locais `build_qemu_c3` continuam ausentes;
- confirmados 20 casos SmartSysApp e 13 casos registry, runners físicos
  ESP32-C3, `py_compile` com código 0 e artefatos de build ESP32-C3 nos
  tamanhos registrados pela implementação;
- achado alto: TESTEXEC-AC-005 permanece `Not Executed`; nenhum dos 33 casos
  foi coletado ou executado em hardware e não existe evidência terminal nova;
- achado médio: TESTEXEC-AC-005, Bootstrap v1.5 e a abertura desta transação
  dizem “dezenove” cenários SmartSysApp, mas a fonte e o runner preservam 20;
- a ausência de `pytest`/plugins no ambiente continua sendo limitação de
  infraestrutura e deve ser resolvida antes da coleta; flash e monitor exigem
  ordem explícita do Arquiteto;
- recomendação: não aceitar nem promover; retornar ao Autor para reconciliar a
  quantidade e depois executar os 20 + 13 casos em ESP32-C3 físico sob atuação
  autorizada. Estados permanecem `Proposed`, `In Progress`, `Not Ready` e
  transação `Open`.

### Decisão arquitetural de aceite e encerramento (01/08/2026)

- o Arquiteto decidiu aceitar a implementação entregue e encerrar a política
  e esta transação no estado observado;
- a política passa a `Active` e a implementação fica `Accepted by Architect`,
  sem promoção técnica para `Implemented` ou `Validated`;
- TESTEXEC-AC-005 permanece `Not Executed`; nenhum dos 20 + 13 casos foi
  executado em ESP32-C3 físico nesta mudança;
- permanecem registradas e aceitas a ausência de `pytest`/plugins no ambiente
  observado e a divergência documental de dezenove versus 20 casos
  SmartSysApp, sem redução efetiva de cobertura;
- a prontidão técnica permanece `Not Ready`; o encerramento representa aceite
  humano explícito do risco residual, não criação de evidência inexistente;
- Bootstrap e Registry conservam seus ciclos e estados próprios;
- `EKM-CHG-0009` está `Closed` por decisão do Arquiteto.
