# Deep sleep configurável para dispositivos client a bateria

**Tipo:** Normativo

**Estado normativo:** Draft

**Estado da implementação:** Not Implemented

**Estado do workflow:** Rascunho e análise

**Versão:** 0.2 experimental

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
- configuração duplicada, tardia ou inválida retorna resultado explícito e é
  preservada em `lastConfigurationResult()`;
- a configuração é copiada, mas nenhum recurso é iniciado antes de `setup()`.

### 3.2 Timer

- timer habilitado exige `interval > 0` e unidade `Minutes` ou `Hours`;
- o intervalo é contado desde o início do deep sleep e não inclui o tempo
  acordado;
- a conversão para microssegundos deve detectar overflow antes de configurar a
  fonte RTC;
- timer desabilitado não configura essa fonte de wakeup; a validade de deep
  sleep sem timer depende de `DEEPSLEEP-PEND-003`.

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

- o product firmware decide ativação, intervalo e duração do indicador;
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
- Kconfig não governa lógica interna dos componentes compartilhados.

## 5. Ciclo de wakeup, sleep e falhas

Deep sleep reinicia o firmware. A configuração do product firmware é reaplicada
em cada boot e não precisa ser persistida pela fachada.

```text
wakeup/reset
→ identificar causa e acionar LED, quando aplicável
→ executar setup e trabalho do ciclo
→ atingir o gatilho de sleep ainda pendente
→ estabilizar producers, reports, tasks e transporte
→ apagar LED, configurar wakeup e iniciar deep sleep
```

Antes do sleep não pode haver report reservado, transmissão em andamento ou
task afetada sem resultado terminal. O runtime atual ainda não possui parada
coordenada e pode repetir reports indefinidamente; a política terminal depende
de `DEEPSLEEP-PEND-001` e `DEEPSLEEP-PEND-004`.

Configuração inválida falha antes de tocar NVS, rádio, RTC ou GPIO. Falha ao
preparar o runtime ou configurar wakeup impede o deep sleep e informa etapa e
resultado. Causa de boot, configuração desabilitada, início e bloqueio do sleep
devem ser distinguíveis por retorno ou log estruturado, sem conteúdo sensível.

## 6. Critérios de aceitação

- **DEEPSLEEP-AC-001 — Compatibilidade:** sem opt-in ou com `enabled=false`,
  API, setup e execução vigentes são preservados e nenhum recurso de sleep ou
  LED é iniciado.
- **DEEPSLEEP-AC-002 — Configuração:** configuração válida é copiada antes de
  `setup()`; valores inválidos, colisão de GPIO e chamada duplicada ou tardia
  são rejeitados antes da operação normal.
- **DEEPSLEEP-AC-003 — Timer:** intervalos válidos em minutos e horas são
  convertidos sem perda semântica; zero, unidade inválida e overflow são
  rejeitados antes de configurar RTC.
- **DEEPSLEEP-AC-004 — LED:** polaridades HIGH e LOW, `DurationMs` e
  `UntilSleep` produzem o nível e o tempo configurados; LED desabilitado não
  toma o GPIO e o indicador é apagado antes do sleep.
- **DEEPSLEEP-AC-005 — Fronteiras:** política permanece no product firmware,
  recurso físico no board model e lifecycle nos componentes responsáveis;
  composição sem `wake_led` exigido falha antes do binário.
- **DEEPSLEEP-AC-006 — Encerramento:** deep sleep começa somente após producers,
  reports, tasks e transporte atingirem a condição terminal aprovada; falha de
  preparação bloqueia o sleep, enquanto orçamento expirado só permite dormir
  se tiver sido definido como resultado terminal, sempre observável.
- **DEEPSLEEP-AC-007 — Wakeup:** timer wakeup, cold boot e outras causas
  confrontadas permanecem distinguíveis, e o boot reaplica a configuração.
- **DEEPSLEEP-AC-008 — Evidência:** lógica cobre validação e falhas com doubles;
  build H2 cobre configurações habilitada e desabilitada; hardware H2 confronta
  timer, LED e corrente quando uma especificação futura autorizar execução.

Build não comprova comportamento físico, e nenhuma execução é autorizada por
esta especificação. Aplicam-se `Repository-Test-Execution-Policy.md` e a guarda
documental EKOM.

## 7. Decisões pendentes do Arquiteto

- **DEEPSLEEP-PEND-001 — Gatilho para dormir (bloqueante):** escolher chamada
  explícita, duração máxima acordado, conclusão de reports ou combinação. A
  autoria recomenda duração máxima com encerramento antecipado após reports.
- **DEEPSLEEP-PEND-002 — Boots que acendem o LED (bloqueante):** decidir entre
  somente wakeup de deep sleep ou todo boot/reset. A autoria recomenda todo
  boot, registrando a causa separadamente.
- **DEEPSLEEP-PEND-003 — Sleep sem fonte de wakeup:** decidir se timer
  desabilitado permite acordar apenas por reset/alimentação. A autoria recomenda
  permitir com diagnóstico explícito.
- **DEEPSLEEP-PEND-004 — Report e rede sem resultado (bloqueante):** definir o
  orçamento para retry, report sem ACK e rede `NotReady`. A autoria recomenda
  dormir ao fim da duração máxima, registrando pendências sem persistir reports
  nesta versão.
- **DEEPSLEEP-PEND-005 — Primeira composição:** escolher product firmware e
  confirmar o LED oferecido pela board da primeira implementação.

A análise de implementabilidade e os impactos técnicos permanecem no relatório
`docs/reports/client-deep-sleep/analysis/2026-08-11-initial-analysis.md`. A
especificação continua em `Draft`; somente o Arquiteto pode promover seu estado
e autorizar implementação.
