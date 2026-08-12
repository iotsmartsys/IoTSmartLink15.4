# Análise independente de implementabilidade — identidade de reports (v0.1)

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/ISSP-Report-Identity.md`

**Revisão confrontada:** v0.1, `Draft` de 12/08/2026, commit `46c84c8`

**Capacidade:** Engenheiro Analista (análise independente da autoria)

**Estado:** Concluído; dois bloqueadores normativos devolvidos ao Arquiteto

**Data:** 12/08/2026

**Operações executadas:** leitura de fontes normativas, código dos dois targets,
consumidor externo `SmartHome-Hub` e documentação do ESP-IDF 6.0.1. Nenhum
build, teste, flash, monitor ou hardware foi executado; nenhum arquivo de
código, configuração ou dependência foi alterado.

**Classificação principal:** **Não pronta — defeito da especificação**
[`Not Ready — Specification Defect`]

> Este relatório registra evidências, achados e recomendações. Não altera a
> fonte normativa, não promove estado, não aceita ADR e não autoriza
> implementação, testes ou hardware.

---

## 1. Resultado

A baseline comporta a funcionalidade. O desenho cabe no recorte autorizado: não
exige persistência, sessão, novo lifecycle, aumento das reservas estáticas,
alteração funcional do deep sleep nem mudança no repositório externo. Não há
pré-requisito arquitetural e o raio de impacto está delimitado.

Dois contratos da própria funcionalidade, porém, estão ausentes ou incorretos na
revisão confrontada e impedem a classificação `Pronta`. Ambos são corrigíveis
localmente pelo Arquiteto, sem especificação complementar.

| # | Achado bloqueante | Seção |
|---|---|---|
| B1 | A ampliação de API contratada excede a autoridade declarada: `issp_transport_154` não consta das emendas | §2, §7.5, DEC-010 |
| B2 | O teste de aceitação local mandatado é inexecutável na plataforma: `uart_write_bytes()` não retorna parcial e bloqueia sem limite | §8.2, DEC-007, AC-007 |

Os demais treze achados são não bloqueantes e estão na seção 5.

---

## 2. Achados bloqueantes

### B1 — Autoridade não declarada para a API pública do `issp_transport_154`

A seção 7.5 exige que o ACK de report só conclua a tentativa quando coincidirem
origem, `device_id`, `sequence`, `report_id` não nulo, `endpoint_id` e status. A
correlação de ACK do client não vive no `issp_core`: vive em
`Issp154AckExpectation`, declarada no header **público**
`components/issp_transport_154/include/issp154_transport.hpp:35-40`, e é
consumida por `Issp154Transport::notifyReceive()`
(`components/issp_transport_154/src/issp154_transport.cpp:899-953`) e por
`Issp154ReportExecutor::processOne()`
(`components/issp_transport_154/src/issp154_report_executor.cpp:109-113`).
Acrescentar `report_id` a essa estrutura é ampliação material de API pública.

A seção 2 declara emenda a `ISSP-Reusable-Components.md` v1.1 apenas para
`issp_core` («amplia de forma material e limitada a API pública de
`issp_core`»). A ADR-0004 registra a consequência somente como «a API pública de
configuração de `IsspDevice` recebe uma fonte injetável de IDs». Já
`ISSP-Reusable-Components.md` v1.1 §5 classifica `Issp154Transport` e seus tipos
necessários como API pública de alto nível e determina: «Qualquer ampliação
material da API exige interrupção e decisão arquitetural».

A própria seção 2 fecha com a regra que torna isto bloqueante: «Se a análise
identificar conflito fora dessas alterações, deve classificar a especificação
como não pronta. O Implementador não pode ampliar silenciosamente o recorte para
resolver autoridade ausente.»

REPORT-ID-DEC-010 fala em «injeção e propagação do ID», o que sugere intenção de
cobrir o caminho de propagação inteiro; mas a intenção não está declarada onde a
regra a exige. Cabe ao Arquiteto decidir se a correção é editorial.

**Correção mínima sugerida:** estender o marcador `Amends` de
`ISSP-Reusable-Components.md` v1.1 na seção 2 para nomear também a API pública
de `issp_transport_154` (`Issp154AckExpectation` e o que a correlação de ACK
exigir), e refletir a mesma extensão na consequência correspondente da ADR-0004.

### B2 — O critério de aceitação local não é executável com a API mandatada

A seção 8.2 mandata: «JSON e delimitador são reunidos em um único buffer fixo e
submetidos por uma única operação `uart_write_bytes()` sob o lock vigente;
somente retorno igual ao tamanho total significa aceitação». O contrato da
função no ESP-IDF 6.0.1 (`components/esp_driver_uart/include/driver/uart.h:516-533`)
é outro:

> «if the 'tx_buffer_size' > 0, this function will return after copying **all**
> the data to tx ring buffer» — retornos: `-1` para erro de parâmetro, caso
> contrário o número de bytes empurrados.

O coordenador instala o driver com `tx_buffer_size = HOST_UART_BUF_SIZE = 4096`
(`coordinator_154/main/main.c:186`). Consequências materiais:

1. **O ramo de falha é inalcançável em produção.** A função não devolve escrita
   parcial: ou copia tudo, ou falha por parâmetro inválido. «Retorno igual ao
   tamanho total» é tautológico no caminho real, de modo que o AC-007 («falha,
   truncamento ou estouro na linha UART não insere ID e não envia ACK») só pode
   ser demonstrado por costura de teste, nunca observado na produção que ele
   pretende governar.
2. **A espera é ilimitada, não a ausência de espera que a seção afirma.** A
   seção declara que «o contrato não exige aguardar o esvaziamento físico da
   UART». Com o ring buffer cheio — host parado, travado ou lento — é
   exatamente o que `uart_write_bytes()` faz: aguarda o dreno físico. A chamada
   ocorre dentro de `host_send_line()` sob `s_host_uart_lock`
   (`coordinator_154/main/main.c:761-780`), no laço de RX do rádio
   (`main.c:1360-1483`). A v0.1 promove essa chamada a passo obrigatório
   **antes** do cache do ID e **antes** do ACK, de forma que um host que pare de
   drenar passa a bloquear o ACK de report, o processamento de qualquer outro
   frame e o atendimento do comando pendente. O client então esgota seus 50 ms
   de ACK (`issp154_report_executor.cpp:12`), reapresenta a cada 1000 ms
   (`issp154_report_executor.cpp:13`) e, em composição com deep sleep, consome
   a janela acordada até o deadline.

O defeito pertence à funcionalidade: é o contrato de «aceitação local» que a
identidade e o ACK passam a depender. Não exige capacidade arquitetural nova.

**Correção mínima sugerida:** definir a aceitação local em termos que a
plataforma entregue e limitar a espera. Por exemplo: exigir verificação prévia
de espaço livre suficiente na fila de transmissão antes da escrita única,
declarar «espaço insuficiente» como a falha observável que não insere ID e não
ACKa, e fixar que a submissão do evento não pode bloquear o laço de RX além de
um limite declarado. A escolha da primitiva permanece do Arquiteto; o que falta
é o contrato, não a técnica.

---

## 3. Verificação dos doze pontos da seção 16

| # | Ponto | Resultado |
|---|---|---|
| 1 | Criação, coalescência, reserva, preparo, retry e conclusão de report | Confirmado; ver §4.1 |
| 2 | Correlações reais do transporte para ACK | Confirmado, com **B1** |
| 3 | Produtores e consumidores do payload nos dois targets | Confirmado; ver §4.2 |
| 4 | Três construtores/parsers de frame e limite IEEE 802.15.4 | Confirmado; folga larga, ver §4.3 |
| 5 | Precedência do registry (conhecido/desconhecido/indisponível) | Confirmado; ver §4.4 |
| 6 | Retorno, atomicidade e buffers da UART do coordenador | **B2** |
| 7 | Parser e preservação da linha no `SmartHome-Hub` | Confirmado; ver §4.5 |
| 8 | `esp_fill_random()` no ESP32-H2 | Confirmado com ressalva; ver §4.6 |
| 9 | Tamanhos reais de `IsspDevice`, `HardwareState`, buffers e reservas | Folga larga; certificação pertence ao build canônico, ver §4.7 |
| 10 | Consumers diretos da API pública, exemplo e fixtures | Confirmado; ver §4.8 |
| 11 | Relações com cada especificação da seção 2 | Versões conferem; **B1** é a única lacuna |
| 12 | Critérios e meios de teste | Confirmado com correções; ver §4.9 |

---

## 4. Evidências por área

### 4.1 Ciclo de vida do report no core e no executor

- Admissão e coalescência ocorrem em `IsspDevice::publishState()`
  (`issp_device.cpp:87-153`): dois laços — atualização do par
  `endpointId`/`eventType` existente e inserção em slot vazio. Ambos avançam
  `generation`; ambos recebem novo ID conforme a seção 7.3, e ambos cabem no
  protocolo da seção 10 (gerar fora, entrar, revalidar, mutar inteiro).
- A reserva e o preparo estão em `acquirePendingReportLocked()`
  (`issp_device.cpp:217-246`) e `preparePendingReport()`
  (`issp_device.cpp:289-327`), que já obtém nova `sequence` a cada preparo —
  exatamente o comportamento que a seção 7.3 manda preservar.
- `completePendingReport()` (`issp_device.cpp:257-287`) só limpa o slot quando
  `slot.generation == token.generation`. Isso já sustenta, sem mudança
  semântica, a regra da seção 7.3 para atualização durante voo: a tentativa
  antiga termina com o ID que carregou em `IsspPreparedReport` e não remove a
  geração nova. **Um único `report_id` por slot é suficiente**: o ID em voo vive
  na cópia preparada e na expectativa de ACK, não no slot.
- O retry externo é `Issp154ReportExecutor::processOne()`
  (`issp154_report_executor.cpp:101-141`), acionado pelo laço de
  `run()` (`:171-210`). Um report por vez, serial — o que confirma o invariante
  que a seção 8.1 pede reconfirmar sobre o tamanho oito da janela: nenhum outro
  produtor mantém mais de oito identidades reapresentáveis por device.
- **Contexto de execução dos publishers**, relevante para a seção 10:
  `DigitalInputBehavior` publica a partir de um callback `esp_timer`
  (`digital_input_behavior.cpp:79-86`, `:333`), isto é, da task do esp_timer;
  `DigitalOutputBehavior` publica de `begin()` e de `handle()`
  (`digital_output_behavior.cpp:59`, `:123`), no contexto de recepção. **Nenhum
  caminho de admissão parte de ISR**, de modo que gerar o ID fora da seção
  crítica é seguro em todos os contextos alcançáveis.
- `publishReport()` (`issp_device.cpp:155-182`) não usa slot, fila nem ACK: ele
  codifica e envia direto. A seção 7.4 é coerente com isso; observe-se apenas
  que o ID desse caminho será inserido na janela do coordenador sem que exista
  tentativa do client correlacionada a ele.

### 4.2 Produtores e consumidores do payload

Do lado do client o tamanho é centralizado em `IsspPayloadSize`
(`issp_protocol.hpp:11`) e todos os pontos derivam dele — encode/decode
(`issp_protocol.cpp`), buffers de `issp_device.cpp:167,305,465` e o payload de
discovery em `issp154_transport.cpp:417`. A mudança 12 → 20 é local e não deixa
literais órfãos.

Do lado do coordenador o único produtor/consumidor é `iot154_packet_t`
(`iot154_packet.h:45-54`), `packed`, consumido por `main.c`,
`device_registry.h` e `iot154_radio.c`. Nenhum outro projeto do repositório
inclui o header. `iot154_checksum()` e `iot154_parse_frame_info()` já operam
sobre `sizeof(*packet)` e acompanham a mudança sem edição de literais.

**Exceção encontrada:** os logs de diagnóstico do transporte tratam o byte 8
como `endpoint`, o que em v2 passa a ser o primeiro byte de `report_id` —
`issp154_transport.cpp:665`, `:712`, `:742`, `:751`. `diagnosticDeviceId()`
(bytes 2–5) e `diagnosticSequence()` (bytes 6–7) permanecem corretos. Sem
correção, o rastro de protocolo passa a imprimir campo errado silenciosamente.

### 4.3 Frames e limite MAC

São três construtores em **cada** target, não três no total:

- client, `issp154_mac_frame.c`: `build_broadcast_from_extended`,
  `build_extended_unicast`, `build_reply`;
- coordenador, `iot154_packet.h`: `iot154_build_frame`,
  `iot154_build_ext_frame`, `iot154_build_broadcast_from_ext_frame`.

Pior caso com payload de 20 bytes: cabeçalho estendido 21 + 20 + FCS 2 = **43
bytes PHY**, contra o limite 127. O guarda do client
`ISSP154_EXTENDED_UNICAST_MAX_PAYLOAD_LENGTH` vale 104
(`issp154_mac_frame.c:25-34`) e continua satisfeito; os buffers são
`kIssp154FrameCapacity = 128` (`issp154_transport.hpp:16`) e
`IOT154_MAX_FRAME_LEN + 1` (`main.c:73,76`). O AC-015 é alcançável com folga.

Efeito colateral do corte de versão que merece registro: um frame v1 chegando a
um receptor v2 é rejeitado por **comprimento**, antes que o byte de versão seja
lido — no coordenador por `iot154_parse_frame_info()` (`iot154_packet.h:232`) e
no client por `length != IsspPayloadSize` em cada `decode*`. O diagnóstico
exigido pela seção 11 («frame v1 recebido em endpoint v2») portanto não emerge
sozinho: exige sondagem explícita do byte de versão no caminho de rejeição.

### 4.4 Registry e precedência

`device_registry_policy_data()` (`device_registry_policy.c:25-55`) reproduz
fielmente a matriz da seção 9 de `ISSP-Coordinator-Paired-Device-Registry.md`
v0.4, inclusive a aceitação operacional de origem desconhecida com janela
aberta. A assimetria declarada na seção 8.3 desta especificação é compatível com
essa fonte e não exige alteração dela.

A limpeza de janela da seção 8.1 («quando o slot passa a representar outro
endereço ou `device_id`») reduz-se, na baseline real, a um único gatilho:
`s_entries[].ext_addr` nunca muda e entradas nunca são removidas — o pareamento
só acrescenta ou atualiza `device_id`
(`device_registry.c:259-320`). Logo, limpar em
`DEVICE_REGISTRY_PAIR_UPDATED` e preservar em `DEVICE_REGISTRY_PAIR_KNOWN`
satisfaz a regra. Detalhe de implementação a resolver: `device_registry_pair()`
não devolve o índice do slot; o índice sai de `device_registry_find()`.

Note-se que o `s_last_seq` vigente (`main.c:115-135`) **não** possui essa
invalidação. A regra da seção 8.1 é, portanto, comportamento novo e correto, não
preservação do existente.

### 4.5 Consumidor externo `SmartHome-Hub` — confirmado

Repositório confrontado em
`/Users/marcelocostamiranda/source/IoT/SmartHome/Modules/SmartHome-Hub`, commit
`629258f`. A afirmação da seção 8.4 procede:

- `main/coordinator_line_parser.c:100-107` valida a linha com `cJSON_Parse()` e
  `cJSON_IsObject()` e entrega **a linha original** ao callback; campos
  desconhecidos não afetam a decisão. O reconhecimento de heartbeat usa
  `strstr()` sobre tokens específicos (`:12-23`) e não é perturbado por um campo
  adicional.
- `main/event_processor.c:13-44` insere `,"owner":"..."` imediatamente antes do
  último `}`, preservando todo o conteúdo anterior.

Buffers confrontados com o evento máximo v2: `device_id` 24 + `capability_name`
64 + `value` 11 + `type` + `event_id` 41, mais a moldura JSON, resultam em
aproximadamente **240 caracteres**. Cabem em
`ISSP154_HOST_EVENT_JSON_BUFFER_SIZE = 384` (`main.c:64`), em
`line_buf[512]` (`coordinator_line_parser.h:24`) e em `s_event_payload[576]`
(`event_processor.c:11`), este último já com o `owner` acrescentado. **Nenhuma
alteração no repositório externo é necessária**, e o AC-011 é verificável por
teste de formatação no coordenador.

### 4.6 `esp_fill_random()` no ESP32-H2 — admissível, com ressalva registrada

Verificado em ESP-IDF 6.0.1 (`~/.espressif/v6.0.1/esp-idf`):

- A condição documentada para número verdadeiramente aleatório no H2 é «RF
  subsystem is enabled», que ativa internamente o High Speed ADC como fonte de
  entropia (`docs/en/api-reference/system/random.rst`).
- `esp_ieee802154_enable()` chama `esp_phy_enable(PHY_MODEM_IEEE802154)`
  (`components/ieee802154/driver/esp_ieee802154_dev.c:1164`), satisfazendo essa
  condição.
- **Toda admissão de report ocorre com o rádio já ligado.** Os behaviors só
  publicam a partir de `begin()`, invocado por `IsspDevice::start()`, que exige
  `transport_.state() == IsspTransportState::Ready` (`issp_device.cpp:43-48`);
  e o estágio `StartDevice` sucede `InitializeNetwork`. Não há caminho de falha
  que alcance admissão com o rádio desligado: falha de rede encerra o `setup()`
  e faz rollback do transporte sem iniciar behaviors.

Ressalvas, nenhuma bloqueante:

1. A documentação do H2 nomeia `esp_openthread_init` como a API de 802.15.4; o
   projeto usa o driver `esp_ieee802154` puro. A equivalência é sustentada pela
   chamada de PHY acima, não por texto documental direto.
2. O client coloca o rádio em sleep em torno de transmissões. Se uma geração
   coincidir com esse intervalo, a saída degrada para pseudoaleatória — semeada
   pelo bootloader e combinada por XOR com o contador RTC e o ciclo de CPU
   (`components/esp_hw_support/hw_random.c:49-87`), portanto ainda não
   determinística entre boots. Isso é coerente com o risco não criptográfico já
   aceito na seção 7.1 e não recria o defeito que a especificação corrige.
3. **Evidência que confirma a proibição da seção 10:** `esp_random()` faz espera
   ocupada de `96 * 16` ciclos APB por palavra de 32 bits no H2
   (`hw_random.c:39-41`), isto é, dezenas de microssegundos para um ID de 64
   bits. Chamá-la dentro do `portMUX` seria concretamente nocivo, não apenas
   estilisticamente indesejável.

### 4.7 Memória e reservas

Crescimento: `+8` bytes por slot pendente × 8 slots = 64 bytes em `IsspDevice`,
mais o par gerador/contexto em `IsspDeviceConfig`; no coordenador, oito
fingerprints × oito slots do registry, em armazenamento estático, na ordem de
setecentos bytes. As reservas são `kHardwareStorageBytes = 8192`
(`smart_sys_app_impl.hpp:57`) e `kImplStorageBytes = 16384`
(`SmartSysApp.h:277`); `HardwareState`
(`smart_sys_app_hardware.cpp:70-84`) agrega transporte, network manager,
device, executor e serviços de reset, e a folga é de ordem de grandeza.

Leitura não certifica `sizeof`. A certificação já existe e é adequada: os
`static_assert` de `smart_sys_app_hardware.cpp:81-84` e `smart_sys_app.cpp:425`
falham o build se a reserva não bastar. Isso é coberto pelo build canônico
intrínseco à implementação autorizada e **não requer experimento separado**. A
DEC-012 continua sendo o desfecho correto caso o build acuse insuficiência.

### 4.8 Consumidores diretos da API pública

`IsspDeviceConfig` é construída em dois lugares:
`smart_sys_app_hardware.cpp:134` (produção) e
`examples/issp_minimal_client/main/main.cpp:36-38` (exemplo). Ambos usam
inicialização designada, de modo que a ampliação do agregado não quebra a
compilação por si só; o exemplo precisará declarar seu próprio gerador conforme
a seção 7.1. O exemplo nunca chama `start()`, então a rejeição de configuração
sem gerador não o afeta em execução. Nenhum behavior, product firmware ou board
model conhece a configuração do device — a seção 7.1 procede nesse ponto.

### 4.9 Meios de teste — duas correções ao mapa da seção 14.1

1. **Melhor do que o previsto:** os vetores dourados dos AC-004 e AC-015 são
   plenamente **host-native**, sem hardware. `issp_protocol.cpp` inclui apenas
   `issp_protocol.hpp` → `issp_types.hpp` → `<cstdint>`, e o
   `iot154_packet.h` real depende só de `stdbool/stddef/stdint/string.h`. Ambos
   compilam em processo host puro, dentro do TESTEXEC-003.
2. **Pior do que o previsto:** «automatizável sem rádio real» não significa «sem
   hardware». AC-001, AC-002, AC-003 e AC-005 exercitam `IsspDevice`, cujo
   header inclui `freertos/FreeRTOS.h` e usa `portMUX_TYPE`
   (`issp_device.hpp:6,107`). Só rodam como test app ESP-IDF em H2 físico
   (`components/issp_core/test_apps/issp_device_concurrency_test`), o que exige
   autorização de execução própria. A seção 14.1 deveria distinguir os dois
   casos.

Além disso, a costura que os AC-006 a AC-010 e AC-012 pressupõem — «policy/core
do coordenador com UART substituível» — **não existe hoje**. A ordem de
processamento de `DATA`, a deduplicação e o envio ao host estão inteiramente em
`coordinator_154/main/main.c:1439-1483` e `:761-840`, arquivo de `app_main` sem
costura e não linkável em teste host. Existe precedente próximo e adequado:
`device_registry_policy.c` com
`coordinator_154/test_apps/device_registry_policy_host_test`. A extração é
trabalho ordinário dentro de um único target, com precedente — **não é
pré-requisito arquitetural** —, mas é volume relevante e deve ser previsto.
`device_registry_policy_data()` também precisará de um terceiro desfecho para
`conflito`, hoje inexistente em sua assinatura booleana `duplicate`.

---

## 5. Achados não bloqueantes

| # | Achado | Roteamento |
|---|---|---|
| N1 | A seção 7.1 afirma «O core não inclui header, tipo ou chamada do ESP-IDF». É factualmente incorreto sobre a baseline: `issp_device.hpp:6` inclui `freertos/FreeRTOS.h` e a classe usa `portMUX_TYPE`. A intenção — não adquirir dependência de ESP-IDF **para a fonte de IDs** — é sustentável | Correção redacional da especificação |
| N2 | Os logs de diagnóstico do transporte leem `endpoint` no byte 8, que em v2 é `report_id` (`issp154_transport.cpp:665,712,742,751`) | Implementação |
| N3 | O diagnóstico «frame v1 em endpoint v2» da seção 11 não emerge sozinho: a rejeição ocorre por comprimento antes da leitura da versão, nos dois targets | Implementação, com nota na especificação |
| N4 | A seção 6.2 diz «os três construtores de frame existentes»; são três em cada target | Correção redacional |
| N5 | A limpeza de janela da seção 8.1 reduz-se a `PAIR_UPDATED`; `device_registry_pair()` não devolve o índice | Implementação |
| N6 | A seção 14.1 não distingue «sem rádio» de «sem hardware»; AC-004/AC-015 são host-native e AC-001/002/003/005 exigem H2 físico | Correção da especificação |
| N7 | A costura de UART substituível e a extração da ordem de `DATA` de `main.c` não estão previstas como volume de trabalho | Planejamento de implementação |
| N8 | `device_registry_policy_data()` precisa de um terceiro desfecho para conflito de fingerprint | Implementação |
| N9 | `publishReport()` direto insere ID na janela do coordenador sem tentativa correlacionada do client | Consequência aceita; registrar |
| N10 | A seção 2 não fixa versão para `ISSP-Consolidation.md`; a vigente é v1.0. As demais referências de versão conferem exatamente | Correção redacional |
| N11 | `esp_random()` faz espera ocupada de dezenas de microssegundos por ID no H2 — evidência que sustenta a proibição da seção 10 | Registrar na especificação |
| N12 | Um único `report_id` por `PendingReportSlot` basta; o ID em voo vive na cópia preparada. A seção 7.3 já é compatível, mas o ponto merece ficar explícito para o Implementador | Nota de implementação |
| N13 | Toolchain disponível e conforme: ESP-IDF 6.0.1 instalado em `~/.espressif/v6.0.1/esp-idf`. Nenhum impedimento de ambiente para os builds da seção 14.2 | Registrar |

---

## 6. Incertezas e experimentos necessários

Nada aqui bloqueia a prontidão; tudo depende de execução autorizada em etapa
posterior.

1. **Tamanhos reais das estruturas.** Não certificáveis por leitura. Resolvidos
   pelos `static_assert` no build canônico intrínseco à implementação
   autorizada, não por experimento à parte.
2. **Qualidade da entropia com o driver 802.15.4 puro.** A cadeia documental
   está fechada pela chamada de PHY, mas a confirmação empírica — dois boots
   consecutivos produzindo IDs distintos — pertence ao AC-013, em hardware, sob
   autorização própria.
3. **Comportamento da fila UART sob host que não drena.** É a demonstração do
   AC-007 e do AC-008. Depende da definição que resolver **B2** e, depois, de
   costura de teste e de execução autorizada.
4. **Corte coordenado v1 → v2 em bancada.** A seção 12 é verificável apenas com
   os dois targets gravados, sob autorização de flash e monitor.

---

## 7. Contenção de escopo

O teste de fronteira do perfil foi executado e **não** resulta em pré-requisito
arquitetural:

1. a capacidade necessária existe na baseline — não há novo lifecycle,
   ownership, persistência, recuperação ou arbitragem entre subsistemas;
2. a ampliação de API é a de propagação de um campo, já decidida na ADR-0004, e
   o defeito B1 é de **declaração de autoridade**, não de ausência de decisão;
3. os consumidores estão delimitados e enumerados: dois targets, um exemplo,
   dois test apps e um consumidor externo que não requer alteração.

Com a funcionalidade desabilitada o sistema permanece exatamente na baseline
atual, com o defeito de deduplicação que a ADR-0004 descreve. Não há regressão
latente introduzida pela dependência.

---

## 8. Classificação e condição de retomada

**Não pronta — defeito da especificação** [`Not Ready — Specification Defect`].

Condições declaradas:

- **Bloqueantes:** B1 e B2.
- **Não bloqueantes:** N1 a N13, incorporáveis a critério do Arquiteto sem
  alterar a viabilidade.

Condição para nova análise: uma revisão da especificação que declare a
autoridade sobre a API pública de `issp_transport_154` e que redefina o critério
de aceitação local em termos executáveis e limitados no tempo. Resolvidos esses
dois pontos, e sem outra mudança material, o recorte é implementável na baseline
vigente.

Os gates da implementação permanecem todos ausentes: análise `Ready` não
alcançada, promoção não registrada, autorização não concedida. Código,
configuração, dependências e artefatos de build permanecem fora de atuação.
