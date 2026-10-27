import "mod_say";
import "mod_file";
import "mod_dir";
import "mod_sound";
import "mod_map";
import "mod_text";
import "mod_regex";
import "mod_string";
import "mod_video";
import "mod_mem";
import "mod_proc";
import "mod_grproc";
import "mod_key";
import "mod_timers";
import "mod_rand";
import "mod_draw";

type datos
    int nivel;
    int vidas[3];
    string nombre;
end

global
    datos d, d2;
    int fh;
    string f;
    int snd, gr, n;
end

begin
    set_mode(320, 240, 16);
    d.nivel = 5; d.vidas[0] = 3; d.vidas[3] = 9; d.nombre = "axel";
    save("savegame/test.sav", d);
    load("savegame/test.sav", d2);
    say("load: " + d2.nivel + " " + d2.vidas[0] + " " + d2.vidas[3]);

    fh = fopen("savegame/txt.txt", O_WRITE);
    fputs(fh, "linea uno");
    fclose(fh);
    fh = fopen("savegame/txt.txt", O_READ);
    say("fgets: " + fgets(fh));
    fclose(fh);

    n = 0;
    f = glob("*.prg");
    while (f != "")
        n++;
        f = glob("*.prg");
    end
    say("glob prg: " + n);
    say("file_exists: " + file_exists("beep.wav"));

    snd = load_wav("beep.wav");
    say("wav id ok: " + (snd != 0));
    play_wav(snd, 0);

    gr = load_png("red.png");
    say("png: " + gr + " w=" + graphic_info(0, gr, G_WIDTH));
    x = new_map(16, 16, 16);
    map_clear(0, x, rgb(0,255,0));
    say("pixel: " + map_get_pixel(0, x, 3, 3));

    write(0, 10, 10, 0, "hola");
    say("regex: " + regex("r+a", "barras"));
    say("rand: " + (rand(1,10) > 0));
    frame;
    frame;
    say("OK2");
    exit();
end
