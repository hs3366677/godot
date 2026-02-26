/**************************************************************************/
/*  ai_assistant_dock.h                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                           MAKABAKA ENGINE                              */
/*                    AI-powered game creation module                     */
/**************************************************************************/

#pragma once

#include "editor/docks/editor_dock.h"

#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_button.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/label.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/tree.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/popup.h"
#include "scene/gui/texture_rect.h"
#include "scene/main/http_request.h"
#include "scene/main/timer.h"

#include "core/crypto/crypto_core.h"
#include "core/io/json.h"
#include "scene/resources/image_texture.h"

class AIAssistantManager;
class EditorFileDialog;

class AIAssistantDock : public EditorDock {
	GDCLASS(AIAssistantDock, EditorDock);

public:
	enum ConnectionStatus {
		DISCONNECTED,
		CONNECTING,
		CONNECTED,
		CONNECTION_ERROR
	};

	enum AuthState {
		AUTH_IDLE,
		AUTH_AUTHORIZING,
		AUTH_POLLING
	};

	enum ProviderRequestType {
		PROVIDER_REQUEST_NONE,
		PROVIDER_REQUEST_FETCH_PROVIDERS,
		PROVIDER_REQUEST_FETCH_AUTH_METHODS
	};

	struct ModelInfo {
		String id;
		String name;
		String provider_id;
	};

	struct ProviderInfo {
		String id;
		String name;
		bool connected = false;
		Vector<ModelInfo> models;
	};

	struct ProviderNameComparator {
		bool operator()(const ProviderInfo &a, const ProviderInfo &b) const {
			return a.name.naturalnocasecmp_to(b.name) < 0;
		}
	};

	struct AttachmentInfo {
		String file_path;
		String filename;
		String mime_type;
		Vector<uint8_t> data;
		Ref<ImageTexture> thumbnail;
	};

private:
	// Instance management
	int instance_id = 0;
	String initial_session_id;
	bool create_new_session_if_none = false; // When true, never steal another dock's session
	bool has_user_message = false; // Track whether user has sent at least one message in this session
	Button *new_instance_button = nullptr;
	void _on_new_instance_pressed();
	void _on_close_instance_pressed();

	// Main container
	VBoxContainer *main_container = nullptr;

	// UI Components
	HBoxContainer *toolbar_container = nullptr;
	Button *session_history_button = nullptr;
	Button *settings_button = nullptr;
	ColorRect *connection_indicator = nullptr;

	// Settings dialog (asset provider configuration + prompt management)
	AcceptDialog *settings_dialog = nullptr;
	TabContainer *settings_tabs = nullptr;

	// Providers tab
	LineEdit *replicate_token_input = nullptr;
	Button *replicate_test_button = nullptr;
	Label *replicate_status_label = nullptr;
	LineEdit *meshy_token_input = nullptr;
	Button *meshy_test_button = nullptr;
	Label *meshy_status_label = nullptr;
	HTTPRequest *settings_http_request = nullptr;
	String settings_testing_provider;

	// Prompt tab
	TextEdit *project_prompt_edit = nullptr;
	Label *project_prompt_path_label = nullptr;
	Button *generate_prompt_button = nullptr;
	Tree *engine_prompt_tree = nullptr;
	RichTextLabel *engine_prompt_preview = nullptr;

	// Model selector (two-level submenu: Provider → Models)
	MenuButton *model_button = nullptr;
	Vector<PopupMenu *> provider_submenus;
	HTTPRequest *model_http_request = nullptr;
	ProviderRequestType provider_request_type = PROVIDER_REQUEST_NONE;

	// Provider/model data
	Vector<ProviderInfo> providers;
	HashMap<String, Vector<Dictionary>> auth_methods;
	String selected_provider_id;
	String selected_model_id;

	// Auth flow
	HTTPRequest *http_auth_request = nullptr;
	AuthState auth_state = AUTH_IDLE;
	String pending_auth_provider_id;
	int pending_auth_method_index = 0;
	String pending_auth_method_type; // "code" or "redirect"
	Timer *auth_poll_timer = nullptr;
	int poll_attempts = 0;

	// Auth code input dialog
	AcceptDialog *auth_code_dialog = nullptr;
	LineEdit *auth_code_input = nullptr;

