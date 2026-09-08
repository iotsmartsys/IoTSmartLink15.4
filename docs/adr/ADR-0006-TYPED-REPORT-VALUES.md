# ADR-0006 — Valores de report com tipo explícito

**Estado:** Accepted — registro das decisões explicitamente confirmadas pelo
Arquiteto na conversa de 08/09/2026; não aprova por antecipação o detalhamento
técnico dos Drafts nem autoriza implementação.

**Data:** 2026-09-08

**Decisores:** Marcelo Miranda, Arquiteto.

**Fontes relacionadas:** `../specs/ISSP-Typed-Values.md` e
`../specs/Fractional-Percentage-Reports.md`.

## Contexto

`value` ocupa um byte e os percentuais de luminosidade e bateria são inteiros.
O Arquiteto solicitou frações como 65,87 sem prejudicar o transporte futuro de
inteiros. Confirmou tipo explícito inteiro/float, saída percentual textual de
duas casas e preservação dos estados binários. Esclareceu que há somente
dispositivo de testes em bancada e dispensou compatibilidade com o formato
anterior. Por fim, determinou registrar e analisar a especificação.

## Decisão

Separar a representação numérica do valor da natureza indicada por evento.
Inteiros permanecem inteiros, sem passagem obrigatória por float. Porta e
presença mantêm códigos 0/1 e as traduções atuais; luminosidade e bateria
passarão a float com saída textual de duas casas e ponto.

Evoluir o protocolo em corte direto dos dois lados, sem negociação, fallback
ou tradução legada. Preservar identidade, retry, deduplicação, ACK e fronteiras
dos componentes. A preparação reutilizável possui contrato e validação próprios;
a funcionalidade percentual depende da preparação implementada e validada.
Essa separação aplica a regra de fronteira da EKOM, sem introduzir nova camada.

**Amends ADR-0004:** substituir a exigência exclusiva de v2 pelo novo corte,
mantendo todas as garantias de identidade e aceitação. **Amends ADR-0005:** o
domínio do evento 6 admite percentual fracionário, sem realocar eventos ou
endpoints; o detalhe dos percentuais 3/6 pertence à especificação dependente.
As relações valem para a evolução explicitada, sem alegação de implementação.

O layout e as larguras técnicas propostos estão no Draft coordenador. Esta
ADR registra a decisão arquitetural confirmada, não atribui ao Arquiteto uma
escolha prévia de offsets ou códigos numéricos do discriminador.

## Consequências

- Os dois codecs e todos os consumidores locais de report precisam preservar
  tipo/conteúdo, inclusive na fila e fingerprint.
- O dispositivo e coordenador da bancada precisam do novo firmware para
  operar juntos; a ordem de documentação não autoriza gravá-los.
- Não se alteram aplicação host externa, persistência ou topologia de código.
- A preparação permanece útil sem luminosidade/bateria; a análise funcional
  não pode suprir a ausência dessa baseline arquitetural.
- Novas larguras ou tipos além do contrato detalhado exigem evolução normativa.
