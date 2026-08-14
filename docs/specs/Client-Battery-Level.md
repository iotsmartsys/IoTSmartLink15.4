# Nível de bateria do client

**ID:** `EKOM-BATTERY-001`

**Classe da fonte:** Normativa

**Versão:** 0.2

**Estado do workflow:** `Draft`

**Análise de implementabilidade:** Pendente para a v0.2. A v0.1 recebeu
classificação **Não pronta — defeito da especificação**; os defeitos apontados
foram corrigidos nesta versão e a análise anterior não se aplica a ela.

**Bloqueio arquitetural:** Nenhum

**Responsável arquitetural:** Marcelo Miranda

**Última atualização:** 13/08/2026

**Escopo:** `client_154`, fachada `SmartSysApp`, product firmware e board model

**Relações normativas e de dependência:**

- Nova [`New`] — não existe autoridade anterior para telemetria de bateria no
  client;
- Depende de [`Depends On`] `docs/adr/ADR-0005-TELEMETRY-ENDPOINTS.md@Proposed`
  — identidade de endpoint de telemetria e registro normativo dos tipos de
  evento; enquanto a ADR permanecer `Proposed`, esta especificação não pode
  receber recomendação de prontidão;
- Depende de [`Depends On`] `docs/specs/Client-Deep-Sleep.md@v0.11` — estado
  material necessário: o boot operacional, o ciclo acordado e a drenagem de
  reports pendentes precisam existir como comportamento implementado. A
  dependência é do comportamento, não da conclusão daquela versão, que
  permanece `In Progress`; nada aqui a emenda;
- Altera [`Amends`] `docs/specs/Firmware-Variants-Menuconfig.md` — a composição
  enumerada do produto `Door sensor battery H2` passa a incluir um recurso
  físico de medição de bateria e a capability correspondente. O mecanismo de
  declaração e validação de recursos é preservado integralmente; apenas a
  enumeração daquele produto é estendida;
- Apoia-se em `docs/adr/ADR-0002-PRODUCT-BOARD-COMPOSITION.md@Accepted` — a nova
  classe de recurso é exercício da regra de composição produto/board, não desvio
  dela;
- Apoia-se em `docs/adr/ADR-0001-ISSP-COMPONENT-BOUNDARIES.md@Accepted` — a
  configuração da capability leva unidade, canal e atenuação de ADC à fachada
  pública. Isso se apoia no precedente já vigente de `gpio_num_t` em
  `SmartSysApp.h`, não rompe a direção de dependência e não expõe tipo de
  protocolo ou de transporte. Se o Arquiteto considerar que o critério de
  reavaliação daquela ADR foi alcançado, a decisão é dele;
- Preserva `docs/specs/ISSP-Configurable-Bootstrap.md`,
  `docs/specs/ISSP-Reusable-Components.md` e
  `docs/specs/ISSP-Report-Identity.md` — nenhuma mudança de wire, de versão de
  protocolo, de checksum, de sequência ou de identidade de report.

Esta especificação **não** emenda `Client-Deep-Sleep.md`. A configuração da
capability vive fora de `DeepSleepConfig`, de modo que nenhuma versão em
implementação daquela fonte é reaberta por este recorte.

## 1. Objetivo e contexto

Um client alimentado por bateria não oferece hoje nenhuma forma de o host
conhecer sua carga. A ausência é observável: um dispositivo que para de reportar
por bateria esgotada é indistinguível de um dispositivo fora de alcance.

Esta especificação define a capability de nível de bateria do client: ela mede a
tensão da bateria por entrada analógica com divisor resistivo, converte a
medição em percentual inteiro e a publica como report ISSP. O ISSP é apenas o
meio de transporte do valor; o recurso especificado é do dispositivo.

O contrato é escrito em três camadas explicitamente separadas, e essa separação
é normativa:

1. o **contrato genérico** da capability, sem nenhum número, química, pino ou
   resistor;
