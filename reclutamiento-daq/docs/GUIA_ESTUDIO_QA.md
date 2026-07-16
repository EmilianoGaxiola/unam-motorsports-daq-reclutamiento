# Guía de estudio para la sesión de Q&A (17 de julio)

Este documento es solo para ti — no es parte del entregable. Es tu chuleta para poder explicar **de tu propia voz** todo lo que hay en el repo, aunque yo haya escrito la mayoría del código. Está organizado por tema, no por archivo, porque así es como probablemente te van a preguntar.

---

## 0. Qué falta (estado real, sin adornos)

Ya está completo y probado en vivo (según tu último mensaje: circuito físico armado, dashboard conectado por WiFi, funcionando al 100%):

- Firmware Reto 1 (`apps-system/firmware/MotorSports.ino`) — terminado, probado con hardware real.
- Dashboard Reto 1 (`reto1/index.html`) — Serial + WiFi, probado en vivo.
- Pipeline + notebook Reto 2 — ejecutado de verdad, con gráficas reales y un reporte técnico de 300 palabras basado en hallazgos reales (no placeholders).
- Dashboard Reto 2 (`reto2/index.html`) — replay del CSV con los mismos detectores que el notebook.
- Documentación de diseño del Reto 1 (`EXPLICACION_RETO1.md`) — **recién sincronizada** con los valores y el circuito reales (ya no tiene `[MEDIR]`).
- Todo commiteado y pusheado a GitHub con Conventional Commits.

Lo único que queda abierto:

- **Simulación de Wokwi**: dijiste que ya la tienen pero que el servidor está saturado y tarda en compilar. Eso no depende de mí — solo falta que termine de compilar y (si aplica) subir el link/proyecto al repo.
- **Repasar tú mismo el circuito y el código** — que es exactamente para lo que es este documento y la lista de archivos de abajo.
- Nada más del lado técnico está pendiente. El resto de este documento es preparación para que puedas defenderlo de palabra.

---

## 1. Qué archivos hojear (y qué buscar en cada uno)

No necesitas leer todo línea por línea. Con esto es suficiente para poder hablar de cada pieza con confianza:

| Archivo | Qué es | Qué mirar |
|---|---|---|
| `apps-system/firmware/MotorSports.ino` | El firmware real, 172 líneas, un solo archivo | Las 6 capas del programa (raw → comandos → calibración → override → seguridad → salida). Es corto, léelo completo una vez. |
| `reclutamiento-daq/docs/EXPLICACION_RETO1.md` | Explicación de diseño ya sincronizada con el firmware real | Es tu resumen en español de por qué cada decisión de circuito/código es como es. Si memorizas un solo documento del Reto 1, es este. |
| `reto1/index.html` | Dashboard web (HTML+JS, un solo archivo) | Busca `ingest(` (procesa cada muestra), `connectSerial`/`connectWifi` (cómo se conecta), y los dos `<canvas>` (gráficas). No necesitas el CSS. |
| `reclutamiento-daq/telemetry-analysis/notebook/pipeline.py` | Las funciones de análisis del Reto 2, ~170 líneas | Las constantes al inicio (umbrales) y las 3 funciones `detect_*`. `detect_persistent_condition` es la regla de 100ms compartida por las tres. |
| `reclutamiento-daq/telemetry-analysis/notebook/analisis_telemetria.ipynb` | El notebook ya ejecutado con gráficas reales | Ábrelo y lee las celdas de texto (markdown) en orden — cuentan la historia completa del análisis, ya con resultados reales, no genéricos. |
| `reto2/index.html` | Dashboard de replay del Reto 2 | Mismo criterio que `pipeline.py` pero en JavaScript — busca las mismas constantes (`PLAUS_TH`, `SAT_TH`, `PERSIST_SAMPLES`) para confirmar que dicen lo mismo en ambos lados. |
| `README.md` (raíz) | Mapa del repo | Para poder navegar rápido si te piden abrir algo en vivo. |

No hace falta que repases `PLAN_IMPLEMENTACION.md`, `PREGUNTAS_MANANA.md`, `PASO1_CONEXION_BASICA.md` ni `CONTEXTO_ARDUINO.md` — son documentos de planeación/historial de cómo se construyó, no explican el resultado final (y `CONTEXTO_ARDUINO.md` ahora tiene una nota al inicio aclarando que está desactualizado a propósito).

---

## 2. Reto 1 — Sistema de Validación de Plausibilidad APPS

### La normativa (por qué existe esto)

Regla T.4.2 de Fórmula SAE: el pedal de aceleración necesita **dos sensores independientes y redundantes**. Si no coinciden, hay que detectarlo y cortar el motor de inmediato. La razón de fondo: un solo sensor no puede distinguir "el pedal está a fondo" de "el sensor se rompió y se quedó pegado en un valor alto" — necesitas un segundo sensor independiente para poder comparar.

### El circuito — puntos que te pueden preguntar

