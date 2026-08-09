# Variantes de firmware selecionáveis pelo `menuconfig`

**Estado normativo:** Proposed
**Estado da implementação:** In Progress
**Revisão de implementabilidade:** Implementable para a direção integral por
decisão do Arquiteto; os bloqueadores B1 a B4 da Fase 2 foram resolvidos e o
confronto focado das resoluções foi executado, sem bloqueador remanescente
**Prontidão:** Needs Analysis até o Arquiteto avaliar o confronto focado; a
análise recomenda prontidão condicionada aos esclarecimentos C1 a C3

## Missão

Permitir que um único projeto ESP-IDF produza firmwares de produtos diferentes,
selecionados no SDK Configuration Editor, sem copiar o runtime ISSP e sem
espalhar condicionais de produto pelos componentes compartilhados.

Esta mudança deve usar o experimento como prova prática da EKOM: o mapa e a
árvore de conhecimento precisam fornecer contexto suficiente para orientar a
implementação e a evolução de uma variante sem exigir redescobrir a arquitetura
do repositório.

## Resultado que buscamos

- acelerar a localização dos pontos corretos para criar ou alterar um produto;
- aumentar a qualidade mantendo produto, hardware e plataforma em fronteiras
  explícitas;
- aumentar a confiança de que uma nova variante não altera protocolo,
  commissioning ou outro produto por acidente;
- produzir um binário com uma única composição de produto e placa;
- tornar visível por que cada arquivo existe e qual direção de dependência é
  permitida.

## Pergunta central de contexto

> Para adicionar ou modificar uma variante de produto, onde devo atuar e o que
> devo preservar para não acoplar regras do produto à plataforma compartilhada?

A resposta começa na visão de domínio de `docs/rfc/KNOWLEDGE-MAP.md` e é
completada por esta especificação: escolha em `Kconfig`, composição no product
firmware, detalhes elétricos no board model, capabilities em componentes
reutilizáveis e runtime comum em `SmartSysApp`/`issp_*`.

## Estado atual observado

> Esta seção descreve o baseline anterior à Fase 1, usado pela análise de
> implementabilidade. O estado vigente está em “Resultado da implementação da
> Fase 1”.

Antes da Fase 1, `client_154/main/main.cpp` concentrava o entrypoint e a
composição do único produto existente. Ele definia a identidade do dispositivo,
GPIO do relé, GPIO e tempos do factory reset, endpoint e tipo de evento;
registrava uma capability de tomada e iniciava `SmartSysApp`.

`components/issp_app_154` já fornece a fachada compartilhada `SmartSysApp` e
compõe internamente core, behavior, transporte, commissioning, reports e reset.
Os componentes `issp_core`, `issp_transport_154` e `issp_behaviors` já possuíam
fronteiras CMake e contratos públicos. Não existia `Kconfig.projbuild` no
`client_154`, catálogo de boards ou pasta de variantes.

Esse estado forneceu um bom ponto de corte: a mudança extraiu de `main.cpp`
somente a composição específica do produto; não reabriu a arquitetura interna
validada de `SmartSysApp` ou dos componentes ISSP.

## Escopo

- uma seção `IoTSmartLink15.4` no `menuconfig`;
- escolha exclusiva de um product firmware;
- escolha exclusiva de um board model compatível;
- composição em build somente da variante e do board escolhidos;
- entrypoint mínimo, sem regras de produto;
- módulos separados para product firmwares e definições de board;
- uso das operações vigentes e extensão aditiva de `SmartSysApp` somente para
  a capability concreta da Fase 2;
- compatibilidade explícita entre product firmware, board model e
  `IDF_TARGET`;
- primeira migração do produto atual de tomada simples como prova de
  preservação;
- segunda variante `Door sensor` e segundo board ESP32-H2 como prova de
  evolução da composição e da compatibilidade por recursos físicos;
- espaço arquitetural para tomada dupla + luz e sensor de presença, sem
  incluí-los nesta entrega.

### Fase 1 — recorte funcional inicial

A primeira implementação funcional contém somente:

- product firmware `Single smart plug`, selecionado por padrão;
- board model descritivo `Current client ESP32-H2 wiring`, selecionado por
  padrão e compatível exclusivamente com `IDF_TARGET=esp32h2`;
- migração da composição atual para as novas fronteiras, sem alterar a
  plataforma compartilhada.

ESP32-C6 não é target suportado desse board nesta fase. Configurar esse board
com `IDF_TARGET=esp32c6` deve falhar com diagnóstico explícito da
incompatibilidade, antes de produzir um firmware utilizável.

A Fase 1 comprova a seleção, as fronteiras e a preservação do produto atual. Ela
não encerra o experimento de múltiplas variantes: esse encerramento exige uma
segunda composição escolhida pelo Arquiteto e o teste 3 da estratégia EKOM.

### Fase 2 — segunda composição: sensor de porta

A segunda variante é `Door sensor`. A escolha é sustentada por dois fatos do
repositório: o coordenador vigente já interpreta o evento `Door` (`event type
1`) e o histórico do antigo `sensor_154` contém a fiação e o comportamento de
entrada usados como referência. O histórico informa contexto; este recorte é o
contrato vigente.

A Fase 2 contém:

- product firmware `Door sensor`, com device ID `0x15400001`, endpoint 1,
  event type 1, `1 = open`, `0 = closed` e report inicial habilitado;
- board model descritivo `Historical door sensor ESP32-H2 wiring`, compatível
  somente com `IDF_TARGET=esp32h2`;
- entrada de contato seco no GPIO 14, pull-up interno e estado lógico ativo em
  nível alto, e botão de usuário no GPIO 9, ativo em nível baixo;
- amostragem periódica a cada 10 ms, em janelas não sobrepostas de cinco
  amostras; cada janela é classificada pela maioria de três níveis e o estado é
  confirmado após duas janelas consecutivas com a mesma classificação;
- latência máxima de 150 ms entre a estabilização física da entrada e a
  confirmação da transição pelo behavior;
- report inicial do estado estabilizado e um novo report para cada transição
  estabilizada posterior, sem exigir reboot;
- `DigitalInputBehavior` reutilizável em `components/issp_behaviors` e a
  operação pública `SmartSysApp::addDoorSensorCapability()`;
- configuração do sensor contendo pino, polaridade, pull, endpoint, evento,
  report inicial, período de 10 ms e parâmetros de debounce; a variante combina
  os valores do board com suas constantes de produto;
- as mesmas regras de ciclo de vida da fachada vigente: capability adicionada
  antes de `setup()`, par endpoint/evento único entre todas as capabilities e
  falha observável por retorno e `lastConfigurationResult()`;
- ausência de tratamento de comandos pelo sensor de porta: comandos dirigidos
  a esse endpoint/evento são reconhecidos pelo behavior, retornam `Unsupported`
  e não alteram a entrada;
- factory reset pelo botão de usuário, com retenção de 10 segundos e polling de
  20 ms, como no produto vigente;
- seleção e build exclusivos da tomada ou do sensor, nunca dos dois no mesmo
  binário.

O mecanismo de observação usa `esp_timer` periódico dentro do behavior, sem
busy-wait, tarefa ou pilha próprias e sem ativar globalmente
`IDeviceBehavior::poll()`. O product firmware fornece semântica, parâmetros e
composição; o board fornece pino, pull e polaridade elétrica.

