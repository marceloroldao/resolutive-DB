# Banco de Dados Resolutivo (BDR)

**Current stable-engine line: BDR v1.1.0 — Released**  
**Current release candidate: BDR v1.2.0-rc1 — Validation candidate, not stable**

**Software DOI (v1.1.0):** 10.5281/zenodo.22130421  
**Previous software DOI (v1.0.0):** 10.5281/zenodo.22120246  
**Software DOI (v0.2.0-rc1):** 10.5281/zenodo.22074886  
**Historical software DOI (v0.1.0):** 10.5281/zenodo.21938148  
**Scientific preprint DOI:** 10.5281/zenodo.21937842

Projeto da **ETBRA Tecnologias** para investigar e desenvolver um mecanismo de armazenamento persistente e endereçamento resolutivo determinístico, com particionamento local, integração nativa e recuperação transacional.

> **Status de engenharia:** v1.1.0 permanece a linha estável publicada. A v1.2.0-rc1 é um candidato pré-release que adiciona o bridge atômico Python, leitura bulk otimizada, Atomic C ABI v2 e validação contra o workload topológico/temporal congelado da Memoria.ia. Não há redesign do BDW4. Resultados de desempenho continuam específicos ao workload e ao ambiente de teste.

## Estado atual — v1.2.0-rc1 candidato

O candidato v1.2.0-rc1 preserva a base v1.1 e acrescenta, de forma aditiva:

- `bdr.AtomicBDR` para Python;
- `write_batch`, `put_many`, `erase_many`, `get_many`, `sync`, `last_sequence` e `durable_sequence`;
- modos `Async`, `BatchSync` e `PerOperationSync`;
- Atomic C ABI v2;
- `get_many` com entrada de chaves compactada no bridge Python;
- fast path opcional `get_many_packed` com uma arena de saída, mantendo fallback para bibliotecas sem o símbolo;
- preservação exata de bytes, UTF-8, chaves binárias com NUL, missing, valores vazios, duplicatas e ordem;
- validação Android NDK `arm64-v8a` da superfície candidata;
- benchmarks V121–V127 contra o workload congelado da Memoria.ia no commit `99a1585d497b98f0fc6f360ec8f39e6771452827`;
- registro explícito de resultados positivos e negativos de desempenho.

A linha v1.1.0 continua sendo a baseline estável publicada até que o RC seja validado, integrado e publicado formalmente. Consulte `RELEASE_NOTES_v1.2.0-rc1.md` e `docs/V1_2_FREEZE_CANDIDATE_RECORD.md`.

## Compatibilidade de dados

A linha candidata v1.2 preserva os formatos existentes:

- **BDR3** — snapshots/checkpoints legados;
- **BDW3** — write-ahead log legado;
- **BDW4** — framing atômico introduzido na v1.1.

A migração continua side-by-side: BDR3/BDW3 existentes permanecem inalterados e novas mutações atômicas usam BDW4. O trabalho v1.2 não redesenha o formato.

## Integração C++

O target público continua sendo:

```cmake
find_package(bdr CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE bdr::bdr)
```

Consumidores existentes podem continuar usando:

```cpp
#include <bdr/database.hpp>
```

Para batches lógicos atômicos:

```cpp
#include <bdr/atomic_database.hpp>
```

A extensão é aditiva; a superfície `bdr::Database` da v1.0 permanece suportada.

## API Python

O pacote candidato usa versão PEP 440 `1.2.0rc1`:

```python
from bdr import AtomicBDR, DurabilityMode, Operation
```

Também permanece disponível a API histórica:

```python
from bdr import PersistentBDR
```

`AtomicBDR.get_many()` prefere o fast path packed quando o símbolo nativo está disponível e recua automaticamente para o `get_many` legado quando não está, preservando compatibilidade de carregamento.

## Evidência para Memoria.ia

O workload congelado usa o codec e a forma topológica/temporal da Memoria.ia validada no commit `99a1585d497b98f0fc6f360ec8f39e6771452827`.

Na rodada de freeze do candidato, em 1.200 observações:

- rebuild BDR bulk-prefetch: ~82,4 ms;
- `bulk_get`: ~11,3 ms;
- cold open BDR: ~12,4 ms;
- SQLite oracle load: ~57,4 ms;
- paridade semântica: verde;
- BDR em disco: ~3,75 MB versus ~6,32 MB no oracle.

