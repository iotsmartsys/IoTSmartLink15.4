# Relatório de implementação — wakeup por contato seco (v0.11)

**Classe da fonte:** Relatório

**Papel:** Engenheiro Implementador

**Especificação:** `docs/specs/Client-Deep-Sleep.md`

**Revisão confrontada:** v0.11, `Proposed`, Pronta e autorizada para
implementação; análise `Ready` para `ea95c77`

**Estado:** Em andamento [`In Progress`]

**Capacidade:** Engenheiro Implementador

**Data:** 12/08/2026

**Resultado:** o acréscimo da v0.11 — wakeup por transição do contato seco por
EXT1, com rearme alternado a cada boot — está implementado integralmente em
código, e o build canônico H2 foi executado com sucesso nas duas composições do
client e no test app afetado; testes, flash, monitor e hardware não foram
autorizados nem executados, e DEEPSLEEP-AC-012 permanece por natureza não
satisfeito, de modo que a implementação **não** é declarada concluída

> Este relatório registra a execução e suas limitações. Não altera fontes
> normativas além do estado de implementação, não promove estado normativo e
> não declara conclusão.

## 1. Gates confrontados antes de iniciar

- **análise `Ready`:** presente, confronto final r5 sobre a revisão `ea95c77`
  (`docs/reports/client-deep-sleep/analysis/2026-08-12-v11-implementability-analysis-r5.md`);
- **versão promovida para Pronta:** presente, registrada no cabeçalho e na
  seção 8 da especificação;
- **autorização de implementação da mesma versão:** presente, concedida pelo
  Arquiteto em 12/08/2026 para a v0.11.

Branch `spec/client-deep-sleep`, árvore limpa no início da atuação. Conforme o
EKOM 3.6, a autorização inclui o build canônico proporcional dos entregáveis
construíveis afetados; nenhuma outra operação foi executada.

## 2. Recorte executado

Implementei o acréscimo integral da v0.11: `ContactWakeupConfig` na seção 3, o
contrato da seção 4.2A, a composição e o recurso `dry_contact_wakeup` da
seção 5, a posição do preparo na ordem obrigatória da seção 6.1 e os critérios
DEEPSLEEP-AC-011 e AC-009 na parte que não depende de hardware. O contrato da
v0.10 não foi reaberto: nenhuma operação existente mudou de semântica, com a
única exceção documentada na seção 4.2 deste relatório.

## 3. O que foi implementado, por fonte

### 3.1 Contrato público (`components/issp_app_154/include/SmartSysApp.h`)

`ContactWakeupConfig { bool enabled; gpio_num_t pin; }` foi acrescentado com os
nomes exatos da seção 3, e `contactWakeup` entrou **ao final** de
`DeepSleepConfig`. Composições que não o declarem continuam válidas e o campo
permanece inerte, o que preserva DEEPSLEEP-AC-001; o `single_smart_plug`, que
nem chama `configureDeepSleep()`, foi construído para confirmar isso.

Nenhum valor novo em `AppResult`, `SetupStage` ou `AppState`, nenhuma operação
pública nova e nenhuma ampliação da API reutilizável: DEEPSLEEP-DEC-013 é
preservada porque a fachada usa o GPIO que o produto lhe entrega e nunca
consulta o estado lógico do `DigitalInputBehavior`.

### 3.2 Validação em duas etapas

- **`configureDeepSleep()`** rejeita com `AppResult::InvalidArgument` todo GPIO
  que o target não aceite como fonte de wakeup, usando
  `esp_sleep_is_valid_wakeup_gpio()`. A fachada não mantém lista própria de
  GPIOs (DEEPSLEEP-DEC-017). No ESP32-H2 essa função resolve para
  `RTC_GPIO_IS_VALID_GPIO`, isto é, a faixa GPIO 7 a 14 que o alvo declara
  (`SOC_RTCIO_PIN_COUNT=8`);