2. os **parâmetros do produto**, fornecidos pelo product firmware;
3. os **fatos elétricos do board model**, apenas repassados pelo produto.

A configuração concreta do `door_sensor_battery_h2` é registrada na seção 8 e é
**informativa**: descreve o primeiro produto, não a capability.

## 2. Escopo

- medição de tensão de bateria por entrada analógica com divisor resistivo;
- conversão em percentual inteiro de 0 a 100;
- publicação como report ISSP em par `endpointId`/`eventType` de telemetria;
- duas regras de gatilho, conforme a composição possua ou não deep sleep;
- classificação de falha, invalidez elétrica e saturação;
- invariantes de configuração validados pela fachada;
- oferta, pelo board model, do recurso físico de medição.

## 3. Fora de escopo

- estimativa de autonomia, curva de descarga por trechos, compensação por
  temperatura, corrente ou envelhecimento;
- persistência de qualquer estado entre ciclos de energia, inclusive RTC memory;
- carga, proteção, balanceamento ou gestão de bateria;
- alteração do protocolo ISSP, do wire, do ACK, do retry ou da identidade de
  report;
- alteração do coordenador ou do host;
- light sleep, fontes de wakeup e política de energia, que pertencem a
  `Client-Deep-Sleep.md`;
- qualquer decisão sobre o projeto ESP-IDF da raiz, que permanece não
  classificado sob `EKM-GAP-0007`.

### 3.1 Arquitetura e organização

**Precedente aplicável:** `addDoorSensorCapability` e `addSwitchPlugCapability`
em `components/issp_app_154` para a forma da capability;
`DigitalInputBehavior` em `components/issp_behaviors` para amostragem periódica
com temporizador próprio; `board_model.hpp` e a composição em
`client_154/main/CMakeLists.txt` para a oferta de recurso físico.

**Elementos preservados:**

- o board model oferece recurso elétrico; o product firmware define política; a
  fachada não descobre pinagem;
- a composição CMake valida produto contra board por recurso requerido;
- capabilities são registradas antes de `setup()`, com par `endpointId` e
  `eventType` único no dispositivo;
- a ordem entre registrar uma capability e configurar deep sleep permanece
  irrelevante enquanto o estado for `Configuring`;
- client e coordenador continuam sem dependência de código entre si.

**Desvios arquiteturais explícitos:** são dois, ambos deliberados.

1. **Primeiro report incondicional.** O precedente `reportOnStart` torna a
   primeira publicação configurável por produto; aqui ela é obrigatória. A regra
   de variação compara contra o último percentual publicado, de modo que sem o
   primeiro report não existe base de comparação e a capability permaneceria
   muda por tempo indeterminado. Um produto não pode desabilitar essa primeira
   publicação.
2. **Falha de configuração do ADC não impede `Running`.** O precedente é que
   `IsspDevice::start()` aborta no primeiro behavior cujo `begin()` falha, e
   alcançar `Running` prova que todos tiveram sucesso. A capability de bateria é
   exceção: telemetria não deve impedir a função principal do produto, e um
   sensor de porta com medição defeituosa continua reportando porta. O custo
   aceito é que a ausência da capability é silenciosa para o host e observável
   apenas em log local.

### 3.2 Limite de escopo funcional

**Capacidades arquiteturais pressupostas:** registro de capabilities e
publicação de reports pela fachada; amostragem periódica por temporizador dentro
de um behavior; ciclo de boot operacional do deep sleep, quando presente. Todas
existem na baseline.

**Preparação arquitetural separada:** Não aplicável. Nenhuma capacidade ausente
foi identificada. A ausência de retenção entre ciclos de energia é decisão de
contrato, não limitação contornada: as regras de gatilho foram definidas para
não exigir RTC memory.

## 4. Requisitos

Todos os requisitos desta seção pertencem à **camada genérica** e não contêm
valores concretos.

