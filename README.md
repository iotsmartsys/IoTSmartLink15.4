# IoTSmartLink15.4

Repositório dos dois alvos de uma solução ISSP sobre IEEE 802.15.4:

- `client_154`: firmware ESP32-H2 com variantes de produto selecionadas por
  `menuconfig`;
- `coordinator_154`: coordenador ESP32-C6 com registry persistente e ponte
  JSON-lines/UART para o host.

Os alvos são fisicamente separados e conectados pelo protocolo ISSP. O client
reutiliza os componentes de `components/`; não existe dependência de código de
aplicação entre client e coordenador.

## Navegação

- visão do sistema: `docs/specs/SYSTEM-DOSSIER.md`;
- mapa de autoridade, árvore e relações: `docs/rfc/KNOWLEDGE-MAP.md`;
- especificações: `docs/specs/`;
- decisões arquiteturais: `docs/adr/`;
- relatórios e evidências históricas: `docs/reports/`;
- instruções para agentes: `AGENTS.md`.

## Targets e validação

Somente ESP32-H2 e ESP32-C6 são admitidos, com vínculo client→H2 e
coordenador→C6. ESP32-C3 e QEMU não são suportados. As suítes permanecem
versionadas, mas sua execução depende de autorização em especificação futura,
conforme `docs/specs/Repository-Test-Execution-Policy.md`.

## Projeto da raiz

O projeto ESP-IDF na raiz é um diagnóstico herdado do template `hello_world`,
vinculado ao ESP32-H2. Ele não representa um produto nem amplia os targets do
repositório. Seu propósito definitivo permanece em `EKM-GAP-0007`.