	// Chat area
	VBoxContainer *chat_tab = nullptr;
	ScrollContainer *chat_scroll = nullptr;
	VBoxContainer *chat_container = nullptr;

	// Collapse/expand for AI response turns
	Vector<int> user_message_indices; // child indices in chat_container for each user message
	HashMap<int, Button *> collapse_buttons; // user_msg_index → collapse button
	HashMap<int, bool> collapsed_turns; // user_msg_index → is collapsed
	HashMap<int, String> user_message_texts; // user_msg_index → display text for sticky header

	// Sticky header — pins current user question at top of scroll
	PanelContainer *sticky_header = nullptr;
	Button *sticky_collapse_btn = nullptr;
	RichTextLabel *sticky_text_label = nullptr;
	int sticky_current_turn_index = -1; // which user_msg_index is currently shown

	VBoxContainer *input_container = nullptr;
	HBoxContainer *button_container = nullptr;
	TextEdit *prompt_input = nullptr;
	Button *send_button = nullptr;
	Button *stop_button = nullptr;

	// Image attachments
	static const int MAX_IMAGE_SIZE_BYTES = 10 * 1024 * 1024;
	static const int THUMBNAIL_SIZE = 64;
	Vector<AttachmentInfo> pending_attachments;
	ScrollContainer *attachment_scroll = nullptr;
	HBoxContainer *attachment_preview_container = nullptr;
	Button *attach_image_button = nullptr;
	Button *screenshot_button = nullptr;
	EditorFileDialog *image_file_dialog = nullptr;

	// Slash command autocomplete (inline, non-modal)
	VBoxContainer *slash_hint_container = nullptr;
	Vector<Button *> slash_hint_buttons;
	Vector<int> slash_hint_cmd_indices; // Maps button index → command index
	int slash_hint_selected = -1; // Currently highlighted hint index (-1 = none)
	void _on_prompt_text_changed();
	void _on_slash_hint_pressed(int p_id);
	void _update_slash_hint_highlight();

	// Mode flags
	bool coding_standards_injected = false; // Reset per session

	// Processing indicator (overlay on prompt_input top-right)
	Label *processing_label = nullptr;
	Timer *processing_timer = nullptr;
	int processing_dots = 0;

	// Streaming updates (poll for message parts during processing)
	HTTPRequest *stream_http_request = nullptr;
	Timer *stream_poll_timer = nullptr;
	String current_message_id;
	int last_part_count = 0;
	bool stream_request_in_progress = false;

	// Track tool UI elements by part ID for status updates
	HashMap<String, RichTextLabel *> tool_containers;
	HashMap<String, uint64_t> tool_start_times; // Track when each tool started
	HashMap<String, String> tool_logged_status; // Track last-logged status per tool for log updates

	// Store full input/output for click-to-view in tool detail popup
	HashMap<String, String> tool_full_inputs;  // part_id → full input text
	HashMap<String, String> tool_full_outputs; // part_id → full output text

	// Separate HashMap for text streaming labels (decoupled from tool_containers)
	HashMap<String, RichTextLabel *> text_stream_labels;

	// Tool detail viewer popup
	AcceptDialog *tool_detail_dialog = nullptr;
	RichTextLabel *tool_detail_content = nullptr;

	// Logs tab
	VBoxContainer *logs_tab = nullptr;
	HBoxContainer *logs_toolbar = nullptr;
	Button *logs_clear_button = nullptr;
	Button *logs_refresh_button = nullptr;
	CheckButton *logs_auto_scroll = nullptr;
	ScrollContainer *logs_scroll = nullptr;
	RichTextLabel *logs_text = nullptr;

	// Log polling
	HTTPRequest *logs_http_request = nullptr;
	Timer *logs_poll_timer = nullptr;
	String last_event_id;
	bool logs_request_in_progress = false;

	// Command polling (for auto-run from OpenCode)
	HTTPRequest *command_http_request = nullptr;
	Timer *command_poll_timer = nullptr;
	bool command_request_in_progress = false;

	// Question handling (AI asking user questions)
	HTTPRequest *question_http_request = nullptr;
	Timer *question_poll_timer = nullptr;
	bool question_request_in_progress = false;
	String current_question_id; // Track current question being displayed
	VBoxContainer *question_container = nullptr; // UI container for question dialog

	// Auto-verification after game runs
	bool pending_auto_verify = false;

