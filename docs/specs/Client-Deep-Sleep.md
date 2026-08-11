# Deep sleep configurável para dispositivos client a bateria

**Tipo:** Normativo

**Estado normativo:** Draft

**Estado da implementação:** Not Implemented

**Estado do workflow:** Rascunho; preparada para análise de implementabilidade

**Versão:** 0.4

**Responsável arquitetural:** Marcelo Miranda

**Última atualização:** 11/08/2026

**Escopo:** `client_154` em ESP32-H2, fachada `SmartSysApp`, product firmware e
board model

---

## 1. Objetivo e recorte

Dispositivos client alimentados por bateria devem poder habilitar deep sleep
explicitamente no product firmware:

```cpp
smartSysApp.configureDeepSleep({
    .enabled = true,
    // demais campos
});
```

O recurso oferece wakeup periódico por timer, com intervalo em minutos ou
horas, e um LED opcional que acende em todo boot. O LED aceita polaridade
`HIGH` ou `LOW` e permanece ligado por uma duração em milissegundos ou até o
próximo deep sleep.

O recorte cobre somente o `client_154` em ESP32-H2. Não cobre light sleep,
sleep isolado do rádio, outras fontes de wakeup, retenção arbitrária em RTC
memory, política de bateria, persistência de reports nem mudanças no
coordenador, protocolo ISSP, ACK ou retry.

## 2. Relações com autoridades vigentes

Esta especificação propõe as seguintes relações normativas, efetivas somente
se o Arquiteto promover o documento:

- **Altera (`Amends`) `ISSP-Configurable-Bootstrap.md` v1.5:** acrescenta
  `configureDeepSleep()` à API pública e uma quiescência privada e limitada
  imediatamente antes do deep sleep. Não cria `stop()` público, retry de
  `setup()`, destruição de objetos nem novo estado público; o lifetime estático
  permanece válido até o reboot causado pelo deep sleep.
- **Altera (`Amends`) `Firmware-Variants-Menuconfig.md`:** renomeia o product
  firmware `door_sensor` para `door_sensor_battery_h2` e acrescenta o requisito
  de composição `wake_led`. O alcance completo do renome está definido na
  seção 5.
- **Preserva `ISSP-Architecture.md` v1.2:** a aplicação continua dona da regra
  de produto; transporte, reports e device conservam suas responsabilidades. A
  quiescência usa seus contratos de encerramento sem transferir política de
  energia aos componentes ISSP.
- **Preserva `ISSP-Commissioning.md` v1.0:** canais, tentativas, validação e
  persistência não mudam. `maxAwakeTimeMs` apenas limita externamente o ciclo
  acordado e pode interrompê-lo para dormir, sem transformar falha em reboot
  contínuo ou busca infinita.
- **Preserva ADR-0002:** product firmware define política e declara recursos;
  board model fornece GPIO e polaridade; Kconfig seleciona a composição.
- **Preserva `Repository-Test-Execution-Policy.md` v0.4:** esta especificação
  não autoriza build, coleta, flash, teste ou execução em hardware.

Ficam fora deste recorte alterações nas autoridades preservadas. Divergência
encontrada pela análise deve retornar ao Autor ou ao Arquiteto; não pode ser
resolvida como detalhe de implementação.

## 3. API pública proposta

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

Os nomes permanecem propostos enquanto o documento estiver em `Draft`.
Erros de configuração usam o `AppResult` vigente e são preservados em
`lastConfigurationResult()`. Falhas ao inicializar a política ou o LED durante
`setup()` usam `SetupStage::InitializePlatform` e o `AppResult` vigente. Não se
introduzem novos valores em `AppResult`, `SetupStage` ou `AppState`.

## 4. Contrato de configuração e comportamento

### 4.1 Configuração

- ausência da chamada ou `enabled=false` preserva o runtime atual e não toca
  GPIO nem fontes de wakeup;
- somente uma chamada é aceita, sempre em `AppState::Configuring`;
- deep sleep habilitado exige `maxAwakeTimeMs > 0`;
- configuração duplicada, tardia ou inválida retorna resultado explícito;
- a configuração é copiada, mas nenhum recurso é iniciado antes de `setup()`.

### 4.2 Timer

- timer habilitado exige `interval > 0` e unidade `Minutes` ou `Hours`;
- o intervalo é contado desde a entrada em deep sleep e não inclui o tempo
  acordado;
- a conversão para microssegundos detecta overflow antes de configurar RTC;
- timer desabilitado não configura essa fonte e permite acordar somente por
  reset ou nova energização;
- antes de dormir sem fonte configurada, a fachada registra diagnóstico
  explícito.

### 4.3 LED e ordem de inicialização

