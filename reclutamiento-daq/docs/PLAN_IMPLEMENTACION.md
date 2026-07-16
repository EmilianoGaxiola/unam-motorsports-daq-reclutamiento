# Plan de Implementación — Reclutamiento DAQ (UNAM Motorsports)

**Periodo:** 13–17 de julio de 2026
**Rol de este chat:** liderar la parte de software/web (dashboards, pipeline de análisis de datos, estructura del repo) y coordinar las instrucciones que se le darán al otro chat de Claude Code encargado del firmware/circuito (ESP32).
**Entrega final:** repositorio de GitHub, commits en formato Conventional Commits.

---

## 0. Resumen ejecutivo

El reclutamiento tiene **dos actividades independientes**, cada una con su propia rúbrica:

| # | Actividad | Puntos totales | Núcleo técnico |
|---|-----------|-----------------|-----------------|
| 1 | Sistema de Validación de Plausibilidad APPS (Fórmula SAE) | 10 (+2 extra) | Firmware ESP32 + circuito físico + simulación |
| 2 | Diagnóstico de Telemetría y Detección de Anomalías | 10 (+2 extra) | Notebook Python (pandas/numpy/scipy) + dashboard |

Los organizadores confirmaron que el `data.csv` sí trae **fallas intencionales de componentes** metidas a propósito. Eso descarta la hipótesis de "archivo equivocado", pero el análisis numérico (ver sección 1) muestra que el criterio exacto para detectar cada falla **no es trivial ni obvio** con el archivo tal cual — hay varias cosas que conviene confirmar mañana (documento aparte: `PREGUNTAS_MANANA.md`) antes de fijar los umbrales definitivos del pipeline.

---

## 1. Lo que ya sabemos del CSV (30,000 filas, 100 Hz, 5 min, 12 columnas)

- Columnas esperadas por el PDF: `Timestamp, speed_kmph, accel_x, accel_y, brake_pressure, throttle`. Todas presentes.
- Columnas extra no mencionadas en el PDF: `steering_angle, lane_deviation, phone_usage, headway_distance, reaction_time, behavior_label`.
  - `behavior_label` tiene 3 clases perfectamente balanceadas: `Safe` (10,000), `Distracted` (10,000), `Aggressive` (10,000).
  - Hipótesis de trabajo (a confirmar): estas columnas están para dar contexto/realismo y posiblemente sirven como pista de validación (ej. la saturación de `accel_x` se concentra en filas `Aggressive`; el patrón de freno+acelerador simultáneo se concentra en `Distracted`/`Aggressive`), no como parte directa del pipeline que pide el PDF.
- **Cero valores faltantes (NaN) en todo el archivo.** Si "pérdida de señal" está codificada en los datos, no es vía huecos/NaN — probablemente hay que buscar otro patrón (valores congelados, un valor centinela, o alguna columna que no hemos identificado como bandera).
- **No encontré tramos donde `accel_x` y `accel_y` se queden congelados juntos** (candidato obvio a "pérdida de señal sostenida"). Esto sigue sin resolverse — es de las preguntas prioritarias de mañana.
- **`accel_x` satura en 1.0 en 12,756/30,000 filas (42%)**, fuertemente correlacionado con `behavior_label == Aggressive` (96.5% de esas filas). Es un candidato razonable a "saturación de sensor" ligada a maniobras agresivas (fuerzas laterales que exceden el rango calibrado del sensor) — esto sí parece consistente con lo que pide el PDF.
- **No hay correlación entre el ruido de `accel_x` y `speed_kmph`** (correlación ≈ -0.11 sobre ventanas de 0.5s). El PDF describe "ruido de resonancia estructural de alta frecuencia presente a alta velocidad" como algo localizado; en los datos actuales el ruido parece estar distribuido uniformemente en todo el registro, no concentrado a alta velocidad. Un filtro pasa-bajas fijo (Butterworth, orden 2–4, corte ~5–10 Hz sobre 100 Hz de muestreo) igual debe aplicarse como buena práctica de acondicionamiento, pero el argumento de "por qué el valor crudo es engañoso" (que pide la rúbrica) puede necesitar ajustarse a lo que realmente se observe.
- **`brake_pressure > 20` y `throttle > 20` simultáneos ocurren en 73% de las filas**, y son el 100% de las filas `Distracted`/`Aggressive`. Con umbrales más altos (50/50, 80/80, 90/90) el "evento" prácticamente desaparece — ya no hay segmentos sostenidos >100ms, solo coincidencias aisladas de una muestra. Esto sugiere que el criterio real de "aplicados de forma simultánea" probablemente no es simplemente `>20 en ambos`, sino algo más estricto o definido distinto (¿ambos por encima de cierto % alto?, ¿usar `behavior_label` como filtro?, ¿una combinación con otra columna?). **Pendiente de confirmar.**

