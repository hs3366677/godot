extends CharacterBody2D
class_name EnemyBase
## Base class for all enemies in the platformer template
## Extend this class to create custom enemy behaviors

signal died

## Enemy settings
@export_group("Movement")
@export var speed: float = 100.0
@export var gravity: float = 980.0

@export_group("Combat")
@export var health: int = 1
@export var damage: int = 1
@export var score_value: int = 100

@export_group("Behavior")
@export var patrol_enabled: bool = true
@export var patrol_distance: float = 100.0
@export var detect_player: bool = false
@export var detection_range: float = 200.0

## State
var is_dead: bool = false
var facing_right: bool = true
var _start_position: Vector2
var _patrol_direction: int = 1

@onready var sprite: AnimatedSprite2D = $AnimatedSprite2D
@onready var collision: CollisionShape2D = $CollisionShape2D


func _ready() -> void:
	_start_position = global_position
	add_to_group("enemies")


func _physics_process(delta: float) -> void:
	if is_dead:
		return

	_apply_gravity(delta)
	_update_behavior(delta)
	_update_animation()

	move_and_slide()
	_check_collisions()


func _apply_gravity(delta: float) -> void:
	if not is_on_floor():
		velocity.y += gravity * delta


func _update_behavior(delta: float) -> void:
	if detect_player and _player_in_range():
		_chase_player(delta)
	elif patrol_enabled:
		_patrol(delta)


func _patrol(delta: float) -> void:
	# Move in patrol direction
	velocity.x = speed * _patrol_direction

	# Check if we've reached patrol bounds
	var distance_from_start = global_position.x - _start_position.x
	if abs(distance_from_start) >= patrol_distance:
		_patrol_direction *= -1
		facing_right = _patrol_direction > 0

	# Turn around at walls or edges
	if is_on_wall():
		_patrol_direction *= -1
		facing_right = _patrol_direction > 0


func _chase_player(delta: float) -> void:
	var player = get_tree().get_first_node_in_group("player")
	if not player:
		return

	var direction = sign(player.global_position.x - global_position.x)
	velocity.x = speed * direction * 1.5  # Move faster when chasing
	facing_right = direction > 0


func _player_in_range() -> bool:
	var player = get_tree().get_first_node_in_group("player")
	if not player:
		return false

	return global_position.distance_to(player.global_position) <= detection_range


func _update_animation() -> void:
	if sprite:
		sprite.flip_h = not facing_right

		if not is_on_floor():
			sprite.play("jump") if sprite.sprite_frames.has_animation("jump") else sprite.play("walk")
		elif abs(velocity.x) > 10:
			sprite.play("walk")
		else:
			sprite.play("idle")


func _check_collisions() -> void:
	for i in get_slide_collision_count():
		var collision_info = get_slide_collision(i)
		var collider = collision_info.get_collider()

		if collider.is_in_group("player"):
			_on_player_collision(collider, collision_info)


func _on_player_collision(player: Node, collision_info: KinematicCollision2D) -> void:
	# Check if player is jumping on top of enemy
	var collision_normal = collision_info.get_normal()

	if collision_normal.y > 0.5:  # Player landed on enemy from above
		take_damage(1)
		# Bounce the player
		if player.has_method("apply_impulse"):
			player.apply_impulse(Vector2(0, -300))
		elif "velocity" in player:
			player.velocity.y = -300
	else:
		# Enemy damages player
		if player.has_method("take_damage"):
			player.take_damage(damage)


func take_damage(amount: int = 1) -> void:
	health -= amount

	if health <= 0:
		die()
	else:
		# Flash or knockback effect
		_on_hurt()


func _on_hurt() -> void:
	# Override for hurt effects
	if sprite:
		sprite.modulate = Color.RED
		await get_tree().create_timer(0.1).timeout
		sprite.modulate = Color.WHITE


func die() -> void:
	is_dead = true
	velocity = Vector2.ZERO

	# Award points
	var game_manager = get_tree().get_first_node_in_group("game_manager")
	if game_manager and game_manager.has_method("add_score"):
		game_manager.add_score(score_value)

	died.emit()

	# Death animation or just remove
	if sprite and sprite.sprite_frames.has_animation("die"):
		sprite.play("die")
		await sprite.animation_finished

	queue_free()
