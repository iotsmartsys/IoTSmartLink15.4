# Deep sleep configurável para dispositivos client a bateria

**Tipo:** Normativo

**Estado normativo:** Draft

**Estado da implementação:** Not Implemented

**Estado do workflow:** Rascunho e análise

**Versão:** 0.1

**Responsável arquitetural:** Marcelo Miranda

**Última atualização:** 11/08/2026

**Escopo:** `client_154` em ESP32-H2, fachada `SmartSysApp`, product firmware e
board model

---

## 1. Intenção confirmada

Dispositivos client alimentados por bateria devem poder optar por deep sleep
sem impor essa política aos produtos alimentados continuamente. O product
firmware deve ativar ou desativar o recurso por configuração explícita da
fachada:

```cpp
smartSysApp.configureDeepSleep({
    .enabled = true,
    // demais campos
});
```

A configuração deve ainda permitir:

- despertar periódico por timer, com intervalo expresso em minutos ou horas;
- acender um LED quando o dispositivo despertar;
- declarar o nível elétrico que acende o LED (`HIGH` ou `LOW`);
- manter o LED ligado por uma duração em milissegundos ou até o dispositivo
  entrar novamente em deep sleep.

## 2. Objetivos

- **DEEPSLEEP-001:** adicionar configuração opt-in de deep sleep à API pública
  `SmartSysApp`.
- **DEEPSLEEP-002:** manter o comportamento dos firmwares atuais quando o
  recurso estiver desabilitado ou não for configurado.
- **DEEPSLEEP-003:** permitir wakeup periódico por timer com unidade explícita
  em minutos ou horas.
- **DEEPSLEEP-004:** sinalizar cada wakeup de deep sleep por LED configurável.
- **DEEPSLEEP-005:** preservar a fronteira entre política de produto e recurso
  físico da board.
- **DEEPSLEEP-006:** impedir entrada em deep sleep enquanto GPIO, tasks, rádio
  ou reports estiverem em estado não definido pelo contrato de encerramento.
- **DEEPSLEEP-007:** tornar configuração, causa de wakeup, início do sleep e
  falhas observáveis sem registrar dados sensíveis.

## 3. Escopo

Inclui:

- contrato público aditivo em `SmartSysApp.h`;
- configuração antes de `setup()`;
- deep sleep do ESP32-H2 no alvo `client_154`;
- timer RTC como fonte opcional de wakeup;
- LED indicador de wakeup;
- validação de GPIO, duração, intervalo, unidade e combinações de campos;
- recurso físico de LED declarado pelo board model;
- política escolhida pelo product firmware;
- encerramento coordenado do runtime antes de iniciar deep sleep;
- critérios de aceite documentais, automatizáveis e físicos.

## 4. Fora de escopo

- light sleep ou sleep isolado do rádio IEEE 802.15.4;
- `coordinator_154` em ESP32-C6;
- wakeup por GPIO, touch, ULP, UART ou rádio;
- retenção arbitrária de estado da aplicação em RTC memory;
- alteração de payload, commissioning, descritor de rede, ACK ou retry;
- introdução de ESP32-C3, QEMU ou execução automática de testes;
- seleção do recurso por Kconfig;
- inventar LED em board que não declare esse recurso físico;
- política de bateria fraca, medição de carga ou carregamento.

## 5. Fronteiras de responsabilidade

### 5.1 Product firmware

O product firmware decide se o deep sleep está habilitado e fornece os valores
de política, incluindo intervalo de wakeup e duração do indicador. Ele combina
esses valores com o LED oferecido pelo board model e chama
`configureDeepSleep()` antes de `setup()`.

O product firmware não deve chamar diretamente `esp_deep_sleep_start()`,
configurar fontes RTC ou controlar o GPIO do indicador durante o ciclo de
sleep.

### 5.2 Board model

O board model declara o recurso físico do LED: GPIO e polaridade elétrica. Ele
não decide se deep sleep será usado, o intervalo de wakeup nem por quanto tempo
o indicador ficará aceso.

Boards sem LED podem continuar válidos para produtos que não exijam o
indicador. Uma composição cujo produto exija o indicador deve declarar o
recurso `wake_led` e ser rejeitada pelo CMake quando o board não o oferecer,
seguindo a ADR-0002.

### 5.3 `SmartSysApp`

`SmartSysApp` valida e conserva a configuração, identifica a causa do boot,
aciona o indicador, configura o timer RTC e coordena o encerramento seguro do
runtime antes do deep sleep. A fachada não deve duplicar protocolo, behavior ou
transporte.

### 5.4 Componentes ISSP

