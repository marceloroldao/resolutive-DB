# Banco de Dados Resolutivo (BDR)

**Current stable-engine line: BDR v1.0.0 — Released**

**Software DOI (v1.0.0):** 10.5281/zenodo.22120246  
**Previous software DOI (v0.2.0-rc1):** 10.5281/zenodo.22074886  
**Historical software DOI (v0.1.0):** 10.5281/zenodo.21938148  
**Scientific preprint DOI:** 10.5281/zenodo.21937842

Projeto da **ETBRA Tecnologias** para investigar e desenvolver um mecanismo de armazenamento persistente e endereçamento resolutivo determinístico, com particionamento local, integração nativa e recuperação transacional.

> **Status de engenharia:** v1.0.0 é a primeira linha de motor estável do BDR. A árvore técnica foi validada, congelada e publicada. O projeto não reivindica complexidade O(1) estrita no pior caso para o motor completo; resultados de desempenho permanecem específicos ao workload e ao ambiente de teste.

## Estado atual — v1.0.0

A v1.0.0 consolida:

- core C++ persistente validado;
- `CompactIndex` como índice interno preferencial;
- `ResolutiveIndex` preservado como fallback interno de regressão;
- API pública C++ congelada em `database.hpp`;
- target CMake estável `bdr::bdr` para consumidores externos;
- entry point CMake na raiz do repositório;
- BDR3 para snapshots/checkpoints;
- BDW3 para write-ahead log;
- streaming checkpoint validado;
- recuperação de torn-tail WAL;
- compatibilidade de persistência em quatro direções entre baseline, candidato e fallback;
- 12 failpoints determinísticos de crash durante checkpoint;
- 200 ciclos checkpoint/reopen com 2.000.000 de operações;
- soak materializado de 50.000.000 de operações;
- validações V99 e V100 concluídas;
- pacote raiz versionado como `1.0.0`.

Consulte `RELEASE_NOTES_v1.0.0.md`, `docs/V1_PUBLIC_API.md`, `docs/V1_FINAL_AUDIT.md` e `docs/V1_RELEASE_CHECKLIST.md`.

## Compatibilidade de dados

A v1 preserva:

- **BDR3** para snapshots/checkpoints;
- **BDW3** para segmentos de write-ahead log.

Mudanças futuras incompatíveis de formato devem introduzir versão/magic explícitos e documentação de migração.

## Integração C++

O contrato de distribuição suportado pela v1 é o pacote CMake estático com target:

```cmake
find_package(bdr CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE bdr::bdr)
```

A API pública de fonte está congelada em `database.hpp`. Headers dos índices internos não fazem parte do contrato público.

## API Python

O pacote raiz `resolutive-db` está versionado como `1.0.0` e preserva a API Python existente:

```python
from bdr import PersistentBDR

# Use a API pública conforme os exemplos e testes do repositório.
```

Experimentos históricos em `experimental/` permanecem preservados como evidência de desenvolvimento e não devem ser confundidos com o contrato público congelado da v1.

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

## Evidência de robustez da v1

A linha v1 foi promovida após validações de API, persistência, crash boundary, compatibilidade, churn, recursos e escala. Entre os gates registrados:

- 50 milhões de operações: PASS;
- 200 ciclos checkpoint/reopen: PASS;
- 12 crash failpoints: PASS;
- instalação CMake + consumidor externo: PASS;
- compatibilidade BDR3/BDW3: PASS;
- BDR CI: PASS;
- V99: PASS;
- V100: PASS.

## Baselines históricas

### v0.2.0-rc1

Primeira release candidate de API/ABI publicada. Software DOI: **10.5281/zenodo.22074886**.

### v0.1.0

Baseline histórica experimental e de pesquisa, preservada de forma imutável. Software DOI: **10.5281/zenodo.21938148**.

## Publicações e citação

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

**Software DOI v1.0.0:** 10.5281/zenodo.22120246  
**Software DOI v0.2.0-rc1:** 10.5281/zenodo.22074886  
**Software DOI v0.1.0:** 10.5281/zenodo.21938148  
**Preprint DOI:** 10.5281/zenodo.21937842
