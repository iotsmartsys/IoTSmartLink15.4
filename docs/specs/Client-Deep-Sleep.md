# Deep sleep configurável para dispositivos client a bateria

**Tipo:** Normativo

**Estado normativo:** `Proposed` — Pronta e autorizada para implementação para
a v0.11; a v0.10 permanece `Proposed`

**Estado da implementação:** não iniciada para a v0.11; `In Progress` somente
para a v0.10

**Estado do workflow:** Autorizada para implementação para a v0.11

**Análise de implementabilidade:** `Ready` para a revisão `ea95c77`, conforme o
confronto final r5; `Ready` também permanece vigente para a v0.10

**Autorização de implementação desta versão:** concedida pelo Arquiteto em
12/08/2026 para a v0.11

**Versão:** 0.11

**Responsável arquitetural:** Marcelo Miranda

**Última atualização:** 12/08/2026

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

O recurso oferece duas fontes de wakeup independentes — timer periódico, com
intervalo em minutos ou horas, e transição do contato seco por EXT1 — e um LED
opcional que acende em todo boot operacional cuja configuração válida alcance a
inicialização de plataforma.

O wakeup por contato existe para que um sensor de porta a bateria reporte a
transição quando ela ocorre, e não somente no próximo período do timer. Em todo
boot operacional o dispositivo reporta e rearma o contato para o nível oposto ao
observado no início da sequência terminal, antes de qualquer operação terminal,
qualquer que seja esse nível e qualquer que tenha sido a causa do boot. O LED
aceita polaridade `HIGH` ou `LOW` e permanece ligado por uma duração em
milissegundos ou até o próximo deep sleep.

O recorte cobre somente o `client_154` em ESP32-H2. Não cobre light sleep,
sleep isolado do rádio, fontes de wakeup além do timer e do contato seco por
EXT1, retenção arbitrária em RTC memory, política de bateria, persistência de
reports nem mudanças no coordenador, protocolo ISSP, ACK ou retry.

A v0.11 acrescenta somente o wakeup por contato e o que ele exige. Todo o
restante do contrato da v0.10 permanece vigente e já implementado; esta versão
não o reabre.

## 2. Relações com autoridades vigentes

As relações abaixo valem para o contrato integral. As da v0.10 estão em
`Proposed` e implementadas; as marcadas como acréscimo da v0.11 também estão em
`Proposed`, promovidas pelo Arquiteto após o confronto final `Ready`.

- **Altera (`Amends`) `ISSP-Configurable-Bootstrap.md` v1.5:** acrescenta
  `configureDeepSleep()` à API pública e uma quiescência privada e limitada
  imediatamente antes do deep sleep, incluindo arbitragem com o factory reset.
  Não cria `stop()` público, retry de `setup()`, destruição de objetos nem novo
  estado público; o lifetime estático permanece válido até o reboot causado
  pelo deep sleep ou pelo factory reset.
- **Altera (`Amends`) `Firmware-Variants-Menuconfig.md`:** renomeia o product
  firmware `door_sensor` para `door_sensor_battery_h2` e acrescenta o requisito
  de composição `wake_led`. O alcance completo do renome está definido na
  seção 5. **Acréscimo da v0.11:** acrescenta o requisito de composição
  `dry_contact_wakeup`, oferecido pelo board somente quando sua entrada de
  contato seco está em um GPIO elegível para EXT1 no target. A capacidade
  física continua sendo do board e a política continua sendo do produto.
- **Acréscimo da v0.11 — altera (`Amends`) `ISSP-Configurable-Bootstrap.md`
  v1.5 novamente:** acrescenta `ContactWakeupConfig` ao contrato de
  `configureDeepSleep()`, conforme a seção 3. Não cria operação pública nova,
  estado público novo nem lifecycle adicional.
- **Altera (`Amends`) `ISSP-Reusable-Components.md` v1.1:** por decisão do
  Arquiteto, amplia materialmente e de forma limitada as APIs públicas de
  `issp_core`, `issp_behaviors` e `issp_transport_154` com as operações de
  quiescência definidas na seção 6. A ampliação serve somente ao encerramento
  seguro antes de deep sleep e não autoriza generalização adicional.
- **Acréscimo da v0.11 — preserva `ISSP-Reusable-Components.md` v1.1:** o
  wakeup por contato **não** amplia novamente a API reutilizável. A fachada
  determina o nível a rearmar com o GPIO que o produto já lhe entrega, sem
  consultar o estado lógico do `DigitalInputBehavior` e sem quarta operação
  compartilhada.
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
- **Preserva `Repository-Test-Execution-Policy.md` v0.4:** targets e execução
  de testes ou hardware permanecem governados por essa política. Esta
  especificação não autoriza coleta ou execução de testes, flash, monitor ou
  hardware.

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

