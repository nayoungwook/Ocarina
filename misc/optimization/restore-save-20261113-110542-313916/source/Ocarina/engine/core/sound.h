#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct MIX_Audio;
struct MIX_Mixer;
struct MIX_Track;

enum class SoundPlaybackMode {
	// Replaying restarts the same track. Intended for music and ambience.
	Single,
	// Replaying uses another voice so identical sound effects can overlap.
	Overlap,
};

class Sound {
public:
	explicit Sound(const std::string &path,
		SoundPlaybackMode playback_mode = SoundPlaybackMode::Single,
		int max_instances = 8);
	~Sound();
	Sound(const Sound &) = delete;
	Sound &operator=(const Sound &) = delete;

	void play();
	// Always requests a separate voice, regardless of the configured mode.
	void play_overlapping();
	void pause();
	void resume();
	void stop();
	void restart();
	void set_frame_pos(int position);

	void set_volume(float volume);
	float get_volume() const;
	void set_playback_mode(SoundPlaybackMode mode);
	SoundPlaybackMode get_playback_mode() const;
	void set_max_instances(int max_instances);
	int get_max_instances() const;
	int get_playing_instance_count() const;

	bool is_loaded() const;
	bool is_playing() const;
	bool is_paused() const;
	void dispose();

private:
	struct Voice {
		MIX_Track *track = nullptr;
		std::uint64_t play_order = 0;
	};

	static bool voice_older(const Voice &left, const Voice &right);
	Voice *create_voice();
	Voice *find_available_voice();
	void play_voice(Voice &voice);
	void destroy_voice(Voice &voice);

	std::string path;
	float volume = 1.0f;
	int start_position = 0;
	int frame_position = 0;
	int max_instances = 8;
	bool loaded = false;
	SoundPlaybackMode playback_mode = SoundPlaybackMode::Single;
	std::uint64_t next_play_order = 1;
	MIX_Mixer *mixer = nullptr;
	MIX_Audio *audio = nullptr;
	std::vector<Voice> voices;
};
