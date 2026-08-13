# Identidade de reports e deduplicação entre boots no ISSP

**Tipo:** Normativo

**Estado normativo:** `Ready` — v0.3 analisada

**Estado da implementação:** v0.3 `In Progress` — delta implementado e builds
concluídos, conforme `2026-08-12-v03-implementation.md`; execução de testes,
flash, monitor e hardware permanece `Not Executed`

**Estado do workflow:** Implementação da v0.3 executada por ordem explícita do
Arquiteto; aguardando Revisão do delta

**Análise de implementabilidade:** `Ready` para o conteúdo normativo da revisão
`9687287`, conforme `2026-08-12-v03-implementability-analysis.md`. O `Ready` de
`f78c6d2` permanece histórico para a v0.2.

**Versão:** 0.3

**Responsável arquitetural:** Marcelo Miranda

**Última atualização:** 12/08/2026

**Escopo:** protocolo ISSP v2, admissão e retry de reports no client,
deduplicação e ponte UART no `coordinator_154`

---

## 1. Objetivo e problema

Um report legítimo não pode ser descartado pelo coordenador apenas porque um
client reiniciou e reutilizou uma sequência já observada. O fluxo deve manter a
deduplicação necessária aos retries sem confundir dois reports de boots ou
admissões distintos.

A baseline usa a sequência de 16 bits como correlação de envio e como
identidade de deduplicação. `IsspDevice` inicia `reportSequence_` em zero a cada
boot; o coordenador conserva `last_seq` em RAM enquanto permanece ligado. Um
novo report com a mesma sequência é então ACKado como duplicado, sem evento ao
host. Deep sleep torna a ocorrência recorrente, mas qualquer reboot isolado do
client alcança o mesmo defeito.

Esta especificação introduz uma identidade própria de report:

```text
identidade lógica = (dispositivo conhecido, report_id)
correlação da tentativa = (origem, device_id, sequence, report_id, endpoint)
```

O `report_id` é um inteiro aleatório não nulo de 64 bits gerado pelo client. O
coordenador não o concede e não há sessão, handshake ou round trip adicional:
o client envia e o coordenador aceita segundo as regras deste contrato.

### 1.1 Resultado contratado

- cada admissão lógica recebe um novo `report_id`;
- todas as tentativas da mesma admissão reutilizam esse ID;
- o ACK de report ecoa o ID recebido;
- o coordenador não reencaminha o mesmo report dentro de sua janela de
  deduplicação, mas sempre tenta ACKar um retry válido;
- reports de boots diferentes não colidem deterministicamente quando a
  sequência reinicia;
- o host recebe uma identidade aditiva e estável para deduplicação posterior.

### 1.2 Limite fundamental da garantia

Este contrato elimina a perda **determinística** causada por `last_seq`; não
promete entrega exatamente uma vez nem ausência absoluta de perda.

O `report_id` oferece unicidade estatística. Uma colisão de 64 bits ainda pode
suprimir um report legítimo. A janela do coordenador é volátil e finita; depois
de reboot ou expulsão de um ID, um retry antigo pode produzir evento repetido.
O ACK confirma aceitação na fila UART local do coordenador, não consumo pelo
host, publicação MQTT ou persistência externa. Essas limitações são riscos
aceitos pelo Arquiteto para manter o fluxo simples.

Entrega durável fim a fim, outbox persistente, confirmação do host e semântica
exactly-once exigem outra especificação.

## 2. Autoridades confrontadas

Esta especificação é preparatória e transversal. A implementação de correções
no deep sleep fica condicionada a esta base, mas o deep sleep não é reaberto.

- **Altera (`Amends`) `ISSP-Architecture.md` v1.2:** separa identidade lógica
  de report e sequência de tentativa; preserva as responsabilidades de device,
  executor, transporte e aplicação.
- **Altera (`Amends`) `ISSP-Reusable-Components.md` v1.1:** amplia de forma
  material e limitada as APIs públicas de `issp_core`, com uma fonte injetável
  de `report_id` e os campos necessários nos tipos de report, e de
  `issp_transport_154`, com o ID em `Issp154AckExpectation` e somente nos tipos
  necessários à sua correlação. ADR-0004 registra a autorização arquitetural.
  Não altera `IIsspTransport`, lifecycle, retry ou API pública de behaviors;
  não autoriza nova dependência de ESP-IDF para a fonte no core nem
  generalização de geradores.
- **Altera (`Amends`) `ISSP-Coordinator-Paired-Device-Registry.md` v0.4:**
  substitui `last_seq` volátil por uma janela volátil de IDs recentes para
  dispositivos conhecidos. O schema, o blob NVS, a capacidade, a identidade
  IEEE e as regras de disponibilidade do registry não mudam.
- **Altera (`Amends`) `ISSP-Commissioning.md` v1.0:** muda o envelope wire para
  v2 também em discovery e command, sem mudar seus estados, canais, tentativas,
  tempos ou persistência. O campo reservado `report_id` é zero nesses tipos.
