# Histórico de mudanças EKOM

Este arquivo registra estado resumido e referências materiais. O histórico
detalhado da EKM 1.x permanece em
`docs/history/ekom-1x/EKM-CHANGELOG.md`; o Git preserva autoria e diferenças.

## Transações legadas preservadas

| ID | Título | Estado | Fonte material principal |
|---|---|---|---|
| `EKM-CHG-0001` | Instituição da governança EKM | Closed | Histórico EKM 1.x |
| `EKM-CHG-0002` | Restauração da arquitetura ISSP | Closed | `ISSP-Architecture.md` |
| `EKM-CHG-0003` | Consistência e ciclo das mudanças | Closed | Histórico EKM 1.x |
| `EKM-CHG-0004` | Componentes ISSP reutilizáveis | Closed | `ISSP-Reusable-Components.md` |
| `EKM-CHG-0005` | Baseline e evidências não ambíguas | Closed | Histórico EKM 1.x |
| `EKM-CHG-0006` | Ciclo de vida das especificações | Closed | Histórico EKM 1.x |
| `EKM-CHG-0007` | Bootstrap configurável do client | Closed | `ISSP-Configurable-Bootstrap.md` |
| `EKM-CHG-0008` | Registry persistente do coordenador | Open | `ISSP-Coordinator-Paired-Device-Registry.md` |
| `EKM-CHG-0009` | Retirada transversal de QEMU | Closed | `Repository-Test-Execution-Policy.md` |
| `EKM-CHG-0010` | Correção dos targets admitidos | Closed | ADR-0003 e política de targets |

## EKOM-CHG-0001 — Adoção e reconciliação do EKOM 3.2

**Estado:** Fechada [`Closed`]

**Especificação relacionada:** Não aplicável; governança documental

**Objetivo:** Adotar o roteamento documental, as ADRs, os relatórios separados,
o dossiê e as três visões do mapa do EKOM 3.2 sem alterar código funcional.

### Decisões relacionadas

- ADR-0001, ADR-0002 e ADR-0003 locais;
- ADR-0003 e ADR-0004 do EKOM 3.2 externo.

### Lacunas

- lacunas técnicas legadas permanecem identificadas no mapa;
- migração não cria requisito funcional novo.

### Relatórios e evidências materiais

- relatório de análise e validação desta reconciliação em `docs/reports/`.

### Resultado

A fundação EKOM 3.2 foi adotada, as ADRs locais foram aceitas, os registros EKM
1.x foram preservados e 39 seções históricas foram separadas em relatórios. O
Arquiteto confirmou a reconciliação e autorizou commit, push, merge e publicação
da `main`.

## EKOM-CHG-0002 — Deep sleep configurável do client

**Estado:** Rascunho e análise [`Draft`]

**Especificação relacionada:** `docs/specs/Client-Deep-Sleep.md`

**Objetivo:** Especificar deep sleep opt-in para devices client a bateria,
wakeup periódico em minutos ou horas e LED indicador configurável por GPIO,
polaridade e tempo ligado.

### Decisões relacionadas

- ADR-0002 preserva a separação entre política do product firmware e recurso
  físico do board model;
- o recurso permanece desabilitado por padrão e não altera o coordenador.
- a v0.4 declara alteração limitada do bootstrap e das variantes, preservando
  arquitetura, commissioning e política de execução de testes.

### Lacunas

- bloqueadores normativos da versão 0.1 foram resolvidos pelo Arquiteto na
  versão 0.3;
- a v0.4 incorporou os pontos normativos devolvidos pela verificação: relação
  com o bootstrap, quiescência sem `stop()` público, ampliação da API, alcance
  integral do renome e ordem do LED em `setup()`;
- a capacidade de observar o deadline durante operação bloqueante permanece
  risco técnico que a nova análise deve delimitar e um experimento futuro deve
  provar; não há alegação antecipada de viabilidade desse mecanismo.

### Relatórios e evidências materiais

- análise inicial em
  `docs/reports/client-deep-sleep/analysis/2026-08-11-initial-analysis.md`;
- análise de verificação em
  `docs/reports/client-deep-sleep/analysis/2026-08-11-verification-analysis.md`;
- testes e hardware permanecem `Not Executed`.

### Resultado

Contrato v0.4 registrado em rascunho, com relações de autoridade e fronteiras
de quiescência, API, renome e inicialização explicitadas. O documento está
preparado para nova análise de implementabilidade; implementação ainda depende
de análise, promoção e autorização explícitas do Arquiteto.
