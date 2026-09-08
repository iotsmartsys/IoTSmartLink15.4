# Sensor de luminosidade a bateria ESP32-H2

**ID:** `EKOM-LIGHT-001`

**Versão:** 0.1

**Estado:** Rascunho; calibração pendente; implementação não iniciada.

**Escopo:** client ESP32-H2 com LDR e ADC, capability de luminosidade,
deep sleep adaptativo e tradução no coordenador ESP32-C6.

## Objetivo e decisões confirmadas

Medir luminosidade relativa e publicar percentual inteiro entre 0 e 100 pelo
IoTSmartLink 15.4. O valor ADC bruto fica disponível somente no diagnóstico
local. A automação realiza toda classificação semântica. ACTIVE, TRANSITION e
NIGHT_SLEEP são estados da política de amostragem.

## Aquisição e normalização

- Uma aquisição por boot operacional; média de 16 amostras,
  separadas por 5 ms. Falha em uma amostra invalida a janela inteira.
- Configurar `darkRaw` e `brightRaw` no firmware a partir de medições da montagem
  real, respectivamente no escuro e em iluminação forte representativa do local.
- Calcular `100 * (raw - darkRaw) / (brightRaw - darkRaw)`, saturar entre 0 e 100
  e arredondar com meio para cima. Preservar sinais e precisão intermediária.
- Admitir polaridade invertida, com `brightRaw < darkRaw`. Rejeitar pontos iguais
  ou fora do domínio do ADC configurado.
- O percentual representa posição no intervalo elétrico calibrado, sem unidade
  lux e sem pressupor resposta linear à iluminação física.
- Cada aquisição válida tenta admitir um report, inclusive com percentual
  inalterado. Falha de leitura não publica zero nem reaproveita leitura antiga.
- Diagnóstico local mostra bruto, percentual, estado e próximo intervalo.

## Política adaptativa

Valores abaixo são defaults confirmados pelo Arquiteto, configuráveis no firmware;
não são constantes dos componentes compartilhados.

| Parâmetro | Default confirmado |
|---|---:|
| Entrada em escuro estável | <= 5% |
| Saída de NIGHT_SLEEP | >= 8% |
| Faixa ACTIVE | >= 40% |
| Delta significativo | >= 3 pontos percentuais |
| Estabilidade mínima | 3 medições e 60 segundos |
| Intervalo TRANSITION | 30 segundos |
| Intervalo ACTIVE | 300 segundos |
| Intervalo inicial NIGHT_SLEEP | 300 segundos |
| Fator de crescimento NIGHT_SLEEP | 2 |
| Intervalo máximo NIGHT_SLEEP | 1.800 segundos |

1. Delta é a diferença absoluta entre dois percentuais válidos consecutivos.
   Sem histórico, iniciar em TRANSITION. A primeira leitura escura pode iniciar
   uma sequência com contagem 1 e duração zero; não comprova estabilidade.
2. Fora de NIGHT_SLEEP, uma leitura <= limite de entrada com delta abaixo do
   significativo prolonga a sequência escura. Exigir simultaneamente contagem
   e duração mínimas entre observações para entrar em NIGHT_SLEEP. Uma leitura
   incompatível reinicia a sequência; se escura, pode iniciar outra sequência.
3. Na entrada em NIGHT_SLEEP, usar o intervalo inicial. Nas aquisições seguintes,
   com luz abaixo do limite de saída e delta abaixo do significativo, permanecer
   em NIGHT_SLEEP, multiplicar o intervalo pelo fator e saturar no máximo, sem
   transbordo. Essa permanência encerra a seleção de estado da aquisição; a
   regra 5 não se aplica a esse caso.
4. Em NIGHT_SLEEP, luz >= limite de saída ou delta significativo seleciona
   TRANSITION imediatamente e reinicia a estabilidade e o crescimento. Não
   selecionar ACTIVE na própria leitura de saída.
