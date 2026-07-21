ISSP — Especificação Arquitetural

Status: Em evolução
Responsável arquitetural: Marcelo Miranda
Última atualização: 20/07/2026

⸻

1. Objetivo

O ISSP é uma biblioteca para comunicação entre dispositivos embarcados utilizando IEEE 802.15.4.

A biblioteca deve abstrair os detalhes do transporte e oferecer à aplicação uma interface orientada a dispositivos, capacidades, comandos e eventos.

O objetivo é permitir que diferentes firmwares utilizem o protocolo sem replicar lógica de comunicação, confirmação, descoberta ou processamento de mensagens.

⸻

2. Princípios arquiteturais

1. A biblioteca deve ser independente do firmware específico da aplicação.
2. A aplicação não deve manipular diretamente frames IEEE 802.15.4.
3. O transporte não deve conhecer regras específicas de sensores ou atuadores.
4. As regras do dispositivo devem ser implementadas por comportamentos ou capacidades.
5. Apenas um componente deve controlar o rádio durante a execução.
6. A biblioteca deve possuir ciclo de vida explícito de inicialização e encerramento.
7. Falhas de comunicação não devem bloquear permanentemente o dispositivo.
8. Estados importantes devem ser publicados por meio de reports.
9. Comandos recebidos devem ser processados pela capacidade responsável.
10. O conhecimento permanente do protocolo deve permanecer neste documento e no código, não apenas nas conversas.

⸻

3. Responsabilidades da biblioteca

A biblioteca ISSP deve ser responsável por:

* inicializar e encerrar o transporte IEEE 802.15.4;
* descobrir e manter o descritor da rede, incluindo canal, PAN ID e endereço do
  coordenador;
* enviar mensagens ao coordenador;
* receber mensagens do coordenador;
* interpretar o envelope do protocolo ISSP;
* encaminhar comandos ao dispositivo ou capacidade correspondente;
* publicar eventos e alterações de estado;
* confirmar entregas quando o protocolo exigir;
* tratar falhas temporárias de comunicação;
* permitir que a aplicação registre seus comportamentos e capacidades.

⸻

4. Responsabilidades da aplicação

A aplicação deve ser responsável por:

* configurar o hardware específico do dispositivo;
* instanciar a biblioteca ISSP;
* registrar capacidades e comportamentos;
* informar alterações de estado;
* executar ações físicas, como acionar relés ou ler sensores;
* definir regras específicas do produto;
* iniciar e encerrar o runtime da biblioteca.

A aplicação não deve implementar diretamente:

* descoberta do coordenador;
* montagem de frames IEEE 802.15.4;
* confirmação de entrega;
* retry de transporte;
* parsing do protocolo ISSP;
* gerenciamento direto do rádio.

⸻

5. Componentes atuais

Issp154Transport

Responsável pelo transporte IEEE 802.15.4.

Deve:

* inicializar o rádio;
* transmitir frames;
* receber frames;
* remover o envelope MAC;
* entregar o payload ISSP ao runtime;
* encerrar o rádio de forma segura.

Não deve:

* conhecer relés, sensores ou botões;
* aplicar regras de negócio;
* interpretar capacidades específicas.

IsspDevice

Representa o dispositivo ISSP durante a execução.

Deve:

* receber comandos;
* localizar o comportamento ou capacidade responsável;
* coordenar a publicação de estados;
* encaminhar reports para execução;
* manter as informações necessárias do dispositivo.

DigitalOutputBehavior

Representa uma saída digital controlável.

Deve:

* manter o estado lógico da saída;
* aplicar comandos compatíveis;
* acionar o hardware por meio de uma abstração apropriada;
* publicar alteração de estado quando necessário.

Issp154ReportExecutor

Responsável por transmitir reports pendentes.

Deve:

* processar reports fora do fluxo principal da aplicação;
* enviar reports utilizando o transporte;
* aguardar confirmação quando aplicável;
* tratar falhas sem bloquear permanentemente reports futuros.

⸻

6. Estado atual

A infraestrutura principal da nova arquitetura já existe e forma o runtime
utilizado atualmente pela aplicação, incluindo:

* transporte IEEE 802.15.4;
* descoberta do coordenador em canal e PAN ID configurados;
* persistência do endereço de destino;
* envio confirmado;
* recepção de payload;
* representação de dispositivo;
* comportamento de saída digital;
* executor de reports;
* ACK e deduplicação de comandos;
* report inicial confirmado;
* reset local por pressão prolongada.

O runtime novo já substitui o cliente legado no `main.cpp`. A próxima evolução
necessária é remover a configuração fixa de canal e PAN ID por meio do fluxo de
commissioning descrito em `ISSP-Commissioning.md`.

⸻

7. Próximo objetivo de implementação

O próximo objetivo é implementar commissioning limitado no tempo:

1. o coordenador abre uma janela de ingresso após o boot;
2. o client sem configuração varre os canais IEEE 802.15.4;
3. o client descobre canal, PAN ID e endereço do coordenador;
4. o descritor completo é persistido e reutilizado;
5. o factory reset remove esse vínculo de rede.

Os requisitos, limites e critérios de aceite estão definidos em
`ISSP-Commissioning.md`.

⸻

8. Fora do escopo atual

Ainda não fazem parte do próximo recorte:

* suporte a múltiplos dispositivos em um mesmo runtime;
* deduplicação avançada de comandos;
* telemetria detalhada;
* otimizações de consumo;
* generalização prematura de APIs;
* alterações no payload do protocolo sem necessidade demonstrada;
* descoberta executada indefinidamente;
* mudança automática de canal de uma rede já formada.

⸻

9. Regra de evolução

Este documento deve evoluir conforme a implementação e a validação prática revelarem novas necessidades.

Novas abstrações, regras ou componentes só devem ser incorporados quando resolverem um problema concreto identificado durante o desenvolvimento.
