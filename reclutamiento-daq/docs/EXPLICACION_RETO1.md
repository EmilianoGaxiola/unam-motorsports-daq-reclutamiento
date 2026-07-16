# Explicación de diseño — Sistema de Validación de Plausibilidad APPS

Este documento cubre lo que pide la rúbrica del Reto 1: explicación del diseño de la simulación, explicación del diseño del código, y explicación de la calibración. Refleja **el firmware real** en [`apps-system/firmware/MotorSports.ino`](../../apps-system/firmware/MotorSports.ino), ya armado y probado en protoboard.

---

## 1. Diseño del circuito

### El problema que resolvemos

Bajo la Regla T.4.2 de Fórmula SAE, el pedal de aceleración debe tener dos sensores independientes (APPS1 y APPS2). Si un cable se rompe o un sensor falla y ambos dejan de coincidir, el sistema debe detectarlo y cortar el motor de inmediato — un solo sensor nunca es confiable por sí mismo porque no hay forma de distinguir "el pedal está a fondo" de "el sensor se rompió y quedó pegado en un valor alto".

### Cómo se logra la asimetría eléctrica real

Ambos potenciómetros se alimentan del mismo riel de **5V**. Cada uno lleva una **resistencia en serie distinta** entre el riel de 5V y un extremo del potenciómetro (el otro extremo va directo a GND, el wiper directo al GPIO):

- **APPS1**: resistencia en serie de **6.8 kΩ**.
- **APPS2**: resistencia en serie de **15 kΩ**.

Esa resistencia en serie más el propio potenciómetro forman un divisor de voltaje que limita el máximo que puede alcanzar cada wiper — por diseño, por debajo de 3.3V (el límite del ADC del ESP32), sin necesidad de un segundo resistor a tierra. Como cada canal usa un valor de resistencia distinto, **el voltaje máximo real que puede alcanzar cada canal es distinto y medible con multímetro** — esto cumple el requisito de "cada canal debe entregar un rango de señal eléctrica diferente" de forma física real, no solo por software.

> **Nota honesta de diseño:** el `rawMin` de cada canal en la calibración de software (ver sección 3) es solo una ventana de calibración — físicamente el wiper de cualquiera de los dos potenciómetros puede llegar a 0V, no hay resistencia extra en el extremo bajo. Esto significa que este circuito, tal como está armado, **no puede distinguir un corto real a tierra de la posición mínima normal del pedal** (ambos casos leerían `raw≈0` y se reportarían igual). Es una simplificación consciente y suficiente para esta demo de reclutamiento — un sistema de producción real normalmente pondría una resistencia también en el extremo bajo para que un corto real se vea como un valor fuera de rango, no como "0% normal".

### Por qué un servo en vez de un motor+relevador

Para demostrar el corte del motor de forma literal, se usa un **servomotor de rotación continua (MG90S 360°)** en vez de un motor DC con módulo relevador. Un servo de rotación continua no tiene ángulo fijo — gira según el ancho de pulso PWM que recibe: ~1500µs es su punto muerto (detenido), valores mayores lo hacen girar en un sentido. Esto simplifica el circuito (un solo cable de señal al servo, sin relevador ni fuente de poder aparte para el motor) sin perder el efecto demostrativo de "el motor gira normalmente y se detiene ante una falla".

- `SERVO_STOP_US = 1500` → falla confirmada, servo detenido (fail-safe).
- `SERVO_RUN_US = 1700` → todo normal, servo girando.

### Por qué un engrane en vez de un eje compartido

Para que ambos potenciómetros se muevan juntos al pisar el pedal, se descartó la opción de un potenciómetro doble de un solo eje (dual-gang). En su lugar, cada potenciómetro tiene su propio engrane, unidos por un engrane intermedio. La razón: si el sistema mecánico de un eje compartido se rompiera, **los dos sensores fallarían idénticos al mismo tiempo**, y el sistema de plausibilidad nunca lo detectaría (ambos seguirían "de acuerdo" en el error). Con dos potenciómetros mecánicamente independientes conectados por engranes, una falla mecánica real (un diente que se pela, un engrane que se safa) sí produce un desacuerdo detectable entre canales.

