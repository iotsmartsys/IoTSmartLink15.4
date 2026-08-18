# Configuração de energia, bateria e factory reset no SDK Configuration Editor

**ID:** `EKOM-CLIENT-CONFIG-001`

**Classe da fonte:** Normativa

**Versão:** 0.1

**Estado normativo:** `Active`

**Estado da implementação:** `Validated` — configuração, código e builds
confrontados; hierarquia do SDK Configuration Editor validada pelo Arquiteto

**Estado do workflow:** Concluída [`Done`] por decisão do Arquiteto em
18/08/2026

**Análise de implementabilidade:** **Pronta** [`Ready`] para a v0.1, na revisão
`401c5f9f865d3ee093fe8e79529ad975690a73d2`, conforme
`docs/reports/client-sdk-configurable-features/analysis/2026-08-18T004649Z-401c5f9-32085660711-implementability-analysis.md`

**Responsável arquitetural:** Marcelo Miranda

**Última atualização:** 18/08/2026

**Escopo:** `client_154`, SDK Configuration Editor, product firmware, board
model e lifecycle periódico da capability de bateria

**Relações normativas:**

- Altera [`Amends`] `docs/specs/Firmware-Variants-Menuconfig.md` — amplia a
  seleção estática para configurar features da composição no limite da
  aplicação, sem levar símbolos `CONFIG_*` aos componentes compartilhados;
- Altera [`Amends`] `docs/specs/Client-Deep-Sleep.md@v0.11` — move o opt-in, a
  janela máxima acordada e o intervalo periódico de despertar dos literais do
  product firmware para a configuração do build. Wakeup pelo contato, LED,
  quiescência, deadline e entrada em deep sleep permanecem sob aquela fonte;
- Altera [`Amends`] `docs/specs/Client-Battery-Level.md@v0.5` — torna opcional
  a capability na composição concreta e, sem deep sleep, posterga a primeira
  medição até um intervalo completo depois de `Running`;
- Altera [`Amends`] `docs/adr/ADR-0002-PRODUCT-BOARD-COMPOSITION.md@Accepted`
  de forma estreita — o número do GPIO de factory reset passa a ser parâmetro
  do board selecionado; oferta do recurso, polaridade e demais fatos elétricos
  continuam pertencendo ao board model;
- Preserva `docs/specs/ISSP-Configurable-Bootstrap.md`, ADR-0001 e ADR-0005 —
  nenhuma API pública, identidade de capability, lógica do factory reset ou
  fronteira de componente é redefinida.

---

## 1. Objetivo e contexto

O `client_154` já permite escolher um product firmware e um board model no SDK
Configuration Editor, mas o produto `Door sensor battery H2` ainda fixa em
código:

- deep sleep habilitado;
- janela máxima acordada de 30 segundos;
- despertar periódico a cada 15 minutos;
- capability de nível de bateria sempre presente;
- GPIO 9 para o botão de factory reset, recebido do board model.

Esta especificação torna essas escolhas configuráveis no build e preserva os
valores atuais como defaults. O firmware continua estático: cada binário contém
uma única composição, sem seleção de feature em runtime.

## 2. Escopo

- checkbox para habilitar deep sleep no produto aplicável;
- configuração da janela máxima acordada, em segundos;
- configuração do intervalo periódico de despertar, em minutos;
- checkbox para habilitar a capability de nível de bateria;
- intervalo periódico da bateria, em minutos, quando não houver deep sleep;
- configuração do GPIO de factory reset com default 9;
- dependências de visibilidade e validade entre as opções;
- requisitos físicos condicionais no CMake conforme as features habilitadas;
- adiamento da medição periódica de bateria até a conclusão bem-sucedida do
  boot quando deep sleep estiver desabilitado;
- emenda estreita da ADR-0002 e reconciliação do conhecimento afetado.

## 3. Fora de escopo

