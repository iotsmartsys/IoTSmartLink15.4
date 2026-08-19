# Sensor de presença a bateria ESP32-H2

**ID:** `EKOM-PRESENCE-001`

**Classe da fonte:** Normativa

**Versão:** 0.1

**Estado normativo:** `Active`

**Estado da implementação:** Não iniciada [`Not Started`]

**Estado do workflow:** Rascunho [`Draft`]; submetida à Análise de
Implementabilidade

**Responsável arquitetural:** Marcelo Miranda

**Última atualização:** 19/08/2026

**Escopo:** `client_154`, capability de presença, composição de produto e board,
deep sleep do client e tradução semântica do evento no coordenador

**Relações normativas:**

- Nova [`New`] — define o product firmware `Presence sensor battery H2` e a
  capability de presença;
- Altera [`Amends`] `docs/specs/Firmware-Variants-Menuconfig.md` — acrescenta
  uma terceira variante, generaliza o board de entrada digital a bateria e
  preserva a seleção estática de exatamente um produto e um board;
- Altera [`Amends`] `docs/specs/Client-Deep-Sleep.md@v0.11` — acrescenta uma
  entrada digital de presença como fonte EXT1, sem mudar o contrato vigente do
  contato do sensor de porta;
- Aplica `docs/specs/Client-Battery-Level.md@v0.5` e
  `docs/specs/Client-SDK-Configurable-Features.md@v0.1` à nova composição, com
  os valores concretos desta fonte;
- Depende da emenda aceita de `docs/adr/ADR-0005-CAPABILITY-IDENTITY.md`, que
  aloca o event type 5 à capability de presença;
- Preserva ADR-0001, ADR-0002, ADR-0003, ADR-0004,
  `docs/specs/ISSP-Architecture.md` e
  `docs/specs/ISSP-Report-Identity.md` no restante.

---

## 1. Objetivo e contexto

Criar um firmware de sensor de presença alimentado por bateria, no ESP32-H2,
que observe a saída digital de um sensor PIR ou radar no GPIO 14, publique
`detected` e `undetected` pelo ISSP e use deep sleep entre os ciclos acordados.

O PIR ou radar permanece alimentado durante o deep sleep. Sua saída é ativa em
nível alto e permanece eletricamente disponível para acordar o ESP32-H2. O
produto reutiliza a pinagem e a eletrônica do board hoje dedicado ao sensor de
porta, mas o board passa a possuir nome e recursos digitais genéricos para
servir aos dois produtos sem atribuir semântica de porta ao hardware.

A capability de porta não é reutilizada como presença: o tipo do evento é a
natureza da capability conforme a ADR-0005, e o coordenador deve apresentar a
nova capability como `Presence Sensor`.

## 2. Escopo

- product firmware `Presence sensor battery H2`, com `deviceId=0x15400002`;
- capability de presença somente leitura no endpoint 1 e event type 5;
- valores `1 = detected` e `0 = undetected`;
- entrada digital GPIO 14, ativa em HIGH e com pull-up;
- estabilização inicial, debounce e publicação de transições pelo behavior
  digital reutilizável vigente;
- report inicial em todo boot operacional;
- deep sleep com deadline acordado, timer, wake LED e wakeup EXT1 pela entrada;
- nível de bateria no endpoint 2 e estado da telemetria no endpoint 3;
- generalização do board existente para `Battery Digital Sensor H2` e dos seus
  recursos de entrada e wakeup, preservando os fatos elétricos;
- preservação funcional do product firmware de porta ao migrá-lo para o board
  e recursos generalizados;
- seleção e compatibilidade estáticas por Kconfig/CMake;
- registro e tradução do event type 5 no coordenador;
- atualização da navegação normativa materialmente afetada.

## 3. Fora de escopo

- alterar layout, versão, checksum, tamanho ou endianness do protocolo wire;
- alterar commissioning, ACK, retry, identidade de reports, registry ou
  persistência;
- alimentar, desligar ou temporizar o PIR/radar pelo ESP32-H2;
- interpretar distância, intensidade, zonas ou múltiplos alvos;
- impor tempo mínimo ou máximo de permanência de `detected`;
- criar rate limit de wakeups, retenção de estado em RTC memory ou filtragem
  adicional entre boots;