- LED habilitado exige GPIO válido para saída;
- `activeHigh=true` acende em `HIGH`; `activeHigh=false` acende em `LOW`;
- `DurationMs` exige `onTimeMs > 0` e apaga o LED ao final do período;
- `UntilSleep` ignora `onTimeMs` e apaga o LED imediatamente antes do sleep;
- LED desabilitado não configura nem toma o GPIO;
- na entrada de `setup()`, a fachada captura o início da janela acordada; a
  primeira operação de plataforma é identificar a causa do boot e configurar
  e acender o LED, antes de NVS, commissioning, rádio ou reports;
- a inicialização evita pulso visível de polaridade oposta na medida suportada
  pelo ESP32-H2, e qualquer falha é registrada.

## 5. Responsabilidades, composição e renome

- o product firmware decide ativação, duração máxima acordado, intervalo e
  duração do indicador;
- o board model fornece GPIO e polaridade elétrica do LED;
- `SmartSysApp` valida a configuração, identifica a causa do boot, controla o
  LED e coordena timer, deadline e entrada segura em deep sleep;
- componentes ISSP continuam responsáveis por device, reports e transporte e
  não conhecem produto, board, LED ou política de energia;
- produto que exige LED declara `wake_led`; o CMake rejeita board sem esse
  recurso conforme a ADR-0002;
- o GPIO do LED não pode colidir com capability, botão ou outro recurso;
- nenhum produto atual passa a usar deep sleep implicitamente;
- a primeira composição usa o product firmware `door_sensor_battery_h2` e o
  board `Door Sensor Battery H2`, que oferece `wake_led` no GPIO 13;
- o nome do produto contém `h2` por decisão do Arquiteto, sem autorizar pinagem
  ou lógica de board no product firmware;
- produto e fachada recebem o GPIO exclusivamente pelo board model; Kconfig
  não governa lógica interna de componentes compartilhados.

O renome de `door_sensor` abrange:

- fonte `firmwares/door_sensor.cpp` para
  `firmwares/door_sensor_battery_h2.cpp`;
- símbolo `IOTSMARTLINK154_PRODUCT_DOOR_SENSOR` para
  `IOTSMARTLINK154_PRODUCT_DOOR_SENSOR_BATTERY_H2`;
- rótulo de menu para `Door sensor battery H2`;
- seleção, branch e nome correspondentes no CMake;
- referências atuais do product firmware em documentação normativa, mapa e
  dossiê.

Nomes de board e símbolos próprios do board não são renomeados. Relatórios,
commits e demais registros históricos preservam o nome observado à época.

## 6. Ciclo acordado, quiescência e falhas

Deep sleep reinicia o firmware. A configuração do product firmware é reaplicada
em cada boot e não é persistida pela fachada.

```text
wakeup/reset
→ setup captura o início da janela, registra a causa e acende o LED
→ executa setup e trabalho do ciclo
→ solicita quiescência antecipada quando os producers do boot terminarem e
  pendingReportCount() == 0, ou força-a ao atingir maxAwakeTimeMs
→ fecha admissão de novo trabalho e estabiliza producers, executor e transporte
→ registra o resultado, apaga o LED, configura wakeup e inicia deep sleep
```

`SmartSysApp` é dona do deadline contado desde a entrada de `setup()`. O
deadline continua ativo durante commissioning, `NotReady`, `Running` e mesmo
enquanto a pilha chamadora não recuperou o controle. A implementação deve
possuir mecanismo capaz de observar o prazo independentemente do retorno de
`app_main()` ou de uma etapa bloqueante de `setup()`.

Depois que os producers do boot terminaram e não podem admitir novo report,
`pendingReportCount() == 0` é o oráculo de sleep antecipado: a contagem inclui
slots reservados e transmissões em andamento. Falha não retryable que conserve
slot ocupado impede o sleep antecipado e é resolvida somente pelo deadline.

A quiescência é operação interna, privada e limitada ao preparo do deep sleep.
Ela fecha a produção de trabalho, encerra ou estabiliza as tasks e o transporte
pelos contratos existentes e mantém fachada e objetos estáticos vivos até o
reboot. Não é novo lifecycle público, não torna `setup()` repetível e não
autoriza reinício do runtime no mesmo boot.

Ao atingir `maxAwakeTimeMs`, a fachada inicia quiescência forçada e dorme mesmo
com report sem ACK ou rede `NotReady`. Antes do sleep, registra causa, contagem
e estado das pendências sem payload e não persiste reports. Falha de
quiescência é registrada, não reabre operação ou retry e não impede o sleep.
Falha ao configurar uma fonte de wakeup solicitada bloqueia o sleep para não
tornar o dispositivo inacessível sem intenção.

