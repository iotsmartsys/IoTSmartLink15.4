# Análise de implementabilidade da v0.11 — wakeup por contato seco

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/Client-Deep-Sleep.md`

**Revisão confrontada:** v0.11, Draft de 11/08/2026

**Estado:** Concluído

**Capacidade:** Engenheiro Analista

**Data:** 12/08/2026

**Resultado:** a baseline comporta o acréscimo, que é pequeno e local, e a
premissa física de retenção do nível é **mais bem sustentada por leitura do que
a especificação supõe**; mas duas afirmações normativas impedem escrever o
código sem escolher por conta própria: a faixa elegível de GPIO para EXT1 está
factualmente errada quanto ao GPIO 7 e contradiz o próprio critério de
derivação, e o momento da leitura do contato é dado em dois lugares de forma
incompatível; nenhuma execução realizada

> Este relatório registra evidências e recomendações. Não altera a fonte
> normativa, não promove estado e não autoriza implementação ou testes.

## 1. Recorte e método

Confrontei o acréscimo da v0.11 — seções 2 (itens marcados como acréscimo), 3
(`ContactWakeupConfig`), 4.2A, 5, 6.1 e os critérios DEEPSLEEP-AC-011 e AC-012 —
contra a implementação vigente da v0.10, contra o board e o produto da primeira
composição e contra o ESP-IDF 6.0.1 instalado em `~/.espressif/v6.0.1/esp-idf`,
que é a autoridade material sobre elegibilidade de GPIO, semântica de EXT1 e
retenção de pad no ESP32-H2.

Diferentemente das análises da v0.4 à v0.10, esta pôde consultar o IDF: ele está
instalado neste ambiente. Isso converteu em evidência de leitura três pontos que
as versões anteriores tinham de deixar como incógnita de toolchain, e é o que
permite afirmar que a seção 4.2A erra em um ponto verificável.

Nenhum build, teste, flash ou execução em hardware foi realizado. A árvore
estava limpa em `spec/client-deep-sleep`.

## 2. O que a baseline já comporta sem esforço

O acréscimo é genuinamente pequeno, e a v0.10 deixou os pontos de inserção
prontos:

- **contrato de configuração.** `DeepSleepConfig` é agregado simples
  (`SmartSysApp.h:83-89`) e o produto o inicializa por designated initializers
  (`door_sensor_battery_h2.cpp:76-91`). Acrescentar `contactWakeup` ao final é
  compatível como a seção 3 afirma: composições que não o declarem continuam
  válidas e o campo é value-inicializado com `enabled=false`, portanto inerte;
- **política.** `smart_sys_app_deep_sleep.cpp` já concentra validação, limite do
  timer, LED, deadline, predicados e sequência terminal, e alcança o runtime só
  por `SetupHooks`. O contato entra como mais um passo do item 1 da ordem
  obrigatória (`smart_sys_app_deep_sleep.cpp:507-536`), mais um ramo de
  validação em `configureDeepSleep()` e mais um `case` no nome da causa de boot;
- **grafo de build.** `esp_sleep_is_valid_wakeup_gpio()` e
  `esp_sleep_enable_ext1_wakeup_io()` vêm de `esp_hw_support`, já em
  `PRIV_REQUIRES`, e `esp_driver_gpio` já está em `REQUIRES`
  (`components/issp_app_154/CMakeLists.txt`). Nenhuma dependência nova, e o TU
  continua target-agnóstico: em target sem RTCIO a função simplesmente devolve
  falso;
- **composição.** O CMake valida recursos por pertinência em lista
  (`client_154/main/CMakeLists.txt:42-49`). Acrescentar `dry_contact_wakeup` às
  listas exigida e oferecida é uma linha em cada ramo. O board já declara pino,
  polaridade e pull do contato (`boards/door_sensor_battery_h2.cpp:12-16`), no
  GPIO 14;
- **API reutilizável.** Confirmo DEEPSLEEP-DEC-013: nada no acréscimo exige
  quarta operação compartilhada. `DigitalInputBehavior::quiesce()` não toca GPIO
  (`digital_input_behavior.cpp:386-401`), então no item 1 da sequência o pad
  ainda está configurado como entrada com o pull do board e `gpio_get_level()`
  é suficiente. A fachada não precisa consultar a capability.

O teste de fronteira não se aplica: não há capacidade arquitetural ausente, não
há novo lifecycle ou ownership — a task privada de lifecycle já existe e já é
dona exclusiva da sequência — e nenhum consumidor fora do recorte é afetado.

## 3. Bloqueador 1 — a faixa elegível de GPIO está errada quanto ao GPIO 7

A seção 4.2A afirma: "No ESP32-H2 com ESP-IDF 6.0.1 a faixa é GPIO 7 a 14, e o
GPIO 7 é RTC GPIO mas **não** é conduzido para wakeup externo". A segunda metade
é falsa no IDF instalado, e a frase é internamente ambígua sobre qual é a faixa
que a fachada deve aceitar — 7 a 14 ou 8 a 14.

A cadeia de evidência é fechada:

- `esp_sleep_enable_ext1_wakeup_io()` valida cada pino por
  `esp_sleep_is_valid_wakeup_gpio()` e rejeita com `ESP_ERR_INVALID_ARG`
  (`components/esp_hw_support/sleep_modes.c:1935-1953`);
- no ESP32-H2, `SOC_RTCIO_PIN_COUNT` é 8, então essa função devolve
  `RTC_GPIO_IS_VALID_GPIO(gpio)` (`sleep_modes.c:1874-1883`;
  `components/soc/esp32h2/include/soc/soc_caps.h:251`);
- esse macro é `rtc_gpio_is_valid_gpio()`, que é verdadeiro exatamente quando
  `rtc_io_num_map[gpio] >= 0` (`components/esp_driver_gpio/src/rtc_io.c:32-39`);
- no ESP32-H2 esse mapa atribui canal a **GPIO 7 até GPIO 14 inclusive**, e −1 a
  todos os demais (`components/esp_hal_gpio/esp32h2/rtc_io_periph.c:9-38`);
- o próprio `soc_caps.h` documenta a intenção do silício: "GPIO7~14 on ESP32H2
  can support chip HP peripheral powerdown-ed sleep wakeup through EXT1 wake up"
  (`soc_caps.h:227`), e o comentário adjacente registra que GPIO7~14 permanecem
  em função LP alimentados por VDD3V3_LP (`soc_caps.h:220`).

Portanto o GPIO 7 **é** aceito pelo IDF 6.0.1 como fonte de wakeup externo, e a
faixa elegível derivada do target é 7 a 14, sem exceção.

O que torna isso bloqueante não é o erro factual isolado, e sim a contradição
que ele cria com o parágrafo seguinte da mesma seção: "a elegibilidade é
derivada das capacidades declaradas pelo target, não de uma lista de produto".
As duas instruções são incompatíveis e o implementador não tem como obedecer às
duas:

- se implementar a derivação exigida — `esp_sleep_is_valid_wakeup_gpio()` —,
  aceitará o GPIO 7, contrariando a prosa da mesma seção e o alcance de
  DEEPSLEEP-AC-011, que manda rejeitar "GPIO fora da faixa elegível do target";
- se excluir o GPIO 7 à mão, terá escrito exatamente a lista de produto que a
  especificação proíbe, e o terá feito sobre uma premissa que o IDF contradiz.

Escolher entre as duas é decisão normativa, não detalhe de implementação, e o
perfil determina devolvê-la em vez de resolvê-la por conveniência. Registro que
o impacto operacional na primeira composição é nulo — ela usa o GPIO 14 — mas o
critério de aceite fala da faixa, não do produto, e o texto do contrato é o que
governa a validação.

Se houver uma restrição elétrica real do GPIO 7 na placa ou no encapsulamento
que motivou a frase, ela é do domínio do board, não do target, e caberia ao
board declarar sua entrada como inelegível — mecanismo que a seção 5 já prevê
("declara se sua entrada de contato é elegível para wakeup externo"). Essa
leitura preservaria a derivação por capacidade do target e a proibição de lista
de produto ao mesmo tempo. Não a adoto: é recomendação ao Arquiteto.

## 4. Bloqueador 2 — o momento da leitura do contato é dado duas vezes, de formas incompatíveis

A seção 4.2A determina: "**Imediatamente antes de dormir**, e depois de preparado
o timer, a fachada lê o nível elétrico do GPIO do contato e arma o wakeup externo
para o nível oposto". DEEPSLEEP-DEC-014 repete: "o nível oposto de EXT1 é
calculado a partir do nível elétrico lido imediatamente antes do sleep".

A ordem obrigatória da seção 6.1 põe a mesma operação no item 1, junto do timer:

```text
→ preparar as fontes de wakeup solicitadas, quando houver
  contato: ler o nível atual e armar EXT1 para o nível oposto, quando habilitado
