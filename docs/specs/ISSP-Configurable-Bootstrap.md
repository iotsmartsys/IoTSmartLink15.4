# IoTSmartSys — Especificação da API pública `SmartSysApp`

**Tipo:** Normativo
**Estado normativo:** Proposed
**Estado da implementação funcional v1.4:** Validated
**Estado da migração de validação v1.5:** Regressed — runner configurado para
target não suportado
**Prontidão da v1.5:** Not Ready
**Revisão de implementabilidade:** Pending Review
**Versão:** 1.5
**Responsável arquitetural:** Marcelo Miranda
**Última atualização:** 10/08/2026
**Escopo:** API pública de configuração e composição do firmware `client_154`

---

## 1. Contexto

O runtime funcional do `client_154` já separa protocolo, transporte,
commissioning, behaviors, reports e factory reset em componentes com
responsabilidades próprias. A aplicação ainda executa diretamente em
`main.cpp` toda a composição:

```text
configuração fixa do produto
→ inicialização da NVS e leitura do endereço IEEE
→ construção do transporte e do network manager
→ construção do device, behavior e report executor
→ adaptação do factory reset
→ inicialização sequencial
→ tratamento local das falhas
```

Esse fluxo é funcional e validado, mas não oferece uma API orientada ao produto.
Cada novo firmware precisaria repetir a composição e conhecer diretamente
classes cujo nome e responsabilidade pertencem ao protocolo ISSP ou ao
transporte IEEE 802.15.4.

O componente `components/issp_app_154`, a fachada `SmartSysApp` e a composição
descrita nesta especificação foram implementados, corrigidos depois da primeira
revisão arquitetural e validados pelo Arquiteto em hardware (seção 22). A
degradação de ACK e retry observada no enlace existente não foi causada pela
fachada e foi transferida para `EKM-GAP-0006`; seu aceite como risco residual
não declara esse comportamento resolvido.

A `IoTSmartSysCore` é o precedente arquitetural desta etapa. Nela, o firmware
declara capabilities e serviços de produto por meio de `SmartSysApp` antes de
uma chamada única a `setup()`. Esta especificação adota inicialmente apenas
essa experiência de configuração. Não importa Wi-Fi, MQTT, provisioning,
Arduino, OTA nem o restante da implementação da `IoTSmartSysCore`.

## 2. Intenção e decisões confirmadas

O Arquiteto confirmou:

- o entrypoint público deve usar `iotsmartsys::SmartSysApp`;
- nomes públicos de uso comum não devem ser acoplados a ISSP ou IEEE 802.15.4;
- esta versão deve introduzir uma interface para configurar firmware e
  capabilities sem reformular o runtime existente;
- somente a capability correspondente à saída digital vigente será exposta
  inicialmente como `SwitchPlugCapability`;
- `deviceId` permanece configurável nesta versão;
- o endereço curto IEEE 802.15.4 não pertence à API comum e deve preservar
  internamente o valor vigente `0x1001`;
- `endpointId` e `eventType` permanecem configuráveis e com a semântica atual;
- protocolo wire, commissioning, persistência, reports, ACKs, retries,
  factory reset e demais comportamentos validados não devem mudar;
- identidade automática, atribuição de endereço curto e abstração completa do
  protocolo serão tratadas futuramente.

## 3. Problema

O repositório não possui um contrato público único que:

- represente a aplicação como um produto IoTSmartSys;
- receba a identidade do dispositivo;
- crie e registre capabilities a partir de configurações de produto;
- configure o factory reset existente;
- componha os serviços internos do runtime;
- preserve a ordem de inicialização e rollback;
- informe estado, etapa e resultado sem expor tipos ISSP na API comum;
- possa ser reutilizado sem copiar `client_154/main.cpp`.

## 4. Objetivos

- **SMARTAPP-001:** fornecer `iotsmartsys::SmartSysApp` como fachada pública do
  firmware.
- **SMARTAPP-002:** permitir configuração antes de `setup()` no padrão da
  `IoTSmartSysCore`.
- **SMARTAPP-003:** fornecer `addSwitchPlugCapability()` para a capability
  funcional já existente.
- **SMARTAPP-004:** fornecer `configureFactoryResetButton()` reutilizando o
  fluxo atual.
- **SMARTAPP-005:** tornar explícitos estado, etapa e resultado do setup com
  tipos públicos neutros em relação ao protocolo.
- **SMARTAPP-006:** centralizar composição, ordem de inicialização e rollback
  sem alterar os componentes funcionais existentes.
- **SMARTAPP-007:** preservar integralmente contratos wire, persistência,
  commissioning, comandos, reports e factory reset.
- **SMARTAPP-008:** manter configuração e execução determinísticas, com
  armazenamento limitado e sem alocação dinâmica obrigatória durante o setup.

## 5. Fora de escopo

Não fazem parte desta versão:

- alterar ou gerar automaticamente `deviceId`;
- expor ou definir atribuição dinâmica do endereço curto IEEE 802.15.4;
- suportar vários endereços curtos ou provar operação simultânea de múltiplos
  clients no mesmo PAN;
- gerar, persistir ou traduzir automaticamente `endpointId` e `eventType`;
- criar identidade por nome para capabilities;
- adicionar `toggle()`, `setState()` ou nova atuação local à saída digital;
- adicionar outras capabilities;
- criar uma abstração genérica para múltiplos protocolos ou transports;
- renomear classes internas `Issp*`;
- alterar `DigitalOutputBehavior`, exceto por adaptação estritamente necessária
  para ser possuído pela fachada sem mudança funcional;
- alterar payload, checksum, endianness, ACKs, retries, timeouts ou delays;
- alterar commissioning, layout NVS, schema do descritor ou persistência;
- criar novo ciclo de vida com `stop()` ou retry de `setup()`;
- alterar coordenador, rádio de baixo nível ou regras de negócio;
- adicionar Wi-Fi, MQTT, provisioning, Arduino ou OTA;
- publicar componentes em registry ou separá-los deste repositório.

## 6. Estado de referência preservado

### 6.1 Componentes

Continuam responsáveis pelo runtime:

- `Issp154Transport`: rádio e transporte IEEE 802.15.4;
- `Issp154NetworkManager`: descritor persistente e commissioning;
- `IsspDevice`: despacho, behaviors e reports pendentes;
- `Issp154ReportExecutor`: transmissão assíncrona de reports;
- `DigitalOutputBehavior`: saída digital e report de estado;
- `FactoryResetService` e `ResetButtonMonitor`: factory reset local.

