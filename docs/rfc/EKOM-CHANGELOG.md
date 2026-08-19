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

**Estado:** v0.3 Concluída [`Done`] por decisão do Arquiteto

**Entrada da Implementação da revisão corrente:** análise `Ready` presente para
o conteúdo normativo de `9687287`; ordem explícita para implementar a v0.3
recebida e executada. As análises das versões anteriores permanecem históricas.

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
- análise independente de implementabilidade da v0.1 em
  `docs/reports/report-identity/analysis/2026-08-12-v01-implementability-analysis.md`.

A análise confrontou os doze pontos da seção 16 sem executar build, testes ou
hardware. A baseline comporta a funcionalidade e não há pré-requisito
arquitetural; o impacto está delimitado. Dois contratos da própria
funcionalidade bloqueiam a prontidão: a ampliação de API contratada alcança a
API pública de `issp_transport_154`, não declarada entre as emendas da seção 2;
e o critério de aceitação local da seção 8.2 é inexecutável com a API mandatada,
porque `uart_write_bytes()` não devolve escrita parcial e aguarda o dreno físico
quando a fila está cheia. Treze achados não bloqueantes acompanham o relatório,
entre eles a confirmação de que o consumidor externo `SmartHome-Hub` aceita o
`event_id` sem alteração e de que `esp_fill_random()` é admissível no H2 porque
toda admissão de report ocorre com o rádio já habilitado.

### Próxima etapa

Decisão do Arquiteto sobre os dois bloqueadores e sobre a incorporação dos
achados não bloqueantes. Nova revisão da especificação exige nova análise da
revisão exata. Os gates de promoção e autorização permanecem ausentes.

O Arquiteto resolveu os dois bloqueadores em 12/08/2026. A v0.2 autoriza a
ampliação pública mínima de `issp_transport_154` para carregar `report_id` em
`Issp154AckExpectation` e nos tipos estritamente necessários à correlação, sem
alterar `IIsspTransport`, lifecycle ou retry. ADR-0004 foi reconciliada com esse
alcance.

A aceitação local deixou de depender de um retorno parcial que
`uart_write_bytes()` não oferece. O caminho de report tenta o lock sem espera,
consulta o limite conservador de `uart_get_tx_buffer_free_size()` e só então
submete JSON mais delimitador em uma única escrita. Lock ocupado, consulta
falha, espaço insuficiente ou retorno inesperado não inserem cache nem produzem
ACK; o laço de RX não espera capacidade ou dreno físico.

A v0.2 incorpora ainda as precisões não bloqueantes materiais da análise:
versão da consolidação, seis caminhos de frame, diagnóstico dos offsets v2,
dependência FreeRTOS preexistente, separação entre testes host-native e H2,
costura privada do coordenador, consequência do envio direto, espera ocupada da
fonte aleatória e suficiência de um ID por slot.

A análise independente da v0.2 está em
`docs/reports/report-identity/analysis/2026-08-12-v02-implementability-analysis.md`
e classifica a revisão como `Pronta`. Ela reconfrontou os dois bloqueadores
contra a plataforma, não apenas contra o texto: `uart_get_tx_buffer_free_size()`
existe no ESP-IDF 6.0.1 com o limite conservador que a seção 8.2 lhe atribui e
dimensiona exatamente o pior caso de `uart_tx_all()`, que é descritor mais até
dois chunks; e a premissa de que todos os produtores da UART do host passam pelo
mesmo lock foi verificada verdadeira, com o console em USB Serial JTAG. Sete
achados não bloqueantes acompanham o relatório, com destaque para o
REPORT-ID-AC-007, que descreve como observável em produção uma contenção de lock
que a tarefa única do coordenador torna inalcançável.

Com base nessa análise, o Arquiteto promoveu a v0.2 para `Proposed`, Pronta para
implementação. A promoção satisfaz o segundo gate e não concede autorização de
implementação, que permanece ausente.