→ encerrar ResetButtonMonitor  → beginQuiescence  → quiesce
→ aguardar pendingReportCount() == 0 até o deadline
→ stop no report executor (até 600 ms)  → end no transporte
→ apagar LED e iniciar deep sleep
```

Entre o item 1 e o sleep há, no caminho antecipado, a espera por entrega
limitada apenas pelo deadline restante — dezenas de segundos são possíveis com
`maxAwakeTimeMs = 30000` — e, em qualquer caminho, até 600 ms de `stop()`. Isso
não é "imediatamente antes de dormir" em nenhum sentido útil.

As duas colocações não são intercambiáveis, e cada uma serve a um requisito real
que a outra sacrifica:

- **no item 1** vale a regra de que "falha ao preparar qualquer fonte solicitada
  aborta a sequência antes de qualquer operação terminal, libera a arbitragem e
  preserva o runtime acessível". Armar depois da quiescência tornaria essa
  garantia impossível: o executor já estaria parado e o transporte encerrado
  quando a falha aparecesse;
- **imediatamente antes do sleep** vale a intenção de DEEPSLEEP-DEC-011, que o
  nível armado corresponda ao estado do contato no instante em que o dispositivo
  efetivamente dorme.

O implementador não pode satisfazer as duas sem uma terceira operação — armar no
item 1 e **rearmar** depois de `stop()`, tratando só a segunda falha como não
abortável — e essa operação não está autorizada em lugar nenhum da
especificação.

Registro a análise da consequência, porque ela deve informar a decisão e reduz a
urgência sem eliminar a necessidade de escolher. Armar cedo é **autocorretivo**:
se o contato transitar entre o item 1 e o sleep, o pino já estará no nível
armado quando o EXT1 entrar em vigor, o wakeup dispara de imediato, o dispositivo
acorda, reporta o estado novo e rearma para o oposto. Não há perda permanente de
transição; há gasto de um ciclo acordado completo — o que interage com
DEEPSLEEP-DEC-015, que decidiu não impor rate limit nesta versão. Armar cedo é,
portanto, defensável, e é a leitura que preserva a garantia de abortar antes de
operação terminal. Mas quem decide qual das duas frases prevalece é o Arquiteto.

## 5. Precisões não bloqueantes

### 5.1 `activeHigh` de `ContactWakeupConfig` não tem efeito especificado

`ContactWakeupConfig` expõe `activeHigh` (seção 3), e a seção 4.2A o descreve
como "a polaridade elétrica do contato fechado, com o mesmo significado que a
capability de porta já usa". Nenhuma regra o consome: o rearme usa
"exclusivamente essa leitura elétrica", e DEEPSLEEP-DEC-014 exclui o estado
lógico do cálculo.

O campo é público e, como está, o implementador o copiaria sem nunca lê-lo. Isso
é observação de contrato, não de estilo: um campo público sem comportamento
especificado convida a usos divergentes depois. Se a intenção é diagnóstico —
registrar no log se o contato dormiu fechado ou aberto —, basta dizê-lo; se não
houver intenção, ele pode sair do contrato sem perda. Não é bloqueante porque
nenhuma decisão de código depende dele.

### 5.2 Contato sem capability que configure o pad

A seção 4.2A diz que a coincidência entre o GPIO do contato e o da capability de
entrada "é o caso normal", o que admite composições em que eles não coincidam.
Nessas, nada configura o pad: `ContactWakeupConfig` não carrega `pull` — ao
contrário de `DryContactInputResource`, que carrega
(`boards/board_model.hpp:29-34`) — e a fachada não configura o GPIO do contato
em lugar nenhum. O resultado seria leitura de nível indefinida no item 1 e pino
flutuante durante o sleep, isto é, exatamente o modo de falha que
DEEPSLEEP-AC-012 existe para excluir.

Na primeira composição eles coincidem e o problema não aparece: o
`DigitalInputBehavior` configura entrada com pull-up
(`digital_input_behavior.cpp:64-74`) a partir de `InputPull::PullUp` declarado
pelo board. Não é bloqueante hoje, mas é uma borda que o contrato permite e não
cobre.

### 5.3 Itens de implementação que a v0.11 torna necessários e não menciona

Não são decisões normativas; registro-os para que não sejam esquecidos:

- **`esp_sleep_enable_ext1_wakeup()` está depreciada na 6.0.** O header manda
  usar `esp_sleep_enable_ext1_wakeup_io()`
  (`components/esp_hw_support/include/esp_sleep.h:315-317,390`). O ESP32-H2
  declara `SOC_PM_SUPPORT_EXT1_WAKEUP_MODE_PER_PIN` (`soc_caps.h:514`), então
  `ESP_EXT1_WAKEUP_ANY_LOW` e `ESP_EXT1_WAKEUP_ANY_HIGH` se aplicam por pino e
  uma única chamada por boot basta — o registro de configuração nasce limpo a
  cada boot, porque o deep sleep reinicia o firmware;
- **a causa do boot precisa distinguir EXT1.** `wakeupCauseName()` hoje devolve
  `"other"` para tudo que não seja timer ou cold boot
  (`smart_sys_app_deep_sleep.cpp:69-80`), e DEEPSLEEP-AC-009 passa a exigir que
  o wakeup pelo contato seja distinguível. O IDF devolve `ESP_SLEEP_WAKEUP_EXT1`
  nesse caso (`sleep_modes.c:2304-2307`);
- **o diagnóstico de ausência de fonte precisa considerar as duas.** O ramo
  atual emite `no_wakeup_source` no `else` do timer
  (`smart_sys_app_deep_sleep.cpp:531-536`); com duas fontes, a condição passa a
  ser "nenhuma das duas habilitada";
- **a costura de doubles.** `SetupHooks` já tem `prepareTimerWakeup`; o contato
  pede o par correspondente para que DEEPSLEEP-AC-010 continue verificável sem
  armar EXT1 de verdade. É costura interna, prevista por AC-010, e não amplia
  contrato normativo.

## 6. DEEPSLEEP-AC-012 — a premissa física está mais bem sustentada do que a especificação supõe

Esta é a parte da análise que mais muda o quadro, e muda a favor.

A seção 4.2A registra que o encadeamento do pull por HOLD "é plausível para o
ESP32-H2 e para a configuração vigente do projeto, **mas não é certificado por
leitura**", e condiciona o mecanismo a "o alvo não possuir domínio `RTC_PERIPH`
desligável e a opção correspondente estar habilitada". A leitura do IDF 6.0.1
mostra que o mecanismo é mais simples e mais forte do que isso, e que não
depende de opção de Kconfig alguma:

- o ESP32-H2 não define `SOC_RTCIO_INPUT_OUTPUT_SUPPORTED` — o C6 define, o H2
  não. Logo `ext1_wakeup_prepare()` executa o ramo `#else`, que roteia o pad para
  função **digital**, habilita a entrada e chama **incondicionalmente**
  `rtcio_hal_hold_enable(rtc_pin)`, com o comentário "hold rtc_pin to use it
  during sleep state" (`sleep_modes.c:2036-2072`). Não há teste de
  `SOC_PM_SUPPORT_RTC_PERIPH_PD` nesse ramo, nem de
  `CONFIG_ESP_SLEEP_GPIO_ENABLE_INTERNAL_RESISTORS`: essa opção pertence ao
  caminho alternativo `esp_sleep_gpio_wakeup_prepare_on_hp_periph_powerdown()`
  (`sleep_modes.c:2127-2136`), que é para alvos sem RTCIO e não é o do H2;
