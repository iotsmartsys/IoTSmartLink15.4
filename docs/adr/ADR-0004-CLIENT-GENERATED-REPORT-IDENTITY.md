# ADR-0004 — Identidade de report gerada pelo client

**Estado:** Accepted

**Data:** 2026-08-12

**Decisores:** Arquiteto humano

**Especificação relacionada:**
`docs/specs/ISSP-Report-Identity.md`

**Substitui:** deduplicação de `DATA` pela última sequência observada, limitada
ao runtime do coordenador

## Contexto

O client reinicia sua sequência de report em zero a cada boot. O coordenador
permanece ligado e conserva a última sequência por dispositivo em RAM. Como
consequência, dois reports legítimos de boots distintos podem possuir a mesma
sequência e o segundo ser classificado como retry, receber ACK e não ser
encaminhado ao host.

Deep sleep tornou o defeito frequente, mas não o criou: qualquer reboot isolado
do client alcança o mesmo conflito. Corrigi-lo somente na política de energia
manteria protocolo e deduplicação semanticamente incorretos.

## Alternativas consideradas

- sessão negociada com o coordenador e incluída na identidade;
- sequência persistida pelo client;
- sequência inicial aleatória, preservando o campo atual como identidade;
- identificador aleatório próprio para cada report lógico, gerado pelo client,
  sem negociação.

## Decisão

Cada report lógico possui um `report_id` não nulo de 64 bits, gerado pelo
client. A identidade é o par `(device, report_id)`; a sequência permanece uma
correlação transitória de tentativa e deixa de definir duplicidade.

O protocolo ISSP passa em corte coordenado para a versão 2. `DATA` carrega o
`report_id`, e seu ACK o ecoa. O coordenador conserva uma janela volátil e
limitada de identidades recentes por dispositivo conhecido. O mesmo report
repetido não produz novo evento, mas recebe novo ACK. Um report com identidade
desconhecida só é lembrado e confirmado depois que seu evento completo foi
aceito pela fila UART local.

Não há sessão, handshake adicional, persistência de identidade ou dependência
de código entre client e coordenador. A escolha aceita unicidade estatística,
não uma garantia matemática ou criptográfica.

## Consequências

- reports de boots distintos deixam de colidir deterministicamente quando a
  sequência reinicia;
- retries internos e externos do mesmo report reutilizam a identidade;
- a API pública de configuração de `IsspDevice` recebe uma fonte injetável de
  IDs, mantendo `issp_core` independente do ESP-IDF;
- client e coordenador precisam ser atualizados juntos; v1 e v2 não convivem;
- o host recebe `event_id` aditivo e pode deduplicar além da janela local;
- a entrega é pelo menos uma vez dentro dos limites declarados: reboot do
  coordenador ou expulsão da janela pode produzir duplicidade;
- ausência de persistência, ACK fim a fim ou outbox significa que perda após a
  fila UART local continua possível e pertence a outro recorte.

## Critério de reavaliação

Reavaliar se houver requisito de entrega durável fim a fim, proteção contra
replay, mais de um coordenador por client, volume que torne insuficiente a
janela fixa ou evidência de colisão incompatível com o risco aceito.
