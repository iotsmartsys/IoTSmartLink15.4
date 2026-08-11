# Deep sleep configurável para dispositivos client a bateria

**Tipo:** Normativo

**Estado normativo:** Draft

**Estado da implementação:** Not Implemented

**Estado do workflow:** Rascunho; preparada para análise de implementabilidade

**Versão:** 0.7

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
  imediatamente antes do deep sleep, incluindo arbitragem com o factory reset.
  Não cria `stop()` público, retry de `setup()`, destruição de objetos nem novo
  estado público; o lifetime estático permanece válido até o reboot causado
  pelo deep sleep ou pelo factory reset.
- **Altera (`Amends`) `Firmware-Variants-Menuconfig.md`:** renomeia o product
  firmware `door_sensor` para `door_sensor_battery_h2` e acrescenta o requisito
  de composição `wake_led`. O alcance completo do renome está definido na
  seção 5.
- **Altera (`Amends`) `ISSP-Reusable-Components.md` v1.1:** por decisão do
  Arquiteto, amplia materialmente e de forma limitada as APIs públicas de
  `issp_core`, `issp_behaviors` e `issp_transport_154` com as operações de
  quiescência definidas na seção 6. A ampliação serve somente ao encerramento
  seguro antes de deep sleep e não autoriza generalização adicional.
- **Preserva `ISSP-Architecture.md` v1.2:** a aplicação continua dona da regra
  de produto; transporte, reports e device conservam suas responsabilidades. A
  quiescência usa seus contratos de encerramento sem transferir política de
  energia aos componentes ISSP.
- **Preserva `ISSP-Commissioning.md` v1.0:** canais, tentativas, validação e
  persistência não mudam. `maxAwakeTimeMs` apenas limita externamente o ciclo
  acordado e pode interrompê-lo para dormir, inclusive durante persistência. No
  boot seguinte, descritor válido é aplicado, ausência inicia commissioning e
  descritor inválido é rejeitado com falha segura, sem uso parcial, reboot
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
  dossiê;
- referência ao símbolo em `client_154/sdkconfig`, atualizada diretamente no
  mesmo renome, sem depender de regeneração por build.

Nomes de board e símbolos próprios do board não são renomeados. Relatórios,
commits e demais registros históricos preservam o nome observado à época.

A validação de colisão é aditiva e restrita ao `wake_led`: ao configurá-lo, a
fachada o compara aos GPIOs de capabilities e factory reset já registrados; ao
registrar esses recursos depois dele, faz a comparação inversa. Com deep sleep
ou LED desabilitado, não se cria validação global entre pares antes aceitos.

## 6. Ciclo acordado, quiescência e falhas

Deep sleep reinicia o firmware. A configuração do product firmware é reaplicada
em cada boot e não é persistida pela fachada.

```text
wakeup/reset
→ setup captura o início da janela, registra a causa e acende o LED
→ executa setup e trabalho do ciclo
→ após Running, inicia quiescência antecipada somente quando ao menos um report
  inicial esperado foi admitido e todos os reports iniciais esperados estão
  prontos, ou força-a ao atingir maxAwakeTimeMs
→ prepara a fonte de wakeup solicitada; falha interrompe o encerramento
→ fecha admissão e estabiliza producers, executor e transporte
→ registra o resultado, apaga o LED e inicia deep sleep
```

`SmartSysApp` é dona do deadline contado desde a entrada de `setup()`. O
deadline continua ativo durante commissioning, `NotReady`, `Running` e mesmo
enquanto a pilha chamadora não recuperou o controle. A implementação deve
possuir mecanismo capaz de observar o prazo independentemente do retorno de
`app_main()` ou de uma etapa bloqueante de `setup()`.

Sleep antecipado exige evidência positiva de report neste boot. São reports
iniciais esperados aqueles cujos behaviors têm `reportOnStart=true`. Deve
existir pelo menos um, e todos precisam confirmar que sua publicação inicial
foi admitida. Para `DigitalInputBehavior`, `initial_stabilization_pending` não é
essa confirmação e mantém o device acordado. Se nenhum report inicial for
esperado ou a estabilização não concluir, somente o deadline permite dormir.

