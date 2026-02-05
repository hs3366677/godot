/**************************************************************************/
/*  ai_assistant_dock.cpp                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                           MAKABAKA ENGINE                              */
/*                    AI-powered game creation module                     */
/**************************************************************************/

#include "ai_assistant_dock.h"

#include "core/config/project_settings.h"
#include "core/input/input_event.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/debugger/script_editor_debugger.h"
#include "editor/editor_interface.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/run/editor_run_bar.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/separator.h"

void AIAssistantDock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_service_url", "url"), &AIAssistantDock::set_service_url);
	ClassDB::bind_method(D_METHOD("get_service_url"), &AIAssistantDock::get_service_url);
	ClassDB::bind_method(D_METHOD("is_connected_to_service"), &AIAssistantDock::is_connected_to_service);

	ClassDB::bind_method(D_METHOD("_on_send_pressed"), &AIAssistantDock::_on_send_pressed);
	ClassDB::bind_method(D_METHOD("_on_clear_pressed"), &AIAssistantDock::_on_clear_pressed);
	ClassDB::bind_method(D_METHOD("_on_reconnect_pressed"), &AIAssistantDock::_on_reconnect_pressed);
	ClassDB::bind_method(D_METHOD("_on_template_selected", "id"), &AIAssistantDock::_on_template_selected);
	ClassDB::bind_method(D_METHOD("_check_service_health_deferred"), &AIAssistantDock::_check_service_health_deferred);
}

AIAssistantDock::AIAssistantDock() {
	set_title(TTR("AI Assistant"));
	set_icon_name(SNAME("Node"));
	set_default_slot(DOCK_SLOT_RIGHT_UL);

	_setup_ui();
	_connect_signals();
}

AIAssistantDock::~AIAssistantDock() {
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
	title_label->set_text("Makabaka AI");
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

	verify_button = memnew(Button);
	verify_button->set_text("Verify");
	verify_button->set_tooltip_text("Read game logs and verify with AI");
	toolbar_container->add_child(verify_button);

	reconnect_button = memnew(Button);
	reconnect_button->set_text("Connect");
	toolbar_container->add_child(reconnect_button);

	toolbar_container->add_spacer();

	status_label = memnew(Label);
	status_label->set_text("Disconnected");
	status_label->add_theme_font_size_override("font_size", 11);
	toolbar_container->add_child(status_label);

	// Model selector (two-level submenu: Provider → Models)
	model_button = memnew(MenuButton);
	model_button->set_text("Select Model");
	model_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main_container->add_child(model_button);

	// Tab container for Chat and Logs
	tab_container = memnew(TabContainer);
	tab_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main_container->add_child(tab_container);

	// === Chat Tab ===
	chat_tab = memnew(VBoxContainer);
	chat_tab->set_name("Chat");
	tab_container->add_child(chat_tab);

	// Chat toolbar with mode toggles and auto-scroll
	HBoxContainer *chat_toolbar = memnew(HBoxContainer);
	chat_tab->add_child(chat_toolbar);

	// Plan mode toggle
	plan_mode_toggle = memnew(CheckButton);
	plan_mode_toggle->set_text("Plan");
	plan_mode_toggle->set_tooltip_text("Plan mode: AI will create a plan before making changes");
	plan_mode_toggle->set_pressed(false);
	chat_toolbar->add_child(plan_mode_toggle);

	// Auto-accept edits toggle
	auto_accept_toggle = memnew(CheckButton);
	auto_accept_toggle->set_text("Auto-accept");
	auto_accept_toggle->set_tooltip_text("Auto-accept: Automatically approve file edits");
	auto_accept_toggle->set_pressed(false);
	chat_toolbar->add_child(auto_accept_toggle);

	chat_toolbar->add_spacer();

	chat_auto_scroll = memnew(CheckButton);
	chat_auto_scroll->set_text("Auto-scroll");
	chat_auto_scroll->set_pressed(true);
	chat_toolbar->add_child(chat_auto_scroll);

	// Chat area
	chat_scroll = memnew(ScrollContainer);
	chat_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	chat_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	chat_tab->add_child(chat_scroll);

	chat_container = memnew(VBoxContainer);
	chat_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	chat_scroll->add_child(chat_container);

	// Welcome message
	RichTextLabel *welcome = memnew(RichTextLabel);
	welcome->set_use_bbcode(true);
	welcome->set_fit_content(true);
	welcome->set_selection_enabled(true);
	welcome->set_context_menu_enabled(true);
	welcome->set_focus_mode(Control::FOCUS_CLICK);
	welcome->set_text("[color=gray]Welcome to Makabaka AI![/color]\n\nDescribe what you want to create, or select a template to get started.\n\n[color=cyan]Examples:[/color]\n- \"Create a 2D platformer\"\n- \"Add a player character\"\n- \"Make the enemy follow the player\"\n\n[color=yellow]Connecting to AI service...[/color]");
	chat_container->add_child(welcome);

	// Separator
	chat_tab->add_child(memnew(HSeparator));

	// Input area
	input_container = memnew(VBoxContainer);
	chat_tab->add_child(input_container);

	prompt_input = memnew(TextEdit);
	prompt_input->set_custom_minimum_size(Size2(0, 60));
	prompt_input->set_placeholder("Describe what you want to create...");
	prompt_input->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);
	input_container->add_child(prompt_input);

	// Button row with Send and Stop
	button_container = memnew(HBoxContainer);
	input_container->add_child(button_container);

	send_button = memnew(Button);
	send_button->set_text("Send (Enter)");
	send_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	button_container->add_child(send_button);

	stop_button = memnew(Button);
	stop_button->set_text("Stop");
	stop_button->set_tooltip_text("Stop AI processing");
	stop_button->set_visible(false); // Hidden by default, shown during processing
	stop_button->add_theme_color_override("font_color", Color(1.0, 0.4, 0.4));
	button_container->add_child(stop_button);

	// Processing indicator — overlay on top-right of prompt_input
	processing_label = memnew(Label);
	processing_label->set_text("AI is thinking...");
	processing_label->add_theme_color_override("font_color", Color(0.6, 0.8, 1.0, 0.9));
	processing_label->add_theme_font_size_override("font_size", 12);
	processing_label->set_visible(false);
	// Anchor to top-right of prompt_input
	processing_label->set_anchors_preset(Control::PRESET_TOP_RIGHT);
	processing_label->set_grow_direction_preset(Control::PRESET_TOP_RIGHT);
	processing_label->set_offset(SIDE_RIGHT, -4);
	processing_label->set_offset(SIDE_TOP, 2);
	prompt_input->add_child(processing_label);

	// Timer for processing animation
	processing_timer = memnew(Timer);
	processing_timer->set_wait_time(0.4);
	processing_timer->set_autostart(false);
	add_child(processing_timer);

	// === Logs Tab ===
	_setup_logs_tab();

	// HTTP Request node for chat
	http_request = memnew(HTTPRequest);
	add_child(http_request);

	// HTTP Request node for logs
	logs_http_request = memnew(HTTPRequest);
	add_child(logs_http_request);

	// Timer for log polling
	logs_poll_timer = memnew(Timer);
	logs_poll_timer->set_wait_time(2.0); // Poll every 2 seconds
	logs_poll_timer->set_autostart(false);
	add_child(logs_poll_timer);

	// HTTP Request node for streaming updates
	stream_http_request = memnew(HTTPRequest);
	add_child(stream_http_request);

	// Timer for stream polling (poll for intermediate steps during processing)
	stream_poll_timer = memnew(Timer);
	stream_poll_timer->set_wait_time(0.5); // Poll every 500ms for responsive updates
	stream_poll_timer->set_autostart(false);
	add_child(stream_poll_timer);

	// HTTP Request node for command polling (auto-run from OpenCode)
	command_http_request = memnew(HTTPRequest);
	add_child(command_http_request);

	// HTTP Request node for model fetching
	model_http_request = memnew(HTTPRequest);
	add_child(model_http_request);

	// Timer for command polling
	command_poll_timer = memnew(Timer);
	command_poll_timer->set_wait_time(0.5); // Poll every 500ms for commands
	command_poll_timer->set_autostart(false);
	add_child(command_poll_timer);

	// HTTP Request node for question polling (AI asking user questions)
	question_http_request = memnew(HTTPRequest);
	add_child(question_http_request);

	// Timer for question polling
	question_poll_timer = memnew(Timer);
	question_poll_timer->set_wait_time(0.5); // Poll every 500ms for questions
	question_poll_timer->set_autostart(false);
	add_child(question_poll_timer);

	// HTTP Request node for session list (finding existing sessions)
	session_list_http_request = memnew(HTTPRequest);
	add_child(session_list_http_request);

	// HTTP Request node for OAuth auth flow
	http_auth_request = memnew(HTTPRequest);
	add_child(http_auth_request);

	// Timer for OAuth polling (redirect-based flow)
	auth_poll_timer = memnew(Timer);
	auth_poll_timer->set_wait_time(3.0);
	auth_poll_timer->set_autostart(false);
	add_child(auth_poll_timer);

	// Auth code input dialog (for code-based OAuth flow)
	auth_code_dialog = memnew(AcceptDialog);
	auth_code_dialog->set_title("Enter Authorization Code");
	auth_code_dialog->set_ok_button_text("Submit");
	auth_code_dialog->set_min_size(Size2(400, 0));

	VBoxContainer *dialog_vbox = memnew(VBoxContainer);
	Label *dialog_label = memnew(Label);
	dialog_label->set_text("Paste the authorization code from the browser:");
	dialog_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD);
	dialog_vbox->add_child(dialog_label);

	auth_code_input = memnew(LineEdit);
	auth_code_input->set_placeholder("Paste code here...");
	dialog_vbox->add_child(auth_code_input);

	auth_code_dialog->add_child(dialog_vbox);
	add_child(auth_code_dialog);

	// Initial status
	_update_status("Disconnected", Color(0.5, 0.5, 0.5));
	_update_connection_indicator();
}

void AIAssistantDock::_setup_logs_tab() {
	logs_tab = memnew(VBoxContainer);
	logs_tab->set_name("Logs");
	tab_container->add_child(logs_tab);

	// Logs toolbar
	logs_toolbar = memnew(HBoxContainer);
	logs_tab->add_child(logs_toolbar);

	logs_refresh_button = memnew(Button);
	logs_refresh_button->set_text("Refresh");
	logs_toolbar->add_child(logs_refresh_button);

	logs_clear_button = memnew(Button);
	logs_clear_button->set_text("Clear");
	logs_toolbar->add_child(logs_clear_button);

	logs_toolbar->add_spacer();

	logs_auto_scroll = memnew(CheckButton);
	logs_auto_scroll->set_text("Auto-scroll");
	logs_auto_scroll->set_pressed(true);
	logs_toolbar->add_child(logs_auto_scroll);

	// Logs scroll area
	logs_scroll = memnew(ScrollContainer);
	logs_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	logs_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	logs_tab->add_child(logs_scroll);

	// Logs text
	logs_text = memnew(RichTextLabel);
	logs_text->set_use_bbcode(true);
	logs_text->set_fit_content(true);
	logs_text->set_selection_enabled(true);
	logs_text->set_context_menu_enabled(true);
	logs_text->set_focus_mode(Control::FOCUS_CLICK);
	logs_text->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	logs_text->set_text("[color=gray]AI Server logs will appear here when connected.[/color]\n[color=yellow]Click 'Refresh' to fetch latest events.[/color]");
	logs_scroll->add_child(logs_text);
}

