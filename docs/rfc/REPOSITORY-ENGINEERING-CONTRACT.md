# Repository Engineering Contract — IoTSmartLink15.4

**Classe da fonte:** Normativa
**Estado:** Approved — vigente
**Versão:** 0.1
**Repositório e escopo:** IoTSmartLink15.4; construção de firmware e seus consumidores locais, conforme seção 1
**Responsável técnico:** Marcelo Miranda — Arquiteto
**Aprovação:** Marcelo Miranda, Arquiteto, em 07/09/2026 — v0.1 e alcance inicial da seção 1
**Decisão humana:** “Sim, aprovo o contrato v0.1 e a habilitação desse alcance inicial.” Decisão de Marcelo Miranda nesta atuação, registrada em `docs/rfc/EKOM-CHANGELOG.md`, `EKOM-CHG-0011`, seção Aprovação e habilitação. O alcance e a avaliação vigente estão em `docs/rfc/REPOSITORY-READINESS.md`.

As regras abaixo são imperativas no alcance aprovado. Regras vigentes nas fontes referenciadas conservam autoridade própria. A seção 13 distingue a consolidação de normas das convenções aprovadas nesta adoção; código frequente não é autoridade normativa.

## 1. Arquitetura e limites — REC-ARCH

Preservar a arquitetura por componentes e composição estática da ADR-0001 e ADR-0002:

| Dono | Responsabilidade e limite |
|---|---|
| `client_154/main/firmwares/` | Compor capabilities, parâmetros e política de produto pela fachada; não implementar rádio, ACK ou persistência da rede |
| `client_154/main/boards/` | Declarar recursos, pinagem e características elétricas reais; não decidir política de produto |
| `issp_app_154` | Fachada `SmartSysApp`, configuração, composição e lifecycle; delegar protocolo, behaviors e transporte |
| `issp_behaviors` | Aquisição/atuação e publicação pela interface do core; não conhecer produto, board ou política de rede |
| `issp_core` | Tipos, codec, device, comandos e fila de reports; não operar periféricos, rádio ou NVS |
| `issp_transport_154` | Rádio client, commissioning, persistência da rede e execução de reports |
| `coordinator_154/main/` | Aplicação C independente: rádio, registry, políticas de reports/comandos e ponte UART |
| `examples/issp_minimal_client` | Consumidor independente de integração; não copiar aplicação client nem iniciar rádio/NVS automaticamente |

O alcance aprovado cobre esses diretórios, CMake/Kconfig e configurações dos três projetos, `cmake/` e os test apps dos componentes/coordenador quando contratados por tarefa. Não inclui o firmware diagnóstico da raiz (`EKM-GAP-0007`), implementação do host, serviços externos, instalação de toolchain ou alterações nas automações e scripts de submissão. Esses recortes precisam de avaliação e regras próprias antes de implementação. Documentação e guarda documental existente podem preparar sua qualificação.

Verificar por inspeção dos donos e do delta; não redesenhar o coordenador para imitar a fachada C++ do client.

## 2. Organização de arquivos — REC-ORG

Adicionar produtos em `client_154/main/firmwares/`, boards em `client_154/main/boards/`, API pública de componente em `include/` e implementação privada em `src/`. Manter entrada client mínima em `app_main.cpp`; seleção de fontes em `client_154/main/CMakeLists.txt`.

Manter módulos e headers privados do coordenador em `coordinator_154/main/`; exemplos em `examples/`; test apps junto ao componente ou alvo dono. Não mover teste nem criar diretório transversal por conveniência.

Registrar comportamento em `docs/specs/`, decisão arquitetural em `docs/adr/`, execução em `docs/reports/`, localização no mapa e estado resumido no changelog. Não versionar build, logs temporários ou credenciais. Verificar destinos pelo delta Git e manifestos CMake.

## 3. Nomenclatura e estilo — REC-STYLE

