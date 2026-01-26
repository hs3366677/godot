class_name AudioManager
extends Node
## Audio Manager Module - Centralized audio management
## Handles music playback, sound effects, volume control, and audio pooling

signal music_started(track_path: String)
signal music_stopped
signal music_finished
signal volume_changed(bus: String, volume: float)

## Volume settings
@export_group("Volume")
@export_range(0.0, 1.0) var master_volume: float = 1.0:
	set(value):
		master_volume = value
		_apply_volume("Master", value)
@export_range(0.0, 1.0) var music_volume: float = 0.8:
	set(value):
		music_volume = value
		_apply_volume("Music", value)
@export_range(0.0, 1.0) var sfx_volume: float = 1.0:
	set(value):
		sfx_volume = value
		_apply_volume("SFX", value)

## Pool settings
@export_group("Pooling")
@export var sfx_pool_size: int = 8
@export var sfx_2d_pool_size: int = 4

## Music settings
@export_group("Music")
@export var default_fade_time: float = 1.0
@export var music_loop: bool = true

## Audio players
var _music_player: AudioStreamPlayer
var _sfx_pool: Array[AudioStreamPlayer] = []
var _sfx_2d_pool: Array[AudioStreamPlayer2D] = []
var _current_sfx_index: int = 0
var _current_sfx_2d_index: int = 0

## State
var current_music_path: String = ""
var is_music_playing: bool = false

## Fade tween
var _fade_tween: Tween

## Singleton
static var instance: AudioManager


func _ready() -> void:
	instance = self
	add_to_group("audio_manager")
	_setup_audio_buses()
	_create_music_player()
	_create_sfx_pool()
	_apply_all_volumes()


func _setup_audio_buses() -> void:
	# Ensure audio buses exist
	# Note: In a real project, you'd set up buses in the Audio Bus Layout
	var master_idx = AudioServer.get_bus_index("Master")
	if master_idx == -1:
		push_warning("Master bus not found")


func _create_music_player() -> void:
	_music_player = AudioStreamPlayer.new()
	_music_player.name = "MusicPlayer"
	_music_player.bus = "Music" if AudioServer.get_bus_index("Music") != -1 else "Master"
	_music_player.finished.connect(_on_music_finished)
	add_child(_music_player)


func _create_sfx_pool() -> void:
	# Create regular SFX pool
	for i in sfx_pool_size:
		var player = AudioStreamPlayer.new()
		player.name = "SFXPlayer_%d" % i
		player.bus = "SFX" if AudioServer.get_bus_index("SFX") != -1 else "Master"
		add_child(player)
		_sfx_pool.append(player)

	# Create 2D SFX pool for positional audio
	for i in sfx_2d_pool_size:
		var player = AudioStreamPlayer2D.new()
		player.name = "SFX2DPlayer_%d" % i
		player.bus = "SFX" if AudioServer.get_bus_index("SFX") != -1 else "Master"
		add_child(player)
		_sfx_2d_pool.append(player)


func _apply_all_volumes() -> void:
	_apply_volume("Master", master_volume)
	_apply_volume("Music", music_volume)
	_apply_volume("SFX", sfx_volume)


func _apply_volume(bus_name: String, volume: float) -> void:
	var bus_idx = AudioServer.get_bus_index(bus_name)
	if bus_idx != -1:
		AudioServer.set_bus_volume_db(bus_idx, linear_to_db(volume))
		volume_changed.emit(bus_name, volume)


## Play background music
func play_music(path: String, fade_time: float = -1.0) -> void:
	if fade_time < 0:
		fade_time = default_fade_time

	var stream = load(path) as AudioStream
	if not stream:
		push_error("Failed to load music: " + path)
		return

	# Fade out current music if playing
	if is_music_playing and fade_time > 0:
		await _fade_out_music(fade_time * 0.5)

	current_music_path = path
	_music_player.stream = stream
	_music_player.volume_db = linear_to_db(0.0) if fade_time > 0 else 0.0
	_music_player.play()
	is_music_playing = true

	# Fade in
	if fade_time > 0:
		_fade_in_music(fade_time * 0.5)

	music_started.emit(path)


