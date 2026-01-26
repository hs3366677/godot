class_name SaveSystem
extends Node
## Save System Module - Game save/load functionality
## Supports multiple save slots, auto-save, and encrypted saves

signal game_saved(slot: int)
signal game_loaded(slot: int)
signal save_deleted(slot: int)
signal auto_save_triggered

## Configuration
@export_group("Save Settings")
@export var save_directory: String = "user://saves/"
@export var save_extension: String = ".sav"
@export var max_slots: int = 3

@export_group("Auto Save")
@export var auto_save_enabled: bool = true
@export var auto_save_interval: float = 300.0  # 5 minutes
@export var auto_save_slot: int = 0

@export_group("Security")
@export var encrypt_saves: bool = false
@export var encryption_key: String = "your_secret_key_here"

## State
var _auto_save_timer: float = 0.0

## Singleton
static var instance: SaveSystem


func _ready() -> void:
	instance = self
	add_to_group("save_system")
	_ensure_save_directory()


func _process(delta: float) -> void:
	if auto_save_enabled:
		_auto_save_timer += delta
		if _auto_save_timer >= auto_save_interval:
			_auto_save_timer = 0.0
			_trigger_auto_save()


func _ensure_save_directory() -> void:
	if not DirAccess.dir_exists_absolute(save_directory):
		DirAccess.make_dir_recursive_absolute(save_directory)


## Save game data to a slot
func save_game(slot: int, data: Dictionary) -> bool:
	if slot < 0 or slot >= max_slots:
		push_error("Invalid save slot: " + str(slot))
		return false

	# Add metadata
	var save_data = {
		"metadata": {
			"slot": slot,
			"timestamp": Time.get_datetime_string_from_system(),
			"version": ProjectSettings.get_setting("application/config/version", "1.0"),
			"playtime": data.get("playtime", 0)
		},
		"game_data": data
	}

	var file_path = _get_save_path(slot)
	var success = false

	if encrypt_saves:
		success = _save_encrypted(file_path, save_data)
	else:
		success = _save_json(file_path, save_data)

	if success:
		game_saved.emit(slot)

	return success


func _save_json(path: String, data: Dictionary) -> bool:
	var file = FileAccess.open(path, FileAccess.WRITE)
	if not file:
		push_error("Failed to open save file: " + path)
		return false

	file.store_string(JSON.stringify(data, "\t"))
	file.close()
	return true


func _save_encrypted(path: String, data: Dictionary) -> bool:
	var file = FileAccess.open_encrypted_with_pass(path, FileAccess.WRITE, encryption_key)
	if not file:
		push_error("Failed to open encrypted save file: " + path)
		return false

	file.store_string(JSON.stringify(data))
	file.close()
	return true


## Load game data from a slot
func load_game(slot: int) -> Dictionary:
	if slot < 0 or slot >= max_slots:
		push_error("Invalid save slot: " + str(slot))
		return {}

	if not has_save(slot):
		push_warning("No save data in slot: " + str(slot))
		return {}

	var file_path = _get_save_path(slot)
	var data: Dictionary

	if encrypt_saves:
		data = _load_encrypted(file_path)
	else:
		data = _load_json(file_path)

	if not data.is_empty():
		game_loaded.emit(slot)
		return data.get("game_data", {})

	return {}


func _load_json(path: String) -> Dictionary:
	var file = FileAccess.open(path, FileAccess.READ)
	if not file:
		push_error("Failed to open save file: " + path)
		return {}

	var json = JSON.new()
	var error = json.parse(file.get_as_text())
	file.close()

	if error != OK:
		push_error("Failed to parse save file: " + path)
		return {}

	return json.data


func _load_encrypted(path: String) -> Dictionary:
	var file = FileAccess.open_encrypted_with_pass(path, FileAccess.READ, encryption_key)
	if not file:
		push_error("Failed to open encrypted save file: " + path)
		return {}

	var json = JSON.new()
	var error = json.parse(file.get_as_text())
	file.close()

	if error != OK:
		push_error("Failed to parse encrypted save file: " + path)
		return {}

	return json.data


## Delete a save slot
func delete_save(slot: int) -> bool:
	if slot < 0 or slot >= max_slots:
		push_error("Invalid save slot: " + str(slot))
		return false

	var file_path = _get_save_path(slot)
	if FileAccess.file_exists(file_path):
		var err = DirAccess.remove_absolute(file_path)
		if err == OK:
			save_deleted.emit(slot)
			return true
		else:
			push_error("Failed to delete save file: " + str(err))
			return false

	return true  # Already doesn't exist


## Check if a slot has save data
func has_save(slot: int) -> bool:
	if slot < 0 or slot >= max_slots:
		return false
	return FileAccess.file_exists(_get_save_path(slot))


## Check if any save data exists
func has_any_save() -> bool:
	for i in max_slots:
		if has_save(i):
			return true
	return false


## Get metadata for a save slot
func get_save_info(slot: int) -> Dictionary:
	if not has_save(slot):
		return {"exists": false, "slot": slot}

	var file_path = _get_save_path(slot)
	var data: Dictionary

	if encrypt_saves:
		data = _load_encrypted(file_path)
	else:
		data = _load_json(file_path)

	if data.is_empty():
		return {"exists": false, "slot": slot}

	var metadata = data.get("metadata", {})
	metadata["exists"] = true
	return metadata


## Get all save slots info
func get_all_save_info() -> Array[Dictionary]:
	var slots: Array[Dictionary] = []
	for i in max_slots:
		slots.append(get_save_info(i))
	return slots


## Get the file path for a slot
func _get_save_path(slot: int) -> String:
	return save_directory + "slot_" + str(slot) + save_extension


## Auto-save handling
func _trigger_auto_save() -> void:
	auto_save_triggered.emit()
	# The game should connect to this signal and call save_game with current data


## Quick save/load (uses slot 0)
func quick_save(data: Dictionary) -> bool:
	return save_game(0, data)


func quick_load() -> Dictionary:
	return load_game(0)


## Settings save (separate from game saves)
func save_settings(settings: Dictionary) -> bool:
	var path = save_directory + "settings.cfg"
	return _save_json(path, settings)


func load_settings() -> Dictionary:
	var path = save_directory + "settings.cfg"
	if not FileAccess.file_exists(path):
		return {}
	return _load_json(path)


## Create save data from current game state
## Override this in your game to collect save data
func create_save_data() -> Dictionary:
	var data = {}

	# Collect from game manager
	var game_manager = get_tree().get_first_node_in_group("game_manager")
	if game_manager:
		data["score"] = game_manager.get("score") if "score" in game_manager else 0
		data["lives"] = game_manager.get("lives") if "lives" in game_manager else 3
		data["current_level"] = game_manager.get("current_level") if "current_level" in game_manager else 0

	# Collect player position
	var player = get_tree().get_first_node_in_group("player")
	if player:
		data["player_position"] = {
			"x": player.global_position.x,
			"y": player.global_position.y
		}

	return data


## Apply loaded save data to game
## Override this in your game to apply save data
func apply_save_data(data: Dictionary) -> void:
	var game_manager = get_tree().get_first_node_in_group("game_manager")
	if game_manager:
		if "score" in data:
			game_manager.set("score", data.score)
		if "lives" in data:
			game_manager.set("lives", data.lives)

	var player = get_tree().get_first_node_in_group("player")
	if player and "player_position" in data:
		player.global_position = Vector2(
			data.player_position.x,
			data.player_position.y
		)
