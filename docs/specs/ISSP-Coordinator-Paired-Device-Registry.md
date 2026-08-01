# ISSP 802.15.4 — Registry de Dispositivos Pareados do Coordenador

**Tipo:** Normativo
**Estado normativo:** Proposed
**Estado da implementação:** In Progress
**Prontidão:** Not Ready
**Revisão de implementabilidade:** Pending Review
**Versão:** 0.2
**Responsável arquitetural:** Marcelo Miranda
**Última atualização:** 01/08/2026
**Escopo:** Persistência NVS dos dispositivos pareados pelo coordenador ISSP

---

## 1. Objetivo

Definir um registry persistente no coordenador que preserve os dispositivos
aceitos durante commissioning e permita reconhecê-los após reinicializações.

O coordenador deve usar esse registry para:

- manter a associação entre o endereço IEEE 802.15.4 e o `device_id` ISSP;
- restaurar os destinos de comandos depois do boot;
- distinguir dispositivos pareados de origens desconhecidas;
- preservar os pareamentos quando a janela de ingresso for fechada ou o
  coordenador for reiniciado.

## 2. Contexto e decisões

### 2.1 Baseline anterior à primeira implementação

Antes do commit `2cc600c`, o firmware:

- inicializa a partição NVS, mas não persiste dispositivos do coordenador;
- mantém em RAM uma tabela fixa de oito entradas contendo endereço estendido,
  `device_id` e última sequência recebida;
- aprende ou substitui entradas quando recebe discovery ou data;
- perde toda a tabela ao reiniciar;
- procura nessa tabela o destino de comandos enviados pelo host;
- aceita `DATA` de origens ainda não registradas, mesmo com a janela fechada.

A especificação de commissioning vigente já determina que, após o fechamento
da janela, o coordenador continue atendendo dispositivos conhecidos e não
remova dispositivos registrados. Ela não define ainda o armazenamento do
registry do coordenador.

### 2.2 Estado observado depois da primeira implementação

O commit `2cc600c` introduziu um registry local, um adaptador NVS, integração
com `main.c` e dez testes sob um substituto de storage. A revisão independente
de 01/08/2026 confirmou build ESP32-C6 e execução QEMU `10/10`, mas também
confirmou que o resultado ainda não atende integralmente a esta especificação:

- `DATA` ainda produz evento e ACK quando o registry está indisponível e a
  janela está aberta;
- a inicialização preexistente ainda pode executar `nvs_flash_erase()` e apagar
  namespaces não relacionados;
- não existe gate automatizado para AC-005;
- sob os oráculos completos desta versão, AC-002, AC-003, AC-004, AC-006 e
  AC-007 possuem somente evidência parcial;
- AC-001 e AC-008 permanecem sem execução terminal em hardware real.

Esses fatos não alteram o comportamento normativo. Código e testes existentes
são baseline de implementação a corrigir e evidência parcial, não fonte de
novos requisitos.

### 2.3 Intenção confirmada pelo Arquiteto

O Arquiteto confirmou em 31/07/2026 que:

1. o pareamento nasce de `DISCOVERY_REQ` válido aceito durante a janela;
2. a confirmação persistente precede a `DISCOVERY_RESP` de sucesso;
3. o endereço IEEE de 8 bytes é a identidade primária e o `device_id` também é
   persistido;
4. depois do fechamento da janela, tráfego operacional somente é aceito para
   dispositivos persistidos;
5. o limite permanece em oito dispositivos, sem substituição automática;
6. novo discovery do mesmo endereço pode atualizar o `device_id`;
7. `last_seq` permanece volátil;
8. falha de gravação não conclui o pareamento;
9. registry ausente ou com schema incompatível inicia vazio;
10. corrupção ou erro real de leitura é registrado sem apagar automaticamente
    toda a NVS;
11. remoção individual, factory reset do coordenador, autenticação e migração
    automática de entradas voláteis ficam fora deste recorte.
12. a nova autoria preserva o escopo funcional completo e deve tornar estados,
    invariantes, critérios, substitutos e evidências objetivamente
    confrontáveis, sem reduzir os treze requisitos.

### 2.4 Solução proposta e autorização arquitetural limitada

Esta especificação propõe um único blob versionado e atomicamente substituível
em namespace próprio do coordenador. A escolha dos nomes internos da chave e
do namespace é detalhe de implementação, desde que o namespace não seja
compartilhado com dados de fábrica, PHY, Wi-Fi ou vínculos persistidos pelos
clients.

O padrão atual afetado é o firmware standalone `coordinator_154`, concentrado
em `main.c`, com acesso direto a NVS, rádio e UART. A mudança permanece local a
esse firmware: não cria componente de domínio compartilhado nem altera
`issp_core`, `issp_transport_154`, protocolo wire ou clients.

São autorizadas abstrações internas pequenas e limitadas a `coordinator_154`
para:

- separar schema, validação e transação do registry do adaptador NVS;
- tornar explícita e testável a decisão de aceitar ou rejeitar
  `DISCOVERY_REQ`, `DATA`, `ACK` e comandos conforme estado do registry, janela
  e identidade da origem;