Bateria, ADC, deep sleep, wake-up por GPIO e métricas de consumo existentes no
histórico não pertencem a esta Fase 2. A retomada desses comportamentos exige
recorte próprio para não confundir a prova de variantes com a reconstrução do
produto de baixo consumo.

## Fora de escopo

- implementar qualquer variante nesta etapa de especificação;
- alterar protocolo wire, transporte, commissioning, ACK, retry, formato ou
  pipeline de reports, NVS ou o comportamento interno do factory reset; a Fase
  2 apenas compõe o serviço vigente com os parâmetros já validados;
- selecionar ou mudar o chip alvo do ESP-IDF pelo `menuconfig`;
- tornar componentes conscientes de produto ou placa;
- carregar duas variantes no mesmo binário ou selecionar produto em runtime;
- criar geração de código, sistema próprio de plugins ou framework genérico de
  boards;
- definir nomes comerciais de placas que ainda não estejam confirmados;
- alterar o coordenador, publicar firmware, fazer deploy ou merge na `main`;
- restaurar bateria, ADC, deep sleep, wake-up por GPIO ou métricas do antigo
  `sensor_154`.

## Conceitos e fronteiras

| Conceito | Responsabilidade | Conhece | Não conhece |
|---|---|---|---|
| Plataforma compartilhada | ciclo de vida da aplicação, ISSP, transporte, commissioning, reports, persistência e reset | contratos técnicos e infraestrutura | modelo comercial, combinação de features ou pinagem de um produto |
| Componente reutilizável | uma capability ou behavior isolado e configurável, como saída ou entrada digital | seu contrato, configuração e abstrações da plataforma | qual produto o usa e qual opção do `menuconfig` o selecionou |
| Product firmware / variant | identidade e composição de capabilities e serviços que definem um produto | APIs públicas, contrato do board e regras do próprio produto | detalhes privados do ISSP, rádio ou outra variante |
| Board model | pinagem, polaridade, recursos físicos e compatibilidade com o chip alvo | propriedades elétricas da placa | protocolo, regras do produto e fluxo do `menuconfig` |
| Seleção de build | escolhe exatamente uma composição válida e informa o CMake | símbolos de variante, board e `IDF_TARGET` | estado em runtime ou lógica interna dos componentes |

`ESP32-H2` e `ESP32-C6` são targets/chips, não nomes suficientes de board
model. Um board model deve representar uma placa concreta e declarar com qual
target é compatível. Até os nomes reais serem confirmados, a implementação deve
usar um identificador descritivo para a fiação atual do client, sem inventar um
nome comercial.

Para a Fase 1, o identificador descritivo representa a fiação vigente do
`client_154` e declara somente ESP32-H2 como target compatível. Suporte futuro
ao ESP32-C6 requer board model próprio ou ampliação explícita da compatibilidade,
acompanhada de validação correspondente.

## Visão do domínio

A árvore do repositório, o diagrama que conecta client e coordenador e o ramo
proposto de variantes estão em `docs/rfc/KNOWLEDGE-MAP.md`. Esse mapa é a porta
de entrada para localizar o client dentro do sistema e navegar por suas
responsabilidades; esta especificação permanece responsável pelo comportamento
da seleção, pelas decisões, restrições e critérios de aceite.

## Fluxo de seleção

```mermaid
flowchart LR
    Target["idf.py set-target"] --> Config["menuconfig"]
    Config --> Product["Choose product"]
    Config --> Board["Choose compatible board"]
    Product --> CMake["Select source composition"]
    Board --> CMake
    CMake --> Binary["One firmware binary"]
```

O `menuconfig` não muda `IDF_TARGET`. Ele apenas impede ou rejeita combinações
incompatíveis com o target já configurado pelo ESP-IDF.

## Decisões e restrições arquiteturais

1. `Kconfig` escolhe a composição do build; ele não controla lógica interna de
   componentes. Símbolos `CONFIG_*` não devem aparecer em `components/issp_*`.
2. Cada build contém exatamente um product firmware e um board model.
3. O CMake traduz a escolha em um conjunto de fontes. Não se compila todas as
   variantes para decidir em runtime.
4. Condicionais de seleção ficam concentradas em `Kconfig.projbuild` e no
   `CMakeLists.txt` do componente `main`. Não se permite uma árvore de `#ifdef`
   dentro de classes de produto ou componentes.
5. `app_main()` apenas obtém a composição selecionada, inicia a aplicação e
   registra o resultado; identidade, capabilities e regras do produto ficam na
   variante.
6. O product firmware recebe uma definição de board por contrato, sem usar
   números de GPIO próprios. O board não instancia capabilities nem inicia a
   plataforma.
7. Capabilities comuns continuam configuráveis e reutilizáveis. Uma nova
   capability só entra em `components/` quando tiver contrato próprio e utilidade
   além da composição de uma única variante.
8. A migração da tomada simples deve preservar os valores atuais: device ID
   `0x15400001`, relé no GPIO 13 ativo em nível alto, reset no GPIO 9 ativo em
   nível baixo por 10 segundos com polling de 20 ms, endpoint 1, event type 2,
   estado inicial desligado e report inicial habilitado.
9. A compatibilidade board/target deve falhar na configuração ou no build com
   mensagem clara. Não se aceita binário silenciosamente configurado para a
   placa errada. Na Fase 1, o board atual aceita somente ESP32-H2; ESP32-C6 é o
   caso negativo obrigatório.
10. A primeira implementação deve ser pequena. Abstrações adicionais só serão
    criadas quando uma segunda variante demonstrar a necessidade.
11. A forma interna do contrato de seleção não é uma decisão arquitetural
    antecipada. A implementação deve usar o menor mecanismo local que satisfaça
    o entrypoint mínimo e a seleção pelo CMake, sem criar abstração transversal.
12. A partir da segunda variante ou do segundo board, cada product firmware deve
    declarar os recursos físicos de que depende e cada board model deve declarar
    os recursos que oferece. A seleção de build deve rejeitar combinações
    incompatíveis com diagnóstico claro antes de produzir o binário.
13. A forma atual de `BoardModel` é local e provisória para a Fase 1, não um
    contrato estável. Os campos `relayPin`, `relayActiveHigh`,
    `factoryResetButtonPin` e `factoryResetButtonActiveLow` podem ser
    substituídos na Fase 2, quando a segunda variante revelar o vocabulário de
    recursos físicos necessário; não devem ser generalizados antecipadamente.
14. A direção integral foi promovida a `Implementable` pelo Arquiteto antes de
    a segunda variante ser definida, como variável deliberada do experimento.
    O recorte concreto da Fase 2 agora exige nova análise antes da implementação;
    a promoção anterior não substitui esse confronto.
15. Na Fase 2, produto e board são compatíveis por classes e quantidades de
    recursos físicos. `Single smart plug` exige uma saída digital e um botão de
    usuário; `Door sensor` exige uma entrada para contato seco e um botão de
    usuário. O board atual oferece a saída e o botão; o board histórico do
    sensor oferece a entrada e o botão.