**Conclusión operativa:** el pipeline (filtro, cálculo de G, diagrama G-G, detección con regla de 100 ms) se construye igual porque es la arquitectura correcta independientemente del dataset final. Lo que se deja como parámetro configurable y pendiente de calibrar son los **umbrales exactos** de cada detector, hasta confirmar mañana los puntos de la sección de preguntas.

---

## 2. Desglose de las dos actividades

### Actividad 1 — Sistema de Validación de Plausibilidad APPS

**Contexto normativo:** Regla T.4.2 de Fórmula SAE — el pedal de aceleración debe tener dos sensores (APPS1/APPS2) redundantes. Si no coinciden, el sistema debe detectarlo, alertar y cortar el motor.

**Requisitos funcionales:**
1. Dos potenciómetros (simulan APPS1/APPS2), monitoreados constantemente por un ESP32.
2. Los dos canales deben moverse juntos al pisar "el acelerador", pero con **rangos de señal distintos (asimetría)** — ej. APPS1 mapeado 0.5–4.5V y APPS2 con otro rango u orientación inversa. Clave para poder simular un corto real.
3. El firmware debe traducir ambas lecturas a una **escala común** (ej. 0–100%) antes de compararlas.
4. Regla de persistencia: una falla solo se vuelve definitiva si se mantiene **>100 ms continuos** (debounce contra ruido).
5. La calibración/escala debe poder cambiarse **sin tocar la lógica de seguridad** (separar capa de calibración de la capa de reglas).
6. Ante corto circuito detectado: el microcontrolador manda una "señal de interrupción".
7. Todo a 5V.

**Entregables y valor:**
- Simulación de circuito (Wokwi/Proteus/Tinkercad) — 2 pts
- Circuito físico + interfaz serial de monitoreo + explicación de calibración — 4 pts
- Código del microcontrolador (estructura, convenciones, documentación) — 4 pts
- **Dashboard web en tiempo real (EXTRA)** — 2 pts

**Materiales:** ESP32, 2 potenciómetros de perilla, resistencias.

**Nota técnica ESP32 (aclarada):** el ESP32 sí tiene un pin de alimentación de 5V (VIN, vía USB o fuente externa) — el riel de poder del sistema puede ser 5V sin problema. Lo que **no** tolera 5V son los **pines ADC** (entrada analógica), limitados a 0–3.3V. Si los potenciómetros se alimentan del riel de 5V, su salida (wiper) puede llegar a 5V, lo cual excede el máximo del ADC. Falta confirmar con el chat de Arduino/el equipo si se resuelve con un divisor de voltaje en la señal antes del ADC, o alimentando los potenciómetros desde el pin de 3.3V del ESP32 en vez del riel de 5V.

### Actividad 2 — Diagnóstico de Telemetría y Detección de Anomalías

**Contexto:** procesar un registro de telemetría (100 Hz) y generar un diagnóstico técnico automatizado, como última línea de defensa de calidad de datos antes de que ingeniería tome decisiones con ellos.