- substituir, em testes, operações NVS, efeitos de rádio, eventos ao host,
  relógio/reboot e resultados de inicialização necessários aos critérios.

A justificativa é permitir que os critérios obrigatórios sejam falsificáveis
sem duplicar a lógica de produção no teste. Essas abstrações não podem criar
uma segunda política paralela: o mesmo código de decisão exercitado pelos
gates automatizados deve governar os efeitos usados pelo runtime de produção.

## 3. Escopo

Inclui:

- schema lógico e validação do registry;
- carregamento no boot;
- criação e atualização por discovery;
- limite e comportamento de capacidade esgotada;
- uso do registry para reports, ACKs e comandos;
- falhas de leitura, validação e gravação;
- logs e evidências exigidas;
- compatibilidade de implantação.

## 4. Fora do escopo

- remoção individual de dispositivo;
- limpeza ou factory reset do registry do coordenador;
- migração automática da tabela volátil anterior;
- importação de devices a partir de reports, comandos do host ou descritores
  NVS dos clients;
- aumento dinâmico do limite de oito entradas;
- persistência de `last_seq`, filas, comandos pendentes ou capabilities;
- reabertura remota ou por botão da janela de ingresso;
- autenticação criptográfica, autorização por capability ou proteção contra
  clonagem de endereço IEEE;
- alteração do payload, checksum, endianness ou tipos do protocolo ISSP;
- alteração de canal, PAN ID, duração da janela, retries ou tempos de rádio;
- política de perda, roaming ou seleção de coordenador.

## 5. Terminologia e identidade

- **Pareado:** dispositivo cuja entrada válida foi confirmada no NVS pelo
  coordenador.
- **Conhecido:** dispositivo pareado presente no registry carregado em RAM.
- **Desconhecido:** endereço IEEE sem entrada válida no registry carregado.
- **Entrada:** associação entre um endereço IEEE estendido e um `device_id`.
- **Registry disponível:** blob ausente, válido ou com schema incompatível que
  pôde ser tratado segundo esta especificação.
- **Registry indisponível:** estado decorrente de corrupção estrutural, erro de
  leitura, erro de abertura ou falha equivalente que não permite distinguir
  com segurança as entradas persistidas.

O endereço IEEE estendido, com exatamente 8 bytes e na mesma ordem de bytes
usada pelo transporte vigente, é a chave primária. Não podem existir duas
entradas com o mesmo endereço. O `device_id` é atributo necessário para montar
mensagens ISSP e não substitui a identidade IEEE.

### 5.1 Precedência normativa dos estados

As decisões do runtime devem ser avaliadas nesta ordem, sem inverter ou omitir
níveis:

1. validade do frame e do endereçamento;
2. disponibilidade do registry;
3. estado da janela de ingresso;
4. origem conhecida ou desconhecida;
5. tipo da mensagem e correlações específicas.

`RegistryUnavailable` tem precedência sobre janela aberta, origem conhecida e
qualquer política operacional. A permissão da seção 9 para não definir se
`DATA` desconhecido é processado durante uma janela aberta só existe em
`RegistryReady`; ela nunca autoriza tráfego quando o registry está
indisponível.

Falha de lookup causada por `RegistryUnavailable` não pode ser tratada como
simples origem desconhecida. O runtime deve conservar essa distinção até a
decisão final de efeitos, para impedir que um caminho de fallback envie ACK,
evento ao host, resposta de discovery ou transmissão de comando.

## 6. Contrato persistente

O registry deve representar logicamente:

```text
Registry
├── schema_version
├── entry_count: 0..8
└── entries[entry_count]
    ├── extended_address: 8 bytes
    └── device_id: uint32
```

O formato físico pode incluir tamanho, marcador, checksum ou outros metadados
de integridade. Esses campos não podem alterar a identidade nem aumentar o
conteúdo funcional persistido sem nova evolução de schema.

São invariantes obrigatórios:

- versão de schema explicitamente reconhecida;
- contagem entre zero e oito;
- tamanho do blob coerente com a versão e a contagem;
- endereço não nulo e diferente de broadcast;
- endereços sem duplicidade;
- substituição atômica do blob completo;
- nenhum estado parcialmente novo pode ser observado após reboot;
- a confirmação de NVS deve terminar com sucesso antes de o novo conteúdo ser
  tratado como pareado.

A transação persistente possui três visões distinguíveis:

```text
durable_before
→ staged_candidate
→ commit
   ├── sucesso → durable_after = staged_candidate → publicar RAM
   └── falha   → durable_after = durable_before  → preservar RAM anterior
```

Fechar um handle, reinicializar o módulo ou reiniciar o coordenador deve
descartar staging não confirmado. A visão durável anterior deve continuar
legível depois de qualquer falha anterior ao sucesso do commit. Uma abstração
que exponha apenas `write()` atomicamente pode ser usada pela lógica do
registry, mas não substitui o gate do adaptador de produção: a evidência deve
exercitar separadamente sucesso e falha das etapas materiais de abertura,
leitura, `set_blob` e commit, ou um substituto que reproduza esses pontos e a
separação entre staging e durable.

`last_seq` não pertence ao blob. O registry somente deve ser regravado quando
uma entrada for criada ou quando o `device_id` de um endereço existente mudar.
Discovery idempotente com o mesmo endereço e o mesmo `device_id` não deve
provocar gravação adicional.

