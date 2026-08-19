# Remediação dos débitos técnicos aceitos

**ID:** `EKOM-DEBT-REMEDIATION-001`

**Classe da fonte:** Normativa

**Versão:** 0.1

**Estado normativo:** `Active`

**Estado da implementação:** Não iniciada

**Estado do workflow:** Rascunho [`Draft`]

**Análise de implementabilidade:** Pendente

**Bloqueio arquitetural:** Nenhum declarado pela Autoria. A seção 5.4 depende de
emenda aceita da ADR-0005; enquanto a emenda não for aceita, aquele bloco não é
implementável.

**Responsável arquitetural:** Marcelo Miranda

**Última atualização:** 18/08/2026

**Escopo:** fachada `SmartSysApp`, product firmwares do `client_154`, exemplo
`issp_minimal_client`, `smart_sys_app_test`, tabela de tipos de evento do
`coordinator_154`, telemetria de bateria observável pelo host e configuração
rastreada do `client_154`

**Relações normativas e de dependência:**

- Altera [`Amends`] `docs/specs/ISSP-Configurable-Bootstrap.md` — remove
  `eventType` das configurações públicas das capabilities existentes, torna a
  unicidade exclusiva do endpoint e registra a ADR-0005 como origem da extensão
  da semântica de `endpointId` e `eventType`;
- Altera [`Amends`] `docs/specs/Client-Battery-Level.md@v0.5` — a degradação de
  calibração de `BATTERY-015` e a inércia por falha de ADC de `BATTERY-016`
  deixam de ser observáveis apenas em log local e passam a ter representação
  observável pelo host, sem alterar o domínio do evento de nível de bateria;
- Altera [`Amends`] `docs/specs/Client-SDK-Configurable-Features.md@v0.1` —
  substitui a exclusão explícita de `client_154/sdkconfig` do recorte por seu
  alinhamento à baseline declarada;
- Depende de [`Depends On`] `docs/adr/ADR-0005-CAPABILITY-IDENTITY.md` com
  emenda aceita que aloque o tipo de evento 4; sem essa aceitação, a seção 5.4
  não é implementável;
- Preserva `docs/specs/Firmware-Variants-Menuconfig.md` e sua decisão A9,
  `docs/specs/ISSP-Architecture.md`, `docs/specs/ISSP-Reusable-Components.md`,
  `docs/specs/Client-Deep-Sleep.md`, `docs/specs/ISSP-Report-Identity.md`,
  ADR-0001, ADR-0002, ADR-0003 e ADR-0004 — nenhuma fronteira de componente,
  layout wire, política de ACK, retry, commissioning ou registry é redefinida.

---

## 1. Objetivo e contexto

A seção 7 do `docs/rfc/KNOWLEDGE-MAP.md` registra cinco débitos aceitos. Cada um
tem critério de quitação declarado, e nenhum foi remediado. Três deles
(`EKOM-DEBT-0001`, `EKOM-DEBT-0002` e `EKOM-DEBT-0003`) originam-se da ADR-0005:
o modelo de identidade de capability foi aceito em 14/08/2026, mas o código, a
navegabilidade normativa e a tabela de tipos de evento do coordenador
permanecem divergentes. `EKOM-DEBT-0004` mantém a telemetria de bateria sem
distinção observável entre ausência, defeito e aproximação. `EKOM-DEBT-0005`
mantém a configuração rastreada divergente da baseline declarada pela decisão
A9.

Esta especificação define o comportamento que, uma vez implementado e
evidenciado, permite ao Arquiteto determinar a quitação dos cinco registros. A
mudança de estado dos débitos no mapa permanece ato exclusivo do Arquiteto e
não decorre da existência desta fonte.

O Arquiteto determinou o recorte único abrangendo os cinco débitos, a inclusão
de `EKOM-DEBT-0004` nesta fonte, a guarda de build para `EKOM-DEBT-0003`, o
alinhamento do `sdkconfig` à baseline do Kconfig e o mecanismo de observabilidade
por tipo de evento próprio.

## 2. Escopo

- remoção de `eventType` das configurações públicas das capabilities existentes
  e injeção do tipo pela fachada;
- unicidade de capability por endpoint;
- migração dos dois product firmwares, do exemplo e do `smart_sys_app_test`;
- emenda de navegabilidade em `ISSP-Configurable-Bootstrap.md`;
- guarda de build no `coordinator_154` que confronta a tabela de tipos de evento
  com o registro da ADR-0005;