Para código novo, usar nomes em inglês e quatro espaços, sem tabs. Em C++, usar tipos em PascalCase, métodos/variáveis em camelCase, membros privados com sufixo `_` e constantes com prefixo `k`; preservar namespaces `iotsmartsys`, `iotsmartsys::app`, `iotsmartsys::core` e `issp` conforme o dono. Em C, usar funções/variáveis/tipos em snake_case, typedefs com `_t`, constantes/macros em UPPER_SNAKE_CASE e prefixo do módulo para símbolos externos.

Usar arquivos novos em snake_case e extensões `.cpp/.hpp` ou `.c/.h`; preservar `SmartSysApp.h` como exceção pública existente. Em edição localizada, preservar convenções e posição de chaves do arquivo; não reformatar trechos alheios. Em arquivo novo sem precedente mais específico, usar chaves em linha própria. Comentários devem explicar contrato, motivo ou restrição.

Verificar por revisão do delta e `git diff --check`. Não há formatter canônico versionado; este contrato não instala ferramenta nem torna a formatação histórica um desvio a corrigir globalmente.

## 4. Dependências — REC-DEP

Aplicação client depende de `issp_app_154`; consumidor técnico explícito pode depender das APIs públicas dos componentes. Fachada depende privadamente de core, behaviors e transporte; behaviors e transporte dependem de core. Proibir dependência reversa de componentes para `client_154`, `coordinator_154` ou `examples` e inclusão de headers privados por caminhos relativos entre componentes.

Declarar `REQUIRES` quando tipos aparecem em headers públicos e `PRIV_REQUIRES` quando usados somente internamente. A fachada pode expor tipos de periférico ESP-IDF fornecidos pelo board (ADR-0001, nota de 14/08/2026), mas não tipos de protocolo, transporte ou commissioning. Preservar o uso limitado de FreeRTOS no core para serialização de reports, explicitamente mantido em `ISSP-Report-Identity.md` v0.3, seção 7.1; isso não autoriza drivers no core.

Client e coordenador compartilham contrato wire, não fontes de suas aplicações. Verificar includes, manifestos e consumidores reais afetados.

## 5. Padrões de implementação — REC-IMPL

Preservar fachada fina, `Impl` opaco, armazenamento de capacidade fixa, ponteiros estáveis e `static_assert` de tamanho/alinhamento. Configurar antes de `setup()`; depois de iniciá-lo, manter a fachada e objetos vivos até reboot. Não introduzir retry de setup, stop público ou destruição operacional por analogia.

Implementar behaviors conforme `IDeviceBehavior` e publicar por `IBehaviorStatePublisher`; parar trabalho autônomo no contrato de quiescência aplicável. Preservar sincronização de reports e executar callbacks, encoding e transporte fora da seção crítica conforme o contrato de identidade.

Endpoint é não zero, único e congelado por produto; evento pertence à capability e requer alocação na ADR-0005. Capability somente leitura reconhece seu par e recusa comandos pelo behavior. DTOs/configurações devem explicitar unidades, domínio, ownership e resultado; serialização wire não depende de layout nativo de structs.

Serviços HTTP, ORM e containers de injeção genéricos não se aplicam ao firmware avaliado; não introduzi-los para satisfazer nomenclatura de template. Verificar interfaces, lifecycle e chamadas com os precedentes da seção 11.

## 6. Persistência e dados — REC-DATA

Manter descritor de rede no network manager e registry do coordenador na sua camada de armazenamento (`device_registry_nvs.c`). Validar versão, tamanho e domínio antes de aplicar blob; preservar commit completo e tratamento distinto de ausência, corrupção e erro.

Não apagar NVS globalmente como recuperação genérica. Preservar somente os caminhos já contratados: recuperação de inicialização do client em `ISSP-Configurable-Bootstrap.md` v1.5 e operações de reset delimitadas. No coordenador, preservar indisponibilidade observável sem erase global do registry.

Não criar histórico persistente, migração ou retenção RTC por inferência de produto semelhante: a tarefa deve contratar conteúdo, dono, integridade, compatibilidade e descarte. Reports pendentes e deduplicação permanecem voláteis nos limites de `ISSP-Report-Identity.md` v0.3. Verificar todas as chamadas de armazenamento alteradas e seus caminhos de falha.

## 7. Erros e logging — REC-ERROR