## 7. Carregamento e estados no boot

Depois de tentar inicializar a NVS e antes de aceitar tráfego de devices, o
coordenador deve carregar e validar o registry. Falha de inicialização,
abertura ou leitura que impeça acesso seguro ao registry resulta em
`RegistryUnavailable`.

```text
Boot
→ LoadRegistry
   ├── ausente ───────────────→ RegistryReady(entries=0)
   ├── válido ────────────────→ RegistryReady(entries=0..8)
   ├── schema incompatível ───→ RegistryReady(entries=0)
   └── corrupção/erro real ───→ RegistryUnavailable
```

Registry ausente ou com versão não reconhecida não autoriza interpretar bytes
parcialmente. O runtime inicia com zero entradas e o próximo pareamento válido
cria um blob no schema vigente.

Em `RegistryUnavailable`, o coordenador pode manter seus serviços de
diagnóstico e interface com o host, mas deve operar de forma fechada para
devices: não confirma pareamento, não aceita tráfego operacional e não envia
comandos. O erro deve permanecer observável. O firmware não deve apagar a
partição NVS inteira como recuperação automática.

`ESP_ERR_NVS_NO_FREE_PAGES`, `ESP_ERR_NVS_NEW_VERSION_FOUND` ou erro
equivalente de inicialização não autorizam `nvs_flash_erase()`. Se o runtime
não puder continuar com host e diagnóstico depois dessa falha, pode terminar
de forma controlada, mas não pode apagar a partição nem iniciar o rádio como se
o registry estivesse vazio.

As entradas válidas carregadas devem popular a tabela operacional sem inventar
um `last_seq`. O primeiro `DATA` válido de cada device depois do boot é tratado
como a primeira sequência observada naquela execução.

## 8. Transação de pareamento

Uma `DISCOVERY_REQ` somente pode iniciar a transação quando:

- a janela de ingresso estiver aberta;
- frame, versão, checksum, endereçamento e campos ISSP forem válidos conforme
  a especificação de commissioning;
- o endereço IEEE de origem for válido;
- o registry estiver disponível.

O fluxo obrigatório é:

```text
DISCOVERY_REQ válida
→ localizar endereço IEEE
   ├── existente e mesmo device_id ─→ sem escrita
   ├── existente e device_id novo ──→ substituir atributo
   ├── novo e há vaga ──────────────→ adicionar entrada
   └── novo e registry cheio ───────→ rejeitar sem substituir
→ para conteúdo alterado: gravar e confirmar blob completo
→ publicar a nova visão em RAM
→ enviar DISCOVERY_RESP de sucesso
```

Uma falha antes da confirmação do commit preserva a visão anterior em RAM,
não conclui o pareamento e não envia `DISCOVERY_RESP` de sucesso.

Se o commit terminar com sucesso e a transmissão da resposta falhar, a entrada
permanece pareada. Uma repetição posterior do mesmo discovery é idempotente e
pode reenviar a resposta sem nova gravação.

Quando o registry estiver cheio, discovery de endereço já existente continua
permitido; discovery de endereço novo deve ser rejeitado de forma observável,
sem desalojar, sobrescrever ou rotacionar outra entrada.

## 9. Tráfego operacional e comandos

Pareamento somente ocorre pelo fluxo da seção 8. Receber `DATA`, `ACK` ou uma
solicitação do host não cria nem atualiza entrada persistente.

- `DATA` de endereço desconhecido com a janela fechada deve ser descartado sem
  evento para o host e sem ACK ISSP.
- receber `DATA` desconhecido durante a janela não cria pareamento nem entrada
  persistente; sua aceitação operacional durante a janela permanece fora deste
  recorte.
- `DATA` conhecido deve preservar o processamento e a confirmação vigentes.
- `ACK` somente pode concluir comando pendente quando, além das correlações já
  existentes, origem IEEE e `device_id` corresponderem à entrada conhecida.
- comando do host para endereço desconhecido deve falhar sem transmissão de
  rádio.
- comando para entrada restaurada do NVS deve poder usar o endereço IEEE e o
  `device_id` persistidos sem aguardar novo report.

O fechamento da janela altera a aceitação de discovery, mas não remove nem
desativa entradas conhecidas.

### 9.1 Matriz normativa de decisão

`qualquer` significa que o resultado independe desse eixo. A matriz é avaliada
somente depois da validação de frame e endereçamento da seção 5.1.