void AIAssistantDock::_connect_signals() {
	// Chat signals
	send_button->connect("pressed", callable_mp(this, &AIAssistantDock::_on_send_pressed));
	stop_button->connect("pressed", callable_mp(this, &AIAssistantDock::_on_stop_pressed));
	clear_button->connect("pressed", callable_mp(this, &AIAssistantDock::_on_clear_pressed));
	verify_button->connect("pressed", callable_mp(this, &AIAssistantDock::_on_verify_pressed));
	reconnect_button->connect("pressed", callable_mp(this, &AIAssistantDock::_on_reconnect_pressed));
	template_button->get_popup()->connect("id_pressed", callable_mp(this, &AIAssistantDock::_on_template_selected));
	prompt_input->connect("gui_input", callable_mp(this, &AIAssistantDock::_on_prompt_input_gui_input));
	http_request->connect("request_completed", callable_mp(this, &AIAssistantDock::_on_http_request_completed));

	// Mode toggles
	plan_mode_toggle->connect("toggled", callable_mp(this, &AIAssistantDock::_on_plan_mode_toggled));
	auto_accept_toggle->connect("toggled", callable_mp(this, &AIAssistantDock::_on_auto_accept_toggled));

	// Processing indicator
	processing_timer->connect("timeout", callable_mp(this, &AIAssistantDock::_on_processing_timer_timeout));

	// Logs signals
	logs_refresh_button->connect("pressed", callable_mp(this, &AIAssistantDock::_on_logs_refresh_pressed));
	logs_clear_button->connect("pressed", callable_mp(this, &AIAssistantDock::_on_logs_clear_pressed));
	logs_http_request->connect("request_completed", callable_mp(this, &AIAssistantDock::_on_logs_http_request_completed));
	logs_poll_timer->connect("timeout", callable_mp(this, &AIAssistantDock::_on_logs_poll_timeout));

	// Command polling signals (auto-run from OpenCode)
	command_http_request->connect("request_completed", callable_mp(this, &AIAssistantDock::_on_command_http_request_completed));
	command_poll_timer->connect("timeout", callable_mp(this, &AIAssistantDock::_on_command_poll_timeout));

	// Question polling signals (AI asking user questions)
	question_http_request->connect("request_completed", callable_mp(this, &AIAssistantDock::_on_question_http_request_completed));
	question_poll_timer->connect("timeout", callable_mp(this, &AIAssistantDock::_on_question_poll_timeout));

	// Stream polling signals (for intermediate steps during AI processing)
	stream_http_request->connect("request_completed", callable_mp(this, &AIAssistantDock::_on_stream_http_request_completed));
	stream_poll_timer->connect("timeout", callable_mp(this, &AIAssistantDock::_on_stream_poll_timeout));

	// Provider/model fetching signals (submenu signals connected in _populate_model_menu)
	model_http_request->connect("request_completed", callable_mp(this, &AIAssistantDock::_on_provider_request_completed));

	// Auth flow signals
	http_auth_request->connect("request_completed", callable_mp(this, &AIAssistantDock::_on_auth_request_completed));
	auth_code_dialog->connect("confirmed", callable_mp(this, &AIAssistantDock::_on_auth_code_submitted));
	auth_poll_timer->connect("timeout", callable_mp(this, &AIAssistantDock::_poll_oauth_callback));

	// Session list signals (for finding existing sessions)
	session_list_http_request->connect("request_completed", callable_mp(this, &AIAssistantDock::_on_session_list_completed));

	// Game stop signal for auto-verification
	EditorRunBar *run_bar = EditorRunBar::get_singleton();
	if (run_bar) {
		run_bar->connect("stop_pressed", callable_mp(this, &AIAssistantDock::_on_game_stopped));
	}

	// Debugger error monitoring - connect in NOTIFICATION_READY since EditorDebuggerNode may not be ready yet
}

void AIAssistantDock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			// Auto-connect to AI service on startup
			call_deferred("_check_service_health_deferred");
			// Enable process to poll for debugger errors
			set_process(true);
		} break;

		case NOTIFICATION_READY: {
			// Connect to debugger error signals
			EditorDebuggerNode *debugger = EditorDebuggerNode::get_singleton();
			if (debugger) {
				debugger->connect("error_selected", callable_mp(this, &AIAssistantDock::_on_debugger_error));
			}
		} break;

		case NOTIFICATION_PROCESS: {
			// Poll for new debugger errors while game is running
			if (EditorInterface::get_singleton()->is_playing_scene()) {
				_check_debugger_errors();
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			// Stop timers
			if (logs_poll_timer && logs_poll_timer->is_inside_tree()) {
				logs_poll_timer->stop();
			}
			if (processing_timer && processing_timer->is_inside_tree()) {
				processing_timer->stop();
			}
			if (stream_poll_timer && stream_poll_timer->is_inside_tree()) {
				stream_poll_timer->stop();
			}
			if (command_poll_timer && command_poll_timer->is_inside_tree()) {
				command_poll_timer->stop();
			}
			if (question_poll_timer && question_poll_timer->is_inside_tree()) {
				question_poll_timer->stop();
			}
		} break;
	}
}

