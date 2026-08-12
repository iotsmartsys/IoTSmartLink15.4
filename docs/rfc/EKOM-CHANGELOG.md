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

**Estado:** Rascunho e análise [`Draft`]

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
  estágio com `maxAwakeTimeMs`.

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
- testes e hardware permanecem `Not Executed`.

### Resultado

Contrato v0.10 registrado em rascunho. As precisões devolvidas pela análise da
v0.9 foram incorporadas sem nova decisão normativa: orçamento acordado,
conservadorismo do limite do timer, distinção dos predicados do polling e
segurança de encerramento quando nunca iniciado. O documento está preparado
para nova análise de implementabilidade; implementação ainda depende de
análise, promoção e autorização explícitas do Arquiteto.