Em 12/08/2026 o Arquiteto ordenou a implementação da v0.2, satisfazendo o
terceiro gate. A implementação está registrada em
`docs/reports/report-identity/implementation/2026-08-12-v02-implementation.md`.

O corte v2 foi aplicado nos dois codecs separados: payload fixo de 20 bytes com
`report_id` little-endian no offset 8, recusa de `DATA` com identidade zero e de
tipo que não é report com identidade não nula. `IsspDevice` recebeu a fonte
injetável de IDs, com a geração fora do `portMUX` e a escolha de slot,
revalidação de colisão e mutação inteira em uma única entrada na seção crítica.
A correlação de ACK do transporte passou a exigir a identidade, de modo que um
ACK de comando não conclui um report. No coordenador, `s_last_seq` deu lugar ao
módulo privado `report_data_policy.c`, com janela FIFO volátil de oito
fingerprints por slot do registry e a ordem consulta, evento, cache e ACK; o
evento ao host ganhou o campo aditivo `event_id`.

Builds canônicos H2 e C6, exemplo mínimo e as quatro test apps de firmware
compilam. Três suítes host-native novas foram criadas e construídas — vetores
dourados por target, formatação do `event_id` e janela de deduplicação — e
permanecem `Not Executed`, como as demais. AC-005, AC-013 e a regressão de
transporte do AC-016 dependem de hardware e continuam sem evidência.

Uma divergência normativa foi devolvida ao Arquiteto: a seção 8.4 manda usar
para o `event_id` a mesma forma textual do campo `device_id` existente, mas seu
exemplo JSON mostra uma forma sem prefixo e em minúsculas. A implementação
seguiu a regra em prosa; o exemplo permanece inconsistente na fonte.

A revisão da v0.2 está em
`docs/reports/report-identity/review/2026-08-12-v02-review.md` e classifica a
implementação como aderente com ressalvas. Ela confirma a divergência da seção
8.4 como defeito da especificação e acrescenta cinco achados: o evento de origem
desconhecida também passou ao caminho UART sem espera, o que a seção 8.3 não
cobre e precisa de ratificação; o parser do coordenador recusa truncamento mas
aceita payload maior que 20 bytes, contra a seção 6.1 e o REPORT-ID-AC-004; o
REPORT-ID-AC-003 não tem oráculo de concorrência real nem da fonte fora do
`portMUX`; a parte UART do REPORT-ID-AC-007 não é exercitada por nenhuma suíte,
porque a costura substitui o evento inteiro; e `publishReport()` não revalida a
identidade contra os slots ocupados, risco residual só alcançável com gerador
determinístico. A revisão não executou build, teste nem hardware.

Em 12/08/2026 o Arquiteto relatou que a implementação foi exercitada em
hardware e se comportou como esperado no caminho principal. Esse relato
sustenta a funcionalidade observada, mas não substitui os cenários adversos
explicitamente não executados pela Revisão.

A Autoria incorporou os dois ajustes normativos devolvidos pela Revisão e abriu
a v0.3 em `Draft`: o exemplo de `event_id` agora reutiliza exatamente a forma
textual do `device_id`, e a aceitação UART sem espera antes do ACK passa a valer
também para origem desconhecida admitida durante commissioning. O contrato de
20 bytes foi explicitado para rejeitar tanto truncamento quanto excedente, e o
caso excedente foi vinculado à suíte já contratada do codec do coordenador.

Como a aceitação de origem desconhecida mudou normativamente, o `Ready` da v0.2
não cobre a v0.3. A próxima etapa é Análise de Implementabilidade do delta; não
há autorização de correção da v0.3 nesta atuação de Autoria.

