# ADR-0005 — Endpoints de telemetria e registro de tipos de evento

**Estado:** Proposed

**Data:** 2026-08-13

**Decisores:** Arquiteto humano

**Especificação relacionada:**
`docs/specs/Client-Battery-Level.md`

**Habilita:** `EKOM-BATTERY-001`

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

**O registro de tipos de evento passa a ter fonte normativa nesta ADR:**

| Tipo | Significado | Domínio do valor |
|---|---|---|
| 1 | Sensor de porta | 1 aberto, 0 fechado |
| 2 | Plug comutável | conforme a especificação do plug |
| 3 | Nível de bateria em percentual | 0 a 100 |

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
