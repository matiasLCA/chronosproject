# Pruebas funcionales automatizadas (build 0.1.1)

Nivel `FTEST_Chronos` + una prueba funcional en Blueprint (padre `FunctionalTest`) por requisito. Solo Blueprints, plugin **Functional Testing Editor** del motor.

## Cómo correrlas todas de una vez
1. Abrir el editor (cualquier mapa; no hace falta tener el mapa de prueba abierto, pero no debe haber mapas sin guardar).
2. **Tools → Test Automation** (ventana Session Frontend, pestaña Automation).
3. En el buscador escribir `FTEST_Chronos`, marcar la casilla de `FTEST_Chronos (11)` y pulsar **Start Tests** (▶). Tarda ~90 s.

Por línea de comandos (sin abrir el editor; **sin probar todavía**):
`UnrealEditor-Cmd.exe "<ruta>\chronosYuko.uproject" -ExecCmds="Automation RunTests Project.Functional Tests.Pruebas.Funcionales.FTEST_Chronos;Quit" -unattended -nullrhi -log`

## Qué hay
| Asset | Qué es |
|---|---|
| `FTEST_Chronos` | Piso, PlayerStart, sol (rotación igual que CHRONOSALPHA), `BP_cycle`, puntos de ataúd (2), basura (3) + basurero, suciedad (3) + escoba, mueble de ánforas. GameMode `GM_FTEST`. |
| `GM_FTEST` / `GS_FTEST` | Hijos de `chronos` / `GS_Chronos`. `GS_FTEST` solo cambia `saltarDespertar` = true (sin intro: la cama no existe en este nivel). |
| `FT_RF01_DuracionDiaNoche` | Mide día y noche (300 s cada uno, ±3 s) y que cada transición ocurra una vez. **Acelera el tiempo ×20 solo dentro de la prueba.** |
| `FT_RF01_TopeSieteDias` | Lleva el juego al día 7 (`AdvanceDay`) y comprueba que se juega (4 tareas, sin fin de demo); deja que el día 7 termine solo y comprueba que la demo termina (`GS_Chronos.demoTerminada`, `WBP_FinDemo` en pantalla), que no se cobran ni reparten tareas y que otro cambio de día no lo repite. Al final deshace el fin de demo. |
| `FT_RF02_AsignacionDiaria` | 6 días seguidos (`BP_cycle.AdvanceDay`): 4 tareas, cremar 1 vez, otras 3 ∈ {basura, barrer, ánfora}. |
| `FT_RF02_ReputacionDiaSiguiente` | Valor inicial 100 (Class Defaults de GS_Chronos); 2 hechas y 2 pendientes → 60 al empezar el día siguiente, una sola vez. |
| `FT_RF08_Linterna` | Batería 120 s en uso (±2), queda en 0, no negativa, luz apagada. Tiempo ×20 en la prueba. |
| `FT_RF08_Incienso` | Sin existencias no consume ni protege; con 1 protege 60 s (±2). Tiempo ×20 en la prueba. |
| `FT_RF12_DecisionLago` | Por la ventana real (`WBP_DecisionCuerpo.elegir`): Silencio +25, Investigar 0, Denunciar −25; al volver a interactuar no se repite. |
| `FT_RF14_Guardado` | Guarda en el slot `Partida_99`, cambia valores, carga con `cargarDatos` + `aplicarPartidaCargada`; borra el slot al final. |
| `FT_RF15_Pausa` | Abre la pausa real (`per_bp.abrirPausa`), 3 s reales sin avance de hora, reanuda con `reanudar`, un solo `OnDayChanged`. |
| `FT_RF16_Derrota` | `ApplyDamage` 1000 a John → vida 0 → `juegoPerdido`, `WBP_Derrota`, pausa. |
| `FT_REGLA_AccionesInvalidas` | Basurero sin bolsa, incienso sin existencias, linterna sin tenerla, mueble de ánforas lejos, barrer sin escoba: nada cambia. |

## Cosas a saber
- Las pruebas del mismo nivel corren en **la misma partida de PIE**, en orden alfabético. Cada una guarda y restaura reputación, maldición y día. `FT_RF16_Derrota` deja el juego en derrota: es la última por orden alfabético; si se agrega otra prueba, que su nombre quede antes.
- `TimeLimit` de `FunctionalTest` se cuenta en tiempo de juego (con ×20 corre 20 veces más rápido): por eso RF-01 tiene 1500.
- Cualquier `Error` en el log durante la prueba la hace fallar aunque todas las comprobaciones pasen (así se encontró el error de foco de `WBP_Pausa`).
- Para reconstruirlas: scripts en `Saved/pruebas_funcionales/` (Python del editor; `ftlib.py` es la plantilla por fases).