A análise do delta está em
`docs/reports/report-identity/analysis/2026-08-12-v03-implementability-analysis.md`
e classifica a v0.3 como `Pronta`. Das três mudanças normativas, duas já estão
satisfeitas pela implementação vigente e não geram trabalho: o exemplo do
`event_id` passou a descrever o que `format_device_id()` e
`iot154_format_event_id()` já produzem, e a aceitação sem espera para origem
desconhecida já é o caminho único de evento desde `22e5e10`, com resultado, log
e caso de teste próprios. A regra da seção 8.3 preenche a célula que a matriz
9.1 do registry deixou deliberadamente aberta, sem persistir, sem criar entrada
e sem tornar a origem conhecida, de modo que não há conflito de autoridade.

Resta a recusa de payload excedente no coordenador, verificada em duas cópias da
mesma condição — `iot154_parse_frame_info()` e `diagnostic_extract_mac()` — mais
dois casos host-native na suíte de vetores já contratada. A igualdade estrita foi
confrontada contra os seis formatos de frame dos dois targets, todos com
`frame[0] == mac_header_len + 20 + 2`, de modo que nenhum frame legítimo passa a
ser rejeitado; frames v1 continuam recusados pelo mínimo já existente.

Seis observações acompanham o relatório, todas não bloqueantes: a seção 4 ainda
exclui de forma mais ampla que a seção 8.3 o tráfego de origem desconhecida; o
coordenador não distingue v1 de truncamento, contra a seção 11; a verificação de
comprimento está duplicada; a contenção do lock UART passa a alcançar a janela de
ingresso; A4, A5 e A6 da Revisão seguem abertos e intocados pelo delta; e a v0.3
foi analisada na árvore de trabalho, sem commit ao qual vincular o `Ready`,
enquanto o `KNOWLEDGE-MAP.md` ainda descreve a identidade de reports como v0.2
`Proposed` com implementação `In Progress`.

O commit `9687287` registrou sem alteração adicional o conteúdo normativo da
v0.3 confrontado pela análise. Com isso, a ressalva sobre ausência de revisão
versionada ficou superada. O Arquiteto reconheceu a classificação `Ready` e o
estado corrente foi reconciliado na especificação, no changelog e no mapa. Pelo
EKOM 4.1 não existe promoção intermediária para “Pronta”: a próxima passagem
depende somente de ordem explícita para implementar a v0.3.

A ordem foi emitida e a v0.3 está implementada, conforme
`docs/reports/report-identity/implementation/2026-08-12-v03-implementation.md`.
D1 e D2 foram confirmados por leitura, sem mutação, porque a implementação
vigente já os satisfazia. D3 exigiu igualdade estrita de comprimento nas duas
cópias da verificação do coordenador — `iot154_parse_frame_info()` e
`diagnostic_extract_mac()` —, com `payload_truncated` e `payload_excessive`
como motivos distinguíveis, mais um caso host-native na suíte de vetores já
contratada. A verificação de mínimo foi preservada, de modo que frames v1
continuam recusados antes de qualquer decodificação, e nenhum dos seis formatos
legítimos passa a ser rejeitado. O build canônico ESP32-C6 e os três builds
host-native do coordenador terminaram com código de saída zero; execução de
testes, flash, monitor e hardware permanece `Not Executed`.

Duas lacunas seguem abertas e devolvidas ao Arquiteto: o coordenador ainda não
distingue frame v1 de truncamento, contra a seção 11 e anterior a este delta
(O2); e a seção 4 da especificação continua excluindo de forma mais ampla que a
seção 8.3 o tráfego de origem desconhecida durante commissioning (O1). A próxima
etapa é a Revisão do delta.

A Revisão da implementação v0.3 está em
`docs/reports/report-identity/review/2026-08-12-v03-review.md`. O delta D3 foi
classificado como aderente: decisão e diagnóstico exigem comprimento exato, o
caso contratado cobre um byte a menos e a mais na função de produção e não
houve ampliação de API, arquitetura, reserva ou deep sleep. D1 e D2 permanecem
coerentes por leitura.