void AIAssistantDock::_check_service_health_deferred() {
	_check_service_health();
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

String AIAssistantDock::_get_project_directory() const {
	return ProjectSettings::get_singleton()->globalize_path("res://");
}

String AIAssistantDock::_load_coding_standards() {
	// Return cached if already loaded.
	if (!_cached_coding_standards.is_empty()) {
		return _cached_coding_standards;
	}

	// Locate the docs/ directory relative to the engine executable.
	// Executable is at: <engine_root>/godot/bin/godot.exe
	// Docs are at:      <engine_root>/docs/
	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	String docs_dir = exe_dir.path_join("..").path_join("..").path_join("docs").simplify_path();

	Ref<DirAccess> dir = DirAccess::open(docs_dir);
	if (dir.is_null()) {
		print_line("[AIAssistant] Could not open docs directory: " + docs_dir);
		return "";
	}

	// Recursively collect all .md files.
	Vector<String> md_files;
	Vector<String> dirs_to_scan;
	dirs_to_scan.push_back(docs_dir);

	while (dirs_to_scan.size() > 0) {
		String current_dir = dirs_to_scan[dirs_to_scan.size() - 1];
		dirs_to_scan.remove_at(dirs_to_scan.size() - 1);

		Ref<DirAccess> d = DirAccess::open(current_dir);
		if (d.is_null()) {
			continue;
		}
		d->list_dir_begin();
		String entry = d->get_next();
		while (!entry.is_empty()) {
			String full_path = current_dir.path_join(entry);
			if (d->current_is_dir()) {
				if (entry != "." && entry != "..") {
					dirs_to_scan.push_back(full_path);
				}
			} else if (entry.ends_with(".md")) {
				md_files.push_back(full_path);
			}
			entry = d->get_next();
		}
		d->list_dir_end();
	}

	// Sort for deterministic order.
	md_files.sort();

	// Read and combine all files.
	String combined = "[CODING STANDARDS] Follow these rules when generating or modifying GDScript game code.\n\n";

	for (int i = 0; i < md_files.size(); i++) {
		Ref<FileAccess> f = FileAccess::open(md_files[i], FileAccess::READ);
		if (f.is_valid()) {
			String relative = md_files[i].replace(docs_dir + "/", "");
			combined += "=== " + relative + " ===\n";
			combined += f->get_as_text();
			combined += "\n\n";
			print_line("[AIAssistant] Loaded coding standard: " + relative);
		}
	}

	if (md_files.size() == 0) {
		print_line("[AIAssistant] No .md files found in: " + docs_dir);
		return "";
	}

	print_line("[AIAssistant] Loaded " + itos(md_files.size()) + " coding standard files from " + docs_dir);
	_cached_coding_standards = combined;
	return _cached_coding_standards;
}

Vector<String> AIAssistantDock::_get_headers_with_directory() const {
	Vector<String> headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back("Accept: application/json");
	headers.push_back("x-opencode-directory: " + _get_project_directory());
	return headers;
}

void AIAssistantDock::_check_service_health() {
	if (pending_request != REQUEST_NONE) {
		return;
	}

	pending_request = REQUEST_HEALTH;
	connection_status = CONNECTING;
	_update_status("Checking service...", Color(1, 1, 0));
	_update_connection_indicator();

	String url = service_url + "/global/health";
	Vector<String> headers = _get_headers_with_directory();
	http_request->request(url, headers);
}

void AIAssistantDock::_create_session() {
	if (pending_request != REQUEST_NONE) {
		return;
	}

	pending_request = REQUEST_SESSION;
	_update_status("Creating session...", Color(1, 1, 0));

	String url = service_url + "/session?directory=" + _get_project_directory().uri_encode();
	Dictionary body;
	// Session creation body (can be empty, directory is in header/query)

	String json_body = JSON::stringify(body);
	http_request->request(url, _get_headers_with_directory(), HTTPClient::METHOD_POST, json_body);
}

void AIAssistantDock::_fetch_config() {
	if (pending_request != REQUEST_NONE) {
		return;
	}

	pending_request = REQUEST_CONFIG;
	_update_status("Loading config...", Color(1, 1, 0));

	String url = service_url + "/config?directory=" + _get_project_directory().uri_encode();
	http_request->request(url, _get_headers_with_directory());
}

void AIAssistantDock::_send_message(const String &p_content) {
	if (pending_request != REQUEST_NONE || session_id.is_empty()) {
		_process_local_command(p_content);
		return;
	}

	pending_request = REQUEST_MESSAGE;
	_update_status("Processing...", Color(1, 1, 0));
	_show_processing();

	// Log user prompt to Logs tab
	String prompt_preview = p_content.substr(0, 200);
	if (p_content.length() > 200) {
		prompt_preview += "...";
	}
	_add_log_entry("USER", prompt_preview, Color(0.4, 0.7, 1.0));

	// Start polling for intermediate steps (tool activities)
	_start_stream_polling("");

	// Use prompt_async endpoint which returns immediately (204 No Content)
	// The AI processes in background and we poll for the response
	String url = service_url + "/session/" + session_id + "/prompt_async?directory=" + _get_project_directory().uri_encode();

	// Build parts array for the message
	Array parts;

	// Inject coding standards as a separate part on first message of each session.
	// Sent as its own part so it's hidden from the chat UI (filtered in _add_user_message).
	if (!coding_standards_injected) {
		String standards = _load_coding_standards();
		if (!standards.is_empty()) {
			coding_standards_injected = true;
			Dictionary standards_part;
			standards_part["type"] = "text";
			standards_part["text"] = standards;
			parts.push_back(standards_part);
		}
	}

	// Build the user message with mode prefixes
	String message_content = p_content;
	if (is_plan_mode) {
		message_content = "[PLAN MODE] Before making any changes, first create a detailed plan and present it for approval. Do not make changes until the plan is approved.\n\n" + message_content;
	}
	if (is_auto_accept) {
		message_content = "[AUTO-ACCEPT] You have permission to automatically apply all file edits without asking for confirmation.\n\n" + message_content;
	}

	Dictionary text_part;
	text_part["type"] = "text";
	text_part["text"] = message_content;
	parts.push_back(text_part);

	Dictionary body;
	body["sessionID"] = session_id;
	body["parts"] = parts;

	// Include selected model so OpenCode uses the right provider/model
	if (!selected_provider_id.is_empty() && !selected_model_id.is_empty()) {
		Dictionary model;
		model["providerID"] = selected_provider_id;
		model["modelID"] = selected_model_id;
		body["model"] = model;
	}

	String json_body = JSON::stringify(body);
	http_request->request(url, _get_headers_with_directory(), HTTPClient::METHOD_POST, json_body);
}

void AIAssistantDock::_on_http_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	RequestType request_type = pending_request;

	// Handle prompt_async endpoint which returns 204 No Content
	// The AI processes in background and we poll for the response via stream polling
	if (request_type == REQUEST_MESSAGE && p_result == HTTPRequest::RESULT_SUCCESS && p_code == 204) {
		// Don't reset pending_request - let stream polling handle completion
		// Keep processing indicator visible
		_update_status("AI is thinking...", Color(0.6, 0.8, 1.0));
		return;
	}

	pending_request = REQUEST_NONE;

	// Hide processing for non-async responses
	_hide_processing();

	if (p_result != HTTPRequest::RESULT_SUCCESS) {
		connection_status = CONNECTION_ERROR;
		_update_status("Connection failed", Color(1, 0, 0));
		_update_connection_indicator();

		String error_msg;
		switch (p_result) {
			case HTTPRequest::RESULT_CHUNKED_BODY_SIZE_MISMATCH:
				error_msg = "Chunked body size mismatch";
				break;
			case HTTPRequest::RESULT_CANT_CONNECT:
				error_msg = "Can't connect to host";
				break;
			case HTTPRequest::RESULT_CANT_RESOLVE:
				error_msg = "Can't resolve hostname";
				break;
			case HTTPRequest::RESULT_CONNECTION_ERROR:
				error_msg = "Connection error";
				break;
			case HTTPRequest::RESULT_TLS_HANDSHAKE_ERROR:
				error_msg = "TLS handshake error";
				break;
			case HTTPRequest::RESULT_NO_RESPONSE:
				error_msg = "No response from server";
				break;
			case HTTPRequest::RESULT_BODY_SIZE_LIMIT_EXCEEDED:
				error_msg = "Body size limit exceeded";
				break;
			case HTTPRequest::RESULT_BODY_DECOMPRESS_FAILED:
				error_msg = "Body decompress failed";
				break;
			case HTTPRequest::RESULT_REQUEST_FAILED:
				error_msg = "Request failed";
				break;
			case HTTPRequest::RESULT_DOWNLOAD_FILE_CANT_OPEN:
				error_msg = "Can't open download file";
				break;
			case HTTPRequest::RESULT_DOWNLOAD_FILE_WRITE_ERROR:
				error_msg = "Download file write error";
				break;
			case HTTPRequest::RESULT_REDIRECT_LIMIT_REACHED:
				error_msg = "Redirect limit reached";
				break;
			case HTTPRequest::RESULT_TIMEOUT:
				error_msg = "Request timeout";
				break;
			default:
				error_msg = "Unknown error (code: " + String::num_int64(p_result) + ")";
				break;
		}

		if (request_type == REQUEST_HEALTH) {
			_add_system_message("Cannot connect to AI service: " + error_msg + "\nMake sure it's running on " + service_url);
		} else {
			_add_system_message("Request failed: " + error_msg);
		}
		return;
	}

	String response_text = String::utf8((const char *)p_body.ptr(), p_body.size());
	JSON json;
	Error err = json.parse(response_text);

	if (err != OK) {
		_add_system_message("Invalid response from server");
		return;
	}

	Dictionary response_data = json.get_data();

	switch (request_type) {
		case REQUEST_HEALTH: {
			if (p_code == 200 && response_data.has("healthy") && bool(response_data["healthy"]) == true) {
				_add_system_message("AI service is running. Looking for existing session...");
				_find_existing_session();
			} else {
				connection_status = CONNECTION_ERROR;
				_update_status("Service error", Color(1, 0, 0));
				_update_connection_indicator();
				_add_system_message("AI service returned unexpected response");
			}
		} break;

		case REQUEST_SESSION: {
			if (p_code == 200 && response_data.has("id")) {
				session_id = response_data["id"];
				coding_standards_injected = false;
				connection_status = CONNECTED;
				_update_status("Connected", Color(0, 1, 0));
				_update_connection_indicator();
				reconnect_button->set_text("Disconnect");
				_add_system_message("Connected to AI service! Session: " + session_id.substr(0, 8) + "...");
				_add_system_message("Project directory: " + _get_project_directory());

				// Start log polling
				if (logs_poll_timer) {
					logs_poll_timer->start();
				}
				// Start command polling (for auto-run from OpenCode)
				if (command_poll_timer) {
					command_poll_timer->start();
				}
				// Start question polling (AI asking user questions)
				if (question_poll_timer) {
					question_poll_timer->start();
				}
				// Fetch config to get saved model, then fetch available models
				_fetch_config();
			} else {
				connection_status = CONNECTION_ERROR;
				_update_status("Session failed", Color(1, 0, 0));
				_update_connection_indicator();
				_add_system_message("Failed to create session");
			}
		} break;

		case REQUEST_CONFIG: {
			if (p_code == 200) {
				// Extract saved model from config
				if (response_data.has("model")) {
					String saved_model = response_data["model"];
					int slash = saved_model.find("/");
					if (slash >= 0) {
						selected_provider_id = saved_model.substr(0, slash);
						selected_model_id = saved_model.substr(slash + 1);
					}
					print_line("AIAssistant: Loaded saved model from config: " + saved_model);
				}
			}
			// Always fetch available providers/models after config (even if config failed)
			_fetch_providers();
		} break;

		case REQUEST_MESSAGE: {
			_hide_processing();
			_update_status("Connected", Color(0, 1, 0));

			// OpenCode returns { info: {...}, parts: [...] }
			// Extract text from parts array
			if (response_data.has("parts")) {
				Array parts = response_data["parts"];
				String full_response;
				for (int i = 0; i < parts.size(); i++) {
					Dictionary part = parts[i];
					String type = part.get("type", "");
					if (type == "text") {
						String text = part.get("text", "");
						if (!text.is_empty()) {
							if (!full_response.is_empty()) {
								full_response += "\n";
							}
							full_response += text;
						}
					}
					// Note: Tool messages are now shown via stream polling during processing
					// We skip showing them again here to avoid duplication
				}
				if (!full_response.is_empty()) {
					_add_ai_message(full_response);
				}
			} else if (response_data.has("error")) {
				_add_system_message("Error: " + String(response_data["error"]));
			} else {
				// Fallback - show raw response for debugging
				_add_system_message("Received response (checking format...)");
			}
		} break;

		default:
			break;
	}
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

void AIAssistantDock::_on_stop_pressed() {
	if (session_id.is_empty()) {
		return;
	}

	_add_system_message("Stopping AI processing...");

	// Call the abort API
	HTTPRequest *abort_request = memnew(HTTPRequest);
	add_child(abort_request);
	abort_request->connect("request_completed", callable_mp((Node *)abort_request, &Node::queue_free).unbind(4));

	String url = service_url + "/session/" + session_id + "/abort?directory=" + _get_project_directory().uri_encode();
	abort_request->request(url, _get_headers_with_directory(), HTTPClient::METHOD_POST, "{}");

	// Hide processing and stop polling
	_hide_processing();
	pending_request = REQUEST_NONE;
	_update_status("Connected", Color(0, 1, 0));
	_add_system_message("AI processing stopped.");
}

void AIAssistantDock::_on_plan_mode_toggled(bool p_enabled) {
	is_plan_mode = p_enabled;
	if (p_enabled) {
		_add_system_message("[Mode] Plan mode enabled - AI will create a plan before making changes.");
	} else {
		_add_system_message("[Mode] Plan mode disabled.");
	}
}

void AIAssistantDock::_on_auto_accept_toggled(bool p_enabled) {
	is_auto_accept = p_enabled;
	if (p_enabled) {
		_add_system_message("[Mode] Auto-accept enabled - File edits will be automatically approved.");
	} else {
		_add_system_message("[Mode] Auto-accept disabled.");
	}
}

void AIAssistantDock::_on_clear_pressed() {
	while (chat_container->get_child_count() > 0) {
		Node *child = chat_container->get_child(0);
		chat_container->remove_child(child);
		memdelete(child);
	}
	chat_history.clear();
	_clear_tool_tracking();
	_add_system_message("Chat cleared.");
}

void AIAssistantDock::_on_reconnect_pressed() {
	if (connection_status == CONNECTED) {
		// Stop log polling
		if (logs_poll_timer) {
			logs_poll_timer->stop();
		}
		// Stop command polling
		if (command_poll_timer) {
			command_poll_timer->stop();
		}
		// Stop question polling
		if (question_poll_timer) {
			question_poll_timer->stop();
		}
		// Hide any pending question dialog
		_hide_question_dialog();
		// Disconnect
		session_id = "";
		coding_standards_injected = false;
		connection_status = DISCONNECTED;
		_update_status("Disconnected", Color(0.5, 0.5, 0.5));
		_update_connection_indicator();
		reconnect_button->set_text("Connect");
		_add_system_message("Disconnected from AI service.");
	} else {
		// Connect
		_add_system_message("Connecting to " + service_url + "...");
		_check_service_health();
	}
}

void AIAssistantDock::_on_template_selected(int p_id) {
	String templates[] = { "platformer_2d", "tower_defense", "rpg_topdown", "shooter_topdown" };

	if (p_id < 4) {
		_add_system_message("Loading template: " + templates[p_id]);
		// TODO: Implement template loading via AI
		if (connection_status == CONNECTED) {
			_send_message("Create a " + templates[p_id] + " game template");
		}
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
	if (connection_status == CONNECTED && !session_id.is_empty()) {
		_send_message(p_prompt);
	} else {
		_process_local_command(p_prompt);
	}
}

void AIAssistantDock::_process_local_command(const String &p_prompt) {
	String lower = p_prompt.to_lower();

	if (lower.contains("help")) {
		_add_ai_message("**Makabaka AI Assistant Help**\n\n**Quick Commands:**\n- run / play - Run the project\n- stop - Stop the project\n- save - Save the current scene\n\n**Full AI Features:**\nClick 'Connect' to connect to the AI service (http://localhost:4096).\n\nMake sure the AI server is running with:\n[code]start_makabaka.bat[/code]");
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
		_add_ai_message("I understand you want to: \"" + p_prompt + "\"\n\nI'm currently offline. Click 'Connect' to connect to the AI service for full natural language game creation.");
	}

	_update_status("Offline", Color(1, 0.5, 0));
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

	_update_status("Connected", Color(0, 1, 0));
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

	// Format message with role inline: "[Role] message text"
	// Use BBCode to color the role label
	String role_hex = p_color.to_html(false);
	String formatted_text = "[color=#" + role_hex + "][b]" + p_sender + ":[/b][/color] " + p_text;

	RichTextLabel *text_label = memnew(RichTextLabel);
	text_label->set_use_bbcode(true);
	text_label->set_fit_content(true);
	text_label->set_text(formatted_text);
	text_label->set_selection_enabled(true);
	text_label->set_context_menu_enabled(true);
	text_label->set_focus_mode(Control::FOCUS_CLICK);
	msg_container->add_child(text_label);

	msg_container->add_child(memnew(HSeparator));

	chat_container->add_child(msg_container);

	// Scroll to bottom if auto-scroll is enabled
	if (chat_auto_scroll && chat_auto_scroll->is_pressed()) {
		callable_mp(chat_scroll, &ScrollContainer::set_v_scroll).call_deferred(INT32_MAX);
	}
}

void AIAssistantDock::_add_user_message(const String &p_text) {
	// Skip injected coding standards — they're internal context, not user-visible.
	if (p_text.begins_with("[CODING STANDARDS]")) {
		return;
	}
	_add_message("You", p_text, Color(0.4, 0.6, 1.0));
}

void AIAssistantDock::_add_ai_message(const String &p_text) {
	_add_message("AI", p_text, Color(0.4, 1.0, 0.6));
}

void AIAssistantDock::_add_system_message(const String &p_text) {
	_add_message("System", p_text, Color(1.0, 1.0, 0.6));
}

void AIAssistantDock::_add_tool_message(const String &p_part_id, const String &p_tool_name, const String &p_status, const Dictionary &p_details) {
	// Track start time for new tools
	if (!tool_start_times.has(p_part_id)) {
		tool_start_times[p_part_id] = Time::get_singleton()->get_ticks_msec();
	}

	// Calculate elapsed time
	uint64_t elapsed_ms = Time::get_singleton()->get_ticks_msec() - tool_start_times[p_part_id];
	float elapsed_sec = elapsed_ms / 1000.0f;

	// Helper lambda to format tool details
	auto format_tool_details = [](const String &tool_name, const String &status, const Dictionary &details, float elapsed) -> String {
		// Determine status color
		String status_color;
		if (status == "completed") {
			status_color = "66ff66"; // Green
		} else if (status == "running" || status == "pending") {
			status_color = "ffff66"; // Yellow
		} else if (status == "error") {
			status_color = "ff6666"; // Red
		} else {
			status_color = "888888"; // Gray
		}

		// Build tool message: "Tool: tool_name [status] (Xs)"
		String time_str = vformat("%.1fs", elapsed);
		String formatted = "[color=#9999ff][b]Tool:[/b][/color] " + tool_name + " [color=#" + status_color + "][" + status + "][/color] [color=#888888](" + time_str + ")[/color]";

		// Special handling for task tool (subagent)
		if (tool_name == "task") {
			String agent_type;
			String description;
			if (details.has("input")) {
				Dictionary input = details["input"];
				agent_type = input.get("subagent_type", "");
				description = input.get("description", "");
			}
			// For task tool, show agent type and description
			if (!agent_type.is_empty()) {
				formatted += " [color=#ff99ff]@" + agent_type + "[/color]";
			}
			if (!description.is_empty()) {
				formatted += " [color=#aaaaaa]" + description + "[/color]";
			}
			// Show detailed subagent tools if available (from metadata)
			if (details.has("metadata")) {
				Dictionary metadata = details["metadata"];
				if (metadata.has("summary")) {
					Array summary = metadata["summary"];
					if (summary.size() > 0) {
						// Show each subagent tool with details
						for (int i = 0; i < summary.size(); i++) {
							Dictionary tool_info = summary[i];
							String sub_tool = tool_info.get("tool", "?");
							Dictionary sub_state = tool_info.get("state", Dictionary());
							String sub_status = sub_state.get("status", "?");
							String sub_title = sub_state.get("title", "");

							// Determine status color for subagent tool
							String sub_status_color;
							if (sub_status == "completed") {
								sub_status_color = "66ff66";
							} else if (sub_status == "running" || sub_status == "pending") {
								sub_status_color = "ffff66";
							} else if (sub_status == "error") {
								sub_status_color = "ff6666";
							} else {
								sub_status_color = "888888";
							}

							// Format: "  > tool_name [status] title" (using ASCII for compatibility)
							formatted += "\n[color=#888888]  >[/color] [color=#9999cc]" + sub_tool + "[/color]";
							formatted += " [color=#" + sub_status_color + "][" + sub_status + "][/color]";
							if (!sub_title.is_empty()) {
								// Truncate long titles
								if (sub_title.length() > 60) {
									sub_title = sub_title.substr(0, 60) + "...";
								}
								formatted += " [color=#aaaaaa]" + sub_title + "[/color]";
							}
						}
					}
				}
			}
			return formatted;
		}

		// Extract brief info from input for display.
		// OpenCode tool input keys use camelCase.
		// The "input" dict may be empty during "pending" state;
		// fall back to parsing the "raw" JSON string if available.
		String input_info;
		Dictionary in;
		if (details.has("input")) {
			Variant input_var = details["input"];
			if (input_var.get_type() == Variant::DICTIONARY) {
				in = input_var;
			}
		}
		// If input dict is empty, try parsing "raw" field (JSON string of the input)
		if (in.is_empty() && details.has("raw")) {
			Variant raw_var = details["raw"];
			if (raw_var.get_type() == Variant::STRING) {
				String raw_str = raw_var;
				if (!raw_str.is_empty()) {
					Variant parsed = JSON::parse_string(raw_str);
					if (parsed.get_type() == Variant::DICTIONARY) {
						in = parsed;
					}
				}
			}
		}
		if (!in.is_empty()) {
			// --- File tools: read, write, edit, multiedit ---
			if (in.has("filePath")) {
				input_info = in["filePath"];
			}
			// --- bash ---
			else if (in.has("command")) {
				// Prefer description (human-readable) over raw command
				if (in.has("description")) {
					input_info = in["description"];
				} else {
					input_info = in["command"];
				}
			}
			// --- glob, grep ---
			else if (in.has("pattern")) {
				input_info = in["pattern"];
				if (in.has("path")) {
					input_info += " in " + String(in["path"]);
				}
			}
			// --- websearch, codesearch ---
			else if (in.has("query")) {
				input_info = in["query"];
			}
			// --- webfetch ---
			else if (in.has("url")) {
				input_info = in["url"];
			}
			// --- lsp ---
			else if (in.has("operation")) {
				input_info = in["operation"];
				if (in.has("filePath")) {
					input_info += " " + String(in["filePath"]);
				}
			}
			// --- skill ---
			else if (in.has("name")) {
				input_info = in["name"];
			}
			// --- list ---
			else if (in.has("path")) {
				input_info = in["path"];
			}
			// --- todowrite ---
			else if (in.has("todos")) {
				Variant todos_var = in["todos"];
				if (todos_var.get_type() == Variant::ARRAY) {
					Array todos = todos_var;
					int completed_count = 0;
					int in_progress_count = 0;
					int pending_count = 0;
					String active_task;
					for (int ti = 0; ti < todos.size(); ti++) {
						Dictionary todo = todos[ti];
						String todo_status = todo.get("status", "");
						if (todo_status == "completed") {
							completed_count++;
						} else if (todo_status == "in_progress") {
							in_progress_count++;
							if (active_task.is_empty()) {
								active_task = todo.get("activeForm", todo.get("content", ""));
							}
						} else {
							pending_count++;
						}
					}
					// Show active task and progress summary
					if (!active_task.is_empty()) {
						input_info = active_task;
					}
					String progress = itos(completed_count) + "/" + itos(todos.size()) + " done";
					if (!input_info.is_empty()) {
						input_info += " (" + progress + ")";
					} else {
						input_info = progress;
					}
				}
			}
			// --- batch ---
			else if (in.has("tool_calls")) {
				Variant tc = in["tool_calls"];
				if (tc.get_type() == Variant::ARRAY) {
					input_info = itos(((Array)tc).size()) + " tool calls";
				}
			}
			// --- apply_patch ---
			else if (in.has("patchText")) {
				String patch = in["patchText"];
				int nl = patch.find("\n");
				input_info = (nl >= 0) ? patch.substr(0, nl) : patch;
			}
			// --- Fallback: first string or array value ---
			else {
				Array keys = in.keys();
				for (int i = 0; i < keys.size(); i++) {
					Variant val = in[keys[i]];
					if (val.get_type() == Variant::STRING && !String(val).is_empty()) {
						input_info = val;
						break;
					} else if (val.get_type() == Variant::ARRAY) {
						input_info = itos(((Array)val).size()) + " items";
						break;
					}
				}
			}
		}

		// For pending: show "preparing..." if no input yet (input comes later with running state)
		// For running: show input info
		// For completed: show title (more human-readable)
		// For error: show input info + full error on new line
		if (status == "running") {
			// Running tools should have input
			if (!input_info.is_empty()) {
				if (input_info.length() > 80) {
					input_info = input_info.substr(0, 80) + "...";
				}
				formatted += " [color=#aaaaaa]" + input_info + "[/color]";
			}
		} else if (status == "completed") {
			// Show input info first (pattern, file path, command — most specific)
			// Then append title if it adds extra context (e.g. result count)
			String title;
			if (details.has("title")) {
				title = details["title"];
			}
			if (!input_info.is_empty()) {
				if (input_info.length() > 80) {
					input_info = input_info.substr(0, 80) + "...";
				}
				formatted += " [color=#aaaaaa]" + input_info + "[/color]";
				// Append title only if different from input_info (adds extra info)
				if (!title.is_empty() && title != input_info && !title.begins_with(input_info)) {
					if (title.length() > 60) {
						title = title.substr(0, 60) + "...";
					}
					formatted += " [color=#888888](" + title + ")[/color]";
				}
			} else if (!title.is_empty()) {
				if (title.length() > 80) {
					title = title.substr(0, 80) + "...";
				}
				formatted += " [color=#aaaaaa]" + title + "[/color]";
			}
		} else if (status == "error") {
			// Show input info on first line
			if (!input_info.is_empty()) {
				if (input_info.length() > 80) {
					input_info = input_info.substr(0, 80) + "...";
				}
				formatted += " [color=#aaaaaa]" + input_info + "[/color]";
			}
			// Show full error on new line
			if (details.has("error")) {
				String error = details["error"];
				if (!error.is_empty()) {
					formatted += "\n[color=#ff6666]Error: " + error + "[/color]";
				}
			}
		}

		return formatted;
	};

	// Check if we already have a UI element for this tool part
	if (tool_containers.has(p_part_id)) {
		// Update existing tool's status, time, and output
		RichTextLabel *tool_label = tool_containers[p_part_id];
		if (tool_label) {
			tool_label->set_text(format_tool_details(p_tool_name, p_status, p_details, elapsed_sec));
		}
		return;
	}

	// Create a new tool message container
	VBoxContainer *container = memnew(VBoxContainer);

	// Create the tool message label (same style as regular messages)
	RichTextLabel *tool_label = memnew(RichTextLabel);
	tool_label->set_use_bbcode(true);
	tool_label->set_fit_content(true);
	tool_label->set_text(format_tool_details(p_tool_name, p_status, p_details, elapsed_sec));
	tool_label->set_selection_enabled(true);
	tool_label->set_context_menu_enabled(true);
	tool_label->set_focus_mode(Control::FOCUS_CLICK);
	container->add_child(tool_label);

	container->add_child(memnew(HSeparator));

	// Store reference for later updates (store the RichTextLabel directly)
	tool_containers[p_part_id] = tool_label;

	chat_container->add_child(container);

	// Scroll to bottom if auto-scroll is enabled
	if (chat_auto_scroll && chat_auto_scroll->is_pressed()) {
		callable_mp(chat_scroll, &ScrollContainer::set_v_scroll).call_deferred(INT32_MAX);
	}
}

void AIAssistantDock::_clear_tool_tracking() {
	tool_containers.clear();
	tool_start_times.clear();
	tool_logged_status.clear();
}

void AIAssistantDock::_update_status(const String &p_text, const Color &p_color) {
	if (status_label) {
		status_label->set_text(p_text);
		status_label->add_theme_color_override("font_color", p_color);
	}
}

void AIAssistantDock::_show_processing() {
	if (processing_label) {
		processing_label->set_visible(true);
		processing_dots = 0;
		processing_label->set_text("AI is thinking");
		processing_timer->start();
	}
	if (send_button) {
		send_button->set_disabled(true);
	}
	if (stop_button) {
		stop_button->set_visible(true);
	}
}

void AIAssistantDock::_hide_processing() {
	if (processing_label) {
		processing_label->set_visible(false);
		processing_timer->stop();
		processing_dots = 0;
	}
	if (send_button) {
		send_button->set_disabled(false);
	}
	if (stop_button) {
		stop_button->set_visible(false);
	}
	// Stop stream polling when processing is done
	_stop_stream_polling();
}

void AIAssistantDock::_on_processing_timer_timeout() {
	processing_dots = (processing_dots + 1) % 4;
	String dots = "";
	for (int i = 0; i < processing_dots; i++) {
		dots += ".";
	}
	processing_label->set_text("AI is thinking" + dots);
}

// === Stream Polling (for intermediate AI steps) ===

void AIAssistantDock::_start_stream_polling(const String &p_message_id) {
	// p_message_id can be empty - we poll messages list to find the latest assistant message
	current_message_id = p_message_id;
	last_part_count = 0;
	// Clear tool tracking for new message stream
	_clear_tool_tracking();
	if (stream_poll_timer) {
		stream_poll_timer->start();
	}
}

void AIAssistantDock::_stop_stream_polling() {
	if (stream_poll_timer) {
		stream_poll_timer->stop();
	}
	current_message_id = "";
	last_part_count = 0;
	stream_request_in_progress = false;
}

void AIAssistantDock::_on_stream_poll_timeout() {
	if (session_id.is_empty()) {
		_stop_stream_polling();
		return;
	}

	// Skip if a request is already in progress
	if (stream_request_in_progress) {
		return;
	}

	// Fetch all messages to find the latest assistant message with in-progress parts
	// We use limit=2 to get just the recent messages (user + assistant response)
	String url = service_url + "/session/" + session_id + "/message?limit=5&directory=" + _get_project_directory().uri_encode();
	stream_request_in_progress = true;
	stream_http_request->request(url, _get_headers_with_directory());
}

void AIAssistantDock::_on_stream_http_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	stream_request_in_progress = false;

	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		// Don't stop polling on error, messages may not be ready yet
		return;
	}

	String response_text = String::utf8((const char *)p_body.ptr(), p_body.size());
	JSON json;
	Error err = json.parse(response_text);

	if (err != OK) {
		return;
	}

	Variant stream_data = json.get_data();
	if (stream_data.get_type() != Variant::ARRAY) {
		return;
	}

	Array messages = stream_data;

	// Find the latest assistant message
	Dictionary latest_assistant;
	for (int i = messages.size() - 1; i >= 0; i--) {
		Dictionary msg = messages[i];
		if (msg.has("info")) {
			Dictionary info = msg["info"];
			String role = info.get("role", "");
			if (role == "assistant") {
				latest_assistant = msg;
				break;
			}
		}
	}

	if (latest_assistant.is_empty() || !latest_assistant.has("parts")) {
		return;
	}

	// Track message ID to detect if it changed
	Dictionary info = latest_assistant["info"];
	String msg_id = info.get("id", "");
	if (msg_id != current_message_id) {
		// New message, reset part count
		current_message_id = msg_id;
		last_part_count = 0;
	}

	Array parts = latest_assistant["parts"];
	int current_count = parts.size();

	// Process all tool parts to update their status (tools can change from pending -> running -> completed)
	// Also process new parts for logging
	for (int i = 0; i < current_count; i++) {
		Dictionary part = parts[i];
		String type = part.get("type", "");

		if (type == "tool") {
			String part_id = part.get("id", "");
			String tool_name = part.get("tool", "unknown");
			Dictionary state = part.get("state", Dictionary());
			String status = state.get("status", "unknown");


			// Skip showing question tool in chat - it's handled via question dialog
			if (tool_name == "question") {
				continue;
			}

			// Skip pending tools — input is empty during pending state.
			// Only show tools once they transition to running/completed/error.
			if (status == "pending") {
				continue;
			}

			// Always update tool message (will create if new, update if exists)
			_add_tool_message(part_id, tool_name, status, state);

			// Log to Logs tab when tool is new or status changed
			String *prev_status = tool_logged_status.getptr(part_id);
			if (!prev_status || *prev_status != status) {
				// Rescan filesystem when file-modifying tools complete
				if (status == "completed" && (!prev_status || *prev_status != "completed")) {
					if (tool_name == "write" || tool_name == "edit" || tool_name == "bash" || tool_name == "create_file") {
						EditorFileSystem::get_singleton()->scan_changes();
					}
				}
				tool_logged_status[part_id] = status;

				// Build input summary for the log
				String input_summary;
				Dictionary input = state.get("input", Dictionary());
				if (input.has("file_path")) {
					input_summary = String(input["file_path"]);
				} else if (input.has("pattern")) {
					input_summary = String(input["pattern"]);
				} else if (input.has("command")) {
					String cmd = String(input["command"]);
					if (cmd.length() > 80) {
						cmd = cmd.substr(0, 80) + "...";
					}
					input_summary = cmd;
				}

				String log_msg = "Tool: " + tool_name + " [" + status + "]";
				if (!input_summary.is_empty()) {
					log_msg += " " + input_summary;
				}
				if (status == "completed" && state.has("output")) {
					String output = state.get("output", "");
					if (output.length() > 100) {
						output = output.substr(0, 100) + "...";
					}
					log_msg += " → " + output;
				}
				_add_log_entry("TOOL", log_msg, Color(0.6, 0.7, 0.9));
			}
		} else if (type == "text") {
			String part_id = part.get("id", "");
			String text = String(part.get("text", "")).strip_edges();
			if (text.is_empty()) {
				continue;
			}

			String text_key = part_id + "_text_stream";
			if (tool_containers.has(text_key)) {
				// Update existing streaming text label
				RichTextLabel *text_label = tool_containers[text_key];
				if (text_label) {
					String formatted = "[color=#66ff99][b]AI:[/b][/color] " + text;
					text_label->set_text(formatted);
				}
			} else {
				// Create a new streaming text label
				VBoxContainer *msg_container = memnew(VBoxContainer);
				RichTextLabel *text_label = memnew(RichTextLabel);
				text_label->set_use_bbcode(true);
				text_label->set_fit_content(true);
				String formatted = "[color=#66ff99][b]AI:[/b][/color] " + text;
				text_label->set_text(formatted);
				text_label->set_selection_enabled(true);
				text_label->set_context_menu_enabled(true);
				text_label->set_focus_mode(Control::FOCUS_CLICK);
				msg_container->add_child(text_label);
				msg_container->add_child(memnew(HSeparator));
				chat_container->add_child(msg_container);
				tool_containers[text_key] = text_label;

				// Log first appearance
				String preview = text.substr(0, 150);
				if (text.length() > 150) {
					preview += "...";
				}
				_add_log_entry("LLM", preview, Color(0.5, 0.9, 0.5));
			}

			// Auto-scroll
			if (chat_auto_scroll && chat_auto_scroll->is_pressed()) {
				callable_mp(chat_scroll, &ScrollContainer::set_v_scroll).call_deferred(INT32_MAX);
			}
		}
	}
	last_part_count = current_count;

	// Check if message is complete (time.completed exists in the info)
	Dictionary time_info = info.get("time", Dictionary());
	if (time_info.has("completed")) {
		// Message is complete! Show final text and update all tool parts with their final state
		for (int i = 0; i < parts.size(); i++) {
			Dictionary part = parts[i];
			String type = part.get("type", "");
			if (type == "text") {
				// Final update to streaming text label with complete text
				String part_id = part.get("id", "");
				String text = String(part.get("text", "")).strip_edges();
				if (!text.is_empty()) {
					String text_key = part_id + "_text_stream";
					if (tool_containers.has(text_key)) {
						// Update existing streaming label with final text
						RichTextLabel *text_label = tool_containers[text_key];
						if (text_label) {
							String formatted = "[color=#66ff99][b]AI:[/b][/color] " + text;
							text_label->set_text(formatted);
						}
					} else {
						// Text was never streamed (edge case), add it now
						_add_ai_message(text);
					}
				}
			} else if (type == "tool") {
				// Update tool with final state (should now be completed or error)
				String part_id = part.get("id", "");
				String tool_name = part.get("tool", "unknown");
				// Skip question tool - handled via question dialog
				if (tool_name == "question") {
					continue;
				}
				Dictionary state = part.get("state", Dictionary());
				String status = state.get("status", "unknown");
				_add_tool_message(part_id, tool_name, status, state);
			}
		}

		// Check for errors
		if (info.has("error")) {
			Dictionary error = info["error"];
			String error_name = error.get("name", "unknown");
			// The actual message is nested in error.data.message
			String error_msg;
			if (error.has("data") && Dictionary(error["data"]).has("message")) {
				Dictionary data = error["data"];
				error_msg = String(data["message"]);
			} else {
				error_msg = error.get("message", "An error occurred");
			}
			_add_system_message("Error (" + error_name + "): " + error_msg);
			_add_log_entry("ERROR", error_name + ": " + error_msg, Color(1.0, 0.4, 0.4));
		}

		// Log completion stats and finish reason
		String finish_reason = info.get("finish", "");
		if (info.has("tokens")) {
			Dictionary tokens = info["tokens"];
			int input_tokens = tokens.get("input", 0);
			int output_tokens = tokens.get("output", 0);
			String stats_msg = vformat("Tokens: %d in / %d out", input_tokens, output_tokens);
			if (!finish_reason.is_empty()) {
				stats_msg += " | Finish: " + finish_reason;
			}
			_add_log_entry("STATS", stats_msg, Color(0.7, 0.7, 0.7));
		}

		// Check if AI made tool calls that need continuation
		// If finish reason is "tool-calls", the AI wants to continue after tools complete
		if (finish_reason == "tool-calls") {
			_add_log_entry("INFO", "AI made tool calls - waiting for tool execution...", Color(0.8, 0.8, 0.5));
			// Don't stop polling yet - tools are still executing
			return;
		}

		// Stop polling and hide processing
		_stop_stream_polling();
		_hide_processing();
		_update_status("Connected", Color(0, 1, 0));

		// Reset pending request since we're done
		pending_request = REQUEST_NONE;
	}
}

