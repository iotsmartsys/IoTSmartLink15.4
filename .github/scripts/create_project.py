#!/usr/bin/env python3
"""
Script to create a GitHub Kanban Project for the ISSP firmware client refactoring.

Usage:
    GH_TOKEN=<token> python3 .github/scripts/create_project.py \
        --owner iotsmartsys \
        --repo IoTSmartLink15.4

Requirements:
    pip install PyGithub requests
"""

import argparse
import os
import sys
import time

import requests

OWNER = "iotsmartsys"
REPO = "IoTSmartLink15.4"
PROJECT_TITLE = "Refatoração ISSP Firmware Client"

# ---------------------------------------------------------------------------
# Labels
# ---------------------------------------------------------------------------

LABELS = [
    {"name": "architecture", "color": "0075ca", "description": "Decisões e definições arquiteturais"},
    {"name": "refactor", "color": "e4e669", "description": "Refatoração de código existente"},
    {"name": "embedded", "color": "d93f0b", "description": "Código para hardware embarcado"},
    {"name": "esp-idf", "color": "b60205", "description": "ESP-IDF framework"},
    {"name": "cpp", "color": "1d76db", "description": "C++ migration or implementation"},
    {"name": "issp", "color": "0e8a16", "description": "ISSP protocol"},
    {"name": "transport", "color": "5319e7", "description": "Transport layer"},
    {"name": "behavior", "color": "fbca04", "description": "Device behavior"},
    {"name": "testing", "color": "006b75", "description": "Tests and validation"},
    {"name": "hardware-test", "color": "c5def5", "description": "Requires physical hardware to validate"},
]

# ---------------------------------------------------------------------------
# Issues
# ---------------------------------------------------------------------------

