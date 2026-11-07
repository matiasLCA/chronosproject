# Flores (florero con flores)

`BP_Flores` = florero (`Mug` de Modular_Rural_Cabin) + una flor de `Garden_Flowers_Pack`. Hay 10 tipos (`tipoFlor` 0–9,
editable por instancia): Lirios blancos, Lirios azules, Tulipanes amarillos, Tulipanes rosados, Amapolas azules,
Amapolas rojizas, Campanillas azules, Claveles rojos, Margaritas amarillas, Margaritas blancas (`nombreFlor()` lo devuelve).
Miden unos 22 cm de alto para entrar en los nichos (`alturaFlores`, 16 cm de flor sobre el florero).

- **E**: recoger (solo con las manos libres). Se llevan con las dos manos, brazos pegados al cuerpo y antebrazos al frente.
- **V**: dejarlas en el suelo delante (la misma tecla suelta la escoba).
- `BP_NichoFlores`: ponerlo dentro de cada nicho (el Mug fantasma marca dónde quedan; orientar +X hacia el jugador).
  Con flores en la mano, E las deja en el nicho; con las manos libres, E las vuelve a tomar.
- No aparecen solas: colocarlas a mano en el mapa o hacer que una misión las cree. Las 10 muestras `Flores1`–`Flores10`
  y `NichoFlores1` están junto al jardín del crematorio (carpeta "Flores" del outliner).

Para las misiones de los NPC: comparar `tipoFlor` o `nombreFlor()` de `nicho.floresColocadas` con lo pedido.
Ajustes: `offsetAgarre` (dónde queda el florero entre las manos) en `BP_Flores`; la pose de los brazos en
`abp_per.actualizarBrazoFlores`.

Pruebas por consola en PIE: `ke <flores> interact <per_bp>`, `ke <per_bp> soltarFlores`, `ke <nicho> interact <per_bp>`.
