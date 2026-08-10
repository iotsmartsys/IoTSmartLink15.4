# Política de Targets Suportados e Execução de Testes do Repositório

**Tipo:** Normativo
**Estado normativo:** Active
**Estado da implementação:** Validated — implementação aprovada pelo Arquiteto
na seção 18.7; as 44 suítes permanecem `Not Executed` por decisão de
TESTEXEC-009
**Prontidão:** Ready — promoção do Arquiteto registrada na seção 17
**Revisão de implementabilidade:** Implementable — baseline v0.3 confrontada na
seção 16; correção v0.4 autorizada pelo Arquiteto e registrada na seção 18.6
**Versão:** 0.4
**Responsável arquitetural:** Marcelo Miranda
**Última atualização:** 10/08/2026
**Escopo:** Targets admitidos e estratégias de execução de testes em todo o
repositório

---

## 1. Intenção confirmada

O Arquiteto determinou que somente ESP32-H2 e ESP32-C6 sejam tratados como
targets físicos deste repositório. Ambos possuem rádio IEEE 802.15.4; um chip
sem esse rádio não pode ser apresentado como target contemplado apenas porque
um teste substitui hardware por fakes.

Também permanece vigente a decisão de que QEMU não seja usado como estratégia
de validação ou execução de testes. As duas decisões são transversais a
especificações, critérios, gates, documentação, runners e artefatos técnicos.

A remoção da ferramenta não reduz requisito funcional, cenário, falha,
condição de borda ou oráculo. Ela altera somente o ambiente no qual a evidência
executável é produzida.

## 2. Escopo

Inclui:

- matriz autoritativa de targets por alvo do repositório;
- especificações e matrizes que prescrevem execução automatizada;
- test apps ESP-IDF e runners associados;
- documentação operacional e rastreabilidade EKM;
- evidências futuras de implementação, revisão e validação;
- inventário dos artefatos técnicos cuja remoção ou migração caberá a uma
  atuação posterior de Engenheiro Implementador;
- invalidação de evidências produzidas em target não suportado e preservação
  dos casos sem executá-los nesta correção.

## 3. Fora de escopo

- alterar comportamento funcional de produto;
- reduzir critérios de aceite ou converter build/inspeção em teste
  comportamental;
- excluir código, test apps, runners, configurações ou diretórios nesta
  atuação de autoria;
- apagar registros históricos de execuções já realizadas;
- reescrever resultado antigo como se tivesse sido executado em outro target;
- instalar ferramentas, executar testes ou realizar validação em hardware.

## 4. Política normativa

### TESTEXEC-001 — Proibição de QEMU

QEMU não pode ser dependência, target, runner, comando, marcador ou ambiente de
execução de nenhum gate vigente ou evidência futura do repositório.

Referências históricas podem permanecer somente para registrar fatos passados.
Elas devem ser interpretadas como evidência legada, não reutilizável para
aprovar versão, implementação ou revisão posterior a esta política.

### TESTEXEC-002 — Preservação de cobertura

Todo cenário anteriormente atribuído ao emulador deve manter condição inicial,
ação, resultado observável, quantidade de casos e condição terminal. A mudança
de ambiente não autoriza eliminar casos nem classificar como aprovado aquilo
que não foi executado.

### TESTEXEC-003 — Execução host-native

Teste sem dependência material de periférico, scheduler, NVS ou comportamento
específico do target pode ser executado em processo host-native, desde que o
substituto preserve toda a semântica exigida pelo critério. O relatório deve
identificar compilador, runner, casos e código de saída.

Se tornar o teste host-native exigir duplicar a política de produção ou omitir
semântica material, essa estratégia reprova o gate e deve ser substituída por
execução em placa física.

Execução host-native usa projeto e toolchain de host puro, sem `IDF_TARGET` e
sem configurar um projeto de firmware ESP-IDF. Ela é permitida exclusivamente
para lógica pura, não representa firmware, placa ou evidência de compatibilidade
IEEE 802.15.4 e deve preservar toda a semântica material do critério.

O target `linux` do ESP-IDF não é admitido pelos projetos atuais. Sua introdução
futura exige especificação própria, projeto host dedicado, fronteira explícita
contra firmware e validação que justifique seu custo.

### TESTEXEC-004 — Execução em target físico

Teste dependente de ESP-IDF, FreeRTOS, NVS real, periférico ou comportamento do
target deve executar em placa física suportada. O app Unity de `SmartSysApp`
executa em ESP32-H2; o app Unity do registry do coordenador executa em ESP32-C6.
Fakes podem evitar a inicialização do rádio, mas não autorizam selecionar outro
target para o firmware de teste.

Flash, monitor e captura automatizada em placa continuam sujeitos à ordem
explícita do Arquiteto prevista pelo `AGENTS.md`.

Esta versão define o target correto caso uma execução seja futuramente
solicitada; ela não solicita nem autoriza executar as suítes existentes. Aplica-se
também TESTEXEC-009.

### TESTEXEC-005 — Evidência terminal

Uma execução comportamental aprova somente com mais de zero casos, resultado
terminal e oráculo observado. Build comprova apenas compilação e link. Erro de
infraestrutura, placa ausente, execução não iniciada ou resultado desconhecido
permanecem `Not Executed`.

### TESTEXEC-006 — Histórico

Resultados QEMU anteriores permanecem no histórico para auditabilidade. Após
esta política, eles não satisfazem gate obrigatório, não podem promover estado
e devem ser substituídos por nova evidência host-native ou física quando o
critério correspondente voltar a ser confrontado.

Builds ou execuções registrados para target fora da matriz da seção 5
permanecem fatos históricos, mas são evidência inválida para compatibilidade ou
aprovação. Seus critérios retornam a `Not Executed` até reexecução em target
admitido por uma especificação futura; o resultado não pode ser renomeado ou
transportado para outro target.

### TESTEXEC-007 — Remoção técnica posterior

A remoção de imports, markers, runners, comentários, configurações e diretórios
de build pertence a uma atuação posterior de Engenheiro Implementador. Essa
atuação deve preservar os testes ainda úteis, migrando seu runner antes de
excluir qualquer artefato necessário à cobertura.

### TESTEXEC-008 — Allowlist de targets

Somente `esp32h2` e `esp32c6` podem aparecer como `IDF_TARGET` em builds de
firmware ESP-IDF, test apps de firmware, runners físicos ou configurações
versionadas deste repositório. Seleções fora dessa allowlist, inclusive
`IDF_TARGET=linux`, devem falhar na configuração com diagnóstico explícito, sem
gerar binário. Host-native de TESTEXEC-003 não configura projeto ESP-IDF.

Além da allowlist do repositório, cada alvo possui vínculo exclusivo: client,
`SmartSysApp`, exemplo mínimo e projeto diagnóstico da raiz usam ESP32-H2;
coordenador e registry usam ESP32-C6. Estar na allowlist não torna H2 e C6
intercambiáveis entre esses alvos.

Listas genéricas herdadas de templates ESP-IDF, como `supported_targets` ou
`preview_targets`, não constituem política do projeto e devem ser removidas dos
runners e da documentação operacional.

### TESTEXEC-009 — Execução somente por especificação futura

