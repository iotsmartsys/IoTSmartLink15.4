# Histórico de mudanças EKOM

Este arquivo registra estado resumido e referências materiais. O histórico
detalhado da EKM 1.x permanece em
`docs/history/ekom-1x/EKM-CHANGELOG.md`; o Git preserva autoria e diferenças.

## Transações legadas preservadas

| ID | Título | Estado | Fonte material principal |
|---|---|---|---|
| `EKM-CHG-0001` | Instituição da governança EKM | Closed | Histórico EKM 1.x |
| `EKM-CHG-0002` | Restauração da arquitetura ISSP | Closed | `ISSP-Architecture.md` |
| `EKM-CHG-0003` | Consistência e ciclo das mudanças | Closed | Histórico EKM 1.x |
| `EKM-CHG-0004` | Componentes ISSP reutilizáveis | Closed | `ISSP-Reusable-Components.md` |
| `EKM-CHG-0005` | Baseline e evidências não ambíguas | Closed | Histórico EKM 1.x |
| `EKM-CHG-0006` | Ciclo de vida das especificações | Closed | Histórico EKM 1.x |
| `EKM-CHG-0007` | Bootstrap configurável do client | Closed | `ISSP-Configurable-Bootstrap.md` |
| `EKM-CHG-0008` | Registry persistente do coordenador | Open | `ISSP-Coordinator-Paired-Device-Registry.md` |
| `EKM-CHG-0009` | Retirada transversal de QEMU | Closed | `Repository-Test-Execution-Policy.md` |
| `EKM-CHG-0010` | Correção dos targets admitidos | Closed | ADR-0003 e política de targets |

## EKOM-CHG-0001 — Adoção e reconciliação do EKOM 3.2

**Estado:** Fechada [`Closed`]

**Especificação relacionada:** Não aplicável; governança documental

**Objetivo:** Adotar o roteamento documental, as ADRs, os relatórios separados,
o dossiê e as três visões do mapa do EKOM 3.2 sem alterar código funcional.

### Decisões relacionadas

- ADR-0001, ADR-0002 e ADR-0003 locais;
- ADR-0003 e ADR-0004 do EKOM 3.2 externo.

### Lacunas

- lacunas técnicas legadas permanecem identificadas no mapa;
- migração não cria requisito funcional novo.

### Relatórios e evidências materiais

- relatório de análise e validação desta reconciliação em `docs/reports/`.

### Resultado

A fundação EKOM 3.2 foi adotada, as ADRs locais foram aceitas, os registros EKM
1.x foram preservados e 39 seções históricas foram separadas em relatórios. O
Arquiteto confirmou a reconciliação e autorizou commit, push, merge e publicação
da `main`.

## EKOM-CHG-0002 — Deep sleep configurável do client

**Estado:** v0.10 e v0.11 em implementação [`In Progress`]

**Gates da revisão corrente da v0.11:** análise `Ready` presente para
`ea95c77`; promoção para Pronta presente; autorização de implementação presente,
concedida em 12/08/2026.

**Especificação relacionada:** `docs/specs/Client-Deep-Sleep.md`

**Objetivo:** Especificar deep sleep opt-in para devices client a bateria,
wakeup periódico em minutos ou horas e LED indicador configurável por GPIO,
polaridade e tempo ligado.

### Decisões relacionadas

- ADR-0002 preserva a separação entre política do product firmware e recurso
  físico do board model;
- o recurso permanece desabilitado por padrão e não altera o coordenador.
- a v0.4 declara alteração limitada do bootstrap e das variantes, preservando
  arquitetura, commissioning e política de execução de testes.
- na v0.5, o Arquiteto autorizou alteração limitada de
  `ISSP-Reusable-Components.md` para criar as operações públicas de quiescência
  necessárias, sem lifecycle público na fachada.
- na v0.6, o Arquiteto decidiu que factory reset e deep sleep obedecem à
  primeira transição aceita, com exclusão sobre operações NVS e encerramento.
- na v0.7, o Arquiteto aceitou preempção diagnosticada de `persistNetwork()` e
  exigiu evidência positiva de report para sleep antecipado, sem quarta
  ampliação da API reutilizável.
- a v0.8 atribui a sequência a uma única task privada, limita a espera de
  entrega pelo deadline e distingue o detentor da transição.
