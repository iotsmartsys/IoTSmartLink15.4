# Relatório de implementação — variantes de firmware

**Classe da fonte:** Relatório

**Papel:** Engenheiro Implementador

**Especificação:** `docs/specs/Firmware-Variants-Menuconfig.md`

**Revisão confrontada:** Registro histórico EKM 1.x preservado na migração para EKOM 3.2

**Estado:** Concluído

> Este relatório preserva uma atuação histórica e não altera fontes normativas.

## Resultado da implementação da Fase 1 (Engenheiro Implementador)

**Estado da Fase 1:** Implementação concluída. Código, validações automatizáveis
e a validação em hardware ESP32-H2 exigida pelos critérios de preservação estão
executados; o Arquiteto declarou a execução em hardware aceitável.

A especificação integral permanece Em andamento [`In Progress`]: a Fase 2 e o
teste 3 da estratégia EKOM continuam sem implementação e sem evidência. A
conclusão da Fase 1 não representa a especificação integral como implementada.

### Estrutura implementada

```text
client_154/main/
├── Kconfig.projbuild                        menu IoTSmartLink15.4
├── CMakeLists.txt                           traduz a escolha em fontes
├── app_main.cpp                             entrypoint mínimo
├── product_firmware.hpp                     contrato de seleção
├── firmwares/single_smart_plug.cpp          composição do produto atual
└── boards/
    ├── board_model.hpp                      contrato elétrico
    └── current_client_esp32h2_wiring.cpp    fiação atual, somente ESP32-H2
```

`client_154/main/main.cpp` foi removido; nenhum arquivo de variante ou board não
suportado foi criado. `client_154/CMakeLists.txt` e os componentes `issp_*` não
foram alterados.

### Decisões de implementação

- o contrato de seleção é uma única função livre,
  `client154::startSelectedProductFirmware()`, que devolve o `SetupResult` da
  fachada. É o menor mecanismo local que satisfaz o entrypoint mínimo e a
  decisão 11; nenhuma classe base, registro ou abstração transversal foi criada;
- `client154::BoardModel` transporta apenas pino e polaridade do relé e do botão
  de reset. Tempo de retenção do reset, identidade, endpoint, tipo de evento,
  estado inicial e report inicial permanecem no product firmware. Essa forma e
  seus nomes orientados ao produto são provisórios da Fase 1, conforme a decisão
  13;
- a seleção do board depende de `IDF_TARGET_ESP32H2` no `Kconfig`; quando nenhum
  board compatível existe para o target, `main/CMakeLists.txt` emite
  `FATAL_ERROR` nomeando o target e a incompatibilidade. O board ainda carrega um
  `#error` defensivo para o caso de ser compilado por outro caminho;
- as verificações de seleção ficam dentro de `if(NOT CMAKE_BUILD_EARLY_EXPANSION)`
  porque os símbolos do próprio menu ainda não existem na expansão inicial de
  requisitos do ESP-IDF;
- o `TAG` de log do entrypoint passou de `iot154_switch` para `iot154_client`,
  já que o entrypoint deixou de pertencer a um produto. As duas mensagens
  (`ISSP runtime did not start...` e `ISSP runtime started`) e a sequência
  configuração → `setup()` foram preservadas literalmente;
- as validações desta implementação usaram builds isolados por `-DSDKCONFIG` e
  não alteraram configuração versionada. Posteriormente,
  `client_154/sdkconfig` recebeu os símbolos default de produto e board e passou
  a representar a configuração H2 vigente.

### Evidências executadas

ESP-IDF v6.0.1, builds isolados fora da árvore do repositório.