- alterar protocolo, wire, coordenador, host, commissioning, ACK, retry ou
  identidade de reports e capabilities;
- alterar GPIO, pull, polaridade ou debounce do contato seco;
- configurar wakeup pelo contato separadamente do deep sleep;
- configurar duração ou modo do wake LED;
- alterar os parâmetros elétricos, químicos, de média ou de variação da
  capability de bateria, exceto seu intervalo sem deep sleep;
- alterar polaridade, hold, polling, limpeza do vínculo ou reboot do factory
  reset;
- corrigir `client_154/sdkconfig` para a composição default do Kconfig;
- criar seleção em runtime, geração de código ou framework genérico de boards;
- alterar o projeto ESP-IDF não classificado da raiz;
- implementar, executar testes, fazer flash, monitorar ou validar hardware
  nesta etapa de Autoria.

## 4. Experiência normativa no menu

Para `Door sensor battery H2`, o SDK Configuration Editor apresenta:

```text
App Client
├── Product firmware
├── Board model
├── Firmware features
│   ├── [*] Enable deep sleep
│   │   ├── Maximum awake time (seconds): 30
│   │   └── Periodic wake-up interval (minutes): 15
│   └── [*] Enable battery level capability
│       └── Battery reading interval (minutes): 120
│           [visível somente sem deep sleep]
└── Board configuration
    └── Factory reset GPIO: 9
```

Os rótulos acima integram o contrato de uso. Os símbolos Kconfig podem receber
os seguintes nomes, preservados pela implementação salvo impedimento técnico
registrado pela análise:

| Símbolo | Tipo | Default | Dependência |
|---|---|---:|---|
| `IOTSMARTLINK154_ENABLE_DEEP_SLEEP` | `bool` | `y` | produto `Door sensor battery H2` |
| `IOTSMARTLINK154_MAX_AWAKE_TIME_SECONDS` | `int` | `30` | deep sleep habilitado |
| `IOTSMARTLINK154_WAKEUP_INTERVAL_MINUTES` | `int` | `15` | deep sleep habilitado |
| `IOTSMARTLINK154_ENABLE_BATTERY_LEVEL` | `bool` | `y` | produto `Door sensor battery H2` |
| `IOTSMARTLINK154_BATTERY_READING_INTERVAL_MINUTES` | `int` | `120` | bateria habilitada e deep sleep desabilitado |
| `IOTSMARTLINK154_FACTORY_RESET_GPIO` | `int` | `9` | composição com botão de usuário |

Opção sem dependência satisfeita não pode ser editada. Esconder a opção e
manter um valor antigo no `sdkconfig` não autoriza seu consumo: a composição usa
somente valores cujas condições estejam satisfeitas.

## 5. Requisitos

### 5.1 Deep sleep

- **`CLIENTCFG-001`:** o product firmware aplicável habilita deep sleep somente
  quando `IOTSMARTLINK154_ENABLE_DEEP_SLEEP` estiver selecionado.
- **`CLIENTCFG-002`:** com deep sleep desabilitado, o product firmware não chama
  `configureDeepSleep()` e não toma wake LED, fonte periódica nem wakeup pelo
  contato. O sensor de porta e as demais features habilitadas continuam
  operando sem política de sleep.
- **`CLIENTCFG-003`:** com deep sleep habilitado, wake LED, wakeup pelo contato,
  quiescência, deadline, arbitragem e entrada em deep sleep preservam
  integralmente `Client-Deep-Sleep.md@v0.11`.
- **`CLIENTCFG-004`:** a janela máxima acordada é recebida em segundos, exige
  valor maior que zero e é convertida sem perda para `maxAwakeTimeMs`. O default
  de 30 segundos preserva o literal vigente de 30.000 ms.
- **`CLIENTCFG-005`:** o intervalo periódico de despertar é recebido em
  minutos, exige valor maior que zero e é encaminhado como
  `DeepSleepTimeUnit::Minutes`. O default de 15 minutos preserva o literal
  vigente.
