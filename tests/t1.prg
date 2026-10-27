import "mod_say";
import "mod_mem";
import "mod_string";
import "mod_proc";
import "mod_video";
import "mod_map";

type punto
    int x, y;
    string nombre;
end

global
    int arr[9];
    punto pts[3];
    int * p;
    punto * pp;
    string s = "hola";
end

process hijo(int n)
begin
    arr[n] = n * 10;
end

begin
    set_mode(320, 240, 16);
    p = alloc(100 * sizeof(int));
    for (x = 0; x < 100; x++) p[x] = x * 3; end
    y = 0;
    for (x = 0; x < 100; x++) y += p[x]; end
    say("suma=" + y);
    free(p);
    pp = &pts[2];
    pp.x = 7; pp.y = 8; pp.nombre = "tres";
    say("pts2=" + pts[2].x + "," + pts[2].y + " " + pts[2].nombre);
    p = &arr[0];
    for (x = 0; x < 10; x++) hijo(x); end
    frame;
    y = 0;
    for (x = 0; x < 10; x++) y += *(p + x); end
    say("procs=" + y);
    s = s + " mundo " + ucase(s);
    say(s);
    x = new_map(64, 64, 16);
    say("map=" + x);
    say("OK");
    exit();
end
