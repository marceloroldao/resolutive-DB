"""Resolutive Database Engine (BDR) public API."""

from .core import BancoDeDadosResolutivo, EncoderResolutivo, EntidadeResolutiva
from .persistent_engine import PersistentBDR, RecoveryError

__version__ = "0.1.0"

__all__ = [
    "BancoDeDadosResolutivo",
    "EncoderResolutivo",
    "EntidadeResolutiva",
    "PersistentBDR",
    "RecoveryError",
    "__version__",
]