Os componentes ISSP continuam responsáveis por device, reports e transporte.
As menores extensões necessárias para observar quiescência e encerrar tasks ou
rádio devem permanecer em seus respectivos componentes, sem conhecer product
firmware, board model, LED ou política de deep sleep.

## 6. Contrato público proposto

Os nomes desta seção são propostos e permanecem sujeitos à decisão pendente da
seção 12. Ajustes sintáticos na análise de implementabilidade não podem mudar a
semântica silenciosamente.

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
```

A fachada recebe a configuração por:

```cpp
iotsmartsys::AppResult
iotsmartsys::SmartSysApp::configureDeepSleep(
    const iotsmartsys::app::DeepSleepConfig &config);
```

### 6.1 Semântica de ativação

- ausência de `configureDeepSleep()` equivale a deep sleep desabilitado;
- `enabled=false` desabilita todo o recurso e preserva o ciclo vigente;
- somente uma chamada a `configureDeepSleep()` pode ser aceita;
- a chamada é permitida somente em `AppState::Configuring`;
- configuração duplicada, tardia ou inválida retorna resultado explícito e é
  preservada por `lastConfigurationResult()`;
- nenhum GPIO, timer RTC ou recurso assíncrono é iniciado pela chamada de
  configuração;
- desabilitar deep sleep não deve exigir que o board forneça LED.

### 6.2 Wakeup por timer

- `timerWakeup.enabled=true` exige `interval > 0`;
- `unit` aceita exclusivamente `Minutes` ou `Hours`;
- a implementação converte o intervalo para microssegundos usando aritmética
  checada antes de chamar a API ESP-IDF;
- overflow ou valor não representável é `AppResult::InvalidArgument`;
- o intervalo representa o tempo aproximado entre o início do deep sleep e o
  wakeup; tempo acordado não integra esse intervalo;
- `timerWakeup.enabled=false` não configura o timer RTC;
- habilitar deep sleep sem fonte de wakeup é permitido somente após decisão
  explícita do Arquiteto na pendência `DEEPSLEEP-PEND-003`.

### 6.3 LED de wakeup

- `wakeLed.enabled=true` exige GPIO válido para saída;
- `activeHigh=true` significa LED aceso em `HIGH` e apagado em `LOW`;
- `activeHigh=false` significa LED aceso em `LOW` e apagado em `HIGH`;
- `DurationMs` exige `onTimeMs > 0`, acende o LED ao reconhecer o wakeup e o
  apaga quando a duração terminar;
- `UntilSleep` ignora `onTimeMs`, mantém o LED aceso durante o período acordado
  e o coloca no nível inativo imediatamente antes do deep sleep;
- `wakeLed.enabled=false` não configura nem toma ownership de GPIO;
- a configuração elétrica do GPIO deve evitar pulso visível no nível oposto
  durante sua inicialização, na medida suportada pelo ESP32-H2;
- falha ao configurar ou acionar um LED habilitado é observável e não pode ser
  registrada como wakeup sinalizado com sucesso.

## 7. Semântica do ciclo de wakeup

Deep sleep reinicia a execução do firmware. A configuração declarada pelo
product firmware é reaplicada em cada boot; ela não precisa ser persistida em
NVS pela fachada.

O ciclo pretendido é:

```text
wakeup/reset
→ identificar causa
→ configurar LED no nível ativo, quando aplicável
→ executar setup vigente
→ operar segundo a política de permanência acordada
→ impedir novo trabalho e estabilizar producers
→ tratar reports conforme política terminal aprovada
→ encerrar executor, device e transporte
→ apagar LED quando UntilSleep
→ configurar fontes de wakeup
→ iniciar deep sleep
```

O timer do modo `DurationMs` começa quando o LED atinge o nível ativo. Se o
dispositivo voltar a dormir antes do término, o LED deve ser colocado no nível
inativo antes do sleep. O indicador nunca pode permanecer deliberadamente
ativo durante deep sleep.

## 8. Preservação e compatibilidade

- `Single smart plug` permanece sem deep sleep por padrão.
- `Door sensor` não passa a usar deep sleep apenas por o board ser descrito
  como alimentado por bateria; a ativação exige decisão no product firmware.
- configurações e consumers atuais continuam compilando sem fornecer novos
  campos.
- o recurso não altera a semântica de `setup()` para aplicações sem deep
  sleep.
- o sleep do rádio já existente não satisfaz esta especificação.
- factory reset e o LED não podem usar o mesmo GPIO na composição selecionada.
- o GPIO do LED também não pode colidir com uma capability ou outro recurso
  tomado pelo firmware.

## 9. Falhas e condições de borda

- configuração inválida falha em `ValidateConfiguration` antes de tocar NVS,
  rádio, RTC ou GPIO;
- overflow na conversão do intervalo é rejeitado;
- unidade fora do enum válido é rejeitada defensivamente;
- falha ao configurar a fonte de wakeup impede iniciar deep sleep e produz
  resultado observável;
- `esp_deep_sleep_start()` não retorna em caso de sucesso; se retornar, o fato
  é tratado como falha;
- a entrada em sleep não pode ocorrer enquanto um report estiver reservado ou
  uma transmissão estiver em andamento;
- o contrato deve definir resultado terminal para report não entregue, para
  que retry indefinido não mantenha dispositivo a bateria acordado para sempre;
- `NotReady` durante commissioning não pode causar loop infinito de reboot ou
  sleep sem política explicitamente aprovada;
- despertar por causa diferente da configurada deve permanecer observável e
  não pode ser rotulado como timer wakeup;
- tempo em milissegundos do LED e intervalo em minutos/horas são domínios
  distintos e não podem ser inferidos um do outro.

## 10. Observabilidade

Devem existir eventos estruturados equivalentes a:

```text
deep_sleep configuration=enabled timer_wakeup=enabled interval=1 unit=hours
wakeup cause=timer led=enabled
wake_led state=on mode=duration_ms duration_ms=500
deep_sleep prepare result=ok
deep_sleep blocked stage=drain_reports result=timeout pending_reports=1
deep_sleep start timer_interval_us=3600000000
```

Os logs devem distinguir configuração desabilitada, cold boot, timer wakeup,
outra causa, início solicitado, bloqueio e falha. Não devem registrar segredo,
credencial ou conteúdo integral de payload.

## 11. Critérios de aceitação

### Configuração e fronteiras

- **DEEPSLEEP-AC-001:** consumer atual sem `configureDeepSleep()` preserva API,
  setup e runtime vigentes.
- **DEEPSLEEP-AC-002:** `enabled=false` não configura GPIO, fonte RTC nem
  executa deep sleep.
- **DEEPSLEEP-AC-003:** configuração válida é copiada antes de `setup()`;
  chamada duplicada, tardia ou inválida é rejeitada e observável.
- **DEEPSLEEP-AC-004:** product firmware contém política; board model contém
  apenas pinagem e polaridade do LED; componentes compartilhados não contêm
  `CONFIG_*` nem nomes de produto ou board.
- **DEEPSLEEP-AC-005:** composição que exige `wake_led` e escolhe board sem o
  recurso falha antes de gerar binário e nomeia produto, board e recurso.
- **DEEPSLEEP-AC-006:** colisão do GPIO do LED com capability, botão ou outro
  recurso da composição é rejeitada antes de operação normal.

### Timer e LED

- **DEEPSLEEP-AC-007:** intervalos de 1 minuto, 2 minutos, 1 hora e 2 horas são
  convertidos respectivamente sem perda semântica para o timer RTC.
- **DEEPSLEEP-AC-008:** intervalo zero, unidade inválida e overflow são
  rejeitados antes de configurar o timer.
- **DEEPSLEEP-AC-009:** LED active high usa `HIGH` para acender e `LOW` para
  apagar; LED active low faz o inverso.
- **DEEPSLEEP-AC-010:** `DurationMs` mantém o LED ligado pelo período
  configurado dentro da tolerância aprovada para hardware e depois o apaga.
- **DEEPSLEEP-AC-011:** `UntilSleep` mantém o LED ligado durante o período
  acordado e o apaga antes do deep sleep.
- **DEEPSLEEP-AC-012:** GPIO do LED não é inicializado quando o indicador está
  desabilitado.

### Ciclo de vida e falhas

- **DEEPSLEEP-AC-013:** wakeup por timer é identificado como timer; cold boot e
  outra causa permanecem distinguíveis.
- **DEEPSLEEP-AC-014:** antes do deep sleep, producers estão estabilizados,
  nenhum report está reservado ou em transmissão, tasks afetadas chegaram a
  estado terminal e o transporte encerrou.
- **DEEPSLEEP-AC-015:** falha em qualquer etapa de preparação impede o início
  do deep sleep e expõe etapa e resultado.
- **DEEPSLEEP-AC-016:** retry de report e commissioning possuem condição
  terminal compatível com o orçamento de bateria aprovado.
- **DEEPSLEEP-AC-017:** início bem-sucedido de deep sleep não retorna; o wakeup
  subsequente reinicia o firmware e reaplica a configuração.
- **DEEPSLEEP-AC-018:** produtos sem opt-in continuam executando indefinidamente
  como antes.

### Evidência

- **DEEPSLEEP-AC-019:** testes de lógica pura cobrem validação, conversão
  checada, polaridade, modos do LED, causas de wakeup e falhas injetadas sem
  alegar comportamento físico.
- **DEEPSLEEP-AC-020:** build do `client_154` em ESP32-H2 cobre ao menos uma
  composição habilitada e uma desabilitada, quando uma especificação futura
  autorizar a execução.
- **DEEPSLEEP-AC-021:** validação física em ESP32-H2 mede wakeups de 1 e 2
  minutos e confronta ao menos um intervalo em horas ou uma estratégia
  equivalente aprovada pelo Arquiteto.
- **DEEPSLEEP-AC-022:** hardware comprova polaridades HIGH e LOW em boards ou
  circuito controlado, `DurationMs`, `UntilSleep`, corrente em deep sleep e
  ausência de LED deliberadamente ativo durante o sleep.
- **DEEPSLEEP-AC-023:** logs e instrumentação distinguem sucesso, falha e
  ausência de evidência para cada cenário executado.
- **DEEPSLEEP-AC-024:** validação documental, `git diff --check` e guarda EKOM
  terminam com sucesso.

Os testes acima são critérios para uma futura especificação de execução. Esta
autoria não autoriza coletar, gravar ou executar suítes ou hardware, conforme
`Repository-Test-Execution-Policy.md`.

## 12. Decisões pendentes do Arquiteto

### DEEPSLEEP-PEND-001 — Gatilho para voltar a dormir (bloqueante)

A intenção ainda não define quando um produto habilitado deve entrar em deep
sleep. As alternativas materialmente diferentes são:

1. **explícita:** o product firmware chama uma operação pública após terminar
   seu trabalho;
2. **tempo acordado:** `DeepSleepConfig` recebe uma duração em milissegundos e
   a fachada inicia o encerramento ao expirar;
3. **reports concluídos:** a fachada inicia o encerramento depois que os
   reports do ciclo forem entregues ou atingirem uma política terminal.

**Recomendação da autoria:** combinar uma janela máxima acordada, configurada
em milissegundos, com encerramento antecipado opcional após conclusão dos
reports. Isso limita consumo mesmo com coordenador ausente, mas exige definir o
destino de reports não entregues.

### DEEPSLEEP-PEND-002 — Boots que acendem o LED (bloqueante)

Confirmar se “sempre que despertar” significa somente retorno de deep sleep ou
também cold boot, reset manual, watchdog e outros resets.

**Recomendação da autoria:** acender em todo boot quando o indicador estiver
habilitado e registrar separadamente a causa. Essa leitura torna o indicador
previsível para o usuário sem falsificar a telemetria.

### DEEPSLEEP-PEND-003 — Deep sleep sem fonte de wakeup

Confirmar se `enabled=true` com `timerWakeup.enabled=false` é uma configuração
válida para acordar apenas por reset/alimentação ou se deve ser rejeitada.

**Recomendação da autoria:** permitir, mas emitir log explícito antes do sleep.

### DEEPSLEEP-PEND-004 — Report não entregue e commissioning

Definir o resultado terminal e o orçamento de tempo para reports sem ACK e
para rede `NotReady`. A política atual de retry indefinido é incompatível com
consumo previsível de bateria.

**Recomendação da autoria:** respeitar uma janela máxima acordada; ao expirar,
registrar contagem pendente e dormir sem descartar silenciosamente o fato.
Persistir reports entre boots não está autorizado por este rascunho.

## 13. Impactos esperados

- `components/issp_app_154/include/SmartSysApp.h`;
- `components/issp_app_154/src/smart_sys_app.cpp`;
- `components/issp_app_154/src/smart_sys_app_hardware.cpp`;
- lifecycle do report executor, device e transporte;
- hooks e test app de `SmartSysApp`;
- `client_154/main/boards/board_model.hpp` e boards que ofereçam LED;
- `client_154/main/CMakeLists.txt` para o recurso `wake_led`;
- product firmware opt-in escolhido pelo Arquiteto;
- documentação de componentes, dossiê, mapa e relatórios correspondentes.

O coordenador e o protocolo wire não precisam mudar para configurar timer e
LED. A política terminal de reports pode revelar mudança adicional, mas ela
não deve ser inferida antes da decisão da seção 12.

## 14. Prontidão

A estrutura é tecnicamente plausível no ESP32-H2, mas a especificação permanece
em `Draft` e não é recomendada como `Implementable` enquanto as pendências
`DEEPSLEEP-PEND-001`, `DEEPSLEEP-PEND-002` e `DEEPSLEEP-PEND-004` não forem
decididas pelo Arquiteto. Somente o Arquiteto pode promover o estado e autorizar
implementação.
