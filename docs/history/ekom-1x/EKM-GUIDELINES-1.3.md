# EKM — Diretrizes de Engenharia e Preservação do Conhecimento

> **Registro histórico:** esta diretriz governou atuações sob a EKM 1.3 e foi
> substituída pelo EKOM 3.2. Os metadados abaixo registram seu estado à época.

**Tipo:** Normativo
**Status:** Active
**Versão:** 1.3
**Responsável:** Marcelo Miranda
**Última atualização:** 22/07/2026
**Escopo:** Todo o repositório

---

## 1. Objetivo

Estabelecer como humanos e assistentes devem implementar mudanças sem perder a
intenção, as decisões, os contratos e as evidências que tornam o sistema
compreensível e reconstruível.

Este documento não substitui as especificações funcionais ou técnicas. Ele
governa como elas são interpretadas, implementadas, validadas e preservadas.

---

## 2. Separação de responsabilidades documentais

### Especificação

Define **o que o sistema deve fazer** em um escopo delimitado:

- objetivo e comportamento esperado;
- entradas, saídas e estados observáveis;
- invariantes e contratos;
- cenários e critérios de aceite;
- limites e itens fora de escopo.

Uma especificação aprovada é a referência da implementação correspondente.

Cada comportamento que precise ser preservado, validado ou reconstruído deve
estar representado por uma especificação normativa. O código não deve ser a
origem acidental de um comportamento do sistema.

### Diretriz de engenharia

Define **como a mudança deve ser conduzida**:

- ordem de consulta das fontes;
- restrições permanentes de implementação;
- proteção de ativos de conhecimento;
- situações que exigem interrupção;
- validações e relatório obrigatórios.

### RFC e ADR

Registram princípios e decisões, incluindo motivação, alternativas, trade-offs
e consequências. Uma decisão nova deve indicar explicitamente quando substitui
uma decisão anterior.

### Relatório

Registra evidências de uma execução. Relatórios não são fonte normativa de
comportamento e não podem alterar implicitamente uma especificação ou decisão.

---

## 3. Princípio de reconstruibilidade

O repositório deve preservar conhecimento suficiente para que uma equipe
competente consiga reconstruir uma implementação funcionalmente equivalente
sem depender de conversas, memória individual ou da implementação atual.

Reconstrução funcionalmente equivalente significa recuperar:

- funcionalidades e comportamentos observáveis;
- contratos internos, externos, wire e de persistência;
- limites e invariantes arquiteturais;
- decisões relevantes e suas motivações;
- propriedades de segurança, confiabilidade e recuperação;
- critérios de aceite e evidências necessárias.

Não é objetivo reproduzir o mesmo código ou binário byte a byte.

---

## 4. Ativos de conhecimento

Todo documento de engenharia relevante deve possuir, explicitamente ou por sua
localização no mapa, uma classificação.

### Normativo

Define obrigação vigente: princípios, arquitetura, especificações, contratos,
RFCs e ADRs ativos.

### Histórico

Preserva contexto ou decisões anteriores. Não governa a implementação atual,
mas não deve ser reescrito como se o passado não tivesse existido.

### Operacional

Define procedimentos de build, deploy, diagnóstico, recuperação ou manutenção.

### Relatório

Registra uma execução, validação, incidente ou experimento específico.

### Informativo

Explica conceitos sem estabelecer obrigação.

O `KNOWLEDGE-MAP.md` registra a classificação, autoridade, escopo e estado dos
principais ativos.

---

## 5. Hierarquia e conflitos

As fontes devem ser consultadas nesta ordem conceitual:

```text
Constituição e princípios do projeto
→ RFCs e ADRs ativos
→ arquitetura e contratos vigentes
→ especificações aprovadas
→ critérios de aceite e testes executáveis
→ implementação atual
→ relatórios e conversas
```

Essa ordem não autoriza escolher silenciosamente um documento e ignorar outro.
Quando duas fontes normativas divergirem:

1. identificar a divergência;
2. determinar se existe substituição explícita e vigente;
3. interromper a parte afetada quando a precedência não for inequívoca;
4. solicitar decisão humana;
5. atualizar as fontes antes ou junto da implementação aprovada.

O código atual não corrige automaticamente um documento, e um documento
desatualizado não autoriza alterar código funcional sem decisão explícita.

---

## 6. Proteção do conhecimento normativo

Documentos normativos são bens de engenharia. Portanto:

1. Não remover decisões vigentes.
2. Não substituir documentos por resumos.
3. Não condensar ou reorganizar conteúdo de modo que elimine contexto,
   restrições, trade-offs ou responsabilidades.
4. Não tratar tarefas de limpeza, consolidação ou formatação como autorização
   para reescrita ampla.
