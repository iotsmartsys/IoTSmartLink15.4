# Política de Execução de Testes do Repositório

**Tipo:** Normativo
**Estado normativo:** Proposed
**Estado da implementação:** In Progress
**Prontidão:** Not Ready
**Revisão de implementabilidade:** Implementable
**Versão:** 0.1
**Responsável arquitetural:** Marcelo Miranda
**Última atualização:** 01/08/2026
**Escopo:** Estratégias de execução de testes em todo o repositório

---

## 1. Intenção confirmada

O Arquiteto determinou que QEMU deixe de ser usado neste repositório como
estratégia de validação ou execução de testes. A decisão é transversal a
especificações, critérios, gates, documentação, runners e artefatos técnicos.

A remoção da ferramenta não reduz requisito funcional, cenário, falha,
condição de borda ou oráculo. Ela altera somente o ambiente no qual a evidência
executável é produzida.

## 2. Escopo

Inclui:

- especificações e matrizes que prescrevem execução automatizada;
- test apps ESP-IDF e runners associados;
- documentação operacional e rastreabilidade EKM;
- evidências futuras de implementação, revisão e validação;
- inventário dos artefatos técnicos cuja remoção ou migração caberá a uma
  atuação posterior de Engenheiro Implementador.

## 3. Fora de escopo

- alterar comportamento funcional de produto;
- reduzir critérios de aceite ou converter build/inspeção em teste
  comportamental;
- excluir código, test apps, runners, configurações ou diretórios nesta
  atuação de autoria;
- apagar registros históricos de execuções já realizadas;
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
target deve executar em placa física suportada. Test apps sem rádio podem usar
ESP32-C3 físico; composição de produção e cenários IEEE 802.15.4 usam os
targets definidos por suas especificações.

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

### TESTEXEC-007 — Remoção técnica posterior

A remoção de imports, markers, runners, comentários, configurações e diretórios
de build pertence a uma atuação posterior de Engenheiro Implementador. Essa
atuação deve preservar os testes ainda úteis, migrando seu runner antes de
excluir qualquer artefato necessário à cobertura.

## 5. Matriz de substituição

| Evidência afetada | Estratégia vigente proposta | Semântica que deve permanecer |
|---|---|---|
| `SmartSysApp::SetupHooks` | host-native fiel ou app Unity em ESP32-C3 físico | estados, ordem, falhas, rollback, contadores e resultado terminal |
| core do registry e backend NVS substituível | host-native fiel ou app Unity em ESP32-C3 físico | staging, durable, namespaces, reboot, falhas e contadores |
| adaptador `device_registry_nvs.c` com NVS real | ESP32-C3 ou ESP32-C6 físico | partição real, reabertura, corrupção, sentinela e commit |
| composição/radio IEEE 802.15.4 | ESP32-C6/H2 físicos conforme a especificação | boot, rádio, commissioning, ACK, reboot e comportamento ponta a ponta |
| exemplo `hello_world` | target Linux host-native ou placa física suportada | saída, hash quando aplicável e término observável |

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
- **TESTEXEC-AC-005:** os dezenove cenários `SmartSysApp` e os cenários do
  registry permanecem rastreados e podem terminar em runner permitido com
  quantidade maior que zero.
- **TESTEXEC-AC-006:** nenhuma matriz funcional perdeu requisito, cenário,
  falha, condição de borda ou gate por causa da retirada do emulador.
- **TESTEXEC-AC-007:** registros QEMU históricos estão claramente separados
  da evidência vigente e não são agregados como aprovação nova.

## 7. Artefatos técnicos candidatos à remoção ou migração

Nenhum item desta seção é excluído pela autoria.

### 7.1 Versionados

- `pytest_hello_world.py`: imports `pytest_embedded_qemu`, tipos `QemuApp` e
  `QemuDut`, marker `qemu` e caso `test_hello_world_host` dependente do runner;
- `components/issp_app_154/test_apps/smart_sys_app_test`: preservar os testes,
  migrar comentários/configuração e definir runner host-native ou físico;
- `coordinator_154/test_apps/device_registry_test`: preservar os testes,
  migrar comentários/configuração e definir runners host-native/físico para
  cada gate;
- comentários em `components/issp_app_154/src/smart_sys_app.cpp`,
  `smart_sys_app_impl.hpp` e nos test apps que apresentam QEMU como target;
- referências operacionais em `components/README.md`, especificações, mapa e
  changelog, distinguindo contrato vigente de registro histórico.

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

## 9. Estado e próxima etapa

Esta versão registra a decisão arquitetural e propõe a migração completa sem
alterar comportamento funcional. Estados: `Proposed`, `In Progress`, `Not Ready` e
`Implementable`.

A análise independente da seção 10 promoveu esta versão a `Implementable`.
Uma ordem própria do Arquiteto continua necessária antes que um Engenheiro
Implementador possa migrar runners, ajustar test apps ou excluir os artefatos
inventariados.

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