- **`BATTERY-001`:** a capability mede a tensão da bateria por entrada analógica
  com divisor resistivo, cujos valores elétricos são recebidos do board model.
- **`BATTERY-002`:** cada medição é a média aritmética de `samples` amostras
  consecutivas, separadas por `sampleIntervalMs`.
- **`BATTERY-003`:** a tensão medida é convertida em percentual inteiro por
  interpolação linear entre `emptyMv`, que corresponde a 0%, e `fullMv`, que
  corresponde a 100%, com aritmética inteira e arredondamento do meio para cima,
  conforme a seção 5.1.
- **`BATTERY-004`:** o percentual publicado é sempre saturado no intervalo de 0
  a 100; nenhum outro valor pode alcançar o wire.
- **`BATTERY-005`:** o percentual é publicado como report ISSP no par
  `endpointId` e `eventType` configurado, único entre as capabilities do
  dispositivo.
- **`BATTERY-006`:** a capability é somente leitura; comando dirigido ao seu par
  `endpointId`/`eventType` é respondido com `Unsupported`.
- **`BATTERY-007`:** em composição com deep sleep habilitado, a capability mede
  e publica em todo boot operacional, qualquer que seja a causa do wakeup, sem
  condição de variação.
- **`BATTERY-008`:** em composição sem deep sleep, a capability mede
  periodicamente a cada `samplePeriodMs` e publica quando o percentual diferir
  do último publicado em pelo menos `reportDeltaPercent` pontos.
- **`BATTERY-009`:** o primeiro percentual válido obtido após o início da
  capability é sempre publicado, independentemente de `reportDeltaPercent`, e
  estabelece a base de comparação.
- **`BATTERY-010`:** o último percentual publicado é mantido apenas em memória
  volátil; nenhum estado da capability sobrevive a um ciclo de energia.
- **`BATTERY-011`:** a configuração é rejeitada quando `samples` for zero,
  quando `reportDeltaPercent` estiver fora do intervalo de 1 a 100, quando
  `fullMv` não for maior que `emptyMv`, ou quando a resistência inferior do
  divisor declarada pelo board for zero. O invariante de `reportDeltaPercent` é
  incondicional, inclusive em composições cujo gatilho não o exercita.
- **`BATTERY-012`:** a coerência entre `samplePeriodMs` e o gatilho é verificada
  em `setup()`, e não no registro da capability, de modo a preservar a
  irrelevância da ordem de configuração.
- **`BATTERY-013`:** erro do ADC ou amostra eletricamente inválida suprime
  somente o report de bateria do ciclo corrente, sem abortar o dispositivo e sem
  afetar boot, demais capabilities ou ciclo de energia.
- **`BATTERY-014`:** percentual fora do intervalo definido por `emptyMv` e
  `fullMv`, quando eletricamente válido, não é falha: satura e é publicado.
- **`BATTERY-015`:** calibração de ADC indisponível não impede a operação; a
  capability converte a leitura bruta linearmente entre zero e a tensão de fundo
  de escala correspondente à atenuação declarada pelo board, registra a
  degradação e continua publicando.
- **`BATTERY-016`:** falha ao configurar a unidade ou o canal do ADC no início
  da capability não impede o dispositivo de alcançar `Running`. A capability
  permanece inerte, sem publicar, e a condição é registrada em log local;
  nenhuma outra capability e nenhum estágio de `setup()` é afetado.
- **`BATTERY-017`:** o report de bateria não integra a evidência de admissão de
  deep sleep. Sua publicação segue as regras de drenagem de reports pendentes já
  contratadas em `Client-Deep-Sleep.md`, mas o ciclo acordado nunca aguarda por
  ele para autorizar o sleep antecipado.

## 5. Fluxos, estados e contratos

### 5.1 Camada genérica — fórmula e invariantes

Sendo `Vpino` a tensão média no pino, `Rtop` e `Rbottom` as resistências do
divisor e `Vbat` a tensão da bateria:

```text
Vbat_mV = Vpino_mV * (Rtop + Rbottom) / Rbottom

span    = fullMv - emptyMv
pct     = ( (Vbat_mV - emptyMv) * 100 + span / 2 ) / span
pct     = satura(pct, 0, 100)
```

A conversão usa **aritmética inteira**, e o arredondamento é do **meio para
cima**, expresso pelo termo `span / 2` acima. Ponto flutuante não é exigido em
nenhum ponto do contrato. A saturação é aplicada depois do arredondamento, de
modo que a expressão intermediária pode ser negativa ou maior que 100 sem violar
`BATTERY-004`.

Invariantes normativos, todos derivados e nenhum escolhido por julgamento:

| Invariante | Origem |
|---|---|
| `samples >= 1` | média inexistente e divisão por zero com zero amostras |
| `1 <= reportDeltaPercent <= 100` | domínio do próprio percentual: 0 publicaria a cada amostragem, acima de 100 é inalcançável e tornaria a capability permanentemente muda |
| `fullMv > emptyMv` | a interpolação é indefinida ou invertida em caso contrário |
| resistência inferior do divisor maior que zero | a razão do divisor divide por ela; zero torna a tensão indeterminável |
| `samplePeriodMs` coerente com o gatilho | zero em composição sem deep sleep nunca mediria; não nulo em composição com deep sleep cria temporizador que não chega a disparar |

`sampleIntervalMs` **não** possui faixa normativa, e o valor zero é
explicitamente admitido, significando amostras consecutivas sem espera. A
divergência em relação ao precedente do botão de factory reset, que rejeita
intervalo zero, é intencional: lá o laço de polling é contínuo, aqui é limitado
por `samples`.

Nenhuma outra faixa numérica é normatizada. Em particular, não existem limites
normativos para `samples`, `sampleIntervalMs` ou `samplePeriodMs` além dos
invariantes acima, porque nenhuma evidência de plataforma, protocolo ou
documento vigente os sustenta.

**Exatidão e transbordo.** A escolha das larguras inteiras intermediárias
pertence à implementação e não é prescrita aqui. O contrato exige apenas que a
média das amostras e a conversão produzam o percentual exato para os valores
declarados pelo board e pelo produto, sem transbordo. Nenhum tipo, largura ou
ordem de operações é imposto.

**Dimensionamento (informativo, não normativo):** em composição com deep sleep,
`samples` multiplicado por `sampleIntervalMs` consome parte da janela acordada
definida por `maxAwakeTimeMs` em `Client-Deep-Sleep.md`. O dimensionamento é
responsabilidade do produto; a fachada não verifica essa relação.

### 5.2 Camada do produto — parâmetros

Fornecidos pelo product firmware ao configurar a capability:

| Parâmetro | Significado |
|---|---|
| `emptyMv` | tensão correspondente a 0% |
| `fullMv` | tensão correspondente a 100% |
| `samples` | número de amostras por medição |
| `sampleIntervalMs` | espera entre amostras consecutivas |
| `samplePeriodMs` | período de amostragem sem deep sleep; zero significa sem amostragem periódica |
| `reportDeltaPercent` | variação mínima, em pontos, para publicar sem deep sleep |
| `endpointId` | endpoint de telemetria do report |
| `eventType` | tipo de evento do report |

`emptyMv` e `fullMv` pertencem ao produto porque decorrem da química e do pack
escolhidos, não da fórmula. `samples` e `sampleIntervalMs` pertencem ao produto
porque trocam ruído por tempo acordado. `reportDeltaPercent` pertence ao produto
porque troca tráfego de rádio por resolução.

### 5.3 Camada do board model — fatos elétricos

O board model que oferece o recurso de medição declara a unidade e o canal do
ADC, a atenuação e as duas resistências do divisor. O produto recebe esses
valores e os repassa à capability sem reescrevê-los; a fachada nunca descobre
pinagem por conta própria.