- alterar a química, a fórmula ou os parâmetros vigentes da bateria;
- mudar a tomada, o coordenador além da tradução do novo evento, ou o projeto
  ESP-IDF não classificado da raiz;
- criar ou alterar qualquer arquivo, aplicação, caso, double ou infraestrutura
  de teste;
- executar testes, flash, monitor ou validação em hardware sem autorização
  operacional própria.

## 4. Identidade e semântica

| Elemento | Valor normativo |
|---|---|
| Product firmware | `Presence sensor battery H2` |
| Símbolo proposto | `IOTSMARTLINK154_PRODUCT_PRESENCE_SENSOR_BATTERY_H2` |
| Device ID | `0x15400002` |
| Capability principal | `Presence Sensor` |
| Endpoint | 1, congelado |
| Event type | 5, global e estável |
| Valor ativo | 1, serializado pelo host como `detected` |
| Valor inativo | 0, serializado pelo host como `undetected` |
| Direção | somente leitura |

O tipo 5 pertence à capability, não ao produto. O produto não o recebe como
parâmetro nem pode sobrescrevê-lo. Comando dirigido ao endpoint 1/event type 5
é reconhecido e respondido com `Unsupported`, sem alterar o estado físico.

O coordenador acrescenta o tipo `Presence Sensor` ao registro já guardado
contra a ADR-0005. O valor textual é `detected` somente para 1 e `undetected`
para 0. Esta extensão usa os campos existentes de endpoint, evento e valor;
nenhuma mudança wire decorre dela.

## 5. Board model e composição

O board `Door Sensor Battery H2` é generalizado e renomeado para
`Battery Digital Sensor H2`. O renome abrange nome de fonte, símbolo Kconfig,
rótulo, seleção CMake e referência vigente no `client_154/sdkconfig`. Registros
históricos preservam o nome observado à época.

O board oferece:

| Recurso | Fato físico |
|---|---|
| `digital_input` | GPIO 14, ativo em HIGH, pull-up |
| `digital_input_wakeup` | o mesmo GPIO 14, elegível para EXT1 no ESP32-H2 |
| `wake_led` | GPIO 13, ativo em HIGH |
| `user_button` | GPIO configurável, default 9, ativo em LOW |
| `battery_measurement` | ADC1 canal 0, 12 dB, divisor 470 kΩ/220 kΩ |

`DryContactInputResource` e o acessador orientado a contato são generalizados
para `DigitalInputResource` e acessador equivalente. Os nomes de recursos CMake
`dry_contact_input` e `dry_contact_wakeup` passam a `digital_input` e
`digital_input_wakeup`.

O product firmware de porta migra mecanicamente para esses recursos. Continua
sendo a única fonte da semântica de porta e preserva device ID, endpoint,
event type, valores, debounce, reports, bateria e política de energia vigentes.
Nenhuma alteração comportamental do sensor de porta é autorizada.

O produto de presença requer `digital_input`, `user_button`, `wake_led`,
`digital_input_wakeup` e `battery_measurement` quando as features
correspondentes estiverem habilitadas. Combinação com board que não ofereça um
recurso requerido falha antes de produzir binário utilizável.

As escolhas default continuam sendo `Single smart plug` e
`Current client ESP32-H2 wiring`. O novo produto e o board generalizado não se
tornam defaults.

## 6. Configuração concreta do produto

### 6.1 Entrada de presença

| Parâmetro | Valor |
|---|---:|
| GPIO | 14, recebido do board |
| Polaridade | HIGH = detected |
| Pull | pull-up |
| Endpoint | 1 |
| Report inicial | habilitado |
| Período de amostragem | 10 ms |
| Amostras por janela | 5 |
| Maioria | 3 |
| Janelas consecutivas | 2 |

Os parâmetros de debounce reutilizam o precedente do sensor de porta. A
capability estabiliza e publica o estado inicial sincronamente antes de iniciar
a amostragem periódica. Depois disso, publica uma vez por transição
estabilizada. Oscilações que não satisfaçam duas janelas consecutivas com a
mesma classificação nova não alteram o estado confirmado.

### 6.2 Deep sleep

| Parâmetro | Valor/default |
|---|---:|
| Habilitado | sim |
| Janela máxima acordada | 30 segundos |
| Timer periódico | 180 minutos |
| Wake LED | GPIO 13, HIGH, 200 ms |
| Wakeup digital | GPIO 14 por EXT1 |

