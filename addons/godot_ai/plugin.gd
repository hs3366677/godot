@tool
extends EditorPlugin

const DOCK_SCENE = preload("res://addons/godot_ai/dock/ai_dock.tscn")
const ServiceManager = preload("res://addons/godot_ai/bridge/service_manager.gd")

var ai_dock: Control
var bridge: GodotAIBridge
var service_manager: ServiceManager


func _enter_tree() -> void:
	# Start the AI service first
	service_manager = ServiceManager.new()
	service_manager.name = "AIServiceManager"
	add_child(service_manager)
	service_manager.start_service()

	# Initialize the bridge
	bridge = GodotAIBridge.new()
	add_child(bridge)

	# Create and add the AI dock
	ai_dock = DOCK_SCENE.instantiate()
	ai_dock.bridge = bridge
	ai_dock.service_manager = service_manager
	add_control_to_dock(DOCK_SLOT_RIGHT_UL, ai_dock)

	# Connect to the service once it's ready
	_connect_to_service()

	print("[GodotAI] Plugin initialized")


func _exit_tree() -> void:
	# Clean up the dock
	if ai_dock:
		remove_control_from_docks(ai_dock)
		ai_dock.queue_free()

	# Clean up the bridge
	if bridge:
		bridge.disconnect_from_service()
		bridge.queue_free()

	# Stop the AI service
	if service_manager:
		service_manager.stop_service()
		service_manager.queue_free()

	print("[GodotAI] Plugin deactivated")


func _connect_to_service() -> void:
	# Wait a moment for the service to start, then connect
	await get_tree().create_timer(2.0).timeout

	if bridge and service_manager and service_manager.is_running():
		var err = bridge.connect_to_service()
		if err == OK:
			print("[GodotAI] Connected to AI service")
		else:
			push_warning("[GodotAI] Could not connect to AI service. It may still be starting.")


func _get_plugin_name() -> String:
	return "GodotAI"