Os 20 casos `SmartSysApp` e os 24 casos do registry permanecem preservados como
ativos de engenharia, mas não devem ser coletados, gravados ou executados nesta
correção. A EKOM não transforma sua execução em etapa automática do workflow.

Somente uma especificação futura, posterior a esta decisão, pode solicitar a
execução de casos, definindo propósito, recorte, ambiente, oráculo, custo aceito
e autoridade para operações em hardware. Até essa solicitação, o estado correto
é `Not Executed`, sem constituir pendência para encerrar a correção de targets.

## 5. Matriz autoritativa de targets e execução

| Alvo ou evidência | Target físico | Estratégia permitida | Semântica obrigatória |
|---|---|---|---|
| `client_154`, `SmartSysApp` e exemplo mínimo do client | ESP32-H2 | build e hardware; lógica pura pode ter teste host-native fiel | composição, estados, falhas, rádio, GPIO e resultado terminal conforme o gate |
| `coordinator_154` e registry | ESP32-C6 | build e hardware; lógica pura pode ter teste host-native fiel | política integrada, NVS, rádio, reboot e resultado terminal conforme o gate |
| projeto diagnóstico da raiz | ESP32-H2 | build e hardware; casos host-native independentes podem permanecer | não representa produto nem amplia a allowlist |
| componentes compartilhados | ESP32-H2 e ESP32-C6 | build nos consumidores reais | nenhuma alegação de suporte além dos dois targets |

Execução host-native é ambiente de teste de lógica, não target de firmware nem
evidência de compatibilidade física. Cada board continua declarando, dentro da
allowlist, com qual target é compatível.

## 6. Critérios de aceite

- **TESTEXEC-AC-001:** nenhuma especificação ou documentação operacional
  vigente prescreve QEMU como estratégia de execução ou aprovação.
- **TESTEXEC-AC-002:** cada gate anteriormente associado a QEMU possui
  substituição explícita host-native ou física, sem perda de cenário ou
  oráculo.
- **TESTEXEC-AC-003:** dependências, imports, markers e runners específicos de
  QEMU estão ausentes do inventário versionado depois da atuação de
  implementação.
- **TESTEXEC-AC-004:** todos os artefatos locais ou gerados da seção 7.2 foram
  removidos sem afetar fontes de teste preservadas; a verificação cobre também
  builds, configurações e caches que não sejam de QEMU.
- **TESTEXEC-AC-005:** os 20 cenários `SmartSysApp` e os 24 cenários do registry
  permanecem rastreados. Nesta correção, a verificação é exclusivamente
  documental: contagem na fonte e inspeção dos oráculos declarados nos runners,
  sem coleta ou execução.
- **TESTEXEC-AC-006:** nenhuma matriz funcional perdeu requisito, cenário,
  falha, condição de borda ou gate por causa da retirada do emulador.
- **TESTEXEC-AC-007:** registros QEMU históricos estão claramente separados
  da evidência vigente e não são agregados como aprovação nova.
- **TESTEXEC-AC-008:** nenhum build ESP-IDF, runner, configuração de firmware ou
  documentação operacional apresenta `IDF_TARGET` fora de `esp32h2` e
  `esp32c6`; host-native atual não usa `IDF_TARGET`, e ocorrências históricas
  restantes estão classificadas como erro e evidência inválida.
- **TESTEXEC-AC-009:** os 20 casos `SmartSysApp` e os 24 casos do registry
  permanecem presentes e identificáveis, sem coleta, flash ou execução nesta
  correção e sem alegação de resultado comportamental novo.
- **TESTEXEC-AC-010:** configurar um test app ou composição com target fora da
  allowlist falha antes do binário e informa os targets admitidos.

## 7. Artefatos técnicos candidatos à remoção ou migração

Nenhum item desta seção é excluído pela autoria.

### 7.1 Versionados

- `README.md` e `pytest_hello_world.py`: remover lista genérica de targets e
  runner herdados do template; preservar somente utilidade comprovada para H2,
  C6 ou host-native fiel;
- `components/issp_app_154/CMakeLists.txt`: rejeitar target fora da allowlist;
- `components/issp_app_154/test_apps/smart_sys_app_test`: preservar os 20 casos,
  migrar runner, `sdkconfig.defaults` e comentários para ESP32-H2;
- `coordinator_154/test_apps/device_registry_test`: preservar os 24 casos e
  migrar runner, `sdkconfig.defaults` e comentários para ESP32-C6;
- comentários em `components/issp_app_154/src/smart_sys_app.cpp`,
  `smart_sys_app_impl.hpp` e nos test apps que apresentam target não suportado;
- `client_154/sdkconfig` é a configuração autoritativa H2 e
  `coordinator_154/sdkconfig` é a configuração autoritativa C6; remover
  `client_154/sdkconfig.esp32c6`, `client_154/sdkconfig.esp32h2`,
  `coordinator_154/sdkconfig.esp32h2`, `coordinator_154/sdkconfig.esp32c6` e
  todas as cópias versionadas `sdkconfig.old` ou `sdkconfig.<target>.old`;
- o `sdkconfig` da raiz permanece autoritativo para o diagnóstico H2;
  `sdkconfig.ci` vazio é resíduo de template e pode ser removido;
- referências operacionais em `components/README.md`, especificações, mapa e
  changelog, distinguindo contrato vigente, erro histórico e evidência inválida.

### 7.2 Locais ou gerados, não versionados

- `components/issp_app_154/test_apps/smart_sys_app_test/build_qemu_c3/`;
- `coordinator_154/test_apps/device_registry_test/build_qemu_c3/`;
- `coordinator_154/test_apps/device_registry_test/build_impl_c3/`;
- `components/issp_app_154/test_apps/smart_sys_app_test/sdkconfig`,
  `sdkconfig.old` e `__pycache__/`;
- imagens `qemu_flash.bin` e `qemu_efuse.bin` contidas nesses diretórios;
- logs temporários de execução QEMU eventualmente mantidos fora do
  repositório.

### 7.3 Ambiente externo

- ferramenta ESP-IDF `qemu-riscv32` e bibliotecas instaladas exclusivamente
  para ela. Sua desinstalação não pertence a este repositório e exige ordem
  separada do Arquiteto.

## 8. Rastreabilidade

| Requisito | Critérios |
|---|---|
| TESTEXEC-001 | AC-001, AC-003, AC-007 |
| TESTEXEC-002 | AC-002, AC-005, AC-006 |
| TESTEXEC-003 | AC-002, AC-005 |
| TESTEXEC-004 | AC-002, AC-005 |
| TESTEXEC-005 | AC-005, AC-007 |
| TESTEXEC-006 | AC-007 |
| TESTEXEC-007 | AC-003, AC-004, AC-006 |
| TESTEXEC-008 | AC-008, AC-009, AC-010 |
| TESTEXEC-009 | AC-005, AC-009 |

## 9. Estado e próxima etapa

Esta versão 0.3 registra a correção arquitetural. B1 a B3 da análise da v0.2
estão resolvidos na seção 15; o confronto focado dessas resoluções está na
seção 16 e devolveu `Implementable`; a promoção do Arquiteto está na seção 17.

