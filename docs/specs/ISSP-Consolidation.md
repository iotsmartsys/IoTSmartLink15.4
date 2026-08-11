# ISSP — Especificação de Consolidação da Refatoração

**Tipo:** Normativo
**Estado normativo:** Active
**Estado da implementação:** Validated
**Versão:** 1.0  
**Responsável arquitetural:** Marcelo Miranda  
**Última atualização:** 21/07/2026
**Escopo:** Consolidação do runtime ISSP validado

---

## 1. Objetivo

Consolidar a refatoração do client ISSP após sua validação funcional em
hardware, removendo o código legado que não participa mais do runtime,
atualizando a documentação, reduzindo a instrumentação temporária e produzindo
uma árvore limpa e validada para revisão e posterior commit ou pull request.

Esta etapa não deve criar funcionalidades, alterar o protocolo wire nem mudar o
comportamento já validado.

Ao final, o firmware do client deve continuar executando exclusivamente o
runtime composto por:

```text
issp_core
+ issp_transport_154
+ issp_behaviors
+ composição da aplicação em main.cpp
```

---

## 2. Estado de referência

Antes da consolidação, os seguintes comportamentos já foram validados em
hardware e devem ser preservados:

- commissioning pelos canais 11 a 26;
- descoberta dinâmica de canal, PAN ID e coordenador;
- persistência e reutilização do descritor da rede;
- janela de ingresso de 60 segundos no coordenador;
- inicialização do runtime sem nova varredura quando existe descritor válido;
- report inicial confirmado;
- comandos `ON`, `OFF` e `TOGGLE`;
- ACK de comandos e reports;
- retry de reports;
- ausência de report otimista no coordenador;
- factory reset após pressão contínua de 10 segundos no GPIO 9;
- retorno ao commissioning após factory reset;
- encerramento controlado com `NotReady` quando nenhuma rede é encontrada.

O `main.cpp` e seu `CMakeLists.txt` já possuem uma alteração aprovada que remove
a dependência direta de `iot154_packet.h` e do componente legado `iot154`. Essa
alteração deve ser preservada.

---

## 3. Escopo obrigatório

Esta consolidação contempla exclusivamente:

1. remoção do código legado não utilizado;
2. atualização da documentação arquitetural e de commissioning;
3. redução dos logs temporários de diagnóstico;
4. builds finais do client ESP32-H2 e do coordenador ESP32-C6;
5. revisão final do diff e preparação das informações para commit/PR.

---

## 4. Remoção do código legado

### 4.1 Regra de segurança

Antes de excluir qualquer arquivo, confirmar por busca no repositório e pelo
grafo de componentes gerado pelo ESP-IDF que ele não é compilado nem
referenciado pelo runtime atual.

Não excluir um arquivo apenas por seu nome. Se algum candidato ainda estiver
referenciado pelo runtime novo, interromper essa remoção específica, registrar
a evidência e manter o arquivo.

O histórico Git é o mecanismo de recuperação. Não criar diretório `archive`,
duplicar o legado ou manter arquivos renomeados como `.old` ou `.txt`.

### 4.2 Candidatos legados do client

Após confirmar que continuam sem referências ativas, remover:

```text
client_154/components/iot154/
client_154/main/main_old.c.txt
client_154/main/radio/
client_154/main/storage/
client_154/main/hardware/
client_154/main/metrics/
client_154/main/power/
client_154/main/config/
client_154/main/Kconfig.projbuild
```

Esses arquivos pertencem ao client anterior e não devem ser migrados para os
novos componentes durante esta consolidação.

### 4.3 Código que deve permanecer

Preservar integralmente, salvo redução de logs ou atualização documental
expressamente prevista nesta especificação:

```text
components/issp_core/
components/issp_transport_154/
components/issp_behaviors/
client_154/main/main.cpp
client_154/main/reset/
coordinator_154/
```

Esses caminhos registram a baseline histórica da consolidação. A evolução
posterior moveu o entrypoint para `client_154/main/app_main.cpp` e o factory
reset para `components/issp_app_154/src/reset/`; o mapa e o dossiê localizam o
estado vigente.

### 4.4 Verificações após remoção

Confirmar que:

- o client não inclui `iot154_packet.h`;
- o `main` não declara `REQUIRES iot154`;
- o componente `iot154` não aparece na lista de componentes do build;
- não existem referências a `iot154_sensor_client_*`;
- não existem arquivos de backup do main legado;
- canal e PAN ID não voltaram a ser constantes operacionais no client;
- a remoção não alterou o payload ISSP nem os builders/parsers atuais.

Os nomes internos `iot154_radio.*` em
`components/issp_transport_154/src/`, usados como implementação privada de
baixo nível pelo transporte atual, não são o cliente legado e não devem ser
removidos ou renomeados neste recorte.

---

## 5. Atualização da documentação

### 5.1 `ISSP-Architecture.md`

Atualizar o documento para registrar:

- commissioning como implementado e validado, não como próximo objetivo;
- runtime novo como único runtime do client;
- remoção do cliente legado;
- dependências atuais entre `issp_core`, `issp_transport_154`,
  `issp_behaviors` e a aplicação;
- factory reset e descritor dinâmico da rede como funcionalidades concluídas;
- ausência de canal e PAN ID fixos no `main.cpp`;
- estado da refatoração funcional como concluído;
- consolidação como etapa encerrada após o cumprimento desta especificação;
- empacotamento/distribuição para outros firmwares como trabalho posterior e
  separado.

Remover ou reescrever as seções que ainda apresentam commissioning como
trabalho futuro.

### 5.2 `ISSP-Commissioning.md`

Alterar o status de proposta para implementada e validada.

Adicionar uma seção curta de resultado contendo os cenários comprovados:

1. descoberta durante janela aberta;
2. segundo boot usando NVS sem scan;
3. janela fechada retornando `NotReady` de forma controlada;
4. factory reset removendo o descritor;
5. redescoberta após reabertura da janela;
6. report inicial com ACK após commissioning.

Não transformar o documento em relatório cronológico de logs.

### 5.3 Consistência

Verificar que nenhum documento ativo afirme que:

- o cliente legado ainda é o runtime principal;
- o endereço do coordenador é a única informação persistida;
- canal e PAN ID são fixos no client;
- commissioning ainda não foi implementado.

Não alterar a RFC do piloto nem documentos históricos apenas para reescrever o
passado. Documentos históricos podem continuar descrevendo o contexto de sua
época quando estiver claro que são registros históricos.

---

## 6. Redução dos logs temporários

### 6.1 Princípio

Remover a instrumentação detalhada criada para diagnosticar RX, TX, ACK,
concorrência e commissioning. Preservar logs operacionais úteis e todos os
warnings e erros que ajudam a diagnosticar falhas reais.

A redução de logs não pode alterar:

- condições;
- ordem de execução;
- delays;
- callbacks;
- locks ou critical sections;
- estados internos;
- retries;
- timeouts;
- resultados retornados.

Não realizar refatoração estrutural enquanto remove logs.

### 6.2 Client — remover ou rebaixar

Remover os logs de sucesso por frame, tentativa ou transição interna das tags:

```text
PHY_TX
RADIO_RX
RX_QUEUE
MAC_PARSE
TRANSPORT_RX
DEVICE_RX
DEVICE_DISPATCH
COMMAND_DECODE
BEHAVIOR_MATCH
COMMAND_ACK
REPORT_TX
REPORT_ACK
REPORT_EXECUTOR
DIGITAL_OUTPUT_ACCEPT
DIGITAL_OUTPUT_HANDLE
```

Quando um desses pontos possuir diagnóstico de erro necessário, preservar uma
mensagem concisa em `WARN` ou `ERROR`, sem imprimir payload completo.

Em particular:

- preservar aviso de fila RX cheia ou frame descartado;
- preservar falha de parsing somente em nível que não cause spam operacional;
- preservar falha definitiva de TX, timeout após esgotar retries e ACK
  incompatível quando relevante;
- preservar falha de execução de behavior ou de alteração do GPIO;
- preservar agendamento de retry apenas como aviso conciso;
- remover logs de `begin`, `checking`, `prepared`, callbacks bem-sucedidas e
  resultados `Ok` por tentativa.

Os `std::printf` temporários do `issp_core` devem ser removidos ou substituídos
somente quando for necessário manter um warning/error. O `issp_core` não deve
ganhar dependência do ESP-IDF apenas para manter logs.

### 6.3 Client — preservar

Preservar logs operacionais concisos para:

```text
ISSP network initialized
ISSP runtime started
COMMISSIONING: persisted_network loaded ...
COMMISSIONING: scan started ...
COMMISSIONING: network discovered ...
COMMISSIONING: network persisted
COMMISSIONING: scan completed result=not_found
RESET_BUTTON: initialized ...
RESET_BUTTON: countdown completed ...
FACTORY_RESET: requested
FACTORY_RESET: begin
FACTORY_RESET: completed ...
```

Também preservar erros de inicialização, persistência, shutdown e factory
reset.

### 6.4 Coordenador — remover ou rebaixar

Remover os logs detalhados de sucesso por frame das tags:

```text
COORD_RADIO_RX
COORD_MAC_PARSE
COORD_PROTOCOL_RX
COORD_REPORT
COORD_REPORT_ACK
```

Preservar warnings/errors concisos para:

- buffer RX ocupado, sobrescrita ou descarte;
- frame ou protocolo inválido quando operacionalmente relevante;
- falha de construção ou transmissão de ACK;
- timeout de ACK de comando;
- comando para destino desconhecido;
- falha real de atualização do registry.

Preservar os logs funcionais já existentes que informam, sem otimismo:

- report efetivamente recebido;
- comando confirmado ou falhado;
- dispositivo desconhecido;
- abertura e fechamento da janela de ingresso.

### 6.5 Commissioning

