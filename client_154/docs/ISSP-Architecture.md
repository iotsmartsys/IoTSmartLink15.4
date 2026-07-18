ISSP — Especificação Arquitetural

Status: Em evolução
Responsável arquitetural: Marcelo Miranda
Última atualização: 17/07/2026

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
* descobrir e manter o endereço do coordenador;
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

A infraestrutura principal da nova arquitetura já existe, incluindo:

* transporte IEEE 802.15.4;
* descoberta do coordenador;
* persistência do destino;
* envio confirmado;
* recepção de payload;
* representação de dispositivo;
* comportamento de saída digital;
* executor de reports.

Entretanto, esses componentes ainda não formam um runtime completo utilizado pela aplicação.

A implementação legada ainda possui responsabilidades que deverão ser migradas gradualmente para a nova arquitetura.

⸻

7. Próximo objetivo de implementação

O próximo objetivo é compor o runtime mínimo da biblioteca na aplicação cliente.

Esse runtime deverá:

1. inicializar o transporte;
2. localizar ou descobrir o coordenador;
3. criar o IsspDevice;
4. registrar um DigitalOutputBehavior;
5. iniciar o Issp154ReportExecutor;
6. receber um comando remoto;
7. alterar fisicamente a saída;
8. publicar o novo estado.

A implementação legada não deverá ser removida antes da validação desse fluxo completo.

⸻

8. Fora do escopo atual

Ainda não fazem parte do próximo recorte:

* remoção definitiva do cliente legado;
* suporte a múltiplos dispositivos em um mesmo runtime;
* deduplicação avançada de comandos;
* telemetria detalhada;
* otimizações de consumo;
* generalização prematura de APIs;
* alterações no protocolo sem necessidade demonstrada.

⸻

9. Regra de evolução

Este documento deve evoluir conforme a implementação e a validação prática revelarem novas necessidades.

Novas abstrações, regras ou componentes só devem ser incorporados quando resolverem um problema concreto identificado durante o desenvolvimento.