16. Cada ramo de seleção no CMake declara uma lista de recursos exigidos pelo
    produto ou oferecidos pelo board. O CMake calcula os recursos ausentes e
    rejeita a composição quando a diferença não é vazia. O diagnóstico deve
    nomear produto, board e recurso ausente. Essa validação pertence à
    composição do build; não autoriza símbolos `CONFIG_*` dentro dos componentes
    nem tenta extrair metadados do código C++.
17. O vocabulário físico de `BoardModel` passa a distinguir saída digital,
    botão de usuário e entrada de contato seco. Campos orientados ao produto,
    como `relayPin`, expiram nesta fase; a representação C++ concreta deve ser
    a menor que expresse esses recursos sem criar um framework genérico.
18. `DigitalInputBehavior` é genérico quanto a produto e evento. Ele estabiliza
    uma entrada, publica estado inicial e transições e reconhece seu par
    endpoint/evento para responder `Unsupported` a comandos. A fachada dá a
    semântica pública de sensor de porta; o behavior não conhece `menuconfig`,
    board ou nome de produto.
19. O protocolo wire e o coordenador não mudam: `event type 1` e os valores
    aberto/fechado já são compreendidos pelo alvo coordenador.
20. O debounce da Fase 2 não reivindica equivalência temporal ao código
    histórico. Preserva a votação por cinco amostras e maioria de três, agora
    com período normativo de 10 ms, janelas não sobrepostas, duas classificações
    consecutivas iguais e latência máxima observável de 150 ms.
21. `DigitalInputBehavior` usa um `esp_timer` periódico para observar a entrada.
    A callback deve ser curta e não bloqueante; não se cria tarefa ou pilha por
    behavior e `IDeviceBehavior::poll()` permanece fora deste recorte.
22. `IsspDevice` passa a garantir segurança entre publicação, reserva e
    conclusão de pending reports com `portMUX_TYPE` interno ou seção crítica
    equivalente. Somente o bookkeeping compartilhado fica protegido; callback,
    notificação, codificação e transporte executam fora da seção crítica. O
    comentário que exige chamadores seriais deve ser atualizado para o novo
    contrato.
23. A preservação de uma variante existente é comportamental. Alterações
    mecânicas necessárias para migrar `single_smart_plug.cpp` ao vocabulário
    físico do board são permitidas, desde que todos os valores da decisão 8 e o
    comportamento observado permaneçam inalterados.
24. `issp_app_154` mantém um registro unificado de behaviors e de pares
    endpoint/evento, independentemente do tipo de capability. `setup()` registra
    esse vetor na ordem de adição e o log de capabilities informa o total de
    capabilities configuradas.
25. `DigitalInputBehavior` pode receber uma fonte de níveis injetável em
    construção reservada a testes, análoga a `SetupHooks`. A configuração
    pública do produto continua usando GPIO real e não expõe essa junção.
26. O CMake permanece como diagnóstico primário de compatibilidade. O contrato
    C++ fornece um acessador por classe de recurso e cada board define somente
    os recursos que oferece, produzindo também falha de ligação se os metadados
    divergirem, sem introduzir geração de código ou framework de boards.

### Registro de conhecimento deste experimento

Por decisão do Arquiteto, este experimento não abre nem atualiza uma transação
`EKM-CHG`. O conhecimento material da implementação deve ser reconciliado de
forma concisa nesta especificação e em `docs/rfc/KNOWLEDGE-MAP.md`; o Git
preserva a linhagem técnica. Essa exceção vale somente para este experimento e
não modifica a governança geral do repositório.

## Experiência proposta no `menuconfig`

```text
IoTSmartLink15.4
├── Product firmware
│   ├── Single smart plug (default)
│   └── Door sensor (Fase 2)
└── Board model
    ├── Current client ESP32-H2 wiring (default)
    └── Historical door sensor ESP32-H2 wiring (Fase 2)
```

Dual smart plug + light e motion sensor permanecem fora do `choice` até
possuírem composição compilável e decisão arquitetural aplicável. O menu não
deve oferecer uma seleção que inevitavelmente falhe por ausência de código.

## Estrutura proposta de arquivos

```text
client_154/
├── CMakeLists.txt
└── main/
    ├── Kconfig.projbuild
    ├── CMakeLists.txt
    ├── app_main.cpp
    ├── product_firmware.hpp
    ├── firmwares/
    │   ├── single_smart_plug.cpp
    │   └── door_sensor.cpp
    └── boards/
        ├── board_model.hpp
        ├── current_client_esp32h2_wiring.cpp
        └── historical_door_sensor_esp32h2_wiring.cpp

components/
├── issp_app_154/
├── issp_behaviors/
│   └── digital_input_behavior.{hpp,cpp}
├── issp_core/
└── issp_transport_154/
```

Somente arquivos de variantes e boards realmente suportados devem existir. A
árvore mostra o destino do conhecimento e não autoriza criar stubs vazios.

## Pontos reais do código afetados pela Fase 2

| Ponto atual | Mudança esperada na Fase 2 | Preservação obrigatória |
|---|---|---|
| `client_154/main/Kconfig.projbuild` | adicionar as escolhas de sensor e board histórico | escolha exclusiva e ausência de lógica funcional |
| `client_154/main/CMakeLists.txt` | selecionar as novas fontes e validar requisitos contra recursos oferecidos | uma variante, um board e diagnóstico antes do binário |
| `client_154/main/firmwares/door_sensor.cpp` | compor identidade, endpoint, evento e debounce do sensor | nenhuma pinagem literal ou lógica de transporte |
| `client_154/main/boards/board_model.hpp` | substituir campos orientados ao relé por recursos físicos | preservar a composição da tomada e evitar framework genérico |
| `client_154/main/boards/historical_door_sensor_esp32h2_wiring.cpp` | declarar entrada de contato seco e botão de usuário | nenhuma regra de produto ou protocolo |
| `components/issp_behaviors` | adicionar `DigitalInputBehavior` e observação por `esp_timer` | reutilizável, sem produto, board, `CONFIG_*`, tarefa ou pilha própria |
| `components/issp_app_154` | expor `addDoorSensorCapability()` e unificar o registro interno | não expor tipos privados do ISSP nem mudar as operações vigentes |
| `components/issp_core` | serializar o bookkeeping dos pending reports | nenhuma regra de produto; callbacks e transporte fora da seção crítica |
| testes de `issp_behaviors` e `issp_app_154` | cobrir estabilização, reports, rejeição de comandos e regressão | doubles devem preservar leitura e transição material |
| `docs/rfc/KNOWLEDGE-MAP.md` | marcar sensor e board como especificados | apontar para este contrato sem duplicá-lo |

## Critérios de aceite

### Seleção e build

- **Dado** um `IDF_TARGET` suportado, **quando** o desenvolvedor abre o SDK
  Configuration Editor, **então** encontra uma seção `IoTSmartLink15.4` com uma
  escolha de product firmware e uma escolha de board model.
- **Dado** o menu de produto com duas ou mais variantes implementadas, **quando**
  uma variante é selecionada a partir da Fase 2, **então** nenhuma segunda
  variante pode permanecer selecionada.
- **Dado** um par produto/board válido, **quando** o projeto é configurado e
  compilado, **então** somente as fontes desse produto e desse board entram no
  binário.
