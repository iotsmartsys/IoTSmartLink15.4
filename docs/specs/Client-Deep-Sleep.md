# Deep sleep configurável para dispositivos client a bateria

**Tipo:** Normativo

**Estado normativo:** Draft

**Estado da implementação:** Not Implemented

**Estado do workflow:** Rascunho e análise

**Versão:** 0.3

**Responsável arquitetural:** Marcelo Miranda

**Última atualização:** 11/08/2026

**Escopo:** `client_154` em ESP32-H2, fachada `SmartSysApp`, product firmware e
board model

---

## 1. Objetivo e escopo

Dispositivos client alimentados por bateria devem poder habilitar deep sleep
explicitamente no product firmware:

```cpp
smartSysApp.configureDeepSleep({
    .enabled = true,
    // demais campos
});
```

O recurso deve oferecer wakeup periódico por timer, com intervalo em minutos
ou horas, e um LED opcional que acende no wakeup. O LED deve aceitar polaridade
`HIGH` ou `LOW` e permanecer ligado por uma duração em milissegundos ou até o
próximo deep sleep.

Esta especificação cobre apenas o `client_154` em ESP32-H2. Não cobre light
sleep, sleep isolado do rádio, outras fontes de wakeup, retenção arbitrária em
RTC memory, política de bateria ou mudanças no coordenador, protocolo,
commissioning, ACK e retry.

## 2. API pública proposta

```cpp
namespace iotsmartsys::app
{

enum class DeepSleepTimeUnit : std::uint8_t
{
    Minutes,
    Hours,
};

enum class WakeLedOnMode : std::uint8_t
{
    DurationMs,
    UntilSleep,
};

struct TimerWakeupConfig
{
    bool enabled;
    std::uint32_t interval;
    DeepSleepTimeUnit unit;
};

struct WakeLedConfig
{
    bool enabled;
    gpio_num_t pin;
    bool activeHigh;
    WakeLedOnMode onMode;
    std::uint32_t onTimeMs;
};

struct DeepSleepConfig
{
    bool enabled;
    std::uint32_t maxAwakeTimeMs;
    TimerWakeupConfig timerWakeup;
    WakeLedConfig wakeLed;
};

}

iotsmartsys::AppResult
iotsmartsys::SmartSysApp::configureDeepSleep(
    const iotsmartsys::app::DeepSleepConfig &config);
```

Os nomes são propostos enquanto a especificação estiver em `Draft`. Mudança de
semântica exige nova decisão, não mero ajuste de implementação.

## 3. Regras de comportamento

### 3.1 Configuração

- ausência da chamada ou `enabled=false` preserva o runtime atual e não toca
  GPIO nem fontes de wakeup;
- somente uma chamada é aceita, sempre em `AppState::Configuring`;
- deep sleep habilitado exige `maxAwakeTimeMs > 0`;
- configuração duplicada, tardia ou inválida retorna resultado explícito e é
  preservada em `lastConfigurationResult()`;
- a configuração é copiada, mas nenhum recurso é iniciado antes de `setup()`.

### 3.2 Timer

- timer habilitado exige `interval > 0` e unidade `Minutes` ou `Hours`;
- o intervalo é contado desde o início do deep sleep e não inclui o tempo
  acordado;
- a conversão para microssegundos deve detectar overflow antes de configurar a
  fonte RTC;
- timer desabilitado não configura essa fonte de wakeup e permite acordar
  somente por reset ou nova energização;
- antes de dormir sem fonte configurada, a fachada registra diagnóstico
  explícito.

### 3.3 LED

- LED habilitado exige GPIO válido para saída;
- `activeHigh=true` acende em `HIGH`; `activeHigh=false` acende em `LOW`;
- `DurationMs` exige `onTimeMs > 0` e apaga o LED ao final do período;
- `UntilSleep` ignora `onTimeMs` e apaga o LED imediatamente antes do sleep;
- LED desabilitado não configura nem toma o GPIO;
- falha de configuração ou acionamento é observável;
- a inicialização deve evitar pulso visível de polaridade oposta, na medida
  suportada pelo ESP32-H2.

## 4. Responsabilidades e compatibilidade

- o product firmware decide ativação, duração máxima acordado, intervalo e
  duração do indicador;
- o board model fornece somente GPIO e polaridade elétrica do LED;
- `SmartSysApp` valida a configuração, identifica a causa do boot, controla o
  LED e coordena timer e entrada segura em deep sleep;
- componentes ISSP continuam responsáveis por device, reports e transporte e
  não conhecem produto, board, LED ou política de energia;
- produto que exige LED declara o recurso `wake_led`; o CMake rejeita board sem
  esse recurso conforme a ADR-0002;
- o GPIO do LED não pode colidir com capability, botão ou outro recurso da
  composição;
- nenhum produto atual passa a usar deep sleep implicitamente, mesmo que sua
  board seja alimentada por bateria;
- a primeira composição será o product firmware `door_sensor_battery_h2`,
  renomeado de `door_sensor`, com o board `Door Sensor Battery H2`;
- o nome do produto contém `h2` por decisão do Arquiteto, mas não autoriza
  pinagem ou outra responsabilidade física dentro do product firmware;
- esse board oferece o recurso `wake_led` no GPIO 13; produto e fachada
  recebem o GPIO exclusivamente pelo board model;
- Kconfig não governa lógica interna dos componentes compartilhados.

## 5. Ciclo de wakeup, sleep e falhas

