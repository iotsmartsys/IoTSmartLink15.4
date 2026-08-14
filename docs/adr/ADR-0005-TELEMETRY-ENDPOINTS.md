# ADR-0005 — Endpoints de telemetria e registro de tipos de evento

**Estado:** Proposed

**Data:** 2026-08-13

**Decisores:** Arquiteto humano

**Especificação relacionada:**
`docs/specs/Client-Battery-Level.md`

**Habilita:** `EKOM-BATTERY-001`

**Relação normativa:** estende [`Amends`] a semântica de `endpointId` e
`eventType` fixada em `docs/specs/ISSP-Configurable-Bootstrap.md`, que permanece
vigente em todo o restante. Nenhuma outra fonte é alterada.

## Contexto

O ISSP identifica cada capability pelo par `endpointId` e `eventType`. Ambos são
campos de um byte do frame, ao lado de `value`. Não existe cluster, atributo,
descritor, binding nem descoberta de lista de endpoints: o dispositivo não
anuncia endpoints, e o coordenador usa `endpointId` apenas para compor o nome da
capability apresentada ao host e para ecoá-lo no ACK.

Até aqui todo par existente descrevia uma função do produto — porta e plug — e
todo endpoint em uso era o endpoint funcional único do dispositivo. Duas
ausências ficaram visíveis quando o client passou a precisar reportar nível de
bateria:

1. **não há autoridade que distinga função de telemetria.** Uma grandeza
   observável e não comandável não tem lugar definido no modelo, e nada impede
   que um comando seja dirigido a ela;
2. **não há autoridade sobre a tabela de tipos de evento.** Os significados
   existem somente em código do coordenador, em `iot154_packet.h` e na tradução
   feita por `type_from_event`, incluindo um tipo já reservado para nível de
   bateria em percentual. Código não cria requisito por inferência, de modo que
   qualquer especificação que dependa desses significados dependeria hoje de uma
   autoridade inexistente.

A tradução para o host é feita apenas pelo tipo de evento, ignorando o endpoint.
Reutilizar um tipo já registrado em outro endpoint produziria rótulo
incorreto no host, e não apenas genérico.

## Alternativas consideradas

- manter telemetria no endpoint funcional, distinguida somente pelo tipo de
  evento, sem criar conceito novo;
- numerar tipos de evento por endpoint, reiniciando a numeração em cada um, o
  que exigiria alterar o coordenador para traduzir pelo par;
- registrar a ausência de autoridade sobre a tabela de eventos como lacuna do
  mapa, adiando a decisão;
- declarar a distinção apenas na especificação funcional de bateria, como
  emenda ao bootstrap configurável.

## Decisão

**Endpoints funcionais e endpoints de telemetria são categorias distintas do
mesmo campo.** Um endpoint funcional descreve função comandável do produto. Um
endpoint de telemetria descreve grandeza observável do dispositivo, é somente
leitura e responde `Unsupported` a qualquer comando dirigido a ele. A distinção
é convenção semântica: não altera wire, versão de protocolo, checksum,
endianness, tipo de frame nem tamanho de payload, e não introduz estrutura de
transporte.

**A categoria de um endpoint é declarada pela especificação que o define, e
nunca deduzida do seu número.** Não existe faixa reservada: nenhum número
identifica por si categoria funcional ou de telemetria, e host, coordenador e
client não podem inferi-la. Produtos permanecem livres para numerar seus
endpoints, desde que o par `endpointId` e `eventType` permaneça único no
dispositivo.

**O `Unsupported` de um endpoint de telemetria é produzido pelo behavior que
reconhece o par e o recusa**, e não pela ausência de behavior correspondente. Os
dois caminhos produzem o mesmo resultado no wire, mas apenas o primeiro mantém a
capability registrada e o par reservado, de modo que uma eventual leitura sob
demanda futura seja mudança de comportamento, e não de roteamento. O precedente
é o sensor de porta.

**Esta ADR normatiza o registro semântico dos tipos de evento, e não o layout
wire.** Tamanho, offsets, checksum, endianness e compatibilidade do protocolo
integral permanecem sob a lacuna `EKM-GAP-0002` e não são consolidados aqui.

**O registro de tipos de evento passa a ter fonte normativa nesta ADR:**

| Tipo | Significado | Domínio do valor | Fonte que o governa |
|---|---|---|---|
| 1 | Sensor de porta | 1 aberto, 0 fechado | `docs/specs/Firmware-Variants-Menuconfig.md` |
| 2 | Plug comutável | ligado, desligado e alternar | `docs/specs/ISSP-Configurable-Bootstrap.md`; `docs/specs/Firmware-Variants-Menuconfig.md` |
| 3 | Nível de bateria em percentual | 0 a 100 | `docs/specs/Client-Battery-Level.md` |

Esta ADR registra a alocação e a estabilidade de cada tipo; o domínio detalhado
e o comportamento de cada um permanecem com a fonte indicada na última coluna.

O registro descreve o que já existe em código; nenhuma alteração de coordenador,
de client ou de host decorre desta ADR.

**Regra de alocação.** Um tipo de evento tem significado global e estável: uma
vez atribuído, não é reutilizado com outro significado, em nenhum endpoint. Um
tipo novo é alocado por emenda a esta ADR. O endpoint distingue instâncias e
categorias dentro de um dispositivo; o tipo de evento distingue a natureza da
grandeza.

**Endpoint não é conceito Zigbee.** Nenhuma semântica de cluster, perfil ou
binding é importada, agora ou por analogia futura.

## Consequências

- telemetria de dispositivo passa a ter lugar definido no modelo, sem inflar o
  endpoint funcional e sem exigir mudança de protocolo;
- uma especificação pode depender do significado de um tipo de evento sem
  depender de código;
- a capability de nível de bateria pode ser especificada como somente leitura,
  com identidade estável e rótulo correto no host;
- a numeração de tipos de evento fica global, e não por endpoint: um produto não
  reinicia a contagem, o que preserva a tradução existente no coordenador;
- nenhuma faixa de endpoint é reservada: a categoria não é legível a partir do
  número, de modo que ferramentas, host e diagnósticos precisam consultar a
  especificação do produto para saber se um endpoint é funcional ou de
  telemetria;
- o par de um endpoint de telemetria permanece registrado no dispositivo, e não
  apenas ausente, o que consome uma entrada de behavior e mantém a recusa
  explícita;
- alocar um tipo de evento novo passa a exigir emenda desta ADR, o que
  acrescenta um passo de governança onde antes bastava editar um cabeçalho;
- convenções que hoje existem apenas em código do coordenador passam a ter fonte
  declarada, sem que isso autorize alterá-las neste recorte;
- nenhuma mudança de código decorre desta ADR; ela não autoriza implementação.

## Critério de reavaliação

Reavaliar se um dispositivo passar a expor múltiplas instâncias da mesma
grandeza, se telemetria precisar aceitar comando — leitura sob demanda, por
exemplo —, se o host exigir descoberta estruturada de endpoints, se o espaço de
um byte para tipos de evento se mostrar insuficiente, ou se o contrato wire
integral for consolidado em fonte dedicada, encerrando `EKM-GAP-0002`.