- **Dadas** as combinações `Single smart plug` + board atual e `Door sensor` +
  board histórico, **quando** cada build H2 é configurado, **então** ambas são
  aceitas e as escolhas default continuam sendo a tomada e o board atual.
- **Dado** o board atual da Fase 1 e `IDF_TARGET=esp32c6`, **quando** a
  configuração ou o build é executado, **então** a combinação é impedida com
  diagnóstico claro de que esse board aceita somente ESP32-H2.
- **Dado** um product firmware cujos recursos exigidos não são oferecidos pelo
  board selecionado, **quando** a configuração ou o build é executado a partir
  da Fase 2, **então** a combinação é impedida com diagnóstico que identifica o
  produto, o board e o recurso ausente.

### Fronteiras

- **Dada** uma nova variante, **quando** ela é adicionada, **então** sua
  composição não altera protocolo nem `issp_transport_154`; `issp_core` muda
  somente para garantir publicação concorrente segura e variantes existentes
  admitem adaptações mecânicas sem mudança de comportamento.
- **Dado** o código compartilhado, **quando** ele é inspecionado, **então** não
  há símbolo `CONFIG_*` de seleção de produto ou board em `components/issp_*`.
- **Dado** um product firmware, **quando** sua composição é inspecionada,
  **então** ele usa o contrato do board e não contém GPIO literal.
- **Dado** o entrypoint, **quando** ele é inspecionado, **então** não contém
  pinagem, identidade, endpoints ou lista de capabilities de um produto.

### Preservação do produto atual

- **Dada** a seleção da tomada simples e do board que representa a fiação
  atual, **quando** o firmware inicia, **então** identidade, relé, reset,
  endpoint, evento, estado inicial e report inicial são configurados com os
  mesmos valores do baseline observado.
- **Dada** a migração estrutural, **quando** os testes e builds vigentes são
  executados, **então** as operações vigentes de `SmartSysApp`, o protocolo e o
  comportamento compartilhado permanecem compatíveis; a API muda somente pela
  adição da capability de sensor de porta.

O primeiro cenário exige validação no hardware ESP32-H2. Comparação estática ou
build isolado não substituem a observação do firmware em execução.

### Sensor de porta

- **Dada** a composição `Door sensor` com o board histórico, **quando** o
  firmware inicia com a entrada estabilizada em nível alto, **então** publica
  endpoint 1, evento 1 e valor 1 (`open`).
- **Dada** a mesma composição com a entrada estabilizada em nível baixo,
  **quando** o firmware inicia, **então** publica endpoint 1, evento 1 e valor 0
  (`closed`).
- **Dado** o firmware em execução, **quando** a entrada muda e satisfaz o
  debounce especificado, **então** publica uma vez o novo estado sem reboot e
  em até 150 ms após a estabilização física.
- **Dada** uma oscilação que não satisfaz o debounce, **quando** a entrada volta
  ao estado anterior, **então** nenhum novo report é publicado.
- **Dado** um estado já publicado, **quando** novas leituras estabilizam no
  mesmo valor, **então** nenhum report duplicado é criado.
- **Dado** um comando para endpoint 1 e evento 1, **quando** ele chega ao sensor
  de porta, **então** o behavior reconhece o par, devolve `Unsupported`, não
  altera a entrada e não cria report de mudança.
- **Dado** o sensor pareado, **quando** o botão de usuário permanece pressionado
  por 10 segundos, **então** o factory reset remove o pareamento e permite novo
  commissioning após reboot.
- **Dadas** publicação de uma transição e transmissão de reports em contextos
  concorrentes, **quando** pending reports são publicados, reservados e
  concluídos, **então** contagem, ordem, geração e conteúdo permanecem íntegros,
  sem report perdido, duplicado ou slot corrompido.
- **Dada** a seleção da tomada com o board do sensor ou do sensor com o board da
  tomada, **quando** o build é configurado, **então** ele falha antes de gerar
  binário e identifica respectivamente os recursos físicos ausentes.

Os três primeiros cenários exigem validação no hardware ESP32-H2. Debounce,
supressão de duplicatas e rejeição de comandos também devem possuir evidência
automatizada com uma fonte de níveis controlável.

### Navegabilidade EKOM

- **Dados** o mapa de conhecimento e esta especificação, **quando** alguém
  recebe a tarefa de adicionar o sensor de porta, **então** consegue identificar
  onde registrar a seleção, onde compor o produto, onde definir a placa e o que
  não deve alterar.
- **Dada** uma dúvida sobre a localização de uma regra, **quando** se aplica a
  regra de navegação da árvore, **então** ela conduz a uma única fronteira
  principal: seleção, product firmware, board, componente ou plataforma.

## Estratégia de validação do experimento EKOM

O experimento será avaliado durante a primeira implementação, não pelo volume
do documento.

1. **Teste de orientação:** entregar a pergunta “como adicionar o sensor de
   porta?” usando somente o mapa e esta especificação. A resposta deve apontar
   os pontos de extensão e as fronteiras preservadas sem varredura ampla do
   repositório.
2. **Teste de mudança controlada:** migrar primeiro a tomada simples. O diff
   deve permanecer concentrado nos pontos reais listados acima e não alterar a
   plataforma compartilhada.
3. **Teste de segunda composição:** implementar o sensor de porta usando o
   product firmware, o board histórico, a capability pública e o behavior
   reutilizável definidos nesta Fase 2. Se isso exigir condicionais internas
   nos componentes, alteração do protocolo ou duplicação de runtime, a
   arquitetura ou o mapa deve voltar ao Arquiteto.
4. **Teste de classificação:** avaliar onde seriam colocados “GPIO da placa”,
   “composição de capabilities do sensor” e “retry do rádio”. A árvore deve
   conduzir respectivamente a board model, product firmware e plataforma.
5. **Teste de seleção:** inspecionar os artefatos de build para confirmar uma
   única variante e um único board, além de executar um caso de incompatibilidade
   board/target.
6. **Teste de preservação:** comparar a composição da tomada simples com os
   valores de baseline e executar o conjunto de validação definido abaixo.

### Conjunto de validação da Fase 1

- `git diff --check` e inspeção do diff contra a tabela de pontos afetados;
- configuração gerada contendo exatamente o product firmware e o board default;
- inspeção de `compile_commands.json` ou `build.ninja` comprovando que somente
  as fontes selecionadas entram no build;
- build do `client_154` para ESP32-H2 com ESP-IDF 6.0.1;
- build de `examples/issp_minimal_client` para ESP32-H2;
- execução da suíte de testes de `SmartSysApp` em QEMU ESP32-C3;
- build de `coordinator_154` para ESP32-C6;
- configuração ou build negativo do board da Fase 1 com target ESP32-C6,
  usando `SDKCONFIG` isolado fora da configuração H2 rastreada e contendo o
  diagnóstico esperado;
- validação no hardware ESP32-H2 de boot até `Running`, report inicial, comandos
  `ON`, `OFF` e `TOGGLE`, pressão de factory reset por 10 segundos, reboot e
  retorno ao commissioning. Essa execução comprova a preservação da migração e
  não declara resolvida a lacuna preexistente de ACK/retry (`EKM-GAP-0006`).

### Conjunto de validação da Fase 2

