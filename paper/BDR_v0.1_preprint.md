# Banco de Dados Resolutivo (BDR): Arquitetura Experimental de Endereçamento Densitário, Persistência Transacional e Avaliação Reprodutível

**Resolutive Database Engine — BDR v0.1.0 Research Preview**

**Autor:** Marcelo Roldão Matos  
**ORCID:** 0009-0003-6075-4680  
**Afiliação:** ETBRA Tecnologias, Rio Grande do Sul, Brasil  
**Data:** 9 de agosto de 2026  
**Versão:** 0.1.0 — Preprint / Research Preview  
**Repositório:** marceloroldao/resolutive-DB

> **Nota de escopo científico.** Este trabalho apresenta uma arquitetura computacional experimental e resultados reproduzíveis de engenharia de software. A terminologia “resolutiva” é empregada como hipótese arquitetural. O artigo não reivindica validação experimental da interpretação física da Ciência Resolutiva nem prova de complexidade O(1) no pior caso para o motor completo.

## Resumo

Este trabalho apresenta o **Banco de Dados Resolutivo (BDR)**, um motor experimental chave-valor que investiga uma estratégia de endereçamento determinístico inspirada no paradigma de **Endereçamento Densitário Harmônico**. Uma chave é transformada em um estado computacional resolutivo compacto, representado por componentes de densidade, fase e metadados normalizados, utilizado para direcionar o acesso a partições locais. A pesquisa evoluiu de uma prova de conceito em memória para protótipos particionados em C++, representações compactas e adaptativas e, finalmente, um motor persistente com `PUT`, `GET` e `DELETE`, write-ahead log segmentado, números de sequência monotônicos, CRC32, group commit, snapshots versionados, checkpoint atômico e recuperação após falha abrupta de processo. A versão v0.1.0 é acompanhada por testes diferenciais, fuzz testing, ciclos de checkpoint/reopen, teste de crash real com `SIGKILL`, integração contínua em Python 3.11 e 3.12, gates de compilação C++ e comparação de durabilidade equivalente com SQLite. Em uma execução reprodutível do CI com 5.000 operações, BDR e SQLite produziram estado final idêntico, com 1.462 registros. No regime de commit durável por operação, o BDR atingiu aproximadamente 2.372 operações/s contra 2.111 operações/s do SQLite; em group commit de 64 operações, os resultados foram aproximadamente 104.250 e 105.228 operações/s, respectivamente. Esses números caracterizam apenas o workload e ambiente medidos e não constituem uma alegação geral de superioridade. Os resultados indicam que a arquitetura BDR já é suficientemente estável para servir como baseline experimental reproduzível para estudos posteriores de particionamento adaptativo, estruturas compactas, concorrência, persistência e escalabilidade.

**Palavras-chave:** banco de dados; armazenamento chave-valor; endereçamento resolutivo; particionamento; hash; Robin Hood hashing; write-ahead log; recuperação de falhas; persistência; benchmarking; ciência reproduzível.

## Abstract

This work presents the **Resolutive Database (BDR)**, an experimental key-value engine investigating a deterministic addressing strategy inspired by the **Harmonic Density Addressing** paradigm. A key is transformed into a compact computational resolutive state composed of density, phase, and normalized metadata, which is used to direct access to local partitions. The research evolved from an in-memory proof of concept to partitioned C++ prototypes, compact and adaptive representations, and finally a persistent engine supporting `PUT`, `GET`, and `DELETE`, segmented write-ahead logging, monotonic sequence numbers, CRC32 integrity checks, group commit, versioned snapshots, atomic checkpoints, and recovery after abrupt process failure. Version v0.1.0 includes differential testing, fuzz testing, checkpoint/reopen cycles, a real `SIGKILL` crash-recovery test, continuous integration on Python 3.11 and 3.12, C++ compilation gates, and a durability-parity comparison against SQLite. In one reproducible CI run with 5,000 operations, BDR and SQLite produced identical final states containing 1,462 records. Under durable commit-per-operation boundaries, BDR achieved approximately 2,372 operations/s versus 2,111 operations/s for SQLite; with group commits of 64 operations, results were approximately 104,250 and 105,228 operations/s, respectively. These measurements characterize only the tested workload and environment and do not constitute a general performance-superiority claim. The results establish BDR v0.1.0 as a reproducible experimental baseline for further research on adaptive partitioning, compact data structures, concurrency, persistence, and scalability.

