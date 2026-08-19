# Relatório de implementação — correção de `EKOM-DEBT-REMEDIATION-001`

**Classe da fonte:** Relatório

**Papel:** Engenheiro Implementador

**Especificação:** `docs/specs/Technical-Debt-Remediation.md` v0.2

**Revisão confrontada:** `e040fc21792cfc632779b703cd77f524162c7841`

**Análise de entrada:** `docs/reports/technical-debt-remediation/analysis/2026-08-19T025345Z-e040fc2-32210000227-implementability-analysis.md`

**Modo da atuação:** `correction`

**Revisão de entrada:** `docs/reports/technical-debt-remediation/review/2026-08-19T150500Z-v0.2-45faea0-implementation-review.md`

**Implementação revisada:** `45faea033d4fd311055d6683f26bf30b5e9e6ebe`

**Estado:** Implementação concluída — encaminhada à Revisão

**Branch:** `spec/technical-debt-remediation`

**Ordem do Arquiteto:** Marcelo Miranda

**Execução:** atuação local Codex

**Data da implementação:** `2026-08-19T15:28:29Z`

---

## Gates

- Especificação v0.2, análise `Ready`, ordem explícita e devolução ordinária da
  revisão confirmadas.
- Branch de trabalho e árvore limpa confirmadas antes da mutação.
- O estado operacional já estava em Implementação [`In Progress`] por efeito da
  iteração anterior.
- A correção preservou versão, recorte, arquitetura e risco autorizados.

## Escopo realizado

- A guarda CMake agora localiza cada capability na tabela da ADR-0005, extrai o
  tipo associado e o confronta com a macro semântica correspondente em
  `iot154_packet.h`. Permanecem verificadas as quatro alocações esperadas e o
  conjunto exato de quatro definições.
- `BATTERY-AC-009` agora distingue a inércia da capability de nível, sem evento
  3, da publicação obrigatória do evento 4 com valor `2` pela capability de
  estado, preservando também o log local e a chegada a `Running`.
- Nenhum teste, comportamento funcional, layout wire ou fonte fora dos dois
  achados da revisão foi alterado.

## Evidências

### Build C6 conforme

- Comando: `IDF_COMPONENT_MANAGER=0 CCACHE_DIR=/tmp/ekom_debt_guard_ccache idf.py -B build_ekom_debt_guard_c6 -D IDF_TARGET=esp32c6 build`.
- Ambiente: ESP-IDF 6.0.1 em
  `/Users/marcelocostamiranda/.espressif/v6.0.1/esp-idf`;
  `coordinator_154`; ESP32-C6.
- Resultado terminal: `Project build complete`; `central_154.bin` com
  `0x46aa0` bytes.
- Código de saída: `0`.

### Divergência deliberada da associação

- Alteração temporária: mantido o conjunto `{1,2,3,4}`, com as associações de
  `Sensor de porta` e `Plug comutável` trocadas somente na ADR-0005.
- Comando: o mesmo build C6 incremental.
- Resultado terminal esperado: configuração interrompida com
  `event registry mismatch: ADR-0005 allocates 'Sensor de porta' as type 2; expected 1`.
- Código de saída: `2`.
- Restauração: a ADR foi restaurada integralmente e o build C6 conforme foi
  repetido com código `0`.

### Inspeções

- `python3 tools/validate_ekom_documents.py . docs/specs/Client-Battery-Level.md docs/specs/Technical-Debt-Remediation.md docs/adr/ADR-0005-CAPABILITY-IDENTITY.md`: código `0`.
- `git diff --check`: código `0`.
- Inspeção final confirmou que a ADR-0005 não integra o delta.

## Tentativas sem evidência positiva

- O caminho `/opt/esp/idf` registrado pela automação não existe neste ambiente;
  a tentativa não iniciou o build e terminou com código `127`.
- A primeira configuração pela toolchain local alcançou a nova guarda e expôs
  um escape duplicado na expressão de linha da ADR; terminou com código `2`. O
  defeito local foi corrigido antes das evidências positiva, negativa e
  restaurada acima.
- A validação documental global encontrou campos obrigatórios ausentes no
  relatório histórico
  `docs/reports/client-sdk-configurable-features/analysis/2026-08-15-0191372-run-31900182023-implementability-analysis.md`.
  O arquivo é anterior e alheio ao recorte; a validação dirigida aos documentos
  desta mudança concluiu normalmente.

## Operações não executadas

- Coleta ou execução de testes: `Not Executed`.
- Flash, monitor e hardware: `Not Executed`.
- Deploy, release, merge, tag e integração: `Not Executed`.

## Handoff para revisão

- Os dois achados da revisão `637fdbf` foram reconciliados.
- A revisão deve confrontar a associação semântica das quatro linhas da ADR com
  as quatro macros e a distinção entre evento 3 silencioso e evento 4 igual a
  `2` em `BATTERY-AC-009`.
- Nenhum débito foi quitado e nenhuma declaração `Done` foi realizada; essas
  decisões permanecem reservadas ao Arquiteto.
