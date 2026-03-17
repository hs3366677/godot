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
#include "editor/editor_data.h"
#include "editor/editor_interface.h"
#include "editor/editor_log.h"
#include "editor/editor_node.h"
#include "editor/settings/editor_settings.h"
#include "editor/docks/filesystem_dock.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/run/editor_run_bar.h"
#include "editor/gui/editor_file_dialog.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/progress_bar.h"
#include "scene/gui/separator.h"
#include "scene/resources/style_box_flat.h"
#include "scene/resources/image_texture.h"
#include "servers/display/display_server.h"

// Convert HTTP response body to String, stripping any NUL bytes to avoid
// Godot's "Unexpected NUL character" error spam from String::utf8().
static String _body_to_string(const PackedByteArray &p_body) {
	if (p_body.is_empty()) {
		return String();
	}
	const uint8_t *src = p_body.ptr();
	bool has_nul = false;
	for (int i = 0; i < p_body.size(); i++) {
		if (src[i] == 0) {
			has_nul = true;
			break;
		}
	}
	if (!has_nul) {
		return String::utf8((const char *)p_body.ptr(), p_body.size());
	}
	PackedByteArray clean;
	clean.resize(p_body.size());
	uint8_t *dst = clean.ptrw();
	int out = 0;
	for (int i = 0; i < p_body.size(); i++) {
		if (src[i] != 0) {
			dst[out++] = src[i];
		}
	}
	clean.resize(out);
	return String::utf8((const char *)clean.ptr(), clean.size());
}

void AIAssistantDock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_service_url", "url"), &AIAssistantDock::set_service_url);
	ClassDB::bind_method(D_METHOD("get_service_url"), &AIAssistantDock::get_service_url);
	ClassDB::bind_method(D_METHOD("is_connected_to_service"), &AIAssistantDock::is_connected_to_service);
	ClassDB::bind_method(D_METHOD("_check_service_health_deferred"), &AIAssistantDock::_check_service_health_deferred);
	ClassDB::bind_method(D_METHOD("_on_reconnect_timeout"), &AIAssistantDock::_on_reconnect_timeout);

	// UI handlers
	ClassDB::bind_method(D_METHOD("_on_send_pressed"), &AIAssistantDock::_on_send_pressed);
	ClassDB::bind_method(D_METHOD("_on_stop_pressed"), &AIAssistantDock::_on_stop_pressed);
	ClassDB::bind_method(D_METHOD("_on_send_or_stop_pressed"), &AIAssistantDock::_on_send_or_stop_pressed);
	ClassDB::bind_method(D_METHOD("_on_setup_pressed"), &AIAssistantDock::_on_setup_pressed);
	ClassDB::bind_method(D_METHOD("_on_wizard_back"), &AIAssistantDock::_on_wizard_back);
	ClassDB::bind_method(D_METHOD("_on_wizard_next"), &AIAssistantDock::_on_wizard_next);
	ClassDB::bind_method(D_METHOD("_on_wizard_skip"), &AIAssistantDock::_on_wizard_skip);
	ClassDB::bind_method(D_METHOD("_on_wizard_api_key_connect", "provider_id"), &AIAssistantDock::_on_wizard_api_key_connect);
	ClassDB::bind_method(D_METHOD("_on_wizard_api_key_submit", "provider_id"), &AIAssistantDock::_on_wizard_api_key_submit);
	ClassDB::bind_method(D_METHOD("_on_wizard_api_key_completed", "result", "code", "headers", "body", "provider_id"), &AIAssistantDock::_on_wizard_api_key_completed);
	ClassDB::bind_method(D_METHOD("_on_wizard_local_health_completed", "result", "code", "headers", "body", "provider_id"), &AIAssistantDock::_on_wizard_local_health_completed);
	ClassDB::bind_method(D_METHOD("_on_wizard_fetch_image_model_completed", "result", "code", "headers", "body"), &AIAssistantDock::_on_wizard_fetch_image_model_completed);
	ClassDB::bind_method(D_METHOD("_on_wizard_image_model_selected", "index"), &AIAssistantDock::_on_wizard_image_model_selected);
	ClassDB::bind_method(D_METHOD("_on_wizard_image_model_saved", "result", "code", "headers", "body"), &AIAssistantDock::_on_wizard_image_model_saved);
	ClassDB::bind_method(D_METHOD("_on_wizard_fetch_rembg_method_completed", "result", "code", "headers", "body"), &AIAssistantDock::_on_wizard_fetch_rembg_method_completed);
	ClassDB::bind_method(D_METHOD("_on_wizard_rembg_method_selected", "index"), &AIAssistantDock::_on_wizard_rembg_method_selected);
	ClassDB::bind_method(D_METHOD("_on_wizard_rembg_method_saved", "result", "code", "headers", "body"), &AIAssistantDock::_on_wizard_rembg_method_saved);
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
	ClassDB::bind_method(D_METHOD("_on_snip_pressed"), &AIAssistantDock::_on_snip_pressed);
	ClassDB::bind_method(D_METHOD("_on_snip_captured", "image", "rect"), &AIAssistantDock::_on_snip_captured);
	ClassDB::bind_method(D_METHOD("_on_snip_annotated", "image", "text_labels"), &AIAssistantDock::_on_snip_annotated);
	ClassDB::bind_method(D_METHOD("_on_snip_cancelled"), &AIAssistantDock::_on_snip_cancelled);
	ClassDB::bind_method(D_METHOD("_toggle_gif_recording"), &AIAssistantDock::_toggle_gif_recording);
	ClassDB::bind_method(D_METHOD("_on_gif_frame_timer"), &AIAssistantDock::_on_gif_frame_timer);
	ClassDB::bind_method(D_METHOD("_on_gif_post_completed", "result", "code", "headers", "body"), &AIAssistantDock::_on_gif_post_completed);
	ClassDB::bind_method(D_METHOD("_on_attachment_gui_input", "event", "file_path"), &AIAssistantDock::_on_attachment_gui_input);
	ClassDB::bind_method(D_METHOD("_on_chat_thumbnail_gui_input", "event", "cache_path", "title"), &AIAssistantDock::_on_chat_thumbnail_gui_input);
	ClassDB::bind_method(D_METHOD("_on_image_files_selected", "paths"), &AIAssistantDock::_on_image_files_selected);
	ClassDB::bind_method(D_METHOD("_on_files_dropped_on_dock", "files"), &AIAssistantDock::_on_files_dropped_on_dock);
	ClassDB::bind_method(D_METHOD("_on_file_removed", "file"), &AIAssistantDock::_on_file_removed);
	ClassDB::bind_method(D_METHOD("_on_editor_selection_changed"), &AIAssistantDock::_on_editor_selection_changed);
	ClassDB::bind_method(D_METHOD("_on_filesystem_selection_changed"), &AIAssistantDock::_on_filesystem_selection_changed);
	ClassDB::bind_method(D_METHOD("_toggle_reasoning_collapse", "key"), &AIAssistantDock::_toggle_reasoning_collapse);

	// HTTP request callbacks
	ClassDB::bind_method(D_METHOD("_on_http_request_completed", "result", "code", "headers", "body"), &AIAssistantDock::_on_http_request_completed);
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

	// Questions
	ClassDB::bind_method(D_METHOD("_on_question_option_pressed", "option_index"), &AIAssistantDock::_on_question_option_pressed);
	ClassDB::bind_method(D_METHOD("_on_question_custom_submitted"), &AIAssistantDock::_on_question_custom_submitted);
}

AIAssistantDock *AIAssistantDock::singleton = nullptr;

AIAssistantDock::AIAssistantDock() {
	singleton = this;
	set_title(TTR("AI"));
	set_icon_name(SNAME("Node"));
	set_default_slot(DOCK_SLOT_RIGHT_UL);

	// Prevent the dock from requesting excessive minimum height that would
	// force the editor window to grow beyond the screen.
	set_clip_contents(true);

	_setup_ui();
	_connect_signals();
}

AIAssistantDock::~AIAssistantDock() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

Size2 AIAssistantDock::get_minimum_size() const {
	// Cap the dock's minimum height so it never forces the editor window
	// to grow beyond the screen. The chat scroll container handles overflow.
	Size2 ms = MarginContainer::get_minimum_size();
	if (ms.height > 200) {
		ms.height = 200;
	}
	return ms;
}

void AIAssistantDock::_setup_ui() {
	// Main container for all UI — use size flags (not anchors) so the
	// MarginContainer correctly participates in minimum-size negotiation
	// and the dock never forces the editor window to grow beyond the screen.
	main_container = memnew(VBoxContainer);
	main_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
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

	// Thinking mode toggle
	thinking_toggle = memnew(CheckButton);
	thinking_toggle->set_text("Think");
	thinking_toggle->set_tooltip_text(TTR("Enable extended thinking mode (Claude/Gemini models)"));
	thinking_toggle->set_pressed(EditorSettings::get_singleton()->get_setting("ai/assistant/thinking_enabled").booleanize());
	thinking_toggle->connect("toggled", callable_mp(this, &AIAssistantDock::_on_thinking_toggled));
	toolbar_container->add_child(thinking_toggle);

	// Auto-test toggle
	autotest_toggle = memnew(CheckButton);
	autotest_toggle->set_text("Auto-Test");
	autotest_toggle->set_tooltip_text(TTR("Auto-run playtest after task completion to verify changes"));
	{
		Variant v = EditorSettings::get_singleton()->get_setting("ai/assistant/autotest_enabled");
		autotest_toggle->set_pressed(v.get_type() == Variant::NIL ? true : v.booleanize());
	}
	autotest_toggle->connect("toggled", callable_mp(this, &AIAssistantDock::_on_autotest_toggled));
	toolbar_container->add_child(autotest_toggle);

	toolbar_container->add_spacer();

	setup_button = memnew(Button);
	setup_button->set_tooltip_text("Setup AI providers - connect your API keys or use free models");
	setup_button->set_flat(true);
	toolbar_container->add_child(setup_button);

	settings_button = memnew(Button);
	settings_button->set_tooltip_text("Configure AI asset generation providers");
	settings_button->set_flat(true);
	toolbar_container->add_child(settings_button);

	// New instance button ("+")
	new_instance_button = memnew(Button);
	new_instance_button->set_text("+");
	new_instance_button->set_tooltip_text(TTR("Open new AI tab"));
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

	// Loading overlay — shown while connecting, hidden once session is ready
	loading_overlay = memnew(VBoxContainer);
	loading_overlay->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	loading_overlay->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	loading_overlay->set_alignment(BoxContainer::ALIGNMENT_CENTER);

	RichTextLabel *loading_label = memnew(RichTextLabel);
	loading_label->set_use_bbcode(true);
	loading_label->set_fit_content(true);
	loading_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	loading_label->set_text("[center][color=yellow]Connecting to AI service...[/color][/center]");
	loading_label->set_name("LoadingLabel");
	loading_overlay->add_child(loading_label);

	// Animated dots progress bar
	ProgressBar *loading_bar = memnew(ProgressBar);
	loading_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	loading_bar->set_custom_minimum_size(Size2(0, 4));
	loading_bar->set_min(0);
	loading_bar->set_max(100);
	loading_bar->set_value(0);
	loading_bar->set_show_percentage(false);
	loading_bar->set_name("LoadingBar");
	loading_bar->set_indeterminate(true);
	loading_overlay->add_child(loading_bar);

	chat_container->add_child(loading_overlay);

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

	send_button = memnew(Button);
	send_button->set_text("Send (Enter)");
	send_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	button_container->add_child(send_button);

	screenshot_button = memnew(Button);
	screenshot_button->set_text(U"\U0001F4F7");
	screenshot_button->set_tooltip_text(TTR("Capture game screenshot and send for AI analysis (game must be running)"));
	screenshot_button->set_shortcut(ED_SHORTCUT("ai_assistant/screenshot", TTR("Screenshot"), KeyModifierMask::ALT | Key::P));
	screenshot_button->set_shortcut_in_tooltip(true);
	button_container->add_child(screenshot_button);

	snip_button = memnew(Button);
	snip_button->set_text(U"\u2702"); // ✂
	snip_button->set_tooltip_text(TTR("Snip a region and annotate for AI analysis."));
	snip_button->set_shortcut(ED_SHORTCUT("ai_assistant/snip_screen", TTR("Snip Screen"), KeyModifierMask::ALT | Key::S));
	snip_button->set_shortcut_in_tooltip(true);
	button_container->add_child(snip_button);

	// GIF recording toggle button
	gif_record_button = memnew(Button);
	gif_record_button->set_text(U"\U0001F3AC"); // 🎬
	gif_record_button->set_toggle_mode(true);
	gif_record_button->set_tooltip_text(TTR("Captures frames and creates an animated GIF for AI analysis."));
	gif_record_button->set_shortcut(ED_SHORTCUT("ai_assistant/toggle_gif_recording", TTR("Toggle GIF Recording"), KeyModifierMask::ALT | Key::G));
	gif_record_button->set_shortcut_in_tooltip(true);
	button_container->add_child(gif_record_button);

	// GIF frame capture timer
	gif_frame_timer = memnew(Timer);
	gif_frame_timer->set_wait_time(1.0 / gif_record_fps); // 10 FPS
	gif_frame_timer->set_one_shot(false);
	add_child(gif_frame_timer);

	attach_image_button = memnew(Button);
	attach_image_button->set_text("Img+");
	attach_image_button->set_tooltip_text("Attach image(s) - PNG, JPG, WebP, GIF (max 10 MB each)");
	button_container->add_child(attach_image_button);

	// Context indicator (shows selected asset/node below input)
	context_label = memnew(Label);
	context_label->set_text("");
	context_label->add_theme_font_size_override("font_size", 11);
	context_label->add_theme_color_override("font_color", Color(0.6, 0.6, 0.6));
	context_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	context_label->set_visible(false);
	input_container->add_child(context_label);

	// Disable input until connected.
	prompt_input->set_editable(false);
	prompt_input->set_placeholder(TTR("Connecting to AI service..."));
	send_button->set_disabled(true);

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
	stream_poll_timer->set_wait_time(0.1); // Poll every 100ms for responsive updates
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
	command_poll_timer->set_wait_time(0.1); // Poll every 100ms for commands
	command_poll_timer->set_autostart(false);
	add_child(command_poll_timer);

	// HTTP Request node for question polling (AI asking user questions)
	question_http_request = memnew(HTTPRequest);
	add_child(question_http_request);

	// Timer for question polling
	question_poll_timer = memnew(Timer);
	question_poll_timer->set_wait_time(0.1); // Poll every 100ms for questions
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

	// Timer for reconnecting when initial health check fails
	reconnect_timer = memnew(Timer);
	reconnect_timer->set_wait_time(3.0);
	reconnect_timer->set_autostart(false);
	add_child(reconnect_timer);

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

	// Settings dialog (Prompt only)
	settings_dialog = memnew(AcceptDialog);
	settings_dialog->set_title("Settings");
	settings_dialog->set_ok_button_text("Save");
	settings_dialog->set_min_size(Size2(600, 400));
	settings_dialog->set_max_size(Size2(800, 600));

	settings_prompt_page = memnew(VBoxContainer);
	settings_prompt_page->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	settings_prompt_page->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	// Project Prompt section header
	Label *project_section = memnew(Label);
	project_section->set_text("Project Prompt");
	project_section->add_theme_font_size_override("font_size", 14);
	settings_prompt_page->add_child(project_section);

	project_prompt_path_label = memnew(Label);
	project_prompt_path_label->set_text("Path: (loading...)");
	project_prompt_path_label->add_theme_font_size_override("font_size", 11);
	project_prompt_path_label->add_theme_color_override("font_color", Color(0.6, 0.6, 0.6));
	settings_prompt_page->add_child(project_prompt_path_label);

	project_prompt_edit = memnew(TextEdit);
	project_prompt_edit->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	project_prompt_edit->set_custom_minimum_size(Size2(0, 200));
	project_prompt_edit->set_placeholder("Write project-specific AI instructions here...\nThis file is read by the AI as context for every conversation.");
	project_prompt_edit->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	settings_prompt_page->add_child(project_prompt_edit);

	HBoxContainer *prompt_buttons = memnew(HBoxContainer);
	generate_prompt_button = memnew(Button);
	generate_prompt_button->set_text("Generate Template");
	generate_prompt_button->set_tooltip_text("Scan project and generate an initial prompt template");
	prompt_buttons->add_child(generate_prompt_button);
	settings_prompt_page->add_child(prompt_buttons);

	// Engine Prompts section (read-only)
	settings_prompt_page->add_child(memnew(HSeparator));

	Label *engine_section = memnew(Label);
	engine_section->set_text("Engine Prompts (Read-Only)");
	engine_section->add_theme_font_size_override("font_size", 14);
	settings_prompt_page->add_child(engine_section);

	engine_prompt_tree = memnew(Tree);
	engine_prompt_tree->set_custom_minimum_size(Size2(0, 100));
	engine_prompt_tree->set_hide_root(true);
	engine_prompt_tree->set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
	settings_prompt_page->add_child(engine_prompt_tree);

	engine_prompt_preview = memnew(RichTextLabel);
	engine_prompt_preview->set_custom_minimum_size(Size2(0, 100));
	engine_prompt_preview->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	engine_prompt_preview->set_selection_enabled(true);
	engine_prompt_preview->set_use_bbcode(false);
	engine_prompt_preview->set_text("Select an engine prompt file above to preview its contents.");
	settings_prompt_page->add_child(engine_prompt_preview);

	settings_dialog->add_child(settings_prompt_page);
	add_child(settings_dialog);

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

	// Image preview dialog (for viewing full-size images from chat thumbnails)
	image_preview_dialog = memnew(AcceptDialog);
	image_preview_dialog->set_title("Image Preview");
	image_preview_dialog->set_min_size(Size2(512, 512));
	image_preview_rect = memnew(TextureRect);
	image_preview_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
	image_preview_rect->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	image_preview_rect->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	image_preview_rect->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	image_preview_dialog->add_child(image_preview_rect);
	add_child(image_preview_dialog);

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
	send_button->connect("pressed", Callable(this, "_on_send_or_stop_pressed"));
	setup_button->connect("pressed", Callable(this, "_on_setup_pressed"));
	settings_button->connect("pressed", Callable(this, "_on_settings_pressed"));
	new_instance_button->connect("pressed", Callable(this, "_on_new_instance_pressed"));
	prompt_input->connect("gui_input", Callable(this, "_on_prompt_input_gui_input"));
	prompt_input->connect("text_changed", Callable(this, "_on_prompt_text_changed"));
	http_request->connect("request_completed", Callable(this, "_on_http_request_completed"));

	// Image attachment signals
	attach_image_button->connect("pressed", Callable(this, "_on_attach_image_pressed"));
	image_file_dialog->connect("files_selected", Callable(this, "_on_image_files_selected"));
	screenshot_button->connect("pressed", Callable(this, "_on_screenshot_pressed"));
	snip_button->connect("pressed", Callable(this, "_on_snip_pressed"));

	// GIF recording button + timer
	gif_record_button->connect("pressed", Callable(this, "_toggle_gif_recording"));
	gif_frame_timer->connect("timeout", Callable(this, "_on_gif_frame_timer"));

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

	// Reconnect timer
	reconnect_timer->connect("timeout", Callable(this, "_on_reconnect_timeout"));

	// Session list signals (for finding existing sessions)
	session_list_http_request->connect("request_completed", Callable(this, "_on_session_list_completed"));

	// Session history panel signals
	session_history_list_http->connect("request_completed", Callable(this, "_on_session_history_list_completed"));
	session_history_button->connect("pressed", Callable(this, "_on_session_history_pressed"));
	session_delete_confirm->connect("confirmed", Callable(this, "_on_session_delete_confirmed"));
	session_rename_dialog->connect("confirmed", Callable(this, "_on_session_rename_confirmed"));
	session_rename_http->connect("request_completed", Callable(this, "_on_session_rename_completed"));

	// Game stop signal (reset state when game stops)
	EditorRunBar *run_bar = EditorRunBar::get_singleton();
	if (run_bar) {
		run_bar->connect("stop_pressed", Callable(this, "_on_game_stopped"));
	}

	// Enable shortcut input so F1 works even when dock doesn't have focus
	set_process_shortcut_input(true);
}

