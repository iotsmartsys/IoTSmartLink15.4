Piloto de Engenharia Assistida por IA — Refatoração ISSP

> **Registro histórico:** este piloto antecede o EKOM 3.2 e preserva o contexto
> experimental da refatoração. Novas atuações seguem `AGENTS.md`, as fontes em
> `docs/` e os perfis externos do EKOM.

Status: Em andamento
Início: 17/07/2026
Projeto: IoTSmartLink15.4
Referência: RFC-0001 — Constituição do Projeto

1. Objetivo

Utilizar a refatoração do protocolo ISSP como ambiente real para desenvolver, observar e validar um método sustentável de colaboração entre um arquiteto de software e inteligências artificiais.

O piloto possui dois resultados esperados:

1. Evoluir a arquitetura e a implementação do ISSP.
2. Desenvolver um método de engenharia assistida por IA que possa ser reutilizado em outros projetos.

2. Escopo inicial

O piloto acompanhará a refatoração dos componentes relacionados ao ISSP, incluindo:

* definição de responsabilidades entre componentes;
* decisões arquiteturais;
* planejamento das mudanças;
* implementação realizada pela IA executora;
* validação em compilação e hardware;
* identificação de falhas, retrabalho e riscos;
* preservação do conhecimento produzido.

O piloto não pretende automatizar o processo de engenharia neste momento.

Primeiro, o método será praticado manualmente. Automatizações somente serão avaliadas depois que os pontos fortes e as falhas do processo forem compreendidos.

3. Papéis

Arquiteto e Tech Lead

Responsável por:

* definir objetivos;
* avaliar alternativas e trade-offs;
* tomar decisões arquiteturais;
* aprovar planos e mudanças;
* validar os resultados.

O responsável por esse papel é Marcelo Miranda.

IA Mentora / Consultora

Responsável por:

* preservar o contexto;
* analisar riscos;
* questionar premissas;
* apresentar alternativas;
* apoiar o planejamento;
* separar decisões arquiteturais de tarefas de implementação.

IA Executora

Responsável por:

* implementar alterações previamente delimitadas;
* realizar refatorações;
* executar verificações técnicas;
* relatar arquivos modificados, decisões locais e resultados.

A IA executora não deve decidir questões arquiteturais relevantes sem orientação do arquiteto.

4. Fluxo inicial

Cada recorte de trabalho seguirá, inicialmente, o seguinte fluxo:

Problema ou necessidade
        ↓
Análise arquitetural
        ↓
Decisão do arquiteto
        ↓
Definição de um recorte pequeno
        ↓
Execução pela IA
        ↓
Compilação e validação
        ↓
Registro do resultado
        ↓
Próximo recorte

5. Regras do piloto

1. Cada execução deve possuir um objetivo claramente delimitado.
2. Mudanças arquiteturais devem ser discutidas antes da implementação.
3. A IA executora deve interromper a execução quando encontrar ambiguidade arquitetural relevante.
4. O resultado deve informar claramente:
    * o que foi alterado;
    * quais arquivos foram modificados;
    * como a mudança foi validada;
    * quais riscos ou pendências permanecem.
5. Conhecimento permanente deve ser registrado no repositório.
6. O processo não deve criar mais burocracia do que valor.
7. O consumo de contexto, o retrabalho e o custo operacional devem ser considerados.

6. Estado inicial

O piloto começa com a refatoração do client ISSP já em andamento.

As mudanças anteriores serão consideradas contexto histórico, mas a aplicação formal do método começa a partir do próximo recorte de implementação.

Não será necessário reprocessar ou documentar retroativamente todos os passos anteriores.

7. Critério inicial de sucesso

O piloto será considerado útil se demonstrar capacidade de:

* manter coerência arquitetural durante uma refatoração extensa;
* reduzir erros causados por falta de contexto;
* delimitar melhor as tarefas entregues à IA executora;
* reduzir retrabalho;
* preservar decisões relevantes;
* manter o arquiteto como responsável pelas decisões;
* permitir continuidade entre diferentes sessões e ferramentas de IA.