| Item do conjunto de validação | Resultado |
|---|---|
| `git diff --check` | sem erro; diff restrito a `client_154/main/` |
| configuração gerada contém exatamente o produto e o board default | `CONFIG_IOTSMARTLINK154_PRODUCT_SINGLE_SMART_PLUG=y` e `CONFIG_IOTSMARTLINK154_BOARD_CURRENT_CLIENT_ESP32H2_WIRING=y` |
| somente as fontes selecionadas entram no build | `compile_commands.json` e `build.ninja` contêm apenas `app_main.cpp`, `firmwares/single_smart_plug.cpp` e `boards/current_client_esp32h2_wiring.cpp` |
| build `client_154` ESP32-H2 | sucesso, 0 warnings |
| build `examples/issp_minimal_client` ESP32-H2 | sucesso, 0 warnings |
| testes `SmartSysApp` em QEMU ESP32-C3 | 20/20 `PASS`, 0 `FAIL` |
| build `coordinator_154` ESP32-C6 | sucesso, 0 warnings |
| build isolado `client_154` ESP32-H2 após manutenção pré-Fase 2 | sucesso com `-DSDKCONFIG` temporário; produto e board default selecionados, sem alterar `client_154/sdkconfig` |
| caso negativo board/target isolado após manutenção pré-Fase 2 | build ESP32-C6 com `-DSDKCONFIG` temporário falha na configuração com “No board model selected in the IoTSmartLink15.4 menu for IDF_TARGET=esp32c6…”, sem produzir binário e sem alterar `client_154/sdkconfig` |
| ausência de `CONFIG_*` de produto ou board em `components/issp_*` | confirmada por varredura; o único uso remanescente é `CONFIG_IDF_TARGET_*` do ESP-IDF |
| validação em hardware ESP32-H2 | executada pelo Arquiteto e declarada aceitável |

#### Validação em hardware ESP32-H2

Executada pelo Arquiteto em placa física e declarada aceitável por ele, que
detém a autoridade sobre a suficiência das evidências. O Implementador não
operou o hardware; registra abaixo o que o log entregue comprova diretamente e o
que se apoia na observação direta do Arquiteto.

Binário observado: ESP-IDF v6.0.1, projeto `sensor_154` — nome de projeto
preexistente do `client_154`, não um firmware de outro produto —, versão de app
`ff3a003`, o commit da Fase 1. Entre `ff3a003` e a versão vigente houve apenas
documentação e a gravação, em `client_154/sdkconfig`, dos dois símbolos default
de produto e board, que o `Kconfig` já aplicava por padrão no build validado. O
binário validado é, portanto, materialmente equivalente ao vigente.

Comprovado diretamente pelo log:

- boot até `Running` pelo caminho novo: `app_setup begin capabilities=1
  factory_reset=configured`, estágios 1 a 6 e `app_setup completed
  state=running`;
- entrypoint mínimo emitindo `iot154_client: ISSP runtime started`, com a
  sequência configuração → `setup()` preservada;
- report inicial com os valores de baseline: `DIGITAL_OUTPUT_START:
  initial_report endpoint=1 event=2 value=0 result=0`, isto é, endpoint 1,
  event type 2, estado inicial desligado e report inicial habilitado;
- factory reset configurado com os valores de baseline: `RESET_BUTTON:
  initialized gpio=9 hold_ms=10000`;
- commissioning persistido recarregado e rádio ativo, com duas recepções
  posteriores validadas em `COMMAND_ORIGIN`.

Apoiado na observação direta do Arquiteto, fora do trecho de log entregue: os
comandos `ON`, `OFF` e `TOGGLE`, a pressão de factory reset por 10 segundos, o
reboot e o retorno ao commissioning. O device ID `0x15400001` e o relé no GPIO
13 ativo em nível alto também não aparecem no trecho; permanecem comprovados
apenas pela inspeção estática de `firmwares/single_smart_plug.cpp` e
`boards/current_client_esp32h2_wiring.cpp` contra a decisão 8.

Esta execução comprova a preservação da migração e não declara resolvida a
lacuna preexistente de ACK/retry (`EKM-GAP-0006`).

