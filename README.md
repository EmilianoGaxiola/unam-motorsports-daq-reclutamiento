# Reclutamiento DAQ — UNAM Motorsports

Repositorio de reclutamiento para el área de Data Acquisition (DAQ) de UNAM Motorsports, Fórmula SAE.

## Estructura

```
├── index.html                          # Landing con acceso a ambos retos
├── reto1/                              # Reto 1 (dashboard web, EXTRA)
├── reto2/                              # Reto 2 (dashboard web, EXTRA)
├── apps-system/firmware/               # Firmware ESP32 del Reto 1
└── reclutamiento-daq/
    ├── docs/                           # Documentación de diseño y planeación
    └── telemetry-analysis/             # Pipeline y notebook del Reto 2
```

## Reto 1 — Sistema de Validación de Plausibilidad APPS

Sistema de seguridad del pedal de aceleración con dos sensores redundantes (Regla T.4.2 Fórmula SAE), corte del motor ante desacuerdo sostenido >100ms, y monitoreo en vivo por cable USB o WiFi.

- Firmware: [`apps-system/firmware/`](apps-system/firmware/)
- Dashboard web: [`reto1/`](reto1/) — en vivo en `https://motorsports.gaxcor.space/reto1/`
- Explicación de diseño: [`reclutamiento-daq/docs/EXPLICACION_RETO1.md`](reclutamiento-daq/docs/EXPLICACION_RETO1.md)

## Reto 2 — Diagnóstico de Telemetría y Detección de Anomalías

Pipeline de análisis de un registro de telemetría a 100Hz: filtro pasa-bajas de fase cero, diagrama G-G, y detección de fallas (saturación de sensor, violaciones de plausibilidad) con regla de persistencia de 100ms.

- Notebook y pipeline: [`reclutamiento-daq/telemetry-analysis/`](reclutamiento-daq/telemetry-analysis/)
- Dashboard web: [`reto2/`](reto2/) — en vivo en `https://motorsports.gaxcor.space/reto2/`

## Convención de commits

Este repositorio sigue [Conventional Commits](https://www.conventionalcommits.org/).