- na v0.9, o Arquiteto tornou `InitializePlatform` não preemptível; o deadline
  continua contado e dispara encerramento forçado após o estágio, se expirado.
- a v0.10 precisa o contrato sem nova decisão arquitetural: orçamento físico
  acordado, limite conservador do timer, polling com predicados separados e
  encerramento seguro para objetos nunca iniciados.
- após a análise da v0.10 recomendar prontidão sem ressalva contratual, o
  Arquiteto promoveu a especificação para pronta para implementação.

### Lacunas

- bloqueadores normativos da versão 0.1 foram resolvidos pelo Arquiteto na
  versão 0.3;
- a v0.4 incorporou os pontos normativos devolvidos pela verificação: relação
  com o bootstrap, quiescência sem `stop()` público, ampliação da API, alcance
  integral do renome e ordem do LED em `setup()`;
- a análise da v0.4 delimitou o deadline em pontos cooperativos; a preempção
  durante escrita em NVS permanece risco técnico dependente de experimento;
- a v0.5 incorpora os pontos devolvidos pela análise da v0.4: relação com
  `ISSP-Reusable-Components.md`, três operações mínimas de quiescência,
  atualização direta de `client_154/sdkconfig` e colisão restrita ao
  `wake_led`;
- a análise da v0.5 confirmou a relação com `ISSP-Reusable-Components.md` e a
  suficiência das três operações;
- a v0.6 incorpora os pontos devolvidos: arbitragem e parada do monitor de
  factory reset, correção da nota para cerca de 1,47 s, waits notificáveis sem
  Kconfig e precisão de que `stop()` encerra tentativas do boot corrente;
- a análise da v0.6 confirmou a arbitragem em código privado da fachada e as
  duas paradas notificáveis, e devolveu ao Arquiteto duas decisões: a exclusão
  sobre `persistNetwork()`, que é privado e inalcançável pela fachada dentro do
  recorte de DEEPSLEEP-DEC-006, e o gatilho do sleep antecipado, que pode ser
  satisfeito por ausência de report em vez de entrega;
- a mesma análise devolveu duas precisões de redação: limite e estouro da espera
  de `stop()` antes de `end()`, e re-arme do hold do factory reset rejeitado por
  arbitragem perdida;
- a v0.7 incorpora essas decisões e precisões com preempção diagnosticada,
  critério positivo de admissão e entrega, timeout de 600 ms com supressão de
  `end()` e re-arme do pedido rejeitado;
- preempção de NVS, corrida entre transições e usabilidade do hold permanecem
  riscos experimentais ou de produto, não decisões normativas pendentes deste
  recorte;
- a análise da v0.7 confirmou os quatro pontos pedidos pela seção 8 no código e
  não encontrou bloqueador estrutural remanescente; devolveu três precisões de
  redação — limite e dono da espera por `pendingReportCount() == 0`, distinção
  do detentor da transição e alcance de DEEPSLEEP-AC-004 diante de
  `ValidateConfiguration` — e dois pontos de método: o meio admissível do
  experimento de DEEPSLEEP-AC-008B e o limite de sono do ESP32-H2 relevante para
  DEEPSLEEP-AC-003;
- a v0.8 incorpora as três precisões, define instrumentação temporária H2 como
  meio futuro do experimento de NVS, valida timer pelo limite efetivo do
  ESP-IDF/H2 e explicita as costuras de teste sem ampliar a API normativa;
- a análise da v0.8 confirmou as cinco confirmações pedidas pela seção 8 e
  devolveu ao Arquiteto uma decisão: a task de lifecycle nasce em
  `InitializePlatform` antes de device, executor e monitor existirem, e a ordem
  obrigatória não admite passo ausente — ou os passos passam a ser condicionais
  à existência do recurso, ou `InitializePlatform` deixa de ser preemptável,
  saída que também cobre a janela de `nvs_flash_erase()`;
- a mesma análise devolveu duas precisões: "dona exclusiva da transição", que
  induz token de escritor único e quebraria a arbitragem, e a validação da faixa
  do intervalo também em `configureDeepSleep()`, sem a qual intervalo constante
  fora de faixa produz dispositivo a bateria que nunca dorme;
