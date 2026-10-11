# Fuente de Poder Lineal Regulable con LM317

## 📋 Descripción del Proyecto
Diseño, simulación y documentación de una fuente de alimentación lineal regulable, desde el transformador hasta la etapa de regulación, con enfoque en buenas prácticas de ingeniería, análisis térmico y validación mediante LTspice.

## 🎯 Especificaciones de Diseño
| Parámetro | Valor Especificado | Notas |
| :--- | :--- | :--- |
| **Voltaje de Entrada (Red)** | 120V AC, 60Hz | |
| **Transformador** | 27V AC, 12.5VA (0.5A máx) | Relación de vueltas ~4.44 |
| **Voltaje de Salida ($V_{out}$)** | 1.25V a 20V | Ajustado por límites térmicos del LM317 |
| **Corriente Máxima ($I_{out}$)** | 0.5A (500 mA) | Limitado por la potencia del transformador |
| **Ripple de Salida** | < 5mV rms | Objetivo a validar con filtrado adecuado |

---

## 📌 Fase 1: Modelado y Simulación del Transformador

### 1.1 Modelo en LTspice
Se utilizó un modelo de transformador basado en inductores acoplados para representar el comportamiento real, incluyendo las resistencias de los devanados de cobre.

- **Inductancia Primaria ($L_1$):** 10 H
- **Resistencia del Primario ($R_{L1}$):** 40 Ω
- **Inductancia Secundaria ($L_2$):** 0.506 H
- **Resistencia del Secundario ($R_{L2}$):** 2 Ω
- **Coeficiente de Acoplamiento:** `K1 L1 L2 1` (Acoplamiento ideal)
- **Carga de prueba:** 10 kΩ (para evitar nodos flotantes)

### 1.2 Resultados de Simulación (Vacío)
![Simulación del Transformador](docs/01_simulacion_transformador_vacio.png)

| Medición | Valor Simulado | Valor Teórico Esperado |
| :--- | :--- | :--- |
| Voltaje Pico Primario ($V_p$) | 169.66 V | 169.7 V ($120V \times \sqrt{2}$) |
| Corriente Pico Primario ($I_{L1}$) | 83.1 mA | ~85 mA (Limitada por la reactancia de 10H) |
| Voltaje Pico Secundario ($V_s$) | 37.8 V | 38.18 V ($27V \times \sqrt{2}$) |
| Corriente Pico Secundario ($I_{L2}$)| 3.78 mA | 3.81 mA ($38.18V / 10k\Omega$) |

**Conclusión de la Fase 1:** El modelo del transformador es físicamente coherente. La pequeña caída de 37.8V respecto a los 38.18V teóricos se debe a las pérdidas resistivas de los devanados (40Ω y 2Ω), lo cual valida el modelo para proceder con la etapa de rectificación.

---
*Próxima Fase: Rectificación y Filtrado (En progreso)*