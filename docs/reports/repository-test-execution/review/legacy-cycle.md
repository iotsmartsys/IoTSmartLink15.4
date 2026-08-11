# Relatório de revisão — política de targets e testes

**Classe da fonte:** Relatório

**Papel:** Engenheiro Revisor

**Especificação:** `docs/specs/Repository-Test-Execution-Policy.md`

**Revisão confrontada:** Registro histórico EKM 1.x preservado na migração para EKOM 3.2

**Estado:** Concluído

> Este relatório preserva uma atuação histórica e não altera fontes normativas.

## 12. Revisão técnica da implementação (Engenheiro Revisor, 01/08/2026)

Recorte revisado: implementação do commit `c2e6c41`, requisitos TESTEXEC-001 a
007, critérios TESTEXEC-AC-001 a 007, matriz de substituição e artefatos
técnicos inventariados na seção 7.

### Evidência independente

- inspeção do diff confirmou a remoção dos imports, tipos, marker e caso QEMU
  do runner raiz e a adição dos dois runners físicos ESP32-C3;
- varredura versionada fora de documentação histórica/política não encontrou
  `pytest_embedded_qemu`, `pytest.mark.qemu` nem comando `idf.py qemu`;
- contagem direta encontrou 20 `TEST_CASE` no app SmartSysApp e 13 no app do
  registry; nenhum caso foi removido pela migração;
- `py_compile` dos três runners terminou com código 0 usando cache temporário;
- os artefatos temporários da implementação foram inspecionados e identificam
  target `esp32c3`: `smart_sys_app_test.bin` com 138272 bytes e SHA-256
  `39905a299b7db6b0a26031ce273b66da893d9b7ee5589dd44196c2a6930ee2c3`, e
  `device_registry_test.bin` com 146320 bytes e SHA-256
  `e69a2684fa114b4611fbf1644e6c24c976c7876ec51529426160fa162cfb8931`;
- os dois diretórios locais `build_qemu_c3` permanecem ausentes e
  `git diff --check` não encontrou erro material.

Nenhum flash, monitor ou teste físico foi executado nesta revisão. A ausência
de `pytest` no ambiente informado pelo Implementador impede até a coleta local;
essa condição continua sendo limitação de infraestrutura, não evidência
comportamental.

### Achados materiais

1. **Alto — TESTEXEC-AC-005 permanece sem evidência terminal.** Os runners
   expressam quantidade maior que zero e oráculo terminal, mas nenhum dos 33
   casos foi coletado ou executado em ESP32-C3 físico. Afeta TESTEXEC-002,
   TESTEXEC-004, TESTEXEC-005 e TESTEXEC-AC-005 e impede promover a
   implementação para `Implemented` ou a prontidão para `Ready`.
2. **Médio — quantidade normativa SmartSysApp diverge da fonte preservada.**
   TESTEXEC-AC-005, Bootstrap v1.5 e a abertura de EKM-CHG-0009 dizem
   “dezenove”, enquanto a fonte contém 20 casos e o runner exige exatamente
   `20 Tests 0 Failures 0 Ignored`. Não houve perda de cobertura, mas a
   discrepância impede usar a quantidade nominal da especificação como oráculo
   inequívoco e deve ser reconciliada por nova atuação de autoria.

### Limitações e recomendação

A revisão está limitada à política transversal e à retirada técnica de QEMU;
não reaprova o comportamento funcional de Bootstrap ou Registry e não converte
build em execução. O ambiente ainda precisa declarar/prover `pytest` e os
plugins ESP-IDF antes que o runner possa ser coletado; flash e captura física
continuam sujeitos à ordem explícita do Arquiteto.

**Recomendação ao Arquiteto:** não aceitar nem promover a implementação nesta
rodada. Solicitar ao Autor a reconciliação da quantidade SmartSysApp e, em
atuação posterior autorizada, prover/coletar os runners e executar os 20 + 13
casos em ESP32-C3 físico. Estados preservados: normativo `Proposed`,
implementação `In Progress`, prontidão `Not Ready` e `EKM-CHG-0009` `Open`.
