# ISSP — Especificação de Bootstrap Configurável

**Tipo:** Normativo
**Estado normativo:** Proposed
**Estado da implementação:** Not Started
**Versão:** 1.0
**Responsável arquitetural:** Marcelo Miranda
**Última atualização:** 23/07/2026
**Escopo:** Composição e inicialização configuráveis do runtime ISSP no client IEEE 802.15.4

---

## 1. Contexto

O runtime funcional do `client_154` já separa protocolo, transporte,
commissioning, behaviors, reports e factory reset em componentes com
responsabilidades próprias. A aplicação, porém, ainda realiza diretamente em
`main.cpp` toda a composição:

```text
configuração fixa do produto
→ construção do transporte e do network manager
→ construção do device, behaviors e report executor
→ adaptação do factory reset
→ inicialização sequencial
→ tratamento local das falhas de cada etapa
→ espera permanente
```

Esse arranjo é funcional e validado, mas faz do firmware de exemplo a única
descrição executável da ordem de inicialização. Cada novo produto que utilize os
componentes compartilhados precisaria repetir a composição, a ordem, o
tratamento de rollback e parte da observabilidade.

A `IoTSmartSysCore` utiliza como referência um modelo no qual a aplicação
configura hardware e capabilities antes de uma operação única de `setup()`. A
fachada é dona da composição do runtime e o bootstrap seleciona e executa um
fluxo explícito. O modelo desta especificação adota esses princípios, não suas
dependências de Wi-Fi, MQTT, provisioning, Arduino ou OTA.

## 2. Problema concreto

O repositório não possui um contrato único que:

- receba a identidade e os parâmetros específicos de um produto;
- aceite seus behaviors antes da inicialização;
- componha os serviços de infraestrutura ISSP;
- preserve uma ordem segura de inicialização;
- informe de modo estruturado em qual etapa uma falha ocorreu;
- desfaça somente os recursos iniciados pela tentativa corrente;
- impeça configuração tardia ou inicialização duplicada;
- possa ser reutilizado por outro firmware sem copiar `client_154/main.cpp`.

Sem esse contrato, a criação de um segundo client funcional pode produzir
ordens de boot incompatíveis, tratamentos diferentes para a mesma falha e
dependência acidental dos detalhes internos do primeiro firmware.

## 3. Objetivos

- **ISSP-BOOT-001:** fornecer uma fachada configurável e reutilizável para o
  runtime ISSP sobre IEEE 802.15.4.
- **ISSP-BOOT-002:** separar configuração de produto, composição do runtime e
  execução das etapas de inicialização.
- **ISSP-BOOT-003:** tornar explícitos o estado, a etapa e o resultado do
  bootstrap.
- **ISSP-BOOT-004:** centralizar a ordem de inicialização e o rollback dos
  recursos iniciados pelo bootstrap.
- **ISSP-BOOT-005:** preservar os componentes, contratos wire, persistência,
  factory reset e report inicial já implementados e validados.
- **ISSP-BOOT-006:** permitir que `client_154` e futuros clients configurem
  identidade, transporte e behaviors sem replicar a orquestração.
- **ISSP-BOOT-007:** manter configuração e execução determinísticas, sem
  alocação dinâmica obrigatória durante o boot.

## 4. Não objetivos

Não fazem parte deste recorte:

- reimplementar, mover ou alterar `FactoryResetService`,
  `ResetButtonMonitor` ou a pressão contínua de 10 segundos;
- criar um novo report inicial ou alterar quando
  `DigitalOutputBehavior::begin()` o publica;
- modificar payload, protocolo wire, checksum, endianness, ACKs, retries,
  timeouts ou delays funcionais;
- alterar commissioning, layout NVS, schema do descritor de rede ou política de
  persistência;
- adicionar Wi-Fi, MQTT, provisioning por portal, OTA ou dependências da
  `IoTSmartSysCore`;
- criar builders para cada tipo de behavior;
- permitir registro tardio de behaviors;
- suportar múltiplos devices ou transports no mesmo bootstrap;
- implementar `stop()` público como novo ciclo de vida operacional;
- alterar coordenador, rádio de baixo nível ou regras de negócio;
- publicar componentes em registry ou movê-los para outro repositório;
- inferir uma política nova de recuperação quando o coordenador não for
  encontrado.

## 5. Estado de referência

### 5.1 Componentes funcionais

O runtime atual é composto por:

- `Issp154Transport`, dono do rádio e do transporte IEEE 802.15.4;
- `Issp154NetworkManager`, dono do descritor persistente e do commissioning;
- `IsspDevice`, dono do despacho, behaviors e fila lógica de reports;
- `Issp154ReportExecutor`, dono da task de transmissão de reports;
- behaviors registrados pela aplicação, atualmente
  `DigitalOutputBehavior`;
- `FactoryResetService` e `ResetButtonMonitor`, pertencentes à composição
  específica do firmware.

### 5.2 Ordem vigente

`client_154/main/main.cpp` executa:

```text
inicialização da NVS
→ obtenção do endereço IEEE 802.15.4
→ construção dos componentes
→ início do monitor de factory reset
→ inicialização ou descoberta da rede
→ registro do behavior
→ início do device
→ início do executor de reports
→ espera permanente
```

Uma falha após o início do transporte solicita `transport.end()` e encerra
`app_main`. O report inicial é produzido pelo behavior durante
`IsspDevice::start()` e fica pendente até que o executor seja iniciado.

### 5.3 Limites já normativos

Continuam autoritativas:

- `docs/specs/ISSP-Architecture.md`;
- `docs/specs/ISSP-Commissioning.md`;
- `docs/specs/ISSP-Reusable-Components.md`;
- as APIs públicas classificadas em `components/README.md`.

Esta especificação adiciona uma camada de composição. Ela não transfere ao
bootstrap as responsabilidades internas desses componentes.

## 6. Decisões arquiteturais

### ISSP-BOOT-DEC-001 — Fachada compartilhada

O bootstrap deve ser exposto por um novo componente compartilhado
`issp_app_154`, localizado em:

```text
components/issp_app_154
```

Sua API principal deve ser `issp::Issp154ClientApp`. O componente pode conhecer
as APIs públicas de `issp_core` e `issp_transport_154`, mas não pode depender de
`client_154/main`, `client_154/main/reset`, coordenador ou exemplos.

### ISSP-BOOT-DEC-002 — Configuração antes de `setup()`

A aplicação deve:

1. construir `Issp154ClientApp` com uma configuração completa;
2. registrar de zero a `kMaxDeviceBehaviors` behaviors;
3. integrar serviços específicos do produto, como o monitor já existente de
   factory reset;
4. chamar `setup()` uma única vez.

Configuração e registro após o primeiro `setup()` devem ser rejeitados. Não
deve existir configuração implícita por variáveis globais mutáveis.

### ISSP-BOOT-DEC-003 — Propriedade dos objetos

`Issp154ClientApp` deve possuir, com armazenamento de duração igual à fachada:

- uma cópia validada da configuração;
- uma cópia do endereço estendido;
- `Issp154Transport`;
- `Issp154NetworkManager`;
- `IsspDevice`;
- `Issp154ReportExecutor`;
- a tabela de referências para behaviors registrados;
- estado e último resultado do bootstrap.

Os behaviors permanecem pertencentes à aplicação. A aplicação deve garantir
que eles sobrevivam à fachada e ao runtime. A fachada não deve executar
`delete`, tomar propriedade por ponteiro bruto nem alocar behaviors.

### ISSP-BOOT-DEC-004 — Fronteira de plataforma

Inicialização da NVS e obtenção do endereço IEEE por `esp_read_mac` permanecem
responsabilidades da composição de plataforma. A fachada recebe o endereço
estendido já resolvido e exige que a NVS esteja pronta antes de `setup()`.

Essa fronteira evita que uma camada de composição ISSP apague toda a NVS ao
tratar `ESP_ERR_NVS_NO_FREE_PAGES` ou `ESP_ERR_NVS_NEW_VERSION_FOUND`. A política
de recuperação global da NVS não pertence ao ISSP.

### ISSP-BOOT-DEC-005 — Factory reset por delegação

A fachada deve expor `clearPersistedNetwork()` como delegação direta para
`Issp154NetworkManager::clearPersistedNetwork()`. O `client_154` deve conectar
essa operação ao `FactoryResetService` existente por um adaptador mínimo.

A fachada não deve:

- monitorar GPIO;
- medir os 10 segundos;
- reiniciar o dispositivo;
- apagar namespaces adicionais;
- executar factory reset dentro de `setup()`.

### ISSP-BOOT-DEC-006 — Setup síncrono e resultado explícito

`setup()` deve ser síncrono e retornar `Issp154BootstrapResult`. O retorno deve
identificar sucesso, estado terminal controlado ou falha, sem exigir parsing de
logs. A chamada não deve criar uma task adicional somente para orquestrar o
bootstrap.

