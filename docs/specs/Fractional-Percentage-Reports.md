# Reports percentuais fracionários de luminosidade e bateria

**ID:** `EKOM-FRACTIONAL-001`

**Versão:** 0.1

**Estado:** Rascunho [`Draft`] — dependente da preparação arquitetural
`ISSP-Typed-Values.md` v0.1 implementada e validada.

**Especificação coordenadora:** `ISSP-Typed-Values.md`.

**Contrato de engenharia:** `../rfc/REPOSITORY-ENGINEERING-CONTRACT.md`,
v0.1 Approved; qualificação em `../rfc/REPOSITORY-READINESS.md`.

## 1. Objetivo e decisões confirmadas

“Light Sensor (%)” e “Battery Level (%)” devem publicar percentuais
fracionários, por exemplo `65.87`, do cálculo até a saída do coordenador.
Marcelo Miranda confirmou em 08/09/2026: float para ambos, duas casas na
publicação, campo JSON textual com ponto e preservação de porta/presença como
inteiros 0/1 com traduções atuais. A ordem autoriza autoria e análise.

## 2. Escopo, dependência e preservação

Inclui cálculo e report em `LightSensorBehavior` e `BatteryLevelBehavior`,
comparação de delta da bateria, armazenamento dos percentuais correspondentes
e tradução/validação percentual no coordenador. Abrange todas as composições
que registram bateria e o produto de luminosidade; não só o dispositivo
selecionado atualmente na bancada.

O valor tipado reutilizável não existe na baseline v2. Seu objetivo, contrato
wire, APIs técnicas, deduplicação e validação independentes pertencem a
`ISSP-Typed-Values.md`, não são redesenhados aqui. A implementação funcional
só começa após essa preparação estar implementada e validada, reconfronto da
nova baseline e análise Ready desta versão, seguida de ordem de implementação.
Não contornar a dependência com escala implícita, novo evento ou truncamento.

Preservar pinagem, board, ADC/calibração por amostra, número de amostras,
cadência, deep sleep, admissão, erros, endpoints, eventos 3 e 6, pareamento,
estado de telemetria e todos os defaults. Não criar precisão configurável,
calibração nova, lux, delta configurável fracionário, host externo ou operações
de hardware. A resolução numérica adicional não promete exatidão do sensor.

## 3. Requisitos

### FP-001 — luminosidade

Para raw válido de 0 a 4095, calcular `100 × raw / 4095`, preservando a fração
e produzir Float32 entre 0 e 100. Retirar o arredondamento final ao inteiro.
Cada aquisição válida conserva a tentativa de report por boot/despertar,
mesmo com percentual igual; leitura inválida continua sem report artificial.
Não refazer leitura durante retry. A apresentação de duas casas ocorre no
coordenador e não reduz a precisão do report admitido.

### FP-002 — bateria

Conservar conversão/calibração ADC por amostra e usar:

```text
Vpino_mV = soma(amostras válidas em mV) / samples
Vbat_mV  = Vpino_mV × (Rtop + Rbottom) / Rbottom
pct      = clamp(100 × (Vbat_mV − emptyMv) / (fullMv − emptyMv), 0, 100)
```

Preservar frações da média, divisor e interpolação, sem divisão inteira que
as descarte; remover o termo de arredondamento ao inteiro do percentual.
Produzir Float32. Intermediários devem evitar overflow e perda que altere
a saída contratada; a conversão ADC vigente pode continuar entregando mV
inteiros. Saturar leitura eletricamente válida abaixo/acima dos extremos em
0.0/100.0. Erro ADC ou configuração inválida não vira zero publicado.

### FP-003 — delta e lifecycle

`reportDeltaPercent` conserva tipo/configuração inteira, domínio 1–100 e
unidade de pontos percentuais. Comparar a diferença absoluta entre o Float32
atual e o último Float32 admitido, antes da formatação: publicar se maior ou
igual ao threshold. Sem epsilon que altere o limiar. A primeira medição válida
é admitida sem condição de delta; em deep sleep conservar publicação por boot.
Só atualizar a base depois de admissão bem-sucedida; recusa não consome delta.
Não criar estado persistente nem alterar o encerramento e espera por ACK.

### FP-004 — domínio e saída

Eventos 3 e 6 produzidos nesta versão usam obrigatoriamente Float32 finito
entre 0 e 100. O coordenador recusa Int32 ou valor fora do domínio nesses
eventos, sem publicação/ACK de sucesso. Demais eventos conservam seu contrato.
Manter nomes `Battery Level (%)`/`Light Sensor (%)`, nomes de capabilities,
endpoints e envelope JSON. `value` continua string decimal, com ponto,
exatamente duas casas e sem `%`: `65.87`, `0.00`, `100.00`.
Aplicar a formatação definida pela preparação sobre o valor recebido, sem
prometer dígitos decimais exatos antes da conversão binary32.