void AIAssistantDock::_on_verify_pressed() {
	if (connection_status != CONNECTED) {
		_add_system_message("Not connected to AI service. Connect first.");
		return;
	}

	_add_system_message("Reading game logs...");

	String logs = _read_game_logs();

	if (logs.is_empty()) {
		_add_system_message("No game logs found. Run the game first to generate logs.");
		return;
	}

	// Send logs to AI for verification
	String prompt = "Please analyze these game logs and verify that the game is working correctly. Check for:\n"
					"1. Are events being logged properly?\n"
					"2. Are there any errors or unexpected behaviors?\n"
					"3. Is the game logic working as expected?\n\n"
					"Game Logs:\n" + logs;

	_add_user_message("[Verify] Analyzing game logs...");
	_send_message(prompt);
}

String AIAssistantDock::_read_game_logs() {
	// Get the user data directory for the project
	String user_dir = OS::get_singleton()->get_user_data_dir();
	String logs_dir = user_dir.path_join("logs");

	// Find the most recent log file
	Ref<DirAccess> dir = DirAccess::open(logs_dir);
	if (dir.is_null()) {
		return "";
	}

	String latest_log_file;
	uint64_t latest_time = 0;

	dir->list_dir_begin();
	String filename = dir->get_next();
	while (!filename.is_empty()) {
		if (!dir->current_is_dir() && filename.ends_with(".jsonl")) {
			String full_path = logs_dir.path_join(filename);
			uint64_t mod_time = FileAccess::get_modified_time(full_path);
			if (mod_time > latest_time) {
				latest_time = mod_time;
				latest_log_file = full_path;
			}
		}
		filename = dir->get_next();
	}
	dir->list_dir_end();

	if (latest_log_file.is_empty()) {
		return "";
	}

	// Read the log file (last 50 lines)
	Ref<FileAccess> file = FileAccess::open(latest_log_file, FileAccess::READ);
	if (file.is_null()) {
		return "";
	}

	Vector<String> lines;
	while (!file->eof_reached()) {
		String line = file->get_line();
		if (!line.is_empty()) {
			lines.push_back(line);
		}
	}

	// Get last 50 lines
	int start = MAX(0, lines.size() - 50);
	String result;
	for (int i = start; i < lines.size(); i++) {
		result += lines[i] + "\n";
	}

	_add_system_message("Found " + itos(lines.size()) + " log entries (showing last " + itos(lines.size() - start) + ")");
	return result;
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

// === Logs Tab Functions ===

void AIAssistantDock::_on_logs_clear_pressed() {
	logs_text->clear();
	logs_text->set_text("[color=gray]Logs cleared.[/color]");
}

void AIAssistantDock::_on_logs_refresh_pressed() {
	_fetch_session_events();
}

void AIAssistantDock::_on_logs_poll_timeout() {
	// Skip if a request is already in progress
	if (logs_request_in_progress) {
		return;
	}

	if (connection_status == CONNECTED && !session_id.is_empty()) {
		_fetch_session_events();
	}
}

void AIAssistantDock::_fetch_session_events() {
	if (session_id.is_empty()) {
		_add_log_entry("INFO", "No active session. Connect first.", Color(1, 1, 0.5));
		return;
	}

	// Skip if a request is already in progress
	if (logs_request_in_progress) {
		return;
	}

	// Fetch session events/messages
	String url = service_url + "/session/" + session_id + "/message?directory=" + _get_project_directory().uri_encode();
	logs_request_in_progress = true;
	logs_http_request->request(url, _get_headers_with_directory());
}

void AIAssistantDock::_on_logs_http_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	logs_request_in_progress = false;

	if (p_result != HTTPRequest::RESULT_SUCCESS) {
		_add_log_entry("ERROR", "Failed to fetch logs", Color(1, 0.4, 0.4));
		return;
	}

	String response_text = String::utf8((const char *)p_body.ptr(), p_body.size());
	JSON json;
	Error err = json.parse(response_text);

	if (err != OK) {
		_add_log_entry("ERROR", "Invalid JSON response", Color(1, 0.4, 0.4));
		return;
	}

	Variant logs_data = json.get_data();

	// Handle array of messages
	if (logs_data.get_type() == Variant::ARRAY) {
		Array messages = logs_data;
		for (int i = 0; i < messages.size(); i++) {
			Dictionary msg = messages[i];

			// API returns { info: { id, role, ... }, parts: [...] }
			Dictionary info = msg.get("info", Dictionary());
			String role = info.get("role", "unknown");
			String id = info.get("id", "");

			// Skip if we've already shown this message
			if (!last_event_id.is_empty() && id <= last_event_id) {
				continue;
			}

			// Get message parts
			if (msg.has("parts")) {
				Array parts = msg["parts"];
				for (int j = 0; j < parts.size(); j++) {
					Dictionary part = parts[j];
					String type = part.get("type", "");

					if (type == "text") {
						String text = part.get("text", "");
						Color color = role == "user" ? Color(0.4, 0.6, 1.0) : Color(0.4, 1.0, 0.6);
						_add_log_entry(role.to_upper(), text.substr(0, 200) + (text.length() > 200 ? "..." : ""), color);
					} else if (type == "tool") {
						String tool_name = part.get("tool", "unknown");
						Dictionary state = part.get("state", Dictionary());
						String status = state.get("status", "unknown");
						Color color = status == "completed" ? Color(0.4, 1.0, 0.6) : (status == "error" ? Color(1.0, 0.4, 0.4) : Color(1.0, 1.0, 0.5));
						_add_log_entry("TOOL", tool_name + " [" + status + "]", color);
					}
				}
			}

			last_event_id = id;
		}
	}

	// Auto-scroll if enabled
	if (logs_auto_scroll->is_pressed()) {
		callable_mp(logs_scroll, &ScrollContainer::set_v_scroll).call_deferred(INT32_MAX);
	}
}