5. Marcar conteúdo obsoleto como `Superseded` ou `Archived`, apontando para a
   decisão substituta, quando a preservação histórica for relevante.
6. Alterações normativas devem ser descritas semanticamente no relatório, não
   apenas listadas como arquivo modificado.
7. Remoção de conhecimento normativo exige autorização humana explícita e
   específica, mesmo quando o texto pareça redundante.

### Mudança significativa em documento normativo

É significativa qualquer alteração que:

- remova ou enfraqueça uma obrigação;
- mude responsabilidades ou fronteiras;
- altere comportamento, contrato ou critério de aceite;
- elimine motivação, risco ou trade-off ainda relevante;
- reduza substancialmente o conteúdo;
- modifique status ou substitua uma decisão.

Antes de executá-la, o assistente deve informar o impacto e obter autorização,
salvo quando a própria especificação aprovada autorizar a mudança de forma
explícita e detalhada.

---

## 7. Requisitos para especificações

A especificação continua sendo a unidade principal para solicitar implementação
ou criação. Ela deve ser autocontida quanto ao comportamento pretendido e pode
referenciar fontes normativas estáveis sem duplicá-las.

Uma especificação deve conter, quando aplicável:

1. metadados: tipo, estado normativo, estado da implementação, versão,
   responsável e escopo;
2. objetivo e problema concreto;
3. estado de referência;
4. comportamento esperado;
5. requisitos e invariantes;
6. contratos afetados;
7. responsabilidades dos componentes;
8. limites e fora de escopo;
9. cenários e critérios objetivos de aceite;
10. validações obrigatórias;
11. ativos de conhecimento que podem ou devem ser alterados;
12. formato do relatório.

### Identificação de requisitos

Requisitos que precisem de rastreabilidade devem possuir identificadores
estáveis, por exemplo:

```text
ISSP-COMM-001
ISSP-RESET-003
```

Os identificadores podem ser referenciados por testes, relatórios e decisões.

### Alterações documentais dentro de uma especificação

Quando a execução puder alterar documento normativo, a especificação deve dizer
explicitamente:

- quais documentos podem ser alterados;
- quais decisões devem ser adicionadas ou atualizadas;
- se alguma decisão pode ser removida;
- qual conteúdo deve ser preservado;
- como a alteração documental será validada e relatada.

Ausência dessa autorização significa preservar o conteúdo normativo existente.

### 7.1 Estado normativo

O estado normativo informa a autoridade atual da especificação, sem afirmar se
o código correspondente está pronto.

- `Draft`: conteúdo em elaboração, sem autoridade para implementação.
- `Proposed`: conteúdo pronto para revisão e decisão.
- `Approved`: conteúdo aprovado, mas ainda não necessariamente vigente.
- `Active`: fonte normativa vigente do comportamento descrito.
- `Superseded`: substituída por outra especificação ou versão explicitamente
  indicada.
- `Withdrawn`: cancelada antes de se tornar vigente.
- `Archived`: preservada apenas para consulta histórica.

Uma especificação `Superseded` deve apontar para sua sucessora. Uma
especificação `Archived` ou `Withdrawn` não autoriza novas implementações.

### 7.2 Estado da implementação

O estado da implementação informa a relação observada entre a especificação e
o sistema atual.

- `Not Started`: nenhuma implementação iniciada.
- `In Progress`: implementação parcial ou validação ainda incompleta.
- `Implemented`: implementação concluída, mas ainda sem toda a validação
  exigida.
- `Validated`: critérios de aceite comprovados com evidência suficiente.
- `Regressed`: comportamento anteriormente validado deixou de atender à
  especificação.
- `Blocked`: implementação ou validação depende de decisão ou condição externa.
- `Retired`: implementação retirada intencionalmente do sistema.

Estados normativo e de implementação são independentes. Esta combinação é
válida e importante:

```text
Estado normativo: Active
Estado da implementação: Regressed
```

Ela significa que o comportamento continua obrigatório, mas o produto atual
não o atende.

### 7.3 Transições e registro

Fluxos usuais, sem impedir transições justificadas, são:

```text
Draft → Proposed → Approved → Active
Active → Superseded → Archived

Not Started → In Progress → Implemented → Validated
Validated → Regressed → In Progress → Implemented → Validated
```

Toda transição material deve registrar:

- responsável e data;
- motivo da transição;
- evidência ou decisão aplicável;
- especificação sucessora, quando houver;
- atualização do `KNOWLEDGE-MAP.md`;
- registro no `EKM-CHANGELOG.md` quando fizer parte de uma transação relevante.

Os estados `Open`, `Closed`, `Blocked` e `Superseded` do histórico EKM são
estados de transações ou lacunas. `Open` e `Closed` não devem ser usados como
estado normativo de especificações.