O conjunto de testes automatizados de `SmartSysApp` possui hoje 20 casos, não os
19 registrados quando a especificação foi escrita; o arquivo de teste não foi
alterado por esta atuação. Sob QEMU, o app de teste continua exibindo o resumo
`0 Tests 0 Failures` do Unity e um panic após o retorno de `app_main()`; ambos
são comportamentos preexistentes do app de teste e não decorrem desta mudança. O
resultado material são os 20 casos executados individualmente com `PASS`.

### Pendências desta implementação

- o teste 3 da estratégia EKOM permanece sem evidência até o sensor de porta da
  Fase 2 ser implementado e validado;
- `EKM-GAP-0006` permanece aberta e não é afetada por esta mudança.

### Manutenção concluída antes da Fase 2

- `client_154/sdkconfig` permanece como a única configuração rastreada e como a
  configuração H2 vigente; as três cópias inertes foram removidas;
- os builds de validação H2 e do caso negativo C6 usam `SDKCONFIG` temporário,
  sem reescrever a configuração rastreada;
- a pinagem deixou de ser duplicada no help do Kconfig e permanece somente no
  arquivo do board;
- o arquivo do board inclui `sdkconfig.h` explicitamente;
- a varredura dos consumidores conhecidos no código-fonte local sob
  `/source/IoT` não encontrou ferramenta de host que filtre pela antiga tag
  `iot154_switch`; as únicas ocorrências remanescentes estão nesta especificação
  e em um worktree histórico, sem participação no build atual.

## Resultado da implementação da Fase 2 (Engenheiro Implementador)

**Estado da Fase 2:** atuação do Engenheiro Implementador encerrada e entregue
para revisão. A implementação estrutural está concluída; validação comportamental
automatizada e validação em hardware permanecem pendentes. O estado integral
continua `In Progress` e esta seção não declara aprovação.

### Composição implementada

- `menuconfig` oferece duas escolhas exclusivas de produto — Single smart plug e
  Door sensor — e duas escolhas exclusivas de board ESP32-H2;
- os defaults do `Kconfig` continuam sendo Single smart plug e o board atual. O
  `client_154/sdkconfig` vigente registra a seleção feita pelo Arquiteto para a
  próxima validação: Door sensor + Door Sensor Battery H2;
- `main/CMakeLists.txt` compila somente o product firmware e o board
  selecionados. Produto declara recursos requeridos, board declara recursos
  oferecidos e uma composição incompatível falha na configuração nomeando
  produto, board e recurso ausente;
- `BoardModel` passou a expor recursos físicos (`DigitalOutputResource`,
  `DryContactInputResource` e `UserButtonResource`), sem nomes ligados a um
  produto. O board atual preserva GPIO 13 ativo alto e botão GPIO 9 ativo baixo;
  o Door Sensor Battery H2 oferece contato seco no GPIO 14 ativo alto com
  pull-up e o mesmo botão;
- `firmwares/single_smart_plug.cpp` foi migrado mecanicamente para os recursos
  físicos, sem alterar seus valores de baseline;
- `firmwares/door_sensor.cpp` compõe device `0x15400001`, endpoint 1, evento 1,
  report inicial, contato seco e factory reset de 10 segundos.

### Plataforma compartilhada implementada

- `DigitalInputBehavior` encapsula entrada digital, polaridade, pull, report
  inicial e transições. O debounce usa amostras a cada 10 ms, janelas não
  sobrepostas de cinco amostras, maioria de três e confirmação por duas janelas
  consecutivas;
- a estabilização inicial é síncrona e o monitoramento periódico usa
  `esp_timer` no dispatcher de tarefa, sem criar uma pilha ou tarefa por
  behavior;
- a junção de teste permite fornecer cada nível e avançar uma amostra
  explicitamente; a cadência real do timer possui um caso separado;
- `SmartSysApp` recebeu `DoorSensorConfig`, `DoorSensorCapability` e um registro
  unificado que preserva a ordem de inclusão e rejeita endpoint/evento duplicado
  também entre tipos de capability;
- `IsspDevice` serializa publicação, reserva, consulta e conclusão de pending
  reports, além do estado de processamento de comando. Codificação, transporte
  e callbacks permanecem fora da seção crítica;
