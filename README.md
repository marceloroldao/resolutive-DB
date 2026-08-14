# Banco de Dados Resolutivo (BDR)

**BDR v0.1.0 — Experimental / Research Preview**

**Zenodo DOI:** 10.5281/zenodo.21937842

Projeto experimental da **ETBRA Tecnologias** para investigar um mecanismo de endereçamento direto inspirado no paradigma de **Endereçamento Densitário Harmônico**, representado pelo espaço resolutivo

\[
\mathcal{R} = (\rho^R, \phi, \theta, f)
\]

onde, nesta implementação computacional:

- `rho_R` seleciona diretamente um cesto/bucket harmônico;
- `phi` funciona como assinatura de fase quantizada;
- `theta` é um metadado angular normalizado;
- `f_nu` é um metadado de frequência/estado normalizado;
- um fingerprint independente confirma a identidade exata da chave.

> **Status científico/engenharia:** v0.1.0 é uma versão experimental/research preview, publicada e citável via Zenodo. Os nomes e conceitos resolutivos são usados como hipótese de arquitetura. O repositório não afirma que a interpretação física subjacente esteja experimentalmente validada nem que o motor completo possua complexidade O(1) garantida no pior caso.

## Publicação científica

**Matos, Marcelo Roldão (2026). Banco de Dados Resolutivo (BDR): Arquitetura Experimental de Endereçamento Densitário, Persistência Transacional e Avaliação Reprodutível. Version 0.1.0. Zenodo. DOI: 10.5281/zenodo.21937842.**

O preprint, código, testes e benchmarks formam a baseline científica da versão v0.1.0. Resultados futuros devem identificar explicitamente a versão/commit utilizado para preservar reprodutibilidade.

## Objetivo

Investigar se um dado pode ser transformado deterministicamente em um endereço resolutivo compacto e recuperado por acesso direto a uma partição local, reduzindo a dependência de estruturas de busca ordenadas ou varreduras lineares.

A implementação utiliza a topologia conceitual:

```text
key / dado
    |
    v
EncoderResolutivo
    |
    +--> rho_R  ----> partição local
    +--> phi    ----> componente de fase
    +--> theta  ----> metadado
    +--> f_nu   ----> metadado
    +--> fingerprint --> confirmação exata
```

## Complexidade

A implementação deve ser avaliada com rigor. O acesso à partição por `rho_R` é direto, mas a resolução interna depende da estrutura local empregada. A v0.1.0 **não reivindica O(1) garantido no pior caso** para o motor completo.

| Estrutura | Busca típica | Observação |
|---|---:|---|
| BDR PoC/hash local | O(1) esperado | particionamento + resolução local |
| Python `dict` | O(1) esperado | baseline hash convencional |
| busca binária / B-tree | O(log N) | estrutura ordenada |
| varredura linear | O(N) | referência de crescimento linear |
| K-NN exato em D dimensões | O(N·D) | comparação vetorial exata típica |

## Motor persistente v0.1.0

A versão persistente inclui:

- `PUT`, `GET` e `DELETE`;
- WAL segmentado;
- sequence numbers monotônicos;
- CRC32;
- group commit e `fsync`;
- snapshot BDR2 versionado;
- checkpoint atômico;
- recuperação após crash;
- testes com `SIGKILL` em CI;
- Python 3.11 e 3.12;
- benchmarks/protótipos C++.

O contrato experimental do formato persistente está documentado em `docs/DISK_FORMAT_V1.md`.

## Instalação

```bash
python -m venv .venv
pip install -e .
```

Execute os testes:

```bash
pytest -q
```

## API pública

```python
from bdr import (
    BancoDeDadosResolutivo,
    EncoderResolutivo,
    EntidadeResolutiva,
    PersistentBDR,
    RecoveryError,
)
```

## Benchmark reprodutível

Os scripts em `benchmarks/` registram a evolução experimental do projeto, incluindo estruturas compactas, Robin Hood particionado, afinidade de fase, crossover de densidade, adaptação por partição, persistência, crash recovery e comparação com SQLite.

Para a comparação de paridade persistente BDR × SQLite:

```bash
python benchmarks/bdr_v01_sqlite_parity.py
```

Resultados de benchmark devem ser interpretados no contexto exato do hardware, sistema operacional, filesystem, runtime e fronteira de durabilidade utilizados. Nenhum resultado isolado constitui alegação de superioridade universal.

## Fidelidade e recuperação

A suíte verifica determinismo, reconstrução exata, update/delete, fuzz diferencial, checkpoint/reopen, corrupção/truncamento, continuidade de sequência, multiwriter e recuperação após crash abrupto.

O princípio de recovery é conservador: corrupção completa ou gap conhecido de sequência deve produzir falha explícita em vez de aceitar silenciosamente um histórico inconsistente.

## Licenciamento dual

Este repositório é **source-available**, não é apresentado como software open source aprovado pela OSI.

### Uso acadêmico e educacional

Permitido gratuitamente, conforme `LICENSE`, para universidades, escolas, pesquisadores, estudantes, instituições públicas de pesquisa, organizações sem fins lucrativos, experimentação, avaliação e benchmarking não comerciais.

### Uso comercial

Exige licença comercial separada da **ETBRA Tecnologias**, incluindo empresas com fins lucrativos, startups, produtos ou serviços pagos, SaaS, produção, integração proprietária, uso interno que suporte atividade comercial e redistribuição/monetização.

O **manuscrito científico** depositado no Zenodo utiliza CC BY-NC-SA 4.0. Essa licença do manuscrito não substitui nem amplia a licença do software.

## Patentes e propriedade intelectual

A licença de software **não concede direitos de patente**. Direitos eventualmente associados ao Banco de Dados Resolutivo, ao Endereçamento Densitário Harmônico, ao Resolutive Database Engine e tecnologias relacionadas permanecem expressamente reservados aos respectivos titulares.

## Próximas etapas

A v0.1.0 passa a ser a baseline publicada. Evoluções futuras serão comparadas contra ela, incluindo datasets maiores, payload externo ao índice, mmap/page cache, filtros para misses, compaction/tombstones, multiprocessamento, leitores com menor contenção, sharding persistente, motor C++ integrado e comparações adicionais com bancos estabelecidos.

## Autoria

**Marcelo Roldão Matos**  
ORCID: 0009-0003-6075-4680  
ETBRA Tecnologias — 2026

**DOI:** 10.5281/zenodo.21937842
