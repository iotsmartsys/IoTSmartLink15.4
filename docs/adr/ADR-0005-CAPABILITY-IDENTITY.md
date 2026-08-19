# ADR-0005 — Identidade de capability: endpoint congelado e tipo de evento como natureza

**Estado:** Accepted

**Data:** 2026-08-14

**Aceita pelo Arquiteto em:** 14/08/2026

**Emenda do tipo de evento 4 aceita pelo Arquiteto em:** 18/08/2026

**Emenda do tipo de evento 5 aceita pelo Arquiteto em:** 19/08/2026

**Decisores:** Arquiteto humano

**Especificações relacionadas:** `docs/specs/Client-Battery-Level.md`,
`docs/specs/Technical-Debt-Remediation.md` e
`docs/specs/Presence-Sensor-Battery-H2.md`

**Habilita:** `EKOM-BATTERY-001`

**Relação normativa:** estende [`Amends`] a semântica de `endpointId` e
`eventType` fixada em `docs/specs/ISSP-Configurable-Bootstrap.md`, que permanece
vigente em todo o restante. Nenhuma outra fonte é alterada.

**Débitos relacionados:** `EKOM-DEBT-0001`, `EKOM-DEBT-0002`, `EKOM-DEBT-0003`
e `EKOM-DEBT-0004`, registrados em `docs/rfc/KNOWLEDGE-MAP.md`.

## Contexto

O ISSP identifica cada capability pelo par `endpointId` e `eventType`. Ambos são
campos de um byte do frame, ao lado de `value`. Não existe cluster, atributo,
descritor, binding nem descoberta de lista de endpoints: o dispositivo não
anuncia endpoints, e o coordenador usa `endpointId` apenas para compor o nome da
capability apresentada ao host e para ecoá-lo no ACK.

Até aqui os dois campos eram tratados como configuração livre do product
firmware. Ao especificar a telemetria de bateria, ficou evidente que eles não
têm a mesma natureza, e que tratá-los igualmente produz composições incorretas
que nada detecta:

1. **o tipo de evento não é escolha do produto.** Ele é o que a capability
   **é**. O coordenador traduz rótulo e semântica do valor exclusivamente a
   partir dele, em `type_from_event` e `value_from_event`, ignorando o endpoint.
   Um sensor de porta composto com o tipo de plug se anunciaria ao host como
   plug, e nenhuma validação impediria;
2. **o endpoint é identidade, não categoria.** Ele distingue qual das
   capabilities daquele produto está falando. Como o host o exibe no nome da
   capability, ele participa da identidade observável do dispositivo;
3. **não havia autoridade sobre a tabela de tipos de evento.** Os significados
   existiam somente em código do coordenador, em `iot154_packet.h`, incluindo um
   tipo já reservado para nível de bateria em percentual. Código não cria
   requisito por inferência.

O registry do coordenador não persiste endpoint algum: guarda apenas endereço
estendido e `device_id`. Endpoint e tipo de evento, portanto, não afetam
pareamento nem persistência.

## Alternativas consideradas

- manter ambos como parâmetros livres do produto, aceitando composições
  incorretas como responsabilidade de quem escreve o firmware;
- categorizar endpoints em funcionais e de telemetria, com ou sem faixa
  reservada de numeração;
- numerar tipos de evento por endpoint, reiniciando a contagem, o que exigiria
  o coordenador traduzir pelo par;
- derivar o endpoint automaticamente da ordem de registro, recalculando-o a cada
  revisão do firmware;
- registrar a ausência de autoridade sobre a tabela de eventos como lacuna,
  adiando a decisão.

## Decisão

**O tipo de evento é a natureza da capability e pertence a ela, não ao
produto.** Cada capability tem um único tipo de evento, fixado por esta ADR. O
product firmware não o escolhe, não o sobrescreve e não o recebe como parâmetro
de configuração. Uma capability de sensor de porta não pode ser declarada com o
tipo de um plug.

**O endpoint é o identificador da capability dentro do produto.** Ele é
atribuído uma vez, tem origem sequencial a partir de 1 na ordem em que as
capabilities do produto foram criadas, e **fica congelado**: não é recalculado
por posição quando outra capability é acrescentada, removida ou reordenada em
revisões posteriores do firmware. O valor 0 não identifica capability alguma e
permanece reservado, como já ocorre nas mensagens de discovery.

**A unicidade é por endpoint, e não pelo par.** Duas capabilities do mesmo
dispositivo não podem compartilhar endpoint, ainda que seus tipos de evento
sejam distintos. O par continua existindo no frame, mas quem identifica é o
endpoint.

**Endpoint não carrega categoria.** Não existem endpoints funcionais e de
telemetria como classes do campo, nem faixa reservada de numeração. Ser somente
leitura é propriedade **da capability**, não do número que a identifica.

