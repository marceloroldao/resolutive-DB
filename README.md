# Banco de Dados Resolutivo (BDR)

**O(1) Resolutive Database Engine — Proof of Concept**

Projeto experimental da **ETBRA Tecnologias** para investigar um mecanismo de endereçamento direto inspirado no paradigma de **Endereçamento Densitário Harmônico**, representado pelo espaço resolutivo

\[
\mathcal{R} = (\rho^R, \phi, \theta, f)
\]

onde, nesta implementação computacional:

- `rho_R` seleciona diretamente um cesto/bucket harmônico;
- `phi` funciona como assinatura de fase quantizada;
- `theta` é um metadado angular normalizado;
- `f_nu` é um metadado de frequência/estado normalizado;
- um fingerprint independente de 128 bits confirma a identidade exata da chave.

> **Status científico/engenharia:** esta versão é uma PoC de software. Os nomes e conceitos resolutivos são usados como hipótese de arquitetura. O repositório não afirma que a interpretação física subjacente esteja experimentalmente validada.

## Objetivo

Investigar se um dado pode ser transformado deterministicamente em um endereço resolutivo compacto e recuperado por acesso direto a memória, reduzindo a dependência de estruturas de busca ordenadas ou varreduras lineares.

A implementação atual usa a seguinte topologia:

```text
key / dado
    |
    v
EncoderResolutivo
    |
    +--> rho_R  ----> acesso direto ao bucket
    +--> phi    ----> slot de fase
    +--> theta  ----> metadado
    +--> f_nu   ----> metadado
    +--> fingerprint --> confirmação exata
                         |
                         v
                  EntidadeResolutiva
```

## Complexidade

A PoC deve ser avaliada com rigor. O acesso ao vetor de buckets por `rho_R` é constante, mas a resolução interna usa dicionários Python, que oferecem **O(1) esperado/amortizado**, não uma garantia matemática de O(1) no pior caso.

| Estrutura | Busca típica | Observação |
|---|---:|---|
| BDR PoC | O(1) esperado | acesso direto + hash interno de fase/fingerprint |
| Python `dict` | O(1) esperado | baseline hash convencional |
| busca binária / B-tree | O(log N) | estrutura ordenada |
| varredura linear | O(N) | referência de crescimento linear |
| K-NN exato em D dimensões | O(N·D) | comparação vetorial exata típica |

Uma versão futura só deverá declarar **O(1) garantido no pior caso** se essa propriedade for demonstrada para a estrutura completa, inclusive tratamento de colisões, crescimento, memória e adversários.

## Estrutura

```text
resolutive-DB/
├── bdr/
│   ├── __init__.py
│   └── core.py
├── benchmarks/
│   └── reproduce_o1.py
├── tests/
│   └── test_bdr.py
├── LICENSE
├── README.md
└── requirements.txt
```

## Instalação

Requer Python 3.10+.

```bash
python -m venv .venv
```

Linux/macOS:

```bash
source .venv/bin/activate
```

Windows PowerShell:

```powershell
.\.venv\Scripts\Activate.ps1
```

Instale as dependências:

```bash
pip install -r requirements.txt
```

## Exemplo mínimo

```python
from bdr import BancoDeDadosResolutivo

bdr = BancoDeDadosResolutivo(bucket_count=1 << 16)

bdr.inserir("sensor:esp32:voltage", b"12.48")
entity = bdr.obter("sensor:esp32:voltage")

print(entity.rho_R)
print(entity.phi)
print(entity.theta)
print(entity.f_nu)
print(entity.payload)
```

## API principal

### `EntidadeResolutiva`

Bloco autossuficiente imutável contendo:

```text
id
rho_R
phi
theta
f_nu
payload
fingerprint
```

### `EncoderResolutivo`

Converte uma chave canônica em um endereço resolutivo determinístico. A versão atual utiliza BLAKE2b para distribuir os estados no espaço computacional de forma reproduzível.

### `BancoDeDadosResolutivo`

Operações principais:

```python
inserir(key, payload)
buscar(key)
obter(key)
remover(key)
contem(key)
limpar()
estatisticas()
entidades()
```