// Acréscimo da v0.11. O produto entrega o GPIO que recebeu do board model;
// a fachada não descobre pinagem por conta própria.
struct ContactWakeupConfig
{
    bool enabled;
    gpio_num_t pin;
};

struct DeepSleepConfig
{
    bool enabled;
    std::uint32_t maxAwakeTimeMs;
    TimerWakeupConfig timerWakeup;
    WakeLedConfig wakeLed;
    ContactWakeupConfig contactWakeup;
};

}

iotsmartsys::AppResult
iotsmartsys::SmartSysApp::configureDeepSleep(
    const iotsmartsys::app::DeepSleepConfig &config);
```

Os nomes desta seção integram o contrato que orienta a implementação, inclusive
os promovidos na v0.11. `contactWakeup` é acrescentado ao final de
`DeepSleepConfig`, de modo que composições que não o declarem continuam válidas
e o campo permanece inerte.
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
- a conversão usa `std::uint64_t`; para `interval` de 32 bits em minutos ou
  horas, o limite material é o aceito por `esp_sleep_enable_timer_wakeup()` no
  ESP32-H2 com ESP-IDF 6.0.1, não overflow aritmético de 64 bits;
- `configureDeepSleep()` confronta o intervalo convertido com esse limite por
  helper privado, puro e sem configurar RTC, derivado das capacidades do target
  e da fonte de slow clock fixada no projeto. Retorna
  `AppResult::InvalidArgument` quando estiver fora da faixa. A costura admite
  limite injetado nos doubles sem criar API normativa;
- como o projeto usa o RC interno calibrado em runtime como slow clock, o
  limite usado em configuração deve ser conservador: todo intervalo aceito por
  `configureDeepSleep()` deve permanecer dentro da faixa aceita no preparo
  para qualquer variação suportada da calibração. O limite não pode ser um
  número de produto sem vínculo com as capacidades do target e a fonte de
  clock selecionada;
- o preparo repete a validação e confronta o retorno da API como defesa em
  profundidade. `ESP_ERR_INVALID_ARG` ou outro erro bloqueia a sequência antes
  de qualquer operação terminal;
- timer desabilitado não configura essa fonte e permite acordar somente por
  reset ou nova energização;
- antes de dormir sem fonte configurada, a fachada registra diagnóstico
  explícito.

### 4.2A Contato seco (acréscimo da v0.11)

- contato habilitado exige GPIO válido para wakeup externo conforme a
  capacidade declarada pelo target. A fachada usa
  `esp_sleep_is_valid_wakeup_gpio()`, não mantém lista própria de GPIOs e
  rejeita com `AppResult::InvalidArgument` todo GPIO que o target não aceite;
- o board oferece o GPIO como recurso de contato apto a wakeup. Eventual
  restrição elétrica ou de roteamento de um pino suportado pelo target pertence
  ao board, não à lista de validação da fachada;
- o GPIO do wakeup deve ser o mesmo de uma capability de contato seco registrada
  na composição. A configuração registrada dessa capability fornece direção e
  pull para o preparo do pad; fonte de contato independente, sem capability
  correspondente, fica fora desta versão;
- a correspondência é validada até `ValidateConfiguration`, sem impor ordem
  entre o registro da capability e `configureDeepSleep()` durante
  `AppState::Configuring`;
- se houver mais de uma capability de contato seco no GPIO solicitado, todas
  devem declarar o mesmo pull. Pulls divergentes tornam indeterminado o preparo
  elétrico e são rejeitados em `ValidateConfiguration` com
  `AppResult::InvalidArgument`; com pulls iguais, qualquer correspondência
  fornece a mesma configuração de entrada a reaplicar;
- **rearme alternado.** No início da sequência terminal, depois de preparado o
  timer e antes de qualquer operação terminal, a fachada reaplica ao pad o modo
  de entrada e o pull da configuração da capability correspondente, lê o nível
  elétrico do GPIO do contato e arma EXT1 para o **nível oposto** ao lido por
  `esp_sleep_enable_ext1_wakeup_io()`. O preparo é idempotente e ocorre mesmo se
  `StartDevice` não tiver sido alcançado ou se o behavior já tiver iniciado.
  Isso vale em todo boot, qualquer que tenha sido a causa do wakeup e qualquer
  que seja o estado do contato;
- a base do rearme é exclusivamente essa leitura elétrica; a fachada não usa o
  estado lógico confirmado pelo debounce e não consulta a capability para
  decidir o nível de EXT1. A polaridade lógica do contato não integra
  `ContactWakeupConfig`, pois não altera o rearme elétrico;
- se o contato transitar depois da armação e antes do sleep, EXT1 provoca
  wakeup imediato quando o chip dormir. A transição não é perdida, mas pode
  custar um ciclo acordado completo;
- as duas fontes são independentes e podem estar habilitadas juntas. Com ambas
  habilitadas, o dispositivo acorda pelo que ocorrer primeiro;
- esta versão não limita a taxa de wakeups por contato. Repique ou uso intenso
  podem produzir ciclos acordados completos sucessivos; o comportamento e seu
  consumo devem ser medidos antes de se criar política temporal adicional;
- o report de cada boot não exige mecanismo novo: com `reportOnStart=true` a
  capability de entrada já estabiliza e publica o estado inicial em `begin()`,
  em todo boot. Esta especificação não cria segundo caminho de publicação;
- o preparo confronta tanto o retorno da configuração do GPIO quanto o da API
  de wakeup externo. Erro em qualquer passo bloqueia a sequência antes de
  qualquer operação terminal, como a seção 6.1 já determina para fonte
  solicitada;
- contato desabilitado não configura essa fonte e não altera o GPIO.

No ESP32-H2, o ESP-IDF 6.0.1 mantém por HOLD durante o deep sleep a configuração
digital do pad, inclusive o pull reaplicado pela fachada a partir da
configuração da capability, e libera esse HOLD automaticamente no boot seguinte
antes de `app_main()`. Essa cadeia é
confirmada por leitura da implementação do target e não depende de opção de
Kconfig neste caminho. Permanece física a suficiência elétrica do pull diante
da fuga, da impedância e do ruído do contato e da fiação; pino instável produz
wakeup espúrio ou ausência de wakeup, e DEEPSLEEP-AC-012 mantém o experimento
obrigatório em hardware.

### 4.3 LED e ordem de inicialização

- LED habilitado exige GPIO válido para saída;
- `activeHigh=true` acende em `HIGH`; `activeHigh=false` acende em `LOW`;
- `DurationMs` exige `onTimeMs > 0` e apaga o LED ao final do período;
- `UntilSleep` ignora `onTimeMs` e apaga o LED imediatamente antes do sleep;
- LED desabilitado não configura nem toma o GPIO;
- na entrada de `setup()`, a fachada captura o início da janela acordada; em
  todo boot com configuração válida que alcance `InitializePlatform`, a
  primeira operação de plataforma é identificar a causa do boot e configurar
  e acender o LED, antes de NVS, commissioning, rádio ou reports;
- a inicialização evita pulso visível de polaridade oposta na medida suportada
  pelo ESP32-H2, e qualquer falha é registrada.

## 5. Responsabilidades, composição e renome

- o product firmware decide ativação, duração máxima acordado, intervalo,
  duração do indicador e uso do contato como fonte de wakeup;
- o board model fornece GPIO e polaridade elétrica do LED e do contato seco, e
  declara se sua entrada de contato é elegível para wakeup externo;
- `SmartSysApp` valida a configuração, identifica a causa do boot, controla o
  LED e coordena timer, deadline e entrada segura em deep sleep;
- componentes ISSP continuam responsáveis por device, reports e transporte e
  não conhecem produto, board, LED ou política de energia;
- produto que exige LED declara `wake_led`; o CMake rejeita board sem esse
  recurso conforme a ADR-0002;
- o GPIO do LED não pode colidir com capability, botão ou outro recurso;
- nenhum produto atual passa a usar deep sleep implicitamente;
- a primeira composição usa o product firmware `door_sensor_battery_h2` e o
  board `Door Sensor Battery H2`, que oferece `wake_led` no GPIO 13 e, pela
  v0.11, `dry_contact_wakeup` na entrada de contato seco do GPIO 14, dentro da
  faixa elegível do ESP32-H2. Nessa composição, o timer periódico é configurado
  para 15 minutos;
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
→ com configuração válida, setup captura a janela, registra a causa e acende LED
→ executa setup e trabalho do ciclo
→ após Running, inicia quiescência antecipada somente quando ao menos um report
  inicial esperado foi admitido e todos os reports iniciais esperados estão
  prontos, ou força-a ao atingir maxAwakeTimeMs
→ prepara a fonte de wakeup solicitada; falha interrompe o encerramento
→ fecha admissão e estabiliza producers, executor e transporte
→ registra o resultado, apaga o LED e inicia deep sleep
```

