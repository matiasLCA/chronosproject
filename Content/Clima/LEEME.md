# Clima (niebla, estrellas, lluvia)

Todo lo crea `BP_GestorClima` (lo spawnea `GS_Chronos` al empezar; `GS_Chronos.gestorClima`).
Los valores ajustables están en sus Class Defaults, categoría "Configuracion":

| Variable | Valor | Qué hace |
|---|---|---|
| `probabilidadNiebla` | 0,15 | Probabilidad (al empezar el juego y al terminar cada noche) de que la próxima noche haya niebla |
| `densidadNiebla` | 250 | Densidad del `ExponentialHeightFog` con niebla (≈ no se ve más allá de 5 m). 60 ≈ 25 m |
| `velocidadNiebla` | 0,12 | Cuánto tarda en entrar/salir (≈ 20–30 s) |
| `colorNiebla` | gris muy oscuro | Color de la niebla de noche |
| `nieblaVolumetrica` | false | Activa la niebla volumétrica solo mientras hay niebla (más cara, se ve el haz de la linterna) |
| `intensidadEstrellas` | 0,4 | Brillo de las estrellas. Solo de noche, menos con niebla o lluvia (0,4 con el tope de exposición equivale a lo que antes era 0,05) |
| `intensidadLuzNoche` | 0,0175 | Luz mínima de noche (lux): apenas siluetas. 0,05 se veía bastante, 0,1 claramente más claro |
| `densidadNoche` | 25 | Niebla de todas las noches, para que lo lejano se apague. 12 deja ver el muro a 60 m; 50 apenas más que 25 |
| `inicioNieblaNoche` | 4000 | Distancia (uu) a la que empieza esa niebla de noche: 40 m. Antes de eso se ve normal |
| `probabilidadLluvia` | 0,1 | Probabilidad de lluvia en cada transición día↔noche (y al empezar) |
| `duracionLluviaMin/Max` | 120 / 300 s | Cuánto dura la lluvia |
| `velocidadLluvia` | 0,15 | Entrada/salida de la lluvia |
| `brilloLluviaNoche` | 0,03 | Brillo de las gotas de noche (de día 1) |

Luz nocturna: el componente `luzNoche` (luz direccional tenue, sin luna ni sombras) se enciende con la noche, y el componente `exposicionNoche` limita la exposición automática a 0 EV100 (Settings → Exposure → Min EV100). Sin ese tope la cámara compensa tanto que 0,05 lux se ve como de día; si se cambia el tope hay que recalibrar `intensidadLuzNoche` e `intensidadEstrellas`.

Modo de prueba: `GS_Chronos` → Class Defaults → "Pruebas" → `lluviaInfinita` (llueve siempre). Desactivarlo antes de entregar.

Por consola en PIE: `ke /Game/UEDPIE_0_CHRONOSALPHA.CHRONOSALPHA:PersistentLevel.BP_GestorClima_C_0 forzarNiebla true`,
`… forzarLluvia true`, `… detenerLluvia`, `… ponerSol 20` (noche), `… ponerDensidadNiebla 60`, `… ponerLuzNoche 0.05`, `… ponerEstrellas 0.4`, `… ponerDensidadNoche 25`, `… ponerInicioNoche 4000`.

Assets: `M_Estrellas` (material aditivo procedural, sin textura) sobre `BP_CupulaEstrellas` (SM_SkySphere ×30);
`NS_Lluvia` (Niagara, parámetros de usuario `Intensidad` y `Brillo`) dentro de `BP_Lluvia`, que sigue al jugador.