ISSUES = [
    # -----------------------------------------------------------------------
    # Issue 1 – Fase 1
    # -----------------------------------------------------------------------
    {
        "title": "[ISSP-01] Baseline funcional: documentar comportamento atual do firmware client",
        "labels": ["issp", "testing", "hardware-test"],
        "body": """\
## Contexto

O firmware client do protocolo ISSP está implementado em C, usando ESP-IDF, e já possui \
comunicação funcional via IEEE 802.15.4 com o coordenador. Antes de qualquer modificação \
estrutural, é necessário registrar formalmente o comportamento esperado, os logs observáveis \
e os critérios objetivos de regressão.

## Objetivo

Documentar o comportamento funcional atual do firmware client de forma que qualquer \
alteração estrutural futura possa ser validada sem ambiguidade.

## Escopo

- Registrar os logs esperados em cada fluxo de operação.
- Descrever o fluxo de associação (discovery + pareamento com coordenador).
- Descrever o fluxo de envio de report de estado do relé.
- Descrever o fluxo de recebimento e processamento de comandos.
- Registrar o estado antes e depois de cada tipo de comando.
- Criar checklist de regressão funcional reutilizável nas demais issues.
- Definir critérios objetivos para validar que nenhuma issue de refatoração \
alterou o protocolo.

## Fora de escopo

- Qualquer alteração no código.
- Testes automatizados.
- Mudanças em payloads, frames, IDs ou regras do protocolo.

## Proposta técnica

Criar o arquivo `client_154/BASELINE.md` com as seções:

1. **Logs esperados** — captura de log serial para cada fluxo.
2. **Fluxo de associação** — sequência de eventos desde o boot até o pareamento bem-sucedido.
3. **Fluxo de report** — sequência desde a mudança de estado do relé até o ACK do coordenador.
4. **Fluxo de comando** — sequência desde a recepção do frame IEEE 802.15.4 até a \
alteração do GPIO.
5. **Tabela de estados** — estado antes e depois de cada comando \
(`IOT154_VALUE_OFF`, `IOT154_VALUE_ON`, `IOT154_VALUE_TOGGLE`).
6. **Checklist de regressão** — lista de verificações reproduzíveis manualmente no hardware.
7. **Critérios de aceite da refatoração** — condições objetivas que devem ser verdadeiras \
ao final de cada fase.

## Critérios de aceite

- [ ] Arquivo `BASELINE.md` criado e revisado.
- [ ] Fluxo de associação documentado com logs reais ou esperados.
- [ ] Fluxo de report documentado com sequência de eventos.
- [ ] Fluxo de comando documentado para os três valores possíveis.
- [ ] Checklist de regressão com itens verificáveis no hardware.
- [ ] Critérios objetivos para todas as fases futuras definidos.

## Testes de compilação

Nenhum — issue documental.

## Testes no hardware

- Executar o firmware atual no hardware.
- Registrar logs reais para cada fluxo.
- Confirmar que os logs documentados correspondem ao comportamento observado.

## Riscos

- Logs reais podem diferir dos esperados se o hardware não estiver configurado corretamente.
- A ausência dessa documentação aumenta o risco de regressão silenciosa nas fases seguintes.

## Dependências

Nenhuma — esta é a primeira issue.

## Arquivos ou componentes afetados

- `client_154/BASELINE.md` (novo)

## Definição de pronto

- [ ] Arquivo `BASELINE.md` commitado no repositório.
- [ ] Revisado por pelo menos um colaborador com acesso ao hardware.
- [ ] Checklist de regressão utilizável a partir da Fase 2.
""",
    },
    # -----------------------------------------------------------------------
    # Issue 2 – Fase 2
    # -----------------------------------------------------------------------
    {
        "title": "[ISSP-02] Preparação para C++: compilar o firmware atual como C++",
        "labels": ["refactor", "cpp", "esp-idf", "embedded"],
        "body": """\
## Contexto

O firmware client está em C puro (`main.c`). A refatoração arquitetural exige C++ para \
a API pública e composição do firmware. O primeiro passo é garantir que o código atual \
compile como C++ sem nenhuma alteração funcional.

## Objetivo

Renomear `main.c` para `main.cpp`, corrigir apenas incompatibilidades de compilação C/C++ \
e garantir que o firmware se comporte de forma idêntica ao baseline documentado na Fase 1.

## Escopo

- Renomear `client_154/main/main.c` → `client_154/main/main.cpp`.
- Manter `extern "C" void app_main()` no arquivo renomeado.
- Adicionar guardas `extern "C"` nos headers C que forem necessários.
- Corrigir apenas erros de compilação introduzidos pela mudança de linguagem \
(casts implícitos, inicializadores designados incompatíveis, etc.).
- Atualizar `CMakeLists.txt` se necessário.

## Fora de escopo

- Qualquer refatoração lógica do código.
- Introdução de classes, templates ou qualquer construto C++.
- Alteração de payloads, frames, IDs ou regras do protocolo.

## Proposta técnica

1. Renomear o arquivo e atualizar o `CMakeLists.txt`.
2. Compilar com `idf.py build` e corrigir todos os erros de compilação C++ um a um.
3. Adicionar `#ifdef __cplusplus / extern "C" { ... } #endif` nos headers que forem \
incluídos por `main.cpp` e que não possuam essa guarda.
4. Verificar os headers: `iot154_packet.h`, `iot154_sensor_client.h`, `iot154_storage.h`.
5. Gravar o firmware no hardware e executar o checklist de regressão da Fase 1.

## Critérios de aceite

- [ ] `main.c` renomeado para `main.cpp`.
- [ ] Firmware compila sem erros ou warnings novos.
- [ ] `app_main` marcado com `extern "C"`.
- [ ] Todos os headers incluídos por `main.cpp` possuem guardas `extern "C"`.
- [ ] Checklist de regressão da Fase 1 executado com sucesso no hardware.
- [ ] Nenhuma alteração funcional introduzida.

## Testes de compilação

```bash
cd client_154
idf.py build
```

Esperado: build bem-sucedido, sem erros novos.

## Testes no hardware

- Flash e boot normal.
- Verificar fluxo de associação (BASELINE.md).
- Verificar fluxo de report (BASELINE.md).
- Verificar fluxo de comando (BASELINE.md).

## Riscos

- Headers C sem guardas `extern "C"` causam erros de linkagem difíceis de depurar.
- Inicializadores designados no estilo C99 podem não ser aceitos por todos os compiladores C++.

## Dependências

- #1 (ISSP-01) — baseline documentado.

## Arquivos ou componentes afetados

- `client_154/main/main.cpp` (renomeado de `main.c`)
- `client_154/main/CMakeLists.txt`
- Headers C que precisarem de guardas `extern "C"`

## Definição de pronto

- [ ] Build bem-sucedido com `idf.py build`.
- [ ] Checklist de regressão da Fase 1 executado com aprovação.
- [ ] PR revisado e mergeado.
""",
    },
    # -----------------------------------------------------------------------
    # Issue 3 – Fase 3
    # -----------------------------------------------------------------------
    {
        "title": "[ISSP-03] Separação de responsabilidades: identificar e isolar domínios no código atual",
        "labels": ["architecture", "refactor", "issp", "embedded"],
        "body": """\
## Contexto

O `main.cpp` atual mistura rádio IEEE 802.15.4, associação, envio, recepção, parsing, \
identidade, GPIO, debounce, lógica do relé e envio de reports em um único arquivo. \
Antes de criar novos componentes, é necessário mapear essas responsabilidades.

## Objetivo

Identificar e separar claramente os domínios de responsabilidade presentes no código atual, \
como preparação para a extração de componentes nas fases seguintes.

## Escopo

- Mapear cada função e variável global do `main.cpp` para um domínio.
- Identificar quais partes dependem diretamente do rádio IEEE 802.15.4.
- Identificar quais partes dependem de GPIO.
- Identificar quais partes são lógica específica do dispositivo (relé, botão).
- Identificar quais partes são protocolo genérico (report, comando, associação).
- Documentar as dependências entre domínios.
- Definir quais dependências são arquiteturalmente indesejáveis e devem ser removidas.

## Fora de escopo

- Qualquer movimentação ou refatoração de código.
- Criação de novos componentes ou arquivos.
- Alteração de payloads, frames, IDs ou regras do protocolo.

## Proposta técnica

Criar o arquivo `client_154/ARCHITECTURE.md` com as seções:

1. **Mapa de responsabilidades** — tabela função × domínio.
2. **Grafo de dependências** — quais domínios dependem de quais.
3. **Dependências indesejáveis** — o que deve ser desacoplado e por quê.
4. **Plano de extração** — ordem recomendada de extração para as fases seguintes.

Domínios a identificar:

| Domínio | Descrição |
|---------|-----------|
| radio | IEEE 802.15.4, init, send, receive, callbacks |
| association | Discovery, pareamento, persistência de endereço |
| protocol | Parsing, montagem de frames, seq, checksum |
| identity | Endereço MAC, device ID |
| commands | Recebimento e despacho de comandos |
| reports | Montagem e envio de reports de estado |
| gpio | Configuração e controle de GPIO |
| device-logic | Lógica específica do relé e botão |
| storage | Persistência de dados (NVS) |

## Critérios de aceite

- [ ] Arquivo `ARCHITECTURE.md` criado com mapa completo de responsabilidades.
- [ ] Todas as funções de `main.cpp` classificadas em pelo menos um domínio.
- [ ] Dependências indesejáveis listadas com justificativa.
- [ ] Plano de extração alinhado com as Fases 4–15.

## Testes de compilação

Nenhum — issue documental.

## Testes no hardware

Nenhum — issue documental.

## Riscos

- Mapeamento incorreto pode levar a extrações problemáticas nas fases seguintes.
- Dependências circulares não identificadas podem bloquear a refatoração.

## Dependências

- #2 (ISSP-02) — código compilando como C++.

## Arquivos ou componentes afetados

- `client_154/ARCHITECTURE.md` (novo)

## Definição de pronto

- [ ] Arquivo `ARCHITECTURE.md` commitado.
- [ ] Mapa de responsabilidades revisado por pelo menos um colaborador.
- [ ] Plano de extração aprovado antes do início da Fase 4.
""",
    },
    # -----------------------------------------------------------------------
    # Issue 4 – Fase 4
    # -----------------------------------------------------------------------
    {
        "title": "[ISSP-04] Componente issp_transport_154: extrair e encapsular o código IEEE 802.15.4",
        "labels": ["refactor", "transport", "esp-idf", "embedded", "issp"],
        "body": """\
## Contexto

Todo o código relacionado ao rádio IEEE 802.15.4 está espalhado entre \
`client_154/components/iot154/` e `client_154/main/radio/`. É necessário criar um \
componente dedicado que exponha uma API C interna estável e isole completamente \
o código de rádio do restante do firmware.

## Objetivo

Criar o componente `issp_transport_154`, mover o código C de rádio IEEE 802.15.4 para \
ele, definir uma API C interna estável e validar que associação, envio e recepção \
continuam funcionando.

## Escopo

- Criar a estrutura do componente em `components/issp_transport_154/`.
- Mover código relacionado ao IEEE 802.15.4 para o novo componente.
- Definir API C pública do componente (ver abaixo).
- Preservar a implementação atual sem alterações lógicas.
- Atualizar `CMakeLists.txt` para incluir o novo componente.
- Validar associação, envio e recepção no hardware.

## Fora de escopo

- Implementação de classes C++.
- Alteração de payloads, frames, IDs ou regras do protocolo.
- Criação da classe `Issp154Transport` (Fase 7).

## Proposta técnica

Estrutura do componente:

```
components/issp_transport_154/
├── CMakeLists.txt
├── include/
│   └── issp_transport_154.h
└── src/
    └── issp_transport_154.c
```

API C interna esperada:

```c
esp_err_t issp154_transport_init(const uint8_t *device_ext_addr);
esp_err_t issp154_transport_start(void);
esp_err_t issp154_transport_send(uint8_t endpoint_id,
                                 uint8_t event_type,
                                 uint8_t value,
                                 uint16_t seq,
                                 iot154_sensor_tx_result_t *result);
bool      issp154_transport_is_connected(void);
void      issp154_transport_set_receive_callback(iot154_sensor_command_cb_t cb);
bool      issp154_transport_discover_central(uint16_t seq,
                                             uint8_t *central_ext_addr);
void      issp154_transport_set_central_ext_addr(const uint8_t *central_ext_addr);
```

## Critérios de aceite

- [ ] Componente `issp_transport_154` criado com estrutura de diretórios correta.
- [ ] API C pública definida e documentada em `issp_transport_154.h`.
- [ ] Código de rádio movido sem alteração lógica.
- [ ] `main.cpp` atualizado para usar a nova API.
- [ ] Build bem-sucedido com `idf.py build`.
- [ ] Checklist de regressão da Fase 1 executado com aprovação no hardware.

## Testes de compilação

```bash
cd client_154
idf.py build
```

## Testes no hardware

- Verificar associação com coordenador.
- Verificar envio de report após mudança de estado.
- Verificar recebimento e processamento de comando remoto.

## Riscos

- Callbacks de recepção podem ter contexto de thread diferente após a extração.
- A API C pode precisar ser ajustada ao integrar com a camada C++ nas fases seguintes.

## Dependências

- #3 (ISSP-03) — mapa de responsabilidades definido.

## Arquivos ou componentes afetados

- `components/issp_transport_154/` (novo)
- `client_154/main/main.cpp`
- `client_154/main/CMakeLists.txt`
- `client_154/CMakeLists.txt`

## Definição de pronto

- [ ] Build bem-sucedido.
- [ ] Checklist de regressão da Fase 1 aprovado no hardware.
- [ ] PR revisado e mergeado.
""",
    },
    # -----------------------------------------------------------------------
    # Issue 5 – Fase 5
    # -----------------------------------------------------------------------
    {
        "title": "[ISSP-05] Componente issp_core: tipos comuns e contratos de protocolo",
        "labels": ["architecture", "cpp", "issp", "refactor"],
        "body": """\
## Contexto

A refatoração exige tipos comuns ao protocolo ISSP que sejam independentes de qualquer \
tecnologia de transporte (IEEE 802.15.4, Wi-Fi, MQTT). Esses tipos devem residir em um \
componente de núcleo que não dependa de nenhum driver ou HAL específico.

## Objetivo

Criar o componente `issp_core` com os tipos comuns do protocolo ISSP, garantindo total \
independência de tecnologia de comunicação, GPIO e drivers de hardware.

## Escopo

- Criar a estrutura do componente em `components/issp_core/`.
- Definir `IsspCommand` — comando recebido pelo dispositivo.
- Definir `IsspReport` — estado a ser publicado pelo dispositivo.
- Definir `IsspDeviceConfig` — configuração de identidade do dispositivo.
- Definir `IsspTransportState` — estado de conexão do transporte.
- Garantir que o componente não dependa de IEEE 802.15.4, Wi-Fi, MQTT ou GPIO.
- Garantir que o componente compile sozinho sem dependências externas além do ESP-IDF core.

## Fora de escopo

- Implementação de lógica de protocolo.
- Criação de interfaces C++ (Fases 6, 9).
- Qualquer driver de hardware.

## Proposta técnica

Estrutura do componente:

```
components/issp_core/
├── CMakeLists.txt
└── include/
    ├── issp_command.h
    ├── issp_report.h
    ├── issp_device_config.h
    └── issp_transport_state.h
```

Tipos esperados:

```cpp
// issp_command.h
struct IsspCommand {
    const char* capability;
    uint8_t     value;
};

// issp_report.h
struct IsspReport {
    const char* capability;
    uint8_t     value;
};

// issp_device_config.h
struct IsspDeviceConfig {
    const char* name;
};

// issp_transport_state.h
enum class IsspTransportState {
    Disconnected,
    Connecting,
    Connected,
};
```

## Critérios de aceite

- [ ] Componente `issp_core` criado com estrutura de diretórios correta.
- [ ] Tipos `IsspCommand`, `IsspReport`, `IsspDeviceConfig`, `IsspTransportState` definidos.
- [ ] Nenhuma dependência de IEEE 802.15.4, Wi-Fi, MQTT ou GPIO.
- [ ] Componente compila isoladamente.
- [ ] Build completo bem-sucedido com `idf.py build`.

## Testes de compilação

```bash
cd client_154
idf.py build
```

## Testes no hardware

Nenhum — issue de infraestrutura de tipos. Regressão validada pelo build.

## Riscos

- Tipos muito específicos do protocolo atual podem dificultar suporte a ISSP11 (Fase 16).
- Dependências transitivas podem introduzir acoplamentos indesejados.

## Dependências

- #4 (ISSP-04) — componente de transporte criado (facilita entender quais tipos abstrair).

## Arquivos ou componentes afetados

- `components/issp_core/` (novo)
- `client_154/CMakeLists.txt`

## Definição de pronto

- [ ] Build bem-sucedido.
- [ ] Tipos revisados e aprovados.
- [ ] PR mergeado.
""",
    },
    # -----------------------------------------------------------------------
    # Issue 6 – Fase 6
    # -----------------------------------------------------------------------
    {
        "title": "[ISSP-06] Interface IIsspTransport: contrato abstrato de transporte",
        "labels": ["architecture", "cpp", "transport", "issp"],
        "body": """\
## Contexto

Para que os behaviors e o `IsspDevice` sejam agnósticos ao transporte, é necessária uma \
interface abstrata que defina o contrato de comunicação sem mencionar IEEE 802.15.4, Wi-Fi \
ou qualquer outro protocolo de rede.

## Objetivo

Definir a interface pura C++ `IIsspTransport` no componente `issp_core`, garantindo que ela \
não dependa de nenhuma tecnologia de transporte específica e que suporte a adição futura \
do `Issp11Transport`.

## Escopo

- Definir `IIsspTransport` em `components/issp_core/include/issp_transport.h`.
- Incluir métodos: `begin()`, `send()`, `isConnected()`, `setReceiveHandler()`.
- Garantir ausência de dependências de Wi-Fi, IEEE 802.15.4, GPIO ou MQTT.
- Documentar o contrato de cada método (pré-condições, pós-condições, thread safety).
- Compilar sem erros.

## Fora de escopo

- Implementação concreta (`Issp154Transport`) — Fase 7.
- Implementação de `Issp11Transport` — Fase 16.

## Proposta técnica

```cpp
// components/issp_core/include/issp_transport.h
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "issp_command.h"
#include "issp_report.h"

using IsspReceiveHandler = void (*)(const IsspCommand& cmd, void* context);

class IIsspTransport
{
public:
    virtual esp_err_t begin() = 0;
    virtual esp_err_t send(const IsspReport& report) = 0;
    virtual bool      isConnected() const = 0;
    virtual void      setReceiveHandler(IsspReceiveHandler handler,
                                        void* context) = 0;
    virtual          ~IIsspTransport() = default;
};
```

## Critérios de aceite

- [ ] Interface `IIsspTransport` definida em `issp_core`.
- [ ] Sem dependência de Wi-Fi, IEEE 802.15.4, GPIO ou MQTT.
- [ ] Sem uso de exceções ou RTTI.
- [ ] Documentação inline de cada método.
- [ ] Build bem-sucedido.

## Testes de compilação

```bash
cd client_154
idf.py build
```

## Testes no hardware

Nenhum — issue de definição de interface.

## Riscos

- Interface muito acoplada ao protocolo atual pode dificultar a adição do ISSP11.
- Assinatura do handler de recepção deve ser compatível com o callback C existente.

## Dependências

- #5 (ISSP-05) — tipos `IsspCommand` e `IsspReport` definidos.

## Arquivos ou componentes afetados

- `components/issp_core/include/issp_transport.h` (novo)

## Definição de pronto

- [ ] Build bem-sucedido.
- [ ] Interface revisada e aprovada antes da Fase 7.
- [ ] PR mergeado.
""",
    },
    # -----------------------------------------------------------------------
    # Issue 7 – Fase 7
    # -----------------------------------------------------------------------
    {
        "title": "[ISSP-07] Issp154Transport: adaptador C++ para o transporte IEEE 802.15.4",
        "labels": ["cpp", "transport", "issp", "refactor", "embedded"],
        "body": """\
## Contexto

Com a interface `IIsspTransport` definida e o componente `issp_transport_154` extraído, \
é necessário criar a classe C++ `Issp154Transport` que implementa a interface e delega \
para a API C do `issp_transport_154`.

## Objetivo

Implementar `Issp154Transport` como adaptador entre a interface `IIsspTransport` e a API \
C do componente `issp_transport_154`, sem duplicar lógica de protocolo.

## Escopo

- Criar `Issp154Transport` em `components/issp_transport_154/`.
- Implementar todos os métodos de `IIsspTransport`.
- Criar ponte de callback C → método C++ usando `void* context`.
- Manter construtor leve (apenas armazena configuração).
- Realizar inicialização de hardware em `begin()`.
- Validar no hardware.

## Fora de escopo

- Alteração da API C do `issp_transport_154`.
- Qualquer lógica de protocolo nova.
- Alteração de payloads, frames, IDs ou regras do protocolo.

## Proposta técnica

```cpp
// components/issp_transport_154/include/Issp154Transport.h
#pragma once
#include "issp_transport.h"

struct Issp154TransportConfig {
    uint8_t channel;
};

class Issp154Transport : public IIsspTransport
{
public:
    explicit Issp154Transport(const Issp154TransportConfig& config);
    esp_err_t begin() override;
    esp_err_t send(const IsspReport& report) override;
    bool      isConnected() const override;
    void      setReceiveHandler(IsspReceiveHandler handler,
                                void* context) override;

private:
    Issp154TransportConfig m_config;
    IsspReceiveHandler     m_handler{nullptr};
    void*                  m_context{nullptr};

    static bool s_command_bridge(uint8_t endpoint_id,
                                 uint8_t event_type,
                                 uint8_t value);
    static Issp154Transport* s_instance;
};
```

Ponte de callback:

```cpp
bool Issp154Transport::s_command_bridge(uint8_t endpoint_id,
                                        uint8_t event_type,
                                        uint8_t value)
{
    if (!s_instance || !s_instance->m_handler) return false;
    IsspCommand cmd{/* mapear endpoint_id/event_type/value para capability+value */};
    s_instance->m_handler(cmd, s_instance->m_context);
    return true;
}
```

## Critérios de aceite

- [ ] `Issp154Transport` implementa todos os métodos de `IIsspTransport`.
- [ ] Construtor não inicializa hardware.
- [ ] `begin()` chama `issp154_transport_init()` e `issp154_transport_start()`.
- [ ] Ponte de callback C → C++ funciona corretamente.
- [ ] Build bem-sucedido.
- [ ] Checklist de regressão da Fase 1 aprovado no hardware.

## Testes de compilação

```bash
cd client_154
idf.py build
```

## Testes no hardware

- Verificar associação após refatoração.
- Verificar envio de report.
- Verificar recebimento de comando.

## Riscos

- Singleton estático `s_instance` limita a um transport por processo; aceitável nesta fase.
- Thread safety do handler deve ser documentado.

## Dependências

- #6 (ISSP-06) — interface `IIsspTransport` definida.
- #4 (ISSP-04) — API C do `issp_transport_154` estável.

## Arquivos ou componentes afetados

- `components/issp_transport_154/include/Issp154Transport.h` (novo)
- `components/issp_transport_154/src/Issp154Transport.cpp` (novo)
- `components/issp_transport_154/CMakeLists.txt`

## Definição de pronto

- [ ] Build bem-sucedido.
- [ ] Checklist de regressão da Fase 1 aprovado no hardware.
- [ ] PR mergeado.
""",
    },
    # -----------------------------------------------------------------------
    # Issue 8 – Fase 8
    # -----------------------------------------------------------------------
    {
        "title": "[ISSP-08] IsspDevice: classe central de runtime do dispositivo",
        "labels": ["architecture", "cpp", "issp", "refactor"],
        "body": """\
## Contexto

Com o transporte encapsulado, é necessário criar a classe `IsspDevice` que representa o \
dispositivo ISSP, gerencia o ciclo de vida do protocolo, despacha comandos e envia reports, \
sem conhecer GPIO ou hardware específico.

## Objetivo

Implementar a classe `IsspDevice` no componente `issp_core`, que recebe configuração e \
transporte, inicia o protocolo, recebe comandos, envia reports e controla o estado de conexão.

## Escopo

- Criar `IsspDevice` em `components/issp_core/`.
- Receber `IsspDeviceConfig` e `IIsspTransport&` no construtor.
- Implementar `start()` que inicializa o transporte e registra o handler de comandos.
- Implementar `sendReport(const IsspReport&)`.
- Expor estado de conexão via `isConnected()`.
- Manter independência de GPIO e hardware específico.
- `main.cpp` deve usar `IsspDevice` para iniciar o protocolo.
- Validar no hardware.

## Fora de escopo

- Sistema de behaviors (Fase 9–10).
- Qualquer lógica de GPIO ou debounce.
- Alteração de payloads, frames, IDs ou regras do protocolo.

## Proposta técnica

```cpp
// components/issp_core/include/IsspDevice.h
#pragma once
#include "issp_device_config.h"
#include "issp_transport.h"
#include "issp_command.h"
#include "issp_report.h"

class IsspDevice
{
public:
    IsspDevice(const IsspDeviceConfig& config, IIsspTransport& transport);
    esp_err_t  start();
    esp_err_t  sendReport(const IsspReport& report);
    bool       isConnected() const;

private:
    IsspDeviceConfig  m_config;
    IIsspTransport&   m_transport;

    static void s_receive_handler(const IsspCommand& cmd, void* context);
    void        onCommandReceived(const IsspCommand& cmd);
};
```

## Critérios de aceite

- [ ] `IsspDevice` criado em `issp_core`.
- [ ] Construtor recebe `IsspDeviceConfig` e `IIsspTransport&`.
- [ ] `start()` inicializa o transporte.
- [ ] `sendReport()` delega para o transporte.
- [ ] Sem dependência de GPIO, drivers ou hardware específico.
- [ ] Build bem-sucedido.
- [ ] Checklist de regressão da Fase 1 aprovado no hardware.

## Testes de compilação

```bash
cd client_154
idf.py build
```

## Testes no hardware

- Boot, associação, report e comando funcionando após integração de `IsspDevice`.

## Riscos

- A interface de `sendReport` deve ser compatível com o sistema de behaviors da Fase 10.
- Thread safety entre o handler de comandos e o loop principal.

## Dependências

- #7 (ISSP-07) — `Issp154Transport` implementado.
- #5 (ISSP-05) — tipos de `issp_core` definidos.

## Arquivos ou componentes afetados

- `components/issp_core/include/IsspDevice.h` (novo)
- `components/issp_core/src/IsspDevice.cpp` (novo)
- `client_154/main/main.cpp`

## Definição de pronto

- [ ] Build bem-sucedido.
- [ ] Checklist de regressão da Fase 1 aprovado.
- [ ] PR mergeado.
""",
    },
    # -----------------------------------------------------------------------
    # Issue 9 – Fase 9
    # -----------------------------------------------------------------------
    {
        "title": "[ISSP-09] Interface IDeviceBehavior: contrato mínimo de comportamento",
        "labels": ["architecture", "cpp", "behavior", "issp"],
        "body": """\
## Contexto

O sistema de behaviors precisa de uma interface abstrata que defina como um behavior \
recebe comandos, publica estado e é inicializado, sem acoplamento ao transporte ou ao \
hardware específico.

## Objetivo

Definir a interface `IDeviceBehavior` em `issp_core`, especificar como behaviors publicam \
estado, como recebem acesso ao runtime, como o runtime limita a quantidade máxima de \
behaviors e como evitar alocação dinâmica.

## Escopo

- Definir `IDeviceBehavior` em `components/issp_core/include/issp_behavior.h`.
- Incluir métodos: `begin()`, `accepts()`, `handle()`, `poll()`.
- Especificar o mecanismo de publicação de estado (callback ou referência ao `IsspDevice`).
- Documentar como o runtime acessa os behaviors.
- Definir `ISSP_MAX_BEHAVIORS` como constante de compilação.
- Compilar sem erros.

## Fora de escopo

- Implementação de behaviors concretos (Fases 11–13).
- Sistema de registro e dispatch (Fase 10).

## Proposta técnica

```cpp
// components/issp_core/include/issp_behavior.h
#pragma once
#include "issp_command.h"
#include "esp_err.h"

class IsspDevice;  // forward declaration

class IDeviceBehavior
{
public:
    virtual esp_err_t begin() = 0;
    virtual bool      accepts(const IsspCommand& cmd) const = 0;
    virtual esp_err_t handle(const IsspCommand& cmd) = 0;
    virtual void      poll() {}
    virtual          ~IDeviceBehavior() = default;

    void setRuntime(IsspDevice* device) { m_device = device; }

protected:
    IsspDevice* m_device{nullptr};
};

#ifndef ISSP_MAX_BEHAVIORS
#define ISSP_MAX_BEHAVIORS 8
#endif
```

## Critérios de aceite

- [ ] Interface `IDeviceBehavior` definida em `issp_core`.
- [ ] Métodos `begin()`, `accepts()`, `handle()`, `poll()` presentes.
- [ ] `ISSP_MAX_BEHAVIORS` definida como constante de compilação ajustável.
- [ ] Mecanismo de acesso ao runtime (`setRuntime`) documentado.
- [ ] Sem uso de `new`, `delete` ou alocação dinâmica.
- [ ] Build bem-sucedido.

## Testes de compilação

```bash
cd client_154
idf.py build
```

## Testes no hardware

Nenhum — issue de definição de interface.

## Riscos

- A interface de publicação de estado deve permitir que behaviors chamem \
`m_device->sendReport()` de forma segura.
- `ISSP_MAX_BEHAVIORS` muito pequeno pode causar falhas silenciosas ao adicionar behaviors.

## Dependências

- #8 (ISSP-08) — `IsspDevice` definido (necessário para forward declaration).

## Arquivos ou componentes afetados

- `components/issp_core/include/issp_behavior.h` (novo)

## Definição de pronto

- [ ] Build bem-sucedido.
- [ ] Interface revisada e aprovada antes da Fase 10.
- [ ] PR mergeado.
""",
    },
    # -----------------------------------------------------------------------
    # Issue 10 – Fase 10
    # -----------------------------------------------------------------------
    {
        "title": "[ISSP-10] Registro e dispatch de behaviors em IsspDevice",
        "labels": ["architecture", "cpp", "behavior", "issp", "refactor"],
        "body": """\
## Contexto

Com a interface `IDeviceBehavior` definida, o `IsspDevice` precisa ser capaz de registrar \
behaviors, inicializá-los no `start()` e despachar comandos recebidos para o behavior \
correto sem grandes blocos `if`/`switch`.

## Objetivo

Adicionar à classe `IsspDevice` o método `addBehavior()`, armazenar behaviors em array \
estático, inicializar todos no `start()`, encaminhar comandos por capability e retornar \
erro explícito quando nenhum behavior aceitar o comando.

## Escopo

- Adicionar `addBehavior(IDeviceBehavior& behavior)` à `IsspDevice`.
- Armazenar behaviors em `IDeviceBehavior* m_behaviors[ISSP_MAX_BEHAVIORS]`.
- Inicializar todos em `start()` chamando `begin()` em cada um.
- No handler de comandos, iterar pelos behaviors e chamar `handle()` no primeiro \
que `accepts()` o comando.
- Retornar `ESP_ERR_NOT_FOUND` quando nenhum behavior aceitar.
- Chamar `poll()` em todos os behaviors no loop principal.
- Evitar `if`/`switch` baseados em capability strings na classe `IsspDevice`.

## Fora de escopo

- Implementação de behaviors concretos (Fases 11–13).
- Relações declarativas entre behaviors (Fase 14).

## Proposta técnica

```cpp
class IsspDevice
{
public:
    // ... métodos existentes ...
    esp_err_t addBehavior(IDeviceBehavior& behavior);
    void      pollBehaviors();

private:
    IDeviceBehavior* m_behaviors[ISSP_MAX_BEHAVIORS]{};
    size_t           m_behavior_count{0};

    void dispatchCommand(const IsspCommand& cmd);
};
```

Loop de dispatch:

```cpp
void IsspDevice::dispatchCommand(const IsspCommand& cmd)
{
    for (size_t i = 0; i < m_behavior_count; ++i) {
        if (m_behaviors[i]->accepts(cmd)) {
            m_behaviors[i]->handle(cmd);
            return;
        }
    }
    ESP_LOGW(TAG, "No behavior accepted command for capability '%s'", cmd.capability);
}
```

## Critérios de aceite

- [ ] `addBehavior()` retorna erro quando `ISSP_MAX_BEHAVIORS` é excedido.
- [ ] Todos os behaviors são inicializados em `start()`.
- [ ] Dispatch sem `if`/`switch` baseado em capability na `IsspDevice`.
- [ ] Erro explícito quando nenhum behavior aceita o comando.
- [ ] `pollBehaviors()` chamado no loop principal.
- [ ] Build bem-sucedido.
- [ ] Checklist de regressão da Fase 1 aprovado (com behavior placeholder se necessário).

## Testes de compilação

```bash
cd client_154
idf.py build
```

## Testes no hardware

- Validar que o dispatch ainda funciona com o behavior de relé atual (placeholder ou migrado).

## Riscos

- `ISSP_MAX_BEHAVIORS` deve ser suficiente para o uso real.
- `poll()` deve ser thread-safe se chamado de task diferente.

## Dependências

- #9 (ISSP-09) — interface `IDeviceBehavior` definida.

## Arquivos ou componentes afetados

- `components/issp_core/src/IsspDevice.cpp`
- `components/issp_core/include/IsspDevice.h`

## Definição de pronto

- [ ] Build bem-sucedido.
- [ ] Checklist de regressão da Fase 1 aprovado.
- [ ] PR mergeado.
""",
    },
    # -----------------------------------------------------------------------
    # Issue 11 – Fase 11
    # -----------------------------------------------------------------------
    {
        "title": "[ISSP-11] DigitalOutputBehavior: primeiro behavior de saída digital configurável",
        "labels": ["cpp", "behavior", "embedded", "esp-idf", "hardware-test"],
        "body": """\
## Contexto

Com o sistema de behaviors funcionando, é hora de implementar o primeiro behavior real que \
substitui a lógica atual do relé: `DigitalOutputBehavior`, configurável e reutilizável para \
qualquer saída digital.

## Objetivo

Implementar `DigitalOutputBehavior` no componente `issp_behaviors`, configurando GPIO, \
aplicando estado inicial, processando comandos, mantendo estado lógico e publicando reports. \
Substituir a lógica específica do relé no `main.cpp`.

## Escopo

- Criar o componente `issp_behaviors`.
- Implementar `DigitalOutputBehavior` com a configuração abaixo.
- Configurar GPIO em `begin()`.
- Processar comandos `ON`, `OFF`, `TOGGLE`.
- Manter estado lógico interno.
- Publicar report via `m_device->sendReport()` após mudança de estado.
- Substituir as funções `relay_configure`, `relay_set`, `relay_mark_state_change` do `main.cpp`.

## Fora de escopo

- `DigitalInputBehavior` (Fase 12).
- Wrappers semânticos como `RelayBehavior` (Fase 13).
- Relações declarativas (Fase 14).

## Proposta técnica

```cpp
struct DigitalOutputConfig {
    const char* capability;
    gpio_num_t  gpio;
    uint32_t    activeLevel;
    bool        initialState;
    bool        reportOnStart;
};

class DigitalOutputBehavior : public IDeviceBehavior
{
public:
    explicit DigitalOutputBehavior(const DigitalOutputConfig& config);
    esp_err_t begin() override;
    bool      accepts(const IsspCommand& cmd) const override;
    esp_err_t handle(const IsspCommand& cmd) override;

private:
    DigitalOutputConfig m_config;
    bool                m_state{false};

    void applyState(bool on);
    void publishReport();
};
```

## Critérios de aceite

- [ ] Componente `issp_behaviors` criado.
- [ ] `DigitalOutputBehavior` implementado com `DigitalOutputConfig`.
- [ ] GPIO configurado em `begin()`, não no construtor.
- [ ] Comandos `ON`, `OFF`, `TOGGLE` processados corretamente.
- [ ] State lógico mantido internamente.
- [ ] Report publicado via `sendReport()` após cada mudança.
- [ ] `reportOnStart` funcional.
- [ ] Lógica do relé removida do `main.cpp`.
- [ ] Build bem-sucedido.
- [ ] Checklist de regressão da Fase 1 aprovado no hardware.

## Testes de compilação

```bash
cd client_154
idf.py build
```

## Testes no hardware

- Comando remoto ON → relé liga, report enviado.
- Comando remoto OFF → relé desliga, report enviado.
- Comando remoto TOGGLE → relé alterna, report enviado.
- Estado inicial aplicado no boot.
- `reportOnStart` envia report na inicialização.

## Riscos

- `activeLevel` deve ser considerado ao mapear estado lógico para nível GPIO.
- Report na inicialização pode ocorrer antes da associação; tratar falha graciosamente.

## Dependências

- #10 (ISSP-10) — sistema de dispatch de behaviors funcional.

## Arquivos ou componentes afetados

- `components/issp_behaviors/` (novo)
- `components/issp_behaviors/include/DigitalOutputBehavior.h` (novo)
- `components/issp_behaviors/src/DigitalOutputBehavior.cpp` (novo)
- `client_154/main/main.cpp`

## Definição de pronto

- [ ] Build bem-sucedido.
- [ ] Checklist de regressão da Fase 1 aprovado no hardware.
- [ ] PR mergeado.
""",
    },
    # -----------------------------------------------------------------------
    # Issue 12 – Fase 12
    # -----------------------------------------------------------------------
    {
        "title": "[ISSP-12] DigitalInputBehavior: entrada digital configurável com debounce",
        "labels": ["cpp", "behavior", "embedded", "esp-idf", "hardware-test"],
        "body": """\
## Contexto

O firmware atual possui lógica de leitura de botão com debounce por polling em `main.cpp`. \
É necessário extrair essa lógica para um behavior reutilizável que abstrai o GPIO de entrada.

## Objetivo

Implementar `DigitalInputBehavior` com suporte a capability, GPIO, pull mode, active level, \
debounce e report on change, substituindo a lógica de botão do `main.cpp`.

## Escopo

- Implementar `DigitalInputBehavior` no componente `issp_behaviors`.
- Parâmetros: `capability`, `gpio`, `pullMode`, `activeLevel`, `debounceMs`, `reportOnChange`.
- Debounce por polling (fora de ISR) usando `xTaskGetTickCount`.
- Report publicado quando estado estabilizado muda.
- Substituir as funções `relay_button_configure`, `relay_button_poll`, \
`relay_button_is_pressed` do `main.cpp`.

## Fora de escopo

- Lógica de ação sobre outros behaviors (Fase 14).
- `ButtonBehavior` wrapper (Fase 13).

## Proposta técnica

```cpp
struct DigitalInputConfig {
    const char*          capability;
    gpio_num_t           gpio;
    gpio_pull_mode_t     pullMode;
    uint32_t             activeLevel;
    uint32_t             debounceMs;
    bool                 reportOnChange;
};

class DigitalInputBehavior : public IDeviceBehavior
{
public:
    explicit DigitalInputBehavior(const DigitalInputConfig& config);
    esp_err_t begin() override;
    bool      accepts(const IsspCommand& cmd) const override;
    esp_err_t handle(const IsspCommand& cmd) override;
    void      poll() override;

private:
    DigitalInputConfig m_config;
    bool               m_stableState{false};
    bool               m_lastSample{false};
    TickType_t         m_lastChangeTick{0};

    bool readRaw() const;
    void publishReport(bool state);
};
```

## Critérios de aceite

- [ ] `DigitalInputBehavior` implementado com todos os parâmetros de configuração.
- [ ] GPIO configurado em `begin()`, não no construtor.
- [ ] Debounce por polling funcional.
- [ ] Report publicado apenas quando estado muda (se `reportOnChange` = true).
- [ ] Estado inicial lido em `begin()`.
- [ ] Lógica de botão removida do `main.cpp`.
- [ ] Build bem-sucedido.
- [ ] Checklist de regressão da Fase 1 aprovado no hardware.

## Testes de compilação

```bash
cd client_154
idf.py build
```

## Testes no hardware

- Pressionar botão → estado muda, report enviado (se conectado).
- Soltar botão → estado muda novamente.
- Bouncing elétrico filtrado corretamente.
- Acionamento rápido (< debounce) não gera transição.
- Estado inicial lido corretamente no boot.
- GPIO flutuante não gera transições espúrias (pull mode correto).
- Mudança durante desconexão → report pendente enviado após reconexão.

## Riscos

- Bouncing pode variar entre diferentes modelos de botão.
- `debounceMs` deve ser ajustável sem recompilação (via `DigitalInputConfig`).

## Dependências

- #11 (ISSP-11) — `DigitalOutputBehavior` implementado (padrão de behavior estabelecido).

## Arquivos ou componentes afetados

- `components/issp_behaviors/include/DigitalInputBehavior.h` (novo)
- `components/issp_behaviors/src/DigitalInputBehavior.cpp` (novo)
- `client_154/main/main.cpp`

## Definição de pronto

- [ ] Build bem-sucedido.
- [ ] Checklist de regressão da Fase 1 aprovado no hardware.
- [ ] PR mergeado.
""",
    },
    # -----------------------------------------------------------------------
    # Issue 13 – Fase 13
    # -----------------------------------------------------------------------
    {
        "title": "[ISSP-13] Behaviors semânticos: RelayBehavior, LedBehavior, ButtonBehavior",
        "labels": ["cpp", "behavior", "refactor", "embedded"],
        "body": """\
## Contexto

Com `DigitalOutputBehavior` e `DigitalInputBehavior` implementados, é possível criar \
wrappers semânticos que expressam a intenção do dispositivo sem duplicar lógica elétrica.

## Objetivo

Implementar `RelayBehavior`, `LedBehavior` e `ButtonBehavior` como wrappers finos sobre \
`DigitalOutputBehavior` e `DigitalInputBehavior`, sem duplicar lógica de GPIO ou debounce.

## Escopo

- Implementar `RelayBehavior` — saída com active-high, sem debounce.
- Implementar `LedBehavior` — saída com active-low configurável, sem debounce.
- Implementar `ButtonBehavior` — entrada com pull-up, debounce e report on change.
- Cada wrapper deve apenas construir o config correto e delegar para o behavior base.
- Atualizar `main.cpp` para usar os wrappers semânticos.

## Fora de escopo

- Relações declarativas entre behaviors (Fase 14).
- Qualquer lógica elétrica que já existe em `DigitalOutputBehavior`/`DigitalInputBehavior`.

## Proposta técnica

```cpp
// RelayBehavior: wrapper sobre DigitalOutputBehavior
struct RelayConfig {
    const char* capability;
    gpio_num_t  gpio;
    uint32_t    activeLevel;
    bool        initialState;
};

class RelayBehavior : public DigitalOutputBehavior
{
public:
    explicit RelayBehavior(const RelayConfig& config);
};

// LedBehavior: wrapper sobre DigitalOutputBehavior
struct LedConfig {
    const char* capability;
    gpio_num_t  gpio;
    uint32_t    activeLevel;
    bool        initialState;
};

class LedBehavior : public DigitalOutputBehavior
{
public:
    explicit LedBehavior(const LedConfig& config);
};

// ButtonBehavior: wrapper sobre DigitalInputBehavior
struct ButtonConfig {
    const char* capability;
    gpio_num_t  gpio;
    uint32_t    debounceMs;
};

class ButtonBehavior : public DigitalInputBehavior
{
public:
    explicit ButtonBehavior(const ButtonConfig& config);
};
```

## Critérios de aceite

- [ ] `RelayBehavior`, `LedBehavior`, `ButtonBehavior` implementados.
- [ ] Nenhuma duplicação de lógica elétrica (GPIO, debounce).
- [ ] `main.cpp` usa wrappers semânticos.
- [ ] Build bem-sucedido.
- [ ] Checklist de regressão da Fase 1 aprovado no hardware.

## Testes de compilação

```bash
cd client_154
idf.py build
```

## Testes no hardware

- Mesmos testes das Fases 11 e 12, agora via wrappers semânticos.

## Riscos

- Herança dos wrappers pode criar problemas se `DigitalOutputBehavior` tiver métodos \
finais — usar composição se necessário.

## Dependências

- #12 (ISSP-12) — `DigitalInputBehavior` implementado.

## Arquivos ou componentes afetados

- `components/issp_behaviors/include/RelayBehavior.h` (novo)
- `components/issp_behaviors/include/LedBehavior.h` (novo)
- `components/issp_behaviors/include/ButtonBehavior.h` (novo)
- `components/issp_behaviors/src/RelayBehavior.cpp` (novo)
- `components/issp_behaviors/src/LedBehavior.cpp` (novo)
- `components/issp_behaviors/src/ButtonBehavior.cpp` (novo)
- `client_154/main/main.cpp`

## Definição de pronto

- [ ] Build bem-sucedido.
- [ ] Checklist de regressão da Fase 1 aprovado no hardware.
- [ ] PR mergeado.
""",
    },
    # -----------------------------------------------------------------------
    # Issue 14 – Fase 14
    # -----------------------------------------------------------------------
    {
        "title": "[ISSP-14] Relações declarativas entre behaviors via IsspDevice",
        "labels": ["architecture", "cpp", "behavior", "issp"],
        "body": """\
## Contexto

O firmware atual vincula o botão ao relé diretamente no `main.cpp` através de chamada \
direta a `relay_set()`. Na arquitetura modular, o botão não pode conhecer o relé; a \
resolução deve ocorrer pelo runtime.

## Objetivo

Permitir configurações declarativas como `ToggleCapability{"power"}` no `ButtonConfig`, \
onde o runtime (`IsspDevice`) resolve a ação e encaminha para o behavior correto sem \
que o botão conheça o relé.

## Escopo

- Definir o tipo `ToggleCapability` (e possivelmente `SetCapability`) em `issp_core`.
- Adicionar suporte a `action` no `ButtonConfig`.
- Implementar no runtime a resolução da ação: encontrar o behavior alvo pela capability \
e encaminhar o comando correspondente.
- Garantir que a alteração atualize hardware e estado.
- Garantir que report seja publicado após a alteração.
- O botão não deve ter referência direta ao relé.

## Fora de escopo

- Simplificação final do `main.cpp` (Fase 15).
- Qualquer lógica de ação não descrita aqui.

## Proposta técnica

```cpp
// issp_core: tipos de ação
struct ToggleCapability { const char* capability; };
struct SetCapability    { const char* capability; uint8_t value; };

using BehaviorAction = /* variant ou union simples */;

// ButtonConfig atualizado
struct ButtonConfig {
    const char*    capability;
    gpio_num_t     gpio;
    uint32_t       debounceMs;
    BehaviorAction action;
};
```

Resolução no runtime:

```cpp
// IsspDevice::onButtonEvent(const char* sourceCapability, bool pressed)
// → encontrar behavior alvo pelo campo action
// → construir IsspCommand e chamar dispatchCommand()
```

## Critérios de aceite

- [ ] `ToggleCapability` definido em `issp_core`.
- [ ] `ButtonConfig` suporta campo `action`.
- [ ] Runtime resolve a ação sem acoplamento direto entre behaviors.
- [ ] Alteração atualiza GPIO, estado lógico e envia report.
- [ ] Build bem-sucedido.
- [ ] Checklist de regressão da Fase 1 aprovado no hardware.

## Testes de compilação

```bash
cd client_154
idf.py build
```

## Testes no hardware

- Pressionar botão → relé alterna → report enviado (fluxo idêntico ao atual).
- Verificar que o relé responde a comandos remotos independentemente do botão.

## Riscos

- Uso de `union` para `BehaviorAction` pode ser frágil; considerar tipo discriminado simples.
- Sem RTTI/exceptions, o dispatch deve ser totalmente baseado em dados.

## Dependências

- #13 (ISSP-13) — wrappers semânticos implementados.

## Arquivos ou componentes afetados

- `components/issp_core/include/issp_action.h` (novo)
- `components/issp_behaviors/include/ButtonBehavior.h`
- `components/issp_core/src/IsspDevice.cpp`

## Definição de pronto

- [ ] Build bem-sucedido.
- [ ] Checklist de regressão da Fase 1 aprovado no hardware.
- [ ] PR mergeado.
""",
    },
    # -----------------------------------------------------------------------
    # Issue 15 – Fase 15
    # -----------------------------------------------------------------------
    {
        "title": "[ISSP-15] Simplificação final do main.cpp: remover toda lógica inline",
        "labels": ["refactor", "cpp", "issp", "architecture"],
        "body": """\
## Contexto

Após todas as fases anteriores, o `main.cpp` ainda pode conter resquícios de callbacks de \
rádio, parsing, debounce, GPIO manual, reports manuais ou dispatch manual. Esta fase remove \
tudo isso e deixa o arquivo apenas com identidade, transporte, behaviors, configurações e \
a chamada de `start()`.

## Objetivo

Reduzir o `main.cpp` ao formato declarativo mínimo, removendo qualquer lógica que já \
pertence a behaviors ou ao runtime.

## Escopo

- Remover callbacks de rádio inline do `main.cpp`.
- Remover lógica de parsing inline.
- Remover lógica de debounce inline.
- Remover manipulação direta de GPIO inline.
- Remover envio manual de reports inline.
- Remover dispatch manual de comandos inline.
- O `main.cpp` final deve conter apenas: identidade, transporte, behaviors, \
configurações e `device.start()`.

## Fora de escopo

- Qualquer nova funcionalidade.
- Alteração de protocolo.

## Proposta técnica

Formato final esperado do `main.cpp`:

```cpp
#include "Issp154Transport.h"
#include "IsspDevice.h"
#include "RelayBehavior.h"
#include "ButtonBehavior.h"

extern "C" void app_main()
{
    static Issp154Transport transport({
        .channel = 15
    });
    static IsspDevice device({
        .name = "interruptor-escritorio"
    }, transport);

    static RelayBehavior relay({
        .capability  = "power",
        .gpio        = GPIO_NUM_13,
        .activeLevel = GPIO_LEVEL_HIGH,
        .initialState = false,
    });
    static ButtonBehavior button({
        .capability  = "button",
        .gpio        = GPIO_NUM_9,
        .debounceMs  = 50,
        .action      = ToggleCapability{"power"},
    });

    device.addBehavior(relay);
    device.addBehavior(button);
    device.start();
}
```

## Critérios de aceite

- [ ] `main.cpp` contém apenas declarações de transport, device, behaviors e `start()`.
- [ ] Nenhuma função de GPIO, parsing, debounce ou report manual no `main.cpp`.
- [ ] Build bem-sucedido.
- [ ] Checklist de regressão da Fase 1 aprovado no hardware.
- [ ] Código de baseline comparado linha a linha confirmando equivalência funcional.

## Testes de compilação

```bash
cd client_154
idf.py build
```

## Testes no hardware

- Execução completa do checklist de regressão da Fase 1.
- Comparação de logs com os logs do baseline documentado.

## Riscos

- Remoção prematura de lógica que ainda não foi migrada para behaviors.
- Diferenças sutis no timing de report entre a versão original e a refatorada.

## Dependências

- #14 (ISSP-14) — relações declarativas entre behaviors implementadas.

## Arquivos ou componentes afetados

- `client_154/main/main.cpp`

## Definição de pronto

- [ ] Build bem-sucedido.
- [ ] Checklist completo de regressão aprovado no hardware.
- [ ] PR revisado e mergeado.
""",
    },
    # -----------------------------------------------------------------------
    # Issue 16 – Fase 16
    # -----------------------------------------------------------------------
    {
        "title": "[ISSP-16] Preparação para ISSP11: validar independência do issp_core",
        "labels": ["architecture", "transport", "issp", "cpp"],
        "body": """\
## Contexto

Um dos princípios da refatoração é que a troca de transporte (de IEEE 802.15.4 para Wi-Fi) \
não deve exigir alteração nos behaviors ou no `IsspDevice`. Esta issue valida essa premissa \
criando o esqueleto de `Issp11Transport` e documentando o contrato de `IIsspTransport`.

## Objetivo

Validar que o `issp_core` não depende de `ISSP154`, documentar o contrato de \
`IIsspTransport`, criar o esqueleto de `Issp11Transport` e provar que os mesmos behaviors \
funcionam com ambos os transportes.

## Escopo

- Verificar que nenhum header ou símbolo de `issp_transport_154` está em `issp_core`.
- Documentar o contrato completo de `IIsspTransport` com pré/pós-condições.
- Criar a estrutura do componente `issp_transport_11` com `Issp11Transport` esqueleto \
(stub que compila mas não implementa Wi-Fi).
- Verificar que o `main.cpp` pode compilar com `Issp11Transport` no lugar de \
`Issp154Transport` sem alterar behaviors ou `IsspDevice`.

## Fora de escopo

- Implementação real da comunicação Wi-Fi ou MQTT.
- Qualquer alteração de protocolo.

## Proposta técnica

Esqueleto do `Issp11Transport`:

```cpp
// components/issp_transport_11/include/Issp11Transport.h
#pragma once
#include "issp_transport.h"

struct Issp11TransportConfig {
    const char* ssid;
    const char* password;
    const char* broker_url;
};

class Issp11Transport : public IIsspTransport
{
public:
    explicit Issp11Transport(const Issp11TransportConfig& config);
    esp_err_t begin() override;
    esp_err_t send(const IsspReport& report) override;
    bool      isConnected() const override;
    void      setReceiveHandler(IsspReceiveHandler handler,
                                void* context) override;
    // Implementação real: TODO
};
```

Teste de compilação com transporte alternativo:

```cpp
// Em main.cpp, trocar Issp154Transport por Issp11Transport e verificar que compila
static Issp11Transport transport({
    .ssid = "test", .password = "test", .broker_url = "mqtt://test"
});
```

## Critérios de aceite

- [ ] `issp_core` sem qualquer dependência de `issp_transport_154`.
- [ ] Contrato de `IIsspTransport` documentado completamente.
- [ ] Esqueleto de `Issp11Transport` compilando.
- [ ] `main.cpp` compilando com `Issp11Transport` no lugar de `Issp154Transport`.
- [ ] Behaviors e `IsspDevice` sem alteração ao trocar transport.
- [ ] Build bem-sucedido.

## Testes de compilação

```bash
# Compilar com Issp154Transport (padrão)
cd client_154 && idf.py build

# Compilar com Issp11Transport (esqueleto)
# Trocar transport em main.cpp, idf.py build
```

## Testes no hardware

Não requerido nesta fase (esqueleto não funcional).

## Riscos

- Dependências transitivas de `Issp154Transport` podem vazar para `issp_core` \
sem que seja percebido.
- O esqueleto de `Issp11Transport` deve ser claramente marcado como não funcional \
para evitar uso acidental.

## Dependências

- #15 (ISSP-15) — `main.cpp` simplificado e modular.

## Arquivos ou componentes afetados

- `components/issp_transport_11/` (novo, esqueleto)
- `components/issp_core/include/issp_transport.h` (documentação)
- `client_154/CMakeLists.txt`

## Definição de pronto

- [ ] Build bem-sucedido com ambos os transportes.
- [ ] Independência do `issp_core` verificada por inspeção.
- [ ] PR revisado e mergeado.
""",
    },
]

