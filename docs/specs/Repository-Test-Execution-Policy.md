# Política de Targets Suportados e Execução de Testes do Repositório

**Tipo:** Normativo
**Estado normativo:** Active
**Estado da implementação:** Validated; 63 casos preservados e `Not Executed`
**Estado do workflow:** Concluída
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

Os 25 casos `SmartSysApp`, 10 de `DigitalInputBehavior`, quatro de concorrência
de `IsspDevice` e 24 do registry permanecem preservados como ativos de
engenharia, mas não devem ser coletados, gravados ou executados sem ordem. A
EKOM não transforma sua execução em etapa automática do workflow.

Somente uma especificação futura, posterior a esta decisão, pode solicitar a
execução de casos, definindo propósito, recorte, ambiente, oráculo, custo aceito
e autoridade para operações em hardware. Até essa solicitação, o estado correto
é `Not Executed`, sem constituir pendência para encerrar a correção de targets.

## 5. Matriz autoritativa de targets e execução

| Alvo ou evidência | Target físico | Estratégia permitida | Semântica obrigatória |
|---|---|---|---|
| `client_154`, `SmartSysApp` e exemplo mínimo do client | ESP32-H2 | build e hardware; lógica pura pode ter teste host-native fiel | composição, estados, falhas, rádio, GPIO e resultado terminal conforme o gate |
| `coordinator_154` e registry | ESP32-C6 | build e hardware; lógica pura pode ter teste host-native fiel | política integrada, NVS, rádio, reboot e resultado terminal conforme o gate |
| test apps de `issp_behaviors` e `issp_core` | ESP32-H2 | build; execução somente quando especificada | debounce, timer e concorrência conforme o oráculo de cada suíte |
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
- **TESTEXEC-AC-005:** os 25 cenários `SmartSysApp`, 10 de
  `DigitalInputBehavior`, quatro de `IsspDevice` e 24 do registry permanecem
  rastreados. Nesta política, a verificação é exclusivamente
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
- **TESTEXEC-AC-009:** os 63 casos versionados permanecem presentes e
  identificáveis, sem coleta, flash ou execução automática e sem alegação de
  resultado comportamental novo.
- **TESTEXEC-AC-010:** configurar um test app ou composição com target fora da
  allowlist falha antes do binário e informa os targets admitidos.

## 7. Artefatos técnicos candidatos à remoção ou migração

Nenhum item desta seção é excluído pela autoria.

### 7.1 Versionados

- `README.md` e `pytest_hello_world.py`: remover lista genérica de targets e
  runner herdados do template; preservar somente utilidade comprovada para H2,
  C6 ou host-native fiel;
- `components/issp_app_154/CMakeLists.txt`: rejeitar target fora da allowlist;
- `components/issp_app_154/test_apps/smart_sys_app_test`: preservar os 25 casos,
  migrar runner, `sdkconfig.defaults` e comentários para ESP32-H2;
- `components/issp_behaviors/test_apps/digital_input_behavior_test`: preservar
  os 10 casos e o vínculo ESP32-H2;
- `components/issp_core/test_apps/issp_device_concurrency_test`: preservar os
  quatro casos e o vínculo ESP32-H2;
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

## 9. Estado vigente e relatórios relacionados

A política v0.4 está `Active / Validated`. Os guards e vínculos H2/C6 estão
implementados. As suítes permanecem preservadas e `Not Executed`; nova execução
depende de especificação futura.

- análise: `docs/reports/repository-test-execution/analysis/legacy-cycles.md`;
- implementação: `docs/reports/repository-test-execution/implementation/legacy-cycles.md`;
- revisão: `docs/reports/repository-test-execution/review/legacy-cycle.md`;
- decisões e validação:
  `docs/reports/repository-test-execution/validation/architectural-cycles.md`.
