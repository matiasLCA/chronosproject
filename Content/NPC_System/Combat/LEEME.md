# Integración del combate (commit `4b374fbe`) — guía de comprobación para Yuko

Escrito el 2026-11-07 desde la copia de Matías (instancia A, rama `main`). Aquí se integró tu sistema de combate sin tener el plugin **Adaptive Pathfinding System (APS)**. Todo lo que no depende de APS quedó compilado, guardado y probado en PIE; lo que depende de APS (los dos controllers y el AnimBP del enemigo) **no se tocó**: en disco son byte a byte los de tu commit. Con el plugin instalado en tu máquina debería cargar todo. Esta guía dice qué revisar y qué resultado esperar.

## 1. Qué cambió aquí (y qué no)

| Asset | Estado | Qué se hizo |
|---|---|---|
| `NPC_System/Combat/Health/AC_Health` | modificado | Regeneración fuera de combate según tu resumen: `EnableOutOfCombatRegen` (false; true solo en el componente de John), `RegenDelay` 8, `RegenPerSecond` 5, `LastCombatTime`, `MarkCombatActivity()`, `RegenerateHealth()` (timer 0,5 s); `TakeDamage` llama `MarkCombatActivity`; `HandleDeath` limpia el timer. Categoría "Regen". |
| `NPC_System/Combat/Components/AC_MeleeCombat` | solo recompilado | Sin cambios de lógica. Compila al existir `MarkCombatActivity`. |
| `Characters/per_bp` | modificado | Componentes `Health` y `MeleeCombat`, barra de vida, daño, inputs y **cápsula bloqueando Visibility** (ver 3.4). |
| `Characters/animation/abp_per` | modificado | Nodo Slot `DefaultSlot` antes del Output Pose (sin él los `AM_John_*` no se veían). |
| `NPC/Visitors/BP_Visitor_Grave` | modificado (**reconstrucción inferida**) | Tu versión no vino en el commit. Se le añadió lo que `BP_CemeteryDirector` pedía (ver 2.2). Si tienes la tuya, quédate con la tuya. |
| `NPC/Visitors/LivingCemetery/BP_CemeteryDirector` | solo recompilado | Compila con el visitante reconstruido. |
| `BP_EnemyBase`, `WBP_CombatHealthBar`, `GS_Chronos` | solo reguardados | Sin cambios de lógica. |
| `NPC/Enemies/AI/BP_EnemyAIController` | **sin tocar** | Padre `APS_AIController`: aquí no carga. |
| `NPC/Visitors/LivingCemetery/BP_CemeteryVisitorController` | **sin tocar** | Ídem. |
| `NPC/Enemies/Animation/ABP_EnemyCombat` | **sin tocar** | No compila aquí por el cast al controller; en tu máquina debería compilar solo. |
| `NPC/Enemies/BP_Enemy_CultGuardian` | **sin tocar** | Conserva `AIControllerClass = BP_EnemyAIController` (aquí se ve None porque la clase no carga). |

El `.uproject` no se commitea en este equipo (`skip-worktree`): si el tuyo tiene APS habilitado, conserva el tuyo.

## 2. Antes de hacer pull

### 2.1 Cierra el editor y resuelve los binarios

Los `.uasset` van por Git LFS y no se pueden mezclar: por cada archivo gana una versión entera. Si tienes cambios sin commitear en estos assets, decide antes del pull (o haz commit en una rama aparte para no perderlos):

| Asset | Qué versión conviene | Por qué |
|---|---|---|
| `per_bp` | **la de este push** | Tiene todos los sistemas de Matías (escoba, flores, basura, incienso, cámara…) más la integración del combate de tu documento. Tu `per_bp` no los tiene. |
| `AC_Health` | la de este push (o la tuya, son equivalentes) | Sigue tu resumen punto por punto. Si usas la tuya, comprueba que `MarkCombatActivity` no tenga parámetros (así la llama `AC_MeleeCombat`). |
| `BP_Visitor_Grave` | **la tuya** | La de aquí es una reconstrucción a partir del Director. |
| `BP_CemeteryDirector` | cualquiera | Aquí solo se recompiló. |
| `abp_per` | la de este push | Trae el Slot. Si usas la tuya, añade un nodo Slot `DefaultSlot` antes del Output Pose. |

