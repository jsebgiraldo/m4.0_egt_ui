# M4 EGT UI — Wireframe & Flujo actual

> Reporte generado capturando pantalla por pantalla del simulador (build
> `develop @ 57f72a7`, 800×480). Documenta **cómo se ve hoy cada screen** y
> **cómo se conectan** en el flujo de la app.
>
> - Vista general de todas las pantallas: [`wireframe-all.png`](wireframe-all.png)
> - PNGs individuales full-res: [`screens/`](screens/)
> - Thumbnails etiquetados: [`thumbs/`](thumbs/)
>
> Nota: la fuente que se ve es DejaVu (fallback) — la fuente de diseño
> Gothic A1 está pendiente de instalar a nivel Linux (duda D1). En el
> dispositivo final el texto será un poco más angosto.

---

## Mapa de flujo

```mermaid
flowchart TD
    subgraph BOOT["🔌 Arranque + Conexión"]
        INIT["01 · WiFi Init<br/>(escaneo automático)"]
        CONNECTING["02 · WiFi Connecting"]
        CONNECTED["03 · WiFi Connected ✓"]
        LIST["04 · WiFi Settings<br/>(lista de redes)"]
        NOTFOUND["08 · WiFi Not Found"]
        UNAVAIL["05 · WiFi Unavailable"]
        OVRINFO["06 · Override Info (popup)"]
        OVRINTRO["07 · Override Intro"]
        PWD["11 · Enter Password"]
    end

    subgraph AUTH["🔑 Setup + Login"]
        SETUP["09 · Setup (logo + Settings)"]
        LOGIN["10 · Technician Login<br/>(rueda)"]
        HOME["12 · HOME<br/>(Start / Demo / Settings)"]
    end

    subgraph MAIN["🏠 Menú principal"]
        SETTINGS["13 · Settings<br/>(brillo / internet / cuenta)"]
        DEMO["14 · Demo Info<br/>(TRAINING ONLY)"]
    end

    subgraph PATIENT["🧑 Client Information (wizard 3 pasos)"]
        GENDER["15 · Gender"]
        AGE["15b · Age"]
        ZIP["15c · ZIP Code"]
    end

    subgraph TREAT["🔥 Tratamiento (por ciclos)"]
        WARM["17 · Warming"]
        READY["18 · Ready (100%)"]
        POS["19 · Position Tip"]
        REPOS["20 · Reposition Tip"]
        ACTIVE["21 · Active (countdown)"]
        NEARLY["22 · Nearly finished"]
        ZERO["23 · Cycle Zero (00:00)"]
        PAUSED["24 · Paused"]
        ENDCONF["25 · End Confirm"]
        DONE["26 · Completed"]
        ENDED["27 · Ended (early)"]
    end

    subgraph ALERTS["⚠️ Alertas durante tratamiento"]
        ETEMP["28 · Error: temp"]
        EFILT["29 · Error: filtro"]
        WTEMP["30 · Warning: temp"]
        WAIR["31 · Warning: airflow"]
        FAULT["32 · Fault crítico"]
    end

    INIT -->|conecta| CONNECTED
    INIT -->|falla| LIST
    INIT -->|sin redes| NOTFOUND
    INIT -->|Skip| LOGIN
    CONNECTING --> CONNECTED
    CONNECTED -->|Continue| LOGIN
    LIST -->|elige red| PWD
    PWD -->|red OK| CONNECTED
    LIST -->|Operate w/o WiFi| UNAVAIL
    LIST -->|gear / Setting| SETTINGS
    NOTFOUND -->|Operate w/o WiFi| UNAVAIL
    NOTFOUND -->|Retry| LIST
    NOTFOUND -->|Setting| SETTINGS
    UNAVAIL -->|info ⓘ| OVRINFO
    UNAVAIL -->|Continue| OVRINTRO
    OVRINTRO -->|Enter Override| PWD
    PWD -->|"9999"| LOGIN

    LIST -->|Back boot| SETUP
    SETUP -->|Settings| SETTINGS
    LOGIN -->|tech + pass OK| HOME
    LOGIN -->|Guest| HOME
    LOGIN -->|Back| SETUP

    HOME -->|Begin Treatment| GENDER
    HOME -->|Demo Mode| DEMO
    HOME -->|Settings| SETTINGS
    DEMO -->|Continue| GENDER
    SETTINGS -->|WiFi| LIST
    SETTINGS -->|Technician Login| LOGIN

    GENDER --> AGE --> ZIP -->|complete| WARM
    WARM --> READY --> POS --> ACTIVE
    ACTIVE -->|últimos seg| NEARLY --> ZERO
    ZERO -->|otro ciclo| REPOS --> ACTIVE
    ZERO -->|fin| DONE
    ACTIVE -->|Pause| PAUSED -->|Resume| POS
    ACTIVE -->|End| ENDCONF -->|confirm| ENDED
    DONE -->|Back to Home| HOME
    ENDED -->|auto 20s / tap| HOME

    ACTIVE -.alerta.-> ETEMP
    ACTIVE -.alerta.-> EFILT
    ACTIVE -.alerta.-> WTEMP
    ACTIVE -.alerta.-> WAIR
    ACTIVE -.fallo.-> FAULT
```