`SmartSysApp` é dona do deadline contado desde a entrada de `setup()`. Uma única
task privada de lifecycle de energia é dona exclusiva da sequência de deep
sleep; o token de transição continua disputado com factory reset. Outros
callbacks e tasks apenas sinalizam eventos e não executam passos de
quiescência ou sleep.

`InitializePlatform` é uma janela não preemptível. O deadline continua sendo
contado durante LED, inicialização ou recuperação da NVS, leitura de MAC e
construção de transport, network manager, device, executor e monitor, mas
nenhuma sequência de sleep começa nesse estágio. A task de lifecycle nasce
somente ao final de `InitializePlatform` bem-sucedido, quando todos os recursos
obrigatórios existem e o monitor opcional já foi tratado. Se o deadline já
tiver expirado, ela entra imediatamente no caminho forçado. Falha em
`ValidateConfiguration` ou `InitializePlatform` não cria a task e não tenta
deep sleep nesse boot.

`maxAwakeTimeMs` é um deadline absoluto contado desde `setup()` e não é
reiniciado depois de `InitializePlatform`. Se esse estágio retornar, o tempo
total acordado até a entrada em deep sleep é delimitado por:

```text
max(duração de InitializePlatform, maxAwakeTimeMs)
+ duração da sequência terminal
```