- **Altera de forma limitada (`Amends`) `ISSP-Configurable-Bootstrap.md` v1.5:**
  a construção interna fornece a fonte de IDs à `IsspDevice`; API pública,
  máquina de estados e política da fachada não mudam.
- **Altera de forma limitada (`Amends`) `ISSP-Consolidation.md` v1.0:** o corte
  v2 é uma evolução posterior aprovada sobre a baseline consolidada. Não
  reabre a migração histórica nem transfere código entre os targets.
- **Preserva `Client-Deep-Sleep.md` v0.11:** timer, EXT1, LED, GPIO, deadline,
  arbitragem, quiescência e descarte de pendências no deadline permanecem
  inalterados. A nova identidade apenas impede que o report inicial de um novo
  boot seja confundido com retry de boot anterior.
- **Preserva `Firmware-Variants-Menuconfig.md` e ADR-0002:** nenhuma opção de
  Kconfig, product firmware ou board model governa o protocolo ou a geração do
  ID.
- **Preserva `Repository-Test-Execution-Policy.md` v0.4 e ADR-0003:** builds são
  parte intrínseca de implementação autorizada; execução de testes, flash,
  monitor e hardware exige autorização própria. Esta versão em Draft não
  autoriza execução.

Se a análise identificar conflito fora dessas alterações, deve classificar a
especificação como não pronta. O Implementador não pode ampliar silenciosamente
o recorte para resolver autoridade ausente.

## 3. Escopo

Inclui:

- identidade e ciclo de vida de cada report lógico;
- API mínima para geração injetável do ID;
- armazenamento do ID junto ao slot pendente;
- correlação do ACK de report;
- layout wire ISSP v2 para todos os tipos atuais;
- corte coordenado de compatibilidade v1 → v2;
- janela volátil de deduplicação do coordenador;
- ordem entre emissão ao host, inserção na janela e ACK;
- `event_id` aditivo no JSON de evento;
- falhas, diagnósticos, limites de memória e evidências necessárias;
- proteção do comportamento vigente do deep sleep.

## 4. Fora do escopo

- sessão ou ID negociado com o coordenador;
- persistência de sequência, `report_id`, reports pendentes ou cache de
  deduplicação;
- outbox no coordenador ou no host;
- ACK do host, MQTT ou aplicação final para o client;
- exatamente uma vez, ordenação global ou retenção ilimitada;
- autenticação, assinatura, confidencialidade, anti-replay ou uso do ID como
  segredo;
- alteração da capacidade de oito reports pendentes ou oito dispositivos;
- alteração da coalescência por endpoint/evento;
- rate limit, batching, fragmentação ou compressão;
- alteração de retries, backoff, timeout, canal, PAN ID ou endereçamento MAC;
- política de tráfego operacional de origem desconhecida durante commissioning;
- mudança no schema persistente do registry;
- mudança no parser ou MQTT do repositório externo `SmartHome-Hub`;
- correção de perdas posteriores à aceitação UART local;
- alteração funcional do deep sleep.

Um requisito de qualquer item acima bloqueia a implementação deste recorte e
deve gerar especificação complementar, não um acréscimo oportunista.

## 5. Termos e invariantes

- **Report lógico:** estado aceito por `publishState()` ou envio direto iniciado
  por uma chamada de `publishReport()`.
- **Admissão:** inserção ou atualização bem-sucedida de um slot pendente.
- **Tentativa interna:** retry realizado dentro de uma chamada confirmada do
  transporte; reutiliza payload, sequência e ID.
- **Tentativa externa:** novo processamento do mesmo slot pelo executor após
  falha retryable; pode receber nova sequência, mas reutiliza o ID.
- **Fingerprint:** `(report_id, endpoint_id, event_type, value)` para um
  dispositivo conhecido. A sequência não participa dele.
- **Aceitação local:** JSON completo e delimitador copiados, sem espera por
  capacidade, para o ring buffer TX da UART do coordenador.
- **ID recente:** fingerprint presente na janela volátil do slot do registry.

Invariantes:

1. `report_id == 0` nunca identifica `DATA` ou ACK de report.
2. Um report lógico possui exatamente um ID durante toda sua vida no client.
3. Uma nova admissão possui um novo ID, mesmo que seu valor seja igual ao
   anterior e mesmo que ocorra no mesmo slot.
4. Sequência pode mudar entre tentativas externas e nunca define duplicidade.
5. O coordenador só registra um ID depois da aceitação local do evento.
6. Um ID registrado é ACKável novamente sem novo evento.
7. O mesmo ID com conteúdo diferente é conflito, não retry.
8. Nenhum estado desta funcionalidade é persistido.
9. Client e coordenador continuam sem dependência de código entre seus
   diretórios; compatibilidade é comprovada por vetores wire comuns.
10. Falha não pode ser convertida em ACK de sucesso antes da aceitação local.

