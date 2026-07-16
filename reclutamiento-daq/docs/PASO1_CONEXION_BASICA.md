# Paso 1 — Conexión básica ESP32 (antes de la lógica de seguridad)

Copia/pega esto en el chat de Arduino como el primer paso a resolver, **antes** de tocar `CONTEXTO_ARDUINO.md` (ese ya tiene la lógica completa de seguridad — este documento es solo para confirmar que el cableado funciona).

**Objetivo:** un solo archivo `.ino`, lo más simple posible, que conecte el ESP32 con los 2 potenciómetros, los 2 botones y los 2 LEDs, y muestre por Serial que cada uno responde. Nada de calibración, nada de reglas de plausibilidad todavía — eso viene después, una vez que esto funcione.

---

## Conexiones (mismos pines que en el documento grande, para no tener que recablear después)

```
POTENCIÓMETRO 1 (a 5V)                  POTENCIÓMETRO 2 (a 3.3V)
  pata izq → 5V                           pata izq → 3.3V
  pata der → GND                          pata der → GND
  wiper → R1 (3.3kΩ) → nodo A              wiper → GPIO35 directo, sin resistencias
  nodo A → R2 (4.7kΩ) → GND
  nodo A → GPIO34

BOTÓN 1                        BOTÓN 2
  pata A → GPIO27                pata A → GPIO26
  pata B → GND                   pata B → GND

LED 1                          LED 2
  ánodo (pata larga) → GPIO25 → resistencia 220-330Ω → LED  ánodo → GPIO33 → resistencia → LED
  cátodo (pata corta) → GND      cátodo → GND
```

Importante: UNAM Motorsports confirmó que el sistema debe manejar dos rangos de voltaje distintos. La forma más simple: **potenciómetro 1 alimentado a 5V real** (necesita divisor de 2 resistencias antes del ADC, para no exceder 3.3V) y **potenciómetro 2 alimentado directo a 3.3V** (ya es seguro por sí solo, va directo al pin, sin nada más). El detalle completo está en `CONTEXTO_ARDUINO.md`. Los botones no necesitan resistencia externa, el código activa el pull-up interno.

---

## El único archivo: `bringup_test.ino`

```cpp
const int PIN_APPS1 = 34;
const int PIN_APPS2 = 35;
const int PIN_BTN1  = 27;
const int PIN_BTN2  = 26;
const int PIN_LED1  = 25;
const int PIN_LED2  = 33;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
}

void loop() {
  int raw1 = analogRead(PIN_APPS1);
  int raw2 = analogRead(PIN_APPS2);

  bool btn1 = (digitalRead(PIN_BTN1) == LOW);   // presionado = LOW (pull-up)
  bool btn2 = (digitalRead(PIN_BTN2) == LOW);

  digitalWrite(PIN_LED1, btn1 ? HIGH : LOW);
  digitalWrite(PIN_LED2, btn2 ? HIGH : LOW);

  Serial.print("APPS1 raw: ");   Serial.print(raw1);
  Serial.print("   APPS2 raw: ");Serial.print(raw2);
  Serial.print("   BTN1: ");     Serial.print(btn1 ? "presionado" : "libre");
  Serial.print("   BTN2: ");     Serial.println(btn2 ? "presionado" : "libre");

  delay(100);
}
```

Sube esto, abre el Monitor Serial a 115200 baud, y deberías ver una línea nueva cada 100ms.

---

## Cómo confirmar que todo está bien conectado

1. **Gira el potenciómetro 1** de un extremo al otro → `APPS1 raw` debe moverse suavemente de cerca de `0` a cerca de `~3650` (no 4095 — el divisor le recorta el tope, eso es correcto). Si se queda fijo sin moverse, revisa que el wiper esté conectado al nodo A y no directo a GPIO34.
2. Repite lo mismo con el **potenciómetro 2** en `APPS2 raw` → aquí sí debe llegar cerca de `4095` (no lleva divisor, es su rango completo) — así se ve la asimetría desde este primer paso: cada canal topa en un número distinto.
3. **Presiona el botón 1** → debe imprimir `presionado` y el LED 1 debe encender. Suéltalo → vuelve a `libre` y el LED se apaga.
4. Repite con el **botón 2** y el LED 2.
5. Confirma que nada esté cruzado (que presionar el botón 1 no encienda el LED 2, por ejemplo).

Si los 5 puntos anteriores funcionan, el cableado está confirmado y ya se puede pasar a `CONTEXTO_ARDUINO.md`, que agrega sobre esta misma base: calibración por canal, comparación entre los dos APPS, la regla de persistencia de 100ms, el formato JSON por Serial, y la lectura de comandos desde la página web.

---

## Sobre Wokwi (opcional, para probar sin hardware todavía)

Si quieres probar este mismo código antes de tener las piezas físicas: en [wokwi.com](https://wokwi.com) puedes crear un proyecto nuevo de tipo **ESP32**, agregar 2 potenciómetros, 2 pulsadores y 2 LEDs desde su catálogo de piezas, conectarlos a los mismos pines de la tabla de arriba, pegar este código, y correr la simulación directo en el navegador — no hay que instalar nada. Es el mismo simulador que ya está sugerido para la entrega final ("Simulación del circuito", 2 puntos de la rúbrica), así que este primer proyecto de Wokwi se puede ir ampliando después en vez de empezar uno nuevo.