- a v0.9 incorpora a decisão e as precisões: criação da task somente após
  `InitializePlatform` bem-sucedido, dona exclusiva apenas da sequência,
  validação antecipada e defensiva do timer e polling limitado a 10 ms;
- preempção de `persistNetwork()`, corridas e consumo no caminho sem `end()`
  permanecem riscos dependentes dos experimentos futuros já declarados, não
  decisões normativas pendentes deste recorte;
- a análise da v0.9 confirmou as cinco confirmações pedidas pela seção 8 e não
  encontrou bloqueador estrutural nem decisão normativa ausente; a janela não
  preemptível também retirou a lacuna da janela de `nvs_flash_erase()`;
- a mesma análise devolveu três precisões — orçamento de tempo acordado, que
  passa a incluir `InitializePlatform` e a sequência terminal; margem
  conservadora do limite do timer, porque o slow clock do projeto é RC interno
  calibrado; e separação entre o predicado de prontidão dos reports iniciais e o
  oráculo de entrega no polling — mais a obrigação de que `quiesce()` e `stop()`
  sejam seguros quando o objeto nunca foi iniciado no boot.
- a v0.10 incorpora essas precisões. Como o deadline permanece absoluto desde
  `setup()`, o orçamento é expresso por `max(duração de InitializePlatform,
  maxAwakeTimeMs) + duração da sequência terminal`, e não pela soma simples do
  estágio com `maxAwakeTimeMs`;
- a análise da v0.10 confirmou os quatro pontos e a correção de
  DEEPSLEEP-AC-007, e recomendou prontidão sem bloqueador, decisão ausente ou
  precisão pendente. As pendências restantes não são do contrato: verificação do
  limite do ESP-IDF, resolvida na implementação com o toolchain presente, e os
  experimentos de hardware já declarados;
- a mesma análise precisou a justificativa da supressão de `end()`:
  `issp154_transport_deinit()` já recusa sob transmissão síncrona, de modo que a
  janela desprotegida é a da espera de ACK sobre o event group;
- a implementação resolveu o item 16 pelo toolchain presente e registrou que, no
  ESP-IDF 6.0.1, `esp_sleep_enable_timer_wakeup()` deriva o limite da constante
  nominal do RC lento, não da calibração em runtime; a margem conservadora do
  contrato permanece correta e hoje é folga adicional;
- a implementação devolve duas questões ao Arquiteto: o crescimento de
  `kImplStorageBytes` de 10240 para 16384 bytes, que aciona o novo confronto
  previsto pelo item 39 de `Firmware-Variants-Menuconfig.md`, e os valores de
  política do produto a bateria, que a especificação atribui ao product firmware
  e que a implementação escolheu para viabilizar a primeira composição;
- build, teste, flash e hardware não foram executados pelo agente. O Arquiteto
  relatou ter compilado e executado o firmware em ESP32-H2, o que retira a
  compilação e os `static_assert` de dimensionamento da lista de incógnitas;
- dessa execução veio a intenção da v0.11: o dispositivo não acorda pela
  transição do contato no GPIO 14, comportamento contratado pela v0.10, cuja
  seção 1 exclui outras fontes de wakeup. A v0.11 especifica o wakeup por
  contato via EXT1, com rearme para o nível oposto a cada boot, timer e contato
  habilitados juntos e sem nova ampliação da API reutilizável;
- a v0.11 deixa três decisões pendentes do Arquiteto: base do nível oposto
  (elétrico ou lógico confirmado), tratamento de contato instável e confirmação
  dos valores de política do produto herdados da implementação da v0.10.
- o Arquiteto resolveu as três pendências da v0.11: EXT1 usa o nível elétrico
  imediatamente anterior ao sleep; não há rate limit nesta versão e o consumo
  será medido; o timer da primeira composição é de 15 minutos. Os valores de
  `maxAwakeTimeMs` e duração do LED não foram alterados nem ratificados pela
  v0.11.

### Relatórios e evidências materiais

- análise inicial em
  `docs/reports/client-deep-sleep/analysis/2026-08-11-initial-analysis.md`;
- análise de verificação em
  `docs/reports/client-deep-sleep/analysis/2026-08-11-verification-analysis.md`;
- análise de implementabilidade da v0.4 em
  `docs/reports/client-deep-sleep/analysis/2026-08-11-v04-implementability-analysis.md`;