| Registry | Janela | Origem | Entrada | Resultado obrigatório |
|---|---|---|---|---|
| Unavailable | qualquer | qualquer | `DISCOVERY_REQ` | não persistir nem responder sucesso; registrar indisponibilidade |
| Unavailable | qualquer | qualquer | `DATA` | descartar sem evento ao host, ACK ou persistência |
| Unavailable | qualquer | qualquer | `ACK` | não concluir comando nem persistir |
| Unavailable | qualquer | qualquer | comando do host | falhar sem transmissão de rádio |
| Ready | fechada | qualquer | `DISCOVERY_REQ` | não iniciar pareamento nem responder discovery |
| Ready | aberta | conhecida | `DISCOVERY_REQ` | executar idempotência ou atualização da seção 8 |
| Ready | aberta | desconhecida | `DISCOVERY_REQ` | criar se houver vaga; rejeitar se cheio; responder somente após commit |
| Ready | qualquer | conhecida | `DATA` | preservar processamento, deduplicação volátil e ACK vigentes; não persistir |
| Ready | fechada | desconhecida | `DATA` | descartar sem evento ao host, ACK ou persistência; registrar origem desconhecida |
| Ready | aberta | desconhecida | `DATA` | aceitação operacional não definida neste recorte; em qualquer opção, não persistir nem tornar conhecida |
| Ready | qualquer | conhecida | `ACK` | concluir somente se endereço IEEE, `device_id` e correlações pendentes coincidirem |
| Ready | qualquer | desconhecida | `ACK` | não concluir comando nem persistir |
| Ready | qualquer | conhecida | comando do host | transmitir usando IEEE e `device_id` persistidos |
| Ready | qualquer | desconhecida | comando do host | falhar sem transmissão de rádio |

Para o único campo deliberadamente não normatizado — aceitação operacional de
`DATA` desconhecido em `RegistryReady` com janela aberta — o gate aprova tanto
processar quanto descartar, mas deve afirmar ausência de escrita, ausência de
nova entrada e permanência da origem como desconhecida. Nenhuma outra célula
aceita resultados alternativos.

### 9.2 Efeitos observáveis do gate de política

Os gates automatizados devem observar, conforme o cenário:

- quantidade e resultado de tentativas de persistência e commits;
- emissão ou ausência de `DISCOVERY_RESP` e ACK ISSP;
- emissão ou ausência de evento ao host;
- transmissão ou ausência de comando de rádio;
- conclusão ou permanência de comando pendente;
- visão em RAM, conteúdo durável e estado depois de reboot;
- classe objetiva do log exigido.

Validar apenas o retorno de uma função do registry não comprova efeitos de
integração em `main.c`. O código de decisão compartilhado autorizado na seção
2.4 deve permitir que o gate substitua esses efeitos sem iniciar rádio ou UART
reais.

## 10. Falhas e recuperação

| Condição | Resultado obrigatório |
|---|---|
| registry ausente | iniciar vazio e permitir pareamento |
| schema não reconhecido | não interpretar entradas; iniciar vazio |
| blob truncado, contagem inválida, endereço inválido ou duplicado | `RegistryUnavailable` |
| checksum ou marcador de integridade inválido | `RegistryUnavailable` |
| `ESP_ERR_NVS_NO_FREE_PAGES`, `ESP_ERR_NVS_NEW_VERSION_FOUND` ou falha equivalente de inicialização | `RegistryUnavailable` ou término controlado; nunca apagar a partição nem iniciar devices como registry vazio |
| erro de abertura ou leitura NVS | `RegistryUnavailable` |
| erro de gravação ou commit | preservar visão anterior; não responder sucesso |
| capacidade cheia com endereço novo | preservar oito entradas; rejeitar discovery |
| mesma identidade e mesmos dados | responder sem regravar |
| mesma identidade com `device_id` novo | confirmar atualização antes de responder |
| TX da resposta falha após commit | preservar pareamento confirmado |

Nenhuma dessas condições autoriza apagar namespaces não relacionados ou a
partição NVS completa. Recuperação administrativa do registry indisponível
fica pendente de especificação futura.

A implementação deve inventariar todas as operações NVS destrutivas alcançáveis
no boot e no runtime do coordenador, inclusive código preexistente. O relatório
de implementação deve demonstrar que não há chamada automática a
`nvs_flash_erase()` nem operação equivalente capaz de apagar namespaces fora
do registry. Inspeção estática dessa lista é necessária, mas AC-007 também deve
preservar uma sentinela executada.

## 11. Logs operacionais mínimos

Os logs devem permitir distinguir ao menos:

```text
DEVICE_REGISTRY: load result=empty entries=0
DEVICE_REGISTRY: load result=ok entries=...
DEVICE_REGISTRY: load result=incompatible_schema
DEVICE_REGISTRY: load result=unavailable reason=...
DEVICE_REGISTRY: pairing result=created device=...
DEVICE_REGISTRY: pairing result=updated device=...
DEVICE_REGISTRY: pairing result=known device=...
DEVICE_REGISTRY: pairing result=rejected reason=full
DEVICE_REGISTRY: pairing result=failed reason=persist
DEVICE_REGISTRY: frame ignored reason=unknown_device
DEVICE_REGISTRY: frame ignored reason=registry_unavailable
```

O identificador pode ser representado pelo endereço IEEE já exposto nos logs
atuais. Não devem ser registrados blobs NVS completos nem conteúdo sem relação
com o diagnóstico.

## 12. Requisitos rastreáveis

- **COORD-REG-001:** inicializar, carregar e validar o registry antes de
  qualquer efeito de tráfego de devices.
- **COORD-REG-002:** persistir atomicamente até oito associações IEEE–`device_id`.
- **COORD-REG-003:** confirmar NVS antes de responder sucesso ao discovery.
- **COORD-REG-004:** não substituir entrada quando a capacidade estiver cheia.
- **COORD-REG-005:** atualizar por endereço existente e tornar repetição
  idempotente.