- como o pad é mantido em função digital e apenas congelado, **o pull que vale
  durante o sleep é exatamente o que a capability configurou acordada** — o
  pull-up interno que `DigitalInputBehavior::begin()` programa a partir do
  `InputPull::PullUp` do board. Não há reconfiguração de pull no caminho EXT1 do
  H2;
- `esp_sleep_isolate_digital_gpio()`, que isolaria pinos no sleep, é compilada
  somente quando `!SOC_GPIO_SUPPORT_HOLD_SINGLE_IO_IN_DSLP`
  (`sleep_gpio.c:228`), e o H2 declara esse caps como 1 (`soc_caps.h:241`). Não
  há, portanto, isolamento concorrente do pino do contato;
- **e o hold é desfeito sozinho no boot seguinte.** `cpu_start.c:845-849` chama
  `esp_deep_sleep_wakeup_io_reset()` quando a razão de reset é
  `RESET_REASON_CORE_DEEP_SLEEP`, e essa função limpa os pinos EXT1 e libera o
  hold de cada um (`sleep_gpio.c:231-245`), antes de `app_main()`. Isso responde
  por leitura a uma pergunta que a especificação nem chegou a formular e que
  seria um defeito clássico: o pad **não** permanece congelado depois do wakeup,
  então `gpio_config()` do `DigitalInputBehavior` volta a valer normalmente em
  todo boot, e o report de `reportOnStart` não lê um nível preso.

