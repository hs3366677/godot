@tool
class_name GodotAIBridge
extends Node
## Core bridge connecting AI services to Godot Engine.
## Handles command execution, state observation, and AI communication.

signal command_executed(result: Dictionary)
signal command_failed(error: String)
signal connection_status_changed(connected: bool)
signal ai_response_received(response: String)

const CommandExecutor = preload("res://addons/godot_ai/bridge/commands/executor.gd")
const StateObserver = preload("res://addons/godot_ai/bridge/state_observer.gd")

## AI service endpoint URL
@export var ai_service_url: String = "ws://localhost:8080"

## Connection status
var is_connected: bool = false

## Internal components
var _executor: CommandExecutor
var _observer: StateObserver
var _websocket: WebSocketPeer
var _transaction_counter: int = 0


func _ready() -> void:
	_executor = CommandExecutor.new()
	_executor.name = "CommandExecutor"
	add_child(_executor)

	_observer = StateObserver.new()
	_observer.name = "StateObserver"
	add_child(_observer)

	_websocket = WebSocketPeer.new()


func _process(_delta: float) -> void:
	if _websocket.get_ready_state() == WebSocketPeer.STATE_OPEN:
		_websocket.poll()
		while _websocket.get_available_packet_count() > 0:
			var packet = _websocket.get_packet()
			_handle_ai_response(packet.get_string_from_utf8())


## Connect to AI service
func connect_to_service(url: String = "") -> Error:
	if url.is_empty():
		url = ai_service_url

	var err = _websocket.connect_to_url(url)
	if err == OK:
		is_connected = true
		connection_status_changed.emit(true)
		print("[GodotAI Bridge] Connected to: ", url)
	else:
		printerr("[GodotAI Bridge] Failed to connect: ", err)

	return err


## Disconnect from AI service
func disconnect_from_service() -> void:
	_websocket.close()
	is_connected = false
	connection_status_changed.emit(false)
	print("[GodotAI Bridge] Disconnected")


## Send a prompt to the AI service
func send_prompt(prompt: String, context: Dictionary = {}) -> void:
	if not is_connected:
		push_warning("[GodotAI Bridge] Not connected to AI service")
		return

	var message = {
		"type": "prompt",
		"content": prompt,
		"context": context,
		"project_state": _observer.get_project_state()
	}

	_websocket.send_text(JSON.stringify(message))


## Execute a command from AI or directly
func execute_command(command: Dictionary) -> Dictionary:
	return _executor.execute(command)


## Execute multiple commands as a transaction
func execute_transaction(commands: Array) -> Dictionary:
	_transaction_counter += 1
	var transaction_id = "tx_%d" % _transaction_counter

	var results = []
	var success = true

	for cmd in commands:
		var result = _executor.execute(cmd)
		results.append(result)
		if not result.get("success", false):
			success = false
			break

	if not success:
		# Rollback on failure
		for i in range(results.size() - 1, -1, -1):
			var cmd = commands[i]
			if cmd.has("rollback"):
				_executor.execute(cmd.rollback)

	return {
		"transaction_id": transaction_id,
		"success": success,
		"results": results
	}


## Observe current state
func observe(query: String) -> Dictionary:
	return _observer.observe(query)


## Get current scene tree structure
func get_scene_tree() -> Dictionary:
	return _observer.get_scene_tree()


## Get project state summary
func get_project_state() -> Dictionary:
	return _observer.get_project_state()


## Handle incoming AI response
func _handle_ai_response(response_text: String) -> void:
	var json = JSON.new()
	var err = json.parse(response_text)
	if err != OK:
		printerr("[GodotAI Bridge] Invalid JSON response")
		return

	var response = json.data
	match response.get("type", ""):
		"commands":
			var commands = response.get("commands", [])
			var result = execute_transaction(commands)
			command_executed.emit(result)

		"message":
			ai_response_received.emit(response.get("content", ""))

		"error":
			command_failed.emit(response.get("message", "Unknown error"))

		_:
			push_warning("[GodotAI Bridge] Unknown response type: ", response.get("type"))
