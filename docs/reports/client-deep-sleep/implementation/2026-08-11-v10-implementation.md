# Relatório de implementação — deep sleep do client

**Classe da fonte:** Relatório

**Papel:** Engenheiro Implementador

**Especificação:** `docs/specs/Client-Deep-Sleep.md`

**Revisão confrontada:** v0.10, `Proposed`, pronta para implementação

**Estado:** Em andamento [`In Progress`]

**Capacidade:** Engenheiro Implementador

**Data:** 11/08/2026

**Resultado:** contrato da v0.10 implementado integralmente em código, incluindo
as três ampliações de DEEPSLEEP-DEC-006, a arbitragem com factory reset, o
renome completo e a composição `wake_led`; nenhum build, teste, flash ou
execução em hardware foi realizado, porque a especificação não os autoriza; por
isso a implementação **não** é declarada concluída e permanece
`In Progress`; duas questões são devolvidas ao Arquiteto

> Este relatório registra a execução e suas limitações. Não altera fontes
> normativas além do estado de implementação e do alcance de renome que a
> seção 5 da especificação determina, não promove estado normativo e não
> declara conclusão.

## 1. Recorte executado

Implementei o contrato completo da v0.10: seções 3 a 6.1 e os critérios
DEEPSLEEP-AC-001 a AC-009. DEEPSLEEP-AC-010 é evidência futura e depende de
autorização de execução; apenas preparei a costura que ele prevê.

Árvore limpa em `spec/client-deep-sleep` no início da atuação. Nenhuma operação
de build, coleta, flash, teste ou hardware foi executada.

## 2. Verificação de toolchain resolvida (item 16 das análises)

A análise da v0.10 deixou em aberto o limite aceito por
`esp_sleep_enable_timer_wakeup()` no ESP32-H2 com ESP-IDF 6.0.1, registrando que
a implementação o resolveria com o toolchain presente. Resolvi por leitura do
IDF instalado em `~/.espressif/v6.0.1/esp-idf`:

- a API rejeita com `ESP_ERR_INVALID_ARG` toda duração acima de
  `((2^(SOC_LP_TIMER_BIT_WIDTH_LO + SOC_LP_TIMER_BIT_WIDTH_HI) - 1) / f_slow)`
  segundos (`components/esp_hw_support/sleep_modes.c:1725-1739`);
- no ESP32-H2 as larguras são 32 e 16, isto é, 2^48 - 1 contagens
  (`components/soc/esp32h2/include/soc/soc_caps.h:418-419`);
- `f_slow` vem de `esp_clk_tree_lp_slow_get_freq_hz(APPROX)`, que com
  `SOC_RTC_SLOW_CLK_SRC_RC_SLOW` retorna a constante
  `SOC_CLK_RC_SLOW_FREQ_APPROX`, 136000 Hz
  (`components/esp_hal_clock/esp32h2/clk_tree_hal.c:49-65`;
  `components/soc/esp32h2/include/soc/clk_tree_defs.h:51`).

Achado material para a seção 4.2: **na v6.0.1 o preparo usa a constante nominal,
não a calibração em runtime**. A margem conservadora exigida pela especificação
continua correta como contrato, e a implementei explicitamente, mas hoje ela é
folga adicional e não uma necessidade aritmética. Como um IDF futuro pode passar
a usar a frequência calibrada — e frequência maior reduz o limite — a
configuração raciocina com um **limite superior** da frequência, não com o
nominal, exatamente na direção que a v0.10 exige.

O limite não é número de produto: é derivado das capacidades declaradas pelo
target e da fonte de slow clock fixada no projeto
(`CONFIG_RTC_CLK_SRC_INT_RC=y`, `client_154/sdkconfig:1148`), com um fator de
desvio de calibração declarado
(`components/issp_app_154/src/smart_sys_app_deep_sleep.cpp:36-55`). Com fator 2
o limite aceito em configuração fica em torno de 1,03e15 µs, cerca de 287 mil
horas — muito acima de qualquer intervalo de produto e ainda assim capaz de
rejeitar um `interval` de 32 bits em horas, como DEEPSLEEP-AC-003 exige.