**Requisitos funcionales:**
1. Cargar `data.csv`, aplicar **filtro pasa-bajas de fase cero** (`scipy.signal.filtfilt`) para quitar ruido de resonancia estructural de alta frecuencia.
2. Calcular la **magnitud máxima del vector G combinado** `|G| = √(ax² + ay²)` **sobre la señal filtrada**, no la cruda.
3. Generar y **leer** un **Diagrama G-G**: identificar asimetría direccional en la envolvente de cargas y argumentar la causa física (ej. desbalance de frenada, distribución de peso, saturación de un eje del sensor, IMU mal alineada).
4. Detectar **fallas de integridad del sensor inercial** (caídas / pérdida de señal sostenida) y reportar el intervalo exacto.
5. Detectar **violaciones de plausibilidad** (freno + acelerador simultáneos) con regla de persistencia >100 ms, reportando el timestamp exacto.
6. Todo documentado en un notebook reproducible (pipeline determinista, 100 Hz fijo), con reporte técnico final ≤300 palabras.

**Entregables y valor:**
- Acondicionamiento de señal + carga G máxima (explicación de orden/frecuencia de corte del filtro) — 2 pts
- Diagnóstico del diagrama G-G — 3 pts
- Detección de fallas y seguridad (timestamps exactos, regla 100 ms) — 3 pts
- Reporte técnico y notebook — 2 pts
- **Dashboard interactivo de visualización (EXTRA)** — 2 pts

---

## 3. Estructura de trabajo y repartición de roles

- **Este chat (Claude Code en el VPS):** liderazgo técnico general del proyecto, estructura de repo, pipeline de análisis de datos en Python (Actividad 2 completa), ambos dashboards web (Actividad 1 EXTRA y Actividad 2 EXTRA), documentación, y redacción de las instrucciones técnicas que se le pasarán al chat de Arduino.
- **Chat de Claude Code (Arduino):** firmware del ESP32, simulación de circuito, ensamblado y calibración del circuito físico, protocolo de comunicación serial que el dashboard va a consumir.
- **Tú:** decides el rumbo, corres las preguntas en la sesión de mañana, y validas antes de construir en serio (sobre todo los umbrales de detección de la Actividad 2, que dependen de las respuestas de mañana).

### Estructura de repositorio propuesta

```
unam-motorsports-daq-reclutamiento/
├── README.md
├── docs/
│   ├── PLAN_IMPLEMENTACION.md
│   └── PREGUNTAS_MANANA.md
├── apps-system/                  # Actividad 1
│   ├── firmware/                 # código ESP32 (a cargo del chat Arduino)
│   ├── simulation/                # Wokwi/Proteus project files
│   └── web-dashboard/            # dashboard tiempo real (a cargo de este chat)
├── telemetry-analysis/           # Actividad 2
│   ├── data/
│   │   └── data.csv
│   ├── notebook/
│   │   └── analisis_telemetria.ipynb
│   └── web-dashboard/            # dashboard interactivo (extra)
└── .gitmessage / commitlint config para Conventional Commits
```

### Cronograma sugerido (13–17 julio)

| Día | Foco |
|-----|------|
| 13 jul (hoy) | Análisis del PDF y CSV, preguntas, setup de repo |
| 14 jul | Q&A con UNAM Motorsports; arrancar firmware ESP32 (chat Arduino) y esqueleto del notebook (este chat) en paralelo |
| 15 jul | Circuito físico + calibración; pipeline de detección de anomalías con umbrales ya confirmados |
| 16 jul | Dashboards web (ambos); diagrama G-G y reporte técnico |
| 17 jul | Integración final, pulido de documentación, commits, entrega |

---

## 4. Próximos pasos inmediatos

1. Ir a la sesión de mañana con las preguntas de `PREGUNTAS_MANANA.md`, priorizando las de detección de fallas en el CSV — son las que bloquean fijar los umbrales del pipeline.
2. En cuanto se confirmen los criterios exactos, recalibramos el pipeline de detección con esos parámetros.
3. Crear el repo de GitHub y decidir convención exacta de Conventional Commits (`feat:`, `fix:`, `docs:`, etc.) — se puede dejar una plantilla de `commitlint`/`.gitmessage` en cuanto se cree el repo.
4. Redactar juntos el mensaje/instrucciones inicial para el chat de Arduino (firmware ESP32 + protocolo serial), una vez resueltas las dudas de hardware.