# ---------------------------------------------------------------------------
# GitHub API helpers
# ---------------------------------------------------------------------------

API_BASE = "https://api.github.com"
GRAPHQL = "https://api.github.com/graphql"


def headers(token: str) -> dict:
    return {
        "Authorization": f"******",
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
    }


def graphql(token: str, query: str, variables: dict = None):
    payload = {"query": query}
    if variables:
        payload["variables"] = variables
    resp = requests.post(
        GRAPHQL,
        json=payload,
        headers={
            "Authorization": f"******",
            "Content-Type": "application/json",
        },
        timeout=30,
    )
    resp.raise_for_status()
    data = resp.json()
    if "errors" in data:
        raise RuntimeError(f"GraphQL error: {data['errors']}")
    return data["data"]


def rest_post(token: str, path: str, body: dict):
    resp = requests.post(
        f"{API_BASE}{path}",
        json=body,
        headers=headers(token),
        timeout=30,
    )
    resp.raise_for_status()
    return resp.json()


def rest_get(token: str, path: str):
    resp = requests.get(
        f"{API_BASE}{path}",
        headers=headers(token),
        timeout=30,
    )
    resp.raise_for_status()
    return resp.json()


# ---------------------------------------------------------------------------
# Label management
# ---------------------------------------------------------------------------


