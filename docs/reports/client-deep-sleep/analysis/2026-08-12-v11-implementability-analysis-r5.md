# Análise de implementabilidade da v0.11 — confronto final da revisão corrente

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/Client-Deep-Sleep.md`

**Revisão confrontada:** v0.11, Draft de 12/08/2026, conforme o commit `ea95c77`

**Estado:** Concluído

**Capacidade:** Engenheiro Analista

**Data:** 12/08/2026

**Relação com relatórios anteriores:** quarta análise da v0.11 e confronto final
pedido pela própria especificação. A r4 classificou como `Ready` o commit
`5b2cc09`; a fonte normativa mudou depois disso, em `ea95c77`, e é essa revisão
que este relatório confronta

**Resultado:** a única cláusula acrescentada depois da r4 — rejeição de pulls
divergentes entre capabilities de contato seco no GPIO do wakeup — é
implementável no estágio que a especificação indica, com dados que a fachada já
copia, sem ampliar API reutilizável e sem criar validação global entre pares
antes aceitos. Reverifiquei por conta própria os fatos de target que sustentam
o acréscimo inteiro. Não encontro bloqueador, decisão normativa ausente nem
contradição interna. Recomenda-se **prontidão**, com quatro precisões declaradas
não bloqueantes e os experimentos de hardware já normativos; nenhuma execução
realizada

> Este relatório registra evidências e recomendações. Não altera a fonte
> normativa, não promove estado e não autoriza implementação ou testes.

## 1. Recorte e método

Antes de qualquer leitura de mérito, verifiquei se o `HEAD` havia avançado desde
a análise anterior, e não apenas se a árvore estava limpa. Havia: `ea95c77`,
"docs: resolve pulls divergentes no wakeup v0.11", posterior ao commit `5cbb3f2`
em que a r4 foi registrada. O `Ready` da r4 pertence ao texto de `5b2cc09` e não
alcança esta revisão; é isso que a própria seção de gates da especificação
declara, e é a razão de existir este confronto.

O delta normativo desde `5b2cc09` é pequeno e integralmente identificado: um
item novo em 4.2A, duas sentenças em DEEPSLEEP-AC-011, uma sentença em
DEEPSLEEP-DEC-017, o registro da confirmação do Arquiteto e a atualização dos
campos de estado e de gates. Nenhuma outra linha do contrato mudou.

Por isso o método desta rodada foi duplo: confrontei em profundidade a cláusula
nova contra o código vigente, e **reverifiquei do zero**, na fonte do ESP-IDF
6.0.1 instalado, os fatos de target de que o acréscimo da v0.11 depende — em vez
de herdá-los da r4. A seção 4 lista o que reli e onde.

Nenhum build, teste, flash ou execução em hardware. Árvore limpa em
`spec/client-deep-sleep`, derivada da `main`.

## 2. A cláusula nova é implementável, e no estágio em que a especificação a põe

A cláusula exige três coisas: que a rejeição ocorra em `ValidateConfiguration`,
que o resultado seja `AppResult::InvalidArgument`, e que pulls iguais sejam
tratados como equivalentes. As três cabem no que existe.

**O dado necessário já está copiado e é completo.** `app::DoorSensorConfig`
carrega `pin` e `pull` (`SmartSysApp.h:33-45`),
e `addDoorSensorCapability()` guarda a configuração inteira em
`doorSensorConfigs_[doorSensorCount_]`
(`smart_sys_app.cpp:260`),
com `doorSensorCount_` como o comprimento válido do array
(`smart_sys_app_impl.hpp:158-163`).
A varredura que a cláusula pede — todas as capabilities cujo `pin` seja o do
wakeup — é uma passagem linear sobre esse array, com os mesmos dados em qualquer
ordem de registro. É exatamente a forma que `configureDeepSleep()` já usa para a
colisão do `wake_led`
(`smart_sys_app_deep_sleep.cpp:182-185`).

**O estágio existe e é o lugar certo.** `setup()` marca
`currentStage_ = SetupStage::ValidateConfiguration` e, hoje, só consulta
`lastConfigurationResult_`
(`smart_sys_app.cpp:324-332`).
O cruzamento novo entra ali, e a propriedade que a seção 6 exige se preserva
sozinha: o bloco inteiro é anterior a `beginPlatformPowerPolicy()` e a
`hooks_.initializePlatform()`, de modo que a rejeição continua ocorrendo antes de
tocar NVS, rádio, RTC ou GPIO, sem LED, sem task de lifecycle e sem deep sleep
neste boot. O caminho de saída já existe e produz o `SetupResult` pedido:
`fail(SetupStage::ValidateConfiguration, AppResult::InvalidArgument)`
(`smart_sys_app.cpp:299-306`).

**A ordem das chamadas continua irrelevante**, como AC-011 exige, precisamente
porque o cruzamento é feito em `setup()` e não dentro de
`configureDeepSleep()` ou de `addDoorSensorCapability()`: quando `setup()` roda,
`AppState::Configuring` já se encerrou e o conjunto está completo.

**A cláusula não cria validação global.** Ela é condicionada ao "GPIO
solicitado", isto é, só existe quando o contato está habilitado. Com deep sleep
ou contato desabilitado, nenhuma comparação nova entre capabilities passa a
existir — que é a mesma contenção que a seção 5 já impõe ao `wake_led`. Não há
ampliação da API reutilizável, não há valor novo em `AppResult`, `SetupStage` ou
`AppState`, e nenhum componente ISSP compartilhado é tocado.

**E a equivalência afirmada é real, não conveniente.** O que o preparo reaplica é
um `gpio_config_t` cujos únicos campos derivados do `pull` são `pull_up_en` e
`pull_down_en`
(`digital_input_behavior.cpp:61-73`).
Duas capabilities com o mesmo `pull` produzem literalmente a mesma escrita; com
`pull` divergente, produzem escritas incompatíveis no mesmo pad, e nenhuma ordem
de varredura é defensável. A cláusula rejeita exatamente o caso indeterminado e
aceita exatamente o caso determinado. É a saída correta entre as duas que a r4
havia oferecido: "a primeira correspondência" teria tornado normativa uma ordem
que a própria especificação declara irrelevante.

## 3. O resto do acréscimo continua fechado

Reconfrontei, contra o texto de `ea95c77`, os pontos que sustentam a v0.11 além
da cláusula nova, e nenhum foi afetado por ela:

- **momento da armação.** 4.2A, a ordem obrigatória de 6.1, AC-011 e DEC-014
  dizem a mesma coisa nos quatro lugares: no início da sequência terminal, depois
  do preparo do timer, antes de qualquer operação terminal. O item 1 de
  `runTerminalSequence()` é onde isso cabe, e o aborto com
  `releaseDeepSleepTransition()` antes de qualquer passo terminal já tem a forma
  exigida
  (`smart_sys_app_deep_sleep.cpp:507-529`);
- **o pad reaplicado sobrevive até o sleep.** Entre o item 1 e
  `esp_deep_sleep_start()` nada mais toca o pad do contato:
  `DigitalInputBehavior::quiesce()` apenas para e destrói o timer
  (`digital_input_behavior.cpp:386-401`)
  e `releaseWakeLed()` atua só no pino do LED
  (`smart_sys_app_deep_sleep.cpp:301-317`).
  Isso importa porque o HOLD do EXT1 não é aplicado no item 1: ele é aplicado
  dentro de `esp_deep_sleep_start()`, no item 7, sobre a configuração que o pad
  tiver naquele instante;
- **idempotência com o behavior já iniciado.** A reaplicação escreve os mesmos
  registradores com os mesmos valores, sob um callback de amostragem que apenas
  lê o pino; não altera nível, direção nem interrupção;
- **compatibilidade da API.** `contactWakeup` entra ao final de
  `DeepSleepConfig`; a fábrica única dos casos existentes usa designated
  initializers e omite o campo novo
  (`test_smart_sys_app.cpp:160-179`),
  que é inicializado por valor e permanece inerte. Nenhum caso existente precisa
  de edição;
- **composição.** `dry_contact_wakeup` entra como mais um item nas listas
  `required_resources` e `offered_resources`
  (`client_154/main/CMakeLists.txt:10-49`);
  o board já declara contato em GPIO 14 com `PullUp`
  (`boards/door_sensor_battery_h2.cpp:12-16`),
  e o produto já entrega esse recurso à fachada
  (`firmwares/door_sensor_battery_h2.cpp:52-67`).
  O timer de 15 minutos de DEC-016 é o valor que o produto já usa;
- **relações normativas.** `ISSP-Reusable-Components.md` v1.1,
  `ISSP-Architecture.md` v1.2, `ISSP-Commissioning.md` v1.0, ADR-0002 e
  `Repository-Test-Execution-Policy.md` v0.4 permanecem preservadas. As emendas a
  `Firmware-Variants-Menuconfig.md` e a `ISSP-Configurable-Bootstrap.md` v1.5
  estão declaradas e a cláusula nova não amplia nenhuma delas;
- **teste de fronteira.** Não se aplica. A capacidade não é inexistente na
  baseline: é uma varredura de configuração num estágio que já existe. Não cria
  lifecycle, ownership, arbitragem entre subsistemas nem consumidor fora do
  recorte.

## 4. Fatos de target reverificados nesta rodada

Reli a fonte do ESP-IDF **6.0.1** — o instalado em `~/.espressif/v6.0.1/esp-idf`,
que é o que as fontes normativas do repositório fixam — e confirmo, por leitura
própria e não por herança:

- `esp_sleep_is_valid_wakeup_gpio()` existe e é puro: resolve para
  `RTC_GPIO_IS_VALID_GPIO()` (`sleep_modes.c:1874-1883`), que é
  `rtc_gpio_is_valid_gpio()`, uma indexação em `rtc_io_num_map`
  (`esp_driver_gpio/src/rtc_io.c:31-38`). Não inicializa periférico, o que o
  torna chamável em `AppState::Configuring`, que é onde AC-011 o exige;
- é o **mesmo** predicado que `esp_sleep_enable_ext1_wakeup_io()` aplica
  internamente a cada bit da máscara (`sleep_modes.c:1946`), de modo que a
  validação antecipada da fachada não pode divergir da da API;
- no ESP32-H2 o mapa resolve para **GPIO 7 a 14**
  (`esp_hal_gpio/esp32h2/rtc_io_periph.c:9-37`), com `SOC_RTCIO_PIN_COUNT = 8`
  (`soc/esp32h2/include/soc/soc_caps.h:251`). O GPIO 14 da composição da seção 5
  está dentro da faixa;
- o H2 declara `SOC_PM_SUPPORT_EXT1_WAKEUP` e
  `SOC_PM_SUPPORT_EXT1_WAKEUP_MODE_PER_PIN` (`soc_caps.h:513-514`), e não declara
  `SOC_RTCIO_INPUT_OUTPUT_SUPPORTED`. É por isso que `ext1_wakeup_prepare()` toma
  o ramo digital: rotea o pad para função digital, habilita a entrada e chama
  `rtcio_hal_hold_enable()` **incondicionalmente**
  (`sleep_modes.c:2064-2071`) — sem depender de domínio de RTC_PERIPH nem de
  opção de Kconfig, como o parágrafo de retenção de 4.2A afirma;
- o HOLD é liberado no boot seguinte antes de `app_main()`:
  `esp_deep_sleep_wakeup_io_reset()` (`sleep_gpio.c:231-245`) é chamado em
  `cpu_start.c:845-849` sob a condição `RESET_REASON_CORE_DEEP_SLEEP`.

Com isso, a cadeia que a especificação descreve é verificável por leitura em toda
a sua extensão lógica, e o que permanece físico é apenas o elo elétrico — a
suficiência do pull diante de fuga, impedância e ruído —, que é precisamente o
que DEEPSLEEP-AC-012 reserva a hardware e que nenhuma leitura ou build satisfaz.

## 5. Precisões declaradas não bloqueantes

Nenhuma destas condiciona a prontidão. Registro-as porque são consequência de
bateria ou de custo de evidência, e a decisão sobre elas é do Arquiteto.

**5.1 — A cláusula nova fecha um membro de uma família, não a família.** A
ambiguidade de dois donos para o mesmo pino permanece aberta em duas outras
formas que o wakeup por contato passa a alcançar: uma capability de saída
(`addSwitchPlugCapability`) registrada no mesmo GPIO do contato, e o GPIO do
botão de factory reset coincidindo com ele. Em ambas, a fachada reaplicaria
`GPIO_MODE_INPUT` sobre um pad de outro dono. Declaro não bloqueante porque isso
não é criado pela v0.11 — as duas colisões já seriam patológicas na v0.10 — e
porque a seção 5 decide explicitamente restringir a validação de colisão ao
`wake_led`, o que é uma decisão vigente do Arquiteto e não uma omissão. Registro
também o que **está** fechado por transitividade: contato e `wake_led` nunca
podem coincidir, porque `configureDeepSleep()` compara o LED às capabilities já
registradas
(`smart_sys_app_deep_sleep.cpp:176-192`)
e `addDoorSensorCapability()` faz a comparação inversa
(`smart_sys_app.cpp:236-240`).

**5.2 — `DigitalInputPull::Floating` é um pull válido.** Se a capability
correspondente o declarar, a reaplicação é eletricamente inócua e o HOLD congela
um pad flutuante durante o sleep, com wakeup espúrio praticamente garantido. A
especificação não rejeita esse caso, e entendo que corretamente: 4.2A atribui a
suficiência elétrica ao board e AC-012 a submete a evidência em hardware. O board
da seção 5 usa `PullUp`. Registro por ser consequência de bateria.

**5.3 — o preparo do contato precisa de costura para caber na evidência
automatizada.** Diferentemente do timer, que tem `prepareTimerWakeup` em
`SetupHooks`, o preparo do contato tocaria `gpio_config()` e
`esp_sleep_enable_ext1_wakeup_io()` diretamente — e o app de testes vigente
declara deliberadamente nunca tocar driver de GPIO, razão pela qual mantém o LED
desabilitado em todo caso que alcança `setup()`
(`test_smart_sys_app.cpp:170-171`).
Sem uma entrada análoga em `SetupHooks`, os casos que AC-011 e AC-008 tornam
interessantes — falha de preparo do contato bloqueia o sleep; caminho forçado sem
`StartDevice` — não têm como ser cobertos por doubles. DEEPSLEEP-AC-010 já
autoriza estender `SetupHooks` como costura interna sem integrar o contrato
normativo, de modo que isto é orientação de implementação, não defeito.

**5.4 — ambiente de build.** Os fatos de target desta análise valem para o
ESP-IDF 6.0.1 em `~/.espressif/v6.0.1/esp-idf`, que é o que as fontes normativas
fixam. Há também um `~/esp/v5.5.1/esp-idf` na máquina, e `IDF_PATH` não está
definido na sessão. Quando uma especificação futura autorizar build ou execução,
a seleção explícita do 6.0.1 é parte da evidência, não detalhe de ambiente.

Herdo ainda, sem alteração, a observação não bloqueante da r4 de que
`ISSP-Configurable-Bootstrap.md` ainda não incorporou a emenda da v0.10 e convém
aplicar as duas emendas juntas.

## 6. Componentes impactados

| Fonte | Mudança | Natureza |
|---|---|---|
| `components/issp_app_154/include/SmartSysApp.h` | `ContactWakeupConfig` (`enabled`, `pin`), campo ao final de `DeepSleepConfig`, costura de preparo do contato em `SetupHooks` | aditiva, compatível |
| `components/issp_app_154/src/smart_sys_app.cpp` | em `ValidateConfiguration`: correspondência contato/capability e rejeição de pulls divergentes no mesmo GPIO | validação, sem tocar hardware |
| `components/issp_app_154/src/smart_sys_app_deep_sleep.cpp` | elegibilidade por `esp_sleep_is_valid_wakeup_gpio()`, reaplicação de modo e pull, leitura do nível, armação de EXT1 no nível oposto, causa `EXT1`, condição de `no_wakeup_source` | política, local |
| `components/issp_app_154/src/smart_sys_app_impl.hpp` | declarações correspondentes | privada |
| `client_154/main/CMakeLists.txt` | `dry_contact_wakeup` nas listas exigida e oferecida | composição |
| `client_154/main/firmwares/door_sensor_battery_h2.cpp` | `contactWakeup` a partir do board | produto |
| `client_154/main/boards/*` | declaração do recurso apto a wakeup | board |
| `components/issp_app_154/test_apps/smart_sys_app_test` | casos novos, incluindo pulls divergentes e o caminho terminal sem `StartDevice`; os casos existentes seguem válidos sem edição | evidência |

Nenhum componente ISSP compartilhado é tocado. Coordenador, protocolo ISSP, ACK e
retry permanecem fora do recorte.

## 7. Experimentos e verificações necessários

A cláusula acrescentada depois da r4 é validada em tempo de configuração e **não
acrescenta experimento**. O conjunto permanece o da r4:

- herdados e abertos: 10, 11, 19, 20 e 21, mais a medição separada dos três
  termos do orçamento de DEEPSLEEP-AC-010;
- **22 — DEEPSLEEP-AC-012, retenção do nível**, no par produto/board da seção 5:
  ausência de wakeup espúrio com o contato estável em cada nível ao longo de um
  período completo de timer, e wakeup efetivo na transição nos dois sentidos;
- **23 — repique e acionamentos sucessivos**, com wakeups contados e corrente
  medida, incluindo o ciclo gasto pelo wakeup imediato descrito em 4.2A;
- **24 — report em todo boot**, que o `reportOnStart` publique em boot por EXT1
  como publica em boot por timer;
- **26 — caminho sem `StartDevice`**, normativo dentro de AC-012: provocar
  `NotReady` em `InitializeNetwork` com o contato habilitado e demonstrar que o
  pad reaplicado sustenta o nível durante o sleep.

Leitura de código e build não satisfazem nenhum deles, e esta especificação
corretamente não autoriza executá-los.

## 8. Classificação e recomendação

**Pronta** [`Ready`].

A revisão corrente da v0.11 comporta-se na baseline e está inteiramente definida.
A cláusula que a distingue de `5b2cc09` escolheu a saída que não torna normativa
uma ordem que a própria especificação declara irrelevante; ela é implementável no
estágio indicado, com dados já copiados, sem API nova, sem ampliação da API
reutilizável e sem criar validação global entre pares antes aceitos. Os fatos de
target que sustentam o acréscimo inteiro foram reverificados nesta rodada na
fonte do ESP-IDF 6.0.1, e a cadeia lógica fecha; o que resta é físico e está
corretamente reservado a DEEPSLEEP-AC-012.

Não encontro bloqueador estrutural, decisão normativa ausente nem contradição
interna. As quatro precisões da seção 5 são declaradas **não bloqueantes** e
nenhuma delas é condição de prontidão.

Sobre independência, como as regras comuns exigem quando ela é material: esta
análise foi produzida em sessão distinta, sem memória das rodadas anteriores, e
reverifiquei na fonte do ESP-IDF os fatos de target em vez de herdá-los da r4 —
as citações coincidiram. Nisso ela é materialmente mais independente do que a r4
declarou ser. Mas continua sendo a mesma capacidade, no mesmo repositório, e li a
r4 antes de concluir; a convergência entre as duas não deve ser lida como duas
observações inteiramente separadas.

A recomendação informa o Arquiteto. Não certifica implementabilidade de forma
absoluta, não promove a especificação para `Pronta` e não autoriza implementação,
build, teste ou execução em hardware.