- **¿Por qué dos potenciómetros y no uno?** Redundancia real: si dependieras de un solo sensor, una falla de ese sensor sería indistinguible de "pedal a fondo".
- **¿Por qué engranes y no un eje compartido (potenciómetro dual-gang)?** Si compartieran eje mecánico, una falla mecánica (diente pelado, engrane zafado) rompería ambos sensores **al mismo tiempo y de la misma forma** — el sistema de plausibilidad nunca lo detectaría porque seguirían "de acuerdo". Con engranes independientes, una falla mecánica real sí produce desacuerdo detectable.
- **¿Cómo se logra que cada canal tenga un rango de voltaje distinto (asimetría real)?** Ambos potenciómetros están a 5V, pero cada uno tiene una **resistencia en serie distinta** entre el riel de 5V y el potenciómetro: 6.8kΩ en APPS1, 15kΩ en APPS2. Esa resistencia + el propio potenciómetro forman un divisor que limita el voltaje máximo de cada canal por debajo de 3.3V (el límite del ADC del ESP32) — y como las resistencias son distintas, el techo real de cada canal es distinto y medible con multímetro (~1.80V en APPS1, ~2.74V en APPS2).
- **¿Por qué no un divisor de dos resistencias (una arriba y una abajo)?** Simplificación consciente: con una sola resistencia en serie basta para limitar el máximo. La contraparte honesta (dilo si te preguntan a fondo): así armado, el circuito no puede distinguir un corto real a tierra de "pedal en posición mínima", porque ambos casos leerían `raw≈0`. Un sistema de producción real pondría también una resistencia en el extremo bajo para que un corto se vea como algo fuera de rango.
- **¿Por qué un servo y no un motor con relevador?** Un servo de rotación continua (MG90S) gira o se detiene según el ancho de pulso PWM que recibe (1500µs = detenido, 1700µs = girando) — un solo cable de señal, sin necesitar relevador ni fuente de poder aparte, pero demuestra igual de bien "el motor se detiene ante una falla".
- **¿Cómo se simula la falla en vivo?** No hay botones físicos — se manda un comando de texto (`OVR1:1`, `OVR2:1`) desde el dashboard web, por Serial (cable) o WebSocket (WiFi). Ambos caminos llaman a la misma función (`applyCommand()`) en el firmware, así que el comportamiento es idéntico sin importar el medio.

### El código — las 6 capas (dilo en este orden, es la estructura real de `MotorSports.ino`)

1. Lectura raw (`analogRead` en GPIO34/35).
2. Comandos remotos (Serial o WebSocket → misma función `applyCommand()`).
3. Calibración (`toPercent()`, ventana `[rawMin, rawMax]` propia por canal).
4. Override manual (si está forzado, reporta 0% sin importar la lectura real).
5. Lógica de seguridad (compara los dos porcentajes, umbral 10%, persistencia 100ms).
6. Salida (mueve el servo, actualiza LEDs, transmite JSON por Serial y WebSocket a la vez).

### La regla de persistencia de 100ms — por qué existe

Si cortaras el motor ante **cualquier** desacuerdo instantáneo, un solo pico de ruido eléctrico apagaría el motor sin que haya una falla real. Exigir que el desacuerdo se sostenga por más de 100ms filtra el ruido puntual y solo confirma una falla cuando es sostenida — el mismo principio de "debounce" que en un botón, aplicado a seguridad.

### Calibración — los números reales

| Canal | `rawMin` | `rawMax` | Voltaje máx. real |
|---|---|---|---|
| APPS1 | 150 | 2239 | ≈1.80V |
| APPS2 | 300 | 3401 | ≈2.74V |

Estos son los valores que están puestos ahora mismo en el firmware (`CAL1`, `CAL2` en `MotorSports.ino`), medidos con el Monitor Serial sobre el circuito físico real.

---

## 3. Reto 2 — Diagnóstico de Telemetría

### El dato de entrada

`data.csv`: 30,000 muestras a 100Hz = 5 minutos de registro. Columnas: `Timestamp`, `accel_x`, `accel_y`, `brake_pressure`, `throttle`.

### Acondicionamiento de señal

- Filtro Butterworth **pasa-bajas, orden 4, corte 10Hz**, aplicado con `scipy.signal.filtfilt` — que aplica el filtro hacia adelante y hacia atrás, cancelando el retraso de fase (**fase cero**: la señal filtrada no está corrida en el tiempo respecto a la cruda).
- **¿Por qué importa la fase cero?** Si el filtro introdujera retraso, un evento que en realidad pasó en t=100.00s se vería en la gráfica filtrada como si hubiera pasado un poco después — mentira temporal que rompe cualquier diagnóstico de "qué pasó primero".
- **¿Por qué reportar |G| sobre la señal filtrada y no la cruda?** La cruda mezcla la aceleración real con ruido de alta frecuencia (vibración, ruido eléctrico del ADC). Un solo pico de ruido puede inflar el máximo reportado — en este dataset, |G| máxima cruda = 2.97g vs filtrada = 1.46g. Reportar la cruda sería literalmente mentir sobre la carga máxima real del auto.

