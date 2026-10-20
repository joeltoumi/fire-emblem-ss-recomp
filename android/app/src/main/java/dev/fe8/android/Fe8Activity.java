package dev.fe8.android;

import org.libsdl.app.SDLActivity;

/**
 * Entry point: SDL's activity loads libSDL3.so and libmain.so and calls
 * SDL_main. libmain.so is a launcher that loads the game + port
 * (libgame.so, built from the repository's CMakeLists.txt) at a low address
 * and runs it (port/src/android_launcher.c).
 */
public class Fe8Activity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }
}