- os componentes compartilhados continuam sem símbolos
  `CONFIG_IOTSMARTLINK154_PRODUCT_*` ou `CONFIG_IOTSMARTLINK154_BOARD_*`.
- os dois novos test apps ESP-IDF vinculam-se explicitamente ao ESP32-H2 e
  incluem o guard comum de targets antes de gerar qualquer binário.

### Evidências obtidas sem executar suítes

As verificações abaixo respeitam `Repository-Test-Execution-Policy.md`: nenhum
QEMU, `pytest`, flash, monitor ou suíte Unity foi executado.

| Verificação | Resultado |
|---|---|
| build Single smart plug + board atual, ESP32-H2 | sucesso, 0 warnings, 252896 bytes |
| build Door sensor + Door Sensor Battery H2, ESP32-H2 | sucesso, 0 warnings, 253120 bytes |
| build `examples/issp_minimal_client`, ESP32-H2 | sucesso, 0 warnings |
| build `coordinator_154`, ESP32-C6 | sucesso, 0 warnings |
| Door sensor + board atual | configuração rejeitada por ausência de `dry_contact_input`; nenhum firmware gerado |
| Single smart plug + Door Sensor Battery H2 | configuração rejeitada por ausência de `digital_output`; nenhum firmware gerado |
| qualquer um dos dois boards com target ESP32-C6 | configuração rejeitada pelo vínculo client→H2; nenhum firmware gerado |
| seleção de fontes | `build.ninja` contém somente o product firmware e o board selecionados em cada composição |
| fronteira dos componentes | nenhuma seleção de produto/board em `components/issp_*`; protocolo, transporte e coordenador sem alteração |
| apps de teste em ESP32-H2 | SmartSysApp, DigitalInputBehavior e concorrência de IsspDevice compilam com 0 warnings |
| integridade textual | `git diff --check` sem erro |

Foram preparados 36 casos automatizados: 24 de SmartSysApp — os 20 casos de
baseline preservados mais quatro casos de porta/registro —, oito de
DigitalInputBehavior e quatro de concorrência de IsspDevice. Todos compilam para
ESP32-H2 e permanecem `Not Executed` por TESTEXEC-009.

As referências a QEMU ESP32-C3 na evidência histórica da Fase 1 não definem mais
uma estratégia vigente. A política `Repository-Test-Execution-Policy.md`
classifica ESP32-C3 como alvo inválido para este repositório; somente ESP32-H2 e
ESP32-C6 são admitidos, e execução de suítes não deve ocorrer sem autorização em
uma especificação futura.

### Limitações e validações pendentes

- `esp_timer_stop()` e `esp_timer_delete()` não constituem barreira contra uma
  callback que já esteja em voo. Em produção, os behaviors têm vida igual à do
  runtime; o teste de destruição pode comprovar ausência de callbacks posteriores,
  mas não eliminar uma janela já iniciada;
- os testes preparados para concorrência exercitam a máquina de estados e as
  interações entre tarefas, mas a suficiência da exclusão mútua depende de sua
  execução e revisão;
- os casos automatizados não foram executados. O Arquiteto declarou a composição
  Door sensor + Door Sensor Battery H2 funcional em hardware; essa declaração
  não discrimina todos os cenários do conjunto de validação nem cobre A1, a
  instrumentação de A2 ou a cadência de A3;
- o teste 3 do experimento EKOM está estruturalmente materializado por duas
  variantes reais, mas seu encerramento e a avaliação de manutenibilidade
  permanecem decisões do Arquiteto após as evidências pendentes.

## Resultado da implementação corretiva (Engenheiro Implementador)

**Estado desta correção:** implementação escrita e compilada; os 39 casos
automatizados permanecem `Not Executed` por TESTEXEC-009. A Fase 2 e o estado
integral continuam `In Progress` e esta seção não declara aprovação nem
substitui a revisão focada prevista.