### 2.2 Si te quedas con tu `BP_Visitor_Grave`

Lo que el Director espera de él (es lo que se reconstruyó aquí; tu versión real seguramente ya lo tiene): funciones `ApplyVisitorProfile()`, `RefreshScheduledVisit(Day:int, Progress:float)`, `EndAmbientConversation()`; variables `VisitorProfile`, `RelationToDeceased`, `bPhysicalSimulation`, `RoutineSlot`, `RoutinePhase`, `LastCompletedDay`, `CompletedVisits`, `GreetingCount`, `LastRoutineDay`, `NextMoveRetryTime`, `LogicalLocation`, `bAmbientTalking`, `LastAmbientDay`, `AmbientCount`, `AmbientRole`, `AmbientStartTime`, `AmbientStartProgress`, `AmbientPartner`; componente TextRender `AmbientSubtitle`.

## 3. Al abrir el proyecto

### 3.0 Estado del plugin aquí (actualizado tras instalarlo)

En la copia de Matías APS ya está instalado en `Plugins\Marketplace\AdaptivePathfindingSystem\` (fuente + icono + `Binaries\Win64\UnrealEditor-AdaptivePathfindingSystem.dll` y `UnrealEditor.modules` con `BuildId 55116800`, los que pasaste). Con él cargado: los dos controllers tienen padre `APS_AIController`, `ABP_EnemyCombat`, `BP_EnemyAIController` y `BP_CemeteryVisitorController` compilan, y el Vigilante detecta, persigue, se reposiciona, ataca y **acierta** a John (probado en PIE). Para que eso pasara hubo que arreglar tres cosas que te conviene revisar en tu copia:

1. `BP_Enemy_CultGuardian` → Class Defaults → **AI Controller Class = `BP_EnemyAIController`**, y la instancia colocada en el mapa sin override a None (aquí estaba en None en los dos sitios y el Vigilante no se movía).
2. **Hace falta navegación**: `APS_NavigationData` hereda de `RecastNavMesh`, así que sin un `NavMeshBoundsVolume` `MoveToActor` devuelve Failed al instante (`LastPathFailed` true, estado Chase sin moverse). Aquí hay un volumen de prueba de 70 × 50 m sobre la zona de los enemigos y el `RecastNavMesh` en **Runtime Generation = Dynamic** (con Static, en este mapa de World Partition, PIE arrancaba sin navmesh: "Unable to find RecastNavMesh instance"). No hay agente APS configurado en Project Settings → Navigation System (`SupportedAgents` vacío): la navmesh es la Recast estándar y tu `APSRouteFollower` (un `PathFollowingComponent` normal) la sigue bien. Si quieres rutas APS de verdad, pásanos tu `Config/DefaultEngine.ini` (sección `[/Script/NavigationSystem.NavigationSystemV1]`) o dinos radio/altura del agente y la clase de NavData.
3. La cápsula de John en Custom con Visibility Block (punto 3.4).

### 3.1 El plugin carga

- Edit → Plugins → "Adaptive Pathfinding System" habilitado.
- Output Log sin la línea `Failed to load Class ... APS_AIController ... as Parent`. Si aparece, el plugin no está activo para este proyecto y nada de lo de abajo va a funcionar.
- Al dar Play **no** debe salir el diálogo "Blueprint Asset Compilation Error" con `ABP_EnemyCombat`. Si sale, el controller no cargó (punto anterior).

### 3.2 Compilan sin errores

Abrir y compilar (o Compile All en File → "Compile All Blueprints" filtrando por estas carpetas):
`AC_Health`, `AC_MeleeCombat`, `per_bp`, `abp_per`, `BP_EnemyBase`, `BP_Enemy_CultGuardian`, `BP_EnemyAIController`, `ABP_EnemyCombat`, `BP_CemeteryDirector`, `BP_Visitor_Grave`, `BP_CemeteryVisitorController`, `WBP_CombatHealthBar`.

Si `BP_EnemyAIController` o `ABP_EnemyCombat` muestran nodos con "In use pin no longer exists", basta con recompilarlos una vez (es un refresco de nodos tras cargar la clase).

### 3.3 Class Defaults del Vigilante

`BP_Enemy_CultGuardian`: **AI Controller Class = `BP_EnemyAIController`**, Auto Possess AI = *Placed in World or Spawned*. Aquí se veía None solo porque la clase no cargaba; con el plugin debe volver a aparecer sola. Si está en None, ponla a mano.

### 3.4 `per_bp` (John)

- Componentes: `Health` (AC_Health: Max Health 100, Start At Max Health, **Enable Out Of Combat Regen = true**) y `MeleeCombat` (AC_MeleeCombat: Damage Per Hit 25, Max Reach 160, Attack Cooldown 1,15, Kick ×1,4 / 185 / 2,5 s, Block 0,7 / 65°, Speed Bonus 150 → 600 +0,5, montages `AM_John_BasicPunch`, `LeftPunch`, `Hook`, `Kick`, `Guard`, `BlockReaction`).
- **CollisionCylinder → Collision Presets = Custom, con Visibility = Block y Camera = Ignore** (el resto en Block, igual que el perfil Pawn). Es lo único "raro" de la integración: la cápsula venía con el perfil Pawn (Visibility Ignore) y tu trazo esférico de `UpdateHitWindow` usa el canal Visibility, así que el enemigo atravesaba a John y nunca le pegaba. OJO: cambiar solo la respuesta dejando el perfil en "Pawn" no sirve (el motor vuelve a leer el perfil al crear la instancia); tiene que quedar en Custom. Tu `BP_EnemyBase` ya tiene la cápsula en Custom con todo en Block, por eso a los enemigos sí se les pega. Comprobado en PIE con el trazo dibujado: con Pawn las esferas atraviesan a John sin impacto; con Custom el Vigilante acierta todos los golpes (4 de 4).
- **Caminar al golpear**: `factorVelocidad` multiplica por `factorGolpe()`: mientras `GetGameTimeInSeconds < MeleeCombat.NextAttackTime` (montage + recuperación de puñetazo o patada) limita la velocidad a `velocidadAlGolpear` (Class Defaults, Combate, 600): corriendo a 1000 se baja a 600; caminando no cambia. Probado en PIE (factor 0,6 a los 0,3 s del golpe, 1 al terminar).
- Inputs (eventos de tecla del Blueprint, no Enhanced Input): clic izquierdo → `atacar` (`TryAttack`; si John lleva la escoba, el clic barre y no ataca), clic derecho mantenido → `empezarBloqueo` / `terminarBloqueo` (`StartBlock` / `EndBlock`), R → `patear` (`TryKick`).
- Eventos: `EventAnyDamage` → `recibirDanoCombate` (ResolveIncomingDamage → Health.TakeDamage); `OnHealthChanged(Health)` → `UpdatePlayerHealthDisplay`; `OnDeath(Health)` → `morirJugador` → `GS_Chronos.perder()` (pantalla "HAS PERDIDO", el texto habla de reputación: pendiente de parametrizar). `iniciarCombate` (InitializeCombatReferences + InitializeHealthHUD) va al final del BeginPlay. HUD: `WBP_CombatHealthBar` 260×12, anclado abajo a la izquierda, posición (32, −42).

### 3.5 `abp_per`

AnimGraph: … → ComponentToLocal → **Slot 'DefaultSlot'** → Output Pose. Tus montages de John usan `DefaultSlot` y el esqueleto `metahuman_base_skel`, el mismo del mesh de `per_bp`.

### 3.6 Barra de vida del enemigo solo de cerca

`BP_EnemyBase.actualizarVisibilidadBarra` (llamada en `InitializeEnemy` y por timer cada 0,2 s) muestra `HealthBar` solo si el pawn del jugador está a menos de `distanciaBarraVida` (Class Defaults, Combate|Configuracion, 600 uu). **El valor también está puesto a 600 en el CDO de `BP_Enemy_CultGuardian`** porque la clase hija no lo heredó sola; si lo cambias, cámbialo en los dos. Probado en PIE: oculta a 6,5 m, visible a 5,5 m.

## 4. Pruebas en PIE

Los dos enemigos están en `CHRONOSALPHA` en (29978, −26510) ("Enemigo", `BP_EnemyBase`) y (29957, −26428) (Vigilante). Con "jugar desde aquí" cerca de ellos. Comandos de consola (tilde `~` en PIE); `<john>` = `/Game/UEDPIE_0_CHRONOSALPHA.CHRONOSALPHA:PersistentLevel.per_bp_C_0`, `<enemigo>` = la ruta del actor en PIE (misma raíz + nombre del actor).

### 4.1 Lo que aquí ya se probó y debe repetirse igual en tu máquina

| Prueba | Cómo | Resultado esperado |
|---|---|---|
| Barra de John | al empezar | barra verde abajo a la izquierda |
| Daño y regeneración | `ke <john> recibirDanoCombate 20` | 100 → 80; sin cambio hasta los 8 s; luego sube 5 por segundo hasta 100 (87,5 a los 9 s, 100 a los 13 s) |
| Puñetazo | a < 1 m del enemigo, mirándolo, clic izquierdo (o `ke <john> atacar`) | enemigo −25, su barra baja |
| Patada | R (o `ke <john> patear`) | enemigo −35 |
| Muerte del enemigo | tres o cuatro golpes más | barra a 0, `bDefeated` true, deja de moverse, sin colisión |
| El enemigo pega a John | `ke <enemigo> RequestAttack <john>` con John en la trayectoria del brazo | John −15 |
| Bloqueo de frente | clic derecho mantenido (o `ke <john> empezarBloqueo`) y que el enemigo pegue de frente | John −4,5; `MeleeCombat.LastHitBlocked` true |
| Bloqueo de espaldas | igual, de espaldas | John −15 |
| Evento de daño sin trazo | `ke <john> ReceiveAnyDamage 15 None None <enemigo>` | John −15 (sirve para separar "no llega el trazo" de "no funciona el daño") |
| Muerte de John | `ke <john> recibirDanoCombate 500` | "HAS PERDIDO" y pausa |

### 4.2 Lo que solo se puede probar con APS (en tu máquina)

- El Vigilante detecta a John al acercarse, pasa a persecución y sigue una ruta válida (APS).
- Gira hacia John cuando John lo rodea o se pone detrás (`UpdateCombatFacing`), y ataca solo (`ChooseCombatAction`), sin `RequestAttack` por consola.
- Causa daño real a John con el ataque autónomo; John contraataca; al perder el objetivo investiga, busca, vuelve al puesto y baja la postura de combate (`bCombatStance` en `ABP_EnemyCombat`).
- Al morir, su IA se detiene del todo (estado `Defeated`).
- No regenera vida mientras persigue (`MarkTargetEngaged`).
- Bonus de daño por velocidad: golpe quieto vs golpe avanzando hacia el objetivo (John: 150 → 600 cm/s, +50 % máximo). Aquí no se midió.

### 4.3 Dato de las pruebas de aquí que te puede interesar

Sin controller, el golpe del Enemigo salía hacia su lado izquierdo (+Y a unos 85 cm) y no de frente: el "Enemigo" le pegaba al Vigilante que tenía al lado y no a John puesto delante a 90 cm. Con tu controller girando hacia el objetivo seguramente no se nota, pero si ves que el enemigo "falla de frente", revisa el socket/ventana de impacto del puñetazo derecho. La patada de John desde atrás a 90 cm tampoco conectó; de frente sí.

## 5. Pendientes que quedaron anotados

- John no tiene reacción de golpe ni montage de derrota (existen `AM_John_HitReaction` y `AM_John_Defeat` sin usar).
- El texto de `WBP_Derrota` ("Tu reputacion llego a 0") sale también al morir en combate.
- Los visitantes del cementerio siguen ocultos y sin moverse (`bPhysicalSimulation` false); su controller depende de APS.
- Si el equipo decide no usar APS, la alternativa documentada es reparentar los dos controllers a `AIController`, quitar `APSRouteFollower` / `ConfigureAPSFollowing` y poner un `NavMeshBoundsVolume` (hoy el mapa no tiene NavMesh).

## 6. Qué devolver

Una lista con cada punto de 3 y 4 marcado como OK / falla, y para lo que falle el mensaje del Output Log. Si cambias algo en `per_bp`, `AC_Health` o `abp_per`, avisa antes de commitear: son binarios y la copia de Matías también los tiene modificados.