**Keywords:** database; key-value storage; resolutive addressing; partitioning; hashing; Robin Hood hashing; write-ahead log; crash recovery; persistence; benchmarking; reproducible research.

---

## 1. Introdução

Motores de banco de dados transformam identificadores lógicos em localizações físicas ou estruturas intermediárias que permitam recuperar dados com baixa latência. Árvores balanceadas, tabelas hash, log-structured merge trees e índices especializados representam diferentes compromissos entre custo de leitura, escrita, memória, ordenação, concorrência e persistência.

O BDR parte de uma pergunta experimental diferente: **é possível decompor deterministicamente uma chave em um endereço multidimensional compacto que direcione a operação para uma região local do espaço de armazenamento antes da resolução exata da identidade?**

A hipótese computacional é representada por

\[
\mathcal{R}(k) = (\rho^R, \phi, \theta, f),
\]

onde `k` é uma chave canônica. Na implementação de referência, `rho_R` seleciona uma partição ou bucket; `phi` atua como componente de fase quantizada; `theta` e `f` representam metadados normalizados; e um fingerprint independente confirma a identidade exata da chave.

O objetivo científico não é renomear uma tabela hash. A questão investigada é se a decomposição do espaço em regiões locais, combinada com estruturas internas especializadas de acordo com densidade e carga, pode produzir propriedades úteis de localidade, contenção, memória e adaptação.

A versão v0.1.0 marca a transição entre uma prova de conceito algorítmica e um **baseline persistente e reproduzível**. A contribuição central deste artigo é documentar essa transição, separar resultados observados de hipóteses ainda abertas e estabelecer um protocolo que permita reprodução e crítica independente.

## 2. Hipótese de endereçamento resolutivo

Seja `K` o espaço de chaves. O encoder define um mapeamento determinístico

\[
E: K \rightarrow \mathcal{R},
\]

com

\[
E(k)=(\rho^R_k,\phi_k,\theta_k,f_k).
\]

Para uma quantidade `B` de buckets, a primeira etapa de localização pode ser descrita abstratamente por

\[
b(k)=Q_{\rho}(\rho^R_k) \bmod B,
\]

em que `Q_rho` é uma quantização determinística da componente de densidade. A resolução local utiliza componentes adicionais e um fingerprint da chave para evitar que uma coincidência no endereço aproximado seja confundida com identidade exata.

Assim, a operação conceitual é

\[
k \xrightarrow{E} \mathcal{R}(k)
\xrightarrow{b} P_b
\xrightarrow{L_b} e_k,
\]

onde `P_b` é a partição selecionada e `L_b` é a estrutura local da partição.

A implementação Python de referência utiliza primitivas hash convencionais internamente. Portanto, o BDR v0.1.0 **não demonstra O(1) no pior caso**. A formulação correta para a PoC é acesso direto à partição seguido de resolução local cujo custo depende da estrutura utilizada.

## 3. Evolução das estruturas locais

Os experimentos do projeto avaliaram diferentes organizações internas. Três linhas se mostraram especialmente relevantes.

### 3.1 Robin Hood particionado

A família V4 emprega hashing Robin Hood dentro de partições locais. O objetivo é reduzir variância de distância de sondagem e evitar uma única estrutura global altamente contenciosa. O particionamento permite que operações independentes sejam direcionadas a regiões distintas.

### 3.2 Representação compacta

A família V5 investiga uma representação compacta para regiões predominantemente orientadas a leitura. Nos experimentos registrados pelo projeto, a estrutura de índice compacta atingiu ordem de grandeza de aproximadamente 16–17 bytes por registro em configurações específicas. Esse resultado não inclui necessariamente todo o custo de payload, allocator, runtime ou persistência e deve ser interpretado no escopo do benchmark correspondente.

### 3.3 Estrutura adaptativa por partição

A família V6 introduz a hipótese de que não existe uma estrutura local universalmente ótima. Seja `rho_i` uma medida operacional da densidade/carga da partição `i`. Um seletor

\[
S(\rho_i,w_i) \rightarrow \{\text{Compact},\text{RobinHood},\ldots\}
\]

pode escolher uma representação conforme densidade e perfil de workload `w_i`.

Essa estratégia desloca a pesquisa de “qual tabela é globalmente melhor?” para “qual representação é adequada para o estado local desta região?”. Na v0.1.0, essa ideia permanece experimental e não é apresentada como mecanismo de otimização universal.

