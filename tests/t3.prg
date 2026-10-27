import "mod_say";
import "mod_mem";
import "mod_proc";
global
    int * bloques[59];
    int i, j, err;
end
begin
    for (i = 0; i < 60; i++)
        bloques[i] = alloc(10485760);
        if (bloques[i] == 0) say("alloc fallo " + i); exit(); end
        for (j = 0; j < 2621440; j += 4096) bloques[i][j] = i * 7 + j; end
    end
    err = 0;
    for (i = 0; i < 60; i++)
        for (j = 0; j < 2621440; j += 4096) if (bloques[i][j] != i * 7 + j) err++; end end
        free(bloques[i]);
    end
    say("errores=" + err);
    say("OK3");
    exit();
end
