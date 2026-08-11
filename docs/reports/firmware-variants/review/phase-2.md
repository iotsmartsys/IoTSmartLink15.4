# Relatório de revisão — variantes de firmware

**Classe da fonte:** Relatório

**Papel:** Engenheiro Revisor

**Especificação:** `docs/specs/Firmware-Variants-Menuconfig.md`

**Revisão confrontada:** Registro histórico EKM 1.x preservado na migração para EKOM 3.2

**Estado:** Concluído

> Este relatório preserva uma atuação histórica e não altera fontes normativas.

## Revisão independente da Fase 2 (Engenheiro Revisor)

Challenge do commit `a6028d3` contra este recorte, com foco na Fase 2: seleção
por `menuconfig`, fronteiras entre produto e board, compatibilidade de recursos,
Door Sensor Battery H2, debounce, ciclo de vida do `esp_timer`, registro
unificado do `SmartSysApp`, concorrência dos pending reports e preservação da
tomada.

Esta seção é consultiva. Não altera implementação, não promove estado, não
aprova nem reprova o workflow; somente o Arquiteto decide relevância,
suficiência das evidências e conclusão.

### Escopo e limitações desta revisão

Leitura estática do commit contra a especificação. Nenhuma suíte, QEMU, flash,
monitor ou hardware foi executado, conforme a ordem recebida e
`Repository-Test-Execution-Policy.md`. Nenhum build foi reexecutado: as
evidências de compilação registradas pelo Implementador não foram confirmadas
de forma independente por esta atuação.

**Independência:** o revisor é um agente de mesma capacidade e mesmo contexto
de repositório que as atuações anteriores desta especificação, sem participação
nesta sessão na autoria ou na implementação revisadas. A semelhança de vieses
entre agentes é declarada como limitação da independência.

### Achados

| # | Achado | Severidade | Natureza |
|---|---|---|---|
| A1 | `begin()` converte oscilação inicial em falha terminal do boot | Média | lacuna normativa + impacto operacional |
| A2 | ausência da instrumentação de latência recomendada na decisão 27 | Média | evidência exigida não produzível |
| A3 | o caso de cadência de 10 ms não existe como teste próprio | Média-baixa | cobertura ausente contra O1 e E2 |
| A4 | `skip_unhandled_events = true` desacopla janela e tempo real | Baixa | risco a observar em E2 |
| A5 | `publishReport()` passa a consumir sequência em falha de codificação | Baixa | mudança de comportamento em código morto |
| A6 | slots do sensor não foram minimizados | Baixa | desvio da variável experimental |
| A7 | `state()` do capability lido fora de sincronização | Baixa | risco teórico de concorrência |
| A8 | força insuficiente de dois casos automatizados | Baixa | oráculo não discriminante |
| A9 | `client_154/sdkconfig` rastreado deixou de representar a tomada | Baixa | informativo |

**A1 — `begin()` converte oscilação inicial em falha terminal do boot.**
`beginTimerBacked()` amostra exatamente `samplesPerWindow × consecutiveWindows`
— dez amostras, duas janelas — e, se as duas classificações divergirem, retorna
`Failed` sem confirmar estado. A falha propaga por `begin()` para o estágio
`StartDevice`, `setup()` aborta e o entrypoint apenas registra
`ISSP runtime did not start`. Não há reintento: uma única oscilação nas duas
primeiras janelas impede o dispositivo de alcançar `Running` até reboot manual.

O recorte exige "report inicial estabilizado e publicado sincronamente por
`begin()`" e a decisão 29 exige o mesmo classificador de duas janelas, mas
nenhum dos dois define o resultado quando a estabilização não converge nesse
orçamento. A implementação escolheu a leitura mais estrita, dentro do que a
redação permite. Como a consequência é o produto não iniciar, a escolha merece
ser normativa e não local. Recomendação ao Arquiteto: decidir entre estender a
amostragem inicial até a confirmação com teto explícito, ou ratificar a falha
terminal na especificação. Falha de publicação do report inicial permanece
caso distinto, já alinhado ao precedente do `DigitalOutputBehavior`.

**A2 — ausência da instrumentação de latência recomendada na decisão 27.**
A análise recomendou que o behavior registrasse dois instantes — primeira
amostra divergente e confirmação — para que a evidência de hardware fosse
legível como `confirmação − primeira amostra divergente + 10 ms`. O código emite
apenas `transition_report endpoint=… event=… value=…`, sem instante nem marca da
primeira divergência. O item de hardware "latência máxima de 150 ms medida até a
confirmação pelo behavior" não é produzível a partir do log atual. A análise
classificou instrumentação como escolha normal de implementação; o achado é
sobre a evidência exigida, não sobre contrato.

