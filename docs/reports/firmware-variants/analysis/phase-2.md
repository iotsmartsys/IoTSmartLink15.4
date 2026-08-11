# Relatório de análise — variantes de firmware

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/Firmware-Variants-Menuconfig.md`

**Revisão confrontada:** Registro histórico EKM 1.x preservado na migração para EKOM 3.2

**Estado:** Concluído

> Este relatório preserva uma atuação histórica e não altera fontes normativas.

## Análise de implementabilidade da Fase 2 (Engenheiro Analista)

Confronto do recorte vigente da Fase 2 com o repositório, a arquitetura, os
precedentes e os critérios de aceite. Esta análise não altera implementação, não
promove estado e não declara aprovação; ela informa o Arquiteto.

O recorte passou por três rodadas de confronto. A primeira levantou os
bloqueadores B1 a B4, resolvidos pelo Arquiteto nas decisões 20 a 26. A segunda
verificou essas resoluções e apontou os esclarecimentos C1 a C3, resolvidos nas
decisões 27 a 31. Esta rodada confronta as decisões 27 a 31 e revisa se as
anteriores continuam sustentadas pelo código.

### Recomendação

**Prontidão sustentada.** Nenhum bloqueador normativo remanescente. As decisões
27 a 31 são realizáveis com o repositório vigente e fecham C1 a C3 sem criar
contradição com o que já estava decidido.

Duas observações são materiais para o Implementador e não decorrem de leitura de
contrato, mas do comportamento das APIs envolvidas: a junção de teste autorizada
não é suficiente para o oráculo agora exigido (O1), e a API do `esp_timer` não
oferece barreira contra callback em voo, o que limita o que o teste de destruição
pode provar (O2). Nenhuma das duas impede iniciar; ambas mudam como o código deve
ser escrito e como a evidência deve ser lida.

### Verificação das decisões 27 a 31

**Decisão 27 — o limite de 150 ms termina na confirmação pelo behavior:
sustentada e coerente.**
Retira do orçamento exatamente o que não é governado pelo debounce: o executor
usa `kReportAckTimeoutMs = 50` e `kPendingReportRetryDelayMs = 1000`
(`issp154_report_executor.cpp`), incompatíveis com 150 ms por razões alheias à
entrada. Os critérios e o conjunto de validação foram alinhados.

Resta um problema de **instrumentação**, não de contrato: em hardware não se
conhece o instante da estabilização física, porque o acionamento manual não tem
borda registrável. A medição verificável a partir de log é
`confirmação − primeira amostra divergente`, que subestima a latência real em
até um período. Recomendação: o behavior registrar os dois instantes e a
evidência de hardware ser lida como `confirmação − primeira amostra divergente +
10 ms` para o limite superior. Pela aritmética do esquema, o pior caso é
40 + 50 + 50 = 140 ms, então esse teto de leitura ainda cabe nos 150 ms sem
folga adicional.

**Decisão 28 — o oráculo é a sequência de classificações: sustentada e mais
correta que a formulação anterior.**
A ressalva do Arquiteto está certa: 50 ms é o mínimo teórico em alinhamento
favorável, não limiar universal. Um pulso de 50 ms em fase desfavorável distribui
as amostras entre janelas e não produz duas maiorias consecutivas; o mesmo pulso
em fase favorável produz. Declarar os níveis por amostra é a única formulação
determinística, e os critérios de aceite foram reescritos nesses termos.

Consequência direta em **O1**, abaixo: para declarar níveis por amostra, o teste
precisa também governar *quando* a amostra ocorre.

**Decisão 29 — estabilização síncrona em `begin()`: sustentada.**
`begin()` é chamado por `IsspDevice::start()` dentro do estágio `StartDevice`, na
tarefa do `app_main`, depois de `InitializeNetwork`. Nada em `setup()` tem
timeout, o `ResetButtonMonitor` já foi iniciado em `InitializePlatform` e a
tarefa `issp154_rx` (prioridade `tskIDLE_PRIORITY + 4`) segue rodando, portanto
~100 ms de espera em `begin()` não afetam nenhum caminho vigente. Publicar antes
de iniciar o timer também preserva a ordem que `DigitalOutputBehavior` já usa:
falha de publicação aborta `begin()` e o estágio `StartDevice`, e nesse caminho
nenhum timer terá sido criado.

Uma precaução concreta: a decisão observa que a espera pode usar um tick por
período com a configuração vigente, porque `CONFIG_FREERTOS_HZ=100` faz
10 ms = 1 tick exatamente. A espera deve ser escrita como
`pdMS_TO_TICKS(período)` e nunca como contagem literal de ticks — a forma em
milissegundos permanece correta se `CONFIG_FREERTOS_HZ` mudar, e a forma literal
passa a amostrar 1 ms por engano.

**Decisão 30 — ciclo de vida do `esp_timer`: sustentada, com o limite descrito em
O2.** Despacho por tarefa é o default de
`esp_timer_create_args_t::dispatch_method` e é o único compatível com
`portENTER_CRITICAL` no caminho de publicação, conforme a decisão 31. `stop` e
`delete` no destrutor e o desfazimento em qualquer falha posterior à criação
resolvem o risco levantado na rodada anterior.

**Decisão 31 — escopo da seção crítica: sustentada e agora completa.**
A enumeração cobre o que a rodada anterior identificou: slots,
`processingCommand_`, `reportNotificationDeferred_`, reserva de sequência e todo
caminho que altere `reportSequence_`, incluindo o `publishReport()` que hoje é
código morto. Codificação, callback, notificação e transporte ficam fora, e a
falha de codificação libera a reserva pelo fluxo protegido normal — que é
exatamente `completePendingReport(token, false)`, reentrando na seção; portanto a
seção não pode ser retida nesse ponto nem ser recursiva.

Dois detalhes que a implementação precisa não deixar passar:

- os leitores `const` `peekPendingReport()` e `pendingReportCount()` são chamados
  pela tarefa `issp154_report_tx` a cada iteração de `run()` e leem estado
  multi-campo que passa a ser mutado por um contexto de prioridade mais alta.
  Precisam entrar na seção também, o que exige um lock `mutable`. O precedente
  exato existe: `Issp154Transport` declara
  `mutable portMUX_TYPE ackLock_ = portMUX_INITIALIZER_UNLOCKED;`;
- `normalizePendingReportOrders()` é O(n²) sobre oito slots dentro do
  bookkeeping; em `portENTER_CRITICAL` isso desabilita interrupções pelo trecho
  mais longo de toda a seção. Vale manter medido, sobretudo porque a tarefa
  `esp_timer` passa a preemptar tudo a cada 10 ms.

### Observações materiais para o Implementador

**O1 — a junção de teste autorizada não basta para o oráculo exigido.**
A decisão 25 autoriza “uma fonte de níveis injetável”. A decisão 28 exige que os
testes “declarem diretamente os níveis de cada amostra”. Apenas injetar a leitura
não produz isso: o `esp_timer` continua decidindo *quando* ler, então o teste
disputaria a fase de amostragem com o timer e a sequência observada seria
não determinística — exatamente o defeito que a decisão 28 quer evitar.

Para que a sequência seja declarada, o teste precisa governar o passo de
amostragem, não só o valor lido. A forma mínima é o construtor reservado a testes
expor também o passo — uma operação que executa uma amostra e a classificação
correspondente — de modo que o teste chame `passo(nível)` na ordem desejada, sem
timer ativo. Isso permanece análogo a `SetupHooks`: junção declaradamente não
contratual, fora da configuração pública do produto.

Consequência para o conjunto de validação: o período de 10 ms deixa de ser
exercitado pelos testes de sequência e precisa de um caso próprio, que arme o
timer real e verifique a cadência por `esp_timer_get_time()`. São dois tipos de
teste com propósitos distintos — sequência determinística sem timer, cadência com
timer — e a evidência deve dizer qual comprova o quê.

Recomendação ao Arquiteto: confirmar que o construtor reservado a testes da
decisão 25 pode expor o passo de amostragem além da fonte de níveis. É extensão
da mesma junção, não uma nova fronteira, mas a redação atual fala apenas de
níveis.

**O2 — `esp_timer` não oferece barreira contra callback em voo.**
`esp_timer_stop()` desarma o timer, e `esp_timer_delete()` **não libera o objeto
na hora**: ele reinsere o timer na lista da tarefa com `EVENT_ID_DELETE_TIMER`
para que a memória seja liberada em contexto de tarefa. Nenhuma das duas espera
uma callback que já esteja executando na tarefa `esp_timer`. Como a destruição
ocorre em outra tarefa, existe uma janela — a duração de um corpo de callback —
em que a callback ainda toca o objeto sendo destruído.

Em produção isso é inofensivo: as instâncias vivem em `SmartSysApp::Impl` e nunca
são destruídas. O ponto é o item “destruição com timer ativo” do conjunto de
validação: ele pode provar que `stop` e `delete` são chamados e que **nenhuma
callback posterior ocorre**; ele não pode provar que a janela de callback em voo
foi eliminada, porque a API não oferece o barramento necessário. Duas leituras
possíveis, ambas honestas:

1. registrar a janela residual como limitação conhecida da API, com a mitigação
   de manter a callback curta — coerente com a decisão 21;
2. eliminá-la estruturalmente, dando à callback um contexto de vida estática
   próprio, separado do behavior. Isso é um passo de desenho maior e não está
   autorizado pelo recorte.

A análise recomenda a leitura 1 e que a evidência do teste diga o que
efetivamente comprova, sem converter “nenhuma falha observada” em “janela
eliminada”.

### Riscos e incertezas remanescentes

**R1 — o que os testes de concorrência do `IsspDevice` comprovam.** Já
reconhecido pelo conjunto de validação, que agora exige distinguir integridade
observada da garantia dada pela seção crítica e pela inspeção. Registrado aqui
apenas para a evidência não regredir: intercalação determinística comprova a
máquina de estados; duas tarefas em alvo single-core aumentam a confiança;
nenhuma das duas é prova de exclusão mútua.

**R2 — infraestrutura de teste nova em dois componentes.** A estrutura de
arquivos já prevê `issp_behaviors/test_apps/digital_input_behavior_test/` e
`issp_core/test_apps/issp_device_concurrency_test/`. O único precedente é
`components/issp_app_154/test_apps/smart_sys_app_test` — esp32c3 sob QEMU,
`MINIMAL_BUILD ON`, Unity, e deliberadamente sem tocar GPIO. Decorre disso uma
recomendação concreta: o construtor reservado a testes do
`DigitalInputBehavior` deve **dispensar a configuração de GPIO**, já que a fonte
injetada substitui o pino; caso contrário o teste passa a depender da emulação de
GPIO do QEMU, terreno que nenhuma suíte do repositório exercita hoje.

**R3 — `esp_timer` sob QEMU esp32c3 é fato não confirmado por leitura.** Nenhum
app de teste do repositório usa `esp_timer`. Se o periódico não funcionar no
ambiente emulado, o caso de cadência de O1 migra para hardware; os testes de
sequência, por não dependerem do timer, continuam válidos sob QEMU. Ver E2.

**R4 — dependência de `esp_timer` no CMake de `issp_behaviors`.** O componente
declara hoje apenas `issp_core` e `esp_driver_gpio`, e `esp_timer` não está entre
os requisitos comuns do ESP-IDF — `issp_app_154` precisa listá-lo
explicitamente. A tabela de pontos afetados já registra a dependência.

**R5 — validação de pino na nova capability.** `addSwitchPlugCapability()` exige
`GPIO_IS_VALID_OUTPUT_GPIO`. A capability de entrada deve usar
`GPIO_IS_VALID_GPIO`, senão rejeita pinos legítimos de entrada. Detalhe pequeno,
mas é o tipo de simetria que se copia por engano.

**R6 — o log de capabilities muda de significado.** A decisão 24 faz
`app_setup begin capabilities=%u` contar capabilities de qualquer tipo. A
evidência de hardware da Fase 1 usou esse log (`capabilities=1`); para a tomada o
número segue 1, e a comparação de preservação deve considerar isso explicitamente.

**R7 — texto do diagnóstico de target.** Com `IDF_TARGET=esp32c6` os dois boards
ficam ocultos e o `FATAL_ERROR` de `main/CMakeLists.txt` ainda cita “the only
board model of this phase”. Precisa nomear os dois boards, senão o caso negativo
produz diagnóstico incorreto.

**R8 — margem da latência.** O pior caso aritmético é 140 ms contra o teto de
150 ms. Somando a leitura instrumentada recomendada na decisão 27, a folga
efetiva para jitter de despacho é praticamente nula. Não é impedimento — é o
número a observar primeiro em E2, e o candidato natural a revisão se o hardware
mostrar jitter.

### Experimentos necessários

- **E1 — compilação de `issp_core` com seção crítica**, confirmando que
  `freertos` como requisito comum basta e que o app de teste sob QEMU esp32c3
  continua ligando;
- **E2 — `esp_timer` periódico a 10 ms**, primeiro sob QEMU esp32c3 (viabilidade
  do caso de cadência) e depois no ESP32-H2, medindo espaçamento real e jitter
  contra o orçamento de R8;
- **E3 — junção de teste com passo de amostragem**, comprovando sequências
  declaradas por amostra sem timer ativo e sem configurar GPIO;
- **E4 — destruição com timer ativo**, comprovando ausência de callback
  posterior e delimitando a janela residual descrita em O2;
- **E5 — `static_assert` de `kImplStorageBytes`** com o número de slots escolhido
  para o sensor; a decisão 21 já eliminou a pilha por behavior, restando
  configuração, estado do debounce e um `esp_timer_handle_t`;
- **E6 — as quatro combinações de seleção e o caso negativo C6 dos dois
  boards**, com `SDKCONFIG` isolado;
- **E7 — as duas composições contra o coordenador**, garantindo que sensor e
  tomada, que compartilham o device ID `0x15400001`, não estejam ativos na mesma
  rede simultaneamente durante a validação.

### Classificação das lacunas

- **decisão normativa ausente:** nenhuma. Resta a confirmação pedida em O1 —
  que o construtor reservado a testes possa expor o passo de amostragem — que é
  extensão da decisão 25, não fronteira nova;
- **escolha normal de implementação:** forma da seção crítica e reordenação de
  `preparePendingReport()`, instrumentação dos dois instantes da latência, forma
  da junção de teste, número de slots do sensor, validação de pino (R5), texto do
  diagnóstico (R7), forma dos acessadores de recurso;
- **dependência externa pendente:** nenhuma. ESP-IDF 6.0.1, `esp_timer`, os
  GPIO 9 e 14 do ESP32-H2, o `event type 1` e o coordenador estão disponíveis e
  confirmados.

### Resultado da análise

O recorte da Fase 2 está confrontado em todas as suas decisões compartilhadas. As
decisões 27 a 31 fecham os três esclarecimentos sem abrir contradição: o limite
de latência ficou onde é governável, o oráculo do debounce ficou determinístico,
o report inicial ganhou instante verificável, o timer ganhou dono e a seção
crítica ganhou escopo completo. Continua valendo o que a segunda rodada
constatou: nenhuma condicional interna nos componentes, nenhuma mudança de
protocolo, nenhuma duplicação de runtime.

O que a implementação precisa levar consigo são as duas observações de API — a
junção de teste precisa governar o passo de amostragem, e a janela de callback em
voo do `esp_timer` limita o que o teste de destruição comprova — mais a leitura
instrumentada da latência, cuja folga sobre o pior caso é de cerca de 10 ms. Cabe
ao Arquiteto confirmar a extensão pedida em O1 e a suficiência deste confronto.
