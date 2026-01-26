class_name GameManager
extends Node
## Game Manager Module - Central game state management
## Handles game lifecycle, scoring, lives, and scene transitions

signal game_started
signal game_paused(is_paused: bool)
signal game_over
signal game_won
signal score_changed(new_score: int)
signal lives_changed(new_lives: int)
signal state_changed(new_state: GameState)

enum GameState { MENU, PLAYING, PAUSED, GAME_OVER, VICTORY }

## Configuration
@export_group("Game Settings")
@export var starting_lives: int = 3
@export var starting_score: int = 0

@export_group("Scenes")
@export var main_menu_scene: String = "res://scenes/main_menu.tscn"
@export var first_level_scene: String = ""
@export var game_over_scene: String = ""
@export var victory_scene: String = ""

## Current state
var current_state: GameState = GameState.MENU
var score: int = 0
var lives: int = 3
var current_level: int = 0
var levels: Array[String] = []

## Singleton pattern
static var instance: GameManager


func _ready() -> void:
	instance = self
	add_to_group("game_manager")
	process_mode = Node.PROCESS_MODE_ALWAYS
	_reset_stats()


func _input(event: InputEvent) -> void:
	if event.is_action_pressed("pause") and current_state == GameState.PLAYING:
		toggle_pause()
	elif event.is_action_pressed("pause") and current_state == GameState.PAUSED:
		toggle_pause()


## Reset all stats to starting values
func _reset_stats() -> void:
	score = starting_score
	lives = starting_lives


## Change game state
func _set_state(new_state: GameState) -> void:
	if current_state == new_state:
		return

	current_state = new_state
	state_changed.emit(new_state)

	match new_state:
		GameState.PLAYING:
			get_tree().paused = false
		GameState.PAUSED:
			get_tree().paused = true
		GameState.GAME_OVER:
			get_tree().paused = true
		GameState.VICTORY:
			get_tree().paused = true


## Start a new game
func start_game() -> void:
	_reset_stats()
	current_level = 0
	_set_state(GameState.PLAYING)

	score_changed.emit(score)
	lives_changed.emit(lives)
	game_started.emit()

	if first_level_scene and not first_level_scene.is_empty():
		load_scene(first_level_scene)
	elif levels.size() > 0:
		load_level(0)


## Toggle pause state
func toggle_pause() -> void:
	if current_state == GameState.PLAYING:
		_set_state(GameState.PAUSED)
		game_paused.emit(true)
	elif current_state == GameState.PAUSED:
		_set_state(GameState.PLAYING)
		game_paused.emit(false)


## Pause the game
func pause_game() -> void:
	if current_state == GameState.PLAYING:
		_set_state(GameState.PAUSED)
		game_paused.emit(true)


## Resume the game
func resume_game() -> void:
	if current_state == GameState.PAUSED:
		_set_state(GameState.PLAYING)
		game_paused.emit(false)


## Add score
func add_score(amount: int) -> void:
	score += amount
	score_changed.emit(score)


## Set score directly
func set_score(new_score: int) -> void:
	score = new_score
	score_changed.emit(score)


## Lose a life
func lose_life() -> void:
	lives -= 1
	lives_changed.emit(lives)

	if lives <= 0:
		trigger_game_over()


## Add a life
func add_life() -> void:
	lives += 1
	lives_changed.emit(lives)


## Trigger game over
func trigger_game_over() -> void:
	_set_state(GameState.GAME_OVER)
	game_over.emit()

	if game_over_scene and not game_over_scene.is_empty():
		await get_tree().create_timer(1.5).timeout
		load_scene(game_over_scene)


## Trigger victory
func trigger_victory() -> void:
	_set_state(GameState.VICTORY)
	game_won.emit()

	if victory_scene and not victory_scene.is_empty():
		await get_tree().create_timer(1.5).timeout
		load_scene(victory_scene)


## Restart current level
func restart_level() -> void:
	_set_state(GameState.PLAYING)
	get_tree().reload_current_scene()


## Load next level
func next_level() -> void:
	current_level += 1
	if current_level < levels.size():
		load_level(current_level)
	else:
		trigger_victory()


## Load specific level by index
func load_level(level_index: int) -> void:
	if level_index >= 0 and level_index < levels.size():
		current_level = level_index
		load_scene(levels[level_index])


## Load a scene by path
func load_scene(scene_path: String) -> void:
	get_tree().change_scene_to_file(scene_path)


## Return to main menu
func quit_to_menu() -> void:
	_set_state(GameState.MENU)
	get_tree().paused = false
	if main_menu_scene and not main_menu_scene.is_empty():
		load_scene(main_menu_scene)


## Quit the game
func quit_game() -> void:
	get_tree().quit()


## Register levels
func register_levels(level_paths: Array[String]) -> void:
	levels = level_paths


## Get current state as string
func get_state_name() -> String:
	match current_state:
		GameState.MENU: return "MENU"
		GameState.PLAYING: return "PLAYING"
		GameState.PAUSED: return "PAUSED"
		GameState.GAME_OVER: return "GAME_OVER"
		GameState.VICTORY: return "VICTORY"
		_: return "UNKNOWN"