---

## 8. Fluxo de implementação

### Antes de alterar

1. Ler `AGENTS.md` e estas diretrizes.
2. Consultar o mapa de conhecimento.
3. Ler integralmente as fontes normativas do escopo.
4. Registrar a baseline real do worktree e as alterações preexistentes.
5. Mapear requisitos para componentes, contratos e validações.
6. Identificar ambiguidades, conflitos e ativos normativos afetados.

### Baseline do worktree

Antes da primeira mutação, registrar de forma proporcional ao risco:

- caminho do repositório e branch atual;
- commit apontado por `HEAD`;
- status de arquivos rastreados e não rastreados;
- diffs staged e unstaged relevantes;
- arquivos fora do Git que pertençam ao escopo;
- hashes de conteúdo quando movimentação, sobrescrita ou equivalência precisem
  ser comprovadas.

O estado real do worktree é a baseline de preservação. `HEAD`, índice ou último
commit não substituem alterações locais existentes. Uma tarefa não pode limpar,
recriar a partir do Git ou normalizar arquivos modificados sem autorização
explícita.

Quando a ferramenta possuir transcript, log ou snapshot local, ele pode
complementar a baseline, mas a evidência durável necessária ao encerramento deve
ser registrada no repositório ou no relatório previsto pela governança.

### Durante a implementação

1. Implementar apenas o escopo aprovado.
2. Preservar comportamento e contratos não autorizados para mudança.
3. Não antecipar melhorias adjacentes.
4. Não resolver decisões arquiteturais relevantes como detalhe local.
5. Manter rastreabilidade entre requisito e alteração.
6. Atualizar documentos normativos apenas dentro da autorização recebida.
7. Adicionar ou atualizar evidências executáveis quando o comportamento mudar.

### Depois da implementação

1. Executar as validações previstas na especificação.
2. Comparar o resultado com cada critério de aceite.
3. Revisar o diff de código e de documentos separadamente.
4. Verificar se houve mudança sem requisito correspondente.
5. Verificar se algum requisito ficou sem implementação ou evidência.
6. Reconciliar o inventário final com a baseline inicial.
7. Produzir o relatório obrigatório.

### Reconciliação inicial e final

Todo arquivo criado, removido, movido ou modificado deve estar associado a:

- requisito aprovado;
- autorização explícita;
- preservação de alteração preexistente; ou
- justificativa registrada e aceita para um desvio.

Isso inclui alterações apenas editoriais, formatação, configuração gerada e
documentos. Um diff funcionalmente neutro continua sendo uma alteração e deve
ser visível no relatório.

Se existir alteração final sem explicação, a execução deve permanecer `Open` ou
`Blocked` até que ela seja removida, autorizada ou incorporada ao escopo.

### Análise de impacto documental

Antes de alterar uma fonte normativa, identificar e revisar, quando
aplicável:

```text
fonte alterada
→ documentos que a referenciam
→ mapa das fontes de verdade
→ lacunas relacionadas
→ RFCs, ADRs e especificações afetadas
→ critérios de aceite e evidências
→ registro da mudança no histórico EKM
```

A análise não exige modificar todos esses ativos. Exige determinar e registrar
quais permanecem consistentes, quais precisam mudar e quais dependem de decisão
humana.

### Transação de conhecimento

Uma mudança normativa é uma transação que inclui:

```text
alteração principal
+ referências dependentes
+ estado no mapa
+ lacunas criadas ou encerradas
+ evidências
+ relatório
= transação EKM completa
```

Código ou documento principal concluído não encerra a transação enquanto algum
ativo dependente permanecer inconsistente. Se a transação não puder ser
completada no escopo autorizado, seu registro deve permanecer `Open` ou
`Blocked`, e o relatório deve explicar a pendência.

### Definition of Done EKM

Antes de declarar conclusão, responder explicitamente:

1. Quais fontes normativas foram alteradas?
2. Quais fontes as referenciam ou dependem delas?
3. O `KNOWLEDGE-MAP.md` foi revisado e continua correto?
4. Alguma lacuna foi criada, alterada ou encerrada?
5. O `EKM-CHANGELOG.md` foi revisado e atualizado quando aplicável?
6. Existem afirmações incompatíveis em fontes ativas?
7. Os critérios de aceite possuem evidência proporcional ao risco?
8. O inventário final foi reconciliado integralmente com a baseline inicial?
9. Todo hash usado como evidência identifica sua natureza e seu algoritmo?
10. Existe algum diff final sem requisito, autorização ou justificativa?
11. A transação de conhecimento está completa?

Resposta negativa não impede uma entrega parcial, mas impede classificá-la como
conforme ou encerrada.

---

## 9. Situações que exigem interrupção

