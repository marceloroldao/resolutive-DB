# Banco de Dados Resolutivo (BDR)

**Current release candidate: BDR v0.2.0-rc1 — Experimental / Release Candidate**

**Software DOI (v0.2.0-rc1):** 10.5281/zenodo.22074886  
**Historical software DOI (v0.1.0):** 10.5281/zenodo.21938148  
**Scientific preprint DOI:** 10.5281/zenodo.21937842

Projeto experimental da **ETBRA Tecnologias** para investigar um mecanismo de armazenamento persistente e endereçamento resolutivo determinístico, com particionamento local, integração por C ABI e recuperação transacional.

> **Status científico/engenharia:** v0.2.0-rc1 é uma release candidate experimental e não deve ser apresentada como banco de dados production-ready. O projeto não reivindica complexidade O(1) estrita no pior caso para o motor completo; resultados de desempenho são específicos ao workload e ao ambiente de teste.

## Estado atual — v0.2.0-rc1

A linha v0.2.0-rc1 consolida:

- core C++ persistente;
- caminho determinístico `rho + local Robin Hood + fingerprint`;
- valores binários de tamanho variável;
- API C++ para `open`, `get`, `put`, `delete`, `wait`, `sync`, `checkpoint` e `close`;
- C ABI v1 congelada para integração externa;
- binding Python nativo `bdr-native` versão `0.2.0rc1`;
- WAL BDW3 com sequence numbers e verificações de integridade;
- snapshots/checkpoints BDR3 com CRC;
- streaming snapshot recovery;
- reparo de torn-tail no último WAL incompleto;
- lock exclusivo por processo;
- propagação de erros de I/O pela API;
- validações de multiwriter, checkpoint, crash recovery, soak e compatibilidade;
- benchmarks comparativos com SQLite, LMDB, LevelDB e RocksDB.

A evidência de fechamento V100 registrou `candidate: true`, incluindo soak de 500.000 mutações, contrato do core, C ABI v1, wheel Python instalado e audits de readiness/metadata/staging. Consulte `docs/release/v0.2.0-rc1-evidence.md` e `RELEASE_NOTES_v0.2.0-rc1.md`.

## Compatibilidade de dados

A v0.2.0-rc1 utiliza:

- **BDR3** para snapshots/checkpoints;
- **BDW3** para segmentos de write-ahead log;
- chave e valor binários de tamanho variável no core nativo.

Mudanças futuras incompatíveis de formato devem introduzir versão/magic explícitos e documentação de migração.

## API Python nativa v0.2.0-rc1

O binding nativo está em `experimental/api_v88/python/bdr_native` e expõe uma interface semelhante a:

```python
from bdr_native import Database

with Database("./data") as db:
    db.put_sync(b"key", b"value")
    value = db.get(b"key")
    db.checkpoint()
```

Essa é a linha relevante para integrações novas, inclusive testes com payloads binários produzidos por outros componentes.

## Objetivo de pesquisa

O BDR investiga se dados podem ser transformados deterministicamente em endereços resolutivos compactos e recuperados por acesso direto a partições locais, reduzindo a dependência de estruturas ordenadas ou varreduras lineares.

A implementação conceitual continua relacionada ao espaço resolutivo:

\[
\mathcal{R} = (\rho^R, \phi, \theta, f)
\]

Na baseline Python original:

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

Os benchmarks da v0.2.0-rc1 mostram que não existe superioridade universal. Em workloads Python registrados no fechamento V100, SQLite foi mais rápido que BDR em algumas configurações. Todas as alegações devem permanecer reproduzíveis e workload-specific.

## Baseline histórica — v0.1.0

A `v0.1.0` permanece a baseline histórica publicada e imutável do projeto.

Ela inclui:

- implementação Python em memória;
- `PersistentBDR` original;
- `PUT`, `GET` e `DELETE`;
- WAL segmentado;
- sequence numbers monotônicos;
- CRC32;
- group commit e `fsync`;
- snapshots BDR2;
- checkpoint atômico;
- recuperação após crash;
- testes com `SIGKILL`;
- benchmarks/protótipos C++ iniciais.

A baseline histórica continua citável como:

**Matos, Marcelo Roldão (2026). Banco de Dados Resolutivo (BDR) / Resolutive Database Engine. Version 0.1.0. Zenodo. DOI: 10.5281/zenodo.21938148.**

## Publicações e citação

### Software BDR v0.2.0-rc1

**Matos, Marcelo Roldão (2026). Banco de Dados Resolutivo (BDR) / Resolutive Database Engine. Version 0.2.0-rc1. Zenodo. DOI: 10.5281/zenodo.22074886.**

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

**Software DOI v0.2.0-rc1:** 10.5281/zenodo.22074886  
**Software DOI v0.1.0:** 10.5281/zenodo.21938148  
**Preprint DOI:** 10.5281/zenodo.21937842
