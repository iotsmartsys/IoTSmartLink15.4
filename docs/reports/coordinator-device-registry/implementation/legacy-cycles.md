# Relatório de implementação — registry do coordenador

**Classe da fonte:** Relatório

**Papel:** Engenheiro Implementador

**Especificação:** `docs/specs/ISSP-Coordinator-Paired-Device-Registry.md`

**Revisão confrontada:** Registro histórico EKM 1.x preservado na migração para EKOM 3.2

**Estado:** Concluído

> Este relatório preserva uma atuação histórica e não altera fontes normativas.

### 16.2 Registro de implementação (Engenheiro Implementador, 31/07/2026)

Arquiteto autorizou implementação com recorte de escopo completo
(COORD-REG-001 a 013) nesta etapa.

**Código:**

- `coordinator_154/main/device_registry.h`/`.c`: schema versionado (seção 6),
  validação de carga (seção 7, tabela da seção 10) e transação de pareamento
  (seção 8) com um seam de storage (`device_registry_storage_t`) que isola a
  lógica de pareamento do acesso a NVS, conforme autorizado pela seção 2.3 da
  versão 0.1 (seção 2.4 atual);
- `coordinator_154/main/device_registry_nvs.c`: implementação real do seam
  sobre `nvs.h`, namespace próprio `coord_reg`, chave `devices`, isolado dos
  namespaces de fábrica/PHY/Wi-Fi/clients; substituição atômica via
  `nvs_set_blob` + `nvs_commit`, seguindo o precedente de
  `issp154_network_manager.cpp` citado na seção 15;
- `coordinator_154/main/main.c`: `device_registry_load()` executa antes de
  `iot154_radio_start_rx()` (seção 7); a tabela volátil `s_devices[8]` e as
  funções `is_duplicate()`/`find_device_by_ext_addr()` (que aprendiam
  qualquer origem — inclusive por `DATA` — com eviction cega do slot 0) foram
  removidas; `DISCOVERY_REQ` chama `device_registry_pair()` e só emite
  `DISCOVERY_RESP` de sucesso após o commit (seção 8); `DATA` de origem
  desconhecida com a janela fechada é descartado sem ACK, evento ou gravação
  (COORD-REG-007/008); a conclusão de comando por `ACK` passa a exigir também
  que a origem seja conhecida e que o `device_id` do frame corresponda ao
  persistido; comando do host resolve o destino via `device_registry_find()`;
  a deduplicação de `last_seq` permanece volátil (COORD-REG-009), agora como
  cache por slot do registry, nunca no blob;
- comportamento de `DATA` de origem desconhecida com a janela **aberta**
  permanece fora do recorte desta especificação (seção 9); a implementação
  preserva o processamento anterior (evento + ACK, sem persistência), por não
  haver decisão normativa para alterá-lo nesta etapa;
- logs `DEVICE_REGISTRY: ...` da seção 11 implementados; foram adicionados
  `discovery ignored reason=registry_unavailable` e `pairing result=failed
  reason=registry_unavailable`, além do vocabulário mínimo exigido, para
  tornar `RegistryUnavailable` observável (seção 7) sem reduzir os tokens
  obrigatórios.

**Testes automatizados:**

`coordinator_154/test_apps/device_registry_test` (Unity, alvo `esp32c3`,
execução prevista via QEMU `idf.py qemu`), seguindo o mesmo precedente de
`components/issp_app_154/test_apps/smart_sys_app_test`. Usa um substituto de
storage em memória que injeta falha de leitura/escrita e corrupção estrutural
— exercitando exclusivamente `device_registry.c`, nunca `device_registry_nvs.c`
nem uma partição NVS real. Cobre:

- COORD-REG-AC-002: falha de commit preserva a visão anterior em RAM e após
  recarregar a partir do que ficou persistido;
- COORD-REG-AC-003: capacidade cheia rejeita sem eviction, oito entradas
  permanecem íntegras após recarregar;
- COORD-REG-AC-004: repetição idêntica não grava; mudança de `device_id`
  grava exatamente uma vez e sobrevive a recarregar;
- COORD-REG-AC-006 (parcial): o blob serializado nunca contém `last_seq`;
- estados de carga usados por AC-001/AC-007: ausente, schema incompatível,
  blob truncado, contagem inválida, checksum inválido, erro real de leitura.

**Limitações registradas (não convertidas em evidência aprovada):**