- **COORD-REG-006:** restaurar devices conhecidos após reboot.
- **COORD-REG-007:** impedir que tráfego operacional registre device.
- **COORD-REG-008:** com a janela fechada, rejeitar tráfego e comandos de
  origem desconhecida.
- **COORD-REG-009:** manter deduplicação de sequência fora da persistência.
- **COORD-REG-010:** preservar a NVS não relacionada em todas as falhas,
  inclusive erros de inicialização e caminhos preexistentes do boot.
- **COORD-REG-011:** operar de forma fechada quando o registry estiver
  indisponível, com precedência sobre janela, identidade e tipo de mensagem.
- **COORD-REG-012:** produzir logs que distingam carga, criação, atualização,
  idempotência, capacidade e falha.
- **COORD-REG-013:** preservar protocolo wire, rádio, janela e comportamento
  funcional dos devices conhecidos.

## 13. Critérios de aceite e evidências

### COORD-REG-AC-001 — Criação e sobrevivência ao reboot

Com registry vazio e janela aberta, enviar discovery válido de um endereço A e
`device_id` X. A execução aprova quando o commit termina, a resposta de sucesso
é emitida, o coordenador é reiniciado e um comando para A é transmitido usando
X antes de qualquer novo report. Reprova se a resposta preceder o commit, se a
entrada desaparecer ou se o comando depender de reaprendizado. Ausência de
reboot ou de comando executado não comprova o critério. Cobre
COORD-REG-001/002/003/006.

**Gate obrigatório:** executar o fluxo ponta a ponta tanto em harness
automatizado com adaptador NVS real quanto em hardware ESP32-C6. A evidência
deve registrar a ordem terminal `commit_ok < discovery_response_tx`, reiniciar
a visão de RAM, demonstrar pelo menos uma entrada relida e observar a
transmissão do comando antes de qualquer `DATA` ou discovery posterior. Build,
serialização isolada ou lookup direto no módulo não aprovam este critério.

O gate automatizado deve ainda executar variantes independentes de boot com
blob ausente, blob válido e schema incompatível; as duas primeiras observam,
respectivamente, `Ready(entries=0)` e restauração, enquanto schema incompatível
observa `Ready(entries=0)` sem interpretar entradas. Discovery com endereço
nulo ou broadcast deve ser rejeitado sem escrita ou resposta. Em uma variante
de falha de TX depois de commit bem-sucedido, A deve permanecer pareado após
reboot e uma repetição A/X deve poder responder sem nova escrita.

### COORD-REG-AC-002 — Falha atômica de persistência

Com um registry válido preexistente, injetar falha de escrita ou commit ao
tentar parear B. A execução aprova quando B não recebe resposta de sucesso, a
visão anterior continua ativa e, após reboot, o blob anterior permanece
integralmente válido. Reprova diante de entrada parcial, perda das entradas
anteriores ou resposta de sucesso. O substituto de NVS deve preservar staging,
commit e conteúdo reiniciado. Cobre COORD-REG-002/003/010.

**Gate obrigatório:** executar separadamente falha de `set_blob` e falha de
commit depois de staging bem-sucedido. Em ambas, observar ausência de
`DISCOVERY_RESP`, visão RAM anterior, conteúdo durável anterior depois de
reboot e sentinela de namespace não relacionado inalterada. O backend de teste
deve possuir buffers distintos de staging e durable; retornar erro antes de
copiar qualquer byte, por si só, não modela falha de commit e não aprova o AC.

### COORD-REG-AC-003 — Limite sem eviction

Partir de oito endereços distintos persistidos e tentar parear um nono. A
execução aprova quando o discovery é rejeitado, nenhum dos oito registros muda
e todos continuam presentes após reboot. Zero registros relidos não constitui
aprovação. Cobre COORD-REG-004.

**Gate obrigatório:** observar, pelo fluxo de discovery, oito respostas de
sucesso precedidas por commits, ausência de resposta e de novo commit para o
nono endereço, igualdade byte a byte do blob anterior e restauração das oito
identidades depois de reboot. Chamar apenas a API interna de `pair()` constitui
evidência parcial porque não observa o efeito de rádio.

### COORD-REG-AC-004 — Atualização e idempotência

Com A/X persistido, repetir discovery A/X e depois executar discovery A/Y. A
execução aprova quando A/X não produz escrita, A/Y produz exatamente um commit,
continua ocupando uma entrada e Y é restaurado após reboot. Reprova se houver
duplicidade, escrita na repetição idêntica ou uso posterior de X. O fake deve
contabilizar commits efetivos. Cobre COORD-REG-005/012.

**Gate obrigatório:** contabilizar tentativas de `set_blob`, commits efetivos e
respostas. A/X repetido deve produzir zero `set_blob` e zero commit adicionais,
mas pode reenviar resposta; A/Y deve produzir exatamente um commit concluído
antes da resposta. Depois de reboot, um comando deve usar Y e nunca X. Lookup
direto sem observar resposta e comando restaurado é evidência parcial.

### COORD-REG-AC-005 — Origem desconhecida