## 6. Contrato do protocolo ISSP v2

### 6.1 Corte de versão

`IOT154_VERSION` e a versão equivalente no core passam de 1 para 2. Todos os
tipos atuais usam payload fixo de 20 bytes. Frames v1, comprimentos menores ou
maiores que 20 bytes e combinações inválidas são rejeitados pelos dois targets;
não há fallback, tradução, bytes adicionais ignorados ou operação mista
silenciosa.

O deployment é uma janela coordenada de manutenção: atualizar coordenador e
clients para v2. Atualizar somente um lado causa indisponibilidade temporária,
mas não altera NVS, pareamento ou descritor de rede; após ambos estarem em v2,
não é necessário recomissionar apenas pela versão.

### 6.2 Layout fixo

| Offset | Tamanho | Campo | Codificação |
|---:|---:|---|---|
| 0 | 1 | `version` | `2` |
| 1 | 1 | `message_type` | tipos vigentes |
| 2 | 4 | `device_id` | unsigned little-endian |
| 6 | 2 | `sequence` | unsigned little-endian |
| 8 | 8 | `report_id` | unsigned little-endian |
| 16 | 1 | `endpoint_id` | sem alteração semântica |
| 17 | 1 | `event_type` | sem alteração semântica |
| 18 | 1 | `value_or_status` | sem alteração semântica |
| 19 | 1 | `checksum` | soma aditiva de 8 bits dos bytes 0–18 |

O aumento de 8 bytes mantém o maior frame vigente abaixo do limite IEEE
802.15.4 de 127 bytes; a análise deve reconfirmar esse cálculo nos três
construtores/parsers do client e nos três do coordenador.

### 6.3 Uso por tipo

| Tipo | `report_id` | Regra adicional |
|---|---|---|
| `DATA` | não nulo | identifica o report lógico |
| ACK de `DATA` | mesmo ID não nulo | ecoa o report recebido |
| `DISCOVERY_REQ` | zero | sem mudança de semântica |
| `DISCOVERY_RESP` | zero | sem mudança de semântica |
| `CMD` | zero | sem mudança de semântica |
| ACK de `CMD` | zero | sem mudança de semântica |

O tipo wire de ACK permanece único. `report_id != 0` identifica ACK de report;
`report_id == 0` identifica ACK de comando. `DATA` com zero, mensagens que não
são report com ID não nulo e status inválido são frames inválidos e não causam
efeito.

Client e coordenador possuem codecs separados. Ambos devem produzir e consumir
os mesmos vetores dourados byte a byte, inclusive checksum, endianness, IDs de
fronteira e combinações inválidas. Uma cópia compartilhada de código entre
targets é proibida.

## 7. Contrato do client

### 7.1 Fonte de IDs e API reutilizável

`issp_core` recebe uma função injetável equivalente a:

```cpp
using ReportIdGenerator = std::uint64_t (*)(void *context);

struct IsspDeviceConfig
{
    std::uint32_t deviceId;
    ReportIdGenerator reportIdGenerator;
    void *reportIdGeneratorContext;
};
```

Nomes exatos podem seguir o estilo do componente, mas contrato e
responsabilidade não podem mudar. Configuração sem gerador é inválida para
construção operacional v2 e deve falhar antes de admitir report.

Esta mudança não adiciona header, tipo ou chamada do ESP-IDF ao core para obter
IDs; as dependências FreeRTOS já usadas por `IsspDevice` permanecem. A fachada
fornece um adaptador baseado em `esp_fill_random()` no ESP32-H2. O consumidor
`examples/issp_minimal_client` e os testes fornecem gerador próprio, podendo ser
determinístico. Behaviors, product firmware e board model não conhecem o
gerador.

A aleatoriedade não é declarada criptográfica. O caminho operacional atual
publica depois da inicialização de rádio. A análise da v0.1 confirmou que
`esp_ieee802154_enable()` habilita o PHY antes de `IsspDevice::start()` e que
nenhum caminho de falha admite report antes de `Ready`. `esp_random()` pode
realizar espera ocupada de dezenas de microssegundos por ID no H2, reforçando a
proibição de chamá-lo dentro do `portMUX`. Se revisão futura invalidar essas
premissas, a especificação volta ao Arquiteto; não se acrescenta persistência
ou sessão.

### 7.2 Geração e falha

Antes de inserir ou atualizar um slot, o device solicita um ID que seja:

- diferente de zero;
- diferente dos IDs de todos os slots ocupados, inclusive in-flight.

A busca é limitada; geração que só produz zero ou colisões retorna falha
explícita e não altera slot, geração, contagem, ordem, report anterior ou
estado in-flight. O número exato de tentativas locais é detalhe de
implementação, desde que finito, determinístico sob fake e coberto por teste.

Não existe busca na rede nem consulta ao coordenador. Colisão com report de
boot anterior ou ID já expulso não é detectável no client e permanece risco
estatístico aceito.