A implementação ordenada por essa promoção está registrada na seção 18. Ela
deixou de ser `Regressed`: nenhum ativo versionado vigente materializa target
fora da allowlist, e E1, E2, E3 e E5 foram executadas com resultado terminal. O
estado é `In Progress`, não `Implemented`, porque TESTEXEC-009 mantém
deliberadamente os 44 casos `Not Executed`; a promoção posterior depende de
decisão do Arquiteto, não de trabalho técnico pendente nesta correção.

A análise independente da v0.2 está registrada na seção 14. Seu resultado foi
retorno à autoria; as decisões posteriores da seção 15 substituem as lacunas
normativas identificadas sem apagar o confronto que as motivou.

As seções 10 a 13 preservam o ciclo da versão 0.1 como registro histórico. As
afirmações nelas contidas sobre target físico substituto, builds ou prontidão
foram supersedidas pela versão 0.2 e não governam nova implementação nem
constituem evidência vigente.

## 10. Revisão de implementabilidade (Engenheiro Analista, 01/08/2026)

A versão 0.1 foi confrontada integralmente com os artefatos versionados, os
test apps existentes, as especificações dependentes de Bootstrap e Registry,
o mapa de conhecimento e `EKM-CHG-0009`.

### Requisitos, critérios e preservação funcional

- TESTEXEC-001 a 007 possuem cobertura em TESTEXEC-AC-001 a 007 e na matriz
  da seção 8;
- TESTEXEC-002 e AC-006 proíbem retirar cenário, falha, condição de borda ou
  oráculo; as matrizes SMARTAPP e COORD-REG permanecem funcionalmente
  preservadas;
- TESTEXEC-005 torna terminal a exigência de AC-005: mais de zero casos,
  resultado final e oráculo observado são obrigatórios; build ou mera
  capacidade de executar não aprovam o critério;
- TESTEXEC-006 e AC-007 separam objetivamente histórico auditável de evidência
  vigente, sem apagar fatos nem reutilizá-los como aprovação.

### Viabilidade e precedentes

- `pytest_hello_world.py` já contém precedentes independentes para target
  Linux host-native e execução genérica em placa; remover os imports, tipos,
  marker e caso específicos do emulador não exige nova arquitetura;
- `smart_sys_app_test` e `device_registry_test` são test apps ESP-IDF comuns,
  com `CMakeLists.txt`, app Unity e artefatos de flash. Podem executar em
  ESP32-C3 físico sem alteração de comportamento de produção;
- a execução host-native é opcional e condicionada à fidelidade integral dos
  substitutos. Quando essa prova não existir, TESTEXEC-003 determina o fallback
  físico, portanto o Implementador não precisa inventar uma nova abstração para
  satisfazer a política;
- G3-N do registry conserva adaptador de produção e NVS real em target físico;
  G3-F conserva a passagem por `device_registry_nvs.c`; G4 e G5 permanecem
  inalterados. A retirada do runner não enfraquece nenhum gate;
- a mudança técnica fica limitada a runners, imports, markers, comentários,
  configurações, documentação e artefatos de build inventariados. Não cria
  componente de produção, pasta estrutural ou política funcional paralela.

### Limites e dependências

- porta serial, placa disponível e automação de flash são dados de execução,
  não decisões normativas ausentes; ausência desses recursos resulta em
  `Not Executed`, conforme TESTEXEC-005;
- flash, monitor e captura física continuam dependendo de ordem explícita do
  Arquiteto, sem conflito com `AGENTS.md`;
- a ferramenta externa e suas bibliotecas estão corretamente fora do escopo do
  repositório; sua eventual desinstalação exige ordem separada;
- as versões 1.5 de `ISSP-Configurable-Bootstrap.md` e 0.4 de
  `ISSP-Coordinator-Paired-Device-Registry.md` permanecem `Pending Review` em
  seus próprios ciclos integrais. Esta promoção comprova a política
  coordenadora e não antecipa a promoção formal dessas especificações.

**Resultado:** `Implementable`.

Não há decisão normativa, de produto ou arquitetura ausente para migrar o
recorte completo. Estados preservados: normativo `Proposed`, implementação
`Not Started` e prontidão `Not Ready`. Esta análise não autoriza programar,
remover artefatos, executar testes nem realizar flash.

## 11. Registro de implementação (Engenheiro Implementador, 01/08/2026)

### Resultado técnico

- removidos de `pytest_hello_world.py` os imports, tipos, marker e caso
  específicos do emulador; a verificação do SHA-256 do ELF foi transferida ao
  teste físico genérico, preservando seu oráculo;
- adicionados runners físicos ESP32-C3
  `pytest_smart_sys_app.py` e `pytest_device_registry.py`, que exigem
  respectivamente os resumos terminais `20 Tests 0 Failures 0 Ignored` e
  `13 Tests 0 Failures 0 Ignored`, seguidos de `OK`;
- adicionado `sdkconfig.defaults` ESP32-C3 ao test app `SmartSysApp`; o test
  app do registry preserva o mesmo target e teve sua documentação migrada;
- removidas referências técnicas ao emulador de código, comentários e
  configurações versionadas; referências documentais restantes são política
  explícita ou histórico classificado como legado;
- removidos os dois diretórios locais, ignorados e regeneráveis
  `build_qemu_c3`, incluindo suas imagens de flash e efuse;
- a ferramenta externa e suas bibliotecas não foram desinstaladas, conforme o
  fora de escopo da seção 7.3.

### Evidência terminal

| Critério | Evidência | Resultado |
|---|---|---|
| TESTEXEC-AC-001 | varredura dos ativos vigentes; nenhuma prescrição positiva do runner removido | Approved |
| TESTEXEC-AC-002 | matriz da seção 5 e runners físicos adicionados, sem alteração das matrizes SMARTAPP/COORD-REG | Approved |
| TESTEXEC-AC-003 | varredura versionada sem imports, tipos, markers ou runners específicos fora de histórico/política | Approved |
| TESTEXEC-AC-004 | os dois caminhos `build_qemu_c3` não existem após remoção; fontes dos testes permanecem | Approved neste worktree |
| TESTEXEC-AC-005 | 20 `TEST_CASE` SmartSysApp e 13 `TEST_CASE` registry preservados; runners físicos escritos; execução física não iniciada | Not Executed |
| TESTEXEC-AC-006 | diff não remove requisitos, ACs, cenários ou gates funcionais | Approved |
| TESTEXEC-AC-007 | política, Bootstrap, Registry, mapa e changelog distinguem histórico de evidência vigente | Approved |

Os três arquivos Python modificados ou criados passaram em `py_compile`. A
configuração física também foi compilada com ESP-IDF 6.0.1 para ESP32-C3 em
builds temporários: `smart_sys_app_test.bin` com 138272 bytes e
`device_registry_test.bin` com 146320 bytes, ambos com código 0. A coleta de
testes não iniciou porque o ambiente Python ESP-IDF não contém o módulo
`pytest` (`No module named pytest`). Nenhum caso foi contado como executado.
Flash, monitor e hardware não foram iniciados porque não houve ordem explícita
do Arquiteto.

### Estado

A implementação permanece `In Progress`: a migração técnica está escrita e os
artefatos locais foram removidos, mas TESTEXEC-AC-005 exige execução terminal
com mais de zero casos em runner permitido. Promoção para `Implemented` depende
da instalação/configuração do runner e de ordem explícita para executar os dois
test apps em ESP32-C3 físico.