## Testes automatizados

Execute:

```bash
pytest -q
```

A suíte cobre:

- determinismo do encoder;
- inserção, substituição, busca e remoção;
- reconstrução exata do payload;
- alta densidade com poucos buckets;
- concorrência por múltiplas threads;
- limpeza de slots vazios e ausência de entidades residuais;
- validação de configurações inválidas.

## Benchmark reprodutível

Execute:

```bash
python benchmarks/reproduce_o1.py
```

Por padrão são usados:

```text
N = 1.000
N = 10.000
N = 100.000
N = 1.000.000
```

O script compara:

- BDR PoC;
- Python `dict`;
- lista ordenada + busca binária;
- varredura linear.

São produzidos:

```text
benchmarks/results/latency.csv
benchmarks/results/lookup_scaling.png
```

Para uma bateria customizada:

```bash
python benchmarks/reproduce_o1.py \
  --sizes 1000,10000,100000,1000000 \
  --queries 5000 \
  --seed 42
```

O gráfico mostra **latência observada**. Uma curva aproximadamente horizontal é evidência empírica compatível com comportamento constante na faixa medida, mas isoladamente não constitui prova formal de complexidade assintótica O(1).

## Fidelidade de reconstrução

Cada consulta confirma simultaneamente:

1. bucket `rho_R`;
2. slot de fase `phi`;
3. fingerprint exato da chave.

O teste `test_exact_reconstruction_fidelity` verifica recuperação byte a byte dos payloads inseridos.

## Hipótese de evolução para embeddings

Uma extensão futura poderá receber vetores/embeddings e definir um encoder explícito

\[
E: \mathbb{R}^D \rightarrow (\rho^R,\phi,\theta,f)
\]

O ponto científico central será determinar se essa redução preserva informação suficiente para recuperação semântica útil sem reintroduzir busca aproximada de custo dependente de N ou D.

Isso deverá ser comparado diretamente com FAISS, HNSW e outros índices vetoriais usando recall@k, precision@k, latência, memória e throughput.

## Licenciamento dual

Este repositório é **source-available**, não é apresentado como software open source aprovado pela OSI.

### Uso acadêmico e educacional

Permitido gratuitamente, conforme o arquivo `LICENSE`, para:

- universidades;
- escolas;
- pesquisadores;
- estudantes;
- instituições públicas de pesquisa;
- organizações sem fins lucrativos;
- experimentação, avaliação e benchmarking não comerciais.

### Uso comercial

Exige licença comercial separada da **ETBRA Tecnologias** para, entre outros casos:

- empresas com fins lucrativos;
- startups;
- produtos ou serviços pagos;
- SaaS;
- produção;
- integração proprietária;
- uso interno que suporte atividade comercial;
- redistribuição ou monetização.

Consulte `LICENSE` para os termos completos.

## Patentes e propriedade intelectual

A licença de software **não concede direitos de patente**. Direitos eventualmente associados ao Banco de Dados Resolutivo, ao Endereçamento Densitário Harmônico, ao Resolutive Database Engine e tecnologias relacionadas permanecem expressamente reservados aos respectivos titulares.

A presença pública de código ou documentação neste repositório não deve ser interpretada como concessão implícita de licença de patente, marca, segredo industrial ou outro direito de propriedade intelectual.

## Próximas etapas técnicas

1. benchmark automatizado em CI e múltiplos sistemas;
2. perfis de memória e cache locality;
3. benchmark até `10^7` registros em máquina adequada;
4. persistência em disco/mmap;
5. implementação C++/Rust para eliminar overhead do Python;
6. encoder de embeddings com métricas de qualidade semântica;
7. comparação com FAISS/HNSW/SQLite/Redis;
8. estudo formal de colisões e limites de complexidade;
9. protocolo de serialização do bloco autossuficiente;
10. versão distribuída/sharded do espaço resolutivo.

## Autoria

**Banco de Dados Resolutivo (BDR)**  
**Resolutive Database Engine**  
ETBRA Tecnologias — 2026