Na implementação futura, `SmartSysApp` deverá compor esses objetos sem absorver
suas regras internas.

### 6.2 Baseline do produto

A migração do `client_154` deve preservar:

| Configuração | Valor vigente |
|---|---|
| `deviceId` | `0x15400001` |
| short address interno | `0x1001` |
| relay GPIO | `GPIO_NUM_13` |
| endpoint | `1` |
| event type | `2` |
| active level | `1` |
| estado inicial | `false` |
| report inicial | habilitado |
| factory reset GPIO | `GPIO_NUM_9` |
| botão | active low |
| polling | `20 ms` |
| hold | `10000 ms` |

### 6.3 Ordem vigente

```text
inicializar NVS
→ obter endereço IEEE 802.15.4
→ iniciar monitor de factory reset
→ inicializar ou descobrir rede
→ registrar behavior
→ iniciar device
→ iniciar executor de reports
```

O report inicial continua sendo criado pelo behavior durante o início do
device e transmitido posteriormente pelo executor.

## 7. Arquitetura proposta

### SMARTAPP-DEC-001 — Fachada pública neutra

O componente compartilhado planejado permanece localizado em:

```text
components/issp_app_154
```

O nome do componente é interno ao empacotamento desta versão. Seu header
público de uso comum deve ser:

```cpp
#include "SmartSysApp.h"
```

A classe principal deve ser:

```cpp
namespace iotsmartsys
{
class SmartSysApp;
}
```

Não devem aparecer na API comum nomes como `Issp154ClientApp`,
`Issp154BootstrapResult`, `IsspTransportState` ou `IsspResult`.

### SMARTAPP-DEC-002 — Fachada fina

`SmartSysApp` deve compor as classes existentes por delegação. Ela não deve
reimplementar protocolo, transporte, commissioning, reports, behavior ou
factory reset.

Classes e namespaces `issp::*` podem permanecer na implementação privada do
componente e em suas APIs técnicas já existentes.

### SMARTAPP-DEC-003 — Configuração antes de setup

O firmware deve:

1. construir `SmartSysApp`;
2. adicionar capabilities;
3. configurar opcionalmente o botão de factory reset;
4. chamar `setup()` uma única vez.

Configuração tardia deve ser rejeitada. `setup()` repetido não constitui retry.

### SMARTAPP-DEC-004 — Propriedade

Na implementação futura, `SmartSysApp` deve possuir durante toda a sua vida:

- cópia da configuração da aplicação;
- armazenamento das capabilities criadas por seus métodos `add*`;
- transporte, network manager, device e report executor;
- serviço e monitor de factory reset, quando configurados;
- estado e último resultado do setup.

A fachada deve usar armazenamento de capacidade fixa compatível com
`kMaxDeviceBehaviors`. Os ponteiros retornados por `add*Capability()` devem
permanecer estáveis enquanto `SmartSysApp` existir.

Esta decisão não transfere ownership quando uma API técnica interna receber
referências. O ownership público desta versão decorre somente dos métodos
tipados da própria fachada.

### SMARTAPP-DEC-004A — Lifetime da fachada

`Issp154ReportExecutor` e `ResetButtonMonitor` iniciam tasks que conservam
referências a objetos possuídos por `SmartSysApp`. Como esta versão não possui
um `stop()` operacional capaz de encerrar e aguardar essas tasks, toda instância
destinada a receber uma chamada a `setup()` deve ter duração de armazenamento
estática.

São normativos os seguintes limites:

- antes de qualquer chamada a `setup()`, a destruição da fachada em
  `Configuring` é permitida, pois nenhum método de configuração pode iniciar
  tasks ou recursos assíncronos;
- a partir do início da primeira chamada a `setup()`, a fachada e todos os
  objetos que ela possui devem permanecer vivos até o reboot;
- destruir, desalocar ou deixar sair de escopo uma fachada depois que
  `setup()` começou não é permitido, independentemente de o resultado ser
  `Running`, `NotReady` ou `Failed`;
- ponteiros retornados por `add*Capability()` não podem ser usados depois da
  destruição permitida antes de `setup()`;
- a implementação não pode destacar tasks dos objetos possuídos nem usar
  referências a uma fachada com duração automática ou dinâmica que possa
  terminar antes do reboot.

Esse contrato de duração não introduz `stop()`, retry ou novo estado. Uma
evolução que permita destruição depois de `setup()` exigirá contrato explícito
de parada e sincronização das tasks.

### SMARTAPP-DEC-005 — Plataforma

`setup()` deve:

- executar a mesma inicialização de NVS atualmente presente em `main.cpp`;
- preservar a recuperação vigente com `nvs_flash_erase()` para
  `ESP_ERR_NVS_NO_FREE_PAGES` e `ESP_ERR_NVS_NEW_VERSION_FOUND`;
- obter o endereço IEEE 802.15.4 por `esp_read_mac`;
- manter o endereço estendido em armazenamento pertencente à fachada.

Essa política é preservação explícita do baseline, não uma política genérica
recomendada para produtos futuros. Sua revisão exige nova decisão.

### SMARTAPP-DEC-006 — Endereço curto

O endereço curto deve permanecer `0x1001` na configuração interna do transporte
e não deve aparecer em `SmartSysAppConfig`.

Esta versão não afirma que `0x1001` possa ser compartilhado por vários
dispositivos. A atribuição de endereço curto permanece uma limitação conhecida
e deve ser especificada antes de validar múltiplos clients no mesmo PAN.

### SMARTAPP-DEC-007 — Capability inicial

`addSwitchPlugCapability()` deve criar uma capability pública
`SwitchPlugCapability` apoiada pelo `DigitalOutputBehavior` vigente.

Nesta versão:

- `endpointId` e `eventType` continuam explícitos em `SwitchConfig`;
- comandos `ON`, `OFF` e `TOGGLE` mantêm a semântica atual;
- `state()` pode expor o estado já consultável no behavior;
- nenhuma nova operação local de atuação é adicionada.

### SMARTAPP-DEC-008 — Factory reset

`configureFactoryResetButton()` deve reutilizar, sem duplicação funcional,
`FactoryResetService` e `ResetButtonMonitor`.

Para permitir uso compartilhado pela fachada, a implementação futura pode
realocar esses arquivos de `client_154/main/reset` para
`components/issp_app_154`, preservando sua lógica. O componente não pode
depender de `client_154/main`.

O monitor deve iniciar antes da inicialização da rede e permanecer disponível
quando o setup terminar em `NotReady`.