## 12. Revisão técnica da implementação (Engenheiro Revisor, 01/08/2026)

Recorte revisado: implementação do commit `c2e6c41`, requisitos TESTEXEC-001 a
007, critérios TESTEXEC-AC-001 a 007, matriz de substituição e artefatos
técnicos inventariados na seção 7.

### Evidência independente

- inspeção do diff confirmou a remoção dos imports, tipos, marker e caso QEMU
  do runner raiz e a adição dos dois runners físicos ESP32-C3;
- varredura versionada fora de documentação histórica/política não encontrou
  `pytest_embedded_qemu`, `pytest.mark.qemu` nem comando `idf.py qemu`;
- contagem direta encontrou 20 `TEST_CASE` no app SmartSysApp e 13 no app do
  registry; nenhum caso foi removido pela migração;
- `py_compile` dos três runners terminou com código 0 usando cache temporário;
- os artefatos temporários da implementação foram inspecionados e identificam
  target `esp32c3`: `smart_sys_app_test.bin` com 138272 bytes e SHA-256
  `39905a299b7db6b0a26031ce273b66da893d9b7ee5589dd44196c2a6930ee2c3`, e
  `device_registry_test.bin` com 146320 bytes e SHA-256
  `e69a2684fa114b4611fbf1644e6c24c976c7876ec51529426160fa162cfb8931`;
- os dois diretórios locais `build_qemu_c3` permanecem ausentes e
  `git diff --check` não encontrou erro material.

Nenhum flash, monitor ou teste físico foi executado nesta revisão. A ausência
de `pytest` no ambiente informado pelo Implementador impede até a coleta local;
essa condição continua sendo limitação de infraestrutura, não evidência
comportamental.

### Achados materiais

1. **Alto — TESTEXEC-AC-005 permanece sem evidência terminal.** Os runners
   expressam quantidade maior que zero e oráculo terminal, mas nenhum dos 33
   casos foi coletado ou executado em ESP32-C3 físico. Afeta TESTEXEC-002,
   TESTEXEC-004, TESTEXEC-005 e TESTEXEC-AC-005 e impede promover a
   implementação para `Implemented` ou a prontidão para `Ready`.
2. **Médio — quantidade normativa SmartSysApp diverge da fonte preservada.**
   TESTEXEC-AC-005, Bootstrap v1.5 e a abertura de EKM-CHG-0009 dizem
   “dezenove”, enquanto a fonte contém 20 casos e o runner exige exatamente
   `20 Tests 0 Failures 0 Ignored`. Não houve perda de cobertura, mas a
   discrepância impede usar a quantidade nominal da especificação como oráculo
   inequívoco e deve ser reconciliada por nova atuação de autoria.

### Limitações e recomendação

A revisão está limitada à política transversal e à retirada técnica de QEMU;
não reaprova o comportamento funcional de Bootstrap ou Registry e não converte
build em execução. O ambiente ainda precisa declarar/prover `pytest` e os
plugins ESP-IDF antes que o runner possa ser coletado; flash e captura física
continuam sujeitos à ordem explícita do Arquiteto.

**Recomendação ao Arquiteto:** não aceitar nem promover a implementação nesta
rodada. Solicitar ao Autor a reconciliação da quantidade SmartSysApp e, em
atuação posterior autorizada, prover/coletar os runners e executar os 20 + 13
casos em ESP32-C3 físico. Estados preservados: normativo `Proposed`,
implementação `In Progress`, prontidão `Not Ready` e `EKM-CHG-0009` `Open`.

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

## 14. Revisão de implementabilidade da v0.2 (Engenheiro Analista, 10/08/2026)

Recorte confrontado: TESTEXEC-001 a 008, TESTEXEC-AC-001 a 010, matriz da
seção 5 e inventário da seção 7, contra os artefatos versionados do
repositório, os dois test apps ESP-IDF, o test app host-native do registry, as
especificações dependentes de Bootstrap e Registry, `KNOWLEDGE-MAP.md` e
`EKM-CHG-0010`. Nenhum código, runner, configuração ou build foi alterado ou
executado nesta atuação.

### 14.1 Fatos confirmados por leitura

- contagem direta na fonte: 20 `TEST_CASE` em
  `components/issp_app_154/test_apps/smart_sys_app_test/main/test_smart_sys_app.cpp`
  e 24 `TEST_CASE` em
  `coordinator_154/test_apps/device_registry_test/main/test_device_registry.c`.
  As quantidades normativas de TESTEXEC-AC-005 e AC-009 correspondem à fonte
  preservada; a divergência histórica “dezenove/13” não está nesta política,
  mas permanece nas especificações dependentes;
- a classificação `Regressed` é factual. Materializam target fora da allowlist:
  `CONFIG_IDF_TARGET="esp32c3"` nos dois `sdkconfig.defaults`; os runners
  `pytest_smart_sys_app.py` e `pytest_device_registry.py`, ambos com
  `@pytest.mark.esp32c3` e `idf_parametrize('target', ['esp32c3'])`; o
  comentário de cabeçalho de `test_device_registry.c`; a tabela genérica da
  primeira linha de `README.md`; e
  `idf_parametrize('target', ['supported_targets', 'preview_targets'])` em
  `pytest_hello_world.py`;
- existe precedente equivalente próximo para TESTEXEC-008 e AC-010:
  `cmake/require_idf_6_0_1.cmake` reprova a configuração com `FATAL_ERROR` e
  diagnóstico explícito, e é incluído por `client_154`, `coordinator_154` e
  `examples/issp_minimal_client` entre `project.cmake` e `project()`. Um guard
  de allowlist segue esse precedente sem criar camada nova. Hoje esse guard não
  é incluído pelos dois test apps nem pelo `CMakeLists.txt` raiz;
- `components/issp_app_154/CMakeLists.txt` já consulta `IDF_TARGET`, porém para
  condicionar as fontes de rádio, não para reprovar target. Na forma atual ele
  é justamente o mecanismo que viabiliza compilar o componente fora da
  allowlist;
- a execução host-native de TESTEXEC-003 tem precedente independente e vigente:
  `coordinator_154/test_apps/device_registry_policy_host_test` compila a
  política real com `-Wall -Wextra -Werror` e registra o caso em `ctest`. A
  política não exige nova abstração para essa estratégia.

### 14.2 Bloqueadores objetivos

**B1 — prescrição vigente de target não suportado em especificações
dependentes.** `ISSP-Coordinator-Paired-Device-Registry.md` v0.4 exige, em
texto vigente de gate, execução “em ESP32-C3 ou ESP32-C6 físico” (seção de
classes de falha e gates G3-N/G3-F), e `ISSP-Configurable-Bootstrap.md` v1.5
prescreve, na sua reautoria, o app Unity “em ESP32-C3 físico” como fallback da
fidelidade host-native. TESTEXEC-AC-001 e AC-008 exigem que nenhuma
especificação vigente apresente esse target. Corrigir a definição de um gate é
ato de autoria sobre especificações que correm em ciclos próprios, não escolha
local de implementação: o Implementador desta política não pode reescrevê-las.
Enquanto essas duas autorias não ocorrerem, AC-001 e AC-008 são inatingíveis
pelo recorte de implementação.