- **`SetupStage::ValidateConfiguration`** confronta a correspondência com a
  capability: GPIO sem capability de contato seco registrada e capabilities no
  mesmo GPIO com pulls divergentes falham nesse estágio com `InvalidArgument`,
  antes de tocar NVS, rádio, RTC ou GPIO. Como o confronto é feito apenas ali, a
  ordem entre registrar a capability e chamar `configureDeepSleep()` durante
  `AppState::Configuring` permanece insignificante.

### 3.3 Preparo da fonte na sequência terminal

`SmartSysApp::Impl::prepareContactWakeup()`
(`components/issp_app_154/src/smart_sys_app_deep_sleep.cpp`) executa, depois do
preparo do timer e antes de qualquer operação terminal:

1. reaplica ao pad `GPIO_MODE_INPUT` e o pull da configuração da capability
   correspondente, com a mesma forma que `DigitalInputBehavior::configureGpio()`
   já usa. É idempotente e não depende de `begin()` nem de `StartDevice`;
2. lê o nível elétrico com `gpio_get_level()`;
3. arma `esp_sleep_enable_ext1_wakeup_io()` para o **nível oposto** ao lido:
   nível alto observado arma `ESP_EXT1_WAKEUP_ANY_LOW` e vice-versa.

O retorno das duas APIs é confrontado; falha em qualquer passo aborta a
sequência antes de qualquer operação terminal, libera a arbitragem e preserva o
runtime acessível, exatamente como já ocorria com o timer. Com as duas fontes
habilitadas, o sucesso de uma não compensa a falha da outra (DEEPSLEEP-DEC-012).

Confirmei por leitura do IDF instalado que `esp_sleep_enable_ext1_wakeup_io()`
tem semântica por pino no H2 (`SOC_PM_SUPPORT_EXT1_WAKEUP_MODE_PER_PIN`), de
modo que uma chamada repetida com nível diferente reescreve o modo daquele pino
— o que sustenta a idempotência exigida pela seção 4.2A.

### 3.4 Composição, board e produto

- `client_154/main/CMakeLists.txt`: `dry_contact_wakeup` passou a ser recurso
  exigido pelo produto `door_sensor_battery_h2` e oferecido pelo board
  `Door Sensor Battery H2`. Composição sem o recurso falha antes do binário;
- `client_154/main/boards/door_sensor_battery_h2.cpp` e `board_model.hpp`:
  registram, em comentário, que o recurso é oferecido porque o contato está no
  GPIO 14, dentro da faixa elegível do target. Nenhum símbolo ou nome de board
  foi renomeado e nenhuma pinagem migrou para o produto;
- `client_154/main/firmwares/door_sensor_battery_h2.cpp`: habilita
  `contactWakeup` com o GPIO que recebeu do board, ao lado do timer de 15
  minutos já vigente (DEEPSLEEP-DEC-016). Os valores de `maxAwakeTimeMs` e da
  duração do LED não foram alterados, por não pertencerem ao acréscimo;
- `client_154/main/Kconfig.projbuild`: o texto de ajuda do produto passou a
  declarar as duas fontes e os dois recursos exigidos.

## 4. Decisões locais e desvios

### 4.1 Costura de `SetupHooks` para o preparo do contato

Acrescentei uma entrada `prepareContactWakeup(context, pin, pull)` à costura de
`SetupHooks`, no lugar simétrico ao `prepareTimerWakeup` já existente, com
fallback para as chamadas reais quando nula. Isso é o que DEEPSLEEP-AC-010
prevê como costura interna e é o único meio de os casos automatizados
observarem a posição do preparo e o bloqueio por falha sem tocar GPIO nem armar
EXT1. Não integra o contrato normativo do produto e não amplia API reutilizável.

Limite honesto dessa escolha: com a costura ativa, o double observa o pino e o
pull reaplicados, **não** o nível lido nem o nível armado. A inversão elétrica
em si só é evidenciável em hardware, que é justamente o que DEEPSLEEP-AC-012
exige.

### 4.2 Causa do boot passou a ser lida como bitmap

