# Relatório de revisão — registry do coordenador

**Classe da fonte:** Relatório

**Papel:** Engenheiro Revisor

**Especificação:** `docs/specs/ISSP-Coordinator-Paired-Device-Registry.md`

**Revisão confrontada:** Registro histórico EKM 1.x preservado na migração para EKOM 3.2

**Estado:** Concluído

> Este relatório preserva uma atuação histórica e não altera fontes normativas.

### 16.3 Revisão técnica (Engenheiro Revisor, 01/08/2026)

Recorte revisado: implementação completa do commit `2cc600c`, requisitos
COORD-REG-001 a COORD-REG-013 e critérios COORD-REG-AC-001 a AC-008.

**Evidência terminal obtida nesta revisão:**

- build de produção com ESP-IDF 6.0.1 para `esp32c6`: concluído com código 0,
  sem warnings do compilador; `central_154.bin` gerado com `0x45bc0` bytes;
- build do app `coordinator_154/test_apps/device_registry_test` para
  `esp32c3`: concluído com código 0;
- execução do app no QEMU: `10 Tests 0 Failures 0 Ignored`, seguida de
  encerramento terminal do QEMU com código 0;
- `git diff --check`: concluído sem erro.

Essas execuções corrigem a limitação ambiental registrada na seção 16.2, mas
comprovam somente compilação e os dez cenários realmente presentes no app de
teste. Não constituem evidência de hardware real nem dos gates ausentes.

**Achados materiais:**

1. **Alto — fail-closed incompleto em `RegistryUnavailable`**
   (`COORD-REG-011`, `COORD-REG-AC-007`). Em `main.c`, quando
   `device_registry_find()` retorna falso, a decisão considera apenas a janela
   de ingresso. Com a janela aberta, `DATA` é encaminhado ao host e recebe ACK
   mesmo quando o registry está indisponível. Isso contraria a seção 7, que
   exige operação fechada para tráfego de devices nesse estado.
2. **Alto — recuperação do boot ainda pode apagar toda a NVS**
   (`COORD-REG-010`, `COORD-REG-AC-007`). `init_nvs()` mantém
   `nvs_flash_erase()` para `ESP_ERR_NVS_NO_FREE_PAGES` e
   `ESP_ERR_NVS_NEW_VERSION_FOUND`. O comportamento pode remover namespaces
   não relacionados antes de o registry ser classificado como indisponível,
   contrariando as seções 7 e 10 e inviabilizando o oráculo de sentinela do
   AC-007.
3. **Alto — gates automatizados obrigatórios incompletos** (seção 13).
   Não existe teste de integração para AC-005; o teste rotulado AC-006 verifica
   apenas o tamanho do blob e não executa sequência, reboot e reenvio; o fake
   do AC-002 oferece uma operação `write` já atômica e não representa staging,
   commit separado e conteúdo reiniciado; e os cenários de AC-007 não modelam
   namespaces nem sentinela. Assim, `10/10` aprova o conjunto escrito, mas não
   os gates AC-002/005/006/007 completos.
4. **Médio — registro de implementação superestima cobertura.** A seção 16.2
   e o changelog declaram teste de contagem inválida, mas os dez casos
   executados não incluem esse cenário. A alegação anterior de ausência de
   `idf.py`/toolchain também deixou de representar o ambiente observado nesta
   revisão.

**Limitações preservadas:** COORD-REG-AC-001 e AC-008 continuam sem execução
terminal em hardware real; AC-007 continua sem prova de isolamento contra NVS
real ou fake semanticamente equivalente. Nenhuma validação humana nem aprovação
do Arquiteto foi fornecida nesta ordem.

**Recomendação ao Arquiteto:** não aceitar nem promover a implementação. Abrir
uma atuação separada de Engenheiro Implementador para corrigir os dois defeitos
de runtime e completar os gates obrigatórios; depois repetir revisão, QEMU e os
cenários de hardware. Estados preservados: `Proposed`, `In Progress` e
`Not Ready`; `EKM-CHG-0008` permanece `Open`.

### 16.12 Revisão técnica da correção v0.3 (Engenheiro Revisor, 01/08/2026)

