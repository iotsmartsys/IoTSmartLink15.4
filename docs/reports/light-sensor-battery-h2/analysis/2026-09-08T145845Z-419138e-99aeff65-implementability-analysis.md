# Análise de implementabilidade — luminosidade H2 v0.3

**Classe da fonte:** Relatório
**Papel:** Engenheiro Analista; participação anterior na autoria como Consultor, sem alegação de independência
**Especificação:** `docs/specs/Light-Sensor-Battery-H2.md`, v0.3
**Revisão confrontada:** `419138e78d8421ab3bf8dfe856530908fd46ded2`
**Estado:** Análise registrada; implementação não iniciada

## Bloqueadores

Nenhum bloqueador normativo ou material identificado. Existe implementação tecnicamente plausível na baseline e no alcance habilitado. O resultado não certifica comportamento executado nem aprova implementação.

## Reconciliação anterior

Relatório confrontado: `docs/reports/light-sensor-battery-h2/analysis/2026-09-07T215407Z-938d852-8740e747-implementability-analysis.md`.

- **B1 — Descartado.** A contradição entre permanência noturna e transição perdeu seu objeto: a v0.3 elimina a máquina de estados, o intervalo adaptativo e a retenção (`docs/specs/Light-Sensor-Battery-H2.md:71`). Não há precedência de transições a decidir.
- **B2 — Descartado.** GPIO2, alimentação de 3,3 V e divisor LDR/10 kΩ estão definidos (`docs/specs/Light-Sensor-Battery-H2.md:40`). A normalização passou a usar a escala digital de 12 bits e exclui calibração por extremos (`:28`). As medições físicas não foram obtidas; deixaram de ser entrada exigida pelo contrato. Não resta dado de calibração a inventar.
- **B3 — Descartado.** A alocação humana do evento 6 consta de `docs/adr/ADR-0005-CAPABILITY-IDENTITY.md:13` e `:117`. A divergência da guarda permanece como trabalho funcional explicitamente contratado (`docs/specs/Light-Sensor-Battery-H2.md:125`), não como decisão normativa ausente.

## Cobertura e challenge

Cobertura integral dos requisitos agrupados nos seis blocos das seções 1–6, dos nove critérios de aceite da seção 7 e dos cinco débitos registrados, todos quitados (`docs/rfc/KNOWLEDGE-MAP.md:170`). Zero lacunas de confronto e zero bloqueadores anteriores sem disposição. As decisões humanas sobre cadência e precisão estão incorporadas; a discussão posterior de duas casas decimais não alterou a v0.3.

Aquisição, composição, admissão, identidade, falhas, tradução e encerramento têm donos e interfaces compatíveis na baseline: `components/issp_core/include/idevice_behavior.hpp:9`, `components/issp_app_154/src/smart_sys_app.cpp:298`, `components/issp_app_154/src/smart_sys_app_deep_sleep.cpp:560` e `coordinator_154/main/main.c:437`. O teste de fronteira não identificou capacidade arquitetural independente: a extensão permanece nos donos naturais, com emendas comportamentais delimitadas na especificação (`docs/specs/Light-Sensor-Battery-H2.md:134`), sem novo lifecycle, persistência ou layout wire.

O challenge final limitado não encontrou contradição interna, critério insatisfazível, remediação necessária fora do recorte ou bloqueador anterior sem disposição. Os critérios distinguem inspeção, construção e observação autorizada; não exigem artefatos automatizados fora do alcance. Evidência de execução futura não é indispensável para decidir esta implementabilidade.

## Restrições materiais não bloqueantes

1. Sono antecipado depende de evidência positiva de admissão da luminosidade. Erro ADC e recusa de admissão não podem produzir essa evidência; continuam sujeitos ao deadline e às exceções vigentes (`docs/specs/Light-Sensor-Battery-H2.md:94`).
2. O timer permanece em minutos, com validação de domínio e limite; defaults e comportamento das outras composições devem ser preservados (`components/issp_app_154/src/smart_sys_app_deep_sleep.cpp:285`; `client_154/main/Kconfig.projbuild:76`).
3. A guarda C6 ainda exige cinco eventos e falhará diante da ADR com seis (`coordinator_154/main/verify_event_registry.cmake:10`). Sua reconciliação pertence à implementação; ignorá-la não satisfaz o critério de construção.
4. O inteiro 0–100 cabe no campo existente; no JSON permanece texto decimal, preservando o tipo do envelope (`components/issp_core/include/issp_types.hpp:21`; `coordinator_154/main/main.c:924`). Não há suporte contratado a fração publicada.
5. Build, testes, flash, monitor e hardware não executados nesta análise. A ausência dessas evidências não demonstra validação física nem entrega ao host; suas permissões continuam delimitadas pela especificação (`docs/specs/Light-Sensor-Battery-H2.md:192`).

## Qualificação do repositório — separada da tarefa

EKOM 5.0 vigente; contrato de engenharia v0.1 Approved, alcance inicial habilitado por Marcelo Miranda. `docs/rfc/REPOSITORY-READINESS.md:15` cobre client H2, componentes, composição, coordenador C6, exemplo e guards necessários. A avaliação complementar `docs/reports/repository-readiness/2026-09-08T012937Z-v0.1-event6-073b9609.md` cobre a alocação aceita do evento 6. A incompatibilidade de governança anterior foi resolvida. O estado global permanece Conditionally Ready pelas exclusões de diagnóstico raiz e automações, sem dependência necessária desta tarefa nessas áreas. Esta análise não amplia a habilitação nem autoriza implementação.

**Classificação principal: Pronta [`Ready`].**
