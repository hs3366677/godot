class_name PlayerMovement
extends Node
## Player Movement Module - Handles character movement, jumping, and physics.
## Designed as a modular component that can be attached to any CharacterBody2D.

signal jumped
signal landed
signal direction_changed(new_direction: int)

## Movement configuration
@export var speed: float = 300.0
@export var jump_force: float = -400.0
@export var gravity: float = 980.0
@export var acceleration: float = 2000.0
@export var friction: float = 1500.0
@export var air_control: float = 0.5

## Double jump configuration
@export var enable_double_jump: bool = false
@export var max_jumps: int = 2

## Wall jump configuration
@export var enable_wall_jump: bool = false
@export var wall_jump_force: Vector2 = Vector2(300, -350)

## Coyote time and jump buffer
@export var coyote_time: float = 0.1
@export var jump_buffer_time: float = 0.1

## Current state
var velocity: Vector2 = Vector2.ZERO
var is_grounded: bool = false
var facing_direction: int = 1  # 1 = right, -1 = left
var jump_count: int = 0

## Internal state
var _character: CharacterBody2D
var _was_grounded: bool = false
var _coyote_timer: float = 0.0
var _jump_buffer_timer: float = 0.0


func _ready() -> void:
	# Find the CharacterBody2D parent
	_character = get_parent() as CharacterBody2D
	if not _character:
		push_error("PlayerMovement must be a child of CharacterBody2D")


func _physics_process(delta: float) -> void:
	if not _character:
		return

	_update_timers(delta)
	_apply_gravity(delta)
	_handle_movement(delta)
	_handle_jump()
	_apply_movement()
	_update_state()


func _update_timers(delta: float) -> void:
	if _coyote_timer > 0:
		_coyote_timer -= delta
	if _jump_buffer_timer > 0:
		_jump_buffer_timer -= delta


func _apply_gravity(delta: float) -> void:
	if not _character.is_on_floor():
		velocity.y += gravity * delta


func _handle_movement(delta: float) -> void:
	var input_dir = Input.get_axis("move_left", "move_right")

	# Update facing direction
	if input_dir != 0:
		var new_direction = sign(input_dir) as int
		if new_direction != facing_direction:
			facing_direction = new_direction
			direction_changed.emit(facing_direction)

	# Apply movement with acceleration/friction
	var control = 1.0 if is_grounded else air_control

	if input_dir != 0:
		velocity.x = move_toward(velocity.x, input_dir * speed, acceleration * control * delta)
	else:
		velocity.x = move_toward(velocity.x, 0, friction * control * delta)


func _handle_jump() -> void:
	# Buffer jump input
	if Input.is_action_just_pressed("jump"):
		_jump_buffer_timer = jump_buffer_time

	# Can jump if: on floor, in coyote time, or has jumps remaining (double jump)
	var can_jump = is_grounded or _coyote_timer > 0 or (enable_double_jump and jump_count < max_jumps)

	if _jump_buffer_timer > 0 and can_jump:
		_perform_jump()
		_jump_buffer_timer = 0
		_coyote_timer = 0

	# Wall jump
	if enable_wall_jump and Input.is_action_just_pressed("jump"):
		if _character.is_on_wall() and not is_grounded:
			_perform_wall_jump()


func _perform_jump() -> void:
	velocity.y = jump_force
	jump_count += 1
	jumped.emit()


func _perform_wall_jump() -> void:
	var wall_normal = _character.get_wall_normal()
	velocity.x = wall_normal.x * wall_jump_force.x
	velocity.y = wall_jump_force.y
	facing_direction = sign(wall_normal.x) as int
	direction_changed.emit(facing_direction)
	jumped.emit()


func _apply_movement() -> void:
	_character.velocity = velocity
	_character.move_and_slide()
	velocity = _character.velocity


func _update_state() -> void:
	_was_grounded = is_grounded
	is_grounded = _character.is_on_floor()

	# Start coyote time when leaving ground
	if _was_grounded and not is_grounded:
		_coyote_timer = coyote_time

	# Reset jump count and emit landed signal
	if is_grounded and not _was_grounded:
		jump_count = 0
		landed.emit()


## Public API

## Set the movement speed
func set_speed(new_speed: float) -> void:
	speed = new_speed


## Set the jump force (negative value for upward jump)
func set_jump_force(force: float) -> void:
	jump_force = force


## Enable or disable double jump
func set_double_jump(enabled: bool, jumps: int = 2) -> void:
	enable_double_jump = enabled
	max_jumps = jumps


## Enable or disable wall jump
func set_wall_jump(enabled: bool) -> void:
	enable_wall_jump = enabled


## Get the current velocity
func get_velocity() -> Vector2:
	return velocity


## Check if the player is on the ground
func is_on_ground() -> bool:
	return is_grounded


## Get the current facing direction
func get_facing_direction() -> int:
	return facing_direction


## Apply an external impulse to the character
func apply_impulse(impulse: Vector2) -> void:
	velocity += impulse


## Reset the character state
func reset() -> void:
	velocity = Vector2.ZERO
	jump_count = 0
	_coyote_timer = 0
	_jump_buffer_timer = 0