Logo, `maxAwakeTimeMs` não é sozinho um teto para o tempo físico acordado:
`InitializePlatform` não possui limite contratual neste recorte e a sequência
terminal ocorre depois do deadline. Esses termos devem ser considerados no
dimensionamento de bateria e medidos separadamente na evidência futura.

Depois de criada, a task observa o prazo durante commissioning, `NotReady`,
`Running` e mesmo enquanto a pilha chamadora não recuperou o controle.

Sleep antecipado exige evidência positiva de report neste boot. São reports
iniciais esperados aqueles cujos behaviors têm `reportOnStart=true`. Deve
existir pelo menos um, e todos precisam confirmar que sua publicação inicial
foi admitida. Para `DigitalInputBehavior`, `initial_stabilization_pending` não é
essa confirmação e mantém o device acordado. Se nenhum report inicial for
esperado ou a estabilização não concluir, somente o deadline permite dormir.

Enquanto o deadline não chega, a task de lifecycle usa polling com período
máximo de 10 ms para reavaliar dois predicados distintos, cada espera limitada
pelo tempo restante até o deadline:

- **prontidão para iniciar a quiescência antecipada:** deve existir ao menos um
  behavior com `reportOnStart=true`, o início de cada behavior esperado deve
  ter sido bem-sucedido e todos devem apresentar evidência positiva de que a
  publicação inicial foi admitida;
- **entrega depois de fechada a admissão e quiescidos os producers:**
  `pendingReportCount() == 0`.

Assim, confirmação do `DigitalInputBehavior` posterior a `Running` ainda pode
habilitar sleep antecipado; a avaliação não ocorre somente uma vez na
transição para `Running`. Contagem pendente igual a zero não satisfaz o
predicado de prontidão e ausência de report nunca equivale a admissão. Como o
device oferece um único handler de report, já pertencente ao executor, o
polling não instala um segundo canal de notificação nem amplia a API
reutilizável.

A fachada determina essa evidência com dados que já possui: configuração
`reportOnStart`, sucesso síncrono de `DigitalOutputBehavior::begin()` e
`DigitalInputBehavior::hasConfirmedState()`, que só confirma o estado inicial
depois de uma publicação requerida bem-sucedida. Não se adiciona quarta
operação à API reutilizável para esse controle.

O comentário do header deve separar `hasConfirmedState()`, agora consulta usada
em produção, das costuras `beginForTest()` e `sampleForTest()`, que permanecem
exclusivas de teste. Nenhuma assinatura muda por essa correção.

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
  novo report. Se `begin()` nunca tiver sido concluído com sucesso, a operação
  é no-op e retorna `IsspResult::Ok`;
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
  notificação de parada é distinguida por flag atômica. Se `start()` nunca
  tiver sido concluído com sucesso, a operação é no-op e retorna
  `IsspResult::Ok`;
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

- o detentor possui três estados distinguíveis: livre, deep sleep ou factory
  reset;

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