Interromper somente a parte afetada e solicitar orientação quando:

- especificações ou fontes normativas forem ambíguas ou conflitantes;
- a implementação exigir decisão arquitetural não autorizada;
- um documento normativo precisar perder conteúdo;
- código funcional divergir da especificação e não houver decisão de migração;
- um contrato wire, público ou persistente precisar mudar;
- a validação obrigatória não puder ser executada;
- arquivos preexistentes do usuário precisarem ser sobrescritos ou removidos;
- o escopo necessário for materialmente maior que o aprovado.

O relatório deve distinguir fato, hipótese e decisão pendente.

---

## 10. Rastreabilidade e evidências

Sempre que proporcional ao risco, manter a relação:

```text
Requisito
→ decisão ou contrato aplicável
→ componente responsável
→ teste ou validação
→ evidência da execução
```

Testes automatizados são especificações executáveis parciais: comprovam
comportamentos, mas não substituem arquitetura, motivação ou contratos não
observáveis.

### Identificação de hashes

Todo hash apresentado deve informar o objeto medido e o algoritmo, por exemplo:

```text
objeto Git SHA-1: 3135c1eb...
arquivo fonte SHA-256: 561d9a0c...
binário firmware SHA-256: f0328238...
```

Não usar apenas “hash” quando valores de naturezas diferentes puderem ser
confundidos. Hash igual comprova igualdade do conteúdo medido, não equivalência
funcional de estados que não participaram da comparação.

Quando um bug for encontrado:

```text
evidência do bug
→ requisito esclarecido ou criado
→ teste de regressão
→ correção
→ atualização da rastreabilidade
```

### Histórico de mudanças de conhecimento

`docs/rfc/EKM-CHANGELOG.md` registra o ciclo das mudanças relevantes de
conhecimento. Ele não substitui Git, especificações, RFCs, ADRs ou o mapa.

Cada registro deve possuir identificador estável `EKM-CHG-NNNN` e um dos
estados:

- `Open`: transação iniciada e ainda incompleta;
- `Closed`: critérios de encerramento atendidos e dependentes consistentes;
- `Blocked`: conclusão depende de decisão, autoridade ou evidência externa;
- `Superseded`: substituído por outro registro explicitamente indicado.

O registro deve conter motivação, ativos afetados, critérios de encerramento,
evidências e mudanças de estado. Não deve duplicar o conteúdo normativo das
fontes.

Lacunas de conhecimento devem possuir identificador estável `EKM-GAP-NNNN`,
estado e critério de encerramento. Encerrar uma lacuna exige atualizar o mapa e,
quando material, o histórico na mesma transação.

---

## 11. Relatório obrigatório de execução

O relatório deve conter, no mínimo:

1. resultado executivo;
2. requisitos atendidos e não atendidos;
3. código e comportamento alterados;
4. contratos alterados ou confirmação de preservação;
5. ativos de conhecimento modificados;
6. decisões adicionadas;
7. decisões modificadas;
8. decisões removidas;
9. conteúdo normativo condensado ou reescrito;
10. desvios da especificação e justificativas;
11. validações executadas e resultados;
12. validações pendentes;
13. riscos e pendências;
14. operações de Git ou externas realizadas.
15. fontes dependentes revisadas;
16. mudanças e lacunas EKM criadas ou atualizadas;
17. resultado da Definition of Done EKM;
18. estado final da transação de conhecimento.
19. baseline inicial e reconciliação do inventário final;
20. tipo, algoritmo e objeto de cada hash utilizado;
21. alterações finais sem requisito ou confirmação explícita de que não existem.

Para cada ativo normativo modificado, declarar explicitamente:

```text
Documento:
Motivo da alteração:
Decisões adicionadas:
Decisões modificadas:
Decisões removidas:
Conteúdo condensado ou reescrito:
Autorização aplicável:
```

Usar `nenhuma` quando não houver item. O silêncio não equivale a ausência de
mudança.

---

## 12. Critérios de conformidade EKM

Uma execução somente é conforme quando:

- o comportamento implementado corresponde à especificação aprovada;
- as restrições permanentes foram respeitadas;
- nenhuma decisão normativa foi removida silenciosamente;
- divergências foram relatadas, não ocultadas;
- requisitos possuem evidência proporcional ao risco;
- o relatório torna visíveis mudanças técnicas e de conhecimento;
- fontes dependentes, mapa, lacunas e histórico permanecem consistentes;
- a Definition of Done EKM foi respondida;
- a baseline real do worktree foi preservada;
- o inventário final não possui alteração inexplicada;
- hashes e demais evidências são reproduzíveis e não ambíguos;
- o repositório permanece suficiente para compreender e reconstruir o sistema.

Build aprovado, isoladamente, não comprova conformidade EKM.
