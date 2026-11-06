#include "sound.h"

#include "error.h"
#include "util.h"

#include <algorithm>
#include <array>
#include <string>

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

static MIX_Mixer *shared_mixer = nullptr;
static int sound_instance_count = 0;
static bool mixer_initialized = false;

static std::string resolve_sound_path(const std::string &path) {
	if (is_absolute_path(path) && file_exists(path))
		return path;

	std::string cwd = current_working_directory();
	std::array<std::string, 4> candidates = {
		join_path(cwd, path),
		join_path(join_path(cwd, "resources"), path),

#ifdef ENGINE_RESOURCE_ROOT
		join_path(ENGINE_RESOURCE_ROOT, path),
#else
		std::string(),
#endif
		join_path("resources", path),
	};

	for (const auto &candidate : candidates) {
		if (!candidate.empty() && file_exists(candidate))
			return candidate;
	}

	return join_path(cwd, path);
}

static MIX_Mixer *acquire_mixer() {
	if (!mixer_initialized) {
		if (!MIX_Init()) {
			error(std::string("SDL3_mixer init failed: ") +
				SDL_GetError());
		}
		mixer_initialized = true;
	}

	if (shared_mixer == nullptr) {
		shared_mixer = MIX_CreateMixerDevice(
			SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
		if (shared_mixer == nullptr) {
			std::string message = std::string("SDL3_mixer create mixer failed: ") + SDL_GetError();
			MIX_Quit();
			mixer_initialized = false;
			error(message);
		}
	}

	++sound_instance_count;
	return shared_mixer;
}

static void release_mixer() {
	if (sound_instance_count > 0)
		--sound_instance_count;

	if (sound_instance_count != 0)
		return;

	if (shared_mixer != nullptr) {
		MIX_DestroyMixer(shared_mixer);
		shared_mixer = nullptr;
	}
	if (mixer_initialized) {
		MIX_Quit();
		mixer_initialized = false;
	}
}

Sound::Sound(const std::string &path, SoundPlaybackMode playback_mode,
	int max_instances)
	: path(path), max_instances(std::max(1, max_instances)),
	  playback_mode(playback_mode) {
	try {
		mixer = acquire_mixer();
		std::string resolved = resolve_sound_path(path);

		audio = MIX_LoadAudio(mixer, resolved.c_str(), true);
		if (audio == nullptr) {
			error("Failed to load sound " + resolved + ": " +
				SDL_GetError());
		}

		voices.reserve(static_cast<std::size_t>(this->max_instances));
		create_voice();
		loaded = true;
		set_volume(volume);
	} catch (...) {
		dispose();
		throw;
	}
}

Sound::~Sound() { dispose(); }

void Sound::play() {
	if (!loaded || voices.empty())
		return;

	if (playback_mode == SoundPlaybackMode::Overlap) {
		play_overlapping();
		return;
	}

	play_voice(voices.front());
}

void Sound::play_overlapping() {
	if (!loaded)
		return;
	Voice *voice = find_available_voice();
	if (voice != nullptr)
		play_voice(*voice);
}

void Sound::pause() {
	for (Voice &voice : voices) {
		if (voice.track != nullptr && MIX_TrackPlaying(voice.track))
			MIX_PauseTrack(voice.track);
	}
}

void Sound::resume() {
	if (!loaded)
		return;
	for (Voice &voice : voices) {
		if (voice.track != nullptr && MIX_TrackPaused(voice.track))
			MIX_ResumeTrack(voice.track);
	}
}

void Sound::stop() {
	for (Voice &voice : voices) {
		if (voice.track != nullptr)
			MIX_StopTrack(voice.track, 0);
	}
}

void Sound::restart() {
	const bool was_active = is_playing() || is_paused();
	stop();
	frame_position = start_position;
	if (was_active)
		play();
}

void Sound::set_frame_pos(int position) {
	frame_position = std::max(0, position);
	for (Voice &voice : voices) {
		if (voice.track != nullptr)
			MIX_SetTrackPlaybackPosition(voice.track, frame_position);
	}
}

void Sound::set_volume(float value) {
	volume = std::max(0.0f, std::min(value, 1.0f));
	for (Voice &voice : voices) {
		if (voice.track != nullptr)
			MIX_SetTrackGain(voice.track, volume);
	}
}

float Sound::get_volume() const { return volume; }

void Sound::set_playback_mode(SoundPlaybackMode mode) {
	if (playback_mode == mode)
		return;
	if (mode == SoundPlaybackMode::Single) {
		stop();
		while (voices.size() > 1) {
			destroy_voice(voices.back());
			voices.pop_back();
		}
	}
	playback_mode = mode;
}

SoundPlaybackMode Sound::get_playback_mode() const {
	return playback_mode;
}

void Sound::set_max_instances(int value) {
	max_instances = std::max(1, value);
	while (voices.size() > static_cast<std::size_t>(max_instances)) {
		destroy_voice(voices.back());
		voices.pop_back();
	}
	if (voices.capacity() < static_cast<std::size_t>(max_instances))
		voices.reserve(static_cast<std::size_t>(max_instances));
}

int Sound::get_max_instances() const { return max_instances; }

int Sound::get_playing_instance_count() const {
	int count = 0;
	for (const Voice &voice : voices) {
		if (voice.track != nullptr && MIX_TrackPlaying(voice.track))
			++count;
	}
	return count;
}

bool Sound::is_loaded() const { return loaded; }

bool Sound::is_playing() const {
	return get_playing_instance_count() > 0;
}

bool Sound::is_paused() const {
	for (const Voice &voice : voices) {
		if (voice.track != nullptr && MIX_TrackPaused(voice.track))
			return true;
	}
	return false;
}

void Sound::dispose() {
	if (!loaded && audio == nullptr && voices.empty() && mixer == nullptr)
		return;

	for (Voice &voice : voices)
		destroy_voice(voice);
	voices.clear();

	if (audio != nullptr) {
		MIX_DestroyAudio(audio);
		audio = nullptr;
	}

	loaded = false;
	if (mixer != nullptr) {
		mixer = nullptr;
		release_mixer();
	}
}

Sound::Voice *Sound::create_voice() {
	if (mixer == nullptr || audio == nullptr)
		return nullptr;

	MIX_Track *track = MIX_CreateTrack(mixer);
	if (track == nullptr) {
		error(std::string("Failed to create sound track: ") +
			SDL_GetError());
	}
	if (!MIX_SetTrackAudio(track, audio)) {
		MIX_DestroyTrack(track);
		error(std::string("Failed to assign sound audio: ") +
			SDL_GetError());
	}
	MIX_SetTrackGain(track, volume);
	voices.push_back(Voice{ track, 0 });
	return &voices.back();
}

Sound::Voice *Sound::find_available_voice() {
	for (Voice &voice : voices) {
		if (voice.track != nullptr && !MIX_TrackPlaying(voice.track) &&
			!MIX_TrackPaused(voice.track)) {
			return &voice;
		}
	}

	if (voices.size() < static_cast<std::size_t>(max_instances))
		return create_voice();

	auto oldest = std::min_element(voices.begin(), voices.end(),
		voice_older);
	if (oldest == voices.end())
		return nullptr;
	MIX_StopTrack(oldest->track, 0);
	return &*oldest;
}

void Sound::play_voice(Voice &voice) {
	if (voice.track == nullptr)
		return;

	frame_position = start_position;
	MIX_SetTrackPlaybackPosition(voice.track, frame_position);
	MIX_SetTrackGain(voice.track, volume);
	if (!MIX_PlayTrack(voice.track, 0)) {
		error(std::string("Failed to play sound: ") + SDL_GetError());
	}
	voice.play_order = next_play_order++;
}

void Sound::destroy_voice(Voice &voice) {
	if (voice.track == nullptr)
		return;
	MIX_StopTrack(voice.track, 0);
	MIX_DestroyTrack(voice.track);
	voice.track = nullptr;
}

bool Sound::voice_older(const Voice &left, const Voice &right) {
	return left.play_order < right.play_order;
}