Em escala estendida, 5k e 10k preservaram paridade semântica. No mesmo runner, 5k mostrou rebuild BDR bulk ~313 ms versus SQLite ~380 ms; em 10k, BDR bulk ~815 ms versus SQLite ~592 ms. O cold open BDR em 10k foi ~67 ms, evidenciando que a maior parcela restante está na reconstrução semântica/objetos fora do motor BDR. Esses resultados são workload-specific e o resultado desfavorável de 10k é mantido como evidência.

## Objetivo de pesquisa

O BDR investiga se dados podem ser transformados deterministicamente em endereços resolutivos compactos e recuperados por acesso direto a partições locais, reduzindo a dependência de estruturas ordenadas ou varreduras lineares.

A implementação conceitual continua relacionada ao espaço resolutivo:

\[
\mathcal{R} = (\rho^R, \phi, \theta, f)
\]

Na baseline conceitual:

- `rho_R` seleciona uma partição/bucket;
- `phi` funciona como assinatura de fase quantizada;
- `theta` e `f_nu` são metadados normalizados;
- um fingerprint independente confirma a identidade exata da chave.

## Complexidade e desempenho

O acesso à partição é direto, mas a resolução interna depende da estrutura local empregada. O projeto **não reivindica O(1) garantido no pior caso** para o motor completo.

| Estrutura | Busca típica | Observação |
|---|---:|---|
| BDR particionado | O(1) esperado | sob hipóteses usuais de hashing e distribuição |
| Python `dict` | O(1) esperado | baseline hash convencional |
| busca binária / B-tree | O(log N) | estrutura ordenada |
| varredura linear | O(N) | referência de crescimento linear |

Os benchmarks preservam resultados favoráveis e desfavoráveis ao BDR. Não existe alegação de superioridade universal sobre SQLite, LMDB, LevelDB ou RocksDB. Comparações devem ser reproduzíveis e workload-specific.

## Evidência de robustez

O candidato v1.2.0-rc1 passou, entre outros gates:

- BDR CI;
- V101–V107;
- V121 telemetry;
- V122 frozen Memoria topological benchmark;
- V123 decode decomposition;
- V124 buffer decomposition;
- V125 packed-key marshalling;
- V126 packed-output probe;
- V127 extended scale 5k/10k;
- Android NDK C ABI `arm64-v8a`;
- restart/torn-tail recovery;
- fallback para biblioteca sem `get_many_packed`.

## Baselines publicadas

### v1.1.0

Evolução estável com persistência lógica atômica BDW4. Software DOI: **10.5281/zenodo.22130421**.

### v1.0.0

Primeira linha estável publicada. Software DOI: **10.5281/zenodo.22120246**.

### v0.2.0-rc1

Primeira release candidate de API/ABI publicada. Software DOI: **10.5281/zenodo.22074886**.

### v0.1.0

Baseline histórica experimental e de pesquisa. Software DOI: **10.5281/zenodo.21938148**.

## Publicações e citação

### Software BDR v1.1.0

**Matos, Marcelo Roldão (2026). Banco de Dados Resolutivo (BDR) / Resolutive Database Engine, v1.1.0. Zenodo. DOI: 10.5281/zenodo.22130421.**

### Software BDR v1.0.0

**Matos, Marcelo Roldão (2026). Banco de Dados Resolutivo (BDR) / Resolutive Database Engine, v1.0.0. Zenodo. DOI: 10.5281/zenodo.22120246.**

### Preprint científico

**Matos, Marcelo Roldão (2026). Banco de Dados Resolutivo (BDR): Arquitetura Experimental de Endereçamento Densitário, Persistência Transacional e Avaliação Reprodutível. Zenodo. DOI: 10.5281/zenodo.21937842.**

O software e o preprint são objetos citáveis separados.

## Licenciamento

Este repositório é **source-available**, não é apresentado como software open source aprovado pela OSI.

Uso acadêmico, educacional e de pesquisa não comercial é permitido nos termos da licença do repositório. Uso comercial, produção, integração proprietária, SaaS e monetização exigem autorização/licença comercial separada da **ETBRA Tecnologias**.

A licença de software não concede direitos de patente.

## Autoria

**Marcelo Roldão Matos**  
ORCID: 0009-0003-6075-4680  
ETBRA Tecnologias — 2026

**Software DOI v1.1.0:** 10.5281/zenodo.22130421  
**Software DOI v1.0.0:** 10.5281/zenodo.22120246  
**Software DOI v0.2.0-rc1:** 10.5281/zenodo.22074886  
**Software DOI v0.1.0:** 10.5281/zenodo.21938148  
**Preprint DOI:** 10.5281/zenodo.21937842