Quando uma especificação futura autorizar execução, o meio admissível para
provocar essa corrida é um build H2 temporariamente instrumentado, não
entregável, com barreira privada imediatamente antes de `nvs_commit()` e uma
varredura controlada de atrasos para disparar o deadline durante o commit. A
instrumentação deve ser removida do artefato entregável; o relatório registra
atrasos, repetições e cada resultado de boot. Uma varredura estatística sem a
barreira pode complementar, mas não substituir, a provocação controlada.

A ordem obrigatória é:

```text
adquirir ou confirmar a transição de deep sleep
  prosseguir se livre ou já detida pelo próprio deep sleep
  abortar somente se detida pelo factory reset
→ preparar as fontes de wakeup solicitadas, quando houver
  timer, quando habilitado
  contato, quando habilitado: reaplicar modo de entrada e pull da capability
  correspondente, ler o nível atual e armar EXT1 para o nível oposto
→ encerrar ResetButtonMonitor, quando configurado
→ beginQuiescence no device
→ quiesce em todos os behaviors registrados
→ no caminho antecipado, aguardar pendingReportCount() == 0 somente até o
  deadline absoluto
→ ao atingir o deadline, a mesma task e o mesmo detentor mudam para o caminho
  forçado e apenas registram a contagem pendente
→ stop no report executor
→ se stop concluir em até 600 ms, end no transporte
  senão registrar o timeout e preservar o transporte sem chamar end
→ apagar LED e iniciar deep sleep
```

Falha ao preparar **qualquer** fonte solicitada aborta a sequência antes de
qualquer operação terminal, libera a arbitragem e preserva o runtime acessível.
Para o contato, falha ao configurar direção ou pull ou ao armar EXT1 é falha de
preparo da fonte. A regra vale inclusive quando `StartDevice` não foi alcançado;
nesse caminho, a fachada não depende de `DigitalInputBehavior::begin()` para
deixar o pad em estado definido.
Com duas fontes habilitadas, o sucesso de uma não compensa a falha da outra: um
dispositivo que dormisse sem o contato armado deixaria de reportar a transição
até o próximo período do timer, e um que dormisse sem o timer perderia o sinal
de vida. Ausência intencional de fonte não aborta e é diagnosticada conforme a
seção 4.2.

No caminho antecipado, a fachada inicia essa sequência depois que `setup()`
atinge `Running`, existe ao menos um report inicial esperado e todos foram
admitidos. Fechar a admissão e parar os producers torna estável o oráculo
`pendingReportCount() == 0`. A espera usa somente o tempo restante de
`maxAwakeTimeMs`; não possui timeout próprio nem pode bloquear a observação do
deadline. No deadline, a mesma dona continua a sequência sem readquirir a
transição, não exige report admitido e não aguarda entrega das pendências.

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

Configuração inválida em `ValidateConfiguration` falha antes de tocar NVS,
rádio, RTC ou GPIO; nesse boot não há LED, task de lifecycle ou deep sleep. É
defeito de desenvolvimento da composição constante, não condição operacional
de campo. Falha de `InitializePlatform` produz o `SetupResult` vigente e também
não inicia lifecycle. Depois desse estágio, se o deadline conduzir diretamente
ao deep sleep, o resultado operacional permanece no log estruturado. Causa de
boot, configuração desabilitada, sleep antecipado, deadline, pendências, início
e bloqueio do sleep devem ser distinguíveis sem conteúdo sensível.
`ESP_SLEEP_WAKEUP_EXT1` é registrado como wakeup pelo contato, separado de
timer, cold boot e outras causas. O diagnóstico `no_wakeup_source` só se aplica
quando timer e contato estiverem ambos desabilitados.

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
  perda semântica; zero, unidade inválida e intervalo acima do limite do
  ESP-IDF/H2 retornam `InvalidArgument` em `configureDeepSleep()`. O preparo
  repete a validação e trata o retorno da API antes de operação terminal. Com o
  RC interno calibrado em runtime, o limite antecipado é conservador: todo
  intervalo aceito em configuração permanece na faixa do preparo em toda
  variação suportada da calibração, salvo falha da API ou do runtime.
- **DEEPSLEEP-AC-004 — LED:** polaridades HIGH e LOW, `DurationMs` e
  `UntilSleep` produzem nível e tempo configurados; em todo boot com
  configuração válida que alcance `InitializePlatform`, o LED acende como
  primeira operação de plataforma e apaga antes do sleep. Configuração inválida
  não toca GPIO.