**A3 — o caso de cadência de 10 ms não existe como teste próprio.**
O1 separa deliberadamente dois propósitos: sequência determinística sem timer e
cadência com timer real medida por `esp_timer_get_time()`. O conjunto de
validação lista "período de 10 ms" como item próprio. Dos oito casos de
`DigitalInputBehavior`, seis usam a junção sem timer e dois armam o timer real,
mas ambos medem latência e ausência de callback posterior, nunca o espaçamento
entre amostras: o caso de latência passaria com período de 12 ms. E2 permanece
sem cobertura automatizada do que se propôs a medir.

**A4 — `skip_unhandled_events = true` desacopla janela e tempo real.**
Sob atraso da tarefa `esp_timer`, eventos são descartados em vez de repostos.
Como o oráculo normativo é a sequência de classificações (decisão 28), não há
defeito de contrato; mas uma janela de cinco amostras pode ocupar mais que 50 ms
de tempo real, esticando silenciosamente o pior caso aritmético de 140 ms contra
o teto de 150 ms. Dada a folga já reconhecida em R8, vale medir junto de E2 em
vez de assumir cadência ideal.

**A5 — `publishReport()` passa a consumir sequência em falha de codificação.**
Para tornar a reserva atômica, `reportSequence_` é lido e incrementado antes de
`encodeReport()`; na falha, a sequência é perdida, criando lacuna na numeração
onde antes não havia. `publishReport()` não possui chamador em produção — a
análise já o classificara como código morto —, então o impacto material hoje é
nulo. Registrado porque é a única mudança de comportamento observável fora do
bookkeeping, num recorte que declara não alterar protocolo. `preparePendingReport()`
não tem o mesmo problema: libera a reserva por `completePendingReport(token, false)`,
como a decisão 31 exige.

**A6 — slots do sensor não foram minimizados.**
`kImplStorageBytes` não foi ampliado e continua guardado por `static_assert`,
o que atende à parte central da variável experimental. Porém `Impl` reserva
`kMaxCapabilities` = 8 slots de `DoorSensorConfig`, `DigitalInputBehavior` e
`DoorSensorCapability` além dos oito da tomada, embora um binário componha
exatamente um produto. É reserva preventiva contra a letra da variável, sem
consequência funcional enquanto o `static_assert` passar. Observação menor no
mesmo trecho: em `behaviorCount_ >= kMaxCapabilities || switchCount_ >= kMaxCapabilities`,
a segunda condição é inalcançável, porque `behaviorCount_` já soma os dois tipos.

**A7 — `state()` do capability lido fora de sincronização.**
`confirmedState_` e `hasConfirmedState_` são escritos pela tarefa `esp_timer` e
lidos por `DoorSensorCapability::state()` a partir de qualquer tarefa, sem
sincronização nem tipo atômico. Em ESP32-H2 single-core, com `bool`, a leitura é
indivisível na prática. As decisões 22 e 31 cobriram apenas `IsspDevice`; o
estado do behavior não foi confrontado por elas. Risco teórico, registrado para
não ficar invisível caso a superfície pública do capability cresça.

**A8 — força insuficiente de dois casos automatizados.**
O caso "setup registers switch and door capabilities in addition order" afirma
duas chamadas de registro e sua posição na sequência de estágios, mas o hook
recebe somente um índice: nada no teste distingue o registro da tomada antes do
sensor. A ordem de adição exigida pela decisão 24 permanece comprovada apenas
por inspeção do código. O caso "addDoorSensorCapability validates pin and
debounce" injeta pino inválido e `majorityThreshold = 6` simultaneamente; uma
única regra de validação já satisfaz a asserção, de modo que o caso não separa
as duas. Ambos são casos preparados e ainda não executados; corrigir o oráculo
antes da execução custa menos que reinterpretar o resultado depois.

**A9 — `client_154/sdkconfig` rastreado deixou de representar a tomada.**
O commit troca a configuração rastreada para `Door sensor` + `Door Sensor
Battery H2`. Os defaults do `Kconfig` continuam sendo a tomada e o board atual,
então o critério de aceite se sustenta na letra. Porém a manutenção pré-Fase 2
registrou `client_154/sdkconfig` como a configuração H2 vigente, e agora um
`idf.py build` sem reconfiguração produz o sensor. A seção de resultado da Fase 2
atribui a escolha ao Arquiteto; o registro existe para que a validação de
preservação da tomada não seja lida como executada sobre a configuração
rastreada.

### Pontos confrontados sem achado material

