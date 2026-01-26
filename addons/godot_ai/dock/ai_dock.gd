@tool
extends Control
## AI Assistant Dock - Main UI for interacting with the AI game creation assistant.

signal prompt_submitted(prompt: String)
signal template_selected(template_id: String)

const CHAT_MESSAGE = preload("res://addons/godot_ai/dock/chat_message.tscn")

## Reference to the bridge
var bridge: Node

## Reference to the service manager
var service_manager: Node

## Chat history
var chat_history: Array = []

## UI References
@onready var prompt_input: TextEdit = %PromptInput
@onready var send_button: Button = %SendButton
@onready var chat_container: VBoxContainer = %ChatContainer
@onready var chat_scroll: ScrollContainer = %ChatScroll
@onready var status_label: Label = %StatusLabel
@onready var template_button: MenuButton = %TemplateButton
@onready var clear_button: Button = %ClearButton
@onready var reconnect_button: Button = %ReconnectButton
@onready var connection_indicator: ColorRect = %ConnectionIndicator


func _ready() -> void:
	_setup_ui()
	_connect_signals()
	_load_templates()
	_show_startup_message()


func _setup_ui() -> void:
	# Set up template menu
	var popup = template_button.get_popup()
	popup.clear()
	popup.add_item("Platformer 2D", 0)
	popup.add_item("Tower Defense", 1)
	popup.add_item("RPG Top-Down", 2)
	popup.add_item("Shooter Top-Down", 3)
	popup.add_separator()
	popup.add_item("Empty Project", 99)

	# Initial status
	_update_status("Ready", Color.GREEN)


func _connect_signals() -> void:
	send_button.pressed.connect(_on_send_pressed)
	clear_button.pressed.connect(_on_clear_pressed)
	reconnect_button.pressed.connect(reconnect)
	prompt_input.gui_input.connect(_on_prompt_input)
	template_button.get_popup().id_pressed.connect(_on_template_selected)

	if bridge:
		bridge.ai_response_received.connect(_on_ai_response)
		bridge.command_executed.connect(_on_command_executed)
		bridge.command_failed.connect(_on_command_failed)
		bridge.connection_status_changed.connect(_on_connection_changed)

	if service_manager:
		service_manager.service_started.connect(_on_service_started)
		service_manager.service_stopped.connect(_on_service_stopped)
		service_manager.service_error.connect(_on_service_error)


func _load_templates() -> void:
	if not bridge:
		return

	var observer = bridge.get_node_or_null("StateObserver")
	if observer:
		var templates = observer.get_available_templates()
		var popup = template_button.get_popup()
		popup.clear()

		for i in templates.size():
			popup.add_item(templates[i].get("name", "Unknown"), i)

		popup.add_separator()
		popup.add_item("Empty Project", 99)


## Handle send button click
func _on_send_pressed() -> void:
	var prompt = prompt_input.text.strip_edges()
	if prompt.is_empty():
		return

	_add_user_message(prompt)
	_process_prompt(prompt)
	prompt_input.text = ""


## Handle Enter key in prompt input
func _on_prompt_input(event: InputEvent) -> void:
	if event is InputEventKey:
		if event.pressed and event.keycode == KEY_ENTER:
			if not event.shift_pressed:
				_on_send_pressed()
				get_viewport().set_input_as_handled()


## Handle template selection
func _on_template_selected(id: int) -> void:
	var templates = ["platformer_2d", "tower_defense", "rpg_topdown", "shooter_topdown"]

	if id < templates.size():
		template_selected.emit(templates[id])
		_add_system_message("Loading template: " + templates[id])
		# TODO: Actually load the template
	elif id == 99:
		_add_system_message("Starting with empty project")


## Handle clear button
func _on_clear_pressed() -> void:
	for child in chat_container.get_children():
		child.queue_free()
	chat_history.clear()


## Process a user prompt
func _process_prompt(prompt: String) -> void:
	prompt_submitted.emit(prompt)
	_update_status("Processing...", Color.YELLOW)

	# If bridge is connected, send to AI service
	if bridge and bridge.is_connected:
		bridge.send_prompt(prompt)
	else:
		# Local processing fallback - parse simple commands
		_process_local_command(prompt)


## Process commands locally when not connected to AI service
func _process_local_command(prompt: String) -> void:
	var lower_prompt = prompt.to_lower()

	# Simple command parsing
	if "create" in lower_prompt and "node" in lower_prompt:
		_handle_create_node_command(prompt)
	elif "add" in lower_prompt and "script" in lower_prompt:
		_handle_add_script_command(prompt)
	elif "run" in lower_prompt or "play" in lower_prompt:
		_handle_run_command()
	elif "stop" in lower_prompt:
		_handle_stop_command()
	elif "save" in lower_prompt:
		_handle_save_command()
	elif "help" in lower_prompt:
		_show_help()
	else:
		_add_ai_message("I understand you want to: \"" + prompt + "\"\n\nConnect to the AI service for full natural language processing, or try these commands:\n- Create a [NodeType] node named [Name]\n- Add script to [NodeName]\n- Run/Play the project\n- Stop the project\n- Save the scene\n- Help")

	_update_status("Ready", Color.GREEN)


