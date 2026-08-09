# Variantes de firmware selecionáveis pelo `menuconfig`

**Estado normativo:** Proposed
**Estado da implementação:** In Progress
**Revisão de implementabilidade:** Implementable para a direção integral por
decisão do Arquiteto; a análise da Fase 2 foi executada e os bloqueadores B1 a
B4 foram resolvidos pelo Arquiteto
**Prontidão:** Needs Analysis — requer confronto focado das resoluções antes de
autorizar implementação

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

Confronto entre o recorte da Fase 2, o repositório vigente, a arquitetura, os
precedentes e os critérios de aceite. Esta análise não altera implementação, não
promove estado e não declara aprovação; ela informa o Arquiteto.

### Recomendação

**Retorno ao rascunho/análise** para os bloqueadores B1 a B4. Eles são
normativos: nenhuma escolha de implementação os resolve sem inventar requisito.

O restante do recorte é implementável com os precedentes existentes: a seleção no
`Kconfig`, a compatibilidade por recursos no CMake, o board histórico, a
composição do product firmware e a extensão da fachada não encontraram
impedimento estrutural. Nenhum bloqueador exige reabrir a arquitetura de
`SmartSysApp` nem o protocolo.

### Evidências confrontadas

- seleção vigente: `client_154/main/Kconfig.projbuild`,
  `client_154/main/CMakeLists.txt` (verificação sob
  `if(NOT CMAKE_BUILD_EARLY_EXPANSION)`), `client_154/sdkconfig` (dois símbolos
  default gravados);
- fronteiras vigentes: `client_154/main/app_main.cpp`,
  `product_firmware.hpp`, `firmwares/single_smart_plug.cpp`,
  `boards/board_model.hpp`, `boards/current_client_esp32h2_wiring.cpp`;
- plataforma: `components/issp_app_154/src/smart_sys_app.cpp`,
  `src/smart_sys_app_impl.hpp`, `src/smart_sys_app_hardware.cpp`,
  `include/SmartSysApp.h`;
- runtime ISSP: `components/issp_core/src/issp_device.cpp`,
  `include/idevice_behavior.hpp`, `include/issp_limits.hpp`
  (`kMaxDeviceBehaviors = 8`), `components/issp_transport_154/src/
  issp154_report_executor.cpp` e `issp154_transport.c` (tarefa `issp154_rx`);
- precedentes de observação não bloqueante:
  `components/issp_app_154/src/reset/reset_button_monitor.cpp` (tarefa estática,
  `vTaskDelay`, `esp_timer_get_time`) e `Issp154ReportExecutor` (tarefa estática
  acordada por notificação);
- behavior existente: `components/issp_behaviors/src/digital_output_behavior.cpp`
  (publica em `begin()` e em `handle()`, nunca de forma assíncrona);
- baseline histórico do sensor, confirmado em `0ff6a39^`:
  `sensor_154/main/hardware/iot154_sensor_input.c` e
  `main/config/iot154_sensor_config.h` — 5 amostras, mínimo 3 baixas, 3 ms,
  2 janelas iguais, máximo 4 janelas, 8 ms, GPIO 14 no H2, pull-up interno,
  `IOT154_SENSOR_LOGIC_ACTIVE = HIGH`, valor `HIGH → 1`; e
  `sensor_154/main/main_old.c.txt`, que mostra o fluxo real: uma amostragem por
  boot, envio e deep sleep, com detecção de mudança por wake-up EXT1;
- coordenador: `coordinator_154/main/iot154_packet.h`
  (`IOT154_EVENT_DOOR 1`) e `main/main.c` (`value == 1 ? "open" : "closed"`) —
  confirmam que o protocolo e o alvo não precisam mudar;
- consumidor externo da fachada: `examples/issp_minimal_client/main/main.cpp`
  usa apenas `addSwitchPlugCapability` e `configureFactoryResetButton`; extensão
  aditiva da API não o afeta;
- `CONFIG_FREERTOS_HZ=100` em `client_154/sdkconfig` e no `sdkconfig.esp32h2`
  histórico;