- **`CLIENTCFG-006`:** transbordo na conversão para a API pública e intervalo
  fora do limite efetivo do ESP32-H2 são rejeitados pelos mecanismos de
  configuração e validação vigentes, sem produzir operação parcial.

### 5.2 Capability de bateria

- **`CLIENTCFG-007`:** `IOTSMARTLINK154_ENABLE_BATTERY_LEVEL` decide se o
  product firmware registra `addBatteryLevelCapability()`.
- **`CLIENTCFG-008`:** com a capability habilitada, endpoint 2, tipo de evento
  3, fatos elétricos do board e demais parâmetros normativos da seção 8 de
  `Client-Battery-Level.md` permanecem inalterados.
- **`CLIENTCFG-009`:** com a capability desabilitada, endpoint 2 não é
  registrado, a aplicação não inicializa ADC nem produz report de bateria e a
  composição não exige o recurso `battery_measurement`.
- **`CLIENTCFG-010`:** com bateria e deep sleep habilitados,
  `samplePeriodMs` permanece zero e a capability mede e publica em todo boot
  operacional conforme `Client-Battery-Level.md`. A opção de intervalo
  periódico da bateria não é consumida.
- **`CLIENTCFG-011`:** com bateria habilitada e deep sleep desabilitado, o
  intervalo em minutos exige valor maior que zero e é convertido sem perda
  para `samplePeriodMs`. O default é 120 minutos, equivalentes a 2 horas.
- **`CLIENTCFG-012`:** no modo periódico sem deep sleep, nenhuma medição nem
  publicação de bateria ocorre durante `SmartSysApp::setup()`. Inicialização do
  ADC durante o boot permanece permitida, desde que não leia nem publique.
- **`CLIENTCFG-013`:** o marco zero do intervalo periódico é a conclusão
  bem-sucedida do boot, definida por `SmartSysApp::setup()` alcançar
  `AppState::Running` com `SetupStage::Completed` e `AppResult::Ok`.
- **`CLIENTCFG-014`:** a primeira medição ocorre somente depois de transcorrido
  um intervalo completo desde `Running`. A primeira leitura válida é sempre
  publicada; leituras posteriores repetem o intervalo e obedecem ao
  `reportDeltaPercent` vigente.
- **`CLIENTCFG-015`:** se o boot terminar em `NotReady` ou falha, o intervalo
  não inicia e nenhuma medição ou publicação de bateria ocorre naquele boot.
- **`CLIENTCFG-016`:** falha na primeira medição após o intervalo conserva a
  política vigente: nenhum report é publicado e o próximo intervalo oferece
  nova oportunidade, sem abortar o dispositivo.

### 5.3 Factory reset

- **`CLIENTCFG-017`:** `IOTSMARTLINK154_FACTORY_RESET_GPIO` parametriza o pino
  do recurso `UserButtonResource` do board selecionado; o default é GPIO 9.
- **`CLIENTCFG-018`:** o board model continua declarando que oferece o botão e
  conserva sua polaridade. O product firmware recebe o recurso pelo contrato do
  board, sem usar diretamente o símbolo Kconfig.
- **`CLIENTCFG-019`:** active low, hold de 10.000 ms, polling de 20 ms, limpeza
  exclusiva do descritor e reboot após sucesso permanecem inalterados.
- **`CLIENTCFG-020`:** GPIO inválido para entrada ou em colisão com recurso da
  composição é rejeitado antes da operação normal pelos mecanismos aplicáveis.

### 5.4 Composição e fronteiras

- **`CLIENTCFG-021`:** símbolos `CONFIG_*` desta especificação ficam restritos
  ao limite `client_154/main`; nenhum aparece em `components/issp_*`.
- **`CLIENTCFG-022`:** o CMake exige `wake_led` e `dry_contact_wakeup` somente
  quando deep sleep estiver habilitado e exige `battery_measurement` somente
  quando a capability de bateria estiver habilitada. `dry_contact_input` e
  `user_button` permanecem requisitos do sensor de porta.
