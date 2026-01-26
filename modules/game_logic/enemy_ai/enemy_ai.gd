class_name EnemyAI
extends Node
## Enemy AI Module - Flexible behavior-based AI system
## Supports patrol, chase, attack, flee, and custom behaviors

signal state_changed(old_state: AIState, new_state: AIState)
signal target_detected(target: Node2D)
signal target_lost
signal attack_started
signal attack_finished
signal patrol_point_reached(index: int)

enum AIState { IDLE, PATROL, CHASE, ATTACK, FLEE, STUNNED, CUSTOM }

## Detection settings
@export_group("Detection")
@export var detection_range: float = 200.0
@export var attack_range: float = 50.0
@export var lose_interest_range: float = 400.0
@export var field_of_view: float = 180.0  # Degrees
@export var require_line_of_sight: bool = true

## Movement settings
@export_group("Movement")
@export var patrol_speed: float = 100.0
@export var chase_speed: float = 150.0
@export var flee_speed: float = 120.0

## Patrol settings
@export_group("Patrol")
@export var patrol_points: Array[Vector2] = []
@export var patrol_wait_time: float = 1.0
@export var loop_patrol: bool = true

## Attack settings
@export_group("Attack")
@export var attack_cooldown: float = 1.0
@export var attack_duration: float = 0.5
@export var attack_damage: int = 1

## Behavior settings
@export_group("Behavior")
@export var initial_state: AIState = AIState.IDLE
@export var can_chase: bool = true
@export var can_flee: bool = false
@export var flee_health_threshold: float = 0.25
@export var target_group: String = "player"

## State
var current_state: AIState = AIState.IDLE
var target: Node2D = null
var velocity: Vector2 = Vector2.ZERO

## Internal
var _owner_body: CharacterBody2D
var _current_patrol_index: int = 0
var _patrol_wait_timer: float = 0.0
var _attack_cooldown_timer: float = 0.0
var _stun_timer: float = 0.0
var _attack_timer: float = 0.0
var _facing_direction: int = 1


func _ready() -> void:
	_owner_body = get_parent() as CharacterBody2D
	if not _owner_body:
		push_warning("EnemyAI should be child of CharacterBody2D")

	_set_state(initial_state)


func _physics_process(delta: float) -> void:
	if not _owner_body:
		return

	_update_timers(delta)
	_detect_targets()
	_process_state(delta)
	_apply_movement(delta)


func _update_timers(delta: float) -> void:
	if _attack_cooldown_timer > 0:
		_attack_cooldown_timer -= delta

	if _stun_timer > 0:
		_stun_timer -= delta
		if _stun_timer <= 0:
			_set_state(AIState.IDLE)

	if _attack_timer > 0:
		_attack_timer -= delta
		if _attack_timer <= 0:
			attack_finished.emit()
			_set_state(AIState.CHASE if target else AIState.PATROL)


func _detect_targets() -> void:
	if current_state == AIState.STUNNED:
		return

	# Already have a target
	if target:
		if not is_instance_valid(target):
			_lose_target()
			return

		var distance = _owner_body.global_position.distance_to(target.global_position)

		# Lost target (too far)
		if distance > lose_interest_range:
			_lose_target()
			return

		# Check line of sight if required
		if require_line_of_sight and not _has_line_of_sight(target):
			_lose_target()
			return

		return

	# Look for new targets
	var potential_targets = get_tree().get_nodes_in_group(target_group)
	for potential in potential_targets:
		if _can_detect(potential):
			_acquire_target(potential)
			break


func _can_detect(potential_target: Node2D) -> bool:
	if not potential_target or not is_instance_valid(potential_target):
		return false

	var distance = _owner_body.global_position.distance_to(potential_target.global_position)

	# Too far
	if distance > detection_range:
		return false

	# Check field of view
	if field_of_view < 360:
		var direction_to_target = (potential_target.global_position - _owner_body.global_position).normalized()
		var facing_vector = Vector2(_facing_direction, 0)
		var angle = rad_to_deg(facing_vector.angle_to(direction_to_target))
		if abs(angle) > field_of_view / 2:
			return false

	# Check line of sight
	if require_line_of_sight and not _has_line_of_sight(potential_target):
		return false

	return true


func _has_line_of_sight(check_target: Node2D) -> bool:
	var space_state = _owner_body.get_world_2d().direct_space_state
	var query = PhysicsRayQueryParameters2D.create(
		_owner_body.global_position,
		check_target.global_position
	)
	query.exclude = [_owner_body]

	var result = space_state.intersect_ray(query)
	if result.is_empty():
		return true

	return result.collider == check_target or result.collider.is_in_group(target_group)


func _acquire_target(new_target: Node2D) -> void:
	target = new_target
	target_detected.emit(target)

	if can_chase:
		_set_state(AIState.CHASE)