- `SOC_GPIO_PIN_COUNT = 28` no ESP32-H2: GPIO 14 é válido como entrada, e o
  hardware histórico já o usou.

### Componentes impactados

| Componente | Natureza da mudança | Observação da análise |
|---|---|---|
| `client_154/main/Kconfig.projbuild` | aditiva | duas entradas novas; ambos os boards dependem de `IDF_TARGET_ESP32H2`, logo aparecem juntos e o default permanece a tomada |
| `client_154/main/CMakeLists.txt` | aditiva + diagnóstico | listas de recursos exigidos/oferecidos e `list(REMOVE_ITEM)` bastam; a mensagem atual do board cita apenas o board da Fase 1 e precisa nomear os dois |
| `client_154/main/boards/board_model.hpp` | **substitutiva** | decisão 17 expira `relayPin` e companhia; obriga editar a tomada (ver B3) |
| `client_154/main/firmwares/single_smart_plug.cpp` | edição de adaptação | apenas renomeia o acesso ao board; valores da decisão 8 preservados |
| `client_154/main/firmwares/door_sensor.cpp` e `boards/historical_door_sensor_esp32h2_wiring.cpp` | novos | sem impedimento |
| `components/issp_behaviors` | aditiva + nova infraestrutura de teste | o componente não possui `test_apps` hoje; `DigitalOutputBehavior` nunca foi testado isoladamente |
| `components/issp_app_154` | aditiva na API, **não aditiva no interior** | ver B2 e R1 |
| `components/issp_core` | nenhuma mudança prevista | ver B2, que pode exigir decisão em contrário |

### Bloqueadores objetivos

**B1 — o debounce especificado não é realizável com o tick vigente e não é
equivalente ao baseline que a especificação invoca.**
`CONFIG_FREERTOS_HZ=100` (10 ms por tick) tanto hoje quanto no
`sdkconfig.esp32h2` histórico. O baseline usa
`vTaskDelay(pdMS_TO_TICKS(3))` e `pdMS_TO_TICKS(8)`, e ambos avaliam para **0
ticks**: o comportamento histórico real foi cinco leituras praticamente
consecutivas com um `yield` entre elas, não amostras separadas por 3 ms, e
janelas sem os 8 ms de intervalo. A Fase 2 pede as duas coisas ao mesmo tempo —
os números e a equivalência ao histórico —, e elas são incompatíveis. O
Arquiteto precisa escolher qual é o contrato:

1. equivalência literal ao histórico: manter `vTaskDelay` e assumir espaçamento
   de 0 ms com o tick vigente, ajustando o texto da Fase 2 para descrever
   contagem de amostras e janelas, não milissegundos;
2. os 3 ms e 8 ms como requisito material: exige `esp_rom_delay_us`/
   `esp_timer` — 15 ms de CPU bloqueada por janela, o que tensiona “sem
   busy-wait” das variáveis experimentais — ou elevar `CONFIG_FREERTOS_HZ` para
   1000, que é configuração compartilhada, altera a tomada já validada em
   hardware e exige autorização e revalidação;
3. outros valores, escolhidos pelo Arquiteto, coerentes com o tick.

Sem essa decisão, o critério “oscilação que não satisfaz o debounce” não tem
definição verificável e nenhum teste pode distinguir aprovação de reprovação.

**B2 — a Fase 2 cria um publicador assíncrono de reports, e o runtime declara
não suportar publicação concorrente.**
`IsspDevice` documenta em `issp_device.hpp`: “Pending-report publication,
reservation, and completion are serial; concurrent callers are not supported”.
`publishState()` altera `pendingReports_`, `pendingReportCount_` e
`nextPendingReportOrder_` sem qualquer seção crítica. Hoje os publicadores são o
contexto de `setup()` (via `IsspDevice::start()`) e a tarefa `issp154_rx`
(comando → `handle()`), enquanto `preparePendingReport`/`completePendingReport`
correm na tarefa `issp154_report_tx`. A Fase 2 exige “um novo report para cada
transição estabilizada posterior, sem exigir reboot”, com mecanismo não
bloqueante dentro do behavior: isso introduz um terceiro contexto de execução
publicando de forma rotineira, e não mais somente em resposta a comando.

