extends CharacterBody2D
## Player Controller for 2D Platformer Template
## Handles movement, jumping, animations, and interactions

signal died
signal coin_collected(total: int)
signal health_changed(current: int, max_health: int)

## Movement settings
@export_group("Movement")
@export var speed: float = 300.0
@export var acceleration: float = 2000.0
@export var friction: float = 1500.0
@export var air_control: float = 0.5

## Jump settings
@export_group("Jumping")
@export var jump_force: float = -400.0
@export var gravity: float = 980.0
@export var max_jumps: int = 1
@export var coyote_time: float = 0.1
@export var jump_buffer_time: float = 0.1

## Player state
var coins: int = 0
var health: int = 3
var max_health: int = 3
var is_dead: bool = false

## Internal state
var _jump_count: int = 0
var _coyote_timer: float = 0.0
var _jump_buffer_timer: float = 0.0
var _was_on_floor: bool = false
var _facing_right: bool = true

@onready var sprite: AnimatedSprite2D = $AnimatedSprite2D
@onready var collision: CollisionShape2D = $CollisionShape2D


func _physics_process(delta: float) -> void:
	if is_dead:
		return

	_update_timers(delta)
	_apply_gravity(delta)
	_handle_movement(delta)
	_handle_jump()
	_update_animation()

	move_and_slide()
	_update_state()


func _update_timers(delta: float) -> void:
	_coyote_timer = max(0, _coyote_timer - delta)
	_jump_buffer_timer = max(0, _jump_buffer_timer - delta)


func _apply_gravity(delta: float) -> void:
	if not is_on_floor():
		velocity.y += gravity * delta


func _handle_movement(delta: float) -> void:
	var input_dir = Input.get_axis("move_left", "move_right")
	var control = 1.0 if is_on_floor() else air_control

	if input_dir != 0:
		velocity.x = move_toward(velocity.x, input_dir * speed, acceleration * control * delta)
		_facing_right = input_dir > 0
	else:
		velocity.x = move_toward(velocity.x, 0, friction * control * delta)


func _handle_jump() -> void:
	# Buffer jump input
	if Input.is_action_just_pressed("jump"):
		_jump_buffer_timer = jump_buffer_time

	# Can jump check
	var can_jump = is_on_floor() or _coyote_timer > 0 or _jump_count < max_jumps

	if _jump_buffer_timer > 0 and can_jump:
		velocity.y = jump_force
		_jump_count += 1
		_jump_buffer_timer = 0
		_coyote_timer = 0

	# Variable jump height
	if Input.is_action_just_released("jump") and velocity.y < 0:
		velocity.y *= 0.5


func _update_animation() -> void:
	if sprite:
		sprite.flip_h = not _facing_right

		if not is_on_floor():
			sprite.play("jump")
		elif abs(velocity.x) > 10:
			sprite.play("run")
		else:
			sprite.play("idle")


func _update_state() -> void:
	# Coyote time
	if _was_on_floor and not is_on_floor():
		_coyote_timer = coyote_time

	# Reset jump count on landing
	if is_on_floor() and not _was_on_floor:
		_jump_count = 0

	_was_on_floor = is_on_floor()


## Public API

func collect_coin() -> void:
	coins += 1
	coin_collected.emit(coins)


func take_damage(amount: int = 1) -> void:
	if is_dead:
		return

	health = max(0, health - amount)
	health_changed.emit(health, max_health)

	if health <= 0:
		die()
	else:
		# Knockback and invincibility frames would go here
		pass


func heal(amount: int = 1) -> void:
	health = min(max_health, health + amount)
	health_changed.emit(health, max_health)


func die() -> void:
	is_dead = true
	velocity = Vector2.ZERO
	if sprite:
		sprite.play("die")
	died.emit()


func respawn(position: Vector2) -> void:
	global_position = position
	health = max_health
	is_dead = false
	velocity = Vector2.ZERO
	health_changed.emit(health, max_health)
