# ISSP — valores tipados inteiros e fracionários

**ID:** `EKOM-TYPED-001`

**Versão:** 0.1

**Estado:** Rascunho [`Draft`]

**Especificação coordenadora:** esta fonte determina a branch
`spec/issp-typed-values` e prepara a capacidade requerida por
`Fractional-Percentage-Reports.md`. Os dois contratos têm análise e passagem
para implementação próprias; a implementação dos sensores depende da baseline
tipada implementada e validada.

**Contrato de engenharia:** `../rfc/REPOSITORY-ENGINEERING-CONTRACT.md`,
v0.1 Approved; qualificação em `../rfc/REPOSITORY-READINESS.md`.

## 1. Objetivo, decisões e limites

Permitir que um report transporte um inteiro ou um float sem converter
obrigatoriamente inteiros para float. O discriminador do valor indica sua
representação; `event_type` continua indicando a natureza da capability.

Marcelo Miranda confirmou nesta conversa em 08/09/2026: valor tipado inteiro ou
float; porta e presença continuam inteiros binários; luminosidade e bateria
passarão a float; saída percentual textual com duas casas; corte direto do
protocolo, pois existe somente dispositivo de testes em bancada. Não há
compatibilidade retroativa, fallback, negociação ou tradução do formato antigo.
A ordem subsequente autoriza registro e análise, não implementação ou hardware.
A decisão arquitetural está registrada na ADR-0006.

As larguras, códigos e layout abaixo são a elaboração técnica deste Draft,
não uma transcrição de escolhas de bytes feitas pelo Arquiteto. Inteiros de
64 bits, double, strings no rádio e novos tipos de capability não são
contratados nesta versão; extensão futura deve definir novo contrato.

## 2. Escopo e fronteiras

Inclui os tipos/API de reports e codec em `issp_core`; propagação em fila,
snapshot, executor e transporte; codec independente, fingerprint e tradução
UART no coordenador; adaptação dos produtores existentes para o tipo inteiro;
consumidores locais de API e testes explicitamente vinculados na seção 7.

O core continua dono do payload; rádio permanece no transporte. Não criar
componente transversal, fonte C/C++ compartilhada entre aplicações client e
coordenador, alocação dinâmica para valores ou exposição de protocolo pela
fachada. Preservar armazenamento fixo e guardas de tamanho/alinhamento.

Não muda aquisição, cálculo ou cadência dos sensores nesta preparação.
Luminosidade e bateria continuam produzindo seus percentuais inteiros até
`Fractional-Percentage-Reports.md`; o suporte genérico ao float pode ser
validado pelos codecs sem depender desses sensores. Não muda host externo,
NVS, pareamento, segurança, lifecycle, Kconfig, board ou pinagem.

## 3. Requisitos do valor e protocolo

### TV-001 — representação tipada

Cada valor de report contém exatamente um discriminador e um conteúdo:

| Código | Nome | Representação e domínio |
|---:|---|---|
| 0 | Int32 | inteiro com sinal, complemento de dois, 32 bits; −2147483648 a 2147483647 |
| 1 | Float32 | IEEE 754 binary32 finito, 32 bits; inclui valores normais e subnormais |

Inteiros são preservados exatamente em todo seu domínio, inclusive 16777217.
Não inferir tipo pela presença de casas decimais nem converter Int32 para
Float32 no transporte. Float32 tem precisão binária finita: `65.87` designa
sua aproximação binary32, não uma garantia de decimal exato ou acurácia física.
Não quantizar Float32 a duas casas antes do transporte.

Na admissão, normalizar −0.0 para +0.0; rejeitar NaN, infinitos, tipo
desconhecido e conversão fora do domínio, sem wrap, saturação genérica ou
admissão parcial. Falha retorna resultado inválido conforme APIs existentes.
No wire, −0.0 é não canônico e deve ser rejeitado. A saturação de percentual
pertence à capability, não ao codec.

### TV-002 — envelope v3

Todos os tipos de mensagem usam payload fixo de 24 bytes:

| Offset | Bytes | Campo | Codificação |
|---:|---:|---|---|
| 0 | 1 | version | 3 |
| 1 | 1 | message_type | códigos vigentes 1 a 5 |
| 2 | 4 | device_id | unsigned little-endian |
| 6 | 2 | sequence | unsigned little-endian |
| 8 | 8 | report_id | unsigned little-endian |
| 16 | 1 | endpoint_id | sem alteração de identidade |
| 17 | 1 | event_type | sem realocação dos eventos |
| 18 | 1 | value_type | 0 ou 1 |
| 19 | 4 | value_or_status | Int32 ou bits Float32 em little-endian |
| 23 | 1 | checksum | soma módulo 256 dos bytes 0–22 |