A versão integral ainda não sustenta `Done`. A Revisão confirmou como defeito
da especificação a contradição entre a exclusão ampla da seção 4 e a regra
específica da seção 8.3; confirmou como defeito de implementação a ausência do
diagnóstico próprio para frame v1 exigido pela seção 11; e preservou como
evidência insuficiente a concorrência da AC-003, a fronteira UART da AC-007 e a
execução do novo caso de comprimento. A recomendação é retornar à Autoria,
corrigir o diagnóstico na Implementação e realizar Revisão curta posterior.

Após a Revisão, o Arquiteto informou validação da implementação em hardware com
comportamento funcional conforme esperado e determinou o encerramento da v0.3.
A decisão e seus limites estão em
`docs/reports/report-identity/validation/2026-08-12-v03-architectural-validation.md`.

A conclusão aceita como riscos residuais conhecidos a contradição redacional
entre as seções 4 e 8.3, a ausência de diagnóstico próprio para frame v1 e a
falta de execução específica dos cenários adversos de concorrência e UART. Ela
não transforma suítes `Not Executed` em sucesso nem afirma cobertura de
cenários não enumerados. A fonte normativa permanece Active e o workflow da
v0.3 passa a Concluído [`Done`]; nova evidência ou necessidade exige decisão de
reabertura.

## EKOM-CHG-0004 — Nível de bateria do client

**Estado:** Fechada [`Closed`] — v0.5 Concluída [`Done`] por decisão do
Arquiteto em 15/08/2026

**Especificação relacionada:** `docs/specs/Client-Battery-Level.md`
(`EKOM-BATTERY-001`)

**Objetivo:** especificar a capability de nível de bateria do client, que mede a
tensão por entrada analógica com divisor resistivo, converte em percentual
inteiro e o publica como report ISSP, sem alterar wire, coordenador ou host.

### Decisões relacionadas

- o contrato é escrito em três camadas separadas de forma normativa: contrato
  genérico sem números, parâmetros do product firmware e fatos elétricos do
  board model; a composição do `door_sensor_battery_h2` é normativa somente para
  aquele produto;
- conversão linear entre `emptyMv` e `fullMv`, com aritmética inteira,
  arredondamento do meio para cima e saturação em 0 e 100;
- gatilho duplo: em composição com deep sleep, publica a cada wakeup, qualquer
  que seja a causa; sem deep sleep, amostra por período próprio e publica na
  variação mínima, com o primeiro valor válido sempre publicado;
- nenhuma faixa numérica é normatizada; existem apenas invariantes derivados,
  porque nenhuma evidência de plataforma sustentava os limites antes propostos;
- falha de leitura ou amostra em extremo de escala suprime somente o report;
  calibração indisponível é modo degradado, não falha; falha de configuração do
  ADC não impede `Running`, como desvio arquitetural explícito;
- o report de bateria não integra a evidência de admissão de deep sleep, o que
  preserva `Client-Deep-Sleep.md` sem emenda;
- ADR-0005 fixa o modelo de identidade, e a v0.3 retira o tipo de evento da
  camada de parâmetros do produto;
- nenhum artefato de teste integra o recorte.

### Relações e limites

A fonte é nova, altera de forma delimitada `Firmware-Variants-Menuconfig.md`
pela composição do produto a bateria, apoia-se em ADR-0001 e ADR-0002 e depende
do comportamento de boot operacional de `Client-Deep-Sleep.md@v0.11`, sem
emendá-la. Preserva bootstrap, componentes reutilizáveis e identidade de report.

### Lacunas e débitos

- a análise da v0.1 classificou **Não pronta — defeito da especificação**; os
  oito achados foram confrontados e resolvidos na v0.2 e na v0.3;
- a análise da v0.3, executada pelo piloto automatizado sobre o commit
  `333adcf`, repetiu a classificação **Não pronta — defeito da especificação**
  com três bloqueadores: ADR-0005 ainda `Proposed`, ausência de autoridade
  normativa para os valores concretos do produto e do board, e fronteira não
  decidida para os tipos de ADC na fachada pública;