Usar `AppResult`/`SetupResult` na fachada, `IsspResult`/`IsspCommandResult` nas APIs técnicas e `esp_err_t` nos limites ESP-IDF/C aplicáveis. Propagar falhas e distinguir indisponibilidade, argumento inválido e falha operacional; não converter falha em sucesso ou medição válida artificial.

Registrar estágio, causa e contexto operacional suficiente pelos mecanismos ESP-IDF já usados; não registrar segredo, token, chave, header de autorização ou connection string em logs, documentos, comandos ou artefatos. Não expor configuração integral para diagnosticar um campo.

Preservar limites de espera, recuperação, quiescência e arbitragem com factory reset. Build não prova entrega, ACK não prova consumo pelo host. Verificar retornos, logs e caminhos de cleanup do delta.

## 8. Tecnologias e abstrações — REC-TECH

Usar ESP-IDF exatamente 6.0.1 e sua toolchain, C/C++ e FreeRTOS existentes, CMake/Kconfig, IEEE 802.15.4, NVS e UART JSON-lines. Client e exemplo usam ESP32-H2; coordenador usa ESP32-C6. Preservar guards de versão/target.

Não admitir ESP32-C3, QEMU ou target ESP-IDF linux. Host-native é processo com toolchain host, somente para lógica pura com substituto fiel, conforme política v0.4. Não adicionar framework, dependência ou abstração transversal sem decisão arquitetural aplicável.

Kconfig seleciona composição e parâmetros entregues pelo produto; componentes compartilhados não leem símbolos de produto/board. A exceção de GPIO configurável de factory reset da ADR-0002 é restrita ao board e não se estende a outros pinos. Verificar símbolos de configuração e fronteiras de build.

## 9. Build canônico — REC-BUILD

Em implementação habilitada e autorizada, ativar o ambiente existente ESP-IDF 6.0.1 (`source "$IDF_PATH/export.sh"`) e confirmar a versão. Não instalar toolchain. Usar diretório de build e cópia de sdkconfig próprios da execução, fora da árvore versionada; preservar configurações autoritativas salvo mudança explicitamente contratada.

Forma canônica, a partir da raiz, após preparar variáveis e cópia da configuração:

```sh
IDF_COMPONENT_MANAGER=0 idf.py -C "$ekom_project" -B "$ekom_build_dir" -D SDKCONFIG="$ekom_sdkconfig" -D IDF_TARGET="$ekom_target" build
```

| Projeto | Target | Configuração de partida |
|---|---|---|
| `client_154` | `esp32h2` | cópia de `client_154/sdkconfig`, com seleção explícita de produto/board afetados |
| `coordinator_154` | `esp32c6` | cópia de `coordinator_154/sdkconfig` |
| `examples/issp_minimal_client` | `esp32h2` | defaults do projeto; configuração local só após conferir sua composição |

Para variante, alterar somente símbolos necessários na cópia; deixar o Kconfig resolver dependências. Não reutilizar configuração desconhecida. Preservar seleção default normativa e verificar `compile_commands.json`/sdkconfig gerado quando composição mudar.

Construir consumidores materialmente afetados: alteração só de produto requer sua composição; fachada/API compartilhada requer client e exemplo; seleção comum requer composições alcançadas; wire ou tradução no coordenador requer C6 e H2 quando o contrato cruzar ambos. Não compilar C6 artificialmente como consumidor da fachada. Test apps seguem seu vínculo físico e só entram no recorte quando contratados.

Registrar ambiente, comando, composição, resultado terminal e saída. Falha ou build ausente impede declarar implementação concluída. A aprovação documental não executa nem substitui build.

## 10. Testes e conformidade — REC-VERIFY

Aplicar `Repository-Test-Execution-Policy.md` v0.4. Criar/alterar testes somente quando exigidos pela especificação e vinculados a cenário, resultado e critério; execução/coleta, flash, monitor e hardware exigem autorização própria. Preservar testes fora do recorte; incompatibilidade é evidência, não autorização de correção.