O recurso corresponde a um divisor permanentemente conectado à bateria. A
drenagem contínua resultante é fato conhecido do arranjo elétrico e risco
residual aceito nesta versão, registrado na seção 9.

A composição CMake passa a exigir o recurso de medição de bateria para todo
produto que registre esta capability, pelo mesmo mecanismo já aplicado aos
demais recursos: produto sem board compatível falha na configuração do build.

### 5.4 Identidade e transporte

O par `endpointId` e `eventType` é a chave de roteamento da capability. No ISSP,
`endpointId` é um campo de um byte do frame, ao lado de `eventType` e `value`;
não existe cluster, atributo, descritor, binding nem descoberta de lista de
endpoints. **Não é endpoint no sentido Zigbee e não introduz estrutura nova de
transporte.**

Portanto esta especificação não altera wire, versão de protocolo, checksum,
endianness, sequência, tipo de frame nem tamanho de payload. O percentual ocupa
o campo `value` de 8 bits já existente.

A distinção entre endpoint funcional e endpoint de telemetria, e o registro
normativo dos tipos de evento, pertencem à `ADR-0005` e não são redefinidos
aqui.

### 5.5 Gatilhos

| Composição | Gatilho | Condição de publicação |
|---|---|---|
| Com deep sleep | todo boot operacional, qualquer que seja a causa do wakeup | incondicional |
| Sem deep sleep | temporizador próprio a cada `samplePeriodMs` | primeira medição válida, ou variação de ao menos `reportDeltaPercent` pontos |

As duas regras são coerentes entre si: em composição com deep sleep nenhum
estado sobrevive ao ciclo de energia, de modo que toda medição é a primeira, e a
publicação incondicional é o mesmo comportamento generalizado.

## 6. Falhas e condições de borda

Três classes distintas, avaliadas nesta ordem:

**Classe 1 — erro retornado pelo ADC.** A leitura de amostra ou a conversão
calibrada retorna erro. Não existe valor. A medição é descartada, nenhum report
de bateria é publicado no ciclo corrente e o dispositivo prossegue. A capability
**não** aborta o dispositivo diante desse erro.

**Classe 2 — valor eletricamente inválido.** Amostra situada em qualquer um dos
extremos da escala de conversão, ambos deriváveis do bitwidth configurado e da
razão do divisor declarada pelo board:

- extremo inferior: o pino está em tensão nula, o que indica divisor aberto,
  curto ou ausência de bateria; a tensão real não é determinável, porque
  qualquer valor abaixo do limiar produz a mesma leitura;
- extremo superior: a conversão deixa de ser injetiva, e a tensão real pode ser
  qualquer valor acima do topo da escala. Com divisor dimensionado para manter a
  bateria cheia abaixo do fundo de escala, a saturação superior indica defeito, e
  não bateria cheia.

Consequência idêntica à classe 1. As classes 1 e 2 são avaliadas **por
amostra**: uma única amostra com erro ou em um dos extremos invalida a medição
inteira, em vez de contaminar a média.

**Classe 3 — valor fora de `emptyMv` ou `fullMv`, eletricamente válido.** Não é
falha. O percentual satura em 0 ou 100 e é publicado normalmente. É justamente o
caso em que o report tem maior valor: bateria abaixo de `emptyMv` deve alcançar
o host como 0%, e não como silêncio.

**Calibração indisponível.** Não é falha. A capability converte a leitura bruta
linearmente entre zero e a tensão de fundo de escala correspondente à atenuação
que o board declara, obtida da fonte do alvo; nenhum valor literal de fundo de
escala pertence a esta especificação, porque ele varia com a atenuação e com o
alvo. A degradação é registrada em log e a publicação continua. O host não
distingue valor calibrado de aproximado; isso é risco residual declarado na
seção 9.

