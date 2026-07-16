# Sistema de Validación de Plausibilidad APPS — documento completo para el chat de Arduino/ESP32

Copia/pega este documento completo como primer mensaje en el chat de Claude Code que va a llevar firmware + circuito. Es la versión definitiva — reemplaza cualquier versión anterior. Todas las decisiones de diseño ya están tomadas para poder construir todo hoy mismo; solo queda un punto realmente pendiente de la sesión de dudas de mañana (al final del documento).

---

## Qué es este proyecto

Reclutamiento para **UNAM Motorsports** (Fórmula SAE UNAM), área de **Data Acquisition (DAQ)**. Hay dos actividades de evaluación; tú eres responsable únicamente de la **Actividad 1**: el sistema de seguridad del pedal de aceleración (APPS). Otro chat de Claude Code lleva en paralelo la Actividad 2 (análisis de telemetría en Python) y **ambos dashboards web** — incluido el que va a consumir en vivo los datos que tu firmware expone. El protocolo Serial definido aquí es el contrato entre los dos.

**Plazo:** 13–17 de julio de 2026. **Entrega:** repositorio de GitHub, commits en formato Conventional Commits (`feat:`, `fix:`, `docs:` — obligatorio).

El dashboard web de este reto **ya existe y ya funciona** en `https://motorsports.gaxcor.space/reto1/` — corre en "modo demo" con datos simulados mientras no haya hardware conectado. En cuanto tu firmware mande datos reales por Serial, se conecta desde el navegador (Chrome/Edge) con el botón "Conectar ESP32" (Web Serial API). No necesitas construir nada del lado web, solo respetar el protocolo de esta guía.

---

## El marco normativo (por qué existe esta actividad)

Regla T.4.2 de Fórmula SAE: el pedal de aceleración debe tener **dos sensores independientes y redundantes** (APPS1 y APPS2). Si no coinciden, el sistema debe reconocerlo como peligro, alertar y **apagar el motor de inmediato**. El objetivo real: que un cable roto o un sensor fallando nunca pueda dejar el auto acelerando solo.

---

## Visión general del flujo completo (léelo antes de empezar a cablear)

1. Alguien mueve el pedal (en esta simulación: gira los dos potenciómetros, que representan APPS1 y APPS2). **APPS1 se alimenta a 5V real, APPS2 se alimenta a 3.3V** — dos rieles de alimentación distintos, confirmado por UNAM Motorsports que el sistema debe manejar dos rangos de voltaje diferentes.
2. Como APPS2 ya corre a 3.3V (el límite exacto del ADC), se conecta **directo** al ESP32, sin nada extra. APPS1 corre a 5V, así que necesita un **divisor de voltaje** (2 resistencias) antes de llegar al ESP32, para no dañar el ADC. Esto de paso ya cumple el requisito de "rango de voltaje distinto por canal" — es asimetría real (dos rieles distintos), no solo una calibración de software. El ESP32 lee ambos continuamente y los convierte a un porcentaje 0–100% cada uno.
3. El ESP32 compara ambos porcentajes. Si difieren más del umbral permitido **de forma continua por más de 100 ms**, se confirma una falla.
4. Ante falla confirmada: se enciende un LED de "falla de sistema", **se corta la corriente al motor con un relevador**, y todo esto se reporta por Serial en tiempo real.
5. Para poder demostrar esto en vivo sin romper cables físicamente: hay **2 botones físicos** (uno por canal) que simulan que ese sensor se rompió, más los **mismos 2 botones también disponibles en el dashboard web** — cualquiera de los dos dispara exactamente lo mismo, porque el ESP32 es la única fuente de verdad.
6. Dos LEDs adicionales (uno por canal) indican cuál sensor está en modo de falla forzada en ese momento.

