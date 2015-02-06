#include <stdio.h>

#include <libspotify/api.h>
#include <allegro5/allegro.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_spotify.h>

// Initialize Allegro.
void init_allegro()
{
	if (!al_init()) {
		fprintf(stderr, "failed to start allegro!\n");
		exit(1);
	}

	if (!al_install_audio()) {
		fprintf(stderr, "failed to install audio subsystem!\n");
		exit(1);
	}

	if (!al_reserve_samples(1)) {
		fprintf(stderr, "failed to reserve an audio sample!\n");
		exit(1);
	}
}

int main(int argc, char *argv[])
{
	if (argc != 3 || !strcmp(argv[1], "") || !strcmp(argv[2], "")) {
		fprintf(stderr, "usage: make run artist=\"<ARTIST>\" track=\"<TRACK>\"\n");
		return 0;
	}

	init_allegro();

	ALLEGRO_EVENT_QUEUE *event_queue = al_create_event_queue();

	ALLEGRO_SPOTIFY_STREAM *spotify_stream = al_new_spotify_stream(event_queue);
	if (spotify_stream == NULL) {
		return 2;
	}

	ALLEGRO_EVENT event;

	// main loop
	while (1) {
		al_wait_for_event(event_queue, &event);
		switch (event.type) {
			case ALLEGRO_EVENT_SPOTIFY_NOTIFY_MAIN_THREAD:
				// process Spotify events
				al_spotify_stream_process_events(spotify_stream);
				break;
			case ALLEGRO_EVENT_SPOTIFY_LOGIN:
				if (event.user.data2 == SP_ERROR_OK) {
					printf("Searching for %s: %s...", argv[1], argv[2]);
					al_play_song(spotify_stream, argv[1], argv[2]);
				} else {
					fprintf(stderr, "failed to log in: %s\n", sp_error_message(event.user.data2));
				}
				break;
			case ALLEGRO_EVENT_SPOTIFY_SEARCH_COMPLETE:
				if (event.user.data2 == 0) {
					printf("track not found.\n");
					return 0;
				} else if (event.user.data2 == 1) {
					printf("found.\nPlaying...\n");
					break;
				} else {
					printf("too many options! %d tracks were found.\n", (int)(event.user.data2));
					return 0;
				}
			case ALLEGRO_EVENT_SPOTIFY_END_OF_TRACK:
				return 0;
			default:
				break;
		}
	}
}