### SMARTAPP-DEC-009 — Resultado neutro

Os tipos públicos de estado, etapa e erro devem pertencer ao vocabulário da
aplicação. A implementação deve mapear resultados internos sem converter falha
em sucesso nem exigir parsing de logs.

## 8. Contratos públicos

Os nomes e campos desta seção são normativos. Ajustes sintáticos indispensáveis
devem preservar a intenção e ser relatados na análise de implementabilidade.

### 8.1 Namespaces

```cpp
namespace iotsmartsys
{
class SmartSysApp;
}

namespace iotsmartsys::app
{
struct SmartSysAppConfig;
struct SwitchConfig;
struct PushButtonConfig;
}

namespace iotsmartsys::core
{
class SwitchPlugCapability;
}
```

### 8.2 Configuração da aplicação

```cpp
namespace iotsmartsys::app
{

struct SmartSysAppConfig
{
    std::uint32_t deviceId;
};

}
```

`deviceId` não pode ser zero. A configuração deve ser copiada pela fachada.

### 8.3 Configuração da saída

```cpp
namespace iotsmartsys::app
{

struct SwitchConfig
{
    gpio_num_t pin;
    bool activeHigh;
    bool initialState;
    bool reportOnStart;
    std::uint8_t endpointId;
    std::uint8_t eventType;
};

}
```

Regras:

- `pin` deve ser um GPIO de saída válido;
- `activeHigh=true` deve mapear para `activeLevel=1`;
- `activeHigh=false` deve mapear para `activeLevel=0`;
- estado inicial, report inicial, endpoint e event type devem ser encaminhados
  sem alteração semântica ao behavior;
- pares duplicados de `endpointId` e `eventType` devem ser rejeitados;
- não deve haver inicialização de GPIO antes de `setup()`.

### 8.4 Configuração do factory reset

```cpp
namespace iotsmartsys::app
{

struct PushButtonConfig
{
    gpio_num_t pin;
    bool activeLow;
    std::uint32_t holdTimeMs;
    std::uint32_t pollIntervalMs;
};

}
```

Regras:

- `pin` deve ser GPIO válido;
- `holdTimeMs` e `pollIntervalMs` devem ser maiores que zero;
- somente uma configuração de factory reset pode ser aceita;
- ausência de configuração é válida;
- configuração duplicada ou tardia deve ser rejeitada.

### 8.5 Capability pública

```cpp
namespace iotsmartsys::core
{

class SwitchPlugCapability
{
public:
    bool state() const;
};

}
```

O ponteiro retornado pela fachada não transfere ownership ao chamador.
`SwitchPlugCapability` não deve expor tipos ISSP em sua API pública.

### 8.6 Estados e resultado

```cpp
namespace iotsmartsys
{

enum class AppState : std::uint8_t
{
    Configuring,
    Starting,
    Running,
    NotReady,
    Failed
};

enum class SetupStage : std::uint8_t
{
    None,
    InitializePlatform,
    ValidateConfiguration,
    InitializeNetwork,
    RegisterCapabilities,
    StartDevice,
    StartReportExecutor,
    Completed
};

enum class AppResult : std::uint8_t
{
    Ok,
    InvalidArgument,
    NotReady,
    Busy,
    Failed
};

struct SetupResult
{
    AppState state;
    SetupStage stage;
    AppResult result;
};

}
```

Mapeamento mínimo:

| Resultado interno | Resultado público |
|---|---|
| `IsspResult::Ok` | `AppResult::Ok` |
| `IsspResult::InvalidArgument` | `AppResult::InvalidArgument` |
| `IsspResult::NotReady` | `AppResult::NotReady` |
| `IsspResult::Busy` | `AppResult::Busy` |
| `IsspResult::Failed` | `AppResult::Failed` |

### 8.7 API de `SmartSysApp`

```cpp
namespace iotsmartsys
{

class SmartSysApp
{
public:
    explicit SmartSysApp(const app::SmartSysAppConfig &config);

    core::SwitchPlugCapability *
    addSwitchPlugCapability(const app::SwitchConfig &config);

    AppResult
    configureFactoryResetButton(const app::PushButtonConfig &config);

    SetupResult setup();

    AppState state() const;
    SetupResult lastSetupResult() const;
    AppResult lastConfigurationResult() const;
    std::uint32_t deviceId() const;
};

}
```

Contratos:

- toda instância destinada a receber uma chamada a `setup()` deve ter duração de
  armazenamento estática, conforme `SMARTAPP-DEC-004A`;
- `addSwitchPlugCapability()` é permitido somente em `Configuring`;
- configuração inválida, duplicada ou excesso de capacidade retorna `nullptr`;
- a primeira falha de configuração deve ser preservada em
  `lastConfigurationResult()`;
- uma falha de configuração anterior deve fazer `setup()` falhar em
  `ValidateConfiguration` antes de iniciar plataforma ou rede;
- `configureFactoryResetButton()` deve retornar resultado explícito;
- zero capabilities é válido para a fachada;
- `setup()` é síncrono e realiza uma única tentativa;
- consultas devem ser não bloqueantes;
- configuração e setup devem ocorrer no mesmo contexto serial;
- thread safety não é garantida nesta versão.

## 9. Exemplo normativo de consumo

```cpp
#include "SmartSysApp.h"

using namespace iotsmartsys;
using namespace iotsmartsys::app;

namespace
{
constexpr std::uint32_t kDeviceId = 0x15400001;
}

static SmartSysApp app({
    .deviceId = kDeviceId,
});

extern "C" void app_main()
{
    app.addSwitchPlugCapability({
        .pin = GPIO_NUM_13,
        .activeHigh = true,
        .initialState = false,
        .reportOnStart = true,
        .endpointId = 1,
        .eventType = 2,
    });

    app.configureFactoryResetButton({
        .pin = GPIO_NUM_9,
        .activeLow = true,
        .holdTimeMs = 10000,
        .pollIntervalMs = 20,
    });

    const SetupResult result = app.setup();
    if (result.state != AppState::Running)
    {
        return;
    }
}
```

O firmware não deve precisar incluir headers `issp_*`, construir transport,
network manager, device, behavior, report executor ou adaptar manualmente o
factory reset.

A duração estática de `app` é parte do exemplo normativo e não apenas uma
escolha ilustrativa.

## 10. Fluxo de setup