A fachada determina essa evidência com dados que já possui: configuração
`reportOnStart`, sucesso síncrono de `DigitalOutputBehavior::begin()` e
`DigitalInputBehavior::hasConfirmedState()`, que só confirma o estado inicial
depois de uma publicação requerida bem-sucedida. Não se adiciona quarta
operação à API reutilizável para esse controle.

Depois da admissão dos reports iniciais, o device fecha novas admissões e os
behaviors entram em quiescência. Então `pendingReportCount() == 0` prova a
entrega: a contagem inclui slots reservados e transmissões em andamento. Falha
não retryable que conserve slot ocupado impede o sleep antecipado e é resolvida
somente pelo deadline.

A quiescência é lifecycle privado da fachada, limitado ao preparo do deep
sleep. Para torná-lo executável, esta especificação autoriza somente as
seguintes ampliações materiais dos contratos reutilizáveis:

- `virtual IsspResult IDeviceBehavior::quiesce()`: operação pública,
  idempotente e terminal no boot. A implementação padrão só é válida para
  behavior que não produz trabalho autonomamente; `DigitalInputBehavior` a
  sobrescreve para parar e excluir seu timer sem destruir o objeto nem publicar
  novo report;
- `IsspResult IsspDevice::beginQuiescence()`: operação pública e idempotente
  que fecha atomicamente o despacho de novos comandos e a admissão de novos
  reports, responde `IsspCommandResult::Failed` a comandos ainda recebidos,
  retorna `IsspResult::NotReady` às novas publicações e preserva todos os slots
  já admitidos;
- `IsspResult Issp154ReportExecutor::stop()`: operação pública, idempotente e
  terminal no boot. Desregistra notificações, interrompe espera ou delay de
  retry, aguarda de forma limitada a tentativa de transporte já ativa e
  encerra sua task sem destruir o executor. O retry de 1000 ms usa espera por
  notificação com timeout, preservando a duração vigente e permitindo a
  interrupção sem `xTaskAbortDelay`, opção de Kconfig ou dependência externa. A
  notificação de parada é distinguida por flag atômica;
- `Issp154Transport::end()` permanece o contrato vigente e não recebe nova
  semântica.

Nenhuma dessas operações permite reiniciar o objeto no mesmo boot. `stop()`
encerra somente as tentativas do boot atual; reports não entregues permanecem
nos slots voláteis até o deep sleep e não são persistidos. A ampliação não cria
`SmartSysApp::stop()`, não torna `setup()` repetível e não autoriza outra
generalização das APIs compartilhadas.

### 6.1 Arbitragem com factory reset

Factory reset e deep sleep compartilham uma arbitragem atômica de transição. A
primeira transição aceita vence:

- factory reset é aceito quando o hold configurado se completa e a transição é
  adquirida; nesse caso, qualquer início de deep sleep é cancelado, a limpeza
  exclusiva do vínculo de rede termina e `esp_restart()` prossegue;
- deep sleep é aceito quando seu gatilho adquire a transição; a partir daí,
  novas solicitações de factory reset são rejeitadas e diagnosticadas neste
  boot;
- se a preparação de uma fonte de wakeup solicitada falhar, a fachada libera a
  transição antes de qualquer encerramento terminal, e factory reset volta a
  poder ser aceito;
- limpeza do vínculo de rede e commit da entrada em deep sleep são mutuamente
  exclusivos, pois ambos passam pela fachada;
- pedido de factory reset rejeitado enquanto o sleep detém a transição não
  consome o hold em curso. Se a transição for liberada por falha de wakeup, o
  monitor pode reapresentar o pedido sem exigir soltura e nova pressão.

Depois que o deep sleep vence e a fonte de wakeup está preparada,
`ResetButtonMonitor` deve encerrar sua task por operação interna, idempotente e
terminal no boot. Seu polling usa espera por notificação com
`pollIntervalMs` como timeout, preservando o período configurado e permitindo
parada limitada sem depender de Kconfig. A parada não altera GPIO, polaridade,
hold ou semântica do factory reset fora da transição para sleep.