- `git diff --check` e inspeção contra os pontos afetados desta fase;
- builds H2 isolados das duas combinações válidas, com inspeção das fontes
  selecionadas;
- configuração negativa das duas combinações cruzadas, sem produção de
  binário e com diagnóstico do recurso ausente;
- caso negativo ESP32-C6 para os dois boards, sempre com `SDKCONFIG` isolado;
- testes automatizados do `DigitalInputBehavior`, com fonte de níveis injetável,
  cobrindo período de 10 ms, janelas, maioria, latência, estado inicial,
  transição, supressão de duplicatas, falha de publicação e comando
  `Unsupported` reconhecido pelo par;
- testes de concorrência do `IsspDevice` exercitando publicação, reserva e
  conclusão intercaladas e confirmando a integridade dos pending reports;
- testes da adição de `addDoorSensorCapability()` e regressão integral da suíte
  vigente de `SmartSysApp`;
- build de `examples/issp_minimal_client` e de `coordinator_154`, sem mudança
  funcional nesses consumidores;
- hardware da tomada simples repetindo a preservação da Fase 1;
- hardware do sensor ESP32-H2 comprovando boot até `Running`, report inicial
  aberto e fechado, transições nos dois sentidos sem reboot, ausência de report
  por oscilação rejeitada, latência máxima de 150 ms, factory reset e evento
  correspondente observado no coordenador.

Para cada item, falha, execução não iniciada ou resultado desconhecido não
constitui aprovação. A evidência deve permitir distinguir aprovação, reprovação
e ausência de execução.

O experimento é útil se reduzir a redescoberta, tornar os limites previsíveis e
permitir explicar o diff antes de escrevê-lo. Ele falha se o implementador ainda
precisar vasculhar o repositório para descobrir responsabilidades, se duas
fronteiras disputarem a mesma regra ou se o menu apenas esconder um firmware
monolítico cheio de condicionais.

## Variáveis experimentais para encerrar o experimento

- nome e revisão comerciais do board podem substituir o identificador
  descritivo somente após confirmação do Arquiteto;
- a representação C++ concreta dos recursos continua local à implementação,
  limitada pelo vocabulário e pelo mecanismo de compatibilidade das decisões
  15 a 17 e 26;
- o número de slots reservado ao sensor deve ser o mínimo comprovado pelo build,
  sem ampliar preventivamente `kImplStorageBytes`;
- a implementação deve registrar se a extensão aditiva de `SmartSysApp` e a
  compatibilidade por recursos confirmaram as fronteiras ou revelaram contexto
  ainda ausente no mapa.

Essas variáveis não autorizam implementação antes do confronto focado das
resoluções arquiteturais. O experimento permanece aberto até a Fase 2 produzir
evidência e o Arquiteto avaliar seu resultado.

## Resultado desta etapa

O Arquiteto aprovou o sensor de porta como segunda composição. A análise de
implementabilidade confirmou a direção estrutural e identificou B1 a B4. O
Arquiteto resolveu esses bloqueadores e autorizou as mudanças compartilhadas
descritas nas decisões 20 a 26. O Consultor reconciliou as decisões nesta
especificação e no mapa, sem iniciar implementação. O recorte requer confronto
focado antes de ser promovido para implementação.

O Arquiteto confirmou o registro documental produzido pelo Consultor e
autorizou seu commit e envio à branch do experimento. Essa confirmação não
representa aprovação da implementação nem substitui o confronto focado da Fase
2.

## Decisão vigente de implementabilidade

O Arquiteto mantém a direção integral como `Implementable`. Para a Fase 2, B1 a
B4 estão normativamente resolvidos, mas o estado permanece `Needs Analysis` até
um Analista confrontar especificamente essas resoluções com o repositório. Isso
não altera a conclusão da Fase 1 nem declara a Fase 2 implementada ou validada.

## Análise de implementabilidade da Fase 2 (Engenheiro Analista)

Confronto focado das resoluções normativas das decisões 20 a 26 com o
repositório vigente, os precedentes e os critérios de aceite. Esta análise não
altera implementação, não promove estado e não declara aprovação; ela informa o
Arquiteto.

A rodada anterior levantou os bloqueadores B1 a B4. O Arquiteto os resolveu nas
decisões 20 a 26 e nos critérios correspondentes. Esta rodada verifica se cada
resolução é sustentada pelo código, e registra o que ela cria de novo.

### Recomendação

**Prontidão recomendada**, condicionada a três esclarecimentos pequenos (C1 a
C3). Nenhum bloqueador normativo permanece: as quatro resoluções são realizáveis
com o repositório vigente e removem as contradições da rodada anterior.

C1 e C3 afetam a aceitação da evidência, não o início da implementação: são
critérios com mais de uma leitura possível. C2 é um número derivável que os
testes precisam fixar. Nenhum deles exige reabrir arquitetura.

### Verificação de cada resolução

**Decisão 20 e 21 — debounce de 10 ms por `esp_timer`: sustentada.**
`esp_timer` é baseado em alarme de hardware com resolução de microssegundos e
não é quantizado pelo tick do FreeRTOS, logo o período de 10 ms é realizável com
`CONFIG_FREERTOS_HZ=100` — que era exatamente a causa de B1. A callback padrão
executa na tarefa `esp_timer` (`esp_timer_create_args_t::dispatch_method`,
default de tarefa), com prioridade `ESP_TASK_TIMER_PRIO = ESP_TASK_PRIO_MAX - 3`,
acima da tarefa `issp154_rx` (`tskIDLE_PRIORITY + 4`) e da
`issp154_report_tx` (`tskIDLE_PRIORITY + 3`). Consequência a registrar: a
amostragem **sempre** preempta o executor de reports, inclusive no meio do
bookkeeping — o que confirma a necessidade da decisão 22 e não é evitável por
ordenação.

A latência de 150 ms é aritmeticamente consistente com o esquema especificado.
Janelas não sobrepostas de cinco amostras a cada 10 ms ocupam 50 ms de
escalonamento. Pior caso: a transição ocorre logo após a primeira amostra da
janela contaminada (40 ms restantes), seguida de duas janelas limpas
(50 + 50 ms) = **140 ms**. A margem para jitter de despacho do `esp_timer`, para
a leitura e para a chamada de publicação é de cerca de 10 ms. É viável, mas é
margem estreita — ver C1.

Requisito concreto de build: `components/issp_behaviors/CMakeLists.txt` hoje
declara apenas `issp_core` e `esp_driver_gpio`. `esp_timer` **não** está entre os
requisitos comuns do ESP-IDF — `issp_app_154` precisa listá-lo explicitamente em
`PRIV_REQUIRES` —, então `issp_behaviors` passará a exigi-lo.

**Decisão 22 — serialização em `IsspDevice`: sustentada, com consequência
arquitetural a reconhecer.**
`components/issp_core/CMakeLists.txt` não declara **nenhum** `REQUIRES` e nenhum
arquivo de `issp_core` inclui FreeRTOS: hoje o componente é C++ puro sobre seus
próprios cabeçalhos. Introduzir `portMUX_TYPE` dá a `issp_core` sua primeira
dependência de porta FreeRTOS/ESP-IDF. `freertos` está entre os requisitos
comuns do ESP-IDF, portanto a compilação não deve exigir mudança de CMake, mas a
propriedade “target-agnostic” do componente passa a valer só dentro do ESP-IDF. A
decisão 22 admite “seção crítica equivalente”; a análise considera o spinlock a
escolha correta para um alvo single-core com três contextos publicando, e apenas
registra que o Arquiteto está aceitando essa dependência.