## 4. Motor persistente

A camada persistente integrada transforma o BDR em um motor chave-valor experimental com operações fundamentais:

\[
\operatorname{PUT}(k,v),\qquad
\operatorname{GET}(k),\qquad
\operatorname{DELETE}(k).
\]

A arquitetura de persistência pode ser resumida por

```text
PUT / GET / DELETE
        |
        v
segmented WAL
        |
        +-- monotonic sequence number
        +-- operation code
        +-- key/value
        +-- CRC32
        |
        v
in-memory state
        |
        v
atomic checkpoint
        |
        +-- BDR2 snapshot
        +-- fsync
        +-- atomic rename
        +-- directory fsync
        |
        v
safe WAL retirement
```

### 4.1 Write-ahead log

Cada registro persistente do WAL contém um número de sequência `s`, código da operação, tamanho da chave, valor, bytes UTF-8 da chave e CRC32 calculado sobre o registro. Os números de sequência são monotônicos dentro de um diretório de banco.

A recuperação exige continuidade após a sequência registrada no snapshot. Um registro final truncado pode ser interpretado como cauda rasgada; um registro completo com checksum inválido produz erro de recuperação.

### 4.2 Snapshot BDR2

O snapshot atual possui magic `BDR2`, versão de formato, sequência do snapshot, número de registros, registros ordenados por chave e CRC32 global. O formato é explicitamente versionado para que mudanças futuras possam ser tratadas por política de compatibilidade e migração.

### 4.3 Checkpoint atômico

O protocolo de checkpoint executa, em ordem conceitual:

1. sincronização do WAL ativo;
2. escrita completa de `snapshot.tmp`;
3. `fsync` do arquivo temporário;
4. rename atômico para `snapshot.bin`;
5. `fsync` do diretório;
6. criação de WAL pós-checkpoint;
7. nova sincronização do diretório;
8. remoção de segmentos antigos já cobertos pelo snapshot;
9. sincronização final do diretório.

O invariante pretendido é que o histórico necessário para recuperação não seja removido antes de existir um snapshot substituto duravelmente publicado.

## 5. Modelo de durabilidade e recuperação

O BDR suporta escrita explicitamente durável e group commit. Para uma fronteira durável `D_j`, define-se o prefixo reconhecido

\[
P_j = \{o_i\mid s_i\leq s(D_j)\}.
\]

Após uma falha abrupta, o requisito testado é

\[
\operatorname{recover}(S,W) \supseteq P_j,
\]

onde `S` é o último snapshot válido e `W` é o conjunto de segmentos WAL recuperáveis. Operações posteriores à última fronteira durável podem ou não sobreviver, dependendo do momento da falha e das garantias do sistema operacional/dispositivo.

A recuperação também verifica checksum, versão, códigos de operação e continuidade de sequência. O objetivo é preferir falha explícita a aceitar silenciosamente uma estrutura conhecida como inconsistente.

## 6. Metodologia de validação

A v0.1.0 foi submetida a uma sequência incremental de testes:

- testes unitários de determinismo, inserção, atualização, consulta e remoção;
- reconstrução exata de payload;
- testes diferenciais contra estruturas de referência;
- fuzz testing de persistência;
- ciclos repetidos de checkpoint e reopen;
- teste de 500 mil operações com checkpoints/reaberturas periódicas, sem divergência observada na execução registrada;
- testes multiwriter;
- integridade de snapshots e limpeza segura de WAL;
- crash real de processo POSIX usando `SIGKILL`;
- execução em CI com Python 3.11 e 3.12;
- compilação e smoke tests dos protótipos C++;
- comparação de persistência com SQLite usando fronteiras equivalentes de commit.

O uso de `SIGKILL` é importante porque impede que o processo execute `close()` ou uma sequência de shutdown limpo. O teste, portanto, avalia o caminho de recuperação a partir dos artefatos já persistidos.

## 7. Comparação de durabilidade com SQLite

Para evitar uma comparação enganosa entre memória volátil e armazenamento transacional, o benchmark de paridade utiliza operações equivalentes e compara dois regimes: commit durável por operação e group commit de 64 operações.

Em uma execução do GitHub Actions com 5.000 operações, foram obtidos os seguintes resultados:

| Regime | Motor | Tempo (s) | Operações/s | Registros finais | Estado igual |
|---|---|---:|---:|---:|---:|
| commit por operação | BDR | 2,108245 | 2.371,64 | 1.462 | sim |
| commit por operação | SQLite | 2,368006 | 2.111,48 | 1.462 | sim |
| group commit 64 | BDR | 0,047962 | 104.249,54 | 1.462 | sim |
| group commit 64 | SQLite | 0,047516 | 105.228,46 | 1.462 | sim |

A igualdade de estado final é parte do gate: desempenho sem equivalência semântica não é aceito como resultado válido.

No commit individual, a razão observada foi aproximadamente

\[
\frac{2371.64}{2111.48}\approx 1.12,
\]

ou cerca de 12% de throughput superior para o BDR nessa execução específica. No group commit, os dois motores ficaram praticamente empatados, com pequena vantagem para SQLite.

Esses resultados **não permitem concluir que BDR é mais rápido que SQLite em geral**. Eles demonstram apenas que, nesse workload sintético, nesse runner Linux e nessas fronteiras de durabilidade, o protótipo BDR alcançou a mesma ordem de desempenho mantendo igualdade do estado final.

## 8. Reprodutibilidade

A implementação, testes e benchmarks que fundamentam este artigo estão versionados no repositório do projeto. O gate de CI inclui:

- Python 3.11;
- Python 3.12;
- suíte automatizada de testes;
- teste de crash/recovery;
- compilação de benchmarks C++;
- execução de smoke tests C++;
- benchmark de paridade BDR × SQLite.

O benchmark de paridade pode ser executado a partir do repositório com:

```bash
python benchmarks/bdr_v01_sqlite_parity.py
```

A versão científica associada a este manuscrito é **BDR v0.1.0**. Para reprodução rigorosa, recomenda-se registrar o commit Git utilizado, sistema operacional, versão de Python/compilador, hardware, sistema de arquivos e configuração de armazenamento.

## 9. Discussão

Os resultados atuais sustentam quatro conclusões limitadas.

Primeiro, o conceito de endereçamento resolutivo pode ser implementado de forma determinística e testável sem depender de uma interpretação física para que a hipótese computacional seja avaliada.

Segundo, particionamento e especialização local são caminhos tecnicamente plausíveis para estudar contenção, memória e cache locality. O benefício real depende da distribuição de chaves e do workload.

Terceiro, o protótipo deixou de ser exclusivamente in-memory. A presença de WAL, snapshots, checkpoints atômicos e recovery permite comparar o sistema com bancos persistentes sob critérios mais justos.

Quarto, os resultados atuais não justificam alegações fortes de complexidade assintótica. Uma curva de latência aproximadamente constante em uma faixa finita de `N` é evidência empírica de scaling naquele intervalo, não prova matemática de O(1) no pior caso.

## 10. Limitações

A v0.1.0 possui limitações deliberadamente explícitas:

1. é um research preview, não um banco recomendado para produção;
2. parte do motor integrado permanece em Python, enquanto várias estruturas de desempenho são protótipos C++ separados;
3. os benchmarks ainda não cobrem suficientemente dezenas ou centenas de milhões de registros;
4. resultados de armazenamento dependem fortemente de hardware, filesystem, kernel e política de `fsync`;
5. a representação de payload e o índice ainda precisam ser separados e caracterizados em escala;
6. não há prova formal de complexidade O(1) no pior caso para o sistema completo;
7. faltam comparações amplas e padronizadas com LMDB, RocksDB, LevelDB, Redis e outros motores;
8. o formato persistente é experimental e ainda não representa uma garantia permanente de compatibilidade.

## 11. Programa experimental futuro

A próxima etapa deverá avaliar:

- milhões e dezenas de milhões de registros;
- payload externo ao índice;
- `mmap` e comportamento de page cache;
- filtros Bloom/Xor para misses;
- tombstones e compaction;
- distribuição de tamanhos de chave e payload;
- concorrência multiprocesso;
- leitores com menor contenção;
- sharding persistente;
- implementação integrada C++ do motor persistente;
- comparação reprodutível com SQLite, LMDB, RocksDB, LevelDB e Redis;
- modelos analíticos de ocupação, colisão e custo local por partição.

O princípio metodológico será conservar a v0.1.0 como baseline: novas estruturas só deverão substituir o baseline quando apresentarem ganho mensurável sem regressão de integridade, recuperação ou reprodutibilidade.

## 12. Conclusão

