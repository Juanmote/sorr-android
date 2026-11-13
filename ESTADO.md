# Estado del proyecto: port Android de Streets of Rage Remake

Última actualización: 13 de noviembre de 2026 (versión 10 de la APK; desde aquí, publicada por GitHub Actions).

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
- Juego en red (v8): funciona perfecto, tema cerrado.

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

## Entregado en la versión 8 (12-11-2026), pendiente de probar

- **Entorno nuevo y firma nueva**: el entorno de la sesión anterior se perdió y con él `sorr-release.jks`. La v8 va
  firmada con una clave nueva: hay que DESINSTALAR la versión anterior una vez (las partidas y perfiles están en la
  carpeta del juego, no se pierden; sí las opciones del lanzador). La clave sigue SIN estar en el repositorio; para no
  perderla otra vez, el usuario puede guardar una copia de `sorr-release.jks` (se le ha ofrecido).
- Icono: el dibujo se ha reducido (~66 %) para que el logo con la llama quepa en la zona segura del icono adaptable
  (círculo de 66 dp de 108); antes se cortaban los lados. Iconos antiguos sacados del centro del nuevo.
- Controles táctiles con la plantilla del usuario (`res/drawable-nodpi/tc_*.webp`, recortados del JPG con el fondo
  negro convertido en transparencia; script en el chat, no en el repo). Cruceta abajo a la izquierda; Ataque, Especial
  y Salto redondos abajo a la derecha; Golpear atrás y Combo especial (pastillas) encima, Combo final encima de esas;
  Policía encima de la cruceta; MENÚ y START (pastilla vacía con texto) abajo en el centro. La parte de arriba queda
  libre por el marcador. Pulsado: más claro, opaco y algo más pequeño. La opacidad 15-90 % del lanzador se convierte
  en 36-92 %. Un toque encima de un botón siempre cuenta para él (antes que la zona ampliada de la cruceta).
- **Juego en red** (`engine/modules/libnet/`), mismo modelo que Outfoxies (net.c, inet.c, inet_code.c traídos de
  `Juanmote/outfoxies-android` y adaptados): el anfitrión ejecuta el juego y manda imagen y sonido; el invitado manda
  sus botones, que entran como las teclas del JUGADOR 2 de la partida guardada. El invitado no necesita los datos.
  - TCP 47820 (UDP 47820 preguntas "SORQ1" / respuestas "SORN1", anuncios al 47821). `NET_VERSION` 1.
  - Vídeo: la pantalla del motor (RGB565, 416x240 o 320x240) sin pérdida: XOR con el fotograma anterior desplazado
    + deflate (miniz). Si la pantalla no cambia no se manda nada. Medido en la fase 1 con dos jugadores: 9-13 KB por
    fotograma (5-6 Mbit/s); menús 1-6 KB.
  - Sonido: `Mix_SetPostMix` en el anfitrión (mod_sound.c llama a `sorrnet_audio_open`), a la mitad de frecuencia en
    ADPCM. El invitado lo reproduce con SDL (máx. ~120 ms de retraso).
  - Teclas del 2P (sorrnet.c, `load_p2_keys`): `savegame/savegame.sor` posiciones 1224-1264 (arriba, abajo,
    izquierda, derecha, especial, ataque, salto, policía, combo especial, combo final, golpear atrás; de serie
    8 5 4 6 J K L H I O P, códigos BennuGD 9 6 5 7 36 37 38 35 23 24 25) y su START en 892 (U = 22; el del 1P,
    Enter, en 888). Se inyectan en `key()` (mod_key.c → `sorrnet_key`). Se releen al entrar el invitado.
  - Internet: igual que Outfoxies (UPnP + código de 8 letras; si no se puede entrar, servidor intermedio bore,
    `bore.pub:7835`, cambiable con la preferencia `net_relay` → `SORR_RELAY_SERVER`).
  - Java: tarjeta "Juego en red" en el lanzador (Crear partida / Unirse en la misma wifi (búsqueda UDP en Java) /
    Unirse por internet con código o IP). `GameActivity` recibe `EXTRA_NET` ("host", "join:ip", "code:XXXX") →
    `SORR_NET`; el invitado arranca con la carpeta interna de la app y `main.c` llama a `sorrnet_guest_main()` sin
    cargar el juego. Encima del juego, un texto con `nativeNetStatus()` (JNI en sorrnet.c): IP de la wifi y código
    de internet mientras no hay invitado, avisos al entrar o salir. Invitado: MENÚ dos veces = salir.
  - Probado en Linux con dos procesos (wifi simulada en 127.0.0.1 y servidor bore falso de Outfoxies): el invitado
    ve el juego, mueve el cursor del 2P, elige personaje con START y juega con Blaze; llega el sonido; por el
    servidor intermedio también entra. SIN PROBAR EN CONSOLAS.