- `EKOM-DEBT-0004` registra a observabilidade ausente no host para capability
  inerte por falha de ADC e para valor aproximado no modo degradado.

### Resolução dos bloqueadores na v0.4

O Arquiteto aceitou a `ADR-0005` em 14/08/2026; tornou a seção 8 normativa para
a composição do primeiro produto e do seu board, preservando as seções 4 a 7 sem
valores; e decidiu, sob o critério de reavaliação da ADR-0001, que tipos de
driver do ESP-IDF podem atravessar a fachada pública, com a decisão registrada
como nota naquela ADR.

### Encerramento

O Arquiteto declarou que a implementação atendeu aos requisitos e que os testes
executados em hardware foram aceitáveis. A v0.5 foi encerrada como Concluída
[`Done`]; riscos residuais e débitos permanecem registrados sem bloquear esta
entrega. Mudança posterior exige novo recorte e decisão própria.

### Resultado da implementação v0.5

A capability foi implementada no behavior reutilizável, na fachada, no product
firmware e no board model, com evento fixo 3, endpoint 2, fallback sem
calibração, invariantes e gatilhos contratados. Os builds canônicos ESP32-H2
de `door_sensor_battery_h2` e da variante preservada `single_smart_plug`
terminaram com código 0. O Implementador não executou teste, flash, monitor ou
hardware naquela atuação, conforme o relatório em
`docs/reports/client-battery-level/implementation/2026-08-15T161416Z-v0.5-implementation.md`.
Posteriormente, a revisão não encontrou defeito material, o Arquiteto executou
testes em hardware e declarou seus resultados aceitáveis. O encerramento está
registrado em
`docs/reports/client-battery-level/validation/2026-08-15T164510Z-v0.5-7ce6c31-hardware-validation-and-closure.md`.

## EKOM-CHG-0005 — Adoção do EKOM 4.4 e registro de débitos técnicos

**Estado:** Fechada [`Closed`]

**Especificação relacionada:** Não aplicável; governança documental

**Objetivo:** adotar o EKOM 4.4 nas fontes locais e instituir o registro de
débito técnico previsto pela `ADR-0013` do modelo externo.

### Decisões relacionadas

- `AGENTS.md`, a diretriz local e o mapa passam a declarar EKOM 4.4;
- débito técnico é condição conhecida cuja correção o Arquiteto postergou
  conscientemente, com gatilho ou critério de quitação, e não se confunde com
  lacuna de conhecimento, defeito, desvio ou risco residual;
- o mapa recebe a seção de débitos como registro canônico, com namespace
  `EKOM-DEBT-NNNN`;
- nenhum agente aceita débito, quitação ou substituição por autoridade própria.

### Débitos aceitos

- `EKOM-DEBT-0001` — capabilities existentes recebem tipo de evento do produto e
  a unicidade validada é do par, divergindo do modelo da ADR-0005;
- `EKOM-DEBT-0002` — `ISSP-Configurable-Bootstrap.md` não aponta para a ADR que
  estende sua semântica;
- `EKOM-DEBT-0003` — a tabela de tipos de evento em código do coordenador não
  tem guarda que a confronte com a ADR;
- `EKOM-DEBT-0004` — o host não distingue capability ausente por falha nem valor
  aproximado de calibrado.

### Resultado

As fontes locais foram reconciliadas com o EKOM 4.4 e os quatro débitos foram
aceitos pelo Arquiteto em 14/08/2026, com condição, alcance, evidência,
consequência, gatilho e relações registrados no mapa. A aceitação não torna
conforme nenhuma das condições registradas nem altera evidência.

## EKOM-CHG-0006 — Features configuráveis do client no SDK Configuration Editor

**Estado:** Fechada [`Closed`] — v0.1 Concluída [`Done`] por decisão do
Arquiteto em 18/08/2026