Esta versão não garante que o hold de factory reset termine antes de um sleep
antecipado. A usabilidade do hold de 10 s no produto a bateria permanece tema
de configuração ou especificação futura; não se cria aqui duração mínima para
`maxAwakeTimeMs` nem se altera o hold vigente.

`persistNetwork()` permanece privado e opaco dentro de `initializeNetwork()`;
não participa dessa arbitragem e não justifica ampliar novamente a API
reutilizável. Se o deadline expirar enquanto `SetupStage::InitializeNetwork`
estiver ativo, a fachada pode iniciar deep sleep mesmo com `nvs_commit()` em
curso. Antes disso, registra `persistence_preemption_possible=true`, sem alegar
que observou a janela privada. No boot seguinte, o fluxo vigente aplica um
descritor válido, inicia commissioning se ele estiver ausente ou falha de forma
segura se estiver inválido. O experimento só aceita a preempção se observar
descritor anterior válido, novo válido ou ausência, nunca conteúdo parcial ou
inválido. Esse comportamento não é certificado por leitura ou build.

A ordem obrigatória é:

```text
adquirir a transição de deep sleep; abortar se factory reset já venceu
→ preparar a fonte de wakeup solicitada, quando houver
→ encerrar ResetButtonMonitor, quando configurado
→ beginQuiescence no device
→ quiesce em todos os behaviors registrados
→ aguardar pendingReportCount() == 0 no caminho antecipado
  ou apenas registrar a contagem no deadline
→ stop no report executor
→ se stop concluir em até 600 ms, end no transporte
  senão registrar o timeout e preservar o transporte sem chamar end
→ apagar LED e iniciar deep sleep
```

Falha ao preparar uma fonte solicitada aborta a sequência antes de qualquer
operação terminal, libera a arbitragem e preserva o runtime acessível. Ausência
intencional de fonte não aborta e é diagnosticada conforme a seção 4.2.

No caminho antecipado, a fachada inicia essa sequência depois que `setup()`
atinge `Running`, existe ao menos um report inicial esperado e todos foram
admitidos. Fechar a admissão e parar os producers torna estável o oráculo
`pendingReportCount() == 0`. No deadline, a mesma sequência não exige report
admitido nem aguarda entrega das pendências.

Os 600 ms de espera por `stop()` cobrem o pior caso estimado de aproximadamente
465 ms para a tentativa ativa, com margem de escalonamento. Se o limite
estourar, chamar `Issp154Transport::end()` é proibido porque o executor ainda
pode usar o event group de ACK; o deep sleep prossegue e o reboot encerra o
rádio. O timeout é registrado explicitamente.

Ao atingir `maxAwakeTimeMs`, a fachada inicia quiescência forçada e dorme mesmo
com report sem ACK ou rede `NotReady`. Antes do sleep, registra causa, contagem
e estado das pendências sem payload e não persiste reports. Falha de
quiescência é sempre registrada e não reabre operação ou retry. No caminho
antecipado, ela cancela somente a antecipação e o device aguarda o deadline no
estado já alcançado. No deadline, a falha não impede o sleep. Falha ao
preparar uma fonte de wakeup solicitada bloqueia a própria sequência, antes da
quiescência, para não tornar o dispositivo inacessível sem intenção.

Os pontos cooperativos hoje observáveis têm ordem de grandeza de até 0,7 s
entre canais do commissioning e 1,47 s entre iterações do executor. Esses
valores são nota de análise, não limite contratual nem mínimo para
`maxAwakeTimeMs`. Um supervisor pode atuar como backstop, mas não pode iniciar
deep sleep enquanto a limpeza de factory reset detiver a transição. A preempção
durante `persistNetwork()` e a corrida com factory reset exigem os experimentos
da seção 8.

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
  públicos vigentes. A colisão compara somente `wake_led` aos demais GPIOs e
  não altera validações quando o recurso está desabilitado.
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
  deve haver pelo menos um report inicial esperado e todos precisam ter sido
  admitidos; só então `pendingReportCount() == 0` permite sleep antecipado e
  prova entrega sem reserva ou transmissão. Ausência de report e
  `initial_stabilization_pending` aguardam o deadline; slot ocupado por falha
  não retryable também.
