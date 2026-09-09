# Sensor de luminosidade ESP32-H2 — percentual direto do ADC

**ID:** `EKOM-LIGHT-001`

**Classe da fonte:** Normativa

**Versão:** 0.4

**Estado:** Concluída [`Done`] por decisão de Marcelo Miranda, após validação em hardware e ordem explícita de integração na `main`. Registro UTC: 09/09/2026.

**Entrega v0.4:** implementação concluída e encaminhada à Revisão; sete builds canônicos H2 com saída 0. Evidências e limitações em `docs/reports/light-sensor-battery-h2/implementation/2026-09-09T015214Z-v0.4-9eba30c9-implementation.md`. Testes e hardware não executados pelo agente naquela etapa. Validação posterior em hardware declarada pelo Arquiteto e encerramento registrados em `docs/reports/light-sensor-battery-h2/validation/2026-09-09T022704Z-v0.4-a89aef87-human-validation.md`.

**Histórico v0.3:** Concluída [`Done`] por decisão de Marcelo Miranda, Arquiteto,
em 08/09/2026, com autorização de integração na `main`.

**Evidências históricas preservadas da v0.3:** análise `Ready` em
`docs/reports/light-sensor-battery-h2/analysis/2026-09-08T145845Z-419138e-99aeff65-implementability-analysis.md`;
implementação e sete builds concluídos em
`docs/reports/light-sensor-battery-h2/implementation/2026-09-08T151247Z-v0.3-346114b7-implementation.md`.
A decisão humana encerra o workflow com essas evidências e limitações;
não foi produzida revisão independente nem executados testes, flash, monitor
ou hardware nesta atuação. Encerramento registrado em `EKOM-CHG-0012`.

**Escopo:** client ESP32-H2 com LDR e ADC, capability de luminosidade, deep
sleep com intervalo fixo configurável, medição opcional de bateria na board
Light Sensor H2 e tradução no coordenador ESP32-C6. A política adaptativa da v0.1 foi retirada por decisão
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

Publicar um inteiro de 0 a 100, arredondado para o inteiro mais próximo, com
meio para cima. O arredondamento é aplicado somente ao percentual final; não
truncar a divisão antes dele. Uma expressão inteira equivalente para `raw`
válido é `(100 × raw + 2047) / 4095`, com divisão inteira e intermediário de
largura suficiente. Não multiplicar por dez nem transmitir fração no wire.
O diagnóstico de uma casa decimal permanece local.

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

## 3. Uma aquisição por despertar e deep sleep fixo

Eliminar ACTIVE, TRANSITION e NIGHT_SLEEP, thresholds de entrada/saída,
comparação de delta, contagem/duração de estabilidade, crescimento do intervalo
e histórico de luminosidade retido em RTC. Nenhum desses elementos participa
da publicação ou da escolha do próximo intervalo. Não gravar histórico de
luminosidade em NVS.

Em cada despertar do deep sleep que alcance o início operacional da capability,
efetuar uma única aquisição e, se válida, uma tentativa de admissão do report.
O primeiro boot e os demais resets que alcancem esse mesmo início operacional
seguem a mesma regra. Não iniciar amostragem periódica a cada segundo nem fazer
novas leituras durante a espera por ACK. Retries do report admitido permanecem
sob o executor existente, com a mesma leitura e identidade dessa admissão.

O produto habilita deep sleep com wakeup por timer. O intervalo é fixo durante
a operação da composição e configurável no firmware pelo menu `App Client`,
em minutos, reutilizando `CONFIG_IOTSMARTLINK154_WAKEUP_INTERVAL_MINUTES` com
**default de 15 minutos** para luminosidade. Kconfig seleciona o valor e o
produto o entrega à fachada; componentes compartilhados não leem esse símbolo.
Validar intervalo positivo e conversão dentro do limite aceito pelo timer,
conforme `Client-Deep-Sleep.md` v0.11. O tempo de sono é contado desde a entrada
em deep sleep; aquisição, commissioning, transmissão e encerramento acrescentam
tempo entre reports. Não mudar o intervalo em função da leitura ou do erro ADC.

