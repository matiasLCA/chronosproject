# Animación de despertar (inicio del juego)

Muestra hecha el 2026-11-07 a partir del target `AquiTestDespertar` de `CHRONOSALPHA` (corregida el mismo día). **Conectada al juego el 2026-11-09** como montaje en `per_bp` (ver "Implementación hecha" abajo).

## Qué hay en esta carpeta

| Asset | Qué es |
|---|---|
| `LS_Despertar` | Level Sequence (30 fps, fotogramas 0–300 = 10 s). Tiene `per_bp` como **spawnable** animado con `MetaHuman_ControlRig` (claves a mano), la hoja `BP_Lista` como spawnable (`Papel_Lista`) que pasa a la mano izquierda desde el fotograma 202, y el rig facial sin claves (cara neutra). |
| `AS_Despertar_Cuerpo` | AnimSequence del cuerpo horneado desde `LS_Despertar` (esqueleto `metahuman_base_skel`, 10 s). Está **enlazado** a la secuencia (binding `CharacterMesh0`). |

Para verla: abrir `LS_Despertar` (con `CHRONOSALPHA` cargado) y darle Play. Con **G** en el viewport se ocultan los íconos del editor.

## Dónde ocurre

- Cama `Bed` y escritorio `Desk_D` del dormitorio, junto al target `AquiTestDespertar` (119320, −24960, 22280).
- El actor `per_bp` está fijo en **(119215, −24995, 22281), yaw 180**. Toda la animación es relativa a esa posición.
- **Termina exactamente en la pose default de `per_bp`**: el primer fotograma de `MM_Idle` (el idle de `bs_locmotion` en `abp_per`), cargado en el rig y copiado en los fotogramas 280 y 300. Así el paso a la jugabilidad no tiene salto si el jugador empieza en ese punto.
- La hoja empieza en (119190, −24930, 22274), sobre la esquina del escritorio más cercana a la cama.

## Cronología

| Tiempo | Fotogramas | Acción |
|---|---|---|
| 0–1,8 s | 0–55 | Dormido boca arriba, piernas rectas, cabeza girada; despierta y mira al techo |
| 1,8–3,3 s | 55–100 | Se apoya en las manos y se sienta en la cama |
| 3,3–4,8 s | 100–145 | Gira hacia el lado libre (−X), saca las piernas y queda sentado en el borde |
| 4,8–5,9 s | 145–178 | Sentado, somnoliento, manos en los muslos |
| 5,9–6,7 s | 178–202 | Mira el escritorio y se estira con la izquierda; la derecha se apoya en el colchón |
| 6,7–7,1 s | 202–212 | Toma la hoja y gira la palma hacia arriba: la hoja queda plana sobre la palma, con el pulgar en su esquina inferior izquierda |
| 7,1–7,7 s | 212–232 | La mira |
| 7,7–8,7 s | 232–260 | Empuja con la mano derecha en la rodilla y se pone de pie, con la hoja en la palma |
| 8,7–9,3 s | 260–280 | Da un pasito (pie izquierdo, luego derecho) y llega a la pose idle |
| 9,3–10 s | 280–300 | Pose default de `per_bp`, quieto; la hoja queda vertical en la mano izquierda, junto al muslo |

## Implementación hecha (2026-11-09)