As tasks internas já pertencentes ao transporte, ao executor e ao monitor de
reset permanecem inalteradas.

### ISSP-BOOT-DEC-007 — Sem política nova de retry

`setup()` deve executar uma única tentativa do fluxo já definido. As tentativas
internas de commissioning e reports permanecem sob seus donos atuais. Repetir
`setup()` após falha não é um mecanismo de retry e deve ser rejeitado.

Uma futura política de reinicialização sem reboot exige nova especificação,
incluindo ciclo de vida completo e capacidade real de reiniciar todos os
componentes.

## 7. Contratos e configuração

Os nomes e campos abaixo são normativos. Ajustes puramente sintáticos exigidos
pelo compilador são permitidos somente se preservarem o contrato e forem
relatados.

### 7.1 Configuração

```cpp
namespace issp
{

struct Issp154ClientAppConfig
{
    std::uint32_t deviceId;
    std::uint16_t shortAddress;
    std::array<std::uint8_t, kIssp154ExtendedAddressSize> extendedAddress;
    bool cca;
};

}
```

Regras:

- `deviceId` não pode ser zero;
- `shortAddress` não pode ser `0xffff`;
- `extendedAddress` não pode ser todo zero nem todo `0xff`;
- `cca` deve ser encaminhado sem alteração ao transporte;
- canal e PAN ID iniciais devem ser zero, pois a rede operacional pertence ao
  descritor carregado ou descoberto;
- `coordinator` deve ser `false`;
- `promiscuous` operacional deve ser `false`; o network manager pode habilitá-lo
  temporariamente durante commissioning conforme contrato vigente;
- a configuração deve ser copiada; a fachada não pode depender da duração de
  buffers fornecidos pelo chamador.

### 7.2 Estados

```cpp
enum class Issp154ClientAppState : std::uint8_t
{
    Configuring,
    Starting,
    Running,
    NotReady,
    Failed
};
```

Transições permitidas:

```text
construção → Configuring
Configuring → Starting
Starting → Running
Starting → NotReady
Starting → Failed
```

Não existem transições de saída de `Running`, `NotReady` ou `Failed` neste
recorte. Factory reset conclui por reboot e não constitui uma transição local.

### 7.3 Etapas e resultado

```cpp
enum class Issp154BootstrapStage : std::uint8_t
{
    None,
    ValidateConfiguration,
    InitializeNetwork,
    RegisterBehaviors,
    StartDevice,
    StartReportExecutor,
    Completed
};

struct Issp154BootstrapResult
{
    Issp154ClientAppState state;
    Issp154BootstrapStage stage;
    IsspResult result;
};
```

Interpretação:

- sucesso: `Running`, `Completed`, `IsspResult::Ok`;
- rede não encontrada no commissioning:
  `NotReady`, `InitializeNetwork`, `IsspResult::NotReady`;
- configuração inválida:
  `Failed`, `ValidateConfiguration`, `IsspResult::InvalidArgument`;
- qualquer outra falha: `Failed`, etapa que falhou e resultado original sempre
  que ele puder ser preservado;
- chamada repetida de `setup()`: estado terminal vigente, `None` e
  `IsspResult::Busy`.

### 7.4 API pública mínima

```cpp
class Issp154ClientApp
{
public:
    explicit Issp154ClientApp(const Issp154ClientAppConfig &config);

    IsspResult addBehavior(IDeviceBehavior &behavior);
    Issp154BootstrapResult setup();

    Issp154ClientAppState state() const;
    Issp154BootstrapResult lastBootstrapResult() const;
    std::uint32_t deviceId() const;
    IsspTransportState transportState() const;

    IsspResult clearPersistedNetwork() const;
};
```

Contratos:

- `addBehavior()` é permitido somente em `Configuring`;
- referências duplicadas ao mesmo behavior devem retornar
  `IsspResult::InvalidArgument`;
- exceder `kMaxDeviceBehaviors` deve retornar
  `IsspResult::InvalidArgument`, preservando o enum público vigente;
- zero behaviors é configuração válida para o bootstrap; o produto pode impor
  exigência mais forte em sua própria composição;
- `setup()` deve registrar todos os behaviors aceitos ou falhar;
- `state()` e `lastBootstrapResult()` devem ser consultas não bloqueantes;
- `clearPersistedNetwork()` deve preservar exatamente o resultado do network
  manager e não deve depender de `setup()` ter sido executado;