- build ESP-IDF (alvo `esp32c6`) e a execução dos testes acima sob QEMU
  **não foram realizados nesta etapa**: este ambiente não possui `idf.py`
  nem toolchain ESP-IDF instalada. Nenhuma compilação nem execução é
  reivindicada como evidência;
- a parte de COORD-REG-AC-007 que exige NVS real com namespace sentinela
  isolado não foi automatizada nem executada — o gate escrito usa somente o
  substituto em memória;
- COORD-REG-AC-001 e COORD-REG-AC-008 exigem execução terminal em hardware
  real (seção 13) e permanecem pendentes, sem qualquer execução ou simulação
  nesta etapa.

**Estado resultante:** `In Progress`. Código e testes automatizáveis
obrigatórios estão escritos, mas a ausência de evidência de build/execução
neste ambiente impede a promoção para `Implemented` (regras comuns §3.2 e
perfil do Engenheiro Implementador). Promoção para `Validated` ou `Done`
não pertence a este papel e não foi solicitada.

### 16.11 Registro corretivo de implementação v0.3 (Engenheiro Implementador, 01/08/2026)

Esta atuação corrige os dois defeitos de runtime apontados na seção 16.3 e
amplia evidência automatizada local, sem promover o estado da implementação.

**Correções implementadas:**

- `init_nvs()` não chama mais `nvs_flash_erase()` para
  `ESP_ERR_NVS_NO_FREE_PAGES` nem `ESP_ERR_NVS_NEW_VERSION_FOUND`; registra a
  indisponibilidade e encerra controladamente antes de inicializar rádio ou
  tráfego de devices;
- `DISCOVERY_REQ`, `DATA`, `ACK` e comando do host agora consideram
  `RegistryUnavailable` antes de janela ou identidade. `DATA` indisponível é
  descartado sem evento/ACK, ACK não conclui comando e comandos do host não
  iniciam transmissão;
- `device_registry_nvs.c` ganhou uma tabela de primitivas NVS substituível
  somente em build de teste. O adaptador de produção continua sendo quem abre,
  chama `nvs_set_blob()`, chama `nvs_commit()` e propaga o resultado;
- o app Unity passou a representar durable, staging, commit e sentinela
  separadamente e executa a falha pós-staging de `nvs_commit()` atravessando o
  adaptador de produção. Também executa corrupções independentes de endereço e
  `device_id` que mantêm o checksum armazenado.

**Execuções terminais:**

| Critério | Cenário | Gate | Comando/alvo | Casos | Resultado | Classificação |
|---|---|---|---|---:|---|---|
| AC-002 | falhas separadas de set e commit com durable/sentinela; falha de commit pós-staging no adaptador | G2, G3-F | `idf.py -B "$TMPDIR/device_registry_test_build_c3" qemu`, ESP32-C3/QEMU | 2 | `13 Tests 0 Failures 0 Ignored` | Partial |
| AC-007 | checksum divergente após mutar endereço ou `device_id` | G2 | mesmo comando, ESP32-C3/QEMU | 1 | `13 Tests 0 Failures 0 Ignored` | Partial |
| AC-008 | compilação da composição de produção | G4 | `idf.py -B "$TMPDIR/coordinator_154_registry_build_c6" build`, ESP32-C6 | 1 | código 0, `central_154.bin` gerado | Partial |

As classificações permanecem `Partial`: não há G1 para observar os efeitos
integrados de `main.c`, G3-N com NVS real, nem a execução G5 em hardware exigida
por AC-001 e AC-008. AC-003, AC-004 e AC-006 ainda exercitam somente o core,
portanto não satisfazem os oráculos de rádio/host da seção 13. A implementação
permanece `In Progress`, a prontidão `Not Ready` e `EKM-CHG-0008` `Open`.

### 16.15 Registro corretivo de implementação v0.4 (Engenheiro Implementador, 01/08/2026)

Ordem recebida: atuar como Engenheiro Implementador sobre a v0.4. Condições de
entrada confirmadas: branch `gap0006-radio-diagnostics` derivada da `main`,
árvore de trabalho limpa, especificação `Implementable` conforme 16.14. Esta
atuação corrige o achado Médio nº2 da revisão 16.12 (precedência de
`RegistryUnavailable` no comando do host) e o achado Médio nº3 (rótulos de
teste que superestimavam evidência), e amplia a cobertura estrutural de
AC-007 em G2. Não fecha G1, G3-N nem G5; não reabre nem reclassifica os
achados Alto nº1 e Médio nº4 de 16.12, que permanecem válidos como débito.

