# Escoba y suciedad

- **E** sobre la escoba: la recoge (queda en las dos manos). **V**: la deja tirada al lado. **Clic izquierdo (mantener)**: barre
  (la escoba va de lado a lado y el jugador camina a 30 % de la velocidad; `per_bp.reduccionBarrido` = 0,7).
- La basura (`BP_Suciedad`) aparece en los `BP_PuntoSuciedad` del mapa (carpeta "Basura" del outliner) con `probabilidadAparicion` (0,25)
  cada `tiempoRenovacion` (300 s) del `BP_GestorSuciedad`, y en grupos de 4 cuando sale la tarea "Barrer la basura del cementerio".
- Cerca de una basura aparece "Está sucio: hay que barrer". Hay que barrer 5 s cada una (`duracionLimpieza`): se va encogiendo cada segundo.
- Para agregar sitios: arrastrar `BP_PuntoSuciedad` al mapa (la malla de vista no se ve en el juego).
- Ajustes de la pose y el barrido: Class Defaults de `BP_Escoba`, categoría "Configuracion" (`offsetCepillo`, `offsetAgarre`,
  `amplitudBarrido`, `frecuenciaBarrido`, `alcanceBarrido`, `bajadaAlSoltar`).
- La malla `broom_escoba` de Fab viene 100× grande: el componente va a escala 0,01.
