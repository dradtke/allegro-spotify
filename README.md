# allegro\_spotify
An Allegro module for streaming Spotify music.

It's in the very early stages of development and has not yet stabilized, but feel free to play around with the included example to get a feel for how it works.

# Building
Right now only Linux is supported. To build:

1. If you don't have a C compiler + standard libraries, Make, and `pkg-config` installed, do that first. The Makefiles are configured to use Clang, but GCC can be used by changing the `CC` variable at the top of each Makefile to `gcc`.
2. Install Allegro and the Allegro Audio module. They should be accessible via `pkg-config`; to make sure that you have them installed, run `pkg-config --list-all | grep allegro` and look for the entries `allegro-5.0` and `allegro_audio-5.0`.
3. Install libspotify. This should also be accessible via `pkg-config`; run `pkg-config --modversion libspotify` to verify that it's installed. If it prints out a version number, you're good to go.
4. Add the file `src/keys.c` and fill it in with your Spotify information. If you've installed libspotify, then you should have access to a developer API key and a premium subscription. The file should look something like this:

```c
#include <stdint.h>
#include <stdlib.h>

const uint8_t g_appkey[] = ...;
const size_t g_appkey_size = sizeof(g_appkey);
const char *username = ...;
const char *password = ...;
```

Once all that's good to go, simply run `make` to build the shared library. Once that's done, see the example's README for more info on building and running the example.
