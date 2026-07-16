# Explicación de diseño — Sistema de Validación de Plausibilidad APPS

Este documento cubre lo que pide la rúbrica del Reto 1: explicación del diseño de la simulación, explicación del diseño del código, y explicación de la calibración. Va destinado a vivir como `README.md` dentro de `apps-system/` en el repo final.

> **Nota:** los valores marcados como `[MEDIR]` deben reemplazarse con las lecturas reales obtenidas del multímetro / Monitor Serial una vez armado el circuito físico — esto es parte de la calibración final.

---

## 1. Diseño del circuito (simulación y físico)

### El problema que resolvemos

Bajo la Regla T.4.2 de Fórmula SAE, el pedal de aceleración debe tener dos sensores independientes (APPS1 y APPS2). Si un cable se rompe o un sensor falla y ambos dejan de coincidir, el sistema debe detectarlo y cortar el motor de inmediato — un solo sensor nunca es confiable por sí mismo porque no hay forma de distinguir "el pedal está a fondo" de "el sensor se rompió y quedó pegado en un valor alto".

### Por qué dos rieles de alimentación distintos (asimetría eléctrica real)

Se decidió alimentar **APPS1 a 5V** y **APPS2 a 3.3V** — dos rieles distintos, no el mismo voltaje con una calibración diferente por software. Esto cumple el requisito de "cada canal debe entregar un rango de señal eléctrica diferente" de forma real y verificable con un multímetro, no solo en el código.

Como el ADC del ESP32 solo tolera hasta 3.3V, el canal de 5V (APPS1) no puede conectarse directo — necesita un **divisor de voltaje** (dos resistencias, 3.3kΩ y 4.7kΩ) que recorta su señal a un máximo seguro de ~2.94V antes de llegar al microcontrolador. El canal de 3.3V (APPS2) ya es seguro por definición — su propia fuente de poder es su límite físico — así que se conecta directo al ADC, sin ningún componente adicional.

### Por qué un engrane en vez de un eje compartido

Para que ambos potenciómetros se muevan juntos al pisar el pedal, se descartó la opción de un potenciómetro doble de un solo eje (dual-gang). En su lugar, cada potenciómetro tiene su propio engrane, unidos por un engrane intermedio. La razón: si el sistema mecánico de un eje compartido se rompiera, **los dos sensores fallarían idénticos al mismo tiempo**, y el sistema de plausibilidad nunca lo detectaría (ambos seguirían "de acuerdo" en el error). Con dos potenciómetros mecánicamente independientes conectados por engranes, una falla mecánica real (un diente que se pela, un engrane que se safa) sí produce un desacuerdo detectable entre canales — es más fiel al espíritu de tener sensores *redundantes de verdad*.

### Por qué motor + relevador (y no solo un LED)

La actividad pide "apagar el motor inmediatamente" ante una falla. Para demostrarlo de forma literal (no solo simbólica), se agregó un motor DC pequeño controlado por un módulo relevador. El relevador aísla eléctricamente el circuito del motor (alimentado por su propio paquete de pilas) del circuito del ESP32 y los sensores — así el ruido eléctrico que genera un motor DC al arrancar/parar nunca contamina las lecturas de los potenciómetros. Se eligió un módulo con optoacoplador, activo en bajo, lo que da un comportamiento "fail-safe" gratuito: si el ESP32 se reinicia o pierde alimentación, el motor queda apagado por default, nunca encendido sin control.

### Por qué botones físicos *y* botones en la web (mecanismo de prueba híbrido)

Para poder demostrar la falla en vivo sin romper cables físicamente cada vez, se agregaron 2 botones (uno por canal) que fuerzan ese canal a reportar 0% (simulando un corto/cable roto). Estos mismos botones también están disponibles como comandos desde el dashboard web, enviados por el mismo canal de comunicación (Serial o WiFi) que ya se usa para la telemetría. Ambos caminos escriben al mismo estado interno del firmware — el ESP32 es la única fuente de verdad, así que la demo funciona igual de bien con o sin laptop conectada, que es justamente el principio de seguridad que se está demostrando: el sistema no depende de una computadora externa para tomar su decisión.

---

## 2. Diseño del código

El firmware separa el problema en capas, para que cambiar una no rompa las demás:

1. **Lectura raw** — `analogRead()` de ambos canales, lectura de botones con antirrebote.
2. **Comandos remotos** — escucha comandos desde el dashboard (por Serial o WebSocket) y los trata exactamente igual que un botón físico.
3. **Calibración** — convierte cada lectura raw a un porcentaje 0–100%, con una ventana `[rawMin, rawMax]` propia por canal. Esta capa se puede recalibrar sin tocar la lógica de seguridad.
4. **Override manual** — si un canal está en modo de falla forzada, reporta 0% sin importar la lectura real.
5. **Lógica de seguridad** — compara ambos porcentajes; si difieren más de un umbral **de forma continua por más de 100 ms**, confirma la falla. La regla de 100 ms existe para no apagar el motor por un pico de ruido de un solo instante — solo una discrepancia sostenida cuenta como falla real.
6. **Salida/actuación** — enciende los LEDs correspondientes, corta el motor vía el relevador, y transmite la telemetría (Serial y/o WiFi) en formato JSON.

La comunicación con el dashboard web usa el mismo protocolo sin importar el medio (cable USB o WiFi/WebSocket): una línea JSON de telemetría por muestra, y comandos de texto plano (`OVR1:1`, etc.) en el sentido contrario. Esto permite operar el sistema por cable o de forma inalámbrica sin cambiar nada de la lógica de seguridad.

---

## 3. Calibración

Cada canal se calibró midiendo su valor `raw` del ADC (0–4095) en los dos extremos del recorrido del pedal, usando el Monitor Serial como instrumento de medición:

| Canal | Riel de alimentación | Divisor | `raw` mínimo medido | `raw` máximo medido | Voltaje máximo real |
|---|---|---|---|---|---|
| APPS1 | 5V | R1=3.3kΩ, R2=4.7kΩ | `[MEDIR]` | `[MEDIR]` (esperado ≈3648) | `[MEDIR]` (esperado ≈2.94V) |
| APPS2 | 3.3V | Ninguno (directo) | `[MEDIR]` | `[MEDIR]` (esperado ≈4095) | `[MEDIR]` (esperado ≈3.3V) |

El umbral de desacuerdo se fijó en 10% (configurable en una sola constante, `FAULT_THRESHOLD_PCT`), y la persistencia en 100 ms (`PERSIST_MS`) — ambos documentados y ajustables sin tocar el resto del código.
