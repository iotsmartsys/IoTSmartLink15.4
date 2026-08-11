# Análise de implementabilidade da v0.6 — deep sleep do client

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/Client-Deep-Sleep.md`

**Revisão confrontada:** v0.6, Draft de 11/08/2026

**Estado:** Concluído

**Capacidade:** Engenheiro Analista

**Data:** 11/08/2026

**Resultado:** Implementabilidade recomendada com condições; duas decisões
normativas ausentes, duas precisões de redação e uma observação de produto;
nenhuma execução realizada

> Este relatório registra evidências e recomendações. Não altera a fonte
> normativa, não promove estado e não autoriza implementação ou testes.

## 1. Recorte e método

A seção 8 da v0.6 determina confirmar a arbitragem entre factory reset,
persistência e deep sleep, a parada notificável do monitor e do executor e a
ordem completa da seção 6. Este relatório cobre os três pontos, verifica a
incorporação dos achados da v0.5 e reexamina os critérios afetados.

Nenhum build, teste, flash ou execução em hardware foi realizado. A árvore
estava limpa em `spec/client-deep-sleep`.

## 2. Incorporação dos achados da v0.5

As três recomendações da análise anterior estão incorporadas e correspondem ao
que foi recomendado:

- o `ResetButtonMonitor` e a janela de factory reset passaram a ter tratamento
  explícito na seção 6.1 e em DEEPSLEEP-DEC-007;
- a nota do executor foi corrigida de 1,15 s para 1,47 s (seção 6);
- a seção 6 passou a declarar que `stop()` encerra as tentativas **deste boot**,
  alinhando-se à responsabilidade descrita em `ISSP-Architecture.md`.

Nenhuma dessas alterações reabre questão já confirmada na v0.5: a relação com
`ISSP-Reusable-Components.md`, a suficiência das três operações, a atualização
direta do `sdkconfig` e a validação restrita de colisão permanecem válidas.

## 3. Arbitragem entre factory reset e deep sleep

### 3.1 A arbitragem cabe em código privado da fachada

`FactoryResetService` e `ResetButtonMonitor` vivem em
`components/issp_app_154/src/reset/` e são construídos apenas em
`smart_sys_app_hardware.cpp:137-152`. A regra "primeira transição aceita vence"
pode, portanto, ser implementada sem tocar nenhuma API compartilhada e sem
ampliar o escopo de DEEPSLEEP-DEC-006. Este é o ponto central da seção 6.1 e
está confirmado.

O ponto de interceptação natural é `FactoryResetService::requestFactoryReset()`
(`factory_reset_service.cpp:24-50`): a aquisição da transição precisa ocorrer
antes de `cleanup_()`, e não depois, porque a limpeza já é a operação
irreversível. A função retorna `void`, então rejeitar e diagnosticar cabe
internamente sem mudar assinatura pública.

Detalhe técnico a tratar: `requested_` é um `bool` simples
(`factory_reset_service.hpp:17`) escrito pela task do monitor e lido por ela
mesma. Como a transição passa a ser disputada por duas tasks, o token precisa de
test-and-set atômico ou de seção crítica. É **escolha normal de implementação**.

### 3.2 Achado: a liberação da transição não re-habilita o factory reset na prática

A seção 6.1 determina que, se a preparação da fonte de wakeup falhar, a fachada
libere a transição e "factory reset volta a poder ser aceito". Isso é verdadeiro
apenas para uma **nova** solicitação, e o monitor não gera nova solicitação: em
`reset_button_monitor.cpp:119-130`, `resetRequested_` é marcado uma vez ao
completar o hold e só volta a `false` quando o botão é solto
(`reset_button_monitor.cpp:110-118`). Um usuário que mantenha o botão
pressionado durante toda a janela nunca reemitirá o pedido.

Correção recomendada, de baixo custo: quando o pedido for rejeitado por
arbitragem perdida, o monitor deve re-armar `resetRequested_` para que o hold
continue valendo. É **precisão de redação normativa** — a seção 6.1 já declara a
intenção; falta dizer que a rejeição não consome o hold em curso.

### 3.3 Observação de produto: a janela acordada limita o factory reset

O produto atual usa `holdTimeMs = 10000` (`firmwares/door_sensor.cpp:18`).
Com deep sleep habilitado e `maxAwakeTimeMs` da ordem de segundos, o hold de
10 s raramente se completa antes do deadline, e a regra "primeira transição
aceita vence" será quase sempre vencida pelo deep sleep. A arbitragem continua
correta; o que fica em questão é a usabilidade do factory reset no produto a
bateria. Não é bloqueador nem defeito da especificação, mas é decisão de produto
que convém registrar antes da implementação: encurtar o hold no
`door_sensor_battery_h2`, exigir `maxAwakeTimeMs > holdTimeMs`, ou aceitar que o
factory reset dependa de reset físico prévio.

## 4. Exclusão mútua sobre NVS — decisão normativa ausente

A seção 6.1 exige que "escrita do descritor por `persistNetwork()`, limpeza do
vínculo e commit da entrada em deep sleep são mutuamente exclusivos" e que "uma
transição já aceita aguarda a operação NVS atômica em curso". Os dois lados são
desiguais:

- **limpeza do vínculo: implementável.** `clearPersistedNetwork()` é público
  (`issp154_network_manager.hpp:31`) e só é alcançado através de
  `clearNetworkConfiguration()` (`smart_sys_app_hardware.cpp:91-101`), função da
  própria fachada. Envolvê-la na exclusão é trivial;
- **escrita do descritor: não implementável no recorte autorizado.**
  `persistNetwork()` é **privado** (`issp154_network_manager.hpp:35`) e só é
  chamado de dentro de `initializeNetwork()`
  (`issp154_network_manager.cpp:187`), que a fachada aciona como uma única
  operação opaca (`smart_sys_app_hardware.cpp:157-161`). A fachada não tem
  nenhum meio de observar, delimitar ou aguardar essa janela.

As saídas possíveis são mutuamente exclusivas e todas pertencem ao Arquiteto:

1. **ampliar uma quarta operação** em `issp_transport_154` que exponha ou
   receba o guarda da janela atômica. Isso extrapola o que a seção 2 e
   DEEPSLEEP-DEC-006 autorizam hoje (apenas `quiesce()`, `beginQuiescence()` e
   `stop()`, com `end()` inalterado), então exige emenda explícita;
2. **proteger todo o hook `initializeNetwork()`**. É implementável sem tocar
   componente compartilhado, mas a janela protegida passa a ser a varredura
   inteira de commissioning, da ordem de 10,6 s, o que contradiz a própria
   seção 6 ("o deadline continua ativo durante commissioning") e o limite
   "somente por essa janela protegida";
3. **aceitar a preempção com diagnóstico**, apoiando-se em que o NVS do ESP-IDF
   é projetado para tolerar perda de energia: a consequência esperada da
   preempção é perder o descritor recém-descoberto, e o boot seguinte revarre.
   Isso substitui uma exclusão dura por um resultado delimitado e registrado.

Recomendo a alternativa 3, por ser a única que preserva simultaneamente o
recorte de DEEPSLEEP-DEC-006, o deadline durante commissioning e a arquitetura
vigente. Ela **não é certificável por leitura**: depende do comportamento real
do NVS sob interrupção de `nvs_commit()` pelo início do deep sleep, que é
exatamente o experimento que a seção 8 já exige. Enquanto a decisão não existir,
DEEPSLEEP-AC-008A não é implementável como escrito.

Registro que este é o único bloqueador estrutural encontrado na v0.6, e que ele
se manifesta apenas no caminho de deadline: no caminho antecipado, a sequência
começa depois de `Running`, quando `setup()` já retornou e nenhuma escrita do
descritor pode estar em curso.

## 5. Paradas notificáveis

### 5.1 `ResetButtonMonitor` — confirmado

A conversão é direta: `vTaskDelay(pollDelay)`
(`reset_button_monitor.cpp:132`) passa a `ulTaskNotifyTake(pdTRUE, pollDelay)`.
Nenhum outro produtor notifica essa task, então o sinal não é ambíguo, o período
configurado é preservado como timeout e nenhuma opção de Kconfig é envolvida,
como a seção 6.1 exige. Falta apenas um handshake de término: a task se
autoexclui em `reset_button_monitor.cpp:75` e a fachada precisa de um sinal
observável — flag atômica marcada antes de `vTaskDelete` — para que a espera
limitada seja verdadeira. É **escolha normal de implementação**.

### 5.2 `Issp154ReportExecutor::stop()` — confirmado, com duas precisões faltando

A conversão do retry recomendada na v0.5 continua válida:
`vTaskDelay(pdMS_TO_TICKS(1000))` (`issp154_report_executor.cpp:145`) passa a
`ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000))`. Duas consequências devem ser
registradas:

- a notificação de report pendente
  (`issp154_report_executor.cpp:117-127`) e a notificação de parada usariam o
  mesmo sinal, então `stop()` exige uma flag `stopRequested_` atômica consultada
  no laço; sem ela, a parada é indistinguível de um novo report;
- uma notificação de report pendente chegando durante o retry passará a encurtar
  o intervalo de 1000 ms. O efeito é benigno — já existe report pendente — mas
  altera a temporização observável e deve constar como caso de teste.

**Precisão faltando na seção 6:** a espera limitada pela tentativa de transporte
ativa não tem limite declarado nem comportamento definido no estouro. O risco é
concreto: `end()` executa `vEventGroupDelete(ackEventGroup_)`
(`issp154_transport.cpp:396-399`) sobre o event group que `sendConfirmed` usa
para esperar ACK. Se a espera estourar com o executor ainda dentro de
`sendConfirmed`, o passo seguinte da ordem obrigatória destrói um handle em uso.
O pior caso de uma chamada a `sendConfirmed` é de cerca de 465 ms — três
tentativas com backoff de 5 ms e 10 ms (`issp154_transport.cpp:596-604`), cada
uma com até 100 ms de TX e 50 ms de espera de ACK.

Recomendação: a seção 6 deve fixar que o limite cobre o pior caso de uma
tentativa de transporte e que, no estouro, a fachada registra o fato e **pula**
`end()`, seguindo direto para o sleep. Pular `end()` é seguro porque o deep
sleep reinicia o firmware e desliga o rádio; executar `end()` sob a corrida não
é. É correção barata e elimina o único ponto da ordem obrigatória que pode
produzir uso de handle destruído.

## 6. Ordem da seção 6.1

A ordem obrigatória está correta e continua exigida pelo código, pelas mesmas
razões confirmadas na v0.5: `end()` depois de `stop()`, preparo da fonte de
wakeup antes de qualquer operação terminal, e fechamento da admissão antes da
parada dos producers. Os dois passos novos encaixam sem conflito: a aquisição da
transição precede tudo, e a parada do monitor fica depois do preparo da fonte,
de modo que um aborto por falha de wakeup ainda deixa o factory reset acessível.

### 6.1 Achado: "os reports de boot já puderam ser admitidos" não é observável

O gatilho antecipado da seção 6 é atingir `Running` com os reports de boot já
admitidos. Para o `DigitalOutputBehavior` isso vale: `begin()` publica de forma
síncrona quando `reportOnStart` está ativo
(`digital_output_behavior.cpp:52-70`). Para o `DigitalInputBehavior` — que é
justamente o behavior da primeira composição — não vale sempre:
`beginTimerBacked()` amostra dentro de um orçamento síncrono de
`samplesPerWindow × consecutiveWindows` períodos
(`digital_input_behavior.cpp:136-157`) e pode terminar em
`initial_stabilization_pending` sem ter publicado nada
(`digital_input_behavior.cpp:158-168`); no `door_sensor` esse orçamento é de
100 ms (5 × 2 × 10 ms).

Nesse caminho, `setup()` retorna `Running` com `pendingReportCount() == 0`
verdadeiro por ausência de report, não por entrega. O oráculo é satisfeito de
imediato e o dispositivo dorme sem ter reportado o estado da porta — o resultado
oposto ao pretendido por DEEPSLEEP-DEC-001 e DEEPSLEEP-AC-006.

É **decisão normativa ausente**, não detalhe de implementação, porque o critério
de antecipação é contrato. Duas saídas razoáveis: condicionar a antecipação a
pelo menos um report do ciclo admitido e entregue, ou declarar explicitamente
que a ausência de report admitido também autoriza o sleep antecipado, com
diagnóstico. A primeira preserva a intenção de DEEPSLEEP-DEC-001.

### 6.2 Mecanismo do deadline — recomendação de implementação

A seção 6 exige mecanismo capaz de observar o prazo independentemente de uma
etapa bloqueante de `setup()`. As etapas bloqueantes confirmadas são a varredura
de commissioning (ordem de 10,6 s) e o orçamento síncrono de amostragem inicial
do `DigitalInputBehavior` (100 ms no produto atual).

Recomendo task dedicada em vez de callback de `esp_timer`: a sequência de
quiescência bloqueia por centenas de milissegundos e o mesmo `esp_timer` task
despacha o amostrador do `DigitalInputBehavior`
(`dispatch_method = ESP_TIMER_TASK`, `digital_input_behavior.cpp:79-86`).
Executar a quiescência nesse contexto acopla os dois e complica a parada do
timer que a própria sequência precisa executar. É **escolha normal de
implementação**, registrada por afetar diretamente DEEPSLEEP-AC-007.

### 6.3 Costura de testes exigida por DEEPSLEEP-AC-010

DEEPSLEEP-AC-010 exige que lógica de validação e falhas seja verificável com
doubles. Os objetos que a quiescência manipula — device, executor, transporte,
monitor — vivem exclusivamente atrás de `hardwareStorage_` e só são
interpretados por `smart_sys_app_hardware.cpp` (`smart_sys_app_impl.hpp:44-51`).
A única costura existente é `SetupHooks` (`SmartSysApp.h:143-152`), declarada no
header como não pertencente ao contrato normativo do produto.

Cobrir AC-007, AC-008 e AC-008A com doubles exige, portanto, estender
`SetupHooks` com as operações da sequência. Existe precedente direto e a
extensão não cria camada nova, mas toca um header público e deve ser decisão
consciente, não efeito colateral. Registro como escolha de implementação com
impacto declarado.

## 7. Composição e renome — reconfirmados

- GPIO 13 permanece livre: a `Door Sensor Battery H2` usa GPIO 14 e GPIO 9
  (`boards/door_sensor_battery_h2.cpp:12-21`);
- `board_model.hpp` recebe novo tipo e acessor seguindo o precedente de
  `DryContactInputResource` (`boards/board_model.hpp:29-42`);
- a validação de composição por `required_resources`/`offered_resources` já
  existe e rejeita o par incompatível antes do binário
  (`client_154/main/CMakeLists.txt:41-49`), então `wake_led` entra como item de
  lista, sem mecanismo novo;
- o alcance do renome da seção 5 confere com o repositório:
  `client_154/main/CMakeLists.txt:15-17`, `Kconfig.projbuild:16`,
  `firmwares/door_sensor.cpp`, `client_154/sdkconfig:949` (linha comentada, logo
  edição textual sem regeneração) e as referências em
  `Firmware-Variants-Menuconfig.md`, `SYSTEM-DOSSIER.md` e `KNOWLEDGE-MAP.md`.
  Os relatórios históricos que citam `door_sensor` ficam preservados, como a
  seção 5 determina.

## 8. Componentes impactados

| Área | Impacto |
|---|---|
| API pública `SmartSysApp` | `configureDeepSleep()`, validação, causa de boot, LED, deadline e sequência de quiescência |
| `SetupHooks` | extensão para tornar a quiescência verificável com doubles (6.3) |
| `issp_core` | `IDeviceBehavior::quiesce()` e `IsspDevice::beginQuiescence()` |
| `issp_behaviors` | override de `quiesce()` no `DigitalInputBehavior`, com proteção do handle de timer |
| `issp_transport_154` | `Issp154ReportExecutor::stop()`, retry notificável e flag de parada |
| `issp_app_154` | arbitragem de transição, parada notificável do monitor, supervisor de deadline e guarda de NVS |
| Product firmware | opt-in, política temporal, hold do factory reset e renome integral |
| Board model e CMake | recurso `wake_led`, GPIO, polaridade e composição |
| Kconfig e `sdkconfig` | símbolo, rótulo e artefato versionado |
| Testes | doubles de RTC, GPIO e sleep; concorrência de `quiesce()`, `stop()` e da arbitragem |

## 9. Restrições confirmadas

- ESP32-H2 é o único target físico do `client_154`; QEMU não é admitido;
- nenhuma execução é autorizada por esta especificação nem por esta atuação;
- Kconfig não governa lógica interna de componentes compartilhados, o que
  permanece satisfeito: monitor e executor param por notificação, sem opção de
  menu;
- a fachada e seus objetos permanecem estáticos e vivos até o reboot; nenhuma
  operação da seção 6 destrói objeto ou permite reinício no mesmo boot;
- `persistNetwork()` é privado e inalcançável pela fachada dentro do recorte
  autorizado (4).

## 10. Experimentos necessários

Além dos oito registrados na análise da v0.5, que permanecem válidos:

9. corrida de arbitragem: gatilho de deep sleep e conclusão do hold de factory
   reset dentro da mesma janela, nas duas ordens, confirmando que o perdedor não
   preempta o vencedor;
10. início de deep sleep durante `nvs_commit()` de `persistNetwork()`,
    confrontando integridade do descritor e o comportamento do boot seguinte —
    decisivo para a alternativa recomendada em 4;
11. estouro da espera limitada de `stop()` com `sendConfirmed` ativo,
    confrontando o comportamento de `end()` (5.2);
12. `Running` alcançado com `initial_stabilization_pending`, confrontando o
    gatilho antecipado (6.1);
13. rejeição de factory reset por arbitragem perdida com o botão ainda
    pressionado, confrontando o re-arme do hold (3.2).

Leitura de código não certifica nenhum desses fatos.

## 11. Recomendação

Recomenda-se **prontidão condicionada**. A v0.6 incorporou corretamente os
achados da v0.5, a arbitragem é implementável em código privado da fachada sem
ampliar API compartilhada, as duas paradas notificáveis são viáveis sem
dependência de Kconfig e a ordem da seção 6.1 continua exigida pelo código.

Antes da implementação, dois pontos exigem decisão do Arquiteto:

1. **exclusão sobre `persistNetwork()`** (4): a exclusão dura escrita na seção
   6.1 não é implementável sem emendar DEEPSLEEP-DEC-006 ou proteger toda a
   varredura de commissioning. Recomendo aceitar a preempção com diagnóstico,
   condicionada ao experimento 10;
2. **gatilho do sleep antecipado** (6.1): `Running` não garante report de boot
   admitido no `DigitalInputBehavior`, e o oráculo pode ser satisfeito por
   ausência em vez de entrega.

E dois pontos exigem precisão de redação, ambos de baixo custo:

3. **limite e estouro da espera de `stop()`** (5.2): fixar o limite e determinar
   que o estouro registra e pula `end()`;
4. **re-arme do hold do factory reset** (3.2): declarar que a rejeição por
   arbitragem perdida não consome o hold em curso.

Fica registrada ainda a observação de produto em 3.3, que não bloqueia a
especificação. Builds, testes e hardware permanecem `Not Executed`.