- capability de estado da telemetria de bateria, com tipo de evento próprio,
  publicada pelo client e traduzida pelo coordenador;
- alinhamento de `client_154/sdkconfig` à composição default declarada;
- reconciliação do conhecimento afetado.

## 3. Fora de escopo

- layout wire, tamanho, offsets, checksum, endianness e encerramento de
  `EKM-GAP-0002`;
- commissioning, pareamento, registry, ACK, retry e identidade de report;
- alocação de tipos de evento além do previsto na seção 5.4;
- alteração da faixa elétrica, química, de média, de variação ou do intervalo da
  capability de nível de bateria;
- alteração de produto, board, GPIO, polaridade, debounce, factory reset, wake
  LED ou política de deep sleep;
- criação de descoberta estruturada de capabilities no host;
- alteração do projeto ESP-IDF não classificado da raiz (`EKM-GAP-0007`);
- quitação dos débitos no mapa, reservada ao Arquiteto;
- flash, monitor, hardware e execução de qualquer suíte automatizada.

### 3.1 Arquitetura e organização

**Precedente aplicável:** `docs/specs/Client-Battery-Level.md@v0.5` e ADR-0005
para composição de capability na fachada; `docs/specs/ISSP-Reusable-Components.md`
e ADR-0001 para a fronteira entre fachada e behaviors.

**Elementos preservados:** `issp_behaviors` permanece genérico e recebe o tipo de
evento por configuração construída na fachada; `client_154` e `coordinator_154`
permanecem alvos separados sem dependência de código; símbolos `CONFIG_*`
permanecem restritos a `client_154/main`.

**Desvio arquitetural explícito:** Nenhum.

### 3.2 Limite de escopo funcional

**Capacidades arquiteturais pressupostas:** composição de capabilities pela
fachada, transporte de reports e tradução de evento no coordenador, todos
vigentes.

**Preparação arquitetural separada:** Não aplicável. A seção 5.4 não cria
lifecycle, dono de execução, persistência ou API transversal novos: ela compõe
uma capability adicional pelos mecanismos existentes. Sua única dependência
externa é a alocação do tipo de evento 4 por emenda da ADR-0005, registrada
como `Depends On`.

## 4. Requisitos

### 4.1 Identidade de capability — `EKOM-DEBT-0001`

- **`DEBTREM-001`:** `SwitchConfig` e `DoorSensorConfig` deixam de expor
  `eventType`. Nenhuma configuração pública de capability existente aceita,
  sobrescreve ou recebe o tipo de evento do product firmware.
- **`DEBTREM-002`:** a fachada injeta o tipo de evento ao construir o behavior:
  1 para a capability de sensor de porta e 2 para a capability de plug
  comutável, conforme o registro da ADR-0005. `issp_behaviors` continua
  recebendo o tipo por configuração e não ganha conhecimento de capability.
- **`DEBTREM-003`:** a unicidade passa a ser por endpoint. Registrar uma
  capability cujo endpoint já esteja ocupado é recusado, ainda que os tipos de
  evento sejam distintos.
- **`DEBTREM-004`:** a recusa por endpoint ocupado preserva o contrato de falha
  vigente: resultado `InvalidArgument` registrado na configuração, capability
  não registrada, nenhuma operação parcial e nenhuma alteração das capabilities
  já registradas.
- **`DEBTREM-005`:** os endpoints vigentes permanecem congelados. A capability
  funcional do produto conserva o endpoint 1 e a capability de nível de bateria
  conserva o endpoint 2. Nenhum endpoint é recalculado por ordem de registro.
- **`DEBTREM-006`:** `single_smart_plug`, `door_sensor_battery_h2` e
  `examples/issp_minimal_client` deixam de declarar tipo de evento e passam a
  compor pela nova configuração, preservando endpoint, pinos e demais
  parâmetros atuais.
- **`DEBTREM-007`:** `components/issp_app_154/test_apps/smart_sys_app_test`
  integra o recorte e é reconciliado com o novo contrato, conforme a seção 7.1.

### 4.2 Navegabilidade normativa — `EKOM-DEBT-0002`

- **`DEBTREM-008`:** `docs/specs/ISSP-Configurable-Bootstrap.md` passa a
  declarar que a semântica de `endpointId` e `eventType` foi estendida pela
  ADR-0005 e por esta especificação, identificando a origem da extensão no
  ponto em que a configuração é descrita.
- **`DEBTREM-009`:** o texto do bootstrap não conserva afirmação de que o tipo
  de evento permanece configurável pelo product firmware das capabilities
  abrangidas por `DEBTREM-001`.

