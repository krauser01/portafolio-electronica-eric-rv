# 🎛️ Calibrador y Emulador Portátil de Señales Industriales (ESP32)

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![ESP32](https://img.shields.io/badge/MCU-ESP32-007DC3?logo=espressif)
![C++](https://img.shields.io/badge/Language-C++-00599C?logo=c%2B%2B)
![KiCad](https://img.shields.io/badge/EDA-KiCad-000000?logo=kiCad)
![PlatformIO](https://img.shields.io/badge/Build-PlatformIO-orange?logo=platformio)

## 📌 Resumen Ejecutivo
Diseño y desarrollo de un instrumento de diagnóstico portátil (MVP) para la validación física de lazos de control en el **Complejo Termoeléctrico Planta Centro (CORPOELEC)**. El sistema emula y mide señales analógicas industriales (**4-20 mA** y **0-10 V**), permitiendo a las cuadrillas de instrumentación verificar la integridad del cableado y la respuesta de tarjetas de E/S sin depender de equipos de calibración importados de alto costo o forzamientos por software.

## 📸 Galería del Proyecto
| Prototipo Final | PCB y Ensamble | Pruebas en Campo |
| :---: | :---: | :---: |
| ![Prototipo Final](pics/prototipo_final.jpg) | ![PCB Ensamblada](pics/pcb_ensamblada.jpg) | ![Pruebas en Campo](pics/pruebas_campo.jpg) |
| *Vista frontal con TFT ST7735* | *Layout y soldadura* | *Validación en banco de pruebas* |

> 💡 *Puedes ver el proceso completo de construcción y pruebas en la carpeta [`/pics`](pics/).*

## 🚀 Impacto y Métricas Clave
- 💰 **Reducción de Costos del 96%**: Costo de materiales de **$65.50 USD** frente a los $1,650 USD de un Fluke 715.
- 🎯 **Precisión Industrial**: Error máximo reducido al **0.25% del Span** (Corriente) y **0.60% del FS** (Voltaje) tras calibración algorítmica.
- 📈 **Estabilidad de Señal**: >98% en pruebas continuas, gracias a filtrado analógico (RC 15.9Hz) y digital (Oversampling + Promedio Móvil).

## 🏗️ Arquitectura del Sistema

### Hardware
| Componente | Especificación / Función |
| :--- | :--- |
| **MCU** | ESP32 (Doble núcleo, 240MHz, ADC 12-bit, PWM/LEDC) |
| **Acondicionamiento Analógico** | LM358 (Buffers/Amplificadores), Shunt 150Ω 1% (Metal Film) |
| **Fuentes de Poder** | Li-Ion 3.7V + TP4056, Boost MT3608 (5V), Boost XL6009 (24V para lazo activo) |
| **Interfaz** | TFT 1.8" ST7735 (SPI), Botones táctiles, LEDs de estado |
| **Protección** | Diodos Zener 3.3V en entradas ADC, Filtros RC pasabajos (fc ≈ 15.9 Hz) |

### Firmware y Procesamiento de Señal
- **Lenguaje**: C++ (Entorno PlatformIO / Arduino Framework).
- **Arquitectura**: Máquina de estados finita (FSM) para la interfaz de usuario, tareas no bloqueantes.
- **Metrología por Software**: Algoritmo de calibración de 2 puntos ($Y = mX + b$) con almacenamiento de coeficientes en **NVS** (Non-Volatile Storage).
- **Filtrado Digital**: Oversampling del ADC interno y filtro de promedio móvil para mitigar ruido EMI industrial (conmutación de tiristores, motores).

## 📂 Estructura del Repositorio
```text
├── Docs/           # Manual de usuario, reporte de calibración, BOM y memorias de cálculo
├── Firmware/       # Código fuente C++ (PlatformIO) con drivers y lógica de calibración
├── Pcb/            # Layout KiCad, Gerbers y renders 3D
├── Schematics/     # Esquemáticos KiCad y PDFs exportados
├── pics/           # Galería fotográfica del paso a paso (Protoboard, PCB, Campo)
└── README.md       # Este archivo