**Falha na configuração do ADC.** Distinta das três classes acima, porque ocorre
antes de qualquer medição. A unidade ou o canal não puderam ser configurados no
início da capability. Conforme `BATTERY-016` e o segundo desvio da seção 3.1, o
dispositivo alcança `Running` normalmente, a capability permanece inerte e a
condição é registrada em log local.

**Retentativa.** Nova tentativa de medição dentro do mesmo ciclo, após falha,
não pertence a este contrato: o resultado observável já é a ausência do report,
e a escolha é da implementação. O próximo gatilho realiza nova medição
normalmente.

**Supressão e ambiguidade.** A supressão do report em falha é indistinguível,
para o host, de perda de rádio. É consequência aceita da decisão de não
introduzir valor sentinela, que manteria o domínio do evento restrito a 0–100.

## 7. Critérios de aceite e validações

### `BATTERY-AC-001` — conversão linear e saturação

**Cobre:** `BATTERY-003`, `BATTERY-004`, `BATTERY-014`

- **Dado que** a capability está configurada com `emptyMv` e `fullMv` válidos;
- **Quando** a tensão média corresponder a `emptyMv`, a `fullMv`, ao ponto
  médio, a um valor cuja fração seja exatamente meio ponto percentual, a um
  valor abaixo de `emptyMv` e a um valor acima de `fullMv`;
- **Então** o percentual publicado é, respectivamente, 0, 100, o valor
  interpolado, o inteiro imediatamente superior, 0 e 100, e nenhum valor fora de
  0 a 100 alcança o wire;
- **Evidência:** inspeção do delta e validação em hardware com fonte variável,
  reservada a etapa posterior.

### `BATTERY-AC-002` — média das amostras

**Cobre:** `BATTERY-002`

- **Dado que** a capability está configurada com `samples` maior que um;
- **Quando** uma medição é realizada;
- **Então** o percentual resulta da média aritmética das amostras lidas, e não
  de uma leitura isolada;
- **Evidência:** inspeção do delta; validação em hardware reservada a etapa
  posterior.

### `BATTERY-AC-003` — gatilho em composição com deep sleep

**Cobre:** `BATTERY-007`, `BATTERY-010`

- **Dado que** um produto com deep sleep habilitado registra a capability;
- **Quando** o dispositivo acorda por timer e, em outro ciclo, por contato seco;
- **Então** um report de bateria é publicado em ambos os boots, sem condição de
  variação;
- **Evidência:** validação em hardware ESP32-H2, reservada a etapa posterior.

### `BATTERY-AC-004` — gatilho periódico e variação mínima

**Cobre:** `BATTERY-008`, `BATTERY-009`

- **Dado que** um produto sem deep sleep registra a capability com
  `samplePeriodMs` não nulo;
- **Quando** ocorrem medições sucessivas cuja variação é menor que
  `reportDeltaPercent` e, depois, uma medição cuja variação alcança o limiar;
- **Então** a primeira medição válida é publicada, as intermediárias não geram
  report, e a que alcança o limiar é publicada e passa a ser a nova base;
- **Evidência:** inspeção do delta; validação em hardware reservada a etapa
  posterior.

### `BATTERY-AC-005` — falha de medição não interrompe o dispositivo

**Cobre:** `BATTERY-013`

- **Dado que** a capability está registrada e operando;
- **Quando** a leitura do ADC retorna erro, ou uma amostra recai em um dos
  extremos da escala;
- **Então** nenhum report de bateria é publicado no ciclo, o dispositivo não
  aborta, e boot, demais capabilities e ciclo de energia seguem inalterados;
- **Evidência:** inspeção do delta; validação em hardware, com o divisor
  desconectado, reservada a etapa posterior.

### `BATTERY-AC-006` — invariantes de configuração

**Cobre:** `BATTERY-011`, `BATTERY-012`