**Especificação relacionada:**
`docs/specs/Client-SDK-Configurable-Features.md`
(`EKOM-CLIENT-CONFIG-001`)

**Objetivo:** tornar configuráveis no build o deep sleep, a janela acordada, o
intervalo de despertar, a capability de bateria, seu intervalo sem deep sleep e
o GPIO de factory reset, preservando os valores atuais como defaults.

### Decisões relacionadas

- deep sleep e bateria recebem checkboxes independentes para o produto
  aplicável;
- o menu do projeto recebe o rótulo `App Client`, com as escolhas de produto e
  board seguidas pelos grupos `Firmware features` e `Board configuration`;
- janela acordada e despertar periódico têm defaults de 30 segundos e 15
  minutos e só são editáveis com deep sleep habilitado;
- bateria sem deep sleep mede periodicamente, com default de 2 horas, somente
  depois de um intervalo completo contado desde `Running`;
- GPIO de factory reset passa a parametrizar o board selecionado, com default 9
  e emenda estreita da ADR-0002;
- `client_154/sdkconfig` permanece inalterado e sua divergência é aceita como
  `EKOM-DEBT-0005`;
- nenhum teste automatizado integra o recorte da versão 0.1.

### Relações e limites

A proposta altera de forma explícita as especificações de variantes, deep
sleep e bateria e preserva bootstrap, arquitetura de componentes, identidade de
capability, protocolo, coordenador e host. Símbolos da configuração permanecem
no limite `client_154/main`.

### Estado

O Arquiteto aprovou o rascunho funcional em 15/08/2026. A análise de
implementabilidade da revisão `401c5f9f865d3ee093fe8e79529ad975690a73d2` foi
classificada como `Ready`, e o Arquiteto autorizou a implementação de
`EKOM-CLIENT-CONFIG-001` na branch `spec/client-sdk-configurable-features`.
A implementação foi recuperada da execução `32091116616`, corrigida após
revisão e validada de forma proporcional; testes, flash, monitor e hardware
permanecem não executados.

O build H2 da composição versionada concluiu com `Project build complete` e
código 0 usando ESP-IDF 6.0.1. Também concluíram builds das variantes sem deep
sleep, sem bateria e com tempos e GPIO alternativos. Colisões do GPIO de
factory reset com contato seco, wake LED e medição de bateria foram rejeitadas
no build antes da geração do binário.

### Encerramento

Em 18/08/2026, o Arquiteto confirmou que a hierarquia `App Client` apareceu no
SDK Configuration Editor no local esperado, considerou suficientes as
evidências registradas e determinou o encerramento da v0.1 como Concluída
[`Done`]. Testes, flash, monitor e hardware permanecem `Not Executed` e não são
representados como evidência de sucesso. Nova necessidade ou evidência material
exige decisão de reabertura.

## EKOM-CHG-0007 — Adoção do EKOM 4.5

**Estado:** Fechada [`Closed`]

**Especificação relacionada:** Não aplicável; governança documental

**Objetivo:** adotar o EKOM 4.5 nas fontes locais e promover a branch corrente
para `main` por ordem explícita do Arquiteto.

### Decisões relacionadas

- `AGENTS.md`, a diretriz local e o mapa passam a declarar EKOM 4.5;
- prontidão de implementação usa suficiência, autoridade normativa limitada e
  controles contra omissão conforme a ADR-0014 do modelo externo;
- relatórios históricos permanecem vinculados à versão usada em sua execução e
  não são reescritos;
- a Action de implementação deriva da branch a especificação, o ID e a análise
  formal mais recente aplicável, bloqueando `Not Ready` posterior e `Ready`
  obsoleto.

### Resultado

As fontes locais foram reconciliadas com o EKOM 4.5 em 16/08/2026. A branch
`spec/client-sdk-configurable-features` foi autorizada pelo Arquiteto para
integração em `main`, sem executar Actions, builds, testes, flash, monitor ou
hardware nesta atuação.