`beginPlatformPowerPolicy()` usava `esp_sleep_get_wakeup_cause()`, que retorna
uma única causa e, por ordem de teste interna, devolve `TIMER` quando timer e
EXT1 disparam juntos — hipótese que só passa a existir agora, com duas fontes
armadas. Isso tornaria o wakeup por contato indistinguível nesse caso, contra
DEEPSLEEP-AC-009. Passei a usar `esp_sleep_get_wakeup_causes()`, que devolve o
bitmap de todas as causas, e o log agora registra
`boot_cause=<cold_boot_or_reset|timer|contact|timer_and_contact|other>`,
`timer_wakeup=`, `contact_wakeup=` e o bitmap bruto.

Registro como desvio deliberado por dois motivos: altera o formato de um log
que a v0.10 já emitia (`raw=` passou de enum para bitmap) e a API singular
estava deprecada no ESP-IDF 6.0.1, o que era o único aviso de compilação do
componente. Se o Arquiteto preferir preservar o formato anterior, a reversão é
local a essa função.

### 4.3 Falta de `esp_timer` nos requisitos do test app

O build do test app `smart_sys_app_test` falhava desde a v0.10 — nunca
construída — porque `main/CMakeLists.txt` não declarava `esp_timer`, incluído
por `test_smart_sys_app.cpp`. Acrescentei `PRIV_REQUIRES esp_timer`. É correção
de build de artefato afetado por esta atuação, não mudança de comportamento.

### 4.4 O que **não** fiz

- não criei rate limit de wakeups por contato (DEEPSLEEP-DEC-015);
- não criei segundo caminho de publicação do estado: o report de cada boot
  continua saindo da capability de entrada com `reportOnStart=true`;
- não acrescentei polaridade lógica a `ContactWakeupConfig`;
- não criei acessor novo de board para elegibilidade: a declaração é o recurso
  `dry_contact_wakeup` da composição, que é o mecanismo da ADR-0002. Se o
  Arquiteto quiser a elegibilidade também como dado em `DryContactInputResource`,
  é decisão dele e não a antecipei.

## 5. Builds executados

Ambiente: macOS, ESP-IDF 6.0.1 em `~/.espressif/v6.0.1/esp-idf`, toolchain
`riscv32-esp-elf` 15.2.0, `IDF_TARGET=esp32h2` em todos os casos.

| Alvo | Comando | Diretório | Estado terminal | Código de saída |
|---|---|---|---|---|
| `client_154`, composição `door_sensor_battery_h2` + board `Door Sensor Battery H2` (a versionada em `client_154/sdkconfig`) | `idf.py build` | `client_154/build` | `Project build complete`, `sensor_154.bin` 0x4b870 bytes | 0 |
| `client_154`, composição `single_smart_plug` + board `Current client ESP32-H2 wiring` (deep sleep não configurado) | `idf.py -B build_single_plug_v11 -D SDKCONFIG=<tmp> -D SDKCONFIG_DEFAULTS=<tmp> build` | `client_154/build_single_plug_v11` | `Project build complete` | 0 |
| test app `smart_sys_app_test` | `idf.py -B build_h2_v11 build` | `components/issp_app_154/test_apps/smart_sys_app_test/build_h2_v11` | `Project build complete`, apenas build | 0 |

Notas:

- o primeiro build exigiu `idf.py fullclean` antes, porque o diretório
  existente fora configurado com outro interpretador Python; nenhum arquivo
  versionado foi alterado por isso;
- os builds alternativos usaram `SDKCONFIG` fora da árvore, de modo que
  `client_154/sdkconfig` permanece exatamente como versionado;
- sem avisos de compilação nos componentes do projeto após a mudança da
  seção 4.2;
- os `static_assert` de `kImplStorageBytes` e `kHardwareStorageBytes` passaram;
  `kImplStorageBytes` **não** precisou crescer nesta versão.

Confronto adicional da guarda de composição: configurar `door_sensor_battery_h2`
com o board `Current client ESP32-H2 wiring` falha na configuração com
`Incompatible IoTSmartLink15.4 composition ... missing physical resource`. O
recurso que aquele board acusa primeiro é `dry_contact_input`, porque ele não
oferece nenhum dos três; não construí um board artificial para isolar
`dry_contact_wakeup`, e registro isso como limitação da evidência de AC-005.

## 6. Casos automatizados escritos e não executados

