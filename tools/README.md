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

O comando de análise usa `SUBMMITION_URL`; o de implementação usa
`SUBMMITION_IMPLEMENTATION_URL`. Ambos leem `TOKEN_EKOM` do `.env` ignorado
pelo Git. O token é enviado somente no header `X-EKOM-Token` e nunca integra o
payload ou os relatórios.

Para solicitar análise na branch `spec/*` corrente:

```sh
./tools/submit_ekom_analysis.sh
```

O n8n encaminha essa submissão ao workflow de análise
`.github/workflows/ekom-analysis.yml`.

Para emitir uma ordem explícita de implementação da versão corrente:

```sh
./tools/submit_ekom_implementation.sh
```

O segundo comando exige árvore limpa e branch sincronizada e envia ao n8n o
evento `submit_for_implementation` com:

- `working_branch`, recuperada da branch Git corrente;
- `authorized_by`, recuperado de `git config user.name`;
- `allow_tests=false`;
- `allow_hardware=false`.

O n8n valida o evento autenticado e dispara
`.github/workflows/ekom-implementation.yml` com `ref=main`, a branch e a ordem
explícita do Arquiteto. O próprio workflow localiza na branch a única
especificação correspondente, lê seu ID e seleciona a análise `Ready` aplicável
à mesma revisão e baseline. O workflow permite criar ou alterar testes quando a
especificação corrente os exige explicitamente, mas não autoriza coletá-los ou
executá-los, nem usar flash, monitor ou hardware.