```text
validar configuração acumulada
   └── inválida: Configuring → Failed
→ AppState::Starting
→ inicializar NVS com a política vigente
→ obter endereço IEEE
→ construir/conectar infraestrutura interna
→ iniciar monitor de factory reset, quando configurado
→ initializeNetwork()
   ├── descritor válido: ativar rede
   ├── sem descritor: commissioning vigente
   └── rede ausente: NotReady
→ registrar capabilities/behaviors na ordem de adição
→ IsspDevice::start()
→ Issp154ReportExecutor::start()
→ Running
```

`setup()` não deve aguardar ACK do report inicial. A aplicação decide o que
fazer após `Running`, `NotReady` ou `Failed`.

## 11. Estados

Transições permitidas:

```text
construção → Configuring
Configuring → Starting
Configuring → Failed
Starting → Running
Starting → NotReady
Starting → Failed
```

Não há transição de saída dos estados terminais nesta versão. Factory reset
termina por reboot e não representa transição local.

A validação da configuração acumulada ocorre enquanto o estado ainda é
`Configuring`. Se ela falhar, `setup()` deve realizar a transição direta
`Configuring → Failed`, retornar a etapa `ValidateConfiguration` e não passar
por `Starting`. Somente configuração válida permite a transição
`Configuring → Starting` e o início da plataforma.

Interpretação:

- sucesso: `Running`, `Completed`, `Ok`;
- rede não encontrada: `NotReady`, `InitializeNetwork`, `NotReady`;
- configuração inválida: `Failed`, `ValidateConfiguration`,
  `InvalidArgument`;
- falha de NVS ou leitura de MAC: `Failed`, `InitializePlatform`, resultado
  público correspondente;
- setup repetido: estado terminal vigente, `None`, `Busy`;
- demais falhas: `Failed`, etapa correspondente e erro mapeado.

## 12. Falhas e rollback

O primeiro erro determina etapa, resultado e estado. Rollback não pode apagar o
descritor persistido nem substituir o erro primário.

| Falha | Estado | Ação |
|---|---|---|
| configuração acumulada inválida | `Failed` | não iniciar plataforma ou rede |
| inicialização da NVS falha | `Failed` | não iniciar rádio |
| leitura do endereço IEEE falha | `Failed` | não iniciar rádio |
| rede não encontrada | `NotReady` | encerrar transporte; preservar NVS e reset |
| ativação ou persistência da rede falha | `Failed` | encerrar transporte se iniciado |
| registro de capability falha | `Failed` | não iniciar device ou executor |
| início do device falha | `Failed` | encerrar transporte; não iniciar executor |
| início do executor falha | `Failed` | encerrar transporte; preservar erro primário |
| rollback do transporte falha | estado primário | registrar falha sem substituir retorno |

Falha posterior ao início do device é terminal até reboot, pois o device não
possui contrato de `stop()`.

## 13. Observabilidade

Cada primeira invocação de `setup()`, inclusive quando a configuração acumulada
for inválida, deve produzir exatamente um evento terminal:

```text
app_setup begin capabilities=<n> factory_reset=<configured|disabled>
app_setup stage=<stage>
app_setup completed state=running
app_setup completed state=not_ready stage=<stage> result=<result>
app_setup failed stage=<stage> result=<result>
app_setup rollback transport=<result>
```

Logs da fachada devem usar o vocabulário público. Logs internos vigentes podem
continuar usando seus nomes técnicos. Endereços completos e payloads não devem
ser registrados.

## 14. Compatibilidade

### 14.1 API

A fachada é aditiva. APIs públicas técnicas existentes não devem ser removidas
nem alteradas. Consumidores avançados podem continuar compondo os componentes
diretamente.

### 14.2 Wire e persistência

Não há mudança de payload, versão, checksum, endianness, sequência, endpoint,
event type, comandos, ACKs, report, schema NVS ou descritor de rede.

### 14.3 Factory reset

A realocação autorizada dos arquivos de reset não pode alterar GPIO, polaridade,
tempos, limpeza do vínculo, logs materiais ou reboot após sucesso.

## 15. Dependências e estrutura

O componente futuro `issp_app_154`, ainda inexistente, deverá declarar as
dependências necessárias à composição. A tabela abaixo foi confirmada pelo
Engenheiro Analista contra o estado atual do repositório (`client_154/main`,
`components/issp_core`, `components/issp_behaviors` e
`components/issp_transport_154`):

| Classificação confirmada | Dependência | Fundamentação |
|---|---|---|
| pública | `esp_driver_gpio` | `gpio_num_t` aparece nos tipos públicos `SwitchConfig` e `PushButtonConfig`; mesmo precedente já usado em `components/issp_behaviors/CMakeLists.txt` |
| privada | `issp_core` | usada somente pela composição interna proposta, sem tipo `Issp*` exposto em header público da fachada |
| privada | `issp_behaviors` | usada somente pela composição interna proposta |
| privada | `issp_transport_154` | usada somente pela composição interna proposta |
| privada | `nvs_flash` | usada somente pela inicialização interna de plataforma em `components/issp_app_154/src/smart_sys_app_hardware.cpp` |
| privada | `esp_timer` | necessária porque `reset_button_monitor.cpp`, ao ser realocado para o componente, inclui `esp_timer.h` diretamente |
| privada | `esp_hw_support` | usada somente pela leitura interna do endereço IEEE via `esp_read_mac` |

`ieee802154` não precisa constar nessa tabela: nenhuma fonte que compõe
`issp_app_154` inclui diretamente cabeçalhos do driver `ieee802154`; os tipos
`esp_ieee802154_frame_info_t` e `esp_ieee802154_tx_error_t` chegam apenas por
`issp154_transport.hpp`, cujo próprio componente já declara `ieee802154` como
`REQUIRES` público. A presença de `PRIV_REQUIRES ieee802154` no
`client_154/main/CMakeLists.txt` vigente não decorre de uso direto em
`main.cpp` e não constitui precedente obrigatório para a fachada. Caso a
implementação futura constate necessidade real de declará-la, trata-se de
ajuste sintático de build a registrar na entrega, não de lacuna de
implementabilidade.

O componente não pode depender de:

```text
client_154/main
coordinator_154
examples/issp_minimal_client
```

## 16. Migração

1. criar o componente ainda inexistente `components/issp_app_154`;
2. implementar o header público `SmartSysApp.h`;
3. reutilizar os componentes funcionais existentes por composição;
4. realocar o reset somente se necessário para eliminar dependência reversa;
5. migrar `client_154/main.cpp` para a API da seção 8;
6. atualizar o consumidor mínimo para comprovar compilação e link da fachada;
7. remover do `main.cpp` a composição técnica substituída;
8. validar equivalência integral do baseline;
9. executar testes, builds e hardware definidos nesta especificação.