### 7.3 Slots, coalescência e retry

`PendingReportSlot` conserva `report_id` ao lado do report e da geração. O ID é
copiado para `IsspPreparedReport`, para o payload e para a expectativa de ACK.
Um único campo no slot é suficiente: durante atualização concorrente, o ID da
geração in-flight permanece na cópia preparada e na expectativa do transporte.

- inserção em slot vazio: novo report e novo ID;
- atualização do mesmo endpoint/evento: nova admissão, nova geração e novo ID,
  mesmo quando o valor não mudou;
- atualização enquanto a geração anterior está in-flight: a tentativa antiga
  termina com seu ID, mas não remove a geração nova; o próximo preparo usa o ID
  novo;
- falha retryable: libera a reserva sem trocar o ID;
- entrega confirmada da geração vigente: remove report e ID;
- quiescência: impede novas admissões e preserva slots e IDs já aceitos.

Tentativas internas reutilizam ID, sequência e payload. Tentativas externas do
executor reutilizam ID e conteúdo, mas preservam o comportamento vigente de
obter nova sequência em cada `preparePendingReport()`.

### 7.4 Envio direto

Cada invocação de `publishReport()` representa novo report lógico e recebe novo
ID. A função continua sem fila ou retry externo. Uma nova invocação feita pelo
caller, mesmo com conteúdo igual, é nova identidade e pode gerar novo evento.
Não se cria API pública para o caller fornecer ou reutilizar ID.

O coordenador pode inserir esse ID em sua janela, mas o envio direto continua
sem aguardar ACK e o client não aprende se houve aceitação local. Essa
assimetria preserva o contrato preexistente de `publishReport()`; torná-lo
confirmado pertence a outro recorte.

### 7.5 Correlação do ACK

Um ACK de report só conclui a tentativa quando coincidirem:

- origem esperada pelo transporte;
- `device_id`;
- `sequence` da tentativa;
- `report_id` não nulo;
- `endpoint_id`;
- status válido.

ACK atrasado de outra sequência, outro ID ou outro endpoint é ignorado. ACK de
comando conserva suas correlações vigentes e exige `report_id == 0`.

## 8. Contrato do coordenador

### 8.1 Janela de deduplicação

Cada slot de dispositivo conhecido no registry possui uma janela FIFO volátil
de oito fingerprints recentes. A janela:

- nasce vazia no boot do coordenador;
- não integra o blob NVS;
- é limpa quando o slot passa a representar outro endereço ou `device_id`;
- é preservada em pareamento idempotente da mesma associação;
- não reordena uma entrada quando recebe retry;
- expulsa a entrada mais antiga ao inserir a nona.

A capacidade oito é fixa neste recorte e independe da sequência. Alterá-la,
persisti-la ou aplicar expiração por tempo requer nova decisão.

O limite acompanha os oito slots pendentes atuais do client. Como o executor
envia um report por vez, ele cobre todos os reports que ainda podem ser
reapresentados pelo fluxo normal daquele client sem criar retenção ilimitada.
Atualização de um slot durante uma tentativa substitui sua geração e não torna
a geração antiga novamente elegível a tentativa externa. A análise deve
reconfirmar esse invariante; se houver outro produtor capaz de manter mais de
oito identidades retryable simultâneas por device, o tamanho volta ao
Arquiteto.

### 8.2 Ordem obrigatória para `DATA` conhecido

Depois de validar frame, endereçamento, versão, checksum, disponibilidade do
registry e identidade conhecida, o coordenador processa `DATA` nesta ordem:

1. procurar `report_id` na janela do dispositivo;
2. se fingerprint idêntico existir, não emitir evento e tentar ACK com a
   sequência e o ID da tentativa recebida;
3. se o ID existir com endpoint/evento/valor diferente, registrar conflito,
   não emitir evento, não alterar a janela e não ACKar;
4. se o ID for novo, montar o JSON e tentar sua aceitação local completa;
5. somente após sucesso, inserir o fingerprint na janela;
6. somente depois da inserção, tentar o ACK com sequência e ID recebidos.

Se montar o JSON falhar, exceder o buffer, a fila UART não aceitar todos os
bytes ou o delimitador não for aceito, o coordenador não insere o ID e não
ACKa. O client pode então repetir o mesmo report.

Se o ACK falhar depois da inserção, o retry seguinte é reconhecido e ACKado sem
novo evento. Inserção no array fixo não pode depender de alocação ou I/O.

Para eventos de report, JSON e delimitador são reunidos em um único buffer
fixo. A aceitação local segue obrigatoriamente esta ordem:

1. exigir `s_host_uart_lock` válido e tentar adquiri-lo com espera zero; lock
   ausente ou ocupado é falha retryable desta recepção;
2. consultar `uart_get_tx_buffer_free_size()` ainda sob o lock;
3. se a consulta falhar ou o limite conservador retornado for menor que a linha
   completa, liberar o lock e falhar sem chamar `uart_write_bytes()`;