### 4.3 Guarda da tabela de tipos de evento — `EKOM-DEBT-0003`

- **`DEBTREM-010`:** o build do `coordinator_154` falha quando a tabela de tipos
  de evento em `coordinator_154/main/iot154_packet.h` divergir do registro da
  ADR-0005 em valor numérico ou em conjunto de tipos alocados.
- **`DEBTREM-011`:** a guarda é verificação em tempo de build, sem exigir
  execução de teste, hardware, flash ou monitor.
- **`DEBTREM-012`:** a guarda cobre os tipos 1, 2, 3 e o tipo 4 introduzido pela
  seção 4.4, e falha com diagnóstico que identifique o tipo divergente.
- **`DEBTREM-013`:** o rótulo genérico para tipo não registrado permanece
  vigente no caminho de recepção; a guarda atua sobre a tabela declarada, não
  sobre o frame recebido.

### 4.4 Observabilidade da telemetria de bateria — `EKOM-DEBT-0004`

- **`DEBTREM-014`:** o tipo de evento 4 é alocado para a capability de estado da
  telemetria de bateria, por emenda da ADR-0005. O tipo 3 conserva integralmente
  seu domínio de 0 a 100; nenhum valor sentinela é acrescentado a ele.
- **`DEBTREM-015`:** o estado da telemetria é uma capability própria, no
  endpoint 3, conforme o modelo da ADR-0005: um tipo de evento por capability e
  identidade pelo endpoint. O endpoint 3 fica congelado para esse uso.
- **`DEBTREM-016`:** o domínio do valor do tipo 4 é: `0` medição calibrada;
  `1` medição aproximada, quando o esquema de calibração do ADC não estiver
  disponível conforme `BATTERY-015`; `2` capability de bateria inerte por falha
  de configuração da unidade ou do canal do ADC conforme `BATTERY-016`.
  Nenhum outro valor é definido.
- **`DEBTREM-017`:** a capability de estado existe somente quando a capability
  de nível de bateria integra a composição. Com a bateria desabilitada, o
  endpoint 3 não é registrado e nenhum report de tipo 4 é produzido.
- **`DEBTREM-018`:** o estado é publicado no primeiro ciclo operacional em que
  for determinado e sempre que mudar de valor. Repetição do mesmo estado não é
  publicada.
- **`DEBTREM-019`:** o estado `2` é publicado ainda que a capability de bateria
  esteja inerte. A inércia contratada em `BATTERY-016` é preservada: a
  capability de nível de bateria continua sem medir e sem publicar reports de
  tipo 3, e `setup()` continua alcançando `Running`.
- **`DEBTREM-020`:** o report de estado não integra a evidência de admissão de
  deep sleep e nunca faz o ciclo acordado aguardar por ele, preservando a
  política de `BATTERY-017`.
- **`DEBTREM-021`:** a capability de estado é somente leitura e recusa comandos
  pelo behavior que reconhece o par, conforme a ADR-0005, mantendo endpoint e
  registro preservados.
- **`DEBTREM-022`:** o coordenador traduz o tipo 4 em rótulo próprio,
  distinguível de `Battery Level (%)`, e traduz os valores `0`, `1` e `2` em
  representações distintas apresentadas ao host. Valor fora do domínio recebe a
  representação numérica de fallback vigente.
- **`DEBTREM-023`:** nenhuma alteração de layout, tamanho, checksum ou
  endianness do frame decorre desta seção; apenas os campos `endpointId`,
  `eventType` e `value` já existentes são usados.

### 4.5 Baseline rastreada — `EKOM-DEBT-0005`

- **`DEBTREM-024`:** `client_154/sdkconfig` passa a selecionar o product
  firmware `Single smart plug` e o board model `Current client ESP32-H2 wiring`,
  em coerência com os defaults do Kconfig e com a decisão A9 de
  `Firmware-Variants-Menuconfig.md`.
- **`DEBTREM-025`:** os símbolos do menu `Firmware features` dependem do produto
  `Door sensor battery H2` e, na baseline alinhada, não são consumidos. Nenhum
  valor desses símbolos é lido por composição cuja dependência não esteja
  satisfeita.
- **`DEBTREM-026`:** `Firmware-Variants-Menuconfig.md` e sua decisão A9
  permanecem inalteradas; nenhuma baseline nova é declarada.