Deep sleep reinicia o firmware. A configuração do product firmware é reaplicada
em cada boot e não precisa ser persistida pela fachada.

```text
wakeup/reset
→ identificar e registrar a causa
→ acionar LED em todo boot, quando habilitado
→ executar setup e trabalho do ciclo
→ dormir antecipadamente quando todos os reports do ciclo forem entregues
  ou iniciar encerramento forçado ao atingir maxAwakeTimeMs
→ estabilizar producers, reports, tasks e transporte
→ apagar LED, configurar wakeup e iniciar deep sleep
```

`maxAwakeTimeMs` é a janela operacional contada desde o início de `setup()` e
limita também commissioning e o estado `NotReady`. Ao expirar, começa o
encerramento forçado e limitado, sem retomar operação ou retry. Após `Running`,
a fachada pode antecipar o encerramento quando todos os reports produzidos no
boot estiverem entregues e não houver report reservado ou transmissão em
andamento. Se nenhum report for produzido, essa condição é satisfeita após o
setup.

Ao atingir a duração máxima, a fachada estabiliza producers, encerra tasks e
transporte e dorme mesmo com report sem ACK ou rede `NotReady`. Antes do sleep,
registra causa, contagem e estado das pendências, sem payload, e não as persiste
nesta versão. O encerramento deve ser limitado e não pode reabrir retry
indefinido. Reports entregues não são classificados como pendentes.

Configuração inválida falha antes de tocar NVS, rádio, RTC ou GPIO. Falha ao
preparar o encerramento antecipado adia o sleep até a duração máxima. Nesse
prazo, falha ao encerrar trabalho é registrada e não reabre retry; o deep sleep
ainda começa. Falha ao configurar uma fonte de wakeup solicitada bloqueia o
sleep para não tornar o device inacessível sem intenção. Causa de boot,
configuração desabilitada, início e bloqueio do sleep devem ser distinguíveis
por retorno ou log estruturado, sem conteúdo sensível.

## 6. Critérios de aceitação

- **DEEPSLEEP-AC-001 — Compatibilidade:** sem opt-in ou com `enabled=false`,
  API, setup e execução vigentes são preservados e nenhum recurso de sleep ou
  LED é iniciado.
- **DEEPSLEEP-AC-002 — Configuração:** configuração válida é copiada antes de
  `setup()`; duração máxima zero, outros valores inválidos, colisão de GPIO e
  chamada duplicada ou tardia são rejeitados antes da operação normal.
- **DEEPSLEEP-AC-003 — Timer:** intervalos válidos em minutos e horas são
  convertidos sem perda semântica; zero, unidade inválida e overflow são
  rejeitados antes de configurar RTC.
- **DEEPSLEEP-AC-004 — LED:** polaridades HIGH e LOW, `DurationMs` e
  `UntilSleep` produzem o nível e o tempo configurados; LED desabilitado não
  toma o GPIO e o indicador é apagado antes do sleep.
- **DEEPSLEEP-AC-005 — Fronteiras:** política permanece no product firmware,
  recurso físico no board model e lifecycle nos componentes responsáveis;
  `door_sensor_battery_h2` recebe do board o `wake_led` no GPIO 13, e composição
  sem esse recurso falha antes do binário.
- **DEEPSLEEP-AC-006 — Encerramento:** reports entregues permitem sleep
  antecipado após producers, tasks e transporte encerrarem. No fim de
  `maxAwakeTimeMs`, o encerramento forçado não reabre operação ou retry:
  registra reports, rede ou falhas pendentes sem persisti-los e inicia o sleep
  após finalização limitada. Falha da fonte de wakeup solicitada bloqueia o
  sleep.
- **DEEPSLEEP-AC-007 — Wakeup:** o LED acende em todo boot; timer wakeup, cold
  boot e outras causas permanecem distinguíveis, e o boot reaplica a
  configuração. Sleep sem timer é permitido e diagnosticado.
- **DEEPSLEEP-AC-008 — Evidência:** lógica cobre validação e falhas com doubles;
  build H2 cobre configurações habilitada e desabilitada; hardware H2 confronta
  timer, LED e corrente quando uma especificação futura autorizar execução.

Build não comprova comportamento físico, e nenhuma execução é autorizada por
esta especificação. Aplicam-se `Repository-Test-Execution-Policy.md` e a guarda
documental EKOM.

## 7. Decisões confirmadas pelo Arquiteto

- **DEEPSLEEP-DEC-001:** o dispositivo possui duração máxima acordado e dorme
  antes dela quando todos os reports do ciclo são entregues.
- **DEEPSLEEP-DEC-002:** o LED habilitado acende em todo boot; a causa é
  registrada separadamente.
- **DEEPSLEEP-DEC-003:** deep sleep sem fonte de wakeup é permitido com
  diagnóstico explícito.
- **DEEPSLEEP-DEC-004:** ao fim da duração máxima, o dispositivo dorme após
  registrar reports e rede pendentes, sem persistir reports nesta versão.
- **DEEPSLEEP-DEC-005:** a primeira composição renomeia o product firmware
  `door_sensor` para `door_sensor_battery_h2` e usa o LED GPIO 13 oferecido pelo
  board `Door Sensor Battery H2`.

A análise de implementabilidade e os impactos técnicos permanecem no relatório
`docs/reports/client-deep-sleep/analysis/2026-08-11-initial-analysis.md`. A
análise não identifica bloqueador normativo remanescente e recomenda prontidão,
mas a especificação continua em `Draft` até promoção e autorização explícitas
do Arquiteto.