- **DEEPSLEEP-AC-007 — Deadline e quiescência:** o deadline é observado em
  `setup()`, commissioning, `NotReady` e `Running`, inclusive sob etapa
  bloqueante; a quiescência segue a ordem da seção 6, usa somente as operações
  públicas autorizadas nos componentes, não destrói objetos e não cria
  `SmartSysApp::stop()` ou retry público. `stop()` possui limite de 600 ms; no
  estouro, `end()` não é chamado.
- **DEEPSLEEP-AC-008 — Sleep forçado:** no deadline, reports, rede e falhas
  pendentes são registrados sem persistência; falha de quiescência não reabre
  trabalho e o sleep começa. Falha da fonte de wakeup solicitada bloqueia o
  sleep.
- **DEEPSLEEP-AC-008A — Arbitragem:** factory reset e deep sleep obedecem à
  primeira transição aceita; limpeza do vínculo e commit do sleep não se
  sobrepõem. Se sleep vencer, o monitor termina antes da quiescência; se factory
  reset vencer, a limpeza e o restart não são preemptados. Pedido rejeitado não
  consome o hold em curso.
- **DEEPSLEEP-AC-008B — Persistência:** o deadline pode preemptar
  `persistNetwork()` com diagnóstico de possibilidade, sem nova API pública. O
  boot seguinte conserva o fluxo vigente de validação. O experimento deve
  produzir descritor anterior válido, novo válido ou ausência, sem conteúdo
  parcial ou inválido, antes da aceitação.
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
- **DEEPSLEEP-DEC-006:** fica autorizada a ampliação material e limitada das
  APIs reutilizáveis por `IDeviceBehavior::quiesce()`,
  `IsspDevice::beginQuiescence()` e `Issp154ReportExecutor::stop()`, com
  semântica terminal no boot e sem `SmartSysApp::stop()` público.
- **DEEPSLEEP-DEC-007:** factory reset e deep sleep seguem a regra "primeira
  transição aceita vence", com exclusão entre limpeza do vínculo e commit do
  sleep; o perdedor não preempta a transição aceita.
- **DEEPSLEEP-DEC-008:** `persistNetwork()` pode ser preemptido pelo deadline,
  com diagnóstico e experimento obrigatório; não se cria quarta ampliação da
  API reutilizável para observar sua janela privada.
- **DEEPSLEEP-DEC-009:** sleep antecipado exige ao menos um report inicial
  esperado, todos os reports iniciais esperados admitidos e, depois da
  quiescência, `pendingReportCount() == 0`. Ausência de report não equivale a
  entrega.

A v0.7 incorpora os achados da análise de implementabilidade da v0.6. Uma nova
análise deve confirmar a preempção diagnosticada de `persistNetwork()`, a
evidência positiva de report inicial, o timeout de `stop()` com supressão de
`end()` e o re-arme do hold. Os comportamentos de NVS e as corridas continuam
exigindo experimento explícito; não são presumidos por inspeção ou build.

Fontes de evidência existentes:

- `docs/reports/client-deep-sleep/analysis/2026-08-11-initial-analysis.md`;
- `docs/reports/client-deep-sleep/analysis/2026-08-11-verification-analysis.md`;
- `docs/reports/client-deep-sleep/analysis/2026-08-11-v04-implementability-analysis.md`;
- `docs/reports/client-deep-sleep/analysis/2026-08-11-v05-implementability-analysis.md`;
- `docs/reports/client-deep-sleep/analysis/2026-08-11-v06-implementability-analysis.md`.

O documento permanece `Draft`. Estar preparado para análise não autoriza
implementação, promoção, execução de testes ou integração.