- **`CLIENTCFG-023`:** cada build contém somente o product firmware, board e
  features selecionados; nenhuma decisão é postergada para runtime.
- **`CLIENTCFG-024`:** combinações sem recurso físico necessário falham antes
  de produzir binário e diagnosticam produto, board e recurso ausente.
- **`CLIENTCFG-025`:** as escolhas exclusivas e os defaults atuais de product
  firmware e board model no Kconfig permanecem inalterados. A seleção divergente
  já rastreada em `client_154/sdkconfig` permanece fora do recorte e registrada
  em `EKOM-DEBT-0005`.

## 6. Fluxos e condições de borda

### 6.1 Bateria com deep sleep

```text
boot operacional
→ capability mede e publica durante o ciclo vigente
→ lifecycle de deep sleep drena pendências segundo seu próprio contrato
→ sleep
```

Esta especificação não move a medição desse modo para depois de `Running`.

### 6.2 Bateria sem deep sleep

```text
boot
→ setup inicializa a composição sem medir ou publicar bateria
→ setup alcança Running
→ inicia contagem do intervalo configurado
→ intervalo completo
→ mede e publica o primeiro valor válido
→ repete o intervalo e aplica reportDeltaPercent
```

`NotReady` e falha são estados terminais do setup vigente e não iniciam o
intervalo. Esta especificação não cria retry de `setup()`, transição posterior
para `Running` nem novo estado público.

### 6.3 Conversões

- segundos para milissegundos e minutos para milissegundos usam aritmética que
  detecta transbordo antes de estreitar para os tipos públicos;
- o intervalo de bateria deve caber em `std::uint32_t` milissegundos; isso
  deriva o limite máximo implementável de 71.582 minutos sem criar uma faixa de
  produto por julgamento;
- o intervalo de despertar conserva a validação de target definida em
  `Client-Deep-Sleep.md` e não recebe um máximo literal novo nesta fonte.

## 7. Critérios de aceite

- **`CLIENTCFG-AC-001` — Visibilidade:** selecionado o produto aplicável, o
  editor apresenta os dois checkboxes, os dois tempos de deep sleep, o
  intervalo periódico da bateria e o GPIO de reset com as dependências e
  defaults da seção 4. Produto não aplicável não recebe opções funcionais sem
  consumidor.
- **`CLIENTCFG-AC-002` — Deep sleep desabilitado:** com o checkbox desmarcado,
  a composição não chama `configureDeepSleep()`, não exige `wake_led` nem
  `dry_contact_wakeup` e continua operando as features restantes.
- **`CLIENTCFG-AC-003` — Defaults de deep sleep:** com defaults, a configuração
  encaminhada contém 30.000 ms de janela acordada e timer de 15 minutos, com
  wake LED e contato preservados.
- **`CLIENTCFG-AC-004` — Tempos alternativos:** valores válidos diferentes dos
  defaults alcançam a API após conversão exata; zero, transbordo e intervalo de
  despertar fora do limite do target são rejeitados.
- **`CLIENTCFG-AC-005` — Bateria com deep sleep:** capability habilitada mantém
  endpoint 2, evento 3, `samplePeriodMs=0` e publicação em todo boot operacional.
- **`CLIENTCFG-AC-006` — Bateria periódica após boot:** com deep sleep
  desabilitado e intervalo configurado, nenhuma leitura ou publicação ocorre
  durante setup; depois de `Running`, um intervalo completo transcorre antes da
  primeira medição e a primeira leitura válida é publicada.
- **`CLIENTCFG-AC-007` — Boot sem sucesso:** com bateria periódica e setup em
  `NotReady` ou falha, o timer não inicia e nenhuma leitura ou publicação de
  bateria ocorre.