Recorte revisado: implementação integral do commit `a739be0`, requisitos
COORD-REG-001 a COORD-REG-013, critérios COORD-REG-AC-001 a AC-008 e gates
G1, G2, G3-N, G3-F, G4 e G5.

**Evidência terminal independente:**

- build limpo da composição de produção com ESP-IDF 6.0.1 para ESP32-C6:
  código 0, `central_154.bin` gerado com `0x45b00` bytes;
- build limpo do app `device_registry_test` para ESP32-C3: código 0;
- execução do app no QEMU: `13 Tests 0 Failures 0 Ignored`, encerramento
  controlado e código 0;
- varredura estática confirmou a remoção das operações de apagamento NVS em
  `coordinator_154/main`;
- `git diff --check`: concluído sem erro antes deste registro de revisão.

Esses resultados confirmam G4 e os treze cenários efetivamente escritos. Não
convertem testes parciais em aprovação dos critérios nem suprem gates que não
foram executados.

**Achados materiais:**

1. **Alto — gates obrigatórios e critérios continuam incompletos.** O app de
   teste compila `device_registry.c` e `device_registry_nvs.c`, mas não executa
   o código de decisão de `main.c`; portanto não existe G1. G3-N com NVS real e
   G5 em hardware também não foram executados. AC-005 não possui cenário, e
   AC-003, AC-004 e AC-006 exercitam apenas parte do core. AC-007 não cobre as
   classes obrigatórias de contagem maior que oito, endereços nulo/broadcast,
   endereço duplicado, erros de inicialização NVS, fail-closed em todas as
   entradas ou sentinela sob G3-N. Consequentemente, nenhum AC está aprovado
   integralmente e a implementação não pode ser promovida.
2. **Médio — comando do host não conserva `RegistryUnavailable` até a decisão
   final** (`COORD-REG-011`, `COORD-REG-012`, AC-007/G1). Em
   `start_host_command()`, uma falha de `device_registry_find()` é registrada
   como `command target not known`; o estado do registry não é consultado e a
   verificação de comando pendente ocorre antes da disponibilidade. O efeito
   seguro de não transmitir é preservado, mas o estado indisponível é
   confundido com identidade desconhecida, contrariando a precedência da
   seção 5.1. Por isso, a afirmação da seção 16.11 de que o comando do host
   passou a considerar explicitamente `RegistryUnavailable` não é sustentada
   pelo código.
3. **Médio — rótulos integrais superestimam a evidência.** Os testes referentes
   a AC-002, AC-003, AC-004 e AC-006 ainda usam `[AC-002]`, `[AC-003]`,
   `[AC-004]` e `[AC-006]`, embora o próprio registro de implementação os
   classifique como parciais. Isso viola o manifesto da seção 13, que reserva
   `[AC-00N]` ao critério completo e exige sufixo `[AC-00N-partial-...]` para
   subconjuntos.
4. **Médio — G3-F foi executado somente de forma parcial.** O caso atravessa o
   adaptador de produção e comprova staging, erro de commit, durable anterior e
   sentinela. Como não executa a política integrada, não observa ausência de
   resposta/publicação exigida pela definição de G3-F e por AC-002. O resultado
   `Partial` da seção 16.11 está correto; referências resumidas não devem
   apresentar esse gate como concluído.

**Limitações preservadas:** não houve execução em rádio/hardware real nem
aprovação humana do Arquiteto. A tentativa adicional de reusar um diretório de
build dentro do sandbox encontrou bloqueio ambiental de `sysctl`; os builds
limpos executados fora desse isolamento terminaram com código 0 e são a
evidência considerada.

**Recomendação ao Arquiteto:** não aceitar nem promover a implementação.
Solicitar atuação corretiva do Engenheiro Implementador para preservar a
distinção de indisponibilidade no comando do host, corrigir os rótulos de
evidência e implementar os gates/cenários pendentes; em seguida repetir revisão
integral, incluindo G1, G3-N e G5. Estados preservados: normativo `Proposed`,
implementação `In Progress`, prontidão `Not Ready` e `EKM-CHG-0008` `Open`.

### 16.16 Revisão técnica da implementação v0.4 (Engenheiro Revisor, 01/08/2026)