4. havendo espaço, submeter o buffer inteiro em uma única chamada
   `uart_write_bytes()`;
5. considerar aceito somente retorno igual ao tamanho total e então liberar o
   lock.

Todos os produtores de `HOST_UART_NUM` continuam serializados pelo mesmo lock;
uma escrita que o contorne invalida a garantia entre consulta e submissão. A
consulta do ESP-IDF 6.0.1 já desconta o descritor e fornece um limite
conservador para a próxima transação. Sob o lock, nenhum produtor reduz o
espaço; o dreno da ISR somente o aumenta. Portanto, satisfeita a consulta, a
escrita copia toda a linha para o ring buffer sem aguardar capacidade ou dreno
físico.

Lock ausente ou ocupado, consulta falha, espaço insuficiente ou retorno
inesperado da escrita são falhas observáveis: não inserem o ID e não produzem
ACK. O contrato não aguarda confirmação física, host ou MQTT. Outros tipos de
linha conservam seu comportamento vigente; o caminho de report não pode
bloquear atrás deles.

### 8.3 Origem desconhecida e registry indisponível

As precedências e efeitos da especificação do registry permanecem vigentes.
Este recorte não torna uma origem conhecida a partir de `DATA` e não cria cache
persistente ou slot de deduplicação para origem desconhecida.

Quando a política vigente aceitar `DATA` desconhecido durante janela aberta, o
coordenador valida `report_id` e aplica a mesma aceitação local sem espera da
seção 8.2: só ACKa depois que JSON e delimitador forem aceitos integralmente
pela UART. Lock indisponível, consulta falha, espaço insuficiente ou retorno
inesperado não produzem ACK; o client pode tentar novamente. Não existe cache
nem promessa de deduplicação entre retries de origem desconhecida. Fechada a
janela, ou com registry indisponível, preserva a rejeição vigente sem evento e
sem ACK.

A assimetria de deduplicação é deliberada. A especificação do registry continua
dona da decisão de admitir ou rejeitar origem desconhecida; este contrato só
governa a aceitação local e o ACK depois que essa admissão ocorrer. Se a análise
considerar deduplicação de desconhecidos necessária ao objetivo, deve bloquear
e devolver ao Arquiteto em vez de criar estado paralelo.

### 8.4 Evento ao host

Eventos `direction:"evt"` acrescentam:

```json
{"device_id":"issp154-00124B0000000001","event_id":"issp154-00124B0000000001:0123456789ABCDEF"}
```

O formato canônico é `<device_id textual>:<report_id com 16 dígitos
hexadecimais maiúsculos>`. O `device_id` textual usa a mesma forma já emitida no
campo existente. Os campos atuais e seus valores permanecem; ordem de
propriedades JSON não é normativa.

O repositório externo `SmartHome-Hub` foi confrontado somente como consumidor:
seu parser aceita objeto JSON com campo desconhecido; o interceptador consulta
campos conhecidos, preserva o objeto recebido e acrescenta `owner` antes de
encaminhá-lo ao bridge MQTT. O buffer de enriquecimento vigente também deve ser
confrontado com o tamanho máximo do novo evento. Nenhuma alteração nesse
repositório é contratada. O retorno MQTT é posterior ao ACK de rádio e não
participa da aceitação local.

Eventos de gateway, ACK/erro de comando e tráfego ESP-NOW não recebem
`event_id` neste recorte.

### 8.5 Costura interna do coordenador

É autorizada uma extração privada e local ao `coordinator_154`, seguindo o
precedente de `device_registry_policy.c`, para concentrar a decisão de `DATA`
novo, retry, conflito e indisponibilidade da aceitação local. O mesmo código de
decisão deve governar produção e testes; `main.c` conserva rádio e efeitos.

A costura pode substituir, em teste, consulta de espaço, submissão UART, ACK e
evento, mas não se torna componente compartilhado, header público ou segunda
política. A política vigente de registry continua sendo entrada da decisão.

## 9. Relação com deep sleep

Esta especificação não altera a máquina de estados do deep sleep.

- o ID é criado na admissão do report inicial, antes de qualquer envio;
- não há nova troca de rede nem espera adicional;
- `pendingReportCount()` conserva sua semântica;
- ACK válido remove o slot como hoje, com correlação adicional do ID;
- retry até o deadline reutiliza o mesmo ID;
- no boot seguinte, `reportOnStart=true` cria nova admissão e novo ID;
- no deadline, pendências continuam descartadas sem persistência, conforme
  `DEEPSLEEP-DEC-004`;
- timer, EXT1, LED, pull, HOLD, factory reset, quiescência, `stop()`, `end()` e
  tempos não mudam.

Se a geração de ID falhar durante publicação inicial, a falha propaga pelo
caminho vigente de start e o lifecycle dorme no deadline segundo o contrato
atual. Não se usa ID zero nem se ignora a falha para favorecer o sono
antecipado.

