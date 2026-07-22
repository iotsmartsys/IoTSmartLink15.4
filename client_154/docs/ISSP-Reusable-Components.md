# ISSP — Especificação de Componentes Reutilizáveis

**Tipo:** Normativo
**Status:** Implemented and validated
**Versão:** 1.1
**Responsável arquitetural:** Marcelo Miranda
**Última atualização:** 21/07/2026
**Escopo:** Empacotamento local e prova de reutilização dos componentes ISSP

---

## 1. Objetivo

Transformar os componentes ISSP atualmente localizados dentro do firmware
`client_154` em componentes compartilhados do repositório, consumíveis por mais
de uma aplicação ESP-IDF sem copiar o firmware, duplicar fontes ou depender da
estrutura interna do client original.

O recorte deve provar a reutilização por meio de duas aplicações independentes:

1. o `client_154` funcional existente;
2. uma aplicação mínima de integração que compile e faça link com as APIs
   públicas dos três componentes.

Esta etapa não publica pacotes em registry e não cria suporte ao Arduino ou ao
PlatformIO.

---

## 2. Problema concreto

O objetivo original da refatoração é permitir a criação de firmwares diferentes
sem copiar todo o repositório ou manter variantes do mesmo firmware. Embora os
componentes novos já possuam fronteiras CMake, eles permanecem fisicamente sob
`client_154/components`, e sua reutilização por outra aplicação ainda não foi
comprovada.

O resultado esperado não é apenas reorganização de diretórios. Deve existir
evidência executável de que um consumidor independente consegue localizar,
compilar e fazer link com os componentes usando somente seus contratos públicos.

---

## 3. Estado de referência

Os componentes atuais são:

```text
client_154/components/issp_core
client_154/components/issp_transport_154
client_154/components/issp_behaviors
```

Responsabilidades vigentes:

- `issp_core`: protocolo, tipos, dispositivo, behaviors abstratos e fila lógica
  de reports; não depende do ESP-IDF;
- `issp_transport_154`: integração ESP-IDF com IEEE 802.15.4, commissioning,
  persistência NVS e executor de reports;
- `issp_behaviors`: behaviors reutilizáveis dependentes das abstrações do core e,
  quando aplicável, de drivers ESP-IDF.

O runtime, protocolo wire e comportamentos descritos em
`ISSP-Architecture.md`, `ISSP-Commissioning.md` e
`ISSP-Consolidation.md` já foram validados e devem ser preservados.

---

## 4. Requisitos

### ISSP-REUSE-001 — Localização compartilhada

Mover, sem copiar ou manter duplicatas, os três componentes para:

```text
components/issp_core
components/issp_transport_154
components/issp_behaviors
```

Depois da migração, não devem permanecer fontes equivalentes em
`client_154/components`.

### ISSP-REUSE-002 — Consumo pelo client existente

O `client_154` deve declarar o diretório compartilhado por meio da configuração
CMake suportada pelo ESP-IDF e continuar consumindo os componentes pelos mesmos
nomes:

```text
issp_core
issp_transport_154
issp_behaviors
```

O `main.cpp` não deve incluir fontes por caminho relativo nem conhecer arquivos
privados dos componentes.

### ISSP-REUSE-003 — Consumidor independente

Criar uma aplicação ESP-IDF mínima em:

```text
examples/issp_minimal_client
```

Ela deve:

- possuir `CMakeLists.txt` de projeto e componente `main` próprios;
- localizar os componentes somente pelo diretório compartilhado;
- declarar dependências pelos nomes públicos dos componentes;
- incluir headers públicos de `issp_core`, `issp_transport_154` e
  `issp_behaviors`;
- instanciar ou referenciar APIs suficientes para forçar compilação e link dos
  componentes;
- não copiar código, configuração, reset, hardware ou `main.cpp` do client
  funcional;
- compilar para ESP32-H2 usando ESP-IDF 6.0.1.