def ensure_labels(token: str, owner: str, repo: str):
    existing = {lb["name"] for lb in rest_get(token, f"/repos/{owner}/{repo}/labels")}
    for label in LABELS:
        if label["name"] in existing:
            print(f"  label '{label['name']}' already exists — skipping")
            continue
        try:
            rest_post(
                token,
                f"/repos/{owner}/{repo}/labels",
                {
                    "name": label["name"],
                    "color": label["color"],
                    "description": label["description"],
                },
            )
            print(f"  created label '{label['name']}'")
        except requests.HTTPError as exc:
            print(f"  WARNING: could not create label '{label['name']}': {exc}")


# ---------------------------------------------------------------------------
# Project management (GitHub Projects v2 via GraphQL)
# ---------------------------------------------------------------------------


def get_org_id(token: str, owner: str) -> str:
    data = graphql(
        token,
        """
        query($login: String!) {
          organization(login: $login) { id }
        }
        """,
        {"login": owner},
    )
    return data["organization"]["id"]


def create_project(token: str, owner_id: str, title: str) -> str:
    data = graphql(
        token,
        """
        mutation($ownerId: ID!, $title: String!) {
          createProjectV2(input: {ownerId: $ownerId, title: $title}) {
            projectV2 { id number url }
          }
        }
        """,
        {"ownerId": owner_id, "title": title},
    )
    project = data["createProjectV2"]["projectV2"]
    print(f"  created project '{title}' — {project['url']}")
    return project["id"]


