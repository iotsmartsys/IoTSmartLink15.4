# Instruções permanentes e roteamento EKM

**Modelo EKM:** 1.14

**Modalidade:** atores com perfis referenciados

**Estado:** vigente

## Autoridade

O Arquiteto humano tem autoridade final sobre intenção, prioridade, escopo,
arquitetura, risco, autorização, validação e integração. A ordem recebida por
prompt ou pipeline identifica papel, resultado, recorte e especificação quando
aplicável.

## Fonte dos perfis

**Raiz local da EKM:**
`/Users/marcelocostamiranda/source/EKM-guidelines`

Antes de qualquer atuação EKM:

1. leia integralmente
   `/Users/marcelocostamiranda/source/EKM-guidelines/roles/REGRAS-COMUNS.md`;
2. leia integralmente somente o perfil correspondente ao papel recebido;
3. leia a especificação indicada, quando aplicável;
4. leia apenas as fontes técnicas pertinentes ao recorte.

| Papel recebido | Perfil |
|---|---|
| Autor da Especificação | `roles/AUTOR-DA-ESPECIFICACAO.md` |
| Engenheiro Analista | `roles/ENGENHEIRO-ANALISTA.md` |
| Engenheiro Implementador | `roles/ENGENHEIRO-IMPLEMENTADOR.md` |
| Engenheiro Revisor | `roles/ENGENHEIRO-REVISOR.md` |
| Consultor de Arquitetura | `roles/CONSULTOR-DE-ARQUITETURA.md` |

Não carregue perfis de outros papéis nem a metodologia EKM completa. Se a
ordem não identificar papel, resultado e recorte, ou se a fonte não estiver
acessível, não inicie a tarefa. A especificação é obrigatória no ciclo
funcional; o Consultor pode receber Não se aplica [`Not Applicable`] em
governança ou apoio fora desse ciclo.

## Fontes locais do projeto

- especificações: `docs/specs/`;
- decisões, evidências e transações: `docs/rfc/EKM-CHANGELOG.md`;
- mapa de conhecimento: `docs/rfc/KNOWLEDGE-MAP.md`;
- visão e navegação do sistema: `docs/specs/SYSTEM-DOSSIER.md`;
- bootstrap e composição da API: `src/Api/Program.cs`;
- superfície HTTP: `src/Api/Controllers/`;
- domínio e contratos de persistência: `src/Core/`;
- persistência MySQL: `src/Data.Repositories/` e
  `scripts/sql/OAuth.Database.Schemes.sql`;
- portal administrativo: `src/UI/`;
- testes da API: `src/Api.Tests/`;
- build e publicação: `Makefile`, `.github/workflows/` e Dockerfiles.

## Comandos canônicos

- build da API: `dotnet build src/Api/Api.csproj`;
- testes automatizados da API:
  `dotnet test src/Api.Tests/Api.Tests.csproj`;
- build do portal: `npm run build --prefix src/UI`.

Validações adicionais pertencem à especificação aplicável. Build de imagem,
push de pacote, webhook, deploy e scripts que publicam artefatos exigem ordem
explícita do Arquiteto.

## Invariantes locais

- nunca registre nem exponha senha, token, authorization code, client secret,
  chave de assinatura, header de autorização ou connection string;
- diferencie clientes públicos de confidenciais; segredo distribuído em
  cliente público não constitui autenticação do cliente;
- alterações em autorização, emissão, validação, renovação, revogação,
  consentimento ou identidade exigem especificação e evidência proporcional ao
  risco;
- preserve alterações preexistentes e não execute migração, rotação,
  reescrita de histórico, publicação ou deploy sem autorização específica;
- código e testes comprovam o estado implementado, mas não criam intenção nem
  contrato normativo por inferência.

As regras comuns e o perfil selecionado definem condições de entrada, promoção
de estados, evidência, Git e encerramento. Regras específicas da tarefa
pertencem à especificação ou à ordem do Arquiteto.
