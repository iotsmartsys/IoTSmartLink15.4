# Análise de implementabilidade da v0.4 — deep sleep do client

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/Client-Deep-Sleep.md`

**Revisão confrontada:** v0.4, Draft de 11/08/2026

**Estado:** Concluído

**Capacidade:** Engenheiro Analista

**Data:** 11/08/2026

**Resultado:** Implementabilidade recomendada com uma decisão arquitetural
pendente e dois ajustes de recorte; nenhuma execução realizada

> Este relatório registra evidências e recomendações. Não altera a fonte
> normativa, não promove estado e não autoriza implementação ou testes.

## 1. Recorte e método

A seção 8 da v0.4 determina que a nova análise confronte as relações da seção 2,
o contrato de quiescência, o alcance do renome e a capacidade de observar o
deadline durante operação bloqueante. Este relatório cobre os quatro pontos e
reexamina os critérios de aceitação afetados. Substitui, para a v0.4, as
conclusões dos relatórios de 11/08/2026 sobre a v0.3, que permanecem válidos
como registro histórico das versões que descrevem.

Nenhum build, teste, flash ou execução em hardware foi realizado. A árvore
estava limpa em `spec/client-deep-sleep`.

## 2. Relações declaradas na seção 2

| Relação declarada | Verificação |
|---|---|
| Altera `ISSP-Configurable-Bootstrap.md` v1.5 | Sustentada. Versão e estado conferem; §5 exclui `stop()` público e retry de `setup()`, e SMARTAPP-DEC-004A exige lifetime estático até o reboot. A v0.4 nega ambos e preserva o lifetime, então a relação `Amends` cobre exatamente a diferença. |
| Altera `Firmware-Variants-Menuconfig.md` | Sustentada quanto ao mérito; o alcance tem uma omissão material — ver 5. |
| Preserva `ISSP-Architecture.md` v1.2 | Sustentada. Versão confere. A exclusão da linha 244 é condicional: "novo ciclo de vida, incluindo `stop()`, **sem caso de uso aprovado**". A promoção desta especificação é o caso de uso aprovado; sem promoção, a exclusão continua ativa. |
| Preserva `ISSP-Commissioning.md` v1.0 | Sustentada com ressalva de mecanismo — ver 4.3. Canais, tentativas, validação e persistência não mudam; o que muda é a possibilidade de interromper o ciclo. |
| Preserva ADR-0002 | Sustentada. `required_resources`/`offered_resources` já são o mecanismo do CMake e acomodam `wake_led` sem estrutura nova. |
| Preserva `Repository-Test-Execution-Policy.md` v0.4 | Sustentada. Versão confere e nada nesta análise foi executado. |

### 2.1 Autoridade impactada e não declarada

`docs/specs/ISSP-Reusable-Components.md` v1.1 (Active, Validated) não aparece na
seção 2 e é materialmente atingida. Ela classifica como APIs públicas de alto
nível do `issp_transport_154` justamente `Issp154Transport`,
`Issp154NetworkManager` e `Issp154ReportExecutor`, e classifica
`issp_behaviors` como API pública. E fecha a seção com:

> "Qualquer ampliação material da API exige interrupção e decisão
> arquitetural." (linhas 215-216)

A quiescência da v0.4 exige operações novas em pelo menos dois desses contratos
(ver 3). Isso é **decisão normativa ausente**, não escolha de implementação: ou
a seção 2 passa a declarar `Amends` também sobre `ISSP-Reusable-Components.md`,
ou o Arquiteto registra que operações internas de quiescência não constituem
ampliação material. `ISSP-Consolidation.md` v1.0 repete a exclusão condicional
de `stop()` (linha 450) e é coberta pela mesma condição de caso de uso aprovado
que a arquitetura; não exige declaração adicional.

## 3. Contrato de quiescência

A §6 determina que a quiescência "encerra ou estabiliza as tasks e o transporte
pelos contratos existentes". A verificação do repositório mostra que os
contratos existentes **não são suficientes**:

| Elemento a estabilizar | Contrato hoje | Falta |
|---|---|---|
| `Issp154Transport` | `end()` público e já usado em rollback (`issp154_transport.cpp:373`) | nada |
| `Issp154ReportExecutor` | apenas `start()` e `processOne()`; a task faz `ulTaskNotifyTake(portMAX_DELAY)` e retry com `vTaskDelay(1000 ms)` sem saída (`issp154_report_executor.cpp:129-155`) | operação terminal e ponto de observação entre iterações |
| `DigitalInputBehavior` (producer) | `stopAndDeleteTimer()` é **privado**, alcançável só pelo destrutor ou por caminhos internos de falha; `IDeviceBehavior` não declara parada | operação de parada acessível à fachada |
| `IsspDevice` | `start()` sem contraparte; nada fecha admissão de novos reports | fechamento de admissão, se a v0.4 exigir garantia além da parada dos producers |
| `ResetButtonMonitor` | apenas `start()`; a task faz laço infinito com `vTaskDelay(pollIntervalMs)` (`reset_button_monitor.cpp:78`) | observação do deadline ou aceitação explícita de que ele não é encerrado |

Consequências:

- a frase "pelos contratos existentes" é **imprecisa** e deve ser corrigida para
  "pelos contratos de encerramento, criando as operações internas necessárias",
  sob pena de o Implementador ler a §6 como proibição de criar operação nova;
- o `DigitalInputBehavior` é o caso decisivo. O producer do `door_sensor` é
  periódico: amostra a cada 10 ms e pode publicar a qualquer instante. Sem
  pará-lo, "os producers do boot terminaram e não podem admitir novo report"
  (§6) é inatingível e DEEPSLEEP-AC-006 não é verificável. Como o destrutor é
  proibido por SMARTAPP-DEC-004A, a parada tem de ser uma operação explícita;
- o `ResetButtonMonitor` não precisa ser encerrado para a correção do sleep — o
  deep sleep reinicia o firmware — mas mantém o pull configurado no GPIO do
  botão, o que é matéria de corrente e não de contrato. A v0.4 não normatiza
  retenção de GPIO; registrar isso como experimento é suficiente.

Escopo classificado: criar as operações é **escolha normal de implementação**;
autorizá-las diante de `ISSP-Reusable-Components.md` é **decisão normativa**
(2.1).

## 4. Observação do deadline sob operação bloqueante

A §6 exige mecanismo capaz de observar o prazo "independentemente do retorno de
`app_main()` ou de uma etapa bloqueante de `setup()`". A verificação delimita o
problema em três classes, com limites medidos por leitura das constantes:

### 4.1 Bloqueio em `Running` — limitado e cooperativo

O executor bloqueia em `sendConfirmed` por até 100 ms de TX
(`kPhysicalTxTimeoutMs`) mais 50 ms de espera de ACK
(`kReportAckTimeoutMs`), e depois em `vTaskDelay(1000 ms)` entre tentativas. Todo
bloqueio é limitado; uma verificação cooperativa entre iterações observa o
deadline com atraso máximo de aproximadamente 1,15 s. Não exige preempção.

### 4.2 Bloqueio em `setup()` — limitado e cooperativo por canal

A varredura de commissioning percorre canais 11 a 26
(`issp154_network_manager.cpp:150-152`), com três tentativas por canal e, por
tentativa, até 100 ms de TX e 120 ms de espera de resposta
(`issp154_transport.cpp:441-490`). O pior caso por canal é cerca de 660 ms e a
varredura completa sem coordenador fica na ordem de 10,6 s, mais o custo de
`begin()`/`end()` do rádio por canal. Portanto:

- uma verificação cooperativa entre canais observa o deadline com atraso máximo
  de aproximadamente 0,7 s;
- `maxAwakeTimeMs` menor que o pior caso da varredura produz sleep antes de
  qualquer chance de commissioning. Isso é comportamento admissível pela §6, mas
  o produto precisa conhecer a ordem de grandeza. Recomenda-se que a
  especificação registre esse pior caso como nota informativa, sem transformá-lo
  em requisito de valor mínimo.

### 4.3 Preempção real — resíduo não verificável por leitura

Resta o caso em que nenhuma verificação cooperativa é alcançada a tempo e um
contexto supervisor precisa dormir enquanto outra task está dentro de uma
operação atômica. O caso material é `persistNetwork()`, que grava o descritor em
NVS dentro da varredura (`issp154_network_manager.cpp:186-190`). Entrar em deep
sleep durante uma escrita de NVS não é comprovável por inspeção e pode
comprometer o descritor persistido — o que atingiria a preservação declarada de
`ISSP-Commissioning.md`. Delimitação recomendada:

- verificação cooperativa como mecanismo primário, nos pontos limitados de 4.1 e
  4.2;
- contexto supervisor (`esp_timer` ou task dedicada) apenas como backstop, com
  exclusão explícita da janela de escrita em NVS;
- experimento obrigatório antes de aceitar DEEPSLEEP-AC-007, conforme a própria
  §8 já determina.

Classificação: **risco técnico com experimento pendente**, não bloqueador
normativo. O mecanismo primário é implementável com os pontos de observação já
existentes.

## 5. Alcance do renome

A varredura do repositório encontra referências ao produto em: `CMakeLists.txt`,
`Kconfig.projbuild`, `client_154/sdkconfig`, `Firmware-Variants-Menuconfig.md`,
`SYSTEM-DOSSIER.md:22`, `KNOWLEDGE-MAP.md:55` e três relatórios históricos. A
lista da §5 cobre fonte, símbolo, rótulo, CMake, documentação normativa, mapa e
dossiê, e preserva corretamente os registros históricos.

Omissão material: **`client_154/sdkconfig`**, artefato gerado e versionado, cita
`CONFIG_IOTSMARTLINK154_PRODUCT_DOOR_SENSOR` na linha 949. A linha está
comentada (`is not set`), logo não há quebra funcional nem seleção afetada, mas
o arquivo só volta a ser coerente com o Kconfig por regeneração em build — e
build não é autorizado por esta especificação. A §5 deve declarar se o artefato
é atualizado junto com o renome ou permanece defasado até a primeira execução
autorizada. É ajuste de recorte, não bloqueador.

A `Firmware-Variants-Menuconfig.md` cita a variante em pelo menos dez pontos
(linhas 76, 101, 109, 262, 349, 371, 396, 419, 459, 665), incluindo cenários de
aceite; a relação `Amends` da seção 2 é necessária e suficiente para cobri-los.

## 6. Oráculo de entrega e composição

- **DEEPSLEEP-AC-006 é verificável.** `pendingReportCount()` conta slots
  ocupados inclusive `inFlight`, e o decremento ocorre apenas em
  `completePendingReport(..., delivered=true)`
  (`issp_device.cpp:238-268`). A afirmação da §6 de que "a contagem inclui slots
  reservados e transmissões em andamento" corresponde ao código. A validade do
  oráculo depende inteiramente da parada dos producers (3).
- **Falha não retryable** interrompe o laço do executor
  (`issp154_report_executor.cpp:148-151`) e conserva o slot ocupado, exatamente
  como a §6 descreve; o caminho só termina em sleep pelo deadline. Comportamento
  coerente e testável com doubles.
- **Composição.** A `Door Sensor Battery H2` usa hoje GPIO 14 (contato seco) e
  GPIO 9 (botão); o GPIO 13 está livre, e DEEPSLEEP-AC-005 não colide com a
  composição atual. O board precisa de novo tipo de recurso e acessor em
  `board_model.hpp`, seguindo o precedente de `DryContactInputResource`, e de
  `wake_led` em `offered_resources`/`required_resources` no CMake.

## 7. Validação de colisão de GPIO

DEEPSLEEP-AC-002 exige rejeitar colisão de GPIO. Hoje a fachada valida apenas
validade do pino e duplicidade de `endpointId`/`eventType`
(`smart_sys_app.cpp:154-266`); **não existe precedente de verificação cruzada de
GPIO** entre capabilities e botão de reset. Duas leituras são possíveis:

1. verificar apenas o GPIO do `wake_led` contra os demais — aditivo e compatível
   com DEEPSLEEP-AC-001;
2. verificar todos os pares — passa a rejeitar composições que hoje são aceitas,
   o que tensiona DEEPSLEEP-AC-001 ainda que nenhum produto atual seja afetado.

A v0.4 não distingue as duas. É ambiguidade normativa pequena, de correção
barata, e cabe ao Arquiteto ou ao Autor. A análise recomenda a leitura 1.

## 8. Ordem do LED

A tensão apontada na verificação da v0.3 foi resolvida: a §4.3 fixa o
acionamento como primeira operação de plataforma, antes de NVS, commissioning,
rádio e reports, e a §3 aloca as falhas em `SetupStage::InitializePlatform` sem
criar valores novos. Isso é implementável dentro do estágio vigente. Permanece
apenas o resíduo físico do pulso de polaridade oposta, já coberto por
experimento e limitado pela §4.3 a "na medida suportada pelo ESP32-H2".

## 9. Componentes impactados

| Área | Impacto |
|---|---|
| API pública `SmartSysApp` | `configureDeepSleep()`, validação, causa de boot, LED, deadline e sleep |
| `issp_behaviors` | operação de parada do producer de entrada digital |
| `issp_transport_154` | operação terminal do executor e ponto de observação do deadline |
| `issp_core` | fechamento de admissão de reports, se exigido além da parada dos producers |
| Product firmware | opt-in, política temporal e renome integral |
| Board model e CMake | recurso `wake_led`, GPIO, polaridade e composição |
| Kconfig e `sdkconfig` | símbolo, rótulo e artefato gerado |
| Especificações | `ISSP-Configurable-Bootstrap.md`, `Firmware-Variants-Menuconfig.md` e, se decidido, `ISSP-Reusable-Components.md` |
| Testes | doubles de RTC, GPIO e sleep; evidência física futura em H2 |

## 10. Restrições confirmadas

- ESP32-H2 é o único target físico do `client_154`; QEMU não é admitido;
- nenhuma execução é autorizada por esta especificação nem por esta atuação;
- deep sleep reinicia o firmware; nada volátil é preservado sem decisão futura;
- Kconfig não pode governar lógica de componentes compartilhados, e o renome não
  introduz símbolo lido por esses componentes;
- a fachada e seus objetos permanecem estáticos e vivos até o reboot.

## 11. Experimentos necessários

1. build H2 das composições habilitada e desabilitada;
2. injeção controlada de wakeup, GPIO e deep sleep para verificar ordem, falhas
   e conversão sem iniciar hardware;
3. deadline expirando durante varredura de commissioning e durante escrita em
   NVS, para confrontar 4.3 — o experimento decisivo desta versão;
4. falha não retryable de report, para confrontar 6;
5. execução física de timer, polaridade, duração do LED, causa de wakeup e
   corrente, inclusive com o pull remanescente do botão;
6. inspeção do GPIO em transição para confirmar ausência de pulso incompatível.

Leitura de código não certifica nenhum desses fatos.

## 12. Recomendação

Recomenda-se **prontidão condicionada**, com um item de autoridade e dois
ajustes de recorte:

1. **decisão arquitetural:** declarar a relação com
   `ISSP-Reusable-Components.md` v1.1, cuja cláusula de ampliação material da API
   é acionada pela quiescência (2.1);
2. **ajuste de recorte:** corrigir na §6 a expressão "pelos contratos
   existentes", que hoje descreve um estado do repositório que não existe (3);
3. **ajuste de recorte:** incluir `client_154/sdkconfig` no alcance do renome ou
   declarar sua defasagem até a primeira execução autorizada (5); e escolher
   entre as duas leituras da colisão de GPIO (7).

Nenhum desses itens é bloqueador de viabilidade técnica. As demais relações da
seção 2 se sustentam, o oráculo de entrega da §6 corresponde ao código, o renome
é executável e a observação do deadline tem mecanismo primário implementável com
atraso limitado a cerca de 0,7 s em `setup()` e 1,15 s em `Running`. O único
resíduo não decidível por leitura é a preempção durante escrita em NVS, que a
própria especificação já submete a experimento. Builds, testes e hardware
permanecem `Not Executed`.
