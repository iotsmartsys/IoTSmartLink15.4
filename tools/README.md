# Guardas estruturais EKOM

`validate_ekom_documents.py` verifica somente regras objetivas do roteamento e
do mapa EKOM 3.2:

- campos mínimos dos relatórios;
- estrutura mínima das ADRs;
- ausência de headings típicos de relatório em especificações.
- presença do índice, árvore e diagrama no mapa;
- árvore ou justificativa explícita de não aplicabilidade;
- Mermaid ou justificativa explícita de não aplicabilidade;
- ausência de placeholders `<...>` que o Mermaid interpreta como HTML.

Uso sobre todo o projeto:

```sh
python3 tools/validate_ekom_documents.py .
```

Em adoção legada, valide primeiro somente arquivos novos ou alterados:

```sh
python3 tools/validate_ekom_documents.py . docs/specs/nova.md docs/reports/mudanca/analysis/resultado.md
```

A guarda não decide se uma ADR é necessária, se árvore ou diagrama possuem boa
semântica, se um achado é relevante ou se a evidência é suficiente. Esses
pontos permanecem sob julgamento humano.
