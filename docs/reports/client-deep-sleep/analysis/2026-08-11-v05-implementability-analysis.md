# Análise de implementabilidade da v0.5 — deep sleep do client

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/Client-Deep-Sleep.md`

**Revisão confrontada:** v0.5, Draft de 11/08/2026

**Estado:** Concluído

**Capacidade:** Engenheiro Analista

**Data:** 11/08/2026

**Resultado:** Implementabilidade recomendada; uma lacuna normativa pontual, uma
correção de nota numérica e um mecanismo a redesenhar; nenhuma execução
realizada

> Este relatório registra evidências e recomendações. Não altera a fonte
> normativa, não promove estado e não autoriza implementação ou testes.

## 1. Recorte e método

A seção 8 da v0.5 determina confirmar a relação com
`ISSP-Reusable-Components.md`, a suficiência e o limite das três novas
operações, a sequência de quiescência, a atualização direta do `sdkconfig` e a
validação restrita de colisão. Este relatório cobre os cinco pontos, reexamina
os critérios afetados e corrige uma nota numérica que a v0.5 herdou da análise
da v0.4.

Nenhum build, teste, flash ou execução em hardware foi realizado. A árvore
estava limpa em `spec/client-deep-sleep`.

## 2. Relação com `ISSP-Reusable-Components.md`

**Confirmada.** A cláusula acionada é "qualquer ampliação material da API exige
interrupção e decisão arquitetural" (v1.1, linhas 215-216). DEEPSLEEP-DEC-006 é
essa decisão e a seção 2 a declara como `Amends` limitado às três operações. O
escopo declarado corresponde ao que a seção 6 usa: nenhuma outra API compartilhada
é ampliada.

Duas verificações de compatibilidade sustentam a relação:

- **A prova de reutilização não quebra.** `examples/issp_minimal_client` usa
  `IsspDevice`, `DigitalOutputBehavior` e `Issp154ReportExecutor` diretamente. Como
  `quiesce()` é declarada virtual **com implementação padrão** e as demais são
  métodos novos, o segundo consumidor continua compilando sem alteração, o que
  preserva o critério de dois consumidores da própria especificação.
- **Os valores de retorno já existem.** `IsspResult::NotReady` e
  `IsspCommandResult::Failed` são valores vigentes
  (`issp_types.hpp:46-52` e `85-92`); a seção 6 não introduz enumeração nova em
  nenhuma camada.

Ressalva de redação, não de mérito: `ISSP-Architecture.md` v1.2 atribui ao
executor "preservar reports não entregues para tentativa posterior" (linha 174).
Com `stop()` terminal no boot, a tentativa posterior passa a ser o próximo boot,
e o slot permanece preservado no device até lá — não há persistência, o que já é
decidido por DEEPSLEEP-DEC-004. A declaração "Preserva `ISSP-Architecture.md`"
continua correta; convém apenas que a seção 6 diga explicitamente que `stop()`
encerra as tentativas **deste boot**.

## 3. Suficiência e limite das três operações

### 3.1 `IDeviceBehavior::quiesce()` — suficiente, com condição de corrida a tratar

O producer que importa é o `DigitalInputBehavior`: amostra por `esp_timer` com
`dispatch_method = ESP_TIMER_TASK` (`digital_input_behavior.cpp:79-86`) e hoje só
para pelo destrutor ou por caminhos internos de falha, via `stopAndDeleteTimer()`
privado. A operação pública resolve exatamente essa lacuna e não exige destruir o
objeto, o que preserva SMARTAPP-DEC-004A.

Condição de corrida a tratar na implementação: `quiesce()` é chamada pela task da
fachada enquanto o callback pode estar executando na task do `esp_timer`. Parar e
excluir o timer nesse instante exige idempotência e proteção do handle — hoje
apenas `confirmedState_` é atômico, e `timer_`/`timerStarted_` não são. É
**escolha normal de implementação**, mas deve ser explicitada como caso de teste.

A ordem da seção 6 já neutraliza o efeito funcional dessa corrida: como
`beginQuiescence()` precede `quiesce()`, uma publicação tardia do callback recebe
`NotReady` e não cria slot. A ordem escolhida está correta e não deve ser
invertida.

### 3.2 `IsspDevice::beginQuiescence()` — suficiente

O device já serializa publicação, reserva, conclusão e inspeção sob
`reportLock_`, e já mantém `processingCommand_` e a notificação diferida
(`issp_device.hpp:100-107`). Fechar despacho de comandos e admissão de reports
atomicamente cabe nesse mesmo mutex, sem estrutura nova. Preservar os slots já
admitidos é o comportamento vigente: nada os remove exceto
`completePendingReport(..., delivered=true)`.

### 3.3 `Issp154ReportExecutor::stop()` — suficiente, mas exige redesenho do delay

Os três efeitos exigidos são desiguais em custo:

- **desregistrar notificação e encerrar a task:** direto. A task é estática
  (`StaticTask_t` e stack próprios), então sair do laço e autoexcluir não libera
  memória nem destrói o executor, como a seção 6 exige;
- **aguardar de forma limitada a tentativa de transporte ativa:** limitado e
  mensurável — ver 4;
- **interromper espera ou delay de retry:** a espera é
  `ulTaskNotifyTake(portMAX_DELAY)` e acorda com uma notificação, mas o retry usa
  `vTaskDelay(1000 ms)` (`issp154_report_executor.cpp:145`), que **não é
  interrompível por notificação**. Abortá-lo exigiria `xTaskAbortDelay()`, cuja
  disponibilidade depende de `INCLUDE_xTaskAbortDelay` na configuração FreeRTOS —
  não confirmável por leitura neste repositório e, se dependesse de opção de
  menu, tensionaria o invariante de que Kconfig não governa lógica interna de
  componentes compartilhados.

Recomendação técnica: substituir o `vTaskDelay` do retry por
`ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000))`, mantendo o mesmo intervalo
observável. O delay passa a ser um ponto cooperativo, `stop()` fica
implementável sem dependência externa e o comportamento de retry vigente é
preservado. É **escolha normal de implementação**, registrada aqui porque a
alternativa introduz dependência de configuração.

## 4. Sequência de quiescência

**A ordem obrigatória da seção 6 está correta e é exigida pelo código**, não
apenas conveniente:

- `end()` do transporte **depois** do `stop()` do executor é obrigatório:
  `end()` apaga o event group de ACK (`issp154_transport.cpp:396-399`) que
  `sendConfirmed` usa para esperar confirmação. A ordem inversa produziria uso de
  handle destruído;
- preparar a fonte de wakeup **antes** de qualquer operação terminal é seguro:
  habilitar wakeup por timer é configuração sem efeito até o início do sleep,
  então abortar a sequência nesse ponto deixa o runtime intacto, como a seção 6
  pretende;
- fechar a admissão antes de parar os behaviors torna o oráculo estável, como em
  3.1.

### 4.1 Lacuna: o monitor de factory reset não está na sequência

`ResetButtonMonitor` mantém uma task própria em laço infinito com
`vTaskDelay(pollIntervalMs)` e **não aparece em nenhum passo da seção 6**. Ele
não precisa ser encerrado para a correção do sleep — o deep sleep reinicia o
firmware — mas o caminho que ele dispara não é inofensivo: ao completar o hold, o
`FactoryResetService` executa limpeza de persistência e chama `esp_restart()`
(`factory_reset_service.cpp:24-48`). Isso significa que um botão mantido
pressionado durante a quiescência pode apagar o descritor em NVS e reiniciar o
dispositivo concorrentemente à entrada em deep sleep.

É a mesma classe de risco que a v0.5 já reconhece para `persistNetwork()`, mas
com origem diferente e não coberta. Como o comportamento desejado é uma escolha
de produto — o factory reset vence o sleep, o sleep vence o factory reset, ou o
monitor é parado na quiescência —, isto é **decisão normativa ausente**, de
correção barata. Recomenda-se acrescentar um passo explícito à seção 6 e cobrir a
janela do factory reset na mesma exclusão declarada para NVS.

## 5. Correção da nota numérica da seção 6

A seção 6 cita "1,15 s entre tentativas do executor" como nota de análise. Esse
número veio do relatório da v0.4 e **está subestimado**: ele contou uma única
tentativa de transporte, mas `sendConfirmed` executa até três tentativas internas
com backoff de 5 ms e 10 ms (`issp154_transport.cpp:28` e `596-604`), cada uma
com até 100 ms de TX (`kPhysicalTxTimeoutMs`) e 50 ms de espera de ACK
(`kReportAckTimeoutMs`). O pior caso de uma chamada a `processOne()` é, portanto,
cerca de 465 ms, e o intervalo máximo entre pontos cooperativos do executor fica
em torno de **1,47 s**, não 1,15 s.

A nota de commissioning permanece correta: três tentativas por canal, cada uma
com até 100 ms de TX e 120 ms de espera, resultam em cerca de 0,66 s por canal,
mais o custo de `begin()`/`end()` do rádio, com varredura completa na ordem de
10,6 s.

Como a própria seção 6 declara que esses valores são nota de análise e não limite
contratual, a correção não altera contrato nem critério; ainda assim o número
deve ser corrigido para não ser citado adiante como medida.

## 6. Atualização direta do `sdkconfig`

Viável e correta. `client_154/sdkconfig` é o único artefato gerado versionado que
cita o símbolo, na linha 949, e a linha está comentada como `is not set`, de modo
que a edição direta é textual e não altera seleção. `client_154/sdkconfig.old`
também cita o símbolo, mas **não é versionado** (`git ls-files` não o lista),
então a seção 5 acerta ao ignorá-lo. Nenhuma regeneração por build é necessária,
o que mantém a preservação de `Repository-Test-Execution-Policy.md`.

## 7. Validação restrita de colisão

Implementável e compatível. Hoje a fachada valida apenas validade do pino e
duplicidade de `endpointId`/`eventType` (`smart_sys_app.cpp:154-266`), sem
verificação cruzada de GPIO. A regra bidirecional da seção 5 — comparar o
`wake_led` aos recursos já registrados e aplicar a comparação inversa aos
registrados depois — cabe no `Impl` com os campos que ele já mantém, e não
depende da ordem em que o product firmware chama os métodos. Ao limitar o
confronto ao `wake_led`, DEEPSLEEP-AC-001 é preservado: composições hoje aceitas
continuam aceitas quando deep sleep ou LED estão desabilitados.

## 8. Composição

Sem mudança em relação à v0.4: a `Door Sensor Battery H2` usa GPIO 14 e GPIO 9, o
GPIO 13 está livre e o `board_model.hpp` recebe novo tipo e acessor seguindo o
precedente de `DryContactInputResource`, com `wake_led` em
`offered_resources`/`required_resources`.

## 9. Componentes impactados

| Área | Impacto |
|---|---|
| API pública `SmartSysApp` | `configureDeepSleep()`, validação, causa de boot, LED, deadline e sequência de quiescência |
| `issp_core` | `IDeviceBehavior::quiesce()` e `IsspDevice::beginQuiescence()` |
| `issp_behaviors` | override de `quiesce()` no `DigitalInputBehavior`, com proteção do handle de timer |
| `issp_transport_154` | `Issp154ReportExecutor::stop()` e conversão do delay de retry em ponto cooperativo |
| `issp_app_154` | política de deadline, supervisor e tratamento do monitor de factory reset |
| Product firmware | opt-in, política temporal e renome integral |
| Board model e CMake | recurso `wake_led`, GPIO, polaridade e composição |
| Kconfig e `sdkconfig` | símbolo, rótulo e artefato versionado |
| Testes | doubles de RTC, GPIO e sleep; concorrência de `quiesce()` e `stop()` |

## 10. Restrições confirmadas

- ESP32-H2 é o único target físico do `client_154`; QEMU não é admitido;
- nenhuma execução é autorizada por esta especificação nem por esta atuação;
- Kconfig não governa lógica interna de componentes compartilhados, o que exclui
  resolver `stop()` por opção de menu do FreeRTOS;
- a fachada e seus objetos permanecem estáticos e vivos até o reboot; nenhuma das
  três operações destrói objeto ou permite reinício no mesmo boot;
- deep sleep reinicia o firmware; nada volátil é preservado sem decisão futura.

## 11. Experimentos necessários

1. build H2 das composições habilitada e desabilitada;
2. injeção controlada de wakeup, GPIO e deep sleep para verificar ordem, falhas e
   conversão sem iniciar hardware;
3. deadline expirando durante a varredura de commissioning e durante escrita do
   descritor em NVS — o experimento decisivo, já exigido pela seção 8;
4. `quiesce()` concorrente ao callback do `esp_timer` e `stop()` concorrente a uma
   tentativa de transporte ativa;
5. factory reset solicitado durante a quiescência, se a seção 6 passar a tratá-lo;
6. falha não retryable de report, confrontando o slot ocupado e o caminho de
   deadline;
7. execução física de timer, polaridade, duração do LED, causa de wakeup e
   corrente, inclusive com o pull remanescente do botão;
8. inspeção do GPIO em transição para confirmar ausência de pulso incompatível.

Leitura de código não certifica nenhum desses fatos.

## 12. Recomendação

Recomenda-se **prontidão**, com um item de autoridade e duas correções pontuais,
todos de baixo custo:

1. **decisão normativa:** tratar o `ResetButtonMonitor` e a janela de factory
   reset na sequência da seção 6 (4.1);
2. **correção factual:** ajustar a nota do executor de 1,15 s para
   aproximadamente 1,47 s (5);
3. **precisão de redação:** declarar que `stop()` encerra as tentativas deste
   boot, alinhando-se à responsabilidade descrita em `ISSP-Architecture.md` (2).

A relação com `ISSP-Reusable-Components.md` está confirmada e delimitada; as três
operações são suficientes para a sequência exigida e nenhuma delas cria lifecycle
público; a ordem da seção 6 é exigida pelo código; o `sdkconfig` é atualizável sem
build; e a colisão restrita preserva DEEPSLEEP-AC-001. O único ponto que exige
trabalho técnico não trivial é o `stop()` do executor, resolvido convertendo o
delay de retry em espera notificável, sem dependência de configuração externa. O
resíduo não decidível por leitura continua sendo a preempção durante escrita em
NVS. Builds, testes e hardware permanecem `Not Executed`.