- **`DEBTREM-027`:** o alinhamento não altera defaults do Kconfig, escolhas
  exclusivas, produtos, boards nem qualquer parâmetro funcional.

## 5. Fluxos, estados e contratos

### 5.1 Registro de capability

```text
product firmware compõe capability
→ fachada verifica endpoint ocupado
   → ocupado: InvalidArgument, nada registrado
   → livre: fachada injeta o tipo de evento da capability
→ behavior construído com endpoint e tipo
→ endpoint reservado até o fim do ciclo de vida da composição
```

O product firmware não participa da escolha do tipo. O endpoint continua sendo
parâmetro da composição, com os valores vigentes congelados por `DEBTREM-005`.

### 5.2 Estado da telemetria de bateria

```text
setup inicializa a capability de bateria
→ configuração do ADC falha
   → estado 2; bateria inerte; Running alcançado
→ configuração bem-sucedida, calibração indisponível
   → estado 1; bateria opera pelo fundo de escala e publica
→ configuração e calibração disponíveis
   → estado 0; bateria opera calibrada e publica
→ estado publicado quando determinado e a cada mudança
```

Registro dos tipos de evento após esta especificação, sujeito à emenda da
ADR-0005:

| Tipo | Capability | Endpoint vigente | Domínio do valor |
|---|---|---|---|
| 1 | Sensor de porta | 1 | 1 aberto, 0 fechado |
| 2 | Plug comutável | 1 | ligado, desligado e alternar |
| 3 | Nível de bateria em percentual | 2 | 0 a 100 |
| 4 | Estado da telemetria de bateria | 3 | 0 calibrado, 1 aproximado, 2 inerte |

O endpoint indicado é o vigente nos produtos atuais; ele identifica a capability
dentro do produto e não categoriza o tipo.

## 6. Falhas e condições de borda

- endpoint ocupado é sempre recusa, e não substituição silenciosa da capability
  registrada;
- capability de bateria desabilitada não produz estado algum, nem o valor `0`;
- transição de estado que ocorra depois da primeira publicação é publicada como
  qualquer mudança, sem retroagir ao ciclo anterior;
- falha isolada de medição, já contratada em `Client-Battery-Level.md`, não é
  estado da telemetria e não altera o valor do tipo 4;
- tipo de evento não registrado recebido pelo coordenador conserva o rótulo
  genérico vigente e não é convertido em estado de telemetria;
- divergência entre a tabela do coordenador e a ADR-0005 interrompe o build,
  sem produzir binário.

## 7. Critérios de aceite e validações

### `DEBTREM-AC-001` — tipo de evento fora da configuração pública

**Cobre:** `DEBTREM-001`, `DEBTREM-002`, `DEBTREM-006`

- **Dado que** um product firmware compõe as capabilities de porta e de plug;
- **Quando** o build é produzido;
- **Então** nenhuma configuração pública aceita tipo de evento, os firmwares e o
  exemplo compilam sem declará-lo, e o behavior construído recebe 1 para porta e
  2 para plug;
- **Evidência:** inspeção do delta e builds canônicos do `client_154` e do
  exemplo.

### `DEBTREM-AC-002` — unicidade por endpoint

**Cobre:** `DEBTREM-003`, `DEBTREM-004`, `DEBTREM-005`

- **Dado que** uma composição registra duas capabilities com o mesmo endpoint e
  tipos de evento distintos;
- **Quando** o segundo registro é solicitado;
- **Então** o resultado é `InvalidArgument`, a segunda capability não é
  registrada, a primeira permanece intacta e os endpoints vigentes dos produtos
  atuais não mudam;
- **Evidência:** casos de `smart_sys_app_test` enumerados em 7.1; execução
  reservada a autorização própria.

### `DEBTREM-AC-003` — navegabilidade do bootstrap

**Cobre:** `DEBTREM-008`, `DEBTREM-009`

- **Dado que** um leitor consulta somente `ISSP-Configurable-Bootstrap.md`;
- **Quando** procura a semântica de `endpointId` e `eventType`;
- **Então** encontra a referência à ADR-0005 e a esta especificação como origem
  da extensão, e não encontra afirmação de que o tipo permanece configurável
  pelo produto;
- **Evidência:** inspeção documental.

### `DEBTREM-AC-004` — guarda da tabela de eventos

**Cobre:** `DEBTREM-010`, `DEBTREM-011`, `DEBTREM-012`, `DEBTREM-013`

- **Dado que** `iot154_packet.h` é alterado para divergir do registro da
  ADR-0005 em valor ou conjunto de tipos;