## 3. O que foi implementado, por fonte

### 3.1 Ampliações autorizadas por DEEPSLEEP-DEC-006

- `issp::IDeviceBehavior::quiesce()`, virtual com implementação padrão de no-op
  bem-sucedido, documentada como válida somente para behavior sem trabalho
  autônomo (`components/issp_core/include/idevice_behavior.hpp`);
- `issp::DigitalInputBehavior::quiesce()` sobrescreve, para e exclui o timer sem
  destruir o objeto nem publicar novo report. O no-op quando `begin()` nunca
  concluiu é exato porque `timer_` só é não nulo depois de `createTimer()` e
  todos os caminhos de falha chamam `stopAndDeleteTimer()`
  (`components/issp_behaviors/src/digital_input_behavior.cpp`);
- `issp::IsspDevice::beginQuiescence()` fecha atomicamente, sob a seção crítica
  já existente, o despacho de comandos e a admissão de reports: comandos ainda
  recebidos respondem `IsspCommandResult::Failed`, novas publicações retornam
  `IsspResult::NotReady` e os slots já admitidos são preservados
  (`components/issp_core/src/issp_device.cpp`);
- `issp::Issp154ReportExecutor::stop()` desregistra a notificação de report
  pendente, interrompe a espera de retry, aguarda de forma limitada a tentativa
  em voo e encerra a task sem destruir o executor
  (`components/issp_transport_154/src/issp154_report_executor.cpp`).

O delay de retry de 1000 ms virou `ulTaskNotifyTake` com timeout, preservando a
duração vigente e permitindo interrupção sem `xTaskAbortDelay`, opção de Kconfig
ou dependência externa, com flag atômica distinguindo a notificação de parada.
`Issp154Transport::end()` não recebeu semântica nova.

### 3.2 Fachada

`configureDeepSleep()` entrou na API pública exatamente com os nomes da seção 3
(`components/issp_app_154/include/SmartSysApp.h`). A política vive em um terceiro
TU target-agnóstico, `src/smart_sys_app_deep_sleep.cpp`, seguindo o precedente da
divisão já existente entre `smart_sys_app.cpp` e `smart_sys_app_hardware.cpp`:
ele alcança device, executor, transporte e monitor apenas por `SetupHooks`, o que
mantém a política testável com doubles e sem conhecer protocolo ou transporte.

Implementado ali: validação e cópia da configuração; conversão e limite do timer;
LED com polaridade, `DurationMs` e `UntilSleep`; captura da janela acordada na
entrada de `setup()`; causa do boot e LED como primeira operação de plataforma;
criação da task de lifecycle somente ao final de `InitializePlatform`
bem-sucedido; predicados separados de prontidão e entrega com polling de 10 ms;
sequência terminal na ordem obrigatória; e a arbitragem de três estados.

Duas notas de fidelidade:

- **o polling de 10 ms é exato neste projeto.** `CONFIG_FREERTOS_HZ=100`
  (`client_154/sdkconfig:1438`), então `pdMS_TO_TICKS(10)` vale um tick e o teto
  do contrato coincide com a granularidade do escalonador;
- **`quiesce()` itera o array de behaviors da fachada**, não o do device, como a
  análise da v0.10 recomendou: cobre estritamente mais quando o setup falhou
  antes de `RegisterCapabilities`, e a semântica de no-op torna isso seguro.

### 3.3 Arbitragem com factory reset

O detentor é um `std::atomic<std::uint8_t>` privado da fachada com os três
estados livre, deep sleep e factory reset. `FactoryResetService` ganhou um par
`acquire`/`release` opcional e passou a devolver `FactoryResetRequestResult`;
`ResetButtonMonitor` só consome o hold quando o pedido é aceito, e reapresenta o
pedido rejeitado enquanto o botão permanecer pressionado, sem exigir soltura e
nova pressão. `IFactoryResetRequester` é interface privada de
`components/issp_app_154/src/reset/`, então a mudança de assinatura não amplia
API reutilizável.