void AIAssistantDock::_add_log_entry(const String &p_type, const String &p_message, const Color &p_color) {
	Dictionary time = Time::get_singleton()->get_time_dict_from_system();
	String timestamp = vformat("%02d:%02d:%02d", (int)time["hour"], (int)time["minute"], (int)time["second"]);

	String color_hex = p_color.to_html(false);
	String entry = vformat("[color=gray]%s[/color] [color=%s][%s][/color] %s\n", timestamp, color_hex, p_type, p_message);

	logs_text->append_text(entry);
}

// === Command Polling (Auto-Run from OpenCode) ===

void AIAssistantDock::_on_command_poll_timeout() {
	if (connection_status != CONNECTED) {
		return;
	}

	// Skip if a request is already in progress
	if (command_request_in_progress) {
		return;
	}

	// Poll for pending commands from OpenCode
	String url = service_url + "/godot/commands?directory=" + _get_project_directory().uri_encode();
	command_request_in_progress = true;
	command_http_request->request(url, _get_headers_with_directory());
}

void AIAssistantDock::_on_command_http_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	command_request_in_progress = false;

	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		return;
	}

	String response_text = String::utf8((const char *)p_body.ptr(), p_body.size());
	JSON json;
	Error err = json.parse(response_text);

	if (err != OK) {
		return;
	}

	Variant command_data = json.get_data();
	if (command_data.get_type() != Variant::ARRAY) {
		return;
	}

	Array commands = command_data;
	for (int i = 0; i < commands.size(); i++) {
		Dictionary cmd = commands[i];
		String action = cmd.get("action", "");
		Dictionary params = cmd.get("params", Dictionary());

		if (!action.is_empty()) {
			_execute_godot_command(action, params);
		}
	}
}