O critério de report entregue do deep sleep passa a exigir ACK v2 plenamente
correlacionado. Isso fortalece o oráculo; não amplia seu tempo máximo.

## 10. Memória, concorrência e limites

- `kMaxPendingReports == 8` e `kMaxDeviceBehaviors == 8` permanecem.
- Cada slot pendente cresce em um `uint64_t`; tipos preparados e decodificados
  também carregam o ID.
- O coordenador adiciona no máximo oito fingerprints por cada um dos oito slots
  do registry, em armazenamento estático.
- O payload cresce de 12 para 20 bytes; buffers de frame, RX/TX, testes e
  `sizeof(iot154_packet_t)` devem ser confrontados.
- A ampliação de `IsspDevice` deve caber em `kHardwareStorageBytes` e
  `kImplStorageBytes` existentes. Aumentar essas reservas não está autorizado
  por esta versão; insuficiência bloqueia e exige decisão arquitetural.
- Não se introduz heap no caminho de report, deduplicação ou ACK.
- Geração de ID ocorre fora de seção crítica. Depois de gerar, cada publisher
  entra na seção crítica, escolhe o slot no estado então vigente, revalida a
  colisão e realiza a mutação inteira no mesmo trecho. Em colisão, sai da seção
  antes de gerar novamente. Isso impede dois publishers concorrentes de aceitar
  o mesmo ID sem bloquear o gerador dentro do `portMUX`.
- Encoding, transporte, UART, log e callbacks continuam fora das seções
  críticas.
- A espera zero do lock e a consulta de espaço UART pertencem ao caminho de
  evento no coordenador e não usam o `portMUX` do client.

A análise deve confrontar especificamente a atomicidade entre geração e
admissão. Chamar uma fonte potencialmente lenta dentro do `portMUX` ou separar
validação e commit em duas entradas na seção crítica são implementações
inválidas.

## 11. Falhas e diagnósticos mínimos

Devem ser distinguíveis em log estruturado, sem registrar segredo:

- gerador ausente;
- ID zero ou colisão local até falha da busca limitada;
- frame v1 recebido em endpoint v2;
- tamanho, checksum ou combinação tipo/ID inválidos;
- conflito de fingerprint para o mesmo dispositivo e ID;
- falha ou truncamento na aceitação UART local;
- report novo aceito, ID e sequência;
- retry deduplicado e re-ACKado;
- falha de transmissão do ACK após cache;
- origem desconhecida ou registry indisponível segundo a política vigente.

O `report_id` pode aparecer em hexadecimal nos logs porque não é segredo. Logs
não constituem persistência nem confirmação fim a fim.

Os helpers de diagnóstico do transporte devem usar os offsets v2; o byte 8 não
é mais endpoint. Como o parser rejeita v1 por comprimento antes do codec, o
diagnóstico específico de versão deve inspecionar com segurança o byte 0 no
caminho de tamanho incompatível, sem tentar decodificar o restante.

## 12. Migração e compatibilidade operacional

1. preservar os blobs NVS do client e do registry; nenhum schema muda;
2. construir client ESP32-H2 e coordenador ESP32-C6 com v2;
3. implantar os dois lados na mesma janela, preferencialmente coordenador antes
   dos clients para limitar emissão v2 sem receptor;
4. aceitar indisponibilidade temporária no intervalo misto;
5. não limpar pareamentos nem forçar commissioning apenas pelo upgrade;
6. validar evento e ACK v2 antes de considerar a migração funcional.

Rollback exige reinstalar v1 nos dois lados. Um lado v1 e outro v2 é estado
incompatível diagnosticável, não modo suportado.

## 13. Critérios de aceitação

**REPORT-ID-AC-001 — ciclo do ID no client.** Inserção recebe ID novo; retries
internos e externos o reutilizam; atualização do slot recebe outro ID; entrega
da geração vigente remove report e ID.

**REPORT-ID-AC-002 — falha atômica de geração.** Geradores controlados que
retornem zero e colisões locais comprovam busca limitada e ausência de mutação
parcial do slot. Gerador ausente é rejeitado.

**REPORT-ID-AC-003 — concorrência.** Publicações concorrentes não aceitam o
mesmo ID, não corrompem ordem/contagem e não executam a fonte dentro de seção
crítica.

**REPORT-ID-AC-004 — vetores wire v2.** Client C++ e coordenador C coincidem
byte a byte para cada tipo, checksum, little-endian, ID zero/não nulo e valores
de fronteira. v1, payload truncado, payload excedente e combinações inválidas
são rejeitados pelos dois parsers.

**REPORT-ID-AC-005 — correlação.** ACK de report só conclui com origem,
device, sequência, ID, endpoint e status válidos; ACK atrasado ou de comando
não conclui report.

**REPORT-ID-AC-006 — retry deduplicado.** Duas recepções do mesmo fingerprint
de dispositivo conhecido geram um evento local e duas tentativas de ACK; a
segunda usa a sequência recebida nela.