O exemplo é uma prova de integração e não precisa implementar um produto nem
ser validado em hardware neste recorte. Ele não deve transmitir automaticamente
ou alterar NVS quando executado sem configuração explícita.

### ISSP-REUSE-004 — Contratos públicos e privados

Cada componente deve expor em `include/` somente headers necessários aos seus
consumidores. Headers e funções usados exclusivamente pela implementação devem
permanecer privados em `src/` ou em diretório privado declarado no CMake.

No mínimo, devem ser classificados e documentados:

- APIs públicas do `issp_core`;
- APIs públicas de alto nível do `issp_transport_154`;
- API pública do `issp_behaviors`;
- dependências públicas e privadas de cada componente;
- restrições de contexto, concorrência e ciclo de vida já presentes nos
  contratos atuais.

O header C de baixo nível `issp154_transport.h` deve ser analisado. Se continuar
público por necessidade técnica, essa decisão e seus consumidores devem ser
documentados. Se puder se tornar privado sem alterar comportamento, a mudança é
permitida, desde que o header C++ público permaneça autocontido.

Não criar abstrações novas apenas para tornar a API esteticamente mais genérica.

### ISSP-REUSE-005 — Comportamento preservado

A movimentação e os ajustes de CMake não podem alterar:

- protocolo wire, payload, checksum ou endianness;
- canal, commissioning ou persistência;
- timeouts, retries e delays funcionais;
- ACKs, reports, comandos ou deduplicação;
- ciclo de vida do rádio;
- factory reset;
- lógica do coordenador;
- regras de negócio do client.

### ISSP-REUSE-006 — Ausência de dependência reversa

Os componentes compartilhados não podem depender de:

```text
client_154/main
client_154/main/reset
coordinator_154
examples/issp_minimal_client
```

Aplicações podem depender dos componentes; componentes não podem depender das
aplicações.

### ISSP-REUSE-007 — Documentação de consumo

Criar documentação operacional concisa em:

```text
components/README.md
```

Ela deve explicar:

- componentes disponíveis e responsabilidades;
- versões suportadas nesta etapa: ESP-IDF 6.0.1 e ESP32-H2 para a prova;
- como adicionar o diretório por CMake;
- dependências de cada componente;
- exemplo mínimo de declaração `REQUIRES`;
- limitações vigentes;
- referência às especificações normativas, sem duplicá-las.

---

## 5. Interfaces públicas mínimas

### `issp_core`

Os headers públicos vigentes incluem tipos, protocolo, interfaces de behavior e
transporte, `IsspDevice` e limites. A implementação deve preservar os nomes e
contratos usados pelo client atual.

### `issp_transport_154`

As APIs públicas de alto nível são `Issp154Transport`,
`Issp154NetworkManager` e `Issp154ReportExecutor`, junto de suas configurações e
tipos necessários. Detalhes de frame MAC, rádio e sincronização física devem
permanecer privados quando não fizerem parte de um contrato público necessário.

### `issp_behaviors`

`DigitalOutputBehavior` e sua configuração permanecem públicos. A dependência do
driver GPIO ESP-IDF é explícita e não deve ser ocultada neste recorte.

Esta seção registra o estado mínimo esperado; a inspeção pode revelar contratos
públicos adicionais indispensáveis. Qualquer ampliação material da API exige
interrupção e decisão arquitetural.

---

## 6. Fora de escopo

- publicação no ESP Component Registry;
- criação de repositório separado;
- versionamento semântico de releases distribuídas;
- suporte ao Arduino framework ou PlatformIO;
- compatibilidade com targets diferentes do ESP32-H2;
- generalização do transport para MQTT, UART ou outros meios;
- mudança do protocolo ISSP;
- novas capabilities ou behaviors;
- alterações funcionais no coordenador;
- reorganização de outros componentes do repositório;
- commit, branch, push ou pull request automáticos.

---

## 7. Validações obrigatórias

### Estruturais

