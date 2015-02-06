#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <libspotify/api.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_spotify.h>

#define DEBUG 0 // change to 1 for debugging information

extern const uint8_t g_appkey[];
extern const size_t g_appkey_size;
extern const char *username;
extern const char *password;

// TODO: finish creating Allegro events for all of the callbacks defined at
//          https://developer.spotify.com/docs/libspotify/12.1.51/structsp__session__callbacks.html

struct _ALLEGRO_SPOTIFY_STREAM {
	sp_session *session;
	ALLEGRO_AUDIO_STREAM *stream;
	ALLEGRO_MUTEX *mutex;
	ALLEGRO_PATH *cache;

	bool logged_in;
	bool playing;
	int frames_per_fragment;

	ALLEGRO_EVENT_SOURCE event_source;
	ALLEGRO_EVENT login_event;
	ALLEGRO_EVENT notify_event;
	ALLEGRO_EVENT end_of_track_event;
	ALLEGRO_EVENT search_complete_event;
};

static void debug(const char *format, ...)
{
	if (!DEBUG)
		return;

	va_list argptr;
	va_start(argptr, format);
	printf("[allegro-spotify] ");
	vprintf(format, argptr);
	printf("\n");
}

static void _al_play(ALLEGRO_SPOTIFY_STREAM *spotify_stream, sp_track *track)
{
	debug("found track, playing it");
	sp_error error = sp_session_player_load(spotify_stream->session, track);
	if (error != SP_ERROR_OK) {
		fprintf(stderr, "error in _al_play: %s\n", sp_error_message(error));
		exit(1);
	}

	sp_session_player_play(spotify_stream->session, 1);
	spotify_stream->playing = 1;
}

static void _on_search_complete(sp_search *search, void *userdata)
{
	debug("search complete");
	ALLEGRO_SPOTIFY_STREAM *spotify_stream = (ALLEGRO_SPOTIFY_STREAM*)(userdata);
	sp_error error = sp_search_error(search);
	if (error != SP_ERROR_OK) {
		fprintf(stderr, "Error: %s\n", sp_error_message(error));
		exit(1);
	}

	int num_tracks = sp_search_num_tracks(search);
	spotify_stream->search_complete_event.user.data2 = num_tracks;
	al_emit_user_event(&spotify_stream->event_source, &spotify_stream->search_complete_event, NULL);

	if (num_tracks == 0) {
		sp_search_release(search);
		spotify_stream->playing = 0;
		return;
	}

	sp_track *track = sp_search_track(search, 0);
	_al_play(spotify_stream, track);
	sp_search_release(search);
}

static void _on_login(sp_session *session, sp_error error)
{
	debug("login: %s", sp_error_message(error));
	ALLEGRO_SPOTIFY_STREAM *spotify_stream = (ALLEGRO_SPOTIFY_STREAM*)(sp_session_userdata(session));
	if (error == SP_ERROR_OK) {
		spotify_stream->logged_in = 1;
	}
	spotify_stream->login_event.user.data2 = error;
	al_emit_user_event(&spotify_stream->event_source, &spotify_stream->login_event, NULL);
}

static void _on_notify_main_thread(sp_session *session)
{
	debug("notifying main thread");
	ALLEGRO_SPOTIFY_STREAM *spotify_stream = (ALLEGRO_SPOTIFY_STREAM*)(sp_session_userdata(session));
	al_emit_user_event(&spotify_stream->event_source, &spotify_stream->notify_event, NULL);
}

static ALLEGRO_AUDIO_STREAM *_create_audio_stream(const sp_audioformat *format, const int bytes_per_fragment)
{
	debug("creating audio stream");
	ALLEGRO_CHANNEL_CONF chan_conf;
	switch (format->channels) {
		case 1:
			chan_conf = ALLEGRO_CHANNEL_CONF_1;
			break;
		case 2:
			chan_conf = ALLEGRO_CHANNEL_CONF_2;
			break;
		default:
			fprintf(stderr, "unexpected number of channels: %d\n", format->channels);
			return NULL;
	}

	ALLEGRO_AUDIO_DEPTH depth;
	switch (format->sample_type) {
		case SP_SAMPLETYPE_INT16_NATIVE_ENDIAN:
			depth = ALLEGRO_AUDIO_DEPTH_INT16;
			break;
		default:
			fprintf(stderr, "unexpected sample type: %d\n", format->sample_type);
			return NULL;
	}

	int fragment_count = 16; // 8 seems to be the minimum for it to work properly

	int sample_size = format->channels * al_get_audio_depth_size(depth);
	int samples = bytes_per_fragment / sample_size;

	ALLEGRO_AUDIO_STREAM *stream = al_create_audio_stream(fragment_count, samples, format->sample_rate, depth, chan_conf);
	al_attach_audio_stream_to_mixer(stream, al_get_default_mixer());

	return stream;
}

/*
 * Spotify music delivery callback.
 * This method is responsible for filling the audio stream's buffer with data.
 */
static int _on_music_delivery(sp_session *session, const sp_audioformat *format, const void *frames, int num_frames)
{
	if (num_frames == 0) {
		return 0;
	}

	ALLEGRO_SPOTIFY_STREAM *spotify_stream = (ALLEGRO_SPOTIFY_STREAM*)(sp_session_userdata(session));
	al_lock_mutex(spotify_stream->mutex);

	// Lazily initialize the underlying audio stream so that it can match Spotify's numbers.
	if (spotify_stream->stream == NULL) {
		int bytes_per_fragment = num_frames * sizeof(int16_t) * format->channels;
		ALLEGRO_AUDIO_STREAM *stream = _create_audio_stream(format, bytes_per_fragment);
		if (stream == NULL) {
			return 0;
		}
		spotify_stream->stream = stream;
		spotify_stream->frames_per_fragment = num_frames;
	}

	void *buffer = al_get_audio_stream_fragment(spotify_stream->stream);
	if (buffer == NULL) {
		al_unlock_mutex(spotify_stream->mutex);
		return 0;
	}

	int frames_to_copy = fmin(num_frames, spotify_stream->frames_per_fragment);
	int bytes_to_copy = frames_to_copy * sizeof(int16_t) * format->channels;

	memcpy(buffer, frames, bytes_to_copy);
	al_set_audio_stream_fragment(spotify_stream->stream, buffer);
	al_unlock_mutex(spotify_stream->mutex);

	return frames_to_copy;
}