def get_status_field(token: str, project_id: str) -> tuple:
    """Return (fieldId, {option_name: option_id})."""
    data = graphql(
        token,
        """
        query($projectId: ID!) {
          node(id: $projectId) {
            ... on ProjectV2 {
              fields(first: 20) {
                nodes {
                  ... on ProjectV2SingleSelectField {
                    id name
                    options { id name }
                  }
                }
              }
            }
          }
        }
        """,
        {"projectId": project_id},
    )
    for field in data["node"]["fields"]["nodes"]:
        if field.get("name") == "Status":
            options = {opt["name"]: opt["id"] for opt in field.get("options", [])}
            return field["id"], options
    return None, {}


def add_status_column(token: str, project_id: str, field_id: str, name: str) -> str:
    data = graphql(
        token,
        """
        mutation($projectId: ID!, $fieldId: ID!, $name: String!) {
          addProjectV2SingleSelectFieldOption(
            input: {projectId: $projectId, fieldId: $fieldId, name: $name}
          ) {
            option { id name }
          }
        }
        """,
        {"projectId": project_id, "fieldId": field_id, "name": name},
    )
    opt = data["addProjectV2SingleSelectFieldOption"]["option"]
    print(f"  created column '{opt['name']}'")
    return opt["id"]


def add_issue_to_project(token: str, project_id: str, issue_node_id: str) -> str:
    data = graphql(
        token,
        """
        mutation($projectId: ID!, $contentId: ID!) {
          addProjectV2ItemById(input: {projectId: $projectId, contentId: $contentId}) {
            item { id }
          }
        }
        """,
        {"projectId": project_id, "contentId": issue_node_id},
    )
    return data["addProjectV2ItemById"]["item"]["id"]


