# Relatório de implementação — identidade de reports (v0.3)

**Classe da fonte:** Relatório

**Papel:** Engenheiro Implementador

**Especificação:** `docs/specs/ISSP-Report-Identity.md`, v0.3

**Revisão confrontada:** v0.3, commit `9687287` quanto ao conteúdo normativo;
`4e3256a` reconciliou apenas campos de estado. Nenhuma cláusula normativa mudou
entre a revisão analisada e a implementada.

**Análise de entrada:**
`docs/reports/report-identity/analysis/2026-08-12-v03-implementability-analysis.md`,
classificação `Pronta` para o conteúdo normativo de `9687287`

**Implementação anterior:**
`docs/reports/report-identity/implementation/2026-08-12-v02-implementation.md`

**Estado:** `In Progress` — delta implementado e builds concluídos; execução de
testes, flash, monitor e hardware permanecem `Not Executed`

**Data:** 12/08/2026

---

## 1. Condições de entrada

| Condição | Situação |
|---|---|
| Análise `Ready` aplicável à versão corrente | presente (v0.3, `9687287`) |
| Ordem explícita do Arquiteto para implementar a v0.3 | presente |
| Branch derivada da `main`, árvore limpa no início | `spec/client-deep-sleep`, limpa |

O estado `In Progress` foi registrado mecanicamente na especificação como
primeiro efeito desta atuação, conforme `REGRAS-COMUNS.md` §2.1; não foi
exigido campo de promoção adicional.

## 2. Recorte implementado

O delta normativo da v0.2 para a v0.3 tem três itens. Conforme a análise, dois
já estavam satisfeitos pela implementação vigente e foram confirmados por
leitura, sem mutação; o terceiro produziu todo o trabalho de código.

| Delta | Ação desta atuação |
|---|---|
| D1 — exemplo do `event_id` (§8.4) | Nenhuma; confirmado que `format_device_id()` e `iot154_format_event_id()` já produzem a forma exemplificada, e que o AC-011 já asserta a string canônica |
| D2 — aceitação local sem espera para origem desconhecida (§8.3) | Nenhuma; confirmado que `report_data_policy_process()` usa o mesmo `emit_event` para conhecido e desconhecido, ACKando somente após a aceitação integral |
| D3 — rejeição de payload excedente (§6.1, AC-004, §14.1) | Implementada nas duas cópias da verificação e coberta por dois casos host-native |

### 2.1 D3 — comprimento de frame estrito no coordenador

O client já recusava comprimento nos dois sentidos, no transporte e em cada
decodificador. O coordenador recusava apenas truncamento, em duas cópias da
mesma condição. Ambas passaram a exigir igualdade estrita:

- `coordinator_154/main/iot154_packet.h:299` —
  `iot154_parse_frame_info()` troca `frame[0] < mac_header_len + sizeof(*packet)
  + IOT154_FCS_LEN` por `!=`. É o ponto que governa a decisão de produção: um
  frame mais longo cujos vinte primeiros bytes de payload formassem um v2 válido
  deixa de ser aceito e não pode mais gerar evento.
- `coordinator_154/main/main.c:355` — `diagnostic_extract_mac()` aplica a mesma
  igualdade e distingue os dois motivos: `payload_truncated` abaixo do esperado
  e `payload_excessive` acima. O motivo alimenta o log
  `invalid MAC frame discarded reason=%s` já existente, atendendo à §11 quanto a
  «tamanho inválido» sem criar estrutura nova.

As duas cópias foram alteradas na mesma atuação, conforme a observação O3 da
análise: se apenas uma mudasse, o diagnóstico e a decisão divergiriam e a
AC-004 poderia passar enquanto a produção regredisse.

A verificação de mínimo anterior ao parsing (`iot154_packet.h:228`) foi
preservada. Ela continua recusando frames v1 de 12 bytes de payload antes de
qualquer decodificação, então o corte de versão da §6.1 não depende da nova
igualdade.

Nenhum frame legítimo passa a ser rejeitado: nos seis formatos dos dois targets
vale `frame[0] == mac_header_len + 20 + 2`, como a análise confrontou
aritmeticamente contra os construtores de
`components/issp_transport_154/src/issp154_mac_frame.c` e de `iot154_packet.h`.

### 2.2 Evidência host-native contratada

`coordinator_154/test_apps/iot154_packet_host_test/test_iot154_packet.c` recebeu
o caso `frame_lengths_other_than_the_payload_are_rejected()`, exigido pela §14.1
e vinculado à AC-004. Ele constrói um frame curto/curto válido pelo construtor
de produção e verifica, sobre `iot154_parse_frame_info()`:

- aceitação no comprimento exato, com `report_id` decodificado;
- rejeição com um byte a menos que o payload fixo;
- rejeição com um byte a mais;
- aceitação novamente ao restaurar o comprimento, comprovando que a rejeição
  vem do comprimento e não de outro campo.

