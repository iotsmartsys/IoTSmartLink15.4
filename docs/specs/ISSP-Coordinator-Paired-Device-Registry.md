# ISSP 802.15.4 — Registry de Dispositivos Pareados do Coordenador

**Tipo:** Normativo
**Estado normativo:** Proposed
**Estado da implementação:** Not Started
**Prontidão:** Not Ready
**Revisão de implementabilidade:** Pending Review
**Versão:** 0.1
**Responsável arquitetural:** Marcelo Miranda
**Última atualização:** 31/07/2026
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

### 2.1 Fatos observados

O firmware atual:

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

### 2.2 Intenção confirmada pelo Arquiteto

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

### 2.3 Solução proposta

Esta especificação propõe um único blob versionado e atomicamente substituível
em namespace próprio do coordenador. A escolha dos nomes internos da chave e
do namespace é detalhe de implementação, desde que o namespace não seja
compartilhado com dados de fábrica, PHY, Wi-Fi ou vínculos persistidos pelos
clients.

Não é proposta nova camada arquitetural. O registry será uma responsabilidade
local do firmware `coordinator_154`, separado do estado de deduplicação
volátil. Uma abstração interna pequena é permitida para isolar NVS da lógica de
commissioning e possibilitar testes com um substituto que preserve a semântica
de leitura, commit atômico e falhas.

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

`last_seq` não pertence ao blob. O registry somente deve ser regravado quando
uma entrada for criada ou quando o `device_id` de um endereço existente mudar.
Discovery idempotente com o mesmo endereço e o mesmo `device_id` não deve
provocar gravação adicional.

## 7. Carregamento e estados no boot

Depois de inicializar a NVS e antes de aceitar tráfego de devices, o coordenador
deve carregar e validar o registry.

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

## 10. Falhas e recuperação

| Condição | Resultado obrigatório |
|---|---|
| registry ausente | iniciar vazio e permitir pareamento |
| schema não reconhecido | não interpretar entradas; iniciar vazio |
| blob truncado, contagem inválida, endereço inválido ou duplicado | `RegistryUnavailable` |
| erro de abertura ou leitura NVS | `RegistryUnavailable` |
| erro de gravação ou commit | preservar visão anterior; não responder sucesso |
| capacidade cheia com endereço novo | preservar oito entradas; rejeitar discovery |
| mesma identidade e mesmos dados | responder sem regravar |
| mesma identidade com `device_id` novo | confirmar atualização antes de responder |
| TX da resposta falha após commit | preservar pareamento confirmado |

Nenhuma dessas condições autoriza apagar namespaces não relacionados ou a
partição NVS completa. Recuperação administrativa do registry indisponível
fica pendente de especificação futura.

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
```

O identificador pode ser representado pelo endereço IEEE já exposto nos logs
atuais. Não devem ser registrados blobs NVS completos nem conteúdo sem relação
com o diagnóstico.

## 12. Requisitos rastreáveis

- **COORD-REG-001:** carregar e validar o registry antes do tráfego de devices.
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
- **COORD-REG-010:** preservar a NVS não relacionada em todas as falhas.
- **COORD-REG-011:** operar de forma fechada quando o registry estiver
  indisponível.
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

### COORD-REG-AC-002 — Falha atômica de persistência

Com um registry válido preexistente, injetar falha de escrita ou commit ao
tentar parear B. A execução aprova quando B não recebe resposta de sucesso, a
visão anterior continua ativa e, após reboot, o blob anterior permanece
integralmente válido. Reprova diante de entrada parcial, perda das entradas
anteriores ou resposta de sucesso. O substituto de NVS deve preservar staging,
commit e conteúdo reiniciado. Cobre COORD-REG-002/003/010.

### COORD-REG-AC-003 — Limite sem eviction

Partir de oito endereços distintos persistidos e tentar parear um nono. A
execução aprova quando o discovery é rejeitado, nenhum dos oito registros muda
e todos continuam presentes após reboot. Zero registros relidos não constitui
aprovação. Cobre COORD-REG-004.

### COORD-REG-AC-004 — Atualização e idempotência

Com A/X persistido, repetir discovery A/X e depois executar discovery A/Y. A
execução aprova quando A/X não produz escrita, A/Y produz exatamente um commit,
continua ocupando uma entrada e Y é restaurado após reboot. Reprova se houver
duplicidade, escrita na repetição idêntica ou uso posterior de X. O fake deve
contabilizar commits efetivos. Cobre COORD-REG-005/012.

### COORD-REG-AC-005 — Origem desconhecida

Com registry válido sem B e a janela fechada, enviar `DATA` válido de B. A
execução aprova quando ele é descartado sem ACK, sem evento ao host, sem
alteração do registry e com log de origem desconhecida. Em seguida, um comando
do host para B deve falhar sem transmissão. Reabrir a execução com a janela
aberta e receber `DATA` de B não pode criar entrada persistente. Cobre
COORD-REG-007/008/012.

### COORD-REG-AC-006 — Deduplicação volátil

Parear A, processar uma sequência S, reiniciar e reenviar S. A execução aprova
quando o blob não contém `last_seq`, nenhum commit decorre da atualização de
sequência e o primeiro frame válido após reboot é tratado como a primeira
observação da nova execução. Cobre COORD-REG-009.

### COORD-REG-AC-007 — Registry indisponível sem apagamento global

Preparar NVS com dados sentinela em namespace não relacionado e provocar blob
corrompido e erro real de leitura em execuções separadas. Cada execução aprova
quando o estado fica indisponível, discovery/tráfego/comando de device não são
confirmados e o dado sentinela permanece idêntico. Tratar erro como registry
vazio ou apagar a partição reprova. O fake ou fixture deve preservar isolamento
de namespaces e falhas de leitura. Cobre COORD-REG-001/010/011/012.

### COORD-REG-AC-008 — Compatibilidade funcional e build

Com dois devices pareados, executar report novo, report duplicado, ACK de
comando correlacionado e fechamento da janela. A execução aprova quando o
comportamento dos devices conhecidos permanece conforme as especificações
vigentes, os desconhecidos permanecem rejeitados e o firmware compila para
ESP32-C6 sem warnings novos. Apenas compilar não comprova o comportamento.
Cobre COORD-REG-006/008/013.

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
| COORD-REG-012 | AC-004, AC-005, AC-007 |
| COORD-REG-013 | AC-008 |

Os critérios AC-002, AC-003, AC-004, AC-005, AC-006 e AC-007 devem possuir
gate automatizado com NVS substituível. AC-001 e AC-008 exigem também execução
terminal em hardware real com quantidade de cenários maior que zero. Erro de
infraestrutura, execução não iniciada ou zero casos não constituem aprovação.

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
- `coordinator_154/main/main.c`: tabela volátil, fluxo de discovery, reports,
  ACKs e comandos;
- `coordinator_154/main/iot154_packet.h`: identidade e envelope vigentes;
- `docs/rfc/KNOWLEDGE-MAP.md`: nova fonte normativa do registry;
- `docs/rfc/EKM-CHANGELOG.md`: transação `EKM-CHG-0008`.

## 16. Estado e próxima etapa

Esta autoria não implementa nem valida o comportamento. A proposta permanece:

- `Proposed`;
- `Not Started`;
- `Not Ready`;
- `Pending Review` pelo Engenheiro Analista.
