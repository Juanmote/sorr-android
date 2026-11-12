# Estado del proyecto: port Android de Streets of Rage Remake

Última actualización: 12 de noviembre de 2026 (versión 7 de la APK).

## Qué es

Port propio de Streets of Rage Remake (SorR 5.2) para Android, hecho desde cero con código abierto.
No reutiliza nada de otros ports. El usuario (Juanmote) no programa: aporta archivos y prueba en su
consola; la parte técnica la hace Claude.

- Repositorio: `Juanmote/sorr-android` (privado). Rama `main`.
- Consola de pruebas: AYN Odin 3 (arm64, Android 15, pantalla 1920x1080). Solo ejecuta 64 bits.
- Datos del juego: los pone el usuario (carpeta de SorR de PC con `SorR.dat`, `mod/`, `palettes/`...).
  En su consola están en la tarjeta SD: `/storage/3E86-BA35/Download/.../SORRv52`.
- Paquete Android: `org.sorr.remake`. Firma propia (`sorr-release.jks`, NO está en el repo: si se
  pierde, hay que desinstalar la app antes de instalar una APK firmada con otra clave).

## Cómo está hecho

- `engine/`: bennugd-monolithic (BennuGD 1 sobre SDL2), tomado de `isage/sorr-vita`, parcheado:
  - `lowmem/`: asignador TLSF que reserva toda la memoria por debajo de 2 GB. Es imprescindible:
    el intérprete guarda punteros en enteros de 32 bits con signo. Se engancha con `-Wl,--wrap=malloc...`
    y `SDL_SetMemoryFunctions`. Entrega la memoria a cero.
  - Arreglos de 64 bits: lectura de `DCB_FILE` (13 bytes en disco), `MN_A2STR/STR2A/STRACAT/POINTER2STR`
    en `interpreter.c`, `libtext.c`, `mod_sort.c`.
  - `mod_sound`: faltaba el callback `size` del `SDL_RWops`.
  - `dirs.c` restaurado del BennuGD original (el de Vita tenía rutas fijas y `glob` desactivado).
  - Arranque Android en `core/bgdi/src/main.c`: argv[1] = carpeta del juego, argv[2] = `SorR.dat`;
    registro en `sorr_log.txt` con pasos y volcado de cierres (señal, pc, backtrace).
  - Escalado manual al presentar el fotograma (`librender/g_screen.c`), según `SORR_ASPECT`:
    `fit`, `crop`, `mix`, `stretch`. `SORR_SMOOTH=1` activa filtro lineal.
- `app/`: Java puro, sin Gradle ni AndroidX.
  - `LauncherActivity`: carpeta del juego, permiso de archivos, perfiles, opciones.
  - `Profiles`: perfiles por copia de archivos. Activo en `savegame/`, copias en `sorr_profiles/<nombre>/`.
    "Unlocked" usa `assets/saves/savegame-unlock.sor` (de PortMaster); los nuevos, `savegame-base.sor`.
  - `GameActivity` (SDLActivity, proceso `:game`): mando físico traducido a teclas (`Keys.java`,
    mismo esquema que PortMaster), controles táctiles (`TouchOverlay`) que se ocultan por completo
    al usar mando y vuelven al tocar la pantalla. Al salir se mata el proceso.
- `native/CMakeLists.txt` + `build_apk.sh`: compila SDL2 2.30.9, SDL_mixer 2.8.0, libpng 1.6.44
  (sin modificar, hay que clonarlos junto a `engine` en `src/`) y monta la APK con aapt2/javac/d8/apksigner
  (build-tools 35, NDK r27c, minSdk 28, librerías alineadas a 16 KB). Las rutas del script apuntan a
  `/home/claude/sorr` y `/home/claude/android/sdk`: ajustar si el entorno es nuevo.
- `tests/`: programas BennuGD de prueba. Se compilan con `bgdc` (hay que compilarlo en 32 bits, `-m32`)
  y se ejecutan con el motor compilado para Linux x86_64, que usa el mismo asignador de memoria baja.

## Limitaciones del entorno de trabajo

- No se puede emular Android (sin KVM). Todo lo de Android lo prueba el usuario en la consola.
- Maven Central y GitLab están bloqueados; GitHub y dl.google.com funcionan.
- Método que ha funcionado: el usuario manda `sorr_log.txt` / `sorr_logcat.txt` (se crean en la
  carpeta del juego) y con el backtrace se localiza el fallo.

## Hecho y confirmado por el usuario

- El juego arranca y se juega bien en la Odin 3 (versión 4).
- Con mando no queda ningún botón táctil en pantalla.
- Perfil "Unlocked" con todo desbloqueado.

## Entregado, pendiente de que el usuario lo pruebe (versión 5)

- Perfiles rehechos (antes no dejaba crear otro perfil y se quedaba en "Unlocked"; causa exacta
  sin confirmar). Ahora propone nombre ("Perfil 2") y muestra el error concreto si falla.
- Modos de proporción: Original / Recortar / Mixta / Estirada. El usuario quería 8:7 sin deformar:
  un 8:7 "real" (que el juego dibuje otro campo de visión) no es posible desde el motor.

## Entregado en la versión 6, pendiente de probar

- Tabla de botones del mando editable desde el lanzador (Controles > Configurar botones del mando):
  cada tecla del juego (X, C, V, B, S, A, D, R, Enter, Esc) admite cualquier botón que mande el mando.
  Se guarda en la preferencia `pad_map` (`Keys.java`). Motivo: el usuario no podía cambiar controles
  dentro del juego y quiere usar los seis botones de la Retroid Pocket Classic. De serie, C y Z van a
  las teclas D y B.
- El perfil "Unlocked" existe siempre y no se puede borrar.
- Icono nuevo (logo, con icono adaptable) y fondo del lanzador con el arte de personajes
  (`res/drawable-nodpi/launcher_art.webp`, dibujado por `ArtView`). Texto "Ported by Juanmote".

## Entregado en la versión 7, pendiente de probar

- Partida "Unlocked" de la APK con los cheats apagados. En `savegame.sor` (enteros de 32 bits), las
  posiciones 176-252 son los cheats: 0 = sin comprar, 1 = comprado y apagado, 2 = activado. Se han
  pasado los 2 a 1 y la posición 916 a 3, según la partida que mandó el usuario.
- Teclas igualadas en las dos partidas de la APK (posiciones 1208-1216 = A, B, S).
- Acciones confirmadas por el usuario: X especial, C ataque, V salto, B combo especial, S combo final,
  A policía, D golpear atrás (R no hace nada). Botones de serie: X=X, B=ataque, A=salto, R2 y L2=combo
  especial, Y y Z=combo final, L1=policía, R1 y C=golpear atrás. El lanzador muestra las acciones.

## Pendiente

1. **Juego en red.** El usuario lo quiere "como en Outfoxies" (hecho en otro chat). El juego no lo trae:
   habría que sincronizar las entradas de los dos jugadores en el motor (mismo `SorR.dat`, misma partida
   y opciones, misma semilla aleatoria). Para probarlo en Linux con dos copias hace falta `SorR.dat`.
2. El usuario irá apuntando más mejoras según juegue.

## Notas

- El registro mostraba que el juego pide 960x544 al iniciar y luego 416x240: su carpeta de datos
  parece venir configurada en panorámico.
- Responder siempre en castellano. Si el chat se alarga, avisar y proponer seguir en uno nuevo.