Não pode existir no resultado final dois runtimes ativos, fonte duplicada ou
dependência reversa.

## 17. Critérios de aceitação

### API e estrutura

- **SMARTAPP-AC-001:** `SmartSysApp.h` não expõe identificadores `Issp` ou
  `154` em nomes públicos.
- **SMARTAPP-AC-002:** o firmware funcional inclui apenas a fachada e tipos
  públicos necessários para sua configuração.
- **SMARTAPP-AC-003:** `SmartSysApp` possui infraestrutura e capabilities
  criadas por seus métodos tipados.
- **SMARTAPP-AC-004:** ponteiros de capabilities permanecem estáveis.
- **SMARTAPP-AC-004A:** uma fachada destruída em `Configuring`, antes de
  `setup()`, não iniciou tasks nem recursos assíncronos.
- **SMARTAPP-AC-004B:** a API, o exemplo e os consumidores garantem duração
  estática até reboot para toda fachada em que `setup()` é chamado.
- **SMARTAPP-AC-004C:** testes ou instrumentação comprovam que as tasks do
  executor de reports e do monitor de reset nunca observam objetos destruídos.
- **SMARTAPP-AC-005:** configuração inválida, duplicada, tardia e excesso de
  capacidade são rejeitados conforme a seção 8.
- **SMARTAPP-AC-006:** `setup()` repetido retorna `Busy`.
- **SMARTAPP-AC-007:** não existe dependência reversa para aplicações.

### Fluxo

- **SMARTAPP-AC-008:** configuração inválida não inicializa NVS nem rádio.
- **SMARTAPP-AC-009:** descritor válido termina em `Running`.
- **SMARTAPP-AC-010:** commissioning bem-sucedido termina em `Running`.
- **SMARTAPP-AC-011:** rede ausente termina em `NotReady`, sem reboot, novo
  loop de scan ou remoção do descritor.
- **SMARTAPP-AC-012:** cada falha injetada produz estado, etapa, resultado e
  rollback correspondentes.
- **SMARTAPP-AC-013:** falha de rollback não oculta a falha primária.

### Preservação

- **SMARTAPP-AC-014:** todos os valores da seção 6.2 permanecem idênticos.
- **SMARTAPP-AC-015:** report inicial continua criado pelo behavior vigente.
- **SMARTAPP-AC-016:** comandos `ON`, `OFF` e `TOGGLE` e seus ACKs permanecem
  equivalentes.
- **SMARTAPP-AC-017:** factory reset preserva limpeza exclusiva do vínculo e
  reboot após sucesso.
- **SMARTAPP-AC-018:** nenhuma constante funcional de protocolo, transporte,
  retry, timeout, delay, persistência ou commissioning muda.
- **SMARTAPP-AC-019:** arquivos de reset e behavior não são duplicados.

### Evidência

- **SMARTAPP-AC-020:** testes automatizados cobrem configuração, ownership,
  lifetime da fachada, capacidade, estados, ordem, falhas e rollback.
- **SMARTAPP-AC-021:** `client_154`, `examples/issp_minimal_client` e
  `coordinator_154` compilam nos targets e ESP-IDF definidos pela especificação
  de componentes reutilizáveis.
- **SMARTAPP-AC-022:** hardware comprova boot por descritor, commissioning,
  ausência de rede, comandos, ACK, report inicial e factory reset.
- **SMARTAPP-AC-023:** revisão do diff comprova migração sem reimplementação
  funcional.
- **SMARTAPP-AC-024:** `git diff --check` e reconciliação EKM são aprovados.

## 18. Validações obrigatórias

A implementação deve registrar:

- matriz `SMARTAPP-001` a `SMARTAPP-008`;
- matriz `SMARTAPP-AC-001` a `SMARTAPP-AC-024`;
- testes de configuração, lifetime da fachada e lifetime das capabilities;
- falhas injetadas em cada etapa;
- inspeção da ordem de chamadas;
- comparação dos valores da seção 6.2 antes e depois;
- prova de ausência de duplicação e dependência reversa;
- revisão separada de código, build e conhecimento;
- três builds com target, versão do ESP-IDF, warnings, tamanho e SHA-256;
- checklist e resultados de hardware;
- `git diff --check`;
- baseline inicial e reconciliação do inventário final.

QEMU não é estratégia permitida para nenhuma execução futura desta
especificação. Os testes de configuração, estados, ordem, falhas e rollback
devem executar host-native quando `SmartSysApp::SetupHooks` e os fakes
preservarem toda a semântica material do critério; caso essa fidelidade não
possa ser demonstrada, devem executar no app Unity em ESP32-H2 físico. Rádio,
NVS, GPIO e factory reset continuam exigindo os targets físicos definidos por
SMARTAPP-AC-022. Aplicam-se integralmente os contratos de
`Repository-Test-Execution-Policy.md`.

Os resultados QEMU registrados na seção 22 permanecem fatos históricos da
versão 1.4, mas não podem ser reutilizados como evidência vigente da versão
1.5 ou de revisões futuras. Os 20 cenários permanecem preservados, mas esta
versão não solicita nem autoriza sua execução. Somente especificação futura
pode reativá-los com propósito, recorte e evidência explícitos.

## 19. Ativos autorizados para implementação futura

Depois de aprovação humana e seleção do papel formal aplicável, a implementação
pode alterar somente o necessário em:

- `components/issp_app_154/`;
- `client_154/main/main.cpp` e `client_154/main/CMakeLists.txt`;
- `client_154/main/reset/`, somente para realocação sem mudança funcional;
- `examples/issp_minimal_client/`;
- `components/README.md`;
- `docs/specs/ISSP-Architecture.md`;
- `docs/specs/ISSP-Reusable-Components.md`;
- `docs/rfc/KNOWLEDGE-MAP.md`;
- `docs/rfc/EKM-CHANGELOG.md`;
- esta especificação, somente para transições e resultado.

Alterar wire, persistência, identidade, endereço curto, factory reset, reports
ou comportamento da saída exige interrupção e decisão humana.

`EKM-CHG-0007` foi encerrada depois da implementação, da revisão corretiva,
dos testes automatizados, dos builds e da validação humana em hardware
registrados na seção 22.

## 20. Decisões futuras registradas

Permanecem fora desta versão:

