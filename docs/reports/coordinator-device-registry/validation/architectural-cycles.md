# Relatório de validação — decisões do registry do coordenador

**Classe da fonte:** Relatório

**Papel:** Arquiteto humano

**Especificação:** `docs/specs/ISSP-Coordinator-Paired-Device-Registry.md`

**Revisão confrontada:** Registro histórico EKM 1.x preservado na migração para EKOM 3.2

**Estado:** Concluído

> Este relatório preserva uma atuação histórica e não altera fontes normativas.

## Estado registrado no início do ciclo legado

A versão v0.4 preservava a implementação funcional parcial e havia recebido a
migração técnica do runner físico registrada pela política transversal. Naquele
momento, os estados registrados eram `Proposed`, `In Progress`, migração de
validação `Regressed`, `Not Ready` e recorte funcional `Implementable`, com a
correção de target ainda `Pending Review`. Esses estados foram posteriormente
superados pela correção de target; não governam a situação vigente.

### 16.4 Encerramento da revisão e retorno à autoria (01/08/2026)

O Arquiteto recebeu os achados da seção 16.3 e determinou uma nova rodada de
autoria antes de qualquer correção de implementação. O objetivo declarado é
preservar o escopo funcional completo e experimentar uma especificação mais
validável, incorporando controles de estados, invariantes, fidelidade dos
substitutos e rastreabilidade entre critérios, testes e evidência.

Esta decisão não aprova os achados, não corrige código e ainda não transforma
as medidas discutidas em requisitos normativos. Ela encerra a atuação atual do
Engenheiro Revisor e devolve formalmente o documento à autoria:

- estado normativo: `Draft`, pois o conteúdo voltará a ser elaborado;
- estado da implementação: `In Progress`, pois existe implementação parcial
  com defeitos e lacunas de evidência registrados;
- prontidão: `Not Ready`;
- revisão de implementabilidade: `Pending Review`, pois a conclusão histórica
  da seção 16.1 não poderá validar antecipadamente o conteúdo revisado;
- transação `EKM-CHG-0008`: `Open`.

Os achados e evidências da seção 16.3 permanecem como entrada factual para a
próxima autoria. A versão revisada deverá passar por nova análise independente
de implementabilidade antes de autorizar outra atuação de implementação.

### 16.5 Reautoria v0.2 (Autor da Especificação, 01/08/2026)

O Arquiteto ordenou revisão integral da especificação, preservando o escopo
funcional completo e os treze requisitos. Esta versão:

- separa o baseline anterior, a implementação existente e o contrato
  normativo;
- define precedência entre validade do frame, disponibilidade do registry,
  janela, identidade e tipo de mensagem;
- acrescenta matriz de decisão para discovery, `DATA`, `ACK` e comandos;
- inclui falhas de inicialização NVS na proibição de apagamento global;
- explicita staging, durable, commit e reboot do contrato persistente;
- autoriza somente abstrações locais necessárias para testar a política usada
  pelo runtime e o adaptador NVS;
- preserva AC-001 a AC-008, mas torna seus gates completos e distingue
  evidência parcial de aprovação;
- define G1 a G5, fidelidade obrigatória dos substitutos, manifesto de
  evidências, verificação ambiental e varreduras de conformidade.

Nenhum código ou teste funcional foi alterado ou executado nesta autoria. A
implementação existente permanece `In Progress`; a proposta v0.2 fica
`Proposed`, `Not Ready` e `Pending Review`. A próxima etapa é uma nova análise
independente de implementabilidade; a conclusão histórica da versão 0.1 não é
reutilizável como aprovação desta versão.

### 16.7 Encerramento corretivo da análise e retorno à autoria (01/08/2026)

Após revisão adversarial do resultado da seção 16.6, o Engenheiro Analista
identificou duas lacunas que impedem afirmar que todos os requisitos da versão
0.2 são plenamente validáveis:

1. a seção 6 torna opcionais marcador, checksum ou metadados equivalentes de
   integridade, enquanto a tabela da seção 10 e AC-007 exigem produzir e
   reprovar checksum ou marcador inválido. O Autor deve tornar inequívoco se o
   schema exige ao menos um mecanismo verificável de integridade e alinhar o
   oráculo de corrupção à decisão;
2. AC-002 exige falhas de `set_blob` e commit somente em G1+G2, que podem usar
   substituto fiel. G3 exercita o adaptador NVS de produção apenas nos cenários
   atualmente enumerados para AC-001/AC-007. Assim, um adaptador real que não
   propague corretamente uma falha de `nvs_commit()` pode satisfazer os gates
   existentes. O Autor deve exigir evidência que atravesse o adaptador
   de produção sob falha de commit, ou definir gate material equivalente que
   feche essa possibilidade.

Esses bloqueios não reduzem COORD-REG-001 a 013, AC-001 a AC-008 nem o escopo
funcional. Eles também não rejeitam a arquitetura local proposta. Exigem apenas
que contrato e gates sejam ajustados antes de nova promoção.

**Resultado corrente:** `Needs Clarification`.

