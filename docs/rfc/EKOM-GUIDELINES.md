# Diretriz local de adoção do EKOM 3.6

**Classe da fonte:** Normativa local

**Estado da fonte:** Vigente

O IoTSmartLink15.4 adota o EKOM 3.6 publicado em
`/Users/marcelocostamiranda/source/EKM-guidelines`. O método, a governança, os
perfis e as ADRs do modelo externo prevalecem sobre instruções históricas do
projeto.

Esta fonte registra somente escolhas locais:

- o namespace `EKM-CHG-*` e `EKM-GAP-*` permanece preservado para registros
  anteriores à migração;
- novas transações podem usar `EKOM-CHG-*`;
- especificações permanecem em `docs/specs/`;
- decisões arquiteturais duráveis ficam em `docs/adr/`;
- relatórios ficam em `docs/reports/<mudança>/<capacidade>/`;
- mapa e changelog ativos ficam em `docs/rfc/`;
- registros anteriores permanecem válidos sob a versão usada e são
  preservados em `docs/history/ekom-1x/` ou no histórico Git.

As regras de targets, execução de testes e hardware pertencem a
`docs/specs/Repository-Test-Execution-Policy.md`, não a esta diretriz.
O build canônico integra toda implementação autorizada de artefato construível,
conforme a ADR-0008 externa; especificações funcionais não repetem sua
permissão. Execução de testes e hardware conserva autorização própria.
