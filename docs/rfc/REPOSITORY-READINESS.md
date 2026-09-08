# Repository Readiness — IoTSmartLink15.4

**Classe da fonte:** Operacional
**Estado:** Not Ready
**Repositório:** IoTSmartLink15.4
**Contrato e revisão:** [Repository Engineering Contract](REPOSITORY-ENGINEERING-CONTRACT.md), v0.1 Proposed; nenhuma revisão aprovada
**Avaliação vigente:** [avaliação inicial](../reports/repository-readiness/2026-09-08T005734Z-v0.1-65e0a87a.md)
**Decisão humana de habilitação:** Pendente

A adoção da EKOM 5.0 foi determinada por Marcelo Miranda, Arquiteto, em 07/09/2026 (`EKOM-CHG-0011`). Essa decisão autoriza a adequação e não aprova o contrato proposto nem habilita implementação retroativamente.

## Escopos

| Escopo e dependências materiais | Estado | Bloqueadores | Decisão humana e alcance |
|---|---|---|---|
| Client H2, produtos/boards, quatro componentes ISSP, CMake/Kconfig e configuração do client | Not Ready | RR-01: aprovação do contrato v0.1 e habilitação ausentes | Pendente |
| Coordenador C6, registry, rádio, reports, ponte UART e construção associada | Not Ready | RR-01; contrato wire aplicável preservado | Pendente |
| Exemplo mínimo H2 e dependências compartilhadas | Not Ready | RR-01 | Pendente |
| Test apps H2/C6 e guards comuns de build | Not Ready | RR-01; criação/alteração/execução continuam limitadas por tarefa | Pendente |
| Diagnóstico ESP-IDF da raiz | Not Ready | RR-02 / EKM-GAP-0007; propósito não classificado | Fora da habilitação inicial proposta |
| Alteração/operação de workflows, scripts de submissão e serviços externos | Not Ready | RR-03: qualificação própria ausente | Fora da habilitação inicial proposta |

Levantamento, propostas, análise e documentação autorizados podem continuar. Nenhuma implementação está liberada, inclusive a via curta do Consultor. O Ready da especificação não substitui esta qualificação.

## Validade e reavaliação

A avaliação aponta a baseline e o contrato fixa as revisões das normas. Aprovação humana deve identificar responsável, papel, data, versão/escopo e referência à decisão. Após decisão, registrar nova avaliação com a aprovação confrontada; preservar a avaliação inicial.

Se houver habilitação parcial, usar `Conditionally Ready` no repositório e identificar explicitamente cada área `Ready`; nenhuma tarefa pode atravessar área bloqueada ou dependência material descoberta sem reavaliação. Evidência superada, mudança normativa material ou revogação suspendem somente o alcance afetado. Não solicitar aprovação repetida quando decisão, revisão e alcance continuarem válidos.

## Limitações preservadas

A guarda global possui falha documental histórica RR-04; a verificação dos documentos novos/alterados é separada. Ausência de campo em relatório antigo não autoriza sua edição nem torna inválido o firmware inteiro. A guarda estrutural não autentica aprovação humana e não é gate automatizado de CI.

A especificação de luminosidade continua com sua análise Not Ready própria; a migração não resolve seus bloqueadores.