**Correção de código:**

- `coordinator_154/main/main.c`, `start_host_command()`: a função consultava
  apenas `device_registry_find()` para decidir se um comando do host podia
  prosseguir; como esse lookup já retorna falso tanto para identidade
  desconhecida quanto para `RegistryUnavailable`, o efeito seguro (não
  transmitir) já existia, mas a mensagem de log e o erro devolvido ao host
  tratavam ambos os casos como `"target not known"`, contrariando a
  precedência da seção 5.1 (a distinção deve ser preservada até a decisão
  final de efeitos). A função passou a checar `device_registry_state()`
  explicitamente antes do lookup de identidade e a propagar um motivo
  distinto (`registry unavailable` vs. `target not known` vs. `another
  command is pending`) até `handle_host_line()`, via o novo tipo
  `host_command_start_result_t`. Nenhum efeito de rádio ou transmissão foi
  alterado; apenas a observabilidade e a precedência da decisão. Cobre
  COORD-REG-011/012.

**Correção de evidência (rótulos):**

- `coordinator_154/test_apps/device_registry_test/main/test_device_registry.c`:
  os quatro casos que a revisão 16.12 apontou como rotulados com o AC
  integral apesar de exercitarem somente `device_registry.c` isolado (sem
  efeito de rádio, host ou comando restaurado) passaram a usar sufixo
  `-partial-core` ou `-partial-schema`, conforme o contrato de rótulos da
  seção 13: `write failure preserves the previous entry across reboot`
  (`AC-002-partial-core`), `capacity full rejects a new address without
  evicting existing entries` (`AC-003-partial-core`), `repeated identical
  pairing does not write, changed device_id commits once`
  (`AC-004-partial-core`) e `persisted blob size matches schema exactly, with
  no room for last_seq` (`AC-006-partial-schema`). Nenhum destes testes teve
  seu comportamento alterado; apenas o rótulo, para deixar de superestimar a
  cobertura.

**Ampliação de cobertura AC-007 (G2, sem hardware):**

Quatro casos novos em `device_registry_test`, cobrindo classes da seção 13
ainda ausentes e executáveis sem NVS real: contagem de entradas acima da
capacidade (classe 2), endereço nulo e endereço broadcast em um blob
corretamente checksumado (classe 3), e endereço duplicado entre duas entradas
válidas com checksum recalculado para permanecer internamente consistente
(classe 4). Todos rotulados `[AC-007-partial-...]`, pois nenhum executa contra
NVS real nem observa a sentinela de namespace exigida pelo gate G3-N completo
do AC-007.

**Execuções terminais desta atuação:**

| Critério/alvo | Gate | Comando | Resultado | Classificação |
|---|---|---|---|---|
| Build de produção ESP32-C6 | G4 | `idf.py -B build_verify_c6 -DIDF_TARGET=esp32c6 build`, ESP-IDF 6.0.1 | código 0, `central_154.bin` `0x45bc0` bytes, zero warnings novos | Approved (G4 apenas) |
| Build do app `device_registry_test` ESP32-C3 | G4 | `idf.py -B build_verify_c3 -DIDF_TARGET=esp32c3 build`, ESP-IDF 6.0.1 | código 0, `device_registry_test.bin` `0x24110` bytes, zero warnings | Approved (G4 apenas) |

**Limitação ambiental verificada (seção 13, "Verificação do ambiente"):** o
ambiente ESP-IDF 6.0.1 (`~/.espressif/v6.0.1/esp-idf`) foi encontrado e
ativado com sucesso (`export.sh`, interpretador `python3.14`) — instalação
válida, não apenas ausente do `PATH`. Nenhuma porta serial ESP32-C3/C6 foi
detectada (`/dev/cu.usbserial*`, `/dev/cu.usbmodem*` ausentes): "instalação
encontrada mas placa física ausente nesta sessão". QEMU está proibido por
`Repository-Test-Execution-Policy.md` (`TESTEXEC-001`) e não foi usado.
Consequentemente, **nenhum caso Unity foi executado nesta atuação** — nem os
quatro casos novos de AC-007, nem os quatro casos relabelados, nem os já
existentes. G4 comprova apenas compilação e link; não constitui e não é
apresentado como evidência comportamental. Flash e execução em placa também
dependem de ordem explícita do Arquiteto (`AGENTS.md`), que não foi recebida
nesta atuação.