O que **continua** exigindo hardware, e por isso mantém DEEPSLEEP-AC-012 intacto
como critério: se o pull-up interno congelado — dezenas de kΩ — sustenta o nível
contra a fuga do pad e a impedância real do contato e da fiação durante minutos
de sleep, e se não há wakeup espúrio por ruído no par produto/board da seção 5.
Isso é eletricidade, não código, e nenhuma leitura o decide.

Minha recomendação, se o Arquiteto quiser precisar a seção 4.2A na mesma
transação em que resolver os dois bloqueadores: substituir a condicional sobre
`RTC_PERIPH` e "a opção correspondente" pela cadeia acima, que é verificável, e
manter AC-012 exatamente como está. A especificação acerta ao exigir hardware; o
que ela descreve como incerto é hoje incerto por menos motivos.

## 7. Relações normativas reconfrontadas

- **`ISSP-Reusable-Components.md` v1.1 — preservada.** Confirmado em 2: o
  acréscimo não amplia API reutilizável;
- **`ISSP-Architecture.md` v1.2 e `ISSP-Commissioning.md` v1.0 — preservadas.**
  O contato não move política para componentes ISSP, não toca canais nem
  persistência;
- **ADR-0002 — preservada.** Board fornece GPIO, polaridade e elegibilidade;
  produto decide usar; Kconfig só seleciona composição. O GPIO 14 do contato
  chega à fachada pelo produto, vindo do board, como a seção 5 exige;
