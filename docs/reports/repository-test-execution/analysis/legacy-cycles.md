# Relatório de análise — política de targets e testes

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/Repository-Test-Execution-Policy.md`

**Revisão confrontada:** Registro histórico EKM 1.x preservado na migração para EKOM 3.2

**Estado:** Concluído

> Este relatório preserva uma atuação histórica e não altera fontes normativas.

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