Não existe no repositório um contexto compartilhado de polling que possa ser
reutilizado: `IDeviceBehavior::poll()` existe em
`idevice_behavior.hpp` mas **não é chamado por nenhum código do repositório**.
O Arquiteto precisa decidir entre:

1. autorizar serialização em `issp_core` (por exemplo `portMUX_TYPE` em
   `IsspDevice`), o que contraria o critério de fronteira “não altera
   `issp_core`” e amplia o recorte;
2. autorizar em `issp_app_154` um caminho de publicação serializado que o
   behavior use em vez de chamar `publishState()` direto, mantendo `issp_core`
   intocado e o behavior alheio a produto;
3. passar a chamar `poll()` a partir de um contexto único e já existente, o que
   transforma `poll()` em contrato ativo e é decisão arquitetural;
4. aceitar explicitamente o risco de corrida como classe preexistente,
   registrando-o como lacuna.

A análise não recomenda a opção 4 sem registro formal: a corrida deixa de ser
eventual e passa a ser o caminho normal de operação do produto.

**B3 — contradição interna entre a decisão 17 e o critério de fronteiras.**
O critério diz: “Dada uma nova variante, quando ela é adicionada, então sua
composição não altera `issp_core`, `issp_transport_154` ou uma variante
existente”. A decisão 17 determina que `relayPin` e companhia expirem nesta
fase, e a tabela de pontos afetados manda substituir os campos em
`board_model.hpp`. Como `firmwares/single_smart_plug.cpp` lê esses campos, a
Fase 2 **necessariamente edita a variante existente**. O Arquiteto precisa
escolher se o critério passa a proibir mudança de comportamento (e não de texto)
da variante existente, ou se a decisão 17 é postergada.

**B4 — a cadência de observação contínua não é especificada e não tem
precedente no histórico.**
A Fase 2 enumera como configuração “pino, polaridade, pull, endpoint, evento,
report inicial e parâmetros de debounce”, mas monitoramento contínuo precisa de
um período entre janelas de amostragem, que nenhum desses itens define. O
histórico não fornece esse valor porque não monitorava: amostrava uma vez por
boot e detectava mudança por wake-up EXT1 do deep sleep — mecanismo explicitamente
fora do escopo desta fase. Logo, o mecanismo de detecção de transição é **novo**,
não uma retomada. O precedente mais próximo é `ResetButtonMonitor`, com
`pollIntervalMs` configurável (20 ms na tomada). Falta o Arquiteto definir se o
período é parâmetro normativo do produto, com valor e latência máxima aceitável,
ou escolha do Implementador com default documentado.

### Riscos e incertezas

**R1 — “extensão aditiva” vale para a API, não para o interior de
`issp_app_154`.** `setup()` itera `switchCount_` e chama
`hooks_.registerCapability(context, index)`; `realRegisterCapability` traduz esse
índice diretamente em `switchBehaviors_[index]`, e
`hasDuplicateSwitchEndpoint()` só compara switches. A Fase 2 exige “par
endpoint/evento único entre todas as capabilities”, o que obriga um registro
unificado de behaviors e de pares endpoint/evento dentro de `Impl`. O caminho de
menor diff, e que preserva os testes vigentes (que contam
`registerCapabilityCalls`), é um `std::array<issp::IDeviceBehavior *,
kMaxDeviceBehaviors>` preenchido na ordem de adição, com `setup()` iterando esse
vetor. Também é preciso decidir o que a linha de log
`app_setup begin capabilities=%u` passa a contar. Nada disso muda operação
pública, mas o Arquiteto deve saber que o diff interno não é aditivo.