Acrescentei 5 casos a
`components/issp_app_154/test_apps/smart_sys_app_test`, todos pela costura de
`SetupHooks`, sem tocar GPIO, sem armar EXT1 e sem dormir de verdade:

- GPIO fora da faixa elegível do target rejeitado em `configureDeepSleep()`;
- contato sem capability correspondente falha em `ValidateConfiguration`, sem
  executar nenhum hook;
- capabilities no mesmo GPIO com pulls divergentes falham nesse estágio, e com
  pulls iguais a configuração é aceita e o ciclo chega ao sleep;
- as duas fontes são preparadas antes de qualquer operação terminal, com o
  contato depois do timer, e o pino e o pull reaplicados são os da capability —
  inclusive com `configureDeepSleep()` chamado **antes** do registro dela;
- falha do preparo do contato bloqueia o sleep mesmo com o timer já armado.

Eles foram compilados, **não coletados e não executados**: a política de
execução de testes e esta especificação exigem autorização própria, que não foi
concedida. A suíte passa de 40 para 45 casos versionados.

## 7. Correspondência com os critérios de aceite

Correspondência por inspeção e build; nenhuma linha desta tabela é evidência de
execução.

| Critério | Onde | Situação |
|---|---|---|
| AC-001 Compatibilidade | `contactWakeup` acrescentado ao final da struct e inerte sem opt-in; composição `single_smart_plug` construída | Implementado; build ok |
| AC-005 Composição | `dry_contact_wakeup` exigido pelo produto e oferecido pelo board; guarda do CMake exercitada | Implementado; isolamento do recurso não evidenciado |
| AC-009 Wakeup | bitmap de causas com `contact`, `timer`, `timer_and_contact` e `cold_boot_or_reset`; `no_wakeup_source` só com as duas fontes desabilitadas | Implementado, não executado |
| AC-011 Contato | validação em duas etapas, reaplicação do pad, leitura e armação do nível oposto na posição obrigatória, bloqueio por falha de qualquer fonte | Implementado, não executado |
| AC-012 Retenção do nível | nada em código ou build satisfaz este critério | **Não satisfeito por natureza**; exige hardware |
| AC-010 Evidência futura | costura de `SetupHooks` estendida e 5 casos escritos | Preparado; execução não autorizada |

## 8. Limitações e encaminhamento

- **DEEPSLEEP-AC-012 continua aberto e é o risco material da versão.** Ausência
  de wakeup espúrio com contato estável, wakeup efetivo nos dois sentidos, o
  caminho sem `StartDevice` e o comportamento sob repique só podem ser
  observados em hardware. Build não é evidência disso;
- a cadeia de HOLD do pad durante o sleep e sua liberação antes de `app_main()`
  foi tomada da especificação, que a declara confirmada por leitura do target;
  não a reverifiquei nem a observei;
- os experimentos abertos das análises anteriores permanecem abertos, incluindo
  a preempção de `persistNetwork()` (AC-008B) e o orçamento de tempo acordado
  (AC-010);
- `wakeLedOn_` continua sendo dado compartilhado sem sincronização, como já
  registrado na v0.10; nada nesta versão alterou esse ponto;
- **incorporação normativa pendente do Arquiteto:**
  `Firmware-Variants-Menuconfig.md` ainda descreve o board `Door Sensor Battery
  H2` como oferecendo entrada de contato, botão e `wake_led` (itens em torno das
  linhas 263 e 398). O recurso `dry_contact_wakeup` é acréscimo declarado pela
  seção 2 da v0.11 àquela autoridade e não o incorporei: diferentemente do
  renome da v0.10, a especificação não delega essa edição mecânica ao
  Implementador. Atualizei apenas mapa e dossiê, que são registros de estado.

Encaminho ao Arquiteto para avaliação, com o desvio da seção 4.2 e a limitação
da seção 4.4 explícitos. A operação que mais reduz risco agora é autorizar o
experimento de hardware de DEEPSLEEP-AC-012 no par produto/board da seção 5,
com o contato acionado nos dois sentidos e um boot que alcance a sequência
terminal sem `StartDevice`.