- nenhuma dessas APIs é garantida como thread-safe neste recorte;
- configuração, registro e `setup()` devem ocorrer no mesmo contexto serial;
- `clearPersistedNetwork()` somente pode ser chamado conforme as restrições de
  concorrência já válidas para NVS e para o serviço de factory reset.

### 7.5 Dependências CMake

`issp_app_154` deve declarar:

```text
REQUIRES: issp_core, issp_transport_154
PRIV_REQUIRES: nenhuma, salvo necessidade demonstrada pela implementação
```

O componente não deve depender de `issp_behaviors`: recebe behaviors pela
interface pública `IDeviceBehavior`.

## 8. Fluxo de inicialização

### 8.1 Composição da aplicação

```text
plataforma inicializa NVS
→ plataforma obtém endereço IEEE 802.15.4
→ aplicação cria Issp154ClientAppConfig
→ aplicação cria Issp154ClientApp
→ aplicação cria e registra behaviors
→ aplicação conecta FactoryResetService existente a clearPersistedNetwork()
→ aplicação inicia ResetButtonMonitor existente
→ aplicação chama Issp154ClientApp::setup()
```

O monitor de reset deve continuar disponível mesmo quando commissioning terminar
em `NotReady`, preservando a recuperação local já implementada.

### 8.2 Execução de `setup()`

```text
validar configuração
→ mudar estado para Starting
→ initializeNetwork()
   ├── descritor válido: ativar rede
   ├── sem descritor: commissioning vigente
   └── rede não encontrada: NotReady
→ registrar behaviors no IsspDevice, na ordem de adição
→ IsspDevice::start()
   └── behaviors podem publicar o report inicial já existente
→ Issp154ReportExecutor::start()
   └── executor percebe reports pendentes
→ Running
```

`setup()` não deve aguardar ACK do report inicial. A entrega e os retries
continuam assíncronos sob `Issp154ReportExecutor`.

## 9. Responsabilidades

### `Issp154ClientApp`

Deve:

- copiar e validar a configuração;
- possuir e conectar os componentes de infraestrutura;
- aceitar e preservar a ordem de registro dos behaviors;
- executar o fluxo normativo de `setup()`;
- manter estado e resultado consultáveis;
- executar rollback limitado quando a tentativa falhar;
- delegar a limpeza do descritor persistido.

Não deve:

- interpretar protocolo wire;
- operar GPIO de produto;
- decidir regras de behavior;
- implementar commissioning, report ou factory reset;
- esconder ou converter uma falha em sucesso;
- reiniciar o dispositivo.

### Aplicação do produto

Deve:

- inicializar pré-requisitos de plataforma;
- fornecer identidade e endereço válidos;
- construir e manter vivos os behaviors;
- configurar hardware antes de `setup()`;
- integrar factory reset e outras funções específicas;
- decidir o comportamento do processo após `Running`, `NotReady` ou `Failed`;
- não acessar os componentes internos da fachada.

### Componentes existentes

`Issp154Transport`, `Issp154NetworkManager`, `IsspDevice`,
`Issp154ReportExecutor` e os behaviors preservam integralmente as
responsabilidades definidas em `ISSP-Architecture.md`.

## 10. Falhas e rollback

### 10.1 Princípios

- O primeiro erro deve determinar etapa, resultado e estado terminal.
- O rollback não pode apagar o descritor persistido.
- Falha de boot não pode disparar factory reset nem reboot automático.
- O rollback deve afetar somente recursos iniciados pela tentativa corrente.
- Erro de rollback deve ser registrado, mas não deve substituir o erro
  primário no retorno.

### 10.2 Matriz

| Falha | Estado | Ação obrigatória |
|---|---|---|
| Configuração inválida | `Failed` | Não iniciar rede nem registrar behaviors |
| Rede não encontrada | `NotReady` | Garantir transporte encerrado; preservar NVS e monitor de reset |
| Erro ao carregar, persistir ou ativar rede | `Failed` | Encerrar transporte se iniciado; preservar descritor conforme regras vigentes |
| Registro de behavior falha | `Failed` | Encerrar transporte; não iniciar device nem executor |
| `IsspDevice::start()` falha | `Failed` | Encerrar transporte; não iniciar executor |
| Executor falha ao iniciar | `Failed` | Encerrar transporte; preservar reports e erro primário |
| `transport.end()` falha no rollback | estado da falha primária | Registrar `rollback_failed` com o resultado do encerramento |