**R2 — orçamento de memória de `SmartSysApp::Impl`.**
`kImplStorageBytes = 10240` com `static_assert`, e `Impl` já contém
`hardwareStorage_` de 8192 bytes mais os três arrays de 8 posições da tomada. A
folga é da ordem de uma unidade de milhar de bytes. Replicar arrays de 8 posições
para o sensor cabe com margem estreita; **embutir uma pilha estática de tarefa no
behavior não cabe** (o precedente `ResetButtonMonitor` usa 2048 words). A falha
seria de compilação, não silenciosa, mas a implementação deve dimensionar o
sensor com o mínimo de slots necessário e manter qualquer pilha fora de `Impl`.

**R3 — dupla fonte de verdade da compatibilidade por recursos.** A decisão 16
coloca as listas de recursos no CMake, e a decisão 17 coloca o vocabulário
físico no `BoardModel` em C++. Nada mantém as duas coerentes: um board pode
declarar no CMake que oferece a entrada de contato seco e não preenchê-la em C++.
Mitigação sugerida, sem framework: `board_model.hpp` declara um acessador por
recurso e cada board define somente os que oferece — a combinação inválida falha
também na ligação, com o CMake permanecendo o diagnóstico primário exigido pelos
critérios.

**R4 — o sensor de porta fica sem caminho de reset de pareamento.** A Fase 2
determina que ele não configure factory reset. Com isso,
`realInitializePlatform` não instancia o serviço de reset e o dispositivo não tem
como esquecer o coordenador persistido; o histórico usava o botão BOOT no GPIO 9
para `iot154_storage_reset_pairing()`. O board histórico tem esse botão, mas a
decisão 15 só lhe atribui a entrada de contato seco. Consequência operacional a
aceitar explicitamente: repareamento do sensor exige apagar NVS por gravação.

**R5 — o critério de rejeição de comando admite duas implementações.**
`IsspDevice::onCommand()` chama `handle()` apenas quando `accepts()` é
verdadeiro, e devolve `Unsupported` quando nenhum behavior aceita. Tanto
`accepts() == false` quanto `accepts() == true` com `handle()` retornando
`IsspCommandResult::Unsupported` satisfazem o critério. Recomendação:
`accepts() == true` e `handle()` devolvendo `Unsupported`, para que o par
endpoint/evento continue pertencendo ao sensor e a deduplicação por sequência de
`IsspDevice` funcione. Escolha de implementação, não lacuna normativa.

**R6 — mensagem do caso negativo de target.** Com `IDF_TARGET=esp32c6` os dois
boards ficam ocultos e o `FATAL_ERROR` atual nomeia apenas “the only board model
of this phase”. O texto precisa ser atualizado para os dois boards, senão o
diagnóstico exigido pelos critérios fica incorreto.

**R7 — `client_154/sdkconfig` não muda.** Os defaults permanecem tomada e board
atual, já gravados. As validações cruzadas e negativas continuam exigindo
`-DSDKCONFIG` isolado, como na Fase 1.

### Experimentos necessários

Fatos que a leitura de código não certifica:

**E1 — fonte de níveis controlável para os testes do `DigitalInputBehavior`.**
`issp_behaviors` não tem `test_apps`; o precedente
(`components/issp_app_154/test_apps/smart_sys_app_test`, esp32c3 sob QEMU) evita
GPIO por completo usando `SetupHooks`. É preciso comprovar por experimento qual
caminho funciona: (a) configurar o pino como `GPIO_MODE_INPUT_OUTPUT` e dirigir o
nível com `gpio_set_level` sob QEMU esp32c3; ou (b) uma junção de teste no
behavior, no precedente de `SetupHooks` — struct separada, fora da configuração
normativa do produto, declaradamente não contratual. A enumeração de campos da
configuração do sensor na Fase 2 não prevê essa junção; se (a) falhar, o
Arquiteto deve autorizar (b) explicitamente.

**E2 — custo e caimento do behavior em `Impl`.** Compilar o sensor composto e
observar o `static_assert` de `kImplStorageBytes`, decidindo o número de slots
antes de escrever os testes (R2).

