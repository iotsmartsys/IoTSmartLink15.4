# Análise inicial de implementabilidade — deep sleep do client

**Classe da fonte:** Relatório

**Papel:** Autor da Especificação com análise inicial de implementabilidade

**Especificação:** `docs/specs/Client-Deep-Sleep.md`

**Revisão confrontada:** v0.1, Draft de 11/08/2026

**Estado:** Concluído com bloqueadores normativos

**Capacidade:** Autor da Especificação com análise inicial

**Data:** 11/08/2026

**Resultado:** Plausível com bloqueadores normativos

> Este relatório registra evidências e recomendações. Não altera a fonte
> normativa, não promove estado e não autoriza implementação ou testes.

## Evidências observadas

- `SmartSysApp` já é a fachada pública e aceita configurações somente antes de
  `setup()`.
- O product firmware já combina regras próprias com recursos oferecidos pelo
  board model.
- O CMake já valida recursos físicos exigidos e oferecidos por composição.
- O `Door Sensor Battery H2` ainda não declara recurso de LED.
- `setup()` termina em `Running` depois de iniciar o report executor.
- `Issp154ReportExecutor::run()` é uma task sem operação de parada e repete
  reports retryable indefinidamente, com delay de 1000 ms.
- `IsspDevice` expõe contagem de reports pendentes, mas o contrato atual não
  demonstra sozinho que não há report reservado ou transmissão em andamento.
- `Issp154Transport` possui `end()`, usado hoje em rollback, mas não existe
  encerramento coordenado do runtime depois de `Running`.
- A especificação vigente de `SmartSysApp` declara que não há `stop()` e exige
  lifetime até reboot devido às tasks conservarem referências.
- O sleep atualmente exposto pelo transporte é sleep do rádio e não equivale a
  deep sleep do ESP32-H2.

## Componentes impactados

| Área | Impacto esperado |
|---|---|
| API pública | Nova configuração aditiva e resultados de preparação/sleep |
| Fachada | Validação, causa de wakeup, LED, timer RTC e coordenação |
| Report executor | Operação terminal de parada e observação de in-flight |
| Device | Estabilização de producers e estado dos reports |
| Transporte | Encerramento seguro após TX/ACK terminal |
| Product firmware | Opt-in e política temporal |
| Board model/CMake | Recurso físico `wake_led`, GPIO e polaridade |
| Testes | Fakes de RTC/GPIO/sleep e futura evidência física H2 |

## Restrições

- somente ESP32-H2 é target físico admitido para `client_154`;
- testes e hardware não podem ser executados sem especificação futura;
- Kconfig não governa lógica de componentes compartilhados;
- product firmware e board model preservam responsabilidades distintas;
- deep sleep reinicia o firmware e não preserva automaticamente estado volátil;
- não se pode declarar entrega de report apenas porque a fila parece vazia sem
  excluir reserva ou transmissão concorrente.

## Bloqueadores

1. não há decisão para o gatilho de entrada em deep sleep;
2. não há orçamento terminal para retry/report e `NotReady`;
3. não há ciclo de parada das tasks iniciadas por `SmartSysApp`;
4. o conjunto de causas de boot que acende o LED não foi confirmado;
5. não foi escolhido o product firmware nem confirmado board/LED da primeira
   implementação.

## Experimentos necessários antes de validação

- build H2 de configurações habilitada e desabilitada;
- injeção controlada das APIs de wakeup, GPIO e deep sleep para verificar
  ordem, falhas e conversão sem iniciar hardware;
- execução física autorizada para timer, polaridade, duração do LED, corrente e
  causa de wakeup;
- falhas controladas de ACK e coordenador ausente para confrontar o orçamento
  acordado;
- inspeção de GPIO em transição para confirmar ausência de pulso incompatível.

Leitura de código não certifica esses comportamentos físicos. Até a decisão dos
bloqueadores e a execução dos experimentos por ordem futura, o resultado
permanece `Not Executed` para comportamento real.

## Recomendação

Manter a especificação em `Draft`. Após decisão do Arquiteto sobre as
pendências da seção 12, atualizar primeiro o contrato, complementar esta
análise e somente então avaliar promoção para `Implementable`.