void AIAssistantDock::shortcut_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_pressed() && !k->is_echo() && k->is_alt_pressed() && k->get_keycode() == Key::G) {
		_toggle_gif_recording();
		get_viewport()->set_input_as_handled();
	}
}

void AIAssistantDock::toggle_gif_recording() {
	_toggle_gif_recording();
}

void AIAssistantDock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			// Set settings button icon (theme available now)
			setup_button->set_button_icon(get_editor_theme_icon(SNAME("Key")));
			settings_button->set_button_icon(get_editor_theme_icon(SNAME("GearSettings")));

			// Auto-connect to AI service on startup
			call_deferred("_check_service_health_deferred");
			// Enable process to poll for debugger errors
			set_process(true);
			// Connect to viewport files_dropped for external drag-and-drop
			get_tree()->get_root()->connect("files_dropped",
					Callable(this, "_on_files_dropped_on_dock"));
			// Listen for file deletions to clean up AI metadata
			FileSystemDock *fs_dock = EditorInterface::get_singleton()->get_file_system_dock();
			if (fs_dock) {
				fs_dock->connect("file_removed", Callable(this, "_on_file_removed"));
				fs_dock->connect("selection_changed", Callable(this, "_on_filesystem_selection_changed"));
			}
			// Listen for node selection changes in the scene tree
			EditorSelection *editor_sel = EditorInterface::get_singleton()->get_selection();
			if (editor_sel) {
				editor_sel->connect("selection_changed", Callable(this, "_on_editor_selection_changed"));
			}
		} break;

		case NOTIFICATION_READY: {
		} break;

		case NOTIFICATION_PROCESS: {
			// Force scroll to bottom for N frames after history load.
			if (scroll_to_bottom_frames > 0) {
				if (user_scrolled_up) {
					// User scrolled away — cancel forced scrolling immediately.
					scroll_to_bottom_frames = 0;
					suppress_scroll_tracking = false;
				} else {
					scroll_to_bottom_frames--;
					if (chat_scroll) {
						suppress_scroll_tracking = true; // One-shot: consumed by _on_chat_scroll_changed
						chat_scroll->set_v_scroll(chat_scroll->get_v_scroll_bar()->get_max());
					}
				}
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
			// Disconnect file_removed signal
			FileSystemDock *fsd = EditorInterface::get_singleton()->get_file_system_dock();
			if (fsd) {
				if (fsd->is_connected("file_removed", Callable(this, "_on_file_removed"))) {
					fsd->disconnect("file_removed", Callable(this, "_on_file_removed"));
				}
				if (fsd->is_connected("selection_changed", Callable(this, "_on_filesystem_selection_changed"))) {
					fsd->disconnect("selection_changed", Callable(this, "_on_filesystem_selection_changed"));
				}
			}
			// Disconnect editor selection
			EditorSelection *editor_sel = EditorInterface::get_singleton()->get_selection();
			if (editor_sel && editor_sel->is_connected("selection_changed", Callable(this, "_on_editor_selection_changed"))) {
				editor_sel->disconnect("selection_changed", Callable(this, "_on_editor_selection_changed"));
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

String AIAssistantDock::_load_engine_prompts() {
	// Return cached if already loaded.
	if (!_cached_engine_prompts.is_empty()) {
		return _cached_engine_prompts;
	}

	// Locate the prompts/ directory relative to the engine executable.
	// Executable is at: <engine_root>/godot/bin/godot.exe
	// Prompts are at:   <engine_root>/prompts/
	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	String docs_dir = exe_dir.path_join("..").path_join("..").path_join("prompts").simplify_path();

	Ref<DirAccess> dir = DirAccess::open(docs_dir);
	if (dir.is_null()) {
		print_line("[AIAssistant] Could not open prompts directory: " + docs_dir);
		return "";
	}

	// Collect top-level .md files only.
	Vector<String> md_files;
	dir->list_dir_begin();
	String entry = dir->get_next();
	while (!entry.is_empty()) {
		if (!dir->current_is_dir() && entry.ends_with(".md")) {
			md_files.push_back(docs_dir.path_join(entry));
		}
		entry = dir->get_next();
	}
	dir->list_dir_end();

	// Sort for deterministic order.
	md_files.sort();

	// Read and combine all files.
	String combined = "[IMPORTANT RULES]\n"
			"1. Always reply in the SAME LANGUAGE as the user's message. If the user writes in Chinese, reply in Chinese. If in English, reply in English.\n"
			"2. Follow the engine prompts below when generating or modifying game code and assets.\n\n";

	for (int i = 0; i < md_files.size(); i++) {
		Ref<FileAccess> f = FileAccess::open(md_files[i], FileAccess::READ);
		if (f.is_valid()) {
			String relative = md_files[i].replace(docs_dir + "/", "");
			combined += "=== " + relative + " ===\n";
			combined += f->get_as_text();
			combined += "\n\n";
			print_line("[AIAssistant] Loaded engine prompt: " + relative);
		}
	}

	if (md_files.size() == 0) {
		print_line("[AIAssistant] No .md files found in: " + docs_dir);
		return "";
	}

	print_line("[AIAssistant] Loaded " + itos(md_files.size()) + " engine prompt files from " + docs_dir);
	_cached_engine_prompts = combined;
	return _cached_engine_prompts;
}

Vector<String> AIAssistantDock::_get_headers_with_directory() const {
	Vector<String> headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back("Accept: application/json");
	headers.push_back("x-opencode-directory: " + _get_project_directory());
	return headers;
}

void AIAssistantDock::_check_service_health() {
	if (connection_status == CONNECTED && !session_id.is_empty()) {
		print_line("[AIAssistant] Health check skipped: already connected with session " + session_id.substr(0, 8));
		return;
	}
	if (pending_request != REQUEST_NONE) {
		print_line("[AIAssistant] Health check skipped: pending_request=" + String::num_int64(pending_request));
		return;
	}

	pending_request = REQUEST_HEALTH;
	connection_status = CONNECTING;
	_update_status("Checking service...", Color(1, 1, 0));
	_update_connection_indicator();
	_update_loading_overlay("Connecting to AI service...");

	String url = service_url + "/global/health";
	print_line("[AIAssistant] Health check → " + url);
	Vector<String> headers = _get_headers_with_directory();
	Error err = http_request->request(url, headers);
	if (err != OK) {
		print_line("[AIAssistant] Health check request failed immediately: error=" + String::num_int64(err));
		pending_request = REQUEST_NONE;
		_schedule_reconnect();
	}
}

void AIAssistantDock::_schedule_reconnect() {
	reconnect_attempts++;
	if (reconnect_attempts > MAX_RECONNECT_ATTEMPTS) {
		connection_status = CONNECTION_ERROR;
		_update_status("Connection failed", Color(1, 0, 0));
		_update_connection_indicator();
		_update_loading_overlay("Could not connect to AI service.\nMake sure it's running on " + service_url);
		_add_system_message("Could not connect to AI service after " + itos(MAX_RECONNECT_ATTEMPTS) + " attempts.\nMake sure the AI server is running on " + service_url);
		return;
	}
	connection_status = CONNECTING;
	_update_status("Reconnecting (" + itos(reconnect_attempts) + "/" + itos(MAX_RECONNECT_ATTEMPTS) + ")...", Color(1, 1, 0));
	_update_connection_indicator();
	_update_loading_overlay("Connecting to AI service...");
	print_line("[AIAssistant] Scheduling reconnect attempt " + itos(reconnect_attempts) + "/" + itos(MAX_RECONNECT_ATTEMPTS) + " in 3s");
	reconnect_timer->start();
}

void AIAssistantDock::_on_reconnect_timeout() {
	reconnect_timer->stop();
	_check_service_health();
}

void AIAssistantDock::_create_session() {
	if (pending_request != REQUEST_NONE) {
		print_line("[AIAssistant] Create session skipped: pending_request=" + String::num_int64(pending_request));
		return;
	}

	// New session → no stale messages to ignore
	ignore_assistant_message_id = "";

	pending_request = REQUEST_SESSION;
	_update_status("Creating session...", Color(1, 1, 0));
	_update_loading_overlay("Creating session...");

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

	// Inject engine prompts + base rules on first message of each session.
	// Sent as its own part so it's hidden from the chat UI (filtered in _add_user_message).
	if (!engine_prompts_injected) {
		engine_prompts_injected = true;
		String standards = _load_engine_prompts();
		if (standards.is_empty()) {
			// Even without engine prompts files, inject base rules.
			standards = "[IMPORTANT RULES]\n"
					"1. Always reply in the SAME LANGUAGE as the user's message. If the user writes in Chinese, reply in Chinese. If in English, reply in English.\n";
		}
		Dictionary standards_part;
		standards_part["type"] = "text";
		standards_part["text"] = standards;
		parts.push_back(standards_part);
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

	// Inject currently selected file(s) from the FileSystem dock.
	// This lets the AI know which asset/file the user is looking at.
	{
		FileSystemDock *fs_dock = FileSystemDock::get_singleton();
		if (fs_dock) {
			Vector<String> selected = fs_dock->get_selected_paths();
			if (!selected.is_empty()) {
				String sel_ctx = "[SELECTED FILE(S) in FileSystem dock]\n";
				for (const String &p : selected) {
					sel_ctx += "- " + p + "\n";
				}
				Dictionary sel_part;
				sel_part["type"] = "text";
				sel_part["text"] = sel_ctx;
				parts.push_back(sel_part);
			}
		}
	}

	// Inject currently selected node(s) from the Scene tree.
	{
		EditorSelection *editor_sel = EditorInterface::get_singleton()->get_selection();
		if (editor_sel) {
			List<Node *> nodes = editor_sel->get_full_selected_node_list();
			if (!nodes.is_empty()) {
				String node_ctx = "[SELECTED NODE(S) in Scene tree]\n";
				for (Node *n : nodes) {
					String node_path = String(n->get_path());
					String node_class = n->get_class();
					node_ctx += "- " + node_path + " (" + node_class + ")\n";
				}
				Dictionary node_part;
				node_part["type"] = "text";
				node_part["text"] = node_ctx;
				parts.push_back(node_part);
			}
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

	// Include thinking variant when toggle is on
	if (thinking_toggle->is_pressed()) {
		body["variant"] = "high";
	}

	// Include auto-test flag (OpenCode injects the instruction into system prompt)
	if (autotest_toggle->is_pressed()) {
		body["autotest"] = true;
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
		// For health check failures, schedule a retry instead of giving up.
		if (request_type == REQUEST_HEALTH) {
			_schedule_reconnect();
			return;
		}
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

	String response_text = _body_to_string(p_body);
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
				// Successfully connected — stop reconnect timer and reset attempts.
				reconnect_timer->stop();
				reconnect_attempts = 0;
				_add_system_message("AI service is running. Looking for existing session...");
				_find_existing_session();
			} else {
				_schedule_reconnect();
			}
		} break;

		case REQUEST_SESSION: {
			if (p_code == 200 && response_data.has("id")) {
				session_id = response_data["id"];
				engine_prompts_injected = false;
				has_user_message = false;
				connection_status = CONNECTED;
				_update_status("Connected", Color(0, 1, 0));
				_update_connection_indicator();

				session_history_button->set_text("New Session");
				// Re-enable input (was disabled during connecting).
				prompt_input->set_editable(true);
				prompt_input->set_placeholder(TTR("Type a message..."));
				send_button->set_disabled(false);

				// Clear "Connecting..." text and show welcome for new session.
				print_line("[AIAssistant] CLEAR_SITE_A: new session created");
				_clear_chat_ui();
				_show_welcome_message();

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

		case REQUEST_DELETE_SESSION: {
			// Session deleted on server, now reset and create new one
			session_id = "";
			has_user_message = false;
			engine_prompts_injected = false;
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

void AIAssistantDock::_on_thinking_toggled(bool p_pressed) {
	EditorSettings::get_singleton()->set_setting("ai/assistant/thinking_enabled", p_pressed);
}

void AIAssistantDock::_on_autotest_toggled(bool p_pressed) {
	EditorSettings::get_singleton()->set_setting("ai/assistant/autotest_enabled", p_pressed);
}

void AIAssistantDock::_on_send_pressed() {
	String prompt = prompt_input->get_text().strip_edges();
	if (prompt.is_empty() && pending_attachments.is_empty()) {
		return;
	}

	if (prompt.is_empty()) {
		prompt = "[Image attachment(s)]";
	}

	user_scrolled_up = false; // Resume auto-scroll when user sends a message
	suppress_scroll_tracking = true; // Prevent layout changes from setting user_scrolled_up
	_add_user_message(prompt);
	scroll_to_bottom_frames = 10; // Brute-force scroll for a few frames
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

	// Remember the current assistant message so the next poll cycle skips it.
	// Without this, the stale aborted message (which has time.completed set)
	// would trick the poller into thinking the NEW response is already done.
	ignore_assistant_message_id = current_message_id;

	// Hide processing and stop polling
	_hide_processing();
	pending_request = REQUEST_NONE;
	_update_status("Connected", Color(0, 1, 0));
	_add_system_message("AI processing stopped.");
}

void AIAssistantDock::_on_send_or_stop_pressed() {
	if (is_processing) {
		_on_stop_pressed();
	} else {
		_on_send_pressed();
	}
}

void AIAssistantDock::_on_clear_pressed() {
	// If the current session has no user messages, delete it (no value in keeping it).
	// Otherwise, leave it in session history for later access.
	if (!has_user_message && !session_id.is_empty()) {
		_fire_and_forget_delete_session(session_id);
	}

	print_line("[AIAssistant] CLEAR_SITE_B: clear button pressed");
	_clear_chat_ui();
	_add_system_message("Chat cleared. Creating new session...");

	// Reset and create new session.
	session_id = "";
	has_user_message = false;
	engine_prompts_injected = false;
	_create_session();
}

void AIAssistantDock::_on_setup_pressed() {
	if (!wizard_dialog) {
		_setup_wizard_ui();
	}

	// Ensure capabilities are available (fallback if server hasn't responded yet)
	if (wizard_capabilities.is_empty()) {
		// Default capabilities matching provider_capabilities.json
		String json_str = R"({
			"anthropic": {"name": "Anthropic", "services": ["chat"], "keyPrefix": "sk-ant-"},
			"openai": {"name": "OpenAI", "services": ["chat", "image-generation"], "keyPrefix": "sk-"},
			"google": {"name": "Google", "services": ["chat"]},
			"openrouter": {"name": "OpenRouter", "services": ["chat"], "keyPrefix": "sk-or-"},
			"replicate": {"name": "Replicate", "services": ["image-generation", "background-removal"], "keyPrefix": "r8_"},
			"photoroom": {"name": "PhotoRoom", "services": ["background-removal"], "keyPrefix": "sk_pr_"},
			"meshy": {"name": "Meshy", "services": ["3d-generation"], "keyPrefix": "msy_"},
			"suno": {"name": "Suno", "services": ["music-generation"]},
			"doubao": {"name": "Doubao", "services": ["chat", "image-generation"]},
			"local_rmbg": {"name": "RMBG-2.0", "services": ["background-removal"], "local": true, "healthCheck": "/ai-assets/rmbg-health"},
			"local_atlas_split": {"name": "Atlas Splitter", "services": ["atlas-split"], "local": true, "healthCheck": "/ai-assets/atlas-split-health"},
			"local_sharp": {"name": "Sharp", "services": ["image-postprocess"], "local": true, "healthCheck": "/ai-assets/sharp-health"},
			"local_gifenc": {"name": "GIFenc", "services": ["gif-recording"], "local": true, "healthCheck": "/ai-assets/gifenc-health"}
		})";
		Variant parsed = JSON::parse_string(json_str);
		if (parsed.get_type() == Variant::DICTIONARY) {
			wizard_capabilities = parsed;
		}
	}

	// Initialize connected state from server's connected set (includes non-chat providers like replicate)
	wizard_connected.clear();
	for (const String &pid : server_connected_providers) {
		wizard_connected.insert(pid, true);
	}
	// Rebuild Step 1 rows to reflect current connected state
	_wizard_rebuild_api_key_rows();
	wizard_step = 0;
	_wizard_show_step(0);
	wizard_dialog->popup_centered();

	// Auto-check health for all local providers every time wizard opens
	Array cap_keys = wizard_capabilities.keys();
	for (int i = 0; i < cap_keys.size(); i++) {
		String provider_id = cap_keys[i];
		Dictionary cap = wizard_capabilities[provider_id];
		bool is_local = cap.get("local", false);
		String health_url = cap.get("healthCheck", "");
		if (!is_local || health_url.is_empty()) {
			continue;
		}
		String url = service_url + health_url;
		Vector<String> headers = _get_headers_with_directory();
		HTTPRequest *req = memnew(HTTPRequest);
		add_child(req);
		req->connect("request_completed", Callable(this, "_on_wizard_local_health_completed").bind(provider_id));
		req->connect("request_completed", callable_mp((Node *)req, &Node::queue_free).unbind(4));
		req->request(url, headers, HTTPClient::METHOD_GET);
	}
}

void AIAssistantDock::_setup_wizard_ui() {
	wizard_dialog = memnew(AcceptDialog);
	wizard_dialog->set_title("Setup Wizard");
	wizard_dialog->set_min_size(Size2(600, 450));
	wizard_dialog->set_ok_button_text(""); // Hide default OK button
	wizard_dialog->get_ok_button()->set_visible(false);
	add_child(wizard_dialog);

	// Outer container: scroll area + nav buttons at bottom
	VBoxContainer *outer = memnew(VBoxContainer);
	outer->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	outer->set_custom_minimum_size(Size2(580, 400));
	wizard_dialog->add_child(outer);

	// Scrollable content area (pages go here)
	ScrollContainer *wizard_scroll = memnew(ScrollContainer);
	wizard_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	wizard_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	outer->add_child(wizard_scroll);

	VBoxContainer *root = memnew(VBoxContainer);
	root->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	wizard_scroll->add_child(root);

	// Step indicator
	wizard_step_label = memnew(Label);
	wizard_step_label->add_theme_font_size_override("font_size", 16);
	root->add_child(wizard_step_label);
	root->add_child(memnew(HSeparator));

	// === Step 1: API Keys ===
	wizard_pages[0] = memnew(VBoxContainer);
	wizard_pages[0]->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	root->add_child(wizard_pages[0]);

	Label *keys_title = memnew(Label);
	keys_title->set_text("Connect your API keys and services.\nClick Connect to enter your API key or test a local service.");
	keys_title->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	keys_title->add_theme_font_size_override("font_size", 12);
	keys_title->add_theme_color_override("font_color", Color(0.6, 0.6, 0.6));
	wizard_pages[0]->add_child(keys_title);

	wizard_api_key_list = memnew(VBoxContainer);
	wizard_api_key_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	wizard_pages[0]->add_child(wizard_api_key_list);
	// Rows are populated dynamically by _wizard_rebuild_api_key_rows()

	// === Step 2: Image Generation Model ===
	wizard_pages[1] = memnew(VBoxContainer);
	wizard_pages[1]->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	wizard_pages[1]->set_visible(false);
	root->add_child(wizard_pages[1]);

	Label *img_title = memnew(Label);
	img_title->set_text("Select the image generation model to use for AI-powered asset creation.\nOnly providers connected in Step 1 are shown.");
	img_title->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	img_title->add_theme_font_size_override("font_size", 12);
	img_title->add_theme_color_override("font_color", Color(0.6, 0.6, 0.6));
	wizard_pages[1]->add_child(img_title);

	wizard_image_model_container = memnew(VBoxContainer);
	wizard_image_model_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	wizard_pages[1]->add_child(wizard_image_model_container);

	// Model selector row
	{
		HBoxContainer *row = memnew(HBoxContainer);
		row->set_h_size_flags(Control::SIZE_EXPAND_FILL);

		Label *name_label = memnew(Label);
		name_label->set_text("Image Model");
		name_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_child(name_label);

		wizard_image_model_selector = memnew(OptionButton);
		wizard_image_model_selector->set_auto_translate(false);
		wizard_image_model_selector->set_custom_minimum_size(Size2(220, 0));
		wizard_image_model_selector->connect("item_selected", Callable(this, "_on_wizard_image_model_selected"));
		row->add_child(wizard_image_model_selector);

		wizard_image_model_status = memnew(Label);
		wizard_image_model_status->set_text("");
		wizard_image_model_status->set_custom_minimum_size(Size2(90, 0));
		row->add_child(wizard_image_model_status);

		wizard_image_model_container->add_child(row);
	}

	// "No provider connected" label (hidden by default)
	wizard_image_no_provider_label = memnew(Label);
	wizard_image_no_provider_label->set_text("No image generation provider connected. Go back to Step 1 to connect a provider with image generation support.");
	wizard_image_no_provider_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	wizard_image_no_provider_label->add_theme_color_override("font_color", Color(1, 0.5, 0));
	wizard_image_no_provider_label->set_visible(false);
	wizard_pages[1]->add_child(wizard_image_no_provider_label);

	// === Step 3: Background Removal ===
	wizard_pages[2] = memnew(VBoxContainer);
	wizard_pages[2]->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	wizard_pages[2]->set_visible(false);
	root->add_child(wizard_pages[2]);

	Label *bg_title = memnew(Label);
	bg_title->set_text("Select the background removal method for sprite processing.\nOnly providers connected in Step 1 are shown.");
	bg_title->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	bg_title->add_theme_font_size_override("font_size", 12);
	bg_title->add_theme_color_override("font_color", Color(0.6, 0.6, 0.6));
	wizard_pages[2]->add_child(bg_title);

	wizard_rembg_container = memnew(VBoxContainer);
	wizard_rembg_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	wizard_pages[2]->add_child(wizard_rembg_container);

	// Method selector row
	{
		HBoxContainer *row = memnew(HBoxContainer);
		row->set_h_size_flags(Control::SIZE_EXPAND_FILL);

		Label *name_label = memnew(Label);
		name_label->set_text("Remove Background");
		name_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_child(name_label);

		wizard_rembg_method_selector = memnew(OptionButton);
		wizard_rembg_method_selector->set_auto_translate(false);
		wizard_rembg_method_selector->set_custom_minimum_size(Size2(220, 0));
		wizard_rembg_method_selector->connect("item_selected", Callable(this, "_on_wizard_rembg_method_selected"));
		row->add_child(wizard_rembg_method_selector);

		wizard_rembg_method_status = memnew(Label);
		wizard_rembg_method_status->set_text("");
		wizard_rembg_method_status->set_custom_minimum_size(Size2(90, 0));
		row->add_child(wizard_rembg_method_status);

		wizard_rembg_container->add_child(row);
	}

	// "No provider connected" label (hidden by default)
	wizard_rembg_no_provider_label = memnew(Label);
	wizard_rembg_no_provider_label->set_text("No background removal provider connected. Go back to Step 1 to connect a provider with background removal support.");
	wizard_rembg_no_provider_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	wizard_rembg_no_provider_label->add_theme_color_override("font_color", Color(1, 0.5, 0));
	wizard_rembg_no_provider_label->set_visible(false);
	wizard_pages[2]->add_child(wizard_rembg_no_provider_label);

	// === Navigation buttons (outside scroll so always visible) ===
	outer->add_child(memnew(HSeparator));

	HBoxContainer *nav = memnew(HBoxContainer);
	nav->set_alignment(BoxContainer::ALIGNMENT_END);

	wizard_skip_button = memnew(Button);
	wizard_skip_button->set_text("Skip");
	wizard_skip_button->connect("pressed", Callable(this, "_on_wizard_skip"));
	nav->add_child(wizard_skip_button);

	nav->add_spacer(false);

	wizard_back_button = memnew(Button);
	wizard_back_button->set_text("Back");
	wizard_back_button->connect("pressed", Callable(this, "_on_wizard_back"));
	nav->add_child(wizard_back_button);

	wizard_next_button = memnew(Button);
	wizard_next_button->set_text("Next");
	wizard_next_button->connect("pressed", Callable(this, "_on_wizard_next"));
	nav->add_child(wizard_next_button);

	outer->add_child(nav);
}

void AIAssistantDock::_wizard_show_step(int p_step) {
	wizard_step = p_step;

	for (int i = 0; i < 3; i++) {
		wizard_pages[i]->set_visible(i == p_step);
	}

	String titles[] = {
		"Step 1 of 3 - API Keys",
		"Step 2 of 3 - Image Generation Model",
		"Step 3 of 3 - Background Removal"
	};
	wizard_step_label->set_text(titles[p_step]);

	wizard_back_button->set_visible(p_step > 0);

	if (p_step == 2) {
		wizard_next_button->set_text("Finish");
	} else {
		wizard_next_button->set_text("Next");
	}

	// Populate dynamic selectors and fetch current config when entering Steps 2 and 3
	if (p_step == 1) {
		_wizard_populate_image_models();
		_wizard_fetch_current_image_model();
	} else if (p_step == 2) {
		_wizard_populate_rembg_methods();
		_wizard_fetch_current_rembg_method();
	}
}

void AIAssistantDock::_wizard_populate_image_models() {
	wizard_image_model_selector->clear();
	wizard_image_model_status->set_text("");

	bool has_any = false;

	// Check which connected providers offer image-generation
	bool replicate_connected = wizard_connected.has("replicate") && wizard_connected["replicate"];
	if (replicate_connected) {
		wizard_image_model_selector->add_item("Replicate / nano-banana-2");
		wizard_image_model_selector->set_item_metadata(wizard_image_model_selector->get_item_count() - 1, "nano-banana-2");
		wizard_image_model_selector->add_item("Replicate / nano-banana-pro");
		wizard_image_model_selector->set_item_metadata(wizard_image_model_selector->get_item_count() - 1, "nano-banana-pro");
		has_any = true;
	}

	// Pre-select based on last known model
	if (has_any && !wizard_current_image_model.is_empty()) {
		for (int i = 0; i < wizard_image_model_selector->get_item_count(); i++) {
			if (wizard_image_model_selector->get_item_metadata(i) == wizard_current_image_model) {
				wizard_image_model_selector->select(i);
				break;
			}
		}
	}

	wizard_image_model_container->set_visible(has_any);
	wizard_image_no_provider_label->set_visible(!has_any);
}

void AIAssistantDock::_wizard_fetch_current_image_model() {
	String url = service_url + "/ai-assets/image-model";
	Vector<String> headers = _get_headers_with_directory();
	HTTPRequest *req = memnew(HTTPRequest);
	add_child(req);
	req->connect("request_completed", Callable(this, "_on_wizard_fetch_image_model_completed"));
	req->connect("request_completed", callable_mp((Node *)req, &Node::queue_free).unbind(4));
	req->request(url, headers, HTTPClient::METHOD_GET);
}

void AIAssistantDock::_on_wizard_fetch_image_model_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		return;
	}
	String body_str = _body_to_string(p_body);
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(body_str) != OK) {
		return;
	}
	Dictionary data = json->get_data();
	if (data.has("model")) {
		wizard_current_image_model = data["model"];
		// Update selector to match
		for (int i = 0; i < wizard_image_model_selector->get_item_count(); i++) {
			if (wizard_image_model_selector->get_item_metadata(i) == wizard_current_image_model) {
				wizard_image_model_selector->select(i);
				break;
			}
		}
	}
}

void AIAssistantDock::_wizard_populate_rembg_methods() {
	wizard_rembg_method_selector->clear();
	wizard_rembg_method_status->set_text("");

	bool has_any = false;

	bool replicate_connected = wizard_connected.has("replicate") && wizard_connected["replicate"];
	bool local_rmbg_connected = wizard_connected.has("local_rmbg") && wizard_connected["local_rmbg"];

	if (replicate_connected) {
		wizard_rembg_method_selector->add_item("Replicate / bria-ai/rmbg-2.0");
		wizard_rembg_method_selector->set_item_metadata(wizard_rembg_method_selector->get_item_count() - 1, "replicate");
		has_any = true;
	}
	if (local_rmbg_connected) {
		wizard_rembg_method_selector->add_item("Local RMBG-2.0");
		wizard_rembg_method_selector->set_item_metadata(wizard_rembg_method_selector->get_item_count() - 1, "local");
		has_any = true;
	}

	// Pre-select based on last known method
	if (has_any && !wizard_current_rembg_method.is_empty()) {
		for (int i = 0; i < wizard_rembg_method_selector->get_item_count(); i++) {
			if (wizard_rembg_method_selector->get_item_metadata(i) == wizard_current_rembg_method) {
				wizard_rembg_method_selector->select(i);
				break;
			}
		}
	}

	wizard_rembg_container->set_visible(has_any);
	wizard_rembg_no_provider_label->set_visible(!has_any);
}

void AIAssistantDock::_wizard_fetch_current_rembg_method() {
	String url = service_url + "/ai-assets/removebg-method";
	Vector<String> headers = _get_headers_with_directory();
	HTTPRequest *req = memnew(HTTPRequest);
	add_child(req);
	req->connect("request_completed", Callable(this, "_on_wizard_fetch_rembg_method_completed"));
	req->connect("request_completed", callable_mp((Node *)req, &Node::queue_free).unbind(4));
	req->request(url, headers, HTTPClient::METHOD_GET);
}

void AIAssistantDock::_on_wizard_fetch_rembg_method_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		return;
	}
	String body_str = _body_to_string(p_body);
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(body_str) != OK) {
		return;
	}
	Dictionary data = json->get_data();
	if (data.has("method")) {
		wizard_current_rembg_method = data["method"];
		// Update selector to match
		for (int i = 0; i < wizard_rembg_method_selector->get_item_count(); i++) {
			if (wizard_rembg_method_selector->get_item_metadata(i) == wizard_current_rembg_method) {
				wizard_rembg_method_selector->select(i);
				break;
			}
		}
	}
}

void AIAssistantDock::_on_wizard_back() {
	if (wizard_step > 0) {
		_wizard_show_step(wizard_step - 1);
	}
}

void AIAssistantDock::_on_wizard_next() {
	if (wizard_step < 2) {
		_wizard_show_step(wizard_step + 1);
	} else {
		_on_wizard_finish();
	}
}

void AIAssistantDock::_on_wizard_skip() {
	if (wizard_step < 2) {
		_wizard_show_step(wizard_step + 1);
	} else {
		_on_wizard_finish();
	}
}

void AIAssistantDock::_on_wizard_finish() {
	wizard_dialog->hide();

	// Show summary in chat
	RichTextLabel *summary = memnew(RichTextLabel);
	summary->set_use_bbcode(true);
	summary->set_fit_content(true);
	summary->set_selection_enabled(true);

	String msg = "[color=green][b]Setup Complete[/b][/color]\n\n";

	// API Keys - list all providers from capabilities
	Array cap_keys = wizard_capabilities.keys();
	for (int i = 0; i < cap_keys.size(); i++) {
		String provider_id = cap_keys[i];
		Dictionary cap = wizard_capabilities[provider_id];
		String name = cap.get("name", provider_id);
		bool is_connected = wizard_connected.has(provider_id) && wizard_connected[provider_id];
		if (is_connected) {
			msg += "[color=green]* " + name + " - connected[/color]\n";
		} else {
			msg += "[color=gray]* " + name + " - skipped[/color]\n";
		}
	}
	// Local RMBG
	bool local_rmbg_connected = wizard_connected.has("local_rmbg") && wizard_connected["local_rmbg"];
	if (local_rmbg_connected) {
		msg += "[color=green]* Local RMBG-2.0 - connected[/color]\n";
	} else {
		msg += "[color=gray]* Local RMBG-2.0 - skipped[/color]\n";
	}

	// Image model
	if (!wizard_current_image_model.is_empty()) {
		msg += "[color=green]* Image Model: " + wizard_current_image_model + "[/color]\n";
	} else {
		msg += "[color=gray]* Image Model - skipped[/color]\n";
	}

	// Background removal
	if (!wizard_current_rembg_method.is_empty()) {
		String method_label = wizard_current_rembg_method == "local" ? "Local RMBG-2.0" : "Replicate / bria-ai/rmbg-2.0";
		msg += "[color=green]* Background Removal: " + method_label + "[/color]\n";
	} else {
		msg += "[color=gray]* Background Removal - skipped[/color]\n";
	}

	msg += "\nYou can reconfigure anytime by clicking the key icon in the toolbar.";

	summary->set_text(msg);
	chat_container->add_child(summary);
	_scroll_chat_to_bottom();
}

void AIAssistantDock::_wizard_rebuild_api_key_rows() {
	// Clear and rebuild Step 1 rows from capabilities (includes local providers)
	while (wizard_api_key_list->get_child_count() > 0) {
		Node *child = wizard_api_key_list->get_child(0);
		wizard_api_key_list->remove_child(child);
		memdelete(child);
	}

	struct RowInfo {
		String id;
		String name;
		String desc;
		bool connected;
	};

	// Separate local vs cloud providers, connected first within each group
	Vector<RowInfo> local_connected, local_disconnected;
	Vector<RowInfo> cloud_connected, cloud_disconnected;

	Array cap_keys = wizard_capabilities.keys();
	for (int i = 0; i < cap_keys.size(); i++) {
		String provider_id = cap_keys[i];
		Dictionary cap = wizard_capabilities[provider_id];
		String name = cap.get("name", provider_id);
		bool is_local = cap.get("local", false);

		// Build description from services list
		Array services = cap.get("services", Array());
		HashSet<String> tags;
		for (int s = 0; s < services.size(); s++) {
			String svc = services[s];
			if (svc == "image-generation") {
				tags.insert("image gen");
			} else if (svc == "background-removal" || svc == "atlas-split" || svc == "image-postprocess") {
				tags.insert("image postprocessing");
			} else if (svc == "3d-generation") {
				tags.insert("3D gen");
			} else if (svc == "music-generation") {
				tags.insert("music gen");
			} else if (svc == "gif-recording") {
				tags.insert("game recording");
			} else {
				tags.insert(svc);
			}
		}
		String desc;
		for (const String &tag : tags) {
			if (!desc.is_empty()) {
				desc += ", ";
			}
			desc += tag;
		}

		bool is_connected = wizard_connected.has(provider_id) && wizard_connected[provider_id];
		RowInfo row = { provider_id, name, desc, is_connected };
		if (is_local) {
			(is_connected ? local_connected : local_disconnected).push_back(row);
		} else {
			(is_connected ? cloud_connected : cloud_disconnected).push_back(row);
		}
	}

	// Build final list: Local category first, then Cloud
	Vector<RowInfo> local_rows;
	for (int i = 0; i < local_connected.size(); i++) {
		local_rows.push_back(local_connected[i]);
	}
	for (int i = 0; i < local_disconnected.size(); i++) {
		local_rows.push_back(local_disconnected[i]);
	}

	Vector<RowInfo> cloud_rows;
	for (int i = 0; i < cloud_connected.size(); i++) {
		cloud_rows.push_back(cloud_connected[i]);
	}
	for (int i = 0; i < cloud_disconnected.size(); i++) {
		cloud_rows.push_back(cloud_disconnected[i]);
	}

	// Add "Local" category header + rows
	if (!local_rows.is_empty()) {
		Label *local_header = memnew(Label);
		local_header->set_text("Local");
		local_header->add_theme_font_size_override("font_size", 13);
		local_header->add_theme_color_override("font_color", Color(0.8, 0.8, 0.8));
		wizard_api_key_list->add_child(local_header);
	}
	for (int i = 0; i < local_rows.size(); i++) {
		VBoxContainer *item = memnew(VBoxContainer);
		item->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		item->set_meta("provider_id", local_rows[i].id);

		HBoxContainer *row = memnew(HBoxContainer);
		row->set_h_size_flags(Control::SIZE_EXPAND_FILL);

		Label *name_label = memnew(Label);
		name_label->set_text(local_rows[i].name);
		name_label->set_auto_translate(false);
		name_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_child(name_label);

		Label *desc = memnew(Label);
		desc->set_text(local_rows[i].desc);
		desc->set_auto_translate(false);
		desc->add_theme_font_size_override("font_size", 11);
		desc->add_theme_color_override("font_color", Color(0.5, 0.5, 0.5));
		desc->set_custom_minimum_size(Size2(180, 0));
		row->add_child(desc);

		if (local_rows[i].connected) {
			Label *status = memnew(Label);
			status->set_text("Connected");
			status->add_theme_color_override("font_color", Color(0.3, 1.0, 0.3));
			status->set_custom_minimum_size(Size2(90, 0));
			row->add_child(status);
		} else {
			Button *connect_btn = memnew(Button);
			connect_btn->set_text("Connect");
			connect_btn->set_custom_minimum_size(Size2(90, 0));
			connect_btn->connect("pressed", Callable(this, "_on_wizard_api_key_connect").bind(local_rows[i].id));
			row->add_child(connect_btn);
		}

		item->add_child(row);
		wizard_api_key_list->add_child(item);
	}

	// Add "Cloud" category header + rows
	if (!cloud_rows.is_empty()) {
		Label *cloud_header = memnew(Label);
		cloud_header->set_text("Cloud");
		cloud_header->add_theme_font_size_override("font_size", 13);
		cloud_header->add_theme_color_override("font_color", Color(0.8, 0.8, 0.8));
		wizard_api_key_list->add_child(cloud_header);
	}
	for (int i = 0; i < cloud_rows.size(); i++) {
		VBoxContainer *item = memnew(VBoxContainer);
		item->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		item->set_meta("provider_id", cloud_rows[i].id);

		HBoxContainer *row = memnew(HBoxContainer);
		row->set_h_size_flags(Control::SIZE_EXPAND_FILL);

		Label *name_label = memnew(Label);
		name_label->set_text(cloud_rows[i].name);
		name_label->set_auto_translate(false);
		name_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		row->add_child(name_label);

		Label *desc = memnew(Label);
		desc->set_text(cloud_rows[i].desc);
		desc->set_auto_translate(false);
		desc->add_theme_font_size_override("font_size", 11);
		desc->add_theme_color_override("font_color", Color(0.5, 0.5, 0.5));
		desc->set_custom_minimum_size(Size2(180, 0));
		row->add_child(desc);

		if (cloud_rows[i].connected) {
			Label *status = memnew(Label);
			status->set_text("Connected");
			status->add_theme_color_override("font_color", Color(0.3, 1.0, 0.3));
			status->set_custom_minimum_size(Size2(90, 0));
			row->add_child(status);
		} else {
			Button *connect_btn = memnew(Button);
			connect_btn->set_text("Connect");
			connect_btn->set_custom_minimum_size(Size2(90, 0));
			connect_btn->connect("pressed", Callable(this, "_on_wizard_api_key_connect").bind(cloud_rows[i].id));
			row->add_child(connect_btn);
		}

		item->add_child(row);
		wizard_api_key_list->add_child(item);
	}
}

void AIAssistantDock::_on_wizard_api_key_connect(const String &p_provider_id) {
	// Find the item by provider_id metadata
	VBoxContainer *item = nullptr;
	for (int i = 0; i < wizard_api_key_list->get_child_count(); i++) {
		VBoxContainer *candidate = Object::cast_to<VBoxContainer>(wizard_api_key_list->get_child(i));
		if (candidate && candidate->has_meta("provider_id") && String(candidate->get_meta("provider_id")) == p_provider_id) {
			item = candidate;
			break;
		}
	}
	if (!item || item->get_child_count() > 1) {
		return; // Not found or already expanded
	}

	// Check if this is a local provider (health check instead of API key)
	bool is_local = false;
	String health_check_url;
	if (wizard_capabilities.has(p_provider_id)) {
		Dictionary cap = wizard_capabilities[p_provider_id];
		is_local = cap.get("local", false);
		health_check_url = cap.get("healthCheck", "");
	}

	if (is_local && !health_check_url.is_empty()) {
		// Health check instead of key input
		HBoxContainer *status_row = memnew(HBoxContainer);
		Label *status = memnew(Label);
		status->set_text("Testing...");
		status->add_theme_color_override("font_color", Color(1, 1, 0));
		status_row->add_child(status);
		item->add_child(status_row);

		String url = service_url + health_check_url;
		Vector<String> headers = _get_headers_with_directory();
		HTTPRequest *req = memnew(HTTPRequest);
		add_child(req);
		req->connect("request_completed", Callable(this, "_on_wizard_local_health_completed").bind(p_provider_id));
		req->connect("request_completed", callable_mp((Node *)req, &Node::queue_free).unbind(4));
		req->request(url, headers, HTTPClient::METHOD_GET);
	} else {
		// API key input for anthropic or replicate
		HBoxContainer *key_row = memnew(HBoxContainer);
		key_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);

		LineEdit *key_input = memnew(LineEdit);
		key_input->set_placeholder("Enter API key...");
		key_input->set_secret(true);
		key_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		key_row->add_child(key_input);

		Label *status_label = memnew(Label);
		status_label->set_text("");
		status_label->set_custom_minimum_size(Size2(80, 0));
		key_row->add_child(status_label);

		Button *submit_btn = memnew(Button);
		submit_btn->set_text("Save");
		submit_btn->connect("pressed", Callable(this, "_on_wizard_api_key_submit").bind(p_provider_id));
		key_row->add_child(submit_btn);

		item->add_child(key_row);
		key_input->grab_focus();
	}
}

