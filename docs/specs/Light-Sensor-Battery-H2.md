# Sensor de luminosidade ESP32-H2 — percentual direto do ADC

**ID:** `EKOM-LIGHT-001`

**Versão:** 0.2

**Estado:** Rascunho; precisão publicada e cadência de operação em definição;
implementação não iniciada.

**Escopo:** client ESP32-H2 com LDR e ADC, capability de luminosidade e tradução
no coordenador ESP32-C6. A política adaptativa da v0.1 foi retirada por decisão
do Arquiteto em 08/09/2026.

## 1. Objetivo e valor publicado

O valor de luminosidade deriva diretamente da leitura ADC, conforme o sketch
fornecido pelo Arquiteto:

```text
percentual = 100 × raw / 4095
```

A divisão deve preservar a parte fracionária antes da formatação ou do
arredondamento de saída. O percentual expressa a fração da escala digital do
ADC; não representa lux nem percentual entre extremos medidos da montagem.
Não pressupõe resposta linear à iluminação física.

Não há `darkRaw`, `brightRaw`, procedimento de calibração de dois pontos,
inversão por extremos calibrados ou dependência de medições no escuro e sob
luz forte para definir essa fórmula. `raw = 0` representa 0% e `raw = 4095`
representa 100%, por definição da escala solicitada.

A precisão transmitida está em definição na seção 6. O log de uma casa decimal
do sketch não determina, sozinho, a representação no protocolo ISSP.

## 2. Montagem e aquisição

O circuito confirmado permanece:

```text
3,3 V → LDR → GPIO2 → resistor de 10 kΩ → GND
```

- Target ESP32-H2; GPIO2 corresponde a ADC1, canal 1.
- Usar `ADC_UNIT_1`, `ADC_CHANNEL_1`, `ADC_ATTEN_DB_12`, ULP desabilitado e
  resolução de 12 bits, com domínio bruto de 0 a 4095.
- O `ADC_BITWIDTH_DEFAULT` do sketch seleciona a largura máxima suportada;
  no H2 usado pelo projeto essa largura é 12 bits. A configuração explícita
  `ADC_BITWIDTH_12` preserva o denominador contratado.
- Cada aquisição faz uma leitura ADC oneshot, como no sketch. A média de
  16 amostras separadas por 5 ms da v0.1 deixa de ser requisito.
- Uma leitura com erro ou valor fora do domínio não gera report, não publica
  zero artificial e não reaproveita leitura antiga.
- Cada aquisição válida tenta admitir um report, inclusive quando o percentual
  não mudou. Preservar os contratos vigentes de admissão, fila, retry e ACK;
  tentativa não equivale a entrega garantida ao host.
- Diagnóstico local apresenta bruto e percentual calculado com uma casa
  decimal, como no sketch; não apresenta estado adaptativo.

O mapeamento e a resolução são sustentados pelos headers
`soc/adc_channel.h`, `soc/soc_caps.h` e `hal/adc_types.h` do ESP-IDF 6.0.1.
O sketch é a referência para a aquisição e o cálculo; seu loop de demonstração
não substitui a organização do firmware nem o lifecycle da fachada.

## 3. Operação sem estados adaptativos

Eliminar ACTIVE, TRANSITION e NIGHT_SLEEP, thresholds de entrada/saída,
comparação de delta, contagem/duração de estabilidade, crescimento do intervalo
e histórico de luminosidade retido em RTC. Nenhum desses elementos participa
da publicação ou da escolha do próximo intervalo. Não gravar histórico de
luminosidade em NVS.

O sketch demonstra leitura periódica com espera de 1.000 ms. A escolha entre
essa operação contínua e deep sleep com intervalo fixo está pendente na seção
6. A retirada da política adaptativa não altera implicitamente o deep sleep
dos demais produtos.

`SmartSysApp` permanece responsável pelo lifecycle. O produto entrega seus
parâmetros; o behavior integra aquisição e publicação aos contratos existentes.
Não criar outro dono de rádio, retry, factory reset ou entrada em deep sleep.
Preservar falhas observáveis e descarte da aquisição inválida; esta mudança de
fórmula não contrata reboot por erro ADC por copiar `ESP_ERROR_CHECK` do sketch.

## 4. Organização, identidade e coordenador

- Produto em `client_154/main/firmwares/`; board em `client_154/main/boards/`.
- Board declara ADC, atenuação e fatos físicos. Produto define composição e
  parâmetros de aquisição. Kconfig escolhe a composição.
- Capability pela fachada `SmartSysApp`, token somente leitura e behavior em
  `issp_behaviors`, seguindo as fronteiras do contrato de engenharia v0.1.
- Endpoint 1 permanece congelado para luminosidade; rejeitar endpoint zero,
  duplicado e comandos dirigidos à capability, pelo behavior correspondente.
- Evento 6 permanece reservado na ADR-0005 para luminosidade. A implementação
  deve reconciliar a guarda do registro, a definição e a tradução no
  coordenador; a guarda vigente ainda exige cinco tipos.
- Coordenador apresenta `Light Sensor`, preservando envelope JSON, nome da
  capability, identidade de cada admissão, deduplicação e ACK. A precisão
  numérica transmitida será fechada pela decisão da seção 6.
- Não criar dependência de código entre client e coordenador.

## 5. Autoridades e versões

Esta v0.2 substitui o contrato de aquisição, normalização e política de
luminosidade do rascunho v0.1, por ordem do Arquiteto. A v0.1 e sua análise são
históricas, preservadas no Git e em `docs/reports/`.

Nova especificação para esta capability e composição. A emenda proposta a
`Firmware-Variants-Menuconfig.md` continua limitada à composição do produto.
A proposta anterior de intervalo adaptativo em segundos e estado RTC em
`Client-Deep-Sleep.md` deixa de integrar o recorte. A relação final com sono
antecipado depende da escolha operacional da seção 6.

Preservar ADR-0001 a ADR-0004, commissioning e identidade de reports nos
comportamentos não alterados. A ADR-0005 continua alocando evento 6 e domínio
inteiro 0–100; mudança dessa representação exigirá decisão normativa explícita,
sem reinterpretar silenciosamente o campo de um byte.

O contrato de engenharia v0.1 e o alcance habilitado de
`docs/rfc/REPOSITORY-READINESS.md` continuam aplicáveis. Esta revisão não
classifica implementabilidade nem autoriza implementação, testes ou hardware.

## 6. Decisões ainda necessárias

| Decisão | Alternativas apresentadas ao Arquiteto | Impacto |
|---|---|---|
| Cadência e energia | Leitura/publicação a cada 1 segundo sem deep sleep, ou deep sleep com intervalo fixo a definir | Fecha aquisição, próximo ciclo e relação com o lifecycle |
| Precisão no report | Percentual inteiro 0–100 arredondado, ou uma casa decimal | Inteiro preserva domínio do evento e campo atual; decimal exige definir representação e impacto no contrato wire |

A pergunta anterior sobre obter `darkRaw` e `brightRaw` fica superada pela
mudança de fórmula; nenhum recorte diagnóstico de calibração é necessário
para satisfazer esta versão.

## 7. Critérios de aceite

| Critério | Resultado observável | Meio |
|---|---|---|
| Normalização | `raw=0` → 0%; `raw=819` → 20%; `raw=4095` → 100%; `raw=2048` → aproximadamente 50,01221% antes da quantização | Inspeção e cálculos conhecidos |
| Aquisição | Uma leitura por aquisição; sem média, extremos calibrados ou histórico adaptativo | Inspeção e observação ADC quando autorizada |
| Falha ADC | Leitura inválida não gera report nem valor artificial; erro observável | Inspeção e falha controlada quando autorizada |
| Publicação | Aquisições válidas tentam reportar mesmo percentual inalterado; cada admissão conserva sua identidade | Inspeção e H2/C6/host quando autorizados |
| Sem estados | Ausência de thresholds, transições, estabilidade, crescimento noturno e retenção RTC da luminosidade | Inspeção do delta |
| Integração | Host recebe Light Sensor, endpoint estável e valor conforme precisão a definir; comandos recusados | Inspeção e H2/C6/host quando autorizados |
| Construção | H2 e C6 afetados compilam; composições existentes preservadas | Builds canônicos da implementação autorizada e inspeção |

Os critérios de cadência, ciclo de energia e quantização final serão completados
após as decisões da seção 6. O rascunho não está sendo apresentado como Ready.
Nenhum artefato de teste automatizado integra este recorte. Execução/coleta de
testes, flash, monitor e hardware seguem autorização própria. Critérios sem
evidência permanecem não executados; build não comprova comportamento físico.