**Débitos explicitamente preservados (não fechados nesta atuação):**

- G1 continua inexistente: nenhum teste exercita o código de decisão integrado
  de `main.c` (`DISCOVERY_REQ`, `DATA`, `ACK`, comando do host) com efeitos
  substituídos. AC-001, AC-003, AC-004, AC-005 e AC-006 continuam sem a
  evidência integrada que a seção 13 exige; AC-005 continua sem qualquer caso,
  integrado ou não;
- G3-N (adaptador de produção com NVS real em placa física) e G5 (hardware
  ponta a ponta) continuam não executados, por ausência de placa nesta sessão;
- as classes de AC-007 dependentes de `ESP_ERR_NVS_NO_FREE_PAGES` /
  `ESP_ERR_NVS_NEW_VERSION_FOUND` na inicialização e a sentinela de namespace
  sob NVS real continuam fora do app de teste, pois dependem de `main.c`
  (G1) ou de NVS real em placa (G3-N), não apenas de `device_registry.c`;
- o achado Alto nº1 de 16.12 (ausência de G1/G3-N/G5 como um todo) e o achado
  Médio nº4 (G3-F parcial, sem observar ausência de resposta/publicação pela
  política integrada) permanecem válidos e não foram tratados nesta atuação;
- nenhum caso automatizado, novo ou preexistente, foi executado nesta sessão;
  toda a evidência comportamental de execuções anteriores (QEMU) permanece
  apenas histórica, conforme `TESTEXEC-006`.

**Estado resultante:** implementação `In Progress`; migração de validação
`In Progress`; prontidão `Not Ready`; `EKM-CHG-0008` `Open`. Esta atuação
corrige dois achados Médios de 16.12 com evidência de compilação limpa e
amplia a cobertura estrutural fonte de AC-007, mas não promove nenhum AC a
`Approved` nem a implementação a `Implemented`: G1, G3-N, G5, a execução
comportamental de qualquer caso e as classes de AC-007 dependentes de
inicialização NVS continuam pendentes. Promoção para `Validated` ou `Done`
não pertence a este papel e não foi solicitada. Recomenda-se ao Arquiteto uma
atuação futura dedicada a extrair o código de decisão de `main.c` para uma
forma testável (G1), condição necessária para aprovar integralmente AC-001,
AC-003, AC-004, AC-005 e AC-006, e a obter acesso a placa física ESP32-C3/C6
para fechar G3-N e G5.

### 16.17 Implementação corretiva de política e runner v0.4 (Engenheiro Implementador, 01/08/2026)

Ordem recebida: atuar como Engenheiro Implementador sobre a especificação
integral. Condições de entrada confirmadas: branch
`gap0006-radio-diagnostics`, derivada da `main`, árvore limpa no início e
revisão de implementabilidade `Implementable`.

**Resultado material:**

- criados `coordinator_154/main/device_registry_policy.{h,c}` como abstração
  local autorizada pela seção 2.4. O mesmo código agora decide a precedência e
  os efeitos permitidos para `DISCOVERY_REQ`, `DATA`, `ACK` e comando do host
  tanto no runtime de produção quanto nos testes;
- `main.c` passou a consumir essas decisões. Para comando do host, a ordem é
  validade do endereço, disponibilidade do registry, comando pendente e
  identidade. Assim, a combinação comando pendente + registry indisponível
  retorna `registry unavailable`, corrigindo o achado nº3 da seção 16.16;
- o runner físico foi alinhado à suíte corrente e agora exige exatamente
  `24 Tests 0 Failures 0 Ignored`: os 17 casos anteriores mais sete casos de
  política compartilhada. Isso corrige o achado nº1 da seção 16.16;
- criado `test_apps/device_registry_policy_host_test`, runner host-native que
  compila o próprio `device_registry_policy.c` com stubs mínimos apenas para
  os tipos não materiais de ESP-IDF. Ele confronta precedência de discovery,
  resposta somente após resultado persistente de pareamento, fail-closed de
  `DATA`, origem desconhecida com janela aberta/fechada, deduplicação, ACK e
  comando do host.

**Inventário de entradas e efeitos:**