Com registry válido sem B e a janela fechada, enviar `DATA` válido de B. A
execução aprova quando ele é descartado sem ACK, sem evento ao host, sem
alteração do registry e com log de origem desconhecida. Em seguida, um comando
do host para B deve falhar sem transmissão. Reabrir a execução com a janela
aberta e receber `DATA` de B não pode criar entrada persistente. Cobre
COORD-REG-007/008/012.

**Gate obrigatório:** usar o código de decisão integrado ao runtime para
observar ausência de evento ao host, ACK, transmissão de comando e mutação do
storage no cenário fechado. Em execução separada com janela aberta, o gate não
impõe aceitar ou descartar o `DATA`, mas deve comprovar zero escrita e que B
continua desconhecido; ao fechar a janela em seguida, novo `DATA` de B deve ser
rejeitado. Um teste somente do módulo de persistência não cobre este AC.

### COORD-REG-AC-006 — Deduplicação volátil

Parear A, processar uma sequência S, reiniciar e reenviar S. A execução aprova
quando o blob não contém `last_seq`, nenhum commit decorre da atualização de
sequência e o primeiro frame válido após reboot é tratado como a primeira
observação da nova execução. Cobre COORD-REG-009.

**Gate obrigatório:** na mesma execução, o primeiro `DATA` A/S gera exatamente
um evento ao host e ACK, a repetição A/S é tratada como duplicada sem segundo
evento e continua recebendo o ACK vigente, e nenhuma das duas atualiza NVS.
Depois de reboot, A/S volta a gerar um evento como primeira observação e ACK,
sem novo commit. A evidência deve também decodificar o blob e demonstrar a
ausência de `last_seq`; verificar apenas seu tamanho é parcial.

### COORD-REG-AC-007 — Registry indisponível sem apagamento global

Preparar NVS com dados sentinela em namespace não relacionado e provocar blob
corrompido e erro real de leitura em execuções separadas. Cada execução aprova
quando o estado fica indisponível, discovery/tráfego/comando de device não são
confirmados e o dado sentinela permanece idêntico. Tratar erro como registry
vazio ou apagar a partição reprova. O fake ou fixture deve preservar isolamento
de namespaces e falhas de leitura. Cobre COORD-REG-001/010/011/012.

**Gate obrigatório:** executar ao menos estas classes separadamente:

1. blob truncado;
2. contagem maior que oito;
3. endereço nulo ou broadcast;
4. endereço duplicado;
5. checksum ou marcador inválido;
6. erro de abertura ou leitura;
7. `ESP_ERR_NVS_NO_FREE_PAGES` e `ESP_ERR_NVS_NEW_VERSION_FOUND` na
   inicialização.

Para cada classe, observar `RegistryUnavailable` ou término controlado
permitido pela seção 7, ausência de resposta/evento/ACK/transmissão/conclusão
de comando e sentinela inalterada. Pelo menos blob corrompido, reboot e
isolamento da sentinela devem ser executados em QEMU contra o adaptador NVS de
produção; falhas não produzíveis pelo backend real podem usar substituto fiel.

### COORD-REG-AC-008 — Compatibilidade funcional e build

Com dois devices pareados, executar report novo, report duplicado, ACK de
comando correlacionado e fechamento da janela. A execução aprova quando o
comportamento dos devices conhecidos permanece conforme as especificações
vigentes, os desconhecidos permanecem rejeitados e o firmware compila para
ESP32-C6 sem warnings novos. Apenas compilar não comprova o comportamento.
Cobre COORD-REG-006/008/013.

**Gate obrigatório:** build limpo com ESP-IDF 6.0.1 para ESP32-C6 e execução
terminal em hardware real com dois devices. Devem ser observados report novo,
duplicado com deduplicação preservada, comando e ACK correlacionado, rejeição
de ACK com identidade divergente, fechamento da janela, continuidade dos
devices conhecidos e rejeição de origem desconhecida. Cada cenário deve ter
quantidade executada maior que zero; build aprovado não substitui hardware.

### Matriz requisito–critério

| Requisito | Critérios |
|---|---|
| COORD-REG-001 | AC-001, AC-007 |
| COORD-REG-002 | AC-001, AC-002 |
| COORD-REG-003 | AC-001, AC-002 |
| COORD-REG-004 | AC-003 |
| COORD-REG-005 | AC-004 |
| COORD-REG-006 | AC-001, AC-008 |
| COORD-REG-007 | AC-005 |
| COORD-REG-008 | AC-005, AC-008 |
| COORD-REG-009 | AC-006 |
| COORD-REG-010 | AC-002, AC-007 |
| COORD-REG-011 | AC-007 |
| COORD-REG-012 | AC-001, AC-002, AC-003, AC-004, AC-005, AC-007 |
| COORD-REG-013 | AC-008 |

Os critérios AC-002, AC-003, AC-004, AC-005, AC-006 e AC-007 devem possuir
gate automatizado com NVS substituível. AC-001 e AC-008 exigem também execução
terminal em hardware real com quantidade de cenários maior que zero. Erro de
infraestrutura, execução não iniciada ou zero casos não constituem aprovação.

### Camadas mínimas de validação

