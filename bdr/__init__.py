"""Resolutive Database Engine (BDR) public API."""

from .atomic import (
    AtomicBDR,
    AtomicBDRError,
    AtomicBDRInvalidArgument,
    AtomicBDRIOError,
    BatchResult,
    DurabilityMode,
    Operation,
    OperationType,
)
from .core import BancoDeDadosResolutivo, EncoderResolutivo, EntidadeResolutiva
from .persistent_engine import PersistentBDR, RecoveryError

__version__ = "1.2.0rc1"

__all__ = [
    "AtomicBDR",
    "AtomicBDRError",
    "AtomicBDRInvalidArgument",
    "AtomicBDRIOError",
    "BatchResult",
    "DurabilityMode",
    "Operation",
    "OperationType",
    "BancoDeDadosResolutivo",
    "EncoderResolutivo",
    "EntidadeResolutiva",
    "PersistentBDR",
    "RecoveryError",
    "__version__",
]