### Cómo se simula la falla en vivo (Serial y WiFi, sin botones físicos)

El circuito final **no usa botones físicos**. La falla se fuerza enviando un comando de texto desde el dashboard web, por el mismo canal de comunicación que ya se usa para la telemetría:

- Por **cable USB** (Web Serial API).
- Por **WiFi** (WebSocket, puerto 81, anunciado como `esp32-apps.local` vía mDNS).

Ambos caminos llaman exactamente a la misma función en el firmware (`applyCommand()`), así que el comportamiento es idéntico sin importar el medio — el ESP32 es la única fuente de verdad, y la demo funciona igual por cable o inalámbrica.

Cada canal tiene un LED simple de 2 patas que indica su estado: **encendido = canal normal, apagado = ese canal está en modo de falla forzada** (override activo). No hay un LED separado de "falla de sistema" — la falla del sistema se ve en que el servo se detiene y en el dashboard web, que se pone en rojo.

---

## 2. Diseño del código

El firmware separa el problema en capas, para que cambiar una no rompa las demás:

1. **Lectura raw** — `analogRead()` de ambos canales (GPIO34/GPIO35).
2. **Comandos remotos** — `checkSerialCommands()` y `webSocketEvent()` escuchan comandos desde el dashboard (por Serial o WebSocket) y ambos llaman a la misma `applyCommand()`, que los trata exactamente igual sin importar el medio.
3. **Calibración** — `toPercent()` convierte cada lectura raw a un porcentaje 0–100%, con una ventana `[rawMin, rawMax]` propia por canal (`CAL1`, `CAL2`). Esta capa se puede recalibrar sin tocar la lógica de seguridad.
4. **Override manual** — si un canal está en modo de falla forzada, reporta 0% sin importar la lectura real.
5. **Lógica de seguridad** — compara ambos porcentajes; si difieren más de `FAULT_THRESHOLD_PCT` (10%) **de forma continua por más de `PERSIST_MS` (100 ms)**, confirma la falla (`faultConfirmed`). La regla de 100 ms existe para no detener el servo por un pico de ruido de un solo instante — solo una discrepancia sostenida cuenta como falla real.
6. **Salida/actuación** — mueve el servo según `faultConfirmed`, actualiza los LEDs de override, y transmite la telemetría (Serial y WebSocket simultáneamente) en una sola línea JSON armada por `buildTelemetryJson()`.

La comunicación con el dashboard web usa el mismo protocolo sin importar el medio (cable USB o WiFi/WebSocket): una línea JSON de telemetría por muestra (~100 muestras/segundo, `delay(10)`), y comandos de texto plano (`OVR1:1`, `OVR1:0`, `OVR2:1`, `OVR2:0`) en el sentido contrario.

```json
{"t":12345,"apps1_raw":512,"apps2_raw":3021,"apps1_pct":24.6,"apps2_pct":73.8,"fault":false,"fault_ms":0,"override1":false,"override2":false}
```

---

## 3. Calibración

Cada canal se calibró midiendo su valor `raw` del ADC (0–4095, referencia 3.3V) en los dos extremos del recorrido del pedal, usando el Monitor Serial como instrumento de medición:

| Canal | Riel | R serie | `rawMin` | `rawMax` | Voltaje máx. real (`rawMax/4095×3.3V`) |
|---|---|---|---|---|---|
| APPS1 | 5V | 6.8 kΩ | 150 | 2239 | ≈1.80V |
| APPS2 | 5V | 15 kΩ | 300 | 3401 | ≈2.74V |

Estos son los valores reales usados en el firmware (`CAL1 = {150, 2239, false}`, `CAL2 = {300, 3401, false}` en `MotorSports.ino`). La diferencia entre 1.80V y 2.74V como techo de cada canal es la asimetría eléctrica real pedida — viene de la diferencia entre las dos resistencias en serie (6.8kΩ vs 15kΩ), no de una calibración arbitraria por software.

El umbral de desacuerdo se fijó en 10% (`FAULT_THRESHOLD_PCT`), y la persistencia en 100 ms (`PERSIST_MS`) — ambos documentados como constantes ajustables sin tocar el resto del código.
