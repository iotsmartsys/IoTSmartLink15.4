# Análise de implementabilidade da v0.11 — revisão 2

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/Client-Deep-Sleep.md`

**Revisão confrontada:** v0.11, Draft de 11/08/2026

**Estado:** Concluído

**Capacidade:** Engenheiro Analista

**Data:** 12/08/2026

**Substitui:** `2026-08-12-v11-implementability-analysis.md`, que permanece no
repositório e cujo bloqueador 2 esta revisão **corrige**

**Resultado:** refeita por ordem do Arquiteto sobre a mesma revisão da
especificação e a mesma árvore, sem insumo novo. A re-derivação confirmou o
bloqueador da faixa elegível de GPIO e **derrubou o segundo bloqueador**: o
critério DEEPSLEEP-AC-011 resolve a ambiguidade de momento que o relatório
anterior classificou como decisão ausente. Resta **um** defeito, de uma
sentença. Nenhuma execução realizada

> Este relatório registra evidências e recomendações. Não altera a fonte
> normativa, não promove estado e não autoriza implementação ou testes.

## 1. Por que há uma revisão e o que mudou

O Arquiteto ordenou refazer a análise. Não houve insumo novo: a especificação
está na mesma v0.11 de 11/08/2026, a árvore continua limpa em
`spec/client-deep-sleep` e o único commit desde então é o do próprio relatório
anterior. Refiz portanto a derivação, tratando as minhas próprias conclusões
como hipóteses a derrubar, que é o único modo de uma repetição valer alguma
coisa.

Uma delas caiu. Registro a correção antes de tudo, porque ela muda a leitura do
que falta:

| Ponto | Relatório anterior | Esta revisão |
|---|---|---|
| Faixa elegível de GPIO / GPIO 7 | Bloqueador | **Confirmado como bloqueador**, com o mesmo fundamento |
| Momento da leitura do contato | Bloqueador | **Rebaixado a precisão de redação**: DEEPSLEEP-AC-011 já decide |
| `activeHigh` sem efeito | Precisão | Mantida |
| Contato sem capability que configure o pad | Precisão | Mantida |
| Cadeia de retenção por HOLD | Mais sustentada por leitura que a especificação supõe | Confirmada, e com o elo da condição `RTC_PERIPH` agora verificado |

Classificação inalterada, mas por um defeito em vez de dois — e é uma diferença
material para o Arquiteto, porque o esforço de correção passa de duas decisões a
uma sentença errada.

## 2. Correção: o momento da leitura do contato não é decisão ausente

O relatório anterior sustentou que a seção 4.2A ("**imediatamente antes de
dormir**, e depois de preparado o timer") e a ordem obrigatória da seção 6.1 (o
contato no item 1, antes de monitor, quiescência, espera por entrega e `stop()`)
colocavam a mesma operação em pontos incompatíveis, e que escolher entre elas
seria decisão normativa.

Reexaminando, essa leitura estava errada por ter parado antes do critério de
aceite. DEEPSLEEP-AC-011 diz, textualmente:

> Com as duas fontes habilitadas, **ambas são armadas antes de qualquer operação
> terminal** e a falha de uma bloqueia o sleep.

Isso não é ambíguo, e é o critério — a parte da especificação que define o
aceite. Ele coincide com a ordem obrigatória da seção 6.1 e com a própria
cláusula "e depois de preparado o timer" da seção 4.2A, que já situa a operação
dentro do passo de preparo das fontes. Três dos quatro lugares convergem no item
1.

O que sobra é uma expressão infeliz: "imediatamente antes de dormir", repetida em
DEEPSLEEP-DEC-014. E, lendo DEC-014 inteira, fica claro que ela nem sequer está
falando de instante:

> o nível oposto de EXT1 é calculado a partir do nível elétrico lido
> imediatamente antes do sleep. **O estado lógico confirmado pelo debounce não
> participa do rearme.**

O contraste que a decisão estabelece é entre **elétrico e lógico**, não entre
cedo e tarde. "Imediatamente antes do sleep" opõe-se a "em algum ponto anterior
do boot", que é onde o estado do debounce teria sido formado — não ao item 1 da
sequência terminal.

Logo o implementador não enfrenta escolha: arma no item 1, com o timer, antes de
qualquer operação terminal, que é o que a garantia de abortar sem tornar o
dispositivo inacessível exige de qualquer forma. Não há decisão a devolver.

Permanece a recomendação de redação, agora **não bloqueante**: trocar
"imediatamente antes de dormir" por algo como "no preparo das fontes de wakeup,
depois do timer e antes de qualquer operação terminal", em 4.2A e em DEC-014,
para que o texto pare de sugerir um instante que a ordem obrigatória não
concede.

E permanece válida a observação de consequência, que continua útil ao
dimensionamento: armar no item 1 é **autocorretivo**. Se o contato transitar
entre a armação e o sleep, o pino já estará no nível armado quando o EXT1 entrar
em vigor, o wakeup dispara de imediato, o dispositivo acorda, reporta o estado
novo e rearma para o oposto. Não há transição perdida; há um ciclo acordado
gasto — despesa que o experimento 23 deve medir junto do repique, já que
DEEPSLEEP-DEC-015 optou por não impor limite de taxa nesta versão.

## 3. Bloqueador confirmado: a exceção do GPIO 7

Reconfrontei a cadeia inteira, e ela se fecha do mesmo modo. A seção 4.2A
afirma que "a faixa é GPIO 7 a 14, e o GPIO 7 é RTC GPIO mas **não** é conduzido
para wakeup externo". No ESP-IDF 6.0.1 instalado, o GPIO 7 é aceito:

- `esp_sleep_enable_ext1_wakeup_io()` rejeita com `ESP_ERR_INVALID_ARG` o pino
  que não passe em `esp_sleep_is_valid_wakeup_gpio()`
  (`components/esp_hw_support/sleep_modes.c:1935-1953`);
- no ESP32-H2, `SOC_RTCIO_PIN_COUNT` é 8, então essa função devolve
  `RTC_GPIO_IS_VALID_GPIO()` (`sleep_modes.c:1874-1883`;
  `components/soc/esp32h2/include/soc/soc_caps.h:251`);
- o macro é `rtc_gpio_is_valid_gpio()`, verdadeiro quando
  `rtc_io_num_map[gpio] >= 0` (`components/esp_driver_gpio/src/rtc_io.c:32-39`);
- no H2 esse mapa dá canal a **GPIO 7 até 14 inclusive**, e −1 aos demais
  (`components/esp_hal_gpio/esp32h2/rtc_io_periph.c:9-38`); o GPIO 7 é o canal 0
  (`components/soc/esp32h2/include/soc/rtc_io_channel.h:10`);
- e o `soc_caps.h` registra a intenção do silício sem exceção: "GPIO7~14 on
  ESP32H2 can support chip HP peripheral powerdown-ed sleep wakeup through EXT1
  wake up" (`soc_caps.h:227`).

Considerei, nesta passagem, se o defeito não seria apenas prosa errada ao lado
de uma regra clara — afinal a mesma seção manda derivar a elegibilidade "das
capacidades declaradas pelo target, não de uma lista de produto", e essa regra
sozinha basta para escrever o código. Concluí que não basta, por dois motivos:

- a sentença errada não é comentário lateral: ela está no bullet que **define o
  requisito** ("contato habilitado exige GPIO elegível para wakeup externo no
  target") e se apresenta como o conteúdo da faixa. Um implementador que a leia
  literalmente escreve a exclusão do GPIO 7 à mão, isto é, exatamente a lista de
  produto que o bullet seguinte proíbe. As duas instruções não podem ser
  obedecidas juntas;
- e a correção não é minha para fazer, porque as duas saídas têm consequências
  normativas distintas. Se a cláusula for engano, ela sai e a derivação vale
  sozinha. Se houver razão real para excluir o GPIO 7 — elétrica, de layout, de
  encapsulamento —, então ela não é do target e sim do board, e o lugar de
  declará-la é o mecanismo que a seção 5 já criou: "o board model ... declara se
  sua entrada de contato é elegível para wakeup externo". Essa segunda saída
  preserva as duas regras ao mesmo tempo, e é a que eu recomendaria; mas escolher
  entre engano e exclusão intencional é do Arquiteto.

Registro de novo, para calibrar urgência: o impacto operacional na primeira
composição é **nulo**, porque ela usa o GPIO 14. O que está em jogo é o texto do
critério, que fala da faixa e não do produto, e a fidelidade do código a ele.

## 4. Verificações novas desta passagem

Refazer só se justifica se produzir evidência que a primeira passagem não tinha.
Estas são as que acrescentei.

### 4.1 A condição `RTC_PERIPH` da seção 4.2A está certa; a da "opção" está errada

A seção 4.2A condiciona a retenção do pull a dois fatores: "o alvo não possui
domínio `RTC_PERIPH` desligável **e** a opção correspondente está habilitada".
Verifiquei os dois separadamente, o que a passagem anterior não fez:

- **primeiro fator, correto.** O ESP32-H2 não declara
  `SOC_PM_SUPPORT_RTC_PERIPH_PD` — o macro simplesmente não existe no
  `soc_caps.h` do alvo. É por isso que o ramo condicional de
  `ext1_wakeup_prepare()` que só segura o pad quando `RTC_PERIPH` seria desligado
  (`sleep_modes.c:2056-2060`) nem é compilado para o H2;
- **segundo fator, incorreto.** No H2 vale o ramo `#else`, que rotea o pad para
  função digital, habilita a entrada e chama `rtcio_hal_hold_enable()`
  **incondicionalmente**, sem consultar Kconfig algum (`sleep_modes.c:2064-2071`).
  A opção `CONFIG_ESP_SLEEP_GPIO_ENABLE_INTERNAL_RESISTORS`, habilitada no
  projeto (`client_154/sdkconfig:1142`), pertence ao caminho alternativo
  `esp_sleep_gpio_wakeup_prepare_on_hp_periph_powerdown()`
  (`sleep_modes.c:2127-2136`), destinado a alvos **sem** RTCIO — não é o do H2 e
  não participa deste encadeamento.