- análise de implementabilidade da v0.5 em
  `docs/reports/client-deep-sleep/analysis/2026-08-11-v05-implementability-analysis.md`;
- análise de implementabilidade da v0.6 em
  `docs/reports/client-deep-sleep/analysis/2026-08-11-v06-implementability-analysis.md`;
- análise de implementabilidade da v0.7 em
  `docs/reports/client-deep-sleep/analysis/2026-08-11-v07-implementability-analysis.md`;
- análise de implementabilidade da v0.8 em
  `docs/reports/client-deep-sleep/analysis/2026-08-11-v08-implementability-analysis.md`;
- análise de implementabilidade da v0.9 em
  `docs/reports/client-deep-sleep/analysis/2026-08-11-v09-implementability-analysis.md`;
- análise de implementabilidade da v0.10 em
  `docs/reports/client-deep-sleep/analysis/2026-08-11-v10-implementability-analysis.md`;
- implementação da v0.10 em
  `docs/reports/client-deep-sleep/implementation/2026-08-11-v10-implementation.md`;
- análise de implementabilidade da v0.11 em
  `docs/reports/client-deep-sleep/analysis/2026-08-12-v11-implementability-analysis.md`;
- nova análise de implementabilidade da v0.11, sobre o texto corrigido pelo
  Arquiteto, em
  `docs/reports/client-deep-sleep/analysis/2026-08-12-v11-implementability-analysis-r3.md`;
- terceira análise da v0.11, sobre o texto com o preparo do pad definido, em
  `docs/reports/client-deep-sleep/analysis/2026-08-12-v11-implementability-analysis-r4.md`;
- confronto final da v0.11 sobre a revisão `ea95c77` em
  `docs/reports/client-deep-sleep/analysis/2026-08-12-v11-implementability-analysis-r5.md`;
- implementação da v0.11 em
  `docs/reports/client-deep-sleep/implementation/2026-08-12-v11-implementation.md`;
- build canônico H2 executado na implementação da v0.11, com código de saída 0
  nas duas composições do client e no test app `smart_sys_app_test`; testes,
  flash, monitor e hardware permanecem `Not Executed`.

### Resultado

Contrato v0.10 promovido pelo Arquiteto para `Proposed` e implementado
integralmente em código, incluindo as três ampliações de DEEPSLEEP-DEC-006, a
arbitragem com factory reset, o renome completo e a composição `wake_led`. A
implementação permanece `In Progress`: build, teste, flash e hardware não foram
autorizados nem executados pelo agente, de modo que nenhuma verificação técnica
produzida por ele sustenta conclusão. A v0.11, que acrescenta o wakeup por
contato, permanece em `Draft`.

A análise de implementabilidade da v0.11 foi executada e classificou a versão
como **Não pronta — defeito da especificação**, por dois defeitos: a exceção do
GPIO 7 na faixa elegível para EXT1, contrariada pelo `rtc_io_num_map` do
ESP32-H2 no ESP-IDF 6.0.1 e incompatível com a derivação por capacidade do
target exigida na mesma seção; e o momento da leitura do contato, dado de formas
divergentes em 4.2A, 6.1 e DEEPSLEEP-DEC-014. A mesma análise registrou que a
cadeia de retenção do nível por HOLD é verificável por leitura em quase toda a
sua extensão, inclusive a liberação automática do hold no boot seguinte,
restando físico apenas o elo elétrico.

O Arquiteto incorporou os achados na v0.11 ainda em `Draft`: a elegibilidade é
derivada da capacidade vigente do target, sem exceção local para GPIO 7; o board
declara o recurso físico e o GPIO precisa corresponder à capability de contato
seco que fornece direção e pull; a leitura e a armação de EXT1 ocorrem no
início da sequência terminal, antes de qualquer operação terminal; e
`ContactWakeupConfig` deixa de expor polaridade lógica sem efeito. A precisão de
retenção por HOLD e liberação no boot foi incorporada, sem substituir o
experimento elétrico de DEEPSLEEP-AC-012. A revisão aguarda nova análise de
implementabilidade e permanece sem promoção ou autorização de implementação.