Restrições concretas que a implementação precisa respeitar, todas verificáveis no
código atual:

- `publishState()` chama `notifyPendingReport()` **dentro** do corpo que altera
  os slots; a decisão 22 exige notificar fora da seção crítica, logo o método
  precisa decidir sob lock e notificar depois;
- `processingCommand_` e `reportNotificationDeferred_` participam da mesma
  decisão e são escritos pela tarefa `issp154_rx` em `onReceive()` e
  `finishCommandProcessing()`; ficam de fora da proteção se apenas os slots forem
  protegidos, e o adiamento da notificação volta a ser corrida;
- `preparePendingReport()` hoje codifica o payload e **depois** incrementa
  `reportSequence_`. Como a codificação deve ficar fora da seção crítica, a
  ordem precisa mudar: reservar slot e tomar a sequência sob lock, codificar
  fora, e devolver o slot com `completePendingReport(token, false)` se a
  codificação falhar — esse retorno reentra na seção crítica, que portanto não
  pode ser recursiva nem estar retida;
- `normalizePendingReportOrders()` é O(n²) sobre 8 slots e roda dentro do
  bookkeeping; em `portENTER_CRITICAL` isso desabilita interrupções por dezenas
  de microssegundos. Aceitável, mas é o trecho mais longo da seção crítica e vale
  medir;
- `publishReport()` também incrementa `reportSequence_` e **não é chamado por
  nenhum código do repositório**. Deixá-lo sem proteção contradiz o novo
  contrato; a recomendação é protegê-lo ou removê-lo junto com a mudança;
- a callback do `esp_timer` **não** pode usar despacho por ISR: `publishState()`
  passaria a rodar em contexto de interrupção e `portENTER_CRITICAL` seria a
  variante errada. A decisão 21 já exige callback curta; o despacho por tarefa
  precisa ser explícito na implementação.

**Decisão 23 — preservação comportamental: resolve B3.**
O critério de fronteiras foi reescrito de forma coerente com a decisão: admite
adaptação mecânica de `single_smart_plug.cpp` e restringe `issp_core` à
publicação concorrente. A contradição da rodada anterior desapareceu.

**Decisão 24 — registro unificado: resolve R1 e é implementável com diff
pequeno.** `setup()` itera `switchCount_` e `realRegisterCapability()` traduz o
índice em `switchBehaviors_[index]`. Um `std::array<issp::IDeviceBehavior *,
kMaxDeviceBehaviors>` preenchido na ordem de adição, iterado por `setup()` e
indexado pelo hook, satisfaz a decisão preservando a assinatura
`registerCapability(void *, std::size_t)` e, com ela, os testes vigentes que
contam `registerCapabilityCalls` e verificam a ordem de registro. A unicidade
endpoint/evento passa a ser verificada contra o registro unificado, substituindo
`hasDuplicateSwitchEndpoint()`.

**Decisão 25 — fonte de níveis injetável: resolve E1 e tem precedente exato.**
`SmartSysApp::SetupHooks` é o precedente de junção de teste declaradamente não
contratual, com construtor dedicado. Como `addDoorSensorCapability()` constrói o
behavior internamente, a junção só é alcançável ao instanciar
`DigitalInputBehavior` diretamente no app de teste de `issp_behaviors` — a
configuração pública do produto continua com GPIO real, como a decisão exige.

**Decisão 26 — acessadores por recurso: resolve R3 e é verificável.**
Hoje `selectedBoard()` é o único ponto de acesso ao board e
`firmwares/single_smart_plug.cpp` é seu único consumidor, então a substituição
por um acessador por classe de recurso é local. A falha de ligação como segunda
linha de defesa funciona: um board que declare no CMake oferecer um recurso e não
defina o acessador correspondente não liga.

**Decisão 15 revisada e factory reset do sensor: coerentes.**
Com o sensor exigindo entrada de contato seco **e** botão de usuário, as duas
combinações cruzadas continuam produzindo exatamente um recurso ausente cada
(tomada no board do sensor: saída digital; sensor no board atual: entrada de
contato seco), o que preserva o diagnóstico exigido. O botão histórico está no
GPIO 9, o mesmo pino e a mesma polaridade já validados em hardware para o
factory reset, e `configureFactoryResetButton()` aceita esses valores sem
mudança. Não há conflito com a entrada no GPIO 14. Isso resolve o risco de
repareamento levantado como R4 na rodada anterior.

### Esclarecimentos a confirmar

**C1 — onde os 150 ms são medidos.**
O escopo da Fase 2 fixa o limite “entre a estabilização física da entrada e a
confirmação da transição pelo behavior”, mas o conjunto de validação lista
“latência máxima de 150 ms” junto dos itens observados no coordenador. As duas
leituras não são equivalentes: publicação enfileira em `pendingReports_`, e a
entrega ao coordenador passa por `sendConfirmed` com timeout de ACK de 50 ms e
retry de 1000 ms (`issp154_report_executor.cpp`), que estouram o orçamento por
motivos alheios ao debounce. Recomendação: medir na fronteira do behavior, com
log instrumentado, e deixar a observação no coordenador como evidência
funcional sem prazo. Some-se a isso a margem de apenas 10 ms sobre o pior caso
aritmético de 140 ms: se o Arquiteto quiser o limite medido ponta a ponta, ou
com folga para jitter, o número precisa subir.

**C2 — qual oscilação é rejeitada, em números.**
O critério “oscilação que não satisfaz o debounce” é autorreferente e os testes
precisam de um limiar. Ele é derivável do esquema especificado: para confirmar um
estado são necessárias duas janelas consecutivas com maioria de três, e a menor
perturbação capaz de produzir isso vai da terceira amostra de uma janela à
terceira da janela seguinte — **50 ms sustentados**. Abaixo disso, nenhuma
oscilação pode gerar report; acima, pode. Recomendação: registrar 50 ms como o
limiar derivado, para que o teste de rejeição use um pulso comprovadamente curto
(por exemplo 30 ms) e o de aceitação um pulso comprovadamente longo.

**C3 — o report inicial é publicado dentro de `begin()` ou pelo primeiro ciclo
do timer.**
O critério diz “quando o firmware inicia … publica”, e o escopo pede “report
inicial do estado estabilizado”. Estabilizar exige duas janelas, isto é ~100 ms.
`begin()` é chamado por `IsspDevice::start()`, dentro do estágio `StartDevice`,
na tarefa do `app_main`. As duas leituras possíveis têm consequências
observáveis diferentes:

1. amostrar de forma síncrona em `begin()`: atrasa `setup()` em ~100 ms e publica
   antes de `Running`, como faz `DigitalOutputBehavior` hoje; o período de 10 ms
   equivale a exatamente um tick com `CONFIG_FREERTOS_HZ=100`, então até
   `vTaskDelay` serve para as janelas iniciais;
2. deixar o primeiro report para o timer: `begin()` retorna `Ok` sem publicar e o
   report inicial aparece pouco depois de `Running`.