Ou seja: a retenção não depende de configuração do projeto. Ela é propriedade do
caminho EXT1 do H2.

### 4.2 O que o HOLD do H2 é, concretamente

Desci ao registrador, o que sustenta a afirmação em vez de apenas repeti-la:
`rtcio_hal_hold_enable()` é `rtcio_ll_force_hold_enable()`
(`components/esp_hal_gpio/include/hal/rtc_io_hal.h:236`), que escreve um bit em
`LP_AON.gpio_hold0` (`components/esp_hal_gpio/esp32h2/include/hal/rtc_io_ll.h:83-86`).
É hold de pad no domínio LP sempre alimentado, e o pad permanece em função
digital — portanto o que fica congelado é a configuração que o
`DigitalInputBehavior` programou acordado, incluindo o pull-up interno vindo do
`InputPull::PullUp` do board (`digital_input_behavior.cpp:64-74`;
`boards/door_sensor_battery_h2.cpp:12-16`).

Continua valendo o achado da passagem anterior de que o hold é desfeito sozinho
no boot seguinte: `cpu_start.c:845-849` chama `esp_deep_sleep_wakeup_io_reset()`
quando o reset é `RESET_REASON_CORE_DEEP_SLEEP`, e essa função libera o hold de
cada pino EXT1 (`sleep_gpio.c:231-245`) antes de `app_main()`. O pad não fica
preso, e o `reportOnStart` de cada boot lê nível vivo.