Con esto, la demo completa se ve así: motor girando, pedal en reposo, todo verde → presionas el botón de APPS2 (físico o en la web, da igual) → LED de APPS2 se enciende → ~100ms después el LED de falla de sistema se enciende, **el motor se detiene**, y el dashboard web se pone en rojo → sueltas/presionas de nuevo → todo vuelve a la normalidad y el motor arranca otra vez.

---

## Lista de materiales (BOM) — confirmada, ya se puede comprar

| Componente | Cantidad | Especificación |
|---|---|---|
| Microcontrolador | 1 | ESP32 DevKit (30 o 38 pines, cualquier variante WROOM estándar) |
| Potenciómetro | 2 | Rotatorio, lineal (taper B), **10 kΩ**, un giro (ej. WH148 B10K, o el miniatura de Steren 10kΩ sin switch). Usar los **dos de 10kΩ idénticos** — si compraste uno de 5kΩ de más, no lo uses aquí, déjalo de repuesto. La asimetría se logra alimentando cada uno de un riel distinto (5V y 3.3V), no con el valor del potenciómetro. |
| Resistencia para divisor APPS1 | 2 | **3.3kΩ** y **4.7kΩ** — limita el canal 1 (alimentado a 5V) a ~0–2.94V antes del ADC. APPS2 se alimenta a 3.3V y va directo al ADC, no necesita divisor. |
| Pulsador momentáneo | 2 | Normalmente abierto, cualquier micro-pulsador de 6 o 12mm |
| LED 5mm | 3 | 2 para override por canal + 1 para falla de sistema. Colores distintos para diferenciar a simple vista. |
| Resistencia | 3 | 220–330Ω, una por LED (limitadora de corriente) |
| Motor DC pequeño (juguete/hobby) | 1 | 3–6V, cualquier motorcito barato tipo "motor 130" — representa "el motor" del auto |
| Módulo relevador 1 canal | 1 | 5V, con optoacoplador (los módulos genéricos tipo SRD-05VDC-SL-C funcionan bien). **No conectes el motor directo al ESP32.** |
| Portapilas (2xAA o similar) | 1 | Fuente de poder **independiente** para el motor — nunca alimentar el motor desde el pin de 3.3V/5V del ESP32 |
| Protoboard + cables jumper | — | Para la primera integración antes de pasar a la caja 3D |

---

## Mapa de pines (ESP32 DevKit)

```
POTENCIÓMETRO APPS1 (a 5V real)          POTENCIÓMETRO APPS2 (a 3.3V)
  izq → 5V                                 izq → 3.3V
  der → GND                                der → GND
  wiper → R1 (3.3kΩ) → nodo A               wiper → GPIO35 (ADC1_CH7) directo, sin divisor
  nodo A → R2 (4.7kΩ) → GND
  nodo A → GPIO34 (ADC1_CH6)

(Solo APPS1 lleva divisor, porque solo APPS1 corre a 5V. APPS2 ya corre exactamente al
límite del ADC — 3.3V — así que su wiper nunca puede superar ese voltaje por sí mismo.)

BOTÓN 1 (falla APPS1)       BOTÓN 2 (falla APPS2)
  pin A → GPIO27               pin A → GPIO26
  pin B → GND                  pin B → GND
  (INPUT_PULLUP interno, no requiere resistencia externa; presionado = LOW)

LED 1 (override APPS1)      LED 2 (override APPS2)
  ánodo → GPIO25 (vía R 220-330Ω)   ánodo → GPIO33 (vía R 220-330Ω)
  cátodo → GND                       cátodo → GND

LED DE FALLA DE SISTEMA
  ánodo → GPIO32 (vía R 220-330Ω) → cátodo → GND

MÓDULO RELEVADOR (corte de motor)
  VCC → 3.3V (o 5V si el módulo lo requiere — revisa la etiqueta del módulo)
  GND → GND
  IN  → GPIO23

  Contactos del relevador (circuito del motor, AISLADO del ESP32):
  Portapilas (+) → COM del relevador
  NO del relevador → Motor (+)
  Motor (−) → Portapilas (−)
```