### Diagrama G-G — el diagnóstico (esto es lo más probable que te pregunten a fondo)

La envolvente NO es el diamante simétrico esperado. Hay **dos fenómenos distintos superpuestos** — sé capaz de explicar que son dos cosas separadas, no una sola:

1. **Sesgo de calibración de cero (offset), sostenido durante toda la sesión**: `accel_x` va de -0.06g a 1.07g y `accel_y` de -0.33g a 1.10g — casi sin excursión negativa en ningún eje, con 8.5% de las muestras pegadas al techo de ~1.0g en `accel_x`. Un IMU bien calibrado en reposo mide ~0g; si el punto cero se corrió durante el montaje físico, toda la señal se reporta desplazada hacia el positivo. Esto es un problema de **puesta a punto del sensor**, no de la física del auto (un auto real sí frena y gira hacia ambos lados).
2. **Pérdida de señal puntual de 1.5s** (banda vertical separada en `accel_x≈0`): es el evento descrito abajo — no está relacionado con el offset, es una falla transitoria e independiente.

### Los tres detectores de falla (todos con la misma regla de persistencia >100ms)

| Detector | Criterio | Resultado real encontrado |
|---|---|---|
| **Pérdida de señal** | Desviación estándar de `accel_x` **o** `accel_y` (por separado) colapsa bajo `1e-3`g en ventana móvil de 20 muestras | 1 evento: t=145.97s→147.46s (1.5s), `accel_x` congelado ~0g mientras `accel_y` seguía variando normal |
| **Plausibilidad** | `brake_pressure` **y** `throttle` ambos >50% al mismo tiempo | 1 evento: t=60.11s→60.40s (300ms) |
| **Saturación** | `\|accel_x\|` o `\|accel_y\|` > 0.98g | 1 evento: t=221.28s→221.37s |

**El punto más importante que debes poder explicar de memoria:** el detector de pérdida de señal originalmente exigía que **ambos** ejes colapsaran a la vez (lógica `Y`), y con ese criterio no encontraba nada. Lo corregí a que baste con que **uno solo** de los dos ejes colapse (lógica `O`), porque una falla real de integridad de un sensor normalmente afecta un solo canal/eje, no los dos simultáneamente — exigir que fallaran los dos a la vez era un criterio que estructuralmente nunca iba a poder detectar la falla más común y realista. Con ese cambio apareció el evento real de 1.5s. Esto es exactamente el tipo de razonamiento que demuestra que entiendes el problema y no solo copiaste código.

### Por qué 50% como umbral de plausibilidad

Se probó con distintos umbrales: con >20% el "evento" cubre 73% del archivo completo (no es un evento raro, es ruido de la métrica); con 80-90% ya no aparece ningún segmento sostenido >100ms. 50% es el punto medio donde aparece exactamente un evento sostenido, consistente con que la actividad describe esto como un evento de seguridad raro y puntual, no una condición normal de manejo.

### La regla de persistencia de 100ms (mismo principio que en el Reto 1)

Los tres detectores comparten la misma función (`detect_persistent_condition` en `pipeline.py`) — evita que un solo instante de ruido puntual se reporte como una falla real. Es deliberado que Reto 1 y Reto 2 compartan este mismo principio de diseño (debounce/persistencia): es la misma idea de ingeniería aplicada dos veces.

---

## 4. Preguntas que probablemente te hagan (con respuesta corta)

- **"¿Por qué 10% de umbral y 100ms de persistencia en el Reto 1?"** → 10% es margen razonable de tolerancia mecánica/eléctrica entre dos sensores que en teoría deberían leer casi idéntico; 100ms evita apagar el motor por un pico de ruido de un solo instante.
- **"¿Qué pasa si el ESP32 se reinicia a medio camino?"** → El servo arranca en `SERVO_STOP_US` (detenido) en `setup()` — fail-safe por default, nunca arranca "corriendo sin control".
- **"¿Por qué transmites por Serial y WiFi al mismo tiempo, no solo uno?"** → Para poder demostrar la misma falla sin importar si el jurado quiere ver el cable conectado o el circuito operando de forma inalámbrica; ambos usan el mismo formato JSON y la misma lógica interna.
- **"¿Cómo sabes que el filtro no te está ocultando información real?"** → Corte de 10Hz es conservador para dinámica vehicular (la mayoría de la dinámica real de chasis rara vez pasa de ~8-15Hz); y se reporta también la métrica cruda como referencia explícita para poder comparar.
- **"¿Por qué el diagrama G-G no es simétrico, es un error tuyo?"** → No es un error del análisis, es lo que dice la data: un sesgo de calibración de cero del sensor físico. El pipeline lo diagnostica correctamente, no lo esconde ni lo "arregla" artificialmente.