- executar `git diff --check`;
- confirmar que não existem fontes ISSP duplicadas;
- confirmar que os componentes compartilhados não referenciam aplicações;
- confirmar que nenhum consumidor inclui headers privados;
- revisar separadamente diffs de código, build e documentação;
- comparar constantes e lógica antes/depois para comprovar movimentação sem
  mudança comportamental não autorizada.

### Builds

Usar ESP-IDF 6.0.1:

1. `client_154`: `idf.py reconfigure build`, target ESP32-H2;
2. `examples/issp_minimal_client`: `idf.py reconfigure build`, target ESP32-H2;
3. `coordinator_154`: `idf.py reconfigure build`, target ESP32-C6, para confirmar
   ausência de regressão indireta no repositório.

Para cada build, informar resultado, target, tamanho do binário, SHA-256 e novos
warnings.

### Hardware

A alteração é estrutural. O build do client funcional é obrigatório; teste de
hardware não é necessário para aceitar a movimentação, desde que o diff prove
ausência de mudança funcional. Se qualquer lógica for alterada, a validação em
hardware volta a ser obrigatória para o fluxo afetado.

---

## 8. Ativos de conhecimento e transação EKM

Esta especificação autoriza atualizar somente o necessário em:

- `client_154/docs/ISSP-Architecture.md`, para registrar a localização
  compartilhada e a prova de consumo;
- `docs/governance/KNOWLEDGE-MAP.md`, para atualizar fontes, implementação,
  evidências e `EKM-GAP-0004`;
- `docs/governance/EKM-CHANGELOG.md`, para manter `EKM-CHG-0004` e seu estado;
- `components/README.md`, como documento operacional de consumo;
- esta especificação, exclusivamente para atualizar status e resultado após
  validação.

Não autoriza remover ou condensar decisões normativas. Afirmações dependentes em
outras fontes devem ser revisadas e relatadas mesmo quando não precisarem de
alteração.

`EKM-CHG-0004` somente pode mudar para `Closed` quando:

- todos os requisitos desta especificação forem atendidos;
- os três builds forem aprovados;
- o mapa refletir a nova localização e evidência;
- `EKM-GAP-0004` estiver encerrada ou seu escopo restante estiver explicitamente
  separado em uma nova lacuna;
- a Definition of Done EKM for respondida integralmente.

---

## 9. Relatório obrigatório

Além do relatório definido em `EKM-GUIDELINES.md`, informar:

- matriz `ISSP-REUSE-001` a `ISSP-REUSE-007` com estado e evidência;
- arquivos movidos, criados e modificados;
- APIs classificadas como públicas e privadas;
- dependências CMake públicas e privadas;
- como cada aplicação localiza os componentes;
- prova de ausência de duplicação e dependência reversa;
- resultados dos três builds;
- mudanças em fontes normativas e dependentes revisados;
- estado de `EKM-CHG-0004` e `EKM-GAP-0004`;
- resposta à Definition of Done EKM;
- desvios, riscos e validações pendentes.

Uma relação de arquivos ou builds aprovados não substitui a comprovação de cada
requisito.

---

## 10. Resultado

Os componentes foram movidos sem duplicação para `components/`, e tanto o
`client_154` quanto `examples/issp_minimal_client` os localizam por
`EXTRA_COMPONENT_DIRS`. O exemplo inclui e referencia as APIs públicas dos três
componentes sem iniciar rádio ou executar operações de NVS.

`issp154_transport.h` permanece público porque o contrato C++
`issp154_transport.hpp` o inclui diretamente e utiliza tipos declarados pelo
ESP-IDF. Seus consumidores diretos são a implementação C e o wrapper C++ do
próprio componente. Frame MAC, rádio e demais detalhes de implementação
permanecem privados em `src/`.

Uma reauditoria comparou as cinco fontes apontadas como preexistentes com os
pós-diffs registrados pela execução de consolidação. Os hashes coincidem
exatamente na nova localização, e as três aplicações foram reconstruídas após
essa comprovação. `EKM-CHG-0004` e `EKM-GAP-0004` estão encerrados com o
histórico da reabertura preservado.
