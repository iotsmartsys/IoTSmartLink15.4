# Política de Targets Suportados e Execução de Testes do Repositório

**Tipo:** Normativo
**Estado normativo:** Proposed
**Estado da implementação:** Regressed — existem runners e configurações para
target não suportado
**Prontidão:** Not Ready
**Revisão de implementabilidade:** Pending Review
**Versão:** 0.2
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
- invalidação de evidências produzidas em target não suportado e estratégia de
  reexecução nos targets admitidos.

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

### TESTEXEC-004 — Execução em target físico

Teste dependente de ESP-IDF, FreeRTOS, NVS real, periférico ou comportamento do
target deve executar em placa física suportada. O app Unity de `SmartSysApp`
executa em ESP32-H2; o app Unity do registry do coordenador executa em ESP32-C6.
Fakes podem evitar a inicialização do rádio, mas não autorizam selecionar outro
target para o firmware de teste.

Flash, monitor e captura automatizada em placa continuam sujeitos à ordem
explícita do Arquiteto prevista pelo `AGENTS.md`.

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
admitido; o resultado não pode ser renomeado ou transportado para outro target.

### TESTEXEC-007 — Remoção técnica posterior

A remoção de imports, markers, runners, comentários, configurações e diretórios
de build pertence a uma atuação posterior de Engenheiro Implementador. Essa
atuação deve preservar os testes ainda úteis, migrando seu runner antes de
excluir qualquer artefato necessário à cobertura.

### TESTEXEC-008 — Allowlist de targets

Somente `esp32h2` e `esp32c6` podem aparecer como `IDF_TARGET` em projetos,
test apps, runners ou configurações versionadas deste repositório. Seleções
fora dessa allowlist devem falhar na configuração com diagnóstico explícito,
sem gerar binário.

Listas genéricas herdadas de templates ESP-IDF, como `supported_targets` ou
`preview_targets`, não constituem política do projeto e devem ser removidas dos
runners e da documentação operacional.

## 5. Matriz autoritativa de targets e execução

| Alvo ou evidência | Target físico | Estratégia permitida | Semântica obrigatória |
|---|---|---|---|
| `client_154`, `SmartSysApp` e exemplo mínimo do client | ESP32-H2 | build e hardware; lógica pura pode ter teste host-native fiel | composição, estados, falhas, rádio, GPIO e resultado terminal conforme o gate |
| `coordinator_154` e registry | ESP32-C6 | build e hardware; lógica pura pode ter teste host-native fiel | política integrada, NVS, rádio, reboot e resultado terminal conforme o gate |
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
- **TESTEXEC-AC-004:** diretórios e imagens locais de build QEMU identificados
  no inventário foram removidos sem afetar fontes de teste preservadas.
- **TESTEXEC-AC-005:** os 20 cenários `SmartSysApp` e os 24 cenários do registry
  permanecem rastreados e podem terminar em runner permitido com quantidade
  maior que zero.
- **TESTEXEC-AC-006:** nenhuma matriz funcional perdeu requisito, cenário,
  falha, condição de borda ou gate por causa da retirada do emulador.
- **TESTEXEC-AC-007:** registros QEMU históricos estão claramente separados
  da evidência vigente e não são agregados como aprovação nova.
- **TESTEXEC-AC-008:** nenhum projeto, runner, configuração ou documentação
  operacional apresenta target fora de `esp32h2` e `esp32c6`; ocorrências
  históricas restantes estão explicitamente classificadas como erro e evidência
  inválida.
- **TESTEXEC-AC-009:** os 20 casos `SmartSysApp` terminam em ESP32-H2 e os 24
  casos do registry terminam em ESP32-C6, com mais de zero casos, resumo Unity e
  `OK`; até essa execução, permanecem `Not Executed`.
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
- referências operacionais em `components/README.md`, especificações, mapa e
  changelog, distinguindo contrato vigente, erro histórico e evidência inválida.

### 7.2 Locais ou gerados, não versionados

- `components/issp_app_154/test_apps/smart_sys_app_test/build_qemu_c3/`;
- `coordinator_154/test_apps/device_registry_test/build_qemu_c3/`;
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

## 9. Estado e próxima etapa

Esta versão 0.2 registra a correção arquitetural sem alterar código, runner ou
configuração. A implementação atual está `Regressed` porque materializa target
fora da allowlist e porque as execuções correspondentes não são evidência
válida. A próxima etapa é análise independente de implementabilidade, seguida
por ordem separada de implementação e revalidação física.

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