static void _on_end_of_track(sp_session *session)
{
	debug("track ended");
	sp_session_player_unload(session);
	ALLEGRO_SPOTIFY_STREAM *spotify_stream = (ALLEGRO_SPOTIFY_STREAM*)(sp_session_userdata(session));
	al_emit_user_event(&spotify_stream->event_source, &spotify_stream->end_of_track_event, NULL);
}

static void _on_log(sp_session *session, const char *data)
{
	// TODO: create a config option for saving this data somewhere
}

static sp_session_callbacks _session_callbacks = {
	.logged_in = &_on_login,
	.notify_main_thread = &_on_notify_main_thread,
	.music_delivery = &_on_music_delivery,
	.log_message = &_on_log,
	.end_of_track = &_on_end_of_track
};

void al_spotify_stream_process_events(ALLEGRO_SPOTIFY_STREAM *spotify_stream)
{
	int next_timeout;
	do {
		sp_session_process_events(spotify_stream->session, &next_timeout);
	}
	while (next_timeout == 0);
}

// TODO: take a username and password
// This method needs to take an event queue because it is the mechanism
// by which Spotify tells Allegro that it needs to process more events,
// and as such it needs to be registered before the session is created.
ALLEGRO_SPOTIFY_STREAM *al_new_spotify_stream(ALLEGRO_EVENT_QUEUE *event_queue) {
	sp_error error;

	// Create and initialize the Spotify stream.
	ALLEGRO_SPOTIFY_STREAM *spotify_stream = (ALLEGRO_SPOTIFY_STREAM*)(al_malloc(sizeof(ALLEGRO_SPOTIFY_STREAM)));
	spotify_stream->session = NULL;
	spotify_stream->logged_in = 0;
	spotify_stream->playing = 0;
	spotify_stream->stream = NULL; // will be initialized lazily
	spotify_stream->mutex = al_create_mutex();

	// Initialize its event source and events.
	spotify_stream->login_event.user.type = ALLEGRO_EVENT_SPOTIFY_LOGIN;
	spotify_stream->login_event.user.data1 = (intptr_t)(spotify_stream);
	spotify_stream->login_event.user.data2 = SP_ERROR_OK;

	spotify_stream->notify_event.user.type = ALLEGRO_EVENT_SPOTIFY_NOTIFY_MAIN_THREAD;
	spotify_stream->notify_event.user.data1 = (intptr_t)(spotify_stream);

	spotify_stream->end_of_track_event.user.type = ALLEGRO_EVENT_SPOTIFY_END_OF_TRACK;
	spotify_stream->end_of_track_event.user.data1 = (intptr_t)(spotify_stream);

	spotify_stream->search_complete_event.user.type = ALLEGRO_EVENT_SPOTIFY_SEARCH_COMPLETE;
	spotify_stream->search_complete_event.user.data1 = (intptr_t)(spotify_stream);

	al_init_user_event_source(&spotify_stream->event_source);
	al_register_event_source(event_queue, &spotify_stream->event_source);

	// Determine the path to use for cache and settings.
	spotify_stream->cache = al_get_standard_path(ALLEGRO_TEMP_PATH);
	al_append_path_component(spotify_stream->cache, "allegro-spotify");

	// Create and populate the session configuration.
	sp_session_config spconfig;
	memset(&spconfig, 0, sizeof(sp_session_config));
	spconfig.api_version = SPOTIFY_API_VERSION;
	spconfig.cache_location = al_path_cstr(spotify_stream->cache, ALLEGRO_NATIVE_PATH_SEP);
	spconfig.settings_location = al_path_cstr(spotify_stream->cache, ALLEGRO_NATIVE_PATH_SEP);
	spconfig.application_key = g_appkey;
	spconfig.user_agent = "allegro";
	spconfig.callbacks = &_session_callbacks;
	spconfig.userdata = spotify_stream;
	spconfig.application_key_size = g_appkey_size;

	// Attempt to create the session.
	if ((error = sp_session_create(&spconfig, &spotify_stream->session)) != SP_ERROR_OK) {
		fprintf(stderr, "unable to create session: %s\n", sp_error_message(error));
		return NULL;
	}

	// On success, log in and get the ball rolling.
	sp_session_login(spotify_stream->session, username, password, 0, NULL);

	return spotify_stream;
}

void al_destroy_spotify_stream(ALLEGRO_SPOTIFY_STREAM *spotify_stream)
{
	sp_session_release(spotify_stream->session);
	al_destroy_audio_stream(spotify_stream->stream);
	al_destroy_mutex(spotify_stream->mutex);
	al_destroy_path(spotify_stream->cache);
	al_free(spotify_stream);
}

void al_play_song(ALLEGRO_SPOTIFY_STREAM *spotify_stream, char *artist, char *track)
{
	debug("play_song(%s, %s)", artist, track);
	char q[4096];
	sprintf(q, "artist:\"%s\" track:\"%s\"", artist, track);
	sp_search_create(spotify_stream->session, q, 0, 1, 0, 0, 0, 0, 0, 0, SP_SEARCH_STANDARD, &_on_search_complete, spotify_stream);
}

