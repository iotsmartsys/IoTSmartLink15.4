# Relatório de implementação — identidade de reports (v0.2)

**Classe da fonte:** Relatório

**Papel:** Engenheiro Implementador

**Especificação:** `docs/specs/ISSP-Report-Identity.md`, v0.2

**Revisão confrontada:** v0.2, `Proposed` de 12/08/2026, commit `238c3c4`. O
conteúdo normativo é idêntico ao de `f78c6d2`, revisão da análise `Ready`; a
diferença entre os dois commits está apenas nos campos de estado.

**Análise de entrada:**
`docs/reports/report-identity/analysis/2026-08-12-v02-implementability-analysis.md`,
classificação `Pronta` para a revisão `f78c6d2`

**Estado:** `In Progress` — implementação e builds concluídos; execução de
testes, flash, monitor e hardware permanecem `Not Executed`

**Data:** 12/08/2026

---

## 1. Condições de entrada

| Condição | Situação |
|---|---|
| Análise `Ready` aplicável à versão corrente | presente |
| Ordem explícita do Arquiteto para implementar a v0.2 | presente |
| Branch derivada da `main`, árvore limpa no início | `spec/client-deep-sleep`, limpa |

Entre `f78c6d2`, revisão confrontada pela análise, e a revisão implementada, a
especificação mudou somente em campos de estado — cabeçalho e seção 17 pela
promoção `238c3c4`. Nenhuma cláusula normativa foi alterada, de modo que a
classificação `Ready` continua aplicável. Conforme `REGRAS-COMUNS.md` §2.1, a
ordem explícita é o ato de autorização; o campo documental foi atualizado
mecanicamente como primeiro efeito desta atuação, não exigido como pré-condição.

## 2. Recorte implementado

### 2.1 Protocolo ISSP v2

`IOT154_VERSION` e a versão do core passaram de 1 para 2, com payload fixo de 20
bytes e o `report_id` little-endian de 64 bits no offset 8. Os dois codecs
continuam separados por target, sem qualquer dependência de código entre
`components/` e `coordinator_154/`.

- `components/issp_core/src/issp_protocol.cpp`: offsets v2, leitura e escrita de
  64 bits, `encodeReport()` com `reportId` e recusa de identidade zero,
  `decodeReport()` recusando `report_id == 0`, `decodeCommand()` e
  `decodeDiscoveryResponse()` recusando identidade não nula, `decodeAck()`
  expondo a identidade que distingue ACK de report e de comando.
- `coordinator_154/main/iot154_packet.h`: campo `report_id`,
  `iot154_packet_report_id_is_consistent()` incorporado a
  `iot154_packet_is_valid()`, e dois `_Static_assert` fixando 20 bytes e o
  limite MAC. Tipos desconhecidos preservam deliberadamente o tratamento atual
  mais adiante no caminho de recepção, em vez de passarem a ser recusados aqui.

O maior frame vigente permanece em 21 + 20 + 2 = 43 bytes contra o limite de
127, confirmado por `_Static_assert` no coordenador e por `static_assert` no
teste host-native do client.

### 2.2 Client — geração, ciclo e correlação

- `IsspDeviceConfig` recebeu `reportIdGenerator` e
  `reportIdGeneratorContext`. Nenhum header, tipo ou chamada do ESP-IDF entrou
  no `issp_core`; as dependências FreeRTOS preexistentes permanecem.
- `PendingReportSlot` conserva um único `report_id`, suficiente porque a
  tentativa in-flight guarda sua identidade na cópia preparada e na expectativa
  do transporte, e `completePendingReport()` já compara gerações.
- `publishState()` foi reescrita em uma busca limitada de oito tentativas.
  **A fonte executa fora da seção crítica.** Depois de gerar, o publisher entra
  uma única vez no `portMUX`, revalida a colisão, escolhe o slot no estado então
  vigente e realiza a mutação inteira no mesmo trecho; em colisão sai da seção
  antes de gerar de novo. Não há validação e commit em duas entradas.
- Falha de geração — só zeros ou só colisões — retorna `Failed` sem alterar
  slot, geração, contagem, ordem ou estado in-flight. Configuração sem gerador
  retorna `InvalidArgument` antes de admitir qualquer report.
- `preparePendingReport()` reutiliza a identidade e preserva o comportamento
  vigente de obter nova sequência a cada tentativa externa.