void AIAssistantDock::_execute_godot_command(const String &p_action, const Dictionary &p_params) {
	if (p_action == "run") {
		// Run the game
		pending_auto_verify = true; // Mark for auto-verification when game stops
		last_debugger_error_count = 0; // Reset error counter for fresh error detection
		pending_debugger_errors.clear(); // Clear any pending errors from previous runs
		String scene = p_params.get("scene", "");
		if (scene.is_empty()) {
			EditorInterface::get_singleton()->play_main_scene();
			_add_system_message("[Auto-Test] Running main scene...");
		} else {
			EditorInterface::get_singleton()->play_custom_scene(scene);
			_add_system_message("[Auto-Test] Running scene: " + scene);
		}
		// Report status to OpenCode
		_report_game_status(true);
	} else if (p_action == "stop") {
		// Stop the game
		if (EditorInterface::get_singleton()->is_playing_scene()) {
			EditorInterface::get_singleton()->stop_playing_scene();
			_add_system_message("[Auto-Test] Stopped game.");
		}
		// Report status to OpenCode
		_report_game_status(false);
	}
}

void AIAssistantDock::_report_game_status(bool p_running) {
	// Report game running status back to OpenCode
	String url = service_url + "/godot/status?directory=" + _get_project_directory().uri_encode();

	Dictionary body;
	body["running"] = p_running;
	if (p_running) {
		body["scene"] = EditorInterface::get_singleton()->get_playing_scene();
	}

	String json_body = JSON::stringify(body);

	// Use a one-off request (we don't need to track the response)
	HTTPRequest *status_request = memnew(HTTPRequest);
	add_child(status_request);
	status_request->request(url, _get_headers_with_directory(), HTTPClient::METHOD_POST, json_body);

	// Clean up the request after it completes
	status_request->connect("request_completed", callable_mp((Node *)status_request, &Node::queue_free).unbind(4));
}

// === Auto-Verification Functions ===

void AIAssistantDock::_on_game_stopped() {
	// Reset error counter for next run
	last_debugger_error_count = 0;

	if (!pending_auto_verify) {
		return; // Only verify if game was started by AI
	}
	pending_auto_verify = false;

	// Small delay to ensure logs are flushed to disk
	SceneTree *tree = get_tree();
	if (tree) {
		tree->create_timer(1.0)->connect("timeout", callable_mp(this, &AIAssistantDock::_auto_verify_game_logs));
	}
}

void AIAssistantDock::_auto_verify_game_logs() {
	if (connection_status != CONNECTED) {
		_add_system_message("[Auto-Verify] Not connected to AI service.");
		return;
	}

	String logs = _read_game_logs();
	if (logs.is_empty()) {
		_add_system_message("[Auto-Verify] No game logs found.");
		return;
	}

	_add_system_message("[Auto-Verify] Analyzing game logs...");

	String prompt = "[Auto-Verification]\n"
					"The game just finished running. Please analyze these logs and verify:\n"
					"1. Are events being logged properly?\n"
					"2. Are there any errors or unexpected behaviors?\n"
					"3. Is the game logic working as expected?\n\n"
					"Game Logs:\n" + logs;

	_send_message(prompt);
}

// === Debugger Error Monitoring ===

void AIAssistantDock::_on_debugger_output(const String &p_msg, int p_type) {
	// EditorLog::MSG_TYPE_ERROR = 1, MSG_TYPE_WARNING = 3
	if (p_type == 1) { // MSG_TYPE_ERROR
		// Capture the error message
		pending_debugger_errors.push_back(p_msg);

		// Log to the Logs tab
		_add_log_entry("ERROR", p_msg, Color(0.9, 0.3, 0.3));

		// Use a timer to batch errors and send them to AI after a short delay
		SceneTree *tree = get_tree();
		if (tree && pending_debugger_errors.size() == 1) {
			tree->create_timer(3.0)->connect("timeout", callable_mp(this, &AIAssistantDock::_send_debugger_errors_to_ai));
		}
	} else if (p_type == 3) { // MSG_TYPE_WARNING
		_add_log_entry("WARN", p_msg, Color(0.9, 0.7, 0.3));
	}
}

void AIAssistantDock::_on_debugger_error(const String &p_file, int p_line, int p_debugger_id) {
	// Capture the error info from error_selected signal
	String error_info = vformat("Error in %s at line %d", p_file, p_line);
	pending_debugger_errors.push_back(error_info);

	// Log to the Logs tab
	_add_log_entry("ERROR", error_info, Color(0.9, 0.3, 0.3));

	// Use a timer to batch errors and send them to AI after a short delay
	// This prevents flooding the AI with individual errors
	SceneTree *tree = get_tree();
	if (tree && pending_debugger_errors.size() == 1) {
		// Only start timer on first error
		tree->create_timer(3.0)->connect("timeout", callable_mp(this, &AIAssistantDock::_send_debugger_errors_to_ai));
	}
}

void AIAssistantDock::_check_debugger_errors() {
	EditorDebuggerNode *debugger = EditorDebuggerNode::get_singleton();
	if (!debugger) {
		return;
	}

	// Check error count from the default debugger
	ScriptEditorDebugger *dbg = debugger->get_default_debugger();
	if (!dbg) {
		return;
	}

	int current_error_count = dbg->get_error_count();

	if (current_error_count > last_debugger_error_count) {
		int new_errors = current_error_count - last_debugger_error_count;
		last_debugger_error_count = current_error_count;

		// Only log once when we first detect errors, not every poll
		if (pending_debugger_errors.is_empty()) {
			_add_log_entry("ERROR", vformat("Debugger detected %d error(s) during game execution.", new_errors), Color(0.9, 0.3, 0.3));
			pending_debugger_errors.push_back(vformat("Total errors detected: %d", current_error_count));

			// Start timer to send errors to AI after game stops or after a delay
			SceneTree *tree = get_tree();
			if (tree) {
				tree->create_timer(5.0)->connect("timeout", callable_mp(this, &AIAssistantDock::_send_debugger_errors_to_ai));
			}
		} else {
			// Just update the total count in the pending list
			pending_debugger_errors.clear();
			pending_debugger_errors.push_back(vformat("Total errors detected: %d", current_error_count));
		}
	}
}

void AIAssistantDock::_send_debugger_errors_to_ai() {
	if (pending_debugger_errors.is_empty()) {
		return;
	}

	String error_summary = pending_debugger_errors[0];
	pending_debugger_errors.clear();

	// Only send to AI if fully connected and ready (no pending requests)
	// This prevents the "I'm offline" message from appearing
	if (connection_status != CONNECTED || session_id.is_empty() || pending_request != REQUEST_NONE) {
		// Already logged the error count, skip sending to AI silently
		return;
	}

	// Build error report with the total count
	String error_report = "[Debugger Errors Detected]\n";
	error_report += error_summary + "\n\n";
	error_report += "Please check the Godot Debugger panel (bottom of the editor) for error details.\n";
	error_report += "Read the error messages there and fix the issues in the relevant script files.\n";
	error_report += "Common issues include: missing InputMap actions, null references, invalid node paths.";

	_add_system_message("[Debugger] Sending error report to AI...");
	_send_message(error_report);
}

// === Session Persistence Functions ===

void AIAssistantDock::_find_existing_session() {
	if (pending_request != REQUEST_NONE) {
		return;
	}

	pending_request = REQUEST_SESSION_LIST;
	_update_status("Finding session...", Color(1, 1, 0));

	// Fetch root sessions for this project directory (sorted by most recently updated)
	String url = service_url + "/session?directory=" + _get_project_directory().uri_encode() + "&roots=true&limit=1";
	session_list_http_request->request(url, _get_headers_with_directory());
}

void AIAssistantDock::_on_session_list_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	pending_request = REQUEST_NONE;

	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		// Failed to get session list, create new session
		_add_system_message("Could not find existing sessions. Creating new session...");
		_create_session();
		return;
	}

	String response_text = String::utf8((const char *)p_body.ptr(), p_body.size());
	JSON json;
	Error err = json.parse(response_text);

	if (err != OK) {
		_add_system_message("Invalid session list response. Creating new session...");
		_create_session();
		return;
	}

	Variant data = json.get_data();
	if (data.get_type() != Variant::ARRAY) {
		_add_system_message("Unexpected session list format. Creating new session...");
		_create_session();
		return;
	}

	Array sessions = data;
	if (sessions.is_empty()) {
		_add_system_message("No existing sessions found. Creating new session...");
		_create_session();
		return;
	}

	// Use the most recent session (first in the list, sorted by updated time)
	Dictionary session = sessions[0];
	if (!session.has("id")) {
		_add_system_message("Invalid session data. Creating new session...");
		_create_session();
		return;
	}

	session_id = session["id"];
	coding_standards_injected = false;
	String title = session.get("title", "Untitled");

	connection_status = CONNECTED;
	_update_status("Connected", Color(0, 1, 0));
	_update_connection_indicator();
	reconnect_button->set_text("Disconnect");

	_add_system_message("Resumed session: " + title);
	_add_system_message("Session ID: " + session_id.substr(0, 8) + "...");
	_add_system_message("Project directory: " + _get_project_directory());

	// Start polling timers
	if (logs_poll_timer) {
		logs_poll_timer->start();
	}
	if (command_poll_timer) {
		command_poll_timer->start();
	}
	if (question_poll_timer) {
		question_poll_timer->start();
	}

	// Load chat history from the session
	_load_session_history();

	// Fetch config and models
	_fetch_config();
}

void AIAssistantDock::_load_session_history() {
	if (session_id.is_empty()) {
		return;
	}

	// Use a separate HTTP request for session history to avoid blocking other requests
	// We'll create a temporary HTTPRequest for this
	HTTPRequest *history_request = memnew(HTTPRequest);
	add_child(history_request);
	history_request->connect("request_completed", callable_mp(this, &AIAssistantDock::_on_session_history_completed));

	String url = service_url + "/session/" + session_id + "/message?directory=" + _get_project_directory().uri_encode();
	history_request->request(url, _get_headers_with_directory());
}

void AIAssistantDock::_on_session_history_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	// Clean up the temporary HTTPRequest
	HTTPRequest *sender = Object::cast_to<HTTPRequest>(get_child(get_child_count() - 1));
	if (sender && sender != http_request && sender != session_list_http_request &&
		sender != model_http_request && sender != http_auth_request &&
		sender != logs_http_request &&
		sender != stream_http_request && sender != command_http_request &&
		sender != question_http_request) {
		sender->queue_free();
	}

	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		_add_system_message("[History] Could not load chat history.");
		return;
	}

	String response_text = String::utf8((const char *)p_body.ptr(), p_body.size());
	JSON json;
	Error err = json.parse(response_text);

	if (err != OK) {
		_add_system_message("[History] Invalid history response.");
		return;
	}

	Variant data = json.get_data();
	if (data.get_type() != Variant::ARRAY) {
		_add_system_message("[History] Unexpected history format.");
		return;
	}

	Array messages = data;
	if (messages.is_empty()) {
		_add_system_message("[History] No previous messages.");
		return;
	}

	_add_system_message("[History] Loading " + itos(messages.size()) + " previous messages...");

	// Process each message
	for (int i = 0; i < messages.size(); i++) {
		Dictionary msg = messages[i];
		if (!msg.has("info") || !msg.has("parts")) {
			continue;
		}

		Dictionary info = msg["info"];
		Array parts = msg["parts"];
		String role = info.get("role", "");

		if (role == "user") {
			// Extract text from user message parts
			for (int j = 0; j < parts.size(); j++) {
				Dictionary part = parts[j];
				String type = part.get("type", "");
				if (type == "text") {
					String text = part.get("text", "");
					if (!text.is_empty()) {
						_add_user_message(text);
					}
				}
			}
		} else if (role == "assistant") {
			// Extract text from assistant message parts
			String full_text;
			for (int j = 0; j < parts.size(); j++) {
				Dictionary part = parts[j];
				String type = part.get("type", "");
				if (type == "text") {
					String text = part.get("text", "");
					if (!text.is_empty()) {
						if (!full_text.is_empty()) {
							full_text += "\n";
						}
						full_text += text;
					}
				}
			}
			if (!full_text.is_empty()) {
				_add_ai_message(full_text);
			}
		}
	}

	_add_log_entry("INFO", "Chat history loaded.", Color(0.7, 0.7, 0.7));
}

// === Provider/Model Fetching + Auth Flow ===

