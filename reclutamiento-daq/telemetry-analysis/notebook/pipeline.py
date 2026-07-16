"""
Pipeline de diagnóstico de telemetría — UNAM Motorsports DAQ.

Funciones reutilizables para el notebook `analisis_telemetria.ipynb`.
Los umbrales marcados como PROVISIONAL están pendientes de confirmar en la
sesión de dudas del 14 de julio (ver docs/PREGUNTAS_MANANA.md) y viven aquí
como constantes para poder ajustarlos en un solo lugar.
"""

from dataclasses import dataclass

import numpy as np
import pandas as pd
from scipy.signal import butter, filtfilt

SAMPLE_RATE_HZ = 100.0
DT = 1.0 / SAMPLE_RATE_HZ

# --- Filtro pasa-bajas (acondicionamiento de señal) ---
FILTER_ORDER = 4
CUTOFF_HZ = 10.0  # PROVISIONAL: no encontramos ruido localizado a alta velocidad;
                   # 10 Hz es un punto de partida razonable para IMUs vehiculares
                   # (la dinámica real del chasis rara vez pasa de ~8-15 Hz).

# --- Regla de persistencia (debounce) ---
PERSISTENCE_MS = 100.0
PERSISTENCE_SAMPLES = int(round(PERSISTENCE_MS / 1000.0 * SAMPLE_RATE_HZ))

# --- Umbral de plausibilidad freno+acelerador ---
# PROVISIONAL (pregunta 3 de docs/PREGUNTAS_MANANA.md): con >20% el "evento"
# cubre el 73% del archivo (no es un evento raro); con >80-90% ya no hay
# segmentos sostenidos >100ms. Se deja en 50% como punto medio documentado
# mientras se confirma el criterio real.
BRAKE_THROTTLE_THRESHOLD_PCT = 50.0

# --- Umbral de saturación del sensor inercial ---
# El ADC/IMU se modela normalizado en [-1, 1]; "saturado" = pegado al límite.
SATURATION_ABS_THRESHOLD = 0.98


def load_telemetry(csv_path: str) -> pd.DataFrame:
    df = pd.read_csv(csv_path)
    df = df.sort_values("Timestamp").reset_index(drop=True)
    return df


def lowpass_filtfilt(signal: np.ndarray, cutoff_hz: float = CUTOFF_HZ,
                      order: int = FILTER_ORDER, fs: float = SAMPLE_RATE_HZ) -> np.ndarray:
    """Filtro Butterworth pasa-bajas de fase cero (sin retraso temporal)."""
    nyq = fs / 2.0
    b, a = butter(order, cutoff_hz / nyq, btype="low")
    return filtfilt(b, a, signal)


def add_filtered_channels(df: pd.DataFrame) -> pd.DataFrame:
    df = df.copy()
    df["accel_x_filt"] = lowpass_filtfilt(df["accel_x"].to_numpy())
    df["accel_y_filt"] = lowpass_filtfilt(df["accel_y"].to_numpy())
    df["g_combined_filt"] = np.sqrt(df["accel_x_filt"] ** 2 + df["accel_y_filt"] ** 2)
    df["g_combined_raw"] = np.sqrt(df["accel_x"] ** 2 + df["accel_y"] ** 2)
    return df


@dataclass
class FaultSegment:
    start_idx: int
    end_idx: int
    start_t: float
    end_t: float
    duration_ms: float


def detect_persistent_condition(df: pd.DataFrame, condition: np.ndarray,
                                 min_samples: int = PERSISTENCE_SAMPLES) -> list[FaultSegment]:
    """
    Dado un arreglo booleano (condición de falla candidata por muestra),
    regresa solo los segmentos donde la condición se mantuvo activa de
    forma continua por >= min_samples (regla de persistencia de 100ms).
    Evita falsos positivos de un solo sample / ruido puntual.
    """
    segments = []
    active = False
    start = 0
    n = len(condition)
    for i in range(n):
        if condition[i] and not active:
            active = True
            start = i
        elif not condition[i] and active:
            active = False
            length = i - start
            if length >= min_samples:
                segments.append(FaultSegment(
                    start_idx=start,
                    end_idx=i - 1,
                    start_t=float(df["Timestamp"].iloc[start]),
                    end_t=float(df["Timestamp"].iloc[i - 1]),
                    duration_ms=length * DT * 1000.0,
                ))
    if active:
        length = n - start
        if length >= min_samples:
            segments.append(FaultSegment(
                start_idx=start,
                end_idx=n - 1,
                start_t=float(df["Timestamp"].iloc[start]),
                end_t=float(df["Timestamp"].iloc[n - 1]),
                duration_ms=length * DT * 1000.0,
            ))
    return segments


