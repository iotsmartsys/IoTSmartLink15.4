# Análise de implementabilidade — luminosidade a bateria H2

**Classe da fonte:** Relatório
**Papel:** Engenheiro Analista
**Especificação:** `docs/specs/Light-Sensor-Battery-H2.md`, rascunho sem versão declarada
**Revisão confrontada:** `938d852bc61aecc4aac50ef2135076b33ed71397`
**Estado:** Análise registrada; implementação não iniciada

## Bloqueadores

**B1 — Permanência noturna ambígua.** Problema: as regras 3 e 5 prescrevem resultados concorrentes. Evidência: `docs/specs/Light-Sensor-Battery-H2.md:56` manda crescer o intervalo em NIGHT_SLEEP com luz abaixo da saída e delta pequeno; `docs/specs/Light-Sensor-Battery-H2.md:62` exclui somente saída imediata e entrada no estado, mandando selecionar TRANSITION nos demais casos. Com estado anterior NIGHT_SLEEP, leituras consecutivas de 2% e intervalo de 300 s, a regra 3 implica permanência e 600 s, enquanto a regra 5 implica TRANSITION e 30 s. Impacto: não há oráculo inequívoco para Política e Retenção. Regra de bloqueio: contradição interna e decisão de comportamento ausente; a precedência não pode ser inventada pelo Implementador.

**B2 — Composição física e calibração sem dados.** Problema: o produto exige valores provenientes da montagem real, mas eles não existem na fonte. Evidência: `docs/specs/Light-Sensor-Battery-H2.md:19`, `:90` e `:110` exigem calibração real e recursos ADC do board, reconhecem ausência de pinagem/circuito/calibração e proíbem substitutos ilustrativos. Impacto: não se pode entregar a composição física contratada nem fixar seus extremos elétricos sem inventar dados externos. Regra de bloqueio: entrada e decisão de composição ausentes na própria funcionalidade. Não constitui prova de impossibilidade física nem necessidade demonstrada de experimento arquitetural prévio.

**B3 — Identidade de evento sem alocação.** Problema: o evento 6 permanece proposta dependente de decisão normativa. Evidência: `docs/specs/Light-Sensor-Battery-H2.md:96` e `:107`; `docs/adr/ADR-0005-CAPABILITY-IDENTITY.md:125` exige emenda para tipo novo e proíbe usar tipo não registrado. A guarda ainda admite exatamente 1–5 (`coordinator_154/main/verify_event_registry.cmake:10`). Impacto: a integração Light Sensor não tem tipo autorizado no registro global. Regra de bloqueio: decisão normativa ausente, reservada ao Arquiteto; alterar somente código e guarda não estabelece a alocação.

## Reconciliação anterior

Não foi localizado relatório anterior desta linhagem em `docs/reports/`; nenhum bloqueador anterior ficou sem disposição. Relatórios de presença informam precedentes, mas não analisam esta especificação.

## Cobertura e challenge

Confrontados integralmente sete blocos de contrato: objetivo, aquisição, política, retenção, energia, organização/tradução e autoridades/pendências; sete critérios de aceite; cinco débitos registrados, todos quitados (`docs/rfc/KNOWLEDGE-MAP.md:159`). Nenhuma lacuna de cobertura; as pendências materiais são B1–B3. Defaults e endpoint explicitamente propostos não foram apresentados como decisões confirmadas.

O challenge limitado encontrou a contradição B1 e as dependências B2–B3 dos critérios. Não encontrou remediação de débito postergada implicitamente exigida. Não foi demonstrado pré-requisito arquitetural independente: aquisição, retenção restrita à política, admissão e timer podem permanecer nos donos naturais dentro do recorte proposto. A classificação não decorre apenas do estado Rascunho.

## Restrições materiais não bloqueantes

- A extensão de segundos e intervalo adaptativo precisa preservar configuração e consumidores existentes: a conversão atual aceita minutos/horas (`components/issp_app_154/src/smart_sys_app_deep_sleep.cpp:63`); a emenda funcional está delimitada em `docs/specs/Light-Sensor-Battery-H2.md:104`.
- Sono antecipado exige evidência positiva da nova capability, preservando arbitragem, quiescência e deadline existentes (`components/issp_app_154/src/smart_sys_app_deep_sleep.cpp:563`). Admissão não comprova entrega ao host.
- Identidade de report conserva a semântica de cada admissão e retry (`docs/specs/ISSP-Report-Identity.md:57`); o percentual cabe no campo vigente de um byte (`coordinator_154/main/iot154_packet.h:69`).
- Nenhum build, teste, flash, monitor ou hardware foi executado. O rascunho exclui artefatos de teste automatizado e mantém autorização operacional própria (`docs/specs/Light-Sensor-Battery-H2.md:128`).

## Qualificação do repositório — separada da tarefa

`AGENTS.md:3` e `docs/rfc/EKOM-GUIDELINES.md:1` declaram EKOM 4.6; os perfis externos lidos declaram EKOM 5.0/perfil 4.0. Não foram localizados contrato de engenharia aprovado com revisão nem Repository Readiness cobrindo client H2, componentes compartilhados e coordenador C6. A habilitação exigida pelo perfil externo não está demonstrada. Este parecer não migra governança nem habilita implementação.

**Classificação principal: Não pronta — defeito da especificação [`Not Ready — Specification Defect`].**