void AIAssistantDock::_on_wizard_api_key_submit(const String &p_provider_id) {
	// Find the item by provider_id metadata
	VBoxContainer *item = nullptr;
	for (int i = 0; i < wizard_api_key_list->get_child_count(); i++) {
		VBoxContainer *candidate = Object::cast_to<VBoxContainer>(wizard_api_key_list->get_child(i));
		if (candidate && candidate->has_meta("provider_id") && String(candidate->get_meta("provider_id")) == p_provider_id) {
			item = candidate;
			break;
		}
	}
	if (!item || item->get_child_count() < 2) {
		return;
	}

	HBoxContainer *key_row = Object::cast_to<HBoxContainer>(item->get_child(1));
	if (!key_row) {
		return;
	}

	LineEdit *key_input = Object::cast_to<LineEdit>(key_row->get_child(0));
	Label *status = Object::cast_to<Label>(key_row->get_child(1));
	if (!key_input || !status) {
		return;
	}

	String key = key_input->get_text().strip_edges();
	if (key.is_empty()) {
		status->set_text("Enter a key");
		status->add_theme_color_override("font_color", Color(1, 0.5, 0));
		return;
	}

	status->set_text("Saving...");
	status->add_theme_color_override("font_color", Color(1, 1, 0));

	String url = service_url + "/provider/" + p_provider_id + "/api-key";
	Dictionary body;
	body["apiKey"] = key;

	Vector<String> headers = _get_headers_with_directory();
	HTTPRequest *req = memnew(HTTPRequest);
	add_child(req);
	req->connect("request_completed", Callable(this, "_on_wizard_api_key_completed").bind(p_provider_id));
	req->connect("request_completed", callable_mp((Node *)req, &Node::queue_free).unbind(4));
	req->request(url, headers, HTTPClient::METHOD_POST, JSON::stringify(body));
}