- **DEEPSLEEP-AC-005 — Composição e renome:** política permanece no produto e
  recurso físico no board; `door_sensor_battery_h2` recebe `wake_led` GPIO 13
  do board, composição incompatível falha antes do binário e todo o alcance do
  renome da seção 5 é aplicado sem alterar registros históricos.
- **DEEPSLEEP-AC-006 — Entrega:** com producers encerrados,
  deve haver pelo menos um report inicial esperado e todos precisam ter sido
  admitidos; só então `pendingReportCount() == 0` permite sleep antecipado e
  prova entrega sem reserva ou transmissão. Ausência de report e
  `initial_stabilization_pending` são reavaliados até o deadline; slot ocupado
  por falha não retryable também aguarda. A espera de entrega usa apenas o tempo
  restante até o deadline. Prontidão e entrega são predicados separados,
  reavaliados por polling em no máximo 10 ms; contagem zero não comprova
  prontidão.
- **DEEPSLEEP-AC-007 — Deadline e quiescência:** o deadline é contado desde
  `setup()`, mas `InitializePlatform` não é preemptível. Ao final
  bem-sucedido do estágio, uma única task privada possui e executa a sequência,
  entra imediatamente no caminho forçado se o prazo expirou, converte o caminho
  antecipado em forçado no deadline e reconhece a transição já detida pelo
  próprio deep sleep. O polling é no máximo 10 ms. A quiescência usa somente as
  operações públicas autorizadas, não destrói objetos e não cria
  `SmartSysApp::stop()` ou retry público. `stop()` possui limite de 600 ms; no
  estouro, `end()` não é chamado. `quiesce()` e `stop()` são no-op com sucesso
  quando o objeto correspondente nunca foi iniciado neste boot, além de serem
  idempotentes depois de iniciado.
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
- **DEEPSLEEP-AC-009 — Wakeup:** timer wakeup, wakeup externo pelo contato,
  cold boot e outras causas são distinguíveis no log estruturado; o boot
  reaplica a configuração e sleep sem nenhuma fonte é permitido com
  diagnóstico. `ESP_SLEEP_WAKEUP_EXT1` identifica o contato, e ausência de
  fonte significa timer e contato simultaneamente desabilitados.
- **DEEPSLEEP-AC-011 — Contato (v0.11):** com o contato habilitado, todo boot
  que alcance a sequência terminal reaplica ao GPIO o modo de entrada e o pull
  da capability correspondente, lê seu nível e arma o wakeup externo para o
  nível oposto, qualquer que seja a causa do boot, o estado do contato ou o fato
  de `StartDevice` ter sido alcançado. GPIO fora da faixa elegível do target é
  rejeitado em `configureDeepSleep()` com `InvalidArgument`. Ausência de
  capability de contato seco correspondente é rejeitada em
  `ValidateConfiguration`, sem tornar significativa a ordem das chamadas em
  `AppState::Configuring`. Múltiplas capabilities correspondentes com pulls
  divergentes também são rejeitadas nesse estágio; pulls iguais são
  eletricamente equivalentes para o preparo. Com as duas fontes habilitadas,
  ambas são armadas antes de qualquer operação terminal e a falha de uma
  bloqueia o sleep. A capability de entrada com `reportOnStart=true` continua
  sendo o único caminho de publicação do estado a cada boot. A primeira
  composição usa timer de 15 minutos e não aplica rate limit aos wakeups por
  contato.
- **DEEPSLEEP-AC-012 — Retenção do nível durante o sleep (v0.11):** o contato
  não pode flutuar enquanto o dispositivo dorme. O critério só é satisfeito por
  evidência em hardware que demonstre, no par produto/board da seção 5, ausência
  de wakeup espúrio com o contato estável e wakeup efetivo na transição, em
  ambos os sentidos, tanto no caminho normal quanto em boot que alcance a
  sequência terminal sem executar `StartDevice`, incluindo `NotReady` em
  `InitializeNetwork`. A evidência também registra wakeups e consumo diante de
  repique ou acionamentos sucessivos, sem impor limite de taxa nesta versão.
  Leitura de código e build não satisfazem este critério.
- **DEEPSLEEP-AC-010 — Evidência futura:** `SetupHooks` pode ser estendido como
  costura interna para verificar com doubles lifecycle, validação e falhas, sem
  integrar o contrato normativo do produto. Evidência futura em hardware mede
  separadamente a duração de `InitializePlatform`, o deadline absoluto e a
  sequência terminal, e confronta o total observado com
  `max(duração de InitializePlatform, maxAwakeTimeMs) + duração da sequência
  terminal`. Hardware H2 confronta timer, LED, deadline e corrente tanto com
  `end()` quanto no timeout que o suprime, registrando também as opções vigentes
  de power-down e mitigação de leakage da flash, somente quando especificação
  futura autorizar execução.

