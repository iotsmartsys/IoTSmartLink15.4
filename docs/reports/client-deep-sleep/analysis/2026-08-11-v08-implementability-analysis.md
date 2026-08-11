# Análise de implementabilidade da v0.8 — deep sleep do client

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/Client-Deep-Sleep.md`

**Revisão confrontada:** v0.8, Draft de 11/08/2026

**Estado:** Concluído

**Capacidade:** Engenheiro Analista

**Data:** 11/08/2026

**Resultado:** Implementabilidade recomendada com condições; as cinco
confirmações pedidas pela seção 8 se sustentam no código; um bloqueador novo
criado pelo próprio dono único, uma janela de NVS não coberta, uma
inconsistência de redação e uma consequência de produto na validação do timer;
nenhuma execução realizada

> Este relatório registra evidências e recomendações. Não altera a fonte
> normativa, não promove estado e não autoriza implementação ou testes.

## 1. Recorte e método

A seção 8 da v0.8 determina confirmar cinco pontos: o dono único da sequência, a
espera limitada pelo deadline, os três estados da arbitragem, o alcance do LED e
a validação do limite real do timer. Este relatório cobre os cinco, verifica a
incorporação dos achados da v0.7 e registra o que o texto novo introduziu.

Nenhum build, teste, flash ou execução em hardware foi realizado. A árvore
estava limpa em `spec/client-deep-sleep`.

## 2. Incorporação dos achados da v0.7

As três precisões e os dois pontos de método estão incorporados e correspondem
ao que foi recomendado:

- a espera de entrega passou a usar somente o tempo restante de
  `maxAwakeTimeMs`, e a sequência ganhou dona única (seção 6 e ordem da 6.1);
- a arbitragem passou a declarar três estados do detentor e retomada sem
  reaquisição (seção 6.1);
- DEEPSLEEP-AC-004 e DEEPSLEEP-DEC-002 passaram a se restringir aos boots que
  alcançam `InitializePlatform`, com a consequência operacional escrita na
  seção 6;
- o meio admissível do experimento de NVS foi nomeado — build H2 temporariamente
  instrumentado, com barreira privada antes de `nvs_commit()` e varredura
  controlada de atrasos (seção 6.1);
- DEEPSLEEP-AC-003 passou a mirar o limite do ESP-IDF/H2 em vez do overflow
  aritmético.

Duas incorporações menores que eu havia registrado também entraram: a correção
do comentário de `hasConfirmedState()` (seção 6) e a extensão de `SetupHooks`
como costura interna declarada, mais o confronto de corrente com e sem `end()`
(DEEPSLEEP-AC-010). Nenhuma delas amplia a API normativa.

## 3. Dono único da sequência — confirmado, com um bloqueador novo

### 3.1 O modelo de dono único é implementável e resolve o achado da v0.7

A escolha de uma task privada de lifecycle, dona da transição e da sequência
completa, é implementável e elimina a concorrência que eu havia registrado. Ela
também é a única forma viável no repositório atual: `app_main()` retorna logo
após `startSelectedProductFirmware()` (`app_main.cpp:10-22`), não há laço de
aplicação, e o `esp_timer` task já despacha o amostrador do
`DigitalInputBehavior` (`dispatch_method = ESP_TIMER_TASK`,
`digital_input_behavior.cpp:79-86`), de modo que hospedar a quiescência nele
acoplaria a sequência ao timer que ela mesma precisa parar.

A ordem interna também é coerente com o código: a task nasce em
`InitializePlatform`, depois do LED, e não existe quando a configuração falha,
porque `ValidateConfiguration` retorna antes (`smart_sys_app.cpp:294-309`).

### 3.2 Bloqueador: a task nasce antes dos objetos que a sequência manipula

A seção 6 determina que a task comece em `InitializePlatform`, logo depois do
acionamento do LED. Nesse instante, nenhum dos objetos da sequência existe. A
ordem real de `realInitializePlatform()` é:

| Passo | Linha |
|---|---|
| `new (self->hardwareStorage_) HardwareState{}` | `smart_sys_app_hardware.cpp:108` |
| `initializeNvs()` | `smart_sys_app_hardware.cpp:110` |
| `esp_read_mac()` | `smart_sys_app_hardware.cpp:117` |
| `transport`, `networkManager`, `device`, `reportExecutor` | `smart_sys_app_hardware.cpp:132-135` |
| `factoryResetService`, `resetButtonMonitor` e `start()` | `smart_sys_app_hardware.cpp:139-147` |

Todos os membros de `HardwareState` são `std::optional` (`:70-79`) e ficam
vazios entre o passo 1 e os passos 4 e 5. Se o deadline expirar nessa janela, a
ordem obrigatória da seção 6.1 manda executar `beginQuiescence` no device e
`stop` no report executor sobre `std::optional` vazios — comportamento
indefinido, não falha diagnosticável. O mesmo vale para "encerrar
`ResetButtonMonitor`, quando configurado": o monitor pode estar configurado na
intenção do produto e ainda não construído.

A janela não é teórica: `initializeNvs()` chama `nvs_flash_erase()` quando o
NVS está sem páginas livres ou com versão nova (`smart_sys_app_hardware.cpp:52-66`),
e apagar a partição é operação de flash de ordem de centenas de milissegundos a
segundos — exatamente o tipo de etapa bloqueante que motivou o deadline
independente.

É **decisão normativa ausente**, não detalhe de implementação, porque a ordem
obrigatória é contrato e hoje ela não admite passo inaplicável. A correção é
barata: declarar que cada passo da sequência é condicional à existência do
recurso correspondente neste boot, que a ausência é registrada e não é falha, e
que a sequência prossegue para o sleep. Alternativa igualmente válida e mais
conservadora: armar o deadline somente ao fim de `InitializePlatform`,
aceitando que a janela de inicialização de plataforma não seja preemptável —
mas isso contradiz a intenção de observar o prazo "mesmo enquanto a pilha
chamadora não recuperou o controle".

Recomendo a primeira, com a segunda como opção do Arquiteto.

### 3.3 Achado relacionado: a janela de `nvs_flash_erase()` não tem diagnóstico

A seção 6.1 trata a preempção de NVS apenas para `persistNetwork()` dentro de
`SetupStage::InitializeNetwork`, com o diagnóstico
`persistence_preemption_possible=true`. Com a task nascendo em
`InitializePlatform`, passa a existir uma segunda janela de NVS preemptável, a
de `initializeNvs()`, e ela é mais severa: apagar ou reinicializar a partição
inteira, e não gravar um descritor de 12 bytes.

DEEPSLEEP-AC-008B não a cobre e nenhum diagnóstico é exigido para ela. Ou o
Arquiteto estende o diagnóstico e o experimento a essa janela, ou declara
explicitamente que `InitializePlatform` não é preemptável — o que é a
alternativa conservadora de 3.2 e resolve os dois achados de uma vez. Registro
que essa segunda saída é a mais simples e a que eu escolheria se o custo de
bateria de uma janela não preemptável for aceitável para o Arquiteto.

### 3.4 Inconsistência de redação: "dona exclusiva da transição"

A seção 6 diz que a task de lifecycle é "a dona exclusiva da transição e da
sequência completa". A seção 6.1 diz que o factory reset "é aceito quando o hold
configurado se completa e a transição é adquirida", e esse caminho executa na
task do `ResetButtonMonitor`: `requestFactoryReset()` faz a limpeza e chama
`esp_restart()` no contexto do chamador (`factory_reset_service.cpp:24-50`,
chamado de `reset_button_monitor.cpp:128`).

As duas frases se contradizem na letra. A intenção legível é que a task seja
dona exclusiva da **sequência de deep sleep**, enquanto a transição é, por
definição, um token disputado por dois escritores. Um implementador que siga a
letra da seção 6 fará o token de escritor único e quebrará a arbitragem.
Precisão de uma palavra, mas material.

## 4. Espera limitada pelo deadline — confirmada

O novo passo da ordem obrigatória — "aguardar `pendingReportCount() == 0`
somente até o deadline absoluto", com conversão para o caminho forçado pela
mesma task e pelo mesmo detentor — é implementável e fecha o buraco que eu havia
registrado na v0.7. Com dono único, a espera limitada e a observação do prazo
passam a ser a mesma linha de execução, e o cenário de retry retryable
permanente (`issp154_report_executor.cpp:15-20,139-147`) deixa de derrotar
`maxAwakeTimeMs`.

Uma consequência de implementação fica registrada: a espera será por
**polling** de `pendingReportCount()`. O único ponto de notificação do device é
`setPendingReportHandler()`, que guarda um único par handler/contexto
(`issp_device.cpp:310-318`) e já está tomado pelo `Issp154ReportExecutor`
(`issp154_report_executor.cpp:57-58`). A task de lifecycle não pode registrar-se
sem deslocar o executor, e criar um segundo canal de notificação seria a quarta
ampliação que DEEPSLEEP-DEC-008 e DEEPSLEEP-DEC-009 proíbem. `pendingReportCount()`
é leitura sob seção crítica curta (`issp_device.cpp:165-171`), então o polling é
barato. Não é bloqueador, mas o período de poll passa a ser parte da precisão do
deadline e deve constar como caso de teste.

## 5. Três estados da arbitragem — confirmados

O detentor com três estados — livre, deep sleep, factory reset — e a retomada
sem reaquisição resolvem o aborto indevido que eu havia registrado. A ordem
obrigatória agora diz explicitamente "prosseguir se livre ou já detida pelo
próprio deep sleep; abortar somente se detida pelo factory reset", o que é
diretamente implementável com uma variável atômica e troca comparada, no ponto
de interceptação já identificado nas análises anteriores:
`FactoryResetService::requestFactoryReset()` antes de `cleanup_()`
(`factory_reset_service.cpp:24-50`).

Continua valendo que `requested_` é `bool` simples
(`factory_reset_service.hpp:17`) e precisa virar test-and-set atômico, e que o
re-arme do hold é local ao monitor, sobre `IFactoryResetRequester`, interface
privada em `components/issp_app_154/src/reset/` e portanto fora de
`ISSP-Reusable-Components.md`.

Uma verificação de coerência que fiz e que passa: a liberação da transição por
falha de wakeup ocorre "antes de qualquer encerramento terminal", e na ordem
obrigatória o preparo da fonte é o passo seguinte à aquisição, antes do
encerramento do monitor. Logo, ao liberar, o monitor ainda está vivo para
reapresentar o pedido — que é precisamente o que a seção 6.1 promete.

## 6. Alcance do LED — confirmado

AC-004, DEC-002, a seção 1 e a seção 4.3 agora concordam entre si e com o
código. `ValidateConfiguration` é avaliada antes de `InitializePlatform`
(`smart_sys_app.cpp:294-309`) e sua única condição de falha é
`lastConfigurationResult_ != AppResult::Ok`, alimentada por
`recordConfigurationFailure()`, de modo que a rejeição realmente antecede
qualquer toque em GPIO. A seção 6 registra a consequência — sem LED, sem task de
lifecycle e sem deep sleep nesse boot — e a qualifica corretamente como defeito
de desenvolvimento de composição constante, já que o product firmware é
compilado com valores fixos (`firmwares/door_sensor.cpp:11-20`).

Observação menor, sem ação recomendada: a seção 4.3 mantém a captura da janela
acordada "na entrada de `setup()`", que ocorre antes da validação. Como é apenas
um carimbo de tempo, não conflita com a proibição de tocar recursos.

## 7. Validação do limite real do timer — confirmada em desenho, com uma consequência

A citação de versão confere: o projeto usa ESP-IDF 6.0.1
(`client_154/sdkconfig:686`, `CONFIG_IDF_INIT_VERSION`), coerente com
`SYSTEM-DOSSIER.md:64`. O raciocínio da seção 4.2 também confere: em 64 bits o
pior caso, `UINT32_MAX` horas, dá cerca de 1,55 × 10^19 µs, abaixo do máximo de
`std::uint64_t`, cerca de 1,84 × 10^19, então o overflow aritmético não é o
limite material.

O valor do limite **não é certificável pela leitura deste repositório**: o
ESP-IDF não está vendorizado nem instalado no ambiente desta atuação, e o limite
depende da largura do temporizador de baixo consumo do ESP32-H2 e do que
`esp_sleep_enable_timer_wakeup()` aceita em 6.0.1. Fica como verificação contra
o toolchain fixado, não como experimento de hardware.

### 7.1 Consequência: intervalo fora de faixa vira dispositivo que nunca dorme

AC-003 move a rejeição do intervalo fora de faixa para o **preparo**, e a seção
4.2 determina que o erro "bloqueia a sequência antes de qualquer operação
terminal". Combinando com a regra da seção 6.1 — falha da fonte solicitada
aborta a sequência e preserva o runtime acessível — o efeito para um product
firmware com intervalo constante acima do limite é: `configureDeepSleep()`
aceita, `setup()` conclui, e o dispositivo a bateria **nunca dorme**, em todo
boot, sem que nada além do log o denuncie.

Como o intervalo é constante de compilação, como o limite do H2 é conhecido em
tempo de compilação e como a seção 4.1 já exige que configuração inválida
retorne resultado explícito, recomendo validar a faixa **também** em
`configureDeepSleep()`, mantendo a verificação no preparo como defesa em
profundidade e como tratamento do retorno da API. Isso não cria valor novo em
`AppResult` — `InvalidArgument` já existe — e converte uma falha silenciosa de
campo em uma rejeição observável na configuração.

## 8. Pontos reconfirmados sem mudança

- **Evidência positiva de report inicial.** Inalterada na v0.8 e confirmada na
  análise da v0.7: `reportOnStart` é dado da configuração já copiada;
  `DigitalOutputBehavior::begin()` publica sincronamente e a falha propaga por
  `IsspDevice::start()` até impedir `Running`; e
  `DigitalInputBehavior::hasConfirmedState()` só é verdadeiro após publicação
  requerida bem-sucedida (`digital_input_behavior.cpp:326-370`). A reavaliação
  contínua acrescentada à seção 6 é justamente o que o classificador periódico
  torna útil;
- **`stop()` em 600 ms e supressão de `end()`.** Os 465 ms conferem com
  `kPhysicalTxTimeoutMs`, `kReportAckTimeoutMs` e `kConfirmedSendBackoffMs`, e
  `end()` desinicializa o rádio antes de destruir o event group
  (`issp154_transport.cpp:390-399`). Continuam valendo as três obrigações
  registradas na análise da v0.7: consultar a flag de parada antes de iniciar
  nova tentativa, desbloquear as duas esperas da task e expor sinal observável
  de término;
- **`quiesce()` no `DigitalInputBehavior`.** É implementável reusando
  `stopAndDeleteTimer()` (`digital_input_behavior.cpp:91-104`), que já zera
  `timer_` e portanto é idempotente e mantém o destrutor seguro. A ordem da
  seção 6.1 — `beginQuiescence` antes de `quiesce` — é o que torna segura uma
  amostragem periódica que dispare durante a parada: a publicação tardia recebe
  `NotReady` e não ocupa slot. A ordem não é preferência de estilo;
- **Composição e renome.** `wake_led` entra como item das listas
  `required_resources` e `offered_resources`
  (`client_154/main/CMakeLists.txt:15-49`); GPIO 13 permanece livre na
  `Door Sensor Battery H2`, que usa 14 e 9
  (`boards/door_sensor_battery_h2.cpp:12-21`); o `board_model.hpp` ganha o
  recurso e o acessor seguindo o precedente existente
  (`boards/board_model.hpp:29-42`); e as referências do renome conferem
  (`Kconfig.projbuild:16`, `client_154/sdkconfig:949`, linha comentada);
- **Comando em voo no instante de `beginQuiescence()`.** Observação da análise
  da v0.7 não invalidada: o GPIO pode ser alterado antes de a publicação ser
  recusada, e o coordenador recebe `Failed` com o efeito já aplicado. Não é
  bloqueador e não afeta o primeiro produto a bateria, que não recebe comandos.

## 9. Componentes impactados

| Área | Impacto |
|---|---|
| API pública `SmartSysApp` | `configureDeepSleep()`, validação de faixa do intervalo (7.1), causa de boot, LED, deadline e sequência |
| `SetupHooks` | costura interna declarada por DEEPSLEEP-AC-010 para lifecycle, validação e falhas |
| `issp_core` | `IDeviceBehavior::quiesce()` e `IsspDevice::beginQuiescence()` |
| `issp_behaviors` | override de `quiesce()` reusando `stopAndDeleteTimer()`; comentário de `hasConfirmedState()` |
| `issp_transport_154` | `Issp154ReportExecutor::stop()`, retry notificável, flag de parada e sinal de término |
| `issp_app_154` | task privada de lifecycle, guardas de existência dos recursos (3.2), token de três estados, parada notificável do monitor e re-arme do pedido rejeitado |
| Product firmware | opt-in, política temporal, hold do factory reset e renome integral |
| Board model e CMake | recurso `wake_led`, GPIO, polaridade e composição |
| Kconfig e `sdkconfig` | símbolo, rótulo e artefato versionado |
| Testes | doubles de RTC, GPIO e sleep; deadline durante `InitializePlatform`; conversão antecipado→forçado; três estados da arbitragem |

## 10. Restrições confirmadas

- ESP32-H2 é o único target físico do `client_154`; QEMU não é admitido;
- nenhuma execução é autorizada por esta especificação nem por esta atuação;
- Kconfig não governa lógica interna de componentes compartilhados;
- `persistNetwork()` permanece privado e inobservável pela fachada;
- o device oferece um único registro de notificação de report pendente, já
  tomado pelo executor, o que torna o polling a única espera admissível no
  recorte autorizado (4);
- `app_main()` retorna após `setup()`, de modo que a task privada de lifecycle é
  a única hospedeira possível da reavaliação e do deadline.

## 11. Experimentos e verificações necessários

Permanecem válidos os itens registrados nas análises da v0.5 à v0.7, com estas
alterações e acréscimos:

- **10 (preempção de NVS)** ganha meio admissível declarado pela seção 6.1 e
  passa a ser provocável de forma controlada;
- **14 e 15** da análise da v0.7 permanecem, agora como confronto da conversão
  antecipado→forçado dentro da mesma task e do mesmo detentor;
- **16 (verificação de toolchain):** limite aceito por
  `esp_sleep_enable_timer_wakeup()` no ESP32-H2 com ESP-IDF 6.0.1, para
  dimensionar AC-003 e a validação recomendada em 7.1. Não é experimento de
  hardware;
- **17 (novo):** deadline expirando durante `InitializePlatform`, antes da
  construção de device, executor e monitor, confrontando as guardas de
  existência exigidas por 3.2;
- **18 (novo):** deadline expirando durante `nvs_flash_erase()` no caminho de
  reinicialização do NVS, confrontando a integridade da partição e o boot
  seguinte (3.3). Só se aplica se o Arquiteto mantiver `InitializePlatform`
  preemptável.

Leitura de código não certifica nenhum desses fatos.

## 12. Recomendação

Recomenda-se **prontidão condicionada**. As cinco confirmações pedidas pela
seção 8 se sustentam: o dono único é a única hospedagem viável e resolve a
concorrência da v0.7; a espera limitada pelo deadline elimina o caminho em que
um retry permanente derrotava `maxAwakeTimeMs`; os três estados da arbitragem
tornam a retomada implementável e continuam cabendo em código privado da
fachada; o alcance do LED ficou consistente entre seção 1, 4.3, 6, AC-004 e
DEC-002; e a validação do timer passou a mirar o limite material correto, com a
versão do ESP-IDF conferida contra o repositório.

Antes da implementação, um ponto exige decisão do Arquiteto:

1. **passos inaplicáveis da ordem obrigatória** (3.2 e 3.3): a task nasce em
   `InitializePlatform`, antes de device, executor e monitor existirem, e a
   ordem obrigatória não admite passo ausente. Ou cada passo passa a ser
   condicional à existência do recurso neste boot, ou `InitializePlatform`
   deixa de ser preemptável. A segunda saída resolve junto a janela de
   `nvs_flash_erase()`, que hoje é preemptável sem diagnóstico e sem cobertura
   em DEEPSLEEP-AC-008B.

E dois pontos de precisão, ambos de custo baixo:

2. **"dona exclusiva da transição"** (3.4): a task é dona exclusiva da
   sequência; a transição é token disputado por dois escritores, e a letra atual
   induz um token de escritor único que quebraria a arbitragem;
3. **faixa do intervalo também na configuração** (7.1): validar em
   `configureDeepSleep()` contra o mesmo limite, mantendo a verificação do
   preparo. Sem isso, um intervalo constante fora de faixa produz um dispositivo
   a bateria que nunca dorme, denunciado apenas por log.

Fica registrada ainda a consequência de implementação em 4 — a espera de entrega
será por polling, porque o único registro de notificação do device já pertence
ao executor — e as observações não invalidadas das análises anteriores. Builds,
testes e hardware permanecem `Not Executed`.