A nova análise foi executada sobre o texto corrigido e confirmou que as quatro
correções estão bem formadas e são implementáveis com dados que a fachada já
possui, encerrando os dois defeitos anteriores. A classificação permanece **Não
pronta — defeito da especificação** por **um defeito novo, criado pela própria
correção**: ao vincular o wakeup a uma capability de contato seco registrada, a
especificação passou a assumir que essa capability configura direção e pull do
pad, mas há caminhos de boot definidos — `NotReady` de `InitializeNetwork` entre
eles — em que ela é registrada e nunca iniciada, e nos quais a fachada armaria
EXT1 sobre um pad em default de reset, com risco de wakeup espúrio no caminho de
falha de rede. O relatório oferece três saídas e recomenda que a fachada aplique
direção e pull da capability correspondente quando ela não tiver iniciado, o que
não amplia API reutilizável. Nenhum gate da v0.11 foi satisfeito por estas
atuações.

O Arquiteto incorporou também esse último achado mantendo a v0.11 em `Draft`.
No preparo do contato, a fachada reaplica sempre e de forma idempotente o modo
de entrada e o pull da capability correspondente antes de ler o nível e armar
EXT1. A regra vale quando o behavior já iniciou e quando `StartDevice` não foi
alcançado; falha de configuração do GPIO é falha de preparo da fonte e aborta
antes da quiescência. DEEPSLEEP-AC-012 passa a exigir evidência também no boot
que alcança a sequência terminal sem `StartDevice`, incluindo `NotReady` em
`InitializeNetwork`. A revisão aguarda nova análise de implementabilidade e
continua sem promoção ou autorização de implementação.

A terceira análise da v0.11 foi executada sobre esse texto e recomenda
**prontidão** [`Ready`]. A reaplicação é implementável com dados que a fachada já
copia em `AppState::Configuring` — `pin` e `pull` de `DoorSensorConfig` —,
disponíveis em todos os caminhos terminais, com o mesmo mapeamento que
`DigitalInputBehavior::configureGpio()` usa, sem ampliar API reutilizável e sem
mover responsabilidade entre camadas; `gpio_config()` é idempotente, o que
sustenta a cláusula de reaplicação com o behavior já iniciado. Não foram
encontrados bloqueador estrutural, decisão normativa ausente nem contradição
interna. Permanece uma precisão declarada **não bloqueante**: duas capabilities
de contato seco no mesmo GPIO com `pull` divergente tornam indeterminada a
configuração a reaplicar, ambiguidade que precede a v0.11 na camada de behaviors
e não é alcançável na composição da seção 5. As pendências restantes são os
experimentos de hardware já normativos. A análise r4 satisfez o gate de análise
para a revisão `5b2cc09`; promoção e autorização permaneceram ausentes.

O Arquiteto incorporou a precisão final ainda em `Draft`: quando mais de uma
capability de contato seco corresponder ao GPIO do wakeup, pulls divergentes são
rejeitados em `ValidateConfiguration` com `InvalidArgument`; pulls iguais são
equivalentes para o preparo idempotente. Como a fonte normativa mudou depois da
r4, a revisão corrente aguarda confronto final antes de promoção e continua sem
autorização de implementação.

O confronto final da revisão corrente foi executado sobre `ea95c77` e recomenda
**prontidão** [`Ready`]. A cláusula acrescentada depois da r4 é implementável no
estágio que a especificação indica: `pin` e `pull` de todas as capabilities já
estão em `doorSensorConfigs_` quando `setup()` alcança `ValidateConfiguration`,
o estágio é anterior a qualquer toque em NVS, rádio, RTC ou GPIO, e o
`SetupResult` com `InvalidArgument` já existe. A cláusula é condicionada ao GPIO
do wakeup, logo não cria validação global entre pares antes aceitos, e a
equivalência entre pulls iguais é real: são os dois únicos campos do
`gpio_config_t` derivados do `pull`. Os fatos de target do acréscimo inteiro
foram reverificados na fonte do ESP-IDF 6.0.1 — pureza e identidade do predicado
de elegibilidade, faixa GPIO 7 a 14 no ESP32-H2, HOLD incondicional no ramo
digital de `ext1_wakeup_prepare()` e liberação no boot seguinte antes de
`app_main()` —, restando físico apenas o elo elétrico reservado a
DEEPSLEEP-AC-012. Sem bloqueador, decisão normativa ausente ou contradição
interna. Permanecem quatro precisões declaradas não bloqueantes: as colisões de
pino ainda abertas fora do `wake_led`, o pull `Floating` aceito, a necessidade de
costura em `SetupHooks` para cobrir o preparo do contato com doubles e a seleção
explícita do ESP-IDF 6.0.1 em evidência futura. O gate de análise passa a estar
satisfeito para a revisão `ea95c77`; promoção e autorização de implementação
permaneciam ausentes naquele momento.

