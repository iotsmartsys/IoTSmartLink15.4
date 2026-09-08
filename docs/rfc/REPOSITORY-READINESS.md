# Repository Readiness — IoTSmartLink15.4

**Classe da fonte:** Operacional
**Estado:** Conditionally Ready
**Repositório:** IoTSmartLink15.4
**Contrato e revisão:** [Repository Engineering Contract](REPOSITORY-ENGINEERING-CONTRACT.md), v0.1 Approved — vigente
**Avaliação vigente:** [avaliação após aprovação](../reports/repository-readiness/2026-09-08T010746Z-v0.1-92b3d036.md), complementada pelo [confronto da alocação do evento 6](../reports/repository-readiness/2026-09-08T012937Z-v0.1-event6-073b9609.md)
**Decisão humana de habilitação:** Marcelo Miranda, Arquiteto, em 07/09/2026 — alcance inicial abaixo; `EKOM-CHG-0011`, seção Aprovação e habilitação

A adoção da EKOM 5.0 e a aprovação/habilitação são decisões distintas, ambas registradas em `EKOM-CHG-0011`. A manifestação de aprovação foi: “Sim, aprovo o contrato v0.1 e a habilitação desse alcance inicial.” Nenhuma habilitação é retroativa.

## Escopos

| Escopo e dependências materiais | Estado | Bloqueadores | Decisão humana e alcance |
|---|---|---|---|
| Client H2, produtos/boards, quatro componentes ISSP, CMake/Kconfig e configuração do client | Ready | Nenhum bloqueador de qualificação no alcance; RR-01 resolvido | Habilitado por Marcelo Miranda em 07/09/2026 |
| Coordenador C6, registry, rádio, reports, ponte UART e construção associada | Ready | Nenhum bloqueador de qualificação no alcance; contrato wire aplicável preservado | Mesma decisão |
| Exemplo mínimo H2 e dependências compartilhadas | Ready | Nenhum bloqueador de qualificação no alcance | Mesma decisão |
| Test apps H2/C6 e guards comuns de build | Ready | Nenhum bloqueador de qualificação; criação/alteração/execução continuam limitadas por tarefa | Mesma decisão |
| Diagnóstico ESP-IDF da raiz | Not Ready | RR-02 / EKM-GAP-0007; propósito não classificado | Excluído da habilitação inicial |
| Alteração/operação de workflows, scripts de submissão e serviços externos | Not Ready | RR-03: qualificação própria ausente | Excluído da habilitação inicial |

As áreas Ready são elegíveis ao workflow de implementação; o estado global Conditionally Ready não libera áreas bloqueadas. Cada tarefa exige cobertura integral do recorte e dependências, análise Ready e ordem explícita quando aplicáveis. A via curta do Consultor conserva seus requisitos. Testes, hardware e operações externas não são autorizados por esta qualificação.

## Validade e reavaliação

A avaliação vigente aponta baseline e decisão; o contrato fixa as revisões das normas. Mudança normativa ou arquitetural material, dependência fora do alcance, evidência superada ou revogação exige reavaliação afetada. Não solicitar aprovação repetida quando decisão, revisão e alcance continuarem válidos.

A [avaliação inicial](../reports/repository-readiness/2026-09-08T005734Z-v0.1-65e0a87a.md) permanece histórica e imutável. A avaliação nova reconcilia RR-01 sem apagar as exclusões RR-02/RR-03 nem a limitação RR-04.

## Limitações preservadas

A guarda global possui falha documental histórica RR-04; a verificação dos documentos novos/alterados é separada. Ausência de campo em relatório antigo não autoriza sua edição nem torna inválido o firmware inteiro. A guarda estrutural não autentica aprovação humana e não é gate automatizado de CI.

A especificação de luminosidade v0.3 recebeu análise `Ready` e ordem humana de
implementação. A reconciliação do evento 6 está implementada: guarda, definição
e tradução passaram no build C6. Evidência e limitações da implementação:
`docs/reports/light-sensor-battery-h2/implementation/2026-09-08T151247Z-v0.3-346114b7-implementation.md`.
A antiga limitação de cinco eventos permanece histórica na avaliação
complementar; não é bloqueio vigente. A aprovação do contrato v0.1 e as
exclusões de alcance permanecem. Revisão, testes e hardware não são autorizados
por esta qualificação.

## Reconfronto do alcance — valores tipados (08/09/2026)

Avaliação dirigida: `docs/reports/repository-readiness/2026-09-08T160535Z-v0.1-dbc2eeac-typed-values.md`. A decisão arquitetural confirmada
na ADR-0006 e os dois Drafts mantêm donos, tecnologias e consumidores dentro
do alcance inicial. O contrato v0.1 e a habilitação humana existente são
preservados; não há nova aprovação atribuída ao agente. A baseline continua
v2, e a preparação v3 não foi implementada. A dependência da funcionalidade
percentual é tratada em sua análise própria, não suprida por esta qualificação.

### Evidência da implementação tipada

A baseline v2 citada no reconfronto acima é a anterior à implementação. A
preparação v3 está agora em código, com 13 builds concluídos e handoff para
Revisão em `docs/reports/issp-typed-values/implementation/2026-09-08T194643Z-v0.1-6f534630-implementation.md`. Nenhuma habilitação foi ampliada; a evidência de build
não significa teste executado nem atualização da bancada.