Nenhuma suíte paralela foi criada e nenhum teste fora do recorte foi alterado.
Do lado do client, a AC-004 já cobre os dois sentidos em
`test_issp_protocol.cpp:157-159`, de modo que «rejeitados pelos dois parsers»
fica satisfeito sem trabalho no diretório do client.

## 3. Preservação e contenção

- Nenhuma API pública mudou; a DEC-010 e a ADR-0004 permanecem intactas.
- Nenhuma reserva estática foi tocada; a DEC-012 permanece satisfeita.
- Nenhum arquivo do recorte de deep sleep foi alterado; a DEC-011 permanece.
- Nenhuma alteração em `components/`, `client_154/`, Kconfig, product firmware,
  board model, schema NVS ou `SmartHome-Hub`.
- Nenhuma dependência de código nova entre os diretórios dos dois targets.
- `coordinator_154/sdkconfig` foi restaurado após o `set-target`, e os
  diretórios de build temporários foram removidos.

## 4. Evidências

### 4.1 Builds canônicos — executados

| Alvo | Target | Comando | Resultado | Código de saída |
|---|---|---|---|---|
| `coordinator_154` | esp32c6 | `idf.py -B build_impl_c6 set-target esp32c6` seguido de `idf.py -B build_impl_c6 build` | sucesso | 0 |

Ambiente: ESP-IDF v6.0.1 em `~/.espressif/v6.0.1/esp-idf`. Binário
`central_154.bin` com 0x48f70 bytes, 71% da partição livre.

`client_154`, `examples/issp_minimal_client` e as test apps de H2 não foram
reconstruídos: o delta não alterou nenhum arquivo lido por eles. A
`device_registry_test` do C6 também não referencia `iot154_packet.h`.

### 4.2 Suítes host-native — construídas, não executadas

| Suíte | Critérios cobertos | Build | Código de saída | Execução |
|---|---|---|---|---|
| `coordinator_154/test_apps/iot154_packet_host_test` | AC-004 e AC-015 lado coordenador, AC-011 formatação | sucesso | 0 | `Not Executed` |
| `coordinator_154/test_apps/report_data_policy_host_test` | AC-006 a AC-010 e janela do AC-012 | sucesso | 0 | `Not Executed` |
| `coordinator_154/test_apps/device_registry_policy_host_test` | regressão da política de registry | sucesso | 0 | `Not Executed` |

As duas últimas foram construídas por consumirem `iot154_packet.h` ou o mesmo
diretório `main/`, para confirmar que a igualdade estrita não quebra sua
compilação. Todas usam `-Wall -Wextra -Werror`. Nenhuma foi executada: não há
autorização de execução nesta atuação.

### 4.3 Não coberto por evidência nesta atuação

- **AC-005**, **AC-013** e a regressão de transporte do **AC-016** continuam
  sem evidência física, como na v0.2.
- A rejeição de payload excedente tem teste host-native construído, não
  executado; nenhuma recepção de rádio real foi observada.
- D1 e D2 foram confirmados por leitura de código e de suíte existente, não por
  execução.

## 5. Limitações, desvios e observações devolvidas

1. **Nenhum desvio da especificação.** Todo o trabalho ficou dentro do recorte
   de D3, no dono natural do código.
2. **O2 da análise não foi implementada.** O coordenador continua reportando
   `frame_too_short` tanto para frame v1 quanto para truncamento abaixo do
   mínimo, embora a §11 os liste como diagnósticos separados. A lacuna é
   anterior à v0.2, não foi criada pelo delta e sua correção não está no recorte
   autorizado; permanece decisão do Arquiteto. Esta atuação distinguiu apenas o
   par truncado/excedente, que é o que a v0.3 introduziu.
3. **O1 permanece aberta.** A seção 4 da especificação ainda exclui, de forma
   mais ampla que a §8.3, «política de tráfego operacional de origem desconhecida
   durante commissioning». É defeito de redação normativa; a cláusula específica
   é inequívoca e não deixou alternativa ao Implementador.
4. **O6 quanto ao mapa permanece aberta.** `docs/rfc/KNOWLEDGE-MAP.md` foi
   reconciliado pelo Arquiteto em `4e3256a`; esta atuação não altera autoridade,
   contenção nem relação material, então não reconcilia o mapa por conta própria.
5. **O4 permanece registrada.** A contenção do lock UART alcança o tráfego de
   commissioning desde a v0.2; o delta não a altera.

## 6. Próxima etapa

Revisão do delta v0.3 contra a especificação e este relatório. Não declaro
aprovação, conclusão nem integração; a suficiência das evidências e o estado
final permanecem com o Arquiteto.
