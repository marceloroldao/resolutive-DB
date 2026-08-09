import time
import random
import math
import bisect
import sys

# =====================================================================
# 1. ARQUITETURA DO BANCO DE DADOS RESOLUTIVO (BDR)
# =====================================================================

class EntidadeResolutiva:
    """
    Bloco Autossuficiente de Dados (Entidade Resolutiva).
    Guarda internamente seu endereço de Densidade (rho_R), Fase (phi),
    Orientação (theta), Frequência (f_nu) e seu Payload.
    """
    def __init__(self, key_id: str, rho_R: int, phi: float, theta: float, f_nu: float, payload: dict):
        self.key_id = key_id
        self.rho_R = rho_R
        self.phi = phi
        self.theta = theta
        self.f_nu = f_nu
        self.payload = payload

class EncoderResolutivo:
    """
    Mapeia a chave/vetor do objeto para as coordenadas do Espaço Resolutivo (rho_R x phi x theta x f).
    """
    @staticmethod
    def encode(key_str: str, space_size: int):
        h = hash(key_str) & 0xFFFFFFFF
        rho_R = h % space_size
        phi = (h * 0.6180339887) % 1.0
        theta = (h * 3.14159265) % (2 * math.pi)
        f_nu = (h % 1000) / 1000.0
        return rho_R, phi, theta, f_nu

class BancoDeDadosResolutivo:
    """
    Motor BDR: busca por densidade resolutiva (rho_R) com validacao por filtro de fase (phi).
    """
    def __init__(self, space_size: int = 2_000_000):
        self.space_size = space_size
        self.buckets = [None] * space_size
        self.count = 0

    def insert(self, key_id: str, payload: dict):
        rho_R, phi, theta, f_nu = EncoderResolutivo.encode(key_id, self.space_size)
        entidade = EntidadeResolutiva(key_id, rho_R, phi, theta, f_nu, payload)
        if self.buckets[rho_R] is None:
            self.buckets[rho_R] = []
        self.buckets[rho_R].append(entidade)
        self.count += 1

    def query(self, key_id: str):
        rho_R, target_phi, _, _ = EncoderResolutivo.encode(key_id, self.space_size)
        cesto = self.buckets[rho_R]
        if not cesto:
            return None
        for ent in cesto:
            if abs(ent.phi - target_phi) < 1e-7 and ent.key_id == key_id:
                return ent
        return None

# =====================================================================
# 2. SUITE DE BENCHMARK COMPARATIVO
# =====================================================================

def run_benchmark():
    sizes = [10_000, 50_000, 200_000, 1_000_000]
    num_queries = 500

    print("=" * 75)
    print("   BENCHMARK RESOLUTIVO - COMPARACAO DE COMPLEXIDADE COMPUTACIONAL")
    print("   Autor: ETBRA Tecnologias / M. R. Matos")
    print("=" * 75)
    print(f"{'Tamanho (N)':<15} | {'BDR (us)':<15} | {'Hash Map (us)':<15} | {'Busca binaria (us)':<20}")
    print("-" * 75)

    for N in sizes:
        keys = [f"USER_KEY_RESOLUTIVE_{i:08d}" for i in range(N)]
        bdr = BancoDeDadosResolutivo(space_size=max(2 * N, 100_000))
        hash_map = {}
        sorted_keys = []

        for k in keys:
            payload = {"status": "active", "val": random.random()}
            bdr.insert(k, payload)
            hash_map[k] = payload
            sorted_keys.append(k)

        sorted_keys.sort()
        query_keys = [random.choice(keys) for _ in range(num_queries)]

        t0 = time.perf_counter()
        for qk in query_keys:
            res = bdr.query(qk)
        t1 = time.perf_counter()
        avg_bdr_us = ((t1 - t0) / num_queries) * 1_000_000

        t0 = time.perf_counter()
        for qk in query_keys:
            res = hash_map.get(qk)
        t1 = time.perf_counter()
        avg_hash_us = ((t1 - t0) / num_queries) * 1_000_000

        t0 = time.perf_counter()
        for qk in query_keys:
            idx = bisect.bisect_left(sorted_keys, qk)
            _ = sorted_keys[idx] if idx < len(sorted_keys) and sorted_keys[idx] == qk else None
        t1 = time.perf_counter()
        avg_btree_us = ((t1 - t0) / num_queries) * 1_000_000

        print(f"{N:<15,d} | {avg_bdr_us:<15.4f} | {avg_hash_us:<15.4f} | {avg_btree_us:<20.4f}")

    print("=" * 75)
    print("   TESTE DE FIDELIDADE DE FASE (phi):")
    test_key = "ETBRA_RESOLUTIVE_CHECK"
    bdr.insert(test_key, {"payload": "Sinal Preservado"})
    recovered = bdr.query(test_key)
    fidelity = 100.0 if (recovered and recovered.payload["payload"] == "Sinal Preservado") else 0.0
    print(f"   Reconstrucao por Coerencia de Fase: {fidelity:.2f}% de Fidelidade Exata")
    print("=" * 75)

if __name__ == "__main__":
    run_benchmark()