- Pruebas en escritorio (`engine/` compilado para Linux, ver abajo): `SORR_KEYS="fotograma:codigo:duracion;..."`
  pulsa teclas de BennuGD (la pantalla de título necesita mantener Enter ~1 s), `SORR_SHOT=prefijo SORR_SHOTS=f1,f2`
  capturas PPM, `SORR_FRAMES=n` sale, `SORR_KEYLOG=1` qué teclas consulta el juego. Invitado: `SORR_GKEYS=
  "vuelta:bits:duracion"` (bits NETB_* de net.h). `SORR_NETLOG=1` tamaño del vídeo. La carpeta de pruebas necesita
  `palettes/` (no la tenemos: se generan `.pal` con la paleta de cada FPG de `SorR.dat`).
  Compilar para Linux: `cmake -S engine -B build-linux` (paquetes libsdl2-dev, libsdl2-mixer-dev, libpng-dev) y
  copiar el binario como `bgdi` (no enlace simbólico) junto a `SorR.dat`.

## Entregado en la versión 9 (13-11-2026), pendiente de probar

- **Entorno nuevo otra vez**: la clave de la v8 se perdió; la v9 se firma con otra (`sorr-release.jks`, se le manda al
  usuario para que la guarde: si la vuelve a subir en otra sesión, se copia a la raíz del repo antes de compilar).
- **Perfil "Principal" con todo desbloqueado**: la carpeta del usuario traía ya una partida desbloqueada en `savegame/`
  y se tomaba como "Principal". `Profiles.migrateOldSave()` (una vez por carpeta, preferencia `migrated_v9:<carpeta>`):
  si la partida de "Principal" tiene 30 o más enteros distintos de 0 entre los bytes 8 y 252 (la desbloqueada tiene ~55,
  una nueva casi ninguno), se guarda como perfil "Partida anterior" y "Principal" empieza con `savegame-base.sor`.
  Se avisa con un diálogo. `savegame/` sigue siendo siempre el perfil activo; las copias, en `sorr_profiles/<nombre>/`.
- **El juego seguía sonando al minimizar o bloquear**: nadie llamaba a `SDL_PumpEvents` y nadie retiraba de la cola los
  eventos `SDL_APP_*` (el manejador de libwm empieza con `return;`), así que SDL nunca llegaba a pausar. Ahora
  `libsdlhandler.c` (cada fotograma, solo Android) hace `SDL_PumpEvents()` y vacía los eventos de app/ventana/pantalla:
  SDL bloquea el hilo del juego en segundo plano y pausa el audio (`Android_PumpEvents_Blocking`).
- **Filtros y colores** como Outfoxies / Master of Darkness, en `engine/modules/libnet/sorrvideo.c` (lo usan
  `g_screen.c` y el invitado `guest.c`): `SORR_FILTER` = sharp | sharpsmooth | smooth | scan | crt | crtcurve,
  `SORR_COLORS` = original | tv | vivid | night | sepia | green (tabla RGB565 de 65536 entradas). `SORR_ASPECT` igual
  que antes. La preferencia antigua `video_smooth` se traduce a "smooth". Probado en escritorio con SDL por software.
- **Vibración** (`sorrvib.c`): no se puede leer la energía como en Outfoxies, así que va por los sonidos: `load_wav`
  guarda el nombre del archivo y `play_wav` decide: policía/cohetes (90 %, 420 ms), explosiones (80, 260), caídas,
  lanzamientos y muertes (68, 160), golpes (50, 85), salto (22, 45), escalado por la intensidad del lanzador
  (`vibration`, 0-100, por defecto 50 → `SORR_VIB`). Una suave no corta a una fuerte. Java: `GameActivity.rumble`
  (motor del mando si lo tiene, si no el de la consola). **Los nombres son una suposición**: el juego escribe
  `sorr_sonidos.txt` en su carpeta (sonidos, veces y vibración asignada) para afinar la lista. Solo en el anfitrión.
- **Lanzador**: tema con los colores del logo (fuego rojo-naranja-amarillo sobre casi negro y el azul de "OF RAGE"),
  fuentes Bangers (títulos, botones principales) y Barlow Condensed (texto), ambas OFL en `assets/fonts`. El foco del
  mando se conserva al rehacer la pantalla (antes se perdía y Android pintaba su "resaltado por defecto" blanco por
  encima de todo); cada botón tiene su estado enfocado (marco amarillo). Deslizadores guardan al moverlos con la cruceta.
  Proporción, Filtro y Colores son botones que cambian al pulsar (izquierda/derecha con el mando) con su explicación.
  Tarjeta "Sonido y vibración": intensidad y "Música del menú" SÍ/NO (`menu_music`).