## 4. Autoridades

**Amends** `Light-Sensor-Battery-H2.md` v0.3, seções 1, 4, 5 e critérios de
normalização/integração, somente para percentual fracionário, formato e
dependência wire. Os oráculos inteiros anteriores são substituídos pelos desta
fonte; fórmula ADC, aquisição e energia continuam sob a especificação original.

**Amends** `Client-Battery-Level.md` v0.5, BATTERY-003/004/008, seção 5.1,
representação da seção 5.4 e BATTERY-AC-001/002, somente para aritmética
fracionária, saturação, delta, transporte e resultado percentual. Preservar
BATTERY-011 e demais invariantes/configurações; a proibição anterior de float
e arredondamento inteiro não se aplica a este recorte novo.

ADR-0006 **Amends** o domínio inteiro do evento 6 da ADR-0005 para admitir
fração, preservando sua alocação, endpoint e natureza. O domínio detalhado dos
eventos 3 e 6 nesta evolução pertence a esta fonte. Identidade e transporte
tipado pertencem à preparação e à ADR-0004 nos pontos preservados.
Bootstrap, deep sleep, features configuráveis e ADR-0001/0002/0003 permanecem
preservados. Nenhum débito técnico é criado ou reaberto por esta autoria.

## 5. Critérios de aceite

| ID | Requisitos | Cenário, ação e resultado observável | Meio |
|---|---|---|---|
| FP-AC-001 | FP-001/004 | raw 0, 20, 21, 819, 2048 e 4095 produz Float32 e saída `0.00`, `0.49`, `0.51`, `20.00`, `50.01`, `100.00`; erro ou raw inválido não reporta | Cálculo puro e inspeção da aquisição |
| FP-AC-002 | FP-002/004 | Com empty=3000, full=4000, divisor 1:1, amostras de pino 1829/1830 mV resultam em 65.90%; Vbat=3658.7 mV resulta em 65.87%; extremos e extrapolação válida saturam; amostra inválida suprime ciclo | Vetores aritméticos/doubles fiéis de amostras |
| FP-AC-003 | FP-003 | Base 65.5, delta 1: 66.25 não publica, 66.5 publica; primeira medição e boot deep sleep independem do delta; recusa de admissão preserva base | Behavior com publisher controlado |
| FP-AC-004 | FP-001/002/004 | Percurso H2 → codec C6 → UART mantém fração e saída `65.87`; Int32, não finito e fora de 0–100 para eventos 3/6 são recusados; binários conservam tradução | Vetores de integração pura e bancada quando autorizada |
| FP-AC-005 | FP-003/004 | Retry mantém ID/valor, erro não fabrica medição, configurações/defaults/cadência não mudam; composições afetadas constroem | Inspeção e builds canônicos |

O valor Vbat sintético de FP-AC-002 é entrada do cálculo puro, não promessa de
que qualquer ADC/board produza exatamente essa tensão. O oráculo é o cálculo
ideal convertido uma vez em binary32; a saída é a formatação desse valor.
Para as contas puras, aceitar erro numérico até um ULP de Float32, sem admitir
mudança do texto esperado nos vetores declarados.

## 6. Testes, construção e permissões

Esta versão **exige criação/alteração** de testes host-native de cálculo de
luminosidade/bateria junto a `issp_behaviors` para FP-AC-001/002, usando a
lógica pura de produção. Exige casos do behavior com ADC/publisher controlados
para FP-AC-003/005; se dependentes de ESP-IDF, usar test app H2, sem simular o
firmware em outro target. Exige extensão dos testes de tradução/integração
do coordenador criados na preparação para FP-AC-004, sem duplicar o codec.
Esses grupos devem cobrir falhas e saturação, não apenas nominal. Outros testes
permanecem fora do recorte, salvo sua vinculação já definida na preparação.

Build canônico da futura implementação: todas as composições H2 com bateria,
produto de luminosidade, exemplo H2 quando consumidor material dos headers
alterados, C6 e test apps alterados conforme REC-BUILD seção 9. Preservar
arquivos autoritativos e usar cópias externas. Nenhum build é realizado nesta
autoria. Coleta/execução de testes, flash, monitor e hardware exigem ordem
própria; permanecem **Not Executed**. Resultado de build não comprova precisão
física, transmissão na bancada ou conclusão humana do workflow.
