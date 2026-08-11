# Relatório de validação — componentes reutilizáveis

**Classe da fonte:** Relatório

**Papel:** Arquiteto humano

**Especificação:** `docs/specs/ISSP-Reusable-Components.md`

**Revisão confrontada:** Registro histórico EKM 1.x preservado na migração para EKOM 3.2

**Estado:** Concluído

> Este relatório preserva uma atuação histórica e não altera fontes normativas.

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