`SmartSysApp` permanece dona do lifecycle, deadline e sequência terminal.
Integrar a evidência de admissão da luminosidade à prontidão para sono
antecipado. Uma aquisição válida ainda não admitida, falha ADC ou ausência de
report não constitui essa evidência. Depois de admitido, preservar fechamento
da admissão, quiescência, espera por reports pendentes e ACK, limite temporal e
arbitragem com factory reset. Se houver falha ADC ou recusa da tentativa de
admissão, não refazer a aquisição nesse boot; aguardar o caminho forçado pelo
deadline e usar o mesmo intervalo configurado para o próximo despertar.

Preservar a janela máxima acordada configurável já oferecida pela composição
(`CONFIG_IOTSMARTLINK154_MAX_AWAKE_TIME_SECONDS`, default vigente de 30 s),
entregue pela aplicação à fachada. Preservar as exceções existentes: falha de
configuração/plataforma que impeça criar o lifecycle não promete entrada em
sleep; falha no preparo do wakeup interrompe a sequência terminal conforme o
contrato vigente. Não tratar o deadline como garantia absoluta do tempo físico
acordado além dos limites de `Client-Deep-Sleep.md`.

Não criar outro dono de rádio, retry, factory reset ou entrada em deep sleep.
Preservar falhas observáveis e descarte da aquisição inválida; esta mudança não
contrata reboot por erro ADC por copiar `ESP_ERROR_CHECK` do sketch. Defaults,
fontes de wakeup e comportamento dos demais produtos permanecem preservados.

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
  coordenador; a guarda da baseline analisada exigia cinco tipos.
- Coordenador apresenta `Light Sensor (%)`, preservando envelope JSON, nome da
  capability, identidade de cada admissão, deduplicação e ACK. O valor é o
  inteiro 0–100, formatado em base decimal, sem casa fracionária nem símbolo `%`,
  mantendo o tipo do campo no envelope JSON vigente.
- Não criar dependência de código entre client e coordenador.

## 5. Autoridades e versões

A v0.3 fechou a cadência e a precisão da v0.2, por decisão do Arquiteto.
Mantém o percentual direto ADC e a retirada dos estados da v0.1. Versões e
análise anteriores são históricas, preservadas no Git e em `docs/reports/`.

Nova especificação para esta capability e composição. A emenda proposta a
`Firmware-Variants-Menuconfig.md` continua limitada à composição do produto.
A proposta anterior de intervalo adaptativo em segundos e estado RTC deixa de
integrar o recorte. **Amends `Client-Deep-Sleep.md` v0.11** somente para incluir
a evidência de admissão inicial da luminosidade no sono antecipado, conforme
seção 3. Preservar configuração e validação de timer em minutos, dono do
lifecycle, quiescência, ACK, deadline e arbitragem. **Amends
`Client-SDK-Configurable-Features.md` v0.1** para oferecer ao novo produto os
parâmetros vigentes de intervalo do timer e janela acordada, sem criar operação
periódica contínua para luminosidade nem alterar as composições existentes.

Preservar ADR-0001 a ADR-0004, commissioning e identidade de reports nos
comportamentos não alterados. A ADR-0005 continua alocando evento 6 e domínio
inteiro 0–100; o arredondamento decidido preserva essa representação e o
domínio semântico. A representação de um byte é histórica: na baseline
tipada, `ISSP-Typed-Values.md` v0.1 e a ADR-0006 governam o transporte
Int32/Float32. Esta v0.4 preserva produtores inteiros e não muda o wire.
A publicação fracionária continua pertencendo a
`Fractional-Percentage-Reports.md`, fora desta revisão.

O contrato de engenharia v0.1 e o alcance habilitado de
`docs/rfc/REPOSITORY-READINESS.md` continuam aplicáveis. Esta revisão não
classifica implementabilidade nem autoriza implementação, testes ou hardware.

## 6. Decisões confirmadas

Em 08/09/2026, o Arquiteto determinou:

- leitura e publicação em cada despertar do deep sleep;
- intervalo de deep sleep configurável, com default de 15 minutos;
- publicação do percentual inteiro de 0 a 100, arredondado.

Essas decisões encerram as alternativas abertas na v0.2. A aquisição única
por boot operacional e o lifecycle descritos na seção 3 delimitam as falhas e
as tentativas de publicação. `darkRaw`/`brightRaw` e recorte diagnóstico de
calibração continuam fora do contrato. As análises e a implementação da v0.3 permanecem históricas nos relatórios
indicados acima; sua autorização não se estende automaticamente à v0.4.