func _handle_create_node_command(prompt: String) -> void:
	# Extract node type and name from prompt
	var words = prompt.split(" ")
	var node_type = "Node2D"
	var node_name = "NewNode"

	for i in words.size():
		var word = words[i]
		if ClassDB.class_exists(word):
			node_type = word
		if word.to_lower() == "named" and i + 1 < words.size():
			node_name = words[i + 1]

	var result = bridge.execute_command({
		"action": "scene.create_node",
		"params": {
			"type": node_type,
			"name": node_name,
			"parent": "/root"
		}
	})

	if result.get("success", false):
		_add_ai_message("Created %s node named '%s'" % [node_type, node_name])
	else:
		_add_ai_message("Failed to create node: " + result.get("error", "Unknown error"))


func _handle_add_script_command(prompt: String) -> void:
	_add_ai_message("To add a script, select a node in the scene tree first, then I can attach a script to it.")


func _handle_run_command() -> void:
	var result = bridge.execute_command({
		"action": "project.run",
		"params": {}
	})
	_add_ai_message("Running project...")


func _handle_stop_command() -> void:
	var result = bridge.execute_command({
		"action": "project.stop",
		"params": {}
	})
	_add_ai_message("Stopped project")


func _handle_save_command() -> void:
	var result = bridge.execute_command({
		"action": "editor.save_scene",
		"params": {}
	})

	if result.get("success", false):
		_add_ai_message("Scene saved: " + result.get("path", ""))
	else:
		_add_ai_message("Failed to save: " + result.get("error", "Unknown error"))


func _show_help() -> void:
	_add_ai_message("""**GodotAI Assistant Help**

**Quick Commands:**
- Create a [NodeType] named [Name]
- Run / Play the project
- Stop the project
- Save the scene

**Templates:**
Click the 'Templates' button to start from a pre-built game template.

**Full AI Features:**
Connect to the AI service for:
- Natural language game creation
- Asset generation
- Code generation
- Iterative refinement

**Examples:**
- "Create a 2D platformer with a jumping character"
- "Add an enemy that patrols left and right"
- "Make the player shoot projectiles when pressing space"
""")


## AI response handlers
func _on_ai_response(response: String) -> void:
	_add_ai_message(response)
	_update_status("Ready", Color.GREEN)


func _on_command_executed(result: Dictionary) -> void:
	if result.get("success", false):
		_update_status("Command executed", Color.GREEN)
	else:
		_update_status("Command failed", Color.RED)


func _on_command_failed(error: String) -> void:
	_add_ai_message("Error: " + error)
	_update_status("Error", Color.RED)


func _on_connection_changed(connected: bool) -> void:
	connection_indicator.color = Color.GREEN if connected else Color.RED
	_update_status("Connected" if connected else "Disconnected",
		Color.GREEN if connected else Color.GRAY)


## Add a user message to chat
func _add_user_message(text: String) -> void:
	_add_message("You", text, Color(0.4, 0.6, 1.0))


## Add an AI message to chat
func _add_ai_message(text: String) -> void:
	_add_message("AI", text, Color(0.4, 1.0, 0.6))


## Add a system message to chat
func _add_system_message(text: String) -> void:
	_add_message("System", text, Color(1.0, 1.0, 0.6))


## Add a message to the chat container
func _add_message(sender: String, text: String, color: Color) -> void:
	var message = {
		"sender": sender,
		"text": text,
		"timestamp": Time.get_datetime_string_from_system()
	}
	chat_history.append(message)

	# Create message UI
	var container = VBoxContainer.new()

	var header = HBoxContainer.new()
	var sender_label = Label.new()
	sender_label.text = sender
	sender_label.add_theme_color_override("font_color", color)
	sender_label.add_theme_font_size_override("font_size", 12)
	header.add_child(sender_label)

	var time_label = Label.new()
	time_label.text = "  " + message.timestamp.split("T")[1].substr(0, 5)
	time_label.add_theme_color_override("font_color", Color(0.6, 0.6, 0.6))
	time_label.add_theme_font_size_override("font_size", 10)
	header.add_child(time_label)

	container.add_child(header)

	var text_label = RichTextLabel.new()
	text_label.bbcode_enabled = true
	text_label.fit_content = true
	text_label.text = text
	text_label.custom_minimum_size.x = 280
	container.add_child(text_label)

	var separator = HSeparator.new()
	container.add_child(separator)

	chat_container.add_child(container)

	# Scroll to bottom
	await get_tree().process_frame
	chat_scroll.scroll_vertical = chat_scroll.get_v_scroll_bar().max_value


## Update status display
func _update_status(text: String, color: Color) -> void:
	if status_label:
		status_label.text = text
		status_label.add_theme_color_override("font_color", color)


## Show startup message
func _show_startup_message() -> void:
	_add_system_message("GodotAI is starting...")
	if service_manager:
		_add_system_message("AI service starting on " + service_manager.get_service_url())


## Service manager signal handlers
func _on_service_started() -> void:
	_add_system_message("AI service started successfully!")
	_update_status("Service Running", Color.YELLOW)
	connection_indicator.color = Color.YELLOW


func _on_service_stopped() -> void:
	_add_system_message("AI service stopped.")
	_update_status("Service Stopped", Color.RED)
	connection_indicator.color = Color.RED


func _on_service_error(message: String) -> void:
	_add_system_message("Service Error: " + message)
	_update_status("Service Error", Color.RED)


## Reconnect to the service
func reconnect() -> void:
	if not service_manager:
		_add_system_message("No service manager available")
		return

	_add_system_message("Reconnecting to AI service...")

	if not service_manager.is_running():
		service_manager.start_service()
		await get_tree().create_timer(2.0).timeout

	if bridge:
		var err = bridge.connect_to_service()
		if err == OK:
			_add_system_message("Connected!")
		else:
			_add_system_message("Failed to connect. Is the service running?")
