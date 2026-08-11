# Relatório de análise — registry do coordenador

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/ISSP-Coordinator-Paired-Device-Registry.md`

**Revisão confrontada:** Registro histórico EKM 1.x preservado na migração para EKOM 3.2

**Estado:** Concluído

> Este relatório preserva uma atuação histórica e não altera fontes normativas.

### 16.1 Revisão de implementabilidade (Engenheiro Analista, 31/07/2026)

Confronto entre requisitos, `docs/specs/ISSP-Commissioning.md`,
`docs/specs/ISSP-Architecture.md`, `coordinator_154/main/main.c` e
`coordinator_154/main/iot154_packet.h`:

- os fatos da seção 2.1 correspondem ao estado real do firmware: `s_devices[8]`
  em RAM sem persistência, `is_duplicate()` cria/atualiza entrada tanto para
  `DISCOVERY_REQ` quanto para `DATA` sem checar janela, e `find_device_by_ext_addr`
  é a única fonte de destino de comando — nenhum fato foi inventado por
  inferência;
- os treze requisitos possuem critério assertável e cobertura na matriz
  requisito–critério (seção 13); nenhum requisito obrigatório ficou sem AC;
- a solução proposta (seção 2.3 da versão 0.1; seção 2.4 atual) não introduz
  nova camada arquitetural de
  domínio; identifica padrão atual, mudança, alcance e justificativa da
  abstração interna de NVS, satisfazendo a exigência de precisão arquitetural
  do perfil do Analista;
- `coordinator_154` não possui hoje separação em componente nem `test_apps`
  (diferente de `components/issp_app_154`). Isso não bloqueia a
  implementabilidade: a especificação já autoriza expressamente uma abstração
  interna pequena para permitir substituto de NVS, e o precedente mais próximo
  do repositório (`SmartSysApp::SetupHooks` + `components/issp_app_154/test_apps`
  sob QEMU) resolve a lacuna de "como" sem exigir nova decisão do Arquiteto;
  fica registrado como observação para o Engenheiro Implementador, não como
  bloqueio;
- nenhum conflito material foi encontrado entre esta especificação e
  `ISSP-Commissioning.md`/`ISSP-Architecture.md`; a janela de ingresso, o
  protocolo wire e a deduplicação volátil permanecem preservados conforme
  seção 4.

Resultado: `Implementable`. Autorização para iniciar implementação depende de
ordem própria do Arquiteto.

### 16.6 Revisão de implementabilidade v0.2 (Engenheiro Analista, 01/08/2026)

Confronto independente da versão 0.2 com `docs/specs/ISSP-Commissioning.md`,
`docs/specs/ISSP-Architecture.md`, o baseline real de
`coordinator_154/main/{main.c,device_registry.h,device_registry.c,device_registry_nvs.c}`,
`coordinator_154/test_apps/device_registry_test` e o precedente de
`components/issp_app_154/test_apps`. A conclusão histórica da seção 16.1 não foi
reutilizada como aprovação.

**Requisitos, critérios e matriz**

- COORD-REG-001 a 013 permanecem cobertos por AC-001 a AC-008 e pela matriz
  requisito–critério da seção 13; nenhum requisito obrigatório ficou sem
  oráculo assertável;
- a matriz da seção 9.1 cobre discovery, `DATA`, `ACK` e comando do host sob
  `RegistryReady` e `RegistryUnavailable`; a única célula deliberadamente não
  normatizada (`DATA` desconhecido com janela aberta em `Ready`) declara
  explicitamente dualidade de aceite operacional com proibição de persistência,
  o que basta para o gate sem nova decisão de produto;
- a precedência da seção 5.1 remove a ambiguidade que permitia tratar
  `RegistryUnavailable` como origem desconhecida; o contrato de staging,
  durable, commit e reboot da seção 6 torna falsificáveis AC-002 e AC-007;
- G1 a G5, o contrato dos substitutos, o manifesto AC–teste–gate–resultado e as
  varreduras de conformidade tornam a evidência parcial distinguível de
  aprovação, sem exigir decisão normativa adicional.

**Arquitetura e precedentes**

- a seção 2.4 identifica o padrão atual (`coordinator_154` concentrado em
  `main.c`), autoriza apenas abstrações locais para política compartilhada e
  adaptador NVS, e proíbe nova camada de domínio compartilhada; isso satisfaz a
  exigência de precisão arquitetural do perfil do Analista;
- o precedente local já existente
  (`device_registry` + seam de storage + `test_apps/device_registry_test`) e o
  precedente de QEMU em `components/issp_app_154/test_apps` resolvem o “como”
  dos gates G1–G3 sem nova pasta estrutural ou componente transversal;
- não há conflito material com commissioning nem com a arquitetura: a janela de
  60 s, a continuidade de devices conhecidos após o fechamento e a preservação
  do protocolo wire permanecem intactas. A frase de commissioning sobre
  “atualização do registry” pelo report inicial não define criação de entrada
  persistente no coordenador; a decisão do Arquiteto em 31/07/2026 e as seções
  4/8/9 desta especificação já fixam pareamento exclusivo por discovery.

**Baseline de implementação observado (fato, não requisito)**

- `init_nvs()` ainda chama `nvs_flash_erase()` em
  `ESP_ERR_NVS_NO_FREE_PAGES`/`ESP_ERR_NVS_NEW_VERSION_FOUND`;
- `DATA` com janela aberta ainda produz evento e ACK quando
  `device_registry_find()` falha, inclusive sob registry indisponível;
- o app de teste atual cobre apenas um subconjunto parcial dos ACs e usa fake de
  storage de buffer único, insuficiente para AC-002/AC-007 completos;
- esses desvios são correções e ampliações de evidência da implementação
  `In Progress`, não lacunas de especificação.

**Resultado:** `Implementable`.

Nenhuma decisão normativa, de produto ou de arquitetura ausente foi identificada
para executar o recorte completo. Esta promoção não autoriza implementação nem
aceita o baseline atual; correção, gates e revisão técnica dependem de ordem
própria do Arquiteto. Estados preservados: normativo `Proposed`, implementação
`In Progress`, prontidão `Not Ready`. Nenhum código, teste ou configuração de
implementação foi alterado nesta análise.

### 16.8 Revisão de implementabilidade independente (Engenheiro Analista, 01/08/2026)

Ordem recebida: atuar como Engenheiro Analista sobre a especificação v0.2
corrente. Condições de entrada confirmadas: branch `gap0006-radio-diagnostics`
derivada da `main`, árvore limpa, especificação em `Proposed` com revisão
corrente `Needs Clarification` após a seção 16.7. A promoção histórica da
seção 16.6 e o encerramento corretivo da 16.7 não foram reutilizados como
aprovação nem como recusa automática; o texto normativo e as fontes técnicas
foram confrontados de novo.

**Fontes confrontadas**

- `docs/specs/ISSP-Coordinator-Paired-Device-Registry.md` (texto normativo
  completo e seções 16.1–16.7 como histórico);
- `docs/specs/ISSP-Commissioning.md` e `docs/specs/ISSP-Architecture.md`;
- baseline verificável de
  `coordinator_154/main/{main.c,device_registry.h,device_registry.c,device_registry_nvs.c}`;
- `coordinator_154/test_apps/device_registry_test` e o precedente de
  `components/issp_app_154/test_apps`;
- `docs/rfc/KNOWLEDGE-MAP.md` e `docs/rfc/EKM-CHANGELOG.md` (`EKM-CHG-0008`).

Nenhum código, teste ou configuração de implementação foi alterado ou
executado nesta análise.

**O que permanece implementável sem nova decisão**

- COORD-REG-001 a 013 continuam cobertos por AC-001 a AC-008 e pela matriz
  requisito–critério; a matriz 9.1 e a precedência 5.1 fecham a política de
  discovery, `DATA`, `ACK` e comando sob `Ready` e `Unavailable`;
- a célula deliberadamente aberta (`DATA` desconhecido com janela aberta em
  `Ready`) declara dualidade de aceite operacional e proíbe persistência; isso
  não exige decisão de produto adicional para o gate;
- a seção 2.4 identifica o padrão atual, autoriza apenas abstrações locais de
  política e adaptador NVS e proíbe nova camada de domínio; o precedente
  `device_registry*` + `test_apps` + QEMU de `issp_app_154` basta para o
  “como” de G1–G3;
- não há conflito material com commissioning nem com a arquitetura: a janela
  de 60 s, a continuidade de devices conhecidos e a preservação do protocolo
  wire permanecem intactas; a frase de commissioning sobre “atualização do
  registry” pelo report inicial não define criação de entrada persistente no
  coordenador e já está delimitada pelas seções 4/8/9 e pela decisão do
  Arquiteto de 31/07/2026;
- os desvios do baseline (`nvs_flash_erase` em `init_nvs`, fail-closed
  incompleto de `DATA` sob `RegistryUnavailable`, fake de buffer único e
  ausência de gates G1 integrais) são débitos da implementação `In Progress`,
  não lacunas normativas novas.

**Decisões ainda ausentes (bloqueantes)**

1. **Integridade do blob versus oráculo de corrupção.** A seção 6 declara que
   o formato físico *pode* incluir tamanho, marcador, checksum ou outros
   metadados de integridade, sem tornar obrigatório ao menos um mecanismo
   verificável além da coerência de tamanho. Em contrapartida, a tabela da
   seção 10 e a classe 5 de AC-007 exigem produzir e reprovar “checksum ou
   marcador de integridade inválido” como `RegistryUnavailable`. Enquanto a
   obrigatoriedade do mecanismo permanecer opcional, um implementador pode
   omitir checksum/marcador e tornar essa classe de AC-007 inexequível, ou
   inventar o mecanismo só no teste. O Autor deve decidir se o schema exige
   ao menos um mecanismo verificável de integridade do conteúdo (e alinhar
   seção 6, invariantes, seção 10 e AC-007) ou se a classe de corrupção por
   checksum/marcador é condicional à presença do campo — hipótese que enfraquece
   o oráculo e também precisa ficar explícita.

2. **Falha de commit no adaptador NVS de produção.** AC-002 e a matriz
   critério–gate exigem falhas separadas de `set_blob` e de commit apenas em
   G1+G2, que podem usar substituto fiel. G3 exercita o adaptador de produção
   nos caminhos nominais de AC-001 e nas corrupções/sentinela de AC-007, mas
   não obriga atravessar `device_registry_nvs.c` sob falha de `nvs_commit()`
   após staging bem-sucedido. Um adaptador real que ignore ou engula falha de
   commit pode, em tese, satisfazer os gates escritos. O Autor deve exigir
   evidência terminal que atravesse o adaptador de produção sob falha de
   commit (por exemplo, ampliar AC-002 ou AC-007/G3 com esse cenário) ou
   definir gate material equivalente que feche essa possibilidade sem depender
   só do fake.

Nenhuma outra decisão normativa, de produto ou de arquitetura ausente foi
identificada para o recorte. Em particular, capacidade 8, identidade IEEE,
`last_seq` volátil, fail-closed de `RegistryUnavailable`, proibição de
`nvs_flash_erase` automático e pareamento exclusivo por discovery já estão
decididos.

**Resultado:** `Needs Clarification`.

Estados preservados: normativo `Proposed`, implementação `In Progress`,
prontidão `Not Ready`, revisão de implementabilidade `Needs Clarification` e
transação `EKM-CHG-0008` `Open`. A especificação permanece com o Autor até as
duas decisões acima serem materializadas no texto normativo e nos gates. Esta
análise não autoriza implementação nem aceita o baseline atual.

### 16.10 Revisão de implementabilidade v0.3 (Engenheiro Analista, 01/08/2026)

Confronto independente da versão 0.3 com os requisitos e critérios completos,
`ISSP-Commissioning.md`, `ISSP-Architecture.md`, o baseline verificável de
`coordinator_154/main/{main.c,device_registry.h,device_registry.c,device_registry_nvs.c}`,
o app `coordinator_154/test_apps/device_registry_test` e o precedente de hooks
e QEMU de `components/issp_app_154/test_apps`.

**Contratos e critérios**

- COORD-REG-001 a 013 permanecem cobertos por AC-001 a AC-008 e pelas duas
  matrizes da seção 13; cenário, ação, resultado, gate e classificação terminal
  continuam explícitos;
- a seção 6 exige valor de integridade definido pelo schema e cobrindo versão,
  contagem e todos os bytes das entradas. AC-007 parte de blob estruturalmente
  válido e corrompe separadamente endereço e `device_id`, sem atualizar o
  valor, impedindo que marcador constante ou cobertura parcial aprove o gate;
- AC-002 exige G1+G2+G3-F. G3-F observa `nvs_set_blob()` bem-sucedido, erro
  posterior de `nvs_commit()`, propagação pelo adaptador, ausência de publicação
  e resposta, durable anterior após reabertura/reboot e sentinela preservada;
- G3-N mantém separado o oráculo com adaptador de produção e NVS real. Assim,
  o substituto de falhas não é convertido indevidamente em prova de
  persistência nominal real.

**Arquitetura e viabilidade**

- a seção 2.4 identifica o padrão atual, limita a mudança a
  `coordinator_154`, autoriza o ponto estreito de injeção na fronteira NVS e
  registra sua justificativa; não cria componente transversal nem altera
  protocolo, clients ou componentes ISSP compartilhados;
- tabela interna de operações, wrapper de link ou mecanismo equivalente são
  alternativas técnicas suficientes para G3-F. Em todas, a decisão de chamar
  `nvs_set_blob()`, chamar `nvs_commit()` e propagar o resultado continua no
  próprio `device_registry_nvs.c`, evitando uma política paralela de teste;
- o precedente local de hooks e test apps sob QEMU confirma que a mudança pode
  ser materializada sem nova pasta estrutural ou decisão arquitetural;
- não há conflito material com commissioning ou arquitetura: janela de 60 s,
  continuidade dos devices conhecidos, protocolo wire, identidade IEEE e
  deduplicação volátil permanecem preservados.

**Revisão adversarial**

- implementação sem valor de integridade, com marcador constante ou que ignore
  endereço ou `device_id` reprova as mutações independentes de AC-007;
- fake correto com adaptador real incorreto não aprova AC-002, pois G3-F exige
  executar `device_registry_nvs.c` e observar o erro devolvido ao core;
- retorno injetado antes de `nvs_set_blob()`, publicação em RAM antes do commit,
  resposta após erro, durable alterado ou sentinela perdida reprovam os oráculos
  expressos do critério;
- código e testes existentes ainda não atendem todos os requisitos e gates,
  mas esses são débitos verificáveis da implementação `In Progress`, não
  decisões ausentes da especificação.

**Resultado:** `Implementable`.

Todo o recorte pode ser executado sem inferência normativa, de produto ou
arquitetura relevante. Esta promoção não aceita a implementação existente, não
autoriza programar e não converte gates não executados em evidência. Estados
preservados: normativo `Proposed`, implementação `In Progress`, prontidão
`Not Ready` e `EKM-CHG-0008` `Open`. Nenhum código, teste ou configuração de
implementação foi alterado ou executado nesta análise.

### 16.14 Revisão de implementabilidade v0.4 (Engenheiro Analista, 01/08/2026)

A versão 0.4 foi confrontada integralmente com COORD-REG-001 a 013,
COORD-REG-AC-001 a AC-008, matriz de decisão, gates G1 a G5, contratos dos
substitutos, manifesto de evidências e fontes relacionadas. Foram também
confrontados o baseline atual de `coordinator_154/main`, o app de teste do
registry, seu runner físico e a política transversal de execução de testes.

**Requisitos, critérios e ambientes**

- os treze requisitos continuam cobertos pelos oito critérios e pela matriz
  requisito–critério; cada AC declara condição inicial, ação, resultado
  observável, evidência que reprova e condição que permanece `Not Executed` ou
  `Partial`;
- a retirada de QEMU não remove cenário, falha, oráculo nem gate. G1 e G2 podem
  executar host-native somente com substitutos fiéis e possuem fallback em
  placa; G3-N exige NVS real em ESP32-C3/C6 físico; G3-F conserva o adaptador
  de produção sob primitivas controladas; G4 continua sendo o build ESP32-C6 e
  G5 continua sendo execução física ponta a ponta;
- a política `Repository-Test-Execution-Policy.md` está `Active` e é compatível
  com essas escolhas. Evidência QEMU anterior permanece apenas histórica e não
  pode aprovar a v0.4;
- indisponibilidade de `pytest`, placa ou porta serial possui resultado
  determinado (`Not Executed`) e não exige decisão de produto. Flash, monitor
  e captura continuam dependentes de ordem explícita do Arquiteto.

**Arquitetura e viabilidade**

- a seção 2.4 identifica o padrão atual concentrado em `main.c`, limita a
  mudança a `coordinator_154` e autoriza uma abstração local para que o mesmo
  código de decisão governe produção e G1. Assim, completar G1 não exige criar
  componente transversal nem inventar política paralela;
- os seams atuais `device_registry_storage_t` e
  `device_registry_nvs_ops_t`, o app Unity e o runner ESP32-C3 demonstram
  pontos técnicos suficientes para G2 e G3-F. G3-N possui caminho explícito
  pelo adaptador de produção e NVS real em placa;
- commissioning, janela de 60 segundos, identidade IEEE, protocolo wire,
  capacidade oito, deduplicação volátil e continuidade dos devices conhecidos
  permanecem compatíveis com `ISSP-Commissioning.md` e
  `ISSP-Architecture.md`;
- a implantação de clients preexistentes está delimitada na seção 14 e exige
  ordem própria por ambiente, mas não deixa comportamento normativo em aberto.

**Confronto adversarial e débitos existentes**

- executar somente os 13 casos atuais em placa não aprova os ACs integrais: o
  manifesto e a matriz de gates continuam exigindo G1, G3-N e G5 onde
  aplicáveis;
- usar host-native para NVS real ou rádio reprova os gates correspondentes;
  reutilizar resultado QEMU histórico também não promove estado;
- G1 ainda não existe, G3-N e G5 não foram executados, AC-005 não possui caso
  integrado e vários cenários atuais são parciais. O comando do host ainda não
  conserva explicitamente `RegistryUnavailable` até o resultado observável e
  os rótulos integrais de testes parciais ainda precisam ser corrigidos;
- esses pontos possuem resultado esperado, gate e oráculo definidos pela
  especificação. São débitos verificáveis da implementação `In Progress`, não
  decisões normativas, de produto ou arquitetura ausentes.

**Resultado:** `Implementable`.

Toda a versão 0.4 pode ser executada sem inferência relevante. Esta promoção
não aceita a implementação existente, não autoriza programação, flash ou
hardware e não converte build, runner escrito ou evidência histórica em teste
aprovado. Estados preservados: normativo `Proposed`, implementação funcional
`In Progress`, migração de validação `In Progress`, prontidão `Not Ready` e
`EKM-CHG-0008` `Open`. Nenhum código, teste ou arquivo de configuração de
implementação foi alterado ou executado nesta análise. Uma nova ordem do
Arquiteto é necessária para atuação de Engenheiro Implementador.
