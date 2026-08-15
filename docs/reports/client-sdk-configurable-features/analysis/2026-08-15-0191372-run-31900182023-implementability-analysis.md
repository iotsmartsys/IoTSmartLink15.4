# Análise de implementabilidade — EKOM-CLIENT-CONFIG-001

**Especificação:** `docs/specs/Client-SDK-Configurable-Features.md`

**Branch analisada:** `spec/client-sdk-configurable-features`

**Revisão analisada:** `0191372`

**Execução do GitHub Actions:** `31900182023`

**Data da análise:** `2026-08-15`

---

## Relatório do Engenheiro Analista

Classificação consultiva: **Pronta [`Ready`]** para a versão 0.1. Existe implementação plausível dentro da baseline e do recorte, sem pré-requisito arquitetural independente. Como a ordem proíbe escrita, este parecer não conclui formalmente o estágio nem promove o estado normativo.

1. Fontes normativas e elementos afetados

- A especificação analisada governa configuração estática, energia, bateria periódica e GPIO de reset.
- `Amends`: `Firmware-Variants-Menuconfig.md`, `Client-Deep-Sleep.md@v0.11`, `Client-Battery-Level.md@v0.5` e ADR-0002.
- Preserva: `ISSP-Configurable-Bootstrap.md`, ADR-0001, ADR-0005, protocolo, coordenador e host.
- Elementos afetados: composição produto/board, recursos físicos condicionais, lifecycle periódico da bateria e validação de GPIO/intervalos.
- `EKOM-DEBT-0005` permanece aceito e fora do recorte, conforme [KNOWLEDGE-MAP.md](/home/runner/work/IoTSmartLink15.4/IoTSmartLink15.4/docs/rfc/KNOWLEDGE-MAP.md:221).

2. Relação com as autoridades vigentes

- ADR-0002 já contém a exceção arquitetural expressamente aceita para parametrizar o GPIO do botão, preservando recurso e polaridade no board model ([ADR-0002](/home/runner/work/IoTSmartLink15.4/IoTSmartLink15.4/docs/adr/ADR-0002-PRODUCT-BOARD-COMPOSITION.md:28)).
- A separação Kconfig → composição, produto → política e board → fatos físicos é preservada.
- Deep sleep continua opt-in e sua validação de timer permanece na fachada ([smart_sys_app_deep_sleep.cpp](/home/runner/work/IoTSmartLink15.4/IoTSmartLink15.4/components/issp_app_154/src/smart_sys_app_deep_sleep.cpp:201)).
- Endpoint 2 e evento 3 da bateria permanecem inalterados.
- A especificação altera legitimamente o gatilho periódico anterior, que hoje mede imediatamente em `begin()`.
- Há divergência administrativa: `AGENTS.md` declara EKOM 4.4 e `.ekom-guidelines` declara 4.5. A análise aplicou as regras canônicas 4.5 fornecidas pela Action.

3. Componentes e arquivos potencialmente impactados

- `client_154/main/Kconfig.projbuild`
- `client_154/main/CMakeLists.txt`
- ambos os arquivos em `client_154/main/boards/`
- `client_154/main/firmwares/door_sensor_battery_h2.cpp`
- potencialmente `components/issp_behaviors/{include,src}/battery_level_behavior.*`
- potencialmente `components/issp_app_154/src/smart_sys_app.cpp` e seu cabeçalho interno
- documentação de autoridade, mapa e changelog na reconciliação posterior.

O `single_smart_plug` é consumidor material do GPIO configurável porque também recebe `selectedUserButton()`.

4. Comportamento existente relevante

- Kconfig seleciona apenas produto e board ([Kconfig.projbuild](/home/runner/work/IoTSmartLink15.4/IoTSmartLink15.4/client_154/main/Kconfig.projbuild:3)).
- CMake exige atualmente todos os recursos de deep sleep e bateria incondicionalmente para o sensor ([CMakeLists.txt](/home/runner/work/IoTSmartLink15.4/IoTSmartLink15.4/client_154/main/CMakeLists.txt:15)).
- O firmware fixa 30 segundos, 15 minutos, bateria habilitada e `samplePeriodMs=0` ([door_sensor_battery_h2.cpp](/home/runner/work/IoTSmartLink15.4/IoTSmartLink15.4/client_154/main/firmwares/door_sensor_battery_h2.cpp:24)).
- `BatteryLevelBehavior::begin()` inicializa ADC, mede imediatamente e só depois inicia o timer ([battery_level_behavior.cpp](/home/runner/work/IoTSmartLink15.4/IoTSmartLink15.4/components/issp_behaviors/src/battery_level_behavior.cpp:179)).
- `Running/Completed/Ok` é estabelecido apenas ao fim de `setup()` ([smart_sys_app.cpp](/home/runner/work/IoTSmartLink15.4/IoTSmartLink15.4/components/issp_app_154/src/smart_sys_app.cpp:511)).

5. Restrições, incertezas e decisões

Não há decisão normativa ausente. São escolhas locais não bloqueantes:

- iniciar o timer periódico somente após o sucesso integral de `setup()`;
- preservar medição imediata quando `samplePeriodMs=0`;
- validar conversões antes de estreitamento;
- manter `CONFIG_*` exclusivamente em `client_154/main`;
- implementar colisões sem transferir fatos físicos do board para componentes compartilhados.

6. Evidências necessárias

Na implementação autorizada: inspeção do menu e do delta, além dos builds canônicos ESP32-H2 das combinações contratadas. Execução, testes, flash e hardware continuam `Not Executed`; validação física posterior é necessária para tempos, bateria periódica, ausência da capability e GPIO alternativo.

7. Controle EKOM

Foram confrontados os 25 requisitos, 12 critérios e o débito relacionado. Não há relatório anterior desta submissão. O challenge final não encontrou contradição interna, critério insatisfazível, remediação de `EKOM-DEBT-0005` implicitamente exigida ou pré-requisito transversal.
