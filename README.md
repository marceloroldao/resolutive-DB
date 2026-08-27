# Banco de Dados Resolutivo (BDR)

**Current stable-engine line: BDR v1.1.0 — Released**

**Software DOI (v1.1.0):** 10.5281/zenodo.22130421  
**Previous software DOI (v1.0.0):** 10.5281/zenodo.22120246  
**Software DOI (v0.2.0-rc1):** 10.5281/zenodo.22074886  
**Historical software DOI (v0.1.0):** 10.5281/zenodo.21938148  
**Scientific preprint DOI:** 10.5281/zenodo.21937842

Projeto da **ETBRA Tecnologias** para investigar e desenvolver um mecanismo de armazenamento persistente e endereçamento resolutivo determinístico, com particionamento local, integração nativa e recuperação transacional.

> **Status de engenharia:** v1.1.0 é a evolução publicada e compatível da linha estável v1, adicionando batches lógicos atômicos em BDW4 para requisitos de persistência da Memoria.ia. O projeto não reivindica complexidade O(1) estrita no pior caso para o motor completo; resultados de desempenho permanecem específicos ao workload e ao ambiente de teste.

## Estado atual — v1.1.0

A v1.1.0 preserva a superfície `bdr::Database` da v1.0 e acrescenta, de forma opt-in:

- API pública `bdr::AtomicDatabase` em `bdr/atomic_database.hpp`;
- `write_batch`, `put_many` e `erase_many`;
- atomicidade all-or-nothing para múltiplos registros físicos de uma operação lógica;
- modos de durabilidade `Async`, `BatchSync` e `PerOperationSync`;
- `last_sequence()` e `durable_sequence()`;
- BDW4 como WAL explicitamente versionado para batches atômicos;
- recuperação de tail BDW4 incompleto até o último boundary válido;
- ordenação monotônica sob produtores concorrentes;
- migração side-by-side de BDR3/BDW3 sem reescrever arquivos legados;
- compatibilidade do target CMake `bdr::bdr` e de consumidores v1.0;
- validações V101–V112, V99 e V100 concluídas.

A v1.0.0 permanece uma baseline publicada e imutável. Consulte `RELEASE_NOTES_v1.1.0.md`, `docs/V1_1_PUBLIC_API.md` e `docs/V1_1_RELEASE_CHECKLIST.md`.

## Compatibilidade de dados

A linha v1.1 preserva leitura dos formatos da v1.0:

- **BDR3** — snapshots/checkpoints legados;
- **BDW3** — write-ahead log legado;
- **BDW4** — novo framing atômico aditivo da v1.1.

A migração é side-by-side: BDR3/BDW3 existentes permanecem inalterados e novas mutações atômicas são gravadas em BDW4.

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

Para batches lógicos atômicos da v1.1:

```cpp
#include <bdr/atomic_database.hpp>
```

A extensão é aditiva; a superfície `bdr::Database` da v1.0 permanece suportada.

## API Python

O pacote raiz `resolutive-db` está versionado como `1.1.0` e preserva a API Python existente:

```python
from bdr import PersistentBDR
```

A nova API atômica validada nesta release é uma superfície C++ opt-in; v1.1.0 não declara um novo contrato de bindings Python para `AtomicDatabase`.

## Evidência para Memoria.ia

O workload representativo V112 usa 512 memórias lógicas × 24 registros físicos = 12.288 registros, com uma fronteira de durabilidade por memória lógica.

Após a otimização V113 do replay BDW4, a execução registrada mostrou:

| Métrica | Cadência v1.0 | Atômico v1.1 | Resultado registrado |
|---|---:|---:|---:|
| Escrita | 4.918,203 ms | 1.696,555 ms | v1.1 ~2,90× mais rápido |
| Reopen + verificação total | 29,120 ms | 13,851 ms | v1.1 ~2,10× mais rápido |
| Espaço em disco | 3.829.120 B | 3.597.664 B | v1.1 menor |

Esses números são evidência de regressão específica do runner/workload, não uma alegação de superioridade universal.

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

A linha v1.1 passou, entre outros gates:

- V101 framing atômico;
- V102 WAL em arquivo;
- V103 commit-boundary failpoints;
- V104 batch API;
- V105 concorrência;
- V106 migração BDR3/BDW3;
- V107 candidato integrado;
- V108 stress/soak integrado;
- V109 matriz de crash recovery;
- V110 contrato de durabilidade;
- V111 compatibilidade da API pública/consumidor externo;
- V112 benchmark representativo Memoria.ia;
- BDR CI;
- V99 e V100 evidence closure.

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