**B2 — alcance de TESTEXEC-008 sobre execução host-native.** O texto normativo
é literal: somente `esp32h2` e `esp32c6` podem aparecer como `IDF_TARGET` em
runners ou configurações versionadas. `pytest_hello_world.py` declara dois
casos `host_test` com `idf_parametrize('target', ['linux'])`, e a estratégia
host-native admitida por TESTEXEC-003 depende de um alvo que não é nenhum dos
dois. A seção 5 afirma que execução host-native não é target de firmware, mas
essa afirmação não está no requisito. Sem decisão de autoria, o Implementador
tem de escolher entre remover casos válidos — conflito com TESTEXEC-002 — e
manter target fora da allowlist — conflito com AC-008. Falta declarar
explicitamente que `linux` e toolchains de host estão fora do alcance de
TESTEXEC-008.

**B3 — inventário da seção 7 incompleto perante AC-008 e ambiguidade da
matriz.** Artefatos versionados não inventariados apresentam target não
admitido ou contrariam a matriz da seção 5:

- `coordinator_154/sdkconfig.old` declara `CONFIG_IDF_TARGET="esp32"`, fora da
  allowlist;
- `client_154/sdkconfig.esp32c6` e `coordinator_154/sdkconfig.esp32h2` estão
  dentro da allowlist, mas contrariam as linhas da matriz que vinculam
  `client_154` ao ESP32-H2 e `coordinator_154` ao ESP32-C6;
- `client_154/sdkconfig.old` e `coordinator_154/sdkconfig.esp32c6.old` são
  baselines versionados de conveniência cuja sujeição a TESTEXEC-008 não está
  declarada.

A autoria precisa decidir duas coisas: se TESTEXEC-008 é allowlist de
repositório — qualquer alvo pode configurar H2 ou C6 — ou vínculo por alvo
conforme a matriz; e se arquivos `sdkconfig` e `.old` versionados contam como
“configurações versionadas” para AC-008. Hoje o texto admite as duas leituras,
com consequências opostas sobre arquivos existentes.

### 14.3 Lacunas menores, não bloqueadoras

- a seção 7.2 inventaria apenas os diretórios `build_qemu_c3`, já removidos.
  Permanecem localmente, ignorados pelo Git,
  `coordinator_154/test_apps/device_registry_test/build_impl_c3/` e os
  `sdkconfig`/`sdkconfig.old` do test app `SmartSysApp` com `esp32c3`, além de
  `__pycache__` do runner. São regeneráveis e não afetam o inventário
  versionado, mas convém citá-los para que AC-004 seja verificável sem
  interpretação;
- `sdkconfig.ci` na raiz está vazio e o projeto raiz é o template
  `hello_world`; a política não declara se esse projeto permanece como alvo do
  repositório ou apenas como resíduo de template.

### 14.4 Riscos e experimentos necessários

Não são confirmáveis por leitura e exigem execução autorizada:

- **E1 — build de `smart_sys_app_test` com `IDF_TARGET=esp32h2`.** Nunca
  exercitado. Com a migração, `issp_app_154` passa a compilar
  `smart_sys_app_hardware.cpp` e `src/reset/*.cpp` e a exigir
  `issp_transport_154`, `nvs_flash`, `esp_timer` e `esp_hw_support` dentro do
  app Unity sob `MINIMAL_BUILD`. Verificar código de saída, warnings, ausência
  de símbolo duplicado e que nenhum dos 20 casos passe a depender de rádio;
- **E2 — build de `device_registry_test` com `IDF_TARGET=esp32c6`**, com
  partição NVS real disponível, também nunca exercitado nesse target;
- **E3 — comprovação do guard de allowlist.** O precedente
  `require_idf_6_0_1.cmake` usa variáveis `IDF_VERSION_*`; ler `IDF_TARGET` por
  `idf_build_get_property` em nível de projeto, antes de `project()`, precisa
  ser comprovado por experimento, com alternativa documentada caso a
  propriedade ainda não esteja definida nesse ponto. AC-010 exige falha antes
  do binário e diagnóstico que informe os targets admitidos;
- **E4 — coleta e execução dos runners.** A ausência de `pytest` e dos plugins
  ESP-IDF registrada em `EKM-CHG-0009` não foi resolvida. Sem esse ambiente,
  sem placas ESP32-H2 e ESP32-C6 e sem ordem explícita de flash e monitor,
  TESTEXEC-AC-009 permanece `Not Executed`.

### 14.5 Classificação exigida pelo perfil

- decisão normativa ausente: B1, B2 e B3;
- escolha normal de implementação: local e forma do guard CMake, texto do
  diagnóstico, migração dos `sdkconfig.defaults` e comentários, e atualização
  dos oráculos dos runners para `20 Tests 0 Failures 0 Ignored` em ESP32-H2 e
  `24 Tests 0 Failures 0 Ignored` em ESP32-C6;
- dependência externa pendente: `pytest` e plugins ESP-IDF, placas físicas H2 e
  C6, porta serial e ordem explícita do Arquiteto para flash, monitor e
  captura.

**Resultado:** `Not Implementable` na forma atual — retorno à autoria.

A intenção arquitetural é clara e o recorte técnico é viável com os precedentes
existentes: allowlist por guard CMake, migração de runners e configurações para
H2 e C6, preservação integral dos 20 e 24 casos e fallback host-native já
precedido. O retorno se deve exclusivamente a B1, B2 e B3, que são decisões
normativas — inclusive sobre especificações de terceiros — e não podem ser
resolvidas por escolha local sem criar contrato por inferência.

Recomendação ao Arquiteto: autorizar uma v0.3 desta política que declare o
alcance de TESTEXEC-008 sobre host-native, resolva a leitura da matriz e
complete o inventário; e ordenar, no mesmo movimento ou antes da implementação,
a autoria das correções de target em Bootstrap v1.5 e Registry v0.4. Feitas
essas correções, a implementação pode ser ordenada com os experimentos E1 a E3
como evidência de configuração e build, permanecendo E4 condicionado a ordem
de execução física.

Estados preservados: normativo `Proposed`, implementação `Regressed`,
prontidão `Not Ready` e `EKM-CHG-0010` `Open`. Esta análise não promove estado,
não autoriza programar, remover artefatos, executar testes nem realizar flash.

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

## 16. Revisão de implementabilidade da v0.3 (Engenheiro Analista, 10/08/2026)

Recorte confrontado: as resoluções da seção 15, os requisitos alterados
TESTEXEC-003, 004, 006, 008 e o novo TESTEXEC-009, os critérios TESTEXEC-AC-008
e AC-009 na redação da v0.3, a matriz da seção 5 com a linha nova do projeto
diagnóstico e o inventário revisado da seção 7. Confrontados também os ativos
versionados do repositório e as duas especificações dependentes após o commit
`cb53449`. Nenhum código, runner, configuração ou build foi alterado ou
executado nesta atuação.

### 16.1 Verificação das resoluções