void AIAssistantDock::_on_wizard_api_key_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body, const String &p_provider_id) {
	// Find the item by provider_id metadata
	VBoxContainer *item = nullptr;
	for (int i = 0; i < wizard_api_key_list->get_child_count(); i++) {
		VBoxContainer *candidate = Object::cast_to<VBoxContainer>(wizard_api_key_list->get_child(i));
		if (candidate && candidate->has_meta("provider_id") && String(candidate->get_meta("provider_id")) == p_provider_id) {
			item = candidate;
			break;
		}
	}
	if (!item || item->get_child_count() < 2) {
		return;
	}
	HBoxContainer *key_row = Object::cast_to<HBoxContainer>(item->get_child(1));
	if (!key_row) {
		return;
	}
	Label *status = Object::cast_to<Label>(key_row->get_child(1));
	if (!status) {
		return;
	}

	if (p_result != HTTPRequest::RESULT_SUCCESS) {
		status->set_text("Connection error");
		status->add_theme_color_override("font_color", Color(1, 0, 0));
		return;
	}

	if (p_code == 200) {
		status->set_text("Connected!");
		status->add_theme_color_override("font_color", Color(0.3, 1.0, 0.3));

		// Set connected flag
		wizard_connected.insert(p_provider_id, true);

		// Replace Connect button with "Connected" label in header row
		HBoxContainer *header_row = Object::cast_to<HBoxContainer>(item->get_child(0));
		if (header_row) {
			int last = header_row->get_child_count() - 1;
			Button *btn = Object::cast_to<Button>(header_row->get_child(last));
			if (btn) {
				btn->queue_free();
				Label *connected = memnew(Label);
				connected->set_text("Connected");
				connected->add_theme_color_override("font_color", Color(0.3, 1.0, 0.3));
				connected->set_custom_minimum_size(Size2(90, 0));
				header_row->add_child(connected);
			}
		}

		// Refresh providers for model menu
		_fetch_providers();
	} else if (p_code == 401) {
		status->set_text("Invalid key");
		status->add_theme_color_override("font_color", Color(1, 0, 0));
	} else {
		status->set_text("Error (" + itos(p_code) + ")");
		status->add_theme_color_override("font_color", Color(1, 0, 0));
	}
}

void AIAssistantDock::_on_wizard_local_health_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body, const String &p_provider_id) {
	// Find the local provider row by metadata
	VBoxContainer *item = nullptr;
	for (int i = 0; i < wizard_api_key_list->get_child_count(); i++) {
		VBoxContainer *candidate = Object::cast_to<VBoxContainer>(wizard_api_key_list->get_child(i));
		if (candidate && candidate->has_meta("provider_id") && String(candidate->get_meta("provider_id")) == p_provider_id) {
			item = candidate;
			break;
		}
	}
	if (!item) {
		return;
	}

	// Update expanded status row if present (user clicked Connect)
	if (item->get_child_count() >= 2) {
		HBoxContainer *status_row = Object::cast_to<HBoxContainer>(item->get_child(1));
		if (status_row && status_row->get_child_count() >= 1) {
			Label *status = Object::cast_to<Label>(status_row->get_child(0));
			if (status) {
				if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
					status->set_text("Service not running");
					status->add_theme_color_override("font_color", Color(1, 0, 0));
				} else {
					status->set_text("Connected!");
					status->add_theme_color_override("font_color", Color(0.3, 1.0, 0.3));
				}
			}
		}
	}

	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		return;
	}

	wizard_connected.insert(p_provider_id, true);

	// Replace Connect button with "Connected" label in header row
	HBoxContainer *header_row = Object::cast_to<HBoxContainer>(item->get_child(0));
	if (header_row) {
		int last = header_row->get_child_count() - 1;
		Button *btn = Object::cast_to<Button>(header_row->get_child(last));
		if (btn) {
			btn->queue_free();
			Label *connected = memnew(Label);
			connected->set_text("Connected");
			connected->add_theme_color_override("font_color", Color(0.3, 1.0, 0.3));
			connected->set_custom_minimum_size(Size2(90, 0));
			header_row->add_child(connected);
		}
	}
}

void AIAssistantDock::_on_wizard_image_model_selected(int p_index) {
	if (p_index < 0 || p_index >= wizard_image_model_selector->get_item_count()) {
		return;
	}
	String model = wizard_image_model_selector->get_item_metadata(p_index);

	wizard_image_model_status->set_text("Saving...");
	wizard_image_model_status->add_theme_color_override("font_color", Color(1, 1, 0));

	Dictionary body;
	body["model"] = model;
	String url = service_url + "/ai-assets/image-model";
	Vector<String> headers = _get_headers_with_directory();
	HTTPRequest *req = memnew(HTTPRequest);
	add_child(req);
	req->connect("request_completed", Callable(this, "_on_wizard_image_model_saved"));
	req->connect("request_completed", callable_mp((Node *)req, &Node::queue_free).unbind(4));
	req->request(url, headers, HTTPClient::METHOD_POST, JSON::stringify(body));
}

void AIAssistantDock::_on_wizard_image_model_saved(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (!wizard_image_model_status) {
		return;
	}
	if (p_result == HTTPRequest::RESULT_SUCCESS && p_code == 200) {
		wizard_image_model_status->set_text("Saved");
		wizard_image_model_status->add_theme_color_override("font_color", Color(0.3, 1.0, 0.3));
		int sel = wizard_image_model_selector->get_selected();
		if (sel >= 0) {
			wizard_current_image_model = wizard_image_model_selector->get_item_metadata(sel);
		}
	} else {
		wizard_image_model_status->set_text("Failed");
		wizard_image_model_status->add_theme_color_override("font_color", Color(1, 0, 0));
	}
}

void AIAssistantDock::_on_wizard_rembg_method_selected(int p_index) {
	if (p_index < 0 || p_index >= wizard_rembg_method_selector->get_item_count()) {
		return;
	}
	String method = wizard_rembg_method_selector->get_item_metadata(p_index);

	wizard_rembg_method_status->set_text("Saving...");
	wizard_rembg_method_status->add_theme_color_override("font_color", Color(1, 1, 0));

	Dictionary body;
	body["method"] = method;
	String url = service_url + "/ai-assets/removebg-method";
	Vector<String> headers = _get_headers_with_directory();
	HTTPRequest *req = memnew(HTTPRequest);
	add_child(req);
	req->connect("request_completed", Callable(this, "_on_wizard_rembg_method_saved"));
	req->connect("request_completed", callable_mp((Node *)req, &Node::queue_free).unbind(4));
	req->request(url, headers, HTTPClient::METHOD_POST, JSON::stringify(body));
}

