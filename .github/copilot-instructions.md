# Instruções do projeto para o GitHub Copilot

Antes de propor ou executar alterações, siga as instruções canônicas em:

- `../AGENTS.md`
- `../docs/rfc/EKM-GUIDELINES.md`
- `../docs/rfc/KNOWLEDGE-MAP.md`
- `../docs/rfc/EKM-CHANGELOG.md`

As especificações definem o comportamento esperado. As diretrizes EKM definem
como implementar e preservar arquitetura, decisões, contratos e demais ativos
de conhecimento.

Consulte os dois estados de cada especificação: estado normativo e estado da
implementação. Um documento `Active` pode possuir implementação `Regressed`.

Não remova ou condense documentos normativos, não resolva divergências
silenciosamente e não amplie o escopo sem autorização explícita.

Antes de concluir uma alteração normativa, revise seus dependentes e atualize o
mapa, as lacunas e o histórico EKM aplicáveis.

Registre o worktree inicial antes de editar; não use somente `HEAD` como
baseline. Reconcilie todos os diffs finais e identifique algoritmo e natureza de
cada hash usado como evidência.
