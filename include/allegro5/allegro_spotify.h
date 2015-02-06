#ifndef ALLEGRO_SPOTIFY_H
#define ALLEGRO_SPOTIFY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ALLEGRO_SPOTIFY_STREAM ALLEGRO_SPOTIFY_STREAM;

// Represents a login event from Spotify. The data2 field contains the status of the login attempt
// as an sp_error value; anything besides SP_ERROR_OK indicates that the login failed.
const int32_t ALLEGRO_EVENT_SPOTIFY_LOGIN = ALLEGRO_GET_EVENT_TYPE('S', 'P', 'O', 'L'); // Spotify Login

// Represents libspotify telling the main thread that it needs to process more events.
// This can be done by calling al_spotify_stream_process_events().
const int32_t ALLEGRO_EVENT_SPOTIFY_NOTIFY_MAIN_THREAD = ALLEGRO_GET_EVENT_TYPE('S', 'N', 'M', 'T'); // Spotify Notify Main

// Represents a notification that the current track has ended.
const int32_t ALLEGRO_EVENT_SPOTIFY_END_OF_TRACK = ALLEGRO_GET_EVENT_TYPE('S', 'E', 'O', 'T'); // Spotify End of Track

// The data2 field contains the number of tracks that were found in the search.
const int32_t ALLEGRO_EVENT_SPOTIFY_SEARCH_COMPLETE = ALLEGRO_GET_EVENT_TYPE('S', 'P', 'S', 'C'); // Spotify Search Complete

ALLEGRO_SPOTIFY_STREAM *al_new_spotify_stream(ALLEGRO_EVENT_QUEUE *event_queue);
void al_spotify_stream_process_events(ALLEGRO_SPOTIFY_STREAM *spotify_stream);
void al_play_song(ALLEGRO_SPOTIFY_STREAM *spotify_stream, char *artist, char *track);

#ifdef __cplusplus
}
#endif

#endif