**B1 — verificado resolvido.** As prescrições vigentes foram corrigidas nas duas
especificações dependentes: `ISSP-Configurable-Bootstrap.md` passa a indicar o
app Unity em ESP32-H2 no fallback da seção 18 e na reautoria da seção 23, e
acrescenta a seção 24; `ISSP-Coordinator-Paired-Device-Registry.md` passa a
exigir ESP32-C6 nas classes de falha, na tabela de gates G3-N e no texto que
descreve as mudanças normativas da v0.4, e acrescenta a seção 16.20. As 19
ocorrências remanescentes de ESP32-C3 nessas duas especificações estão
exclusivamente em registros de ciclo — seção 22 do Bootstrap e seções 16.x do
registry — e nenhuma delas é prescrição: a varredura por texto imperativo
associado a esse target não retorna ocorrência. TESTEXEC-006, a seção 24 e a
seção 16.20 classificam esses registros como histórico auditável e evidência
inválida, o que é coerente com a regra de não reescrever registro passado.

**B2 — verificado resolvido quanto à intenção, com imprecisão factual
residual.** TESTEXEC-003 e TESTEXEC-008 agora delimitam a fronteira: host-native
é ambiente de lógica pura e a allowlist governa firmware e test apps ESP-IDF. O
precedente `coordinator_154/test_apps/device_registry_policy_host_test` é
compatível sem ressalva, porque usa CMake e toolchain de host, sem ESP-IDF.

A imprecisão está na afirmação de que execuções host-native “não definem
`IDF_TARGET`”. Os dois casos `host_test` de `pytest_hello_world.py` usam
`idf_parametrize('target', ['linux'])`, e o alvo `linux` do ESP-IDF define
`CONFIG_IDF_TARGET="linux"`. A intenção normativa é inequívoca e o Implementador
deve preservá-los, mas a verificação literal de AC-008 sobre esse arquivo
depende de interpretação. Recomendo à autoria nomear `linux` explicitamente como
ambiente host admitido em TESTEXEC-003, ou determinar que esses dois casos
migrem para o precedente de host puro. É ajuste de uma frase, não bloqueador.

**B3 — verificado resolvido, com risco material de execução.** A allowlist e o
vínculo por alvo agora são regras simultâneas e explícitas, o `sdkconfig`
principal de cada projeto é a configuração autoritativa e o projeto diagnóstico
da raiz está vinculado ao ESP32-H2, inclusive na matriz da seção 5. O item de
`coordinator_154/sdkconfig.old` com target `esp32`, que motivou o bloqueador,
está coberto pela remoção das cópias `.old`.

O risco é que as cópias mandadas remover não são equivalentes às preservadas:
`client_154/sdkconfig` tem 2145 linhas contra 1645 de `client_154/sdkconfig.esp32h2`,
com 154 linhas `CONFIG_` divergentes entre os dois; `coordinator_154/sdkconfig`
tem 2359 linhas contra 1757 de `coordinator_154/sdkconfig.esp32c6`. A remoção só
é segura depois de comprovar que nenhuma configuração intencional existe apenas
na cópia removida. Verifiquei também que nenhum `CMakeLists.txt`, script,
workflow ou especificação referencia essas cópias por nome — a única menção está
nesta política —, portanto a remoção não quebra ferramenta alguma.

### 16.2 Consistência interna da v0.3

- TESTEXEC-009 e a nova redação de AC-009 são coerentes com TESTEXEC-005: os 20
  e 24 casos permanecem `Not Executed` por decisão, não por falha, e nenhum
  estado é promovido por ausência de execução;
- as validações E1 a E3 promovidas pela seção 15 são builds e prova de
  configuração; não colidem com TESTEXEC-009, porque build não é execução
  comportamental e TESTEXEC-005 já proíbe tratá-lo como tal;
- a matriz da seção 5 e o vínculo de TESTEXEC-008 são consistentes entre si e
  com o inventário da seção 7.1;
- a rastreabilidade da seção 8 cobre TESTEXEC-009 por AC-005 e AC-009.

Duas lacunas menores de verificabilidade, ambas resolvíveis com uma linha de
autoria e nenhuma delas impeditiva:

- **G1:** a seção 7.2 passou a inventariar artefatos locais que não são de
  QEMU — `build_impl_c3/`, o `sdkconfig`/`sdkconfig.old` local do test app
  `SmartSysApp` e `__pycache__/` —, mas AC-004 continua redigido apenas para
  “diretórios e imagens locais de build QEMU”. A remoção desses novos itens fica
  sem critério de aceite correspondente;
- **G2:** AC-005 fala em “podem terminar em runner permitido com quantidade
  maior que zero”. Sob TESTEXEC-009, sua verificação nesta correção é
  documental — inspeção da fonte e do oráculo declarado no runner. Convém
  explicitar isso para que uma revisão futura não leia AC-005 como pendência de
  execução.

### 16.3 Validações obrigatórias

Mantidas da seção 14 e confirmadas como necessárias, todas de configuração e
build, nenhuma comportamental:

- **E1:** build de `smart_sys_app_test` com `IDF_TARGET=esp32h2`. Nunca
  exercitado; a migração faz `issp_app_154` compilar `smart_sys_app_hardware.cpp`
  e `src/reset/*.cpp` e exigir `issp_transport_154`, `nvs_flash`, `esp_timer` e
  `esp_hw_support` dentro do app Unity sob `MINIMAL_BUILD`;
- **E2:** build de `device_registry_test` com `IDF_TARGET=esp32c6`, também nunca
  exercitado nesse target;
- **E3:** prova do guard de allowlist, que deve reprovar antes de gerar binário e
  informar os targets admitidos. O precedente `cmake/require_idf_6_0_1.cmake`
  usa variáveis `IDF_VERSION_*`; ler `IDF_TARGET` por `idf_build_get_property`
  em nível de projeto, antes de `project()`, precisa ser comprovado, com
  alternativa documentada caso a propriedade ainda não esteja disponível nesse
  ponto. O guard precisa alcançar também os dois test apps e o projeto da raiz,
  que hoje não incluem nenhum guard;
- **E5 (novo):** diff entre cada `sdkconfig` autoritativo e a cópia sufixada ou
  `.old` correspondente antes da remoção, comprovando que nenhuma configuração
  intencional é perdida.

E4 foi retirado do encerramento pela seção 15. A ausência de `pytest` e dos
plugins ESP-IDF deixa de ser pendência desta correção e passa a ser dependência
de uma especificação futura que volte a solicitar execução.

### 16.4 Classificação exigida pelo perfil

- decisão normativa ausente: nenhuma que impeça migrar o recorte completo. G1,
  G2 e a precisão textual de B2 são ajustes de redação, não decisões novas;
- escolha normal de implementação: local e forma do guard CMake, texto do
  diagnóstico, migração dos `sdkconfig.defaults`, dos comentários e dos markers
  dos runners para H2 e C6, atualização de `README.md`,
  `pytest_hello_world.py` e `components/README.md`;
- dependência externa pendente: nenhuma para esta correção. Placas físicas,
  porta serial, `pytest` e ordem de flash pertencem à especificação futura
  prevista por TESTEXEC-009.

**Resultado:** `Implementable`.

As três resoluções da seção 15 eliminam os bloqueadores da v0.2 e o recorte
técnico continua coberto por precedentes existentes. Recomendo ao Arquiteto
ordenar a implementação com E1, E2, E3 e E5 como evidência obrigatória de
configuração e build, e incorporar na mesma ordem os ajustes de redação de G1,
G2 e da fronteira `linux` — sem eles a implementação continua possível, mas
AC-004, AC-005 e AC-008 ficam parcialmente verificáveis por interpretação.

