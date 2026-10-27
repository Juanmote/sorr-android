import "mod_say";
import "mod_proc";
import "mod_string";
import "mod_text";
import "mod_video";
type ficha
    int n;
    char nombre[20];
end
global
    ficha f[2];
    char suelto[30];
    string s;
    int * p;
    int v = 42;
end
begin
    set_mode(320,240,16);
    f[0].nombre = "Axel";
    f[1].nombre = "Blaze";
    suelto = "Skate";
    s = f[0].nombre + "-" + f[1].nombre + "-" + suelto;
    say("a2str: " + s);
    suelto = suelto + "!!";
    say("concat: " + suelto);
    p = &v;
    write_var(0, 10, 10, 0, p);
    write_var(0, 10, 30, 0, suelto);
    frame;
    say("OK4");
    exit();
end