A promoção `Implementable` da seção 16.6 fica sem efeito para a versão corrente
e é preservada somente como registro histórico. Estados mantidos: normativo
`Proposed`, implementação `In Progress`, prontidão `Not Ready` e transação
`EKM-CHG-0008` `Open`. A etapa de análise está encerrada e a especificação é
devolvida ao Autor. Nenhum código, teste ou configuração de implementação foi
alterado ou executado nesta correção.

### 16.9 Reautoria v0.3 (Autor da Especificação, 01/08/2026)

O Arquiteto ordenou aplicar os ajustes devolvidos pelas análises corretivas da
versão 0.2. Esta autoria preserva COORD-REG-001 a 013, AC-001 a AC-008 e todo o
escopo funcional, e resolve os dois bloqueios:

1. a seção 6 passa a exigir um valor de integridade definido pelo schema,
   calculado sobre `schema_version`, `entry_count` e todos os bytes das
   entradas. Marcador constante isolado não satisfaz o contrato; ausência,
   truncamento ou divergência levam a `RegistryUnavailable` e possuem oráculo
   executável em AC-007;
2. AC-002 passa a exigir G3-F além de G1+G2. G3-F compila e executa o próprio
   `device_registry_nvs.c`, injeta erro somente na fronteira de
   `nvs_commit()` depois de `nvs_set_blob()` bem-sucedido e observa propagação
   do erro, ausência de resposta/publicação, restauração do durable anterior e
   preservação da sentinela.

G3 foi explicitado em duas modalidades: G3-N usa o adaptador de produção com
NVS real para caminhos nominais, corrupção e reboot; G3-F usa o mesmo adaptador
sob falha controlada de primitiva. Nenhuma modalidade pode substituir ou
reimplementar a lógica de `device_registry_nvs.c`.

As seções 16.7 e 16.8 permanecem como histórico das análises que originaram
estas correções; não representam o estado corrente. Nenhum código, teste ou
configuração de implementação foi alterado ou executado nesta autoria.

Estado da versão 0.3: normativo `Proposed`, implementação existente
`In Progress`, prontidão `Not Ready`, revisão de implementabilidade
`Pending Review` e `EKM-CHG-0008` `Open`. A próxima etapa é nova análise
independente da versão 0.3.

### 16.13 Reautoria v0.4 — retirada de QEMU (Autor da Especificação, 01/08/2026)

O Arquiteto determinou remover QEMU de todo o repositório como estratégia de
validação ou execução de testes, sem reduzir requisitos funcionais. Esta
autoria preserva COORD-REG-001 a 013, AC-001 a AC-008, todos os cenários,
falhas, oráculos e gates materiais.

As mudanças normativas desta versão são:

- G1 e G2 podem executar host-native quando seus substitutos preservarem toda
  a semântica material; caso contrário executam em placa física;
- G3-N passa a executar o adaptador de produção e a NVS real exclusivamente em
  ESP32-C6 físico;
- G3-F continua atravessando `device_registry_nvs.c` e pode executar
  host-native ou em placa física sob primitivas controladas;
- G4 permanece build da composição ESP32-C6 e G5 permanece hardware real
  ponta a ponta com rádio;
- resultados QEMU das seções históricas continuam auditáveis, mas tornam-se
  evidência legada e não podem aprovar a versão 0.4 nem revisão posterior;
- os test apps existentes são preservados nesta autoria e passam ao inventário
  de migração de `Repository-Test-Execution-Policy.md`; sua alteração ou
  exclusão depende de atuação posterior do Engenheiro Implementador.

Nenhum código, teste, configuração, runner ou artefato técnico foi alterado,
excluído ou executado. A implementação funcional preexistente permanece
`In Progress`; a migração de validação da v0.4 fica `Not Started`. Estados da
versão: `Proposed`, `Not Ready` e `Pending Review`; `EKM-CHG-0008` continua
`Open`. A próxima etapa é análise independente de implementabilidade.

### 16.20 Correção de target da validação v0.4 (10/08/2026)

Por decisão do Arquiteto e conforme
`Repository-Test-Execution-Policy.md` v0.3, o app Unity do registry, G3-N, G4
e G5 executam exclusivamente em ESP32-C6 quando exigirem firmware ou placa
física. G1, G2 e G3-F podem permanecer host-native somente quando preservarem
a semântica material definida nesta especificação; host-native não constitui
`IDF_TARGET` nem evidência de compatibilidade física.

Os 24 casos presentes na fonte permanecem preservados. Builds ou resultados
históricos em target não suportado continuam auditáveis, mas são evidência
inválida para a migração v0.4. Os casos permanecem deliberadamente
`Not Executed`: esta versão não solicita coleta, flash ou execução em ESP32-C6,
que somente uma especificação futura poderá autorizar.

As cláusulas anteriores que descrevem como G1 a G5 seriam executados continuam
definindo os oráculos, mas não constituem ordem vigente de execução. Esta
correção não altera o contrato do registry nem promove os critérios funcionais
ainda pendentes.
