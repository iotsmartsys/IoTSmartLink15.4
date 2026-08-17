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

## Submissões EKOM pelo n8n

Os comandos de submissão usam `SUBMMITION_URL`, `TOKEN_EKOM` e `SUBMITTED_BY`
do `.env` ignorado pelo Git. O token é enviado somente no header
`X-EKOM-Token` e nunca integra o payload ou os relatórios.

Para solicitar análise na branch `spec/*` corrente:

```sh
./tools/submit_ekom_analysis.sh
```

Para emitir uma ordem explícita de implementação da versão corrente:

```sh
./tools/submit_ekom_implementation.sh
```

O segundo comando exige árvore limpa e branch sincronizada, localiza o relatório
formal `Ready` aplicável e envia ao n8n o evento `submit_for_implementation` com:

- `change_id`;
- `specification_path`;
- `analysis_report_path`;
- `working_branch`;
- `architect_authorization=true`;
- `authorized_by` recebido de `SUBMITTED_BY`;
- `allow_tests=false`;
- `allow_hardware=false`.

O n8n deve disparar `.github/workflows/ekom-implementation.yml` com `ref=main`
e mapear esses campos diretamente para os inputs homônimos. O workflow não
autoriza testes, flash, monitor ou hardware neste piloto.
