# Relatório de validação — variantes de firmware

**Classe da fonte:** Relatório

**Papel:** Arquiteto humano

**Especificação:** `docs/specs/Firmware-Variants-Menuconfig.md`

**Revisão confrontada:** Registro histórico EKM 1.x preservado na migração para EKOM 3.2

**Estado:** Concluído

> Este relatório preserva decisões e evidências históricas e não altera fontes
> normativas por autoridade própria.

## Resultado desta etapa

O Arquiteto aprovou o sensor de porta como segunda composição. A análise de
implementabilidade confirmou a direção estrutural e identificou B1 a B4. O
Arquiteto resolveu esses bloqueadores e autorizou as mudanças compartilhadas
descritas nas decisões 20 a 26. O Consultor reconciliou as decisões nesta
especificação e no mapa, sem iniciar implementação. Naquele momento, o recorte
ainda requeria confronto focado antes de ser promovido para implementação.

O Arquiteto confirmou o registro documental produzido pelo Consultor e
autorizou seu commit e envio à branch do experimento. Essa confirmação não
representava aprovação da implementação nem substituía o confronto focado da
Fase 2, posteriormente executado.

O confronto focado posterior sustentou as decisões 20 a 26 e não encontrou
bloqueador normativo. O Arquiteto considerou o confronto suficiente, resolveu
C1 a C3 nas decisões 27 a 29, reconheceu as consequências de ciclo de vida,
dependência FreeRTOS e infraestrutura de testes e promoveu a Fase 2 para
`Implementable / Ready`. A promoção autoriza implementação, mas não declara
execução ou validação concluída.

O Arquiteto confirmou este registro e autorizou seu commit e envio à branch do
experimento. A confirmação ratifica C1 a C3 e a promoção de prontidão, sem
declarar a Fase 2 implementada ou validada.

## Decisão vigente de implementabilidade

O Arquiteto mantém a direção integral como `Implementable` e promove a Fase 2
para `Implementable / Ready`. B1 a B4 e C1 a C3 estão normativamente resolvidos;
o Implementador pode iniciar o recorte definido. Isso não altera a conclusão da
Fase 1 nem declara a Fase 2 implementada ou validada.

## Encerramento da Fase 2 e do experimento EKOM (Arquiteto)

O Arquiteto declarou a implementação corretiva funcional em hardware e aprovou
a revisão focada do commit `9c313a7`. Essa aprovação ratifica a leitura da
decisão 32: falha de publicação não confirma estado nem derruba o boot; falha de
leitura, criação ou armamento do timer continua terminal. As evidências foram
consideradas suficientes para promover esta especificação a `Active` e a
implementação a `Validated`.

O resultado de hardware foi fornecido como declaração de suficiência do
Arquiteto, sem log detalhado anexado nesta atuação. Não se inferem medições ou
cenários além da aprovação informada. Os 39 casos automatizados permanecem
deliberadamente `Not Executed` sob TESTEXEC-009; essa condição é conhecida e não
é pendência para este encerramento.

A1, A2, A3, A7, A8 e A9 estão encerrados pela implementação corretiva e pela
aprovação. A4, A5 e A6 permanecem limitações aceitas nas decisões 38 e 39, sem
impedir uso ou evolução dentro do contrato vigente. A janela de callback em voo
do `esp_timer` continua limitação conhecida da API.

O experimento EKOM é encerrado como útil para este recorte: o mapa e a
especificação permitiram localizar composição, product firmware, board e
componentes compartilhados; a segunda variante foi adicionada sem condicionais
de produto nos componentes, sem duplicar runtime e sem alterar protocolo ou
coordenador. A revisão também revelou e corrigiu a fronteira semântica do board,
que passou a representar o modelo físico `Door Sensor Battery H2`.

Evoluções futuras — novas variantes, boards, uso de `publishReport()` ou mudança
do ciclo de vida dos behaviors — exigem novo confronto com as limitações
registradas, mas não reabrem esta implementação automaticamente.