	// Debugger error monitoring
	int last_debugger_error_count = 0;
	Vector<String> pending_debugger_errors;

	// HTTP connection to OpenCode
	HTTPRequest *http_request = nullptr;
	String service_url = "http://localhost:4096";
	String session_id;
	ConnectionStatus connection_status = DISCONNECTED;

	// Chat history
	Vector<Dictionary> chat_history;

	// Internal methods
	void _setup_ui();
	void _connect_signals();
	void _update_connection_indicator();

	// OpenCode HTTP API
	String _get_project_directory() const;
	Vector<String> _get_headers_with_directory() const;
	void _check_service_health();
	void _check_service_health_deferred();
	void _create_session();
	void _fetch_config();
	void _send_message(const String &p_content);
	String _load_coding_standards();
	String _cached_coding_standards;
	void _on_http_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);

	// UI handlers
	void _on_send_pressed();
	void _on_stop_pressed();
	void _on_clear_pressed();
	void _on_prompt_input_gui_input(const Ref<InputEvent> &p_event);

	// Image attachment handling
	void _on_attach_image_pressed();
	void _on_screenshot_pressed();
	void _on_image_files_selected(const PackedStringArray &p_paths);
	void _on_files_dropped_on_dock(const PackedStringArray &p_files);
	void _on_remove_attachment(int p_index);
	bool _add_attachment_from_file(const String &p_path);
	bool _add_attachment_from_clipboard_image();
	void _rebuild_attachment_previews();
	void _clear_attachments();
	String _get_mime_type_for_extension(const String &p_extension) const;
	String _encode_data_url(const String &p_mime, const Vector<uint8_t> &p_data) const;
	bool _is_supported_image_extension(const String &p_extension) const;

	void _on_settings_pressed();
	void _on_replicate_test_pressed();
	void _on_meshy_test_pressed();
	void _test_provider(const String &p_provider_id, LineEdit *p_input, Button *p_button, Label *p_status);
	void _on_settings_save_pressed();
	void _on_settings_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _auto_configure_providers();

	// Prompt management
	String _load_project_prompt();
	void _save_project_prompt(const String &p_content);
	String _generate_project_prompt_template();
	Vector<String> _get_engine_prompt_files();
	void _populate_engine_prompt_tree();
	void _on_engine_prompt_selected();
	void _on_generate_prompt_pressed();
	void _process_slash_command(const String &p_command);
	void _on_providers_status_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);

	// Provider/model fetching
	void _fetch_providers();
	void _fetch_auth_methods();
	void _on_provider_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);

	// Model menu
	void _populate_model_menu();
	void _on_submenu_model_selected(int p_id);
	void _update_model_button_text();
	void _update_model_config(const String &p_model_id);

	// Auth flow
	void _start_auth_for_provider(const String &p_provider_id);
	void _start_oauth_flow();
	void _poll_oauth_callback();
	void _on_auth_code_submitted();
	void _on_auth_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);

	// Processing indicator
	void _show_processing();
	void _hide_processing();
	void _on_processing_timer_timeout();

	// Streaming updates
	void _start_stream_polling(const String &p_message_id);
	void _stop_stream_polling();
	void _on_stream_poll_timeout();
	void _on_stream_http_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);

	// Verification
	void _on_verify_pressed();
	String _read_game_logs();

	// Message handling
	void _add_message(const String &p_sender, const String &p_text, const Color &p_color);
	void _add_user_message(const String &p_text);
	void _add_ai_message(const String &p_text);
	void _add_system_message(const String &p_text);
	void _add_tool_message(const String &p_part_id, const String &p_tool_name, const String &p_status, const Dictionary &p_details);
	void _scroll_chat_to_bottom();
	void _toggle_turn_collapse(int p_user_msg_index);
	void _on_chat_scroll_changed(double p_value);
	void _update_sticky_header();
	void _clear_tool_tracking();
	String _format_tool_display_name(const String &p_tool_name) const;
	void _on_tool_meta_clicked(const Variant &p_meta);

	// Command processing
	void _process_prompt(const String &p_prompt);
	void _process_local_command(const String &p_prompt);
	void _execute_commands(const Array &p_commands);
	Dictionary _execute_single_command(const Dictionary &p_command);

	// Status updates
	void _update_status(const String &p_text, const Color &p_color);

	// Logs handling
	void _setup_logs_tab();
	void _on_logs_clear_pressed();
	void _on_logs_refresh_pressed();
	void _on_logs_poll_timeout();
	void _on_logs_http_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _add_log_entry(const String &p_type, const String &p_message, const Color &p_color);
	void _fetch_session_events();

	// Command polling (auto-run from OpenCode)
	void _on_command_poll_timeout();
	void _on_command_http_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _execute_godot_command(const String &p_action, const Dictionary &p_params);
	void _report_game_status(bool p_running);

	// Question handling (AI asking user questions)
	void _on_question_poll_timeout();
	void _on_question_http_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _show_question_dialog(const Dictionary &p_question);
	void _hide_question_dialog();
	void _on_question_option_pressed(int p_option_index);
	void _on_question_custom_submitted();
	void _send_question_reply(const String &p_request_id, const Array &p_answers);

	// Screenshot capture (for godot_screenshot tool and 📷 button)
	static AIAssistantDock *singleton;
	bool _add_attachment_from_raw_data(const String &p_filename, const String &p_mime, const Vector<uint8_t> &p_data);
	void _post_screenshot_result(const String &p_id, const String &p_b64);
	// Instance callbacks
	void _on_screenshot_for_tool(int64_t p_w, int64_t p_h, const String &p_path, const Rect2i &p_rect, const String &p_id);
	void _on_screenshot_for_button(int64_t p_w, int64_t p_h, const String &p_path, const Rect2i &p_rect);
	// Static callbacks — bypass ObjectDB checks, dispatch to singleton
	static void _screenshot_for_button_static(int64_t p_w, int64_t p_h, const String &p_path, const Rect2i &p_rect);
	static void _screenshot_for_tool_static(int64_t p_w, int64_t p_h, const String &p_path, const Rect2i &p_rect, const String &p_id);

	// Auto-verification
	void _on_game_stopped();
	void _auto_verify_game_logs();

	// Debugger error monitoring
	void _check_debugger_errors();
	void _on_debugger_error(const String &p_file, int p_line, int p_debugger_id);
	void _on_debugger_output(const String &p_msg, int p_type);
	void _send_debugger_errors_to_ai();

	// Pending request type
	enum RequestType {
		REQUEST_NONE,
		REQUEST_HEALTH,
		REQUEST_SESSION_LIST,  // Find existing session
		REQUEST_SESSION,       // Create new session
		REQUEST_SESSION_HISTORY, // Load session messages
		REQUEST_DELETE_SESSION, // Delete current session
		REQUEST_CONFIG,
		REQUEST_MESSAGE
	};
	RequestType pending_request = REQUEST_NONE;

	// Session persistence
	HTTPRequest *session_list_http_request = nullptr;
	void _find_existing_session();
	void _on_session_list_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _load_session_history();
	void _on_session_history_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);

	// Session history panel (custom popup)
	HTTPRequest *session_history_list_http = nullptr;
	Vector<Dictionary> cached_session_list;
	PopupPanel *session_popup = nullptr;
	VBoxContainer *session_popup_list = nullptr;
	void _clear_chat_ui();
	void _on_session_history_pressed();
	void _fetch_session_list();
	void _on_session_history_list_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _on_session_item_clicked(int p_index);
	void _on_session_new_pressed();
	void _switch_to_session(const String &p_session_id, const String &p_title);
	void _fire_and_forget_delete_session(const String &p_session_id);

	// Session delete confirmation
	ConfirmationDialog *session_delete_confirm = nullptr;
	int session_pending_delete_index = -1;
	void _on_session_delete_pressed(int p_index);
	void _on_session_delete_confirmed();

	// Session rename
	AcceptDialog *session_rename_dialog = nullptr;
	LineEdit *session_rename_input = nullptr;
	int session_pending_rename_index = -1;
	HTTPRequest *session_rename_http = nullptr;
	void _on_session_rename_pressed(int p_index);
	void _on_session_rename_confirmed();
	void _on_session_rename_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);

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

	// Instance management
	void set_instance_id(int p_id);
	int get_instance_id() const { return instance_id; }
	void set_initial_session_id(const String &p_session_id);
	void set_create_new_session_if_none(bool p_create) { create_new_session_if_none = p_create; }
	String get_session_id() const { return session_id; }
	bool get_has_user_message() const { return has_user_message; }
	void cleanup_before_close();
};
