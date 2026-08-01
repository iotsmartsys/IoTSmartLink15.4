# Política de Execução de Testes do Repositório

**Tipo:** Normativo
**Estado normativo:** Proposed
**Estado da implementação:** Not Started
**Prontidão:** Not Ready
**Revisão de implementabilidade:** Pending Review
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
alterar implementação. Estados: `Proposed`, `Not Started`, `Not Ready` e
`Pending Review`.

A próxima etapa é análise independente de implementabilidade. Somente depois
de promoção e ordem própria do Arquiteto um Engenheiro Implementador poderá
migrar runners, ajustar test apps e excluir os artefatos inventariados.
