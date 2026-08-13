# Análise de implementabilidade da v0.11 — preparo do pad definido

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/Client-Deep-Sleep.md`

**Revisão confrontada:** v0.11, Draft de 12/08/2026, conforme o commit `5b2cc09`

**Estado:** Concluído

**Capacidade:** Engenheiro Analista

**Data:** 12/08/2026

**Relação com relatórios anteriores:** terceira análise da v0.11. Confirma e
encerra `2026-08-12-v11-implementability-analysis-r3.md`, cujo defeito único o
Arquiteto incorporou em `5b2cc09`

**Resultado:** o defeito remanescente foi fechado pela saída que o relatório
anterior recomendava, e a cláusula nova é implementável com dados que a fachada
já copia, sem ampliar API reutilizável e sem tocar componente ISSP
compartilhado. Não encontro bloqueador, decisão normativa ausente nem
contradição interna. Recomenda-se **prontidão**, com uma precisão declarada não
bloqueante e os experimentos de hardware já normativos; nenhuma execução
realizada

> Este relatório registra evidências e recomendações. Não altera a fonte
> normativa, não promove estado e não autoriza implementação ou testes.

## 1. Recorte e método

Confrontei a v0.11 como está em `5b2cc09`, que acrescenta o preparo do pad pela
fachada, contra a implementação vigente da v0.10, o board e o produto da
primeira composição, e o ESP-IDF 6.0.1 instalado.

Verifiquei primeiro se o `HEAD` havia avançado desde a análise anterior, e não
apenas se a árvore estava limpa — a inversão dessa checagem foi o erro de
processo registrado na seção 1 do relatório r3. Havia avançado: `5b2cc09`,
"docs: define preparo do GPIO no wakeup v0.11", posterior ao commit `7d4990c` da
r3. É contra esse texto que esta análise foi feita.

Nenhum build, teste, flash ou execução em hardware. Árvore limpa em
`spec/client-deep-sleep`.

## 2. O defeito da r3 está fechado, e pela saída certa

A r3 identificou que a v0.11 anterior enunciava como fato que "essa capability
configura direção e pull do pad", enquanto a validação garantia apenas que ela
estivesse **registrada** — e que existem caminhos de boot em que ela é registrada
e nunca iniciada, nos quais EXT1 seria armado sobre um pad em default de reset.

O Arquiteto adotou a primeira das três saídas oferecidas, e o texto novo é
preciso onde precisava ser:

- a fonte do dado mudou de "essa capability configura" para "a **configuração
  registrada** dessa capability fornece direção e pull para o preparo do pad"
  (4.2A). A afirmação passou a ser sobre um dado copiado, que existe sempre, e
  não sobre um efeito colateral de `begin()`, que pode não ocorrer;
- o rearme alternado passou a incluir a reaplicação, com a condição explícita:
  "o preparo é idempotente e ocorre mesmo se `StartDevice` não tiver sido
  alcançado ou se o behavior já tiver iniciado" (4.2A);
- a ordem obrigatória da seção 6.1 incorporou o passo no mesmo item 1, e a regra
  de falha foi estendida — "falha ao configurar direção ou pull ou ao armar EXT1
  é falha de preparo da fonte. A regra vale inclusive quando `StartDevice` não
  foi alcançado; nesse caminho, a fachada não depende de
  `DigitalInputBehavior::begin()` para deixar o pad em estado definido";
- DEEPSLEEP-AC-011 deixou de falar em "todo boot operacional" e passou a "todo
  boot que alcance a sequência terminal", com "ou o fato de `StartDevice` ter
  sido alcançado" entre as circunstâncias indiferentes;
- DEEPSLEEP-AC-012 passou a exigir a evidência "tanto no caminho normal quanto em
  boot que alcance a sequência terminal sem executar `StartDevice`, incluindo
  `NotReady` em `InitializeNetwork`", o que absorve como normativo o experimento
  26 que a r3 propunha;
- DEEPSLEEP-DEC-017 consolidou a decisão, e o parágrafo de retenção trocou "o
  pull estabelecido pela capability" por "o pull reaplicado pela fachada a partir
  da configuração da capability", ficando consistente com o mecanismo.

O conjunto é coerente: não sobrou nenhuma sentença que ainda atribua a
configuração do pad ao `begin()` do behavior.

## 3. A cláusula nova é implementável com o que a fachada já possui

Este é o ponto que a análise precisa sustentar, e ele se sustenta.

**O dado existe e é copiado na configuração.** `app::DoorSensorConfig` carrega
`pin` e `pull` (`SmartSysApp.h:26-40`), e a fachada guarda o array
`doorSensorConfigs_` preenchido em `addDoorSensorCapability()`
(`smart_sys_app_impl.hpp:158`; `smart_sys_app.cpp:212-245`). A cópia acontece em
`AppState::Configuring` e independe de qualquer estágio de `setup()`, logo está
disponível no item 1 da sequência terminal em **todos** os caminhos que a r3
enumerou — `NotReady` de `InitializeNetwork` (`smart_sys_app.cpp:358-372`), falha
dura de rede, falha de `RegisterCapabilities` (`smart_sys_app.cpp:378-386`) e
falha de `StartDevice` (`smart_sys_app.cpp:388-394`).

**A reaplicação é a mesma operação que o behavior faz, com os mesmos valores.**
`DigitalInputBehavior::configureGpio()` monta um `gpio_config_t` com
`GPIO_MODE_INPUT`, pull-up ou pull-down conforme `config_.pull` e
`GPIO_INTR_DISABLE` (`digital_input_behavior.cpp:62-75`). A fachada precisa
apenas do mesmo mapeamento sobre `app::DigitalInputPull`, e
`smart_sys_app_deep_sleep.cpp` já inclui `driver/gpio.h` (linha 13) e já usa
`gpio_config()` para o LED (linhas 245-257). Não há dependência nova, não há
consulta ao behavior e não há ampliação de API reutilizável — o que confirma
DEEPSLEEP-DEC-013 e DEC-017 nesse ponto.

**A idempotência afirmada pelo texto é real.** `gpio_config()` escreve
registradores de modo, pull e interrupção; reescrevê-los com os mesmos valores
não produz efeito observável. O caso "o behavior já tiver iniciado" é, portanto,
seguro mesmo com o timer de amostragem ainda rodando: a reaplicação ocorre no
item 1, antes de `quiesce()` (item 4), e não altera nível, direção nem
interrupção sob o callback que apenas lê o pino
(`digital_input_behavior.cpp:228`).

**O contrato de falha fecha.** A extensão "erro em qualquer passo bloqueia a
sequência antes de qualquer operação terminal" reusa a regra que a seção 6.1 já
aplicava às fontes, e o código já tem a forma dela: aborto com
`releaseDeepSleepTransition()` antes de qualquer passo terminal
(`smart_sys_app_deep_sleep.cpp:507-529`). O passo novo entra no mesmo bloco.

**E a ordem interna do item 1 é a certa.** Reaplicar antes de ler é o que torna a
leitura significativa; ler antes de armar é o que define o nível oposto; e tudo
isso antes do encerramento do monitor mantém a garantia de que uma falha preserva
o runtime acessível.

## 4. Precisão declarada não bloqueante: duas capabilities no mesmo GPIO

A cláusula nova fala em "a capability correspondente", no singular. A composição
pode registrar mais de uma capability de contato seco no mesmo pino: a validação
de `addDoorSensorCapability()` rejeita par endpoint/evento duplicado
(`smart_sys_app.cpp:231-235`) e colisão com `wake_led`
(`smart_sys_app.cpp:236-240`), mas **não** rejeita pino repetido. Duas
capabilities no mesmo GPIO com `pull` divergente tornam indeterminado qual
configuração a fachada reaplica.

Declaro esta condição **não bloqueante**, por três razões:

- a ambiguidade não é criada pela v0.11: ela já existe na camada de behaviors,
  onde as duas instâncias chamariam `configureGpio()` sobre o mesmo pino e a
  ordem de registro decidiria o resultado. A v0.11 apenas a herda;
- ela não é alcançável na composição da seção 5, que registra uma única
  capability de contato (`firmwares/door_sensor_battery_h2.cpp:56-67`);
- e uma composição que declare pulls divergentes para o mesmo pino físico é
  defeito de desenvolvimento da composição constante, categoria que a seção 6 já
  trata como tal.

Se o Arquiteto quiser fechá-la, a correção é de uma sentença e cabe em qualquer
das duas formas: dizer "a primeira capability correspondente", que é o que uma
varredura naturalmente faz; ou rejeitar em `ValidateConfiguration` a
correspondência múltipla com `pull` divergente, junto da verificação de
correspondência que a mesma seção já manda fazer ali. Não recomendo condicionar
a prontidão a isso.

## 5. Observações que não pedem mudança

- **a aptidão do pino ao wakeup é declarada por nome no CMake, não verificada
  contra a pinagem.** O recurso `dry_contact_wakeup` é validado por pertinência
  em lista (`client_154/main/CMakeLists.txt:42-49`), de modo que um board que o
  declare mas conecte o contato a um pino fora de 7–14 só falharia em runtime,
  em `configureDeepSleep()`, deixando um dispositivo a bateria permanentemente
  acordado. É o mesmo padrão já aceito para `wake_led` e cai na categoria de
  defeito de desenvolvimento da composição que a seção 6 descreve. Registro por
  ser consequência de bateria, não por pedir mudança;
- **bloquear o sleep diante de falha do GPIO tem custo de bateria.** A regra
  nova é coerente com a regra geral das fontes e com DEC-012, mas convém ter
  presente que uma falha persistente de `gpio_config()` no pino do contato
  impediria o dispositivo de dormir em todo boot. Na prática `gpio_config()` só
  falha por argumento inválido, e o pino já passou por
  `esp_sleep_is_valid_wakeup_gpio()` e por `GPIO_IS_VALID_GPIO()` no registro da
  capability, de modo que o caminho é improvável;
- **o resultado da rejeição por ausência de capability chega por `SetupResult`,
  não por `lastConfigurationResult()`**, porque é cruzamento feito em
  `ValidateConfiguration` e não em uma chamada de configuração. É consistente
  com AC-011, que pede rejeição "em `ValidateConfiguration`", e não exige valor
  novo de `AppResult`: `InvalidArgument` serve.

## 6. Pontos reconfirmados

Reconfirmo, tendo reexaminado cada um contra o texto vigente:

- **elegibilidade.** `esp_sleep_is_valid_wakeup_gpio()` é o mesmo predicado que
  `esp_sleep_enable_ext1_wakeup_io()` usa internamente (`sleep_modes.c:1946`), é
  puro — indexação em `rtc_io_num_map`, `esp_driver_gpio/src/rtc_io.c:32-39` — e
  portanto chamável em `AppState::Configuring`, antes de qualquer inicialização,
  que é onde AC-011 o exige. No ESP32-H2 resolve para GPIO 7 a 14
  (`esp_hal_gpio/esp32h2/rtc_io_periph.c:9-38`);
- **cadeia de retenção.** O ramo do H2 em `ext1_wakeup_prepare()` rotea o pad
  para função digital, habilita a entrada e chama `rtcio_hal_hold_enable()`
  incondicionalmente (`sleep_modes.c:2064-2071`), que é um bit em
  `LP_AON.gpio_hold0` (`esp_hal_gpio/esp32h2/include/hal/rtc_io_ll.h:83-86`); o
  hold é liberado no boot seguinte por `esp_deep_sleep_wakeup_io_reset()`,
  chamado quando o reset é `RESET_REASON_CORE_DEEP_SLEEP`
  (`cpu_start.c:845-849`; `sleep_gpio.c:231-245`). Com a reaplicação da seção 3,
  o que o HOLD congela passa a ser um pad definido em **todos** os caminhos
  terminais, que era exatamente o buraco apontado pela r3;
- **compatibilidade.** `contactWakeup` entra ao final de `DeepSleepConfig`
  (`SmartSysApp.h:83-89`); os 15 casos existentes constroem a configuração por
  fábrica única com designated initializers
  (`test_apps/smart_sys_app_test/main/test_smart_sys_app.cpp:160-172`) e seguem
  válidos sem edição, com o contato inerte;
- **composição.** `dry_contact_wakeup` entra como um item em cada lista do CMake;
  o board já declara pino, polaridade e pull do contato no GPIO 14
  (`boards/door_sensor_battery_h2.cpp:12-16`);
- **relações normativas.** `ISSP-Reusable-Components.md` v1.1,
  `ISSP-Architecture.md` v1.2, `ISSP-Commissioning.md` v1.0, ADR-0002 e
  `Repository-Test-Execution-Policy.md` v0.4 permanecem preservadas; as emendas a
  `Firmware-Variants-Menuconfig.md` e a `ISSP-Configurable-Bootstrap.md` v1.5
  estão declaradas e são pequenas. Mantenho a observação, não bloqueante, de que
  o bootstrap ainda não incorporou a emenda da v0.10 e convém aplicar as duas
  juntas;
- **teste de fronteira.** Não se aplica: sem capacidade arquitetural ausente, sem
  novo lifecycle ou ownership — a task privada já existe e já é dona exclusiva da
  sequência —, sem arbitragem nova entre subsistemas e sem consumidor afetado
  fora do recorte. A reaplicação do pad é operação local da fachada sobre dado
  que ela já copia.

## 7. Componentes impactados

| Fonte | Mudança | Natureza |
|---|---|---|
| `components/issp_app_154/include/SmartSysApp.h` | `ContactWakeupConfig` (`enabled`, `pin`), campo ao final de `DeepSleepConfig`, costura em `SetupHooks` | aditiva, compatível |
| `components/issp_app_154/src/smart_sys_app.cpp` | cruzamento contato/capability em `ValidateConfiguration` | validação, sem tocar hardware |
| `components/issp_app_154/src/smart_sys_app_deep_sleep.cpp` | elegibilidade, reaplicação de modo e pull, leitura, armação de EXT1, causa `EXT1`, condição de `no_wakeup_source` | política, local |
| `components/issp_app_154/src/smart_sys_app_impl.hpp` | declarações correspondentes | privada |
| `client_154/main/CMakeLists.txt` | `dry_contact_wakeup` nas listas exigida e oferecida | composição |
| `client_154/main/firmwares/door_sensor_battery_h2.cpp` | `contactWakeup` a partir do board | produto |
| `client_154/main/boards/*` | declaração do recurso apto a wakeup | board |
| `test_apps/smart_sys_app_test` | casos novos, incluindo o caminho sem `StartDevice`; os 15 existentes seguem válidos sem edição | evidência |

Nenhum componente ISSP compartilhado é tocado; coordenador, protocolo, ACK e
retry permanecem fora do recorte.

## 8. Experimentos e verificações necessários

Herdados e abertos: 10, 11, 19, 20 e 21, mais a medição dos três termos do
orçamento de DEEPSLEEP-AC-010. O item 16 foi resolvido na implementação da v0.10.

Da v0.11:

- **22 — DEEPSLEEP-AC-012, retenção do nível**, no par produto/board da seção 5:
  ausência de wakeup espúrio com o contato estável em cada nível ao longo de um
  período completo de timer, e wakeup efetivo na transição nos dois sentidos;
- **23 — repique e acionamentos sucessivos**, com wakeups contados e corrente
  medida, incluindo o ciclo gasto pelo wakeup imediato descrito em 4.2A;
- **24 — report em todo boot**, que o `reportOnStart` publique em boot por EXT1
  como publica em boot por timer;
- **26 — caminho sem `StartDevice`**, agora normativo dentro de AC-012: provocar
  `NotReady` em `InitializeNetwork` com o contato habilitado e demonstrar que o
  pad reaplicado sustenta o nível durante o sleep, sem wakeup espúrio. É a
  evidência que fecha o defeito da r3 no plano físico, já que no plano do
  contrato ele está fechado.

Leitura de código e build não satisfazem nenhum deles, e esta especificação
corretamente não autoriza executá-los.

## 9. Classificação e recomendação

**Pronta** [`Ready`].

A baseline comporta o acréscimo, e o acréscimo está inteiramente definido. As
três rodadas de análise convergiram: a elegibilidade passou a usar o predicado do
próprio ESP-IDF; o momento da armação ficou unívoco nos quatro lugares em que
aparece; o campo público sem efeito saiu do contrato; a cadeia de retenção
descreve o mecanismo real do ESP32-H2 e mantém corretamente a fronteira física em
DEEPSLEEP-AC-012; e o último buraco — o pad indefinido em boot sem `StartDevice`
— foi fechado com a reaplicação idempotente a partir de um dado que a fachada já
copia, sem ampliar API reutilizável e sem mover responsabilidade entre camadas.

Não encontro bloqueador estrutural, decisão normativa ausente nem contradição
interna. As pendências restantes não são do contrato: são os experimentos de
hardware, que a própria especificação declara obrigatórios e não autoriza, e a
precisão da seção 4, que declaro **não bloqueante** e cuja correção cabe em uma
sentença se o Arquiteto a quiser.

Registro, por serem úteis a quem implementar e não por exigirem mudança
normativa: reusar em `smart_sys_app_deep_sleep.cpp` o mesmo mapeamento de
`pull` que `DigitalInputBehavior::configureGpio()` usa, para que reaplicação e
configuração original não possam divergir; localizar a capability
correspondente por varredura de `doorSensorConfigs_`, que é onde o dado vive em
todos os caminhos; e prever o caso do caminho forçado sem `StartDevice` entre os
casos automatizados da costura de AC-010, porque é o caminho que nenhuma
evidência anterior exercitava.

Sobre independência, como as regras comuns exigem quando ela é material: esta
análise **não é independente**. É a terceira rodada produzida pela mesma
capacidade, na mesma sessão, sobre revisões sucessivas que responderam aos meus
próprios achados, e uma delas conteve um erro de processo meu, registrado na r3.
Recomendar prontidão depois de três rodadas com esse histórico é precisamente o
caso em que o Arquiteto pode querer confronto por outro ator antes de promover.

A recomendação informa o Arquiteto. Não certifica implementabilidade de forma
absoluta e não promove a especificação para `Pronta`.