**E3 — comportamento real do debounce escolhido em B1.** Medir, em hardware
ESP32-H2, o espaçamento efetivo entre amostras e a latência da transição, para
que “oscilação rejeitada” tenha evidência distinguível de “report perdido”.

**E4 — as duas composições no coordenador.** O protocolo não muda, mas a
observação do evento `Door` no coordenador com o `device ID 0x15400001` só é
verificável em execução real; note que a tomada usa **o mesmo** device ID, logo o
experimento precisa garantir que os dois produtos não estejam ativos
simultaneamente na mesma rede durante a validação.

**E5 — builds e caso negativo.** As quatro combinações (duas válidas, duas
cruzadas) e o caso C6 dos dois boards só se comprovam configurando e compilando
com `SDKCONFIG` isolado.

### Classificação das lacunas

- **decisão normativa ausente:** B1, B2, B3, B4; autorização explícita para R4 e,
  se E1(a) falhar, para a junção de teste de E1(b);
- **escolha normal de implementação:** R1 (registro unificado interno), R3
  (acessadores por recurso), R5 (forma da rejeição de comando), R6 (texto do
  diagnóstico), número de slots do sensor;
- **dependência externa pendente:** nenhuma. ESP-IDF 6.0.1, o coordenador, o
  GPIO 14 do ESP32-H2 e o `event type 1` já estão disponíveis e confirmados.

### Resultado da análise

A Fase 2 é estruturalmente compatível com as fronteiras da Fase 1 e não exige
condicionais internas nos componentes, mudança de protocolo nem duplicação de
runtime — os três sintomas de falha do teste 3 da estratégia EKOM. Os
impedimentos são de contrato, não de arquitetura: dois deles (B1 e B4) vêm de o
recorte pedir comportamento contínuo a partir de um baseline de disparo único, e
os outros dois (B2 e B3) de o recorte tocar limites que os próprios critérios
declaram intocáveis. Cabe ao Arquiteto decidir B1 a B4 e a suficiência deste
confronto.

### Resoluções arquiteturais posteriores à análise

O Arquiteto considerou o confronto suficiente para decidir os bloqueadores. As
decisões abaixo substituem as lacunas normativas registradas em B1 a B4, sem
apagar a análise que as motivou:

- **B1 e B4 resolvidos:** período de 10 ms por `esp_timer`, duas janelas não
  sobrepostas de cinco amostras, maioria de três, duas classificações iguais e
  latência máxima de 150 ms. A equivalência ao histórico limita-se à topologia
  da votação, GPIO e polaridade; atrasos de 3 ms e 8 ms não são contrato;
- **B2 resolvido:** `IsspDevice` passa a proteger internamente o bookkeeping de
  pending reports com `portMUX_TYPE` ou seção crítica equivalente, mantendo
  callbacks, notificações, codificação e transporte fora da região protegida;
- **B3 resolvido:** a proibição passa a ser de mudança comportamental na
  variante existente. Adaptação mecânica de `single_smart_plug.cpp` ao novo
  vocabulário do board é autorizada com preservação integral da decisão 8;
- **R1:** registro interno de behaviors e unicidade endpoint/evento tornam-se
  unificados; o log informa o total de capabilities;
- **R3:** CMake fornece o diagnóstico primário e acessadores C++ por recurso
  fornecem defesa de ligação contra drift;
- **R4:** o board histórico oferece também o botão do GPIO 9 e o sensor configura
  factory reset por 10 segundos, com polling de 20 ms;
- **R5:** o behavior reconhece o par endpoint/evento e `handle()` devolve
  `Unsupported`;
- **E1:** está autorizada uma fonte de níveis injetável, reservada a testes e
  ausente da configuração pública de produção;
- `IDeviceBehavior::poll()` não é ativado e nenhuma tarefa ou pilha própria é
  criada pelo behavior.

Essas resoluções ainda precisam de confronto focado. Elas não constituem ordem
de implementação nem evidência de funcionamento.

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