O estado interno criado por `IsspDevice::start()` não possui atualmente um
contrato de `stop()`. Por isso, falha posterior ao início do device é terminal
até reboot. A especificação não deve simular reinicialização segura.

## 11. Observabilidade

Cada tentativa deve produzir eventos estruturados, sem endereços completos,
payloads ou dados sensíveis:

```text
bootstrap begin behaviors=<n>
bootstrap stage=<stage>
bootstrap completed state=running
bootstrap completed state=not_ready stage=<stage> result=<result>
bootstrap failed stage=<stage> result=<result>
bootstrap rollback transport=<result>
```

Requisitos:

- **ISSP-BOOT-008:** os nomes de etapa dos logs devem corresponder a
  `Issp154BootstrapStage`;
- **ISSP-BOOT-009:** deve existir exatamente um evento terminal por chamada
  inicial válida de `setup()`;
- **ISSP-BOOT-010:** logs não substituem `Issp154BootstrapResult`;
- **ISSP-BOOT-011:** logs vigentes de commissioning, report executor, behavior
  e factory reset devem ser preservados;
- **ISSP-BOOT-012:** nível de log deve distinguir progresso (`INFO`), ausência
  controlada de rede (`WARN`) e falha (`ERROR`).

## 12. Compatibilidade

### 12.1 Comportamento preservado

A migração deve preservar:

- os valores configurados atualmente pelo `client_154`;
- a ordem observável do boot, exceto pela centralização dos logs;
- scan, descritor NVS e ativação da rede;
- registro do relay antes do início do device;
- publicação e entrega do report inicial;
- ACKs, comandos `ON`, `OFF`, `TOGGLE` e deduplicação;
- task e política do executor de reports;
- monitor de GPIO 9, pressão de 10 segundos e reboot do factory reset;
- encerramento controlado quando não há rede.

### 12.2 API

As APIs públicas existentes não devem ser removidas nem alteradas. A fachada é
aditiva. Consumidores podem continuar compondo os componentes diretamente,
embora clients funcionais novos devam preferir a fachada após sua validação.

### 12.3 Persistência e wire

Não há mudança de contrato persistente ou wire. Nenhuma migração de NVS é
necessária. Binários anteriores e posteriores devem reconhecer o mesmo
descritor de rede.

## 13. Migração do `client_154`

A implementação futura deve ocorrer em etapas verificáveis:

1. criar `issp_app_154` e testes de unidade/host para validação, estados,
   registro, ordem e falhas com doubles dos componentes quando necessário;
2. adicionar o componente ao diretório compartilhado e documentar sua API;
3. migrar `client_154/main.cpp` para construir a configuração, os behaviors e
   os serviços específicos do produto;
4. substituir a composição direta de transporte, network manager, device e
   executor por `Issp154ClientApp`;
5. manter a inicialização de NVS, leitura do MAC, relay e factory reset no
   composition root do produto;
6. validar equivalência de constantes e contratos;
7. construir e validar `client_154`, consumidor mínimo e coordenador;
8. executar validação em hardware dos fluxos afetados.

Não deve existir período final com dois runtimes ativos nem cópia dos
componentes. O exemplo mínimo pode continuar exercitando APIs de baixo nível,
mas deve também comprovar compilação e link da nova fachada.

## 14. Critérios de aceitação

### Contrato e estrutura

- **ISSP-BOOT-AC-001:** `issp_app_154` existe em `components/`, sem dependência
  reversa para aplicações.
- **ISSP-BOOT-AC-002:** a API pública atende integralmente à seção 7.
- **ISSP-BOOT-AC-003:** configuração inválida não inicia qualquer componente.
- **ISSP-BOOT-AC-004:** behaviors são registrados uma vez, na ordem declarada,
  antes de `IsspDevice::start()`.
- **ISSP-BOOT-AC-005:** configuração tardia, duplicação, excesso de capacidade e
  `setup()` repetido retornam os resultados especificados.

### Fluxo e falhas

- **ISSP-BOOT-AC-006:** o caminho com descritor válido termina em `Running`.
- **ISSP-BOOT-AC-007:** o caminho de commissioning bem-sucedido termina em
  `Running`.
- **ISSP-BOOT-AC-008:** rede não encontrada termina em `NotReady`, sem reboot,
  loop de scan adicional ou remoção de NVS.
- **ISSP-BOOT-AC-009:** cada falha injetada nas etapas da seção 10 produz etapa,
  estado, resultado e rollback correspondentes.