### 4.3 A evidência automatizada existente não é invalidada pelo acréscimo

Delimitação de impacto que faltava. Os 15 casos de deep sleep constroem a
configuração por uma única fábrica com designated initializers,
`makeDeepSleepConfig()`
(`test_apps/smart_sys_app_test/main/test_smart_sys_app.cpp:160-172`).
Acrescentar `contactWakeup` ao final de `DeepSleepConfig` é compatível na fonte,
o campo fica value-inicializado com `enabled=false` e o contato permanece inerte
em todos os casos existentes. Nenhum caso escrito precisa mudar para continuar
significando o que significa — o que também confirma, do lado dos testes, a
afirmação da seção 3 sobre composições que não declarem o campo.

### 4.4 A compatibilidade de AC-001 é estrutural, não apenas configurável

`configureDeepSleep()` é chamado por exatamente um produto,
`door_sensor_battery_h2.cpp:76-91`. O `single_smart_plug` não o chama, de modo
que nenhuma operação do TU de deep sleep é alcançada nessa composição, e o
acréscimo do contato não muda isso.

## 5. O que permanece do relatório anterior

Reconfirmo sem alteração, tendo reexaminado cada ponto:

- **a baseline comporta o acréscimo.** Pontos de inserção prontos em
  `smart_sys_app_deep_sleep.cpp`, agregado extensível em `SmartSysApp.h:83-89`,
  validação de composição por lista em `client_154/main/CMakeLists.txt:42-49`, e
  nenhuma dependência de build nova: `esp_hw_support` já está em `PRIV_REQUIRES`
  e `esp_driver_gpio` em `REQUIRES`;