- `Issp154AckExpectation` carrega `reportId`; a correlação em
  `notifyReceive()` passou a exigir identidade não nula e igual à esperada, de
  modo que um ACK de comando (`report_id == 0`) nunca conclui um report.

### 2.3 Coordenador — janela, ordem e evento

Novo módulo privado `coordinator_154/main/report_data_policy.{c,h}`, seguindo o
precedente de `device_registry_policy.c`: lógica pura, sem header público, sem
componente compartilhado e sem segunda política. A política de registry vigente
continua sendo entrada da decisão — `report_data_policy_process()` chama
`device_registry_policy_data()`.

- Janela FIFO volátil de oito fingerprints por slot do registry, em
  armazenamento estático, sem heap e sem I/O na inserção. Nasce vazia no boot,
  não integra o blob NVS, não reordena entrada em retry e expulsa a mais antiga
  na nona.
- A ordem obrigatória da seção 8.2 vive inteiramente nesse módulo: consulta,
  evento, inserção do fingerprint somente após aceitação local completa, ACK
  somente após a inserção.
- `s_last_seq` e `is_known_data_duplicate()` foram removidos de `main.c`.
- `host_try_send_line_nowait()` implementa a sequência de aceitação local: lock
  exigido e tomado com espera zero, `uart_get_tx_buffer_free_size()` ainda sob o
  lock, submissão única de JSON mais delimitador apenas quando o limite cobre a
  linha inteira, e aceitação apenas com retorno igual ao total. As demais linhas
  do host conservam `host_send_line()` bloqueante, sem alteração.
- A janela é limpa em `DEVICE_REGISTRY_PAIR_UPDATED`, com o índice obtido de
  `device_registry_find()`, e preservada em pareamento idempotente.
- ACK de comando passou a exigir `packet.report_id == 0` na correlação.

### 2.4 Evento ao host

`host_send_event()` acrescenta o campo aditivo `event_id` e passou a devolver a
aceitação local. A formatação canônica foi extraída para
`iot154_format_event_id()`, no mesmo header de helpers puros, de modo que
produção e teste usem o mesmo código. Os campos atuais e seus valores foram
preservados; nenhuma alteração é contratada no `SmartHome-Hub`.

## 3. Divergência normativa encontrada

**A seção 8.4 é internamente inconsistente quanto ao `event_id`.** A regra em
prosa diz que «o `device_id` textual usa a mesma forma já emitida no campo
existente», e esse campo é produzido por `format_device_id()` como
`issp154-00124B0000000001`, com prefixo e hexadecimal maiúsculo. O exemplo JSON
da mesma seção mostra `00124b0000000001:0123456789ABCDEF`, sem prefixo e em
minúsculas.

Implementei a **regra em prosa**, por ser a cláusula normativa, e o resultado é
`issp154-00124B0000000001:0123456789ABCDEF`. O exemplo permanece divergente na
fonte. Decisão do Arquiteto: corrigir o exemplo ou, se o exemplo for a intenção
real, emitir revisão que mande usar uma forma textual diferente da do campo
existente — nesse caso a implementação precisa mudar.

## 4. Decisão local registrada

**Aceitação local também condiciona o ACK de origem desconhecida.** A seção 8.3
manda encaminhar e ACKar «conforme o comportamento existente»; o invariante 10
proíbe converter falha em ACK de sucesso antes da aceitação local. Adotei a
leitura do invariante, que é a cláusula mais forte e explicitamente numerada:
quando a linha do evento não é aceita localmente, não há ACK, para origem
conhecida ou desconhecida. A decisão de aceitar ou rejeitar o frame permanece
inteiramente com a política de registry, então o recorte de admissão durante
commissioning não mudou. O efeito observável é que um evento descartado faz o
client repetir, em vez de receber um ACK que não corresponde a nada.

## 5. Evidências

### 5.1 Builds canônicos — executados

| Alvo | Target | Comando | Resultado | Código de saída |
|---|---|---|---|---|
| `client_154` | esp32h2 | `idf.py -B build_impl_h2 build` | sucesso | 0 |
| `coordinator_154` | esp32c6 | `idf.py -B build_impl_c6 build` | sucesso | 0 |
| `examples/issp_minimal_client` | esp32h2 | `idf.py -B build_impl_h2 build` | sucesso | 0 |
| `issp_device_concurrency_test` | esp32h2 | `idf.py -B build_impl_h2 build` | sucesso | 0 |
| `smart_sys_app_test` | esp32h2 | `idf.py -B build_impl build` | sucesso | 0 |
| `digital_input_behavior_test` | esp32h2 | `idf.py -B build_impl build` | sucesso | 0 |
| `device_registry_test` | esp32c6 | `idf.py -B build_impl build` | sucesso | 0 |