Esta especificação não autoriza coleta ou execução de testes, flash, monitor ou
hardware.

## 8. Decisões do Arquiteto e entrega à análise

- **DEEPSLEEP-DEC-001:** há duração máxima acordado e sleep antecipado quando
  todos os reports do ciclo forem entregues.
- **DEEPSLEEP-DEC-002:** o LED habilitado acende em todo boot operacional com
  configuração válida que alcance `InitializePlatform`; a causa é registrada
  separadamente. Falha em `ValidateConfiguration` não toca GPIO.
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
- **DEEPSLEEP-DEC-011 (v0.11):** o dispositivo passa a acordar também por
  transição do contato seco, por EXT1, e rearma essa fonte para o nível oposto
  ao observado no início da sequência terminal, antes de qualquer operação
  terminal, em todo boot e independentemente do estado do contato e da causa
  do wakeup.
- **DEEPSLEEP-DEC-012 (v0.11):** timer e contato permanecem habilitados juntos
  no produto a bateria. O timer é o sinal de vida periódico; o contato é o
  evento. A falha de preparo de qualquer uma das duas bloqueia o sleep.
- **DEEPSLEEP-DEC-013 (v0.11):** o wakeup por contato não amplia a API
  reutilizável. A fachada usa o GPIO que o produto lhe entrega a partir do board;
  `ContactWakeupConfig` não contém polaridade lógica sem efeito no rearme.
- **DEEPSLEEP-DEC-014 (v0.11):** o nível oposto de EXT1 é calculado a partir do
  nível elétrico lido no início da sequência terminal, depois do preparo do
  timer e antes de qualquer operação terminal. O estado lógico confirmado pelo
  debounce não participa do rearme. Transição posterior pode causar wakeup
  imediato, sem perda do evento.
- **DEEPSLEEP-DEC-015 (v0.11):** não há rate limit de wakeups por contato nesta
  versão. Repique e acionamentos sucessivos serão medidos antes de eventual
  política adicional.
- **DEEPSLEEP-DEC-016 (v0.11):** o timer periódico da primeira composição é de
  15 minutos. A decisão não altera nem ratifica os valores vigentes de
  `maxAwakeTimeMs` e duração do LED, que estão fora do acréscimo da v0.11.
- **DEEPSLEEP-DEC-017 (v0.11):** a fachada deriva elegibilidade de wakeup da
  capacidade vigente do target, sem lista própria de GPIOs. O board declara o
  recurso físico, e o GPIO deve corresponder a uma capability de contato seco
  registrada. No preparo da fonte, a fachada reaplica sempre, de forma
  idempotente, o modo de entrada e o pull dessa configuração antes de ler o
  nível e armar EXT1; assim o pad fica definido mesmo quando `StartDevice` não
  foi alcançado, sem ampliar API reutilizável. Se mais de uma capability usar o
  GPIO, pulls divergentes são configuração inválida; pulls iguais são
  equivalentes para esse preparo.
- **DEEPSLEEP-DEC-010:** `InitializePlatform` não é preemptível. O deadline é
  contado durante o estágio, mas a task de lifecycle nasce somente após sucesso
  e inicia o caminho forçado imediatamente se o prazo já tiver expirado.

### Decisões da v0.11 confirmadas pelo Arquiteto

As antigas PEND-A, PEND-B e PEND-C foram resolvidas em
DEEPSLEEP-DEC-014 a DEEPSLEEP-DEC-016. Depois da análise de 12/08/2026, o
Arquiteto confirmou a derivação da elegibilidade pelo target, o vínculo com a
capability registrada, a armação no início da sequência terminal e a remoção da
polaridade sem efeito, consolidadas em DEEPSLEEP-DEC-011, DEC-013, DEC-014 e
DEC-017. Depois da nova análise, confirmou também a reaplicação idempotente da
configuração do pad pela fachada em todo caminho terminal, inclusive sem
`StartDevice`. Por fim, confirmou a rejeição de múltiplas capabilities no GPIO
do wakeup quando seus pulls divergirem. Não permanece decisão normativa aberta
para o acréscimo da v0.11. Os valores vigentes de `maxAwakeTimeMs` e duração do
LED não pertencem ao acréscimo e não foram ratificados por essas decisões.