## EKOM-CHG-0008 — Adoção do EKOM 4.6

**Estado:** Em andamento [`In Progress`]

**Especificação relacionada:** Não aplicável; governança documental

**Objetivo:** adotar o EKOM 4.6 nas fontes locais de governança.

### Decisões relacionadas

- `AGENTS.md`, a diretriz local e o mapa passam a declarar EKOM 4.6;
- a Autoria de especificação nova ou revisão material passa a usar
  investigação dirigida, rascunho funcional reconciliado com o Arquiteto e
  ordem explícita antes da escrita normativa, conforme o perfil externo do
  Autor da Especificação 3.3 e a DD-045;
- as regras de implementabilidade, implementação e revisão herdadas da 4.5
  permanecem vigentes;
- relatórios e transações históricos permanecem vinculados à versão usada em
  sua execução e não são reescritos.

### Resultado

As fontes locais de governança foram reconciliadas com o EKOM 4.6 em
18/08/2026. A alteração é exclusivamente documental; nenhum código, teste,
dependência, build, Action, flash, monitor ou hardware foi alterado ou
executado. O encerramento da transação permanece reservado ao Arquiteto.

## EKOM-CHG-0009 — Remediação dos débitos técnicos aceitos

**Estado:** Em andamento [`In Progress`]

**Especificação relacionada:** `docs/specs/Technical-Debt-Remediation.md`
(`EKOM-DEBT-REMEDIATION-001`)

**Objetivo:** contratar a remediação de `EKOM-DEBT-0001` a `EKOM-DEBT-0005` em
recorte único, determinado pelo Arquiteto.

### Decisões relacionadas

- recorte único cobrindo os cinco débitos aceitos;
- `eventType` sai das configurações públicas das capabilities existentes, a
  fachada injeta o tipo e a unicidade passa a ser por endpoint, com os endpoints
  vigentes congelados;
- `smart_sys_app_test` integra explicitamente o recorte, com dois grupos de
  casos vinculados a critérios de aceite;
- `ISSP-Configurable-Bootstrap.md` passa a apontar a ADR-0005 como origem da
  extensão de sua semântica;
- a tabela de tipos de evento do coordenador ganha guarda em tempo de build
  contra divergência do registro da ADR-0005;
- a observabilidade da telemetria de bateria usa o tipo de evento 4, alocado por
  emenda da ADR-0005 aceita pelo Arquiteto em 18/08/2026, em capability própria
  no endpoint 3, com valores 0 calibrado, 1 aproximado e 2 inerte, sem ampliar o
  domínio do evento 3; a reserva permanece global quando a bateria está
  desabilitada;
- `client_154/sdkconfig` é alinhado à baseline da decisão A9, preservando
  `Firmware-Variants-Menuconfig.md`.

### Relações e limites

A proposta altera o bootstrap, a especificação de bateria e a de features
configuráveis, e depende da alocação do tipo 4 já aceita na ADR-0005. Preserva
layout wire, `EKM-GAP-0002`, commissioning, registry, ACK, retry,
identidade de report e as fronteiras entre `client_154` e `coordinator_154`.

A Autoria registrou a ressalva de que o bloco de observabilidade atravessa
client, wire, coordenador e host sob lacuna de protocolo ainda `Partial`. A
análise da v0.1 não identificou pré-requisito arquitetural e bloqueou somente a
alocação normativa então pendente. O Arquiteto determinou a inclusão no recorte
único e aceitou a emenda que remove essa pendência.

### Estado

Especificação v0.2 registrada em Rascunho [`Draft`] em 18/08/2026. A análise da
v0.2 concluiu `Ready` e a implementação foi autorizada pelo Arquiteto em
19/08/2026. A implementação permanece `In Progress`; nenhum débito foi
quitado e a quitação permanece reservada ao Arquiteto.
