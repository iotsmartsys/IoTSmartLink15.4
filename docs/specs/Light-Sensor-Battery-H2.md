# Sensor de luminosidade a bateria ESP32-H2

**Estado:** Rascunho; implementação não iniciada.

**Escopo:** client ESP32-H2 com LDR e ADC, capability de luminosidade,
deep sleep adaptativo e tradução no coordenador ESP32-C6.

## Objetivo e decisões confirmadas

Medir luminosidade relativa e publicar percentual inteiro entre 0 e 100 pelo
IoTSmartLink 15.4. O valor ADC bruto fica disponível somente no diagnóstico
local. A automação realiza toda classificação semântica. ACTIVE, TRANSITION e
NIGHT_SLEEP são estados da política de amostragem.

## Aquisição e normalização

- Uma aquisição por boot operacional; proposta de média de 16 amostras,
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

## Política adaptativa proposta

Valores abaixo são defaults propostos de produto, configuráveis no firmware;
não são constantes dos componentes compartilhados.

| Parâmetro | Valor proposto |
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
   com luz abaixo do limite de saída e delta abaixo do significativo, multiplicar
   o intervalo pelo fator e saturar no máximo, sem transbordo.
4. Em NIGHT_SLEEP, luz >= limite de saída ou delta significativo seleciona
   TRANSITION imediatamente e reinicia a estabilidade e o crescimento. Não
   selecionar ACTIVE na própria leitura de saída.
5. Fora dessa saída imediata e sem entrada em NIGHT_SLEEP, selecionar ACTIVE
   somente com histórico válido, luz >= limite ACTIVE e delta abaixo do
   significativo. Nos demais casos, selecionar TRANSITION.
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
- Endpoint 1 proposto para luminosidade no novo produto, com identidade
  congelada; rejeitar endpoint zero ou duplicado e comandos à capability.
- Propor evento 6 na ADR-0005, pertencente à capability e não configurável pelo
  produto. Atualizar a guarda do registro de eventos.
- Coordenador traduz para `Light Sensor` e percentual decimal, preservando
  envelope JSON, nome de capability, identidade do report, deduplicação e ACK.
- Preservar layout ISSP e independência de código entre os dois targets.

## Autoridades e pendências

Nova especificação para aquisição e política de luminosidade. Emenda proposta
a `Firmware-Variants-Menuconfig.md` para a composição e a `Client-Deep-Sleep.md`
para intervalo adaptativo em segundos e admissão da luminosidade no sono
antecipado. Alocação do evento depende de emenda da ADR-0005. Preservar ADR-0001
a ADR-0004, commissioning e ISSP-Report-Identity nos demais comportamentos.

Pinagem, circuito e calibração reais não foram fornecidos. Não tratar valores
ilustrativos como calibração nem pinagem de outro sensor como fato deste board.
Defaults adaptativos e alocação do evento permanecem propostas. A análise de
implementabilidade deve registrar classificação separada; este documento não
declara Ready ou aprovação arquitetural.

## Critérios de aceite

| Critério | Resultado observável | Meio |
|---|---|---|
| Calibração | Extremos, saturação e polaridade invertida corretos | Cálculo e medições conhecidas |
| Falha ADC | Sem valor artificial; intervalo curto | Inspeção e falha controlada |
| Política | Primeiro boot, fronteiras, delta e estabilidade corretos | Sequências controladas de leituras |
| Retenção | Crescimento limitado; RTC inválida e resets descartam histórico | Sequências e resets controlados |
| Integração | Host recebe Light Sensor e percentual; identidade estável; comandos recusados | H2, C6 e host |
| Lifecycle | Report participa do sono antecipado; falhas respeitam deadline | Observação com e sem ACK |
| Construção | Client H2 e coordenador C6 compilam; composições existentes preservadas | Builds canônicos e inspeção do delta |

Nenhum artefato de teste automatizado integra este rascunho. Execução de testes,
flash, monitor e hardware seguem autorização própria da política local.
Critérios sem evidência permanecem não executados. Build comprova construção,
não funcionamento físico nem autonomia da bateria.
