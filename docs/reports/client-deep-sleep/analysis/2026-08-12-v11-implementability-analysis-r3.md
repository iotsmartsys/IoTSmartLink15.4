# Nova análise de implementabilidade da v0.11 — texto corrigido pelo Arquiteto

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/Client-Deep-Sleep.md`

**Revisão confrontada:** v0.11, Draft de 12/08/2026, conforme o commit `b10a3a2`

**Estado:** Concluído

**Capacidade:** Engenheiro Analista

**Data:** 12/08/2026

**Relação com relatórios anteriores:** confirma e encerra
`2026-08-12-v11-implementability-analysis.md`, cujos achados o Arquiteto
incorporou. Substitui um relatório intermediário `-r2`, retirado por ter sido
escrito contra o texto superado; a seção 1 registra o erro de processo que o
produziu

**Resultado:** as quatro correções do Arquiteto estão bem formadas e são
implementáveis com o que a fachada já possui; nenhum dos defeitos anteriores
sobrevive. Resta **um** defeito novo, criado pela própria correção: a
especificação passa a apoiar-se em que a capability configure direção e pull do
pad, mas existem caminhos de boot em que ela é registrada e **nunca iniciada**, e
neles a fachada armaria EXT1 sobre um pad não configurado; nenhuma execução
realizada

> Este relatório registra evidências e recomendações. Não altera a fonte
> normativa, não promove estado e não autoriza implementação ou testes.

## 1. Erro de processo desta atuação, registrado antes das conclusões

Ao receber a ordem de refazer a análise, verifiquei mudanças com `git status` e
`git diff`, que comparam a árvore de trabalho com o `HEAD`. Ambos vieram limpos e
concluí que nada mudara. A conclusão era falsa: o `HEAD` havia avançado. O
Arquiteto commitara `b10a3a2`, "docs: corrige contrato de wakeup da v0.11", que
reescreve 138 linhas da especificação incorporando os achados do primeiro
relatório. A comparação que eu deveria ter feito era contra o commit da análise
anterior, não contra o `HEAD`.

A consequência foi um relatório intermediário que reanalisou o texto superado e
declarou, em seu próprio resumo, que a ordem de refazer viera "sem insumo novo".
Ele foi retirado da árvore por ser ativamente enganoso — descreve como
bloqueador vigente uma cláusula que já não existe — e permanece recuperável no
histórico do Git, em `cfed030`. O único achado seu que sobrevive é interno e sem
efeito prático: naquela leitura eu havia rebaixado corretamente o segundo
bloqueador do primeiro relatório, que o Arquiteto já havia corrigido de qualquer
modo.

Registro isto porque relatório de análise é evidência, e evidência produzida sob
premissa errada precisa ser identificada como tal por quem a produziu. Esta
análise, a partir daqui, é feita contra a v0.11 de 12/08/2026.

## 2. As quatro correções do Arquiteto, confrontadas uma a uma

### 2.1 Elegibilidade derivada do target — resolvida e implementável

A seção 4.2A passou a determinar que "a fachada usa
`esp_sleep_is_valid_wakeup_gpio()`, não mantém lista própria de GPIOs e rejeita
com `AppResult::InvalidArgument` todo GPIO que o target não aceite", e
DEEPSLEEP-DEC-017 consolida a regra. A exceção do GPIO 7 saiu, e com ela a
contradição entre a lista e a derivação.

Confirmo que a escolha é exata e que a chamada é segura no ponto em que a
especificação a coloca:

- é a mesma função que o próprio `esp_sleep_enable_ext1_wakeup_io()` usa para
  validar cada pino (`sleep_modes.c:1946`), de modo que a validação antecipada e
  a defesa em profundidade do preparo passam a usar um único predicado, sem
  divergência possível entre eles;
- no ESP32-H2 ela resolve para `rtc_io_num_map[gpio] >= 0`
  (`sleep_modes.c:1874-1883`; `esp_driver_gpio/src/rtc_io.c:32-39`), isto é,
  GPIO 7 a 14 (`esp_hal_gpio/esp32h2/rtc_io_periph.c:9-38`);
- **é pura**: uma indexação em tabela `const`. Não toca RTC, não exige
  inicialização de driver e pode ser chamada em `AppState::Configuring`, antes
  de qualquer estágio de `setup()` — que é exatamente onde AC-011 a exige. Isso
  importa porque `configureDeepSleep()` corre antes de `beginPlatformPowerPolicy()`
  e não pode depender de hardware inicializado;
- o grafo de build já a alcança: `esp_hw_support` está em `PRIV_REQUIRES` e
  `esp_driver_gpio` em `REQUIRES` (`components/issp_app_154/CMakeLists.txt`).

O deslocamento da restrição elétrica para o board ("eventual restrição elétrica
ou de roteamento de um pino suportado pelo target pertence ao board") é
coerente com a ADR-0002 e com o mecanismo que a seção 5 já previa. Nada a
observar.

### 2.2 Momento da armação — resolvido

Seção 1, seção 4.2A, DEEPSLEEP-DEC-011 e DEEPSLEEP-DEC-014 passaram a dizer, nas
quatro ocorrências, "no início da sequência terminal, antes de qualquer operação
terminal", coincidindo com a ordem obrigatória da seção 6.1 e com AC-011. A
expressão "imediatamente antes de dormir" desapareceu do contrato.

O Arquiteto ainda acrescentou ao texto a consequência que eu havia registrado
como nota: "se o contato transitar depois da armação e antes do sleep, EXT1
provoca wakeup imediato quando o chip dormir. A transição não é perdida, mas
pode custar um ciclo acordado completo". Confirmo que é o comportamento
correto do EXT1, que é sensível a nível e não a borda, e que a redação agora
descreve o sistema real. Nada a decidir.

### 2.3 `activeHigh` retirado de `ContactWakeupConfig` — resolvido

O campo saiu da seção 3, e DEC-013 explicita que "`ContactWakeupConfig` não
contém polaridade lógica sem efeito no rearme". O contrato deixa de expor campo
público sem comportamento. `ContactWakeupConfig` fica com `enabled` e `pin`,
ambos consumidos.

Confirmo que a compatibilidade afirmada pela seção 3 continua valendo: o campo
`contactWakeup` entra ao final de `DeepSleepConfig` (`SmartSysApp.h:83-89`), e os
15 casos existentes o deixam value-inicializado, porque constroem a configuração
por uma fábrica única com designated initializers
(`test_apps/smart_sys_app_test/main/test_smart_sys_app.cpp:160-172`). Nenhum caso
escrito precisa mudar para continuar significando o que significa.

### 2.4 Vínculo com a capability e a cadeia de retenção — resolvidos no plano em que foram formulados

A borda que eu havia levantado — contato em GPIO sem capability que configurasse
o pad — foi fechada por três cláusulas novas: o GPIO "deve ser o mesmo de uma
capability de contato seco registrada na composição"; "essa capability configura
direção e pull do pad"; e "fonte de contato independente, sem capability
correspondente, fica fora desta versão". A validação "é feita até
`ValidateConfiguration`, sem impor ordem entre o registro da capability e
`configureDeepSleep()`".

Confirmo que isso é implementável e que o estágio escolhido é o certo:

- a fachada já guarda `doorSensorConfigs_` e `doorSensorCount_`
  (`smart_sys_app_impl.hpp:158-163`), então a correspondência é uma varredura por
  `pin` sobre dados que ela possui, sem ampliar API reutilizável e sem consultar
  o behavior;
- o estágio `ValidateConfiguration` hoje apenas repete o resultado de
  configuração e declara não tocar "NVS, radio, RTC or GPIO"
  (`smart_sys_app.cpp:324-332`). Acrescentar ali um cruzamento entre duas
  configurações já copiadas preserva essa propriedade, e preserva também a regra
  da seção 6 de que falha nesse estágio não acende LED, não cria task de
  lifecycle e não tenta deep sleep;
- e resolve a independência de ordem, porque em `ValidateConfiguration` todas as
  chamadas de `AppState::Configuring` já ocorreram.

Nota de implementação, não defeito: o resultado dessa rejeição chega ao produto
por `SetupResult`, não por `lastConfigurationResult()`, que carrega os resultados
das chamadas de configuração. É consistente com AC-011 pedir rejeição "em
`ValidateConfiguration`", e nenhum valor novo de `AppResult` é necessário —
`InvalidArgument` serve.

Sobre a cadeia de retenção, o parágrafo final da seção 4.2A foi reescrito para o
que a leitura sustenta, e verifiquei a redação nova inteira contra o IDF:

- "mantém por HOLD durante o deep sleep a configuração digital do pad": o ramo
  do H2 em `ext1_wakeup_prepare()` rotea o pad para função digital, habilita a
  entrada e chama `rtcio_hal_hold_enable()` (`sleep_modes.c:2064-2071`), que é um
  bit em `LP_AON.gpio_hold0`
  (`esp_hal_gpio/esp32h2/include/hal/rtc_io_ll.h:83-86`);
- "libera esse HOLD automaticamente no boot seguinte antes de `app_main()`":
  `cpu_start.c:845-849` chama `esp_deep_sleep_wakeup_io_reset()` quando o reset é
  `RESET_REASON_CORE_DEEP_SLEEP`, e essa função libera o hold de cada pino EXT1
  (`sleep_gpio.c:231-245`);
- "não depende de opção de Kconfig neste caminho": correto, o ramo do H2 é
  incondicional; `CONFIG_ESP_SLEEP_GPIO_ENABLE_INTERNAL_RESISTORS` pertence ao
  caminho de alvos sem RTCIO (`sleep_modes.c:2127-2136`), e o H2 não declara
  `SOC_PM_SUPPORT_RTC_PERIPH_PD`, o que também elimina o ramo condicional
  vizinho;
- e a fronteira física permanece corretamente colocada: a suficiência elétrica
  do pull diante de fuga, impedância e ruído continua exigindo DEEPSLEEP-AC-012
  em hardware.

O texto normativo agora descreve o mecanismo real. Não tenho reparo.

### 2.5 Diagnósticos incorporados

A seção 6 e AC-009 passaram a exigir `ESP_SLEEP_WAKEUP_EXT1` como causa distinta
e a restringir `no_wakeup_source` ao caso em que "timer e contato estiverem ambos
desabilitados". Ambos correspondem ao IDF (`sleep_modes.c:2304-2307`) e ao ponto
do código que precisa mudar (`smart_sys_app_deep_sleep.cpp:69-80` e `531-536`).
São trabalho de implementação, sem incerteza.

## 3. Defeito remanescente: a capability pode ser registrada e nunca iniciada

Este é o único achado novo, e ele nasce da correção 2.4. Ao vincular o wakeup à
capability, a especificação passou a apoiar-se numa premissa que enuncia como
fato: "essa capability configura direção e pull do pad". A premissa é verdadeira
quando a capability **inicia**, e a validação nova garante apenas que ela esteja
**registrada**.

O pad do contato é configurado dentro de `DigitalInputBehavior::begin()`, que
chama `configureGpio()` (`digital_input_behavior.cpp:106-117` e `62-75`).
`begin()` só é alcançado em `SetupStage::StartDevice`. Existem caminhos em que a
task de lifecycle nasce, o deadline vence e a sequência terminal roda **sem que
esse estágio tenha sido alcançado**:

- `InitializeNetwork` devolve `NotReady` — commissioning incompleto — e `setup()`
  retorna com `AppState::NotReady`, sem `RegisterCapabilities` nem `StartDevice`
  (`smart_sys_app.cpp:358-372`);
- `InitializeNetwork` falha de forma dura, ou `RegisterCapabilities` falha, com
  `rollbackTransport()` e `fail()` (`smart_sys_app.cpp:372`, `378-386`);
- `StartDevice` falha (`smart_sys_app.cpp:388-394`): como `IsspDevice::start()`
  aborta no primeiro behavior que não retorne `Ok`, o do contato pode estar entre
  os não iniciados.

Em todos eles a task já existe, porque nasce ao final de `InitializePlatform`
bem-sucedido (`smart_sys_app.cpp:353`), e o caminho forçado é justamente o
comportamento desejado: um dispositivo a bateria que não conseguiu subir a rede
deve dormir, não ficar acordado. A v0.10 tratou esse caminho com cuidado, e é por
isso que `quiesce()` e `stop()` são no-op bem-sucedidos quando nada foi iniciado.

O que a v0.11 acrescenta a esse mesmo caminho é a armação de EXT1. E aí a
premissa falha: o pad nunca foi posto em entrada com o pull do board, está no
default de reset — o HOLD do preparo congelaria um pino sem pull definido. O
resultado é exatamente o modo de falha que DEEPSLEEP-AC-012 existe para excluir,
com dois agravantes:

- **é o pior momento possível.** É o boot em que o dispositivo já falhou em
  subir a rede; um wakeup espúrio repetido por pino flutuante consome bateria em
  ciclos acordados que também falharão, e o produto não tem rate limit nesta
  versão, por DEEPSLEEP-DEC-015;
- **o experimento de AC-012 não o alcança.** Aquele critério exercita o par
  produto/board no caminho normal, com a capability iniciada. Nada nele
  observaria este caso, então o defeito não seria capturado pela evidência que a
  especificação já exige.

Não é o caso de resolver isto como detalhe de implementação, e por isso o
devolvo. As saídas têm consequências normativas distintas:

- **a fachada configura o pad antes de ler.** Os dados necessários já estão nela:
  a correspondência obrigatória identifica a capability, e
  `doorSensorConfigs_[i].pull` carrega o pull do board
  (`smart_sys_app_impl.hpp:158`). Não amplia API reutilizável nem consulta o
  behavior. É a saída que eu recomendaria, porque torna a premissa verdadeira em
  todo boot; exige uma sentença dizendo que a fachada aplica direção e pull da
  capability correspondente quando ela não tiver iniciado, e uma revisão da
  cláusula "contato desabilitado ... não altera o GPIO", que hoje sugere que o
  contato habilitado é que pode alterá-lo;
- **não armar o contato quando a capability não iniciou**, dormindo só com o
  timer. Preserva a bateria e é simples, mas contraria a intenção de DEC-012, e
  precisa dizer explicitamente que essa ausência não é "falha ao preparar uma
  fonte solicitada" — senão a regra da seção 6.1 bloquearia o sleep e manteria
  acordado justamente o dispositivo que não conseguiu subir a rede;
- **aceitar e documentar**, restringindo a promessa de AC-012 ao caminho em que a
  capability iniciou. É a saída que menos trabalho exige e a que mais deixa em
  aberto para o campo.

## 4. Pontos reconfirmados sem mudança

- **a baseline comporta o acréscimo.** Pontos de inserção prontos em
  `smart_sys_app_deep_sleep.cpp`, agregado extensível, validação de composição
  por lista em `client_154/main/CMakeLists.txt:42-49` para acomodar
  `dry_contact_wakeup`, e nenhuma dependência de build nova;
- **DEEPSLEEP-DEC-013 confere.** Nada exige quarta operação compartilhada:
  `DigitalInputBehavior::quiesce()` não toca GPIO
  (`digital_input_behavior.cpp:386-401`), e a leitura do item 1 antecede a
  quiescência;
- **o teste de fronteira não se aplica.** Sem capacidade arquitetural ausente,
  sem novo lifecycle ou ownership, sem consumidor afetado fora do recorte. O
  defeito da seção 3 é de comportamento da própria funcionalidade e de seus donos
  naturais;
- **relações normativas.** `ISSP-Reusable-Components.md` v1.1,
  `ISSP-Architecture.md` v1.2, `ISSP-Commissioning.md` v1.0, ADR-0002 e
  `Repository-Test-Execution-Policy.md` v0.4 permanecem preservadas; as emendas a
  `Firmware-Variants-Menuconfig.md` e a `ISSP-Configurable-Bootstrap.md` v1.5
  estão declaradas e são pequenas. Mantenho a observação, não bloqueante, de que
  o bootstrap ainda não incorporou a emenda da v0.10 e conviria aplicar as duas
  juntas;
- **implementação:** `esp_sleep_enable_ext1_wakeup_io()` é a chamada correta na
  6.0, agora nomeada pela própria especificação, e o H2 declara
  `SOC_PM_SUPPORT_EXT1_WAKEUP_MODE_PER_PIN` (`soc_caps.h:514`), de modo que uma
  chamada por boot basta; a costura de doubles pede o par de `prepareTimerWakeup`
  para o contato, costura interna prevista por AC-010.

## 5. Componentes impactados

| Fonte | Mudança | Natureza |
|---|---|---|
| `components/issp_app_154/include/SmartSysApp.h` | `ContactWakeupConfig` (`enabled`, `pin`), campo ao final de `DeepSleepConfig`, costura em `SetupHooks` | aditiva, compatível |
| `components/issp_app_154/src/smart_sys_app.cpp` | cruzamento contato/capability em `ValidateConfiguration` | validação, sem tocar hardware |
| `components/issp_app_154/src/smart_sys_app_deep_sleep.cpp` | elegibilidade por `esp_sleep_is_valid_wakeup_gpio()`, leitura e armação de EXT1, causa `EXT1`, condição de `no_wakeup_source` | política, local |
| `components/issp_app_154/src/smart_sys_app_impl.hpp` | declarações correspondentes | privada |
| `client_154/main/CMakeLists.txt` | `dry_contact_wakeup` nas listas exigida e oferecida | composição |
| `client_154/main/firmwares/door_sensor_battery_h2.cpp` | `contactWakeup` a partir do board | produto |
| `client_154/main/boards/*` | declaração do recurso apto a wakeup | board |
| `test_apps/smart_sys_app_test` | casos novos; os 15 existentes seguem válidos sem edição | evidência |

Nenhum componente ISSP compartilhado é tocado.

## 6. Experimentos e verificações necessários

Herdados e abertos: 10, 11, 19, 20 e 21, mais a medição dos três termos do
orçamento de DEEPSLEEP-AC-010. O item 16 foi resolvido na implementação da v0.10.

Da v0.11, mantidos:

- **22 — DEEPSLEEP-AC-012, retenção do nível**, no par produto/board da seção 5:
  ausência de wakeup espúrio com o contato estável em cada nível ao longo de um
  período completo de timer, e wakeup efetivo na transição nos dois sentidos;
- **23 — repique e acionamentos sucessivos**, com wakeups contados e corrente
  medida, incluindo o ciclo gasto pelo wakeup imediato que a seção 4.2A agora
  descreve;
- **24 — report em todo boot**, que o `reportOnStart` publique em boot por EXT1
  como publica em boot por timer.

Acrescentado por esta análise, e dependente da decisão da seção 3:

- **26 — armação do contato em boot sem `StartDevice`.** Provocar `NotReady` em
  `InitializeNetwork` — dispositivo não comissionado — com contato habilitado, e
  observar o comportamento do pino durante o sleep e a contagem de wakeups no
  período seguinte. É o caminho que nenhum dos experimentos anteriores exercita.

Leitura de código não certifica nenhum deles.

## 7. Classificação e recomendação

**Não pronta — defeito da especificação** [`Not Ready — Specification Defect`].

As quatro correções do Arquiteto estão bem formadas, correspondem ao que foi
recomendado e se sustentam no código e no IDF: a elegibilidade passou a usar o
mesmo predicado que o próprio ESP-IDF usa e é chamável onde a especificação a
coloca; o momento da armação ficou unívoco nos quatro lugares; o campo sem efeito
saiu; e o parágrafo de retenção passou a descrever o mecanismo real do ESP32-H2.
Nenhum defeito anterior sobrevive.

O que impede recomendar prontidão é um defeito **novo**, criado pela correção
2.4 e não existente no texto anterior: ao vincular o wakeup a uma capability
registrada, a especificação assumiu como fato que essa capability configura o
pad, e há caminhos de boot definidos — `NotReady` de rede à frente deles — em que
ela é registrada e nunca iniciada, e nos quais a fachada armaria EXT1 sobre um
pad em default de reset. O defeito é de comportamento da funcionalidade e de seus
donos naturais, o que exclui pré-requisito arquitetural; a correção é de uma a
duas sentenças, entre as três saídas da seção 3, e recomendo a primeira.

Não classifico como **evidência requerida**: seguindo o precedente da v0.10,
critério de hardware em aberto não impede que a baseline comporte a mudança, e a
especificação já declara DEEPSLEEP-AC-012 insatisfazível neste recorte. O que
impede começar é o contrato.

Sobre independência, como as regras comuns exigem quando ela é material: esta
análise **não é independente**. Foi produzida pela mesma capacidade e na mesma
sessão que a análise cujos achados o Arquiteto acabou de incorporar, e a seção 1
registra um erro de processo meu nesta mesma atuação. Se o Arquiteto quiser
confronto independente antes de promover, ele deve vir de outro ator — e o
defeito da seção 3 é um bom teste para esse confronto, por ser o tipo de caminho
que se perde entre versões.

A recomendação informa o Arquiteto. Não certifica implementabilidade de forma
absoluta e não promove a especificação para `Pronta`.