GPIO34/35 son ADC-only, ideales para los potenciómetros. GPIO25/26/27/32/33/23 son pines de propósito general seguros — evita GPIO0/2/12/15 (pines de strapping) para no interferir con el arranque del ESP32.

**Por qué solo un canal lleva divisor:** UNAM Motorsports confirmó que el sistema debe manejar dos rangos de voltaje distintos — la forma más simple de lograrlo es alimentar cada potenciómetro de un riel distinto (5V y 3.3V), en vez de calcular dos proporciones de divisor. APPS2 a 3.3V ya es seguro para el ADC por sí solo (su propia fuente de poder es el límite físico de lo que puede entregar). APPS1 a 5V sí necesita el divisor para no exceder el ADC. Fórmula del divisor: `Vout = Vin × R2/(R1+R2)` → con R1=3.3kΩ y R2=4.7kΩ, `Vout_max = 5V × 4.7/8 ≈ 2.94V`.

**Ojo con la polaridad del relevador:** la gran mayoría de estos módulos genéricos son **activos en bajo** (GPIO en `LOW` energiza el relevador y cierra el contacto; `HIGH` lo abre). Eso además nos da un comportamiento "fail-safe" gratis: si el ESP32 se reinicia o pierde alimentación, el pin queda flotando/alto y el motor se queda **apagado por default**, nunca encendido sin control. Pruébalo al conectar — si tu módulo específico funciona al revés, solo invierte la condición en el código (una línea).

---

## Qué tiene que hacer el programa, capa por capa

No lo escribas todo suelto en `loop()` — sepáralo mentalmente (y en funciones) así:

1. **Lectura raw**: `analogRead()` de los dos potenciómetros, más lectura de los botones físicos con antirrebote.
2. **Comandos remotos**: leer del Serial si llegó un comando desde el dashboard web (`OVR1:1`, etc.) y aplicarlo igual que un botón físico.
3. **Calibración** (capa independiente y sustituible): convierte cada lectura raw a un porcentaje 0–100%, con su propia ventana `[rawMin, rawMax]` por canal — aquí vive la asimetría entre APPS1 y APPS2. Cambiar estos números **nunca** debe afectar la lógica de seguridad de abajo.
4. **Override manual**: si el canal está en "falla forzada" (por botón físico o por comando web), se reporta 0% sin importar lo que diga el potenciómetro real.
5. **Lógica de seguridad** (capa que no se toca al recalibrar): compara ambos porcentajes, aplica el umbral de desacuerdo y la regla de persistencia de 100 ms, decide `faultConfirmed`.
6. **Salida/actuación**: enciende los LEDs que correspondan, **corta o permite el paso de corriente al motor a través del relevador**, y emite una línea de telemetría por Serial.

---

## El código completo (un solo archivo, punto de partida real — no solo boceto)