Configuração inválida falha antes de tocar NVS, rádio, RTC ou GPIO. Falha de
plataforma durante o início de `setup()` produz o `SetupResult` vigente quando
o fluxo puder retornar; se o deadline conduzir diretamente ao deep sleep, o
resultado operacional permanece no log estruturado. Causa de boot,
configuração desabilitada, sleep antecipado, deadline, pendências, início e
bloqueio do sleep devem ser distinguíveis sem conteúdo sensível.

## 7. Critérios de aceitação

- **DEEPSLEEP-AC-001 — Compatibilidade:** sem opt-in ou com `enabled=false`, a
  API e execução vigentes são preservadas e nenhum recurso de sleep ou LED é
  iniciado.
- **DEEPSLEEP-AC-002 — Configuração:** configuração válida é copiada antes de
  `setup()`; duração máxima zero, valores inválidos, colisão de GPIO e chamada
  duplicada ou tardia são rejeitados antes da operação normal pelos resultados
  públicos vigentes.
- **DEEPSLEEP-AC-003 — Timer:** minutos e horas válidos são convertidos sem
  perda semântica; zero, unidade inválida e overflow são rejeitados antes de
  configurar RTC.
- **DEEPSLEEP-AC-004 — LED:** polaridades HIGH e LOW, `DurationMs` e
  `UntilSleep` produzem nível e tempo configurados; o LED acende em todo boot
  como primeira operação de plataforma e apaga antes do sleep.
- **DEEPSLEEP-AC-005 — Composição e renome:** política permanece no produto e
  recurso físico no board; `door_sensor_battery_h2` recebe `wake_led` GPIO 13
  do board, composição incompatível falha antes do binário e todo o alcance do
  renome da seção 5 é aplicado sem alterar registros históricos.
- **DEEPSLEEP-AC-006 — Entrega:** com producers encerrados,
  `pendingReportCount() == 0` permite sleep antecipado e prova ausência de
  reserva ou transmissão. Slot ocupado por falha não retryable aguarda o
  deadline.
- **DEEPSLEEP-AC-007 — Deadline e quiescência:** o deadline é observado em
  `setup()`, commissioning, `NotReady` e `Running`, inclusive sob etapa
  bloqueante; a quiescência é privada, limitada, não destrói objetos e não cria
  `stop()` ou retry público.
- **DEEPSLEEP-AC-008 — Sleep forçado:** no deadline, reports, rede e falhas
  pendentes são registrados sem persistência; falha de quiescência não reabre
  trabalho e o sleep começa. Falha da fonte de wakeup solicitada bloqueia o
  sleep.
- **DEEPSLEEP-AC-009 — Wakeup:** timer wakeup, cold boot e outras causas são
  distinguíveis; o boot reaplica a configuração e sleep sem timer é permitido
  com diagnóstico.
- **DEEPSLEEP-AC-010 — Evidência futura:** lógica de validação e falhas é
  verificável com doubles; build H2 cobre composições habilitada e desabilitada;
  hardware H2 confronta timer, LED, corrente e o deadline sob bloqueio somente
  quando especificação futura autorizar execução.

Build não comprova comportamento físico. Esta especificação não autoriza
executar builds ou testes.

## 8. Decisões do Arquiteto e entrega à análise

- **DEEPSLEEP-DEC-001:** há duração máxima acordado e sleep antecipado quando
  todos os reports do ciclo forem entregues.
- **DEEPSLEEP-DEC-002:** o LED habilitado acende em todo boot; a causa é
  registrada separadamente.
- **DEEPSLEEP-DEC-003:** deep sleep sem fonte de wakeup é permitido com
  diagnóstico explícito.
- **DEEPSLEEP-DEC-004:** no deadline, o dispositivo dorme após registrar
  reports e rede pendentes, sem persistir reports nesta versão.
- **DEEPSLEEP-DEC-005:** a primeira composição renomeia `door_sensor` para
  `door_sensor_battery_h2` e usa o LED GPIO 13 do board
  `Door Sensor Battery H2`.

A v0.4 incorpora os achados normativos da análise de verificação da v0.3. Uma
nova análise de implementabilidade deve confrontar as relações da seção 2, o
contrato de quiescência, o alcance do renome e a capacidade de observar o
deadline durante operações bloqueantes. Essa última capacidade exige
experimento explícito; não é presumida por inspeção ou build.

Fontes de evidência existentes:

- `docs/reports/client-deep-sleep/analysis/2026-08-11-initial-analysis.md`;
- `docs/reports/client-deep-sleep/analysis/2026-08-11-verification-analysis.md`.

O documento permanece `Draft`. Estar preparado para análise não autoriza
implementação, promoção, execução de testes ou integração.