A análise recomenda a leitura 1, por ser a que preserva a semântica do report
inicial já validada na tomada e por dar ao critério um instante verificável.

### Riscos e incertezas remanescentes

**R1 — ciclo de vida do `esp_timer` dentro do behavior.**
`IDeviceBehavior` não tem `end()` nem qualquer caminho de parada, e
`DigitalOutputBehavior::begin()` trata falha de publicação anulando
`publisher_` e retornando erro. O behavior de entrada precisa garantir que
nenhum timer permaneça ativo quando não há publicador válido: em produção as
instâncias vivem em `SmartSysApp::Impl` e nunca são destruídas, mas o app de
teste construirá e destruirá muitas, e `setup()` pode falhar depois de
`begin()` e disparar `rollbackTransport`. Sem parada e remoção no destrutor e no
caminho de falha, a callback dispara sobre objeto destruído ou publica em
publicador nulo.

**R2 — o que os testes de concorrência podem provar.**
O conjunto de validação pede “testes de concorrência do `IsspDevice`
exercitando publicação, reserva e conclusão intercaladas”. Um teste
determinístico intercalando as três operações comprova a integridade da máquina
de estados; ele **não** comprova exclusão mútua, que continua sustentada por
inspeção e pelo tipo de seção crítica escolhido. Um teste com duas tarefas em
alvo single-core aumenta a confiança, mas não é prova. Recomendação: registrar
essa distinção na evidência, para não converter “sem falha observada” em “corrida
ausente”.

**R3 — infraestrutura de teste nova em dois componentes.**
Nem `issp_behaviors` nem `issp_core` possuem `test_apps`; o único precedente é
`components/issp_app_154/test_apps/smart_sys_app_test`, esp32c3 sob QEMU com
`MINIMAL_BUILD` e Unity. A Fase 2 exige cobertura automatizada nos dois. É custo
de implementação previsível, não impedimento, mas é maior do que a tabela de
pontos afetados sugere.

**R4 — orçamento de memória de `Impl`.**
`kImplStorageBytes = 10240` com `static_assert`, e `Impl` já contém
`hardwareStorage_` de 8192 bytes mais os três arrays de oito posições da tomada.
A decisão 21 remove o problema maior ao proibir pilha por behavior: o que sobra
no behavior é a configuração, o estado do debounce e um `esp_timer_handle_t`. As
variáveis experimentais já mandam usar o mínimo de slots comprovado pelo build,
o que é a mitigação correta; a falha, se houver, é de compilação.

**R5 — o log de capabilities e a suíte vigente.**
A decisão 24 muda o total informado por `app_setup begin capabilities=%u`. A
validação em hardware da Fase 1 usou esse log como evidência (`capabilities=1`).
A comparação de preservação da tomada deve considerar que o número passa a
contar capabilities configuradas de qualquer tipo — para a tomada continua 1.

**R6 — texto do diagnóstico de target.**
Com `IDF_TARGET=esp32c6` os dois boards ficam ocultos e o `FATAL_ERROR` atual de
`main/CMakeLists.txt` cita “the only board model of this phase”. O texto precisa
nomear os dois boards, senão o diagnóstico exigido pelo caso negativo fica
incorreto.

### Experimentos necessários

- **E1 — compilação de `issp_core` com seção crítica**, confirmando que
  `freertos` como requisito comum basta e que nenhum consumidor (incluindo o app
  de teste sob QEMU esp32c3) quebra;
- **E2 — despacho e jitter do `esp_timer` a 10 ms no ESP32-H2**, medindo o
  espaçamento real das amostras e a latência da transição contra o orçamento de
  C1;
- **E3 — fonte de níveis injetável em execução**, comprovando que a junção da
  decisão 25 cobre período, janelas, maioria, latência, duplicatas e falha de
  publicação sem GPIO real;
- **E4 — `static_assert` de `kImplStorageBytes`** com o número de slots
  escolhido para o sensor;
- **E5 — as quatro combinações de seleção e o caso negativo C6 dos dois
  boards**, com `SDKCONFIG` isolado;
- **E6 — as duas composições contra o coordenador.** O protocolo não muda, mas
  note que sensor e tomada compartilham o device ID `0x15400001`: a validação
  precisa garantir que os dois produtos não estejam ativos na mesma rede
  simultaneamente.

### Classificação das lacunas

- **decisão normativa ausente:** nenhuma que impeça a implementação. C1 e C3 são
  escolhas de leitura de critério que o Arquiteto deve confirmar antes de a
  evidência ser aceita; C2 é a fixação de um número derivado;
- **escolha normal de implementação:** forma da seção crítica e reordenação de
  `preparePendingReport()`, ciclo de vida do timer (R1), número de slots do
  sensor, texto do diagnóstico (R6), forma dos acessadores de recurso;
- **dependência externa pendente:** nenhuma. ESP-IDF 6.0.1, `esp_timer`, o
  GPIO 14 e o GPIO 9 do ESP32-H2, o `event type 1` e o coordenador estão
  disponíveis e confirmados.

### Resultado da análise

As resoluções das decisões 20 a 26 são sustentadas pelo repositório e coerentes
entre si. B1 deixou de existir porque `esp_timer` não depende do tick; B2 passou
a ter mecanismo autorizado e localizado; B3 foi resolvido tornando a preservação
comportamental; B4 recebeu período, janelas e limite de latência normativos. O
recorte continua não exigindo condicionais internas nos componentes, mudança de
protocolo nem duplicação de runtime.

O que a Fase 2 cria de novo, e que o Arquiteto deve reconhecer, é uma
dependência de porta FreeRTOS em `issp_core`, um contexto de execução de alta
prioridade publicando reports a cada 10 ms e infraestrutura de teste em dois
componentes que ainda não a possuem. Cabe ao Arquiteto confirmar C1 a C3, a
suficiência deste confronto e a promoção do recorte.

## Resultado da implementação da Fase 1 (Engenheiro Implementador)

**Estado da Fase 1:** Implementação concluída. Código, validações automatizáveis
e a validação em hardware ESP32-H2 exigida pelos critérios de preservação estão
executados; o Arquiteto declarou a execução em hardware aceitável.

A especificação integral permanece Em andamento [`In Progress`]: a Fase 2 e o
teste 3 da estratégia EKOM continuam sem implementação e sem evidência. A
conclusão da Fase 1 não representa a especificação integral como implementada.

### Estrutura implementada

```text
client_154/main/
├── Kconfig.projbuild                        menu IoTSmartLink15.4
├── CMakeLists.txt                           traduz a escolha em fontes
├── app_main.cpp                             entrypoint mínimo
├── product_firmware.hpp                     contrato de seleção
├── firmwares/single_smart_plug.cpp          composição do produto atual
└── boards/
    ├── board_model.hpp                      contrato elétrico
    └── current_client_esp32h2_wiring.cpp    fiação atual, somente ESP32-H2
```

`client_154/main/main.cpp` foi removido; nenhum arquivo de variante ou board não
suportado foi criado. `client_154/CMakeLists.txt` e os componentes `issp_*` não
foram alterados.

### Decisões de implementação

- o contrato de seleção é uma única função livre,
  `client154::startSelectedProductFirmware()`, que devolve o `SetupResult` da
  fachada. É o menor mecanismo local que satisfaz o entrypoint mínimo e a
  decisão 11; nenhuma classe base, registro ou abstração transversal foi criada;
