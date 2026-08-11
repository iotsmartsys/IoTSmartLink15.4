# Análise de verificação da v0.3 — deep sleep do client

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/Client-Deep-Sleep.md`

**Revisão confrontada:** v0.3, Draft de 11/08/2026

**Estado:** Concluído

**Capacidade:** Engenheiro Analista com análise de verificação

**Data:** 11/08/2026

**Resultado:** Prontidão condicionada a três decisões normativas do Arquiteto;
nenhuma execução realizada

> Este relatório registra evidências e recomendações. Não altera a fonte
> normativa, não promove estado e não autoriza implementação ou testes.

Esta atuação reconfronta a especificação v0.3 com o repositório atual e verifica
cada evidência do relatório `2026-08-11-initial-analysis.md`. O recorte foi
autorizado pelo Arquiteto como análise de verificação. Nenhum build, teste ou
hardware foi executado; a árvore estava limpa na branch `spec/client-deep-sleep`.

## 1. Verificação das evidências do relatório inicial

| Evidência do relatório inicial | Verificação |
|---|---|
| `SmartSysApp` é a fachada pública e aceita configuração antes de `setup()` | Confirmada — `components/issp_app_154/include/SmartSysApp.h:133` |
| Product firmware combina regra própria com recurso do board | Confirmada — `client_154/main/firmwares/door_sensor.cpp:43` |
| CMake valida recursos exigidos e oferecidos | Confirmada — `client_154/main/CMakeLists.txt` (`required_resources`/`offered_resources`) |
| `Door Sensor Battery H2` não declara recurso de LED | Confirmada — oferece apenas `dry_contact_input` e `user_button` |
| `setup()` termina em `Running` após iniciar o report executor | Confirmada — `SetupStage::StartReportExecutor` e `AppState::Running` |
| `Issp154ReportExecutor::run()` é task sem parada, com retry e delay de 1000 ms | Confirmada — `components/issp_transport_154/src/issp154_report_executor.cpp:129-155` |
| `IsspDevice` não demonstra sozinho ausência de reserva ou transmissão | **Divergente; ver 2.1** |
| `Issp154Transport::end()` existe e é usado em rollback | Confirmada — `components/issp_transport_154/src/issp154_transport.cpp:373` |
| Especificação vigente de `SmartSysApp` nega `stop()` e exige lifetime até reboot | Confirmada e mais restritiva do que o registrado; ver 2.2 |
| Sleep exposto pelo transporte é sleep de rádio, não deep sleep do SoC | Confirmada |

## 2. Achados desta verificação

### 2.1 A contagem de reports pendentes já distingue entrega

`IsspDevice::pendingReportCount()` conta slots ocupados **inclusive** os que
estão `inFlight`; o decremento ocorre somente em `completePendingReport()` com
`delivered=true` (`components/issp_core/src/issp_device.cpp:238-268`).
`peekPendingReport()` ignora reservados, mas a contagem não. Logo
`pendingReportCount() == 0`, avaliado depois de estabilizar os producers, já é um
oráculo suficiente para "todos os reports do ciclo entregues, sem report
reservado nem transmissão em andamento" exigido por DEEPSLEEP-DEC-001 e
DEEPSLEEP-AC-006. O relatório inicial subestimou o contrato existente. Restrições
que permanecem:

- a condição só é estável depois que os producers pararem de publicar; o
  `DigitalInputBehavior` publica por `esp_timer` e possui `stopAndDeleteTimer()`,
  o que torna a estabilização possível sem contrato novo;
- entrega com falha não retryable mantém o slot ocupado
  (`issp154_report_executor.cpp:148-151` interrompe o laço), de modo que o
  encerramento antecipado nunca ocorre nesse caso e o sleep só acontece por
  `maxAwakeTimeMs` — comportamento compatível com a v0.3, mas que deve ser
  coberto por caso de teste;
- a contagem não observa recepção de comando em curso; nada na v0.3 exige isso.

### 2.2 Conflito entre especificações Active sobre encerramento e ciclo de vida

`docs/specs/ISSP-Configurable-Bootstrap.md` (Active, Validated, v1.5) é a fonte
normativa da fachada e:

- exclui explicitamente "criar novo ciclo de vida com `stop()` ou retry de
  `setup()`" (§5, linha 119);
- fixa em SMARTAPP-DEC-004A que não existe `stop()` operacional capaz de
  encerrar e aguardar as tasks de `Issp154ReportExecutor` e `ResetButtonMonitor`,
  e que "uma evolução que permita destruição depois de `setup()` exigirá contrato
  explícito de parada e sincronização das tasks" (linhas 243-267);
- declara falha posterior ao início do device terminal até reboot "pois o device
  não possui contrato de `stop()`" (linha 661).

A v0.3 exige, após `Running`, "estabilizar producers, encerra tasks e transporte"
(§5) e o critério DEEPSLEEP-AC-006 depende disso. Isso não é apenas trabalho
técnico, como registrou o relatório inicial: é relação normativa entre duas
especificações, e a especificação de menor precedência não pode assumi-la por
inferência. Diferenciação exigida:

- **decisão normativa ausente:** qual especificação governa o encerramento e em
  que termos — `Client-Deep-Sleep` derroga o item de fora de escopo do
  `ISSP-Configurable-Bootstrap`, ou este é emendado pelo Arquiteto;
- **escolha normal de implementação:** o mecanismo de parada (flag terminal,
  notificação, espera limitada) desde que a autorização normativa exista.

Observação material que reduz o risco: deep sleep reinicia o firmware, então
nenhum objeto é destruído e o lifetime estático de SMARTAPP-DEC-004A é
preservado. O que a v0.3 exige é quiescência limitada antes do sleep, não
destruição. Um contrato de "quiescência sem `stop()` público" é a leitura mais
próxima do precedente; ainda assim depende de decisão explícita.

### 2.3 A API pública nova altera uma especificação Active

`configureDeepSleep()` amplia a API pública descrita por
`ISSP-Configurable-Bootstrap.md`. A v0.3 propõe a assinatura, mas não declara a
relação entre as duas especificações (extensão, emenda ou sucessão), nem onde
`SetupStage`/`AppResult` acomodam as novas falhas de preparação e sleep. Sem essa
declaração, o Implementador teria de escolher a fronteira normativa sozinho.

### 2.4 Ausência de dono do orçamento acordado

`client_154/main/app_main.cpp` retorna logo após `setup()`; não existe laço de
aplicação. Portanto `maxAwakeTimeMs`, contado "desde o início de `setup()`" e
limitando "também commissioning e o estado `NotReady`" (§5), precisa de um dono
dentro da fachada. O risco é que `setup()` bloqueia na descoberta de rede e em
NVS: na expiração, o contexto que detecta o prazo é distinto do contexto
bloqueado, e entrar em deep sleep a partir do contexto do `esp_timer` enquanto a
task principal está dentro de rádio ou NVS não é comprovável por leitura. Não é
bloqueador de contrato, mas é o principal risco técnico da implementação e exige
experimento antes de aceitar DEEPSLEEP-AC-006 como validável.

### 2.5 Ordem do indicador em relação a `setup()`

§3.1 determina que "nenhum recurso é iniciado antes de `setup()`", enquanto o
fluxo da §5 posiciona "acionar LED em todo boot" antes de "executar setup". As
duas leituras são conciliáveis se o acionamento for o primeiro passo interno de
`setup()`, mas o texto admite também acionamento anterior à chamada. É
ambiguidade normativa pequena e de correção barata, não bloqueador.

### 2.6 Impacto de composição maior do que o registrado

O renome de `door_sensor` para `door_sensor_battery_h2` (DEEPSLEEP-DEC-005)
atinge fontes não listadas no relatório inicial:

- `client_154/main/Kconfig.projbuild:16` — símbolo
  `IOTSMARTLINK154_PRODUCT_DOOR_SENSOR` e rótulo "Door sensor";
- `client_154/main/CMakeLists.txt` — ramo do produto, `selected_product` e
  `required_resources`;
- `docs/specs/Firmware-Variants-Menuconfig.md` (Active) — nome da variante,
  tabela de arquivos e cenários de aceite citam "Door sensor" em pelo menos oito
  pontos (linhas 76, 101, 109, 262, 349, 371, 396, 419, 459, 665);
- `docs/specs/SYSTEM-DOSSIER.md:22`.

A v0.3 não declara se o renome atinge o símbolo Kconfig e o rótulo do menu ou
apenas o arquivo de firmware. Como o rótulo é contrato de composição na
especificação de variantes, a decisão é do Arquiteto.

O recurso `wake_led` exige ainda: novo tipo em
`client_154/main/boards/board_model.hpp` com acessor próprio, oferta em
`offered_resources` do board e exigência em `required_resources` do produto,
seguindo o precedente de `dry_contact_input`. Verificado que o GPIO 13 está livre
na `Door Sensor Battery H2` — hoje só usa GPIO 14 (contato seco) e GPIO 9 (botão)
— portanto DEEPSLEEP-AC-005 não colide com a composição atual.

## 3. Componentes impactados — atualização

A tabela do relatório inicial permanece válida. Acrescentam-se:

| Área | Impacto adicional |
|---|---|
| `ISSP-Configurable-Bootstrap.md` | Fronteira de `stop()`/quiescência e API pública ampliada |
| `Firmware-Variants-Menuconfig.md` | Renome da variante e do rótulo de menu |
| Kconfig e `SYSTEM-DOSSIER.md` | Símbolo, rótulo e inventário de variantes |
| `SetupHooks` do test seam | Novos hooks para RTC, GPIO e sleep, se a evidência lógica os exigir |

## 4. Restrições confirmadas

- somente ESP32-H2 é target físico do `client_154`; QEMU não é admitido;
- nenhuma execução de build, teste ou hardware é autorizada por esta
  especificação ou por esta atuação;
- Kconfig não governa lógica de componentes compartilhados; o renome não pode
  introduzir símbolo lido por componente compartilhado;
- o `ResetButtonMonitor` também mantém task e configura pull no GPIO do botão;
  qualquer encerramento coordenado deve considerá-lo, e a configuração elétrica
  remanescente influencia corrente em sleep.

## 5. Experimentos necessários

Mantêm-se os cinco experimentos do relatório inicial. Acrescentam-se:

- expiração de `maxAwakeTimeMs` durante `setup()` bloqueado em descoberta de
  rede, para confrontar 2.4;
- falha não retryable de report, para confrontar a permanência do slot ocupado e
  o caminho de sleep forçado descrito em 2.1;
- medição de corrente com o pull do botão e demais GPIOs no estado deixado pelo
  encerramento, já que a v0.3 não normatiza retenção.

Leitura de código não certifica nenhum desses fatos.

## 6. Recomendação

Recomenda-se ao Arquiteto **resolver três pontos antes de promover a
especificação**:

1. a relação normativa com `ISSP-Configurable-Bootstrap.md` quanto a encerramento
   e ampliação da API pública (2.2 e 2.3);
2. o alcance do renome da variante sobre Kconfig, rótulo de menu e
   `Firmware-Variants-Menuconfig.md` (2.6);
3. a ordem do acionamento do LED em relação a `setup()` (2.5).

Nenhum dos três é bloqueador de viabilidade técnica: as fronteiras atuais
suportam o contrato, e o achado 2.1 mostra que o oráculo de entrega já existe.
São decisões de autoridade normativa que não cabem ao Analista nem ao
Implementador. O risco técnico dominante permanece o dono do orçamento acordado
(2.4), a ser confrontado por experimento. Builds, testes e hardware permanecem
`Not Executed`.