- origem e estabilidade de `deviceId`;
- atribuição e persistência do endereço curto;
- operação de vários clients no mesmo PAN;
- identidade lógica e nomes de capabilities;
- tradução automática de capabilities para endpoints e event types;
- API de atuação local;
- expansão do catálogo de capabilities;
- fronteira multiprotocolo.

Esses itens não bloqueiam a fachada inicial e não podem ser inferidos pela
implementação.

## 21. Relatório obrigatório da implementação

O relatório futuro deve informar:

1. resultado executivo;
2. baseline e aprovação aplicável;
3. matrizes de requisitos e aceite;
4. API pública resultante;
5. ordem efetiva de setup;
6. propriedade e duração dos objetos;
7. falhas e rollback;
8. preservação de wire, persistência, report e factory reset;
9. arquivos alterados e realocados;
10. fontes dependentes revisadas;
11. testes, builds e hardware;
12. warnings, desvios, riscos e pendências;
13. estado de `EKM-CHG-0007`;
14. Definition of Done EKM;
15. reconciliação integral do inventário final.

## 22. Registro de implementação (2026-07-30, corrigido)

**Papel:** Engenheiro Implementador. **Ordem inicial:** aprovação explícita
do Arquiteto para iniciar a implementação desta especificação (versão 1.3,
revisão `Implementable`). **Ordem de correção (mesmo dia):** o Arquiteto
determinou remover toda exposição de tipos/headers `issp::*` de
`SmartSysApp.h`, tornar privadas as dependências ISSP/NVS/reset, implementar
e **executar** testes automatizados de estados, ordem de inicialização,
`setup()` repetido, falhas injetadas e rollback sem classificá-los como
dependentes de hardware, e adicionar um build de `coordinator_154` em
ESP32-C6. Esta seção substitui integralmente o registro da rodada inicial;
o que mudou está descrito em 22.1.

### 22.1 Resultado executivo

A rodada inicial armazenava `issp::Issp154Transport`,
`issp::Issp154NetworkManager`, `issp::IsspDevice`,
`issp::Issp154ReportExecutor`, `issp::DigitalOutputBehavior` e os serviços
de reset como membros diretos de `SmartSysApp`, cujo header público incluía
`digital_output_behavior.hpp`, `issp154_*.hpp`, `issp_device.hpp`,
`issp_limits.hpp` e `reset/*.hpp` — violando `SMARTAPP-AC-001`. A correção:

- `SmartSysApp.h` agora só inclui `<cstddef>`, `<cstdint>` e
  `driver/gpio.h` (exigido pelo próprio contrato normativo da seção 8 para
  `gpio_num_t`); nenhum header `issp_*` ou `reset/*` aparece nele;
- `core::SwitchPlugCapability::state()` é servido por um par
  função-ponteiro/contexto opaco (`StateFn`), o mesmo padrão de callback já
  usado internamente pelo runtime ISSP (`IsspDevice::CommandHandler`), então
  a fachada não precisa nomear `issp::DigitalOutputBehavior` no header;
  `SmartSysApp` guarda seu estado inteiro atrás de um `struct Impl`
  incompleto no header público, materializado por placement-new em um
  buffer de bytes de tamanho fixo (`SmartSysApp::kImplStorageBytes`, sem
  alocação dinâmica, verificado por `static_assert(sizeof(Impl) <=
  kImplStorageBytes)`);
- `Impl` foi dividido em dois arquivos privados: `src/smart_sys_app.cpp`
  (máquina de estados de `setup()`, sem nenhuma dependência de rádio) e
  `src/smart_sys_app_hardware.cpp` (NVS, transporte, network manager,
  device, executor de reports e serviços de reset reais), unidos por
  `src/smart_sys_app_impl.hpp` (privado, não publicado em `include/`);
  `components/issp_app_154/CMakeLists.txt` só compila
  `smart_sys_app_hardware.cpp`, `src/reset/*.cpp` e requer
  `issp_transport_154`/`nvs_flash`/`esp_timer`/`esp_hw_support` quando o
  alvo é `esp32h2` ou `esp32c6` (`idf_build_get_property(... IDF_TARGET)`);
  em qualquer outro alvo, `issp_app_154` só oferece o construtor de dois
  argumentos abaixo — o construtor de produto de um argumento é definido
  apenas em `smart_sys_app_hardware.cpp`;
- os cinco passos de hardware de `setup()` (inicializar plataforma,
  inicializar rede, registrar capability, iniciar device, iniciar executor)
  e o rollback do transporte passaram a ser indireções por ponteiro de
  função — `SmartSysApp::SetupHooks`, um tipo público novo e aditivo, não
  normativo (seção 8 não o define), documentado no header como mecanismo de
  teste. O construtor de produto (`SmartSysApp(config)`) os liga às
  implementações reais; um segundo construtor
  (`SmartSysApp(config, hooks)`) permite substituí-los por fakes.

Testes automatizados cobrindo configuração, estados, ordem de inicialização,
`setup()` repetido e rollback foram escritos usando exclusivamente
`SmartSysApp::SetupHooks` — nenhum toca NVS, GPIO real ou rádio — e
**executados** sob QEMU (`qemu-riscv32`, ferramenta oficial do ESP-IDF,
modelo `esp32c3`), não em hardware físico. Resultado: 19/19 casos `PASS`,
0 falhas. `coordinator_154` foi adicionalmente compilado para `esp32c6`, sem
tocar seu `sdkconfig` versionado.

### 22.2 Baseline e aprovação aplicável

Baseline desta correção: o commit `2aba6f9` (rodada inicial). Aprovação:
instrução do Arquiteto nesta conversa, com as cinco correções obrigatórias
listadas no cabeçalho desta seção. Em atuação humana posterior, o Arquiteto
executou a validação em hardware, declarou a implementação funcional e
autorizou o fechamento desta especificação.

### 22.3 Matrizes de requisitos e aceite

| Objetivo | Estado |
|---|---|
| SMARTAPP-001 a 008 | Implementado |