- **Quando** o build canônico do `coordinator_154` é executado;
- **Então** a compilação falha identificando o tipo divergente, e o build da
  tabela conforme conclui normalmente;
- **Evidência:** build canônico C6 conforme e build de divergência deliberada,
  descartado após a observação.

### `DEBTREM-AC-005` — estado da telemetria observável

**Cobre:** `DEBTREM-014`, `DEBTREM-015`, `DEBTREM-016`, `DEBTREM-018`,
`DEBTREM-022`, `DEBTREM-023`

- **Dado que** um dispositivo com bateria habilitada opera com medição
  calibrada, e depois sem calibração disponível;
- **Quando** cada estado é determinado;
- **Então** o client publica endpoint 3, tipo 4, valor `0` e depois `1`, sem
  republicar estado repetido, e o coordenador apresenta ao host rótulo próprio e
  representações distintas, com o layout do frame inalterado;
- **Evidência:** inspeção do delta e do codec; validação em hardware reservada a
  etapa posterior sob autorização própria.

### `DEBTREM-AC-006` — inércia por falha de ADC observável

**Cobre:** `DEBTREM-019`, `DEBTREM-020`

- **Dado que** a configuração da unidade ou do canal do ADC falha;
- **Quando** `setup()` é executado;
- **Então** o dispositivo alcança `Running`, a capability de bateria permanece
  inerte sem reports de tipo 3, o estado `2` é publicado, e a admissão de deep
  sleep não aguarda por esse report;
- **Evidência:** inspeção do delta; validação em hardware reservada a etapa
  posterior sob autorização própria.

### `DEBTREM-AC-007` — bateria desabilitada

**Cobre:** `DEBTREM-017`, `DEBTREM-021`

- **Dado que** a capability de nível de bateria não integra a composição;
- **Quando** o dispositivo opera;
- **Então** o endpoint 3 não é registrado e nenhum report de tipo 4 é produzido;
  com a bateria habilitada, um comando dirigido ao endpoint 3 é recusado pelo
  behavior, sem desregistrar a capability;
- **Evidência:** builds com bateria habilitada e desabilitada; inspeção do
  delta.

### `DEBTREM-AC-008` — baseline rastreada

**Cobre:** `DEBTREM-024`, `DEBTREM-025`, `DEBTREM-026`, `DEBTREM-027`

- **Dado que** o `client_154/sdkconfig` rastreado é reutilizado sem
  regeneração;
- **Quando** o build canônico do `client_154` é executado;
- **Então** a composição produzida é `Single smart plug` com
  `Current client ESP32-H2 wiring`, os símbolos do menu de features não são
  consumidos, e nenhum default do Kconfig ou parâmetro funcional muda;
- **Evidência:** inspeção do arquivo rastreado e build canônico H2.

### 7.1 Evidências planejadas

**Artefatos de teste no recorte:**
`components/issp_app_154/test_apps/smart_sys_app_test`, exclusivamente para:

- **`DEBTREM-AC-002`, cobrindo `DEBTREM-003` e `DEBTREM-004`:** caso que
  registra duas capabilities com o mesmo endpoint e tipos distintos; resultado
  esperado é recusa com `InvalidArgument` e preservação da primeira; meio é o
  test app existente do componente; consumidor material é a fachada
  `SmartSysApp`;
- **`DEBTREM-AC-001`, cobrindo `DEBTREM-001` e `DEBTREM-002`:** reconciliação
  dos casos existentes que hoje declaram `eventType` na configuração, de modo a
  compor pelo novo contrato e conferir o tipo injetado no behavior; meio é o
  mesmo test app; consumidor material é a fachada.

Nenhum outro artefato de teste integra o recorte. Nenhum caso do
`coordinator_154` ou dos test apps de outros componentes é criado ou alterado.

Demais evidências:

- inspeção do delta de fachada, behaviors, firmwares, exemplo e coordenador;
- builds canônicos do `client_154` (H2) e do `coordinator_154` (C6) conforme
  `Repository-Test-Execution-Policy.md`;
- build de divergência deliberada da tabela de eventos, para observar a falha da
  guarda, descartado em seguida;
- builds do `client_154` com a capability de bateria habilitada e desabilitada;
- inspeção documental do bootstrap e do `sdkconfig` rastreado.

Criar teste não autoriza executá-lo. A execução do `smart_sys_app_test`, flash,
monitor e validação em hardware do estado da telemetria permanecem
`Not Executed` até autorização própria do Arquiteto.

