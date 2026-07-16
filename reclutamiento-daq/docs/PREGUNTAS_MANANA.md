# Preguntas para la sesión de dudas — 14 de julio

## Sobre el dataset (`data.csv`) — prioridad alta

Los organizadores ya confirmaron que el archivo trae fallas de componentes metidas a propósito. El análisis numérico que hice muestra que el **criterio exacto** para encontrar cada falla no es obvio con los datos tal cual, así que las preguntas van enfocadas a precisar el criterio, no a cuestionar el archivo:

1. **Pérdida de señal:** no encontré ni un solo NaN en todo el archivo, ni tramos donde `accel_x` y `accel_y` se queden "congelados" al mismo tiempo (candidato típico para representar un sensor caído). ¿Cómo está codificada la pérdida de señal en los datos — un valor centinela, un patrón específico, algo en otra columna? ¿O hay que inferirla de otra forma (ej. varianza anormalmente baja sostenida)?

2. **Ruido de resonancia a alta velocidad:** medí la rugosidad de `accel_x` en ventanas de 0.5s y la correlacioné con `speed_kmph`; la correlación salió prácticamente nula (-0.11). El PDF describe el ruido como algo que aparece "a alta velocidad". ¿Debemos esperar que el ruido estructural esté concentrado en tramos específicos, o es un ruido de fondo presente en todo el registro que simplemente hay que filtrar igual?

3. **Violación de plausibilidad (freno + acelerador simultáneos):** con el umbral `brake>20% & throttle>20%` el "evento" cubre el 73% del archivo completo (no es raro ni puntual). Si subo el umbral a `>80%` o `>90%` en ambos, ya no hay ni un solo segmento sostenido de más de 100 ms — se vuelve una coincidencia aislada de una sola muestra. ¿Cuál es el criterio exacto de "aplicados de forma simultánea" que debemos usar? ¿Hay un umbral de referencia (ej. basado en algo similar a la regla FSAE real de implausibilidad, tipo >25% de swing) o se espera que nosotros lo justifiquemos y lo dejemos como parámetro documentado?

4. **Columnas extra** (`steering_angle`, `lane_deviation`, `phone_usage`, `headway_distance`, `reaction_time`, `behavior_label`): ¿son solo contexto/relleno que debemos ignorar, o se espera que las usemos de alguna forma (por ejemplo, `behavior_label` como referencia para validar que nuestro detector encuentra las fallas en las filas correctas)?

5. **Saturación del sensor:** `accel_x` llega exactamente a 1.0 en el 42% de las filas, y ese 42% está concentrado casi por completo (96.5%) en filas con `behavior_label == Aggressive`. ¿Es correcto interpretar esto como saturación real del sensor durante maniobras agresivas (fuerzas laterales que exceden el rango calibrado), o hay otra lectura que se nos esté escapando?

## Sobre la actividad de hardware (APPS)

6. El ESP32 tiene un pin de alimentación de 5V (VIN), pero sus **pines ADC** solo toleran hasta 3.3V. Si el pedal/potenciómetro se alimenta del riel de 5V, su salida podría superar el máximo del ADC. ¿Se espera que resolvamos esto con un divisor de voltaje en la línea de señal, o la intención es alimentar los potenciómetros desde el pin de 3.3V del ESP32 y reservar el riel de 5V solo para la alimentación general del sistema?

7. "El microcontrolador debe mandar una señal de interrupción" ante corto circuito — ¿es una interrupción física por GPIO que corte una etapa de potencia (relevador/MOSFET simulando el corte del motor), o basta con reportarlo lógicamente (flag + mensaje por serial)?

8. Para la asimetría de rangos entre los dos canales APPS: ¿esperan una relación específica (ej. APPS1 creciente 0.5–4.5V y APPS2 decreciente en el mismo rango, como en autos reales) o solo piden que los rangos sean distintos entre sí, sin importar la relación exacta?

9. ¿El simulador debe ser uno específico (Wokwi, Tinkercad, Proteus) o es libre?

## Sobre entregables y logística

10. ¿El dashboard web (EXTRA, en ambas actividades) debe conectarse al hardware/datos en vivo, o basta con que funcione sobre datos de prueba/reproducidos?

11. ¿Hay una plantilla o estructura de repositorio esperada, o el formato (carpetas, README, etc.) es libre mientras se sigan Conventional Commits?

12. ¿Se evalúa un repo por persona o por equipo? ¿Cómo se entrega (link, invitación a un org de GitHub, PR)?