Ambiente: ESP-IDF v6.0.1 em `~/.espressif/v6.0.1/esp-idf`. Os diretórios de
build temporários foram removidos ao fim; `client_154/sdkconfig` e
`coordinator_154/sdkconfig` foram restaurados, porque o `set-target` só
introduziu ruído de comentário `# default:`, sem mudança semântica.

Os `static_assert` de `smart_sys_app_hardware.cpp` e `smart_sys_app.cpp`
permanecem satisfeitos com as reservas `kHardwareStorageBytes` e
`kImplStorageBytes` **inalteradas**, o que fecha a exigência da REPORT-ID-DEC-012
pelo build do H2.

### 5.2 Suítes host-native — construídas, não executadas

| Suíte | Critérios cobertos | Build | Execução |
|---|---|---|---|
| `components/issp_core/test_apps/issp_protocol_host_test` (nova) | AC-004, AC-015 lado client | sucesso | `Not Executed` |
| `coordinator_154/test_apps/iot154_packet_host_test` (nova) | AC-004, AC-015 lado coordenador, AC-011 formatação | sucesso | `Not Executed` |
| `coordinator_154/test_apps/report_data_policy_host_test` (nova) | AC-006 a AC-010 e parte de janela do AC-012 | sucesso | `Not Executed` |
| `coordinator_154/test_apps/device_registry_policy_host_test` | regressão da política de registry | sucesso | `Not Executed` |

Os vetores dourados são literais idênticos nas duas suítes de codec, conforme o
invariante 9: os vetores são o artefato comum, o código não é compartilhado
entre os diretórios dos targets. A duplicação da tabela é deliberada e é o
custo dessa separação; qualquer divergência futura entre as duas cópias fica
visível como falha de uma das suítes.

### 5.3 ESP32-H2 físico — construído, não executado

`issp_device_concurrency_test` recebeu quatro casos novos para AC-001, AC-002 e
AC-003, com um gerador determinístico e roteirizável que reproduz zero,
colisão e sequência controlada. Continuam pertencendo ao H2 físico porque
`IsspDevice` depende de FreeRTOS e do `portMUX`, e permanecem `Not Executed`.

### 5.4 Não coberto por evidência automatizada nesta atuação

- **AC-005** (correlação de ACK) e a parte de regressão de transporte do
  **AC-016**: implementados, mas dependem de `Issp154Transport` com rádio; sua
  evidência pertence ao H2 físico e não foi criada como caso automatizado.
- **AC-013** (dois boots com deep sleep): depende de flash, monitor e dois
  targets ligados.
- **AC-011** na parte de confronto do parser do `SmartHome-Hub`: a formatação
  canônica tem teste; a aceitação pelo consumidor externo foi confrontada pela
  análise, não por execução.
- A parte de associação, NVS e limite do **AC-012** permanece com
  `device_registry_test`, no C6 físico.

Nenhum desses itens foi convertido em sucesso. Build comprova compilação e link,
não comportamento de rádio, UART ou hardware.

## 6. Observações de execução

- O achado N1 da análise se confirma no código: com tarefa única no
  coordenador, `s_host_uart_lock` nunca é encontrado ocupado. As quatro
  condições do AC-007 estão implementadas e são todas demonstráveis pela
  costura; apenas *espaço insuficiente* é alcançável na produção atual.
- O achado N6 permanece: a guarda de tamanho de `host_send_line()` continua em
  4095, maior que o máximo que o anel de 4096 chega a aceitar. Fora do recorte,
  irrelevante para o evento de report, e não foi tocado.
- Durante a implementação executei um programa host descartável em `/tmp` para
  confirmar `sizeof(iot154_packet_t)` e a formatação do `event_id` antes de
  escrever as suítes. Não é suíte do repositório nem evidência de aceite;
  registro por transparência.

## 7. Estado de saída

Implementação `In Progress` até a Revisão. Código, testes e conhecimento
afetados foram atualizados na mesma atuação. Não declaro conclusão: cabe ao
Arquiteto decidir `Done`, reabertura ou integração, e cabe a ordem própria
autorizar a execução das suítes e do hardware.