- `AM_Despertar` (en esta carpeta): montaje de `AS_Despertar_Cuerpo`, slot `DefaultSlot` (el nodo Slot de `abp_per` va al final del AnimGraph, así que mientras suena pisa los IK), 10 s. Sin notifies: los tiempos van por timer.
- `per_bp` (Class Defaults, categoría "Despertar"): `iniciarDespertar` se llama al final del BeginPlay salvo `GS_Chronos.saltarDespertar` (categoría Pruebas, para probar otras cosas sin la intro). Pone a John en `posicionDespertar` **(89845, −24995, 22281)** + `subidaMallaDespertar` 20,6 cm (porque la malla de `per_bp` ahora va a Z −100,7 y la animación se horneó con la malla a −80,1), yaw 180 (`yawDespertar`), sin input ni movimiento; crea un `BP_Lista` en `posicionListaDespertar` (89820, −24930, 22274); `PlayAnimMontage(montajeDespertar)` y un timer de 0,02 s (`actualizarDespertar`): a `tiempoAgarreLista` 6,9 s `agarrarListaDespertar` (pega la hoja a `hand_l` con la transformación relativa de la tabla de abajo y la guarda en `per_bp.lista`), entre `tiempoBajadaDespertar` 7,7 y `tiempoFinDespertar` 9,4 s baja la malla hasta su Z normal, y a 9,4 s `terminarDespertar`: si no hay suelo a 4 m debajo lo manda a `posicionRespaldoDespertar` (65200, −27850, 22256, dentro del crematorio), Walking, input, esconde la hoja (queda en `per_bp.lista`, se usa con G) y muestra `UI/WBP_Tutorial` (teclas de a dos, un par por lado, 5 s cada paso con fundidos).
- **La casa se movió** desde que se grabó la secuencia: la cama `Bed` está ahora en (89942, −24976, 22250) y `Desk_D` en (89746, −24906), 29.370 uu más al oeste que cuando se midió (119320). El target `AquiTestDespertar` y el spawnable de `LS_Despertar` siguen en la posición vieja, que hoy cae dentro de la colisión de las montañas (`SM_Iceland_Eroded_Mountain28/6`): John se caía al vacío. `posicionDespertar` y la hoja se desplazaron lo mismo que el escritorio. Probado en PIE en la casa nueva el 2026-11-09: John despierta sobre la cama, se sienta en el borde libre, toma la hoja del escritorio y queda de pie junto a él (capturas `Saved/Screenshots/WindowsEditor/rev_intro_*.png`). Si algún día se vuelve a mover la casa, ajustar `posicionDespertar` (y `posicionListaDespertar`) en Class Defaults de `per_bp`. Para ver `LS_Despertar` en su sitio habría que mover su spawnable y el target a la casa nueva.
- **Espera de carga y cámara (2026-11-09, mañana)**: desde el menú la casa se carga del disco (World Partition), así que `iniciarDespertar` deja la pantalla en negro y no arranca el montaje hasta que `zonaDespertarCargada` ve cargados la cama, el suelo y el escritorio (`puntosCargaDespertar`, relativos a `posicionDespertar`) durante 1 s (`margenCargaDespertar`; máximo 30 s). Después `empezarAnimDespertar` y, 0,4 s más tarde, `aclararDespertar` (fundido de 1,5 s). Durante la intro se ve desde una cámara fija en la esquina del dormitorio (`crearCamaraDespertar`, `offsetCamaraDespertar` / `objetivoCamaraDespertar`); al terminar vuelve a la cámara del jugador en 1,2 s (`soltarCamaraDespertar`). Con `GS_Chronos.saltarDespertar` activo no hay intro y, entrando por el menú, John cae en el origen (no hay PlayerStart).
- El menú principal (`UI/Menu`, botón "Partida nueva") solo abre `CHRONOSALPHA`, así que la intro arranca con el nivel (también con "Jugar de nuevo" de `WBP_Derrota`).

## Receta original (2026-11-07; ya aplicada arriba, las coordenadas son las viejas)

Datos que hacen falta, ya medidos:

| Dato | Valor |
|---|---|
| Posición del jugador al empezar | (119215, −24995, 22281), rotación yaw 180 (pitch 0, roll 0) |
| Animación | `AS_Despertar_Cuerpo`, 10 s, 30 fps, sin root motion |
| Hoja sobre el escritorio | `BP_Lista` en (119190, −24930, 22274), rotación 0 |
| Toca la hoja | 6,73 s (fotograma 202) |
| Hoja ya en la palma | 7,07 s (fotograma 212) |
| Termina en la pose idle | 9,33 s (fotograma 280) |
| Hoja en la mano: socket | `hand_l` del mesh del cuerpo (`CharacterMesh0`) |
| Hoja en la mano: Relative Location | X 20,38 · Y −4,98 · Z −4,26 |
| Hoja en la mano: Relative Rotation | Pitch 10,15 · Yaw 92,43 · Roll −177,62 |