---

## Galería — todas las pantallas

![Todas las pantallas](wireframe-all.png)

---

## 1 · Arranque + Conexión WiFi

| | Pantalla | Qué muestra / qué hace |
|---|---|---|
| <img src="thumbs/01-wifi-init.png" width="260"> | **WiFi Init** | Logo centrado + spinner. Escaneo automático al bootear. Sale a: Connected (éxito), WiFi Settings (falla), Login (skip). |
| <img src="thumbs/02-wifi-connecting.png" width="260"> | **WiFi Connecting** | Anillo de progreso animado mientras intenta unirse a una red elegida. |
| <img src="thumbs/03-wifi-connected.png" width="260"> | **WiFi Connected** | Check verde + "Wi-Fi Connected". Botón **Continue** → Technician Login. |
| <img src="thumbs/04-wifi-settings.png" width="260"> | **WiFi Settings** | Lista de redes (señal + chevron). Tap red → password. Gear → Settings. Icono wifi-off → operar sin WiFi. |
| <img src="thumbs/08-wifi-not-found.png" width="260"> | **WiFi Not Found** | Banner naranja. 3 tarjetas: Operate without WiFi / Retry WiFi / Setting. |
| <img src="thumbs/05-wifi-unavailable.png" width="260"> | **WiFi Unavailable** | "Connection remains unavailable — N días". Continue → Override Intro. ⓘ → popup. |
| <img src="thumbs/06-wifi-override-info.png" width="260"> | **Override Info (popup)** | Overlay oscuro explicando el override (N días, contactar Larada Sciences). X cierra. |
| <img src="thumbs/07-wifi-override-intro.png" width="260"> | **Override Intro** | Botón grande **Enter Override password** → keypad. Fila Back/Retry/Setting. |
| <img src="thumbs/11-password.png" width="260"> | **Enter Password** | Teclado completo. Usado para password de red, override ("9999") y login de técnico. |

## 2 · Setup + Login

| | Pantalla | Qué muestra / qué hace |
|---|---|---|
| <img src="thumbs/09-setup.png" width="260"> | **Setup** | Landing con logo + afordancia **Settings** abajo. Punto de retorno del boot (no salta el login). |
| <img src="thumbs/10-login.png" width="260"> | **Technician Login** | Rueda de técnicos (tap selecciona/abre password). Botón **Guest**. Back → Setup. |
| <img src="thumbs/12-home.png" width="260"> | **HOME** | Logo + botón **Start** (gradiente cyan). Tarjetas **Demo Mode** y **Setting**. |

## 3 · Menú principal

| | Pantalla | Qué muestra / qué hace |
|---|---|---|
| <img src="thumbs/13-settings.png" width="260"> | **Settings** | Brillo (slider), Internet Connection (WiFi/Ethernet), sección Account (Technician Login) + About this device. Scrollable. |
| <img src="thumbs/14-demo-info.png" width="260"> | **Demo Info** | "TRAINING ONLY" + badge DEMO MODE. Continue → Client Info (demo). |

## 4 · Client Information (wizard 3 pasos)

