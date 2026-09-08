# Diretriz local de adoção do EKOM 5.0

**Classe da fonte:** Normativa local

**Estado da fonte:** Vigente

O IoTSmartLink15.4 adota o EKOM 5.0 publicado em
`/Users/marcelocostamiranda/source/EKM-guidelines`. O método, a governança, os
perfis e as ADRs do modelo externo prevalecem sobre instruções históricas do
projeto.

## Decisão de adoção

Marcelo Miranda, como Arquiteto, determinou em 07/09/2026: “Vamos adotar a EKOM
5.0 e adequar o repositório conforme as exigências dela.” Referência persistente:
`EKOM-CHG-0011` em `EKOM-CHANGELOG.md`. A migração passa a reger novas atuações;
registros e conclusões anteriores preservam a versão usada. A decisão não aprova
o contrato de engenharia sugerido nem habilita implementação retroativamente.

## Escolhas locais

Esta fonte registra somente escolhas locais:

- o namespace `EKM-CHG-*` e `EKM-GAP-*` permanece preservado para registros
  anteriores à migração;
- novas transações podem usar `EKOM-CHG-*`;
- débitos técnicos aceitos usam `EKOM-DEBT-*` e são registrados na seção
  própria de `docs/rfc/KNOWLEDGE-MAP.md`, que é seu registro canônico; débito
  não substitui lacuna, defeito, desvio nem risco residual, e sua aceitação,
  quitação ou substituição é exclusiva do Arquiteto;
- especificações permanecem em `docs/specs/`;
- decisões arquiteturais duráveis ficam em `docs/adr/`;
- relatórios ficam em `docs/reports/<mudança>/<capacidade>/`; avaliações de
  qualificação usam `docs/reports/repository-readiness/<identificador-unico>.md`;
- contrato de construção fica em `docs/rfc/REPOSITORY-ENGINEERING-CONTRACT.md`;
- estado, avaliação e decisão de habilitação ficam em
  `docs/rfc/REPOSITORY-READINESS.md`;
- mapa e changelog ativos ficam em `docs/rfc/`;
- registros anteriores permanecem válidos sob a versão usada e são
  preservados em `docs/history/ekom-1x/` ou no histórico Git.

As regras de targets, execução de testes e hardware pertencem a
`docs/specs/Repository-Test-Execution-Policy.md`, não a esta diretriz.
O build canônico integra toda implementação autorizada de artefato construível,
conforme a ADR-0008 externa; especificações funcionais não repetem sua
permissão. Execução de testes e hardware conserva autorização própria.

## Entrada operacional

A qualificação antecede o workflow: contrato aprovado, avaliação válida e
habilitação humana cobrindo a tarefa. Em alcance habilitado, análise Ready da
versão corrente e ordem explícita bastam para implementar; a ordem é o ato de
aprovação, sem promoção documental intermediária. Via curta do Consultor segue
seu perfil e não dispensa qualificação. O agente registra In Progress e mantém
conclusão, reabertura e integração sob autoridade humana.

O contrato v0.1 foi aprovado por Marcelo Miranda em 07/09/2026, com habilitação
do alcance inicial, conforme `EKOM-CHG-0011`, seção Aprovação e habilitação.
O repositório está Conditionally Ready: as áreas explicitamente Ready podem
seguir o workflow, e diagnóstico raiz e automações permanecem excluídos. Seguir
o alcance publicado no registro de readiness; a decisão não autoriza testes,
hardware ou implementação funcional sem a ordem aplicável.

A guarda existente continua útil para estrutura do delta; não é necessário
instalar ou alterar automações para simular autenticação da decisão humana.
Falhas históricas são registradas sem reescrever relatórios imutáveis. Os
adaptadores de agentes remetem ao roteador, sem fixar outra versão do método.