## Stop music
func stop_music(fade_time: float = -1.0) -> void:
	if not is_music_playing:
		return

	if fade_time < 0:
		fade_time = default_fade_time

	if fade_time > 0:
		await _fade_out_music(fade_time)

	_music_player.stop()
	is_music_playing = false
	current_music_path = ""
	music_stopped.emit()


## Pause music
func pause_music() -> void:
	_music_player.stream_paused = true


## Resume music
func resume_music() -> void:
	_music_player.stream_paused = false


func _fade_in_music(duration: float) -> void:
	if _fade_tween:
		_fade_tween.kill()
	_fade_tween = create_tween()
	_fade_tween.tween_property(_music_player, "volume_db", 0.0, duration)


func _fade_out_music(duration: float) -> void:
	if _fade_tween:
		_fade_tween.kill()
	_fade_tween = create_tween()
	_fade_tween.tween_property(_music_player, "volume_db", -80.0, duration)
	await _fade_tween.finished


func _on_music_finished() -> void:
	if music_loop and current_music_path:
		_music_player.play()
	else:
		is_music_playing = false
		music_finished.emit()


## Play a sound effect (returns the player for further control)
func play_sfx(path: String, volume_db: float = 0.0, pitch: float = 1.0) -> AudioStreamPlayer:
	var stream = load(path) as AudioStream
	if not stream:
		push_error("Failed to load SFX: " + path)
		return null

	var player = _get_available_sfx_player()
	player.stream = stream
	player.volume_db = volume_db
	player.pitch_scale = pitch
	player.play()

	return player


## Play sound effect with random pitch variation
func play_sfx_varied(path: String, pitch_variation: float = 0.1) -> AudioStreamPlayer:
	var pitch = randf_range(1.0 - pitch_variation, 1.0 + pitch_variation)
	return play_sfx(path, 0.0, pitch)


## Play positional sound effect (2D)
func play_sfx_at(path: String, position: Vector2, volume_db: float = 0.0) -> AudioStreamPlayer2D:
	var stream = load(path) as AudioStream
	if not stream:
		push_error("Failed to load SFX: " + path)
		return null

	var player = _get_available_sfx_2d_player()
	player.stream = stream
	player.volume_db = volume_db
	player.global_position = position
	player.play()

	return player


func _get_available_sfx_player() -> AudioStreamPlayer:
	# Round-robin through pool
	var player = _sfx_pool[_current_sfx_index]
	_current_sfx_index = (_current_sfx_index + 1) % _sfx_pool.size()

	# Stop if still playing (oldest sound)
	if player.playing:
		player.stop()

	return player


func _get_available_sfx_2d_player() -> AudioStreamPlayer2D:
	var player = _sfx_2d_pool[_current_sfx_2d_index]
	_current_sfx_2d_index = (_current_sfx_2d_index + 1) % _sfx_2d_pool.size()

	if player.playing:
		player.stop()

	return player


## Volume setters
func set_master_volume(volume: float) -> void:
	master_volume = clamp(volume, 0.0, 1.0)


func set_music_volume(volume: float) -> void:
	music_volume = clamp(volume, 0.0, 1.0)


func set_sfx_volume(volume: float) -> void:
	sfx_volume = clamp(volume, 0.0, 1.0)


## Get volumes
func get_master_volume() -> float:
	return master_volume


func get_music_volume() -> float:
	return music_volume


func get_sfx_volume() -> float:
	return sfx_volume


## Mute/unmute
func mute_all() -> void:
	AudioServer.set_bus_mute(AudioServer.get_bus_index("Master"), true)


func unmute_all() -> void:
	AudioServer.set_bus_mute(AudioServer.get_bus_index("Master"), false)


func is_muted() -> bool:
	return AudioServer.is_bus_mute(AudioServer.get_bus_index("Master"))