Recorte revisado: implementação integral disponível até o commit `135aef1`,
COORD-REG-001 a 013, COORD-REG-AC-001 a AC-008, matriz de decisão, gates G1 a
G5, contrato dos substitutos, manifesto de evidências e migração para runners
permitidos pela política transversal.

**Evidência terminal independente**

- build limpo do app `device_registry_test` com ESP-IDF 6.0.1 para ESP32-C3:
  código 0, `device_registry_test.bin` com `0x24110` bytes e SHA-256
  `b737a30b39e8ae1273b6cc37cd39fdd2fbd6e34689076c82ecff5e117e03242a`;
- build limpo da composição de produção com ESP-IDF 6.0.1 para ESP32-C6:
  código 0, `central_154.bin` com `0x45bc0` bytes e SHA-256
  `a4fe24567b1b14b4db4c42dc1ef7cef816dcbdb1092ea462db0b94a082d4f4aa`;
- nenhuma ocorrência técnica vigente de import, marker ou comando QEMU foi
  encontrada fora de política/histórico; nenhuma operação automática
  `nvs_flash_erase`, `nvs_erase_all` ou `nvs_erase_key` foi encontrada em
  `coordinator_154/main`;
- a fonte contém 17 `TEST_CASE`, todos os rótulos AC usados no app declaram
  cobertura parcial, e os quatro novos cenários estruturais de AC-007 compilam.

Não houve execução comportamental. Nenhuma porta serial ESP32-C3/C6 foi
detectada, e flash/monitor não foram autorizados. QEMU não foi usado. Os builds
acima aprovam somente compilação e link; não aprovam nenhum AC.

**Achados materiais**

1. **Alto — o runner físico não corresponde à suíte atual.**
   `pytest_device_registry.py` ainda exige exatamente
   `13 Tests 0 Failures 0 Ignored`, mas a fonte contém 17 casos depois da
   adição dos quatro cenários de AC-007. Mesmo que os 17 casos terminem sem
   falha em placa, o runner não observará seu oráculo e falhará por timeout ou
   resultado ausente. Afeta a migração de validação v0.4 e impede produzir
   evidência terminal dos casos existentes em runner permitido.
2. **Alto — gates obrigatórios continuam incompletos.** G1 permanece
   inexistente; G3-N e G5 não foram implementados/executados; AC-005 continua
   sem caso integrado; e as classes de inicialização NVS e sentinela real de
   AC-007 permanecem ausentes. G3-F ainda cobre somente adaptador/core, sem
   observar ausência de resposta/publicação pela política integrada. Assim,
   nenhum AC possui todos os gates obrigatórios `Approved`, e a implementação
   não pode ser promovida para `Implemented` ou `Validated`.
3. **Médio — a precedência de `RegistryUnavailable` no comando do host ainda
   é incompleta** (COORD-REG-011/012, AC-007/G1). A correção distingue
   indisponibilidade de identidade desconhecida, mas
   `start_host_command()` ainda verifica `s_pending_command.active` antes de
   consultar `device_registry_state()`. Quando há comando pendente e registry
   indisponível ao mesmo tempo, o resultado final continua sendo
   `another command is pending`, embora a seção 5.1 determine que
   `RegistryUnavailable` preceda correlações e políticas operacionais. O efeito
   seguro de não transmitir é preservado, mas estado e observabilidade ainda
   violam o contrato.

**Correções confirmadas e limitações**

Os quatro rótulos que superestimavam AC-002/003/004/006 foram corrigidos para
`Partial`, e os quatro cenários novos ampliam cobertura estrutural de AC-007
sem alegar aprovação integral. Essas correções são válidas, porém não foram
executadas e não fecham G1 ou G3-N. A revisão não realizou correção de código,
teste ou runner e não recebeu validação humana nem aceite do Arquiteto.

**Recomendação ao Arquiteto:** não aceitar nem promover esta implementação.
Solicitar atuação corretiva de Engenheiro Implementador para alinhar o runner
aos 17 casos, aplicar a precedência integral no comando do host e completar
G1 e seus cenários; G3-N/G5 continuam dependendo de atuação física autorizada.
Depois, repetir revisão integral. Estados preservados: normativo `Proposed`,
implementação funcional e migração de validação `In Progress`, prontidão
`Not Ready`, revisão de implementabilidade `Implementable` e
`EKM-CHG-0008` `Open`.