Estados preservados: normativo `Proposed`, implementação `Regressed`, prontidão
`Not Ready` e `EKM-CHG-0010` `Open`. Esta análise não promove estado, não
autoriza programar, remover artefatos, executar testes nem realizar flash.

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

## 18. Registro de implementação da v0.3 (Engenheiro Implementador, 10/08/2026)

Recorte implementado: TESTEXEC-001 a 009 na redação da v0.3, a matriz da seção
5 e o inventário da seção 7, sob a promoção da seção 17. Ambiente: ESP-IDF
6.0.1 (`v6.0.1-dirty`), toolchain `riscv32-esp-elf-gcc` 15.2.0, macOS. Nenhum
flash, monitor, `pytest` ou execução comportamental foi iniciado.

### 18.1 Resultado técnico

- **Guard de allowlist.** Novo `cmake/require_supported_target.cmake`, ao lado
  do precedente `cmake/require_idf_6_0_1.cmake` e com a mesma forma de uso.
  Ele reprova com `FATAL_ERROR` quando `IDF_TARGET` está fora de
  `esp32h2`/`esp32c6` e, separadamente, quando o target admitido não é o do
  vínculo declarado pelo projeto em `ISSP_TARGET_BINDING`. Ambas as regras de
  B3 são verificadas simultaneamente. O guard é incluído entre
  `project.cmake` e `project()` em `CMakeLists.txt` da raiz, `client_154`,
  `coordinator_154`, `examples/issp_minimal_client` e nos dois test apps —
  os seis alvos exigidos pela seção 17;
- **decisão de leitura do target.** A questão em aberto de E3 foi resolvida
  por leitura da fonte do ESP-IDF 6.0.1: `project.cmake` chama `__target_init()`
  no próprio include, resolvendo o cache `IDF_TARGET` a partir do ambiente, do
  cache ou do `sdkconfig`, antes de `project()`. O guard lê a variável
  `IDF_TARGET` diretamente, exatamente como o precedente lê `IDF_VERSION_*`;
  `idf_build_get_property` não foi necessário e a alternativa documentada não
  precisou ser acionada;
- **allowlist no componente.** `components/issp_app_154/CMakeLists.txt` passou
  a reprovar target fora da allowlist, protegendo o componente perante um
  consumidor que não inclua o guard de projeto. Como todo target admitido
  possui rádio IEEE 802.15.4, `smart_sys_app_hardware.cpp` e
  `src/reset/*.cpp` entram em todo build do componente;
- **test apps migrados.** `sdkconfig.defaults` de `smart_sys_app_test` passou a
  `esp32h2` e o de `device_registry_test` a `esp32c6`. Os runners
  `pytest_smart_sys_app.py` e `pytest_device_registry.py` passaram a
  `@pytest.mark.esp32h2`/`esp32c6` e `idf_parametrize` do target
  correspondente, preservando os oráculos terminais
  `20 Tests 0 Failures 0 Ignored` e `24 Tests 0 Failures 0 Ignored` seguidos de
  `OK`. Nenhum `TEST_CASE` foi adicionado, removido ou alterado;
- **listas genéricas de template.** `pytest_hello_world.py` deixou de usar
  `idf_parametrize('target', ['supported_targets', 'preview_targets'])` e passou
  a `esp32h2`, o vínculo do projeto diagnóstico da raiz. A tabela genérica de
  treze targets do `README.md` da raiz foi substituída pelo target único, e os
  links de getting started de ESP32/ESP32-S2 pelo de ESP32-H2;
- **comentários corrigidos** em `smart_sys_app.cpp`, `smart_sys_app_impl.hpp`,
  `test_smart_sys_app.cpp`, `test_device_registry.c` e `components/README.md`,
  que apresentavam ESP32-C3 como target dos test apps e descreviam a
  condicional de fontes já superada;
- **configurações removidas** após E5: `client_154/sdkconfig.esp32c6`,
  `client_154/sdkconfig.esp32h2`, `client_154/sdkconfig.old`,
  `coordinator_154/sdkconfig.esp32c6`, `coordinator_154/sdkconfig.esp32c6.old`,
  `coordinator_154/sdkconfig.esp32h2`, `coordinator_154/sdkconfig.old` e o
  `sdkconfig.ci` vazio da raiz. `client_154/sdkconfig`,
  `coordinator_154/sdkconfig` e o `sdkconfig` da raiz permanecem autoritativos.
  `.gitignore` passou a ignorar `sdkconfig.old` e `sdkconfig.esp32*` para que as
  cópias não retornem;
- **artefatos locais da seção 7.2 removidos:**
  `coordinator_154/test_apps/device_registry_test/build_impl_c3/`,
  o `__pycache__/` do runner do registry e o `sdkconfig`/`sdkconfig.old` locais
  de `smart_sys_app_test`. Os dois `build_qemu_c3` já estavam ausentes;
- a ferramenta externa `qemu-riscv32` **não** foi desinstalada, conforme a
  seção 7.3.

### 18.2 Validações obrigatórias

| Validação | Comando/meio | Resultado |
|---|---|---|
| E1 | `idf.py -D IDF_TARGET=esp32h2 build` de `smart_sys_app_test`, build fora da árvore | código 0, 0 warnings, sem símbolo duplicado ou referência indefinida; `smart_sys_app_test.bin` 261424 bytes, SHA-256 `1f10c5438bf3974049f3f1c5c2ed6bc63fc3470fb73071bc8d746a2d885e29f2` |
| E1 (rádio) | `riscv32-esp-elf-nm -u` no objeto `test_smart_sys_app.cpp.obj`, combinado à inspeção dos `SetupHooks` e da composição do test app | 16 símbolos indefinidos, **zero** referências diretas citando `ieee802154`, transporte ou rádio; os hooks substituem os passos materiais de hardware |
| E2 | `idf.py -D IDF_TARGET=esp32c6 build` de `device_registry_test`, build fora da árvore | código 0, 0 warnings; `device_registry_test.bin` 156064 bytes, SHA-256 `e9283d8b0393e2c12466ecdcf9d85d6eabce12251cc96d994b8f520d2fe95fb3`; partição `nvs` real de 24K presente em `0x9000` |
| E3 | 16 configurações com target indevido nos seis projetos | as dez provas originais e seis casos adicionais com `IDF_TARGET=linux` reprovaram com código 2, diagnóstico da allowlist e **zero** binários; nenhuma suíte foi coletada ou executada |
| E5 | diff `CONFIG_` de cada `sdkconfig` autoritativo contra cada cópia, antes da remoção | nenhuma configuração intencional existia apenas na cópia removida |

**E3 em detalhe.** Seis casos usaram `esp32c3`, fora da allowlist, na raiz, em
`client_154`, `coordinator_154`, `examples/issp_minimal_client` e nos dois test
apps; quatro casos usaram target dentro da allowlist mas fora do vínculo
(`client_154` e `smart_sys_app_test` com `esp32c6`; `coordinator_154` e
`device_registry_test` com `esp32h2`). A correção v0.4 acrescenta seis casos com
`IDF_TARGET=linux`, um por projeto ESP-IDF, para provar que a antiga exceção
global foi encerrada. As seis configurações usaram `-DSDKCONFIG` temporário fora
da árvore, pois os quatro projetos de produto possuem `sdkconfig` autoritativo
H2/C6 que corretamente impediria a troca antes de o guard ser alcançado. Todos
os seis casos retornaram código 2 pelo guard, exibiram os targets admitidos e
produziram zero binários. Diagnóstico observado no primeiro grupo:

```
CMake Error at cmake/require_supported_target.cmake:35 (message):
  IoTSmartLink15.4 does not support IDF_TARGET 'esp32c3'; admitted targets
  are esp32h2;esp32c6.  Both carry an IEEE 802.15.4 radio; no other chip is a
  target of this repository, and host-native logic tests are a separate
  strategy that never builds firmware.
-- Configuring incomplete, errors occurred!
```

**E5 em detalhe.** Toda cópia removida foi gerada por ESP-IDF 5.5.1, versão
reprovada por `require_idf_6_0_1.cmake`, exceto `client_154/sdkconfig.old`, que
é 6.0.1. Entre os pares de mesmo target, as únicas divergências de valor em
símbolo comum foram `CONFIG_IDF_INIT_VERSION`, o formato de
`CONFIG_ESPTOOLPY_BEFORE`/`AFTER` — todas geradas pela troca de versão — e, no
coordenador, o console: o arquivo autoritativo escolhe USB Serial/JTAG
(`CONFIG_ESP_CONSOLE_UART_NUM=-1`) e a cópia antiga usava UART0, ou seja, a
escolha intencional está no arquivo preservado, não na cópia. As 8 divergências
de `client_154/sdkconfig.old` são 7 valores de log level superados pelo arquivo
autoritativo mais o símbolo obsoleto
`CONFIG_IOT154_COORDINATOR_SEND_FAILURE_LIMIT`, sem Kconfig nem referência em
código no repositório. Varredura independente confirmou que apenas
`CONFIG_IDF_TARGET_ESP32H2` e `CONFIG_IDF_TARGET_ESP32C6` são consumidos por
código-fonte deste repositório. Nenhuma reconciliação foi necessária e nenhuma
dúvida material interrompeu a remoção.

**Não regressão dos alvos de produto.** Sob o guard, `client_154` (H2),
`coordinator_154` (C6), `examples/issp_minimal_client` (H2) e o projeto
diagnóstico da raiz (H2) compilaram com código 0 e 0 warnings.

### 18.3 Evidência por critério

| Critério | Evidência | Resultado |
|---|---|---|
| TESTEXEC-AC-001 | varredura versionada: nenhuma prescrição vigente de QEMU; as ocorrências restantes são política explícita ou registro de ciclo | Approved |
| TESTEXEC-AC-002 | runners migrados preservam quantidade e oráculo terminal; nenhum cenário removido | Approved |
| TESTEXEC-AC-003 | varredura versionada sem imports, markers, runners ou comandos de QEMU fora de histórico/política | Approved |
| TESTEXEC-AC-004 | todos os artefatos da seção 7.2 ausentes; fontes de teste preservadas | Approved |
| TESTEXEC-AC-005 | verificação documental: 20 e 24 `TEST_CASE` contados na fonte; oráculos terminais inspecionados nos dois runners; sem coleta ou execução | Approved |
| TESTEXEC-AC-006 | diff não remove requisito, AC, cenário, falha, borda ou gate | Approved |
| TESTEXEC-AC-007 | registros QEMU e ESP32-C3 permanecem apenas em seções de ciclo, classificados como evidência inválida | Approved |
| TESTEXEC-AC-008 | nenhum `CONFIG_IDF_TARGET` versionado fora de `esp32h2`/`esp32c6`; casos Linux inoperantes removidos e host-native vigente restrito a toolchain de host puro | Approved |
| TESTEXEC-AC-009 | as 20 e as 24 descrições de caso estão presentes e identificáveis nos ELF de E1 e E2; nenhum caso coletado, gravado ou executado | Approved |
| TESTEXEC-AC-010 | E3: dez configurações originais e seis rejeições adicionais de `linux` reprovadas antes do binário, com os targets admitidos no diagnóstico | Approved |

### 18.4 Limitações e desvios

- **desvio `linux` encerrado na v0.4.** Os dois casos `host_test` herdados do
  template foram removidos porque não eram construtíveis, não protegiam cenário
  do domínio e não justificavam uma exceção global. O guard e
  `issp_app_154` agora rejeitam todo target diferente de H2/C6. Host-native
  continua disponível pelo precedente de host puro do registry; um uso futuro
  de ESP-IDF/Linux depende de especificação e projeto dedicados;
- **`Not Executed` por decisão, não por falha.** Nenhum dos 44 casos foi
  coletado, gravado ou executado, conforme TESTEXEC-009. `pytest` e os plugins
  ESP-IDF continuam ausentes do ambiente, e isso deixou de ser pendência desta
  correção;
- flash, monitor e captura em placa não foram iniciados: não houve ordem
  explícita do Arquiteto e E4 permanece fora do encerramento;
- todos os builds ocorreram fora da árvore, com `sdkconfig` temporário, de modo
  que nenhum artefato gerado entrou no repositório.

### 18.5 Estado

Implementação `Validated`. A migração de targets e a correção da fronteira
Linux estão completas; E3 foi ampliada para os seis projetos, sem pendência
técnica neste recorte. A limitação `Not Executed` de TESTEXEC-009 continua:
nenhuma das 44 suítes fez parte desta validação. O estado normativo passa a
`Active` e `EKM-CHG-0010` é encerrada pela decisão registrada na seção 18.7.

### 18.6 Correção arquitetural v0.4

A revisão do commit `ad5777b` identificou que o retorno antecipado para
`IDF_TARGET=linux` pulava a verificação de `ISSP_TARGET_BINDING` nos seis
projetos. A decisão arquitetural foi fechar os projetos ESP-IDF exclusivamente
em H2/C6, remover os dois casos Linux inoperantes do runner raiz e preservar
host-native somente pelo precedente de toolchain de host puro.

A correção não altera os 20 casos `SmartSysApp`, os 24 casos do registry,
firmware, protocolo ou comportamento de runtime. Sua validação se limita a
inspeção, consistência do diff e seis configurações negativas adicionais; não
autoriza coleta, execução, flash, monitor ou `pytest`.

As seis configurações foram executadas com ESP-IDF 6.0.1, build e `sdkconfig`
temporários fora da árvore. Todas terminaram com código 2 no guard, diagnóstico
nomeando ESP32-H2 e ESP32-C6 e zero arquivos `.bin`.

Como a correção simplificou a composição CMake de `issp_app_154`, E1 e E2
foram recompiladas, sem executar os casos. O build H2 terminou com código 0,
zero warnings e binário de 261424 bytes; o build C6 terminou com código 0, zero
warnings e binário de 156064 bytes. Os tamanhos permanecem idênticos aos da
implementação original.

### 18.7 Aprovação arquitetural e promoção

Em 10/08/2026, o Arquiteto informou ter realizado o teste da implementação e a
aprovou. A decisão promove esta política para `Active`, a implementação para
`Validated` e encerra `EKM-CHG-0010`.

A aprovação não reclassifica as 44 suítes como executadas: elas permanecem
preservadas e `Not Executed` conforme TESTEXEC-009. Uma execução futura ainda
depende de especificação que declare propósito, recorte, ambiente, oráculo e
custo aceito.