def detect_plausibility_violations(df: pd.DataFrame,
                                    threshold_pct: float = BRAKE_THROTTLE_THRESHOLD_PCT) -> list[FaultSegment]:
    condition = ((df["brake_pressure"] > threshold_pct) & (df["throttle"] > threshold_pct)).to_numpy()
    return detect_persistent_condition(df, condition)


def detect_sensor_saturation(df: pd.DataFrame,
                              threshold: float = SATURATION_ABS_THRESHOLD) -> list[FaultSegment]:
    condition = ((df["accel_x"].abs() > threshold) | (df["accel_y"].abs() > threshold)).to_numpy()
    return detect_persistent_condition(df, condition)


def detect_signal_dropout(df: pd.DataFrame, window: int = 20, std_threshold: float = 1e-3) -> list[FaultSegment]:
    """
    Candidato a detección de 'pérdida de señal': ventana móvil donde la
    desviación estándar de accel_x y accel_y colapsa a (casi) cero, es
    decir, el sensor se queda 'congelado' en vez de seguir midiendo ruido
    normal de vibración. PROVISIONAL: no se encontró un patrón de este tipo
    en el archivo actual (ver hallazgos en docs/PLAN_IMPLEMENTACION.md);
    esta función queda lista para cuando se confirme cómo se codifica el
    dropout en el dataset definitivo.
    """
    ax = df["accel_x"].to_numpy()
    ay = df["accel_y"].to_numpy()
    n = len(df)
    condition = np.zeros(n, dtype=bool)
    for i in range(0, n - window):
        if ax[i:i + window].std() < std_threshold and ay[i:i + window].std() < std_threshold:
            condition[i:i + window] = True
    return detect_persistent_condition(df, condition, min_samples=PERSISTENCE_SAMPLES)


def max_g_combined(df: pd.DataFrame) -> float:
    return float(df["g_combined_filt"].max())


if __name__ == "__main__":
    import sys

    csv_path = sys.argv[1] if len(sys.argv) > 1 else "../data/data.csv"
    df = load_telemetry(csv_path)
    df = add_filtered_channels(df)

    print(f"Filas: {len(df)}")
    print(f"Duración: {df['Timestamp'].max():.2f} s")
    print(f"|G| máxima (señal filtrada): {max_g_combined(df):.3f}")
    print(f"|G| máxima (señal cruda, referencia): {df['g_combined_raw'].max():.3f}")

    plaus = detect_plausibility_violations(df)
    print(f"\nViolaciones de plausibilidad (umbral {BRAKE_THROTTLE_THRESHOLD_PCT}%, >100ms): {len(plaus)}")
    for s in plaus[:10]:
        print(f"  t={s.start_t:.2f}s -> t={s.end_t:.2f}s ({s.duration_ms:.0f} ms)")

    sat = detect_sensor_saturation(df)
    print(f"\nSegmentos de saturación de sensor (>100ms): {len(sat)}")
    for s in sat[:10]:
        print(f"  t={s.start_t:.2f}s -> t={s.end_t:.2f}s ({s.duration_ms:.0f} ms)")

    dropout = detect_signal_dropout(df)
    print(f"\nSegmentos candidatos a pérdida de señal (>100ms): {len(dropout)}")
    for s in dropout[:10]:
        print(f"  t={s.start_t:.2f}s -> t={s.end_t:.2f}s ({s.duration_ms:.0f} ms)")