- **ISSP-BOOT-AC-010:** falha de rollback não oculta a falha primária.

### Preservação funcional

- **ISSP-BOOT-AC-011:** o relay mantém GPIO 13, endpoint 1, event type 2,
  active high, estado inicial `false` e `reportOnStart=true`.
- **ISSP-BOOT-AC-012:** o client mantém short address `0x1001` e device ID
  `0x15400001`.
- **ISSP-BOOT-AC-013:** factory reset mantém GPIO 9, active low, polling de
  20 ms e hold de 10 segundos, limpando somente o vínculo de rede e reiniciando
  após sucesso.
- **ISSP-BOOT-AC-014:** o report inicial continua sendo criado pelo behavior e
  transmitido pelo executor existente.
- **ISSP-BOOT-AC-015:** nenhuma constante de protocolo, retry, timeout, delay,
  frame, persistência ou commissioning muda.

### Evidência

- **ISSP-BOOT-AC-016:** testes automatizados cobrem configuração, estados,
  capacidade, ordem, todos os ramos de falha e rollback.
- **ISSP-BOOT-AC-017:** `client_154`, `examples/issp_minimal_client` e
  `coordinator_154` compilam para os targets e a versão ESP-IDF definidos em
  `ISSP-Reusable-Components.md`.
- **ISSP-BOOT-AC-018:** hardware comprova boot por descritor, commissioning,
  ausência de rede, comandos, ACK, report inicial e factory reset.
- **ISSP-BOOT-AC-019:** buscas e revisão do diff comprovam ausência de
  reimplementação do factory reset e do report inicial.
- **ISSP-BOOT-AC-020:** `git diff --check` e a reconciliação EKM são aprovados.

## 15. Validações obrigatórias

A execução de implementação deve registrar:

- matriz `ISSP-BOOT-001` a `ISSP-BOOT-012` e
  `ISSP-BOOT-AC-001` a `ISSP-BOOT-AC-020`;
- testes de unidade com falhas injetadas em cada etapa;
- inspeção da ordem de chamadas;
- comparação das configurações antes e depois da migração;
- prova de que `FactoryResetService`, `ResetButtonMonitor`,
  `DigitalOutputBehavior::begin()` e `Issp154ReportExecutor` não foram
  duplicados;
- builds, target, versão do ESP-IDF, warnings, tamanho e SHA-256 dos binários;
- checklist e resultados de hardware;
- revisão separada dos diffs de código, build e conhecimento;
- `git diff --check`;
- baseline inicial e reconciliação do inventário final.

## 16. Ativos de conhecimento e autorização

A implementação desta especificação, depois de aprovação humana explícita, pode
alterar somente o necessário em:

- `components/issp_app_154/`;
- `client_154/main/main.cpp` e seu `CMakeLists.txt`;
- `examples/issp_minimal_client/`, para prova de consumo;
- `components/README.md`;
- `docs/specs/ISSP-Architecture.md`, para registrar a camada implementada;
- `docs/specs/ISSP-Reusable-Components.md`, somente para adicionar o novo
  componente e suas evidências sem reescrever decisões anteriores;
- `docs/rfc/KNOWLEDGE-MAP.md`;
- `docs/rfc/EKM-CHANGELOG.md`;
- esta especificação, para transições de estado e resultado.

Não está autorizada a remoção ou condensação de decisões normativas. Necessidade
de alterar qualquer contrato wire, persistente, de factory reset ou report
inicial exige interrupção e decisão humana.

`EKM-CHG-0007` deve permanecer `Open` enquanto a especificação não estiver
aprovada e a implementação não possuir todas as evidências. A criação desta
especificação não autoriza sua implementação.

## 17. Formato obrigatório do relatório

Além do relatório exigido por `EKM-GUIDELINES.md`, informar:

1. resultado executivo;
2. baseline e aprovação humana aplicável;
3. matriz de requisitos e critérios de aceitação;
4. configuração e API públicas resultantes;
5. ordem efetiva de boot;
6. propriedade e duração dos objetos;
7. falhas e rollback validados;
8. preservação de factory reset e report inicial;
9. contratos wire, persistentes e públicos alterados ou preservados;
10. arquivos de código, build e conhecimento alterados;
11. fontes dependentes revisadas;
12. testes, builds e hardware;
13. warnings, desvios, riscos e pendências;
14. estado de `EKM-CHG-0007`;
15. resposta à Definition of Done EKM;
16. reconciliação integral do inventário final.
