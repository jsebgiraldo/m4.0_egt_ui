# M4 EGT App — ToDo Master

> Lista viva de tareas de UI/UX y bugs. Cada tarea tiene un **qué** (descripción
> sin ambigüedad), un **Figma** (node-id si existe), y un **estado**.
>
> Estados: `[ ]` pendiente · `[~]` parcial · `[x]` hecho · `❓` necesita aclaración

---

## 1. Conexión sin internet (flujo WiFi no disponible)

Flujo progresivo cuando el dispositivo no logra conectarse a WiFi:

- `[x]` **Estado 1 — "Wi-Fi Network not found"**: banner naranja + card de
  override. (Figma 52:2774) — implementado en `screen_wifi_unavailable`.
- `[ ]` **Estado 2 — "WiFi remains unavailable, device will continue to
  operate normally for N days"**: mensaje de operación temporal con
  countdown de **N días**.
  - `[x]` Número N en **backend** (`/etc/m4-egt/override_days`, default 7,
    `ui::get/set_override_days`). La pantalla override-info ya muestra
    "operate normally for N days".
  - `[ ]` Editor en **Settings**: pendiente de un slot de diseño
    (Settings está pixel-matched, no hay diseño para el control).
- `[ ]` **Estado 3 — "WiFi remains unavailable, enter override password"**:
  es la **misma pantalla de override** que ya tenemos; solo hay que
  ajustar el look para que coincida con **Figma 134:1421**.
  - `[~]` El flujo funciona end-to-end con el teclado pulido; el match
    exacto a 134:1421 queda pendiente (API rate-limited).

---

## 2. Home — botón START no funciona

- `[x]` **El botón "START" (Home, Figma 140:852) no conducía a ninguna
  pantalla.** Causa: el Label "Start" se agregaba encima del botón y se
  comía el click. Arreglado — START ahora arranca el flujo real.

---

## 3. Demonstration Mode — pantalla "TRAINING ONLY" faltante

- `[x]` **Pantalla "TRAINING ONLY" cableada al flujo Demo.** La pantalla
  ya existía (`screen_demo_info`) pero Demo Mode la saltaba; ahora va
  Demo Mode → Demonstration Mode/Training Only → patient info. Badge
  DEMO MODE arreglado (se cortaba). Falta confirmar contra Figma 84:608.

---

## 4. Color de botones — accent dependiente del flujo

Regla: **azul = Demo Mode, verde = flujo real (START)**. El accent de los
botones de acción se decide por el flujo de entrada.

- **Flujo Demo Mode** → botones **azules/cyan** (Continue, Go, Begin).
- **Flujo real (START)** → botones **verdes**.

- `[x]` Accent por flujo implementado vía `flow_accent(demo)`: wizard
  Continue + Summary GO + Begin Treatment quedan **verdes en real**,
  **azules en demo**.
- `[~]` **Botones Back y GO del Summary mal posicionados** — deben quedar
  cerrados (forma de botón) y centrados. *Parcial:* el card ya se centró;
  falta revisar los botones inferiores.

---

## 5. Client Information — validación de campos

- `[x]` **Continue se habilita solo con el campo requerido lleno.**
  Implementado vía `make_continue_btn(enabled, ...)` (gris + sin handler
  cuando vacío).
  - Aplica a **Gender** (debe elegir Male/Female) y **ZIP** (debe
    ingresar el código).
  - **Age** queda exento: el wheel-picker siempre tiene un valor por
    defecto, así que es trivial y nunca está "vacío".

---

## 6. Cumulative Treatment Time — header

- `[x]` **Centrado, doble línea, más abajo.** Header ahora es un grupo
  centrado "CUMULATIVE / TREATMENT TIME" + tiempo, bajado de y=15 a y=28.

---

## 7. Finish / Treatment Ended

- `[x]` **Botón del medio que no iba** — arreglado: el checkmark y el
  botón Back-to-Home ya no se solapan (hero ring + título + botón abajo).
- `[~]` **Pantalla "Treatment Ended"** (Figma 81:406): versión interina
  limpia lista (✕ naranja + "Treatment Ended" + Back to Home). Falta
  ajustar al Figma exacto (API rate-limited).

---

## 8. Ready / 100% screen (Figma 168:812)

- `[x]` **El "%" estaba muy lejos del número** — acercado al valor.
- `[x]` **Barra de progreso verde llena (100%)** agregada en Ready.
- `[x]` **Begin Treatment verde y centrado** en el flujo real (azul en demo).

---

## Backlog técnico (sesión de tratamiento)

- `[ ]` Definir en Figma las pantallas de notificación del **límite de
  proceso** (aviso 5 min / 1 min). MVP actual: banner ámbar/rojo con
  countdown — placeholder para revisión con cliente.
- `[ ]` Conectar los callbacks `on_alert_tone` / `on_tip_led_flash` al
  firmware (hoy stubbeados, solo logean).