5. Somente quando a aquisição começou fora de NIGHT_SLEEP e não determinou
   entrada nesse estado, selecionar ACTIVE com histórico válido, luz >= limite
   ACTIVE e delta abaixo do significativo. Nos demais casos desse caminho,
   selecionar TRANSITION. As regras 3 e 4 resolvem exclusivamente permanência
   e saída quando a aquisição começou em NIGHT_SLEEP.
6. Falha ADC limpa histórico de comparação e estabilidade e seleciona
   TRANSITION. A próxima leitura válida segue o tratamento de primeira leitura.

Validar ordenação dos thresholds, contagens positivas e intervalos positivos
compatíveis com o timer. O máximo noturno limita o período de sono sem
observação; aquisição e transmissão acrescentam latência à entrega.

## Retenção e integração de energia

Reter em RTC somente medição anterior, estabilidade, estado e intervalo.
Validar integridade e compatibilidade com a configuração; aceitar histórico
somente em retorno válido de deep sleep por timer. Descartá-lo nos demais
resets, após factory reset ou mudança incompatível de configuração. Não gravar
histórico em NVS a cada aquisição.

`SmartSysApp` permanece responsável pelo lifecycle. Integrar leitura inicial e
admissão do report à evidência de encerramento antecipado deste produto.
Admissão não equivale a entrega: preservar espera por reports pendentes, ACK,
quiescência, deadline e arbitragem com factory reset. Sem medição válida, usar
intervalo TRANSITION no caminho forçado. Preservar o tratamento existente de
falha ao preparar wakeup. Não criar outro responsável por entrar em deep sleep.

## Organização e coordenador

- Produto em `client_154/main/firmwares/`; board em `client_154/main/boards/`.
- Board declara ADC, atenuação e recursos físicos. Produto define calibração,
  aquisição e política. Kconfig seleciona composição.
- Capability pela fachada `SmartSysApp`, token somente leitura e behavior em
  `issp_behaviors`, conforme precedentes de bateria e presença.
- Endpoint 1 para luminosidade no novo produto, com identidade
  congelada; rejeitar endpoint zero ou duplicado e comandos à capability.
- Evento 6 alocado pela emenda aceita da ADR-0005, pertencente à capability e
  não configurável pelo produto. A implementação deve atualizar a guarda do
  registro de eventos conjuntamente com a definição e tradução no coordenador.
- Coordenador traduz para `Light Sensor` e percentual decimal, preservando
  envelope JSON, nome de capability, identidade do report, deduplicação e ACK.
- Preservar layout ISSP e independência de código entre os dois targets.

## Autoridades e pendências

Nova especificação para aquisição e política de luminosidade. Emenda proposta
a `Firmware-Variants-Menuconfig.md` para a composição e a `Client-Deep-Sleep.md`
para intervalo adaptativo em segundos e admissão da luminosidade no sono
antecipado. O evento 6 está alocado pela emenda aceita da ADR-0005. Preservar ADR-0001
a ADR-0004, commissioning e ISSP-Report-Identity nos demais comportamentos.