- **seleção por `menuconfig`:** `choice` exclusivo com dois produtos e dois
  boards; ambos os boards com `depends on IDF_TARGET_ESP32H2`; defaults
  preservados no `Kconfig`; nenhuma lógica funcional no menu;
- **compatibilidade por recursos:** `required_resources` e `offered_resources`
  por ramo de seleção no CMake, com `FATAL_ERROR` nomeando produto, board e
  recurso ausente, conforme a decisão 16; o diagnóstico de target passou a
  nomear os dois boards, resolvendo R7;
- **fronteiras:** nenhum símbolo `CONFIG_IOTSMARTLINK154_*` em `components/` ou
  `examples/`; o product firmware não contém GPIO literal; o board não contém
  regra de produto; `#error` defensivo no board novo; o contrato por acessador
  da decisão 26 é efetivo, pois a tomada não define `selectedDryContactInput()`
  e o sensor não define `selectedDigitalOutput()`;
- **Door Sensor Battery H2:** contato seco no GPIO 14 ativo alto com pull-up
  interno, botão de usuário no GPIO 9 ativo baixo, compatível somente com
  ESP32-H2 — conforme o recorte;
- **debounce:** janelas não sobrepostas de cinco amostras, maioria de três,
  confirmação por duas classificações consecutivas, supressão de duplicata e
  não-confirmação quando a publicação falha, todos implementados como
  especificado; as sequências dos testes (`{1,1,0,1,0}` e `{0,0,1,0,1}`) são
  oráculos declarados por amostra, conforme a decisão 28;
- **ciclo de vida do `esp_timer`:** despacho `ESP_TIMER_TASK`; `stop` e `delete`
  no destrutor; desfazimento em todos os caminhos de falha posteriores à
  criação; `pdMS_TO_TICKS()` com guarda contra período de zero tick, exatamente
  a precaução pedida na decisão 29;
- **registro unificado:** `behaviors_` e `endpointEventPairs_` preenchidos na
  ordem de adição, duplicata de par endpoint/evento rejeitada também entre tipos,
  `realRegisterCapability()` migrado para o vetor unificado, `SetupHooks`
  inalterado e o log de capabilities passando a contar `behaviorCount_` — que
  continua `1` para a tomada, preservando a leitura de R6;
- **concorrência dos pending reports:** `mutable portMUX_TYPE` conforme o
  precedente `Issp154Transport::ackLock_`; todos os caminhos de retorno de
  `publishState()`, `peekPendingReport()` e `completePendingReport()` liberam a
  seção; codificação, callback, notificação e transporte ficam fora dela; o
  handler é copiado sob lock antes de ser chamado; a liberação por falha de
  codificação reentra na seção sem aninhá-la; `xTaskNotifyGive()` é seguro a
  partir da tarefa `esp_timer`;
- **preservação da tomada:** a migração de `single_smart_plug.cpp` é mecânica e
  todos os valores da decisão 8 permanecem — GPIO 13 ativo alto, botão GPIO 9
  ativo baixo com 10 s e polling de 20 ms, endpoint 1, event type 2, estado
  inicial desligado e report inicial habilitado;
- **novos test apps:** `ISSP_TARGET_BINDING esp32h2` declarado entre
  `project.cmake` e `project()`, `MINIMAL_BUILD ON` e `sdkconfig.defaults` em H2,
  idênticos ao precedente `smart_sys_app_test` e conformes a TESTEXEC-008.

Nenhum achado exige reabrir as decisões 15 a 31, alterar protocolo, transporte,
commissioning ou o coordenador, nem introduziu condicional de produto dentro dos
componentes compartilhados. O teste 3 da estratégia EKOM está estruturalmente
sustentado pelo que foi inspecionado.

### Recomendação de prontidão

**Não recomendo tratar a Fase 2 como pronta para conclusão.** A implementação
estrutural está conforme o recorte e nenhum bloqueador arquitetural foi
encontrado; a recomendação decorre de três pontos, e a decisão é do Arquiteto:

1. **A1 requer decisão normativa** antes da validação em hardware, porque
   determina se um boot com entrada instável é falha aceita ou defeito;
2. **A2 e A3 impedem produzir duas evidências que o próprio conjunto de
   validação da Fase 2 exige** — a latência medida em hardware e o período de
   10 ms verificado por teste próprio;
3. **A8 recomenda ajustar o oráculo de dois casos antes de executá-los**, para
   que a execução pendente produza evidência discriminante.

Os demais achados são registros para não regredirem na leitura das evidências e
não condicionam a continuidade. A execução das suítes e da validação em
hardware permanece pendente e fora desta atuação; ausência de achados
adicionais não é prova de correção.
