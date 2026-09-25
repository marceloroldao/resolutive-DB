# Banco de Dados Resolutivo (BDR)

**Current stable-engine line: BDR v1.1.0 — Released**  
**Current release candidate: [BDR v1.2.0-rc4](https://github.com/marceloroldao/resolutive-DB/releases/tag/v1.2.0-rc4) — Published pre-release**

**Software DOI (v1.2.0-rc4):** [10.5281/zenodo.22948288](https://doi.org/10.5281/zenodo.22948288)  
**Software DOI (v1.2.0-rc1):** 10.5281/zenodo.22784729  
**Software DOI (v1.1.0):** 10.5281/zenodo.22130421  
**Previous software DOI (v1.0.0):** 10.5281/zenodo.22120246  
**Software DOI (v0.2.0-rc1):** 10.5281/zenodo.22074886  
**Historical software DOI (v0.1.0):** 10.5281/zenodo.21938148  
**Scientific preprint DOI:** 10.5281/zenodo.21937842

Projeto da **ETBRA Tecnologias** para investigar e desenvolver um mecanismo de armazenamento persistente e endereçamento resolutivo determinístico, com particionamento local, integração nativa e recuperação transacional.

> **Status de engenharia:** v1.1.0 permanece a linha estável publicada. A v1.2.0-rc1 está publicada como pre-release e arquivada no Zenodo sob DOI `10.5281/zenodo.22784729`; ela adiciona o bridge atômico Python, leitura bulk otimizada, Atomic C ABI v2 e validação contra o workload topológico/temporal congelado da Memoria.ia. Não há redesign do BDW4. Resultados de desempenho continuam específicos ao workload e ao ambiente de teste.

## Estado atual — v1.2.0-rc4 publicado

O RC4 adiciona `bdr_atomic_c_clear()` à Atomic C ABI v2 e foi arquivado no Zenodo sob DOI `10.5281/zenodo.22948288`. A tag publicada aponta para `317882a00f041fc1568ff986af8016b09453f21a`. A v1.1.0 continua a baseline estável. O RC1 abaixo permanece como marco histórico da linha candidata.

## Histórico — v1.2.0-rc1 publicado

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

A linha v1.1.0 continua sendo a baseline estável publicada enquanto o RC1 passa pela validação de integração antes de eventual promoção para v1.2.0 estável. Consulte `RELEASE_NOTES_v1.2.0-rc1.md` e `docs/V1_2_FREEZE_CANDIDATE_RECORD.md`.

## Compatibilidade de dados

A linha candidata v1.2 preserva os formatos existentes:

- **BDR3** — snapshots/checkpoints legados;
- **BDW3** — write-ahead log legado;
- **BDW4** — framing atômico introduzido na v1.1.

A migração continua side-by-side: BDR3/BDW3 existentes permanecem inalterados e novas mutações atômicas usam BDW4. O trabalho v1.2 não redesenha o formato.

## Integração C++

O target público continua sendo: