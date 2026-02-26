/**************************************************************************/
/*  ai_assistant_dock.cpp                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                           MAKABAKA ENGINE                              */
/*                    AI-powered game creation module                     */
/**************************************************************************/

#include "ai_assistant_dock.h"
#include "ai_assistant_manager.h"

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
#include "editor/settings/editor_settings.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/run/editor_run_bar.h"
#include "editor/gui/editor_file_dialog.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/separator.h"
#include "scene/resources/style_box_flat.h"
#include "scene/resources/image_texture.h"
#include "servers/display/display_server.h"

void AIAssistantDock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_service_url", "url"), &AIAssistantDock::set_service_url);
	ClassDB::bind_method(D_METHOD("get_service_url"), &AIAssistantDock::get_service_url);
	ClassDB::bind_method(D_METHOD("is_connected_to_service"), &AIAssistantDock::is_connected_to_service);
	ClassDB::bind_method(D_METHOD("_check_service_health_deferred"), &AIAssistantDock::_check_service_health_deferred);

	// UI handlers
	ClassDB::bind_method(D_METHOD("_on_send_pressed"), &AIAssistantDock::_on_send_pressed);
	ClassDB::bind_method(D_METHOD("_on_stop_pressed"), &AIAssistantDock::_on_stop_pressed);
	ClassDB::bind_method(D_METHOD("_on_settings_pressed"), &AIAssistantDock::_on_settings_pressed);
	ClassDB::bind_method(D_METHOD("_on_new_instance_pressed"), &AIAssistantDock::_on_new_instance_pressed);
	ClassDB::bind_method(D_METHOD("_on_prompt_input_gui_input", "event"), &AIAssistantDock::_on_prompt_input_gui_input);
	ClassDB::bind_method(D_METHOD("_on_prompt_text_changed"), &AIAssistantDock::_on_prompt_text_changed);
	ClassDB::bind_method(D_METHOD("_on_chat_scroll_changed", "value"), &AIAssistantDock::_on_chat_scroll_changed);
	ClassDB::bind_method(D_METHOD("_scroll_chat_to_bottom"), &AIAssistantDock::_scroll_chat_to_bottom);
	ClassDB::bind_method(D_METHOD("_on_tool_meta_clicked", "meta"), &AIAssistantDock::_on_tool_meta_clicked);
	ClassDB::bind_method(D_METHOD("_toggle_turn_collapse", "user_msg_index"), &AIAssistantDock::_toggle_turn_collapse);
	ClassDB::bind_method(D_METHOD("_on_slash_hint_pressed", "id"), &AIAssistantDock::_on_slash_hint_pressed);
	ClassDB::bind_method(D_METHOD("_on_remove_attachment", "index"), &AIAssistantDock::_on_remove_attachment);

	// Image attachments
	ClassDB::bind_method(D_METHOD("_on_attach_image_pressed"), &AIAssistantDock::_on_attach_image_pressed);
	ClassDB::bind_method(D_METHOD("_on_screenshot_pressed"), &AIAssistantDock::_on_screenshot_pressed);
	ClassDB::bind_method(D_METHOD("_on_image_files_selected", "paths"), &AIAssistantDock::_on_image_files_selected);
	ClassDB::bind_method(D_METHOD("_on_files_dropped_on_dock", "files"), &AIAssistantDock::_on_files_dropped_on_dock);

	// HTTP request callbacks
	ClassDB::bind_method(D_METHOD("_on_http_request_completed", "result", "code", "headers", "body"), &AIAssistantDock::_on_http_request_completed);
	ClassDB::bind_method(D_METHOD("_on_settings_request_completed", "result", "code", "headers", "body"), &AIAssistantDock::_on_settings_request_completed);
	ClassDB::bind_method(D_METHOD("_on_providers_status_completed", "result", "code", "headers", "body"), &AIAssistantDock::_on_providers_status_completed);
	ClassDB::bind_method(D_METHOD("_on_provider_request_completed", "result", "code", "headers", "body"), &AIAssistantDock::_on_provider_request_completed);
	ClassDB::bind_method(D_METHOD("_on_auth_request_completed", "result", "code", "headers", "body"), &AIAssistantDock::_on_auth_request_completed);
	ClassDB::bind_method(D_METHOD("_on_stream_http_request_completed", "result", "code", "headers", "body"), &AIAssistantDock::_on_stream_http_request_completed);
	ClassDB::bind_method(D_METHOD("_on_logs_http_request_completed", "result", "code", "headers", "body"), &AIAssistantDock::_on_logs_http_request_completed);
	ClassDB::bind_method(D_METHOD("_on_command_http_request_completed", "result", "code", "headers", "body"), &AIAssistantDock::_on_command_http_request_completed);
	ClassDB::bind_method(D_METHOD("_on_question_http_request_completed", "result", "code", "headers", "body"), &AIAssistantDock::_on_question_http_request_completed);
	ClassDB::bind_method(D_METHOD("_on_session_list_completed", "result", "code", "headers", "body"), &AIAssistantDock::_on_session_list_completed);
	ClassDB::bind_method(D_METHOD("_on_session_history_completed", "result", "code", "headers", "body"), &AIAssistantDock::_on_session_history_completed);
	ClassDB::bind_method(D_METHOD("_on_session_history_list_completed", "result", "code", "headers", "body"), &AIAssistantDock::_on_session_history_list_completed);
	ClassDB::bind_method(D_METHOD("_on_session_history_pressed"), &AIAssistantDock::_on_session_history_pressed);
	ClassDB::bind_method(D_METHOD("_on_session_item_clicked", "index"), &AIAssistantDock::_on_session_item_clicked);
	ClassDB::bind_method(D_METHOD("_on_session_new_pressed"), &AIAssistantDock::_on_session_new_pressed);
	ClassDB::bind_method(D_METHOD("_on_session_delete_pressed", "index"), &AIAssistantDock::_on_session_delete_pressed);
	ClassDB::bind_method(D_METHOD("_on_session_delete_confirmed"), &AIAssistantDock::_on_session_delete_confirmed);
	ClassDB::bind_method(D_METHOD("_on_session_rename_pressed", "index"), &AIAssistantDock::_on_session_rename_pressed);
	ClassDB::bind_method(D_METHOD("_on_session_rename_confirmed"), &AIAssistantDock::_on_session_rename_confirmed);
	ClassDB::bind_method(D_METHOD("_on_session_rename_completed", "result", "code", "headers", "body"), &AIAssistantDock::_on_session_rename_completed);
	ClassDB::bind_method(D_METHOD("cleanup_before_close"), &AIAssistantDock::cleanup_before_close);

	// Settings
	ClassDB::bind_method(D_METHOD("_on_replicate_test_pressed"), &AIAssistantDock::_on_replicate_test_pressed);
	ClassDB::bind_method(D_METHOD("_on_meshy_test_pressed"), &AIAssistantDock::_on_meshy_test_pressed);
	ClassDB::bind_method(D_METHOD("_on_settings_save_pressed"), &AIAssistantDock::_on_settings_save_pressed);
	ClassDB::bind_method(D_METHOD("_on_generate_prompt_pressed"), &AIAssistantDock::_on_generate_prompt_pressed);
	ClassDB::bind_method(D_METHOD("_on_engine_prompt_selected"), &AIAssistantDock::_on_engine_prompt_selected);

	// Timers
	ClassDB::bind_method(D_METHOD("_on_processing_timer_timeout"), &AIAssistantDock::_on_processing_timer_timeout);
	ClassDB::bind_method(D_METHOD("_on_logs_poll_timeout"), &AIAssistantDock::_on_logs_poll_timeout);
	ClassDB::bind_method(D_METHOD("_on_command_poll_timeout"), &AIAssistantDock::_on_command_poll_timeout);
	ClassDB::bind_method(D_METHOD("_on_question_poll_timeout"), &AIAssistantDock::_on_question_poll_timeout);
	ClassDB::bind_method(D_METHOD("_on_stream_poll_timeout"), &AIAssistantDock::_on_stream_poll_timeout);
	ClassDB::bind_method(D_METHOD("_poll_oauth_callback"), &AIAssistantDock::_poll_oauth_callback);
	ClassDB::bind_method(D_METHOD("_on_logs_refresh_pressed"), &AIAssistantDock::_on_logs_refresh_pressed);
	ClassDB::bind_method(D_METHOD("_on_logs_clear_pressed"), &AIAssistantDock::_on_logs_clear_pressed);
	ClassDB::bind_method(D_METHOD("_on_auth_code_submitted"), &AIAssistantDock::_on_auth_code_submitted);

	// Model menu
	ClassDB::bind_method(D_METHOD("_on_submenu_model_selected", "id"), &AIAssistantDock::_on_submenu_model_selected);

	// Game lifecycle
	ClassDB::bind_method(D_METHOD("_on_game_stopped"), &AIAssistantDock::_on_game_stopped);
	ClassDB::bind_method(D_METHOD("_auto_verify_game_logs"), &AIAssistantDock::_auto_verify_game_logs);
	ClassDB::bind_method(D_METHOD("_send_debugger_errors_to_ai"), &AIAssistantDock::_send_debugger_errors_to_ai);
	ClassDB::bind_method(D_METHOD("_on_debugger_error", "file", "line", "debugger_id"), &AIAssistantDock::_on_debugger_error);

	// Questions
	ClassDB::bind_method(D_METHOD("_on_question_option_pressed", "option_index"), &AIAssistantDock::_on_question_option_pressed);
	ClassDB::bind_method(D_METHOD("_on_question_custom_submitted"), &AIAssistantDock::_on_question_custom_submitted);
}

AIAssistantDock *AIAssistantDock::singleton = nullptr;

AIAssistantDock::AIAssistantDock() {
	singleton = this;
	set_title(TTR("AI Assistant"));
	set_icon_name(SNAME("Node"));
	set_default_slot(DOCK_SLOT_RIGHT_UL);

	_setup_ui();
	_connect_signals();
}

AIAssistantDock::~AIAssistantDock() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

void AIAssistantDock::_setup_ui() {
	// Main container for all UI
	main_container = memnew(VBoxContainer);
	main_container->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	add_child(main_container);

	// Single toolbar row: session | model | settings | + | connection indicator
	toolbar_container = memnew(HBoxContainer);
	main_container->add_child(toolbar_container);

	session_history_button = memnew(Button);
	session_history_button->set_text("Sessions");
	session_history_button->set_tooltip_text(TTR("Browse and switch between conversation sessions"));
	toolbar_container->add_child(session_history_button);

	// Custom popup panel for session list.
	session_popup = memnew(PopupPanel);
	session_popup->set_min_size(Size2(300, 0));
	add_child(session_popup);

	ScrollContainer *session_scroll = memnew(ScrollContainer);
	session_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	session_scroll->set_custom_minimum_size(Size2(300, 200));
	session_popup->add_child(session_scroll);

	session_popup_list = memnew(VBoxContainer);
	session_popup_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	session_scroll->add_child(session_popup_list);

	// Delete confirmation dialog for sessions.
	session_delete_confirm = memnew(ConfirmationDialog);
	session_delete_confirm->set_title(TTR("Delete Session"));
	session_delete_confirm->set_text(TTR("Are you sure you want to delete this session?"));
	add_child(session_delete_confirm);

	// Rename dialog for sessions.
	session_rename_dialog = memnew(AcceptDialog);
	session_rename_dialog->set_title(TTR("Rename Session"));
	session_rename_dialog->set_ok_button_text(TTR("Rename"));
	VBoxContainer *rename_vbox = memnew(VBoxContainer);
	session_rename_dialog->add_child(rename_vbox);
	Label *rename_label = memnew(Label);
	rename_label->set_text(TTR("New session title:"));
	rename_vbox->add_child(rename_label);
	session_rename_input = memnew(LineEdit);
	session_rename_input->set_custom_minimum_size(Size2(300, 0));
	rename_vbox->add_child(session_rename_input);
	add_child(session_rename_dialog);

	// HTTPRequest for session rename.
	session_rename_http = memnew(HTTPRequest);
	add_child(session_rename_http);

	// Model selector (two-level submenu: Provider → Models)
	model_button = memnew(MenuButton);
	model_button->set_text("Select Model");
	toolbar_container->add_child(model_button);

	toolbar_container->add_spacer();

	settings_button = memnew(Button);
	settings_button->set_text("Settings");
	settings_button->set_tooltip_text("Configure AI asset generation providers");
	toolbar_container->add_child(settings_button);

	// New instance button ("+")
	new_instance_button = memnew(Button);
	new_instance_button->set_text("+");
	new_instance_button->set_tooltip_text(TTR("Open new AI Assistant"));
	toolbar_container->add_child(new_instance_button);

	connection_indicator = memnew(ColorRect);
	connection_indicator->set_custom_minimum_size(Size2(12, 12));
	connection_indicator->set_color(Color(0.5, 0.5, 0.5));
	toolbar_container->add_child(connection_indicator);

	// === Chat Area (directly in main container, no tabs) ===
	chat_tab = memnew(VBoxContainer);
	chat_tab->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main_container->add_child(chat_tab);

	// Sticky header — pins current user question at top of chat scroll
	sticky_header = memnew(PanelContainer);
	sticky_header->set_visible(false);
	Ref<StyleBoxFlat> sticky_bg;
	sticky_bg.instantiate();
	sticky_bg->set_bg_color(Color(0.15, 0.18, 0.25, 0.95));
	sticky_bg->set_content_margin_all(4);
	sticky_header->add_theme_style_override("panel", sticky_bg);
	chat_tab->add_child(sticky_header);

	HBoxContainer *sticky_hbox = memnew(HBoxContainer);
	sticky_hbox->add_theme_constant_override("separation", 4);
	sticky_header->add_child(sticky_hbox);

	sticky_collapse_btn = memnew(Button);
	sticky_collapse_btn->set_text(U"\u25BC");
	sticky_collapse_btn->set_custom_minimum_size(Size2(24, 24));
	sticky_collapse_btn->set_tooltip_text("Collapse/expand this AI response");
	sticky_collapse_btn->add_theme_font_size_override("font_size", 10);
	sticky_hbox->add_child(sticky_collapse_btn);

	sticky_text_label = memnew(RichTextLabel);
	sticky_text_label->set_use_bbcode(true);
	sticky_text_label->set_fit_content(true);
	sticky_text_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	sticky_text_label->set_custom_minimum_size(Size2(0, 24));
	sticky_hbox->add_child(sticky_text_label);

	// Chat area
	chat_scroll = memnew(ScrollContainer);
	chat_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	chat_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	chat_tab->add_child(chat_scroll);

	chat_container = memnew(VBoxContainer);
	chat_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	chat_scroll->add_child(chat_container);
	chat_container->connect("resized", Callable(this, "_scroll_chat_to_bottom"));

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

	// Attachment thumbnail preview strip (horizontal scroll, hidden when empty)
	attachment_scroll = memnew(ScrollContainer);
	attachment_scroll->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	attachment_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_SHOW_ALWAYS);
	attachment_scroll->set_custom_minimum_size(Size2(0, THUMBNAIL_SIZE + 8));
	attachment_scroll->set_visible(false);
	input_container->add_child(attachment_scroll);

	attachment_preview_container = memnew(HBoxContainer);
	attachment_preview_container->add_theme_constant_override("separation", 4);
	attachment_scroll->add_child(attachment_preview_container);

	prompt_input = memnew(TextEdit);
	prompt_input->set_custom_minimum_size(Size2(0, 60));
	prompt_input->set_placeholder("Describe what you want to create...");
	prompt_input->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);
	input_container->add_child(prompt_input);

	// Button row with Attach, Send and Stop
	button_container = memnew(HBoxContainer);
	input_container->add_child(button_container);

	attach_image_button = memnew(Button);
	attach_image_button->set_text("Img+");
	attach_image_button->set_tooltip_text("Attach image(s) - PNG, JPG, WebP, GIF (max 10 MB each)");
	button_container->add_child(attach_image_button);

	screenshot_button = memnew(Button);
	screenshot_button->set_text(U"\U0001F4F7");
	screenshot_button->set_tooltip_text(TTR("Capture game screenshot and send for AI analysis (game must be running)"));
	button_container->add_child(screenshot_button);

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

	// Slash command autocomplete (inline container above prompt_input)
	slash_hint_container = memnew(VBoxContainer);
	slash_hint_container->set_visible(false);
	input_container->add_child(slash_hint_container);
	input_container->move_child(slash_hint_container, 0); // Place above prompt_input

	// File dialog for image selection (multi-select, filesystem access)
	image_file_dialog = memnew(EditorFileDialog);
	image_file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILES);
	image_file_dialog->set_title(TTR("Select Image(s) to Attach"));
	image_file_dialog->add_filter("*.png", TTR("PNG Image"));
	image_file_dialog->add_filter("*.jpg", TTR("JPEG Image"));
	image_file_dialog->add_filter("*.jpeg", TTR("JPEG Image"));
	image_file_dialog->add_filter("*.webp", TTR("WebP Image"));
	image_file_dialog->add_filter("*.gif", TTR("GIF Image"));
	image_file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	add_child(image_file_dialog);

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

	// HTTP Request node for session history menu
	session_history_list_http = memnew(HTTPRequest);
	add_child(session_history_list_http);

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

	// Settings dialog (asset providers + prompt management)
	settings_dialog = memnew(AcceptDialog);
	settings_dialog->set_title("Settings");
	settings_dialog->set_ok_button_text("Save");
	settings_dialog->set_min_size(Size2(550, 500));

	settings_tabs = memnew(TabContainer);
	settings_tabs->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	// ── Tab 0: Providers ──
	VBoxContainer *providers_tab = memnew(VBoxContainer);
	providers_tab->set_name("Providers");
	settings_tabs->add_child(providers_tab);

	// Replicate (2D textures)
	HBoxContainer *replicate_header = memnew(HBoxContainer);
	Label *replicate_label = memnew(Label);
	replicate_label->set_text("Replicate API Token:");
	replicate_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	replicate_header->add_child(replicate_label);
	replicate_test_button = memnew(Button);
	replicate_test_button->set_text("Test");
	replicate_header->add_child(replicate_test_button);
	replicate_status_label = memnew(Label);
	replicate_status_label->set_text("");
	replicate_status_label->add_theme_font_size_override("font_size", 11);
	replicate_status_label->set_custom_minimum_size(Size2(80, 0));
	replicate_header->add_child(replicate_status_label);
	providers_tab->add_child(replicate_header);

	Label *replicate_hint = memnew(Label);
	replicate_hint->set_text("Get your token from replicate.com/account/api-tokens");
	replicate_hint->add_theme_font_size_override("font_size", 11);
	replicate_hint->add_theme_color_override("font_color", Color(0.6, 0.6, 0.6));
	providers_tab->add_child(replicate_hint);

	replicate_token_input = memnew(LineEdit);
	replicate_token_input->set_placeholder("r8_...");
	replicate_token_input->set_secret(true);
	providers_tab->add_child(replicate_token_input);

	// Meshy (3D models)
	providers_tab->add_child(memnew(HSeparator));

	HBoxContainer *meshy_header = memnew(HBoxContainer);
	Label *meshy_label = memnew(Label);
	meshy_label->set_text("Meshy API Key (3D Models):");
	meshy_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	meshy_header->add_child(meshy_label);
	meshy_test_button = memnew(Button);
	meshy_test_button->set_text("Test");
	meshy_header->add_child(meshy_test_button);
	meshy_status_label = memnew(Label);
	meshy_status_label->set_text("");
	meshy_status_label->add_theme_font_size_override("font_size", 11);
	meshy_status_label->set_custom_minimum_size(Size2(80, 0));
	meshy_header->add_child(meshy_status_label);
	providers_tab->add_child(meshy_header);

	Label *meshy_hint = memnew(Label);
	meshy_hint->set_text("Get your key from meshy.ai — used for AI 3D model generation");
	meshy_hint->add_theme_font_size_override("font_size", 11);
	meshy_hint->add_theme_color_override("font_color", Color(0.6, 0.6, 0.6));
	providers_tab->add_child(meshy_hint);

	meshy_token_input = memnew(LineEdit);
	meshy_token_input->set_placeholder("msy_...");
	meshy_token_input->set_secret(true);
	providers_tab->add_child(meshy_token_input);

	// ── Tab 1: Prompt ──
	VBoxContainer *prompt_tab = memnew(VBoxContainer);
	prompt_tab->set_name("Prompt");
	settings_tabs->add_child(prompt_tab);

	// Project Prompt section header
	Label *project_section = memnew(Label);
	project_section->set_text("Project Prompt");
	project_section->add_theme_font_size_override("font_size", 14);
	prompt_tab->add_child(project_section);

	project_prompt_path_label = memnew(Label);
	project_prompt_path_label->set_text("Path: (loading...)");
	project_prompt_path_label->add_theme_font_size_override("font_size", 11);
	project_prompt_path_label->add_theme_color_override("font_color", Color(0.6, 0.6, 0.6));
	prompt_tab->add_child(project_prompt_path_label);

	project_prompt_edit = memnew(TextEdit);
	project_prompt_edit->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	project_prompt_edit->set_custom_minimum_size(Size2(0, 200));
	project_prompt_edit->set_placeholder("Write project-specific AI instructions here...\nThis file is read by the AI as context for every conversation.");
	project_prompt_edit->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	prompt_tab->add_child(project_prompt_edit);

	HBoxContainer *prompt_buttons = memnew(HBoxContainer);
	generate_prompt_button = memnew(Button);
	generate_prompt_button->set_text("Generate Template");
	generate_prompt_button->set_tooltip_text("Scan project and generate an initial prompt template");
	prompt_buttons->add_child(generate_prompt_button);
	prompt_tab->add_child(prompt_buttons);

	// Engine Prompts section (read-only)
	prompt_tab->add_child(memnew(HSeparator));

	Label *engine_section = memnew(Label);
	engine_section->set_text("Engine Prompts (Read-Only)");
	engine_section->add_theme_font_size_override("font_size", 14);
	prompt_tab->add_child(engine_section);

	engine_prompt_tree = memnew(Tree);
	engine_prompt_tree->set_custom_minimum_size(Size2(0, 100));
	engine_prompt_tree->set_hide_root(true);
	engine_prompt_tree->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	prompt_tab->add_child(engine_prompt_tree);

	engine_prompt_preview = memnew(RichTextLabel);
	engine_prompt_preview->set_custom_minimum_size(Size2(0, 100));
	engine_prompt_preview->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	engine_prompt_preview->set_selection_enabled(true);
	engine_prompt_preview->set_use_bbcode(false);
	engine_prompt_preview->set_text("Select an engine prompt file above to preview its contents.");
	prompt_tab->add_child(engine_prompt_preview);

	settings_dialog->add_child(settings_tabs);
	add_child(settings_dialog);

	settings_http_request = memnew(HTTPRequest);
	add_child(settings_http_request);
	settings_http_request->connect("request_completed", Callable(this, "_on_settings_request_completed"));
	replicate_test_button->connect("pressed", Callable(this, "_on_replicate_test_pressed"));
	meshy_test_button->connect("pressed", Callable(this, "_on_meshy_test_pressed"));
	generate_prompt_button->connect("pressed", Callable(this, "_on_generate_prompt_pressed"));
	engine_prompt_tree->connect("item_selected", Callable(this, "_on_engine_prompt_selected"));
	settings_dialog->connect("confirmed", Callable(this, "_on_settings_save_pressed"));

	// Tool detail viewer popup (shows full input/output on click)
	tool_detail_dialog = memnew(AcceptDialog);
	tool_detail_dialog->set_title("Tool Details");
	tool_detail_dialog->set_min_size(Size2(600, 400));
	tool_detail_content = memnew(RichTextLabel);
	tool_detail_content->set_use_bbcode(true);
	tool_detail_content->set_selection_enabled(true);
	tool_detail_content->set_context_menu_enabled(true);
	tool_detail_content->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tool_detail_dialog->add_child(tool_detail_content);
	add_child(tool_detail_dialog);

	// Initial status
	_update_status("Disconnected", Color(0.5, 0.5, 0.5));
	_update_connection_indicator();
}

void AIAssistantDock::_setup_logs_tab() {
	// Logs panel is hidden but still in tree so log entries can be appended.
	logs_tab = memnew(VBoxContainer);
	logs_tab->set_name("Logs");
	logs_tab->set_visible(false);
	main_container->add_child(logs_tab);

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
	send_button->connect("pressed", Callable(this, "_on_send_pressed"));
	stop_button->connect("pressed", Callable(this, "_on_stop_pressed"));
	settings_button->connect("pressed", Callable(this, "_on_settings_pressed"));
	new_instance_button->connect("pressed", Callable(this, "_on_new_instance_pressed"));
	prompt_input->connect("gui_input", Callable(this, "_on_prompt_input_gui_input"));
	prompt_input->connect("text_changed", Callable(this, "_on_prompt_text_changed"));
	http_request->connect("request_completed", Callable(this, "_on_http_request_completed"));

	// Image attachment signals
	attach_image_button->connect("pressed", Callable(this, "_on_attach_image_pressed"));
	image_file_dialog->connect("files_selected", Callable(this, "_on_image_files_selected"));
	screenshot_button->connect("pressed", Callable(this, "_on_screenshot_pressed"));

	// Sticky header scroll tracking
	chat_scroll->get_v_scroll_bar()->connect("value_changed", Callable(this, "_on_chat_scroll_changed"));

	// Processing indicator
	processing_timer->connect("timeout", Callable(this, "_on_processing_timer_timeout"));

	// Logs signals
	logs_refresh_button->connect("pressed", Callable(this, "_on_logs_refresh_pressed"));
	logs_clear_button->connect("pressed", Callable(this, "_on_logs_clear_pressed"));
	logs_http_request->connect("request_completed", Callable(this, "_on_logs_http_request_completed"));
	logs_poll_timer->connect("timeout", Callable(this, "_on_logs_poll_timeout"));

	// Command polling signals (auto-run from OpenCode)
	command_http_request->connect("request_completed", Callable(this, "_on_command_http_request_completed"));
	command_poll_timer->connect("timeout", Callable(this, "_on_command_poll_timeout"));

	// Question polling signals (AI asking user questions)
	question_http_request->connect("request_completed", Callable(this, "_on_question_http_request_completed"));
	question_poll_timer->connect("timeout", Callable(this, "_on_question_poll_timeout"));

	// Stream polling signals (for intermediate steps during AI processing)
	stream_http_request->connect("request_completed", Callable(this, "_on_stream_http_request_completed"));
	stream_poll_timer->connect("timeout", Callable(this, "_on_stream_poll_timeout"));

	// Provider/model fetching signals (submenu signals connected in _populate_model_menu)
	model_http_request->connect("request_completed", Callable(this, "_on_provider_request_completed"));

	// Auth flow signals
	http_auth_request->connect("request_completed", Callable(this, "_on_auth_request_completed"));
	auth_code_dialog->connect("confirmed", Callable(this, "_on_auth_code_submitted"));
	auth_poll_timer->connect("timeout", Callable(this, "_poll_oauth_callback"));

	// Session list signals (for finding existing sessions)
	session_list_http_request->connect("request_completed", Callable(this, "_on_session_list_completed"));

	// Session history panel signals
	session_history_list_http->connect("request_completed", Callable(this, "_on_session_history_list_completed"));
	session_history_button->connect("pressed", Callable(this, "_on_session_history_pressed"));
	session_delete_confirm->connect("confirmed", Callable(this, "_on_session_delete_confirmed"));
	session_rename_dialog->connect("confirmed", Callable(this, "_on_session_rename_confirmed"));
	session_rename_http->connect("request_completed", Callable(this, "_on_session_rename_completed"));

	// Game stop signal for auto-verification
	EditorRunBar *run_bar = EditorRunBar::get_singleton();
	if (run_bar) {
		run_bar->connect("stop_pressed", Callable(this, "_on_game_stopped"));
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
			// Connect to viewport files_dropped for external drag-and-drop
			get_tree()->get_root()->connect("files_dropped",
					Callable(this, "_on_files_dropped_on_dock"));
		} break;

		case NOTIFICATION_READY: {
			// Debugger error monitoring is handled via NOTIFICATION_PROCESS polling.
			// EditorDebuggerNode does not expose an error signal we can connect to.
		} break;

		case NOTIFICATION_PROCESS: {
			// Only primary instance polls for debugger errors.
			if (instance_id == 0 && EditorInterface::get_singleton()->is_playing_scene()) {
				_check_debugger_errors();
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			// Disconnect files_dropped signal
			if (get_tree() && get_tree()->get_root() &&
					get_tree()->get_root()->is_connected("files_dropped",
							Callable(this, "_on_files_dropped_on_dock"))) {
				get_tree()->get_root()->disconnect("files_dropped",
						Callable(this, "_on_files_dropped_on_dock"));
			}
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
		print_line("[AIAssistant] Health check skipped: pending_request=" + String::num_int64(pending_request));
		return;
	}

	pending_request = REQUEST_HEALTH;
	connection_status = CONNECTING;
	_update_status("Checking service...", Color(1, 1, 0));
	_update_connection_indicator();

	String url = service_url + "/global/health";
	print_line("[AIAssistant] Health check → " + url);
	Vector<String> headers = _get_headers_with_directory();
	Error err = http_request->request(url, headers);
	if (err != OK) {
		print_line("[AIAssistant] Health check request failed immediately: error=" + String::num_int64(err));
		pending_request = REQUEST_NONE;
		connection_status = CONNECTION_ERROR;
		_update_status("Request failed (err " + String::num_int64(err) + ")", Color(1, 0, 0));
		_update_connection_indicator();
		_add_system_message("Failed to send health check request. Error code: " + String::num_int64(err));
	}
}

void AIAssistantDock::_create_session() {
	if (pending_request != REQUEST_NONE) {
		print_line("[AIAssistant] Create session skipped: pending_request=" + String::num_int64(pending_request));
		return;
	}

	pending_request = REQUEST_SESSION;
	_update_status("Creating session...", Color(1, 1, 0));

	String url = service_url + "/session?directory=" + _get_project_directory().uri_encode();
	print_line("[AIAssistant] Creating session → " + url);
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

	has_user_message = true;
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

	// Inject project context (worldbuilding + visual bible) on every message for ALL instances.
	// The context is dynamic — it changes based on whether docs/worldbuilding.md exists.
	{
		String project_context = AIAssistantManager::get_singleton()->build_project_context();
		if (!project_context.is_empty()) {
			Dictionary ctx_part;
			ctx_part["type"] = "text";
			ctx_part["text"] = project_context;
			parts.push_back(ctx_part);
		}
	}

	String message_content = p_content;

	Dictionary text_part;
	text_part["type"] = "text";
	text_part["text"] = message_content;
	parts.push_back(text_part);

	// Append image attachments as file parts
	for (int i = 0; i < pending_attachments.size(); i++) {
		const AttachmentInfo &att = pending_attachments[i];
		Dictionary file_part;
		file_part["type"] = "file";
		file_part["mime"] = att.mime_type;
		file_part["filename"] = att.filename;
		file_part["url"] = _encode_data_url(att.mime_type, att.data);
		parts.push_back(file_part);
	}

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
	print_line("[AIAssistant] HTTP completed: request_type=" + String::num_int64(request_type) + " result=" + String::num_int64(p_result) + " code=" + String::num_int64(p_code) + " body_size=" + String::num_int64(p_body.size()));

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
				has_user_message = false;
				connection_status = CONNECTED;
				_update_status("Connected", Color(0, 1, 0));
				_update_connection_indicator();
	
				session_history_button->set_text("New Session");
				// Re-enable input (may have been disabled in "no session" state).
				prompt_input->set_editable(true);
				prompt_input->set_placeholder(TTR("Type a message..."));
				send_button->set_disabled(false);

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
				// Auto-configure saved provider keys on the server
				_auto_configure_providers();

				// Fetch config to get saved model, then fetch available models
				_fetch_config();
			} else {
				connection_status = CONNECTION_ERROR;
				_update_status("Session failed", Color(1, 0, 0));
				_update_connection_indicator();
				_add_system_message("Failed to create session");
			}
		} break;

		case REQUEST_DELETE_SESSION: {
			// Session deleted on server, now reset and create new one
			session_id = "";
			has_user_message = false;
			coding_standards_injected = false;
			_add_system_message("Previous session deleted. Creating new session...");
			_create_session();
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
			// Restore status — config loading is done.
			_update_status("Connected", Color(0, 1, 0));
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
	if (prompt.is_empty() && pending_attachments.is_empty()) {
		return;
	}

	if (prompt.is_empty()) {
		prompt = "[Image attachment(s)]";
	}

	_add_user_message(prompt);
	_process_prompt(prompt);
	prompt_input->set_text("");
	_clear_attachments();
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

void AIAssistantDock::_on_clear_pressed() {
	// If the current session has no user messages, delete it (no value in keeping it).
	// Otherwise, leave it in session history for later access.
	if (!has_user_message && !session_id.is_empty()) {
		_fire_and_forget_delete_session(session_id);
	}

	_clear_chat_ui();
	_add_system_message("Chat cleared. Creating new session...");

	// Reset and create new session.
	session_id = "";
	has_user_message = false;
	coding_standards_injected = false;
	_create_session();
}

void AIAssistantDock::_on_settings_pressed() {
	replicate_status_label->set_text("");
	meshy_status_label->set_text("");
	// Pre-fill with saved tokens from EditorSettings
	String saved_replicate = EDITOR_GET("ai/providers/replicate/api_key");
	if (!saved_replicate.is_empty()) {
		replicate_token_input->set_text(saved_replicate);
	}
	String saved_meshy = EDITOR_GET("ai/providers/meshy/api_key");
	if (!saved_meshy.is_empty()) {
		meshy_token_input->set_text(saved_meshy);
	}

	// Load project prompt (CLAUDE.md)
	project_prompt_edit->set_text(_load_project_prompt());
	String prompt_path = _get_project_directory().path_join("CLAUDE.md");
	project_prompt_path_label->set_text("Path: " + prompt_path);

	// Populate engine prompt file list
	_populate_engine_prompt_tree();
	engine_prompt_preview->set_text("Select an engine prompt file above to preview its contents.");

	settings_dialog->popup_centered();
}

void AIAssistantDock::_on_replicate_test_pressed() {
	_test_provider("replicate", replicate_token_input, replicate_test_button, replicate_status_label);
}

void AIAssistantDock::_on_meshy_test_pressed() {
	_test_provider("meshy", meshy_token_input, meshy_test_button, meshy_status_label);
}

void AIAssistantDock::_test_provider(const String &p_provider_id, LineEdit *p_input, Button *p_button, Label *p_status) {
	String token = p_input->get_text().strip_edges();
	if (token.is_empty()) {
		p_status->set_text("No key");
		p_status->add_theme_color_override("font_color", Color(1, 0.5, 0));
		return;
	}

	p_status->set_text("Testing...");
	p_status->add_theme_color_override("font_color", Color(1, 1, 0));
	p_button->set_disabled(true);
	settings_testing_provider = p_provider_id;

	String url = service_url + "/ai-assets/providers/configure";
	Dictionary body;
	body["providerId"] = p_provider_id;
	body["apiKey"] = token;

	String json_body = JSON::stringify(body);
	print_line("[AI Settings] Testing provider: " + p_provider_id + " url: " + url);
	print_line("[AI Settings] Body: " + json_body);

	Vector<String> headers = _get_headers_with_directory();
	Error err = settings_http_request->request(url, headers, HTTPClient::METHOD_POST, json_body);
	if (err != OK) {
		print_line("[AI Settings] HTTPRequest::request() failed with error: " + itos(err));
		p_status->set_text("Req error");
		p_status->add_theme_color_override("font_color", Color(1, 0, 0));
		p_button->set_disabled(false);
	}
}

void AIAssistantDock::_on_settings_save_pressed() {
	Vector<String> headers = _get_headers_with_directory();
	bool any_saved = false;

	// Save Replicate token
	String replicate_token = replicate_token_input->get_text().strip_edges();
	if (!replicate_token.is_empty()) {
		EditorSettings::get_singleton()->set("ai/providers/replicate/api_key", replicate_token);
		any_saved = true;

		// Configure on running server (fire-and-forget)
		Dictionary body;
		body["providerId"] = "replicate";
		body["apiKey"] = replicate_token;
		String url = service_url + "/ai-assets/providers/configure";
		HTTPRequest *req = memnew(HTTPRequest);
		add_child(req);
		req->connect("request_completed", callable_mp((Node *)req, &Node::queue_free).unbind(4));
		req->request(url, headers, HTTPClient::METHOD_POST, JSON::stringify(body));
	}

	// Save Meshy token
	String meshy_token = meshy_token_input->get_text().strip_edges();
	if (!meshy_token.is_empty()) {
		EditorSettings::get_singleton()->set("ai/providers/meshy/api_key", meshy_token);
		any_saved = true;

		Dictionary body;
		body["providerId"] = "meshy";
		body["apiKey"] = meshy_token;
		String url = service_url + "/ai-assets/providers/configure";
		HTTPRequest *req = memnew(HTTPRequest);
		add_child(req);
		req->connect("request_completed", callable_mp((Node *)req, &Node::queue_free).unbind(4));
		req->request(url, headers, HTTPClient::METHOD_POST, JSON::stringify(body));
	}

	if (any_saved) {
		EditorSettings::get_singleton()->save();
	}

	// Save project prompt (CLAUDE.md)
	String prompt_content = project_prompt_edit->get_text();
	if (!prompt_content.is_empty()) {
		_save_project_prompt(prompt_content);
	} else {
		// If content was cleared and file exists, save empty to clear it
		String prompt_path = _get_project_directory().path_join("CLAUDE.md");
		if (FileAccess::exists(prompt_path)) {
			_save_project_prompt("");
		}
	}
}

void AIAssistantDock::_on_settings_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	// Resolve which provider's UI to update
	Button *btn = replicate_test_button;
	Label *status = replicate_status_label;
	if (settings_testing_provider == "meshy") {
		btn = meshy_test_button;
		status = meshy_status_label;
	}
	btn->set_disabled(false);

	String response_text = String::utf8((const char *)p_body.ptr(), p_body.size());
	print_line("[AI Settings] Test result for " + settings_testing_provider + ": http_result=" + itos(p_result) + " code=" + itos(p_code) + " body=" + response_text);

	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		status->set_text("Failed");
		status->add_theme_color_override("font_color", Color(1, 0, 0));
		return;
	}

	JSON json;
	Error err = json.parse(response_text);

	if (err != OK) {
		status->set_text("Error");
		status->add_theme_color_override("font_color", Color(1, 0, 0));
		return;
	}

	Dictionary result = json.get_data();
	if (result.get("success", false)) {
		status->set_text("Connected!");
		status->add_theme_color_override("font_color", Color(0, 1, 0));
		_add_system_message(settings_testing_provider.capitalize() + " provider configured successfully!");
	} else {
		String error_msg = result.get("error", "Unknown error");
		print_line("[AI Settings] Provider returned error: " + error_msg);
		status->set_text("Failed");
		status->add_theme_color_override("font_color", Color(1, 0, 0));
	}
}

void AIAssistantDock::_auto_configure_providers() {
	// Send all saved provider API keys to the running server.
	// Called on connect/reconnect so the server always has the keys.
	struct ProviderEntry {
		const char *id;
		const char *setting;
	};
	ProviderEntry entries[] = {
		{ "replicate", "ai/providers/replicate/api_key" },
		{ "meshy", "ai/providers/meshy/api_key" },
		{ "doubao", "ai/providers/doubao/api_key" },
		{ "suno", "ai/providers/suno/api_key" },
	};

	Vector<String> headers = _get_headers_with_directory();

	for (const ProviderEntry &entry : entries) {
		String api_key = EDITOR_GET(entry.setting);
		if (api_key.is_empty()) {
			continue;
		}

		Dictionary body;
		body["providerId"] = entry.id;
		body["apiKey"] = api_key;

		String json_body = JSON::stringify(body);
		String url = service_url + "/ai-assets/providers/configure";

		// Use a one-off HTTPRequest — fire and forget
		HTTPRequest *req = memnew(HTTPRequest);
		add_child(req);
		req->connect("request_completed", callable_mp((Node *)req, &Node::queue_free).unbind(4));
		req->request(url, headers, HTTPClient::METHOD_POST, json_body);
	}
}

// ── Prompt Management ─────────────────────────────────────────────────────────

String AIAssistantDock::_load_project_prompt() {
	String prompt_path = _get_project_directory().path_join("CLAUDE.md");
	Ref<FileAccess> f = FileAccess::open(prompt_path, FileAccess::READ);
	if (f.is_valid()) {
		return f->get_as_text();
	}
	return "";
}

void AIAssistantDock::_save_project_prompt(const String &p_content) {
	String prompt_path = _get_project_directory().path_join("CLAUDE.md");
	Ref<FileAccess> f = FileAccess::open(prompt_path, FileAccess::WRITE);
	if (f.is_valid()) {
		f->store_string(p_content);
		_add_system_message("[System] Project prompt saved to CLAUDE.md");
	} else {
		_add_system_message("[System] Failed to save CLAUDE.md — check file permissions.");
	}
}

String AIAssistantDock::_generate_project_prompt_template() {
	String project_name = ProjectSettings::get_singleton()->get("application/config/name");
	if (project_name.is_empty()) {
		project_name = "My Game";
	}

	String project_dir = _get_project_directory();
	String tmpl;

	tmpl += "# " + project_name + " — AI Development Guide\n\n";
	tmpl += "## Project Overview\n";
	tmpl += "[Brief description of the game, genre, target platform]\n\n";

	// Scan top-level directories
	tmpl += "## Directory Structure\n";
	Ref<DirAccess> dir = DirAccess::open(project_dir);
	if (dir.is_valid()) {
		dir->list_dir_begin();
		Vector<String> dirs;
		String entry = dir->get_next();
		while (!entry.is_empty()) {
			if (dir->current_is_dir() && entry != "." && entry != ".." && !entry.begins_with(".")) {
				dirs.push_back(entry);
			}
			entry = dir->get_next();
		}
		dir->list_dir_end();
		dirs.sort();

		for (int i = 0; i < dirs.size(); i++) {
			tmpl += "- res://" + dirs[i] + "/\n";
		}
	}
	tmpl += "\n";

	tmpl += "## Asset Paths\n";
	tmpl += "[List key asset directories and what they contain]\n\n";

	tmpl += "## Architecture\n";
	tmpl += "[Key technical decisions — composition, data-driven, etc.]\n\n";

	tmpl += "## Rules\n";
	tmpl += "[Project-specific coding rules or constraints]\n";

	return tmpl;
}

Vector<String> AIAssistantDock::_get_engine_prompt_files() {
	Vector<String> result;

	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	String docs_dir = exe_dir.path_join("..").path_join("..").path_join("docs").simplify_path();

	Ref<DirAccess> dir = DirAccess::open(docs_dir);
	if (dir.is_null()) {
		return result;
	}

	dir->list_dir_begin();
	String entry = dir->get_next();
	while (!entry.is_empty()) {
		if (!dir->current_is_dir() && entry.ends_with(".md")) {
			result.push_back(docs_dir.path_join(entry));
		}
		entry = dir->get_next();
	}
	dir->list_dir_end();
	result.sort();

	return result;
}

void AIAssistantDock::_populate_engine_prompt_tree() {
	engine_prompt_tree->clear();
	TreeItem *root = engine_prompt_tree->create_item();

	Vector<String> files = _get_engine_prompt_files();
	for (int i = 0; i < files.size(); i++) {
		TreeItem *item = engine_prompt_tree->create_item(root);
		item->set_text(0, files[i].get_file());
		item->set_metadata(0, files[i]); // Store full path in metadata
	}
}

void AIAssistantDock::_on_engine_prompt_selected() {
	TreeItem *selected = engine_prompt_tree->get_selected();
	if (!selected) {
		return;
	}

	String file_path = selected->get_metadata(0);
	Ref<FileAccess> f = FileAccess::open(file_path, FileAccess::READ);
	if (f.is_valid()) {
		engine_prompt_preview->set_text(f->get_as_text());
	} else {
		engine_prompt_preview->set_text("Could not read file: " + file_path);
	}
}

void AIAssistantDock::_on_generate_prompt_pressed() {
	String tmpl = _generate_project_prompt_template();
	String existing = project_prompt_edit->get_text().strip_edges();
	if (!existing.is_empty()) {
		// Append template below existing content
		project_prompt_edit->set_text(existing + "\n\n" + tmpl);
	} else {
		project_prompt_edit->set_text(tmpl);
	}
}

void AIAssistantDock::_on_providers_status_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	// Clean up the one-off request node
	Node *sender = Object::cast_to<Node>(get_child(get_child_count() - 1));

	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		_add_ai_message("Failed to fetch provider status.");
		return;
	}

	String response_text = String::utf8((const char *)p_body.ptr(), p_body.size());
	JSON json;
	Error err = json.parse(response_text);

	if (err != OK) {
		_add_ai_message("Invalid response from server.");
		return;
	}

	Array providers_data = json.get_data();
	if (providers_data.is_empty()) {
		_add_ai_message("**No asset providers configured.**\n\nUse `/connect replicate <token>` or click **Settings** to configure one.");
		return;
	}

	String msg = "**Configured Asset Providers:**\n\n";
	for (int i = 0; i < providers_data.size(); i++) {
		Dictionary p = providers_data[i];
		String id = p.get("name", "");
		Array types = p.get("supportedTypes", Array());
		msg += "- **" + id + "**: ";
		for (int j = 0; j < types.size(); j++) {
			if (j > 0) msg += ", ";
			msg += String(types[j]);
		}
		msg += "\n";
	}
	_add_ai_message(msg);
}

void AIAssistantDock::_on_prompt_text_changed() {
	String text = prompt_input->get_text().strip_edges();

	// Clear previous hint buttons
	for (int i = slash_hint_buttons.size() - 1; i >= 0; i--) {
		slash_hint_container->remove_child(slash_hint_buttons[i]);
		memdelete(slash_hint_buttons[i]);
	}
	slash_hint_buttons.clear();
	slash_hint_cmd_indices.clear();

	if (text.begins_with("/") && !text.contains("\n")) {
		String filter = text.to_lower();

		struct CommandDef {
			String command;
			String fill_text;
			String description;
		};
		CommandDef commands[] = {
			{ "/connect replicate <token>", "/connect replicate ", "Configure Replicate API" },
			{ "/providers", "/providers", "List configured providers" },
			{ "/disconnect replicate", "/disconnect replicate", "Remove Replicate provider" },
		};

		for (int i = 0; i < 3; i++) {
			const CommandDef &cmd = commands[i];
			if (cmd.command.to_lower().begins_with(filter) || filter == "/") {
				Button *btn = memnew(Button);
				btn->set_text(cmd.command + "   " + cmd.description);
				btn->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
				btn->add_theme_color_override("font_color", Color(0.7, 0.85, 1.0));
				btn->set_flat(true);
				btn->set_focus_mode(Control::FOCUS_NONE); // Don't steal focus from input

				int cmd_index = i;
				btn->connect("pressed", Callable(this, "_on_slash_hint_pressed").bind(cmd_index));

				slash_hint_container->add_child(btn);
				slash_hint_buttons.push_back(btn);
				slash_hint_cmd_indices.push_back(i);
			}
		}

		slash_hint_container->set_visible(slash_hint_buttons.size() > 0);
		slash_hint_selected = slash_hint_buttons.size() > 0 ? 0 : -1;
		_update_slash_hint_highlight();
	} else {
		slash_hint_container->set_visible(false);
		slash_hint_selected = -1;
	}
}

void AIAssistantDock::_on_slash_hint_pressed(int p_id) {
	String commands[] = {
		"/connect replicate ",
		"/providers",
		"/disconnect replicate",
	};

	if (p_id >= 0 && p_id < 3) {
		prompt_input->set_text(commands[p_id]);
		prompt_input->set_caret_column(commands[p_id].length());
		prompt_input->set_caret_line(0);
		prompt_input->grab_focus();
	}

	// Hide hints after selection
	slash_hint_container->set_visible(false);
}

void AIAssistantDock::_update_slash_hint_highlight() {
	for (int i = 0; i < slash_hint_buttons.size(); i++) {
		if (i == slash_hint_selected) {
			slash_hint_buttons[i]->add_theme_color_override("font_color", Color(1.0, 1.0, 1.0));
			Ref<StyleBox> hover_style = slash_hint_buttons[i]->get_theme_stylebox("hover");
			if (hover_style.is_valid()) {
				slash_hint_buttons[i]->add_theme_style_override("normal", hover_style);
			}
		} else {
			slash_hint_buttons[i]->add_theme_color_override("font_color", Color(0.7, 0.85, 1.0));
			slash_hint_buttons[i]->remove_theme_style_override("normal");
		}
	}
}

void AIAssistantDock::_on_prompt_input_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> key = p_event;
	if (!key.is_valid() || !key->is_pressed()) {
		return;
	}

	// Ctrl+V: Check for clipboard image before default text paste
	if (key->get_keycode() == Key::V && key->is_ctrl_pressed() && !key->is_shift_pressed()) {
		if (DisplayServer::get_singleton()->clipboard_has_image()) {
			if (_add_attachment_from_clipboard_image()) {
				prompt_input->accept_event();
				return;
			}
		}
		// No image in clipboard — fall through to normal text paste
	}

	bool hints_visible = slash_hint_container->is_visible() && slash_hint_buttons.size() > 0;

	if (key->get_keycode() == Key::ENTER && !key->is_shift_pressed()) {
		slash_hint_container->set_visible(false);
		_on_send_pressed();
		prompt_input->accept_event();
	} else if (key->get_keycode() == Key::ESCAPE) {
		if (hints_visible) {
			slash_hint_container->set_visible(false);
			slash_hint_selected = -1;
			prompt_input->accept_event();
		}
	} else if (key->get_keycode() == Key::TAB && hints_visible) {
		// Tab autocompletes the selected hint (map button index → command index)
		if (slash_hint_selected >= 0 && slash_hint_selected < slash_hint_cmd_indices.size()) {
			_on_slash_hint_pressed(slash_hint_cmd_indices[slash_hint_selected]);
		}
		prompt_input->accept_event();
	} else if (key->get_keycode() == Key::DOWN && hints_visible) {
		slash_hint_selected = (slash_hint_selected + 1) % slash_hint_buttons.size();
		_update_slash_hint_highlight();
		prompt_input->accept_event();
	} else if (key->get_keycode() == Key::UP && hints_visible) {
		slash_hint_selected = (slash_hint_selected - 1 + slash_hint_buttons.size()) % slash_hint_buttons.size();
		_update_slash_hint_highlight();
		prompt_input->accept_event();
	}
}

// ============================================================
// Image Attachment Methods
// ============================================================

bool AIAssistantDock::_is_supported_image_extension(const String &p_extension) const {
	String ext = p_extension.to_lower();
	return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "webp" || ext == "gif";
}

String AIAssistantDock::_get_mime_type_for_extension(const String &p_extension) const {
	String ext = p_extension.to_lower();
	if (ext == "png") {
		return "image/png";
	} else if (ext == "jpg" || ext == "jpeg") {
		return "image/jpeg";
	} else if (ext == "webp") {
		return "image/webp";
	} else if (ext == "gif") {
		return "image/gif";
	}
	return "application/octet-stream";
}

String AIAssistantDock::_encode_data_url(const String &p_mime, const Vector<uint8_t> &p_data) const {
	String b64 = CryptoCore::b64_encode_str(p_data.ptr(), p_data.size());
	return "data:" + p_mime + ";base64," + b64;
}

void AIAssistantDock::_on_attach_image_pressed() {
	image_file_dialog->popup_file_dialog();
}

void AIAssistantDock::_on_image_files_selected(const PackedStringArray &p_paths) {
	bool any_added = false;
	for (int i = 0; i < p_paths.size(); i++) {
		if (_add_attachment_from_file(p_paths[i])) {
			any_added = true;
		}
	}
	if (any_added) {
		_rebuild_attachment_previews();
	}
}

void AIAssistantDock::_on_files_dropped_on_dock(const PackedStringArray &p_files) {
	if (!is_visible_in_tree()) {
		return;
	}

	// Only accept drops when mouse is over the input area
	Vector2 global_mouse = get_global_mouse_position();
	Rect2 input_rect = input_container->get_global_rect();
	if (!input_rect.has_point(global_mouse)) {
		return;
	}

	bool any_added = false;
	for (int i = 0; i < p_files.size(); i++) {
		String ext = p_files[i].get_extension().to_lower();
		if (_is_supported_image_extension(ext)) {
			if (_add_attachment_from_file(p_files[i])) {
				any_added = true;
			}
		}
	}

	if (any_added) {
		_rebuild_attachment_previews();
	}
}

bool AIAssistantDock::_add_attachment_from_file(const String &p_path) {
	String ext = p_path.get_extension().to_lower();
	if (!_is_supported_image_extension(ext)) {
		_add_system_message("Unsupported image format: " + ext + ". Use PNG, JPG, WebP, or GIF.");
		return false;
	}

	Error err;
	Vector<uint8_t> data = FileAccess::get_file_as_bytes(p_path, &err);
	if (err != OK || data.is_empty()) {
		_add_system_message("Failed to read image file: " + p_path);
		return false;
	}

	if (data.size() > MAX_IMAGE_SIZE_BYTES) {
		float size_mb = data.size() / (1024.0f * 1024.0f);
		_add_system_message(vformat("Image too large (%.1f MB). Maximum size is 10 MB: %s", size_mb, p_path.get_file()));
		return false;
	}

	// Load image for thumbnail
	Ref<Image> img = Image::load_from_file(p_path);
	if (img.is_null() || img->is_empty()) {
		_add_system_message("Failed to load image for preview: " + p_path.get_file());
		return false;
	}

	// Create thumbnail preserving aspect ratio
	Ref<Image> thumb = img->duplicate();
	int tw = thumb->get_width();
	int th = thumb->get_height();
	if (tw > th) {
		th = MAX(1, th * THUMBNAIL_SIZE / tw);
		tw = THUMBNAIL_SIZE;
	} else {
		tw = MAX(1, tw * THUMBNAIL_SIZE / th);
		th = THUMBNAIL_SIZE;
	}
	thumb->resize(tw, th);

	AttachmentInfo att;
	att.file_path = p_path;
	att.filename = p_path.get_file();
	att.mime_type = _get_mime_type_for_extension(ext);
	att.data = data;
	att.thumbnail = ImageTexture::create_from_image(thumb);
	pending_attachments.push_back(att);

	return true;
}

bool AIAssistantDock::_add_attachment_from_clipboard_image() {
	Ref<Image> clipboard_image = DisplayServer::get_singleton()->clipboard_get_image();
	if (clipboard_image.is_null() || clipboard_image->is_empty()) {
		return false;
	}

	Vector<uint8_t> png_data = clipboard_image->save_png_to_buffer();
	if (png_data.is_empty()) {
		_add_system_message("Failed to encode clipboard image as PNG.");
		return false;
	}

	if (png_data.size() > MAX_IMAGE_SIZE_BYTES) {
		float size_mb = png_data.size() / (1024.0f * 1024.0f);
		_add_system_message(vformat("Clipboard image too large (%.1f MB). Maximum size is 10 MB.", size_mb));
		return false;
	}

	String timestamp = Time::get_singleton()->get_datetime_string_from_system().replace(":", "").replace("-", "").replace("T", "_");
	String filename = "clipboard_" + timestamp + ".png";

	// Create thumbnail
	Ref<Image> thumb = clipboard_image->duplicate();
	int tw = thumb->get_width();
	int th = thumb->get_height();
	if (tw > th) {
		th = MAX(1, th * THUMBNAIL_SIZE / tw);
		tw = THUMBNAIL_SIZE;
	} else {
		tw = MAX(1, tw * THUMBNAIL_SIZE / th);
		th = THUMBNAIL_SIZE;
	}
	thumb->resize(tw, th);

	AttachmentInfo att;
	att.file_path = "";
	att.filename = filename;
	att.mime_type = "image/png";
	att.data = png_data;
	att.thumbnail = ImageTexture::create_from_image(thumb);
	pending_attachments.push_back(att);

	_rebuild_attachment_previews();
	return true;
}

void AIAssistantDock::_on_remove_attachment(int p_index) {
	if (p_index < 0 || p_index >= pending_attachments.size()) {
		return;
	}
	pending_attachments.remove_at(p_index);
	_rebuild_attachment_previews();
}

void AIAssistantDock::_rebuild_attachment_previews() {
	// Clear existing preview children
	while (attachment_preview_container->get_child_count() > 0) {
		Node *child = attachment_preview_container->get_child(0);
		attachment_preview_container->remove_child(child);
		child->queue_free();
	}

	if (pending_attachments.is_empty()) {
		attachment_scroll->set_visible(false);
		return;
	}

	attachment_scroll->set_visible(true);

	for (int i = 0; i < pending_attachments.size(); i++) {
		const AttachmentInfo &att = pending_attachments[i];

		VBoxContainer *item = memnew(VBoxContainer);
		item->set_custom_minimum_size(Size2(THUMBNAIL_SIZE + 8, THUMBNAIL_SIZE + 24));

		TextureRect *tex_rect = memnew(TextureRect);
		tex_rect->set_texture(att.thumbnail);
		tex_rect->set_custom_minimum_size(Size2(THUMBNAIL_SIZE, THUMBNAIL_SIZE));
		tex_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
		tex_rect->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
		tex_rect->set_tooltip_text(att.filename + " (" + String::humanize_size(att.data.size()) + ")");
		item->add_child(tex_rect);

		Button *remove_btn = memnew(Button);
		remove_btn->set_text("x");
		remove_btn->set_tooltip_text("Remove " + att.filename);
		remove_btn->set_custom_minimum_size(Size2(0, 18));
		remove_btn->add_theme_font_size_override("font_size", 10);
		remove_btn->connect("pressed", Callable(this, "_on_remove_attachment").bind(i), CONNECT_DEFERRED);
		item->add_child(remove_btn);

		attachment_preview_container->add_child(item);
	}
}

void AIAssistantDock::_clear_attachments() {
	pending_attachments.clear();
	_rebuild_attachment_previews();
}

void AIAssistantDock::_process_prompt(const String &p_prompt) {
	// Intercept slash commands before sending to AI
	if (p_prompt.begins_with("/")) {
		_process_slash_command(p_prompt);
		return;
	}

	if (connection_status == CONNECTED && !session_id.is_empty()) {
		_send_message(p_prompt);
	} else {
		_process_local_command(p_prompt);
	}
}

void AIAssistantDock::_process_slash_command(const String &p_command) {
	String lower = p_command.to_lower().strip_edges();

	if (lower.begins_with("/connect replicate")) {
		// Extract token if provided inline: /connect replicate r8_xxxxx
		String token;
		PackedStringArray parts = p_command.strip_edges().split(" ");
		if (parts.size() >= 3) {
			token = parts[2];
		}

		if (token.is_empty()) {
			_add_ai_message("Please provide your Replicate API token:\n\n`/connect replicate <your_token>`\n\nGet your token from: **replicate.com/account/api-tokens**");
			return;
		}

		_add_system_message("Configuring Replicate provider...");

		// POST to configure endpoint
		String url = service_url + "/ai-assets/providers/configure";
		Dictionary body;
		body["providerId"] = "replicate";
		body["apiKey"] = token;

		String json_body = JSON::stringify(body);
		Vector<String> headers = _get_headers_with_directory();
		settings_http_request->request(url, headers, HTTPClient::METHOD_POST, json_body);
	} else if (lower.begins_with("/disconnect replicate")) {
		_add_system_message("Provider disconnection is not yet supported. Restart the engine to reset providers.");
	} else if (lower == "/providers") {
		if (connection_status != CONNECTED) {
			_add_ai_message("Not connected to AI service. Click **Connect** first.");
			return;
		}

		// Fetch provider status
		String url = service_url + "/ai-assets/providers/status";
		Vector<String> headers = _get_headers_with_directory();

		// Use a one-off request to fetch and display
		HTTPRequest *status_request = memnew(HTTPRequest);
		add_child(status_request);
		status_request->connect("request_completed", Callable(this, "_on_providers_status_completed"));
		status_request->request(url, headers);
	} else {
		// Unknown slash command - pass through to AI
		if (connection_status == CONNECTED && !session_id.is_empty()) {
			_send_message(p_command);
		} else {
			_add_ai_message("Unknown command: " + p_command + "\n\nAvailable commands:\n- `/connect replicate <token>` - Configure Replicate\n- `/providers` - List configured providers\n- `/disconnect replicate` - Remove provider");
		}
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
}

void AIAssistantDock::_scroll_chat_to_bottom() {
	chat_scroll->set_v_scroll(INT32_MAX);
}

void AIAssistantDock::_toggle_turn_collapse(int p_user_msg_index) {
	bool is_collapsed = collapsed_turns.has(p_user_msg_index) && collapsed_turns[p_user_msg_index];
	bool new_state = !is_collapsed;
	collapsed_turns[p_user_msg_index] = new_state;

	// Update button text
	if (collapse_buttons.has(p_user_msg_index)) {
		collapse_buttons[p_user_msg_index]->set_text(new_state ? U"\u25B6" : U"\u25BC"); // ▶ collapsed, ▼ expanded
	}

	// Find the range of children to hide/show: from (p_user_msg_index + 1) to next user message index
	int start_idx = p_user_msg_index + 1;
	int end_idx = chat_container->get_child_count(); // default: to end

	for (int i = 0; i < user_message_indices.size(); i++) {
		if (user_message_indices[i] == p_user_msg_index) {
			if (i + 1 < user_message_indices.size()) {
				end_idx = user_message_indices[i + 1];
			}
			break;
		}
	}

	for (int i = start_idx; i < end_idx && i < chat_container->get_child_count(); i++) {
		Node *child = chat_container->get_child(i);
		Control *ctrl = Object::cast_to<Control>(child);
		if (ctrl) {
			ctrl->set_visible(!new_state);
		}
	}

	// Sync sticky header if it's showing this turn
	if (sticky_current_turn_index == p_user_msg_index) {
		sticky_collapse_btn->set_text(new_state ? U"\u25B6" : U"\u25BC");
	}
}

void AIAssistantDock::_on_chat_scroll_changed(double p_value) {
	_update_sticky_header();
}

void AIAssistantDock::_update_sticky_header() {
	if (user_message_indices.is_empty()) {
		sticky_header->set_visible(false);
		sticky_current_turn_index = -1;
		return;
	}

	// Find which user message's response is currently in view.
	// The sticky header shows the user message whose container is scrolled
	// above (or at) the top of the visible area.
	float scroll_top = chat_scroll->get_v_scroll();
	int found_index = -1;

	for (int i = user_message_indices.size() - 1; i >= 0; i--) {
		int child_idx = user_message_indices[i];
		if (child_idx >= chat_container->get_child_count()) {
			continue;
		}
		Control *child = Object::cast_to<Control>(chat_container->get_child(child_idx));
		if (!child) {
			continue;
		}
		// The child's position.y is relative to chat_container (which scrolls).
		// If its top is at or above the scroll position, this turn is currently being viewed.
		float child_top = child->get_position().y;
		if (child_top <= scroll_top + 5) { // small threshold
			found_index = child_idx;
			break;
		}
	}

	if (found_index < 0) {
		sticky_header->set_visible(false);
		sticky_current_turn_index = -1;
		return;
	}

	// Only show sticky header if the user message itself is scrolled out of view
	Control *user_msg = Object::cast_to<Control>(chat_container->get_child(found_index));
	if (user_msg) {
		float msg_bottom = user_msg->get_position().y + user_msg->get_size().y;
		if (msg_bottom > scroll_top) {
			// User message is still partially visible — no need for sticky
			sticky_header->set_visible(false);
			sticky_current_turn_index = -1;
			return;
		}
	}

	// Update sticky header content
	if (sticky_current_turn_index != found_index) {
		// Disconnect old signal — disconnect all "pressed" connections to avoid stale binds
		List<Object::Connection> pressed_conns;
		sticky_collapse_btn->get_signal_connection_list("pressed", &pressed_conns);
		for (const Object::Connection &conn : pressed_conns) {
			sticky_collapse_btn->disconnect("pressed", conn.callable);
		}

		sticky_current_turn_index = found_index;

		// Set text
		if (user_message_texts.has(found_index)) {
			sticky_text_label->set_text(user_message_texts[found_index]);
		}

		// Set collapse state
		bool is_collapsed = collapsed_turns.has(found_index) && collapsed_turns[found_index];
		sticky_collapse_btn->set_text(is_collapsed ? U"\u25B6" : U"\u25BC");

		// Connect to toggle for this turn
		sticky_collapse_btn->connect("pressed", Callable(this, "_toggle_turn_collapse").bind(found_index));
	}

	sticky_header->set_visible(true);
}

void AIAssistantDock::_add_user_message(const String &p_text) {
	// Skip injected coding standards — they're internal context, not user-visible.
	if (p_text.begins_with("[CODING STANDARDS]")) {
		return;
	}

	String display_text = p_text;
	if (!pending_attachments.is_empty()) {
		display_text += "\n[color=gray][" + itos(pending_attachments.size()) + " image(s) attached][/color]";
	}

	// Build user message container with collapse button
	Dictionary msg;
	msg["sender"] = "You";
	msg["text"] = display_text;
	msg["timestamp"] = Time::get_singleton()->get_datetime_string_from_system();
	chat_history.push_back(msg);

	VBoxContainer *msg_container = memnew(VBoxContainer);

	// Header row: collapse button + message text
	HBoxContainer *header_row = memnew(HBoxContainer);
	msg_container->add_child(header_row);

	Button *collapse_btn = memnew(Button);
	collapse_btn->set_text(U"\u25BC"); // ▼ (expanded)
	collapse_btn->set_custom_minimum_size(Size2(24, 24));
	collapse_btn->set_tooltip_text("Collapse/expand AI response");
	collapse_btn->add_theme_font_size_override("font_size", 10);
	header_row->add_child(collapse_btn);

	String role_hex = Color(0.4, 0.6, 1.0).to_html(false);
	String formatted_text = "[color=#" + role_hex + "][b]You:[/b][/color] " + display_text;

	RichTextLabel *text_label = memnew(RichTextLabel);
	text_label->set_use_bbcode(true);
	text_label->set_fit_content(true);
	text_label->set_text(formatted_text);
	text_label->set_selection_enabled(true);
	text_label->set_context_menu_enabled(true);
	text_label->set_focus_mode(Control::FOCUS_CLICK);
	text_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header_row->add_child(text_label);

	msg_container->add_child(memnew(HSeparator));
	chat_container->add_child(msg_container);

	// Track this user message index for collapse/expand
	int child_idx = chat_container->get_child_count() - 1;
	user_message_indices.push_back(child_idx);
	collapse_buttons[child_idx] = collapse_btn;
	collapsed_turns[child_idx] = false;
	user_message_texts[child_idx] = formatted_text;
	collapse_btn->connect("pressed", Callable(this, "_toggle_turn_collapse").bind(child_idx));
}

void AIAssistantDock::_add_ai_message(const String &p_text) {
	_add_message("AI", p_text, Color(0.4, 1.0, 0.6));
}

void AIAssistantDock::_add_system_message(const String &p_text) {
	// System messages go to the Logs tab only (not shown in chat).
	_add_log_entry("SYSTEM", p_text, Color(1.0, 1.0, 0.6));
}

void AIAssistantDock::_add_tool_message(const String &p_part_id, const String &p_tool_name, const String &p_status, const Dictionary &p_details) {
	// Track start time for new tools
	if (!tool_start_times.has(p_part_id)) {
		tool_start_times[p_part_id] = Time::get_singleton()->get_ticks_msec();
	}

	// Calculate elapsed time
	uint64_t elapsed_ms = Time::get_singleton()->get_ticks_msec() - tool_start_times[p_part_id];
	float elapsed_sec = elapsed_ms / 1000.0f;

	// --- Extract input dictionary ---
	Dictionary in;
	if (p_details.has("input")) {
		Variant input_var = p_details["input"];
		if (input_var.get_type() == Variant::DICTIONARY) {
			in = input_var;
		}
	}
	if (in.is_empty() && p_details.has("raw")) {
		Variant raw_var = p_details["raw"];
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

	// --- Build input description for header line ---
	String input_info;
	String full_input; // Full input text for click-to-view

	if (p_tool_name == "task") {
		// Task tool: show @agent_type + description
		String agent_type = in.get("subagent_type", "");
		String description = in.get("description", "");
		if (!agent_type.is_empty()) {
			input_info = "@" + agent_type;
		}
		if (!description.is_empty()) {
			if (!input_info.is_empty()) {
				input_info += "  " + description;
			} else {
				input_info = description;
			}
		}
		full_input = in.get("prompt", description);
	} else if (in.has("filePath")) {
		// File tools: read, write, edit, multiedit, create_file
		String file_path = in["filePath"];
		input_info = file_path;
		// For read: append line range if offset/limit provided
		if (p_tool_name == "read" || p_tool_name == "Read") {
			int offset = in.get("offset", 0);
			int limit = in.get("limit", 0);
			if (offset > 0 || limit > 0) {
				if (limit > 0) {
					input_info += " (lines " + itos(offset) + "-" + itos(offset + limit) + ")";
				} else {
					input_info += " (from line " + itos(offset) + ")";
				}
			}
		}
		full_input = file_path;
		if (in.has("content")) {
			full_input += "\n" + String(in["content"]);
		}
		if (in.has("old_string")) {
			full_input += "\nold_string: " + String(in["old_string"]) + "\nnew_string: " + String(in["new_string"]);
		}
	} else if (in.has("command")) {
		// Bash: prefer description, full input is the command
		if (in.has("description")) {
			input_info = in["description"];
		} else {
			input_info = in["command"];
		}
		full_input = in["command"];
	} else if (in.has("pattern")) {
		// Grep, Glob: show "pattern" (in path)
		String pattern = in["pattern"];
		input_info = "\"" + pattern + "\"";
		if (in.has("path")) {
			input_info += " (in " + String(in["path"]) + ")";
		}
		full_input = pattern;
	} else if (in.has("query")) {
		input_info = in["query"];
		full_input = input_info;
	} else if (in.has("url")) {
		input_info = in["url"];
		full_input = input_info;
	} else if (in.has("operation")) {
		input_info = in["operation"];
		if (in.has("filePath")) {
			input_info += " " + String(in["filePath"]);
		}
		full_input = input_info;
	} else if (in.has("name")) {
		// Skill
		input_info = in["name"];
		full_input = input_info;
	} else if (in.has("path")) {
		input_info = in["path"];
		full_input = input_info;
	} else if (in.has("todos")) {
		Variant todos_var = in["todos"];
		if (todos_var.get_type() == Variant::ARRAY) {
			Array todos = todos_var;
			int completed_count = 0;
			String active_task;
			String todo_detail;
			for (int ti = 0; ti < todos.size(); ti++) {
				Dictionary todo = todos[ti];
				String todo_status = todo.get("status", "");
				String todo_content = todo.get("content", "");
				if (todo_status == "completed") {
					completed_count++;
					todo_detail += "[x] " + todo_content + "\n";
				} else if (todo_status == "in_progress") {
					if (active_task.is_empty()) {
						active_task = todo.get("activeForm", todo_content);
					}
					todo_detail += "[>] " + todo_content + "\n";
				} else {
					todo_detail += "[ ] " + todo_content + "\n";
				}
			}
			if (!active_task.is_empty()) {
				input_info = active_task;
			}
			String progress = itos(completed_count) + "/" + itos(todos.size()) + " done";
			if (!input_info.is_empty()) {
				input_info += " (" + progress + ")";
			} else {
				input_info = progress;
			}
			full_input = todo_detail;
		}
	} else if (in.has("tool_calls")) {
		Variant tc = in["tool_calls"];
		if (tc.get_type() == Variant::ARRAY) {
			input_info = itos(((Array)tc).size()) + " tool calls";
		}
		full_input = input_info;
	} else if (in.has("patchText")) {
		String patch = in["patchText"];
		int nl = patch.find("\n");
		input_info = (nl >= 0) ? patch.substr(0, nl) : patch;
		full_input = patch;
	} else {
		// Fallback: first string or array value
		Array keys = in.keys();
		for (int i = 0; i < keys.size(); i++) {
			Variant val = in[keys[i]];
			if (val.get_type() == Variant::STRING && !String(val).is_empty()) {
				input_info = val;
				full_input = input_info;
				break;
			} else if (val.get_type() == Variant::ARRAY) {
				input_info = itos(((Array)val).size()) + " items";
				full_input = input_info;
				break;
			}
		}
	}

	// Store full input for click-to-view
	if (!full_input.is_empty()) {
		tool_full_inputs[p_part_id] = full_input;
	}

	// --- Build status indicator ---
	String status_icon;
	String status_color;
	String time_str = vformat("%.1fs", elapsed_sec);

	if (p_status == "completed") {
		status_icon = String::utf8("\u2713"); // checkmark
		status_color = "66ff66";
	} else if (p_status == "running" || p_status == "pending") {
		// Show live progress title from ctx.metadata() if available
		String live_title = p_details.get("title", "");
		status_icon = live_title.is_empty() ? "..." : live_title;
		status_color = "ffff66";
	} else if (p_status == "error") {
		status_icon = String::utf8("\u2717"); // X mark
		status_color = "ff6666";
	} else {
		status_icon = "...";
		status_color = "888888";
	}

	// --- Build formatted BBCode ---
	String display_name = _format_tool_display_name(p_tool_name);
	String formatted = "[color=#9999ff][b]" + display_name + "[/b][/color]";

	// Description
	if (!input_info.is_empty()) {
		String desc = input_info;
		if (desc.length() > 120) {
			desc = desc.substr(0, 120) + "...";
		}
		formatted += "  [color=#aaaaaa]" + desc + "[/color]";
	}

	// Status + time
	formatted += "  [color=#" + status_color + "]" + status_icon + "[/color] [color=#888888](" + time_str + ")[/color]";

	// --- Task tool: subagent tools (same 2-line format, indented with │) ---
	if (p_tool_name == "task" && p_details.has("metadata")) {
		Dictionary metadata = p_details["metadata"];
		if (metadata.has("summary")) {
			Array summary = metadata["summary"];
			for (int i = 0; i < summary.size(); i++) {
				Dictionary tool_info = summary[i];
				String sub_tool = tool_info.get("tool", "?");
				Dictionary sub_state = tool_info.get("state", Dictionary());
				String sub_status = sub_state.get("status", "?");
				String sub_title = sub_state.get("title", "");
				Dictionary sub_input = sub_state.get("input", Dictionary());
				String sub_output = sub_state.get("output", "");

				// Sub-tool status icon
				String sub_icon;
				String sub_color;
				if (sub_status == "completed") {
					sub_icon = String::utf8("\u2713");
					sub_color = "66ff66";
				} else if (sub_status == "running" || sub_status == "pending") {
					sub_icon = "...";
					sub_color = "ffff66";
				} else if (sub_status == "error") {
					sub_icon = String::utf8("\u2717");
					sub_color = "ff6666";
				} else {
					sub_icon = "...";
					sub_color = "888888";
				}

				// Sub-tool description
				String sub_desc = sub_title;
				if (sub_desc.length() > 80) {
					sub_desc = sub_desc.substr(0, 80) + "...";
				}

				// Line 1: │ ToolName  description  ✓
				String sub_display = _format_tool_display_name(sub_tool);
				formatted += "\n[color=#555555]" + String::utf8("\u2502") + "[/color] [color=#9999cc][b]" + sub_display + "[/b][/color]";
				if (!sub_desc.is_empty()) {
					formatted += "  [color=#888888]" + sub_desc + "[/color]";
				}
				formatted += "  [color=#" + sub_color + "]" + sub_icon + "[/color]";

				// Line 2: │ N lines of output (clickable)
				if (!sub_output.is_empty()) {
					String sub_key = p_part_id + "_sub_" + itos(i);
					tool_full_outputs[sub_key] = sub_output;
					int line_count = sub_output.split("\n").size();
					String out_summary = itos(line_count) + (line_count == 1 ? " line of output" : " lines of output");
					formatted += "\n[color=#555555]" + String::utf8("\u2502") + "[/color] [url=tool://" + sub_key + "/output][color=#6699cc]" + out_summary + "[/color][/url]";
				}
			}
		}
	}

	// --- Output summary line (clickable link) ---
	if (p_status == "completed" && p_details.has("output")) {
		String output = p_details["output"];
		if (!output.is_empty()) {
			tool_full_outputs[p_part_id] = output;
			int line_count = output.split("\n").size();
			String out_summary = itos(line_count) + (line_count == 1 ? " line of output" : " lines of output");
			formatted += "\n[url=tool://" + p_part_id + "/output][color=#6699cc]" + out_summary + "[/color][/url]";
		}
	} else if (p_status == "error") {
		String error_text;
		if (p_details.has("error")) {
			error_text = p_details["error"];
		}
		if (!error_text.is_empty()) {
			tool_full_outputs[p_part_id] = error_text;
			// Show first line of error as clickable link
			int nl = error_text.find("\n");
			String error_preview = (nl >= 0) ? error_text.substr(0, nl) : error_text;
			if (error_preview.length() > 80) {
				error_preview = error_preview.substr(0, 80) + "...";
			}
			formatted += "\n[url=tool://" + p_part_id + "/output][color=#ff6666]Error: " + error_preview + "[/color][/url]";
		}
	}

	// --- Update existing or create new UI element ---
	if (tool_containers.has(p_part_id)) {
		RichTextLabel *tool_label = tool_containers[p_part_id];
		if (tool_label) {
			tool_label->set_text(formatted);
		}
		return;
	}

	// Create a new tool message container
	VBoxContainer *container = memnew(VBoxContainer);

	RichTextLabel *tool_label = memnew(RichTextLabel);
	tool_label->set_use_bbcode(true);
	tool_label->set_fit_content(true);
	tool_label->set_text(formatted);
	tool_label->set_selection_enabled(true);
	tool_label->set_context_menu_enabled(true);
	tool_label->set_focus_mode(Control::FOCUS_CLICK);
	tool_label->connect("meta_clicked", Callable(this, "_on_tool_meta_clicked"));
	container->add_child(tool_label);

	container->add_child(memnew(HSeparator));

	tool_containers[p_part_id] = tool_label;
	chat_container->add_child(container);
}

void AIAssistantDock::_clear_tool_tracking() {
	tool_containers.clear();
	tool_start_times.clear();
	tool_logged_status.clear();
	text_stream_labels.clear();
	tool_full_inputs.clear();
	tool_full_outputs.clear();
}

String AIAssistantDock::_format_tool_display_name(const String &p_tool_name) const {
	if (p_tool_name == "bash") return "Bash";
	if (p_tool_name == "read") return "Read";
	if (p_tool_name == "write") return "Write";
	if (p_tool_name == "edit") return "Edit";
	if (p_tool_name == "multiedit") return "MultiEdit";
	if (p_tool_name == "glob") return "Glob";
	if (p_tool_name == "grep") return "Grep";
	if (p_tool_name == "task") return "Task";
	if (p_tool_name == "todowrite") return "TodoWrite";
	if (p_tool_name == "websearch") return "WebSearch";
	if (p_tool_name == "webfetch") return "WebFetch";
	if (p_tool_name == "apply_patch") return "Patch";
	if (p_tool_name == "list") return "List";
	if (p_tool_name == "create_file") return "Create";
	if (p_tool_name == "skill") return "Skill";
	return p_tool_name.capitalize();
}

void AIAssistantDock::_on_tool_meta_clicked(const Variant &p_meta) {
	String meta = p_meta;
	if (!meta.begins_with("tool://")) {
		return;
	}
	String rest = meta.substr(7); // after "tool://"
	int slash = rest.rfind("/");
	if (slash < 0) {
		return;
	}
	String part_id = rest.substr(0, slash);
	String section = rest.substr(slash + 1);

	String content;
	if (section == "input" && tool_full_inputs.has(part_id)) {
		content = tool_full_inputs[part_id];
	} else if (section == "output" && tool_full_outputs.has(part_id)) {
		content = tool_full_outputs[part_id];
	}
	if (content.is_empty()) {
		return;
	}

	tool_detail_dialog->set_title("Tool " + section.capitalize());
	tool_detail_content->set_text(content);
	tool_detail_dialog->popup_centered_ratio(0.6);
}

void AIAssistantDock::_update_status(const String &p_text, const Color &p_color) {
	// Status is logged but not shown in UI (connection indicator dot is sufficient).
	_add_log_entry("STATUS", p_text, p_color);
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
			if (text_stream_labels.has(text_key)) {
				// Update existing streaming text label
				RichTextLabel *text_label = text_stream_labels[text_key];
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
				text_stream_labels[text_key] = text_label;

				// Log first appearance
				String preview = text.substr(0, 150);
				if (text.length() > 150) {
					preview += "...";
				}
				_add_log_entry("LLM", preview, Color(0.5, 0.9, 0.5));
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
					if (text_stream_labels.has(text_key)) {
						// Update existing streaming label with final text
						RichTextLabel *text_label = text_stream_labels[text_key];
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
	} else if (p_action == "scan_filesystem") {
		// Refresh the FileSystem dock to detect new/modified files
		EditorInterface::get_singleton()->get_resource_filesystem()->scan();
		_add_system_message("[Editor] File system scan triggered.");
	} else if (p_action == "reload_scene") {
		// Reload the currently open scene to pick up external changes
		Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
		if (root && !root->get_scene_file_path().is_empty()) {
			EditorInterface::get_singleton()->reload_scene_from_path(root->get_scene_file_path());
			_add_system_message("[Editor] Current scene reloaded.");
		} else {
			_add_system_message("[Editor] No scene open to reload.");
		}
	} else if (p_action == "screenshot") {
		// Request screenshot from the running game via debugger protocol
		String screenshot_id = p_params.get("id", "");
		if (screenshot_id.is_empty()) {
			return;
		}

		if (!EditorRunBar::get_singleton()->is_playing()) {
			_add_system_message("[Screenshot] No game running — start the game first.");
			_post_screenshot_result(screenshot_id, "");
			return;
		}

		bool ok = EditorRunBar::get_singleton()->request_screenshot(
				callable_mp_static(&AIAssistantDock::_screenshot_for_tool_static).bind(screenshot_id));

		if (!ok) {
			_add_system_message("[Screenshot] Could not request screenshot — game may not be embedded.");
			_post_screenshot_result(screenshot_id, "");
		}
	}
}

// === Screenshot Capture ===

// Static callbacks — bypass ObjectDB validity checks, dispatch to singleton instance.
void AIAssistantDock::_screenshot_for_button_static(int64_t p_w, int64_t p_h, const String &p_path, const Rect2i &p_rect) {
	if (singleton) {
		singleton->_on_screenshot_for_button(p_w, p_h, p_path, p_rect);
	}
}

void AIAssistantDock::_screenshot_for_tool_static(int64_t p_w, int64_t p_h, const String &p_path, const Rect2i &p_rect, const String &p_id) {
	if (singleton) {
		singleton->_on_screenshot_for_tool(p_w, p_h, p_path, p_rect, p_id);
	}
}

void AIAssistantDock::_on_screenshot_pressed() {
	if (!EditorRunBar::get_singleton()->is_playing()) {
		_add_system_message("[Screenshot] No game running — press F5 to run the game first.");
		return;
	}

	bool ok = EditorRunBar::get_singleton()->request_screenshot(
			callable_mp_static(&AIAssistantDock::_screenshot_for_button_static));

	if (!ok) {
		_add_system_message("[Screenshot] Could not request screenshot. Make sure the game is running in embedded mode.");
	}
}

void AIAssistantDock::_on_screenshot_for_tool(int64_t p_w, int64_t p_h, const String &p_path, const Rect2i &p_rect, const String &p_id) {
	Ref<Image> img = Image::load_from_file(p_path);
	if (img.is_null() || img->is_empty()) {
		_post_screenshot_result(p_id, "");
		return;
	}
	Vector<uint8_t> png_data = img->save_png_to_buffer();
	String b64 = CryptoCore::b64_encode_str(png_data.ptr(), png_data.size());
	_post_screenshot_result(p_id, b64);
}

void AIAssistantDock::_on_screenshot_for_button(int64_t p_w, int64_t p_h, const String &p_path, const Rect2i &p_rect) {
	if (p_path.is_empty()) {
		_add_system_message("[Screenshot] Failed: no screenshot returned from game.");
		return;
	}
	Ref<Image> img = Image::load_from_file(p_path);
	if (img.is_null() || img->is_empty()) {
		_add_system_message("[Screenshot] Failed to load screenshot.");
		return;
	}
	Vector<uint8_t> png_data = img->save_png_to_buffer();
	if (!_add_attachment_from_raw_data("screenshot.png", "image/png", png_data)) {
		_add_system_message("[Screenshot] Failed to attach screenshot.");
		return;
	}
	if (prompt_input->get_text().is_empty()) {
		prompt_input->set_text("Analyze this screenshot for visual design issues and fix them.");
	}
	prompt_input->grab_focus();
}


bool AIAssistantDock::_add_attachment_from_raw_data(const String &p_filename, const String &p_mime, const Vector<uint8_t> &p_data) {
	if (p_data.is_empty()) {
		return false;
	}

	// Load image to generate thumbnail
	Ref<Image> img;
	img.instantiate();
	Error err = img->load_png_from_buffer(p_data);
	if (err != OK || img->is_empty()) {
		return false;
	}

	// Generate thumbnail
	Ref<Image> thumb = img->duplicate();
	int tw = thumb->get_width();
	int th = thumb->get_height();
	if (tw > th) {
		th = MAX(1, th * THUMBNAIL_SIZE / tw);
		tw = THUMBNAIL_SIZE;
	} else {
		tw = MAX(1, tw * THUMBNAIL_SIZE / th);
		th = THUMBNAIL_SIZE;
	}
	thumb->resize(tw, th);

	AttachmentInfo att;
	att.file_path = "";
	att.filename = p_filename;
	att.mime_type = p_mime;
	att.data = p_data;
	att.thumbnail = ImageTexture::create_from_image(thumb);
	pending_attachments.push_back(att);

	_rebuild_attachment_previews();
	return true;
}

void AIAssistantDock::_post_screenshot_result(const String &p_id, const String &p_b64) {
	String url = service_url + "/godot/screenshot-result";

	Dictionary body;
	body["id"] = p_id;
	body["data"] = p_b64;
	String json_body = JSON::stringify(body);

	HTTPRequest *req = memnew(HTTPRequest);
	add_child(req);
	req->request(url, _get_headers_with_directory(), HTTPClient::METHOD_POST, json_body);
	req->connect("request_completed", callable_mp((Node *)req, &Node::queue_free).unbind(4));
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
		tree->create_timer(1.0)->connect("timeout", Callable(this, "_auto_verify_game_logs"));
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
			tree->create_timer(3.0)->connect("timeout", Callable(this, "_send_debugger_errors_to_ai"));
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
		tree->create_timer(3.0)->connect("timeout", Callable(this, "_send_debugger_errors_to_ai"));
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
				tree->create_timer(5.0)->connect("timeout", Callable(this, "_send_debugger_errors_to_ai"));
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

	// If a session ID was pre-set (e.g., restored from saved state), use it directly.
	if (!initial_session_id.is_empty()) {
		session_id = initial_session_id;
		initial_session_id = "";
		connection_status = CONNECTED;
		_update_connection_indicator();
		_update_status("Connected (restored)", Color(0.5, 1, 0.5));
		session_history_button->set_text("Restored Session");
		_add_system_message("Reconnected to previous session.");
		_load_session_history();
		_fetch_providers();
		return;
	}

	// Docks that must not share sessions (e.g. Art Director) always create a fresh one.
	if (create_new_session_if_none) {
		_create_session();
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

	session_history_button->set_text(title.length() > 25 ? title.substr(0, 22) + "..." : title);

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

	// Auto-configure saved provider keys on the server
	_auto_configure_providers();

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
	history_request->connect("request_completed", Callable(this, "_on_session_history_completed"));

	String url = service_url + "/session/" + session_id + "/message?directory=" + _get_project_directory().uri_encode();
	history_request->request(url, _get_headers_with_directory());
}

void AIAssistantDock::_on_session_history_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	// Clean up the temporary HTTPRequest
	HTTPRequest *sender = Object::cast_to<HTTPRequest>(get_child(get_child_count() - 1));
	if (sender && sender != http_request && sender != session_list_http_request &&
		sender != session_history_list_http &&
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
			has_user_message = true;
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
			// Extract text and tool parts from assistant message
			for (int j = 0; j < parts.size(); j++) {
				Dictionary part = parts[j];
				String type = part.get("type", "");
				if (type == "text") {
					String text = part.get("text", "");
					if (!text.is_empty()) {
						_add_ai_message(text);
					}
				} else if (type == "tool") {
					String part_id = part.get("id", "tool_hist_" + itos(i) + "_" + itos(j));
					String tool_name = part.get("tool", "unknown");
					if (tool_name == "question") {
						continue;
					}
					Dictionary state = part.get("state", Dictionary());
					String tool_status = state.get("status", "completed");
					_add_tool_message(part_id, tool_name, tool_status, state);
				}
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
		sub->connect("id_pressed", Callable(this, "_on_submenu_model_selected"));
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
		option_btn->connect("pressed", Callable(this, "_on_question_option_pressed").bind(i));
		content->add_child(option_btn);
	}

	// Custom input option
	HBoxContainer *custom_container = memnew(HBoxContainer);
	LineEdit *custom_input = memnew(LineEdit);
	custom_input->set_placeholder("Or type a custom answer...");
	custom_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	custom_input->set_name("CustomInput");
	custom_input->connect("text_submitted", Callable(this, "_on_question_custom_submitted").unbind(1));
	custom_container->add_child(custom_input);

	Button *submit_btn = memnew(Button);
	submit_btn->set_text("Submit");
	submit_btn->connect("pressed", Callable(this, "_on_question_custom_submitted"));
	custom_container->add_child(submit_btn);

	content->add_child(custom_container);

	panel->add_child(content);
	question_container->add_child(panel);

	// Add to chat container
	chat_container->add_child(question_container);

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

// === Instance Management ===

void AIAssistantDock::set_instance_id(int p_id) {
	instance_id = p_id;
	if (instance_id > 0) {
		set_title(vformat("AI Assistant #%d", instance_id + 1));
	}
}

void AIAssistantDock::set_initial_session_id(const String &p_session_id) {
	initial_session_id = p_session_id;
}

void AIAssistantDock::_on_new_instance_pressed() {
	AIAssistantManager *manager = AIAssistantManager::get_singleton();
	if (manager && manager->can_spawn()) {
		manager->spawn_instance();
	}
}

void AIAssistantDock::_on_close_instance_pressed() {
	if (instance_id == 0) {
		return; // Cannot close primary instance.
	}
	AIAssistantManager *manager = AIAssistantManager::get_singleton();
	if (manager) {
		manager->close_instance(instance_id);
	}
}

// === Session History & Cleanup Functions ===

void AIAssistantDock::_clear_chat_ui() {
	while (chat_container->get_child_count() > 0) {
		Node *child = chat_container->get_child(0);
		chat_container->remove_child(child);
		memdelete(child);
	}
	chat_history.clear();
	_clear_tool_tracking();
	_clear_attachments();
	user_message_indices.clear();
	collapse_buttons.clear();
	collapsed_turns.clear();
	user_message_texts.clear();
	sticky_header->set_visible(false);
	sticky_current_turn_index = -1;
}

void AIAssistantDock::_fire_and_forget_delete_session(const String &p_session_id) {
	// Attach a temporary HTTPRequest to the editor main screen so it survives
	// even if this dock is destroyed before the request completes.
	Node *parent = EditorInterface::get_singleton()->get_editor_main_screen();
	if (!parent) {
		return;
	}
	HTTPRequest *req = memnew(HTTPRequest);
	parent->add_child(req);
	req->connect("request_completed", callable_mp((Node *)req, &Node::queue_free).unbind(4));
	String url = service_url + "/session/" + p_session_id;
	req->request(url, _get_headers_with_directory(), HTTPClient::METHOD_DELETE);
}

void AIAssistantDock::cleanup_before_close() {
	// Auto-delete empty sessions (no user messages) when closing the tab.
	if (!has_user_message && !session_id.is_empty()) {
		_fire_and_forget_delete_session(session_id);
	}

	// Stop all timers to prevent callbacks after destruction.
	if (logs_poll_timer) {
		logs_poll_timer->stop();
	}
	if (command_poll_timer) {
		command_poll_timer->stop();
	}
	if (question_poll_timer) {
		question_poll_timer->stop();
	}
	if (stream_poll_timer) {
		stream_poll_timer->stop();
	}
	if (processing_timer) {
		processing_timer->stop();
	}
	if (auth_poll_timer) {
		auth_poll_timer->stop();
	}
}

void AIAssistantDock::_on_session_history_pressed() {
	if (connection_status != CONNECTED) {
		return;
	}

	// Position the popup below the button.
	Vector2 btn_pos = session_history_button->get_screen_position();
	Vector2 btn_size = session_history_button->get_size();
	session_popup->set_position(Vector2i(btn_pos.x, btn_pos.y + btn_size.y));
	session_popup->popup();

	// Fetch fresh session list.
	_fetch_session_list();
}

void AIAssistantDock::_fetch_session_list() {
	// Clear and show loading state.
	while (session_popup_list->get_child_count() > 0) {
		Node *child = session_popup_list->get_child(0);
		session_popup_list->remove_child(child);
		child->queue_free();
	}

	Label *loading = memnew(Label);
	loading->set_text("Loading...");
	loading->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	session_popup_list->add_child(loading);

	String url = service_url + "/session?directory=" + _get_project_directory().uri_encode() + "&roots=true&limit=20";
	session_history_list_http->cancel_request();
	session_history_list_http->request(url, _get_headers_with_directory());
}

void AIAssistantDock::_on_session_history_list_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	// Clear loading state.
	while (session_popup_list->get_child_count() > 0) {
		Node *child = session_popup_list->get_child(0);
		session_popup_list->remove_child(child);
		child->queue_free();
	}
	cached_session_list.clear();

	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		Label *err_label = memnew(Label);
		err_label->set_text("[Failed to load sessions]");
		session_popup_list->add_child(err_label);
		return;
	}

	String response_text = String::utf8((const char *)p_body.ptr(), p_body.size());
	JSON json;
	Error err = json.parse(response_text);

	if (err != OK || json.get_data().get_type() != Variant::ARRAY) {
		Label *err_label = memnew(Label);
		err_label->set_text("[Invalid response]");
		session_popup_list->add_child(err_label);
		return;
	}

	Array sessions = json.get_data();

	// "+ New Session" button at top.
	Button *new_btn = memnew(Button);
	new_btn->set_text("+ New Session");
	new_btn->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	new_btn->connect("pressed", Callable(this, "_on_session_new_pressed"));
	session_popup_list->add_child(new_btn);

	if (!sessions.is_empty()) {
		session_popup_list->add_child(memnew(HSeparator));
	}

	// Session rows: [label] [delete button]
	for (int i = 0; i < sessions.size(); i++) {
		Dictionary session = sessions[i];
		cached_session_list.push_back(session);

		String title = session.get("title", "Untitled");
		String sid = session.get("id", "");

		if (title.length() > 35) {
			title = title.substr(0, 32) + "...";
		}

		// Format time.
		Dictionary time_dict = session.get("time", Dictionary());
		double updated_ms = time_dict.get("updated", 0.0);
		String time_str;
		if (updated_ms > 0) {
			Dictionary datetime = Time::get_singleton()->get_datetime_dict_from_unix_time((int64_t)(updated_ms / 1000.0));
			time_str = vformat("%02d/%02d %02d:%02d",
				(int)datetime["month"], (int)datetime["day"],
				(int)datetime["hour"], (int)datetime["minute"]);
		}

		HBoxContainer *row = memnew(HBoxContainer);
		session_popup_list->add_child(row);

		// Session button (click to switch).
		Button *session_btn = memnew(Button);
		String label = sid == session_id ? String(U"\u2713 ") + title : title;
		if (!time_str.is_empty()) {
			label += "  [" + time_str + "]";
		}
		session_btn->set_text(label);
		session_btn->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		session_btn->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
		session_btn->connect("pressed", Callable(this, "_on_session_item_clicked").bind(i));
		row->add_child(session_btn);

		// Rename button.
		Button *rename_btn = memnew(Button);
		rename_btn->set_text(U"\u270E");
		rename_btn->set_tooltip_text("Rename this session");
		rename_btn->set_custom_minimum_size(Size2(28, 0));
		rename_btn->connect("pressed", Callable(this, "_on_session_rename_pressed").bind(i));
		row->add_child(rename_btn);

		// Delete button.
		Button *del_btn = memnew(Button);
		del_btn->set_text("-");
		del_btn->set_tooltip_text("Delete this session");
		del_btn->set_custom_minimum_size(Size2(28, 0));
		del_btn->connect("pressed", Callable(this, "_on_session_delete_pressed").bind(i));
		row->add_child(del_btn);
	}

	if (sessions.is_empty()) {
		Label *empty_label = memnew(Label);
		empty_label->set_text("[No sessions found]");
		empty_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		session_popup_list->add_child(empty_label);
	}
}

void AIAssistantDock::_on_session_new_pressed() {
	session_popup->hide();

	if (!has_user_message && !session_id.is_empty()) {
		_fire_and_forget_delete_session(session_id);
	}

	_clear_chat_ui();
	_add_system_message("Creating new session...");

	session_id = "";
	has_user_message = false;
	coding_standards_injected = false;
	_create_session();
}

void AIAssistantDock::_on_session_item_clicked(int p_index) {
	session_popup->hide();

	if (p_index < 0 || p_index >= cached_session_list.size()) {
		return;
	}

	Dictionary session = cached_session_list[p_index];
	String sid = session.get("id", "");
	String title = session.get("title", "Untitled");

	if (sid == session_id) {
		return; // Already on this session.
	}

	_switch_to_session(sid, title);
}

void AIAssistantDock::_switch_to_session(const String &p_session_id, const String &p_title) {
	// If current session is empty (no user messages), auto-delete it.
	if (!has_user_message && !session_id.is_empty()) {
		_fire_and_forget_delete_session(session_id);
	}

	_clear_chat_ui();

	// Switch to new session.
	session_id = p_session_id;
	has_user_message = false;
	coding_standards_injected = false;

	// Re-enable input (may have been disabled in "no session" state).
	prompt_input->set_editable(true);
	prompt_input->set_placeholder(TTR("Type a message..."));
	send_button->set_disabled(false);

	String display_title = p_title.length() > 25 ? p_title.substr(0, 22) + "..." : p_title;
	session_history_button->set_text(display_title);

	_add_system_message("Switched to session: " + p_title);
	_add_system_message("Session ID: " + session_id.substr(0, 8) + "...");

	// Load chat history from the new session.
	_load_session_history();
}

void AIAssistantDock::_on_session_delete_pressed(int p_index) {
	if (p_index < 0 || p_index >= cached_session_list.size()) {
		return;
	}

	session_popup->hide();

	// Store which session is pending deletion.
	session_pending_delete_index = p_index;

	Dictionary session = cached_session_list[p_index];
	String title = session.get("title", "Untitled");
	session_delete_confirm->set_text(TTR("Delete session \"") + title + "\"?");
	session_delete_confirm->popup_centered();
}

void AIAssistantDock::_on_session_delete_confirmed() {
	int p_index = session_pending_delete_index;
	if (p_index < 0 || p_index >= cached_session_list.size()) {
		return;
	}

	Dictionary session = cached_session_list[p_index];
	String sid = session.get("id", "");
	String title = session.get("title", "Untitled");

	if (sid.is_empty()) {
		return;
	}

	// Fire-and-forget DELETE.
	_fire_and_forget_delete_session(sid);

	// If the deleted session is the current one, clear UI and enter "no session" state.
	if (sid == session_id) {
		_clear_chat_ui();

		session_id = "";
		has_user_message = false;
		coding_standards_injected = false;

		session_history_button->set_text("No Session");
		prompt_input->set_editable(false);
		prompt_input->set_placeholder(TTR("Select or create a session to start chatting..."));
		send_button->set_disabled(true);
	}

	_add_system_message("Session \"" + title + "\" deleted.");

	// Remove from cached list.
	cached_session_list.remove_at(p_index);
	session_pending_delete_index = -1;
}

void AIAssistantDock::_on_session_rename_pressed(int p_index) {
	if (p_index < 0 || p_index >= cached_session_list.size()) {
		return;
	}

	session_popup->hide();

	session_pending_rename_index = p_index;

	Dictionary session = cached_session_list[p_index];
	String title = session.get("title", "Untitled");
	session_rename_input->set_text(title);
	session_rename_input->select_all();
	session_rename_dialog->popup_centered();
	session_rename_input->grab_focus();
}

void AIAssistantDock::_on_session_rename_confirmed() {
	int p_index = session_pending_rename_index;
	if (p_index < 0 || p_index >= cached_session_list.size()) {
		return;
	}

	String new_title = session_rename_input->get_text().strip_edges();
	if (new_title.is_empty()) {
		return;
	}

	Dictionary session = cached_session_list[p_index];
	String sid = session.get("id", "");
	if (sid.is_empty()) {
		return;
	}

	// Send PATCH request to rename session.
	String url = service_url + "/session/" + sid;
	Vector<String> headers = _get_headers_with_directory();

	Dictionary body;
	body["title"] = new_title;
	String body_str = JSON::stringify(body);

	session_rename_http->request(url, headers, HTTPClient::METHOD_PATCH, body_str);

	// Optimistically update cached data and UI.
	session["title"] = new_title;
	cached_session_list.set(p_index, session);

	if (sid == session_id) {
		String display = new_title.length() > 25 ? new_title.substr(0, 22) + "..." : new_title;
		session_history_button->set_text(display);
	}

	_add_system_message("Session renamed to \"" + new_title + "\".");
	session_pending_rename_index = -1;
}

void AIAssistantDock::_on_session_rename_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		_add_system_message("Failed to rename session (HTTP " + String::num_int64(p_code) + ").");
	}
}