- **Dado que** um produto configura a capability;
- **Quando** informar `samples` igual a zero, `reportDeltaPercent` igual a zero
  ou acima de 100, `fullMv` menor ou igual a `emptyMv`, ou `samplePeriodMs`
  incoerente com o gatilho da composição;
- **Então** a configuração é rejeitada, os três primeiros no registro e o último
  em `setup()`, e o dispositivo não alcança `Running` com configuração
  incoerente;
- **Evidência:** inspeção do delta e build canônico.

### `BATTERY-AC-007` — identidade e ausência de comando

**Cobre:** `BATTERY-005`, `BATTERY-006`

- **Dado que** a capability está registrada em endpoint de telemetria;
- **Quando** outra capability tentar registrar o mesmo par, e quando um comando
  for dirigido ao par da bateria;
- **Então** o registro duplicado é rejeitado e o comando é respondido com
  `Unsupported`;
- **Evidência:** inspeção do delta; validação em hardware reservada a etapa
  posterior.

### `BATTERY-AC-008` — operação sem calibração

**Cobre:** `BATTERY-015`

- **Dado que** o esquema de calibração do ADC não está disponível no alvo;
- **Quando** uma medição é realizada;
- **Então** a conversão ocorre pelo fundo de escala, a degradação é registrada e
  o report continua sendo publicado;
- **Evidência:** validação em hardware reservada a etapa posterior.

### `BATTERY-AC-009` — falha de configuração do ADC não impede a operação

**Cobre:** `BATTERY-016`

- **Dado que** um produto registra a capability e a configuração da unidade ou
  do canal do ADC falha;
- **Quando** `setup()` é executado;
- **Então** o dispositivo alcança `Running`, as demais capabilities operam
  normalmente, a capability de bateria permanece inerte sem publicar, e a
  condição fica registrada em log local;
- **Evidência:** inspeção do delta; validação em hardware reservada a etapa
  posterior.

### `BATTERY-AC-010` — bateria não retém o dispositivo acordado

**Cobre:** `BATTERY-017`

- **Dado que** um produto com deep sleep habilitado registra a capability;
- **Quando** a medição falhar em um boot, e quando ela tiver sucesso em outro;
- **Então** a admissão de sleep antecipado permanece exatamente a mesma dos dois
  casos, sem prolongar a janela acordada à espera do report de bateria, e o
  report bem-sucedido continua sendo transmitido pela drenagem de pendentes;
- **Evidência:** inspeção do delta; validação em hardware ESP32-H2 reservada a
  etapa posterior.

### 7.1 Evidências planejadas

- **Artefatos de teste no recorte:** Nenhum. Nenhum teste automatizado é criado
  ou alterado por esta versão, e nenhum artefato de teste integra o recorte de
  implementação.
- evidência de construção: build canônico ESP32-H2 dos entregáveis afetados,
  obrigação transversal do Implementador;
- evidência de inspeção: revisão do delta contra os requisitos e critérios desta
  seção;
- evidência de hardware, reservada a etapa posterior e a autorização própria:
  medição com fonte variável cobrindo `emptyMv`, `fullMv`, ponto médio e valores
  fora da faixa; ciclo de deep sleep com wakeup por timer e por contato; divisor
  desconectado para a classe 2; alvo sem calibração disponível para
  `BATTERY-AC-008`.

Compilação não comprova execução. A ausência de teste automatizado nesta versão
é decisão registrada, e o risco correspondente está na seção 9.

## 8. Configuração inicial do `door_sensor_battery_h2` — informativa

Esta seção **não é normativa**. Ela registra a configuração do primeiro produto
que adota a capability, para rastreabilidade. Nenhum valor abaixo é regra da
capability, e alterá-los não exige emendar as seções 4 a 7.

**Fatos do board model `Door Sensor Battery H2`:**

| Elemento | Valor |
|---|---|
| Unidade e canal do ADC | ADC1, canal 0, correspondente a GPIO1 no ESP32-H2 |
| Atenuação | 12 dB |
| `Rtop` | 470 kΩ |
| `Rbottom` | 220 kΩ |
| Habilitação do divisor | inexistente; divisor permanentemente conectado |