### 16.18 Revisão técnica da correção de política e runner v0.4 (Engenheiro Revisor, 01/08/2026)

Recorte revisado: implementação integral disponível até o commit `072b641`,
COORD-REG-001 a 013, COORD-REG-AC-001 a AC-008, matriz de decisão da seção
9.1, gates G1 a G5, contrato dos substitutos e manifesto de evidências.
Nenhuma validação humana nem aprovação do Arquiteto foi recebida nesta ordem;
esta atuação registra apenas a revisão.

**Evidência terminal independente obtida nesta revisão:**

- build limpo da composição de produção com ESP-IDF 6.0.1 para ESP32-C6:
  código 0, `central_154.bin` com `0x45c60` bytes — idêntico ao tamanho
  declarado na seção 16.17;
- build limpo do app `device_registry_test` com ESP-IDF 6.0.1 para ESP32-C3:
  código 0;
- reconstrução e execução independentes de
  `test_apps/device_registry_policy_host_test` via `cmake`/`ctest`
  (host-native, fora de qualquer ambiente ESP-IDF): `1/1 Test ... Passed`,
  saída do binário `7 Tests 0 Failures 0 Ignored`, confirmando o resultado
  declarado na seção 16.17;
- leitura integral de `device_registry_policy.{h,c}`, `main.c` (pontos de
  entrada `DISCOVERY_REQ`, `DATA`, `ACK`, comando do host),
  `test_device_registry.c` (24 `TEST_CASE`, todos rotulados com sufixo
  `-partial-...`) e `pytest_device_registry.py` (exige exatamente
  `24 Tests 0 Failures 0 Ignored`);
- `grep` de `nvs_flash_erase`/`nvs_erase_all`/`nvs_erase_key` em
  `coordinator_154/main`: nenhuma ocorrência;
- `git diff --check`: concluído sem erro.

Os SHA-256 dos binários reconstruídos nesta revisão divergem dos publicados na
seção 16.17 porque o diretório de build usado foi diferente (caminho embutido
no artefato); os tamanhos em bytes, que não dependem do caminho, coincidem
exatamente e são o dado material de confronto.

**Correções confirmadas da seção 16.17:**

1. `device_registry_policy_host_command()` (`device_registry_policy.c`) agora
   avalia validade do endereço, depois disponibilidade do registry, depois
   comando pendente, depois identidade — nessa ordem — antes de decidir se um
   comando do host inicia. Quando comando pendente e `RegistryUnavailable`
   ocorrem juntos, o resultado passa a ser indisponibilidade, corrigindo o
   achado Médio nº3 da seção 16.16. `main.c` delega integralmente a essa
   função, sem lógica paralela própria. Cobre COORD-REG-011/012.
2. O runner físico (`pytest_device_registry.py`) foi realinhado à contagem
   real de casos do app Unity (24), corrigindo o achado Alto nº1 (parte do
   runner) da seção 16.16.
3. Nenhum rótulo `[AC-00N]` íntegro permanece no app Unity; todos os 24 casos
   usam sufixo `-partial-...`, preservando o contrato de rótulos da seção 13.
4. `device_registry_policy.c` é o mesmo arquivo-fonte usado por `main.c` e
   pelo teste host-native (confirmado pelo `CMakeLists.txt` do teste e pelo
   log de build), afastando a hipótese de segunda política paralela vedada
   pela seção 2.4.
5. Nenhuma chamada residual a `nvs_flash_erase`, `nvs_erase_all` ou
   `nvs_erase_key` existe em `coordinator_154/main`.

**Achados materiais:**

