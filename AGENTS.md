# Instruções para assistentes de engenharia

Estas instruções se aplicam a todo o repositório.

## Leitura obrigatória

Antes de analisar ou alterar o projeto:

1. Leia `docs/governance/EKM-GUIDELINES.md`.
2. Consulte `docs/governance/KNOWLEDGE-MAP.md` para localizar as fontes de
   verdade do escopo afetado.
3. Leia integralmente as especificações, documentos arquiteturais, contratos e
   decisões normativas apontados pelo mapa.
4. Consulte `docs/governance/EKM-CHANGELOG.md` para verificar mudanças de
   conhecimento abertas relacionadas ao escopo.

## Regras fundamentais

- Especificações definem **o que o sistema deve fazer**.
- As diretrizes EKM definem **como implementar, validar e preservar o
  conhecimento do projeto**.
- Não trate o código atual como autoridade absoluta quando ele divergir de uma
  fonte normativa.
- Não remova, resuma, condense ou reescreva conhecimento normativo sem
  autorização explícita e específica.
- Não transforme ambiguidades arquiteturais em decisões locais silenciosas.
- Quando código, especificação, arquitetura, contrato ou teste divergirem,
  interrompa a parte afetada e relate a divergência com evidências.
- Implemente apenas o escopo aprovado. Código morto adjacente não é autorização
  para ampliar uma tarefa.
- Preserve alterações preexistentes que não pertençam ao escopo.
- Não execute commit, push, criação de branch ou PR sem autorização explícita.
- Antes da primeira alteração, registre a baseline real do worktree: branch,
  commit, status, diffs e arquivos não rastreados relevantes. O `HEAD` isolado
  não substitui essa baseline.
- Preserve e reconcilie alterações preexistentes. Todo diff final, inclusive
  formatação, deve possuir requisito, autorização ou justificativa explícita.
- Identifique a natureza e o algoritmo de hashes usados como evidência, por
  exemplo: objeto Git SHA-1, arquivo SHA-256 ou binário SHA-256.
- Trate alterações normativas como uma transação de conhecimento: revise fontes
  dependentes, mapa, lacunas e histórico antes de declarar conclusão.
- Uma implementação pronta não encerra uma mudança EKM enquanto existirem
  documentos, referências, lacunas ou evidências inconsistentes.

## Relatório obrigatório

Toda entrega deve distinguir:

- código e comportamento alterados;
- contratos alterados;
- ativos de conhecimento alterados;
- decisões adicionadas, modificadas ou removidas;
- desvios da especificação;
- validações executadas e pendentes.

O relatório também deve declarar se a transação EKM está completa, quais fontes
dependentes foram revisadas e quais registros `Open`, `Closed`, `Blocked` ou
`Superseded` foram criados ou atualizados.

Inclua a reconciliação entre os inventários inicial e final. Uma alteração não
explicada impede declarar a execução conforme ou encerrada.

Uma lista de arquivos modificados não substitui essa análise.
