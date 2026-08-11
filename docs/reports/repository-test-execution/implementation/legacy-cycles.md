# Relatório de implementação — política de targets e testes

**Classe da fonte:** Relatório

**Papel:** Engenheiro Implementador

**Especificação:** `docs/specs/Repository-Test-Execution-Policy.md`

**Revisão confrontada:** Registro histórico EKM 1.x preservado na migração para EKOM 3.2

**Estado:** Concluído

> Este relatório preserva uma atuação histórica e não altera fontes normativas.

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
