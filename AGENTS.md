# Instruções permanentes e roteamento EKOM

**Modelo EKOM:** 5.0

**Modalidade:** capacidades referenciadas e governança proporcional

**Estado:** vigente

## Autoridade

O Arquiteto humano tem autoridade final sobre intenção, prioridade, escopo,
arquitetura, risco aceitável, relevância das críticas, suficiência das
evidências, aprovação, conclusão ou reabertura e integração. A especificação é
a fonte da verdade para comportamento e governa a execução dos agentes.

## Fonte dos perfis

**Raiz do EKOM:** `/Users/marcelocostamiranda/source/EKM-guidelines`

Antes de qualquer atuação EKOM:

1. leia integralmente `roles/REGRAS-COMUNS.md` na raiz do EKOM;
2. leia o perfil correspondente à capacidade recebida;
3. leia a especificação indicada, quando aplicável;
4. leia somente as fontes técnicas pertinentes.

| Capacidade recebida | Perfil |
|---|---|
| Autor da Especificação | `roles/AUTOR-DA-ESPECIFICACAO.md` |
| Engenheiro Analista | `roles/ENGENHEIRO-ANALISTA.md` |
| Engenheiro Implementador | `roles/ENGENHEIRO-IMPLEMENTADOR.md` |
| Crítico ou Engenheiro Revisor | `roles/ENGENHEIRO-REVISOR.md` |
| Consultor de Arquitetura | `roles/CONSULTOR-DE-ARQUITETURA.md` |

Análise de implementabilidade é obrigatória antes da implementação, mas pode
ser executada na mesma atuação quando autorizada. Challenge é consultivo e
proporcional ao risco, não um gate universal.

Em repositório habilitado, implementação exige análise `Ready` da versão
corrente e ordem explícita do Arquiteto para implementá-la. A ordem aprova e
autoriza a passagem; não existe promoção ou campo documental intermediário.
O Implementador registra mecanicamente `In Progress` ao iniciar. Ready de outra
versão ou ordem ambígua não autoriza mutação.

A via curta do Consultor só se aplica quando o Arquiteto determinar que a
alteração é pequena e ordenar explicitamente implementação sem especificação,
dentro dos limites do perfil. Ela também exige qualificação do repositório.

Com a entrada satisfeita, o build canônico dos entregáveis construíveis afetados
integra a implementação. Criação/alteração de testes exige vínculo explícito na
especificação; coleta/execução, flash, monitor e hardware exigem autorização
própria. Revisão é o quarto estágio; conclusão e integração são decisões humanas.

Mudança material autorizada inclui commit e push da branch de trabalho, com
árvore limpa. Não inclui merge, force push, tag, release ou deploy. Para trabalho
governado por especificação principal, adotar `spec/<slug-do-documento>` conforme
a regra externa; a fundação documental de governança não exige especificação
funcional artificial.

## Fontes locais do projeto

- contrato de engenharia: `docs/rfc/REPOSITORY-ENGINEERING-CONTRACT.md`;
- qualificação e alcance habilitado: `docs/rfc/REPOSITORY-READINESS.md`;
- avaliações de qualificação: `docs/reports/repository-readiness/`;
- especificações: `docs/specs/`;
- ADRs: `docs/adr/`;
- relatórios: `docs/reports/`;
- transações e lacunas: `docs/rfc/EKOM-CHANGELOG.md`;
- débitos técnicos aceitos: `docs/rfc/KNOWLEDGE-MAP.md`, namespace
  `EKOM-DEBT-NNNN`;
- mapa de conhecimento: `docs/rfc/KNOWLEDGE-MAP.md`;
- visão e navegação: `docs/specs/SYSTEM-DOSSIER.md`;
- diretriz local de adoção: `docs/rfc/EKOM-GUIDELINES.md`;
- arquitetura e contratos: `docs/specs/ISSP-Architecture.md`,
  `docs/specs/ISSP-Commissioning.md` e `components/README.md`;
- build canônico proposto: contrato de engenharia, seção 9; sua vigência
  depende de aprovação;
- targets e execução de testes:
  `docs/specs/Repository-Test-Execution-Policy.md`;
- guarda documental: `python3 tools/validate_ekom_documents.py .`.

## Invariantes locais

- Os únicos targets físicos admitidos são ESP32-H2 para `client_154` e
  ESP32-C6 para `coordinator_154`; o repositório não contempla ESP32-C3.
- QEMU não é estratégia admitida. Testes não são executados automaticamente;
  sua execução depende de autorização explícita no recorte aplicável.
- Build canônico dos targets afetados é obrigatório na implementação autorizada
  e segue `Repository-Test-Execution-Policy.md`; build falho ou não executado
  não sustenta implementação concluída.
- Product firmware define composição funcional; board model define recursos e
  pinagem físicos; Kconfig escolhe a composição e não governa lógica interna
  de componentes compartilhados.
- O client e o coordenador são alvos separados conectados pelo protocolo ISSP
  sobre IEEE 802.15.4; não crie dependência de código entre seus diretórios.
- Preserve arquitetura, organização e separação de responsabilidades; desvio
  exige decisão arquitetural explícita.
- Análise, implementação, challenge e validação produzem relatórios separados;
  não são anexados à especificação.
- O mapa combina índice de autoridade, árvore e Mermaid conforme a ADR-0004 do
  EKOM.
- Somente o Arquiteto incorpora achados em fontes normativas, aceita ADRs,
  promove estados e determina conclusão ou reabertura.
- Débito técnico é condição conhecida cuja correção o Arquiteto postergou
  conscientemente, com gatilho ou critério de quitação. Não se confunde com
  lacuna de conhecimento, defeito, desvio ou risco residual, e nenhum agente o
  aceita por autoridade própria; agentes só registram fatos e o estado
  operacional `In Remediation` quando sustentado pela atuação.
- Nunca registre segredo, token, chave, header de autorização ou connection
  string no repositório ou em saídas de agentes.

## Qualificação do repositório — EKOM 5.0

Antes de qualquer implementação, ler contrato e registro de readiness acima;
confirmar revisão aprovada, avaliação válida, decisão humana e cobertura de
todo o recorte e suas dependências materiais. `Not Ready` bloqueia código,
testes, configuração e build de implementação. `Conditionally Ready` habilita
somente áreas explicitamente `Ready` e confirmadas pelo Arquiteto. Ausência,
insuficiência ou aprovação pendente não pode ser suprida pelo Ready da tarefa.
Levantamento, proposta, análise e documentação autorizados podem preparar a
qualificação. A adoção não certifica retroativamente o legado.

Aplicar especificação da tarefa → contrato aprovado → precedentes oficiais →
código existente → preferência local. Uma especificação não revoga regra
arquitetural silenciosamente; exceção exige decisão com regra, motivo, alcance
e validade. Mudança material exige reavaliação do alcance; aprovação vigente
não é repetida a cada tarefa.

Norma: `docs/REPOSITORY-READINESS.md` na raiz externa EKOM. O validador local é
estrutural e não autentica aprovação nem substitui esta guarda operacional.
O estado atual é localizado em `docs/rfc/REPOSITORY-READINESS.md`.

> **Specifications orchestrate. Code implements.**