**Uma capability somente leitura recusa comandos pelo behavior que reconhece o
par**, e não pela ausência de behavior correspondente. Os dois caminhos produzem
o mesmo resultado no wire, mas apenas o primeiro mantém a capability registrada
e o endpoint reservado, de modo que uma eventual leitura sob demanda futura seja
mudança de comportamento, e não de roteamento. O precedente é o sensor de porta.

**Esta ADR normatiza o registro semântico dos tipos de evento, e não o layout
wire.** Tamanho, offsets, checksum, endianness e compatibilidade do protocolo
integral permanecem sob a lacuna `EKM-GAP-0002`.

**Registro dos tipos de evento:**

| Tipo | Capability | Domínio do valor | Fonte que o governa |
|---|---|---|---|
| 1 | Sensor de porta | 1 aberto, 0 fechado | `docs/specs/Firmware-Variants-Menuconfig.md` |
| 2 | Plug comutável | ligado, desligado e alternar | `docs/specs/ISSP-Configurable-Bootstrap.md`; `docs/specs/Firmware-Variants-Menuconfig.md` |
| 3 | Nível de bateria em percentual | 0 a 100 | `docs/specs/Client-Battery-Level.md` |
| 4 | Estado da telemetria de bateria | 0 calibrado, 1 aproximado, 2 inerte | `docs/specs/Technical-Debt-Remediation.md` |
| 5 | Sensor de presença | 1 detected, 0 undetected | `docs/specs/Presence-Sensor-Battery-H2.md` |

Esta ADR registra a alocação e a estabilidade de cada tipo; o domínio detalhado
e o comportamento permanecem com a fonte indicada na última coluna. Os tipos 1
a 4 descrevem o que já existe em código do coordenador. O tipo 4 permanece
reservado globalmente ainda que uma composição não habilite bateria e não
registre nem publique essa capability. O tipo 5 fica alocado globalmente pela
emenda aceita; sua implementação permanece sob
`docs/specs/Presence-Sensor-Battery-H2.md`.

**Regra de alocação.** Um tipo de evento tem significado global e estável: uma
vez atribuído, não é reutilizado com outro significado, em nenhum endpoint. Um
tipo novo é alocado por emenda a esta ADR, e a alocação acompanha a criação de
uma capability, não a de um produto. Um tipo não registrado é rotulado
genericamente pelo coordenador e não deve ser usado.

**Endpoint não é conceito Zigbee.** Nenhuma semântica de cluster, perfil ou
binding é importada, agora ou por analogia futura.

## Estado atual reconciliado

A implementação de `Technical-Debt-Remediation.md@v0.2` removeu `eventType` das
configurações públicas das capabilities existentes, transferiu a injeção do
tipo para a fachada e tornou a unicidade exclusiva do endpoint. A capability de
estado da telemetria e sua tradução no coordenador também foram implementadas
para o tipo 4.

O Arquiteto validou a implementação em hardware e determinou, em 19/08/2026, a
quitação de `EKOM-DEBT-0001` e `EKOM-DEBT-0004`. O registro histórico da
postergação e de seus critérios permanece no mapa de conhecimento.

## Consequências

- uma capability não pode ser composta com a natureza de outra, e o erro deixa
  de depender da atenção de quem escreve o firmware;
- o rótulo apresentado ao host passa a ser consequência da capability escolhida,
  e não de uma constante repetida em cada produto;
- o endpoint ganha estabilidade de identidade: automações, histórico e nomes do
  lado do host não mudam quando o firmware ganha ou perde capabilities;
- a origem sequencial deixa de ser regra de recálculo e passa a ser apenas o
  critério do primeiro valor atribuído;
- unicidade por endpoint é mais restritiva que a regra vigente: composições que
  hoje seriam aceitas com o mesmo endpoint e eventos distintos deixam de ser
  válidas quando a regra for implementada;
- a fachada passa a injetar o tipo de evento ao construir o behavior, o que
  mantém `issp_behaviors` genérico e preserva a separação da ADR-0001;
- alocar um tipo de evento novo passa a exigir emenda desta ADR, o que
  acrescenta um passo de governança onde antes bastava editar um cabeçalho, e
  costuma implicar trabalho no coordenador para rótulo e semântica do valor;
- convenções que hoje existem apenas em código do coordenador passam a ter fonte
  declarada, sem que isso autorize alterá-las neste recorte;
- nenhuma mudança de código decorre desta ADR; ela não autoriza implementação.

## Critério de reavaliação

Reavaliar se um produto precisar expor múltiplas instâncias da mesma capability
com identidade estável independente do endpoint, se telemetria precisar aceitar
comando — leitura sob demanda, por exemplo —, se o host exigir descoberta
estruturada de capabilities, se o espaço de um byte para tipos de evento ou para
endpoints se mostrar insuficiente, ou se o contrato wire integral for
consolidado em fonte dedicada, encerrando `EKM-GAP-0002`.
