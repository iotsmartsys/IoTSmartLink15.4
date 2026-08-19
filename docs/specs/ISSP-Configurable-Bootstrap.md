# IoTSmartSys — Especificação da API pública `SmartSysApp`

**Tipo:** Normativo
**Estado normativo:** Active
**Estado da implementação:** Validated
**Estado do workflow:** Concluída
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
revisão arquitetural e validados pelo Arquiteto em hardware, conforme o
relatório histórico de implementação. A
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
- `endpointId` permanece configurável pela composição, com identidade congelada
  por capability. A semântica de `endpointId` e `eventType` foi estendida pela
  ADR-0005 e por `Technical-Debt-Remediation.md`: o tipo identifica a natureza
  da capability e é injetado pela fachada para as capabilities abrangidas;
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

- `endpointId` continua explícito em `SwitchConfig`; `eventType` é fixado pela
  fachada conforme a ADR-0005;
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
};

}
```

Regras:

- `pin` deve ser um GPIO de saída válido;
- `activeHigh=true` deve mapear para `activeLevel=1`;
- `activeHigh=false` deve mapear para `activeLevel=0`;
- estado inicial, report inicial, endpoint e event type devem ser encaminhados
  sem alteração semântica ao behavior;
- endpoints ocupados devem ser rejeitados, independentemente do tipo de evento;
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

Os resultados QEMU preservados no relatório histórico permanecem fatos da
versão 1.4, mas não podem ser reutilizados como evidência vigente da versão
1.5 ou de revisões futuras. Os 20 cenários permanecem preservados, mas esta
versão não solicita nem autoriza sua execução. Somente especificação futura
pode reativá-los com propósito, recorte e evidência explícitos.

## 19. Ativos da implementação preservada

A implementação concluída ficou delimitada aos seguintes ativos; evolução
posterior exige novo recorte autorizado:

- `components/issp_app_154/`;
- `client_154/main/app_main.cpp` e `client_154/main/CMakeLists.txt`;
- `components/issp_app_154/src/reset/`;
- `examples/issp_minimal_client/`;
- `components/README.md`;
- `docs/specs/ISSP-Architecture.md`;
- `docs/specs/ISSP-Reusable-Components.md`;
- `docs/rfc/KNOWLEDGE-MAP.md`;
- `docs/rfc/EKOM-CHANGELOG.md`;
- esta especificação, somente para transições e resultado.

Alterar wire, persistência, identidade, endereço curto, factory reset, reports
ou comportamento da saída exige interrupção e decisão humana.

`EKM-CHG-0007` foi encerrada depois da implementação, da revisão corretiva,
dos testes automatizados, dos builds e da validação humana em hardware
registrados no relatório histórico relacionado.

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

## 21. Evidências exigidas da implementação

O relatório separado de implementação deve informar:

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

## 22. Relatórios relacionados

- implementação histórica:
  `docs/reports/configurable-bootstrap/implementation/v1.4.md`;
- análise e correção de targets: relatórios da política de execução.

## 23. Atualização v1.5 — retirada de QEMU

O Arquiteto determinou retirar QEMU de todo o repositório como estratégia de
validação e execução, sem reduzir requisitos funcionais. SMARTAPP-001 a 008 e
SMARTAPP-AC-001 a AC-024 permanecem inalterados em intenção, cenários e
oráculos.

A versão 1.4 e sua implementação funcional continuam registradas como baseline
historicamente `Validated`. A versão 1.5 altera a estratégia de evidência:

- os 20 testes de `SmartSysApp::SetupHooks` devem migrar para runner
  host-native fiel ou ESP32-H2 físico;
- os resultados QEMU do relatório histórico são evidência legada, auditável, mas não
  reutilizável para aprovação posterior;
- o test app, seus testes e os hooks são preservados nesta autoria; imports,
  runners, comentários e diretórios específicos do emulador são candidatos à
  migração ou remoção posterior conforme
  `Repository-Test-Execution-Policy.md`;
- hardware ESP32-H2/ESP32-C6 continua sendo a evidência obrigatória dos
  comportamentos físicos de SMARTAPP-AC-022.

Naquele ciclo de autoria, nenhum código, teste, configuração, runner ou
artefato técnico foi alterado, excluído ou executado, e a versão permaneceu
`Proposed / Not Ready`. A correção posterior de target e a validação do
Arquiteto, preservadas nos relatórios relacionados e na política de execução,
substituem esse estado histórico.

## 24. Target vigente da validação v1.5

Por decisão do Arquiteto e conforme
`Repository-Test-Execution-Policy.md` v0.3, qualquer fallback físico do app
Unity `SmartSysApp` executa exclusivamente em ESP32-H2. Execução host-native
fiel permanece permitida para lógica pura e não constitui `IDF_TARGET` nem
evidência de compatibilidade física.

Os 25 casos atualmente presentes na fonte permanecem preservados. Builds ou resultados
históricos em target não suportado continuam auditáveis, mas são evidência
inválida para a migração v1.5. Os casos permanecem deliberadamente
`Not Executed`: esta versão não solicita coleta, flash ou execução em ESP32-H2,
que somente uma especificação futura poderá autorizar. Esta correção não altera
a API nem o baseline funcional validado em hardware.
