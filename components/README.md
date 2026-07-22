# Componentes ISSP compartilhados

**Tipo:** Operacional
**Status:** Active
**Compatibilidade comprovada:** ESP-IDF 6.0.1, ESP32-H2

Este diretório contém os componentes reutilizáveis do runtime ISSP:

- `issp_core`: tipos, protocolo, abstrações de behavior e transporte,
  `IsspDevice` e fila lógica de reports; não depende do ESP-IDF;
- `issp_transport_154`: transporte IEEE 802.15.4, commissioning, NVS e executor
  de reports; depende de `issp_core`, `ieee802154` e `nvs_flash`;
- `issp_behaviors`: behaviors reutilizáveis; atualmente expõe
  `DigitalOutputBehavior` e depende de `issp_core` e `esp_driver_gpio`.

## Consumo por uma aplicação ESP-IDF

Antes de incluir `project.cmake`, adicione o diretório compartilhado:

```cmake
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
```

No componente consumidor, declare apenas os nomes públicos necessários:

```cmake
idf_component_register(
    SRCS "main.cpp"
    REQUIRES issp_core issp_transport_154 issp_behaviors
)
```

As dependências CMake são:

| Componente | Públicas (`REQUIRES`) | Privadas (`PRIV_REQUIRES`) |
|---|---|---|
| `issp_core` | nenhuma | nenhuma |
| `issp_transport_154` | `ieee802154`, `issp_core`, `nvs_flash` | nenhuma |
| `issp_behaviors` | `issp_core`, `esp_driver_gpio` | nenhuma |

As dependências são públicas porque seus tipos ou headers aparecem nos
contratos em `include/`; não há dependências CMake exclusivamente privadas
nesta versão. Seus diretórios `src/` são privados. Em
`issp_transport_154`, `issp154_transport.h` continua público porque
`issp154_transport.hpp` o inclui e expõe tipos do ESP-IDF usados pelas
assinaturas privadas da classe; os consumidores diretos atuais são a própria
implementação C e o wrapper C++. Os headers de frame MAC e rádio permanecem
privados em `src/`.

As APIs públicas, as restrições de contexto, concorrência e ciclo de vida estão
nos headers em `include/`. O exemplo `examples/issp_minimal_client` prova
compilação e link sem iniciar rádio, transmitir frames ou alterar NVS.

Limitações desta etapa: suporte e prova restritos a ESP-IDF 6.0.1 e ESP32-H2;
sem publicação em registry, Arduino, PlatformIO, outros transports ou garantia
para outros targets.

Comportamento e contratos normativos permanecem definidos em
`docs/specs/ISSP-Architecture.md`, `docs/specs/ISSP-Commissioning.md` e
`docs/specs/ISSP-Reusable-Components.md`.