O monitor também ganhou `stop()` interno, idempotente e terminal, com o polling
passando a `ulTaskNotifyTake` usando `pollIntervalMs` como timeout. GPIO,
polaridade, hold e semântica do factory reset fora da transição não mudaram.

`persistNetwork()` permaneceu privado e opaco. O diagnóstico
`persistence_preemption_possible=true` é emitido quando o caminho forçado começa
com `SetupStage::InitializeNetwork` em curso, declarado como possibilidade e sem
alegar observação da janela privada.

### 3.4 Composição e renome

Renome aplicado integralmente com `git mv`, preservando a linhagem: fonte,
símbolo `IOTSMARTLINK154_PRODUCT_DOOR_SENSOR_BATTERY_H2`, rótulo de menu
`Door sensor battery H2`, seleção e branch no CMake, `client_154/sdkconfig` e as
referências em `Firmware-Variants-Menuconfig.md`, `SYSTEM-DOSSIER.md` e
`KNOWLEDGE-MAP.md`. Relatórios e registros históricos não foram tocados; a
sentença do item 37 daquela especificação, que registra a seleção usada à época
pelo Arquiteto para validar o produto, foi preservada com o nome observado
então.

`wake_led` entrou como recurso exigido pelo produto e oferecido pelo board
`Door Sensor Battery H2` no GPIO 13, ativo em nível alto. Nomes e símbolos
próprios do board não foram renomeados.

## 4. Questões devolvidas ao Arquiteto

### 4.1 `kImplStorageBytes` cresceu, o que reabre o item 39 de Firmware-Variants

A task privada de lifecycle tem pilha estática, como todo o restante do
repositório, e é membro de `SmartSysApp::Impl`. Isso obrigou a elevar
`kImplStorageBytes` de 10240 para 16384 bytes.

O item 39 de `Firmware-Variants-Menuconfig.md` aceita os slots fixos por tipo
"enquanto `kImplStorageBytes` não crescer e o `static_assert` permanecer
válido". O gatilho dessa condição foi atingido. Não é um bloqueio da
implementação — o `static_assert` continua sendo a guarda — mas é exatamente o
novo confronto que aquele item previa, e cabe ao Arquiteto decidir se o desenho
de slots fixos por tipo permanece aceito ou se o crescimento deve ser evitado de
outra forma (por exemplo, pilha da task fora de `Impl`, com o custo de deixar de
ser por instância).

Registro também que os 4096 bytes de pilha e o crescimento existem em toda
composição, inclusive nas que não habilitam deep sleep, porque a alocação é
estática.

### 4.2 Os números de política do produto a bateria são meus, não da especificação

A seção 5 atribui ao product firmware a decisão de ativação, duração máxima
acordado, intervalo e duração do indicador, e a especificação não fixa valores.
Para que a primeira composição exista, escolhi e documentei:
`maxAwakeTimeMs = 30000`, intervalo de 15 minutos e LED em `DurationMs` de
200 ms (`client_154/main/firmwares/door_sensor_battery_h2.cpp`).

São escolhas de produto sujeitas à confirmação do Arquiteto. Chamo atenção para
uma consequência já antecipada pela seção 6.1: com 30 s acordado e hold de
factory reset de 10 s, o hold é utilizável, mas a especificação não garante que
ele termine antes de um sleep antecipado.

## 5. Limitações e o que não foi feito

- **nenhum build, teste, flash ou execução em hardware.** A especificação proíbe
  no recorte, e a política de execução de testes também. A consequência honesta
  é que **nada aqui está compilado**: erros de compilação, o `static_assert` de
  `kImplStorageBytes` e o de `kHardwareStorageBytes` permanecem não verificados.
  Foi por isso que dimensionei o buffer com folga em vez de ajustá-lo ao mínimo;