- `client154::BoardModel` transporta apenas pino e polaridade do relé e do botão
  de reset. Tempo de retenção do reset, identidade, endpoint, tipo de evento,
  estado inicial e report inicial permanecem no product firmware. Essa forma e
  seus nomes orientados ao produto são provisórios da Fase 1, conforme a decisão
  13;
- a seleção do board depende de `IDF_TARGET_ESP32H2` no `Kconfig`; quando nenhum
  board compatível existe para o target, `main/CMakeLists.txt` emite
  `FATAL_ERROR` nomeando o target e a incompatibilidade. O board ainda carrega um
  `#error` defensivo para o caso de ser compilado por outro caminho;
- as verificações de seleção ficam dentro de `if(NOT CMAKE_BUILD_EARLY_EXPANSION)`
  porque os símbolos do próprio menu ainda não existem na expansão inicial de
  requisitos do ESP-IDF;
- o `TAG` de log do entrypoint passou de `iot154_switch` para `iot154_client`,
  já que o entrypoint deixou de pertencer a um produto. As duas mensagens
  (`ISSP runtime did not start...` e `ISSP runtime started`) e a sequência
  configuração → `setup()` foram preservadas literalmente;
- as validações desta implementação usaram builds isolados por `-DSDKCONFIG` e
  não alteraram configuração versionada. Posteriormente,
  `client_154/sdkconfig` recebeu os símbolos default de produto e board e passou
  a representar a configuração H2 vigente.

### Evidências executadas

ESP-IDF v6.0.1, builds isolados fora da árvore do repositório.

| Item do conjunto de validação | Resultado |
|---|---|
| `git diff --check` | sem erro; diff restrito a `client_154/main/` |
| configuração gerada contém exatamente o produto e o board default | `CONFIG_IOTSMARTLINK154_PRODUCT_SINGLE_SMART_PLUG=y` e `CONFIG_IOTSMARTLINK154_BOARD_CURRENT_CLIENT_ESP32H2_WIRING=y` |
| somente as fontes selecionadas entram no build | `compile_commands.json` e `build.ninja` contêm apenas `app_main.cpp`, `firmwares/single_smart_plug.cpp` e `boards/current_client_esp32h2_wiring.cpp` |
| build `client_154` ESP32-H2 | sucesso, 0 warnings |
| build `examples/issp_minimal_client` ESP32-H2 | sucesso, 0 warnings |
| testes `SmartSysApp` em QEMU ESP32-C3 | 20/20 `PASS`, 0 `FAIL` |
| build `coordinator_154` ESP32-C6 | sucesso, 0 warnings |
| build isolado `client_154` ESP32-H2 após manutenção pré-Fase 2 | sucesso com `-DSDKCONFIG` temporário; produto e board default selecionados, sem alterar `client_154/sdkconfig` |
| caso negativo board/target isolado após manutenção pré-Fase 2 | build ESP32-C6 com `-DSDKCONFIG` temporário falha na configuração com “No board model selected in the IoTSmartLink15.4 menu for IDF_TARGET=esp32c6…”, sem produzir binário e sem alterar `client_154/sdkconfig` |
| ausência de `CONFIG_*` de produto ou board em `components/issp_*` | confirmada por varredura; o único uso remanescente é `CONFIG_IDF_TARGET_*` do ESP-IDF |
| validação em hardware ESP32-H2 | executada pelo Arquiteto e declarada aceitável |

#### Validação em hardware ESP32-H2

Executada pelo Arquiteto em placa física e declarada aceitável por ele, que
detém a autoridade sobre a suficiência das evidências. O Implementador não
operou o hardware; registra abaixo o que o log entregue comprova diretamente e o
que se apoia na observação direta do Arquiteto.

Binário observado: ESP-IDF v6.0.1, projeto `sensor_154` — nome de projeto
preexistente do `client_154`, não um firmware de outro produto —, versão de app
`ff3a003`, o commit da Fase 1. Entre `ff3a003` e a versão vigente houve apenas
documentação e a gravação, em `client_154/sdkconfig`, dos dois símbolos default
de produto e board, que o `Kconfig` já aplicava por padrão no build validado. O
binário validado é, portanto, materialmente equivalente ao vigente.

Comprovado diretamente pelo log:

- boot até `Running` pelo caminho novo: `app_setup begin capabilities=1
  factory_reset=configured`, estágios 1 a 6 e `app_setup completed
  state=running`;
- entrypoint mínimo emitindo `iot154_client: ISSP runtime started`, com a
  sequência configuração → `setup()` preservada;
- report inicial com os valores de baseline: `DIGITAL_OUTPUT_START:
  initial_report endpoint=1 event=2 value=0 result=0`, isto é, endpoint 1,
  event type 2, estado inicial desligado e report inicial habilitado;
- factory reset configurado com os valores de baseline: `RESET_BUTTON:
  initialized gpio=9 hold_ms=10000`;
- commissioning persistido recarregado e rádio ativo, com duas recepções
  posteriores validadas em `COMMAND_ORIGIN`.

Apoiado na observação direta do Arquiteto, fora do trecho de log entregue: os
comandos `ON`, `OFF` e `TOGGLE`, a pressão de factory reset por 10 segundos, o
reboot e o retorno ao commissioning. O device ID `0x15400001` e o relé no GPIO
13 ativo em nível alto também não aparecem no trecho; permanecem comprovados
apenas pela inspeção estática de `firmwares/single_smart_plug.cpp` e
`boards/current_client_esp32h2_wiring.cpp` contra a decisão 8.

Esta execução comprova a preservação da migração e não declara resolvida a
lacuna preexistente de ACK/retry (`EKM-GAP-0006`).

O conjunto de testes automatizados de `SmartSysApp` possui hoje 20 casos, não os
19 registrados quando a especificação foi escrita; o arquivo de teste não foi
alterado por esta atuação. Sob QEMU, o app de teste continua exibindo o resumo
`0 Tests 0 Failures` do Unity e um panic após o retorno de `app_main()`; ambos
são comportamentos preexistentes do app de teste e não decorrem desta mudança. O
resultado material são os 20 casos executados individualmente com `PASS`.

### Pendências desta implementação

- o teste 3 da estratégia EKOM permanece sem evidência até o sensor de porta da
  Fase 2 ser implementado e validado;
- `EKM-GAP-0006` permanece aberta e não é afetada por esta mudança.

### Manutenção concluída antes da Fase 2

- `client_154/sdkconfig` permanece como a única configuração rastreada e como a
  configuração H2 vigente; as três cópias inertes foram removidas;
- os builds de validação H2 e do caso negativo C6 usam `SDKCONFIG` temporário,
  sem reescrever a configuração rastreada;
- a pinagem deixou de ser duplicada no help do Kconfig e permanece somente no
  arquivo do board;
- o arquivo do board inclui `sdkconfig.h` explicitamente;
- a varredura dos consumidores conhecidos no código-fonte local sob
  `/source/IoT` não encontrou ferramenta de host que filtre pela antiga tag
  `iot154_switch`; as únicas ocorrências remanescentes estão nesta especificação
  e em um worktree histórico, sem participação no build atual.