Preservar os logs mínimos definidos em `ISSP-Commissioning.md`.

Não remover o delay funcional de turnaround de 20 ms usado nas respostas de
discovery e nos ACKs de report. Ele não é instrumentação.

---

## 7. Validação estática e estrutural

Executar após as alterações:

```bash
git diff --check
```

Realizar buscas que comprovem:

- ausência de referências ao cliente legado;
- ausência da dependência `iot154` no client novo;
- ausência das tags temporárias de sucesso listadas nesta especificação;
- permanência dos logs operacionais mínimos;
- permanência dos componentes novos no executável.

Inspecionar o diff completo para confirmar que nenhuma constante de protocolo,
timeout, retry, delay funcional, FCF, formato de frame, endereço ou regra de
negócio foi alterada acidentalmente.

---

## 8. Builds finais

Usar ESP-IDF 6.0.1.

### 8.1 Client

Executar no `client_154`:

```bash
idf.py reconfigure build
```

Target esperado: ESP32-H2.

### 8.2 Coordenador

Executar no `coordinator_154`:

```bash
idf.py reconfigure build
```

Target esperado: ESP32-C6.

### 8.3 Evidências obrigatórias

Para cada firmware informar:

- resultado do build;
- target;
- tamanho do binário;
- SHA-256;
- warnings novos;
- quantidade de espaço livre na menor partição de aplicação.

Nenhum warning novo é aceitável. Warnings preexistentes devem ser listados sem
ser corrigidos fora do escopo.

---

## 9. Revisão final e preparação de commit/PR

### 9.1 Revisão

Ao final:

1. executar `git status --short`;
2. executar `git diff --check`;
3. revisar `git diff --stat`;
4. revisar o diff completo de todos os arquivos modificados e removidos;
5. separar claramente mudanças desta consolidação de alterações preexistentes;
6. confirmar que nenhum artefato de build, `sdkconfig.old`, log ou arquivo
   temporário foi incluído.

Não reverter alterações preexistentes aprovadas. Não usar `git reset`,
`checkout` destrutivo ou limpeza ampla do worktree.

### 9.2 Preparação

Produzir:

- resumo executivo da mudança;
- lista de arquivos modificados;
- lista de arquivos removidos;
- decisões de consolidação;
- validações realizadas;
- riscos ou pendências;
- sugestão de título de commit;
- sugestão de descrição de PR;
- checklist de validação em hardware.

Título sugerido, sujeito ao resultado real:

```text
refactor: consolidate reusable ISSP client runtime
```

### 9.3 Limite de autorização

Não executar `git add`, `git commit`, `git push`, criar branch ou abrir pull
request sem autorização explícita posterior do arquiteto.

---

## 10. Validação em hardware após consolidação

O Codex deve preparar o checklist, mas a execução física pertence ao arquiteto.

Checklist mínimo:

1. boot com descritor persistido, sem scan;
2. comando `ON` e `OFF` com ACK;
3. report de estado recebido pelo coordenador;
4. ausência de report otimista com o client desligado;
5. factory reset por 10 segundos;
6. scan sem janela aberta terminando em `NotReady`;
7. redescoberta com janela aberta;
8. report inicial confirmado após commissioning.

---

## 11. Fora de escopo

- novas funcionalidades;
- alteração do protocolo wire;
- alteração de canal, PAN ID ou política de commissioning;
- mudança dos timeouts, retries ou delays validados;
- novo mecanismo de heartbeat;
- nova política de perda de coordenador;
- autenticação ou criptografia;
- suporte a novos behaviors;
- generalização das APIs;
- implementação de `stop()` ou novo ciclo de vida sem caso de uso aprovado;
- movimentação dos componentes para outro repositório;
- publicação no ESP-IDF Component Registry;
- criação efetiva de commit, branch ou PR.

Esses itens pertencem à etapa posterior de empacotamento e distribuição.

---

## 12. Critérios de conclusão

A consolidação estará concluída quando:

- todo código legado confirmado como morto tiver sido removido;
- o runtime novo não depender do componente legado `iot154`;
- a documentação refletir o estado implementado e validado;
- a instrumentação temporária tiver sido removida sem perda dos logs
  operacionais mínimos;
- client e coordenador compilarem para seus targets corretos sem warnings
  novos;
- `git diff --check` passar;
- o diff final estiver revisado e limitado ao escopo;
- o relatório de entrega e o checklist de hardware estiverem prontos;
- nenhuma operação de Git externa à preparação tiver sido executada.

---

## 13. Evidências exigidas da consolidação

O relatório separado deve preservar, nesta ordem:

1. resultado executivo;
2. arquivos modificados;
3. arquivos removidos;
4. confirmação da remoção do legado;
5. política final de logs;
6. documentação atualizada;
7. build do client;
8. build do coordenador;
9. warnings;
10. revisão de escopo;
11. riscos e pendências;
12. título sugerido de commit;
13. descrição sugerida de PR;
14. checklist para validação em hardware.

Não avançar para empacotamento ou distribuição dos componentes.
