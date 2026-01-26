@tool
extends Node
class_name AIServiceManager
## Manages the AI service Python process lifecycle.
## Automatically starts the service when the plugin loads and stops it on exit.

signal service_started
signal service_stopped
signal service_error(message: String)

## Service configuration
@export var python_executable: String = "python"
@export var service_host: String = "localhost"
@export var service_port: int = 8080
@export var auto_restart: bool = true
@export var startup_timeout: float = 10.0

## Process state
var _process_id: int = -1
var _is_running: bool = false
var _output: Array = []
var _service_path: String = ""

## Health check
var _health_check_timer: Timer


func _ready() -> void:
	_service_path = _find_service_path()
	_setup_health_check()


func _exit_tree() -> void:
	stop_service()


func _setup_health_check() -> void:
	_health_check_timer = Timer.new()
	_health_check_timer.wait_time = 30.0  # Check every 30 seconds
	_health_check_timer.timeout.connect(_on_health_check)
	add_child(_health_check_timer)


## Find the AI service path relative to the addon
func _find_service_path() -> String:
	# Try multiple possible locations
	var possible_paths = [
		"res://ai_service/src/main.py",
		"res://../ai_service/src/main.py",
		ProjectSettings.globalize_path("res://") + "../ai_service/src/main.py",
		ProjectSettings.globalize_path("res://ai_service/src/main.py"),
	]

	for path in possible_paths:
		var global_path = path
		if path.begins_with("res://"):
			global_path = ProjectSettings.globalize_path(path)

		if FileAccess.file_exists(global_path) or FileAccess.file_exists(path):
			print("[AIServiceManager] Found service at: ", global_path)
			return global_path

	# Default to project-relative path
	var default_path = ProjectSettings.globalize_path("res://").get_base_dir() + "/ai_service/src/main.py"
	print("[AIServiceManager] Using default service path: ", default_path)
	return default_path


## Start the AI service
func start_service() -> Error:
	if _is_running:
		print("[AIServiceManager] Service already running")
		return OK

	if _service_path.is_empty():
		_service_path = _find_service_path()

	# Check if service file exists
	if not FileAccess.file_exists(_service_path):
		var error_msg = "AI service not found at: " + _service_path
		push_error("[AIServiceManager] " + error_msg)
		service_error.emit(error_msg)
		return ERR_FILE_NOT_FOUND

	# Build the command
	var args = [_service_path]

	# Set environment variables for the service
	var env = {
		"GODOT_AI_HOST": service_host,
		"GODOT_AI_PORT": str(service_port)
	}

	# Get the working directory (ai_service/src)
	var working_dir = _service_path.get_base_dir()

	print("[AIServiceManager] Starting AI service...")
	print("[AIServiceManager] Python: ", python_executable)
	print("[AIServiceManager] Script: ", _service_path)
	print("[AIServiceManager] Working dir: ", working_dir)

	# Start the process
	# Note: OS.create_process returns the PID on success, -1 on failure
	_process_id = OS.create_process(python_executable, args, false)

	if _process_id <= 0:
		var error_msg = "Failed to start AI service process"
		push_error("[AIServiceManager] " + error_msg)
		service_error.emit(error_msg)
		return ERR_CANT_CREATE

	_is_running = true
	_health_check_timer.start()

	print("[AIServiceManager] Service started with PID: ", _process_id)
	service_started.emit()

	return OK


## Stop the AI service
func stop_service() -> void:
	if not _is_running:
		return

	print("[AIServiceManager] Stopping AI service...")

	_health_check_timer.stop()

	if _process_id > 0:
		# Try graceful shutdown first by killing the process
		var result = OS.kill(_process_id)
		if result == OK:
			print("[AIServiceManager] Service stopped (PID: ", _process_id, ")")
		else:
			push_warning("[AIServiceManager] Could not stop service process")

	_process_id = -1
	_is_running = false
	service_stopped.emit()


## Restart the service
func restart_service() -> Error:
	stop_service()
	await get_tree().create_timer(1.0).timeout
	return start_service()


## Check if service is running
func is_running() -> bool:
	if not _is_running or _process_id <= 0:
		return false

	# Check if process is still alive
	return OS.is_process_running(_process_id)


## Get service URL
func get_service_url() -> String:
	return "ws://%s:%d" % [service_host, service_port]


## Health check callback
func _on_health_check() -> void:
	if _is_running and not is_running():
		push_warning("[AIServiceManager] Service process died unexpectedly")
		_is_running = false
		service_stopped.emit()

		if auto_restart:
			print("[AIServiceManager] Auto-restarting service...")
			await get_tree().create_timer(2.0).timeout
			start_service()


## Get the service configuration as a dictionary
func get_config() -> Dictionary:
	return {
		"host": service_host,
		"port": service_port,
		"url": get_service_url(),
		"running": is_running(),
		"pid": _process_id,
		"service_path": _service_path
	}
