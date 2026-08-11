# Relatório de validação — política de targets e testes

**Classe da fonte:** Relatório

**Papel:** Arquiteto humano

**Especificação:** `docs/specs/Repository-Test-Execution-Policy.md`

**Revisão confrontada:** Registro histórico EKM 1.x preservado na migração para EKOM 3.2

**Estado:** Concluído

> Este relatório preserva uma atuação histórica e não altera fontes normativas.

## 13. Decisão arquitetural de aceite e encerramento (01/08/2026)

O Arquiteto, como autoridade final sobre risco, aceite e encerramento, decidiu
aceitar a implementação entregue e encerrar esta especificação e
`EKM-CHG-0009` no estado observado.

O significado exato da decisão é:

- a política normativa passa a `Active` e continua proibindo QEMU como
  estratégia vigente de validação ou execução de testes;
- a implementação é `Accepted by Architect`; esse estado registra aceite
  humano do resultado e do risco residual, não promoção técnica para
  `Implemented` ou `Validated`;
- TESTEXEC-AC-005 permanece factualmente `Not Executed`: os 20 casos
  SmartSysApp e 13 casos Registry não foram coletados nem executados em
  ESP32-C3 físico nesta mudança;
- a ausência de `pytest` e dos plugins ESP-IDF no ambiente observado permanece
  limitação conhecida;
- a divergência documental entre dezenove casos normativos e 20 casos
  preservados na fonte e no runner permanece registrada e aceita; ela não
  representa redução de cobertura;
- o encerramento desta política não promove nem encerra as especificações de
  Bootstrap ou Registry, que conservam seus próprios estados e gates.

Prontidão técnica permanece `Not Ready` por ausência da evidência terminal,
mas não há nova etapa obrigatória desta política: eventual instalação do
runner ou execução física será uma atuação futura somente mediante nova ordem
do Arquiteto. A especificação e a transação estão encerradas por decisão
humana explícita, com as limitações acima preservadas para auditabilidade.

## 15. Resoluções arquiteturais da v0.3 (10/08/2026)

O Arquiteto considerou procedentes B1 a B3 e decidiu:

- **B1 resolvido:** esta autoria pode corrigir os gates vigentes de
  `ISSP-Configurable-Bootstrap.md` e
  `ISSP-Coordinator-Paired-Device-Registry.md`. O app Unity de `SmartSysApp`
  usa ESP32-H2 e o app Unity do registry usa ESP32-C6; nenhum gate pode oferecer
  target alternativo fora desse vínculo;
- **B2 resolvido:** Linux e toolchains host-native permanecem permitidos para
  lógica pura e não constituem `IDF_TARGET`. TESTEXEC-008 governa somente
  firmware e test apps ESP-IDF;
- **B3 resolvido:** a allowlist do repositório e o vínculo por alvo são regras
  simultâneas. Apenas o `sdkconfig` principal de cada projeto é configuração
  autoritativa; cópias `.old`, cópias sufixadas por target e o `sdkconfig.ci`
  vazio da raiz são artefatos removíveis. O projeto diagnóstico da raiz fica
  vinculado ao ESP32-H2 enquanto permanecer no repositório.

Os experimentos E1 a E3 da seção 14 tornam-se validações obrigatórias da
implementação: são builds e prova do guard, não execução comportamental dos
casos. E4 é retirado do encerramento desta correção e permanece deliberadamente
`Not Executed`; somente especificação futura pode voltar a solicitá-lo. Esta
autoria não executa nem antecipa esses experimentos.

A v0.3 permanece `Proposed / Regressed / Not Ready` até confronto focado das
resoluções. Nenhum código, runner, configuração ou build foi alterado.

## 17. Decisão arquitetural posterior ao confronto (10/08/2026)

O Arquiteto considerou suficiente o confronto da seção 16 e promoveu a v0.3
para `Implementable / Ready`. A promoção autoriza uma ordem separada de
implementação; não declara a correção implementada e não autoriza coletar,
gravar ou executar as suítes preservadas.

As imprecisões não bloqueadoras foram resolvidas no texto vigente:

- host-native atual usa apenas toolchain de host puro, sem `IDF_TARGET`; a
  correção v0.4 retirou dos projetos atuais a exceção para o target `linux` do
  ESP-IDF;
- AC-004 cobre todos os artefatos da seção 7.2, não apenas os originados por
  QEMU;
- AC-005 é verificado documentalmente por contagem das fontes e inspeção dos
  oráculos, sem execução dos casos.

E1, E2, E3 e E5 são evidências obrigatórias da implementação:

- E1 e E2 compilam os test apps respectivamente em ESP32-H2 e ESP32-C6, sem
  iniciar seus casos;
- E3 deve comprovar o guard de allowlist no projeto da raiz, `client_154`,
  `coordinator_154`, `examples/issp_minimal_client` e nos dois test apps;
- E5 deve comparar cada `sdkconfig` autoritativo com toda cópia sufixada ou
  `.old` antes da remoção. Cada diferença é classificada como intencional,
  gerada, default ou obsoleta. Configuração intencional ainda necessária deve
  ser reconciliada no arquivo autoritativo; dúvida material interrompe a
  remoção e retorna à autoria.

E4 permanece fora do encerramento. Somente especificação futura pode autorizar
execução comportamental, pytest, flash ou monitor das suítes.