La transformación relativa de la hoja es la del actor `BP_Lista` (su raíz) respecto al control `hand_l` del rig. Ese control coincide con el hueso, pero no se probó en el juego: si la hoja queda girada, ajustar solo esa rotación.

Pasos:
1. Clic derecho en `AS_Despertar_Cuerpo` → *Create → Create AnimMontage* (`AM_Despertar`). Usar el slot `DefaultSlot` o uno nuevo.
2. En `abp_per`, insertar un nodo **Slot** con ese slot justo antes de la salida de la máquina de estados (antes de los IK de linterna, incienso y bolsa).
3. En el montaje, añadir dos Notifies: `agarrarLista` a ~6,9 s y `finDespertar` a ~9,4 s.
4. En `per_bp`, al empezar la partida:
   - colocar el actor en la posición de arriba (`SetActorLocationAndRotation`) y la rotación de control en yaw 180;
   - desactivar el input (`DisableInput`) y hacer `Play Anim Montage (AM_Despertar)`.
5. En `agarrarLista`: `AttachActorToComponent(lista, Mesh, socket "hand_l", Keep Relative)`, luego `SetActorRelativeLocation` y `SetActorRelativeRotation` con los valores de la tabla, y guardar la hoja en `per_bp.lista`.
6. En `finDespertar` (o al terminar el montaje): `EnableInput`. Esconder la hoja o dejarla en la mano, según cómo deba funcionar la lista con la tecla G.

Alternativa: **como cinemática** (sin tocar `abp_per`). Colocar `LS_Despertar` en el nivel (Level Sequence Actor), cambiar el binding `per_bp_C` de spawnable a *possessable/replaceable* (Binding Properties) para que anime al jugador real, bloquear el input mientras dura y devolverlo en `OnFinished`. La hoja ya viene animada en la secuencia.

## Si se edita la animación

- Las poses son claves del `MetaHuman_ControlRig` en `LS_Despertar` (brazos en IK: `arm_l/r_fk_ik_switch` = true; piernas IK; columna, cuello y cabeza FK; dedos FK, que se flexionan con el canal yaw).
- Los controles del lado derecho (mano, pie y polos) deben tener **escala 1**: si quedan con escala X = −1 esa mano o pie se ve invertido ("atravesado").
- Después de editar, volver a hornear el cuerpo: clic derecho en `CharacterMesh0` → **Create Linked Animation Sequence**. El editor crea la animación nueva junto al esqueleto (`assets/MetaHumans/Common/Female/Medium/NormalWeight/Body/metahuman_base_skel_Sequence`) y mueve el enlace a ella: borrar `AS_Despertar_Cuerpo` (cierra la secuencia) y mover la nueva a esta carpeta con ese nombre. Si existe `AM_Despertar`, revisar que siga apuntando a `AS_Despertar_Cuerpo`. No usar *Add AutoBake*: al guardar no re-horneó y agregó una pista vacía "No Animation".
- La hoja no sigue sola a la mano: tiene claves de transformación cada 2 fotogramas calculadas a partir de la mano izquierda. Si se cambian las claves de esa mano después del 202, hay que recalcularlas.

## Pendiente / limitaciones

- La cara no tiene expresión.
- La transición de la hoja del escritorio a la palma (202–212) es rápida y se ve algo forzada.
- Para alcanzar el escritorio se inclina bastante: quedaría más natural con una mesita de luz más cerca.
- Al final la hoja queda en la mano junto al muslo; en el juego se esconde y queda en `per_bp.lista` (hecho el 2026-11-09).
- `LS_Despertar` y `AquiTestDespertar` siguen en la posición vieja de la casa (119320): la secuencia en el editor se ve dentro de la montaña.