Verificar construção pelas regras REC-BUILD, limites e estilo por inspeção dirigida do delta, e documentos novos/alterados pela guarda `python3 tools/validate_ekom_documents.py .` com lista de arquivos quando houver limitação histórica registrada. A guarda é estrutural; não autentica aprovação, não certifica arquitetura e não habilita implementação. Não alterar relatórios históricos para obter resultado verde.

Considerar conforme apenas a obrigação sustentada por sua evidência. Não presumir sucesso comportamental a partir de build; declarar `Not Executed` quando aplicável. Ausência de suíte executada nesta adoção não invalida por si só a capacidade de construir dentro das regras.

## 11. Precedentes oficiais — REC-PRECEDENT

Os exemplos existem na baseline avaliada e foram oficializados pela aprovação deste contrato; não legitimam todo o arquivo como norma.

| Alteração | Caminho e símbolo | Regra exemplificada | Limite |
|---|---|---|---|
| Produto | `client_154/main/firmwares/presence_sensor_battery_h2.cpp`: `startSelectedProductFirmware` | composição pela fachada e recursos do board | não copiar pinagem, calibração ou semântica para outro sensor |
| Board | `client_154/main/boards/board_model.hpp`: `DigitalInputResource` | fatos físicos separados da política | recurso novo precisa de contrato próprio |
| Capability | `components/issp_app_154/src/smart_sys_app.cpp`: `addPresenceSensorCapability` | configuração, identidade e token estável | não reutilizar evento 5 para outra natureza |
| Behavior | `components/issp_behaviors/src/battery_level_behavior.cpp`: `begin`, `quiesce` | dono dos recursos e encerramento | calibração e gatilho de bateria não regem luminosidade |
| Reports | `components/issp_core/src/issp_device.cpp`: `publishState` | admissão serializada e retorno explícito | não duplicar fila no produto |
| Rede | `components/issp_transport_154/src/issp154_network_manager.cpp`: `loadPersistedNetwork` | validação antes de aplicação | não estender schema sem contrato |
| Registry | `coordinator_154/main/device_registry_nvs.c`: `nvs_storage_write` | persistência encapsulada com commit | não implica aprovação funcional de todos os cenários |
| Tradução | `coordinator_154/main/main.c`: `type_from_event`, `value_from_event` | semântica de evento e valor no host | alocação pela ADR-0005 |
| Integração | `examples/issp_minimal_client/main/main.cpp` | consumidor independente | prova de build, não produto operacional |

## 12. Evolução e exceções — REC-EVOLVE

Aplicar especificação da tarefa → contrato aprovado → precedentes oficiais → demais código → preferência local, dentro da autoridade de cada fonte. Exceção exige decisão humana que identifique regra, motivo, alcance e validade. Nova camada, ownership, lifecycle, API reutilizável, persistência, protocolo ou dependência transversal exige avaliação arquitetural; quando independente da funcionalidade e material, preparar separadamente.

Mudança material deste contrato, de fonte referenciada ou baseline exige reavaliação do alcance afetado. Edição ordinária conforme não exige reaprovação universal. Manter histórico de revisões e avaliações; somente Marcelo Miranda ou responsável técnico humano designado aprova contrato e habilitação.

## 13. Natureza das regras e decisões registradas

**Consolidação de decisões vigentes:** fronteiras, lifecycle, dependências, dados, identidade, targets e permissões remetem às autoridades abaixo, com suas emendas delimitadas. A menção histórica de independência ESP-IDF do core não exclui o FreeRTOS explicitamente preservado pela identidade v0.3.

**Convenções aprovadas nesta adoção:** REC-STYLE; operacionalização uniforme dos comandos e seleção de consumidores REC-BUILD; conjunto de precedentes REC-PRECEDENT; alcance inicial da seção 1. Foram propostas a partir da baseline e aprovadas por Marcelo Miranda em 07/09/2026. A aprovação não altera o conteúdo técnico da v0.1 submetida.

**Pendências externas ao alcance inicial:** propósito do diagnóstico raiz; qualificação de alteração/operação das automações e serviços externos. Não foram aceitas como débito técnico. Qualificação não quita lacunas funcionais nem muda estados históricos de especificações.