- **`CLIENTCFG-AC-008` — Bateria desabilitada:** endpoint 2, inicialização do
  ADC, reports e requisito `battery_measurement` ficam ausentes, sem alterar o
  sensor de porta.
- **`CLIENTCFG-AC-009` — Factory reset default:** GPIO 9 preserva polaridade,
  hold, polling, limpeza do vínculo e reboot vigentes.
- **`CLIENTCFG-AC-010` — Factory reset alternativo:** um GPIO válido alternativo
  selecionado no menu chega ao monitor pelo recurso do board, sem alterar as
  demais regras; valor inválido ou colisão é rejeitado.
- **`CLIENTCFG-AC-011` — Fronteiras:** inspeção comprova ausência dos símbolos
  em `components/issp_*`, uma única composição no binário e requisitos CMake
  condicionais às features.
- **`CLIENTCFG-AC-012` — Preservação externa:** nenhum arquivo ou contrato do
  coordenador, host, wire, commissioning, ACK, retry ou identidade é alterado.

## 8. Evidências e testes

**Artefatos de teste no recorte:** Nenhum. Esta versão não cria nem altera
testes automatizados.

A futura implementação deve produzir:

- inspeção das dependências, defaults e configuração gerada pelo menu;
- builds isolados com deep sleep habilitado e desabilitado;
- builds com bateria habilitada nos modos com e sem deep sleep;
- build com bateria desabilitada;
- builds com tempos e GPIO diferentes dos defaults;
- inspeção das fontes e recursos selecionados em cada composição;
- inspeção do lifecycle comprovando que o modo periódico não mede nem publica
  durante setup e só inicia o intervalo depois de `Running`.

Validação posterior em hardware, sob autorização própria, deve confrontar os
dois tempos de deep sleep, a leitura periódica sem deep sleep, a ausência da
capability e o factory reset em GPIO alternativo. Build não comprova esses
comportamentos. Testes, flash, monitor e hardware não são autorizados por esta
especificação.

## 9. Decisões confirmadas

**Decisões confirmadas pelo Arquiteto em 15/08/2026:**

- deep sleep, janela acordada e intervalo de despertar passam ao menu;
- defaults preservam 30 segundos e 15 minutos;
- bateria pode permanecer habilitada sem deep sleep;
- nesse modo, seu intervalo é configurável e tem default de 2 horas;
- a medição e o report periódicos só ocorrem depois de boot concluído com
  sucesso e de um intervalo completo contado desde `Running`;
- GPIO de factory reset torna-se parâmetro do board selecionado, com default 9;
- `client_154/sdkconfig` permanece como está e sua divergência é aceita como
  `EKOM-DEBT-0005`.

As pendências submetidas à análise foram resolvidas na revisão `Ready` e na
implementação: a amostragem periódica é iniciada somente depois de `Running`,
as conversões possuem faixas Kconfig delimitadas, os recursos físicos são
condicionais à composição e colisões do GPIO configurável são rejeitadas no
build do board selecionado.

## 10. Encerramento

Em 18/08/2026, o Arquiteto confirmou que a hierarquia `App Client` no SDK
Configuration Editor funcionou como esperado e determinou o encerramento da
v0.1. A decisão considera a análise `Ready`, o relatório de implementação da
execução `32091116616` e a validação de encerramento registrada em
`docs/reports/client-sdk-configurable-features/validation/2026-08-18-architect-menu-validation-and-closure.md`.

Foram concluídos builds ESP32-H2 das composições default, sem deep sleep, sem
bateria e com tempos e GPIO alternativos. Configurações em que o GPIO de
factory reset colide com contato seco, wake LED ou medição de bateria foram
rejeitadas antes da geração do binário. Testes, flash, monitor e hardware
permanecem `Not Executed`; o Arquiteto considerou essa evidência suficiente
para declarar a v0.1 Concluída [`Done`]. Nova necessidade ou evidência material
exige decisão de reabertura.
