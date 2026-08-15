# Análise de implementabilidade da v0.4 — `EKOM-BATTERY-001`

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/Client-Battery-Level.md`

**Revisão confrontada:** v0.4, `Draft`, na branch `spec/client-battery-level`,
`HEAD` `df65449`; o texto normativo da v0.4 foi introduzido em `32288af`

**Estado:** Concluído

**Data:** 14/08/2026

**Modelo EKOM:** 4.4 (`REGRAS-COMUNS.md` 3.2, `ENGENHEIRO-ANALISTA.md` 3.1)

**Relação com relatórios anteriores:** primeira análise da v0.4. As análises da
v0.1 e da v0.3 concluíram **Não pronta — defeito da especificação** e pertencem
àquelas revisões; nenhuma delas alcança esta versão, e nenhuma versão desta
especificação recebeu `Ready` até aqui. Por isso este confronto é integral, e
não um delta

**Resultado:** a funcionalidade cabe na baseline arquitetural e no recorte
autorizado, e a maior parte do contrato é implementável como está. Restam,
porém, quatro bloqueadores: duas autoridades vigentes cujos contratos a
capability altera estão declaradas como preservadas; um critério de aceite que
não é satisfazível sem a retificação postergada em `EKOM-DEBT-0001`; e um
requisito de modo degradado cuja fonte de valor não existe no alvo. Todos são
correções da própria funcionalidade e de seus donos naturais

> Este relatório registra evidências e recomendações. Não altera fonte
> normativa, não promove estado, não aceita débito e não autoriza implementação
> ou testes.

## 0. Método e condições de entrada

Li integralmente `AGENTS.md`, `roles/REGRAS-COMUNS.md`, o perfil do Engenheiro
Analista e a especificação indicada. As diretrizes canônicas foram lidas na raiz
declarada pelo `AGENTS.md`, `/Users/marcelocostamiranda/source/EKM-guidelines`;
não existe `.ekom-guidelines/` nesta árvore local, e o conteúdo lido é o do
EKOM 4.4.

Árvore limpa, branch `spec/client-battery-level` derivada da `main`. Nenhum
build, teste, flash, monitor ou hardware foi executado. Os fatos de plataforma
foram verificados na fonte do ESP-IDF 6.0.1 instalado em
`~/.espressif/v6.0.1/esp-idf`, e não herdados de memória ou de análise anterior.

Não reconstruí o contrato a partir do workflow: a especificação versionada é a
fonte da demanda.

## 1. Fontes normativas e elementos afetados

| Fonte | Elemento que a capability toca |
|---|---|
| `docs/specs/Client-Battery-Level.md` v0.4 | contrato integral em confronto |
| `docs/adr/ADR-0005-CAPABILITY-IDENTITY.md` `Accepted` | endpoint congelado, tipo como natureza, unicidade por endpoint, tipo 3 registrado |
| `docs/adr/ADR-0001-ISSP-COMPONENT-BOUNDARIES.md` `Accepted` | tipos de driver do ESP-IDF na fachada, pela nota de reavaliação de 14/08/2026 |
| `docs/adr/ADR-0002-PRODUCT-BOARD-COMPOSITION.md` `Accepted` | nova classe de recurso físico oferecida pelo board |
| `docs/specs/ISSP-Configurable-Bootstrap.md` | **API pública de `SmartSysApp`**: operações de composição, structs de configuração, ciclo `Configuring`/`setup()`, rejeição de duplicidade |
| `docs/specs/ISSP-Reusable-Components.md` | **API pública de `issp_behaviors`** e dependências públicas e privadas de cada componente |
| `docs/specs/Firmware-Variants-Menuconfig.md` | composição enumerada do `Door sensor battery H2`, validação de recursos no CMake, regra de unicidade do par |
| `docs/specs/Client-Deep-Sleep.md` v0.11 | boot operacional, janela acordada, admissão antecipada, drenagem de pendentes |
| `docs/specs/ISSP-Report-Identity.md` | identidade de report gerada pelo client |
| `docs/rfc/KNOWLEDGE-MAP.md` | `EKOM-DEBT-0001` a `EKOM-DEBT-0004`, `EKM-GAP-0002`, `EKM-GAP-0007` |

## 2. Relação da mudança com cada autoridade vigente

### 2.1 Relações declaradas e confirmadas

- **`ADR-0005` — confirmada.** O tipo 3 está registrado na ADR e existe no
  código do coordenador; o endpoint 2 está livre no `door_sensor_battery_h2`,
  cujo sensor de porta ocupa o endpoint 1
  (`client_154/main/firmwares/door_sensor_battery_h2.cpp:11`). A ausência de
  `eventType` na configuração do produto é coerente com a decisão.
- **`ADR-0001` — confirmada.** A nota de reavaliação de 14/08/2026 autoriza
  expressamente tipos de periférico do ESP-IDF na fachada, pelo precedente de
  `gpio_num_t` em `SwitchConfig` e `PushButtonConfig`
  (`components/issp_app_154/include/SmartSysApp.h:6,18,49`).
- **`ADR-0002` — confirmada.** O mecanismo produto/board é exercido, não
  desviado: o board declara fatos elétricos, o produto declara política.
- **`Firmware-Variants-Menuconfig.md` — `Amends` correta quanto à composição.**
  A seção 8 da especificação materializa a emenda, no mesmo padrão com que
  `Client-Deep-Sleep.md` acrescentou `wake_led` e `dry_contact_wakeup`. O item
  de fora de escopo daquela fonte — "restaurar bateria, ADC, deep sleep,
  wake-up por GPIO ou métricas do antigo `sensor_154`"
  (`docs/specs/Firmware-Variants-Menuconfig.md:168`) — não é obstáculo: ele
  delimita aquela entrega, e o precedente de `Client-Deep-Sleep.md`, que emendou
  a mesma fonte para deep sleep e wake-up por GPIO, já fixou como esse limite é
  ultrapassado, isto é, por emenda declarada.
- **`Client-Deep-Sleep.md` — dependência de comportamento, sem emenda,
  correta.** `BATTERY-017` mantém o report de bateria fora da evidência de
  admissão antecipada, e o código correspondente conta apenas capabilities de
  switch e de sensor com `reportOnStart`
  (`components/issp_app_154/src/smart_sys_app_deep_sleep.cpp:487-528`). Nada em
  `readyForEarlyQuiescence()` precisa mudar.
- **`ISSP-Report-Identity.md` — preservação correta.** A capability publica pelo
  caminho existente `publishState()`, que gera identidade por admissão
  (`components/issp_core/src/issp_device.cpp:87-192`). Nenhum campo, tamanho,
  sequência ou checksum é tocado.

### 2.2 Relações omitidas — bloqueadores 1 e 2

**`ISSP-Configurable-Bootstrap.md` está declarada como preservada, e não é.**
A especificação justifica a preservação por wire: "nenhuma mudança de wire, de
versão de protocolo, de checksum, de sequência ou de identidade de report". Mas
aquela fonte não governa wire: ela é a "Especificação da API pública
`SmartSysApp`" (linha 1), enumera as operações de composição em `SMARTAPP-003`,
fixa a forma das structs de configuração e determina, na linha 390, que "pares
duplicados de `endpointId` e `eventType` devem ser rejeitados".

A capability exige, inevitavelmente, uma operação pública nova da fachada e uma
struct de configuração pública nova — é assim que o product firmware a registra,
e é o que a própria seção 5.2 descreve. Isso é emenda àquela fonte. O precedente
é explícito e recente: `Client-Deep-Sleep.md` declara "**Altera (`Amends`)
`ISSP-Configurable-Bootstrap.md` v1.5:** acrescenta `configureDeepSleep()` à API
pública" para uma adição de porte equivalente.

A `ADR-0005` emenda aquela fonte apenas quanto à semântica de `endpointId` e
`eventType`, e diz textualmente "Nenhuma outra fonte é alterada". Ela não cobre,
e não pretende cobrir, o acréscimo de operação pública.

**`ISSP-Reusable-Components.md` está declarada como preservada, e também não
é.** A seção 5 daquela fonte enumera a API pública de `issp_behaviors` e as
dependências públicas e privadas de cada componente. A capability acrescenta um
behavior reutilizável com configuração própria e, sobretudo, torna `esp_adc` uma
dependência **pública** do componente que expõe os tipos de ADC no header: hoje
`issp_app_154` declara publicamente apenas `esp_driver_gpio`
(`components/issp_app_154/CMakeLists.txt`), e `issp_behaviors` declara
`issp_core`, `esp_driver_gpio` e `esp_timer`.

A nota da `ADR-0001` autoriza o **padrão** — tipo de driver na fachada, com
dependência pública declarada. Ela não classifica a relação desta especificação
com a fonte que enumera essas dependências. O precedente novamente existe:
`Client-Deep-Sleep.md` declara "**Altera (`Amends`) `ISSP-Reusable-Components.md`
v1.1**" ao ampliar APIs públicas de `issp_core`, `issp_behaviors` e
`issp_transport_154`.

Ambos os casos caem exatamente sob `REGRAS-COMUNS` 3: "Uma especificação nova
não prevalece silenciosamente sobre outra fonte normativa vigente. Relação
indefinida ou conflito entre autoridades retorna ao Arquiteto antes da
prontidão."

### 2.3 Conflito remanescente sobre unicidade — bloqueador 3

`ADR-0005` decidiu unicidade **por endpoint**. Duas fontes vigentes ainda dizem
o contrário, e a ADR declara não alterar nenhuma delas além do bootstrap:

- `ISSP-Configurable-Bootstrap.md:390` — rejeição de pares duplicados (esta a
  ADR emenda);
- `Firmware-Variants-Menuconfig.md:130` — "par endpoint/evento único entre todas
  as capabilities", como regra de ciclo de vida da fachada (esta a ADR **não**
  emenda, e a seção 8 da especificação de bateria emenda daquela fonte apenas a
  composição do produto).

O código materializa a regra do par
(`components/issp_app_154/src/smart_sys_app.cpp:147-159`), e a correção geral é
o que `EKOM-DEBT-0001` postergou.

O efeito prático está detalhado em 5.3: `BATTERY-AC-007` exige a rejeição
bidirecional, e apenas uma direção é alcançável dentro do recorte autorizado.

## 3. Componentes e arquivos potencialmente impactados

| Arquivo | Mudança esperada | Natureza |
|---|---|---|
| `components/issp_app_154/include/SmartSysApp.h` | struct de configuração pública da capability, tipo de capability opaca e a operação de registro; inclusão de header do `esp_adc` | emenda de API pública (bloqueador 1) |
| `components/issp_app_154/CMakeLists.txt` | `esp_adc` em `REQUIRES`, junto de `esp_driver_gpio` | dependência pública (bloqueador 2) |
| `components/issp_app_154/src/smart_sys_app_impl.hpp` | arrays de configuração, behavior e capability da bateria; ver a restrição de armazenamento em 5.5 | implementação |
| `components/issp_app_154/src/smart_sys_app.cpp` | registro, invariantes de `BATTERY-011`, unicidade aplicável, coerência de `samplePeriodMs` em `setup()` | implementação, condicionada à decisão de 5.3 |
| `components/issp_behaviors/*` | behavior de medição com temporizador próprio, conversão e classificação de falha | emenda de API pública (bloqueador 2) |
| `components/issp_behaviors/CMakeLists.txt` | `esp_adc` | dependência |
| `client_154/main/boards/board_model.hpp` | recurso de medição de bateria: unidade, canal, atenuação, `Rtop`, `Rbottom` e — ver 5.4 — o fundo de escala | composição |
| `client_154/main/boards/door_sensor_battery_h2.cpp` | valores da seção 8 | composição |
| `client_154/main/firmwares/door_sensor_battery_h2.cpp` | registro da capability com os parâmetros da seção 8 | composição |
| `client_154/main/CMakeLists.txt` | recurso exigido pelo produto e oferecido pelo board | composição; nome do recurso não fixado, ver 5.6 |

**Não impactados, com evidência:**

- **coordenador** — `IOT154_EVENT_BATTERY_LEVEL_PERCENT 3` já existe
  (`coordinator_154/main/iot154_packet.h:29`), `type_from_event()` já rotula
  "Battery Level (%)" e `value_from_event()` já cai no render numérico para esse
  tipo (`coordinator_154/main/main.c:437-468`). Nenhuma mudança é necessária, e
  a fronteira client/coordenador é preservada;
- **`issp_core`** — `publishState()`, a coalescência por par pendente e a
  drenagem servem à capability sem alteração;
- **artefatos de teste** — a especificação declara explicitamente que nenhum
  integra o recorte, o que é coerente com `REGRAS-COMUNS` 5. Nenhum teste
  existente deixa de compilar por esta mudança, porque nada de assinatura
  vigente é alterado.

## 4. Comportamento existente relevante

1. **`IsspDevice::start()` aborta no primeiro `begin()` que falha**
   (`components/issp_core/src/issp_device.cpp:43-67`). O desvio 2 da seção 3.1 e
   `BATTERY-016` são implementáveis sem tocar `issp_core`: basta o `begin()` da
   bateria registrar a falha em log e retornar `Ok`, permanecendo inerte. Ver a
   precisão de 5.2.
2. **`readyForEarlyQuiescence()`** exige `Running` e ao menos um report inicial
   esperado, e conta apenas switch e sensor de porta
   (`smart_sys_app_deep_sleep.cpp:487-528`). A bateria não entra na contagem, o
   que é exatamente `BATTERY-017`.
3. **A sequência terminal drena pendentes antes do sleep** no caminho não
   forçado (`smart_sys_app_deep_sleep.cpp:761-779`). É o que faz o report de
   bateria chegar ao coordenador sem prolongar a janela acordada — desde que já
   esteja admitido quando a sequência começa. Ver 5.1.
4. **`DigitalInputBehavior` é o precedente de forma**: temporizador `esp_timer`
   próprio, `accepts()` pelo par e `handle()` devolvendo `Unsupported`
   (`components/issp_behaviors/src/digital_input_behavior.cpp:374-384`), que é o
   caminho que `BATTERY-006` exige.
5. **A validação em `setup()` já tem precedente**: `validateContactWakeup()`
   roda em `SetupStage::ValidateConfiguration` justamente para preservar a
   irrelevância da ordem de configuração — o mesmo estágio que `BATTERY-012`
   indica.
6. **O projeto ESP-IDF da raiz** contém a referência histórica da medição
   (`main/hello_world_main_battery.c`): ADC1, canal 0, GPIO1, atenuação 12 dB,
   `R_TOP` 470 kΩ, `R_BOTTOM` 220 kΩ e curve fitting com fallback. Confirma os
   fatos da seção 8; permanece não normativo sob `EKM-GAP-0007`, e a
   especificação o trata assim corretamente.

## 5. Restrições, incertezas e decisões necessárias

### 5.1 Momento da medição em composição com deep sleep — restrição, não defeito

`BATTERY-007` exige publicação em todo boot operacional e `BATTERY-017` proíbe
prolongar a janela acordada por causa dela. Os dois só coexistem se a medição
estiver **admitida antes de `Running`**, isto é, dentro do `begin()` do behavior:
a partir de `Running`, o ciclo pode iniciar a sequência terminal a qualquer
polling de 10 ms, e um report ainda não admitido não é esperado por ninguém. A
especificação deixa a estrutura interna à implementação, o que é correto, mas a
restrição é material e deve ser registrada — não é um grau de liberdade real.

Não é bloqueador: existe caminho implementável, e é o mesmo do report inicial do
sensor de porta.

### 5.2 `BATTERY-016` e a premissa de `readyForEarlyQuiescence()`

O comentário vigente em `smart_sys_app_deep_sleep.cpp:489-492` afirma que
alcançar `Running` prova que todo `begin()` teve sucesso. Com a bateria
retornando `Ok` enquanto inerte, a afirmação deixa de valer para toda capability
— embora continue valendo para as que a função conta. É precisão de comentário e
de implementação, não defeito normativo, e deve constar do relatório de
implementação.

### 5.3 `BATTERY-AC-007` não é satisfazível no recorte autorizado — **bloqueador 3**

O critério exige: "**Quando** outra capability tentar registrar o mesmo
endpoint, ainda que com tipo de evento distinto (…) **Então** o registro é
rejeitado".

Dentro do recorte, apenas uma direção é alcançável:

- bateria registrada sobre endpoint já ocupado — verificável na própria operação
  nova, sem tocar nada existente;
- sensor de porta ou plug registrado sobre o endpoint da bateria com tipo
  distinto — **aceito**, porque essas operações validam pelo par
  (`smart_sys_app.cpp:174,231`), e mudá-las é a retificação que
  `EKOM-DEBT-0001` postergou.

A especificação não diz qual direção o critério cobre, e o `EKOM-DEBT-0001` não
supre a lacuna: aceitar débito não torna conforme a condição, e a seção 5.4 da
própria especificação reconhece que as capabilities existentes não seguem o
modelo. Um critério de aceite que não distingue sucesso de falha no recorte
autorizado é defeito de especificação (`REGRAS-COMUNS` 5).

Decisão necessária do Arquiteto, entre:

1. restringir explicitamente `BATTERY-011`/`AC-007` à direção alcançável,
   registrando a assimetria como consequência declarada do `EKOM-DEBT-0001`;
2. incluir no recorte a unicidade por endpoint da fachada inteira, o que exige
   emendar também `Firmware-Variants-Menuconfig.md:130` e converte parte do
   `EKOM-DEBT-0001` em remediação autorizada;
3. adiar `AC-007` inteiro para a remediação do débito.

A recomendação do Analista é a alternativa 1: é a única que preserva o recorte,
e a assimetria fica explícita em vez de silenciosa.

### 5.4 `BATTERY-015` não tem fonte para o fundo de escala — **bloqueador 4**

A especificação determina que, sem calibração, a conversão use "a tensão de
fundo de escala correspondente à atenuação declarada pelo board, obtida da fonte
do alvo", e proíbe literal na especificação.

Verifiquei na fonte do ESP-IDF 6.0.1: **não existe** constante pública que
relacione atenuação a fundo de escala. `soc/esp32h2/include/soc/soc_caps.h`
declara `SOC_ADC_ATTEN_NUM (4)` e `SOC_ADC_RTC_MAX_BITWIDTH (12)`, e nenhum
header público de `esp_adc` expõe tabela de milivolts por atenuação. A única
constante desse tipo em toda a árvore pertence ao esquema *line fitting* do
ESP32 clássico, que não se aplica ao H2. A referência histórica da raiz usa o
literal `(raw * 3300) / 4095`.

Logo, "a fonte do alvo" não fornece o valor, e o Implementador só poderia
introduzir um número de datasheet por conta própria — precisamente o que a
seção 5.1 recusa como método. Decisão normativa ausente.

Recomendação: que o fundo de escala seja mais um **fato elétrico do board**,
declarado ao lado da atenuação em 5.3 e com valor em 8. É a camada correta, é
coerente com o restante do contrato e não cria faixa normativa alguma.

### 5.5 Armazenamento opaco da fachada — restrição verificável por build

`SmartSysApp::kImplStorageBytes` vale 16384 e é verificado por `static_assert`
(`SmartSysApp.h:277`, `smart_sys_app.cpp:425`). O `Impl` já consome 8192 bytes
de `hardwareStorage_`, 4096 de pilha estática do lifecycle e três arrays de oito
posições por tipo de capability. Acrescentar um terceiro tipo adiciona,
estimados, entre 1 e 1,5 KB. A margem existe, mas é estreita; a verificação é do
build, não da leitura. Se o limite for excedido, o ajuste da constante é local e
já previsto pelo próprio comentário do header. Registrado como experimento
necessário, não como bloqueador.

### 5.6 Nome do recurso de composição — precisão não bloqueante

A seção 5.3 exige que o CMake passe a demandar "o recurso de medição de
bateria", mas nenhuma seção fixa o identificador usado nas listas
`required_resources`/`offered_resources`, ao contrário de `wake_led` e
`dry_contact_wakeup`, nomeados pelas fontes que os introduziram. O diagnóstico
de composição incompatível imprime esse nome. Sugestão: `battery_voltage_divider`.
Não bloqueia: nenhum comportamento observável depende do nome.

### 5.7 Bloqueio do task de `esp_timer` durante a medição — restrição

Behaviors não possuem tarefa nem pilha próprias (`Firmware-Variants-Menuconfig.md`,
tabela de pontos afetados). Em composição sem deep sleep, a amostragem periódica
roda no task do `esp_timer`, e `samples × sampleIntervalMs` bloqueia esse task —
40 ms na composição da seção 8, se ela vier a operar sem deep sleep. É
consequência aceitável e limitada, mas pertence ao relatório de implementação,
com `skip_unhandled_events` já em uso no precedente.

### 5.8 Incertezas que não são decisão normativa

- **Impedância da fonte.** O divisor 470k/220k apresenta impedância equivalente
  de aproximadamente 150 kΩ ao pino, muito acima do que a amostragem SAR do
  ESP32-H2 assume. O efeito é erro de leitura, não falha. Os valores são fatos
  do board declarados pelo Arquiteto e a seção 9 já aceita o risco de drenagem;
  o risco de **exatidão** só se resolve com medição em placa.
- **Faixa útil.** Com bateria em 4150 mV, o pino vê cerca de 1323 mV, e em
  3300 mV cerca de 1052 mV. Ambos ficam confortavelmente dentro da faixa de
  12 dB, e a saturação superior da classe 2 não é alcançada em operação normal —
  o que confirma a leitura da especificação de que ela indica defeito.
- **Mapeamento de canal.** `ADC1_CHANNEL_0_GPIO_NUM 1` em
  `soc/esp32h2/include/soc/adc_channel.h` confirma o fato declarado na seção 8.
  Nenhuma colisão com GPIO 14, 9 ou 13 do board.

## 6. Experimentos e evidências necessários

| # | Experimento | Por que leitura não basta | Autorização |
|---|---|---|---|
| 1 | Build canônico ESP32-H2 do `client_154` | `static_assert` de `kImplStorageBytes` e resolução das dependências `esp_adc` só se provam compilando | intrínseca à implementação autorizada (`REGRAS-COMUNS` 2.2) |
| 2 | Build da composição incompatível produto/board | o diagnóstico do recurso ausente é produzido pelo CMake | idem |
| 3 | Medição com fonte variável em `emptyMv`, `fullMv`, ponto médio, meio ponto percentual e fora da faixa | conversão, arredondamento e saturação sobre hardware real | hardware, autorização própria |
| 4 | Divisor desconectado | classe 2, extremo inferior | hardware, autorização própria |
| 5 | Ciclo de deep sleep com wakeup por timer e por contato | `BATTERY-007`, `AC-003` e `AC-010` | hardware, autorização própria |
| 6 | Exatidão sob impedância de 150 kΩ | erro de amostragem SAR não é derivável por leitura | hardware, autorização própria |
| 7 | Alvo sem calibração disponível (`AC-008`) | na H2 o curve fitting depende da versão de calibração em eFuse; **não é possível escolher** um alvo sem ela | hardware; ver a ressalva abaixo |

**Ressalva sobre o experimento 7.** `esp_adc/esp32h2/curve_fitting_coefficients.c`
obtém os coeficientes por `esp_efuse_rtc_calib_get_ver()`, e o esquema falha com
`ESP_ERR_NOT_SUPPORTED` apenas em peça cuja versão de calibração esteja fora da
faixa suportada. O modo degradado é alcançável em teoria e não é produzível sob
demanda. `BATTERY-AC-008` provavelmente permanecerá `Not Executed` por ausência
de meio, e não por ausência de permissão. Não é bloqueador — a evidência de
hardware já está reservada a etapa posterior —, mas o Arquiteto deve saber que
esse critério pode nunca receber evidência física, e que injeção de falha por
double exigiria artefato de teste, hoje explicitamente fora do recorte.

## 7. Classificação de implementabilidade

### **Não pronta — defeito da especificação** [`Not Ready — Specification Defect`]

**Bloqueadores:**

1. **Relação com `ISSP-Configurable-Bootstrap.md` declarada como preservação.**
   A capability acrescenta operação e configuração públicas à fachada, que é o
   contrato governado por aquela fonte. Relação correta: `Amends`, pelo
   precedente de `Client-Deep-Sleep.md`.
2. **Relação com `ISSP-Reusable-Components.md` declarada como preservação.** A
   API pública de `issp_behaviors` e o conjunto de dependências públicas dos
   componentes mudam, com `esp_adc` passando a público. Relação correta:
   `Amends`. A nota da `ADR-0001` autoriza o padrão, mas não classifica a
   relação.
3. **`BATTERY-AC-007` não distingue sucesso de falha no recorte autorizado.** A
   unicidade por endpoint só é alcançável em uma direção enquanto
   `EKOM-DEBT-0001` estiver aberto, e `Firmware-Variants-Menuconfig.md:130`
   permanece afirmando a regra do par sem emenda declarada.
4. **`BATTERY-015` depende de um valor que o alvo não fornece.** Não existe
   fonte pública de fundo de escala por atenuação no ESP-IDF 6.0.1 para o
   ESP32-H2; o modo degradado fica sem fonte governante.

**Por que não é pré-requisito arquitetural.** Apliquei o teste de fronteira do
perfil: todas as capacidades necessárias existem na baseline — registro de
capability, behavior com temporizador próprio, publicação por `publishState()`,
drenagem antes do sleep, validação de recursos no CMake. Nenhum lifecycle,
ownership, protocolo, persistência ou consumidor fora do recorte é criado. As
quatro correções pertencem à própria funcionalidade e a seus donos naturais:
três são declarações de relação e um critério, e a quarta é uma decisão de
camada entre board e capability. `Not Ready — Specification Defect` é a classe
correta; não é o segundo retorno com bloqueador arquitetural, e não recomendo
análise arquitetural abrangente.

**O que não bloqueia.** Fórmula, invariantes derivados, classificação de falhas,
gatilhos, identidade, ausência de mudança no coordenador, exclusão do report da
evidência de admissão de sleep e a normatividade restrita da seção 8 estão
implementáveis como escritos. A v0.4 resolveu materialmente os três bloqueadores
devolvidos à v0.3.

**Próxima ação recomendada ao Arquiteto:** decidir 5.3 e 5.4, corrigir as duas
relações normativas de 2.2 em uma v0.5 e submetê-la a nova análise. Nenhuma
ordem de implementação é cabível até que exista `Ready` da versão corrente.
