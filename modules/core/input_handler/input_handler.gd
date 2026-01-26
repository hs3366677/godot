class_name InputHandler
extends Node
## Input Handler Module - Centralized input processing
## Supports keyboard, gamepad, and touch input with rebinding

signal input_scheme_changed(scheme: String)
signal action_pressed(action: String)
signal action_released(action: String)

enum InputScheme { KEYBOARD, GAMEPAD, TOUCH }

## Configuration
@export var deadzone: float = 0.2
@export var current_scheme: InputScheme = InputScheme.KEYBOARD

## Movement action names
@export_group("Movement Actions")
@export var move_left_action: String = "move_left"
@export var move_right_action: String = "move_right"
@export var move_up_action: String = "move_up"
@export var move_down_action: String = "move_down"

## Common action names
@export_group("Common Actions")
@export var jump_action: String = "jump"
@export var attack_action: String = "attack"
@export var interact_action: String = "interact"
@export var pause_action: String = "pause"

## Default bindings for rebinding
var default_bindings: Dictionary = {}

## Touch input state
var _touch_movement: Vector2 = Vector2.ZERO
var _touch_actions: Dictionary = {}

## Singleton
static var instance: InputHandler


func _ready() -> void:
	instance = self
	add_to_group("input_handler")
	_save_default_bindings()
	_detect_input_scheme()


func _input(event: InputEvent) -> void:
	_detect_input_change(event)
	_emit_action_signals(event)


func _detect_input_change(event: InputEvent) -> void:
	var new_scheme = current_scheme

	if event is InputEventKey or event is InputEventMouseButton:
		new_scheme = InputScheme.KEYBOARD
	elif event is InputEventJoypadButton or event is InputEventJoypadMotion:
		new_scheme = InputScheme.GAMEPAD
	elif event is InputEventScreenTouch or event is InputEventScreenDrag:
		new_scheme = InputScheme.TOUCH

	if new_scheme != current_scheme:
		current_scheme = new_scheme
		input_scheme_changed.emit(get_scheme_name())


func _emit_action_signals(event: InputEvent) -> void:
	for action in InputMap.get_actions():
		if event.is_action_pressed(action):
			action_pressed.emit(action)
		elif event.is_action_released(action):
			action_released.emit(action)


func _detect_input_scheme() -> void:
	# Check for connected gamepads
	if Input.get_connected_joypads().size() > 0:
		current_scheme = InputScheme.GAMEPAD
	else:
		current_scheme = InputScheme.KEYBOARD


func _save_default_bindings() -> void:
	for action in InputMap.get_actions():
		if not action.begins_with("ui_"):
			default_bindings[action] = InputMap.action_get_events(action).duplicate()


## Get movement input as Vector2
func get_movement_vector() -> Vector2:
	if current_scheme == InputScheme.TOUCH:
		return _touch_movement

	var movement = Vector2.ZERO

	movement.x = Input.get_axis(move_left_action, move_right_action)
	movement.y = Input.get_axis(move_up_action, move_down_action)

	# Apply deadzone
	if movement.length() < deadzone:
		return Vector2.ZERO

	return movement.normalized() if movement.length() > 1.0 else movement


## Get horizontal movement only
func get_horizontal_input() -> float:
	if current_scheme == InputScheme.TOUCH:
		return _touch_movement.x

	var input = Input.get_axis(move_left_action, move_right_action)
	return input if abs(input) > deadzone else 0.0


## Get vertical movement only
func get_vertical_input() -> float:
	if current_scheme == InputScheme.TOUCH:
		return _touch_movement.y

	var input = Input.get_axis(move_up_action, move_down_action)
	return input if abs(input) > deadzone else 0.0


## Check if action was just pressed this frame
func is_action_just_pressed(action: String) -> bool:
	if current_scheme == InputScheme.TOUCH:
		return _touch_actions.get(action + "_just_pressed", false)
	return Input.is_action_just_pressed(action)


## Check if action is currently held
func is_action_pressed(action: String) -> bool:
	if current_scheme == InputScheme.TOUCH:
		return _touch_actions.get(action, false)
	return Input.is_action_pressed(action)


## Check if action was just released this frame
func is_action_just_released(action: String) -> bool:
	if current_scheme == InputScheme.TOUCH:
		return _touch_actions.get(action + "_just_released", false)
	return Input.is_action_just_released(action)


## Rebind an action to a new event
func rebind_action(action: String, new_event: InputEvent) -> void:
	if not InputMap.has_action(action):
		push_error("Action does not exist: " + action)
		return

	# Clear existing events
	InputMap.action_erase_events(action)
	# Add new event
	InputMap.action_add_event(action, new_event)


## Add an event to an action (keeps existing bindings)
func add_binding(action: String, new_event: InputEvent) -> void:
	if not InputMap.has_action(action):
		push_error("Action does not exist: " + action)
		return

	InputMap.action_add_event(action, new_event)


## Reset all bindings to defaults
func reset_to_defaults() -> void:
	for action in default_bindings:
		InputMap.action_erase_events(action)
		for event in default_bindings[action]:
			InputMap.action_add_event(action, event)


## Reset a specific action to default
func reset_action_to_default(action: String) -> void:
	if action in default_bindings:
		InputMap.action_erase_events(action)
		for event in default_bindings[action]:
			InputMap.action_add_event(action, event)


## Get all bindings for an action
func get_action_bindings(action: String) -> Array:
	if not InputMap.has_action(action):
		return []
	return InputMap.action_get_events(action)


## Get binding as displayable text
func get_action_binding_text(action: String) -> String:
	var events = get_action_bindings(action)
	if events.is_empty():
		return "Unbound"

	return events[0].as_text()


## Get current input scheme name
func get_scheme_name() -> String:
	match current_scheme:
		InputScheme.KEYBOARD: return "keyboard"
		InputScheme.GAMEPAD: return "gamepad"
		InputScheme.TOUCH: return "touch"
		_: return "unknown"


## Touch input methods (for mobile games)

## Set touch movement (called by touch UI)
func set_touch_movement(movement: Vector2) -> void:
	_touch_movement = movement


## Set touch action state (called by touch UI buttons)
func set_touch_action(action: String, pressed: bool) -> void:
	var was_pressed = _touch_actions.get(action, false)

	_touch_actions[action] = pressed
	_touch_actions[action + "_just_pressed"] = pressed and not was_pressed
	_touch_actions[action + "_just_released"] = not pressed and was_pressed


## Clear touch just_pressed/just_released flags (call at end of frame)
func clear_touch_frame_flags() -> void:
	for action in _touch_actions.keys():
		if action.ends_with("_just_pressed") or action.ends_with("_just_released"):
			_touch_actions[action] = false