void AIAssistantDock::_on_wizard_rembg_method_saved(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (!wizard_rembg_method_status) {
		return;
	}
	if (p_result == HTTPRequest::RESULT_SUCCESS && p_code == 200) {
		wizard_rembg_method_status->set_text("Saved");
		wizard_rembg_method_status->add_theme_color_override("font_color", Color(0.3, 1.0, 0.3));
		int sel = wizard_rembg_method_selector->get_selected();
		if (sel >= 0) {
			wizard_current_rembg_method = wizard_rembg_method_selector->get_item_metadata(sel);
		}
	} else {
		wizard_rembg_method_status->set_text("Failed");
		wizard_rembg_method_status->add_theme_color_override("font_color", Color(1, 0, 0));
	}
}

void AIAssistantDock::_on_settings_pressed() {
	// Load project prompt (CLAUDE.md)
	project_prompt_edit->set_text(_load_project_prompt());
	String prompt_path = _get_project_directory().path_join("CLAUDE.md");
	project_prompt_path_label->set_text("Path: " + prompt_path);

	// Populate engine prompt file list
	_populate_engine_prompt_tree();
	engine_prompt_preview->set_text("Select an engine prompt file above to preview its contents.");

	settings_dialog->popup_centered();
}

void AIAssistantDock::_on_settings_save_pressed() {
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
	String prompts_dir = exe_dir.path_join("..").path_join("..").path_join("prompts").simplify_path();

	Ref<DirAccess> dir = DirAccess::open(prompts_dir);
	if (dir.is_null()) {
		return result;
	}

	dir->list_dir_begin();
	String entry = dir->get_next();
	while (!entry.is_empty()) {
		if (!dir->current_is_dir() && entry.ends_with(".md")) {
			result.push_back(prompts_dir.path_join(entry));
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

	String response_text = _body_to_string(p_body);
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
	// Auto-resize input box: 1 line = ~30px, min 60px (2 lines), max 6 lines.
	int line_count = prompt_input->get_line_count();
	int line_height = prompt_input->get_line_height();
	int min_height = 60;
	int max_lines = 6;
	int desired = CLAMP(line_count, 2, max_lines) * line_height;
	if (desired < min_height) {
		desired = min_height;
	}
	prompt_input->set_custom_minimum_size(Size2(0, desired));

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

		// Build command list (skills are handled by OpenCode, not listed here)
		Vector<CommandDef> commands;
		commands.push_back({ "/connect replicate <token>", "/connect replicate ", "Configure Replicate API" });
		commands.push_back({ "/providers", "/providers", "List configured providers" });
		commands.push_back({ "/disconnect replicate", "/disconnect replicate", "Remove Replicate provider" });

		for (int i = 0; i < commands.size(); i++) {
			const CommandDef &cmd = commands[i];
			if (cmd.command.to_lower().begins_with(filter) || filter == "/") {
				Button *btn = memnew(Button);
				btn->set_text(cmd.command + "   " + cmd.description);
				btn->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
				btn->add_theme_color_override("font_color", Color(0.7, 0.85, 1.0));
				btn->set_flat(true);
				btn->set_focus_mode(Control::FOCUS_NONE); // Don't steal focus from input

				btn->connect("pressed", Callable(this, "_on_slash_hint_pressed").bind(i));

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
	// Build the same command list as _on_prompt_text_changed
	Vector<String> fill_texts;
	fill_texts.push_back("/connect replicate ");
	fill_texts.push_back("/providers");
	fill_texts.push_back("/disconnect replicate");

	if (p_id >= 0 && p_id < fill_texts.size()) {
		prompt_input->set_text(fill_texts[p_id]);
		prompt_input->set_caret_column(fill_texts[p_id].length());
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

	if (key->get_keycode() == Key::ENTER && key->is_shift_pressed()) {
		// Shift+Enter: insert newline explicitly.
		prompt_input->insert_text_at_caret("\n");
		prompt_input->accept_event();
	} else if (key->get_keycode() == Key::ENTER && !key->is_shift_pressed()) {
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
	Vector<uint8_t> file_data = FileAccess::get_file_as_bytes(p_path, &err);
	if (err != OK || file_data.is_empty()) {
		_add_system_message("Failed to read image file: " + p_path);
		return false;
	}

	// Load image (needed for thumbnail and potential downscaling)
	Ref<Image> img = Image::load_from_file(p_path);
	if (img.is_null() || img->is_empty()) {
		_add_system_message("Failed to load image: " + p_path.get_file());
		return false;
	}

	String mime_type = _get_mime_type_for_extension(ext);

	// Auto-downscale if image data exceeds the API size limit
	if (file_data.size() > MAX_IMAGE_SIZE_BYTES) {
		float original_mb = file_data.size() / (1024.0f * 1024.0f);
		Ref<Image> scaled = img->duplicate();

		// Try progressively smaller scales until it fits
		for (float scale = 0.75f; scale >= 0.1f; scale -= 0.1f) {
			int new_w = MAX(1, (int)(img->get_width() * scale));
			int new_h = MAX(1, (int)(img->get_height() * scale));
			scaled = img->duplicate();
			scaled->resize(new_w, new_h, Image::INTERPOLATE_LANCZOS);

			// Encode as JPEG for better compression (screenshots/photos)
			file_data = scaled->save_jpg_to_buffer(0.85f);
			if (file_data.size() <= MAX_IMAGE_SIZE_BYTES) {
				break;
			}
		}

		if (file_data.size() > MAX_IMAGE_SIZE_BYTES) {
			_add_message("Error", vformat("Could not compress image below limit: %s", p_path.get_file()), Color(1.0, 0.4, 0.4));
			return false;
		}

		mime_type = "image/jpeg";
		float new_mb = file_data.size() / (1024.0f * 1024.0f);
		_add_log_entry("INFO", vformat("Image resized: %.1f MB -> %.1f MB (%dx%d): %s",
				original_mb, new_mb, img->get_width(), img->get_height(), p_path.get_file()),
				Color(0.8, 0.8, 0.5));
		img = scaled;
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
	att.mime_type = mime_type;
	att.data = file_data;
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

	String mime_type = "image/png";
	Vector<uint8_t> send_data = png_data;
	Ref<Image> send_image = clipboard_image;

	// Auto-downscale if clipboard image exceeds the API size limit
	if (png_data.size() > MAX_IMAGE_SIZE_BYTES) {
		float original_mb = png_data.size() / (1024.0f * 1024.0f);

		// Try progressively smaller scales, encode as JPEG for compression
		for (float scale = 0.75f; scale >= 0.1f; scale -= 0.1f) {
			int new_w = MAX(1, (int)(clipboard_image->get_width() * scale));
			int new_h = MAX(1, (int)(clipboard_image->get_height() * scale));
			Ref<Image> scaled = clipboard_image->duplicate();
			scaled->resize(new_w, new_h, Image::INTERPOLATE_LANCZOS);

			send_data = scaled->save_jpg_to_buffer(0.85f);
			if (send_data.size() <= MAX_IMAGE_SIZE_BYTES) {
				send_image = scaled;
				break;
			}
		}

		if (send_data.size() > MAX_IMAGE_SIZE_BYTES) {
			_add_message("Error", "Could not compress clipboard image below limit.", Color(1.0, 0.4, 0.4));
			return false;
		}

		mime_type = "image/jpeg";
		float new_mb = send_data.size() / (1024.0f * 1024.0f);
		_add_log_entry("INFO", vformat("Clipboard image resized: %.1f MB -> %.1f MB (%dx%d)",
				original_mb, new_mb, send_image->get_width(), send_image->get_height()),
				Color(0.8, 0.8, 0.5));
	}

	String timestamp = Time::get_singleton()->get_datetime_string_from_system().replace(":", "").replace("-", "").replace("T", "_");
	String filename = "clipboard_" + timestamp + (mime_type == "image/jpeg" ? ".jpg" : ".png");

	// Create thumbnail
	Ref<Image> thumb = send_image->duplicate();
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
	att.mime_type = mime_type;
	att.data = send_data;
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
		tex_rect->set_tooltip_text(att.filename + " (" + String::humanize_size(att.data.size()) + ")\nDouble-click to open");

		// Double-click to open in system viewer
		if (!att.file_path.is_empty()) {
			tex_rect->connect("gui_input", Callable(this, "_on_attachment_gui_input").bind(att.file_path));
		}
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

void AIAssistantDock::_on_attachment_gui_input(const Ref<InputEvent> &p_event, const String &p_file_path) {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->is_double_click() && mb->get_button_index() == MouseButton::LEFT) {
		OS::get_singleton()->shell_open(p_file_path);
	}
}

void AIAssistantDock::_clear_attachments() {
	pending_attachments.clear();
	_rebuild_attachment_previews();
}

void AIAssistantDock::_on_chat_thumbnail_gui_input(const Ref<InputEvent> &p_event, const String &p_cache_path, const String &p_title) {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->is_double_click() && mb->get_button_index() == MouseButton::LEFT) {
		_show_image_preview(p_cache_path, p_title);
	}
}

void AIAssistantDock::_show_image_preview(const String &p_cache_path, const String &p_title) {
	if (p_cache_path.is_empty() || image_preview_dialog == nullptr) {
		return;
	}

	// Load the full image from disk cache on demand
	Ref<Image> img;
	img.instantiate();
	Error err = img->load(p_cache_path);
	if (err != OK || img->is_empty()) {
		print_line("[AA] Failed to load cached image: " + p_cache_path);
		return;
	}

	Ref<ImageTexture> tex = ImageTexture::create_from_image(img);
	image_preview_rect->set_texture(tex);
	image_preview_dialog->set_title(p_title.is_empty() ? "Image Preview" : p_title);

	// Size the dialog based on the image, capped to reasonable screen proportions
	int img_w = tex->get_width();
	int img_h = tex->get_height();
	int max_w = 900;
	int max_h = 700;
	if (img_w > max_w || img_h > max_h) {
		float scale = MIN((float)max_w / img_w, (float)max_h / img_h);
		img_w = (int)(img_w * scale);
		img_h = (int)(img_h * scale);
	}
	// Ensure minimum size
	img_w = MAX(img_w, 256);
	img_h = MAX(img_h, 256);
	image_preview_dialog->set_min_size(Size2(img_w, img_h));

	image_preview_dialog->popup_centered();
}

String AIAssistantDock::_get_image_cache_dir() const {
	String project_dir = _get_project_directory();
	return project_dir.path_join(".godot").path_join("ai_cache").path_join("images");
}

String AIAssistantDock::_save_image_to_cache(const Vector<uint8_t> &p_data, const String &p_extension) {
	String cache_dir = _get_image_cache_dir();

	// Ensure directory exists
	Error mkdir_err = DirAccess::make_dir_recursive_absolute(cache_dir);

	// Generate a unique filename from content hash
	unsigned char md5_hash[16];
	CryptoCore::md5(p_data.ptr(), p_data.size(), md5_hash);
	String hash = String::md5(md5_hash);
	String filename = hash + "." + p_extension;
	String full_path = cache_dir.path_join(filename);

	// Skip if already cached
	if (FileAccess::exists(full_path)) {
		return full_path;
	}

	// Write raw bytes to file
	Ref<FileAccess> f = FileAccess::open(full_path, FileAccess::WRITE);
	if (f.is_null()) {
		print_line("[AA] Failed to write image cache: " + full_path);
		return "";
	}
	f->store_buffer(p_data.ptr(), p_data.size());
	f->close();
	return full_path;
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
		HTTPRequest *req = memnew(HTTPRequest);
		add_child(req);
		req->connect("request_completed", callable_mp((Node *)req, &Node::queue_free).unbind(4));
		req->request(url, headers, HTTPClient::METHOD_POST, json_body);
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
		// Unknown local command — pass through to AI (skills are handled by OpenCode)
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
		_add_ai_message("**RedBlue AI Help**\n\n**Quick Commands:**\n- run / play - Run the project\n- stop - Stop the project\n- save - Save the current scene\n\n**Full AI Features:**\nClick 'Connect' to connect to the AI service (http://localhost:4096).\n\nMake sure the AI server is running with the RedBlue launcher.");
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

	chat_container->add_child(msg_container);
}

void AIAssistantDock::_scroll_chat_to_bottom() {
	if (!user_scrolled_up) {
		// Use a few frames of brute-force scrolling to ensure we reach the bottom
		// even after layout updates change the scrollbar range.
		scroll_to_bottom_frames = MAX(scroll_to_bottom_frames, 5);
	}
}

void AIAssistantDock::_clear_scroll_suppress() {
	suppress_scroll_tracking = false;
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

void AIAssistantDock::_toggle_reasoning_collapse(const String &p_key) {
	if (!reasoning_labels.has(p_key)) {
		return;
	}
	RichTextLabel *label = reasoning_labels[p_key];
	bool is_visible = label->is_visible();
	label->set_visible(!is_visible);

	// Update toggle button text
	VBoxContainer *container = reasoning_containers[p_key];
	if (container && container->get_child_count() > 0) {
		Button *btn = Object::cast_to<Button>(container->get_child(0));
		if (btn) {
			if (is_visible) {
				btn->set_text(String::utf8("\xf0\x9f\x92\xad") + " Thinking (collapsed)");
			} else {
				btn->set_text(String::utf8("\xf0\x9f\x92\xad") + " Thinking...");
			}
		}
	}
}

void AIAssistantDock::_on_chat_scroll_changed(double p_value) {
	// Track whether user has scrolled away from the bottom.
	// If within 30px of the max scroll, consider it "at bottom".
	VScrollBar *vbar = chat_scroll->get_v_scroll_bar();
	if (vbar) {
		double max_scroll = vbar->get_max() - vbar->get_page();
		bool is_at_bottom = (p_value >= max_scroll - 30.0);

		if (suppress_scroll_tracking) {
			// This callback was triggered by a programmatic set_v_scroll.
			// Consume the flag so the next scroll event is tracked normally.
			suppress_scroll_tracking = false;
		} else {
			user_scrolled_up = !is_at_bottom;
		}
	}
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
	// Skip system-injected context — not user-visible.
	if (p_text.begins_with("[CODING STANDARDS]") ||
			p_text.begins_with("[SELECTED FILE(S)") ||
			p_text.begins_with("[SELECTED NODE(S)")) {
		return;
	}

	String display_text = p_text;

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

	// Show image thumbnails inline if there are pending attachments
	if (!pending_attachments.is_empty()) {
		HBoxContainer *thumb_row = memnew(HBoxContainer);
		thumb_row->add_theme_constant_override("separation", 4);

		for (int i = 0; i < pending_attachments.size(); i++) {
			const AttachmentInfo &att = pending_attachments[i];
			if (att.thumbnail.is_null()) {
				continue;
			}

			TextureRect *tex_rect = memnew(TextureRect);
			tex_rect->set_texture(att.thumbnail);
			tex_rect->set_custom_minimum_size(Size2(THUMBNAIL_SIZE, THUMBNAIL_SIZE));
			tex_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
			tex_rect->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
			tex_rect->set_tooltip_text(att.filename + "\nDouble-click to view full size");
			tex_rect->set_mouse_filter(Control::MOUSE_FILTER_STOP);

			// Save original image to disk cache, bind cache path for on-demand preview
			String ext = "png";
			if (att.mime_type == "image/jpeg" || att.mime_type == "image/jpg") {
				ext = "jpg";
			} else if (att.mime_type == "image/webp") {
				ext = "webp";
			}
			String cache_path = _save_image_to_cache(att.data, ext);

			if (!cache_path.is_empty()) {
				tex_rect->connect("gui_input", Callable(this, "_on_chat_thumbnail_gui_input").bind(cache_path, att.filename));
			} else if (!att.file_path.is_empty()) {
				// Fallback: open with system viewer if we can't decode
				tex_rect->connect("gui_input", Callable(this, "_on_attachment_gui_input").bind(att.file_path));
			}

			thumb_row->add_child(tex_rect);
		}

		msg_container->add_child(thumb_row);
	}

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

void AIAssistantDock::_show_welcome_message() {
	RichTextLabel *welcome = memnew(RichTextLabel);
	welcome->set_use_bbcode(true);
	welcome->set_fit_content(true);
	welcome->set_selection_enabled(true);

	String msg = "[color=gray]Welcome to RedBlue AI![/color]\n\n";
	msg += "[color=cyan]Start creating:[/color]\n";
	msg += "- \"Create a 2D platformer\" — guided game creation\n";
	msg += "- /create-game — step-by-step workflow\n";
	msg += "- /worldbuilding — build your game world\n\n";
	msg += "[color=cyan]Quick help:[/color]\n";
	msg += "- \"Add a player character\"\n";
	msg += "- \"Fix the enemy AI\"\n";
	msg += "- \"Make the UI look better\"";

	welcome->set_text(msg);
	chat_container->add_child(welcome);
}

void AIAssistantDock::_update_loading_overlay(const String &p_text) {
	if (!loading_overlay || !loading_overlay->is_inside_tree()) {
		// Re-create the overlay (e.g. after _clear_chat_ui destroyed it)
		loading_overlay = memnew(VBoxContainer);
		loading_overlay->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		loading_overlay->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		loading_overlay->set_alignment(BoxContainer::ALIGNMENT_CENTER);

		RichTextLabel *loading_label = memnew(RichTextLabel);
		loading_label->set_use_bbcode(true);
		loading_label->set_fit_content(true);
		loading_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		loading_label->set_name("LoadingLabel");
		loading_overlay->add_child(loading_label);

		ProgressBar *loading_bar = memnew(ProgressBar);
		loading_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		loading_bar->set_custom_minimum_size(Size2(0, 4));
		loading_bar->set_show_percentage(false);
		loading_bar->set_name("LoadingBar");
		loading_bar->set_indeterminate(true);
		loading_overlay->add_child(loading_bar);

		chat_container->add_child(loading_overlay);
	}
	Node *found = loading_overlay->find_child("LoadingLabel", false, false);
	RichTextLabel *label = found ? Object::cast_to<RichTextLabel>(found) : nullptr;
	if (label) {
		label->set_text("[center][color=yellow]" + p_text + "[/color][/center]");
	}
}

void AIAssistantDock::_hide_loading_overlay() {
	if (loading_overlay && loading_overlay->is_inside_tree()) {
		loading_overlay->queue_free();
	}
	loading_overlay = nullptr;
}

void AIAssistantDock::_add_tool_message(const String &p_part_id, const String &p_tool_name, const String &p_status, const Dictionary &p_details) {
	// Update dock title with current tool activity
	if (is_processing) {
		if (p_status == "running") {
			current_tool_name = p_tool_name;
			_update_dock_title();
		} else if (p_status == "completed" || p_status == "error") {
			if (current_tool_name == p_tool_name) {
				current_tool_name = "";
				_update_dock_title();
			}
		}
	}

	// Track start time for new tools
	if (!tool_start_times.has(p_part_id)) {
		tool_start_times[p_part_id] = Time::get_singleton()->get_ticks_msec();
	}

	// Freeze elapsed time when tool completes (so timer stops ticking)
	uint64_t elapsed_ms;
	if (tool_completed_times.has(p_part_id)) {
		elapsed_ms = tool_completed_times[p_part_id];
	} else {
		elapsed_ms = Time::get_singleton()->get_ticks_msec() - tool_start_times[p_part_id];
		if (p_status == "completed" || p_status == "error") {
			tool_completed_times[p_part_id] = elapsed_ms;
		}
	}
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

	// Description (clickable if truncated)
	if (!input_info.is_empty()) {
		String desc = input_info;
		// Truncate at first newline — only show one line
		int nl_pos = desc.find("\n");
		bool truncated = false;
		if (nl_pos >= 0) {
			desc = desc.substr(0, nl_pos);
			truncated = true;
		}
		if (desc.length() > 120) {
			desc = desc.substr(0, 120);
			truncated = true;
		}
		if (truncated) {
			desc += "...";
		}
		if (truncated) {
			formatted += "  [url=tool://" + p_part_id + "/input][color=#aaaaaa]" + desc + "[/color][/url]";
		} else {
			formatted += "  [color=#aaaaaa]" + desc + "[/color]";
		}
	}

	// Status + time
	formatted += "  [color=#" + status_color + "]" + status_icon + "[/color] [color=#888888](" + time_str + ")[/color]";

	// --- TodoWrite: render checklist inline ---
	if (p_tool_name == "todowrite" && in.has("todos")) {
		Variant todos_var = in["todos"];
		if (todos_var.get_type() == Variant::ARRAY) {
			Array todos = todos_var;
			for (int ti = 0; ti < todos.size(); ti++) {
				Dictionary todo = todos[ti];
				String todo_status = todo.get("status", "");
				String todo_content = todo.get("content", "");
				String todo_active = todo.get("activeForm", todo_content);

				String icon;
				String color;
				String label;
				if (todo_status == "completed") {
					icon = String::utf8("\u2713"); // checkmark
					color = "66ff66";
					label = todo_content;
				} else if (todo_status == "in_progress") {
					icon = String::utf8("\u25B6"); // play triangle
					color = "ffcc44";
					label = todo_active;
				} else {
					icon = String::utf8("\u25CB"); // circle
					color = "888888";
					label = todo_content;
				}
				formatted += "\n  [color=#" + color + "]" + icon + "[/color] " + label;
			}
		}
	}

	// --- Task tool: subagent tools (same 2-line format, indented with │) ---
	if (p_tool_name == "task" && p_details.has("metadata")) {
		Dictionary task_metadata = p_details["metadata"];
		if (task_metadata.has("summary")) {
			Array summary = task_metadata["summary"];
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
	if (p_status == "completed") {
	}
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
			tool_label->clear();
			tool_label->append_text(formatted);
			tool_label->update_minimum_size();
		}
	} else {
		// Create a new tool message container
		VBoxContainer *container = memnew(VBoxContainer);

		RichTextLabel *tool_label = memnew(RichTextLabel);
		tool_label->set_use_bbcode(true);
		tool_label->set_fit_content(true);
		tool_label->append_text(formatted);
		tool_label->set_selection_enabled(true);
		tool_label->set_context_menu_enabled(true);
		tool_label->set_focus_mode(Control::FOCUS_CLICK);
		tool_label->connect("meta_clicked", Callable(this, "_on_tool_meta_clicked"));
		container->add_child(tool_label);

		tool_containers[p_part_id] = tool_label;
		tool_vbox_containers[p_part_id] = container;
		chat_container->add_child(container);
	}

	// --- Show image thumbnails for tool attachments (screenshots, recordings) ---
	if (p_status == "completed" && !tool_images_added.has(p_part_id) && p_details.has("attachments")) {
		Variant att_var = p_details["attachments"];
		if (att_var.get_type() == Variant::ARRAY) {
			Array attachments = att_var;
			if (!attachments.is_empty() && tool_vbox_containers.has(p_part_id)) {
				VBoxContainer *container = tool_vbox_containers[p_part_id];

				// Insert thumbnail row before the HSeparator (last child)
				HBoxContainer *thumb_row = memnew(HBoxContainer);
				thumb_row->add_theme_constant_override("separation", 4);

				for (int ai = 0; ai < attachments.size(); ai++) {
					Dictionary att = attachments[ai];
					String mime = att.get("mime", "");
					String url = att.get("url", "");

					if (!mime.begins_with("image/") || url.is_empty()) {
						continue;
					}

					// Extract base64 data from data URL: "data:image/png;base64,XXXX"
					int comma_pos = url.find(",");
					if (comma_pos < 0) {
						continue;
					}
					String b64_data = url.substr(comma_pos + 1);
					CharString b64_utf8 = b64_data.utf8();
					size_t b64_len = b64_utf8.length();
					size_t max_decoded_len = b64_len / 4 * 3 + 4;
					Vector<uint8_t> raw;
					raw.resize(max_decoded_len);
					size_t decoded_len = 0;
					Error decode_err = CryptoCore::b64_decode(raw.ptrw(), max_decoded_len, &decoded_len, (const uint8_t *)b64_utf8.get_data(), b64_len);
					if (decode_err != OK || decoded_len == 0) {
						continue;
					}
					raw.resize(decoded_len);

					// Save to disk cache
					String ext = "png";
					if (mime == "image/jpeg" || mime == "image/jpg") {
						ext = "jpg";
					} else if (mime == "image/webp") {
						ext = "webp";
					}
					String cache_path = _save_image_to_cache(raw, ext);
					if (cache_path.is_empty()) {
						continue;
					}

					// Decode image just for thumbnail (small memory footprint)
					Ref<Image> img;
					img.instantiate();
					if (mime == "image/png") {
						img->load_png_from_buffer(raw);
					} else if (mime == "image/jpeg" || mime == "image/jpg") {
						img->load_jpg_from_buffer(raw);
					} else if (mime == "image/webp") {
						img->load_webp_from_buffer(raw);
					} else if (mime == "image/gif") {
						img->load_png_from_buffer(raw); // fallback
					}

					if (img->is_empty()) {
						continue;
					}

					// Create thumbnail only — full image stays on disk
					int tw = img->get_width();
					int th = img->get_height();
					int thumb_size = THUMBNAIL_SIZE * 2; // Larger thumbnails for tool output
					if (tw > th) {
						th = MAX(1, th * thumb_size / tw);
						tw = thumb_size;
					} else {
						tw = MAX(1, tw * thumb_size / th);
						th = thumb_size;
					}
					img->resize(tw, th);
					Ref<ImageTexture> thumb_tex = ImageTexture::create_from_image(img);

					TextureRect *tex_rect = memnew(TextureRect);
					tex_rect->set_texture(thumb_tex);
					tex_rect->set_custom_minimum_size(Size2(tw, th));
					tex_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
					tex_rect->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
					tex_rect->set_tooltip_text(p_tool_name + " output\nDouble-click to view full size");
					tex_rect->set_mouse_filter(Control::MOUSE_FILTER_STOP);
					tex_rect->connect("gui_input", Callable(this, "_on_chat_thumbnail_gui_input").bind(cache_path, p_tool_name + " #" + itos(ai + 1)));

					thumb_row->add_child(tex_rect);
				}

				if (thumb_row->get_child_count() > 0) {
					container->add_child(thumb_row);
					tool_images_added.insert(p_part_id);
					_scroll_chat_to_bottom();
				} else {
					memdelete(thumb_row);
				}
			}
		}
	}
}

void AIAssistantDock::_clear_tool_tracking() {
	tool_containers.clear();
	tool_vbox_containers.clear();
	tool_images_added.clear();
	tool_start_times.clear();
	tool_completed_times.clear();
	tool_logged_status.clear();
	text_stream_labels.clear();
	reasoning_labels.clear();
	reasoning_containers.clear();
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

	// Handle compaction summary view link
	if (meta == "compaction://view") {
		if (!compaction_summary_text.is_empty()) {
			tool_detail_dialog->set_title("Context Summary");
			tool_detail_content->set_text(compaction_summary_text);
			tool_detail_dialog->popup_centered_ratio(0.6);
		}
		return;
	}

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

void AIAssistantDock::_update_dock_title() {
	if (!is_processing) {
		// Idle: clear color (transparent = default)
		set_title_color(Color(0, 0, 0, 0));
		return;
	}

	// Pulse alpha between 0.5 and 1.0 using processing_dots as phase
	float alpha = 0.6f + 0.4f * Math::sin(processing_dots * Math::PI * 0.5f);

	if (!current_tool_name.is_empty()) {
		// Tool running: orange
		set_title_color(Color(1.0, 0.7, 0.2, alpha));
	} else {
		// Thinking: blue
		set_title_color(Color(0.4, 0.7, 1.0, alpha));
	}
}

void AIAssistantDock::_show_processing() {
	is_processing = true;
	current_tool_name = "";
	if (processing_label) {
		processing_label->set_visible(true);
		processing_dots = 0;
		processing_label->set_text("AI is thinking");
		processing_timer->start();
	}
	if (send_button) {
		send_button->set_text("Stop");
		send_button->set_disabled(false);
		send_button->add_theme_color_override("font_color", Color(1.0, 0.4, 0.4));
	}
	_update_dock_title();
}

void AIAssistantDock::_hide_processing() {
	is_processing = false;
	current_tool_name = "";
	if (processing_label) {
		processing_label->set_visible(false);
		processing_timer->stop();
		processing_dots = 0;
	}
	if (send_button) {
		send_button->set_text("Send (Enter)");
		send_button->set_disabled(false);
		send_button->remove_theme_color_override("font_color");
	}
	// Stop stream polling when processing is done
	_stop_stream_polling();
	_update_dock_title();
}

void AIAssistantDock::_on_processing_timer_timeout() {
	processing_dots = (processing_dots + 1) % 4;
	String dots = "";
	for (int i = 0; i < processing_dots; i++) {
		dots += ".";
	}
	processing_label->set_text("AI is thinking" + dots);
	_update_dock_title();
}

// === Stream Polling (for intermediate AI steps) ===

void AIAssistantDock::_start_stream_polling(const String &p_message_id) {
	// p_message_id can be empty - we poll messages list to find the latest assistant message
	current_message_id = p_message_id;
	last_part_count = 0;
	stream_empty_poll_count = 0;
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

	String response_text = _body_to_string(p_body);
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

	// Update tool states from ALL assistant messages (not just the latest).
	// This ensures tools from previous messages get their "completed" state
	// even after the dock has moved on to processing a newer message.
	for (int mi = 0; mi < messages.size(); mi++) {
		Dictionary msg = messages[mi];
		if (!msg.has("info") || !msg.has("parts")) continue;
		Dictionary msg_info = msg["info"];
		if (String(msg_info.get("role", "")) != "assistant") continue;
		Array msg_parts = msg["parts"];
		for (int pi = 0; pi < msg_parts.size(); pi++) {
			Dictionary part = msg_parts[pi];
			if (String(part.get("type", "")) != "tool") continue;
			String part_id = part.get("id", "");
			String tool_name = part.get("tool", "unknown");
			if (tool_name == "question") continue;
			Dictionary state = part.get("state", Dictionary());
			String status = state.get("status", "unknown");
			if (status == "pending") continue;
			// Skip tools already finalized (completed/error) — no need to re-process
			if (tool_completed_times.has(part_id) && tool_images_added.has(part_id)) continue;
			// Only update if we already have a container for this tool (don't create new ones here)
			if (tool_containers.has(part_id)) {
				_add_tool_message(part_id, tool_name, status, state);
			}
		}
	}

	// Find the latest assistant message, skipping any aborted/stale message.
	Dictionary latest_assistant;
	for (int i = messages.size() - 1; i >= 0; i--) {
		Dictionary msg = messages[i];
		if (msg.has("info")) {
			Dictionary info = msg["info"];
			String role = info.get("role", "");
			if (role == "assistant") {
				String msg_id = info.get("id", "");
				bool is_summary = info.get("summary", false);

				// Skip explicitly ignored message (set by _on_stop_pressed)
				if (!ignore_assistant_message_id.is_empty() && msg_id == ignore_assistant_message_id) {
					continue;
				}

				// Skip compaction summary messages — we already displayed the compact indicator
				// and are now waiting for the real response.
				if (is_summary) {
					// Remember this as the compaction summary for the "Context compacted" UI
					if (compaction_summary_message_id.is_empty()) {
						compaction_summary_message_id = msg_id;
					}
					continue;
				}

				// Also skip any completed message that has an AbortedError — it was
				// cancelled and should not be treated as a valid response.
				if (info.has("error")) {
					Dictionary err = info["error"];
					String err_name = err.get("name", "");
					if (err_name == "MessageAbortedError") {
						// Remember this ID so we keep skipping it on future polls
						ignore_assistant_message_id = msg_id;
						continue;
					}
				}

				latest_assistant = msg;
				break;
			}
		}
	}

	// Found a new assistant message — clear the ignore/skip filters
	if (!latest_assistant.is_empty()) {
		if (!ignore_assistant_message_id.is_empty()) {
			ignore_assistant_message_id = "";
		}
		if (!compaction_summary_message_id.is_empty()) {
			compaction_summary_message_id = "";
		}
	}

	if (latest_assistant.is_empty() || !latest_assistant.has("parts")) {
		// No assistant message found yet — AI may still be thinking.
		// After ~120s of empty polls (1200 × 100ms), stop polling but keep the session.
		stream_empty_poll_count++;
		if (stream_empty_poll_count > 1200) {
			print_line("[AIAssistant] No assistant response after 120s of polling. Stopping poll (session preserved).");
			_stop_stream_polling();
			_hide_processing();
			pending_request = REQUEST_NONE;
			_add_system_message("AI response timed out. Your session is preserved — try sending your message again.");
			_update_status("Connected", Color(0, 1, 0));
		}
		return;
	}

	// Got an assistant message — reset empty poll counter
	stream_empty_poll_count = 0;

	// Track message ID to detect if it changed
	Dictionary info = latest_assistant["info"];
	String msg_id = info.get("id", "");
	bool is_compaction_summary = info.get("summary", false);
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
					if (tool_name == "write" || tool_name == "edit" || tool_name == "bash" || tool_name == "create_file" ||
							tool_name.begins_with("godot_")) {
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

			// Compaction summaries: store text but don't render in chat
			if (is_compaction_summary) {
				compaction_summary_text = text;
				compaction_summary_message_id = msg_id;
				continue;
			}

			String text_key = part_id + "_text_stream";
			if (text_stream_labels.has(text_key)) {
				// Update existing streaming text label
				RichTextLabel *text_label = text_stream_labels[text_key];
				if (text_label) {
					String formatted = "[color=#66ff99][b]AI:[/b][/color] " + text;
					text_label->set_text(formatted);
					// Trigger relayout so ScrollContainer picks up the new height.
					text_label->update_minimum_size();
					_scroll_chat_to_bottom();
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
				chat_container->add_child(msg_container);
				text_stream_labels[text_key] = text_label;

				// Log first appearance
				String preview = text.substr(0, 150);
				if (text.length() > 150) {
					preview += "...";
				}
				_add_log_entry("LLM", preview, Color(0.5, 0.9, 0.5));
			}
		} else if (type == "reasoning") {
			String part_id = part.get("id", "");
			String text = String(part.get("text", "")).strip_edges();
			if (text.is_empty()) {
				continue;
			}

			String reasoning_key = part_id + "_reasoning";
			if (reasoning_labels.has(reasoning_key)) {
				// Update existing reasoning label
				RichTextLabel *label = reasoning_labels[reasoning_key];
				if (label) {
					label->set_text(text);
					label->update_minimum_size();
					_scroll_chat_to_bottom();
				}
			} else {
				// Create collapsible reasoning block
				VBoxContainer *reasoning_block = memnew(VBoxContainer);

				// Toggle button
				Button *toggle_btn = memnew(Button);
				toggle_btn->set_text(String::utf8("\xf0\x9f\x92\xad") + " Thinking...");
				toggle_btn->set_flat(true);
				toggle_btn->add_theme_color_override("font_color", Color(0.5, 0.5, 0.6));
				toggle_btn->add_theme_font_size_override("font_size", 12);
				toggle_btn->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
				toggle_btn->connect("pressed", Callable(this, "_toggle_reasoning_collapse").bind(reasoning_key));
				reasoning_block->add_child(toggle_btn);

				// Content label (initially visible, will collapse on click)
				RichTextLabel *label = memnew(RichTextLabel);
				label->set_use_bbcode(true);
				label->set_fit_content(true);
				label->set_text(text);
				label->set_selection_enabled(true);
				label->set_context_menu_enabled(true);
				label->set_focus_mode(Control::FOCUS_CLICK);
				label->add_theme_color_override("default_color", Color(0.55, 0.55, 0.65));
				label->add_theme_font_size_override("normal_font_size", 12);
				reasoning_block->add_child(label);

				chat_container->add_child(reasoning_block);
				reasoning_labels[reasoning_key] = label;
				reasoning_containers[reasoning_key] = reasoning_block;
				_scroll_chat_to_bottom();
			}
		}
	}
	last_part_count = current_count;

	// Check if message is complete (time.completed exists in the info)
	Dictionary time_info = info.get("time", Dictionary());
	if (time_info.has("completed")) {
		// Compaction summary completed — show compact indicator and keep polling for the real response
		if (is_compaction_summary) {
			// Store the final summary text
			for (int i = 0; i < parts.size(); i++) {
				Dictionary part = parts[i];
				if (String(part.get("type", "")) == "text") {
					String text = String(part.get("text", "")).strip_edges();
					if (!text.is_empty()) {
						compaction_summary_text = text;
					}
				}
			}

			// Show compact clickable indicator in chat
			VBoxContainer *msg_container = memnew(VBoxContainer);
			RichTextLabel *label = memnew(RichTextLabel);
			label->set_use_bbcode(true);
			label->set_fit_content(true);
			label->set_text("[color=#aaaaaa][i]Context compacted[/i][/color]  [url=compaction://view][color=#6699cc]View summary[/color][/url]");
			label->set_selection_enabled(true);
			label->connect("meta_clicked", Callable(this, "_on_tool_meta_clicked"));
			msg_container->add_child(label);
			chat_container->add_child(msg_container);

			_add_log_entry("INFO", "Context compacted — continuing with fresh context", Color(0.8, 0.8, 0.5));

			// Don't stop polling — the server will produce a real response next.
			// Reset message tracking so we pick up the next assistant message.
			current_message_id = "";
			last_part_count = 0;
			return;
		}

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
			} else if (type == "reasoning") {
				// Finalize reasoning: update toggle button to "Thought" and collapse
				String part_id = part.get("id", "");
				String reasoning_key = part_id + "_reasoning";
				if (reasoning_containers.has(reasoning_key)) {
					VBoxContainer *container = reasoning_containers[reasoning_key];
					if (container && container->get_child_count() > 0) {
						Button *btn = Object::cast_to<Button>(container->get_child(0));
						if (btn) {
							btn->set_text(String::utf8("\xf0\x9f\x92\xad") + " Thought (click to expand)");
						}
					}
					// Auto-collapse reasoning when done
					if (reasoning_labels.has(reasoning_key)) {
						reasoning_labels[reasoning_key]->set_visible(false);
					}
				}
			}
		}

		// Check for errors — show in chat so user can see them
		if (info.has("error")) {
			Dictionary error = info["error"];
			String error_name = error.get("name", "unknown");
			// The actual message is nested in error.data.message
			String error_msg;
			if (error.has("data") && Dictionary(error["data"]).has("message")) {
				Dictionary error_data = error["data"];
				error_msg = String(error_data["message"]);
			} else {
				error_msg = error.get("message", "An error occurred");
			}
			// Show error in chat (red) so user sees it, not just in logs
			_add_message("Error", error_name + ": " + error_msg, Color(1.0, 0.4, 0.4));
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

	String response_text = _body_to_string(p_body);
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

	String response_text = _body_to_string(p_body);
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
	} else if (p_action == "eval") {
		// Evaluate a GDScript expression in the running game via debug protocol
		String eval_id = p_params.get("id", "");
		String expression = p_params.get("expression", "");
		if (eval_id.is_empty() || expression.is_empty()) {
			return;
		}

		if (!EditorRunBar::get_singleton()->is_playing()) {
			_post_eval_result(eval_id, "", "No game running — start the game first.");
			return;
		}

		ScriptEditorDebugger *debugger = EditorDebuggerNode::get_singleton()->get_current_debugger();
		if (!debugger || !debugger->is_session_active()) {
			_post_eval_result(eval_id, "", "No active debugger session.");
			return;
		}

		// Send via custom "ai:eval" protocol (works while game is running, no breakpoint needed)
		debugger->request_ai_eval(expression, eval_id,
				callable_mp_static(&AIAssistantDock::_on_ai_eval_return_static).bind(eval_id));
		_add_system_message("[Sim] Evaluating: " + expression);
	} else if (p_action == "record") {
		// Start a GIF recording programmatically from OpenCode
		String record_id = p_params.get("id", "");
		int duration_ms = p_params.get("duration_ms", 3000);
		int fps = p_params.get("fps", 8);

		if (record_id.is_empty()) {
			return;
		}

		if (!EditorRunBar::get_singleton()->is_playing()) {
			// Post empty result to unblock the tool
			Vector<String> empty_frames;
			_post_gif_result(record_id, empty_frames);
			return;
		}

		// Configure and start recording
		gif_record_fps = fps;
		gif_max_frames = (duration_ms / 1000) * fps + fps; // duration + 1s buffer
		gif_frame_timer->set_wait_time(1.0 / fps);
		gif_record_id = record_id;
		gif_frames.clear();
		gif_recording = true;
		gif_record_from_tool = true; // Don't attach to input panel — tool will poll server
		gif_record_button->set_pressed_no_signal(true);
		gif_frame_timer->start();
		_add_system_message("[GIF] Recording " + itos(duration_ms) + "ms at " + itos(fps) + "fps...");

		// Auto-stop after duration_ms (toggle will no-op if already stopped)
		SceneTree *tree = get_tree();
		if (tree) {
			tree->create_timer(duration_ms / 1000.0)->connect("timeout",
					callable_mp(this, &AIAssistantDock::_stop_gif_recording_if_active));
		}
	} else if (p_action == "get_logs") {
		// Read editor output logs and debugger error/warning counts for AI analysis
		String log_id = p_params.get("id", "");
		int max_lines = p_params.get("lines", 50);
		if (log_id.is_empty()) {
			return;
		}

		// 1. Read Output panel logs from EditorLog
		String content;
		int line_count = 0;
		EditorLog *editor_log = EditorNode::get_log();
		if (editor_log) {
			content = editor_log->get_recent_log_text(max_lines);
			// Count non-empty lines
			Vector<String> lines = content.split("\n");
			for (int i = 0; i < lines.size(); i++) {
				if (!lines[i].is_empty()) {
					line_count++;
				}
			}
		}

		// 2. Append debugger errors/warnings with details
		ScriptEditorDebugger *debugger = EditorDebuggerNode::get_singleton()->get_current_debugger();
		if (debugger) {
			int err_count = debugger->get_error_count();
			int warn_count = debugger->get_warning_count();
			content += "\n--- Debugger Summary ---\n";
			content += "Errors: " + itos(err_count) + "\n";
			content += "Warnings: " + itos(warn_count) + "\n";
			if (err_count > 0 || warn_count > 0) {
				String error_text = debugger->get_error_text(max_lines);
				if (!error_text.is_empty()) {
					content += "\n--- Debugger Details ---\n";
					content += error_text;
				}
			}
		}

		_post_log_result(log_id, content, line_count);
		_add_system_message("[Logs] Sent " + itos(line_count) + " log lines to AI.");
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


bool AIAssistantDock::_add_attachment_from_raw_data(const String &p_filename, const String &p_mime, const Vector<uint8_t> &p_data, const String &p_file_path) {
	if (p_data.is_empty()) {
		return false;
	}

	// Load image to generate thumbnail
	Ref<Image> img;
	img.instantiate();
	Error err;
	if (p_mime == "image/gif") {
		// GIF: use first frame from gif_frames for thumbnail (if available)
		if (!gif_frames.is_empty()) {
			Ref<FileAccess> ff = FileAccess::open(gif_frames[0], FileAccess::READ);
			if (ff.is_valid()) {
				Vector<uint8_t> png_buf;
				png_buf.resize(ff->get_length());
				ff->get_buffer(png_buf.ptrw(), png_buf.size());
				ff.unref();
				err = img->load_png_from_buffer(png_buf);
			}
		}
		if (img->is_empty()) {
			// Create a small placeholder
			img->initialize_data(64, 64, false, Image::FORMAT_RGBA8);
			img->fill(Color(0.3, 0.3, 0.3, 1.0));
		}
	} else {
		err = img->load_png_from_buffer(p_data);
		if (err != OK || img->is_empty()) {
			return false;
		}
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
	att.file_path = p_file_path;
	// If no file path, save to temp file so double-click can open it.
	if (att.file_path.is_empty()) {
		String temp_dir = OS::get_singleton()->get_user_data_dir() + "/tmp";
		Ref<DirAccess> da = DirAccess::open(OS::get_singleton()->get_user_data_dir());
		if (da.is_valid() && !da->dir_exists("tmp")) {
			da->make_dir("tmp");
		}
		String temp_path = temp_dir + "/" + p_filename;
		Ref<FileAccess> f = FileAccess::open(temp_path, FileAccess::WRITE);
		if (f.is_valid()) {
			f->store_buffer(p_data.ptr(), p_data.size());
			f.unref();
			att.file_path = temp_path;
		}
	}
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

// === Eval Result ===

void AIAssistantDock::_on_ai_eval_return_static(const Array &p_data, const String &p_id) {
	if (singleton) {
		singleton->_on_ai_eval_return(p_data, p_id);
	}
}

void AIAssistantDock::_on_ai_eval_return(const Array &p_data, const String &p_id) {
	// p_data format from ai:eval_return: [eval_id, value_string, error_string]
	if (p_data.size() >= 3) {
		String value = p_data[1];
		String error = p_data[2];
		if (!error.is_empty()) {
			_post_eval_result(p_id, "", error);
		} else {
			_post_eval_result(p_id, value);
		}
	} else {
		_post_eval_result(p_id, "", "Invalid eval return data");
	}
}

void AIAssistantDock::_post_log_result(const String &p_id, const String &p_content, int p_line_count) {
	String url = service_url + "/godot/log-result";

	Dictionary body;
	body["id"] = p_id;
	body["content"] = p_content;
	body["lineCount"] = p_line_count;
	String json_body = JSON::stringify(body);

	HTTPRequest *req = memnew(HTTPRequest);
	add_child(req);
	req->request(url, _get_headers_with_directory(), HTTPClient::METHOD_POST, json_body);
	req->connect("request_completed", callable_mp((Node *)req, &Node::queue_free).unbind(4));
}

void AIAssistantDock::_post_eval_result(const String &p_id, const String &p_value, const String &p_error) {
	String url = service_url + "/godot/eval-result";

	Dictionary body;
	body["id"] = p_id;
	body["value"] = p_value;
	if (!p_error.is_empty()) {
		body["error"] = p_error;
	}
	String json_body = JSON::stringify(body);

	HTTPRequest *req = memnew(HTTPRequest);
	add_child(req);
	req->request(url, _get_headers_with_directory(), HTTPClient::METHOD_POST, json_body);
	req->connect("request_completed", callable_mp((Node *)req, &Node::queue_free).unbind(4));
}

// === GIF Recording ===

void AIAssistantDock::_toggle_gif_recording() {
	if (!gif_recording) {
		// Start recording
		if (!EditorRunBar::get_singleton()->is_playing()) {
			_add_system_message("[GIF] Cannot record — game is not running. Start the game first.");
			gif_record_button->set_pressed_no_signal(false);
			return;
		}
		gif_recording = true;
		gif_record_from_tool = false; // Manual recording — attach to input panel
		gif_frames.clear();
		gif_record_id = String::num_int64(OS::get_singleton()->get_ticks_msec());
		gif_record_button->set_pressed_no_signal(true);
		gif_record_button->set_text(String::utf8("\xe2\x8f\xba REC")); // ⏺ REC
		gif_record_button->add_theme_color_override("font_color", Color(1.0, 0.2, 0.2, 1.0));
		gif_frame_timer->start();
		_add_system_message("[GIF] Recording... auto-stops after " + itos(gif_max_frames / gif_record_fps) + "s.");
	} else {
		// Stop recording
		gif_recording = false;
		gif_frame_timer->stop();
		gif_record_button->set_pressed_no_signal(false);
		gif_record_button->set_text(U"\U0001F3AC"); // 🎬
		gif_record_button->remove_theme_color_override("font_color");

		if (gif_frames.size() == 0) {
			_add_system_message("[GIF] Recording stopped — no frames captured.");
			return;
		}

		print_line("[GIF] Posting " + itos(gif_frames.size()) + " frames to " + service_url + "/godot/record-result");
		_add_system_message("[GIF] Stopped (" + itos(gif_frames.size()) + " frames). Sending...");
		_post_gif_result(gif_record_id, gif_frames);
		// Don't clear gif_frames here — _on_gif_post_completed needs them for thumbnail
	}
}

void AIAssistantDock::_stop_gif_recording_if_active() {
	if (gif_recording) {
		_toggle_gif_recording(); // Stop
	}
}

void AIAssistantDock::_on_gif_frame_timer() {
	if (!gif_recording) {
		return;
	}

	// Auto-stop if game stopped or max frames reached
	if (!EditorRunBar::get_singleton()->is_playing() || gif_frames.size() >= gif_max_frames) {
		_toggle_gif_recording(); // Stop
		return;
	}

	// Request a screenshot for this frame
	bool ok = EditorRunBar::get_singleton()->request_screenshot(
			callable_mp_static(&AIAssistantDock::_gif_frame_screenshot_static));
	if (!ok) {
		print_line("[GIF] request_screenshot returned false");
	}
}

void AIAssistantDock::_gif_frame_screenshot_static(int64_t p_w, int64_t p_h, const String &p_path, const Rect2i &p_rect) {
	if (singleton) {
		singleton->_on_gif_frame_screenshot(p_w, p_h, p_path, p_rect);
	}
}

void AIAssistantDock::_on_gif_frame_screenshot(int64_t p_w, int64_t p_h, const String &p_path, const Rect2i &p_rect) {
	if (!gif_recording || p_path.is_empty()) {
		return;
	}
	// Store the temp file path — we'll read them when recording stops
	gif_frames.push_back(p_path);
}

void AIAssistantDock::_post_gif_result(const String &p_id, const Vector<String> &p_frames) {
	String url = service_url + "/godot/record-result";

	// Send file paths instead of base64 data (avoids large HTTP body)
	Dictionary body;
	body["id"] = p_id;
	body["fps"] = gif_record_fps;
	Array paths_array;
	for (int i = 0; i < p_frames.size(); i++) {
		paths_array.push_back(p_frames[i]);
	}
	body["framePaths"] = paths_array;
	String json_body = JSON::stringify(body);

	print_line("[GIF] POST " + itos(p_frames.size()) + " frame paths (" + itos(json_body.length()) + " bytes)");

	HTTPRequest *req = memnew(HTTPRequest);
	add_child(req);

	gif_post_req = req;
	req->connect("request_completed", Callable(this, "_on_gif_post_completed"));
	Error err = req->request(url, _get_headers_with_directory(), HTTPClient::METHOD_POST, json_body);
	if (err != OK) {
		print_line("[GIF] POST failed to send, error: " + itos(err));
		_add_system_message("[GIF] Failed to send recording.");
		req->queue_free();
	}
}

void AIAssistantDock::_on_gif_post_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	print_line("[GIF] POST completed: result=" + itos(p_result) + " code=" + itos(p_code) + " body_size=" + itos(p_body.size()));
	if (gif_post_req) {
		gif_post_req->queue_free();
		gif_post_req = nullptr;
	}
	if (p_code != 200 || p_body.size() == 0) {
		print_line("[GIF] Upload failed: HTTP " + itos(p_code));
		_add_system_message("[GIF] Upload failed (HTTP " + itos(p_code) + ").");
		return;
	}

	// Parse response to get gifPath
	String body_str = String::utf8((const char *)p_body.ptr(), p_body.size());
	print_line("[GIF] Response body: " + body_str);
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(body_str) != OK) {
		print_line("[GIF] JSON parse failed");
		_add_system_message("[GIF] Failed to parse server response.");
		return;
	}

	Dictionary result = json->get_data();
	String gif_path = result.get("gifPath", "");
	int frame_count = result.get("frameCount", 0);
	print_line("[GIF] gifPath=" + gif_path + " frameCount=" + itos(frame_count));

	if (gif_path.is_empty()) {
		String gif_error = result.get("gifError", "unknown error");
		print_line("[GIF] No gifPath, error: " + gif_error);
		_add_system_message("[GIF] Server couldn't encode GIF: " + gif_error);
		return;
	}

	if (gif_record_from_tool) {
		// Tool-triggered recording — don't attach to input panel, the tool polls the server
		_add_system_message("[GIF] Recording complete (" + itos(frame_count) + " frames). AI tool will process it.");
	} else {
		// Manual recording — attach GIF to input panel
		Ref<FileAccess> f = FileAccess::open(gif_path, FileAccess::READ);
		if (f.is_null()) {
			print_line("[GIF] Cannot open file: " + gif_path);
			_add_system_message("[GIF] Cannot read GIF file: " + gif_path);
			gif_frames.clear();
			return;
		}

		Vector<uint8_t> gif_data;
		gif_data.resize(f->get_length());
		f->get_buffer(gif_data.ptrw(), gif_data.size());
		f.unref();

		if (_add_attachment_from_raw_data("recording.gif", "image/gif", gif_data, gif_path)) {
			_add_system_message("[GIF] Recording attached (" + itos(frame_count) + " frames). Double-click thumbnail to preview. Send a message to analyze it.");
			if (prompt_input->get_text().is_empty()) {
				prompt_input->set_text("Analyze this gameplay recording and identify any visual or gameplay issues.");
			}
			prompt_input->grab_focus();
		} else {
			_add_system_message("[GIF] Failed to attach GIF file.");
		}
	}
	gif_frames.clear(); // Clean up frame paths now that we're done
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

void AIAssistantDock::_on_game_stopped() {
	// Game stopped — no action needed. Error monitoring during auto-test is handled by OpenCode.
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
		// Re-enable input and clear "Connecting..." text.
		prompt_input->set_editable(true);
		prompt_input->set_placeholder(TTR("Type a message..."));
		send_button->set_disabled(false);
		print_line("[AIAssistant] CLEAR_SITE_C: restored session (initial_session_id)");
		_clear_chat_ui();
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
	_update_loading_overlay("Finding session...");

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

	String response_text = _body_to_string(p_body);
	JSON json;
	Error err = json.parse(response_text);

	if (err != OK) {
		_add_system_message("Invalid session list response. Creating new session...");
		_create_session();
		return;
	}

	Variant parsed_data = json.get_data();
	if (parsed_data.get_type() != Variant::ARRAY) {
		_add_system_message("Unexpected session list format. Creating new session...");
		_create_session();
		return;
	}

	Array sessions = parsed_data;
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
	engine_prompts_injected = false;
	String session_title = session.get("title", "Untitled");

	connection_status = CONNECTED;
	_update_status("Connected", Color(0, 1, 0));
	_update_connection_indicator();

	// Re-enable input (was disabled during connecting).
	prompt_input->set_editable(true);
	prompt_input->set_placeholder(TTR("Type a message..."));
	send_button->set_disabled(false);

	// Clear "Connecting..." text before loading session history.
	print_line("[AIAssistant] CLEAR_SITE_D: found existing session from session list");
	_clear_chat_ui();

	session_history_button->set_text(session_title.length() > 25 ? session_title.substr(0, 22) + "..." : session_title);

	_add_system_message("Resumed session: " + session_title);
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

	_update_loading_overlay("Loading chat history...");

	// Use a separate HTTP request for session history to avoid blocking other requests
	// We'll create a temporary HTTPRequest for this
	HTTPRequest *history_request = memnew(HTTPRequest);
	add_child(history_request);
	history_request->connect("request_completed", Callable(this, "_on_session_history_completed"));

	String url = service_url + "/session/" + session_id + "/message?directory=" + _get_project_directory().uri_encode();
	history_request->request(url, _get_headers_with_directory());
}

void AIAssistantDock::_on_session_history_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	// Hide loading overlay — history loading is done (success or failure)
	_hide_loading_overlay();

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

	String response_text = _body_to_string(p_body);
	JSON json;
	Error err = json.parse(response_text);

	if (err != OK) {
		_add_system_message("[History] Invalid history response.");
		return;
	}

	Variant parsed_data = json.get_data();
	if (parsed_data.get_type() != Variant::ARRAY) {
		_add_system_message("[History] Unexpected history format.");
		return;
	}

	Array messages = parsed_data;
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
			// Extract text from user message parts, skipping injected system content
			for (int j = 0; j < parts.size(); j++) {
				Dictionary part = parts[j];
				String type = part.get("type", "");
				if (type == "text") {
					String text = part.get("text", "");
					if (text.is_empty()) {
						continue;
					}
					// Skip injected system parts (engine prompts, project context, skill instructions)
					if (text.begins_with("[IMPORTANT RULES]") ||
							text.begins_with("[PROJECT CONTEXT") ||
							text.begins_with("<skill-instructions")) {
						continue;
					}
					_add_user_message(text);
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

	// Force scroll to bottom for the next ~3 seconds (180 frames at 60fps).
	// Layout of many history nodes takes many frames. Each process tick, we
	// set_v_scroll to max, which ensures we end up at the bottom regardless
	// of how long layout takes.
	user_scrolled_up = false;
	suppress_scroll_tracking = true;
	scroll_to_bottom_frames = 180;
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
		body_str = _body_to_string(p_body);
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

			// Store capabilities for wizard use
			if (resp.has("capabilities")) {
				wizard_capabilities = resp["capabilities"];
			}

			// Build connected set and store for wizard use.
			HashSet<String> connected_set;
			for (int i = 0; i < connected.size(); i++) {
				connected_set.insert(String(connected[i]));
			}
			server_connected_providers = connected_set;

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
			// Prefer Anthropic Opus, then any Anthropic model, then fall back to first connected.
			if (selected_model_id.is_empty()) {
				// First pass: look for Anthropic Opus model.
				for (int i2 = 0; i2 < providers.size(); i2++) {
					if (providers[i2].id == "anthropic" && providers[i2].connected) {
						for (int m = 0; m < providers[i2].models.size(); m++) {
							if (providers[i2].models[m].id.contains("opus")) {
								selected_provider_id = "anthropic";
								selected_model_id = providers[i2].models[m].id;
								break;
							}
						}
						// If no Opus found, use the Anthropic default.
						if (selected_model_id.is_empty() && defaults.has("anthropic")) {
							selected_provider_id = "anthropic";
							selected_model_id = String(defaults["anthropic"]);
						}
						break;
					}
				}
				// Fall back to first connected provider's default.
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
			}

			print_line("[AIAssistant] Parsed " + itos(providers.size()) + " providers, " + itos(connected_set.size()) + " connected");

			_populate_model_menu();
			_update_model_button_text();

			int total_models = 0;
			for (int i = 0; i < providers.size(); i++) {
				total_models += providers[i].models.size();
			}
			_add_log_entry("INFO", "Loaded " + itos(total_models) + " models from " + itos(providers.size()) + " provider(s).", Color(0.7, 0.7, 0.7));

			// Guide the user when no providers are authenticated — show in chat.
			if (connected_set.is_empty()) {
				RichTextLabel *guide = memnew(RichTextLabel);
				guide->set_use_bbcode(true);
				guide->set_fit_content(true);
				guide->set_selection_enabled(true);
				guide->set_text(
						"[color=yellow][b]Setup Required[/b][/color]\n\n"
						"No AI provider is connected yet. To get started:\n"
						"1. Click [b]Select Model[/b] in the toolbar above\n"
						"2. Choose a provider (e.g. Anthropic, OpenAI, Google)\n"
						"3. Follow the sign-in flow to authenticate\n\n"
						"Free models are available immediately — or connect your own API key for premium models.");
				chat_container->add_child(guide);
				_scroll_chat_to_bottom();
			}

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
		body_str = _body_to_string(p_body);
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

	String response_text = _body_to_string(p_body);
	JSON json;
	Error err = json.parse(response_text);

	if (err != OK) {
		return;
	}

	Variant parsed_data = json.get_data();
	if (parsed_data.get_type() != Variant::ARRAY) {
		return;
	}

	Array questions = parsed_data;
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

	// Immediately disable all option buttons to prevent double-clicks
	for (int i = 0; i < content->get_child_count(); i++) {
		Button *child_btn = Object::cast_to<Button>(content->get_child(i));
		if (child_btn) {
			child_btn->set_disabled(true);
		}
	}

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

	// Immediately disable all interactive elements to prevent double-submit
	custom_input->set_editable(false);
	PanelContainer *panel = Object::cast_to<PanelContainer>(question_container->get_child(0));
	if (panel) {
		VBoxContainer *content = Object::cast_to<VBoxContainer>(panel->get_child(0));
		if (content) {
			for (int i = 0; i < content->get_child_count(); i++) {
				Button *child_btn = Object::cast_to<Button>(content->get_child(i));
				if (child_btn) {
					child_btn->set_disabled(true);
				}
			}
		}
	}

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
		set_title(vformat("AI #%d", instance_id + 1));
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
	print_line("[AIAssistant] _clear_chat_ui() called. session_id=" + session_id.substr(0, 8) + " connection_status=" + String::num_int64(connection_status) + " children=" + String::num_int64(chat_container->get_child_count()));
	// Print a simplified call stack for debugging
	if (chat_container->get_child_count() > 3) {
		// Only log stack trace when clearing a non-trivial chat (more than welcome+system messages)
		WARN_PRINT("[AIAssistant] _clear_chat_ui() clearing " + String::num_int64(chat_container->get_child_count()) + " children while session is active!");
	}
	while (chat_container->get_child_count() > 0) {
		Node *child = chat_container->get_child(0);
		chat_container->remove_child(child);
		memdelete(child);
	}
	loading_overlay = nullptr; // Was a child of chat_container, now deleted
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

void AIAssistantDock::_on_file_removed(const String &p_file) {
	// When a file is deleted in the editor, tell OpenCode to clean up its AI metadata.
	if (service_url.is_empty() || connection_status != CONNECTED) {
		return;
	}
	// Only care about asset files (images, models, audio)
	String ext = p_file.get_extension().to_lower();
	if (ext != "png" && ext != "jpg" && ext != "jpeg" && ext != "webp" &&
			ext != "glb" && ext != "gltf" && ext != "tres" &&
			ext != "mp3" && ext != "ogg" && ext != "wav") {
		return;
	}

	// Fire-and-forget DELETE to /ai-assets/metadata/{res_path}
	Node *parent = EditorInterface::get_singleton()->get_editor_main_screen();
	if (!parent) {
		return;
	}
	HTTPRequest *req = memnew(HTTPRequest);
	parent->add_child(req);
	req->connect("request_completed", callable_mp((Node *)req, &Node::queue_free).unbind(4));
	String url = service_url + "/ai-assets/metadata/" + p_file.uri_encode();
	req->request(url, _get_headers_with_directory(), HTTPClient::METHOD_DELETE);
}

void AIAssistantDock::_on_editor_selection_changed() {
	_update_context_label();
}

void AIAssistantDock::_on_filesystem_selection_changed() {
	_update_context_label();
}

void AIAssistantDock::_update_context_label() {
	String display;

	// Node selection
	EditorSelection *editor_sel = EditorInterface::get_singleton()->get_selection();
	if (editor_sel) {
		List<Node *> nodes = editor_sel->get_full_selected_node_list();
		if (nodes.size() == 1) {
			display += String::utf8("\xf0\x9f\x93\x8d ") + nodes.front()->get()->get_name();
		} else if (nodes.size() > 1) {
			display += String::utf8("\xf0\x9f\x93\x8d ") + itos(nodes.size()) + " nodes";
		}
	}

	// Asset/file selection
	FileSystemDock *fs_dock = FileSystemDock::get_singleton();
	if (fs_dock) {
		Vector<String> selected = fs_dock->get_selected_paths();
		if (!selected.is_empty()) {
			if (!display.is_empty()) {
				display += "  ";
			}
			if (selected.size() == 1) {
				display += String::utf8("\xf0\x9f\x93\x81 ") + selected[0].get_file();
			} else {
				display += String::utf8("\xf0\x9f\x93\x81 ") + itos(selected.size()) + " files";
			}
		}
	}

	if (display.is_empty()) {
		context_label->set_visible(false);
	} else {
		context_label->set_text(display);
		context_label->set_visible(true);
	}
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

	String response_text = _body_to_string(p_body);
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

		String session_title = session.get("title", "Untitled");
		String sid = session.get("id", "");

		if (session_title.length() > 35) {
			session_title = session_title.substr(0, 32) + "...";
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
		String label = sid == session_id ? String(U"\u2713 ") + session_title : session_title;
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

	print_line("[AIAssistant] CLEAR_SITE_E: new session button pressed");
	_clear_chat_ui();
	_add_system_message("Creating new session...");

	session_id = "";
	has_user_message = false;
	engine_prompts_injected = false;
	_create_session();
}

void AIAssistantDock::_on_session_item_clicked(int p_index) {
	session_popup->hide();

	if (p_index < 0 || p_index >= cached_session_list.size()) {
		return;
	}

	Dictionary session = cached_session_list[p_index];
	String sid = session.get("id", "");
	String session_title = session.get("title", "Untitled");

	if (sid == session_id) {
		return; // Already on this session.
	}

	_switch_to_session(sid, session_title);
}

void AIAssistantDock::_switch_to_session(const String &p_session_id, const String &p_title) {
	// If current session is empty (no user messages), auto-delete it.
	if (!has_user_message && !session_id.is_empty()) {
		_fire_and_forget_delete_session(session_id);
	}

	print_line("[AIAssistant] CLEAR_SITE_F: switching to session " + p_session_id.substr(0, 8));
	_clear_chat_ui();

	// Switch to new session.
	session_id = p_session_id;
	has_user_message = false;
	engine_prompts_injected = false;

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
	String session_title = session.get("title", "Untitled");
	session_delete_confirm->set_text(TTR("Delete session \"") + session_title + "\"?");
	session_delete_confirm->popup_centered();
}

void AIAssistantDock::_on_session_delete_confirmed() {
	int p_index = session_pending_delete_index;
	if (p_index < 0 || p_index >= cached_session_list.size()) {
		return;
	}

	Dictionary session = cached_session_list[p_index];
	String sid = session.get("id", "");
	String session_title = session.get("title", "Untitled");

	if (sid.is_empty()) {
		return;
	}

	// Fire-and-forget DELETE.
	_fire_and_forget_delete_session(sid);

	// If the deleted session is the current one, clear UI and enter "no session" state.
	if (sid == session_id) {
		print_line("[AIAssistant] CLEAR_SITE_G: deleted current session");
		_clear_chat_ui();

		session_id = "";
		has_user_message = false;
		engine_prompts_injected = false;

		session_history_button->set_text("No Session");
		prompt_input->set_editable(false);
		prompt_input->set_placeholder(TTR("Select or create a session to start chatting..."));
		send_button->set_disabled(true);
	}

	_add_system_message("Session \"" + session_title + "\" deleted.");

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
	String session_title = session.get("title", "Untitled");
	session_rename_input->set_text(session_title);
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

// ═══════════════════════════════════════════════════════════════════════════
// Snipping Tool
// ═══════════════════════════════════════════════════════════════════════════

void AIAssistantDock::_on_snip_pressed() {
	// Launch external ScreenCapture tool for snipping.
	// Search in multiple locations: next to exe, and in repo dist/.
	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	String tool_path = exe_dir.path_join("tools").path_join("ScreenCapture.exe");

	if (!FileAccess::exists(tool_path)) {
		// Fallback: check parent dir (for dev builds where exe is in godot/bin/).
		tool_path = exe_dir.get_base_dir().path_join("dist").path_join("redblue-engine").path_join("tools").path_join("ScreenCapture.exe");
	}

	if (!FileAccess::exists(tool_path)) {
		_add_system_message("[Snip] ScreenCapture.exe not found. Place it at: " + exe_dir.path_join("tools").path_join("ScreenCapture.exe"));
		return;
	}

	String temp_dir = OS::get_singleton()->get_user_data_dir();
	String temp_path = temp_dir.path_join("snip.png");

	List<String> args;
	args.push_back("--path:" + temp_path);
	args.push_back("--tool:rect,arrow,line,text,number,|,undo,redo,|,save,close");

	print_line("[Snip] Launching: " + tool_path);

	int exit_code = -1;
	OS::get_singleton()->execute(tool_path, args, nullptr, &exit_code);
	print_line("[Snip] Exit code: " + itos(exit_code));

	if (exit_code == 8) {
		// Exit code 8 = saved to file.
		Ref<Image> img;
		img.instantiate();
		Error err = img->load(temp_path);
		if (err == OK && img.is_valid()) {
			Vector<uint8_t> png = img->save_png_to_buffer();
			_add_attachment_from_raw_data("snip.png", "image/png", png);
			_add_system_message("[Snip] Screenshot attached.");

			if (prompt_input->get_text().is_empty()) {
				prompt_input->set_text("Analyze the highlighted areas in this screenshot.");
			}
			prompt_input->grab_focus();
		}
		// Clean up temp file.
		Ref<DirAccess> da = DirAccess::open(temp_dir);
		if (da.is_valid()) {
			da->remove("snip.png");
		}
	}
	// Other exit codes = user cancelled, do nothing.
	// Other exit codes = user cancelled, do nothing.
}

void AIAssistantDock::_on_snip_captured(const Ref<Image> &p_image, const Rect2 &p_rect) {
	// Unused — kept for backward compatibility with bind_methods.
}

void AIAssistantDock::_on_snip_annotated(const Ref<Image> &p_image, const PackedStringArray &p_text_labels) {
	// Unused — kept for backward compatibility with bind_methods.
}

void AIAssistantDock::_on_snip_cancelled() {
	// Unused — kept for backward compatibility with bind_methods.
}
