# ISSP — Especificação Arquitetural

**Tipo:** Normativo
**Status:** Active — refatoração funcional e consolidação concluídas
**Versão:** 1.1
**Responsável arquitetural:** Marcelo Miranda
**Última atualização:** 21/07/2026
**Escopo:** Runtime ISSP do client IEEE 802.15.4

---

## 1. Objetivo

O ISSP é uma biblioteca para comunicação entre dispositivos embarcados
utilizando IEEE 802.15.4.

A biblioteca deve abstrair os detalhes do transporte e oferecer à aplicação
uma interface orientada a dispositivos, capacidades, comandos e eventos. O
objetivo é permitir que diferentes firmwares utilizem o protocolo sem replicar
lógica de comunicação, confirmação, descoberta ou processamento de mensagens.

## 2. Princípios arquiteturais

1. A biblioteca deve ser independente do firmware específico da aplicação.
2. A aplicação não deve manipular diretamente frames IEEE 802.15.4.
3. O transporte não deve conhecer regras específicas de sensores ou atuadores.
4. As regras do dispositivo devem ser implementadas por behaviors ou
   capacidades.
5. Apenas o transporte deve controlar o rádio durante a execução.
6. A biblioteca deve possuir ciclo de vida explícito de inicialização e
   encerramento.
7. Falhas de comunicação e descoberta não devem bloquear permanentemente o
   dispositivo.
8. Estados importantes devem ser publicados por meio de reports.
9. Comandos recebidos devem ser processados pela capacidade responsável e
   confirmados conforme o protocolo.
10. O conhecimento permanente do protocolo deve permanecer neste documento,
    nos contratos normativos aplicáveis e no código, não apenas em conversas.
11. O payload e as regras permanentes do protocolo residem em `issp_core`; o
    transporte IEEE 802.15.4 não deve redefini-los.

## 3. Runtime e dependências atuais do client

O único runtime do client é composto por:

```text
main.cpp
├── issp_behaviors
│   └── issp_core
├── issp_transport_154
│   └── issp_core
└── reset e composição específica da aplicação
```

- `issp_core` implementa o protocolo, a representação do dispositivo, o
  despacho e a deduplicação de comandos e a fila lógica de reports. Não depende
  do ESP-IDF.
- `issp_transport_154` implementa o rádio, os frames MAC, RX/TX, ACKs, retries,
  o executor de reports e o `Issp154NetworkManager`.
- `issp_behaviors` implementa capacidades reutilizáveis sobre as interfaces de
  `issp_core`, incluindo a saída digital.
- `main.cpp` configura o hardware e a identidade do produto, instancia e
  conecta os componentes, inicia o runtime e integra o factory reset.

O cliente anterior (`iot154`, `iot154_sensor_client` e os módulos auxiliares do
`main`) foi removido depois de confirmada sua ausência no executável. Os arquivos
privados `iot154_radio.*` de `issp_transport_154` são a implementação de baixo
nível do transporte atual e não pertencem ao cliente legado.

## 4. Responsabilidades da biblioteca ISSP

A biblioteca ISSP deve:

- inicializar e encerrar o transporte IEEE 802.15.4;
- descobrir, validar, persistir, aplicar e manter o descritor da rede, incluindo
  canal, PAN ID e endereço do coordenador;
- enviar mensagens ao coordenador e receber mensagens dele;
- interpretar o envelope do protocolo ISSP;
- encaminhar comandos ao dispositivo ou behavior responsável;
- publicar eventos e alterações de estado;
- confirmar entregas quando o protocolo exigir;
- tratar falhas temporárias de comunicação com limites de tempo e tentativas;
- permitir que a aplicação registre seus behaviors e capacidades.

## 5. Responsabilidades da aplicação

A aplicação deve:

- configurar o hardware específico e a identidade do produto;
- instanciar transporte, network manager, dispositivo, behaviors e executor de
  reports;
- registrar capacidades e behaviors;
- informar alterações de estado;
- executar ações físicas, como acionar relés ou ler sensores;
- definir regras específicas do produto;
- iniciar e encerrar o runtime da biblioteca;
- conectar o botão e o serviço de factory reset.

A aplicação não deve implementar diretamente:

- descoberta ou varredura do coordenador;
- persistência ou parsing do descritor da rede;
- montagem de frames IEEE 802.15.4;
- confirmação de entrega;
- retry de transporte;
- parsing do protocolo ISSP;
- gerenciamento direto do rádio.

## 6. Responsabilidades dos componentes

### `Issp154Transport`

É responsável pelo transporte IEEE 802.15.4. Deve:

- inicializar e encerrar o rádio com segurança;
- configurar canal e PAN ID quando estiver parado ou em commissioning;
- transmitir e receber frames;
- montar e remover o envelope MAC;
- disponibilizar ao commissioning o canal e os metadados MAC da resposta;
- entregar o payload ISSP ao runtime;
- controlar expectativas de ACK e tentativas confirmadas.

Não deve conhecer relés, sensores ou botões, aplicar regras de negócio,
interpretar capacidades específicas nem decidir quando persistir ou invalidar
uma rede.

### `Issp154NetworkManager`

Deve:

- carregar e validar o descritor persistido;
- coordenar a varredura limitada dos canais;
- solicitar discovery ao transporte;
- montar e persistir atomicamente o descritor completo;
- aplicar o descritor somente após persistência bem-sucedida;
- limpar somente o vínculo de rede quando solicitado pelo factory reset.

### `IsspDevice`

Representa o dispositivo ISSP durante a execução. Deve:

- receber e deduplicar comandos;
- localizar o behavior ou capacidade responsável;
- coordenar a publicação de estados;
- manter e preparar a fila lógica de reports pendentes;
- encaminhar reports para execução;
- manter as informações necessárias do dispositivo.

### `DigitalOutputBehavior`

Representa uma saída digital controlável. Deve:

- manter o estado lógico da saída;
- aplicar os comandos compatíveis `ON`, `OFF` e `TOGGLE`;
- acionar o hardware por meio da abstração apropriada;
- publicar o estado inicial configurado e alterações de estado quando
  necessário.

### `Issp154ReportExecutor`

É responsável por transmitir reports pendentes. Deve:

- processar reports fora do fluxo principal da aplicação;
- enviar reports utilizando o transporte;
- aguardar confirmação quando aplicável;
- executar retries limitados;
- preservar reports não entregues para tentativa posterior, sem bloquear
  permanentemente reports futuros.

## 7. Commissioning, persistência e recuperação

O commissioning está implementado e validado conforme
`ISSP-Commissioning.md`. Sem descritor válido, o client varre de forma limitada
os canais 11 a 26, com no máximo três tentativas por canal, usando o PAN curinga
`0xffff` somente durante a descoberta. Uma resposta válida fornece, pelos
metadados MAC, o PAN ID e o endereço estendido do coordenador; o canal é o que
está sendo examinado.

O descritor completo contém versão de schema, canal, PAN ID e endereço estendido
do coordenador. Ele deve ser validado, persistido atomicamente e aplicado apenas
depois da persistência bem-sucedida. Nos boots seguintes, um descritor válido é
reutilizado sem novo scan. Um descritor inválido é rejeitado, e a antiga chave
que continha apenas o endereço do coordenador não é aceita como descritor
completo.

Não há canal nem PAN ID operacional fixos em `main.cpp`; os valores zero da
configuração inicial não representam uma rede operacional. O coordenador
controla uma janela de ingresso de 60 segundos. Quando nenhuma rede é
encontrada, a inicialização termina de forma controlada com `NotReady`, sem
busca infinita ou reboot contínuo.

O factory reset por pressão contínua de 10 segundos no GPIO 9 remove o descritor
de rede, confirma a operação de persistência e reinicia o dispositivo. Após o
boot, o client retorna ao commissioning. O reset não deve apagar firmware,
dados de fábrica ou namespaces sem relação explícita com o vínculo de rede.

## 8. Estado implementado e validado

A infraestrutura forma o runtime utilizado pela aplicação e inclui:

- transporte IEEE 802.15.4 com descritor dinâmico de rede;
- commissioning limitado e boot por NVS sem scan;
- envio e recepção de payload ISSP;
- representação do dispositivo e behavior de saída digital;
- ACK e deduplicação de comandos;
- fila e executor de reports confirmados com retries;
- report inicial confirmado;
- comandos `ON`, `OFF` e `TOGGLE`;
- ausência de report otimista no coordenador;
- factory reset local e redescoberta;
- encerramento controlado com `NotReady` quando nenhuma rede é encontrada.

A refatoração funcional e a consolidação estão concluídas. A consolidação
removeu o runtime legado e reduziu a instrumentação temporária sem autorização
para alterar payload, protocolo, timeouts, retries, delays funcionais ou regras
de negócio.

## 9. Limites e trabalhos posteriores

Não fazem parte da arquitetura implementada neste recorte:

- suporte a múltiplos dispositivos em um mesmo runtime;
- deduplicação avançada de comandos além do comportamento vigente;
- telemetria detalhada;
- otimizações de consumo;
- generalização prematura de APIs;
- alterações no payload do protocolo sem necessidade demonstrada e aprovação;
- descoberta ou heartbeat executados indefinidamente;
- mudança automática de canal de uma rede já formada;
- seleção entre múltiplos coordenadores, roaming ou autenticação de ingresso;
- invalidação automática do descritor por uma única falha;
- nova política de perda do coordenador;
- implementação de novo ciclo de vida, incluindo `stop()`, sem caso de uso
  aprovado;
- empacotamento ou distribuição para outros firmwares, movimentação para outro
  repositório e publicação em registry.

O protocolo wire ainda requer uma especificação dedicada para assegurar sua
reconstruibilidade completa. Essa lacuna registrada não autoriza inferir ou
alterar layout, tipos, checksum, endianness ou compatibilidade a partir desta
consolidação.

## 10. Regra de evolução

Este documento deve evoluir conforme a implementação e a validação prática
revelarem novas necessidades, preservando as decisões vigentes e a
rastreabilidade exigida pelas diretrizes EKM.

Novas abstrações, regras ou componentes somente devem ser incorporados quando
resolverem um problema concreto identificado durante o desenvolvimento. Mudança
de protocolo, contrato, responsabilidade, comportamento ou limite arquitetural
exige especificação ou decisão normativa explícita; não deve ser introduzida
como detalhe local de implementação.
