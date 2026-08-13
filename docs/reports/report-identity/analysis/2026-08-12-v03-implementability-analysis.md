# Análise de implementabilidade — identidade de reports (v0.3)

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/ISSP-Report-Identity.md`

**Revisão confrontada:** v0.3, `Draft` de 12/08/2026, **ainda não commitada**. A
leitura ocorreu sobre a árvore de trabalho, sobre a base `42a7c13`. Ver a
observação O6 quanto à vinculação do `Ready` a um hash.

**Capacidade:** Engenheiro Analista

**Estado:** Concluído; nenhum bloqueador remanescente

**Data:** 12/08/2026

**Análises anteriores:**
`docs/reports/report-identity/analysis/2026-08-12-v01-implementability-analysis.md`;
`docs/reports/report-identity/analysis/2026-08-12-v02-implementability-analysis.md`

**Recorte desta análise:** o delta normativo da v0.2 para a v0.3. As cláusulas
cujo texto não mudou permanecem cobertas pelo `Ready` da revisão `f78c6d2`; a
seção 4 declara explicitamente quais itens da seção 16 da especificação foram
reconfrontados por serem materialmente afetados pelo delta.

**Operações executadas:** leitura da especificação e das autoridades da seção 2,
do código dos dois targets, das suítes host-native existentes e da guarda
documental `python3 tools/validate_ekom_documents.py .`, que retornou
`Roteamento documental EKOM válido.` Nenhum build, teste, flash, monitor ou
hardware foi executado; nenhum arquivo de código, configuração ou dependência
foi alterado.

**Classificação principal:** **Pronta** [`Ready`]

> Este relatório registra evidências, achados e recomendações. Não altera a
> fonte normativa, não promove estado, não aceita ADR e não autoriza
> implementação, testes ou hardware. A promoção e a autorização permanecem
> exclusivamente com o Arquiteto.

---

## 1. Resultado

O delta da v0.3 é composto por três mudanças normativas. Duas já estão
satisfeitas pela implementação vigente e não geram trabalho de código; a
terceira é uma correção local, delimitada e sem alcance transversal.

| Delta | Efeito sobre a implementação | Efeito sobre a evidência |
|---|---|---|
| D1 — exemplo do `event_id` (§8.4) | Nenhum; o código já produz a forma agora exemplificada | Nenhum; AC-011 já asserta a string canônica |
| D2 — aceitação local sem espera para origem desconhecida (§8.3) | Nenhum; o comportamento já é esse desde `22e5e10` | Nenhum; o caso já existe na suíte de política |
| D3 — rejeição de payload excedente (§6.1, AC-004, §14.1) | Dois pontos no `coordinator_154`, ambos de uma linha | Dois casos novos na suíte host-native já contratada |

Não há capacidade arquitetural ausente, ampliação de API, mudança de lifecycle,
persistência, ownership ou consumidor fora do recorte. O teste de fronteira do
perfil não se aplica: a correção altera somente o comportamento da própria
funcionalidade, dentro do dono natural do código.

Restam seis observações, **todas declaradas não bloqueantes** na seção 5.

---

## 2. Confronto do delta

### 2.1 D1 — exemplo do `event_id` (§8.4)

A v0.3 substitui o exemplo por
`issp154-00124B0000000001:0123456789ABCDEF` e mostra o campo `device_id` ao
lado, tornando explícito que a esquerda do par é literalmente o valor do campo
existente.

A implementação já produz exatamente isso:

- `format_device_id()` emite `ISSP154_HOST_DEVICE_ID_PREFIX "%02X…"`, ou seja
  `issp154-` seguido de dezesseis dígitos hexadecimais maiúsculos
  (`coordinator_154/main/main.c:368`);
- `iot154_format_event_id()` concatena esse texto, `:` e dezesseis dígitos
  maiúsculos, e recusa buffer insuficiente sem emitir identidade truncada
  (`coordinator_154/main/iot154_packet.h:131`);
- `host_send_event()` monta a linha com os campos atuais preservados e o
  `event_id` aditivo (`coordinator_154/main/main.c:888`).

A AC-011 já é exercitada com a string canônica literal e com os limites do
buffer em `test_iot154_packet.c:174`. O buffer do `event_id`
(`ISSP154_HOST_DEVICE_ID_LEN + 1 + 16 + 1 == 42`) dimensiona exatamente o pior
caso, e o teste cobre 41 e 42 bytes.

**Conclusão:** o defeito A1 da Revisão é fechado por correção da fonte, sem
mutação de código nem de teste. A cláusula permanece implementável e já
implementada.

### 2.2 D2 — aceitação local sem espera para origem desconhecida (§8.3)

A v0.3 estende a origem desconhecida admitida durante a janela a mesma sequência
da §8.2: lock com espera zero, consulta de espaço sob o lock, submissão única,
ACK somente após a aceitação integral.

O código já se comporta assim. `report_data_policy_process()` usa o mesmo
`effects->emit_event` para conhecido e desconhecido e só ACKa depois dele; a
falha do desconhecido tem resultado próprio
(`REPORT_DATA_OUTCOME_UNKNOWN_LOCAL_UNAVAILABLE`) e log próprio
(`coordinator_154/main/report_data_policy.c:174`;
`coordinator_154/main/main.c:1027`). O efeito de produção é
`coordinator_emit_event()` → `host_send_event()` → `host_try_send_line_nowait()`,
o mesmo caminho da origem conhecida; nenhum produtor de evento usa o
`host_send_line()` bloqueante. O caso já está coberto em
`test_report_data_policy.c:303`.

Também confirmo que a §8.3 **não** amplia autoridade sem declaração. A
`ISSP-Coordinator-Paired-Device-Registry.md` deixa a célula
`Ready | aberta | desconhecida | DATA` deliberadamente não normatizada: «o gate
aprova tanto processar quanto descartar, mas deve afirmar ausência de escrita,
ausência de nova entrada e permanência da origem como desconhecida». A regra da
v0.3 satisfaz as três condições — não persiste, não cria entrada e não torna a
origem conhecida — de modo que ela preenche um campo aberto em vez de contrariar
uma célula normativa. A precedência de `RegistryUnavailable` e a rejeição com
janela fechada continuam intactas.

**Conclusão:** o achado A2 da Revisão é ratificado pela fonte, sem trabalho de
implementação e sem conflito de autoridade.

### 2.3 D3 — rejeição de payload excedente (§6.1, AC-004, §14.1)

Esta é a única mudança que produz trabalho.

**Assimetria confirmada.** O client recusa comprimento nos dois sentidos, em duas
camadas: o transporte compara o comprimento real extraído do MAC
(`issp154_transport.cpp:845`) e cada decodificador exige `length ==
IsspPayloadSize` (`issp_protocol.cpp:173`, `207`, `239`, `294`). O coordenador
recusa apenas truncamento, em dois pontos que duplicam a mesma verificação:

- `iot154_parse_frame_info()`, `coordinator_154/main/iot154_packet.h:300`;
- `diagnostic_extract_mac()`, `coordinator_154/main/main.c:356`.

Ambos usam `frame[0] < mac_header_len + sizeof(*packet) + IOT154_FCS_LEN`. Um
frame mais longo cujos vinte primeiros bytes de payload formem um v2 válido é
aceito hoje e pode gerar evento.

**A igualdade estrita é segura.** Reconfrontei os três formatos de frame de cada
target contra a derivação de `mac_header_len` do coordenador, porque uma
divergência de um byte transformaria a correção em indisponibilidade de
produção:

| Formato | FCF | Cabeçalho | `frame[0]` | `mac_header_len` derivado |
|---|---|---:|---:|---:|
| Curto/curto | `0x9841` | 9 | 31 | 9 |
| Broadcast curto, origem estendida | `0xd841` | 15 | 37 | 15 |
| Unicast estendido | `0xdc41` | 21 | 43 | 21 |

Os três têm compressão de PAN ativa, e o percurso de `iot154_parse_frame_info()`
reproduz exatamente o percurso dos construtores de
`components/issp_transport_154/src/issp154_mac_frame.c` e de `iot154_packet.h`.
Em todos, `frame[0] == mac_header_len + 20 + 2`. Nenhum frame legítimo dos dois
targets é rejeitado pela troca de `<` por `!=`.

**Frames v1 continuam recusados.** Um payload v1 de 12 bytes produz `frame[0]`
abaixo do mínimo e é recusado pela verificação de mínimo já existente
(`iot154_packet.h:228`), antes de qualquer decodificação.

**Efeito colateral favorável.** `iot154_parse_frame_info()` não inspeciona bit de
segurança nem versão de frame, ao contrário do extrator do client. Com a
igualdade estrita, um frame com cabeçalho de segurança auxiliar ou com IEs passa
a ser recusado por comprimento, o que estreita a superfície de recepção sem
cláusula adicional.

**Ponto de aterrissagem do diagnóstico.** `diagnostic_extract_mac()` já calcula
`*payload_length` em `main.c:361` e esse valor **não é consumido em lugar
nenhum** hoje. Ele é o lugar natural para o motivo distinguível que a §11 exige
para «tamanho inválido», sem criar estrutura nova.

**Evidência.** A suíte `coordinator_154/test_apps/iot154_packet_host_test`
exercita hoje somente a struct do pacote e o `event_id`; não chama
`iot154_parse_frame_info()`. Os dois casos pedidos pela §14.1 — payload menor e
maior que 20 bytes — cabem nela sem suíte paralela: o cabeçalho depende apenas de
`stdbool/stddef/stdint/string.h`, os construtores de frame estão no mesmo
cabeçalho e o CMake já compila com `-Wall -Wextra -Werror`. Do lado do client, a
AC-004 já cobre os dois sentidos em `test_issp_protocol.cpp:157-159`
(`kVectorSize - 1` e `kVectorSize + 1`), de modo que «rejeitados pelos dois
parsers» fica satisfeito com trabalho apenas no coordenador.

**Conclusão:** o achado A3 da Revisão é implementável no recorte, com duas
alterações de uma linha, um motivo de log e dois casos de teste host-native.

---

## 3. Contenção de escopo

- Nenhuma persistência, sessão, outbox, ACK do host ou mudança de schema é
  exigida pelo delta.
- Nenhuma API pública cresce; a DEC-010 não é ampliada e a ADR-0004 não muda.
- Nenhuma reserva estática é tocada; a DEC-012 permanece satisfeita.
- Nenhum arquivo do recorte de deep sleep é afetado; a DEC-011 permanece.
- Nenhuma alteração no `SmartHome-Hub`, no Kconfig, no product firmware ou no
  board model.
- O trabalho de D3 fica inteiramente em `coordinator_154/`, sem dependência de
  código entre os diretórios dos dois targets.

---

## 4. Itens da seção 16 reconfrontados

Reconfrontei os itens materialmente afetados pelo delta; os demais permanecem
cobertos pela análise da v0.2.

| Item | Resultado |
|---|---|
| 3 — produtores e consumidores do payload | Confrontado; só o coordenador recusa comprimento de forma incompleta |
| 4 — construtores/parsers e limite MAC | Confrontado nos seis formatos; igualdade estrita segura, limite de 127 preservado |
| 5 — precedência do registry | Confrontado; a célula aberta da matriz 9.1 admite a regra da §8.3 |
| 6 — retorno, atomicidade e buffers da UART | Confrontado; `host_try_send_line_nowait()` já é o caminho único de evento |
| 12 — meios host-native versus H2 físico | Confrontado; D3 é integralmente host-native |
| 14 — sequência sem espera, consulta, escrita, cache e ACK | Confrontado; idêntica para conhecido e desconhecido |

Itens 1, 2, 7, 8, 9, 10, 11 e 13 não são tocados pelo delta e permanecem como
analisados na v0.2.

---

## 5. Observações não bloqueantes

**O1 — a seção 4 não acompanhou a seção 8.3.** A lista de fora de escopo ainda
exclui «política de tráfego operacional de origem desconhecida durante
commissioning», enquanto a §8.3 passa a prescrever a aceitação local e o ACK
exatamente desse tráfego. A §8.3 resolve a ambiguidade em prosa — a decisão de
admitir continua na especificação do registry, e este contrato governa a
aceitação local — mas a redação da §4 ficou mais ampla que a exclusão real.
Recomendo estreitá-la para a política de **admissão**. Não bloqueante: a cláusula
específica é inequívoca e o Implementador não tem alternativa razoável.

**O2 — v1 e truncamento não são distinguíveis no coordenador.** A §11 lista
«frame v1 recebido em endpoint v2» e «tamanho inválido» como diagnósticos
separados; o coordenador reporta `frame_too_short` para os dois
(`main.c:254`). É anterior à v0.2 e não foi criado pelo delta, mas D3 abre
justamente esse código. Se o Arquiteto quiser a §11 plenamente honrada no
coordenador, este é o momento barato de fazê-lo; caso contrário, permanece
lacuna declarada.

**O3 — a verificação de comprimento existe em duas cópias.** `iot154_packet.h` e
`diagnostic_extract_mac()` duplicam a regra. Se apenas uma mudar, o diagnóstico e
a decisão discordam e a AC-004 pode passar enquanto a produção regride. Não é
defeito de especificação; é restrição de implementação a registrar no relatório
do estágio 3.

**O4 — contenção de lock alcança agora o tráfego de commissioning.**
`host_send_line()` toma o lock com `portMAX_DELAY` e bloqueia dentro de
`uart_write_bytes()`; com a §8.3 estendida, um evento de origem desconhecida pode
falhar com `uart_lock_busy` enquanto uma linha de gateway, ACK ou erro drena. A
§8.2 já declara lock ocupado como falha observável e retryable, então o contrato
cobre o caso; registro que o risco, antes restrito à origem conhecida, passa a
alcançar a janela de ingresso.

**O5 — A4, A5 e A6 da Revisão continuam abertos.** O delta não os toca: o oráculo
de concorrência da AC-003, a parte UART da AC-007 e a ausência de revalidação de
identidade em `publishReport()` permanecem como decisões do Arquiteto. Nenhum
deles bloqueia a v0.3, porque nenhum foi alterado por ela.

**O6 — a fonte analisada não está commitada, e o mapa está defasado.** A §16 exige
confronto «na revisão exata desta fonte»; a v0.3 está apenas na árvore de
trabalho, junto com o parágrafo de Autoria no `EKOM-CHANGELOG.md`. Um `Ready`
sobre conteúdo não commitado não se vincula a hash. Recomendo commitar a Autoria
e registrar o hash antes de tratar este `Ready` como gate satisfeito. Na mesma
linha, `docs/rfc/KNOWLEDGE-MAP.md:40` ainda descreve a identidade de reports como
«v0.2 `Proposed`, Pronta; implementação autorizada e `In Progress`», estado que a
abertura da v0.3 tornou obsoleto. A reconciliação do mapa cabe ao Arquiteto.

---

## 6. Experimentos e execução

Nada no delta depende de experimento para ser decidido: a segurança da igualdade
estrita foi obtida por confronto aritmético dos seis formatos de frame contra o
código dos construtores e dos parsers, não por inferência.

Quando a implementação for autorizada, permanecem intrínsecos o build canônico
ESP32-C6 do `coordinator_154` e o build host-native da suíte
`iot154_packet_host_test`. O build ESP32-H2 do `client_154` não é exigido pelo
delta, mas segue a política vigente se o Arquiteto quiser o par completo. A
execução de testes, flash, monitor e hardware continua `Not Executed` até
autorização própria; AC-005, AC-013 e a regressão de transporte da AC-016 seguem
sem evidência física, como na v0.2.

---

## 7. Classificação e próxima ação

**Pronta** [`Ready`] para a v0.3, com as seis observações da seção 5 declaradas
não bloqueantes.

Próxima ação sugerida ao Arquiteto:

1. commitar a Autoria da v0.3 e registrar o hash ao qual este `Ready` se vincula
   (O6);
2. decidir O1 e O2 na Autoria, se quiser incorporá-las antes da ordem;
3. emitir ordem explícita de implementação da v0.3, cujo trabalho de código se
   resume a D3 no `coordinator_154`.

Não declaro `Done`, aprovação, reprovação, promoção ou integração.
