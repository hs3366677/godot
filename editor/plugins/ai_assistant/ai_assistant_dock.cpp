/**************************************************************************/
/*  ai_assistant_dock.cpp                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/

#include "ai_assistant_dock.h"

#include "core/config/project_settings.h"
#include "core/input/input_event.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "editor/editor_interface.h"
#include "scene/gui/separator.h"

void AIAssistantDock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_service_url", "url"), &AIAssistantDock::set_service_url);
	ClassDB::bind_method(D_METHOD("get_service_url"), &AIAssistantDock::get_service_url);
	ClassDB::bind_method(D_METHOD("is_connected_to_service"), &AIAssistantDock::is_connected_to_service);

	ClassDB::bind_method(D_METHOD("_on_send_pressed"), &AIAssistantDock::_on_send_pressed);
	ClassDB::bind_method(D_METHOD("_on_clear_pressed"), &AIAssistantDock::_on_clear_pressed);
	ClassDB::bind_method(D_METHOD("_on_reconnect_pressed"), &AIAssistantDock::_on_reconnect_pressed);
	ClassDB::bind_method(D_METHOD("_on_template_selected", "id"), &AIAssistantDock::_on_template_selected);
}

AIAssistantDock::AIAssistantDock() {
	set_title(TTR("AI Assistant"));
	set_icon_name(SNAME("Node"));
	set_default_slot(DOCK_SLOT_RIGHT_UL);

	_setup_ui();
	_connect_signals();
	_find_service_path();
}

AIAssistantDock::~AIAssistantDock() {
	_stop_ai_service();
	_disconnect_from_service();
}

void AIAssistantDock::_setup_ui() {
	// Main container for all UI
	main_container = memnew(VBoxContainer);
	main_container->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	add_child(main_container);

	// Header
	header_container = memnew(HBoxContainer);
	main_container->add_child(header_container);

	title_label = memnew(Label);
	title_label->set_text("AI Assistant");
	title_label->add_theme_font_size_override("font_size", 16);
	header_container->add_child(title_label);

	header_container->add_spacer();

	connection_indicator = memnew(ColorRect);
	connection_indicator->set_custom_minimum_size(Size2(12, 12));
	connection_indicator->set_color(Color(0.5, 0.5, 0.5));
	header_container->add_child(connection_indicator);

	// Toolbar
	toolbar_container = memnew(HBoxContainer);
	main_container->add_child(toolbar_container);

	template_button = memnew(MenuButton);
	template_button->set_text("Templates");
	toolbar_container->add_child(template_button);

	PopupMenu *template_popup = template_button->get_popup();
	template_popup->add_item("Platformer 2D", 0);
	template_popup->add_item("Tower Defense", 1);
	template_popup->add_item("RPG Top-Down", 2);
	template_popup->add_item("Shooter Top-Down", 3);
	template_popup->add_separator();
	template_popup->add_item("Empty Project", 99);

	clear_button = memnew(Button);
	clear_button->set_text("Clear");
	toolbar_container->add_child(clear_button);

	reconnect_button = memnew(Button);
	reconnect_button->set_text("Reconnect");
	toolbar_container->add_child(reconnect_button);

	toolbar_container->add_spacer();

	status_label = memnew(Label);
	status_label->set_text("Starting...");
	status_label->add_theme_font_size_override("font_size", 11);
	toolbar_container->add_child(status_label);

	// Chat area
	chat_scroll = memnew(ScrollContainer);
	chat_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	chat_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	main_container->add_child(chat_scroll);

	chat_container = memnew(VBoxContainer);
	chat_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	chat_scroll->add_child(chat_container);

	// Welcome message
	RichTextLabel *welcome = memnew(RichTextLabel);
	welcome->set_use_bbcode(true);
	welcome->set_fit_content(true);
	welcome->set_text("[color=gray]Welcome to GodotAI![/color]\n\nDescribe what you want to create, or select a template to get started.\n\n[color=cyan]Examples:[/color]\n- \"Create a 2D platformer\"\n- \"Add a player character\"\n- \"Make the enemy follow the player\"");
	chat_container->add_child(welcome);

	// Separator
	main_container->add_child(memnew(HSeparator));

	// Input area
	input_container = memnew(VBoxContainer);
	main_container->add_child(input_container);

	prompt_input = memnew(TextEdit);
	prompt_input->set_custom_minimum_size(Size2(0, 60));
	prompt_input->set_placeholder("Describe what you want to create...");
	prompt_input->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);
	input_container->add_child(prompt_input);

	send_button = memnew(Button);
	send_button->set_text("Send (Enter)");
	input_container->add_child(send_button);

	// Initial status
	_update_status("Initializing...", Color(1, 1, 0));
}

void AIAssistantDock::_connect_signals() {
	send_button->connect("pressed", callable_mp(this, &AIAssistantDock::_on_send_pressed));
	clear_button->connect("pressed", callable_mp(this, &AIAssistantDock::_on_clear_pressed));
	reconnect_button->connect("pressed", callable_mp(this, &AIAssistantDock::_on_reconnect_pressed));
	template_button->get_popup()->connect("id_pressed", callable_mp(this, &AIAssistantDock::_on_template_selected));

	prompt_input->connect("gui_input", callable_mp(this, &AIAssistantDock::_on_prompt_input_gui_input));
}

void AIAssistantDock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_add_system_message("GodotAI is starting...");
			_start_ai_service();

			// Connect after a delay
			SceneTree *tree = get_tree();
			if (tree) {
				tree->create_timer(2.0)->connect("timeout", callable_mp(this, &AIAssistantDock::_connect_to_service));
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			_stop_ai_service();
			_disconnect_from_service();
		} break;

		case NOTIFICATION_PROCESS: {
			_process_websocket();
		} break;
	}
}

void AIAssistantDock::_find_service_path() {
	// Look for the AI service relative to the project/editor
	String exe_path = OS::get_singleton()->get_executable_path().get_base_dir();

	Vector<String> possible_paths;
	possible_paths.push_back(exe_path + "/ai_service/src/main.py");
	possible_paths.push_back(exe_path + "/../ai_service/src/main.py");
	possible_paths.push_back("res://ai_service/src/main.py");

	for (const String &path : possible_paths) {
		if (FileAccess::exists(path)) {
			service_script_path = path;
			print_line("[AIAssistant] Found service at: " + path);
			return;
		}
	}

	service_script_path = exe_path + "/ai_service/src/main.py";
	print_line("[AIAssistant] Using default service path: " + service_script_path);
}

void AIAssistantDock::_start_ai_service() {
	if (service_running) {
		return;
	}

	if (service_script_path.is_empty()) {
		_add_system_message("AI service script not found. Running in offline mode.");
		_update_status("Offline Mode", Color(1, 0.5, 0));
		return;
	}

	List<String> args;
	args.push_back(service_script_path);

	Error err = OS::get_singleton()->create_process(python_executable, args, &service_pid);

	if (err != OK || service_pid <= 0) {
		_add_system_message("Failed to start AI service. Running in offline mode.");
		_update_status("Offline Mode", Color(1, 0.5, 0));
		service_running = false;
		return;
	}

	service_running = true;
	_add_system_message("AI service started (PID: " + itos(service_pid) + ")");
	_update_status("Service Starting...", Color(1, 1, 0));

	print_line("[AIAssistant] Service started with PID: " + itos(service_pid));
}

void AIAssistantDock::_stop_ai_service() {
	if (!service_running || service_pid <= 0) {
		return;
	}

	OS::get_singleton()->kill(service_pid);
	service_pid = 0;
	service_running = false;

	print_line("[AIAssistant] Service stopped");
}

bool AIAssistantDock::_is_service_running() {
	if (!service_running || service_pid <= 0) {
		return false;
	}
	return OS::get_singleton()->is_process_running(service_pid);
}

void AIAssistantDock::_connect_to_service() {
	if (connection_status == CONNECTED) {
		return;
	}

	websocket = Ref<WebSocketPeer>(WebSocketPeer::create());
	if (websocket.is_null()) {
		_add_system_message("WebSocket not available.");
		_update_status("WebSocket Error", Color(1, 0, 0));
		connection_status = CONNECTION_ERROR;
		_update_connection_indicator();
		return;
	}
	Error err = websocket->connect_to_url(service_url);

	if (err != OK) {
		_add_system_message("Failed to connect to AI service.");
		_update_status("Connection Failed", Color(1, 0, 0));
		connection_status = CONNECTION_ERROR;
		_update_connection_indicator();
		return;
	}

	connection_status = CONNECTING;
	_update_status("Connecting...", Color(1, 1, 0));
	_update_connection_indicator();

	set_process(true);
}

void AIAssistantDock::_disconnect_from_service() {
	if (websocket.is_valid()) {
		websocket->close();
		websocket.unref();
	}

	connection_status = DISCONNECTED;
	_update_connection_indicator();
	set_process(false);
}

void AIAssistantDock::_process_websocket() {
	if (!websocket.is_valid()) {
		return;
	}

	websocket->poll();

	WebSocketPeer::State state = websocket->get_ready_state();

	switch (state) {
		case WebSocketPeer::STATE_OPEN: {
			if (connection_status != CONNECTED) {
				connection_status = CONNECTED;
				_update_status("Connected", Color(0, 1, 0));
				_update_connection_indicator();
				_add_system_message("Connected to AI service!");
			}

			while (websocket->get_available_packet_count() > 0) {
				Vector<uint8_t> packet;
				websocket->get_packet_buffer(packet);
				String message = String::utf8((const char *)packet.ptr(), packet.size());
				_handle_websocket_message(message);
			}
		} break;

		case WebSocketPeer::STATE_CLOSING: {
			// Wait for close
		} break;

		case WebSocketPeer::STATE_CLOSED: {
			connection_status = DISCONNECTED;
			_update_status("Disconnected", Color(0.5, 0.5, 0.5));
			_update_connection_indicator();
			set_process(false);
		} break;

		case WebSocketPeer::STATE_CONNECTING: {
			// Still connecting
		} break;
	}
}

void AIAssistantDock::_handle_websocket_message(const String &p_message) {
	JSON json;
	Error err = json.parse(p_message);
	if (err != OK) {
		print_line("[AIAssistant] Invalid JSON received");
		return;
	}

	Dictionary msg_data = json.get_data();
	String type = msg_data.get("type", "");

	if (type == "connected") {
		_add_system_message(msg_data.get("message", "Connected"));
	} else if (type == "message") {
		_add_ai_message(msg_data.get("content", ""));
	} else if (type == "commands") {
		String message = msg_data.get("message", "");
		if (!message.is_empty()) {
			_add_ai_message(message);
		}
		Array commands = msg_data.get("commands", Array());
		_execute_commands(commands);
	} else if (type == "error") {
		_add_system_message("Error: " + String(msg_data.get("message", "Unknown error")));
	}
}

void AIAssistantDock::_send_websocket_message(const Dictionary &p_message) {
	if (!websocket.is_valid() || connection_status != CONNECTED) {
		return;
	}

	String json_str = JSON::stringify(p_message);
	websocket->send_text(json_str);
}

void AIAssistantDock::_on_send_pressed() {
	String prompt = prompt_input->get_text().strip_edges();
	if (prompt.is_empty()) {
		return;
	}

	_add_user_message(prompt);
	_process_prompt(prompt);
	prompt_input->set_text("");
}

void AIAssistantDock::_on_clear_pressed() {
	// Clear all children except the first (welcome message will be re-added)
	while (chat_container->get_child_count() > 0) {
		Node *child = chat_container->get_child(0);
		chat_container->remove_child(child);
		memdelete(child);
	}
	chat_history.clear();
}

void AIAssistantDock::_on_reconnect_pressed() {
	_add_system_message("Reconnecting...");

	_disconnect_from_service();

	if (!_is_service_running()) {
		_start_ai_service();

		SceneTree *tree = get_tree();
		if (tree) {
			tree->create_timer(2.0)->connect("timeout", callable_mp(this, &AIAssistantDock::_connect_to_service));
		}
	} else {
		_connect_to_service();
	}
}

void AIAssistantDock::_on_template_selected(int p_id) {
	String templates[] = { "platformer_2d", "tower_defense", "rpg_topdown", "shooter_topdown" };

	if (p_id < 4) {
		_add_system_message("Loading template: " + templates[p_id]);
		// TODO: Implement template loading
	} else if (p_id == 99) {
		_add_system_message("Starting with empty project");
	}
}

void AIAssistantDock::_on_prompt_input_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> key = p_event;
	if (key.is_valid() && key->is_pressed() && key->get_keycode() == Key::ENTER && !key->is_shift_pressed()) {
		_on_send_pressed();
		prompt_input->accept_event();
	}
}

void AIAssistantDock::_process_prompt(const String &p_prompt) {
	_update_status("Processing...", Color(1, 1, 0));

	if (connection_status == CONNECTED) {
		Dictionary message;
		message["type"] = "prompt";
		message["content"] = p_prompt;
		message["context"] = Dictionary();
		message["project_state"] = Dictionary();
		_send_websocket_message(message);
	} else {
		_process_local_command(p_prompt);
	}
}

void AIAssistantDock::_process_local_command(const String &p_prompt) {
	String lower = p_prompt.to_lower();

	if (lower.contains("help")) {
		_add_ai_message("**GodotAI Assistant Help**\n\n**Quick Commands:**\n- Create a [NodeType] named [Name]\n- Run / Play the project\n- Stop the project\n- Save the scene\n\n**Full AI Features:**\nConnect to the AI service for natural language processing.\n\nUse the 'Reconnect' button to connect.");
	} else if (lower.contains("run") || lower.contains("play")) {
		EditorInterface::get_singleton()->play_main_scene();
		_add_ai_message("Running project...");
	} else if (lower.contains("stop")) {
		EditorInterface::get_singleton()->stop_playing_scene();
		_add_ai_message("Stopped project");
	} else if (lower.contains("save")) {
		EditorInterface::get_singleton()->save_scene();
		_add_ai_message("Scene saved");
	} else {
		_add_ai_message("I understand you want to: \"" + p_prompt + "\"\n\nI'm running in offline mode. Click 'Reconnect' to connect to the AI service for full natural language processing.");
	}

	_update_status("Ready", Color(0, 1, 0));
}

void AIAssistantDock::_execute_commands(const Array &p_commands) {
	for (int i = 0; i < p_commands.size(); i++) {
		Dictionary cmd = p_commands[i];
		Dictionary result = _execute_single_command(cmd);

		if (!result.get("success", false)) {
			_add_system_message("Command failed: " + String(result.get("error", "Unknown error")));
			break;
		}
	}

	_update_status("Ready", Color(0, 1, 0));
}

Dictionary AIAssistantDock::_execute_single_command(const Dictionary &p_command) {
	Dictionary result;
	result["success"] = false;

	String action = p_command.get("action", "");
	Dictionary params = p_command.get("params", Dictionary());

	if (action.is_empty()) {
		result["error"] = "Missing action";
		return result;
	}

	// TODO: Implement command execution using EditorInterface
	// This would interact with the editor to create nodes, scripts, etc.

	result["success"] = true;
	result["message"] = "Executed: " + action;
	return result;
}

void AIAssistantDock::_add_message(const String &p_sender, const String &p_text, const Color &p_color) {
	Dictionary msg;
	msg["sender"] = p_sender;
	msg["text"] = p_text;
	msg["timestamp"] = Time::get_singleton()->get_datetime_string_from_system();
	chat_history.push_back(msg);

	VBoxContainer *msg_container = memnew(VBoxContainer);

	HBoxContainer *header = memnew(HBoxContainer);
	Label *sender_label = memnew(Label);
	sender_label->set_text(p_sender);
	sender_label->add_theme_color_override("font_color", p_color);
	sender_label->add_theme_font_size_override("font_size", 12);
	header->add_child(sender_label);

	Label *time_label = memnew(Label);
	String time_str = msg["timestamp"];
	if (time_str.length() > 11) {
		time_label->set_text("  " + time_str.substr(11, 5));
	}
	time_label->add_theme_color_override("font_color", Color(0.6, 0.6, 0.6));
	time_label->add_theme_font_size_override("font_size", 10);
	header->add_child(time_label);

	msg_container->add_child(header);

	RichTextLabel *text_label = memnew(RichTextLabel);
	text_label->set_use_bbcode(true);
	text_label->set_fit_content(true);
	text_label->set_text(p_text);
	text_label->set_custom_minimum_size(Size2(280, 0));
	msg_container->add_child(text_label);

	msg_container->add_child(memnew(HSeparator));

	chat_container->add_child(msg_container);

	// Scroll to bottom
	chat_scroll->call_deferred("set_v_scroll", chat_scroll->get_v_scroll_bar()->get_max());
}

void AIAssistantDock::_add_user_message(const String &p_text) {
	_add_message("You", p_text, Color(0.4, 0.6, 1.0));
}

void AIAssistantDock::_add_ai_message(const String &p_text) {
	_add_message("AI", p_text, Color(0.4, 1.0, 0.6));
}

void AIAssistantDock::_add_system_message(const String &p_text) {
	_add_message("System", p_text, Color(1.0, 1.0, 0.6));
}

void AIAssistantDock::_update_status(const String &p_text, const Color &p_color) {
	if (status_label) {
		status_label->set_text(p_text);
		status_label->add_theme_color_override("font_color", p_color);
	}
}

void AIAssistantDock::_update_connection_indicator() {
	if (!connection_indicator) {
		return;
	}

	switch (connection_status) {
		case DISCONNECTED:
			connection_indicator->set_color(Color(0.5, 0.5, 0.5));
			break;
		case CONNECTING:
			connection_indicator->set_color(Color(1, 1, 0));
			break;
		case CONNECTED:
			connection_indicator->set_color(Color(0, 1, 0));
			break;
		case CONNECTION_ERROR:
			connection_indicator->set_color(Color(1, 0, 0));
			break;
	}
}

void AIAssistantDock::set_service_url(const String &p_url) {
	service_url = p_url;
}

String AIAssistantDock::get_service_url() const {
	return service_url;
}

bool AIAssistantDock::is_connected_to_service() const {
	return connection_status == CONNECTED;
}

AIAssistantDock::ConnectionStatus AIAssistantDock::get_connection_status() const {
	return connection_status;
}