**Parâmetros do product firmware:**

| Parâmetro | Valor |
|---|---|
| `emptyMv` | 3300 |
| `fullMv` | 4150 |
| `samples` | 8 |
| `sampleIntervalMs` | 5 |
| `samplePeriodMs` | 0, porque o gatilho é o boot operacional |
| `reportDeltaPercent` | 5 |
| `endpointId` | 2 |
| `eventType` | 3 |

O valor 5 de `reportDeltaPercent` satisfaz o invariante incondicional de
`BATTERY-011`, mas **não é exercitado** neste produto, cujo gatilho é o boot
operacional. Ele passa a valer se o mesmo produto vier a operar sem deep sleep.

O evento 3 é o já registrado para nível de bateria em percentual; o coordenador
o traduz para o host sem qualquer alteração. A origem histórica desses valores
elétricos é o projeto ESP-IDF da raiz, que permanece não classificado sob
`EKM-GAP-0007` e **não** é fonte normativa.

## 9. Riscos, pendências e decisões do Arquiteto

**Decisões confirmadas pelo Arquiteto nesta versão:**

- percentual de 0 a 100 como resultado observável, sem valor sentinela para
  falha;
- interpolação linear, sem tabela por trechos;
- média de amostras, sem mediana nem descarte de extremos;
- divisor permanentemente ligado, sem GPIO de habilitação;
- endpoint dedicado de telemetria, com o tipo de evento já registrado;
- configuração própria da capability, fora de `DeepSleepConfig`;
- reporte incondicional a cada wakeup em composição com deep sleep, e por
  variação em composição sem deep sleep;
- toda política parametrizada pelo produto, sem faixas normativas escolhidas por
  julgamento;
- primeiro report incondicional, como desvio explícito do precedente
  `reportOnStart`;
- calibração indisponível como modo degradado, não como falha;
- nenhum artefato de teste no recorte.

**Decisões acrescentadas na v0.2, em resposta à análise da v0.1:**

- arredondamento inteiro com o meio para cima, sem ponto flutuante;
- fallback sem calibração definido pelo fundo de escala da atenuação declarada
  pelo board, sem literal nesta especificação;
- falha de configuração do ADC **não** impede `Running`, como segundo desvio
  arquitetural explícito;
- o report de bateria **não** integra a evidência de admissão de deep sleep,
  preservando `Client-Deep-Sleep` sem emenda;
- invariante incondicional de `reportDeltaPercent`, com valor válido registrado
  também para o produto cujo gatilho não o exercita;
- transbordo tratado como responsabilidade da implementação, com exigência
  apenas de exatidão para os valores declarados.

**Riscos residuais aceitos:**

- drenagem contínua do divisor permanentemente conectado, inerente ao arranjo
  elétrico da placa atual;
- ausência de prova automatizada da fórmula de conversão nesta versão;
- indistinguibilidade, no host, entre supressão por falha e perda de rádio;
- indistinguibilidade, no host, entre valor calibrado e valor aproximado no modo
  degradado.

**Pendências:**

- a `ADR-0005` está em `Proposed`; sua aceitação é do Arquiteto e condiciona
  qualquer recomendação de prontidão desta especificação;
- o mapa de conhecimento e o changelog ainda não registram esta especificação
  nem a ADR; a atualização não integrou a autorização desta atuação;
- a v0.2 ainda não foi analisada; a análise da v0.1 concluiu **Não pronta —
  defeito da especificação** e não se aplica a esta versão;
- a ausência da capability por falha de configuração do ADC é silenciosa para o
  host, consequência aceita do segundo desvio arquitetural;
- os valores elétricos da seção 8 foram confirmados pelo Arquiteto por
  declaração, sem medição em placa registrada nesta atuação.