Antes do sleep, a fachada reaplica o GPIO 14 como entrada com pull-up, lê o
nível elétrico e arma EXT1 para o nível oposto. Esse rearme ocorre em todo boot
que alcance a sequência terminal, independentemente da causa do wakeup e de
`StartDevice` ter sido alcançado. Timer e entrada são fontes independentes e
ambas são armadas antes da operação terminal.

O estado lógico não é persistido entre boots. Como `reportOnStart=true`, cada
wakeup estabiliza e publica o nível corrente pelo único caminho normal da
capability. Não há report especial criado pela causa EXT1.

Não existe rate limit: repique ou transições sucessivas podem provocar ciclos
acordados completos consecutivos.

### 6.3 Bateria e factory reset

A bateria aplica integralmente `Client-Battery-Level.md`:

- endpoint 2 para percentual, event type 3;
- endpoint 3 para estado da telemetria, event type 4;
- `emptyMv=3300`, `fullMv=4150`, 8 amostras espaçadas por 5 ms e delta de 5%;
- com deep sleep habilitado, uma medição e publicação em todo boot operacional;
- ADC e divisor recebidos do board sem redescoberta pela fachada.

O factory reset usa o botão do board, ativo em LOW, com GPIO default 9, hold de
10 segundos e polling de 20 ms. A arbitragem vigente com deep sleep é
preservada.

## 7. API e responsabilidades

`issp_behaviors` reutiliza `DigitalInputBehavior`; ele permanece genérico e não
conhece presença, porta, produto, board ou Kconfig.

`SmartSysApp` acrescenta `addPresenceSensorCapability()` com configuração de
entrada equivalente à necessária pelo behavior, mas injeta internamente o
event type 5. A operação rejeita GPIO inválido, endpoint zero ou ocupado,
configuração inválida de debounce e chamada fora de `Configuring` pelos
resultados públicos vigentes.

A extensão de wakeup para presença é aditiva. O contrato público vigente de
`ContactWakeupConfig` e `contactWakeup`, usado pelo sensor de porta, permanece
compatível. A fachada admite configurar a entrada digital da presence
capability como fonte EXT1 sem fazer o produto mentir que ela é contato seco.
Somente uma fonte digital EXT1 pode ser selecionada por composição nesta
versão; configuração ambígua é rejeitada antes da operação normal.

O product firmware define identidade, endpoints, debounce e política de
energia. O board fornece pinagem, polaridade e fatos elétricos. Kconfig escolhe
composição e valores públicos do produto, sem alcançar componentes
compartilhados.

O coordenador conhece somente o registro semântico global do event type 5 e
sua tradução para o host; não recebe dependência de código do client.

## 8. Requisitos

- **`PRESENCE-001`:** o build seleciona exatamente um product firmware e um
  board, incluindo a nova combinação presença + `Battery Digital Sensor H2`.
- **`PRESENCE-002`:** a composição publica endpoint 1, event type 5 e valor 1
  para HIGH estabilizado, ou valor 0 para LOW estabilizado.
- **`PRESENCE-003`:** a capability publica o estado estabilizado em todo boot
  operacional e cada transição estabilizada posterior uma única vez.
- **`PRESENCE-004`:** comandos para a capability retornam `Unsupported` e não
  alteram GPIO ou estado confirmado.
- **`PRESENCE-005`:** deep sleep usa deadline de 30 segundos, timer de 180
  minutos, LED de 200 ms e GPIO 14 como fonte EXT1 alternada.
- **`PRESENCE-006`:** o preparo do wakeup reaplica entrada com pull-up e arma o
  nível oposto ao lido; falha de configuração do GPIO ou EXT1 bloqueia o sleep
  conforme o contrato vigente.
- **`PRESENCE-007`:** bateria e estado da telemetria ocupam endpoints 2 e 3 e
  preservam os contratos vigentes de publicação e degradação.
- **`PRESENCE-008`:** o board generalizado preserva GPIO 14, GPIO 13, GPIO 9,
  ADC1 canal 0 e divisor 470 kΩ/220 kΩ; o produto de porta migra sem mudança
  de comportamento.
- **`PRESENCE-009`:** o coordenador traduz event type 5 como `Presence Sensor`,
  1 como `detected` e 0 como `undetected`.
