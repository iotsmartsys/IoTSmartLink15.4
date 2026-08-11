# Relatório de validação — commissioning ISSP

**Classe da fonte:** Relatório

**Papel:** Arquiteto humano

**Especificação:** `docs/specs/ISSP-Commissioning.md`

**Revisão confrontada:** Registro histórico EKM 1.x preservado na migração para EKOM 3.2

**Estado:** Concluído

> Este relatório preserva uma atuação histórica e não altera fontes normativas.

## 12. Resultado validado

Foram comprovados em hardware os seguintes cenários:

1. descoberta do coordenador durante a janela de ingresso aberta;
2. segundo boot carregando o descritor da NVS, sem novo scan;
3. janela fechada encerrando a descoberta de forma controlada com `NotReady`;
4. factory reset removendo o descritor completo;
5. redescoberta após a reabertura da janela;
6. report inicial confirmado por ACK após commissioning.