Com base no confronto final r5, o Arquiteto promoveu a v0.11 para `Proposed`,
Pronta para implementação. A promoção satisfaz o segundo gate, mas não concede
autorização de implementação, que permanece ausente e separada.

O projeto adotou a regra transversal do EKOM 3.6: build canônico integra a
implementação autorizada e não é permissão ou requisito repetido pela
especificação funcional. A v0.11 removeu a antiga proibição e a menção de build
em DEEPSLEEP-AC-010; execução de testes, flash, monitor e hardware permanece sob
autorização própria e sob `Repository-Test-Execution-Policy.md`.

Depois de confirmados a análise `Ready` e o estado Pronta para implementação, o
Arquiteto concedeu em 12/08/2026 autorização explícita para implementar a v0.11
de `Client-Deep-Sleep.md`. Os três gates da implementação normativa estão
satisfeitos. O build canônico proporcional integra a implementação conforme o
EKOM 3.6; testes, flash, monitor e hardware não foram autorizados.

O acréscimo da v0.11 foi implementado: `ContactWakeupConfig` no contrato
público, elegibilidade derivada da capacidade do target, correspondência com a
capability de contato seco validada em `ValidateConfiguration`, reaplicação
idempotente do pad e armação de EXT1 para o nível oposto ao lido no início da
sequência terminal, bloqueio do sleep por falha de qualquer fonte solicitada e o
recurso de composição `dry_contact_wakeup`. O build canônico H2 foi executado com
sucesso nas duas composições do client e no test app afetado. A implementação
permanece `In Progress`: DEEPSLEEP-AC-012 exige evidência em hardware, que não
foi autorizada, e as 45 suítes do `SmartSysApp` continuam não executadas. Dois
pontos foram devolvidos ao Arquiteto: a leitura da causa do boot passou a usar
`esp_sleep_get_wakeup_causes()` para não perder o contato quando as duas fontes
disparam juntas, e a incorporação de `dry_contact_wakeup` em
`Firmware-Variants-Menuconfig.md` permanece pendente da autoridade normativa.

## EKOM-CHG-0003 — Identidade de reports entre boots

**Estado:** autoria [`Draft`]

**Gates da revisão corrente:** análise independente ausente; promoção ausente;
autorização de implementação ausente.

**Especificação relacionada:** `docs/specs/ISSP-Report-Identity.md`

**Objetivo:** substituir a deduplicação por última sequência por uma identidade
de report que sobreviva semanticamente ao reboot do client, sem sessão,
handshake ou persistência.

### Decisões relacionadas

- ADR-0004 adota `report_id` aleatório não nulo de 64 bits gerado pelo client;
- protocolo ISSP v2 transporta e ecoa o ID em DATA/ACK;
- retries reutilizam o ID e novas admissões recebem outro;
- o coordenador usa janela FIFO volátil de oito fingerprints por dispositivo
  conhecido;
- evento integralmente aceito na fila UART precede cache e ACK;
- entrega durável, confirmação do host e exactly-once permanecem fora do
  recorte.

### Relações e limites

A fonte altera de forma delimitada arquitetura, componentes reutilizáveis,
registry, commissioning, bootstrap e baseline wire consolidada. Preserva deep
sleep, variantes, NVS, retries e separação de código entre targets. A mudança
do protocolo exige corte coordenado v1 → v2, sem recomissionamento causado
apenas pela versão.

### Relatórios e evidências materiais

- diagnóstico do defeito em
  `docs/reports/client-deep-sleep/analysis/2026-08-12-v11-hardware-forwarding-diagnosis.md`;
- análise independente de implementabilidade da v0.1 ainda não executada.

### Próxima etapa

Submeter a revisão exata da v0.1 a análise independente conforme a seção 16 da
especificação. Autoria não altera código nem satisfaz os gates de promoção e
implementação.