| | Pantalla | Qué muestra / qué hace |
|---|---|---|
| <img src="thumbs/15-patient-gender.png" width="260"> | **Gender** | Tarjetas Male / Female. Back / Skip / Continue. |
| <img src="thumbs/15b-patient-age.png" width="260"> | **Age** | Rueda de rangos etarios (under 5 … 50+). |
| <img src="thumbs/15c-patient-zip.png" width="260"> | **ZIP Code** | Keypad 0-9. Continue se habilita solo con 5 dígitos. Reset / Skip. |
| <img src="thumbs/16-patient-gender-demo.png" width="260"> | **Gender (Demo)** | Misma pantalla, badge DEMO MODE; acentos cyan en vez de verde. |

## 5 · Tratamiento (por ciclos)

| | Pantalla | Qué muestra / qué hace |
|---|---|---|
| <img src="thumbs/17-treatment-warming.png" width="260"> | **Warming** | "70% — Warming up". Barra de progreso de calentamiento. |
| <img src="thumbs/18-treatment-ready.png" width="260"> | **Ready** | "100% — Ready for Treatment". Botón **Begin Treatment**. |
| <img src="thumbs/19-treatment-position.png" width="260"> | **Position Tip** | Countdown `00:05` "Position the Applicator Tip". |
| <img src="thumbs/20-treatment-reposition.png" width="260"> | **Reposition Tip** | Igual a Position pero entre ciclos ("Reposition…"). |
| <img src="thumbs/21-treatment-active.png" width="260"> | **Active** | Countdown grande `00:25`, "Treatment started", dots por ciclo, Pause/End. |
| <img src="thumbs/22-treatment-nearly.png" width="260"> | **Nearly finished** | Glow verde respirando + dots casi llenos. Últimos segundos del ciclo. |
| <img src="thumbs/23-treatment-zero.png" width="260"> | **Cycle Zero** | `00:00`, barra llena verde + check. Fin de ciclo → otro ciclo o Completed. |
| <img src="thumbs/24-treatment-paused.png" width="260"> | **Paused** | "Treatment Paused". Resume / End. Resume vuelve a Position Tip. |
| <img src="thumbs/25-treatment-end-confirm.png" width="260"> | **End Confirm** | "Are you sure you want to end treatment?" Confirmación antes de terminar. |
| <img src="thumbs/26-treatment-completed.png" width="260"> | **Completed** | Check azul + "Treatment Completed". **Back to Home**. |
| <img src="thumbs/27-treatment-ended.png" width="260"> | **Ended (early)** | "Treatment Ended" (fin anticipado). Auto-retorno a Home en ~20s. |

## 6 · Alertas durante tratamiento

| | Pantalla | Qué muestra / qué hace |
|---|---|---|
| <img src="thumbs/28-error-temp.png" width="260"> | **Error: temp** | Modal azul "Operating Conditions out of range". Pause / Resume / End. |
| <img src="thumbs/29-error-filter.png" width="260"> | **Error: filtro** | Modal azul "Change the air filter". Pause / Begin / End. |
| <img src="thumbs/30-warning-temp.png" width="260"> | **Warning: temp** | Modal naranja "Temperature out of range". Sin botones (informativo). |
| <img src="thumbs/31-warning-airflow.png" width="260"> | **Warning: airflow** | Modal naranja "Airflow obstruction detected". |
| <img src="thumbs/32-fault-critical.png" width="260"> | **Fault crítico** | Modal rojo "Operating temperature out of safe range" + Error Code. Shutdown. |

---

## Notas

- **Fuente**: todo el texto se renderiza con DejaVu (fallback). La fuente de
  diseño **Gothic A1** está pendiente de instalar a nivel Linux (duda D1);
  con ella el texto será más angosto y fiel a Figma.
- **Estado de paridad con Figma**: las pantallas pasaron por el sweep de
  parity (ver [`../figma-parity/`](../figma-parity/)) — coinciden con el
  diseño salvo la fuente y las decisiones de producto abiertas (D2-D7).
- **Cómo reproducir una pantalla**: `EGT_START_SCREEN=<nombre> ./build-x86/egt-app`
  (las de tratamiento: `EGT_START_SCREEN=treatment EGT_MOCK_TREATMENT=<estado>`).
