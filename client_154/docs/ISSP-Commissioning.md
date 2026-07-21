# ISSP 802.15.4 — Especificação de Commissioning

**Status:** Proposta para implementação  
**Versão:** 1.0  
**Responsável arquitetural:** Marcelo Miranda  
**Última atualização:** 20/07/2026

---

## 1. Objetivo

Definir como um client ISSP localiza uma rede IEEE 802.15.4, obtém seus
parâmetros operacionais e os preserva para as inicializações seguintes.

O commissioning deve eliminar a necessidade de fixar no firmware do client:

- o canal IEEE 802.15.4;
- o PAN ID;
- o endereço IEEE do coordenador.

O processo deve ser limitado no tempo. O coordenador não deve permanecer
indefinidamente aberto ao ingresso de novos dispositivos.

---

## 2. Princípios

1. O coordenador é a autoridade sobre o canal e o PAN ID da rede.
2. O client persiste um descritor completo da rede, e não apenas o endereço do
   coordenador.
3. Dispositivos já provisionados continuam operando quando a janela de ingresso
   estiver fechada.
4. A descoberta possui duração e número de tentativas limitados.
5. Falha de descoberta não deve causar reboot contínuo nem busca infinita.
6. Factory reset remove todo o descritor da rede.
7. O payload ISSP existente deve permanecer compatível; canal e PAN ID serão
   obtidos dos metadados do transporte IEEE 802.15.4.

---

## 3. Descritor persistente da rede

O client deve persistir atomicamente um descritor equivalente a:

```cpp
struct Issp154NetworkDescriptor
{
    std::uint8_t schemaVersion;
    std::uint8_t channel;
    std::uint16_t panId;
    std::array<std::uint8_t, 8> coordinatorExtendedAddress;
};
```

Regras de validação:

- `schemaVersion` deve ser reconhecida;
- `channel` deve estar entre 11 e 26;
- `panId` não pode ser o PAN curinga `0xffff`;
- o endereço do coordenador não pode ser nulo, broadcast ou possuir tamanho
  diferente de 8 bytes;
- um blob ausente inicia commissioning;
- um blob presente, porém inválido, deve ser rejeitado e registrado como erro de
  persistência, sem ser utilizado parcialmente.

O descritor substituirá progressivamente a chave atual que contém somente os
8 bytes do endereço do coordenador. A migração da chave existente deve ser
explícita: ela não contém canal e PAN ID suficientes e, portanto, não pode ser
considerada um descritor completo.

---

## 4. Janela de ingresso do coordenador

Ao iniciar, o coordenador deve abrir automaticamente uma janela de ingresso de
60 segundos.

Durante a janela:

- recebe `DISCOVERY_REQ` destinadas ao PAN curinga;
- valida o frame, a versão, o checksum e os campos ISSP;
- responde com `DISCOVERY_RESP` unicast para o endereço IEEE de origem;
- utiliza na resposta o PAN ID operacional do coordenador;
- mantém o mesmo canal operacional durante toda a janela.

Após 60 segundos:

- deixa de responder a novas `DISCOVERY_REQ`;
- continua processando reports, ACKs e comandos de dispositivos conhecidos;
- não remove dispositivos registrados;
- registra de forma objetiva que a janela foi encerrada.

O estado mínimo do coordenador será:

```text
Boot
→ JoinWindowOpen
→ 60 segundos
→ JoinWindowClosed
```

Uma forma explícita de reabrir a janela sem reiniciar o coordenador poderá ser
adicionada posteriormente, mas não faz parte do primeiro recorte.

---

## 5. Descoberta no client

Quando não houver descritor válido na NVS, o client entra em modo de
commissioning.

Sequência inicial de canais:

```text
11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26
```

Em cada canal, o client deve:

1. configurar o rádio temporariamente para commissioning;
2. usar o PAN curinga `0xffff` para a solicitação broadcast;
3. enviar `DISCOVERY_REQ` com seu endereço IEEE como origem;
4. aguardar uma `DISCOVERY_RESP` por tempo limitado;
5. realizar no máximo três tentativas antes de avançar para o próximo canal.

A resposta somente será aceita quando:

- for recebida no canal atualmente examinado;
- tiver tipo `DISCOVERY_RESP`;
- possuir versão e checksum válidos;
- corresponder ao `deviceId` e à sequence da solicitação ativa;
- possuir status `Ok`;
- tiver endereço IEEE de origem válido;
- informar, pelo cabeçalho MAC, um PAN ID operacional diferente de `0xffff`.

Ao aceitar a resposta, o client compõe o descritor usando:

- canal: canal atualmente selecionado pelo scanner;
- PAN ID: PAN ID de origem extraído do cabeçalho MAC da resposta;
- coordenador: endereço IEEE de origem extraído do cabeçalho MAC da resposta.

Não é necessário adicionar canal ou PAN ID ao payload ISSP da
`DISCOVERY_RESP`.

---

## 6. Persistência e ativação

Após uma descoberta válida:

```text
descoberta
→ validação do descritor
→ persistência atômica
→ aplicação de channel e PAN ID no transporte
→ configuração do destino
→ runtime operacional
```

O transporte somente pode ser considerado pronto para o runtime depois que o
descritor completo tiver sido persistido e aplicado com sucesso.

Se a persistência falhar:

- o descritor não deve ser aplicado parcialmente;
- o erro deve ser registrado;
- o firmware deve permanecer em estado seguro e não iniciar fluxos dependentes
  da rede.

---

## 7. Inicializações posteriores

Quando houver descritor válido:

```text
Boot
→ carrega descritor
→ configura channel e PAN ID
→ configura endereço do coordenador
→ inicia runtime
→ envia report inicial confirmado
```

O report inicial funciona como verificação operacional do coordenador e como
atualização do registry, mas não faz parte do commissioning.

Falha no report inicial não deve apagar imediatamente o descritor nem iniciar
uma varredura automática no mesmo recorte. A política de perda ou mudança da
rede será especificada separadamente para evitar invalidação por falhas
transitórias.

---

## 8. Factory reset

O factory reset solicitado pelo botão local deve:

1. apagar o descritor completo da rede;
2. confirmar a conclusão da operação de persistência;
3. executar reinicialização controlada.

Após reiniciar, o client volta ao modo de commissioning.

O factory reset não deve apagar firmware, dados de fábrica nem namespaces sem
relação explícita com o vínculo de rede.

---

## 9. Responsabilidades dos componentes

### `Issp154Transport`

- configurar e alterar canal e PAN ID quando estiver parado ou em modo próprio
  de commissioning;
- transmitir e receber frames;
- disponibilizar canal e metadados MAC da resposta;
- não decidir quando persistir ou invalidar uma rede.

### `Issp154NetworkManager`

- carregar e validar o descritor;
- coordenar a varredura de canais;
- solicitar descoberta ao transporte;
- montar e persistir o descritor completo;
- aplicar o descritor somente após persistência bem-sucedida;
- substituir a responsabilidade atual de `Issp154DestinationManager`.

### Coordenador

- controlar a janela de ingresso;
- responder discovery somente enquanto a janela estiver aberta;
- anunciar implicitamente seu PAN ID pelo cabeçalho MAC da resposta;
- continuar atendendo dispositivos conhecidos após fechar a janela.

### Aplicação (`main.cpp`)

- instanciar e conectar os componentes;
- fornecer políticas configuráveis, como duração da janela quando aplicável;
- não executar varredura, persistência ou parsing de discovery diretamente.

---

## 10. Logs operacionais mínimos

Coordenador:

```text
COMMISSIONING: join_window opened duration_s=60
COMMISSIONING: discovery accepted ...
COMMISSIONING: discovery ignored reason=join_window_closed
COMMISSIONING: join_window closed
```

Client:

```text
COMMISSIONING: persisted_network loaded channel=... pan_id=...
COMMISSIONING: scan started channels=11-26
COMMISSIONING: channel=... attempt=...
COMMISSIONING: network discovered channel=... pan_id=... coordinator=...
COMMISSIONING: network persisted
COMMISSIONING: scan completed result=not_found
```

Não devem ser registrados payloads completos nem dados sem relação com o
diagnóstico do commissioning.

---

## 11. Critérios de aceite

1. Client sem descritor encontra o coordenador em qualquer canal entre 11 e 26
   durante a janela aberta.
2. Channel, PAN ID e endereço IEEE são persistidos e reutilizados no boot
   seguinte sem nova varredura.
3. O coordenador deixa de responder discovery após 60 segundos, mas mantém o
   tráfego operacional dos clients conhecidos.
4. Client iniciado sem descritor após o fechamento da janela encerra a
   varredura de forma controlada e não entra em loop infinito.
5. Factory reset remove o descritor e força novo commissioning no boot seguinte.
6. O payload ISSP atual permanece inalterado.
7. Nenhum valor fixo de canal ou PAN ID é necessário no `main.cpp` do client.
8. Builds do client ESP32-H2 e do coordenador ESP32-C6 passam sem warnings novos.

---

## 12. Fora do escopo inicial

- seleção automática do melhor canal pelo coordenador;
- mudança de canal de uma rede já formada;
- múltiplos coordenadores e seleção por prioridade;
- reabertura remota ou por botão da janela;
- autenticação criptográfica de ingresso;
- roaming;
- invalidação automática do descritor por uma única falha;
- discovery ou heartbeat executados indefinidamente.

---

## 13. Sequência recomendada de implementação

1. Introduzir o descritor persistente e sua validação, ainda sem varredura.
2. Permitir reconfiguração controlada de channel e PAN ID no transporte.
3. Implementar scanner limitado de canais no novo `Issp154NetworkManager`.
4. Implementar a janela de ingresso temporária no coordenador.
5. Integrar o manager ao `main.cpp`, removendo channel e PAN ID fixos.
6. Conectar o factory reset à remoção do descritor.
7. Executar os cenários completos de commissioning e regressão do runtime.