- **`PRESENCE-010`:** componentes compartilhados não contêm símbolo Kconfig,
  identidade de produto ou pinagem de board.
- **`PRESENCE-011`:** tomada, defaults, wire, commissioning, ACK, retry,
  report ID e separação entre targets permanecem inalterados.
- **`PRESENCE-012`:** nenhum artefato de teste é criado ou alterado.

## 9. Falhas e condições de borda

- GPIO 14 inelegível para EXT1 no target configurado torna a configuração
  inválida; a implementação não mantém lista paralela de pinos.
- Colisão entre entrada, LED, botão ou ADC é rejeitada pela composição ou pela
  fachada antes da operação normal.
- Falha da bateria suprime sua telemetria conforme a fonte vigente e não impede
  a função de presença.
- Falha ao preparar uma fonte de wakeup solicitada impede a entrada em deep
  sleep; as regras vigentes de diagnóstico e quiescência permanecem aplicáveis.
- HIGH ou LOW persistente não gera reports repetidos dentro do mesmo boot além
  do report inicial.
- Uma transição ocorrida depois de armar EXT1 e antes do sleep pode provocar
  wakeup imediato; isso não é classificado como perda do evento.
- Repique ou atividade contínua podem elevar consumo porque não existe rate
  limit neste recorte.
- Se o sensor deixar de dirigir o pino apesar de permanecer alimentado, o
  pull-up tende a HIGH e pode produzir `detected`; diagnóstico elétrico do
  sensor não integra esta versão.

## 10. Critérios de aceite e evidências

- **`PRESENCE-AC-001 — Seleção`:** inspeção da configuração e build H2 da nova
  composição demonstram seleção exclusiva do produto e do board, recursos
  satisfeitos e ausência de outra variante no binário.
- **`PRESENCE-AC-002 — Identidade`:** inspeção confronta endpoint 1, event type
  5, `deviceId=0x15400002` e injeção do tipo pela fachada com a ADR-0005.
- **`PRESENCE-AC-003 — Entrada`:** inspeção confronta GPIO 14, HIGH ativo,
  pull-up e os parâmetros de debounce; evidência futura em hardware distingue
  report inicial, `detected`, `undetected` e ausência de repetição estável.
- **`PRESENCE-AC-004 — Wakeup`:** inspeção confronta o rearme EXT1 alternado e
  os valores de 30 segundos, 180 minutos e 200 ms; somente hardware futuro pode
  demonstrar wakeup nos dois sentidos e ausência de wakeup espúrio.
- **`PRESENCE-AC-005 — Bateria`:** inspeção e build confrontam endpoints,
  parâmetros e recursos; funcionamento e percentual real dependem de hardware.
- **`PRESENCE-AC-006 — Coordenador`:** build C6 e inspeção confrontam o registro
  do tipo 5 e sua tradução; somente execução futura demonstra a linha entregue
  ao host.
- **`PRESENCE-AC-007 — Preservação`:** builds canônicos das composições H2 de
  tomada, porta e presença, além do C6, terminam com sucesso; inspeção demonstra
  ausência de alteração wire ou dependência de código entre targets.
- **`PRESENCE-AC-008 — Testes fora do recorte`:** o delta não cria nem altera
  arquivos sob `test`, `tests` ou `test_apps`, nem infraestrutura equivalente.
  Nenhuma execução de teste é apresentada como evidência.

Build comprova construção, não execução. Flash, monitor e hardware exigem
autorização própria. Até essa autorização, os comportamentos físicos permanecem
`Not Executed` e não podem ser apresentados como validados.

## 11. Decisões confirmadas pelo Arquiteto

Em 19/08/2026, o Arquiteto confirmou:

- ESP32-H2 e GPIO 14;
- PIR/radar continuamente alimentado durante deep sleep;
- HIGH ativo e pull-up;
- event type 5, com `1 = detected` e `0 = undetected`;
- reutilização da eletrônica existente e generalização do board para
  `Battery Digital Sensor H2`;
- deep sleep com 30 segundos acordado, timer de 180 minutos e LED por 200 ms;
- `deviceId=0x15400002`;
- proibição explícita de criar ou alterar testes neste recorte.

Não permanece decisão normativa aberta nesta versão. A escrita desta v0.1 e
sua submissão à Análise de Implementabilidade foram autorizadas pelo Arquiteto
em 19/08/2026.
