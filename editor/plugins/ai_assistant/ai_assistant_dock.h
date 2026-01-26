/**************************************************************************/
/*  ai_assistant_dock.h                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/

#pragma once

#include "editor/docks/editor_dock.h"

#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/label.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/text_edit.h"

#include "core/io/json.h"
#include "modules/websocket/websocket_peer.h"

class AIAssistantDock : public EditorDock {
	GDCLASS(AIAssistantDock, EditorDock);

public:
	enum ConnectionStatus {
		DISCONNECTED,
		CONNECTING,
		CONNECTED,
		CONNECTION_ERROR
	};

private:
	// Main container
	VBoxContainer *main_container = nullptr;

	// UI Components
	HBoxContainer *header_container = nullptr;
	Label *title_label = nullptr;
	ColorRect *connection_indicator = nullptr;

	HBoxContainer *toolbar_container = nullptr;
	MenuButton *template_button = nullptr;
	Button *clear_button = nullptr;
	Button *reconnect_button = nullptr;
	Label *status_label = nullptr;

	ScrollContainer *chat_scroll = nullptr;
	VBoxContainer *chat_container = nullptr;

	VBoxContainer *input_container = nullptr;
	TextEdit *prompt_input = nullptr;
	Button *send_button = nullptr;

	// WebSocket connection
	Ref<WebSocketPeer> websocket;
	String service_url = "ws://localhost:8080";
	ConnectionStatus connection_status = DISCONNECTED;

	// AI Service process
	OS::ProcessID service_pid = 0;
	bool service_running = false;
	String python_executable = "python";
	String service_script_path;

	// Chat history
	Vector<Dictionary> chat_history;

	// Internal methods
	void _setup_ui();
	void _connect_signals();
	void _update_connection_indicator();

	// Service management
	void _find_service_path();
	void _start_ai_service();
	void _stop_ai_service();
	bool _is_service_running();

	// WebSocket handling
	void _connect_to_service();
	void _disconnect_from_service();
	void _process_websocket();
	void _handle_websocket_message(const String &p_message);
	void _send_websocket_message(const Dictionary &p_message);

	// UI handlers
	void _on_send_pressed();
	void _on_clear_pressed();
	void _on_reconnect_pressed();
	void _on_template_selected(int p_id);
	void _on_prompt_input_gui_input(const Ref<InputEvent> &p_event);

	// Message handling
	void _add_message(const String &p_sender, const String &p_text, const Color &p_color);
	void _add_user_message(const String &p_text);
	void _add_ai_message(const String &p_text);
	void _add_system_message(const String &p_text);

	// Command processing
	void _process_prompt(const String &p_prompt);
	void _process_local_command(const String &p_prompt);
	void _execute_commands(const Array &p_commands);
	Dictionary _execute_single_command(const Dictionary &p_command);

	// Status updates
	void _update_status(const String &p_text, const Color &p_color);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	AIAssistantDock();
	~AIAssistantDock();

	void set_service_url(const String &p_url);
	String get_service_url() const;

	bool is_connected_to_service() const;
	ConnectionStatus get_connection_status() const;
};