O wire não depende de padding, alinhamento, union ou representação nativa da
struct. A API interna pode ter forma própria, mantendo o contrato observável.
Os codecs permanecem independentes e devem coincidir byte a byte. Atualizar
comprimentos, offsets, buffers e os três formatos de endereçamento MAC em
cada alvo. O maior frame tem 21 + 24 + 2 = 47 bytes, abaixo de 127; o byte
local que informa comprimento não integra essa contagem MAC.

### TV-003 — validação e mensagens de controle

Rejeitar versão diferente de 3, tamanho diferente de 24, checksum incorreto,
tipo de valor/mensagem desconhecido e combinações inválidas antes de efeitos.
Não ignorar bytes extras nem reinterpretar frames v2 como v3.

DATA admite os dois tipos e mantém `report_id` não zero. Comandos, ACKs e
discovery usam Int32; preservar os códigos e domínios vigentes de comando e
status, sem converter float em comando/status. DISCOVERY_REQ usa valor zero;
DISCOVERY_RESP conserva status vigente. ACK de report ecoa o ID não zero;
ACK de comando tem ID zero; CMD e discovery têm ID zero. ACK transporta
status, não o valor medido. Preservar validações de destino, endpoint, rede,
registro e correlação existentes. Entrada malformada não publica evento,
altera deduplicação ou produz ACK de sucesso.

### TV-004 — identidade e repetição

O report admitido conserva tipo e conteúdo canônico em fila, snapshot,
tentativas e retries, com seu mesmo `report_id`. Fingerprint passa a conter
`(report_id, endpoint_id, event_type, value_type, quatro bytes do valor)` por
dispositivo. A comparação é exata, nunca com epsilon ou pelo texto arredondado.
Mesmo ID com mudança de tipo ou qualquer bit de conteúdo é conflito. Dois
floats que formatam igual ainda podem ser conteúdos distintos. Nova admissão
tem novo ID mesmo com tipo/valor iguais. Preservar janela, capacidade,
serialização, expulsão e aceitação UART anterior a registro/ACK da ADR-0004.

### TV-005 — produtores e publicação host

Nesta preparação, todos os produtores atuais passam a declarar Int32 e
mantêm valores e traduções vigentes. Porta 0/1 continua closed/open; presença
0/1 continua undetected/detected; plug e estado de telemetria mantêm códigos
e traduções. Esses eventos exigem Int32 e domínio vigente; Float32 não é
reinterpretado como estado binário. Bateria/luz admitem transporte numérico
Int32 ou Float32 de 0 a 100; seus produtores só migram sob o contrato dependente.

No fallback numérico existente, Int32 é texto decimal exato, sem casas; Float32
é texto decimal com duas casas, ponto, sem unidade ou notação exponencial.
Arredondar ao centésimo mais próximo, empate para longe de zero, sobre o valor
binary32 recebido. Resultado zero deve ser `0.00`. Dimensionar a saída para
todo Float32 finito; não emitir JSON truncado. Preservar envelope, strings,
`event_id`, `type`, `direction`, nome da capability e identidade do dispositivo.
Não acrescentar discriminador ao JSON nem mudar os comandos recebidos do host.
Falha de formatação/enfileiramento não é aceitação nem ACK de sucesso.

### TV-006 — corte e preservação

Somente v3 opera após a atualização dos dois lados da bancada. Não implementar
coexistência nem migração de NVS; a mudança não requer recomissionamento apenas
pela versão. Atualizar as composições H2 e o coordenador C6 no recorte de build,
incluindo o exemplo consumidor. Flash/atualização física não é autorizado por
esta especificação. Sem os sensores percentuais, a preparação ainda precisa
preservar as capabilities digitais, comandos, discovery e ACKs.

## 4. Autoridades e relação normativa

Esta fonte é **New** para o valor tipado reutilizável e **Amends**
`ISSP-Report-Identity.md` v0.3, seções 5, 6, 8 e critérios wire correspondentes,
somente quanto a envelope v3, representação e fingerprint. Preserva geração e
lifetime de ID, sincronização, janela e fronteira de aceitação UART.
ADR-0006 **Amends** ADR-0004 quanto ao corte v2 → v3, mantendo suas garantias.

**Amends** `Client-Battery-Level.md` v0.5, seção 5.4, e
`Light-Sensor-Battery-H2.md` v0.3, seções 1, 4 e 5, exclusivamente na menção
a campo de um byte/layout inalterado: na preparação seus inteiros trafegam
como Int32 v3, sem mudança do cálculo ou texto. A mudança de precisão desses
sensores é de autoridade exclusiva da especificação dependente.

