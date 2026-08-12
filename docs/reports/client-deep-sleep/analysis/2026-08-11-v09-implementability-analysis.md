# Análise de implementabilidade da v0.9 — deep sleep do client

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/Client-Deep-Sleep.md`

**Revisão confrontada:** v0.9, Draft de 11/08/2026

**Estado:** Concluído

**Capacidade:** Engenheiro Analista

**Data:** 11/08/2026

**Resultado:** Implementabilidade recomendada com condições; as cinco
confirmações pedidas pela seção 8 se sustentam no código e o bloqueador da v0.8
está resolvido; nenhuma decisão normativa ausente; três precisões de redação,
uma obrigação de implementação e duas verificações de plataforma; nenhuma
execução realizada

> Este relatório registra evidências e recomendações. Não altera a fonte
> normativa, não promove estado e não autoriza implementação ou testes.

## 1. Recorte e método

A seção 8 da v0.9 determina confirmar cinco pontos: a janela não preemptível, a
criação tardia da task, a distinção entre dona da sequência e detentor do token,
a validação antecipada do timer e o polling limitado. Este relatório cobre os
cinco, verifica a incorporação dos achados da v0.8 e registra o que o texto novo
introduziu.

Nenhum build, teste, flash ou execução em hardware foi realizado. A árvore
estava limpa em `spec/client-deep-sleep`.

## 2. Incorporação dos achados da v0.8

A decisão e as duas precisões devolvidas estão incorporadas, e a decisão adotada
foi a alternativa conservadora que eu havia indicado como preferível:

- DEEPSLEEP-DEC-010 e a seção 6 tornaram `InitializePlatform` não preemptível e
  moveram a criação da task para o fim do estágio bem-sucedido;
- a seção 6 passou a dizer que a task é dona exclusiva **da sequência**, com o
  token de transição explicitamente disputado com o factory reset;
- a seção 4.2 e DEEPSLEEP-AC-003 passaram a validar a faixa do intervalo em
  `configureDeepSleep()`, com repetição no preparo como defesa em profundidade.

A consequência que eu havia registrado em 3.3 da análise da v0.8 — a janela de
`nvs_flash_erase()` sem diagnóstico nem cobertura em DEEPSLEEP-AC-008B — fica
resolvida pela mesma decisão: como nenhuma sequência de sleep começa em
`InitializePlatform`, aquela janela deixa de ser preemptável e não precisa de
diagnóstico próprio. Também está incorporada a consequência de implementação que
eu havia registrado sobre o polling, agora com período máximo declarado.

## 3. Janela não preemptível e criação tardia da task — confirmadas

### 3.1 O ponto de criação existe e é limpo

A seção 6 exige que a task nasça "ao final de `InitializePlatform` bem-sucedido,
quando todos os recursos obrigatórios existem e o monitor opcional já foi
tratado". Esse instante é diretamente identificável na máquina de estados: o
retorno `Ok` de `hooks_.initializePlatform()` (`smart_sys_app.cpp:305-309`),
imediatamente antes de `InitializeNetwork`. Nesse ponto,
`realInitializePlatform()` já construiu transport, network manager, device e
executor (`smart_sys_app_hardware.cpp:132-135`) e já tratou o monitor opcional,
inclusive o `start()` cuja falha aborta o estágio
(`smart_sys_app_hardware.cpp:137-152`).

Criar a task no `smart_sys_app.cpp`, e não dentro do hook, tem duas virtudes que
registro por afetarem DEEPSLEEP-AC-010: mantém o ponto de criação no arquivo
target-agnóstico, alcançável pelos testes que usam o construtor com
`SetupHooks`, e não acrescenta responsabilidade ao hook real, que os testes
nunca linkam.

### 3.2 O bloqueador da v0.8 está eliminado

Com a task nascendo depois do estágio, desaparece a situação em que a ordem
obrigatória mandava operar sobre `std::optional` vazios. Todos os recursos
obrigatórios da sequência existem quando a task passa a existir, e o único
recurso opcional — o `ResetButtonMonitor` — já é condicional na própria ordem
("encerrar `ResetButtonMonitor`, quando configurado"). Confirmado.

### 3.3 Confirmação positiva: falha posterior ao estágio ainda leva ao sono

Verifiquei o caso que mais importa para bateria e ele funciona. Depois de
`InitializePlatform` bem-sucedido, a task existe; qualquer falha posterior
—`InitializeNetwork` retornando `NotReady` por coordenador ausente
(`smart_sys_app.cpp:317-326`), ou falha dura com `rollbackTransport()`
(`smart_sys_app.cpp:337,347,356`) — deixa o dispositivo com a task viva, e o
deadline conduz ao caminho forçado em vez de mantê-lo acordado indefinidamente.

Confirmei que cada passo terminal é seguro nesse estado: `Issp154Transport::end()`
retorna `Ok` de imediato quando o transporte já está `Stopped`
(`issp154_transport.cpp:375-377`), de modo que o rollback anterior não cria
conflito; e os behaviors nunca iniciados não têm timer a parar.

Disso decorre uma **obrigação de implementação** que registro por não estar
escrita: `quiesce()` e `Issp154ReportExecutor::stop()` precisam ser seguros
quando o objeto nunca foi iniciado neste boot, não apenas idempotentes sob
repetição. O executor só ganha `taskHandle_` em `start()`
(`issp154_report_executor.cpp:44-56`), que não roda quando o setup falha antes
de `StartReportExecutor`; `stop()` sobre `taskHandle_ == nullptr` deve ser no-op
diagnosticado, não falha. O mesmo vale para `quiesce()` em behavior registrado
mas nunca iniciado. A palavra "idempotente" da seção 6 cobre a repetição, não a
ausência de início.

### 3.4 Precisão: `maxAwakeTimeMs` deixa de ser cota do tempo acordado total

DEEPSLEEP-DEC-010 tem um efeito que a especificação não declara. Como o deadline
é contado desde a entrada de `setup()` mas nenhuma sequência começa durante
`InitializePlatform`, o tempo acordado real passa a ser, no pior caso, a duração
do estágio mais a sequência terminal — e não `maxAwakeTimeMs`.

Os dois excedentes são conhecidos e delimitáveis:

- **antes do deadline:** a duração de `InitializePlatform`, dominada por
  `initializeNvs()`, que chama `nvs_flash_erase()` no caminho de partição sem
  páginas livres ou com versão nova (`smart_sys_app_hardware.cpp:52-66`) — apagar
  a partição é operação de flash de centenas de milissegundos a segundos;
- **depois do deadline:** a própria sequência, cujo componente dominante é o
  limite de 600 ms de `stop()`, mais preparo da fonte, quiescência e apagamento
  do LED.

O texto atual é honesto quanto ao segundo — "ao atingir `maxAwakeTimeMs`, a
fachada **inicia** quiescência forçada" —, mas silencioso quanto ao primeiro.
Para um produto a bateria, cujo dimensionamento depende do tempo acordado por
ciclo, recomendo declarar o orçamento em uma frase: `maxAwakeTimeMs` limita a
parte preemptível do ciclo, e o tempo acordado total é a duração de
`InitializePlatform` mais esse orçamento mais a sequência terminal. Não muda
comportamento; evita que DEEPSLEEP-AC-010 confronte o consumo contra uma
expectativa que o contrato nunca prometeu.

## 4. Dona da sequência e detentor do token — confirmada

A seção 6 agora separa corretamente os dois conceitos, e a separação corresponde
ao código: o caminho de factory reset executa a limpeza e `esp_restart()` no
contexto do chamador, que é a task do monitor
(`factory_reset_service.cpp:24-50`, chamada de `reset_button_monitor.cpp:128`),
enquanto a sequência de deep sleep pertence à task de lifecycle. Um token com
três estados, disputado por dois escritores e resolvido por troca comparada, é
implementável nesse arranjo. A leitura que induziria escritor único desapareceu.

Continua valendo que `requested_` é `bool` simples
(`factory_reset_service.hpp:17`) e precisa de test-and-set atômico, e que o
re-arme do hold é local ao monitor sobre `IFactoryResetRequester`, interface
privada em `components/issp_app_154/src/reset/` e portanto fora de
`ISSP-Reusable-Components.md`.

## 5. Validação antecipada do timer — confirmada, com uma condição de fronteira

O desenho da seção 4.2 é implementável: um helper puro, chamado por
`configureDeepSleep()`, devolvendo `AppResult::InvalidArgument` sem tocar RTC, e
repetido no preparo como defesa em profundidade. `InvalidArgument` já existe em
`AppResult` (`SmartSysApp.h:117-124`), então nada é acrescentado ao enum, como a
seção 3 exige. Existe também precedente de código condicional ao target dentro
do componente compartilhado (`smart_sys_app.cpp:102`), usado para
disponibilidade de tradução e não para regra de produto, o que mantém intacto o
invariante de que Kconfig não governa lógica interna de componentes
compartilhados.

Dois pontos precisam de atenção, ambos decorrentes da configuração real do
projeto.

### 5.1 O slow clock do projeto é o RC interno, e isso torna o limite calibrado

O `client_154` usa `CONFIG_RTC_CLK_SRC_INT_RC=y` com
`CONFIG_RTC_CLK_CAL_CYCLES=1024` (`client_154/sdkconfig:1148-1151`), não
cristal externo. O oscilador RC interno é calibrado em tempo de execução e varia
com peça e temperatura, de modo que a conversão de microssegundos para ticks do
temporizador de baixo consumo — e portanto o intervalo máximo efetivamente
aceito — não é uma constante de compilação exata.

A frase da seção 4.2, "derivado das capacidades do target e da fonte de slow
clock fixada no projeto", é implementável desde que o limite antecipado seja
**conservador**: calculado para o extremo que produz a menor duração
representável, de modo que tudo que a configuração aceitar continue aceito pelo
preparo. Sem essa qualificação, um intervalo próximo da fronteira passa em
`configureDeepSleep()` e é rejeitado no preparo — e, pela regra da seção 6.1, a
falha da fonte solicitada aborta a sequência e preserva o runtime, reproduzindo
exatamente o "dispositivo a bateria que nunca dorme" que a v0.9 quis eliminar,
agora restrito à faixa de fronteira. Recomendo uma frase declarando que o limite
antecipado é conservador em relação ao aceito pelo preparo.

### 5.2 A origem do valor ainda não é certificável aqui

O ESP-IDF não está vendorizado nem instalado no ambiente desta atuação, então
não posso certificar por leitura nem o valor do limite nem o nome da capacidade
que o exprime em 6.0.1. A versão confere (`client_154/sdkconfig:686`,
`CONFIG_IDF_INIT_VERSION="6.0.1"`, coerente com `SYSTEM-DOSSIER.md:64`).

Registro como verificação de toolchain, com uma recomendação de implementação: o
limite deve vir das capacidades declaradas pelo próprio ESP-IDF para o target,
não de um número escrito à mão atrás de `CONFIG_IDF_TARGET_ESP32H2`. Um valor
mágico no componente compartilhado envelheceria em silêncio a cada atualização
de IDF, e é justamente o tipo de acoplamento que o invariante local evita.

## 6. Polling limitado — confirmado, com o predicado trocado

O período máximo de 10 ms é implementável e barato: `pendingReportCount()` é
leitura sob seção crítica curta (`issp_device.cpp:165-171`), e a alternativa por
notificação está de fato indisponível, porque `setPendingReportHandler()` guarda
um único par handler/contexto (`issp_device.cpp:310-318`) já tomado pelo
executor (`issp154_report_executor.cpp:57-58`). A justificativa escrita na seção
6 corresponde ao código.

**Precisão:** a seção 6 atribui ao polling de `pendingReportCount()` a
reavaliação da *prontidão dos reports iniciais*, e esses são predicados
diferentes. A prontidão é dada por `reportOnStart`, pelo resultado de
`IsspDevice::start()` e por `DigitalInputBehavior::hasConfirmedState()`
(`digital_input_behavior.cpp:386-389`), como a própria seção 6 declara dois
parágrafos adiante; `pendingReportCount()` é o oráculo de **entrega**, usado
depois da quiescência. Ambos são polled, mas na ordem atual do texto um
implementador pode condicionar a antecipação à contagem pendente — que é
satisfeita por ausência de report, exatamente o defeito que DEEPSLEEP-DEC-009
existe para impedir. Recomendo nomear os dois predicados separadamente e aplicar
o teto de 10 ms a ambos.

## 7. Pontos reconfirmados sem mudança

- **Evidência positiva de report inicial.** Inalterada e já confirmada:
  `DigitalOutputBehavior::begin()` publica sincronamente e a falha propaga por
  `IsspDevice::start()` até impedir `Running`
  (`digital_output_behavior.cpp:52-71`, `issp_device.cpp:49-62`), e
  `hasConfirmedState()` só é verdadeiro após publicação requerida bem-sucedida
  (`digital_input_behavior.cpp:326-370`);
- **`stop()` em 600 ms e supressão de `end()`.** Os 465 ms conferem com
  `kPhysicalTxTimeoutMs`, `kReportAckTimeoutMs` e `kConfirmedSendBackoffMs`;
  `end()` desinicializa o rádio antes de destruir o event group
  (`issp154_transport.cpp:390-399`). Permanecem as três obrigações já
  registradas: consultar a flag de parada antes de iniciar nova tentativa,
  desbloquear as duas esperas da task e expor sinal observável de término;
- **Ordem `beginQuiescence` antes de `quiesce`.** Continua sendo o que torna
  segura uma amostragem periódica que dispare durante a parada: a publicação
  tardia recebe `NotReady` e não ocupa slot. `quiesce()` reusa
  `stopAndDeleteTimer()` (`digital_input_behavior.cpp:91-104`), que zera
  `timer_` e mantém o destrutor seguro;
- **Arbitragem e re-arme do hold.** Interceptação em
  `requestFactoryReset()` antes de `cleanup_()`; `resetRequested` é variável
  local do laço (`reset_button_monitor.cpp:83`) e `pressed`/`pressedAtUs`
  sobrevivem à rejeição, de modo que o pedido reaparece no poll seguinte;
- **Composição e renome.** `wake_led` entra como item das listas
  `required_resources` e `offered_resources`
  (`client_154/main/CMakeLists.txt:15-49`); GPIO 13 permanece livre na
  `Door Sensor Battery H2` (`boards/door_sensor_battery_h2.cpp:12-21`); o
  `board_model.hpp` ganha recurso e acessor pelo precedente existente; as
  referências do renome conferem (`Kconfig.projbuild:16`,
  `client_154/sdkconfig:949`);
- **Alcance do LED.** Seções 1, 4.3, 6, AC-004 e DEC-002 concordam entre si e com
  a precedência de `ValidateConfiguration` sobre `InitializePlatform`
  (`smart_sys_app.cpp:294-309`).

## 8. Editorial

DEEPSLEEP-AC-007 abre com "o deadline é observado em contado desde `setup()`",
frase truncada pela edição. Como o texto do critério é contrato, convém
corrigi-la.

## 9. Componentes impactados

| Área | Impacto |
|---|---|
| API pública `SmartSysApp` | `configureDeepSleep()`, validação conservadora de faixa, causa de boot, LED, deadline e sequência |
| `SetupHooks` | costura interna declarada por DEEPSLEEP-AC-010, incluindo limite injetado nos doubles |
| `issp_core` | `IDeviceBehavior::quiesce()` e `IsspDevice::beginQuiescence()` |
| `issp_behaviors` | override de `quiesce()` reusando `stopAndDeleteTimer()`; comentário de `hasConfirmedState()` |
| `issp_transport_154` | `Issp154ReportExecutor::stop()`, retry notificável, flag de parada, sinal de término e no-op quando nunca iniciado |
| `issp_app_154` | task criada ao fim de `InitializePlatform`, token de três estados, polling de dois predicados, parada notificável do monitor e re-arme do pedido rejeitado |
| Product firmware | opt-in, política temporal, hold do factory reset e renome integral |
| Board model e CMake | recurso `wake_led`, GPIO, polaridade e composição |
| Kconfig e `sdkconfig` | símbolo, rótulo e artefato versionado |
| Testes | doubles de RTC, GPIO e sleep; deadline já expirado ao fim do estágio; falha posterior ao estágio conduzindo ao caminho forçado; conversão antecipado→forçado |

## 10. Restrições confirmadas

- ESP32-H2 é o único target físico do `client_154`; QEMU não é admitido;
- nenhuma execução é autorizada por esta especificação nem por esta atuação;
- Kconfig não governa lógica interna de componentes compartilhados; o
  condicional de target existente serve à disponibilidade de tradução, não à
  regra de produto (`smart_sys_app.cpp:102`);
- o device oferece um único registro de notificação de report pendente, já
  tomado pelo executor, o que torna o polling a única espera admissível;
- o slow clock do projeto é o RC interno calibrado
  (`client_154/sdkconfig:1148-1151`), não cristal externo;
- `app_main()` retorna após `setup()`, de modo que a task privada de lifecycle é
  a única hospedeira possível da reavaliação e do deadline.

## 11. Experimentos e verificações necessários

Permanecem válidos os itens registrados nas análises anteriores. O item 18 da
análise da v0.8 — deadline durante `nvs_flash_erase()` — fica **retirado**, por
ter deixado de existir com DEEPSLEEP-DEC-010. Acrescentam-se:

- **16 (verificação de toolchain, ampliada):** limite aceito por
  `esp_sleep_enable_timer_wakeup()` no ESP32-H2 com ESP-IDF 6.0.1 e a capacidade
  do IDF que o exprime, para dimensionar o helper de 4.2 e garantir a margem
  conservadora de 5.1;
- **19 (novo):** deadline já expirado no instante em que a task nasce,
  confrontando a entrada imediata no caminho forçado exigida por DEC-010;
- **20 (novo):** falha de `InitializeNetwork`, nas variantes `NotReady` e dura
  com rollback, confrontando se o caminho forçado conclui o sono com transporte
  já `Stopped` e executor nunca iniciado (3.3);
- **21 (nota para o experimento de corrente de AC-010):** o projeto está com
  `CONFIG_ESP_SLEEP_POWER_DOWN_FLASH` desligado e
  `CONFIG_ESP_SLEEP_FLASH_LEAKAGE_WORKAROUND=y`
  (`client_154/sdkconfig:1135-1136`). A medição de corrente deve registrar essa
  configuração, porque ela afeta o valor absoluto tanto no caminho com `end()`
  quanto no que o suprime.

Leitura de código não certifica nenhum desses fatos.

## 12. Recomendação

Recomenda-se **prontidão condicionada**, sem decisão normativa ausente. Esta é a
primeira revisão da série em que não encontro bloqueador estrutural nem questão
que exija escolha do Arquiteto entre alternativas normativas. As cinco
confirmações pedidas se sustentam: a janela não preemptível elimina o problema
dos recursos inexistentes e, de quebra, a janela de `nvs_flash_erase()`; o ponto
de criação da task existe e é limpo na máquina de estados; a distinção entre
dona da sequência e detentor do token corresponde ao código; a validação
antecipada do timer cabe em `AppResult` vigente; e o polling de 10 ms é a única
espera admissível e é barata.

Antes da implementação, três precisões de redação:

1. **orçamento de tempo acordado** (3.4): declarar que `maxAwakeTimeMs` limita a
   parte preemptível do ciclo e que o tempo acordado total inclui
   `InitializePlatform` e a sequência terminal;
2. **margem conservadora do limite do timer** (5.1): o limite antecipado deve
   ser conservador em relação ao aceito no preparo, porque o slow clock do
   projeto é RC interno calibrado; sem isso, a faixa de fronteira reproduz o
   dispositivo que nunca dorme;
3. **predicados do polling** (6): separar a reavaliação de prontidão dos reports
   iniciais do oráculo de entrega `pendingReportCount() == 0`, aplicando o teto
   de 10 ms a ambos.

Uma obrigação de implementação a registrar no momento da autoria ou da
implementação: `quiesce()` e `stop()` devem ser seguros quando o objeto nunca
foi iniciado neste boot, e não apenas idempotentes sob repetição (3.3).

E duas verificações de plataforma: o limite e a capacidade do ESP-IDF 6.0.1 para
o H2 (5.2), e o registro da configuração de flash em deep sleep na medição de
corrente de DEEPSLEEP-AC-010 (item 21). Fica ainda a correção editorial de
DEEPSLEEP-AC-007 (8). Builds, testes e hardware permanecem `Not Executed`.