1. **Alto — G1, no sentido normativo da seção 13, continua inexistente.** Os
   sete casos "integrated ..." do app Unity (`test_device_registry.c`,
   linhas 581–663) e as sete funções do teste host-native chamam
   `device_registry_policy_discovery()`, `_data()`, `_ack()` e
   `_host_command()` diretamente, com parâmetros informados manualmente.
   Nenhum dos dois exercita o despacho real de `main.c` (recepção de frame,
   `handle_host_line()` etc.) nem observa se `main.c` de fato emite ou omite
   evento ao host, ACK ISSP, transmissão de comando ou chamada de persistência
   conforme a decisão devolvida pela política — a seção 13 exige que G1
   observe exatamente esses efeitos substituídos a partir do código
   efetivamente usado por `main.c` em execução, não apenas o retorno da
   função de decisão isolada. A inspeção estática dos blocos de aplicação de
   efeito em `main.c` (por exemplo, em torno das linhas 1452–1483 e 1503)
   sugere que eles ramificam de forma direta sobre a struct retornada, mas o
   perfil do Revisor veda declarar validação operacional apenas por inspeção
   estática. O rótulo `-partial-g1` usado nesses casos é honesto quanto a essa
   limitação e não a esconde, mas nenhum de AC-001, AC-003, AC-004, AC-005 e
   AC-006 pode ser promovido a `Approved` enquanto essa lacuna persistir, pois
   todos exigem G1 na matriz da seção 13. Reabre, sob nova causa técnica, a
   parte do achado Alto nº1 da seção 16.16 relativa a G1.
2. **Alto — G3-N e G5 continuam não executados.** Nenhuma execução em placa
   física com adaptador de produção sobre NVS real ou com rádio ocorreu nesta
   atuação nem nas anteriores desde a seção 16.12. AC-001, as classes de
   AC-007 dependentes de inicialização NVS e sentinela sob NVS real, e AC-008
   continuam sem essa evidência terminal. **Observação factual sem efeito
   normativo:** esta sessão de revisão detectou a porta serial
   `/dev/cu.usbmodem101`, ausente nas sessões registradas em 16.15 e 16.17;
   nenhuma tentativa de flash ou execução em hardware foi realizada, pois tal
   ação depende de ordem explícita do Arquiteto conforme `AGENTS.md`, não
   recebida nesta revisão. Reabre a parte de G3-N/G5 do achado Alto nº1 da
   seção 16.16.
3. **Médio — classes de AC-007 dependentes de inicialização NVS e sentinela
   sob NVS real permanecem ausentes de qualquer app de teste.** Confirmado
   por leitura integral de `test_device_registry.c`: nenhum dos 24 casos
   injeta `ESP_ERR_NVS_NO_FREE_PAGES`/`ESP_ERR_NVS_NEW_VERSION_FOUND` na
   inicialização nem verifica sentinela de namespace sob backend NVS real.
   Débito já registrado em 16.15/16.16, ainda não tratado nesta atuação.

**Limitações da revisão:** não houve tentativa de flash ou execução em
hardware físico, por ausência de ordem do Arquiteto; a porta serial
observada não foi caracterizada (não se confirmou tratar-se de um ESP32-C3
ou C6). Os SHA-256 recalculados nesta revisão não são diretamente comparáveis
aos da seção 16.17 pelo motivo já descrito; o confronto material usado foi o
tamanho em bytes dos binários.

**Recomendação ao Arquiteto:** não aceitar nem promover a implementação para
`Implemented` ou `Validated`. As duas correções da seção 16.17 são válidas,
verificadas de forma independente nesta revisão, e fecham os achados Médio
nº3 e parte do Alto nº1 (runner) da seção 16.16; ainda assim, nenhum AC está
integralmente `Approved`, porque G1 no sentido normativo e G3-N/G5 continuam
ausentes. Sugere-se: (i) uma atuação do Engenheiro Implementador que faça
`main.c` ser efetivamente exercitado sob G1, substituindo apenas os efeitos
de host/rádio/persistência na fronteira, em vez de invocar a política
isolada; (ii) decidir se autoriza uma atuação específica de flash/execução
em hardware físico, dado que uma porta serial está presente nesta sessão, o
que poderia avançar G3-N, G5, AC-001 e AC-008; (iii) quando houver placa
disponível, adicionar ao gate G3-N as classes de inicialização NVS e a
sentinela de namespace sob backend real. Estados preservados: normativo
`Proposed`, implementação funcional e migração de validação `In Progress`,
prontidão `Not Ready`, revisão de implementabilidade `Implementable` e
`EKM-CHG-0008` `Open`.