### Origem da v0.11 e evidência observada

A v0.11 nasce de um fato de campo, não de análise: com a v0.10 implementada, o
Arquiteto compilou o firmware e o executou no ESP32-H2, e relatou que o
dispositivo **não acorda pela transição do contato no GPIO 14**. Esse
comportamento é o contratado pela v0.10, cuja seção 1 exclui outras fontes de
wakeup, e não um defeito da implementação. A intenção do Arquiteto passou a
exigir a segunda fonte, e é ela que esta versão especifica.

Da mesma execução decorre a primeira evidência material da v0.10 que não veio de
leitura: **o build e a execução em hardware ocorreram e foram relatados pelo
Arquiteto**. O relatório de implementação registrava build e hardware como não
executados por não haver autorização; esse registro é corrigido pela observação
do Arquiteto, sem que esta especificação passe a autorizar execução por conta
própria. A verificação de compilação e dos `static_assert` de dimensionamento
deixa de ser incógnita; o comportamento físico do resto do ciclo continua sem
evidência registrada.

### Histórico das versões anteriores

A v0.10 incorpora as precisões da análise de implementabilidade da v0.9, sem
introduzir nova decisão normativa: orçamento total acordado, margem
conservadora do timer com RC interno, separação dos predicados de prontidão e
entrega e segurança de `quiesce()` e `stop()` quando nunca iniciados. A análise
da v0.10 confirmou esses quatro pontos e a correção de DEEPSLEEP-AC-007, sem
encontrar bloqueador estrutural, decisão normativa ausente ou precisão de
redação pendente. Com base nessa evidência, o Arquiteto promoveu a especificação
para `Proposed`, pronta para implementação. Os comportamentos de NVS e as
corridas continuam exigindo experimento explícito; não são presumidos por
inspeção ou build.

Fontes de evidência existentes:

- `docs/reports/client-deep-sleep/analysis/2026-08-11-initial-analysis.md`;
- `docs/reports/client-deep-sleep/analysis/2026-08-11-verification-analysis.md`;
- `docs/reports/client-deep-sleep/analysis/2026-08-11-v04-implementability-analysis.md`;
- `docs/reports/client-deep-sleep/analysis/2026-08-11-v05-implementability-analysis.md`;
- `docs/reports/client-deep-sleep/analysis/2026-08-11-v06-implementability-analysis.md`;
- `docs/reports/client-deep-sleep/analysis/2026-08-11-v07-implementability-analysis.md`;
- `docs/reports/client-deep-sleep/analysis/2026-08-11-v08-implementability-analysis.md`;
- `docs/reports/client-deep-sleep/analysis/2026-08-11-v09-implementability-analysis.md`;
- `docs/reports/client-deep-sleep/analysis/2026-08-11-v10-implementability-analysis.md`;
- `docs/reports/client-deep-sleep/analysis/2026-08-12-v11-implementability-analysis.md`;
- `docs/reports/client-deep-sleep/analysis/2026-08-12-v11-implementability-analysis-r3.md`;
- `docs/reports/client-deep-sleep/analysis/2026-08-12-v11-implementability-analysis-r4.md`;
- `docs/reports/client-deep-sleep/analysis/2026-08-12-v11-implementability-analysis-r5.md`;
- `docs/reports/client-deep-sleep/implementation/2026-08-11-v10-implementation.md`.

A v0.10 permanece `Proposed` e implementada. As primeiras análises da v0.11
foram `Not Ready — Specification Defect`; a r4 classificou como `Ready` a
revisão do commit `5b2cc09` e devolveu somente a precisão não bloqueante sobre
pulls divergentes no mesmo GPIO. O confronto final r5 classificou a revisão
`ea95c77` como `Ready`, sem bloqueador, decisão normativa ausente ou contradição
interna. Com base nessa evidência, o Arquiteto promoveu a v0.11 para `Proposed`,
Pronta para implementação, preservando DEEPSLEEP-AC-012 como evidência
obrigatória de hardware que não pode ser satisfeita por leitura ou build.

Os gates da revisão corrente da v0.11 estão satisfeitos: análise `Ready`
presente para `ea95c77`, promoção para Pronta registrada e autorização de
implementação concedida pelo Arquiteto em 12/08/2026. A implementação normativa
da v0.11 pode começar.

Conforme o EKOM 3.6, a autorização de implementação inclui e exige o build
canônico proporcional dos entregáveis construíveis afetados. Coleta ou execução
de testes, flash, monitor e hardware permanecem dependentes de autorização
própria.
