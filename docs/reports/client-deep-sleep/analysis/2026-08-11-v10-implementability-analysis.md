# Análise de implementabilidade da v0.10 — deep sleep do client

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/Client-Deep-Sleep.md`

**Revisão confrontada:** v0.10, Draft de 11/08/2026

**Estado:** Concluído

**Capacidade:** Engenheiro Analista

**Data:** 11/08/2026

**Resultado:** Implementabilidade recomendada; as quatro precisões e a correção
de DEEPSLEEP-AC-007 estão incorporadas e confirmadas no código; nenhum
bloqueador, nenhuma decisão normativa ausente e nenhuma precisão pendente;
restam verificações de toolchain resolvidas na própria implementação e os
experimentos de hardware já declarados; nenhuma execução realizada

> Este relatório registra evidências e recomendações. Não altera a fonte
> normativa, não promove estado e não autoriza implementação ou testes.

## 1. Recorte e método

A seção 8 da v0.10 determina confirmar quatro pontos — orçamento total acordado,
margem conservadora do timer com RC interno, separação dos predicados de
prontidão e entrega, e segurança de `quiesce()` e `stop()` quando nunca
iniciados — mais a correção de DEEPSLEEP-AC-007. Este relatório cobre os cinco e
registra o que encontrei ao reconfrontar a sequência terminal.

Nenhum build, teste, flash ou execução em hardware foi realizado. A árvore
estava limpa em `spec/client-deep-sleep`.

## 2. Orçamento total acordado — confirmado

A fórmula da seção 6 é aritmeticamente correta para o comportamento descrito:

```text
max(duração de InitializePlatform, maxAwakeTimeMs) + duração da sequência terminal
```

Os dois ramos conferem com a máquina de estados. Se o estágio durar mais que o
prazo, a task nasce com o deadline já expirado e entra de imediato no caminho
forçado, e o termo dominante é a duração do estágio. Se durar menos, o sono
ocorre por antecipação ou no deadline, e o termo dominante é `maxAwakeTimeMs` —
inclusive a espera por entrega, que a seção 6.1 confina ao tempo restante e
portanto não acrescenta termo novo. A parcela anterior a `InitializePlatform` é
apenas `ValidateConfiguration`, uma comparação (`smart_sys_app.cpp:296-299`), e
não desloca o resultado.

A declaração de que `maxAwakeTimeMs` não é sozinho um teto para o tempo físico
acordado é o ponto que faltava, e DEEPSLEEP-AC-010 passou a exigir medição
separada dos três termos. Confirmado.

### 2.1 A sequência terminal é limitada, e o único termo sem limite contratual é curto

Reconfrontei os passos posteriores ao deadline para verificar se o terceiro
termo da fórmula é delimitável na prática. É:

- preparo da fonte de wakeup: chamada de configuração, sem espera;
- encerramento do `ResetButtonMonitor`: limitado pelo `pollIntervalMs`
  configurado, 20 ms no produto atual (`firmwares/door_sensor.cpp:19`);
- `beginQuiescence` e `quiesce`: seções críticas curtas e parada de um
  `esp_timer` cujo callback é uma leitura de GPIO mais classificação
  (`digital_input_behavior.cpp:209-246`);
- `stop()` do executor: **600 ms**, único limite contratual e termo dominante;
- `end()` do transporte: `issp154_transport_deinit()` não bloqueia — testa
  `s_tx_sync_state` sob seção crítica e retorna `ESP_ERR_INVALID_STATE` de
  imediato se houver transmissão em curso, e nos demais casos apenas desfaz
  rádio, task de RX, fila e event group (`issp154_transport.c:275-311`);
- apagamento do LED e entrada em deep sleep.

Portanto o terceiro termo é dominado pelos 600 ms e não há passo de duração
aberta. Registro isso como nota de dimensionamento para DEEPSLEEP-AC-010, não
como pedido de mudança: a especificação faz certo em medir em vez de contratar.

### 2.2 Refinamento do motivo da regra de supressão de `end()`

Ao ler o deinit encontrei um detalhe que precisa a justificativa da seção 6.1 e
que registro por reforçá-la, não por contradizê-la. A camada C já se protege de
**uma** das duas janelas de risco: se houver transmissão síncrona em curso,
`issp154_transport_deinit()` recusa com `ESP_ERR_INVALID_STATE`
(`issp154_transport.c:277-282`), e `Issp154Transport::end()` retorna antes de
alcançar `vEventGroupDelete(ackEventGroup_)` (`issp154_transport.cpp:390-399`).

A janela que **não** está protegida é exatamente a que a especificação teme: com
o quadro já transmitido e o executor esperando ACK dentro de
`waitAckAttemptOutcome()` sobre `ackEventGroup_`
(`issp154_transport.cpp:239-290`), o estado de TX está ocioso, o deinit sucede e
`vEventGroupDelete()` destrói o handle sob o esperador. A regra de pular `end()`
no estouro dos 600 ms continua sendo a proteção necessária, e agora com o
recorte exato de qual sub-janela ela cobre.

## 3. Margem conservadora do timer — confirmada

A seção 4.2 e DEEPSLEEP-AC-003 agora exigem que todo intervalo aceito em
`configureDeepSleep()` permaneça na faixa aceita no preparo "para qualquer
variação suportada da calibração", e proíbem número de produto sem vínculo com
as capacidades do target e a fonte de clock. A direção da desigualdade está
correta: o limite de configuração deve ser o ínfimo dos limites de preparo sobre
as calibrações admitidas, e não o nominal.

A premissa confere com o projeto: `CONFIG_RTC_CLK_SRC_INT_RC=y` com
`CONFIG_RTC_CLK_CAL_CYCLES=1024` (`client_154/sdkconfig:1148-1151`), isto é, RC
interno calibrado em runtime, não cristal externo. A ressalva "salvo falha da API
ou do runtime" em AC-003 é adequada, porque preserva a defesa em profundidade
sem transformá-la em contradição com a garantia de configuração.

A proibição do número mágico também está bem colocada em relação ao invariante
local: o componente compartilhado já possui precedente de condicional por target
usado para disponibilidade de tradução, não para regra de produto
(`smart_sys_app.cpp:102`), e derivar o limite das capacidades declaradas pelo
ESP-IDF mantém esse limite.

Permanece a verificação registrada nas análises anteriores: o valor e o nome da
capacidade em ESP-IDF 6.0.1 não são certificáveis por leitura deste
repositório, porque o IDF não está vendorizado nem instalado neste ambiente. A
versão confere (`client_154/sdkconfig:686`). Diferentemente das versões
anteriores, essa pendência não bloqueia nada: é verificação que a própria
implementação resolve com o toolchain presente.

## 4. Predicados separados — confirmados

A seção 6 passou a enumerar os dois predicados e a declarar que "contagem
pendente igual a zero não satisfaz o predicado de prontidão e ausência de report
nunca equivale a admissão", e DEEPSLEEP-AC-006 repete a separação. O defeito que
a redação anterior podia induzir desapareceu.

Confirmo que os dois predicados são calculáveis com o que a fachada já possui:

- **prontidão.** O conjunto esperado vem de `reportOnStart` nas configurações
  copiadas (`smart_sys_app_impl.hpp:81,88`). "O início de cada behavior esperado
  deve ter sido bem-sucedido" é observável pelo resultado agregado do hook
  `startDevice`, porque `IsspDevice::start()` aborta no primeiro behavior que não
  retorne `Ok` (`issp_device.cpp:49-62`) e a falha impede `Running`
  (`smart_sys_app.cpp:342-349`); logo `Running` já implica sucesso de todos. A
  evidência positiva de admissão é o próprio `begin()` para
  `DigitalOutputBehavior`, que publica de forma síncrona e propaga a falha
  (`digital_output_behavior.cpp:52-71`), e `hasConfirmedState()` para
  `DigitalInputBehavior`, que só é verdadeiro depois de publicação requerida
  bem-sucedida (`digital_input_behavior.cpp:326-370`);
- **entrega.** `pendingReportCount()` decrementa apenas em entrega confirmada com
  geração coerente (`issp_device.cpp:255-264`) e mantém contados os slots em voo.

O polling a 10 ms é seguro contra corrida em ambos: `confirmedState_` é
`std::atomic<std::uint8_t>` (`digital_input_behavior.hpp:97`) e a leitura
relaxada basta para um predicado booleano de prontidão; `pendingReportCount()`
lê sob a mesma seção crítica curta usada pelos produtores
(`issp_device.cpp:165-171`). Em um alvo single-core, o custo de interrupções
desabilitadas a cada 10 ms é desprezível.

Observação de leitura, sem pedido de mudança: a ordem obrigatória diz "quiesce em
todos os behaviors registrados", e existem dois registros possíveis — o array da
fachada, preenchido na configuração (`smart_sys_app_impl.hpp:77-79`), e o do
device, preenchido em `RegisterCapabilities`. Eles divergem apenas quando o setup
falha antes desse estágio. Com a semântica de no-op da seção 6, as duas leituras
são seguras; iterar o array da fachada cobre estritamente mais e é o que eu
recomendaria na implementação.

## 5. `quiesce()` e `stop()` quando nunca iniciados — confirmados

A seção 6 acrescentou as duas cláusulas de no-op com sucesso, e ambas são
diretamente implementáveis com estado que já existe, sem campo novo:

- **`DigitalInputBehavior::quiesce()`.** `stopAndDeleteTimer()` já retorna cedo
  quando `timer_ == nullptr` (`digital_input_behavior.cpp:91-104`), e `timer_` só
  é não nulo depois de `createTimer()`. Todos os caminhos de falha de
  `beginTimerBacked()` chamam `stopAndDeleteTimer()` e zeram `publisher_`
  (`digital_input_behavior.cpp:129-134,143-148,171-176`), inclusive a falha de
  `esp_timer_start_periodic`. Não existe estado intermediário em que o objeto
  fique com timer vivo e `begin()` malsucedido, então o predicado "begin()
  concluído com sucesso" é observável e o no-op é exato;
- **`Issp154ReportExecutor::stop()`.** `taskHandle_` nasce nulo
  (`issp154_report_executor.cpp:31`) e só é atribuído depois de
  `xTaskCreateStatic()` ter retornado handle válido
  (`issp154_report_executor.cpp:44-56`), de modo que `taskHandle_ == nullptr`
  identifica com precisão o executor nunca iniciado.

Confirmo também o caso que motivou a cláusula: depois de `InitializePlatform`
bem-sucedido a task existe, e uma falha posterior — `InitializeNetwork` em
`NotReady` (`smart_sys_app.cpp:317-326`) ou falha dura com `rollbackTransport()`
— conduz ao caminho forçado com executor nunca iniciado, behaviors nunca
iniciados e transporte possivelmente já `Stopped`, caso em que `end()` retorna
`Ok` de imediato (`issp154_transport.cpp:375-377`). A sequência inteira é segura
nesse estado, que é justamente o que evita manter um dispositivo a bateria
acordado após um setup malsucedido.

## 6. Correção de DEEPSLEEP-AC-007 — confirmada

O critério agora abre com "o deadline é contado desde `setup()`, mas
`InitializePlatform` não é preemptível", sem o truncamento da v0.9. Ele também
incorporou as cláusulas de no-op e o teto de 10 ms, ficando consistente com a
seção 6.

## 7. Pontos reconfirmados sem mudança

Permanecem válidos e não foram alterados pela v0.10: a arbitragem de três
estados em código privado da fachada; o re-arme do hold sobre
`IFactoryResetRequester`, interface privada em
`components/issp_app_154/src/reset/`; os 465 ms de pior caso do transporte que
justificam os 600 ms; a ordem `beginQuiescence` antes de `quiesce`; o alcance do
LED diante de `ValidateConfiguration`; a composição e o renome, com GPIO 13
livre na `Door Sensor Battery H2` e `wake_led` entrando como item das listas de
recursos (`client_154/main/CMakeLists.txt:15-49`); e a não ampliação da API
reutilizável além das três operações de DEEPSLEEP-DEC-006.

## 8. Componentes impactados

Sem alteração material em relação à análise da v0.9. A tabela daquele relatório
permanece válida; a v0.10 não move responsabilidade entre camadas, não cria
operação nova e não muda assinatura alguma.

## 9. Restrições confirmadas

- ESP32-H2 é o único target físico do `client_154`; QEMU não é admitido;
- nenhuma execução é autorizada por esta especificação nem por esta atuação;
- Kconfig não governa lógica interna de componentes compartilhados;
- o device oferece um único registro de notificação de report pendente, já
  tomado pelo executor, o que mantém o polling como única espera admissível;
- o slow clock do projeto é o RC interno calibrado
  (`client_154/sdkconfig:1148-1151`);
- `issp154_transport_deinit()` não bloqueia e recusa quando há transmissão
  síncrona em curso (`issp154_transport.c:275-282`);
- `app_main()` retorna após `setup()`, de modo que a task privada de lifecycle é
  a única hospedeira possível da reavaliação e do deadline.

## 10. Experimentos e verificações necessários

O conjunto está estável. Permanecem os itens das análises anteriores, com o item
18 já retirado na v0.9, e mantêm-se em aberto:

- **16 (verificação de toolchain):** limite aceito por
  `esp_sleep_enable_timer_wakeup()` no ESP32-H2 com ESP-IDF 6.0.1 e a capacidade
  do IDF que o exprime, incluindo a margem conservadora exigida pela seção 4.2.
  Resolvida na implementação, com o toolchain presente;
- **10, 11, 19, 20 e 21:** preempção de `persistNetwork()` pelo meio instrumentado
  já declarado; estouro da espera de `stop()` com `sendConfirmed` ativo, agora
  focado na sub-janela de espera de ACK identificada em 2.2; deadline já expirado
  quando a task nasce; falha posterior ao estágio conduzindo ao caminho forçado;
  e medição de corrente registrando as opções vigentes de power-down e mitigação
  de leakage da flash;
- **novo desdobramento de 21:** a medição dos três termos da fórmula da seção 6,
  exigida por DEEPSLEEP-AC-010, deve incluir o caminho em que
  `initializeNvs()` recorre a `nvs_flash_erase()`
  (`smart_sys_app_hardware.cpp:52-66`), que é o pior caso conhecido do primeiro
  termo.

Leitura de código não certifica nenhum desses fatos.

## 11. Recomendação

Recomenda-se **prontidão**. As quatro precisões da análise da v0.9 e a correção
de DEEPSLEEP-AC-007 estão incorporadas, correspondem ao que foi recomendado e se
sustentam no código: a fórmula do orçamento é correta nos dois ramos e o
terceiro termo é dominado pelos 600 ms de `stop()`; a margem conservadora do
timer está na direção certa da desigualdade e a premissa do RC interno confere
com o `sdkconfig`; os dois predicados são calculáveis com o que a fachada já
possui e o polling a 10 ms é livre de corrida sobre um atômico e uma seção
crítica curta; e as duas cláusulas de no-op correspondem a estado que já existe
nos objetos, sem campo novo.

Não encontro bloqueador estrutural, decisão normativa ausente nem precisão de
redação pendente. Diferentemente das revisões anteriores, as pendências restantes
não são do contrato: são a verificação do limite do ESP-IDF, que a implementação
resolve com o toolchain presente, e os experimentos de hardware já declarados,
que esta especificação corretamente não autoriza.

Registro, por serem úteis a quem implementar e não por exigirem mudança
normativa: iterar o array de behaviors da fachada, e não o do device, ao aplicar
`quiesce()` (4); e considerar, no experimento de `stop()`, que a camada C já
recusa o deinit sob transmissão síncrona, de modo que a janela a confrontar é a
da espera de ACK (2.2). Builds, testes e hardware permanecem `Not Executed`.

A recomendação informa o Arquiteto. Não certifica implementabilidade de forma
absoluta e não promove a especificação para `Pronta`.