**REPORT-ID-AC-007 — fila local sem bloqueio.** Lock UART ausente ou ocupado,
falha da consulta ou espaço insuficiente são observáveis em produção, não
chamam a escrita, não inserem ID e não enviam ACK. Com espaço suficiente, uma
única escrita recebe JSON mais delimitador e não espera dreno. Retorno
inesperado não insere ID nem ACKa. Retry após recuperação volta a tentar o
evento.

**REPORT-ID-AC-008 — falha depois da aceitação.** Falha de ACK após evento e
cache faz o retry seguinte receber ACK sem segundo evento.

**REPORT-ID-AC-009 — conflito de identidade.** Mesmo dispositivo e ID com
conteúdo diferente não produz evento nem ACK e gera diagnóstico próprio.

**REPORT-ID-AC-010 — janela e reboot.** Nove IDs demonstram FIFO de oito e
expulsão do mais antigo; reboot simulado esvazia a janela sem alterar registry.
O teste reconhece que o ID expulso pode voltar a gerar evento.

**REPORT-ID-AC-011 — JSON e host existente.** Evento aceito inclui `event_id`
canônico e preserva os campos atuais. Confronto do parser do `SmartHome-Hub`
confirma aceitação e preservação da linha sem exigir alteração externa.

**REPORT-ID-AC-012 — registry.** Associação, NVS, limite e precedência do
registry permanecem; troca real de identidade limpa a janela e pareamento
idempotente a preserva. Origem desconhecida segue a política declarada na
seção 8.3.

**REPORT-ID-AC-013 — deep sleep.** Dois boots consecutivos do
`door_sensor_battery_h2`, com sequência reiniciada, produzem IDs diferentes,
dois eventos ao coordenador e ACKs correspondentes, sem mudar deadline,
wakeups, LED ou ordem terminal.

**REPORT-ID-AC-014 — consumidores e reservas.** `client_154`,
`coordinator_154` e `examples/issp_minimal_client` compilam; os `static_asserts`
de armazenamento permanecem satisfeitos sem aumentar as reservas.

**REPORT-ID-AC-015 — limite MAC.** Os seis formatos de frame existentes — três
por target — com payload v2 permanecem abaixo de 127 bytes e seus parsers
recusam truncamento.

**REPORT-ID-AC-016 — ausência de regressão.** Discovery, commands, ACK de
command, commissioning, quiescência e report direto mantêm seus resultados,
com `report_id == 0` fora do fluxo de DATA.

## 14. Evidência e execução futura

### 14.1 Evidência host-native sem execução física

- vetores dourados em ambos os codecs para AC-004 e AC-015;
- no codec do coordenador, caso explícito de payload menor e maior que 20 bytes
  para AC-004, sem criar suíte paralela;
- teste de formatação para AC-011;
- policy/core privado extraído do coordenador, com efeitos substituíveis, para
  AC-006 a AC-010 e AC-012.

Esses binários podem ser construídos com toolchain host puro. Sua execução
continua dependente da autorização aplicável.

### 14.2 Evidência automatizada em ESP32-H2

AC-001, AC-002, AC-003 e AC-005 exercitam `IsspDevice`, que depende do
FreeRTOS/`portMUX`, e pertencem a test app ESP-IDF no H2 físico. Regressões do
device e transporte relacionadas a AC-016 pertencem ao mesmo meio. Não devem
ser classificadas como host-native nem executadas sem autorização de hardware.

As costuras devem envolver a fonte de ID, aceite UART e efeitos de rádio sem
duplicar a política de produção. Criar um segundo fluxo só para testes é
inválido.

### 14.3 Builds intrínsecos à implementação

Quando a implementação estiver autorizada, são obrigatórios:

- build canônico ESP32-H2 de `client_154` com ESP-IDF 6.0.1;
- build canônico ESP32-C6 de `coordinator_154` com o toolchain definido pela
  política vigente;
- build host-native puro das suítes aplicáveis, sem executá-las quando não
  houver autorização de execução.

Build não prova comportamento de rádio, UART ou hardware.

O ESP-IDF 6.0.1 necessário está disponível no ambiente canônico em
`~/.espressif/v6.0.1/esp-idf`; caminho local é fato de ambiente, não requisito
portável do código.

### 14.4 Execução dependente de autorização

Testes, flash, monitor e hardware ficam `Not Executed` até autorização própria.
Quando autorizados, AC-013 deve observar dois boots com deep sleep e o
coordenador continuamente ligado. Deve-se provocar também perda de ACK após a
aceitação local para demonstrar AC-008. A evidência distingue logs do client,
coordenador e host; não infere MQTT a partir do ACK de rádio.

## 15. Decisões arquiteturais registradas

- **REPORT-ID-DEC-001:** identidade é `(device, report_id)`; sequência é
  correlação de tentativa.
- **REPORT-ID-DEC-002:** ID aleatório, não nulo, de 64 bits, gerado pelo client;
  unicidade é estatística e risco de colisão é aceito.