void AIAssistantDock::_fetch_providers() {
	if (provider_request_type != PROVIDER_REQUEST_NONE) {
		return;
	}

	provider_request_type = PROVIDER_REQUEST_FETCH_PROVIDERS;
	String url = service_url + "/provider";

	print_line("[AIAssistant] Fetching providers: GET " + url);

	Vector<String> headers = _get_headers_with_directory();
	headers.push_back("Accept: application/json");
	Error err = model_http_request->request(url, headers);
	if (err != OK) {
		print_line("[AIAssistant] Failed to fetch providers");
		provider_request_type = PROVIDER_REQUEST_NONE;
	}
}

void AIAssistantDock::_fetch_auth_methods() {
	if (provider_request_type != PROVIDER_REQUEST_NONE) {
		return;
	}

	provider_request_type = PROVIDER_REQUEST_FETCH_AUTH_METHODS;
	String url = service_url + "/provider/auth";

	print_line("[AIAssistant] Fetching auth methods: GET " + url);

	Vector<String> headers = _get_headers_with_directory();
	headers.push_back("Accept: application/json");
	Error err = model_http_request->request(url, headers);
	if (err != OK) {
		print_line("[AIAssistant] Failed to fetch auth methods");
		provider_request_type = PROVIDER_REQUEST_NONE;
	}
}

void AIAssistantDock::_on_provider_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	String body_str;
	if (p_body.size() > 0) {
		body_str = String::utf8((const char *)p_body.ptr(), p_body.size());
	}

	print_line("[AIAssistant] Provider HTTP response (type=" + itos(provider_request_type) + "): code=" + itos(p_code) + " body_size=" + itos(p_body.size()));

	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		print_line("[AIAssistant] Request failed: result=" + itos(p_result) + " code=" + itos(p_code));
		if (body_str.length() > 0) {
			print_line("[AIAssistant] Response body (first 200 chars): " + body_str.substr(0, 200));
		}
		if (provider_request_type == PROVIDER_REQUEST_FETCH_PROVIDERS) {
			_add_log_entry("ERROR", "Failed to fetch providers", Color(1, 0.4, 0.4));
		}
		provider_request_type = PROVIDER_REQUEST_NONE;
		return;
	}

	switch (provider_request_type) {
		case PROVIDER_REQUEST_FETCH_PROVIDERS: {
			provider_request_type = PROVIDER_REQUEST_NONE;

			Variant result = JSON::parse_string(body_str);
			if (result.get_type() != Variant::DICTIONARY) {
				print_line("[AIAssistant] Failed to parse providers JSON");
				return;
			}

			Dictionary resp = result;
			Array all = resp.get("all", Array());
			Dictionary defaults = resp.get("default", Dictionary());
			Array connected = resp.get("connected", Array());

			// Build connected set.
			HashSet<String> connected_set;
			for (int i = 0; i < connected.size(); i++) {
				connected_set.insert(String(connected[i]));
			}

			// Preserve locally-authenticated provider (may not be in server response yet).
			if (!pending_auth_provider_id.is_empty() && auth_state == AUTH_IDLE) {
				for (int i = 0; i < providers.size(); i++) {
					if (providers[i].id == pending_auth_provider_id && providers[i].connected) {
						connected_set.insert(pending_auth_provider_id);
						break;
					}
				}
			}

			// Parse providers and models.
			providers.clear();
			for (int i = 0; i < all.size(); i++) {
				if (all[i].get_type() != Variant::DICTIONARY) {
					continue;
				}
				Dictionary prov = all[i];
				ProviderInfo pi;
				pi.id = prov.get("id", "");
				pi.name = prov.get("name", pi.id);
				pi.connected = connected_set.has(pi.id);

				if (prov.has("models")) {
					Dictionary models_dict = prov["models"];
					Array model_keys = models_dict.keys();
					for (int k = 0; k < model_keys.size(); k++) {
						String model_key = model_keys[k];
						Dictionary model_data = models_dict[model_key];
						ModelInfo mi;
						mi.id = model_data.get("id", model_key);
						mi.name = model_data.get("name", mi.id);
						mi.provider_id = pi.id;

						// Filter: skip thinking variants and dated versions.
						bool is_thinking = mi.id.contains("-thinking");
						bool is_dated = false;
						if (mi.id.length() > 9) {
							String suffix = mi.id.substr(mi.id.length() - 9, 9);
							if (suffix.begins_with("-20") && suffix.substr(1).is_valid_int()) {
								is_dated = true;
							}
						}
						if (mi.id.contains("@20")) {
							is_dated = true;
						}
						String full_id = pi.id + "/" + mi.id;
						if ((is_thinking || is_dated) && full_id != (selected_provider_id + "/" + selected_model_id)) {
							continue;
						}

						pi.models.push_back(mi);
					}
				}

				if (pi.models.size() > 0) {
					providers.push_back(pi);
				}
			}

			// Sort providers alphabetically by name.
			providers.sort_custom<ProviderNameComparator>();

			// Auto-select default model if none selected.
			if (selected_model_id.is_empty() && defaults.size() > 0) {
				Array def_keys = defaults.keys();
				for (int i2 = 0; i2 < def_keys.size(); i2++) {
					String prov_id = def_keys[i2];
					if (connected_set.has(prov_id)) {
						selected_provider_id = prov_id;
						selected_model_id = String(defaults[prov_id]);
						break;
					}
				}
			}

			print_line("[AIAssistant] Parsed " + itos(providers.size()) + " providers, " + itos(connected_set.size()) + " connected");

			_populate_model_menu();
			_update_model_button_text();

			int total_models = 0;
			for (int i = 0; i < providers.size(); i++) {
				total_models += providers[i].models.size();
			}
			_add_log_entry("INFO", "Loaded " + itos(total_models) + " models from " + itos(providers.size()) + " provider(s).", Color(0.7, 0.7, 0.7));

			// Chain: fetch auth methods next.
			_fetch_auth_methods();
		} break;

		case PROVIDER_REQUEST_FETCH_AUTH_METHODS: {
			provider_request_type = PROVIDER_REQUEST_NONE;

			Variant result = JSON::parse_string(body_str);
			if (result.get_type() != Variant::DICTIONARY) {
				print_line("[AIAssistant] Failed to parse auth methods JSON");
				return;
			}

			Dictionary resp = result;
			auth_methods.clear();
			Array keys = resp.keys();
			for (int i = 0; i < keys.size(); i++) {
				String provider_id = keys[i];
				Array methods = resp[provider_id];
				Vector<Dictionary> method_list;
				for (int j = 0; j < methods.size(); j++) {
					if (methods[j].get_type() == Variant::DICTIONARY) {
						method_list.push_back(methods[j]);
					}
				}
				auth_methods.insert(provider_id, method_list);
			}

			print_line("[AIAssistant] Loaded auth methods for " + itos(auth_methods.size()) + " providers");
		} break;

		default:
			provider_request_type = PROVIDER_REQUEST_NONE;
			break;
	}
}

// === Model Menu (Two-Level Submenu) ===

void AIAssistantDock::_populate_model_menu() {
	PopupMenu *popup = model_button->get_popup();
	popup->clear();

	// Clean up old submenus.
	for (int i = 0; i < provider_submenus.size(); i++) {
		provider_submenus[i]->queue_free();
	}
	provider_submenus.clear();

	for (int i = 0; i < providers.size(); i++) {
		const ProviderInfo &prov = providers[i];

		// Create a submenu for this provider.
		PopupMenu *sub = memnew(PopupMenu);
		sub->set_name("provider_" + itos(i));
		sub->connect("id_pressed", callable_mp(this, &AIAssistantDock::_on_submenu_model_selected));
		provider_submenus.push_back(sub);

		// Add models to the submenu.
		for (int j = 0; j < prov.models.size(); j++) {
			const ModelInfo &model = prov.models[j];
			int item_id = i * 1000 + j;

			sub->add_item(model.name, item_id);

			int idx = sub->get_item_index(item_id);
			sub->set_item_metadata(idx, prov.id + "/" + model.id);

			// Mark selected model with a checkmark.
			if (prov.id == selected_provider_id && model.id == selected_model_id) {
				sub->set_item_checked(idx, true);
			}
		}

		// Add submenu to main popup with provider name.
		String label = prov.name;
		if (!prov.connected) {
			label += " [Not Connected]";
		}
		popup->add_submenu_node_item(label, sub);
	}
}

void AIAssistantDock::_on_submenu_model_selected(int p_id) {
	// Find the item in the submenus.
	String meta;
	for (int i = 0; i < provider_submenus.size(); i++) {
		PopupMenu *sub = provider_submenus[i];
		int index = sub->get_item_index(p_id);
		if (index >= 0) {
			meta = sub->get_item_metadata(index);
			break;
		}
	}

	if (meta.is_empty()) {
		return;
	}

	// meta is "provider_id/model_id".
	int slash = meta.find("/");
	if (slash < 0) {
		return;
	}

	String provider_id = meta.substr(0, slash);
	String model_id = meta.substr(slash + 1);

	// Check if provider is connected.
	bool is_connected = false;
	for (int i = 0; i < providers.size(); i++) {
		if (providers[i].id == provider_id) {
			is_connected = providers[i].connected;
			break;
		}
	}

	if (is_connected) {
		// Select model immediately.
		selected_provider_id = provider_id;
		selected_model_id = model_id;
		_populate_model_menu();
		_update_model_button_text();
		_update_model_config(provider_id + "/" + model_id);

		// Find model name for message.
		for (int i = 0; i < providers.size(); i++) {
			if (providers[i].id == provider_id) {
				for (int j = 0; j < providers[i].models.size(); j++) {
					if (providers[i].models[j].id == model_id) {
						_add_system_message("Switched to model: " + providers[i].models[j].name);
						break;
					}
				}
				break;
			}
		}
	} else {
		// Store pending selection and trigger auth.
		selected_provider_id = provider_id;
		selected_model_id = model_id;
		_update_model_button_text();
		_start_auth_for_provider(provider_id);
	}
}

void AIAssistantDock::_update_model_button_text() {
	if (selected_model_id.is_empty()) {
		model_button->set_text("Select Model");
		return;
	}

	// Find model name.
	String model_name = selected_model_id;
	bool is_connected = false;
	for (int i = 0; i < providers.size(); i++) {
		if (providers[i].id == selected_provider_id) {
			is_connected = providers[i].connected;
			for (int j = 0; j < providers[i].models.size(); j++) {
				if (providers[i].models[j].id == selected_model_id) {
					model_name = providers[i].models[j].name;
					break;
				}
			}
			break;
		}
	}

	if (is_connected) {
		model_button->set_text(model_name);
	} else {
		model_button->set_text(model_name + " (auth needed)");
	}
}

void AIAssistantDock::_update_model_config(const String &p_model_id) {
	// Update the OpenCode config to use the selected model.
	String url = service_url + "/config?directory=" + _get_project_directory().uri_encode();

	Dictionary body;
	body["model"] = p_model_id;

	String json_body = JSON::stringify(body);

	HTTPRequest *config_request = memnew(HTTPRequest);
	add_child(config_request);
	config_request->request(url, _get_headers_with_directory(), HTTPClient::METHOD_PATCH, json_body);
	config_request->connect("request_completed", callable_mp((Node *)config_request, &Node::queue_free).unbind(4));
}

// === Auth Flow ===

void AIAssistantDock::_start_auth_for_provider(const String &p_provider_id) {
	if (auth_state != AUTH_IDLE) {
		_add_system_message("Authentication already in progress.");
		return;
	}

	pending_auth_provider_id = p_provider_id;

	// Look up auth methods for this provider.
	if (!auth_methods.has(p_provider_id)) {
		_add_system_message("No authentication methods available for " + p_provider_id + ". Try reconnecting.");
		return;
	}

	const Vector<Dictionary> &methods = auth_methods[p_provider_id];
	if (methods.size() == 0) {
		_add_system_message("No authentication methods available for " + p_provider_id);
		return;
	}

	// Find first OAuth method.
	pending_auth_method_index = 0;
	for (int i = 0; i < methods.size(); i++) {
		String type = methods[i].get("type", "");
		if (type == "oauth") {
			pending_auth_method_index = i;
			break;
		}
	}

	_add_system_message("Starting authentication for " + p_provider_id + "...");
	_start_oauth_flow();
}

void AIAssistantDock::_start_oauth_flow() {
	auth_state = AUTH_AUTHORIZING;

	String url = service_url + "/provider/" + pending_auth_provider_id + "/oauth/authorize";

	print_line("[AIAssistant] Starting OAuth: POST " + url);

	Vector<String> headers = _get_headers_with_directory();
	headers.push_back("Accept: application/json");
	String body = "{\"method\": " + itos(pending_auth_method_index) + "}";
	Error err = http_auth_request->request(url, headers, HTTPClient::METHOD_POST, body);
	if (err != OK) {
		_add_system_message("Failed to start authentication.");
		auth_state = AUTH_IDLE;
	}
}