Na mesma data, o Arquiteto confirmou que a board de luminosidade utiliza
exatamente o hardware e a funcionalidade de bateria da board de porta,
determinou acrescentar esse recurso à Light Sensor H2 e autorizou registrar
o rascunho e sua análise de implementabilidade. A seção 8 incorpora esse recorte.

## 7. Critérios de aceite

| Critério | Resultado observável | Meio |
|---|---|---|
| Normalização | Reports: `raw=0` → 0; `raw=20` → 0; `raw=21` → 1; `raw=819` → 20; `raw=2048` → 50; `raw=4095` → 100 | Inspeção e cálculos conhecidos |
| Aquisição | Uma única leitura no início operacional de cada boot/despertar; sem nova leitura durante espera por ACK, média ou extremos calibrados | Inspeção e observação ADC quando autorizada |
| Falha ADC | Leitura inválida não gera report nem valor artificial; erro observável; caminho forçado conserva intervalo configurado | Inspeção e falha controlada quando autorizada |
| Publicação | Aquisições válidas tentam reportar mesmo percentual inalterado; cada admissão conserva sua identidade | Inspeção e H2/C6/host quando autorizados |
| Sem estados | Ausência de thresholds, transições, estabilidade, crescimento noturno e retenção RTC da luminosidade | Inspeção do delta |
| Integração | Host recebe Light Sensor (%), endpoint 1 e inteiro arredondado em base decimal; evento 6 e layout preservados; comandos recusados | Inspeção e H2/C6/host quando autorizados |
| Cadência | Configuração default arma timer de 15 minutos; configuração de 20 minutos arma 20 minutos; zero ou valor fora do limite do timer é rejeitado; não há ciclo de 1 segundo | Inspeção de configuração e observação quando autorizada |
| Lifecycle | Report admitido participa do sono antecipado; pendentes aguardam ACK até os limites vigentes; erro ADC ou recusa de admissão não habilita sono antecipado; falha de preparo de wakeup interrompe encerramento | Inspeção e observação com/sem ACK e falhas quando autorizadas |
| Construção | H2 e C6 afetados compilam; composições existentes preservadas | Builds canônicos da implementação autorizada e inspeção |

Os critérios incorporam as decisões de cadência, ciclo de energia e
quantização final e permanecem aplicáveis na v0.4, acrescidos dos critérios
da seção 8. A evidência histórica não certifica a nova composição.
Nenhum artefato de teste automatizado integra este recorte. Execução/coleta de
testes, flash, monitor e hardware seguem autorização própria. Critérios sem
evidência permanecem não executados; build não comprova comportamento físico.


## 8. Inclusão da bateria na Light Sensor H2 — v0.4

### 8.1 Requisitos e limites

- **LIGHT-BAT-001 — Recurso físico:** manter o board model Light Sensor H2 e
  seu LDR. Acrescentar `battery_measurement`, acessível pelo contrato existente
  `selectedBatteryMeasurement()`, com ADC1, canal 0, atenuação de 12 dB,
  resistor superior de 470 kΩ e inferior de 220 kΩ, como na board Battery
  Digital Sensor H2 usada pelo sensor de porta. O circuito de luminosidade
  permanece no canal 1/GPIO2. Não incorporar entrada de porta, LED ou botão
  como consequência dessa adição.
- **LIGHT-BAT-002 — Seleção e composição:** oferecer
  `CONFIG_IOTSMARTLINK154_ENABLE_BATTERY_LEVEL` ao produto Light sensor battery
  H2, com default habilitado. Quando habilitado, o produto requer o recurso
  físico de bateria e registra a capability pela fachada com dados obtidos
  da board. Quando desabilitado, não registra nível nem estado de bateria,
  não adquire seu canal ADC e não exige o recurso na validação da composição.
  Preservar a guarda CMake de recursos requeridos/oferecidos e rejeitar
  composição habilitada sem o recurso correspondente. Manter defaults dos
  outros produtos.
- **LIGHT-BAT-003 — Política e identidade:** reutilizar a capability e a
  política de bateria do produto de porta: `emptyMv=3300`, `fullMv=4150`,
  oito amostras separadas por 5 ms e `reportDeltaPercent=5`. Luminosidade
  continua no endpoint 1/evento 6; bateria usa endpoint 2/evento 3 e seu
  estado de telemetria, criado pela fachada, endpoint 3/evento 4. Preservar
  unicidade e somente leitura. Preservar cálculo, saturação, calibração,
  fallback e estados calibrado/aproximado/inerte de `Client-Battery-Level.md`
  v0.5 e `Technical-Debt-Remediation.md` v0.2. Ambos os percentuais continuam
  inteiros nesta revisão, inclusive sob transporte tipado.
