# Instruções permanentes e roteamento EKOM

**Modelo EKOM:** 4.5

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

Implementação exige análise `Ready`, promoção registrada e autorização da mesma
versão. Com esses gates satisfeitos, o build canônico dos entregáveis
construíveis afetados integra a implementação e não exige cláusula na
especificação. Coleta ou execução de testes, flash, monitor e hardware exigem
autorização própria.

## Fontes locais do projeto

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

> **Specifications orchestrate. Code implements.**