func _lose_target() -> void:
	target = null
	target_lost.emit()

	if patrol_points.size() > 0:
		_set_state(AIState.PATROL)
	else:
		_set_state(AIState.IDLE)


func _set_state(new_state: AIState) -> void:
	if new_state == current_state:
		return

	var old_state = current_state
	current_state = new_state
	state_changed.emit(old_state, new_state)


func _process_state(delta: float) -> void:
	match current_state:
		AIState.IDLE:
			_process_idle(delta)
		AIState.PATROL:
			_process_patrol(delta)
		AIState.CHASE:
			_process_chase(delta)
		AIState.ATTACK:
			_process_attack(delta)
		AIState.FLEE:
			_process_flee(delta)
		AIState.STUNNED:
			velocity = Vector2.ZERO


func _process_idle(delta: float) -> void:
	velocity = Vector2.ZERO

	# Start patrol if we have points
	if patrol_points.size() > 0:
		_patrol_wait_timer -= delta
		if _patrol_wait_timer <= 0:
			_set_state(AIState.PATROL)


func _process_patrol(delta: float) -> void:
	if patrol_points.is_empty():
		_set_state(AIState.IDLE)
		return

	var target_point = patrol_points[_current_patrol_index]
	var direction = (target_point - _owner_body.global_position).normalized()
	var distance = _owner_body.global_position.distance_to(target_point)

	if distance < 10:
		# Reached patrol point
		patrol_point_reached.emit(_current_patrol_index)
		_current_patrol_index += 1

		if _current_patrol_index >= patrol_points.size():
			if loop_patrol:
				_current_patrol_index = 0
			else:
				_set_state(AIState.IDLE)
				return

		_patrol_wait_timer = patrol_wait_time
		_set_state(AIState.IDLE)
		return

	velocity = direction * patrol_speed
	_facing_direction = 1 if direction.x > 0 else -1


func _process_chase(delta: float) -> void:
	if not target or not is_instance_valid(target):
		_lose_target()
		return

	var direction = (target.global_position - _owner_body.global_position).normalized()
	var distance = _owner_body.global_position.distance_to(target.global_position)

	# Check if should flee
	if can_flee and _should_flee():
		_set_state(AIState.FLEE)
		return

	# In attack range
	if distance <= attack_range and _attack_cooldown_timer <= 0:
		_set_state(AIState.ATTACK)
		return

	velocity = direction * chase_speed
	_facing_direction = 1 if direction.x > 0 else -1


func _process_attack(delta: float) -> void:
	velocity = Vector2.ZERO

	if _attack_timer <= 0:
		# Start attack
		_attack_timer = attack_duration
		_attack_cooldown_timer = attack_cooldown
		attack_started.emit()
		_perform_attack()


func _perform_attack() -> void:
	# Override this or connect to attack_started signal
	if target and target.has_method("take_damage"):
		target.take_damage(attack_damage)


func _process_flee(delta: float) -> void:
	if not target or not is_instance_valid(target):
		_lose_target()
		return

	# Flee away from target
	var direction = (_owner_body.global_position - target.global_position).normalized()
	velocity = direction * flee_speed
	_facing_direction = 1 if direction.x > 0 else -1

	# Check if safe
	var distance = _owner_body.global_position.distance_to(target.global_position)
	if distance > lose_interest_range:
		_lose_target()


func _should_flee() -> bool:
	if not can_flee:
		return false

	# Check health if owner has it
	if _owner_body.has_method("get_health_percent"):
		return _owner_body.get_health_percent() < flee_health_threshold

	return false


func _apply_movement(_delta: float) -> void:
	if _owner_body:
		_owner_body.velocity.x = velocity.x
		# Don't override Y for platformers (gravity)


## Public API

## Set patrol waypoints
func set_patrol_points(points: Array[Vector2]) -> void:
	patrol_points = points
	_current_patrol_index = 0


## Add a patrol point
func add_patrol_point(point: Vector2) -> void:
	patrol_points.append(point)


## Manually set target
func set_target(new_target: Node2D) -> void:
	_acquire_target(new_target)


## Clear current target
func clear_target() -> void:
	_lose_target()


## Stun the AI
func stun(duration: float) -> void:
	_stun_timer = duration
	_set_state(AIState.STUNNED)


## Force a state change
func force_state(new_state: AIState) -> void:
	_set_state(new_state)


## Get current state name
func get_state_name() -> String:
	match current_state:
		AIState.IDLE: return "IDLE"
		AIState.PATROL: return "PATROL"
		AIState.CHASE: return "CHASE"
		AIState.ATTACK: return "ATTACK"
		AIState.FLEE: return "FLEE"
		AIState.STUNNED: return "STUNNED"
		_: return "UNKNOWN"


## Get facing direction
func get_facing_direction() -> int:
	return _facing_direction