- **Música del lanzador**: tema propio, rock de los 80 en chiptune (Mi menor, 152 BPM, 32 compases, 50 s en bucle),
  sintetizado con `tools/compose_menu_theme.py` → `res/raw/menu_theme.ogg` (ffmpeg libvorbis q4). MediaPlayer en bucle,
  se pausa al salir del lanzador.
- **Cruceta táctil**: queda translúcida como los botones; solo se ilumina el brazo pulsado (recorte del dibujo).

## Versión 10 (13-11-2026): primera publicada en Releases

- **Releases automáticas**: `.github/workflows/release.yml`. Cada push a `main` (salvo .md, tools/, tests/) compila con
  `build_apk.sh` en GitHub y publica `vVERSION.N` (N = número de ejecución) con la APK. versionCode = 1000 + N (siempre
  sube), versionName = `android:versionName` del manifiesto + ".N". La clave sale de los secretos del repositorio
  (`SORR_KEYSTORE_B64` = jks en base64, `SORR_KEYSTORE_PASS`, `SORR_KEY_ALIAS` opcional; se aceptan otros nombres
  habituales, ver el workflow). Sin clave falla y no publica. El usuario dice que ya ha subido la clave; no se pueden
  leer los nombres de los secretos desde la sesión. `build_apk.sh` ahora toma de variables de entorno ROOT, ANDROID_SDK,
  NDK, SORR_KEYSTORE(_PASS), SORR_KEY_ALIAS/PASS, SORR_VERSION_CODE/NAME, SORR_REQUIRE_KEY; `native/CMakeLists.txt`
  busca `src/` junto al repo (no ruta fija).
- **Vibración con los sonidos reales** (la lista por palabras de la v9 no acertaba: los archivos se llaman `se1.wav`...).
  Comprobado ejecutando el juego en Linux con el SorR.dat del usuario (SORR_SNDLOG=1 da "snd fotograma archivo"):
  `se1` golpe que entra (lo dé o lo reciba el jugador; al aire no suena nada), `se37` grito del jugador al recibir
  (fuerte), `se45` caída al suelo, `se59/60/61` remates, `se17` especial, `se2` salto, `se30` aterrizaje, `se50/54`
  golpes de enemigos que fallan, `se26` flecha GO, `se24/25/62` menús/pausa, `se75` coche de policía de la intro.
  Explosiones (se47, 49, 56, 81) y policía (se41, 77, 84, 109) asignadas por su forma de onda, sin comprobar en juego.
  Tabla `known[]` en `sorrvib.c`; nombres que empiezan por "se" y no están en la tabla no vibran; mods: por palabras.
  Intensidad: 50 % = tabla tal cual, 100 % = el doble.
- **Salir del juego cierra también el lanzador**: `GameActivity.onDestroy` (salida normal) crea `files/exit_all`; el
  lanzador lo ve en `onResume` y hace `finishAndRemoveTask()`. Si el juego se cierra por un fallo no se crea, así que el
  lanzador sigue y guarda el logcat. Al abrir el lanzador de cero se borra un aviso viejo.
- **Idioma**: `L.java` (`L.t(es, en)`): castellano si el sistema está en castellano, si no inglés. Lanzador, perfiles,
  tabla de botones y "MENU" táctil. El motor recibe `SORR_LANG` y los avisos del juego en red salen igual
  (`sorrnet_en()`). Los nombres de perfil ya creados ("Principal") no se traducen.
- Pruebas en escritorio: SDL2 y SDL_mixer compilados desde `src/` a un prefijo (no hay paquetes -dev), motor con
  `cmake -S engine -B build -DCMAKE_PREFIX_PATH=prefijo`, paletas generadas de los FPG de 8 bits de SorR.dat
  (`palettes/{chars,enemies,backup_*}/<nombre>.pal` = "pal\x1a\x0d\x0a\x00" + bytes 7..1351 del FPG).
  Guion que entra en la fase 1: Enter (28) 40 fotogramas cada 150 hasta el 900; luego C=46 ataque, V=47 salto,
  X=45 especial, flecha derecha=77. Los archivos embebidos en SorR.dat: entradas de 13 bytes (SName, Flags, SFile,
  OFile) justo antes de cada nombre.

## Pendiente

1. Comprobar que la primera release de GitHub sale firmada con la clave del usuario (si falla, ver qué nombres de
   secretos ha usado).
2. Que el usuario pruebe vibración, idioma y cierre conjunto; confirmar explosiones y policía en juego.
3. El usuario irá apuntando más mejoras según juegue.
## Notas

- El registro mostraba que el juego pide 960x544 al iniciar y luego 416x240: su carpeta de datos
  parece venir configurada en panorámico.
- Responder siempre en castellano. Si el chat se alarga, avisar y proponer seguir en uno nuevo.