## 8. Conhecimento afetado

- `docs/adr/ADR-0005-CAPABILITY-IDENTITY.md`: exige emenda aceita pelo Arquiteto
  alocando o tipo de evento 4 e atualizando a seção de estado divergente quando
  a remediação for concluída;
- `docs/specs/ISSP-Configurable-Bootstrap.md`: emendado conforme `DEBTREM-008` e
  `DEBTREM-009`;
- `docs/specs/Client-Battery-Level.md`: emendado quanto à observabilidade de
  `BATTERY-015` e `BATTERY-016`, preservando o domínio do evento 3;
- `docs/specs/Client-SDK-Configurable-Features.md`: a exclusão do `sdkconfig` do
  recorte deixa de valer;
- `docs/rfc/KNOWLEDGE-MAP.md`: índice, árvore e seção 7 reconciliados; a
  quitação dos débitos é ato do Arquiteto;
- `docs/specs/SYSTEM-DOSSIER.md` e `docs/rfc/EKOM-CHANGELOG.md`: navegação e
  estado resumido;
- preservados: `ISSP-Architecture.md`, `ISSP-Reusable-Components.md`,
  `ISSP-Commissioning.md`, `ISSP-Coordinator-Paired-Device-Registry.md`,
  `ISSP-Report-Identity.md`, `Client-Deep-Sleep.md`,
  `Firmware-Variants-Menuconfig.md` e `Repository-Test-Execution-Policy.md`.

## 9. Relações, decisões, lacunas e débitos

**Fatos observados:**

- `SwitchConfig.eventType` e `DoorSensorConfig.eventType` em
  `components/issp_app_154/include/SmartSysApp.h`; `BatteryLevelConfig` já nasce
  sem tipo de evento;
- `hasDuplicateEndpoint` compara o par em
  `components/issp_app_154/src/smart_sys_app.cpp`, enquanto
  `hasOccupiedEndpoint` já existe no mesmo arquivo;
- consumidores em `client_154/main/firmwares/single_smart_plug.cpp`,
  `client_154/main/firmwares/door_sensor_battery_h2.cpp` e
  `examples/issp_minimal_client/main/main.cpp`;
- `IOT154_EVENT_DOOR`, `IOT154_EVENT_POWER` e
  `IOT154_EVENT_BATTERY_LEVEL_PERCENT` em `coordinator_154/main/iot154_packet.h`,
  traduzidos em `type_from_event` e `value_from_event` em
  `coordinator_154/main/main.c`, com rótulo genérico `"Device"` para tipo não
  registrado;
- o frame expõe apenas `endpointId`, `eventType` e `value`, de um byte cada, e o
  contrato wire integral permanece aberto em `EKM-GAP-0002`;
- os defaults do Kconfig são `Single smart plug` e
  `Current client ESP32-H2 wiring`, enquanto o `sdkconfig` rastreado seleciona o
  sensor de porta e seu board.

**Intenção e decisões confirmadas pelo Arquiteto em 18/08/2026:**

- recorte único cobrindo os cinco débitos;
- `EKOM-DEBT-0004` incluído nesta fonte, apesar da ressalva de que sua parte
  atravessa client, wire, coordenador e host sob `EKM-GAP-0002` `Partial`;
- observabilidade por tipo de evento 4 no endpoint 3, com valores 0, 1 e 2;
- guarda de `EKOM-DEBT-0003` em tempo de build;
- `EKOM-DEBT-0005` quitado por alinhamento do `sdkconfig` à baseline da decisão
  A9.

**Solução proposta:** conforme as seções 4 a 7. A Autoria recomenda que a
emenda da ADR-0005 seja aceita antes da ordem de implementação, para que a seção
4.4 não permaneça dependente de decisão pendente durante a execução.

**Decisões pendentes:**

- aceitação, pelo Arquiteto, da emenda da ADR-0005 que aloca o tipo de evento 4
  e fixa o endpoint 3 para a capability de estado da telemetria;
- momento da quitação de cada débito no `KNOWLEDGE-MAP.md`, após implementação e
  evidência.

**Relações:** `EKOM-DEBT-0001` a `EKOM-DEBT-0005`; ADR-0005; ADR-0001;
`EKM-GAP-0002`; `docs/specs/Client-Battery-Level.md`;
`docs/specs/ISSP-Configurable-Bootstrap.md`;
`docs/specs/Client-SDK-Configurable-Features.md`;
`docs/specs/Firmware-Variants-Menuconfig.md`.