void AIAssistantDock::_poll_oauth_callback() {
	if (auth_state != AUTH_POLLING) {
		auth_poll_timer->stop();
		return;
	}

	poll_attempts++;
	if (poll_attempts > 60) { // 3 minutes max.
		auth_poll_timer->stop();
		auth_state = AUTH_IDLE;
		_add_system_message("Authentication timed out.");
		return;
	}

	String url = service_url + "/provider/" + pending_auth_provider_id + "/oauth/callback";

	Vector<String> headers = _get_headers_with_directory();
	headers.push_back("Accept: application/json");
	String body = "{\"method\": " + itos(pending_auth_method_index) + "}";
	Error err = http_auth_request->request(url, headers, HTTPClient::METHOD_POST, body);
	if (err != OK) {
		print_line("[AIAssistant] Failed to send poll request");
	}
}

void AIAssistantDock::_on_auth_code_submitted() {
	String code = auth_code_input->get_text().strip_edges();
	if (code.is_empty()) {
		_add_system_message("No code entered. Authentication cancelled.");
		auth_state = AUTH_IDLE;
		return;
	}

	_add_system_message("Submitting authorization code...");

	String url = service_url + "/provider/" + pending_auth_provider_id + "/oauth/callback";

	print_line("[AIAssistant] Submitting OAuth code: POST " + url);

	Vector<String> headers = _get_headers_with_directory();
	headers.push_back("Accept: application/json");

	String body = "{\"method\": " + itos(pending_auth_method_index) + ", \"code\": \"" + code.json_escape() + "\"}";
	Error err = http_auth_request->request(url, headers, HTTPClient::METHOD_POST, body);
	if (err != OK) {
		_add_system_message("Failed to submit authorization code.");
		auth_state = AUTH_IDLE;
	}
}

void AIAssistantDock::_on_auth_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	String body_str;
	if (p_body.size() > 0) {
		body_str = String::utf8((const char *)p_body.ptr(), p_body.size());
	}

	print_line("[AIAssistant] Auth HTTP response (state=" + itos(auth_state) + "): code=" + itos(p_code));

	switch (auth_state) {
		case AUTH_AUTHORIZING: {
			if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
				_add_system_message("Authentication request failed.");
				auth_state = AUTH_IDLE;
				return;
			}

			Variant result = JSON::parse_string(body_str);
			if (result.get_type() == Variant::DICTIONARY) {
				Dictionary resp = result;
				String auth_url = resp.get("url", "");
				pending_auth_method_type = resp.get("method", "");

				if (!auth_url.is_empty()) {
					_add_system_message("Opening browser for authentication...");
					OS::get_singleton()->shell_open(auth_url);

					if (pending_auth_method_type == "code") {
						// Code-based flow: show dialog for user to paste the code.
						auth_state = AUTH_POLLING;
						auth_code_input->set_text("");
						auth_code_dialog->popup_centered();
						_add_system_message("After authorizing in the browser, paste the code in the dialog.");
					} else {
						// Redirect-based flow: poll for callback completion.
						auth_state = AUTH_POLLING;
						poll_attempts = 0;
						auth_poll_timer->start();
					}
				} else {
					_add_system_message("No authorization URL returned.");
					auth_state = AUTH_IDLE;
				}
			} else {
				_add_system_message("Invalid authorization response.");
				auth_state = AUTH_IDLE;
			}
		} break;

		case AUTH_POLLING: {
			if (p_code == 200) {
				// Success — auth is complete.
				auth_poll_timer->stop();
				auth_state = AUTH_IDLE;

				// Mark provider as connected locally.
				for (int i = 0; i < providers.size(); i++) {
					if (providers[i].id == pending_auth_provider_id) {
						providers.write[i].connected = true;
						break;
					}
				}

				_populate_model_menu();
				_update_model_button_text();
				_add_system_message("Authentication successful! " + pending_auth_provider_id + " is now connected.");

				// Update config if a model was pending.
				if (!selected_model_id.is_empty()) {
					_update_model_config(selected_provider_id + "/" + selected_model_id);
				}

				// Re-fetch providers to get fresh state.
				_fetch_providers();
			}
			// Non-200 during polling is normal (auth not yet complete), keep polling.
		} break;

		default:
			break;
	}
}

// === Question Handling (AI Asking User Questions) ===

void AIAssistantDock::_on_question_poll_timeout() {
	if (connection_status != CONNECTED) {
		return;
	}

	// Skip if a request is already in progress
	if (question_request_in_progress) {
		return;
	}

	// Poll for pending questions from OpenCode
	String url = service_url + "/question";
	question_request_in_progress = true;
	question_http_request->request(url, _get_headers_with_directory());
}

void AIAssistantDock::_on_question_http_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	question_request_in_progress = false;

	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		return;
	}

	String response_text = String::utf8((const char *)p_body.ptr(), p_body.size());
	JSON json;
	Error err = json.parse(response_text);

	if (err != OK) {
		return;
	}

	Variant data = json.get_data();
	if (data.get_type() != Variant::ARRAY) {
		return;
	}

	Array questions = data;
	if (questions.is_empty()) {
		// No pending questions
		return;
	}

	// Find a question for our current session
	Dictionary question_request;
	String question_id;

	for (int i = 0; i < questions.size(); i++) {
		Dictionary q = questions[i];
		String q_session = q.get("sessionID", "");
		String q_id = q.get("id", "");

		// Only show questions for our session
		if (q_session == session_id && !q_id.is_empty()) {
			question_request = q;
			question_id = q_id;
			break;
		}
	}

	// Skip if no question found for our session
	if (question_id.is_empty()) {
		return;
	}

	// Only show if it's a new question (and not already showing this one)
	if (question_id != current_question_id) {
		print_line("AIAssistant: Showing question dialog for question: " + question_id);
		current_question_id = question_id;
		_show_question_dialog(question_request);
	}
}

void AIAssistantDock::_show_question_dialog(const Dictionary &p_question) {
	// Clean up any existing question container (but don't clear current_question_id)
	if (question_container && question_container->is_inside_tree()) {
		question_container->queue_free();
	}
	question_container = nullptr;

	Array questions = p_question.get("questions", Array());
	if (questions.is_empty()) {
		return;
	}

	// For now, just handle the first question
	Dictionary q = questions[0];
	String question_text = q.get("question", "");
	String header = q.get("header", "Question");
	Array options = q.get("options", Array());
	bool multiple = q.get("multiple", false);

	// Create question dialog UI
	question_container = memnew(VBoxContainer);
	question_container->add_theme_constant_override("separation", 8);

	// Question panel with styling
	PanelContainer *panel = memnew(PanelContainer);
	VBoxContainer *content = memnew(VBoxContainer);
	content->add_theme_constant_override("separation", 8);

	// Header
	Label *header_label = memnew(Label);
	header_label->set_text("[AI Question] " + header);
	header_label->add_theme_color_override("font_color", Color(1.0, 0.8, 0.2));
	content->add_child(header_label);

	// Question text
	RichTextLabel *question_label = memnew(RichTextLabel);
	question_label->set_use_bbcode(true);
	question_label->set_fit_content(true);
	question_label->set_text(question_text);
	content->add_child(question_label);

	// Options buttons
	for (int i = 0; i < options.size(); i++) {
		Dictionary opt = options[i];
		String label = opt.get("label", "Option " + itos(i + 1));
		String description = opt.get("description", "");

		Button *option_btn = memnew(Button);
		option_btn->set_text(label);
		if (!description.is_empty()) {
			option_btn->set_tooltip_text(description);
		}
		option_btn->connect("pressed", callable_mp(this, &AIAssistantDock::_on_question_option_pressed).bind(i));
		content->add_child(option_btn);
	}

	// Custom input option
	HBoxContainer *custom_container = memnew(HBoxContainer);
	LineEdit *custom_input = memnew(LineEdit);
	custom_input->set_placeholder("Or type a custom answer...");
	custom_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	custom_input->set_name("CustomInput");
	custom_input->connect("text_submitted", callable_mp(this, &AIAssistantDock::_on_question_custom_submitted).unbind(1));
	custom_container->add_child(custom_input);

	Button *submit_btn = memnew(Button);
	submit_btn->set_text("Submit");
	submit_btn->connect("pressed", callable_mp(this, &AIAssistantDock::_on_question_custom_submitted));
	custom_container->add_child(submit_btn);

	content->add_child(custom_container);

	panel->add_child(content);
	question_container->add_child(panel);

	// Add to chat container
	chat_container->add_child(question_container);

	// Scroll to bottom if auto-scroll is enabled
	if (chat_auto_scroll && chat_auto_scroll->is_pressed()) {
		callable_mp(chat_scroll, &ScrollContainer::set_v_scroll).call_deferred(INT32_MAX);
	}

	_add_log_entry("QUESTION", "AI is asking: " + question_text.substr(0, 100) + (question_text.length() > 100 ? "..." : ""), Color(1.0, 0.8, 0.2));
}

void AIAssistantDock::_hide_question_dialog() {
	if (question_container && question_container->is_inside_tree()) {
		question_container->queue_free();
	}
	question_container = nullptr;
	current_question_id = "";
}

void AIAssistantDock::_on_question_option_pressed(int p_option_index) {
	print_line("AIAssistant: Question option pressed: " + itos(p_option_index));

	if (current_question_id.is_empty()) {
		print_line("AIAssistant: No current question ID");
		return;
	}

	if (!question_container || !question_container->is_inside_tree()) {
		print_line("AIAssistant: Question container not valid");
		return;
	}

	// Find the button that was pressed to get its text
	// Structure: question_container -> panel -> content -> [header, question, btn0, btn1, ..., custom_container]
	PanelContainer *panel = Object::cast_to<PanelContainer>(question_container->get_child(0));
	if (!panel) {
		print_line("AIAssistant: Panel not found");
		return;
	}

	VBoxContainer *content = Object::cast_to<VBoxContainer>(panel->get_child(0));
	if (!content) {
		print_line("AIAssistant: Content not found");
		return;
	}

	// Button indices: 0=header, 1=question, 2...n=options, n+1=custom container
	int button_index = 2 + p_option_index;
	if (button_index >= content->get_child_count()) {
		print_line("AIAssistant: Button index out of range: " + itos(button_index));
		return;
	}

	Button *btn = Object::cast_to<Button>(content->get_child(button_index));
	if (!btn) {
		print_line("AIAssistant: Button not found at index " + itos(button_index));
		return;
	}

	String answer = btn->get_text();
	print_line("AIAssistant: Sending answer: " + answer);

	// Build answers array (array of arrays)
	Array answers;
	Array first_answer;
	first_answer.push_back(answer);
	answers.push_back(first_answer);

	String question_id = current_question_id; // Copy before hiding
	_add_user_message("[Answer] " + answer);
	_hide_question_dialog();
	_send_question_reply(question_id, answers);
}

void AIAssistantDock::_on_question_custom_submitted() {
	print_line("AIAssistant: Custom answer submitted");

	if (current_question_id.is_empty()) {
		print_line("AIAssistant: No current question ID");
		return;
	}

	if (!question_container || !question_container->is_inside_tree()) {
		print_line("AIAssistant: Question container not valid");
		return;
	}

	// Find the custom input LineEdit
	Node *found = question_container->find_child("CustomInput", true, false);
	LineEdit *custom_input = found ? Object::cast_to<LineEdit>(found) : nullptr;
	if (!custom_input) {
		print_line("AIAssistant: CustomInput LineEdit not found");
		return;
	}

	String answer = custom_input->get_text().strip_edges();
	if (answer.is_empty()) {
		print_line("AIAssistant: Empty answer, ignoring");
		return;
	}

	print_line("AIAssistant: Sending custom answer: " + answer);

	// Build answers array (array of arrays)
	Array answers;
	Array first_answer;
	first_answer.push_back(answer);
	answers.push_back(first_answer);

	String question_id = current_question_id; // Copy before hiding
	_add_user_message("[Answer] " + answer);
	_hide_question_dialog();
	_send_question_reply(question_id, answers);
}

void AIAssistantDock::_send_question_reply(const String &p_request_id, const Array &p_answers) {
	String url = service_url + "/question/" + p_request_id + "/reply";

	Dictionary body;
	body["answers"] = p_answers;

	String json_body = JSON::stringify(body);

	// Use a one-off request
	HTTPRequest *reply_request = memnew(HTTPRequest);
	add_child(reply_request);
	reply_request->request(url, _get_headers_with_directory(), HTTPClient::METHOD_POST, json_body);

	// Clean up after completion
	reply_request->connect("request_completed", callable_mp((Node *)reply_request, &Node::queue_free).unbind(4));

	_add_log_entry("QUESTION", "Sent answer to AI", Color(0.5, 0.9, 0.5));
}