| Camada | Finalidade | Pode comprovar | Não pode substituir |
|---|---|---|---|
| G1 — política integrada | executar o mesmo código de decisão usado por `main.c` com efeitos substituídos | matriz da seção 9, ordem persistência/resposta, ACK, host, comando e deduplicação | adaptador NVS real e hardware |
| G2 — backend NVS fiel | injetar falhas por etapa e preservar staging/durable/namespaces/reboot | atomicidade sob falha, contadores e isolamento lógico | integração real do adaptador |
| G3 — QEMU com NVS real | executar o adaptador de produção e partição NVS | schema, reabertura, reboot, corrupção, namespace sentinela e caminho nominal de commit | rádio e hardware ESP32-C6 |
| G4 — build de produção | compilar/linkar a composição real ESP32-C6 | compatibilidade de build e warnings | comportamento executado |
| G5 — hardware real | executar coordenador e devices físicos | rádio, reboot, commissioning e compatibilidade ponta a ponta | gates automatizados de falhas |

G1 pode compartilhar o mesmo app de teste com G2. G3 pode usar ESP32-C3 sob
QEMU quando o código exercitado não depender do rádio, mas deve compilar e
executar `device_registry_nvs.c`, não apenas o core do registry. G4 e G5 usam a
versão ESP-IDF fixada pelo projeto.

### Contrato obrigatório dos substitutos

O relatório de implementação deve preencher a coluna “Representação” antes de
usar um substituto como evidência:

| Semântica material | Representação mínima exigida |
|---|---|
| inicialização NVS | resultado injetável, incluindo no-free-pages e new-version |
| namespaces | mapa independente por namespace; sentinela fora do registry |
| abertura/leitura | ausência distinta de erro; tamanho e bytes preservados |
| staging | candidato separado do conteúdo durável |
| `set_blob` | falha injetável antes do commit sem alterar durable |
| commit | sucesso/falha injetável depois de staging; só sucesso substitui durable |
| fechamento/reboot | descarta staging e RAM; preserva durable e sentinela |
| observabilidade | contadores e ordem de chamadas/efeitos assertáveis |

Um fake com único buffer que só copia em sucesso comprova preservação da visão
RAM do core, mas não falha pós-staging, commit ou reboot durável. Ele pode ser
mantido como teste parcial, porém não sustenta AC-002 ou AC-007 completos.

### Matriz critério–gate–evidência

| Critério | Gates obrigatórios | Evidência terminal mínima |
|---|---|---|
| AC-001 | G1 + G3 + G5 | ordem commit/resposta, reboot, entrada relida e comando antes de report |
| AC-002 | G1 + G2 | falhas separadas de set/commit, visão anterior e sentinela após reboot |
| AC-003 | G1 + G2 | oito entradas, nono rejeitado, zero commit adicional e oito relidas |
| AC-004 | G1 + G2 | contadores de set/commit/resposta e comando restaurado com Y |
| AC-005 | G1 + G2 | ausência de ACK/evento/TX/escrita e permanência como desconhecido |
| AC-006 | G1 + G2 | evento/duplicata/reboot/reenvio, zero commits e blob sem sequência |
| AC-007 | G1 + G2 + G3 | classes de erro, fail-closed em todas as entradas e sentinela preservada |
| AC-008 | G1 + G4 + G5 | build sem warnings e cenários funcionais com dois devices |

### Manifesto obrigatório de evidências

Antes de declarar `Implemented`, o Implementador deve registrar uma linha por
AC e por execução material:

| Campo | Conteúdo obrigatório |
|---|---|
| critério | ID exato AC-001..AC-008 |
| cenário | condição inicial e variante executada |
| gate | G1..G5 |
| teste/comando | nome exato do caso e comando reproduzível |
| alvo/ambiente | target, versão ESP-IDF, backend e hardware quando aplicável |
| casos | quantidade maior que zero |
| resultado terminal | código de saída e resumo pass/fail |
| oráculo observado | efeitos, contadores, ordem, durable, logs ou rádio aplicáveis |
| classificação | `Approved`, `Failed`, `Partial` ou `Not Executed` |
| limitação | cláusula do AC não comprovada, quando houver |

Somente `Approved` com todos os gates obrigatórios cobre o AC. `Partial` nunca
é agregado como aprovado, mesmo quando todos os casos presentes passam. Um
teste só pode usar o rótulo integral `[AC-00N]` se cobrir o critério completo;
subconjuntos devem usar rótulo explícito como `[AC-00N-partial-schema]`.
Afirmações de cobertura na especificação ou changelog devem citar os nomes
exatos dos testes e seus resultados terminais, não apenas ramos existentes no
código.

### Verificação do ambiente de validação

Antes de registrar uma ferramenta como indisponível, a execução deve:

1. verificar se está no `PATH` e registrar a versão quando encontrada;
2. procurar a instalação e scripts de ativação referenciados pelo projeto ou
   disponíveis nos locais configurados do ambiente;
3. tentar ativar a versão ESP-IDF exigida e capturar o erro terminal;
4. diferenciar “não está no PATH desta sessão”, “instalação encontrada mas
   inválida” e “instalação não encontrada após as verificações”.

Indisponibilidade ambiental não remove nem reduz um gate obrigatório: sua
classificação permanece `Not Executed`, com os comandos e erros registrados.