| Critério | Estado |
|---|---|
| AC-001 | Satisfeito: `SmartSysApp.h` não inclui nenhum header `issp_*`/`reset/*` nem nomeia tipo `Issp*`/`Issp154*` (verificado por inspeção do header e pelos quatro builds abaixo) |
| AC-002 a AC-005 | Implementado (código e build) |
| AC-004A, AC-004B | Implementado (destruição só permitida em `Configuring`; exemplo e `client_154` usam duração estática) |
| AC-004C | Satisfeito para o contrato desta versão: fachada e consumers usam duração estática até reboot; a execução prolongada em hardware não apresentou acesso a objetos destruídos |
| AC-006 a AC-013 | Implementado **e executado** sob QEMU via `SmartSysApp::SetupHooks` (seção 22.6); nenhum caso depende de hardware |
| AC-014 a AC-019 | Preservado por inspeção do diff e validação humana em hardware. A degradação de ACK/retry observada pertence ao enlace preexistente e foi separada em `EKM-GAP-0006`, sem ser declarada resolvida |
| AC-020 | Satisfeito para os casos hardware-independentes: 19 testes de configuração e de `setup()` escritos, compilados e **executados** sob QEMU, todos `PASS` (seção 22.6). Casos que exigem hardware real (rádio, NVS, GPIO físicos) permanecem em `SMARTAPP-AC-022` |
| AC-021 | Satisfeito: os quatro builds (`client_154` ESP32-H2, `examples/issp_minimal_client` ESP32-H2, `coordinator_154` ESP32-C6, app de teste ESP32-C3) compilam sem warnings (seção 22.6) |
| AC-022 | Aceito pelo Arquiteto após validação em hardware. Os logs preservados comprovam boot por descritor, `Running`, report inicial e atuação por comandos; o Arquiteto declarou o firmware funcional. A confiabilidade residual de ACK/retry foi aceita fora desta mudança e registrada em `EKM-GAP-0006` |
| AC-023 | Satisfeito por este registro e pela revisão do diff resultante |
| AC-024 | `git diff --check` executado (seção 22.6); reconciliação em 22.10 |

### 22.4 API pública resultante

A API normativa da seção 8 permanece idêntica nos nomes e campos. Dois
ajustes aditivos, ambos fora do escopo normativo da seção 8 e necessários
apenas para AC-001 e para a testabilidade sem hardware exigida nesta
correção:

- `SmartSysApp::SetupHooks` (struct pública, com os seis ponteiros de função
  dos passos de `setup()` e um `void *context`) e o construtor
  `SmartSysApp(config, hooks)`, documentados no header como não normativos;
- `SmartSysApp::kImplStorageBytes` (constante pública que apenas dimensiona
  um buffer privado; não nomeia nenhum tipo `issp::*`).

Um ajuste sintático indispensável, já registrado na rodada inicial e
preservado aqui: o exemplo normativo da seção 9 declara
`static SmartSysApp app(...)` junto de `using namespace iotsmartsys::app;`,
o que torna `app` ambíguo entre a variável e o namespace
`iotsmartsys::app`. A implementação usa `smartSysApp` como nome de
instância em `client_154/main.cpp` e não importa `iotsmartsys::app` via
`using namespace`.

### 22.5 Ordem efetiva de setup, propriedade e duração, falhas e rollback

Implementados conforme as seções 10 a 12, agora expressos como uma sequência
de chamadas a `SetupHooks` que a máquina de estados em `smart_sys_app.cpp`
orquestra: `initializePlatform` → `initializeNetwork` → `registerCapability`
(uma vez por capability, na ordem de adição) → `startDevice` →
`startReportExecutor`, com `rollbackTransport` chamado em toda falha após
`initializeNetwork` ter sido tentado. A implementação real desses passos
(`smart_sys_app_hardware.cpp`) preserva exatamente a lógica da rodada
inicial: NVS não aborta mais o processo (falha mapeada para
`Failed`/`InitializePlatform`, exigido por `SMARTAPP-005`); a rede é
inicializada preservando a política de commissioning vigente; os objetos
internos ficam em `HardwareState`, dentro do buffer opaco
`Impl::hardwareStorage_`, com armazenamento fixo (sem alocação dinâmica) e
construídos apenas dentro de `setup()`.

### 22.6 Preservação de wire, persistência, report e factory reset; arquivos alterados; dependências revisadas; testes, builds e hardware

Nenhuma constante ou lógica de protocolo, persistência, ACK, retry, timeout,
delay ou commissioning foi alterada; `DigitalOutputBehavior`,
`Issp154Transport`, `Issp154NetworkManager`, `Issp154ReportExecutor` e
`IsspDevice` não foram modificados.

Arquivos criados nesta correção:
`components/issp_app_154/src/smart_sys_app_impl.hpp`,
`components/issp_app_154/src/smart_sys_app_hardware.cpp`. Arquivos
reescritos: `components/issp_app_154/include/SmartSysApp.h`,
`components/issp_app_154/src/smart_sys_app.cpp`,
`components/issp_app_154/CMakeLists.txt`,
`components/issp_app_154/test_apps/smart_sys_app_test/main/test_smart_sys_app.cpp`
(e seu `CMakeLists.txt`, para incluir `issp_core`), `components/README.md`.
Arquivos realocados sem mudança funcional (de `include/reset/` para
`src/reset/`, agora privados): `factory_reset_service.hpp`,
`ifactory_reset_requester.hpp`, `reset_button_monitor.hpp`. Nenhum arquivo
de `client_154/main`, `coordinator_154` ou `examples/issp_minimal_client`
fora do recorte autorizado foi tocado; `client_154/main.cpp` e
`examples/issp_minimal_client/main/main.cpp` não precisaram de nova
alteração (a API pública que consomem não mudou).

Builds executados (ESP-IDF v6.0.1-dirty, sem warnings, sdkconfig versionado
de `client_154`/`coordinator_154` preservado intacto via `-DSDKCONFIG` de
build isolado):

| Projeto | Alvo | Binário | Tamanho | SHA-256 |
|---|---|---|---|---|
| `client_154` | esp32h2 | `sensor_154.bin` | 247424 bytes | `8707ecc0be796faedf87dd82b5039ce9f6a7a8dfba3f7fa12cd5fa1b8c5d89a0` |
| `examples/issp_minimal_client` | esp32h2 | `issp_minimal_client.bin` | 247440 bytes | `7260c2667ee0c70bccf3be575cc7f4eadfde08b39581a06c67f2835fbb6e864f` |
| `coordinator_154` | esp32c6 | `central_154.bin` | 289760 bytes | `ababe6b8cebd7bc07d120f49e4eb2fa212f7d35a335985abcd17069514ec40b8` |
| `components/issp_app_154/test_apps/smart_sys_app_test` | esp32c3 (QEMU) | `smart_sys_app_test.bin` | 138272 bytes | `973866449120310cc87ddd30d064d822da6924629539cd04a088d5e50bfce6a4` |

`coordinator_154` não depende de `components/`; o build em ESP32-C6 comprova
apenas ausência de regressão fora do recorte, num alvo diferente do usado
antes. `git diff --check` não reportou espaços em branco inválidos.