Recorte executado: decisões 32 a 39, limitado a `DigitalInputBehavior`, seus
casos automatizados, aos casos afetados de `SmartSysApp`, a
`client_154/sdkconfig` e à reconciliação documental. Protocolo, transporte,
commissioning, coordenador, composição de produto, recursos do board e
`SetupHooks` não foram alterados.

### Correções implementadas

- **A1 (decisão 32).** `beginTimerBacked()` deixou de retornar `Failed` quando
  as duas janelas síncronas divergem. Ele registra
  `initial_stabilization_pending`, arma o `esp_timer` periódico e retorna `Ok`,
  preservando a classificação já observada; o primeiro par consecutivo
  confirmado publica um único report inicial. Falha ao criar o timer, período
  de zero tick e falha ao armar o timer continuam sendo falha de
  inicialização, com o mesmo desfazimento anterior;
- **A2 (decisão 33).** O behavior marca o instante da primeira amostra que
  diverge do estado confirmado, mantém essa marca enquanto a tentativa de
  transição não for descartada por duas classificações consecutivas do estado
  anterior e a descarta ao confirmar. O log de transição passou a ser
  `transition_report endpoint=… event=… value=… first_divergence_us=…
  confirmed_us=… latency_upper_ms=…`, com o limite superior calculado como
  `confirmação − primeira divergência + samplePeriodMs`. O log do report
  inicial permanece com o texto anterior. A instrumentação não toca protocolo,
  fila, ACK, retry nem coordenador;
- **A3 (decisão 34).** Novo caso `the periodic timer samples at the configured
  10 ms cadence`, que arma o timer real, mede os intervalos entre amostras com
  `esp_timer_get_time()`, exige ao menos onze intervalos e média entre 9 ms e
  11 ms, e registra o maior intervalo observado sem usá-lo como oráculo de
  debounce. As amostras da estabilização síncrona são descartadas do cálculo
  para que o caso meça somente a cadência do timer;
- **A7 (decisão 35).** `hasConfirmedState_` e `confirmedState_` foram
  substituídos por um único `std::atomic<std::uint8_t>` com os valores
  desconhecido/inativo/ativo. Leitor e callback observam uma palavra coerente
  sem data race. A API pública do behavior e do capability não mudou e a seção
  crítica de `IsspDevice` não foi ampliada;
- **A8 (decisão 36).** `addDoorSensorCapability validates pin and debounce` foi
  separado em `rejects an invalid pin` e `rejects an invalid debounce
  configuration`, cada um violando exatamente uma regra. O caso de registro foi
  renomeado para `setup registers both capabilities of the unified registry` e
  afirma somente `Running` e duas chamadas de registro; as asserções de posição
  na sequência de estágios foram removidas por não discriminarem o tipo de
  capability. Nenhuma junção nova foi criada e `SetupHooks` permanece
  inalterado;
- **A9 (decisão 37).** `client_154/sdkconfig` volta a selecionar
  `CONFIG_IOTSMARTLINK154_PRODUCT_SINGLE_SMART_PLUG=y` e
  `CONFIG_IOTSMARTLINK154_BOARD_CURRENT_CLIENT_ESP32H2_WIRING=y`. Os defaults
  do `Kconfig` não mudaram e nenhuma outra chave do arquivo foi alterada;
- **A4, A5 e A6 (decisões 38 e 39).** Preservados. `skip_unhandled_events =
  true` continua no `esp_timer_create_args_t`; `publishReport()` mantém a
  reserva de sequência antes da codificação; `kImplStorageBytes` continua em
  10240 com os slots fixos por tipo e o `static_assert` vigente.

### Decisões locais de implementação