```cpp
// ============================================================
// Sistema de Validacion de Plausibilidad APPS - UNAM Motorsports DAQ
// Reclutamiento - Reto 1
// ============================================================

// ---- Pines ----
const int PIN_APPS1 = 34, PIN_APPS2 = 35;
const int PIN_BTN1 = 27,  PIN_BTN2 = 26;
const int PIN_LED1 = 25,  PIN_LED2 = 33, PIN_LED_FAULT = 32;
const int PIN_RELAY = 23;   // modulo relevador, activo en bajo (LOW = motor encendido)

// ---- Calibracion por canal (AJUSTAR con lecturas reales del multimetro/Serial) ----
// APPS1 (5V + divisor) llega a ~2.94V max -> raw ~3648 (de 4095 a 3.3V)
// APPS2 (3.3V directo, sin divisor) llega a ~3.3V max -> raw ~4095
struct Calib { int rawMin; int rawMax; bool invert; };
Calib CAL1 = {50, 3648, false};
Calib CAL2 = {30, 4045, false};   // rango distinto = asimetria REAL (riel de 5V vs 3.3V)

const float FAULT_THRESHOLD_PCT = 10.0;   // desacuerdo maximo tolerado
const unsigned long PERSIST_MS = 100;

bool overrideCh1 = false, overrideCh2 = false;
unsigned long faultStart = 0;
bool faultConfirmed = false;

float toPercent(int raw, Calib c) {
  raw = constrain(raw, c.rawMin, c.rawMax);
  float pct = (raw - c.rawMin) * 100.0 / (c.rawMax - c.rawMin);
  return c.invert ? 100.0 - pct : pct;
}

// Antirrebote simple con toggle. Mejorar a version no bloqueante (millis())
// antes de la entrega final -- esta version con delay() es solo punto de partida.
void checkButton(int pin, bool &overrideFlag, int ledPin) {
  if (digitalRead(pin) == LOW) {
    delay(30);
    if (digitalRead(pin) == LOW) {
      overrideFlag = !overrideFlag;
      digitalWrite(ledPin, overrideFlag ? HIGH : LOW);
      while (digitalRead(pin) == LOW) {}   // esperar a que se suelte
    }
  }
}

// Comandos desde el dashboard web (mismo Serial, sentido contrario). Texto plano,
// una linea por comando: OVR1:1 / OVR1:0 / OVR2:1 / OVR2:0
void checkSerialCommands() {
  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line == "OVR1:1")      { overrideCh1 = true;  digitalWrite(PIN_LED1, HIGH); }
    else if (line == "OVR1:0") { overrideCh1 = false; digitalWrite(PIN_LED1, LOW);  }
    else if (line == "OVR2:1") { overrideCh2 = true;  digitalWrite(PIN_LED2, HIGH); }
    else if (line == "OVR2:0") { overrideCh2 = false; digitalWrite(PIN_LED2, LOW);  }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
  pinMode(PIN_LED_FAULT, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);   // motor encendido al arrancar (sin falla todavia)
}

void loop() {
  checkButton(PIN_BTN1, overrideCh1, PIN_LED1);
  checkButton(PIN_BTN2, overrideCh2, PIN_LED2);
  checkSerialCommands();

  int raw1 = analogRead(PIN_APPS1);
  int raw2 = analogRead(PIN_APPS2);

  float pct1 = overrideCh1 ? 0.0 : toPercent(raw1, CAL1);
  float pct2 = overrideCh2 ? 0.0 : toPercent(raw2, CAL2);

  float delta = abs(pct1 - pct2);
  unsigned long faultMs = 0;
  if (delta > FAULT_THRESHOLD_PCT) {
    if (faultStart == 0) faultStart = millis();
    faultMs = millis() - faultStart;
    faultConfirmed = faultMs > PERSIST_MS;
  } else {
    faultStart = 0; faultMs = 0; faultConfirmed = false;
  }

  digitalWrite(PIN_LED_FAULT, faultConfirmed ? HIGH : LOW);
  digitalWrite(PIN_RELAY, faultConfirmed ? HIGH : LOW);   // HIGH = corta el motor

  Serial.print("{\"t\":");        Serial.print(millis());
  Serial.print(",\"apps1_raw\":"); Serial.print(raw1);
  Serial.print(",\"apps2_raw\":"); Serial.print(raw2);
  Serial.print(",\"apps1_pct\":"); Serial.print(pct1, 1);
  Serial.print(",\"apps2_pct\":"); Serial.print(pct2, 1);
  Serial.print(",\"fault\":");     Serial.print(faultConfirmed ? "true" : "false");
  Serial.print(",\"fault_ms\":");  Serial.print(faultMs);
  Serial.print(",\"override1\":"); Serial.print(overrideCh1 ? "true" : "false");
  Serial.print(",\"override2\":"); Serial.print(overrideCh2 ? "true" : "false");
  Serial.println("}");

  delay(10);   // ~100 muestras/segundo
}
```