- **DEEPSLEEP-DEC-013 confere.** Nada exige quarta operação compartilhada:
  `DigitalInputBehavior::quiesce()` não toca GPIO
  (`digital_input_behavior.cpp:386-401`), então no item 1 o pad ainda é entrada
  com o pull do board e `gpio_get_level()` basta;
- **o teste de fronteira não se aplica.** Sem capacidade arquitetural ausente,
  sem novo lifecycle ou ownership — a task privada já existe e já é dona
  exclusiva da sequência — e sem consumidor afetado fora do recorte;
- **precisão 5.1 — `activeHigh` de `ContactWakeupConfig` não tem efeito
  especificado.** O rearme usa "exclusivamente essa leitura elétrica" e DEC-014
  exclui o estado lógico; nenhuma regra consome o campo. Declare-o diagnóstico
  ou remova-o do contrato;
- **precisão 5.2 — contato em GPIO sem capability.** A seção 4.2A admite que os
  dois GPIOs não coincidam; nessas composições nada configura o pad,
  `ContactWakeupConfig` não carrega `pull` — ao contrário de
  `DryContactInputResource` (`boards/board_model.hpp:29-34`) — e o pino
  flutuaria durante o sleep, que é o modo de falha que AC-012 existe para
  excluir. Não ocorre na primeira composição;
- **itens de implementação a não esquecer:** `esp_sleep_enable_ext1_wakeup()`
  está depreciada na 6.0 em favor de `esp_sleep_enable_ext1_wakeup_io()`
  (`esp_sleep.h:315-317,390`), e o H2 declara
  `SOC_PM_SUPPORT_EXT1_WAKEUP_MODE_PER_PIN` (`soc_caps.h:514`), de modo que uma
  chamada por boot basta; `wakeupCauseName()` precisa do caso
  `ESP_SLEEP_WAKEUP_EXT1` (`sleep_modes.c:2304-2307`) para DEEPSLEEP-AC-009, hoje
  devolvendo `"other"` (`smart_sys_app_deep_sleep.cpp:69-80`); o diagnóstico de
  ausência de fonte passa a depender das duas
  (`smart_sys_app_deep_sleep.cpp:531-536`); e a costura de doubles pede o par de
  `prepareTimerWakeup` para o contato, costura interna prevista por AC-010.

Relações normativas reconfrontadas sem mudança: `ISSP-Reusable-Components.md`
v1.1, `ISSP-Architecture.md` v1.2, `ISSP-Commissioning.md` v1.0, ADR-0002 e
`Repository-Test-Execution-Policy.md` v0.4 permanecem preservadas; as emendas a
`Firmware-Variants-Menuconfig.md` e a `ISSP-Configurable-Bootstrap.md` v1.5 estão
declaradas e são pequenas. Mantenho a observação de estado, não bloqueante, de
que o bootstrap ainda não incorporou a emenda da v0.10 e conviria aplicar as
duas juntas.

## 6. Componentes impactados

