import "mod_say";
import "mod_proc";
import "mod_sound";
import "mod_video";
global int m, w; end
begin
    set_mode(416,240,16);
    m = load_song("beep.wav");
    say("song=" + (m != 0));
    play_song(m, 0);
    w = load_wav("beep.wav");
    play_wav(w, 0);
    frame; frame;
    stop_song();
    unload_song(m);
    say("OK5");
    exit();
end