## 14. Fontes e revisões vinculadas

Versões consultadas: Architecture v1.2; Reusable Components v1.1; Configurable Bootstrap v1.5; Commissioning v1.0; Report Identity v0.3; Client Deep Sleep v0.11; Client Battery Level v0.5; Client SDK Configurable Features v0.1; Presence Sensor Battery H2 v0.2; Registry do coordenador v0.4; Repository Test Execution Policy v0.4. As ADRs 0001–0005 e Firmware Variants não têm versão semântica própria: os conteúdos abaixo fixam também suas revisões e emendas. Hash é da fonte completa; alteração exige avaliar relevância material, não reaprovação automática por qualquer edição.

| Fonte local | SHA-256 do conteúdo confrontado |
|---|---|
| `docs/specs/ISSP-Architecture.md` | `e962bef7992f67201413ec20dd82daa01141bc9cd4fdd4514cd72dea81cf329f` |
| `docs/specs/ISSP-Reusable-Components.md` | `5ae22f431f49cc666b33d5f1b68575f1a9ff9ac033d316c12c4c5d1eeaa17af6` |
| `docs/specs/ISSP-Configurable-Bootstrap.md` | `f36a8dcf7eb0a7cfa010271a69163406a396589a57e5eaa69d27b72fbbc796a1` |
| `docs/specs/ISSP-Commissioning.md` | `3032208ae3a0e823651aae5b086ac2ffe79da1c66375e981fcc0e96a61ad785a` |
| `docs/specs/ISSP-Report-Identity.md` | `8b1cf41018d4d5f71d38b2d84edd48b48f0f2dff82bc92384dc8839b65293fb7` |
| `docs/specs/Client-Deep-Sleep.md` | `22a4c0f58f01c74ba63b9ec610d937cdaec51eb7672525ab28f9ed085bb467d3` |
| `docs/specs/Client-Battery-Level.md` | `289700c3eff6522a33dbf9cace60f0cdb54a851ca6ea342756a9b8963334d5b4` |
| `docs/specs/Client-SDK-Configurable-Features.md` | `c8d7190ac38412805cc1e2c1fd269fd8e86a698b7a014e57ae4425c1a7cf5b5a` |
| `docs/specs/Presence-Sensor-Battery-H2.md` | `d47fa7934fcabd3e16646cbbae161315ee47d484ccec5f9c7ed5ec60d9728473` |
| `docs/specs/ISSP-Coordinator-Paired-Device-Registry.md` | `eee3ffd1dd34d8838c0a756b7c99ea49ba57bb5ad9a4c840abd1c0e51fa2bc59` |
| `docs/specs/Repository-Test-Execution-Policy.md` | `fb9d9e7cda8216adf1cda9c5cf1cc4ef94019c6e5e93614af20b3908ae9aac00` |
| `docs/specs/Firmware-Variants-Menuconfig.md` | `522a9024fb4347c5a3f7c20dc6f011c7644ee0732d38b7e1cc1746b7750c90a1` |
| `docs/adr/ADR-0001-ISSP-COMPONENT-BOUNDARIES.md` | `7131afc4fbdb610791cbd68afb1bdd4c17aac5ea8f2da6f7922a13b11ecc8083` |
| `docs/adr/ADR-0002-PRODUCT-BOARD-COMPOSITION.md` | `e7bf22ae720c3793375b32fb1056bfd74d55a7844a2b626bebfed7ac7512914c` |
| `docs/adr/ADR-0003-SUPPORTED-TARGETS-AND-TEST-EXECUTION.md` | `ac171aecbf571afa8f11dae49f16315315a6edff5d15ac38bffb87ebdc43c172` |
| `docs/adr/ADR-0004-CLIENT-GENERATED-REPORT-IDENTITY.md` | `92332a266e1dbff2e88311422a78d872c4e49b81144bc32e7293e60247bd5d57` |
| `docs/adr/ADR-0005-CAPABILITY-IDENTITY.md` | `923194e08e1d13b5605ba4190d7b36ab8e48ec5579c0066806cbb8d8a659729d` |
