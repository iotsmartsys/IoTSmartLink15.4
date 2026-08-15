# Análise experimental de implementabilidade — `EKOM-BATTERY-001` v0.4

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/Client-Battery-Level.md` v0.4

**Revisão confrontada:** `6f2e067`

**Estado:** Concluído

**Data:** 14/08/2026

**Modelo:** branch EKOM
`experiment/implementability-readiness-boundary`,
`REGRAS-COMUNS.md` 3.5-experimental e
`ENGENHEIRO-ANALISTA.md` 3.4-experimental

> Este relatório é evidência histórica. Não altera a especificação, não aceita
> ou quita débito, não promove estado e não autoriza implementação.

## Classificação

**Não pronta — defeito da especificação** [`Not Ready — Specification Defect`]

## Problemas bloqueantes

### 1. `BATTERY-AC-007` exige comportamento excluído do recorte

**Problema:** o critério exige que outra capability seja rejeitada ao tentar
registrar o endpoint já ocupado pela bateria, mesmo com tipo de evento distinto.
As operações das capabilities existentes continuam validando a duplicidade pelo
par `endpointId` e `eventType`, e sua correção foi postergada em
`EKOM-DEBT-0001` e declarada fora do recorte desta especificação.

**Evidência:** `BATTERY-005` e `BATTERY-011` exigem unicidade por endpoint;
`BATTERY-AC-007` descreve a tentativa posterior de outra capability. A seção
5.4 declara que a divergência das capabilities existentes não integra o
recorte. Em `smart_sys_app.cpp:147-159`, `hasDuplicateEndpoint()` compara o par,
e `addSwitchPlugCapability()` e `addDoorSensorCapability()` usam essa validação.
`KNOWLEDGE-MAP.md:144-164` registra a correção global como dívida aceita e
postergada.

**Impacto:** depois de registrar bateria no endpoint 2 com evento 3, uma
capability existente pode registrar endpoint 2 com evento 1 ou 2. O critério de
aceite falha. Tornar a rejeição bidirecional exige modificar as operações
existentes ou o registro compartilhado, remediando comportamento explicitamente
fora do recorte. A decisão de que o débito não bloquearia a bateria não foi
incorporada como limitação do critério normativo.

## Reconciliação dos achados anteriores

| Achado anterior | Disposição | Regra e evidência |
|---|---|---|
| Relação com `ISSP-Configurable-Bootstrap.md` | Descartado | A capability adiciona contrato próprio sem alterar comportamento das operações existentes. Autoridade é limitada ao comportamento explicitamente contratado; extensão aditiva não exige `Amends`. |
| Relação com `ISSP-Reusable-Components.md` | Descartado | Novo behavior e dependência são extensão aditiva. Compartilhar componente ou ampliar inventário aberto não demonstra interferência material. |
| `BATTERY-AC-007` versus unicidade existente | Mantido | O conflito é interno entre critério, recorte e remediação postergada. Autoridade limitada não afasta consistência da própria especificação. |
| `BATTERY-015` sem fonte para fundo de escala | Reclassificado como não bloqueante | A documentação oficial do ESP32-H2 remete às faixas de atenuação do datasheet, que informa faixa efetiva de 0 a 3300 mV para `ATTEN3`. Obter parâmetro de fonte técnica durante a implementação é escolha local permitida. |

Fontes oficiais usadas na última disposição:

- <https://docs.espressif.com/projects/esp-idf/en/latest/esp32h2/api-reference/peripherals/adc/index.html>
- <https://documentation.espressif.com/esp32-h2_datasheet_en.html>

## Controle de cobertura

- **Requisitos confrontados:** 17/17 (`BATTERY-001` a `BATTERY-017`)
- **Critérios de aceite confrontados:** 10/10 (`BATTERY-AC-001` a
  `BATTERY-AC-010`)
- **Débitos relacionados confrontados:** 4/4 (`EKOM-DEBT-0001` a
  `EKOM-DEBT-0004`)
- **Bloqueadores anteriores reconciliados:** 4/4
- **Lacunas de cobertura:** nenhuma

## Challenge de `Ready`

**Executado:** não aplicável, porque a classificação resultou não `Ready` antes
do challenge final.

O confronto que impediu `Ready` foi limitado à consistência interna entre
critério, recorte e dívida postergada; nenhuma investigação aberta por regressão
foi usada.

## Restrições materiais não bloqueantes

1. Em composição com deep sleep, a medição inicial precisa admitir o report
   antes de `Running` para que a drenagem existente possa transmiti-lo sem
   integrar a evidência de admissão de sleep.
2. O build deve confirmar `kImplStorageBytes` e a resolução das dependências
   públicas de `esp_adc`; leitura estática não certifica esses limites.
3. Em composição sem deep sleep, `samples × sampleIntervalMs` pode ocupar o task
   de `esp_timer`; para o primeiro produto, com deep sleep, esse caminho
   periódico não é exercitado.
4. O divisor de 470 kΩ/220 kΩ apresenta aproximadamente 150 kΩ de impedância
   equivalente no pino; a consequência sobre exatidão depende de hardware.
5. A indisponibilidade real da calibração depende da peça e pode não ser
   reproduzível sob demanda; `BATTERY-AC-008` pode permanecer sem evidência
   física mesmo após implementação.

## Limitações da análise

Nenhum build, teste, flash, monitor ou hardware foi executado. O confronto foi
somente leitura sobre a revisão indicada. As evidências de execução permanecem
para os estágios e autorizações correspondentes.