| Entrada | Código compartilhado usado por `main.c` | Precedência/efeitos |
|---|---|---|
| `DISCOVERY_REQ` | `device_registry_policy_discovery()` e `device_registry_policy_discovery_response()` | unavailable antes da janela; resposta somente para known/updated/created |
| `DATA` | `device_registry_policy_data()` | unavailable sem evento/ACK; desconhecido fechado sem efeitos; conhecido preserva evento/dedup/ACK |
| `ACK` | `device_registry_policy_ack()` | unavailable não conclui; Ready exige identidade e todas as correlações |
| comando do host | `device_registry_policy_host_command()` | validade, unavailable, pending, identidade; só então inicia TX |

Varredura das operações NVS destrutivas em `coordinator_154/main` não encontrou
`nvs_flash_erase`, `nvs_erase_all` nem `nvs_erase_key`. A política não contém
operação de persistência; `DATA`, `ACK` e comando não podem criar entrada.

**Evidência terminal desta atuação:**

| Critério | Cenário | Gate | Teste/comando | Alvo/ambiente | Casos | Resultado | Oráculo | Classificação/limitação |
|---|---|---|---|---|---:|---|---|---|
| AC-001/002/003/005/006/007/008 | decisões compartilhadas de discovery, DATA, ACK e host | G1 | `cmake -S test_apps/device_registry_policy_host_test -B /tmp/device_registry_policy_host_test_build -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/device_registry_policy_host_test_build && ctest --test-dir /tmp/device_registry_policy_host_test_build --output-on-failure` | host-native, AppleClang 21, stubs somente de tipos | 7 | código 0; `7 Tests 0 Failures 0 Ignored` | ações e flags de evento, ACK, logs, resposta, conclusão e TX permitida | `Partial`: não executa ordem integral de callbacks, reboot/NVS real nem rádio |
| AC-001..008 | fonte Unity ampliada | G1/G2/G3-F parciais | `idf.py -B build_impl_c3 -DIDF_TARGET=esp32c3 build` | ESP-IDF 6.0.1, ESP32-C3 | 24 escritos, 0 executados | código 0; binário `0x24a30` bytes, SHA-256 `311e92fbcaa6bf289811bdbe27334c809f174ec6129f8ecba783fb4a74006889` | compilação/link dos casos e política real | `Not Executed` comportamentalmente; G4 de app apenas |
| AC-008 | composição de produção | G4 | `idf.py -B build_impl_c6 -DIDF_TARGET=esp32c6 build` | ESP-IDF 6.0.1, ESP32-C6 | 1 build | código 0; `central_154.bin` `0x45c60` bytes, SHA-256 `a672c78cc69da90a9659e34b4fb6c304569d43d8a86ae9401d7b2076ec26f568` | `main.c` ligado ao código compartilhado sem erro do compilador | `Approved` para G4 apenas |

A primeira tentativa do build de produção não encontrou `idf.py` no `PATH`, e
a segunda expôs ambiente Python não selecionado; ambas terminaram com erro. A
terceira ativou explicitamente o Python 3.14 da instalação ESP-IDF 6.0.1 e
terminou com código 0, sendo a evidência considerada.

Na reconstrução terminal dos artefatos finais, a tentativa sem ativar o
ambiente selecionou Python 3.11 e falhou antes do build; a primeira tentativa
C3 foi bloqueada pelo sandbox na consulta `psutil`. As repetições com o
ambiente Python 3.14 explícito e, para C3, permissão somente de build,
terminaram com código 0. Nenhuma dessas tentativas executou flash ou testes.

**Confronto adversarial e débitos preservados:** uma política que inverta
unavailable/pending, permita efeitos de `DATA` indisponível, conclua ACK sem
identidade ou responda a pairing failed reprova o runner host-native. Porém,
uma integração que deixe de executar um callback esperado ainda pode escapar
do teste puramente decisório; por isso G1 e G3-F permanecem parciais para os
ACs que exigem ordem e efeitos completos. G3-N e G5 continuam `Not Executed`:
nenhuma porta ESP32-C3/C6 foi detectada, e não houve ordem de flash/monitor.
As classes de inicialização NVS e a sentinela contra NVS real de AC-007 também
permanecem pendentes. QEMU não foi usado.

**Estado resultante:** implementação funcional e migração de validação
`In Progress`, prontidão `Not Ready`, revisão `Implementable` e
`EKM-CHG-0008` `Open`. Nenhum AC é promovido integralmente a `Approved` e a
implementação não é promovida para `Implemented`, pois os gates físicos e os
oráculos integrados restantes ainda são obrigatórios.
