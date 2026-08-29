# 🌌 AUDIO-VOYAGER // Autonomous Procedural Gyroid SDF & AGC Audio Engine

**audio-voyager** es una experiencia inmersiva hipnótica autónoma guiada en tiempo real por el audio de tu sistema. El programa no utiliza preajustes estáticos ni temas scripteados: es un viaje cinematográfico continuo a través de geometrías de **Gyroid infinito** y fractales **KIFS** generados procedimentalmente en GPU mediante **Signed Distance Fields (SDF Raymarching)**, un sistema de **Auto-Gain Control (AGC)** dinámico y mapeo de color continuo **HSV**.

---

## ⚡ Arquitectura Matemática (Fase 5 - Versión Definitiva)

### 1. Auto-Gain Control Dinámico (`AutoGainControl` en `SemanticBrain.cpp`)
- **Ventana Deslizante de Calibración (~12s)**: Rastrea en vivo los picos y pisos de ruido para:
  - `Dissonance` (Aspereza armónica de Sethares)
  - `SpectralCentroid` (Centro de masa del espectro)
  - `RMS / Energy` (Masa acústica cinética)
  - `Sub-Bass` y `High-Treble`
- **Rango Completo $0.0 \to 1.0$ Garantizado**: Toda canción o fuente de audio, sin importar su volumen base de masterización, alcanza de forma dinámica y fluida el rango completo $[0.0, 1.0]$ en sus momentos más intensos.

### 2. Espacio Procedural Infinito (Cero Túneles Fijos - `shaders/raymarching.frag`)
- **Topología de Gyroid de Superficie Mínima**:
  $$\text{Gyroid}(p) = |\sin(p.x) \cos(p.y) + \sin(p.y) \cos(p.z) + \sin(p.z) \cos(p.x)| - \text{thickness}$$
- **Plegado Espacial KIFS Guiado por Disonancia**:
  - *Baja Disonancia / Armonía*: Superficies mínimas fluidas, líquidas y elásticas.
  - *Alta Disonancia / Caos*: Plegados fractales iterativos con bordes duros y cristales afilados.
- **Modulación de Escala por Energía / Onsets**:
  El espacio se contrae y dilata rítmicamente con los pulsos de sub-graves y los impactos de percusión (*drops*).

### 3. Teoría del Color Sinestésica Continua (Mapeo HSV $\to$ RGB)
- **Hue (Tono)**: Mapeado al `SpectralCentroid` normalizado.
  - Frecuencias graves $\to$ Rojos, naranjas y oros cálidos.
  - Frecuencias medias $\to$ Verdes esmeralda y cian.
  - Frecuencias agudas $\to$ Azules eléctricos, violetas y magentas.
- **Saturación**: La alta disonancia desatura el entorno hacia cromo blanco y negro de alto contraste.
- **Emisividad / Brillo**: Modulado directamente por la energía RMS y los Onsets para alimentar el Bloom HDR.

---

## 🚀 Cómo Ejecutar (Arranque Zero-Friction)

### Opción 1: Ejecutar el Binario Nativo en Windows

Simplemente haz doble clic en:
```text
audio_voyager.exe
```
O ejecútalo desde tu terminal de **PowerShell**:
```powershell
.\audio_voyager.exe
```
*(El programa iniciará de inmediato en modo cinematográfico autónomo, escuchando lo que suena en tu PC).*

---

### Opción 2: Carpeta Portable para Llevar en una USB

Ubicada en:
```text
dist\audio-voyager-windows\
```
Contenido de la carpeta portable:
- `audio_voyager.exe` (Binario autónomo de 64 bits con enlace estático)
- `run_audio_voyager.bat` (Lanzador con un solo clic)
- `shaders/` (Todos los shaders de Gyroid SDF, Bloom y Post-Procesado)

---

### Opción 3: Modo Generador Sintético (Pruebas de Laboratorio)

```powershell
.\audio_voyager.exe --synthetic
```

---

## 🎮 Controles y Atajos de Teclado

| Tecla | Acción |
| :--- | :--- |
| **`F11`** | Conmutar **Pantalla Completa sin Bordes (Borderless Fullscreen)** |
| **`F12`** | Mostrar / Ocultar el **Panel Secreto de Depuración (Dear ImGui HUD)** con métricas AGC |
| **`ESC`** | Salir limpiamente |

---

## 🔄 Recompilación Portable con Docker

```powershell
docker compose run --rm audio-voyager bash /workspace/scripts/build_windows_exe.sh
```