- **falha de publicação durante `begin()` deixou de abortar a inicialização.**
  A decisão 32 mantém como falha de inicialização apenas a criação e o
  armamento do timer e determina que falha ao publicar não confirme o estado e
  possa ser tentada novamente pelo classificador. A tentativa síncrona passou a
  registrar `initial_report failed result=…` e a prosseguir; como o
  classificador já mantém a contagem saturada em `consecutiveWindows`, a
  próxima janela com a mesma classificação tenta publicar de novo. Falha de
  leitura do nível — retorno fora de `{0,1}` — continua abortando `begin()`,
  por ser defeito de leitura da entrada e não divergência de estabilização;
- **separação entre ler e classificar.** `sampleCurrentLevel()` foi dividido em
  `readLevel()` e `processSample()` para que a tentativa síncrona distinga esses
  dois casos sem duplicar a máquina de debounce;
- **descarte da marca de divergência.** A marca é limpa quando uma
  classificação igual ao estado confirmado atinge `consecutiveWindows`, isto é,
  quando a tentativa de transição é efetivamente descartada, e também ao
  confirmar um novo estado.

### Evidências obtidas sem executar suítes

Ambiente: ESP-IDF v6.0.1 (`v6.0.1-dirty`), toolchain `riscv32-esp-elf-gcc`
15.2.0, macOS. Todos os builds usaram diretório e `SDKCONFIG` isolados fora da
árvore do repositório; nenhum QEMU, `pytest`, flash, monitor ou suíte Unity foi
executado.

| Verificação | Resultado |
|---|---|
| build Single smart plug + Current client ESP32-H2 wiring | sucesso, 0 warnings, 288624 bytes |
| build Door sensor + Door Sensor Battery H2 | sucesso, 0 warnings, 288880 bytes |
| build `digital_input_behavior_test`, ESP32-H2 | sucesso, 0 warnings, 147616 bytes |
| build `smart_sys_app_test`, ESP32-H2 | sucesso, 0 warnings, 266864 bytes |
| build `issp_device_concurrency_test`, ESP32-H2 | sucesso, 0 warnings, 137808 bytes |
| build `examples/issp_minimal_client`, ESP32-H2 | sucesso, 0 warnings, 253472 bytes |
| seleção de fontes | `build.ninja` de cada composição contém somente o product firmware e o board selecionados |
| casos preparados | 25 `SmartSysApp` + 10 `DigitalInputBehavior` + 4 concorrência = 39, conforme o recorte |
| configuração rastreada | `client_154/sdkconfig` seleciona Single smart plug e o board atual |
| integridade textual | `git diff --check` sem erro; diff restrito aos cinco arquivos do recorte mais especificação e mapa |

Os tamanhos acima foram medidos com a configuração rastreada como base e não
são comparáveis diretamente aos 252896 e 253120 bytes registrados na entrega
anterior, que partiu de outra configuração de build. `coordinator_154` não foi
recompilado: ele não consome `issp_behaviors` e nenhum arquivo do seu grafo de
dependências foi tocado por esta correção.

### Limitações registradas no handoff do Implementador

Esta subseção registra o estado no handoff da implementação corretiva. O
encerramento do Arquiteto abaixo substitui as pendências de revisão e hardware,
sem apagar as limitações técnicas aqui identificadas.

- os 39 casos permanecem `Not Executed` por TESTEXEC-009; compilar não comprova
  comportamento. Em particular, A1, A2, A3 e A7 têm o código escrito, mas seus
  oráculos ainda não foram observados;
- o caso de cadência e o de fallback inicial dependem do `esp_timer` real e da
  tarefa `esp_timer` em ESP32-H2; a média de 9 ms a 11 ms e o descarte de
  eventos de A4 só se tornam observáveis na execução;
- a janela de callback em voo do `esp_timer` descrita em O2 permanece limitação
  conhecida da API, sem alteração nesta correção;
- a instrumentação de A2 subestima a latência real em até um período de
  amostragem, o que é a razão de o limite superior somar `samplePeriodMs`; a
  confirmação focada em hardware prevista no conjunto de validação continua
  pendente;
- a leitura da decisão 32 quanto à falha de publicação em `begin()` é registrada
  acima como decisão local e fica sujeita à revisão focada e à avaliação do
  Arquiteto.
