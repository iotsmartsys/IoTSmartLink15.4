# Diagnóstico de hardware — reports após deep sleep não chegam ao host

**Classe da fonte:** Relatório

**Papel:** Engenheiro Analista

**Especificação:** `docs/specs/Client-Deep-Sleep.md` v0.11

**Revisão confrontada:** código em `641c3fd` e logs de hardware fornecidos pelo
Arquiteto

**Estado:** Concluída

**Data:** 12/08/2026

**Origem da evidência:** logs de execução em hardware fornecidos pelo
Arquiteto; nenhuma execução de build, teste, flash, monitor ou hardware foi
realizada pelo Analista

## 1. Resultado

O coordenador encaminha ao host o primeiro report recebido no boot corrente do
coordenador, mas descarta como duplicados os reports dos boots seguintes do
client. O deep sleep reinicia o client e recria `IsspDevice` com a sequência de
report em zero; o coordenador permanece ligado e conserva em RAM o último valor
zero. Como a deduplicação usa apenas o slot do device e a sequência ISSP, cada
novo report com sequência zero é indistinguível de uma retransmissão do
primeiro.

O defeito observado não está no preparo de EXT1 nem, pela evidência disponível,
na emissão JSON da primeira ocorrência. Ele expõe uma ausência de contrato
entre identidade de report, reinício do produtor e deduplicação no coordenador.

## 2. Evidência dos logs

A primeira recepção é classificada como nova:

```text
DATA new dev=0x15400001 seq=0 endpoint=1 event=1 value=0
```

Nesse ramo, `coordinator_154/main/main.c` chama `host_send_event()`, que forma o
evento `direction="evt"` e o escreve como JSON-lines na UART1, TX GPIO16. A
saída não aparece no monitor USB usado nos logs, porque o console e a ponte com
o host são interfaces distintas.

As cinco recepções posteriores são classificadas assim:

```text
DATA duplicate dev=0x15400001 seq=0
```

Todas também chegam com sequência MAC zero. Isso é compatível com boots
distintos do client: tanto o contador ISSP quanto o contador MAC são recriados.
O coordenador envia ACK novamente para cada duplicata, conforme o contrato
vigente, mas não chama `host_send_event()` nesse ramo.

## 3. Prova no código

- `IsspDevice::IsspDevice()` inicializa `reportSequence_` com zero;
- `preparePendingReport()` atribui a sequência corrente e a incrementa somente
  na RAM desse boot;
- deep sleep reinicia o firmware e não existe epoch de boot nem restauração da
  sequência no contrato do report;
- `is_known_data_duplicate()` no coordenador guarda somente
  `s_last_seq[registry_index]` e considera duplicado qualquer novo frame cuja
  sequência seja igual à última;
- `device_registry_policy_data()` define `emit_host_event = !duplicate`;
- o ramo `DATA duplicate` envia ACK, mas não emite evento ao host.

O contrato `COORD-REG-AC-006` confirma a deduplicação dentro de uma execução do
coordenador e só libera a mesma sequência depois do reboot do próprio
coordenador. Ele não define como distinguir um reboot apenas do client.

## 4. Observações separadas

O monitor registra checksum diferente entre o aplicativo gravado e o ELF
fornecido ao `idf_monitor`: o firmware em execução anuncia a versão `0ca1d97`,
enquanto a árvore confrontada está em `641c3fd`. Isso reduz a confiabilidade de
decodificação de endereços, mas não explica o achado: `main.c` do coordenador
não mudou entre essas revisões no caminho de deduplicação e encaminhamento.

Os logs provam que o coordenador concluiu a transmissão física de cada ACK;
não provam que o client o recebeu. Porém, a repetição de sequência MAC zero em
todas as recepções aponta para novos boots, não para retransmissões dentro do
mesmo boot. Logs do client seriam necessários para concluir separadamente a
correlação de ACK.

Se o host não recebeu nem mesmo o primeiro evento, há uma segunda investigação
depois da decisão `emit_host_event`: ligação da UART1, TX GPIO16, baud 115200,
consumidor e parser. O código ignora o retorno de `uart_write_bytes()`, de modo
que os logs atuais provam a chamada lógica, não a entrega física dos bytes ao
host.

## 5. Fronteira e encaminhamento

A capacidade ausente é uma identidade de report válida entre reinícios do
produtor. Ela satisfaz o teste de pré-requisito arquitetural:

1. não existe na baseline: sequência e deduplicação não possuem epoch de boot;
2. pode receber contrato e validação próprios, independentemente de deep sleep;
3. afeta protocolo, client, coordenador e qualquer consumidor sujeito a reboot
   isolado do client, inclusive com deep sleep desabilitado.

Portanto, não recomendo corrigir isso como detalhe local da v0.11. O Arquiteto
deve reabrir a conclusão funcional afetada e autorizar uma análise arquitetural
abrangente, seguida de especificação preparatória para identidade e
deduplicação de reports entre boots. Essa preparação deve confrontar ao menos:

- epoch/session no protocolo wire;
- sequência inicial fornecida pelo client e sua sobrevivência ou renovação em
  deep sleep, cold boot e perda de energia;
- deduplicação de retransmissão dentro da sessão;
- compatibilidade e migração dos dois firmwares;
- comportamento de primeiro report após reboot isolado de cada lado;
- custo de persistência, desgaste de flash e colisão após wrap ou reset.

Remover deduplicação, aceitar toda sequência zero ou usar somente valor, tempo
ou sequência MAC como heurística não distingue com segurança retransmissão de
novo report e não deve ser adotado sem contrato.

## 6. Experimentos que discriminam os caminhos

Sem alterar firmware, um confronto posterior pode:

1. manter o client parado, reiniciar somente o coordenador e provocar um report;
   o próximo `seq=0` deve voltar a aparecer como `DATA new` e gerar um evento;
2. observar diretamente UART1 TX GPIO16 para confirmar o JSON do primeiro
   `DATA new`;
3. capturar simultaneamente logs do client para distinguir `ack_matched`,
   timeout e cada novo boot.

Essas execuções exigem autorização própria. O Analista não as realizou.

## 7. Classificação

**Não pronta — pré-requisito arquitetural** [`Not Ready — Architectural
Prerequisite`].

A classificação se aplica à correção necessária para garantir encaminhamento
de reports entre boots. O wakeup físico observado pode estar funcionando, mas
a funcionalidade ponta a ponta não pode ser considerada concluída enquanto o
coordenador suprimir reports legítimos de novos boots.
