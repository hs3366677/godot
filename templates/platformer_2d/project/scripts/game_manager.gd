extends Node
## Game Manager for 2D Platformer Template
## Handles game state, scoring, lives, and level transitions

signal game_started
signal game_over
signal game_paused(is_paused: bool)
signal level_completed(level: int)
signal score_changed(new_score: int)
signal lives_changed(new_lives: int)

## Game settings
@export var starting_lives: int = 3
@export var points_per_coin: int = 100

## Current game state
var score: int = 0
var lives: int = 3
var current_level: int = 1
var is_paused: bool = false
var is_game_over: bool = false

## Level paths
var levels: Array[String] = [
	"res://scenes/levels/level_01.tscn",
	"res://scenes/levels/level_02.tscn",
	"res://scenes/levels/level_03.tscn"
]

## Checkpoint system
var last_checkpoint: Vector2 = Vector2.ZERO
var has_checkpoint: bool = false


func _ready() -> void:
	lives = starting_lives
	process_mode = Node.PROCESS_MODE_ALWAYS


func _input(event: InputEvent) -> void:
	if event.is_action_pressed("pause"):
		toggle_pause()


## Start a new game
func start_game() -> void:
	score = 0
	lives = starting_lives
	current_level = 1
	is_game_over = false
	has_checkpoint = false

	score_changed.emit(score)
	lives_changed.emit(lives)
	game_started.emit()

	load_level(current_level)


## Add points to score
func add_score(points: int) -> void:
	score += points
	score_changed.emit(score)


## Called when player collects a coin
func on_coin_collected() -> void:
	add_score(points_per_coin)


## Called when player dies
func on_player_died() -> void:
	lives -= 1
	lives_changed.emit(lives)

	if lives <= 0:
		_trigger_game_over()
	else:
		# Respawn at checkpoint or level start
		await get_tree().create_timer(1.0).timeout
		respawn_player()


## Respawn the player
func respawn_player() -> void:
	var player = get_tree().get_first_node_in_group("player")
	if player and player.has_method("respawn"):
		var spawn_pos = last_checkpoint if has_checkpoint else _get_level_spawn()
		player.respawn(spawn_pos)


func _get_level_spawn() -> Vector2:
	var spawn = get_tree().get_first_node_in_group("spawn_point")
	if spawn:
		return spawn.global_position
	return Vector2(100, 100)


## Set checkpoint
func set_checkpoint(position: Vector2) -> void:
	last_checkpoint = position
	has_checkpoint = true


## Complete current level
func complete_level() -> void:
	level_completed.emit(current_level)
	current_level += 1

	if current_level <= levels.size():
		await get_tree().create_timer(1.0).timeout
		load_level(current_level)
	else:
		# Game complete!
		_trigger_game_complete()


## Load a specific level
func load_level(level_num: int) -> void:
	if level_num < 1 or level_num > levels.size():
		push_error("Invalid level number: ", level_num)
		return

	has_checkpoint = false
	var level_path = levels[level_num - 1]
	get_tree().change_scene_to_file(level_path)


## Toggle pause state
func toggle_pause() -> void:
	if is_game_over:
		return

	is_paused = not is_paused
	get_tree().paused = is_paused
	game_paused.emit(is_paused)


## Trigger game over
func _trigger_game_over() -> void:
	is_game_over = true
	game_over.emit()


## Trigger game complete (all levels finished)
func _trigger_game_complete() -> void:
	print("Congratulations! Game Complete!")
	print("Final Score: ", score)
	# Could show a victory screen here


## Restart the current level
func restart_level() -> void:
	has_checkpoint = false
	load_level(current_level)


## Return to main menu
func return_to_menu() -> void:
	is_paused = false
	get_tree().paused = false
	get_tree().change_scene_to_file("res://scenes/main_menu.tscn")