- **`Repository-Test-Execution-Policy.md` v0.4 — preservada.** Nada aqui
  autoriza build ou hardware, e DEEPSLEEP-AC-012 permanece insatisfazível dentro
  deste recorte, por decisão da própria especificação;
- **`Firmware-Variants-Menuconfig.md` — emenda declarada e pequena.** O
  mecanismo de recursos suporta `dry_contact_wakeup` sem mudança estrutural;
- **`ISSP-Configurable-Bootstrap.md` v1.5 — emenda declarada.** Observação de
  estado, não bloqueio: esse documento ainda não incorporou a emenda da v0.10
  (não menciona `configureDeepSleep()`), e a v0.11 declara emendá-lo "novamente".
  A incorporação é do Arquiteto e não afeta implementabilidade; registro para
  que as duas emendas sejam aplicadas juntas quando ele decidir.

## 8. Componentes impactados

| Fonte | Mudança | Natureza |
|---|---|---|
| `components/issp_app_154/include/SmartSysApp.h` | `ContactWakeupConfig` e campo ao final de `DeepSleepConfig`; par de costura em `SetupHooks` | aditiva, compatível |
| `components/issp_app_154/src/smart_sys_app_deep_sleep.cpp` | validação de elegibilidade, leitura e armação de EXT1, causa de boot, diagnóstico de ausência de fonte | política, local |
| `components/issp_app_154/src/smart_sys_app_impl.hpp` | declarações correspondentes | privada |
| `client_154/main/CMakeLists.txt` | `dry_contact_wakeup` nas listas exigida e oferecida | composição |
| `client_154/main/firmwares/door_sensor_battery_h2.cpp` | `contactWakeup` a partir do board; timer já em 15 minutos, como DEEPSLEEP-DEC-016 fixa | produto |
| `client_154/main/boards/board_model.hpp` e `boards/door_sensor_battery_h2.cpp` | declaração de elegibilidade da entrada, se o Arquiteto adotar a rota da seção 3 | board |
| `components/issp_app_154/test_apps/...` | casos da costura, escritos e não executados | evidência |

