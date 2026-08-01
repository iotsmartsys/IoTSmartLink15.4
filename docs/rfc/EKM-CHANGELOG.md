# EKM — Histórico de Mudanças do Conhecimento

**Tipo:** Operacional
**Status:** Active
**Versão:** 1.7
**Responsável:** Marcelo Miranda
**Última atualização:** 31/07/2026
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

- revisão independente promove a especificação para `Implementable` — **satisfeito** em 31/07/2026;
- Arquiteto autoriza implementação;
- requisitos COORD-REG-001 a COORD-REG-013 são implementados;
- gates automatizados e cenários de hardware AC-001 a AC-008 terminam com
  evidência aprovada;
- mapa, especificações relacionadas e transação são reconciliados;
- implantação em ambiente real permanece sujeita a ordem própria.