Preserva ADR-0001/0002/0003/0005, `ISSP-Architecture.md` v1.2,
`ISSP-Commissioning.md`, bootstrap, reutilização, variantes, deep sleep,
registry e remediação nos comportamentos não emendados. As menções anteriores
ao envelope não impedem o corte expressamente confirmado. A lacuna
`EKM-GAP-0002` não é declarada integralmente encerrada por este recorte.
As relações definem o contrato da evolução; o estado Draft não afirma que
v3 já substituiu a baseline executável.

## 5. Critérios de aceite

| ID | Requisitos | Cenário, ação e resultado observável | Meio |
|---|---|---|---|
| TV-AC-001 | TV-001/002 | Codificar e decodificar 0, 1, −1, limites Int32 e 16777217 conserva tipo e valor exatos; Float32 65.87, 0.5, limites finitos e subnormal conserva bits canônicos | Vetores independentes dos dois codecs |
| TV-AC-002 | TV-001/003 | NaN, ±infinito, −0 wire, tipo desconhecido, payload curto/longo, v2, checksum e controle Float32 são recusados sem sucesso ou evento; −0 admitido localmente torna-se +0 | Casos de falha dos codecs/admissão |
| TV-AC-003 | TV-002/003/006 | DATA, CMD, ACK dos dois contextos e discovery respeitam campos e 24 bytes nos três formatos MAC de ambos os alvos | Vetores e inspeção dos construtores/parsers |
| TV-AC-004 | TV-004 | Retry idêntico gera só ACK; mesmo ID com outro tipo ou fração gera conflito, inclusive se texto for igual; ID novo gera novo evento; UART indisponível não registra sucesso | Política pura e snapshot/fila |
| TV-AC-005 | TV-005 | Binários preservam textos; Int32 16777217 vira string exata; Float32 65.87 vira `65.87`, 0 vira `0.00`, 1.125 vira `1.13`, −1.125 vira `-1.13`; limite finito não trunca JSON | Formatação pura e inspeção da integração UART |
| TV-AC-006 | TV-005/006 | Produtos e exemplo continuam usando as APIs; percentuais ainda são produzidos como inteiros nesta preparação; nenhuma nova dependência cruzada/persistência | Inspeção do delta e builds canônicos |

## 6. Construção e qualificação

Aplicar REC-BUILD seção 9: composições H2 single smart plug, porta, presença e
luminosidade, coordenador C6 e exemplo mínimo H2; construir test apps alterados
nos targets/ambientes próprios. Preservar configurações autoritativas, usando
cópias e diretórios externos. Tamanhos opacos devem continuar comprovados pelas
guardas existentes. Build da implementação autorizada é obrigatório; não
comprova funcionamento físico nem substitui execução dos cenários.

O alcance está contido nas áreas habilitadas pelo contrato v0.1; a alteração
material de wire exige confronto de validade da qualificação neste recorte.
Não estender habilitação a host externo, firmware raiz ou automações.

## 7. Artefatos de teste e permissões

Esta versão **exige criação/alteração** dos seguintes grupos na implementação:

- codecs host-native existentes `issp_protocol_host_test` e
  `iot154_packet_host_test`: TV-AC-001/002/003, com vetores dourados derivados
  do layout, não gerados pelo codec sob teste; cobrir todos os formatos MAC;
- `report_data_policy_host_test`: TV-AC-004, incluindo tipo, bits fracionários,
  retries, conflito, ID novo e recusa UART;
- lógica de formatação junto ao coordenador, em test app host-native próprio:
  TV-AC-005, usando o mesmo caminho puro de produção e oráculos textuais;
- `issp_device_concurrency_test`: TV-AC-002/004 para admissão inválida,
  snapshot tipado e estabilidade durante retry, preservando os oráculos de
  concorrência existentes no H2;
- adaptação estritamente de uso da API nos testes existentes
  `digital_input_behavior_test` e `smart_sys_app_test`: TV-AC-006 e tradução
  binária de TV-AC-005, preservando cenários e resultados originais.

Não alterar testes de registry sem vínculo com estes critérios. Criar testes
não os autoriza a coletar/executar. Nesta atuação: testes, flash, monitor e
hardware **Not Executed**; execução futura requer ordem com recorte e ambiente
conforme `Repository-Test-Execution-Policy.md` v0.4. Oráculos de build,
inspeção e execução são evidências distintas; ausência não significa sucesso.