def set_item_status(
    token: str,
    project_id: str,
    item_id: str,
    field_id: str,
    option_id: str,
):
    graphql(
        token,
        """
        mutation($projectId: ID!, $itemId: ID!, $fieldId: ID!, $optionId: String!) {
          updateProjectV2ItemFieldValue(
            input: {
              projectId: $projectId,
              itemId: $itemId,
              fieldId: $fieldId,
              value: {singleSelectOptionId: $optionId}
            }
          ) {
            projectV2Item { id }
          }
        }
        """,
        {
            "projectId": project_id,
            "itemId": item_id,
            "fieldId": field_id,
            "optionId": option_id,
        },
    )


# ---------------------------------------------------------------------------
# Issue creation
# ---------------------------------------------------------------------------


def create_issues(token: str, owner: str, repo: str) -> list:
    """Create all issues and return list of (issue_number, node_id)."""
    results = []
    for issue in ISSUES:
        try:
            created = rest_post(
                token,
                f"/repos/{owner}/{repo}/issues",
                {
                    "title": issue["title"],
                    "body": issue["body"],
                    "labels": issue["labels"],
                },
            )
            print(f"  created issue #{created['number']}: {issue['title'][:60]}...")
            results.append((created["number"], created["node_id"]))
            time.sleep(0.5)  # avoid secondary rate limits
        except requests.HTTPError as exc:
            print(f"  ERROR creating issue '{issue['title'][:50]}': {exc}")
    return results


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

