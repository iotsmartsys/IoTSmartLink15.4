# Análise independente de implementabilidade — identidade de reports (v0.2)

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/ISSP-Report-Identity.md`

**Revisão confrontada:** v0.2, `Draft` de 12/08/2026, commit `f78c6d2`. A
leitura ocorreu com a revisão ainda na árvore de trabalho; seu conteúdo foi
verificado idêntico ao commitado antes do fecho deste relatório.

**Capacidade:** Engenheiro Analista (análise independente da autoria)

**Estado:** Concluído; nenhum bloqueador remanescente

**Data:** 12/08/2026

**Análise anterior:**
`docs/reports/report-identity/analysis/2026-08-12-v01-implementability-analysis.md`

**Operações executadas:** leitura das fontes normativas revisadas, do código dos
dois targets, do consumidor externo `SmartHome-Hub` e do código do driver UART e
do gerador aleatório do ESP-IDF 6.0.1. Nenhum build, teste, flash, monitor ou
hardware foi executado; nenhum arquivo de código, configuração ou dependência
foi alterado.

**Classificação principal:** **Pronta** [`Ready`]

> Este relatório registra evidências, achados e recomendações. Não altera a
> fonte normativa, não promove estado, não aceita ADR e não autoriza
> implementação, testes ou hardware. A promoção e a autorização permanecem
> exclusivamente com o Arquiteto.

---

## 1. Resultado

Os dois bloqueadores da v0.1 foram resolvidos, e ambos foram reconfrontados
contra a plataforma real, não apenas contra o texto. A ampliação de API agora
tem autoridade declarada e delimitada. A aceitação local deixou de depender de
uma semântica que a API mandatada não oferecia e passou a apoiar-se em uma
consulta que existe no ESP-IDF 6.0.1 com exatamente as propriedades que a
especificação lhe atribui.

A baseline comporta a funcionalidade, o recorte é executável sem redesenho
transversal, não há capacidade arquitetural ausente e o raio de impacto
permanece delimitado. Todos os treze achados não bloqueantes da v0.1 foram
incorporados ou roteados corretamente.

Restam sete observações, **todas declaradas não bloqueantes** na seção 5. Nenhuma
delas altera a viabilidade nem condiciona a prontidão; a mais relevante é uma
imprecisão factual dentro do REPORT-ID-AC-007, que descreve como observável em
produção uma condição que a topologia de tarefas do coordenador torna
inalcançável hoje.

---

## 2. Verificação dos bloqueadores da v0.1

### B1 — Autoridade sobre a API pública do `issp_transport_154`: **resolvido**

A seção 2 passou a declarar a emenda a `ISSP-Reusable-Components.md` v1.1 sobre
«as APIs públicas de `issp_core` […] e de `issp_transport_154`, com o ID em
`Issp154AckExpectation` e somente nos tipos necessários à sua correlação», e
delimita negativamente o alcance: «Não altera `IIsspTransport`, lifecycle, retry
ou API pública de behaviors». A REPORT-ID-DEC-010 foi reescrita no mesmo alcance
e a ADR-0004 recebeu a consequência correspondente.

Isso satisfaz a exigência de `ISSP-Reusable-Components.md` v1.1 §5 («Qualquer
ampliação material da API exige interrupção e decisão arquitetural») para o
componente que a seção 7.5 efetivamente alcança:
`components/issp_transport_154/include/issp154_transport.hpp:35-40`. A delimitação
negativa é verificável e correta — `IIsspTransport`
(`components/issp_core/include/iissp_transport.hpp`) não participa da correlação
de ACK, que vive inteiramente em `Issp154Transport::notifyReceive()` e em
`Issp154ReportExecutor::processOne()`.

### B2 — Critério de aceitação local: **resolvido, e a premissa da plataforma confere**

A seção 8.2 substituiu o critério inexecutável por uma sequência de cinco
passos. Confrontei cada premissa contra o ESP-IDF 6.0.1:

**A consulta existe e é exatamente o que a especificação afirma.**
`uart_get_tx_buffer_free_size(uart_port_t, size_t *)` está declarada em
`components/esp_driver_uart/include/driver/uart.h:628-640`, com a documentação:
«It returns the tight conservative bound for NOSPLIT ring buffer overall
enqueueable payload across up to two chunks.» A implementação
(`components/esp_driver_uart/src/uart.c:1778-1852`) calcula o payload utilizável
descontando o cabeçalho de 8 bytes por segmento e, ao final, subtrai
explicitamente `desc_cost = RINGBUF_ITEM_HDR_SIZE + align4(sizeof(uart_tx_data_t))`.
A afirmação da especificação — «já desconta o descritor e fornece um limite
conservador para a próxima transação» — está correta ao nível do código.

**O limite casa com o pior caso real do enfileiramento.** `uart_tx_all()`
(`uart.c:1627-1660`), com `tx_buf_size > 0`, envia primeiro o item descritor e
depois percorre `while (size > 0)`, fatiando o payload em no máximo dois itens
quando o espaço livre dá a volta no anel. É precisamente a estrutura «descritor
mais até dois chunks» que a consulta dimensiona. Satisfeita a consulta, nenhum
`xRingbufferSend(..., portMAX_DELAY)` do caminho encontra o anel cheio, e a
escrita não aguarda capacidade nem dreno físico. A conclusão da seção 8.2 é
sustentada pelo código, não apenas plausível.

**A premissa de exclusividade dos produtores é verdadeira na baseline.** A
especificação condiciona a garantia a «Todos os produtores de `HOST_UART_NUM`
continuam serializados pelo mesmo lock». Verifiquei: as duas únicas escritas em
`HOST_UART_NUM` de todo o `coordinator_154` estão dentro de `host_send_line()`
(`coordinator_154/main/main.c:774-775`); não há `uart_tx_chars()` nem
`uart_write_bytes_with_break()`. O console não concorre: `sdkconfig` fixa
`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` e `CONFIG_ESP_CONSOLE_UART_NUM=-1`, de
modo que nenhum `ESP_LOGx` alcança a UART1. A condição declarada é, portanto,
satisfeita hoje e é verificável de forma barata em revisão futura.

**A fatia interna em dois itens não quebra a integridade da linha.** O descritor
carrega o tamanho total e a ISR consome os itens em ordem, de modo que JSON e
delimitador chegam ao host como uma linha contígua. O objetivo original da
seção 8.2 — impedir que uma segunda chamada crie um ponto intermediário
irreversível — permanece atendido com uma única chamada de API.

---

## 3. Pontos 13 e 14 da seção 16 (novos nesta revisão)

### 13. Extração privada da política de `DATA` e ausência de segundo fluxo

A seção 8.5 autoriza a costura que a v0.1 apontou como trabalho não previsto, e
a delimita bem: local ao `coordinator_154`, seguindo o precedente de
`device_registry_policy.c`, sem virar componente compartilhado nem header
público, com `main.c` conservando rádio e efeitos.

A autorização é executável. O precedente citado existe e é próximo:
`coordinator_154/main/device_registry_policy.{c,h}` é lógica pura, e
`coordinator_154/test_apps/device_registry_policy_host_test` já a exercita em
processo host puro com apenas dois stubs (`esp_err.h` e um `iot154_packet.h`
reduzido). A exigência de que «o mesmo código de decisão deve governar produção
e testes» é compatível com esse precedente, que já é chamado por `main.c:1450`.

Confirmo também que a decisão a extrair precisa de um desfecho a mais:
`device_registry_policy_data()` (`device_registry_policy.c:25-55`) recebe hoje um
booleano `duplicate` e não sabe representar `conflito` de fingerprint nem
`aceitação local indisponível`. Isso é escolha normal de implementação dentro da
costura autorizada, não decisão normativa ausente.

### 14. Sequência sem espera: lock, consulta, escrita, cache e ACK

A ordem dos passos é internamente coerente e mapeável no código atual. O ponto
de inserção é `main.c:1466-1483`, onde hoje o evento é emitido e o ACK é
disparado; a mutação de deduplicação que hoje ocorre cedo demais, dentro de
`is_known_data_duplicate()` (`main.c:121-135`), passa a ser dividida em consulta
e inserção pós-aceitação, como a seção 8.2 exige.

Uma consequência da topologia atual merece registro, e é a origem do achado N1
da seção 5: **todo o coordenador roda em uma única tarefa**. `app_main()`
(`main.c:1322-1638`) executa o laço de RX, `poll_host_uart()` e
`process_pending_command()` em sequência, e o único `xTaskCreate` do arquivo
está comentado (`main.c:1342`). Como `s_host_uart_lock` é um mutex tomado e
devolvido apenas por essa tarefa, ele nunca pode ser encontrado ocupado.

Isso não invalida o passo 1 — exigir o lock com espera zero continua correto,
barato e necessário para que a garantia entre consulta e submissão sobreviva a
um segundo produtor futuro. Invalida apenas a afirmação do AC-007 de que essa
condição específica é observável em produção.

---

## 4. Reconfirmação das evidências da v0.1 sobre a revisão exata

Os itens abaixo foram reconfrontados contra a v0.2 e permanecem válidos sem
alteração. O detalhamento está no relatório da v0.1 e não é repetido aqui.

| Ponto da seção 16 | Situação na v0.2 |
|---|---|
| 1. Ciclo do report no core e no executor | Confirmado; a nova frase da seção 7.3 sobre um único campo no slot corresponde ao que `completePendingReport()` já garante por comparação de geração |
| 2. Correlações reais do transporte | Confirmado; autoridade agora declarada |
| 3. Produtores e consumidores do payload | Confirmado; `IsspPayloadSize` centraliza o client e `iot154_packet_t` é o único do coordenador |
| 4. Construtores/parsers e limite MAC | Confirmado com a redação corrigida: três por target, seis ao todo; pior caso 43 bytes PHY contra 127 |
| 5. Precedência do registry | Confirmado; a seção 8.3 permanece compatível com a matriz da seção 9 do registry |
| 6. Retorno, atomicidade e buffers da UART | **Reverificado nesta revisão**; ver seção 2 |
| 7. `SmartHome-Hub` | Confirmado; parser cJSON tolerante, linha preservada, evento v2 de ~240 caracteres contra buffers de 384, 512 e 576 |
| 8. `esp_fill_random()` no H2 | Confirmado; a seção 7.1 agora incorpora a evidência da cadeia `esp_ieee802154_enable()` → `esp_phy_enable()` e a espera ocupada de `esp_random()` |
| 9. Tamanhos e reservas | Folga de ordem de grandeza; certificação pertence aos `static_assert` no build canônico |
| 10. Consumidores diretos da API pública | Confirmado; produção e exemplo usam inicialização designada e absorvem a ampliação do agregado |
| 11. Relações com as especificações da seção 2 | Todas as versões conferem, agora incluindo `ISSP-Consolidation.md` v1.0 |
| 12. Meios host-native versus H2 físico | Confirmado; a separação das seções 14.1 e 14.2 corresponde às dependências reais de compilação |

Sobre o item 12, reafirmo a distinção que a v0.2 acolheu: `issp_protocol.cpp`
depende apenas de `<cstdint>` e o `iot154_packet.h` real apenas de
`stdbool/stddef/stdint/string.h`, de modo que os vetores dourados são
genuinamente host-native; já `IsspDevice` inclui `freertos/FreeRTOS.h` e usa
`portMUX_TYPE` (`issp_device.hpp:6,107`), o que prende AC-001, AC-002, AC-003 e
AC-005 ao H2 físico.

---

## 5. Achados não bloqueantes

Nenhum destes condiciona a prontidão. São precisões e notas de execução.

**N1 — REPORT-ID-AC-007 afirma observabilidade que a topologia atual não
oferece.** O critério diz que «Lock UART ausente ou ocupado, falha da consulta ou
espaço insuficiente são observáveis em produção». Das quatro condições, apenas
*espaço insuficiente* é alcançável na baseline: o lock nunca é contendido porque
há uma única tarefa (seção 3 acima); *lock ausente* só ocorre se
`init_host_uart()` falhar, caso em que `ESP_ERROR_CHECK` já aborta
(`main.c:189-190`); e *retorno inesperado* exigiria mudança do contrato do
driver. As quatro continuam corretas como contrato e todas são demonstráveis
pela costura da seção 8.5. Sugestão de redação: «observáveis pelo mesmo código
de decisão em produção e em teste», em vez de «observáveis em produção».

**N2 — O laço de RX continua bloqueável por linhas que não são de report.**
`host_send_ack()`, `host_send_error()` e `host_send_gateway()` conservam o
`host_send_line()` bloqueante e executam na mesma tarefa do laço de RX
(`main.c:1096-1102`, `:1180-1231`, `:1358`). A seção 8.2 é precisa ao limitar sua
promessa («o caminho de report não pode bloquear atrás deles»), mas convém não
generalizar a frase para o laço inteiro: com o host parado, uma linha de erro de
comando ainda pode deter o RX. O caminho de report nunca *origina* esse bloqueio,
e nenhum report é perdido — o slot pendente do client sobrevive até seu deadline.

**N3 — Cobertura do `IsspDecodedAck` pela cláusula de autoridade.** A correlação
da seção 7.5 exige o ID também em `IsspDecodedAck`
(`components/issp_core/include/issp_types.hpp:61-67`), que é público do
`issp_core`, não do transporte. A cláusula do `issp_core` fala em «campos
necessários nos tipos de report» e a DEC-010 em «injeção/propagação do ID», o
que o cobre por leitura razoável. Como B1 nasceu exatamente de uma lacuna desse
tipo, sugiro nomear `IsspDecodedAck` explicitamente para não deixar a decisão ao
Implementador.

**N4 — Divisão real do AC-012 entre dois meios.** A seção 14.1 lista AC-012 como
host-native. A parte de janela e precedência é; já «Associação, NVS, limite» do
registry passa por `device_registry.c`, que depende de `esp_log.h` e do
adaptador NVS, e hoje é coberta pelo test app
`coordinator_154/test_apps/device_registry_test`. Vale registrar a divisão para
que a evidência futura não seja declarada completa por um meio só.

**N5 — Limpeza da janela reduz-se a um gatilho.** Na baseline,
`s_entries[].ext_addr` nunca muda e entradas nunca são removidas
(`device_registry.c:259-320`); só `device_id` muda. Logo a regra da seção 8.1
realiza-se como «limpar em `DEVICE_REGISTRY_PAIR_UPDATED`, preservar em
`DEVICE_REGISTRY_PAIR_KNOWN`». `device_registry_pair()` não devolve o índice; ele
vem de `device_registry_find()`. Nota de implementação.

**N6 — Guarda de tamanho de `host_send_line()` é maior que o anel.** A guarda
vigente rejeita `len >= HOST_UART_LINE_MAX - 1`, isto é, 4095, enquanto o máximo
que o anel de 4096 bytes chega a aceitar é da ordem de 4068 após descontar
cabeçalho e descritor. Irrelevante para o evento de report, de ~240 caracteres,
mas é inconsistência latente que a nova consulta de espaço torna visível.

**N7 — Limite conservador sob fragmentação extrema.** O bound da IDF soma o
payload de até dois segmentos e subtrai o custo do descritor do total, sem provar
que o descritor cabe em um segmento contíguo específico. É o contrato público da
própria IDF, adotado como premissa de plataforma; o comportamento em anel muito
fragmentado só é observável forçando o buffer quase cheio pela costura da seção
8.5. Risco residual aceito, registrado para o teste de AC-007.

---

## 6. Incertezas e experimentos necessários

Nenhum bloqueia a prontidão; todos dependem de autorização posterior.

1. **Tamanhos reais das estruturas.** Não certificáveis por leitura. Resolvidos
   pelos `static_assert` de `smart_sys_app_hardware.cpp:81-84` e
   `smart_sys_app.cpp:425` no build canônico intrínseco à implementação
   autorizada. Não é experimento à parte.
2. **Entropia efetiva com o driver 802.15.4 puro.** A cadeia documental está
   fechada; a confirmação empírica é o próprio AC-013, em hardware.
3. **Comportamento do anel quase cheio.** Demonstração de AC-007 e AC-008 pela
   costura da seção 8.5, com consulta e submissão substituídas.
4. **Corte coordenado v1 → v2 em bancada.** Seção 12; exige flash e monitor nos
   dois targets.

---

## 7. Contenção de escopo

O teste de fronteira do perfil foi executado novamente sobre a v0.2 e **não**
resulta em pré-requisito arquitetural:

1. a capacidade necessária existe na baseline — nenhum lifecycle, ownership,
   persistência, recuperação ou arbitragem entre subsistemas é criado;
2. a ampliação de API é a propagação de um campo por dois componentes, agora com
   decisão arquitetural registrada e alcance delimitado negativamente;
3. a costura da seção 8.5 é privada a um target, tem precedente direto no
   repositório e é explicitamente proibida de virar componente compartilhado ou
   segunda política;
4. os consumidores estão enumerados: dois targets, um exemplo, três test apps e
   um consumidor externo que não requer alteração.

Com a funcionalidade desabilitada o sistema permanece na baseline atual, com o
defeito de deduplicação descrito pela ADR-0004. Não há regressão latente
introduzida pela dependência.

---

## 8. Classificação

**Pronta** [`Ready`].

Condições declaradas, conforme exige o perfil:

- **Bloqueantes:** nenhuma.
- **Não bloqueantes:** N1 a N7. São precisões de redação, notas de implementação
  e um risco residual de plataforma. Sua incorporação é decisão do Arquiteto e
  não altera a viabilidade do recorte.

Esta classificação informa o Arquiteto e não promove a especificação. Os gates
da implementação permanecem: promoção da v0.2 para Pronta para implementação
não registrada e autorização de implementação desta versão não concedida.
Enquanto isso, código, configuração, dependências e artefatos de build
permanecem fora de atuação.
