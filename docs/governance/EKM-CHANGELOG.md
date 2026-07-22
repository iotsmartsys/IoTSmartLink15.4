# EKM — Histórico de Mudanças do Conhecimento

**Tipo:** Operacional
**Status:** Active
**Versão:** 1.0
**Responsável:** Marcelo Miranda
**Última atualização:** 21/07/2026
**Escopo:** Todo o repositório

---

## 1. Objetivo

Registrar o ciclo de mudanças relevantes no conhecimento do projeto, incluindo
seu estado, ativos afetados, critérios de encerramento e evidências.

Este histórico não substitui Git, especificações, RFCs, ADRs ou o mapa de
conhecimento. Ele indica se a transação de conhecimento correspondente está
aberta, encerrada, bloqueada ou substituída.

---

## 2. Estados

- `Open`: mudança iniciada e ainda incompleta.
- `Closed`: critérios atendidos, evidências registradas e dependentes
  consistentes.
- `Blocked`: depende de decisão, autoridade ou evidência externa.
- `Superseded`: substituída por outro registro indicado explicitamente.

Somente uma transação que satisfaça a Definition of Done EKM pode ser marcada
como `Closed`.

---

## EKM-CHG-0001 — Instituição da governança EKM

**Status:** `Closed`
**Tipo:** Criação de governança
**Aberta em:** 21/07/2026
**Encerrada em:** 21/07/2026

### Motivação

Separar especificações de comportamento das diretrizes permanentes de
implementação e preservação do conhecimento.

### Ativos afetados

- `AGENTS.md`;
- `.github/copilot-instructions.md`;
- `docs/governance/EKM-GUIDELINES.md`;
- `docs/governance/KNOWLEDGE-MAP.md`.

### Critérios de encerramento

- instruções canônicas disponíveis no repositório;
- especificação preservada como unidade principal de implementação;
- fontes de verdade classificadas e mapeadas;
- proteção e relatório de mudanças normativas definidos.

### Evidências

- arquivos de governança criados e relidos integralmente;
- `git diff --check` aprovado.

---

## EKM-CHG-0002 — Restauração da arquitetura ISSP

**Status:** `Closed`
**Tipo:** Correção de conhecimento
**Aberta em:** 21/07/2026
**Encerrada em:** 21/07/2026

### Motivação

A consolidação removeu conhecimento normativo de
`client_154/docs/ISSP-Architecture.md` sem tornar essa perda visível no
relatório de execução.

### Ativos afetados

- `client_154/docs/ISSP-Architecture.md`;
- `docs/governance/KNOWLEDGE-MAP.md`;
- este histórico.

### Critérios de encerramento

- conhecimento arquitetural vigente restaurado;
- atualizações legítimas da consolidação preservadas;
- conteúdo validado estaticamente contra a implementação;
- lacuna `EKM-GAP-0001` encerrada no mapa;
- relatório EKM auditado.

### Evidências

- `ISSP-Architecture.md` v1.1;
- comparação com a versão anterior no Git e com
  `ISSP-Consolidation.md`;
- auditoria contra componentes e composição atuais;
- `git diff --check` aprovado.

---

## EKM-CHG-0003 — Consistência global e ciclo das mudanças EKM

**Status:** `Closed`
**Tipo:** Evolução da governança
**Aberta em:** 21/07/2026
**Encerrada em:** 21/07/2026

### Motivação

A primeira restauração orientada pela EKM corrigiu o documento principal, mas
não atualizou o mapa nem encerrou sua lacuna. A governança precisava exigir
consistência entre ativos dependentes, não apenas conformidade local.

### Ativos afetados

- `AGENTS.md`;
- `.github/copilot-instructions.md`;
- `docs/governance/EKM-GUIDELINES.md`;
- `docs/governance/KNOWLEDGE-MAP.md`;
- este histórico.

### Critérios de encerramento

- análise de impacto documental definida;
- transação de conhecimento definida;
- Definition of Done EKM definida;
- mudanças e lacunas possuem IDs e estados;
- instruções de assistentes e mapa atualizados;
- restauração arquitetural refletida de forma consistente.

### Evidências

- EKM Guidelines v1.1;
- Knowledge Map v1.1;
- histórico EKM criado;
- adaptadores de Codex e Copilot atualizados;
- revisão do diff documental e `git diff --check` aprovados.

---

## EKM-CHG-0004 — Componentes ISSP reutilizáveis

**Status:** `Open`
**Tipo:** Evolução arquitetural e empacotamento
**Aberta em:** 21/07/2026
**Encerrada em:** —

### Motivação

Comprovar o objetivo original da refatoração: permitir que firmwares ESP-IDF
consumam a stack ISSP sem copiar o firmware `client_154` ou manter variantes do
mesmo projeto.

### Ativos afetados previstos

- componentes ISSP atualmente em `client_154/components`;
- configuração CMake do `client_154`;
- novo consumidor mínimo em `examples/issp_minimal_client`;
- `client_154/docs/ISSP-Reusable-Components.md`;
- `client_154/docs/ISSP-Architecture.md`;
- `docs/governance/KNOWLEDGE-MAP.md`;
- `components/README.md`;
- este histórico.

### Critérios de encerramento

- componentes movidos para localização compartilhada sem duplicação;
- client existente consumindo o pacote compartilhado;
- segundo consumidor independente compilando e fazendo link;
- contratos públicos, privados e dependências documentados;
- builds do client, exemplo e coordenador aprovados;
- `EKM-GAP-0004` encerrada ou escopo residual separado explicitamente;
- transação EKM e relatório completos.

### Evidências atuais

- especificação `ISSP-Reusable-Components.md` criada para revisão;
- implementação ainda não iniciada.