GPIO2 e alimentação de 3,3 V foram confirmados pelo Arquiteto. GPIO2 corresponde
à unidade ADC1, canal 1 no ESP32-H2, conforme `soc/adc_channel.h` do ESP-IDF
6.0.1 e a [tabela de funções analógicas do datasheet Espressif](https://www.espressif.com/sites/default/files/documentation/esp32-h2_datasheet_en.pdf).
O circuito confirmado é `3,3 V → LDR → GPIO2 → resistor de 10 kΩ → GND`.
O board deve declarar `ADC_UNIT_1`, `ADC_CHANNEL_1`, `ADC_ATTEN_DB_12` e leitura
com `ADC_BITWIDTH_12`, domínio bruto 0–4095. Os identificadores de atenuação e
resolução existem em `hal/adc_types.h` do ESP-IDF 6.0.1. A medição de calibração
deve usar exatamente essa configuração e a mesma janela de aquisição do produto.

`darkRaw` e `brightRaw` ainda não foram medidos. Alimentação de 3,3 V e domínio
0–4095 não substituem extremos medidos: não adotar 0 e 4095 como calibração por
padrão. O circuito determina a montagem, não uma garantia de linearidade física
ou de uso de toda a faixa do ADC.
A análise de implementabilidade deve registrar classificação separada; este
documento não declara Ready nem autorização de implementação.

## Qualificação e decisões em aberto

O repositório está habilitado no alcance aplicável sob EKOM 5.0, conforme
`docs/rfc/REPOSITORY-READINESS.md` e contrato de engenharia v0.1 aprovado.
Isso não substitui análise desta versão nem ordem de implementação.

Esta revisão explicita a precedência noturna nas regras 3–5. O Arquiteto
confirmou os defaults, endpoint 1 e reserva do evento 6 nesta atuação de
retomada, bem como GPIO2, alimentação de 3,3 V e resistor de 10 kΩ ao GND,
com LDR entre alimentação e GPIO2. A ADR-0005 incorpora a alocação.
Permanecem ausentes somente os extremos reais `darkRaw`/`brightRaw` desse
circuito na configuração ADC declarada. O Arquiteto informou que ainda não
realizou as medições; esta atuação não autoriza executá-las.

A análise anterior permanece histórica em
`docs/reports/light-sensor-battery-h2/analysis/2026-09-07T215407Z-938d852-8740e747-implementability-analysis.md`.
Esta revisão de rascunho não declara seus bloqueadores resolvidos por análise.

## Critérios de aceite

| Critério | Resultado observável | Meio |
|---|---|---|
| Calibração | Extremos, saturação e polaridade invertida corretos | Cálculo e medições conhecidas |
| Falha ADC | Sem valor artificial; intervalo curto | Inspeção e falha controlada |
| Política | Primeiro boot, fronteiras, delta e estabilidade corretos; permanência noturna não passa pela seleção de ACTIVE | Sequências controladas de leituras |
| Retenção | Crescimento limitado; RTC inválida e resets descartam histórico | Sequências e resets controlados |
| Integração | Host recebe Light Sensor e percentual; identidade estável; comandos recusados | H2, C6 e host |
| Lifecycle | Report participa do sono antecipado; falhas respeitam deadline | Observação com e sem ACK |
| Construção | Client H2 e coordenador C6 compilam; composições existentes preservadas | Builds canônicos e inspeção do delta |

Nenhum artefato de teste automatizado integra este rascunho. Execução de testes,
flash, monitor e hardware seguem autorização própria da política local.
Critérios sem evidência permanecem não executados. Build comprova construção,
não funcionamento físico nem autonomia da bateria.

### Oráculos da precedência noturna

Com os defaults confirmados e histórico válido:

- NIGHT_SLEEP, leitura anterior 2%, leitura atual 2%, intervalo anterior 300 s:
  permanecer em NIGHT_SLEEP e selecionar 600 s.
- NIGHT_SLEEP, leitura anterior 2%, leitura atual 2%, intervalo anterior 1.800 s:
  permanecer em NIGHT_SLEEP e selecionar 1.800 s.
- NIGHT_SLEEP, leitura anterior 7%, leitura atual 8%: sair para TRANSITION e
  selecionar 30 s, mesmo com delta abaixo de 3 pontos.
- NIGHT_SLEEP, leitura anterior 2%, leitura atual 5%: sair para TRANSITION pelo
  delta de 3 pontos, ainda que abaixo do limiar de saída de 8%.
- NIGHT_SLEEP, leitura anterior 2%, leitura atual 50%: selecionar TRANSITION;
  somente uma aquisição válida posterior pode selecionar ACTIVE.

Esses oráculos definem os resultados esperados; não registram testes executados nem
acrescentam artefatos de teste automatizado ao recorte.