| Fonte | Mudança | Natureza |
|---|---|---|
| `components/issp_app_154/include/SmartSysApp.h` | `ContactWakeupConfig`, campo ao final de `DeepSleepConfig`, costura em `SetupHooks` | aditiva, compatível |
| `components/issp_app_154/src/smart_sys_app_deep_sleep.cpp` | elegibilidade, leitura e armação de EXT1, causa de boot, diagnóstico de ausência de fonte | política, local |
| `components/issp_app_154/src/smart_sys_app_impl.hpp` | declarações correspondentes | privada |
| `client_154/main/CMakeLists.txt` | `dry_contact_wakeup` nas listas exigida e oferecida | composição |
| `client_154/main/firmwares/door_sensor_battery_h2.cpp` | `contactWakeup` a partir do board | produto |
| `client_154/main/boards/board_model.hpp` e `boards/door_sensor_battery_h2.cpp` | declaração de elegibilidade, se o Arquiteto adotar essa rota | board |
| `test_apps/smart_sys_app_test` | casos novos; os 15 existentes seguem válidos sem edição | evidência |

Nenhum componente ISSP compartilhado é tocado; coordenador, protocolo, ACK e
retry permanecem fora, como a seção 1 recorta.

## 7. Experimentos e verificações necessários

Herdados e ainda abertos: 10, 11, 19, 20 e 21, mais a medição dos três termos do
orçamento de DEEPSLEEP-AC-010. O item 16 foi resolvido na implementação da v0.10.

Acrescentados pela v0.11, sem mudança nesta revisão:

- **22 — DEEPSLEEP-AC-012, retenção do nível.** No par
  `door_sensor_battery_h2` / `Door Sensor Battery H2`: ausência de wakeup espúrio
  com o contato estável em cada nível, ao longo de um período completo de timer,
  e wakeup efetivo na transição nos dois sentidos;
- **23 — repique e acionamentos sucessivos.** Wakeups contados e corrente
  medida, como DEC-015 determina antes de qualquer política de taxa. Medir junto
  o ciclo gasto pelo caso autocorretivo da seção 2, que é a mesma despesa;
- **24 — report em todo boot.** Que o `reportOnStart` publique em boot por EXT1
  como publica em boot por timer, sem segundo caminho de publicação;
- **25 — liberação do hold.** A leitura de `cpu_start.c` já a resolve; um boot
  por EXT1 seguido de leitura correta do contato confirmaria de graça, junto de
  22.

Leitura de código não certifica nenhum deles.

## 8. Classificação e recomendação

**Não pronta — defeito da especificação** [`Not Ready — Specification Defect`].

A classificação é a mesma do relatório anterior, mas por **um** defeito, não
dois, e é essa a informação que a repetição produziu. O defeito é de uma
sentença da seção 4.2A: a exceção do GPIO 7 é contrariada pelo ESP-IDF 6.0.1 e é
incompatível com a derivação por capacidade do target exigida no bullet seguinte
da mesma seção. Ele pertence à própria funcionalidade e a seus donos naturais —
o texto da seção e, possivelmente, a declaração do board —, o que exclui
pré-requisito arquitetural pelo teste de fronteira.

A decisão a tomar é binária: ou a cláusula é engano e sai, ficando a derivação
sozinha; ou a exclusão é intencional, e então é do board, declarada pelo
mecanismo que a seção 5 já prevê. Recomendo a segunda formulação se houver
qualquer razão elétrica real, porque ela preserva as duas regras.

Não classifico como **evidência requerida**, ainda que DEEPSLEEP-AC-012 dependa
de hardware: seguindo o precedente da v0.10, critério de hardware em aberto não
impede que a baseline comporte a mudança, e a especificação já declara esse
critério insatisfazível neste recorte. O que impede começar é o contrato.

Resolvida essa sentença, e acompanhando-a as precisões das seções 2 e 5 — a
expressão "imediatamente antes de dormir", o `activeHigh` sem efeito e a borda do
contato sem capability —, não vejo obstáculo estrutural ao acréscimo.

Sobre independência, como as regras comuns exigem quando ela é material: esta
revisão **não é independente**. Foi produzida pela mesma capacidade que escreveu
o relatório anterior, na mesma sessão e com o mesmo contexto, e o achado
principal desta passagem é a correção de um erro meu. Se o Arquiteto quiser
confronto independente do bloqueador remanescente, ele deve vir de outro ator.

A recomendação informa o Arquiteto. Não certifica implementabilidade de forma
absoluta e não promove a especificação para `Pronta`.