### Varreduras de conformidade antes da entrega

O relatório do Implementador deve conter:

- inventário dos pontos de entrada `DISCOVERY_REQ`, `DATA`, `ACK` e comando do
  host, indicando como cada um aplica a precedência da seção 5.1;
- inventário das operações NVS destrutivas no boot e runtime, inclusive código
  preexistente, confrontado com COORD-REG-010;
- confronto de todas as células da matriz da seção 9 com pelo menos um teste
  G1 ou justificativa explícita de comportamento fora do escopo;
- matriz AC–teste–gate–resultado sem células implícitas;
- revisão adversarial final procurando uma implementação incorreta plausível
  que ainda passaria em cada gate.

## 14. Implantação e compatibilidade

A primeira instalação desta funcionalidade encontra registry ausente e inicia
com zero devices. A tabela volátil de firmware anterior não será migrada.

Um client que já possui descritor de rede não executa discovery apenas por
enviar seu report inicial. Portanto, para ingressar no novo registry, cada
client preexistente deve apagar seu vínculo por meio do factory reset vigente e
executar novo commissioning enquanto a janela do coordenador estiver aberta.
Essa operação de implantação não é autorizada por esta especificação e deve ser
ordenada separadamente para cada ambiente.

## 15. Fontes relacionadas e conhecimento afetado

- `docs/specs/ISSP-Commissioning.md`: precedente da janela e do discovery;
- `docs/specs/ISSP-Architecture.md`: contratos do runtime e do protocolo;
- `coordinator_154/main/main.c`: integração atual de boot, discovery, reports,
  ACKs, comandos e inicialização NVS;
- `coordinator_154/main/device_registry.{h,c}` e
  `device_registry_nvs.c`: baseline de core e adaptador a confrontar;
- `coordinator_154/test_apps/device_registry_test`: evidência automatizada
  parcial existente e precedente local a ampliar;
- `coordinator_154/main/iot154_packet.h`: identidade e envelope vigentes;
- `docs/rfc/KNOWLEDGE-MAP.md`: nova fonte normativa do registry;
- `docs/rfc/EKM-CHANGELOG.md`: transação `EKM-CHG-0008`.

## 16. Estado e próxima etapa

Esta autoria v0.2 não implementa nem valida o comportamento. O estado corrente
é:

- `Proposed`;
- `In Progress`, porque existe implementação parcial ainda não conforme;
- `Not Ready`;
- `Pending Review`, porque a análise de 31/07/2026 pertence à versão 0.1 e não
  promove antecipadamente esta revisão.

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

### 16.4 Encerramento da revisão e retorno à autoria (01/08/2026)

O Arquiteto recebeu os achados da seção 16.3 e determinou uma nova rodada de
autoria antes de qualquer correção de implementação. O objetivo declarado é
preservar o escopo funcional completo e experimentar uma especificação mais
validável, incorporando controles de estados, invariantes, fidelidade dos
substitutos e rastreabilidade entre critérios, testes e evidência.

Esta decisão não aprova os achados, não corrige código e ainda não transforma
as medidas discutidas em requisitos normativos. Ela encerra a atuação atual do
Engenheiro Revisor e devolve formalmente o documento à autoria:

- estado normativo: `Draft`, pois o conteúdo voltará a ser elaborado;
- estado da implementação: `In Progress`, pois existe implementação parcial
  com defeitos e lacunas de evidência registrados;
- prontidão: `Not Ready`;
- revisão de implementabilidade: `Pending Review`, pois a conclusão histórica
  da seção 16.1 não poderá validar antecipadamente o conteúdo revisado;
- transação `EKM-CHG-0008`: `Open`.

Os achados e evidências da seção 16.3 permanecem como entrada factual para a
próxima autoria. A versão revisada deverá passar por nova análise independente
de implementabilidade antes de autorizar outra atuação de implementação.

### 16.5 Reautoria v0.2 (Autor da Especificação, 01/08/2026)

O Arquiteto ordenou revisão integral da especificação, preservando o escopo
funcional completo e os treze requisitos. Esta versão:

- separa o baseline anterior, a implementação existente e o contrato
  normativo;
- define precedência entre validade do frame, disponibilidade do registry,
  janela, identidade e tipo de mensagem;
- acrescenta matriz de decisão para discovery, `DATA`, `ACK` e comandos;
- inclui falhas de inicialização NVS na proibição de apagamento global;
- explicita staging, durable, commit e reboot do contrato persistente;
- autoriza somente abstrações locais necessárias para testar a política usada
  pelo runtime e o adaptador NVS;
- preserva AC-001 a AC-008, mas torna seus gates completos e distingue
  evidência parcial de aprovação;
- define G1 a G5, fidelidade obrigatória dos substitutos, manifesto de
  evidências, verificação ambiental e varreduras de conformidade.

Nenhum código ou teste funcional foi alterado ou executado nesta autoria. A
implementação existente permanece `In Progress`; a proposta v0.2 fica
`Proposed`, `Not Ready` e `Pending Review`. A próxima etapa é uma nova análise
independente de implementabilidade; a conclusão histórica da versão 0.1 não é
reutilizável como aprovação desta versão.
