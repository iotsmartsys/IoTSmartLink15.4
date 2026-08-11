# Componentes ISSP compartilhados

**Tipo:** Operacional
**Status:** Active
**Compatibilidade comprovada:** ESP-IDF 6.0.1, ESP32-H2

Este diretório contém os componentes reutilizáveis do runtime ISSP:

- `issp_core`: tipos, protocolo, abstrações de behavior e transporte,
  `IsspDevice` e fila lógica de reports; usa a seção crítica do FreeRTOS para
  proteger reports pendentes;
- `issp_transport_154`: transporte IEEE 802.15.4, commissioning, NVS e executor
  de reports; depende de `issp_core`, `ieee802154` e `nvs_flash`;
- `issp_behaviors`: behaviors reutilizáveis; atualmente expõe
  `DigitalOutputBehavior` e `DigitalInputBehavior` e depende de `issp_core`,
  `esp_driver_gpio` e `esp_timer`;
- `issp_app_154`: fachada pública `iotsmartsys::SmartSysApp`; compõe por
  delegação `issp_core`, `issp_behaviors` e `issp_transport_154`, além de
  possuir o factory reset local (`FactoryResetService`,
  `ResetButtonMonitor`, realocados de `client_154/main/reset`); seu único
  header público, `SmartSysApp.h`, não inclui nenhum header `issp_*` nem
  `reset/*` e não expõe identificadores `Issp` ou `154` em nomes públicos —
  o estado interno vive atrás de um ponteiro opaco (`Impl`). `Impl` é
  compartilhado por dois arquivos privados: `src/smart_sys_app.cpp` (a
  máquina de estados de `setup()`, independente de alvo) e
  `src/smart_sys_app_hardware.cpp` (os passos reais de NVS, rede, device e
  executor, compilado somente para `esp32h2`/`esp32c6`); testes automatizados
  substituem esses passos por fakes via `SmartSysApp::SetupHooks`.

## Consumo por uma aplicação ESP-IDF

Antes de incluir `project.cmake`, adicione o diretório compartilhado:

```cmake
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
```

No componente consumidor, declare apenas os nomes públicos necessários. Uma
aplicação de produto normalmente só precisa da fachada:

```cmake
idf_component_register(
    SRCS "main.cpp"
    REQUIRES issp_app_154
)
```

Um consumidor avançado pode continuar compondo os componentes técnicos
diretamente:

```cmake
idf_component_register(
    SRCS "main.cpp"
    REQUIRES issp_core issp_transport_154 issp_behaviors
)
```

As dependências CMake são:

| Componente | Públicas (`REQUIRES`) | Privadas (`PRIV_REQUIRES`) |
|---|---|---|
| `issp_core` | `freertos` | nenhuma |
| `issp_transport_154` | `ieee802154`, `issp_core`, `nvs_flash` | nenhuma |
| `issp_behaviors` | `issp_core`, `esp_driver_gpio`, `esp_timer` | nenhuma |
| `issp_app_154` | `esp_driver_gpio` | `issp_core`, `issp_behaviors` (todo alvo); `issp_transport_154`, `nvs_flash`, `esp_timer`, `esp_hw_support` (somente `esp32h2`/`esp32c6`) |

Em `issp_core`, `issp_transport_154` e `issp_behaviors`, as dependências
públicas listadas acima são públicas porque seus tipos ou headers aparecem
nos contratos em `include/`; não há dependências CMake exclusivamente
privadas nesses três componentes. Seus diretórios `src/` são privados. Em
`issp_transport_154`, `issp154_transport.h` continua público porque
`issp154_transport.hpp` o inclui e expõe tipos do ESP-IDF usados pelas
assinaturas privadas da classe; os consumidores diretos atuais são a própria
implementação C e o wrapper C++. Os headers de frame MAC e rádio permanecem
privados em `src/`.

