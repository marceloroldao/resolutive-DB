import time
import random
import hashlib
import bisect
import sys

# =====================================================================
# 1. MOTOR BDR V2 - ESPAÇO RESOLUTIVO MULTIDIMENSIONAL COM ACESSO O(1)
# =====================================================================

class EncoderResolutivoV2:
    """
    Encoder Determinístico e Decorrelacionado (SHA-256 Sliced).
    Mapeia a chave para as dimensões (rho_R, phi_quant, signature_64).
    """
    @staticmethod
    def encode(key_str: str, space_size_rho: int, phase_buckets: int = 65536):
        h = hashlib.sha256(key_str.encode('utf-8')).digest()
        h_rho = int.from_bytes(h[0:8], byteorder='big')
        rho_R = h_rho % space_size_rho
        h_phi = int.from_bytes(h[8:16], byteorder='big')
        phi_quant = h_phi % phase_buckets
        signature = int.from_bytes(h[16:24], byteorder='big')
        return rho_R, phi_quant, signature

class BancoDeDadosResolutivoV2:
    """
    Motor BDR v2: Endereçamento Sub-Matricial Direto.
    Acesso direto por [rho_R][(phi_quant, signature)] sem loop linear 'for'.
    """
    def __init__(self, space_size_rho: int = 100_000, phase_buckets: int = 65536):
        self.space_size_rho = space_size_rho
        self.phase_buckets = phase_buckets
        self.buckets = [None] * space_size_rho
        self.count = 0

    def insert(self, key_id: str, payload: dict):
        rho_R, phi_quant, sig = EncoderResolutivoV2.encode(key_id, self.space_size_rho, self.phase_buckets)
        if self.buckets[rho_R] is None:
            self.buckets[rho_R] = {}
        self.buckets[rho_R][(phi_quant, sig)] = (key_id, payload)
        self.count += 1

    def query(self, key_id: str):
        rho_R, phi_quant, sig = EncoderResolutivoV2.encode(key_id, self.space_size_rho, self.phase_buckets)
        cesto = self.buckets[rho_R]
        if cesto is None:
            return None
        item = cesto.get((phi_quant, sig))
        if item and item[0] == key_id:
            return item[1]
        return None

def run_density_experiment():
    FIXED_M = 100_000
    N_sizes = [10_000, 50_000, 100_000, 500_000, 1_000_000, 2_000_000]
    num_queries = 1_000
    print("=" * 85)
    print("   BENCHMARK RESOLUTIVO V2 - TESTE DE ALTA DENSIDADE (M = CONSTANTE = 100.000)")
    print("   Avaliando o impacto do aumento da taxa de ocupação λ = N / M no tempo de busca O(1)")
    print("=" * 85)
    print(f"{'N (Registros)':<14} | {'Densidade λ':<12} | {'BDR v2 (µs)':<14} | {'Dict Python (µs)':<16} | {'Busca Bin (µs)':<15}")
    print("-" * 85)
    for N in N_sizes:
        lambda_density = N / FIXED_M
        keys = [f"ETBRA_RESOLUTIVE_KEY_{i:08d}" for i in range(N)]
        bdr = BancoDeDadosResolutivoV2(space_size_rho=FIXED_M, phase_buckets=65536)
        py_dict = {}
        sorted_keys = []
        for k in keys:
            payload = {"data": i}
            bdr.insert(k, payload)
            py_dict[k] = payload
            sorted_keys.append(k)
        sorted_keys.sort()
        query_sample = [random.choice(keys) for _ in range(num_queries)]
        t0 = time.perf_counter()
        for qk in query_sample:
            _ = bdr.query(qk)
        t1 = time.perf_counter()
        avg_bdr = ((t1 - t0) / num_queries) * 1_000_000
        t0 = time.perf_counter()
        for qk in query_sample:
            _ = py_dict.get(qk)
        t1 = time.perf_counter()
        avg_dict = ((t1 - t0) / num_queries) * 1_000_000
        t0 = time.perf_counter()
        for qk in query_sample:
            idx = bisect.bisect_left(sorted_keys, qk)
            _ = sorted_keys[idx] if idx < len(sorted_keys) and sorted_keys[idx] == qk else None
        t1 = time.perf_counter()
        avg_bisect = ((t1 - t0) / num_queries) * 1_000_000
        print(f"{N:<14,d} | {lambda_density:<12.2f} | {avg_bdr:<14.4f} | {avg_dict:<16.4f} | {avg_bisect:<15.4f}")
    print("=" * 85)

if __name__ == "__main__":
    run_density_experiment()