- **LIGHT-BAT-004 — Cadência e coexistência:** a bateria faz uma medição
  inicial por boot operacional, com `samplePeriodMs=0`, e tenta publicar
  o primeiro percentual válido mesmo sem variação. O produto sempre habilita
  deep sleep, independentemente do símbolo genérico de opt-in dos produtos
  digitais. Não oferecer nem consumir intervalo periódico de bateria para
  luminosidade. Os dois canais devem poder produzir suas medições no mesmo
  boot sem disputa provocada pela composição pelo ADC1; preservar a aquisição
  única e a liberação do ADC da luminosidade. Não criar novo gerenciador de
  ADC, tarefa periódica ou dono de lifecycle.
- **LIGHT-BAT-005 — Falhas e energia:** preservar as falhas da bateria
  contratadas nas fontes acima: falha de configuração deixa a telemetria
  inerte e observável, sem impedir a função principal; erro de aquisição ou
  amostra inválida não fabrica percentual nem interrompe a luminosidade.
  A bateria não passa a ser evidência obrigatória de admissão para sono
  antecipado. Seus reports admitidos e os de estado participam da drenagem
  vigente. Timer, deadline, ACK, retries, falhas de luminosidade e exceções
  de encerramento permanecem como nas seções anteriores.

### 8.2 Relações normativas

Esta v0.4 é **New** para a oferta de bateria pela Light Sensor H2 e **Amends**
`Firmware-Variants-Menuconfig.md` exclusivamente na composição desse produto e
recursos dessa board. **Amends** `Client-SDK-Configurable-Features.md` v0.1
somente na elegibilidade da opção de bateria e na ausência do intervalo
periódico para esse produto com deep sleep permanente. Preserva as políticas
vigentes dos produtos de porta e presença.

**Preserva** o contrato genérico de `Client-Battery-Level.md` v0.5, a
observabilidade de `Technical-Debt-Remediation.md` v0.2, `Client-Deep-Sleep.md`
v0.11 com a emenda da seção 3, as ADRs de fronteiras/composição/identidade e o
transporte governado pela ADR-0006 e `ISSP-Typed-Values.md` v0.1. A mudança
não depende de percentuais fracionários, não os implementa e não reclassifica
sua especificação. Não há nova API compartilhada, política de compatibilidade,
projeto elétrico ou mudança no coordenador contratada pela inclusão de bateria.

### 8.3 Critérios adicionais de aceite

| Critério / requisito | Resultado observável | Meio |
|---|---|---|
| Recurso / LIGHT-BAT-001 | Board Light Sensor H2 oferece medição da bateria com os mesmos parâmetros físicos da board de porta, preservando o LDR | Inspeção de board e declaração de recursos |
| Seleção / LIGHT-BAT-002 | Bateria habilitada por default; configuração desligada contém somente luminosidade; recurso ausente é rejeitado pela guarda existente | Inspeção e builds canônicos H2 com bateria ligada/desligada |
| Identidade e política / LIGHT-BAT-003 | Endpoints 1/2/3 e eventos 6/3/4, política e estados existentes, sem publicação fracionária | Inspeção; observação H2/C6/host quando autorizada |
| Cadência e ADC / LIGHT-BAT-004 | Luminosidade e bateria podem medir no mesmo boot; período de bateria zero, sem opção periódica; aquisição de luminosidade permanece única | Inspeção de configuração e lifetime; observação quando autorizada |
| Falhas e drenagem / LIGHT-BAT-005 | Falha de bateria não suprime função principal nem fabrica nível; reports admitidos drenam sob lifecycle existente, sem novo requisito de admissão para dormir | Inspeção; falhas controladas e observação quando autorizadas |

Nenhum artefato de teste automatizado novo ou alterado integra a v0.4.
Os builds canônicos da implementação autorizada seguem a seção 9 do contrato
aprovado e `Repository-Test-Execution-Policy.md`, cobrindo o produto H2 com
bateria ligada/desligada e os demais consumidores afetados pelo delta real.
Build não demonstra a coexistência física em bancada. Testes, coleta, flash,
monitor e hardware dependem de autorização própria; não foram autorizados
pela ordem de registro e análise.