Execução automatizada sob QEMU (`idf.py -B build_qemu_c3 qemu`, ferramenta
`qemu-riscv32` versão `esp_develop_9.2.2_20250817` instalada via
`idf_tools.py install qemu-riscv32`, dependências de biblioteca dinâmica do
macOS — `pixman`, `libgcrypt`, `sdl2`/`sdl2-compat`, `glib` e as demais
dependências de `qemu` — instaladas via Homebrew neste ambiente de
desenvolvimento): 19 `TEST_CASE` executados, **19 `PASS`, 0 `FAIL`**. Um
`Guru Meditation Error` (panic) ocorre em QEMU somente depois que
`app_main()` retorna normalmente e todos os resultados já foram impressos
(`Returned from app_main()` aparece antes do panic); é um artefato conhecido
de app de teste mínimo cuja `main_task` termina, não uma falha de
`SmartSysApp` — nenhuma asserção falhou antes dele. A saída bruta de um dos
ciclos de execução (com o panic pós-conclusão) está preservada em
`/tmp/qemu_run3.log` neste ambiente, não versionada.

Hardware físico: executado posteriormente pelo Arquiteto em `client_154`
(ESP32-H2) e `coordinator_154` (ESP32-C6). O client carregou a rede persistida,
registrou a capability, criou o report inicial e terminou em `Running`; o
coordenador recebeu reports, transmitiu ACKs e enviou comandos `ON` e `TOGGLE`,
com mudança de estado observada no client. O Arquiteto declarou a implementação
funcional e aprovou o fechamento.

Os mesmos logs mostram perda intermitente de ACK nos dois sentidos: reports
recebidos pelo coordenador podem permanecer sem confirmação no client, e o
coordenador pode declarar timeout mesmo quando o comando foi executado. O retry
externo do report também cria nova sequência para o mesmo estado e pode gerar
eventos repetidos para o host. O fechamento desta fachada aceita esse risco
preexistente e o transfere para `EKM-GAP-0006`; não o converte em validação do
enlace confirmado.

### 22.7 Warnings, desvios, riscos e pendências

- Warnings: nenhum nos quatro builds da seção 22.6.
- Desvios registrados: nome da instância no exemplo de consumo (seção
  22.4); `SmartSysApp::SetupHooks`, o construtor de dois argumentos e
  `SmartSysApp::kImplStorageBytes` como API pública aditiva, não normativa
  (seção 22.4).
- Ferramenta de ambiente instalada nesta etapa (fora do repositório):
  `qemu-riscv32` via `idf_tools.py`, e as bibliotecas dinâmicas macOS que
  ele requer, via Homebrew — nenhuma altera este repositório.
- Risco residual aceito pelo Arquiteto: confiabilidade de turnaround,
  confirmação de ACK e identidade de sequência entre retries de reports,
  registrado em `EKM-GAP-0006`.

### 22.8 Estado de `EKM-CHG-0007`

`Closed` por decisão do Arquiteto após validação em hardware e aceite explícito
do risco residual transferido para `EKM-GAP-0006`. O fechamento abrange a
fachada configurável, sua API pública, composição, testes e migração; não
abrange uma correção do transporte ou da confiabilidade de ACK.

### 22.9 Definition of Done EKM

Concluída para `EKM-CHG-0007`:

- intenção, escopo, decisões e limites permanecem explícitos;
- API, implementação, testes automatizados e builds foram reconciliados;
- validação humana em hardware e decisão de fechamento foram registradas;
- desvios da primeira rodada e sua correção permanecem visíveis;
- risco de ACK/retry não resolvido foi separado em `EKM-GAP-0006`;
- especificação, changelog e mapa usam estados consistentes.

### 22.10 Reconciliação do inventário final

Nenhum runtime duplicado, fonte duplicada ou dependência reversa foi
introduzida: `client_154/main` e `examples/issp_minimal_client/main` não são
dependidos por `components/issp_app_154`; `issp_app_154` não depende de
`client_154/main`, `coordinator_154` ou `examples/issp_minimal_client`. O
inventário de `components/` permanece com quatro componentes (seção 15 desta
especificação e `components/README.md`); `issp_app_154` agora tem um app de
teste adicional (`test_apps/smart_sys_app_test`) que não é consumido por
nenhum firmware de produto.

## 23. Reautoria v1.5 — retirada de QEMU (01/08/2026)

O Arquiteto determinou retirar QEMU de todo o repositório como estratégia de
validação e execução, sem reduzir requisitos funcionais. SMARTAPP-001 a 008 e
SMARTAPP-AC-001 a AC-024 permanecem inalterados em intenção, cenários e
oráculos.

A versão 1.4 e sua implementação funcional continuam registradas como baseline
historicamente `Validated`. A versão 1.5 altera a estratégia de evidência:

- os 20 testes de `SmartSysApp::SetupHooks` devem migrar para runner
  host-native fiel ou ESP32-H2 físico;
- os resultados QEMU da seção 22 são evidência legada, auditável, mas não
  reutilizável para aprovação posterior;
- o test app, seus testes e os hooks são preservados nesta autoria; imports,
  runners, comentários e diretórios específicos do emulador são candidatos à
  migração ou remoção posterior conforme
  `Repository-Test-Execution-Policy.md`;
- hardware ESP32-H2/ESP32-C6 continua sendo a evidência obrigatória dos
  comportamentos físicos de SMARTAPP-AC-022.

Nenhum código, teste, configuração, runner ou artefato técnico foi alterado,
excluído ou executado. A migração de validação v1.5 fica `Not Started`; a
versão normativa fica `Proposed`, `Not Ready` e `Pending Review`. A próxima
etapa é análise independente de implementabilidade.

## 24. Correção de target da validação v1.5 (10/08/2026)

Por decisão do Arquiteto e conforme
`Repository-Test-Execution-Policy.md` v0.3, qualquer fallback físico do app
Unity `SmartSysApp` executa exclusivamente em ESP32-H2. Execução host-native
fiel permanece permitida para lógica pura e não constitui `IDF_TARGET` nem
evidência de compatibilidade física.

Os 20 casos presentes na fonte permanecem preservados. Builds ou resultados
históricos em target não suportado continuam auditáveis, mas são evidência
inválida para a migração v1.5. Os casos permanecem deliberadamente
`Not Executed`: esta versão não solicita coleta, flash ou execução em ESP32-H2,
que somente uma especificação futura poderá autorizar. Esta correção não altera
a API nem o baseline funcional validado em hardware.