- **REPORT-ID-DEC-003:** não há sessão, handshake, consulta ao coordenador ou
  persistência.
- **REPORT-ID-DEC-004:** retries do mesmo report reutilizam ID; nova admissão
  recebe novo ID.
- **REPORT-ID-DEC-005:** protocolo passa em corte coordenado para v2, payload
  fixo de 20 bytes, sem compatibilidade silenciosa com v1.
- **REPORT-ID-DEC-006:** deduplicação conhecida usa janela FIFO volátil de oito
  fingerprints por slot do registry.
- **REPORT-ID-DEC-007:** evento deve ser aceito integralmente e sem espera por
  capacidade no ring buffer UART antes de cache e ACK, conforme a sequência da
  seção 8.2.
- **REPORT-ID-DEC-008:** ACK local não significa entrega ao host/MQTT; entrega
  durável é outro recorte.
- **REPORT-ID-DEC-009:** `event_id` é aditivo e canônico; host existente não é
  alterado.
- **REPORT-ID-DEC-010:** API reutilizável cresce somente em `issp_core` para
  injeção/propagação do ID e em `issp_transport_154` para sua correlação em
  `Issp154AckExpectation` e tipos estritamente necessários, conforme ADR-0004.
- **REPORT-ID-DEC-011:** deep sleep permanece funcionalmente inalterado.
- **REPORT-ID-DEC-012:** insuficiência das reservas estáticas bloqueia; esta
  versão não autoriza aumentá-las.

## 16. Verificação obrigatória antes de prontidão

Uma análise independente deve confrontar, na revisão exata desta fonte:

1. todos os pontos de criação, coalescência, reserva, preparo, retry e conclusão
   de report no core e no executor;
2. as correlações reais do transporte para ACK interno e externo;
3. todos os produtores e consumidores do payload nos dois targets;
4. os três construtores/parsers de frame de cada target e o limite IEEE
   802.15.4;
5. a precedência do registry para conhecido, desconhecido e indisponível;
6. retorno, atomicidade e buffers da UART do coordenador;
7. tolerância do parser e preservação da linha no `SmartHome-Hub` vigente;
8. disponibilidade e propriedades documentadas de `esp_fill_random()` no H2;
9. tamanhos reais de `IsspDevice`, `HardwareState`, buffers e reservas;
10. consumers diretos da API pública, inclusive exemplo e fixtures;
11. as relações com cada especificação da seção 2;
12. os critérios e meios host-native versus H2 físico sem executar testes ou
    hardware sem autorização;
13. a extração privada da política de DATA e a ausência de segundo fluxo de
    produção;
14. a sequência sem espera de lock, consulta de espaço, escrita, cache e ACK.

A análise classifica **não pronta — defeito da especificação** se faltar
contrato necessário; **não implementável no recorte — pré-requisito
arquitetural** se a solução exigir persistência, sessão, mudança do host,
aumento de reservas, API além da DEC-010 ou alteração funcional do deep sleep.
Nesses casos deve recomendar especificação complementar, não acumular a mudança
nesta fonte.

## 17. Estado do workflow

A v0.2 recebeu análise `Ready`, foi implementada e passou pela Revisão. A
evidência de hardware do caminho principal foi relatada pelo Arquiteto como
funcional, sem converter automaticamente cenários adversos não executados em
sucesso.

A Revisão devolveu dois ajustes normativos incorporados nesta v0.3:

1. o exemplo de `event_id` passa a usar exatamente a forma textual existente de
   `device_id`, preservando a implementação;
2. a aceitação UART sem espera e a regra “aceitar localmente antes de ACKar”
   passam a abranger também origem desconhecida admitida durante commissioning.

O parser do coordenador aceitar payload maior que 20 bytes permanece defeito de
implementação contra contrato já vigente; a v0.3 apenas torna explícito o caso
de aceite e o teste já pertencente ao AC-004.

Como o item 2 altera comportamento normativo, a análise `Ready` da v0.2 não
cobre esta revisão. A Análise de Implementabilidade própria da v0.3 classificou
o conteúdo normativo registrado em `9687287` como `Ready`, sem bloqueador.

Por ordem explícita do Arquiteto, a v0.3 foi implementada. D1 e D2 já estavam
satisfeitos pela implementação vigente e não geraram mutação; a recusa de
payload excedente foi aplicada às duas cópias da verificação de comprimento no
coordenador, com motivos de log distinguíveis, e coberta pelos dois casos
host-native que a seção 14.1 vincula ao AC-004. O build canônico ESP32-C6 do
`coordinator_154` e os builds host-native afetados foram concluídos; execução de
testes, flash, monitor e hardware permanece `Not Executed` por falta de
permissão operacional própria. O registro está em
`docs/reports/report-identity/implementation/2026-08-12-v03-implementation.md`.
A próxima etapa é a Revisão do delta.
