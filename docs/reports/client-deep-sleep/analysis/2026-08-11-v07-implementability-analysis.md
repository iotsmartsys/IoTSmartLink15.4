# Análise de implementabilidade da v0.7 — deep sleep do client

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/Client-Deep-Sleep.md`

**Revisão confrontada:** v0.7, Draft de 11/08/2026

**Estado:** Concluído

**Capacidade:** Engenheiro Analista

**Data:** 11/08/2026

**Resultado:** Implementabilidade recomendada com condições; os quatro pontos
pedidos pela seção 8 estão confirmados no código; três precisões de redação,
uma verificação de plataforma e uma lacuna de método experimental; nenhum
bloqueador estrutural remanescente; nenhuma execução realizada

> Este relatório registra evidências e recomendações. Não altera a fonte
> normativa, não promove estado e não autoriza implementação ou testes.

## 1. Recorte e método

A seção 8 da v0.7 determina confirmar quatro incorporações da análise da v0.6:
a preempção diagnosticada de `persistNetwork()`, a evidência positiva de report
inicial, o timeout de `stop()` com supressão de `end()` e o re-arme do hold do
factory reset. Este relatório cobre os quatro, reexamina os critérios que a v0.7
reescreveu e registra os achados novos introduzidos pelo próprio texto novo.

Nenhum build, teste, flash ou execução em hardware foi realizado. A árvore
estava limpa em `spec/client-deep-sleep`.

## 2. Preempção diagnosticada de `persistNetwork()` — confirmada

A decisão da v0.7 (DEEPSLEEP-DEC-008 e seção 6.1) substitui a exclusão dura da
v0.6 por preempção aceita com diagnóstico. Ela é implementável no recorte
autorizado:

- `persistNetwork()` continua privado (`issp154_network_manager.hpp:35`) e é
  chamado uma única vez, dentro de `initializeNetwork()`
  (`issp154_network_manager.cpp:187`). A fachada não observa essa janela, e a
  v0.7 não pretende observá-la;
- o diagnóstico que a v0.7 exige depende apenas de dado que a fachada já possui:
  o estágio corrente da máquina de estados. `SetupStage::InitializeNetwork` é
  ativado em `smart_sys_app.cpp:311-313` e permanece até o retorno do hook. Ligar
  `persistence_preemption_possible=true` a esse estágio não exige API nova,
  campo novo em `SetupStage` nem acesso a `issp_transport_154`;
- o conjunto de desfechos admitidos pela seção 6.1 corresponde ao que a
  sequência real pode produzir. A escrita é `nvs_set_blob` seguido de
  `nvs_erase_key` e um único `nvs_commit`
  (`issp154_network_manager.cpp:104-109`): interrupção antes do commit perde a
  descoberta inteira e deixa o descritor anterior ou a ausência; interrupção
  durante o commit cai no esquema de consistência do próprio NVS. Conteúdo
  parcial do descritor não é um desfecho que o código possa produzir por si.

Fica registrado que o diagnóstico é deliberadamente grosseiro: a janela
sinalizada é toda a etapa `InitializeNetwork`, da ordem de 10,6 s na varredura
completa, enquanto a janela real do commit é de milissegundos. Isso é coerente
com o texto da seção 6.1, que proíbe alegar observação da janela privada, mas
significa que o diagnóstico será emitido em praticamente todo deadline durante
commissioning e quase nunca corresponderá a uma preempção efetiva.

### 2.1 Lacuna: DEEPSLEEP-AC-008B não declara meio admissível de experimento

AC-008B condiciona a aceitação a um experimento que produza descritor anterior
válido, novo válido ou ausência. Pela mesma razão do parágrafo anterior, a
sobreposição entre o início do deep sleep e o `nvs_commit()` não é provocável
por configuração: seria preciso acertar uma janela de milissegundos dentro de
uma varredura de segundos, escolhendo `maxAwakeTimeMs`.

Nenhum meio admissível para forçar a corrida está declarado. As saídas que vejo,
todas do Arquiteto:

1. build instrumentado temporário, não entregável, que atrase dentro de
   `persistNetwork()` para tornar a janela alcançável. Não amplia API pública e
   não conflita com DEEPSLEEP-DEC-008, mas precisa ser autorizado como meio;
2. varredura estatística de `maxAwakeTimeMs` em torno do instante medido de
   persistência, com número de repetições declarado;
3. aceitar evidência indireta — integridade do descritor após N ciclos de deep
   sleep durante commissioning — reconhecendo que não cobre a janela do commit.

Recomendo a alternativa 1, com a alternativa 2 como confirmação. Isso não é
decisão normativa do contrato; é definição do meio de validação que AC-008B já
exige e que a especificação futura de execução precisará nomear.

## 3. Evidência positiva de report inicial — confirmada

Esta é a incorporação mais substantiva da v0.7 e ela se sustenta integralmente
sobre dados que a fachada já possui. Confirmei os três elos:

- **`reportOnStart` é dado da configuração já copiada.** `Impl` guarda
  `switchConfigs_` e `doorSensorConfigs_` (`smart_sys_app_impl.hpp:81,88`), logo
  o conjunto de reports iniciais esperados é calculável sem consultar behavior
  algum;
- **`DigitalOutputBehavior`: sucesso síncrono de `begin()` prova admissão.**
  `begin()` publica dentro da própria chamada quando `reportOnStart` está ativo
  e devolve o resultado da publicação em caso de falha
  (`digital_output_behavior.cpp:52-71`). `IsspDevice::start()` aborta no primeiro
  behavior que não retorne `Ok` (`issp_device.cpp:49-62`), e esse resultado
  chega à fachada pelo hook `startDevice`, cujo insucesso impede `Running`
  (`smart_sys_app.cpp:342-349`). Portanto `Running` já implica que todo report
  inicial de saída esperado foi admitido, sem consulta adicional;
- **`DigitalInputBehavior::hasConfirmedState()` prova admissão quando
  `reportOnStart=true`.** `publishConfirmedState()` só grava `confirmedState_`
  depois de `publishState()` retornar `Ok`; a falha retorna antes da gravação
  (`digital_input_behavior.cpp:326-370`). Com `reportOnStart=true` a publicação
  é obrigatória no caminho inicial, então `hasConfirmedState()` verdadeiro
  equivale a publicação inicial admitida. `initial_stabilization_pending`
  (`digital_input_behavior.cpp:159-167`) deixa `confirmedState_` em
  `kStateUnknown` e mantém o predicado falso, exatamente como a seção 6 afirma.

Confirmo também que nenhuma quarta operação da API reutilizável é necessária:
`Impl` guarda os behaviors como tipos concretos
(`std::optional<issp::DigitalInputBehavior>` e
`std::optional<issp::DigitalOutputBehavior>`,
`smart_sys_app_impl.hpp:82-90`), não apenas como `IDeviceBehavior *`, e
`smart_sys_app.cpp` já consome `DigitalInputBehavior::state()` por esse caminho
(`smart_sys_app.cpp:32`). DEEPSLEEP-DEC-009 é implementável sem tocar
`ISSP-Reusable-Components.md` além do que DEEPSLEEP-DEC-006 já autoriza.

O oráculo de entrega também confere: `pendingReportCount_` só é decrementado em
`completePendingReport()` quando a entrega é confirmada e a geração do slot não
mudou (`issp_device.cpp:255-264`), e o slot permanece contado enquanto está em
voo. A afirmação da seção 6 de que a contagem inclui slots reservados e
transmissões em andamento está correta.

### 3.1 Precisão: a confirmação pode chegar depois de `Running`

No produto atual, o orçamento síncrono do `DigitalInputBehavior` é de dez
amostras de 10 ms e exige duas janelas com a mesma classificação
(`digital_input_behavior.cpp:136-157` com os valores de
`firmwares/door_sensor.cpp:14-17`). Convergir dentro do orçamento é possível,
mas não garantido. Quando não converge, o classificador periódico continua e
pode confirmar o estado inicial cerca de 100 ms **depois** de `setup()` ter
retornado `Running`.

A seção 6 diz "após Running, inicia quiescência antecipada somente quando ...
todos os reports iniciais esperados estão prontos", o que é compatível com
avaliação contínua, mas a frase seguinte — "se ... a estabilização não concluir,
somente o deadline permite dormir" — pode ser lida como avaliação única no
instante de `Running`. As duas leituras produzem produtos diferentes: uma dorme
antecipadamente 100 ms depois, a outra fica acordada até o deadline. Recomendo
declarar que o gatilho é reavaliado até o deadline.

### 3.2 Precisão de cabeçalho, sem efeito de API

`hasConfirmedState()` já é público (`digital_input_behavior.hpp:61`), mas está
posicionado logo abaixo do comentário que apresenta `beginForTest()` e
`sampleForTest()` como costura exclusiva de teste, e hoje só é usado pelos
testes. Passar a consumi-lo em produção não altera assinatura nem amplia a API —
mantendo verdadeira a afirmação da seção 6 —, porém o comentário do cabeçalho
deve ser ajustado para não descrever como test-only algo de que o contrato passa
a depender. É correção de comentário, registrada para não ser feita por
descuido.

## 4. Timeout de `stop()` com supressão de `end()` — confirmado

O número da seção 6 confere. O pior caso de uma chamada a `sendConfirmed()` é de
três tentativas (`issp154_transport.cpp:596`), cada uma com até 100 ms de
transmissão física (`kPhysicalTxTimeoutMs`, `issp154_transport.cpp:20,688`) e
50 ms de espera de ACK (`kReportAckTimeoutMs`,
`issp154_report_executor.cpp:11,84`), mais backoffs de 5 ms e 10 ms
(`kConfirmedSendBackoffMs`, `issp154_transport.cpp:28`): 3 × 150 + 15 = 465 ms.
Os 600 ms deixam cerca de 135 ms de margem de escalonamento.

A proibição de chamar `end()` no estouro está corretamente motivada e é ainda
mais necessária do que o texto afirma: `end()` executa
`issp154_transport_deinit()` **antes** de destruir o event group
(`issp154_transport.cpp:390-399`), de modo que uma tentativa ativa sofreria
duas violações, não uma — desinicialização do rádio sob transmissão e
`vEventGroupDelete()` sobre o handle que `waitAckAttemptOutcome()` está usando
(`issp154_transport.cpp:239-290`). Pular `end()` elimina as duas.

Três obrigações de implementação decorrem disso e ficam registradas por
afetarem DEEPSLEEP-AC-007:

- o limite de 600 ms só cobre o pior caso se `stopRequested_` for consultado
  **antes de iniciar nova tentativa**, no topo do laço interno
  (`issp154_report_executor.cpp:133-153`). Consultado apenas no caminho de
  retry, o pior caso passa a ser múltiplas tentativas encadeadas enquanto
  houver slots ocupados;
- `stop()` precisa desbloquear as **duas** esperas da task: o
  `ulTaskNotifyTake(pdTRUE, portMAX_DELAY)` externo
  (`issp154_report_executor.cpp:132`) e o retry de 1000 ms convertido
  (`issp154_report_executor.cpp:145`). Ambas são a mesma notificação, o que
  torna a flag atômica exigida pela seção 6 indispensável para distinguir
  parada de report novo;
- a espera limitada exige sinal observável de término da task. Vale a mesma
  observação já feita para o `ResetButtonMonitor`: a task se autoexclui e a
  fachada precisa de uma flag marcada antes de `vTaskDelete`.

### 4.1 Verificação adicional exigida pela supressão de `end()`

Pular `end()` significa entrar em deep sleep com o rádio 802.15.4 ainda
inicializado. O reboot posterior resolve o estado lógico, como a seção 6 afirma,
mas a corrente de sono nesse caminho não é a mesma medição do caminho normal.
DEEPSLEEP-AC-010 já pede confronto de corrente em hardware; recomendo que ele
distinga explicitamente as duas variantes, com `end()` e sem `end()`. Leitura de
código não certifica consumo.

## 5. Re-arme do hold do factory reset — confirmado, e mais barato que o previsto

A seção 6.1 da v0.7 declara que o pedido rejeitado não consome o hold em curso.
O mecanismo é local ao monitor e não toca nenhuma API compartilhada:

- `resetRequested` é **variável local** de `ResetButtonMonitor::run()`
  (`reset_button_monitor.cpp:83`), não um membro — corrijo aqui a referência da
  análise da v0.6, que a citou como `resetRequested_`. O re-arme é uma
  atribuição dentro do próprio laço;
- `pressed` e `pressedAtUs` não são alterados pela rejeição
  (`reset_button_monitor.cpp:103-130`), então `elapsedMs` continua maior ou
  igual a `holdTimeMs` e o pedido reaparece no poll seguinte, sem exigir soltura
  e nova pressão. É exatamente o comportamento que a seção 6.1 pede;
- para re-armar, o monitor precisa conhecer o desfecho, e
  `IFactoryResetRequester::requestFactoryReset()` retorna `void`
  (`ifactory_reset_requester.hpp:7`). A interface vive em
  `components/issp_app_154/src/reset/`, diretório privado do componente, fora de
  qualquer `include/`. Mudar sua assinatura é alteração interna do
  `issp_app_154` e **não** amplia `ISSP-Reusable-Components.md` nem exige emenda
  a DEEPSLEEP-DEC-006. Confirmado;
- `FactoryResetService::requested_` (`factory_reset_service.hpp:17`) já é
  restaurado a `false` nos caminhos de falha (`factory_reset_service.cpp:35-46`),
  então não latcheia o pedido rejeitado. Continua valendo o achado da v0.6: com
  duas tasks disputando a transição, o token precisa de test-and-set atômico ou
  de seção crítica.

## 6. Achados novos, introduzidos pelo texto da v0.7

### 6.1 A espera por `pendingReportCount() == 0` não tem limite nem dono declarado

A v0.7 acrescentou o passo "aguardar `pendingReportCount() == 0` no caminho
antecipado" à ordem obrigatória da seção 6.1, sem limite de tempo e sem dizer
quem o executa. Isso cria um caminho em que o próprio deadline deixa de ser
observável, contrariando DEEPSLEEP-AC-007:

- o executor reagenda indefinidamente enquanto houver slot ocupado e o resultado
  for retryable — `NotReady`, `Busy` ou `Failed` recaem em
  `isRetryableResult()` e voltam ao laço após o retry
  (`issp154_report_executor.cpp:15-20,139-147`). Com o coordenador ausente, a
  contagem nunca chega a zero;
- se o dono do caminho antecipado ficar bloqueado nessa espera **depois** de já
  ter adquirido a transição, encerrado o `ResetButtonMonitor` e fechado a
  admissão, nada mais observa `maxAwakeTimeMs`. O dispositivo fica acordado,
  mudo e sem factory reset, indefinidamente — o oposto do objetivo do recurso;
- se, em vez disso, uma task supervisora separada disparar a sequência forçada
  enquanto a antecipada está bloqueada, duas execuções concorrem sobre as mesmas
  operações. A idempotência declarada na seção 6 protege cada operação
  isoladamente, mas não a ordem entre elas.

Recomendo duas frases na seção 6.1, de custo baixo: a espera do caminho
antecipado é limitada pelo tempo restante até `maxAwakeTimeMs`, findo o qual a
sequência prossegue como no deadline; e a sequência tem dono único, a mesma
entidade que observa o deadline. Isso converge os dois caminhos e é coerente com
a recomendação de task dedicada registrada em 6.2 da análise da v0.6 — que
permanece válida e agora é reforçada, porque `app_main()` retorna logo após
`setup()` (`app_main.cpp:10-22`) e não existe laço de aplicação para hospedar a
reavaliação de 3.1.

### 6.2 A arbitragem precisa distinguir o detentor da transição

Consequência direta de 6.1. A ordem obrigatória começa com "adquirir a transição
de deep sleep; abortar se factory reset já venceu". Se o caminho antecipado já
detém a transição e a sequência é retomada no deadline, uma aquisição que
apenas teste "livre ou ocupada" abortaria indevidamente. O token precisa
distinguir três estados — livre, detido pelo deep sleep, detido pelo factory
reset — e a retomada pelo próprio deep sleep deve prosseguir. É precisão de
redação de uma linha; sem ela, a leitura literal da ordem obrigatória bloqueia o
sono forçado justamente no caso em que ele é necessário.

### 6.3 `DEEPSLEEP-AC-004` e o boot com configuração inválida

AC-004 afirma que o LED acende em todo boot como primeira operação de
plataforma. A máquina de estados executa `ValidateConfiguration` **antes** de
`InitializePlatform` (`smart_sys_app.cpp:294-309`), e a seção 6 exige que
configuração inválida falhe antes de tocar GPIO. As duas regras são compatíveis
entre si, mas AC-004 é literalmente falso nesse boot: o LED não acende.

Recomendo restringir AC-004 aos boots que alcançam a inicialização de
plataforma. Fica registrada a consequência operacional: nesse boot não há LED,
não há supervisor de deadline e, portanto, não há deep sleep — o dispositivo
permanece acordado. Como a configuração é constante de compilação do product
firmware, o caso é defeito de desenvolvimento e não de campo, mas convém que a
especificação diga isso em vez de deixá-lo implícito.

### 6.4 `DEEPSLEEP-AC-003`: o limite real não é o overflow aritmético

A seção 4.2 exige detectar overflow na conversão para microssegundos. Em
aritmética de 64 bits o overflow não ocorre: o pior caso, `UINT32_MAX` horas,
dá cerca de 1,55 × 10^19 µs, abaixo do máximo de `std::uint64_t`, cerca de
1,84 × 10^19. O limite efetivo é o aceito por `esp_sleep_enable_timer_wakeup()` e
pela largura do temporizador RTC do ESP32-H2, que é menor e depende da versão do
ESP-IDF fixada no projeto.

Portanto a validação exigida por AC-003 deve mirar o limite do alvo, não apenas
a aritmética, e esse limite **não é certificável pela leitura deste
repositório**: depende do IDF. Registro como verificação necessária contra a
versão fixada, antes da implementação da validação.

### 6.5 Observação: comando em voo no instante de `beginQuiescence()`

`beginQuiescence()` responde `NotReady` a novas publicações. Um comando já
dentro de `IsspDevice::onCommand()` nesse instante terá o GPIO alterado e
`state_` atualizado por `DigitalOutputBehavior::handle()`
(`digital_output_behavior.cpp:106-128`) antes de a publicação ser recusada, e o
coordenador receberá `Failed` com o efeito físico já aplicado. O texto cobre a
resposta, não a divergência entre efeito e resposta.

Não é bloqueador: o deep sleep reinicia o firmware e o boot seguinte republica o
estado inicial, e o primeiro produto a bateria é um sensor sem comandos. Fica
registrado porque a divergência é observável pelo coordenador.

## 7. Pontos reconfirmados sem mudança

- **Composição e renome (seção 5).** Continuam implementáveis exatamente como a
  análise da v0.6 confirmou. `wake_led` entra como item das listas
  `required_resources` e `offered_resources`
  (`client_154/main/CMakeLists.txt:15-49`), sem mecanismo novo; a
  `Door Sensor Battery H2` usa GPIO 14 e GPIO 9
  (`boards/door_sensor_battery_h2.cpp:12-21`), deixando o GPIO 13 livre; o
  `board_model.hpp` ganha um `WakeLedResource` e seu acessor seguindo o
  precedente de `DryContactInputResource` (`boards/board_model.hpp:29-42`), e a
  regra já documentada ali — board declara apenas o que oferece — faz o link
  falhar em composição inconsistente. As referências do renome conferem
  (`Kconfig.projbuild:16`, `client_154/sdkconfig:949`, linha comentada e
  portanto editável sem regeneração);
- **Arbitragem em código privado da fachada.** `FactoryResetService` e
  `ResetButtonMonitor` só são construídos em
  `smart_sys_app_hardware.cpp:137-152`, e a interceptação natural continua sendo
  `requestFactoryReset()` antes de `cleanup_()`
  (`factory_reset_service.cpp:24-50`);
- **Paradas notificáveis sem Kconfig.** A conversão do `vTaskDelay(pollDelay)`
  do monitor (`reset_button_monitor.cpp:132`) e do retry do executor permanece
  direta e sem opção de menu, satisfazendo o invariante local;
- **Costura de testes.** A observação 6.3 da análise da v0.6 continua válida e
  ficou maior: a sequência da v0.7 ganhou passos, e cobrir AC-006, AC-007,
  AC-008 e AC-008A com doubles exige estender `SetupHooks`
  (`SmartSysApp.h:143-152`), header público declarado como fora do contrato
  normativo. Extensão consciente, não efeito colateral;
- **Lifetime estático.** Nenhuma operação da seção 6 destrói objeto ou permite
  reinício no mesmo boot; a fachada e seus objetos permanecem vivos até o
  reboot.

## 8. Componentes impactados

| Área | Impacto |
|---|---|
| API pública `SmartSysApp` | `configureDeepSleep()`, validação, causa de boot, LED, deadline e sequência de quiescência |
| `SetupHooks` | extensão para tornar a quiescência e o sleep verificáveis com doubles |
| `issp_core` | `IDeviceBehavior::quiesce()` e `IsspDevice::beginQuiescence()` |
| `issp_behaviors` | override de `quiesce()` no `DigitalInputBehavior`; ajuste do comentário de `hasConfirmedState()` (3.2) |
| `issp_transport_154` | `Issp154ReportExecutor::stop()`, retry notificável, flag de parada e sinal de término da task |
| `issp_app_154` | arbitragem com detentor distinguível, task supervisora dona da sequência, parada notificável do monitor e re-arme do pedido rejeitado |
| Product firmware | opt-in, política temporal, hold do factory reset e renome integral |
| Board model e CMake | recurso `wake_led`, GPIO, polaridade e composição |
| Kconfig e `sdkconfig` | símbolo, rótulo e artefato versionado |
| Testes | doubles de RTC, GPIO e sleep; concorrência de `quiesce()`, `stop()` e da arbitragem; caminho antecipado com confirmação tardia |

## 9. Restrições confirmadas

- ESP32-H2 é o único target físico do `client_154`; QEMU não é admitido;
- nenhuma execução é autorizada por esta especificação nem por esta atuação;
- Kconfig não governa lógica interna de componentes compartilhados: monitor e
  executor param por notificação, sem opção de menu;
- `persistNetwork()` permanece privado e inobservável pela fachada, o que a v0.7
  passou a assumir explicitamente em vez de contornar;
- `app_main()` retorna após `setup()`, de modo que qualquer reavaliação
  pós-`Running` depende de task própria da fachada.

## 10. Experimentos e verificações necessários

Permanecem válidos os experimentos 1 a 13 registrados nas análises da v0.5 e da
v0.6, com estas alterações e acréscimos:

- **10 (preempção de NVS)** ganha um pré-requisito de método: sem o meio
  admissível discutido em 2.1, o experimento não é provocável de forma
  determinística;
- **11 (estouro da espera de `stop()`)** deve confrontar também a permanência do
  rádio inicializado, medindo a corrente de sono com e sem `end()` (4.1);
- **12 (`Running` com `initial_stabilization_pending`)** deve incluir a
  confirmação tardia pelo classificador periódico, que é o caso decidido em 3.1;
- **14 (novo):** espera do caminho antecipado com report retryable permanente —
  coordenador ausente ou mudo — confrontando se `maxAwakeTimeMs` continua sendo
  honrado (6.1);
- **15 (novo):** deadline atingido durante a espera do caminho antecipado, com a
  transição já detida pelo deep sleep, confrontando a retomada da sequência
  (6.2);
- **16 (verificação, não experimento):** limite máximo aceito por
  `esp_sleep_enable_timer_wakeup()` no ESP32-H2 na versão do ESP-IDF fixada,
  para dimensionar a validação exigida por AC-003 (6.4).

Leitura de código não certifica nenhum desses fatos.

## 11. Recomendação

Recomenda-se **prontidão condicionada**. A v0.7 incorporou corretamente os
quatro pontos devolvidos pela análise da v0.6, e todos se sustentam no código:
a preempção diagnosticada usa apenas o estágio que a fachada já conhece; a
evidência positiva de report inicial é derivável de `reportOnStart`, do
resultado de `IsspDevice::start()` e de `hasConfirmedState()`, sem quarta
operação na API reutilizável; os 465 ms do pior caso de transporte conferem com
os constantes do código e justificam os 600 ms; e o re-arme do hold é local ao
monitor, sobre uma interface privada do `issp_app_154`. O bloqueador estrutural
da v0.6 — a exclusão dura sobre `persistNetwork()` — está resolvido.

Antes da implementação, três precisões de redação, todas de custo baixo e todas
tocando critérios de aceitação:

1. **limite e dono da espera por `pendingReportCount() == 0`** (6.1): declarar
   que a espera do caminho antecipado é limitada pelo tempo restante até
   `maxAwakeTimeMs` e que a sequência tem dono único. Sem isso, um report
   retryable permanente derrota o deadline exigido por AC-007;
2. **detentor da transição** (6.2): distinguir livre, detida pelo deep sleep e
   detida pelo factory reset, para que a retomada no deadline não aborte;
3. **alcance de AC-004** (6.3): restringir "acende em todo boot" aos boots que
   alcançam a inicialização de plataforma, já que `ValidateConfiguration`
   precede `InitializePlatform` e a configuração inválida não pode tocar GPIO.

E dois pontos de método, que não alteram o contrato:

4. **meio admissível para o experimento de AC-008B** (2.1), a ser nomeado pelo
   Arquiteto ou pela especificação futura de execução;
5. **limite de `esp_sleep_enable_timer_wakeup()` no H2** (6.4), a verificar
   contra o ESP-IDF fixado antes de implementar a validação de AC-003.

Ficam registradas ainda a correção de comentário em 3.2, a observação de
divergência entre efeito e resposta em 6.5 e a observação de produto sobre o
hold de 10 s, feita na análise da v0.6 e não invalidada pela v0.7. Builds,
testes e hardware permanecem `Not Executed`.