Nenhum componente ISSP compartilhado é tocado. O coordenador, o protocolo, o ACK
e o retry permanecem intactos, como a seção 1 recorta.

## 9. Restrições confirmadas

- ESP32-H2 é o único target físico do `client_154`; QEMU não é admitido;
- nenhuma execução é autorizada por esta especificação nem por esta atuação;
- a elegibilidade de GPIO para EXT1 no H2 é 7 a 14, derivada de
  `rtc_io_num_map`, e não é configurável;
- o EXT1 do H2 congela o pad por HOLD ao dormir e o libera no boot seguinte, sem
  depender de opção de Kconfig;
- o pull que vale durante o sleep é o configurado no domínio comum enquanto
  acordado, o que amarra o contato à capability que o configura;
- `maxAwakeTimeMs = 30000` e hold de factory reset de 10 s permanecem os valores
  vigentes do produto, fora do acréscimo da v0.11 e não ratificados por ele.

## 10. Experimentos e verificações necessários

O conjunto herdado permanece: 10, 11, 19, 20 e 21 das análises anteriores, mais
a medição dos três termos do orçamento exigida por DEEPSLEEP-AC-010. O item 16
(limite do timer) está resolvido desde a implementação da v0.10.

A v0.11 acrescenta:

- **22 — DEEPSLEEP-AC-012, retenção do nível.** No par
  `door_sensor_battery_h2` / `Door Sensor Battery H2`, com o contato estável em
  cada um dos dois níveis: ausência de wakeup espúrio ao longo de um período de
  timer completo; e wakeup efetivo na transição, nos dois sentidos. Leitura e
  build não satisfazem;
- **23 — repique e acionamentos sucessivos.** Wakeups contados e corrente
  medida sob repique, como DEEPSLEEP-DEC-015 determina antes de qualquer
  política de taxa. Convém medir junto o custo do ciclo acordado disparado pelo
  caso autocorretivo descrito em 4, que é a mesma despesa;
- **24 — report em todo boot.** Que o `reportOnStart` publique o estado em boot
  por EXT1 como publica em boot por timer, sem segundo caminho de publicação,
  confirmando a premissa da seção 4.2A;
- **25 — liberação do hold, se o Arquiteto quiser confirmação empírica.** A
  leitura de `cpu_start.c` a resolve; um boot por EXT1 seguido de leitura correta
  do contato a confirmaria de graça, junto de 22.

## 11. Classificação e recomendação

**Não pronta — defeito da especificação** [`Not Ready — Specification Defect`].

Os dois bloqueadores são da própria funcionalidade e de seus donos naturais — o
texto da seção 4.2A e a ordem da seção 6.1 —, não de capacidade arquitetural
ausente, o que exclui pré-requisito arquitetural pelo teste de fronteira. Ambos
são correções curtas:

1. **faixa elegível.** Decidir se o GPIO 7 é elegível. A derivação exigida pela
   própria seção o aceita, e o IDF 6.0.1 confirma; se houver restrição real, ela
   pertence ao board, que já tem o mecanismo para declará-la;
2. **momento da leitura.** Decidir se o rearme ocorre no item 1, como a seção
   6.1 ordena e como a garantia de abortar antes de operação terminal exige, ou
   imediatamente antes do sleep, como a seção 4.2A e DEEPSLEEP-DEC-014 dizem —
   e, nesse caso, dizer o que acontece com uma falha de armação depois da
   quiescência.

Não classifico como **evidência requerida** apesar de DEEPSLEEP-AC-012 depender
de hardware: seguindo o precedente da v0.10, critério de hardware em aberto não
impede que a baseline comporte a mudança, e a especificação corretamente já o
declara insatisfazível neste recorte. O que impede começar é o contrato, não a
evidência. Se os dois pontos forem resolvidos, não vejo obstáculo estrutural ao
acréscimo, e as precisões da seção 5 podem acompanhar a mesma revisão sem
mudar a classificação.

Encaminho também, como ganho colateral desta análise, a precisão possível da
seção 4.2A descrita em 6: o mecanismo de retenção do nível é hoje verificável
por leitura em quase toda a sua cadeia, e o único elo genuinamente físico é o
elétrico. Isso não altera DEEPSLEEP-AC-012, que continua correto como está.

A recomendação informa o Arquiteto. Não certifica implementabilidade de forma
absoluta e não promove a especificação para `Pronta`.