DESIRED_COLUMNS = ["Backlog", "Ready", "In progress", "Testing", "Done"]


def main():
    parser = argparse.ArgumentParser(description="Create ISSP Kanban project on GitHub")
    parser.add_argument("--owner", default=OWNER)
    parser.add_argument("--repo", default=REPO)
    parser.add_argument(
        "--token",
        default=os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN"),
    )
    parser.add_argument(
        "--project-title", default=PROJECT_TITLE
    )
    args = parser.parse_args()

    if not args.token:
        print("ERROR: set GH_TOKEN or GITHUB_TOKEN environment variable")
        sys.exit(1)

    token = args.token
    owner = args.owner
    repo = args.repo

    print(f"\n=== Creating labels in {owner}/{repo} ===")
    ensure_labels(token, owner, repo)

    print(f"\n=== Creating issues in {owner}/{repo} ===")
    issue_ids = create_issues(token, owner, repo)

    print(f"\n=== Creating GitHub Project '{args.project_title}' ===")
    try:
        owner_id = get_org_id(token, owner)
    except Exception as exc:
        print(f"  WARNING: could not get org ID ({exc}); trying as user...")
        data = graphql(
            token,
            "query($login: String!) { user(login: $login) { id } }",
            {"login": owner},
        )
        owner_id = data["user"]["id"]

    project_id = create_project(token, owner_id, args.project_title)

    print("\n=== Configuring Kanban columns (Status field) ===")
    field_id, existing_options = get_status_field(token, project_id)
    column_ids = {}
    for col in DESIRED_COLUMNS:
        if col in existing_options:
            column_ids[col] = existing_options[col]
            print(f"  column '{col}' already exists — reusing")
        else:
            column_ids[col] = add_status_column(token, project_id, field_id, col)

    print("\n=== Adding issues to project (Backlog) ===")
    backlog_id = column_ids.get("Backlog")
    for _number, node_id in issue_ids:
        try:
            item_id = add_issue_to_project(token, project_id, node_id)
            if field_id and backlog_id:
                set_item_status(token, project_id, item_id, field_id, backlog_id)
            print(f"  added issue node {node_id} → Backlog")
            time.sleep(0.3)
        except Exception as exc:
            print(f"  WARNING: could not add issue {node_id} to project: {exc}")

    print("\n=== Done ===")
    print("All labels, issues and the Kanban project have been created.")
    print("Open the project at: https://github.com/orgs/{}/projects/".format(owner))


if __name__ == "__main__":
    main()