- por isso o estado da implementação foi registrado como `In Progress`, e não
  como implementação concluída: o código está completo, as verificações técnicas
  não foram autorizadas, e limitação ausente não vira sucesso;
- os experimentos 10, 11, 19, 20 e 21 das análises permanecem abertos, agora
  também o desdobramento que mede os três termos do orçamento de
  DEEPSLEEP-AC-010, incluindo o caminho em que `initializeNvs()` recorre a
  `nvs_flash_erase()`;
- casos automatizados foram **escritos e não executados**: 15 casos de deep
  sleep em `components/issp_app_154/test_apps/smart_sys_app_test` e 2 casos de
  `beginQuiescence` em `components/issp_core/test_apps/issp_device_concurrency_test`.
  Eles usam a costura de `SetupHooks` prevista por DEEPSLEEP-AC-010 e nunca
  armam fonte de wakeup, tocam GPIO de LED ou dormem de verdade;
- os casos de deep sleep aguardam a task de lifecycle por observação com timeout
  e acrescentam uma folga de 50 ms antes de o `SmartSysApp` sair de escopo, para
  que a task alcance `vTaskDelete()`. É uma mitigação de janela, não uma prova de
  ausência de corrida; se a execução for autorizada e a suíte apresentar
  instabilidade, esse é o primeiro ponto a confrontar;
- `wakeLedOn_` é lido e escrito pela task do `esp_timer` e pela task de
  lifecycle sem sincronização. O pior caso é uma escrita redundante de nível
  desligado imediatamente antes do sleep; registro por ser um dado compartilhado
  e não por observar defeito.

## 6. Correspondência com os critérios de aceite

Correspondência por inspeção, não por execução.

| Critério | Onde | Situação |
|---|---|---|
| AC-001 Compatibilidade | `configureDeepSleep()` com `enabled=false` copia e não inicia nada; sem a chamada, nenhuma operação do TU de deep sleep é alcançada | Implementado, não executado |
| AC-002 Configuração | validações e colisão aditiva restrita ao `wake_led`, nos dois sentidos | Implementado, não executado |
| AC-003 Timer | conversão em `std::uint64_t`, limite derivado do target com margem, e repetição da validação no preparo com tratamento do retorno da API | Implementado, não executado |
| AC-004 LED | nível antes da direção, `DurationMs` por `esp_timer` one-shot, `UntilSleep` apagado antes do sleep, nada tocado em falha de `ValidateConfiguration` | Implementado, não executado |
| AC-005 Composição e renome | `wake_led` nas listas do CMake e alcance integral do renome | Implementado, não verificado por build |
| AC-006 Entrega | predicados separados; contagem zero não comprova prontidão; espera limitada pelo deadline | Implementado, não executado |
| AC-007 Deadline e quiescência | task única nascida ao final de `InitializePlatform`, caminho forçado imediato se expirado, transição já detida reconhecida, 600 ms com supressão de `end()` | Implementado, não executado |
| AC-008 Sleep forçado | registro de causa, contagem e estado; falha de quiescência não reabre trabalho; falha de fonte bloqueia o sleep | Implementado, não executado |
| AC-008A Arbitragem | detentor de três estados, hold não consumido em pedido rejeitado, monitor encerrado antes da quiescência | Implementado, não executado |
| AC-008B Persistência | diagnóstico de possibilidade sem nova API pública | Implementado; experimento pendente |
| AC-009 Wakeup | causa do boot registrada e distinguível; sleep sem timer permitido com diagnóstico | Implementado, não executado |
| AC-010 Evidência futura | costura de `SetupHooks` estendida e casos escritos | Preparado; execução não autorizada |

## 7. Encaminhamento

Encaminho para avaliação do Arquiteto com as duas questões da seção 4 e a
limitação central da seção 5. A operação que mais reduz risco agora é autorizar
um build H2 nas composições habilitada e desabilitada; ele não comprova
comportamento físico, mas resolve os `static_assert` e a compilação, que são
hoje as únicas incógnitas puramente mecânicas do trabalho.