`issp_app_154` é diferente: `esp_driver_gpio` é a única dependência pública,
porque `gpio_num_t` aparece nos tipos públicos `SwitchConfig` e
`PushButtonConfig` de `SmartSysApp.h` — o mesmo precedente já usado em
`issp_behaviors`. `issp_core` e `issp_behaviors` são privadas em todo alvo,
porque `SmartSysApp.h` não inclui nenhum header `issp_*` ou `driver/*` além
de `esp_driver_gpio`: o estado que os usa vive inteiramente em `Impl`,
compartilhado por `src/smart_sys_app_impl.hpp` (privado) e construído dentro
de um buffer opaco de tamanho fixo em `SmartSysApp`
(`SmartSysApp::kImplStorageBytes`) por placement-new — sem alocação
dinâmica e sem vazar nenhum tipo interno para `include/`.

`issp_transport_154`, `nvs_flash`, `esp_timer` e `esp_hw_support`, além dos
arquivos-fonte `src/smart_sys_app_hardware.cpp` e `src/reset/*.cpp`, entram
sempre no build: os únicos alvos admitidos, `esp32h2` e `esp32c6`, possuem
rádio IEEE 802.15.4. O `CMakeLists.txt` consulta `IDF_TARGET` via
`idf_build_get_property` para reprovar com `FATAL_ERROR` qualquer outro alvo,
não mais para condicionar fontes. `smart_sys_app_hardware.cpp` é o único
arquivo do componente que constrói o transporte, o network manager, o device, o
executor de reports e os serviços de reset reais; ele preenche
`Impl::hardwareStorage_` — um segundo buffer opaco, interno a `Impl` — com
um `HardwareState` que só `smart_sys_app_hardware.cpp` conhece, e é o único
lugar que define o construtor de produto de um argumento de `SmartSysApp`. Os
testes automatizados (seção seguinte) usam o construtor de dois argumentos
(`SmartSysApp::SetupHooks`): o código de hardware é linkado, mas nenhum caso o
alcança. `ieee802154` não é dependência, direta nem transitiva, do
build público; `esp_timer` é privada porque é usada apenas por
`reset_button_monitor.cpp`; `esp_hw_support` é privada porque é usada apenas
pela leitura interna do endereço IEEE. Os headers de `reset/` vivem em
`src/reset/` (`PRIV_INCLUDE_DIRS`), nunca em `include/`.

As APIs públicas, as restrições de contexto, concorrência e ciclo de vida estão
nos headers em `include/`. O exemplo `examples/issp_minimal_client` prova
compilação e link sem iniciar rádio, transmitir frames ou alterar NVS, tanto
pela composição direta quanto pela fachada `SmartSysApp`.

Testes automatizados de `issp_app_154` (configuração, estados, ordem de
inicialização, `setup()` repetido, falhas injetadas e rollback) vivem em
`test_apps/smart_sys_app_test` e usam exclusivamente
`SmartSysApp::SetupHooks` com fakes — nenhum toca NVS, GPIO real ou rádio.
QEMU não é mais um runner permitido. Esses testes podem executar host-native
quando os fakes preservarem integralmente a semântica material; o fallback
físico vigente é
`test_apps/smart_sys_app_test/pytest_smart_sys_app.py`, em ESP32-H2; ele exige
o resumo terminal dos 25 casos Unity. Sob TESTEXEC-009 esses casos permanecem
`Not Executed`: somente uma especificação futura pode solicitar sua coleta,
gravação ou execução. A validação de rádio, NVS, GPIO e factory reset
permanece em hardware real, conforme
`docs/specs/Repository-Test-Execution-Policy.md` e os critérios específicos da
especificação.

Limitações desta etapa: suporte e prova de hardware restritos a ESP-IDF
6.0.1, ESP32-H2 (`client_154`, `examples/issp_minimal_client`) e ESP32-C6
(`coordinator_154`); sem publicação em registry, Arduino, PlatformIO, outros
transports ou garantia para outros targets.

Comportamento e contratos normativos permanecem definidos em
`docs/specs/ISSP-Architecture.md`, `docs/specs/ISSP-Commissioning.md` e
`docs/specs/ISSP-Reusable-Components.md`.