Esto es un punto de partida real y funcional, no solo un boceto — pero antes de la entrega final: mejora el antirrebote a una versión no bloqueante con `millis()` (el `delay()`/`while()` actual bloquea el resto del programa mientras el botón está presionado), y separa esto en archivos/funciones bien nombrados en vez de un solo `.ino` gigante. La rúbrica pide explícitamente "buena estructura, sigue las convenciones".

---

## Cómo probarlo, en orden

1. **Bring-up básico primero** (ver `PASO1_CONEXION_BASICA.md` si lo tienes a la mano): confirma que cada potenciómetro mueve su `raw` de 0 a 4095 sin saltos raros, y que cada botón enciende su LED correspondiente.
2. Sube el código completo de arriba. Abre el Monitor Serial a 115200 baud y confirma que llega una línea JSON cada ~10ms.
3. Mueve el pedal (ambos potenciómetros a la vez) — `apps1_pct`/`apps2_pct` deben subir y bajar juntos, `fault` debe quedarse en `false`.
4. Presiona el botón 1 (o 2) — su LED se enciende, unos 100ms después `fault` pasa a `true`, el LED de falla de sistema se enciende, **y el motor debe detenerse**.
5. Suelta/vuelve a presionar el botón — todo debe regresar a la normalidad y el motor debe volver a girar.
6. Conecta el dashboard web (`https://motorsports.gaxcor.space/reto1/`, botón "Conectar ESP32") y repite el punto 4 usando los botones "Forzar falla" de la página en vez de los físicos — debe verse exactamente igual.

---

## Protocolo Serial (contrato con el dashboard web — ya implementado del lado web)

Saliente, una línea JSON por muestra, terminada en `\n`, a 115200 baud:

```json
{"t":12345,"apps1_raw":512,"apps2_raw":3021,"apps1_pct":24.6,"apps2_pct":73.8,"fault":false,"fault_ms":0,"override1":false,"override2":false}
```

Entrante, texto plano, una línea por comando:

| Línea recibida | Efecto |
|---|---|
| `OVR1:1` / `OVR1:0` | activa/desactiva el override de APPS1 |
| `OVR2:1` / `OVR2:0` | activa/desactiva el override de APPS2 |

El dashboard web (`reto1/index.html`) ya sabe leer y mandar todo esto. Asegúrate de que `checkSerialCommands()` se llame en cada vuelta del `loop()`, si no los botones de la web no van a tener efecto.

---

## Simulación en Wokwi (antes del hardware físico)

En [wokwi.com](https://wokwi.com), crea un proyecto nuevo de tipo **ESP32**, agrega desde el catálogo: 2 potenciómetros, 2 pulsadores, 3 LEDs, y un módulo de relevador (busca "relay module" en las piezas de Wokwi — sí está disponible) con un motor DC. Conecta todo según el mapa de pines de arriba y pega el código completo. Esto cubre directo el entregable "Simulación del circuito" (2 puntos) antes de tocar una sola pieza física.

---

## Dónde vive esto en el repo

```
apps-system/
├── firmware/       ← tu código (ESP32, .ino o PlatformIO)
├── simulation/      ← proyecto de Wokwi
└── web-dashboard/   ← NO es tuyo, ya construido, vive en reto1/ en la raíz del sitio
```

Commits con scope claro, ej. `feat(firmware): agrega corte de motor via relevador`, `fix(firmware): corrige calibracion de apps2`.

---

## Único punto realmente pendiente de mañana

- Si el criterio de asimetría (`APPS1 creciente / APPS2 con rango distinto`, ambos crecientes) es aceptable o si UNAM Motorsports espera una relación específica tipo "uno crece mientras el otro decrece" (como en algunos autos reales). El código ya soporta ambos casos (`invert` en `Calib`), así que cambiarlo después es solo ajustar una constante, no rediseñar nada.