O Banco de Dados Resolutivo v0.1.0 estabelece uma primeira implementação persistente e reproduzível de uma arquitetura de armazenamento baseada em endereçamento determinístico e particionamento local. A principal contribuição da versão não é uma alegação de superioridade universal, mas a construção de um objeto experimental auditável: há código, formato de disco documentado, testes de integridade, crash recovery real, CI, benchmarks e baseline comparativo.

A comparação com SQLite mostra que, no workload persistente avaliado, o BDR opera na mesma ordem de desempenho e produz estado final equivalente. Isso é suficiente para justificar a continuação da pesquisa, mas não para generalizar desempenho ou complexidade.

A v0.1.0 deve, portanto, ser interpretada como um **Research Preview** e como ponto de referência para testar uma hipótese mais ampla: se estruturas locais selecionadas de acordo com densidade, distribuição e workload podem formar um mecanismo de armazenamento adaptativo com vantagens mensuráveis em regimes específicos.

## 13. Disponibilidade de código e dados

O código-fonte disponível, scripts de benchmark, testes, especificação do formato de disco e histórico experimental são mantidos no repositório `marceloroldao/resolutive-DB`.

O depósito Zenodo correspondente deverá arquivar uma cópia imutável da versão utilizada no artigo. Após a publicação, o DOI do Zenodo deve ser adicionado a esta seção e ao arquivo `CITATION.cff`.

## 14. Licenciamento e propriedade intelectual

Este manuscrito pode ser distribuído para leitura, citação e avaliação científica sob **Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)**, preservando atribuição ao autor.

O software BDR possui licença separada: **BDR Academic and Non-Commercial Research License v1.0**. Ela permite uso acadêmico, educacional, avaliação e benchmarking não comercial nas condições descritas no arquivo `LICENSE` do repositório. Uso comercial requer licença separada da ETBRA Tecnologias.

A licença do manuscrito **não altera nem amplia a licença do software**. Nenhuma licença de patente é concedida pelo manuscrito ou pela licença de software. Direitos eventualmente associados ao Banco de Dados Resolutivo, Endereçamento Densitário Harmônico, Resolutive Database Engine e tecnologias relacionadas permanecem reservados aos respectivos titulares.

## 15. Declarações

**Conflitos de interesse:** o autor declara ser responsável pelo desenvolvimento do projeto BDR e possuir interesse intelectual e potencial interesse comercial relacionado à tecnologia descrita.  
**Financiamento:** não foi declarado financiamento externo específico para este trabalho.  
**Contribuição do autor:** concepção, arquitetura, desenvolvimento experimental, direção da pesquisa, interpretação dos resultados e preparação do manuscrito: Marcelo Roldão Matos.  
**Status de revisão:** preprint; não revisado por pares.

## Referências

1. SQLite Consortium. *SQLite Documentation: Atomic Commit, Write-Ahead Logging and Transactional Behavior*. SQLite documentation.
2. Celis, P. *Robin Hood Hashing*. University of Waterloo, 1986.
3. Knuth, D. E. *The Art of Computer Programming, Volume 3: Sorting and Searching*. Addison-Wesley.
4. O'Neil, P.; Cheng, E.; Gawlick, D.; O'Neil, E. “The Log-Structured Merge-Tree (LSM-Tree).” *Acta Informatica*, 33, 351–385, 1996.
5. Rosenblum, M.; Ousterhout, J. K. “The Design and Implementation of a Log-Structured File System.” *ACM Transactions on Computer Systems*, 10(1), 26–52, 1992.
6. Bernstein, P. A.; Hadzilacos, V.; Goodman, N. *Concurrency Control and Recovery in Database Systems*. Addison-Wesley, 1987.
7. Gray, J.; Reuter, A. *Transaction Processing: Concepts and Techniques*. Morgan Kaufmann, 1992.
8. Matos, M. R. *Resolutive Science Mathematical Specification (RSMS)*, release candidate 1.0-rc.1, 2026.
9. Matos, M. R. *Banco de Dados Resolutivo (BDR) / Resolutive Database Engine*, version 0.1.0, software and reproducible research repository, 2026.

---

**Copyright © 2026 Marcelo Roldão Matos.**  
**Manuscript license:** CC BY-NC-SA 4.0.  
**Software license:** BDR Academic and Non-Commercial Research License v1.0.  
**ORCID:** 0009-0003-6075-4680
