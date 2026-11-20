# The Chronos Curse

Videojuego de horror y supervivencia para un jugador, ambientado en un cementerio. De día el jugador cuida el cementerio; de noche sobrevive en él. Lo que se hace de día cambia lo que ocurre de noche.

Este repositorio contiene el proyecto de Unreal Engine del MVP (build 0.1.3), desarrollado como proyecto de título de Ingeniería en Informática, INACAP La Serena, 2026.

## Estado

- Versión: 0.1.3 (`ProjectVersion` en `Config/DefaultGame.ini`).
- Plataforma: Windows de 64 bits.
- Motor: Unreal Engine 5.8.
- El juego se ejecuta de forma local: no usa servidores, cuentas ni servicios remotos.

## Qué incluye el MVP

- Ciclo de día y noche de 10 minutos reales, con un máximo de 7 días.
- 4 tareas por día: cremar un cuerpo (fija) y 3 al azar.
- Reputación, que sube o baja según las tareas, y maldición oculta.
- Amenazas nocturnas: cuervos, Whisper, Hollow Walker y Vigilantes.
- Una decisión narrativa con tres opciones y tres finales.
- Guardado manual desde la pausa, hasta 5 partidas, con archivo firmado.

## Requisitos para abrir el proyecto

- Unreal Engine 5.8.
- Git y Git LFS (los archivos `.uasset` y `.umap` se guardan con LFS).
- Visual Studio 2022 con la carga de trabajo «Desarrollo de juegos con C++», solo si Unreal pide recompilar los plugins.

## Cómo abrirlo

1. Instalar Git LFS y ejecutar `git lfs install`.
2. Clonar el repositorio, rama `main`. Con LFS instalado, los recursos se descargan completos.
3. Abrir `chronosYuko.uproject` con Unreal Engine 5.8.
4. Si Unreal avisa que faltan módulos de los plugins o que deben recompilarse, aceptar.
5. El editor abre el mapa `CHRONOSALPHA`. Para jugar desde el menú, abrir el mapa `MainMenu`.

## Estructura

| Ruta | Contenido |
|---|---|
| `chronosYuko.uproject` | Archivo del proyecto |
| `Config/` | Configuración del proyecto (versión, mapas de arranque, gráficos) |
| `Content/` | Blueprints, niveles y recursos del juego |
| `Content/CHRONOSALPHA.umap` | Mapa jugable |
| `Content/MainMenu.umap` | Menú principal |
| `Content/Global/` | Clases centrales del juego y estado de la partida |
| `Content/Guardado/` | Guardado y carga de partidas |
| `Content/Interactivo/` | Objetos con los que el jugador interactúa y tareas |
| `Content/NPC/` | Personajes no jugadores y su comportamiento |
| `Content/Pruebas/` | Mapa y Blueprints de las pruebas automatizadas |
| `Plugins/ChronosGuardado/` | Plugin propio en C++: firma y comprueba el archivo de guardado |
| `Plugins/Marketplace/AdaptivePathfindingSystem/` | Plugin de terceros (navegación de los enemigos) |

El juego está hecho en Blueprints. No hay carpeta `Source`: el único código C++ está en los dos plugins.

## Pruebas automatizadas

Hay 11 pruebas funcionales en `Content/Pruebas/Funcionales/` (mapa `FTEST_Chronos`).

1. Abrir el proyecto en el editor.
2. Ir a Tools, Test Automation.
3. Filtrar por `FTEST_Chronos` y ejecutar.

## Empaquetar una build

Se empaqueta para Win64 con los mapas `CHRONOSALPHA` y `MainMenu`. Antes de empaquetar hay que revisar que las opciones de prueba de `GS_Chronos` (`saltarDespertar`, `modoPruebaCiclo`, `lluviaInfinita`) estén apagadas y que el número de versión de `Config/DefaultGame.ini` coincida con el que muestra el juego en pantalla.

Ejemplo, ajustando las rutas a cada equipo:

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="<carpeta>\chronosYuko.uproject" -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -iostore -compressed -package -archive -archivedirectory="<carpeta de salida>" -prereqs
```

## Controles básicos

| Tecla | Acción |
|---|---|
| W, A, S, D | Moverse |
| E | Interactuar |
| F | Linterna |
| G | Lista de tareas |
| P | Pausa (desde ahí se guarda) |

## Defectos y seguimiento

Los defectos se registran en GitHub Issues de este repositorio, con la versión de la build, los pasos para reproducirlo y el resultado esperado.

## Recursos de terceros

El proyecto usa recursos y plugins de terceros obtenidos de Fab y de otras fuentes, con su origen y licencia registrados. Los que exigen atribución están en la pantalla de Créditos del juego.

## Documentación

La documentación técnica completa (arquitectura, guardado, configuración gráfica, mantención), el manual de usuario y el plan de pruebas forman parte de los anexos del informe del proyecto de título.
