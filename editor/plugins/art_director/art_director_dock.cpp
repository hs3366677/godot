/**************************************************************************/
/*  art_director_dock.cpp                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                           MAKABAKA ENGINE                              */
/*                    AI-powered game creation module                     */
/**************************************************************************/

#include "art_director_dock.h"

#include "core/input/input_event.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "editor/editor_node.h"
#include "editor/editor_main_screen.h"
#include "scene/resources/image_texture.h"

// =============================================================================
// ArtDirectorPanel
// =============================================================================

ArtDirectorPanel::ArtDirectorPanel() {
	set_h_size_flags(SIZE_EXPAND_FILL);
	set_v_size_flags(SIZE_EXPAND_FILL);
	_setup_ui();
}

void ArtDirectorPanel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_profile_completed"), &ArtDirectorPanel::_on_profile_completed);
	ClassDB::bind_method(D_METHOD("_on_images_completed"), &ArtDirectorPanel::_on_images_completed);
	ClassDB::bind_method(D_METHOD("_on_models_completed"), &ArtDirectorPanel::_on_models_completed);
	ClassDB::bind_method(D_METHOD("_on_gallery_refresh_timeout"), &ArtDirectorPanel::_on_gallery_refresh_timeout);
	ClassDB::bind_method(D_METHOD("_on_open_image_pressed", "abs_path"), &ArtDirectorPanel::_on_open_image_pressed);
	ClassDB::bind_method(D_METHOD("_on_exploration_refresh_pressed"), &ArtDirectorPanel::_on_exploration_refresh_pressed);
	ClassDB::bind_method(D_METHOD("_on_cornerstone_refresh_pressed"), &ArtDirectorPanel::_on_cornerstone_refresh_pressed);
}

void ArtDirectorPanel::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		_refresh_profile();
		_refresh_images();
		_refresh_models();
		gallery_refresh_timer->start();
	}
}

// ── UI Setup ──────────────────────────────────────────────────────────────────

void ArtDirectorPanel::_setup_ui() {
	MarginContainer *margin = memnew(MarginContainer);
	margin->set_h_size_flags(SIZE_EXPAND_FILL);
	margin->set_v_size_flags(SIZE_EXPAND_FILL);
	margin->add_theme_constant_override("margin_left", 8);
	margin->add_theme_constant_override("margin_right", 8);
	margin->add_theme_constant_override("margin_top", 8);
	margin->add_theme_constant_override("margin_bottom", 8);
	add_child(margin);

	ScrollContainer *scroll = memnew(ScrollContainer);
	scroll->set_h_size_flags(SIZE_EXPAND_FILL);
	scroll->set_v_size_flags(SIZE_EXPAND_FILL);
	scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	margin->add_child(scroll);

	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->set_h_size_flags(SIZE_EXPAND_FILL);
	scroll->add_child(vbox);

	// ── Model picker (top, global) ──────────────────────────────────────
	HBoxContainer *model_row = memnew(HBoxContainer);
	vbox->add_child(model_row);

	Label *model_label = memnew(Label);
	model_label->set_text(TTR("Model:"));
	model_row->add_child(model_label);

	model_picker = memnew(OptionButton);
	model_picker->set_h_size_flags(SIZE_EXPAND_FILL);
	model_picker->add_item(TTR("Loading..."), 0);
	model_row->add_child(model_picker);

	vbox->add_child(memnew(HSeparator));

	// ── ① Style Explorations ────────────────────────────────────────────
	HBoxContainer *exp_header = memnew(HBoxContainer);
	vbox->add_child(exp_header);

	Label *exp_step = memnew(Label);
	exp_step->set_text(String::utf8("\u2460 Style Explorations"));
	exp_step->add_theme_color_override("font_color", Color(0.8, 0.8, 1.0));
	exp_step->set_h_size_flags(SIZE_EXPAND_FILL);
	exp_header->add_child(exp_step);

	// Session navigation
	session_prev_button = memnew(Button);
	session_prev_button->set_text(String::utf8("\u25C0"));
	session_prev_button->set_tooltip_text(TTR("Previous session"));
	session_prev_button->connect("pressed", callable_mp(this, &ArtDirectorPanel::_on_session_prev_pressed));
	exp_header->add_child(session_prev_button);

	session_label = memnew(Label);
	session_label->set_text("0/0");
	session_label->set_custom_minimum_size(Size2(48, 0));
	session_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	exp_header->add_child(session_label);

	session_next_button = memnew(Button);
	session_next_button->set_text(String::utf8("\u25B6"));
	session_next_button->set_tooltip_text(TTR("Next session"));
	session_next_button->connect("pressed", callable_mp(this, &ArtDirectorPanel::_on_session_next_pressed));
	exp_header->add_child(session_next_button);

	exploration_refresh_button = memnew(Button);
	exploration_refresh_button->set_text(String::utf8("\u27F3"));
	exploration_refresh_button->set_tooltip_text(TTR("Refresh"));
	exploration_refresh_button->connect("pressed", callable_mp(this, &ArtDirectorPanel::_on_exploration_refresh_pressed));
	exp_header->add_child(exploration_refresh_button);

	ScrollContainer *exp_scroll = memnew(ScrollContainer);
	exp_scroll->set_custom_minimum_size(Size2(0, THUMB_SIZE + 32));
	exp_scroll->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	vbox->add_child(exp_scroll);

	exploration_gallery = memnew(HBoxContainer);
	exploration_gallery->set_h_size_flags(SIZE_EXPAND_FILL);
	exp_scroll->add_child(exploration_gallery);

	vbox->add_child(memnew(HSeparator));

	// ── ② Selected Style ────────────────────────────────────────────────
	Label *style_step = memnew(Label);
	style_step->set_text(String::utf8("\u2461 Selected Style"));
	style_step->add_theme_color_override("font_color", Color(0.8, 0.8, 1.0));
	vbox->add_child(style_step);

	HBoxContainer *style_row = memnew(HBoxContainer);
	vbox->add_child(style_row);

	selected_style_thumb = memnew(TextureRect);
	selected_style_thumb->set_custom_minimum_size(Size2(THUMB_SIZE, THUMB_SIZE));
	selected_style_thumb->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
	selected_style_thumb->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	style_row->add_child(selected_style_thumb);

	VBoxContainer *style_info = memnew(VBoxContainer);
	style_info->set_h_size_flags(SIZE_EXPAND_FILL);
	style_row->add_child(style_info);

	selected_style_label = memnew(Label);
	selected_style_label->set_text(TTR("Style: (not set - run godot_art_explore to begin)"));
	selected_style_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	style_info->add_child(selected_style_label);

	selected_ref_label = memnew(Label);
	selected_ref_label->set_text(TTR("Reference: (none)"));
	selected_ref_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	style_info->add_child(selected_ref_label);

	vbox->add_child(memnew(HSeparator));

	// ── ③ Cornerstone Assets ────────────────────────────────────────────
	HBoxContainer *corner_header = memnew(HBoxContainer);
	vbox->add_child(corner_header);

	Label *corner_step = memnew(Label);
	corner_step->set_text(String::utf8("\u2462 Cornerstone Assets"));
	corner_step->add_theme_color_override("font_color", Color(0.8, 0.8, 1.0));
	corner_step->set_h_size_flags(SIZE_EXPAND_FILL);
	corner_header->add_child(corner_step);

	cornerstone_refresh_button = memnew(Button);
	cornerstone_refresh_button->set_text(String::utf8("\u27F3"));
	cornerstone_refresh_button->set_tooltip_text(TTR("Refresh"));
	cornerstone_refresh_button->connect("pressed", callable_mp(this, &ArtDirectorPanel::_on_cornerstone_refresh_pressed));
	corner_header->add_child(cornerstone_refresh_button);

	ScrollContainer *corner_scroll = memnew(ScrollContainer);
	corner_scroll->set_custom_minimum_size(Size2(0, THUMB_SIZE + 32));
	corner_scroll->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	vbox->add_child(corner_scroll);

	cornerstone_gallery = memnew(HBoxContainer);
	cornerstone_gallery->set_h_size_flags(SIZE_EXPAND_FILL);
	corner_scroll->add_child(cornerstone_gallery);

	// ── HTTP infrastructure ──────────────────────────────────────────────
	profile_http_request = memnew(HTTPRequest);
	add_child(profile_http_request);
	profile_http_request->connect("request_completed", callable_mp(this, &ArtDirectorPanel::_on_profile_completed));

	images_http_request = memnew(HTTPRequest);
	add_child(images_http_request);
	images_http_request->connect("request_completed", callable_mp(this, &ArtDirectorPanel::_on_images_completed));

	models_http_request = memnew(HTTPRequest);
	add_child(models_http_request);
	models_http_request->connect("request_completed", callable_mp(this, &ArtDirectorPanel::_on_models_completed));

	gallery_refresh_timer = memnew(Timer);
	gallery_refresh_timer->set_wait_time(5.0);
	gallery_refresh_timer->connect("timeout", callable_mp(this, &ArtDirectorPanel::_on_gallery_refresh_timeout));
	add_child(gallery_refresh_timer);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

String ArtDirectorPanel::_get_project_root() const {
	return ProjectSettings::get_singleton()->globalize_path("res://");
}

String ArtDirectorPanel::_abs_path_from_res(const String &p_res_path) const {
	if (p_res_path.begins_with("res://")) {
		String root = _get_project_root();
		return root.path_join(p_res_path.substr(6));
	}
	return p_res_path;
}

void ArtDirectorPanel::_refresh_profile() {
	String dir = _get_project_root().uri_encode();
	String url = service_url + "/godot/art-director/profile?directory=" + dir;
	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	profile_http_request->request(url, headers);
}

void ArtDirectorPanel::_refresh_images() {
	if (images_request_in_progress) {
		return;
	}
	images_request_in_progress = true;
	String dir = _get_project_root().uri_encode();
	String url = service_url + "/godot/art-director/images?directory=" + dir;
	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	images_http_request->request(url, headers);
}

void ArtDirectorPanel::_refresh_models() {
	String url = service_url + "/godot/art-director/models";
	PackedStringArray headers;
	models_http_request->request(url, headers);
}

void ArtDirectorPanel::_populate_model_picker(OptionButton *p_picker, const String &p_default_id) {
	String current = p_picker->get_item_count() > 0 ? p_picker->get_item_text(p_picker->get_selected()) : "";

	if (current.is_empty() || current == TTR("Loading...")) {
		for (int i = 0; i < p_picker->get_item_count(); i++) {
			if (p_picker->get_item_metadata(i) == p_default_id) {
				p_picker->select(i);
				return;
			}
		}
		if (p_picker->get_item_count() > 0) {
			p_picker->select(0);
		}
	}
}

String ArtDirectorPanel::get_exploration_model() const {
	if (!model_picker || model_picker->get_item_count() == 0) {
		return "flux-schnell";
	}
	Variant meta = model_picker->get_item_metadata(model_picker->get_selected());
	return meta.get_type() == Variant::STRING ? String(meta) : "flux-schnell";
}

void ArtDirectorPanel::_load_thumbnail(const String &p_abs_path, ThumbInfo &r_info) {
	Ref<Image> img = Image::load_from_file(p_abs_path);
	if (img.is_null()) {
		return;
	}
	// Preserve aspect ratio: fit within THUMB_SIZE box
	int orig_w = img->get_width();
	int orig_h = img->get_height();
	int new_w, new_h;
	if (orig_w >= orig_h) {
		new_w = THUMB_SIZE;
		new_h = MAX(1, orig_h * THUMB_SIZE / orig_w);
	} else {
		new_h = THUMB_SIZE;
		new_w = MAX(1, orig_w * THUMB_SIZE / orig_h);
	}
	img->resize(new_w, new_h, Image::INTERPOLATE_BILINEAR);
	Ref<ImageTexture> tex = ImageTexture::create_from_image(img);
	r_info.texture = tex;
}

void ArtDirectorPanel::_rebuild_gallery(HBoxContainer *p_gallery, const Vector<ThumbInfo> &p_thumbs, const String &p_category) {
	while (p_gallery->get_child_count() > 0) {
		Node *child = p_gallery->get_child(0);
		p_gallery->remove_child(child);
		child->queue_free();
	}

	if (p_thumbs.is_empty()) {
		Label *empty_lbl = memnew(Label);
		empty_lbl->set_text(TTR("(none)"));
		p_gallery->add_child(empty_lbl);
		return;
	}

	for (int i = 0; i < p_thumbs.size(); i++) {
		const ThumbInfo &info = p_thumbs[i];
		String abs_path = _abs_path_from_res(info.res_path);

		VBoxContainer *item_vbox = memnew(VBoxContainer);
		p_gallery->add_child(item_vbox);

		// Use TextureRect for proper aspect-ratio display
		TextureRect *thumb_rect = memnew(TextureRect);
		thumb_rect->set_custom_minimum_size(Size2(THUMB_SIZE, THUMB_SIZE));
		thumb_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
		thumb_rect->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
		if (info.texture.is_valid()) {
			thumb_rect->set_texture(info.texture);
		}
		String tip = info.tooltip.is_empty()
			? TTR("Double-click to open in viewer")
			: info.tooltip + "\n\n" + TTR("Double-click to open in viewer");
		thumb_rect->set_tooltip_text(tip);
		thumb_rect->set_mouse_filter(Control::MOUSE_FILTER_STOP);

		// Store abs_path for double-click open
		thumb_rect->set_meta("abs_path", abs_path);
		thumb_rect->connect("gui_input", callable_mp(this, &ArtDirectorPanel::_on_thumb_gui_input).bind(abs_path));
		item_vbox->add_child(thumb_rect);

		// Number label under each thumb
		Label *num_lbl = memnew(Label);
		num_lbl->set_text(String::num_int64(i + 1));
		num_lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		num_lbl->set_custom_minimum_size(Size2(THUMB_SIZE, 0));
		num_lbl->set_tooltip_text(tip);
		item_vbox->add_child(num_lbl);
	}
}

void ArtDirectorPanel::_rebuild_exploration_gallery() {
	if (exploration_sessions.is_empty()) {
		session_label->set_text("0/0");
		session_prev_button->set_disabled(true);
		session_next_button->set_disabled(true);

		// Clear gallery
		while (exploration_gallery->get_child_count() > 0) {
			Node *child = exploration_gallery->get_child(0);
			exploration_gallery->remove_child(child);
			child->queue_free();
		}
		Label *empty_lbl = memnew(Label);
		empty_lbl->set_text(TTR("(none)"));
		exploration_gallery->add_child(empty_lbl);
		return;
	}

	// Clamp index
	if (current_session_index < 0) {
		current_session_index = 0;
	}
	if (current_session_index >= exploration_sessions.size()) {
		current_session_index = exploration_sessions.size() - 1;
	}

	session_label->set_text(String::num_int64(current_session_index + 1) + "/" + String::num_int64(exploration_sessions.size()));
	session_prev_button->set_disabled(current_session_index <= 0);
	session_next_button->set_disabled(current_session_index >= exploration_sessions.size() - 1);

	const SessionInfo &session = exploration_sessions[current_session_index];
	_rebuild_gallery(exploration_gallery, session.images, "exploration");
}

// ── HTTP callbacks ─────────────────────────────────────────────────────────────

void ArtDirectorPanel::_on_profile_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		selected_style_label->set_text(TTR("Style: (server not available)"));
		return;
	}

	String body_str = String::utf8((const char *)p_body.ptr(), p_body.size());
	JSON json;
	if (json.parse(body_str) != OK) {
		return;
	}

	Dictionary data = json.get_data();
	if (!data.has("profile") || data["profile"].get_type() == Variant::NIL) {
		selected_style_label->set_text(TTR("Style: (not set - run godot_art_explore to begin)"));
		selected_ref_label->set_text(TTR("Reference: (none)"));
		selected_style_thumb->set_texture(Ref<Texture2D>());
		return;
	}

	Dictionary profile = data["profile"];
	String art_direction = profile.get("art_direction", "");
	String reference_asset = profile.get("reference_asset", "");

	selected_style_label->set_text(TTR("Style: ") + (art_direction.is_empty() ? "(empty)" : art_direction));
	selected_ref_label->set_text(TTR("Ref: ") + (reference_asset.is_empty() ? "(none)" : reference_asset));

	// Load reference image thumbnail
	if (!reference_asset.is_empty()) {
		String abs_path = _abs_path_from_res(reference_asset);
		Ref<Image> img = Image::load_from_file(abs_path);
		if (img.is_valid()) {
			img->resize(THUMB_SIZE, THUMB_SIZE, Image::INTERPOLATE_BILINEAR);
			Ref<ImageTexture> tex = ImageTexture::create_from_image(img);
			selected_style_thumb->set_texture(tex);
		} else {
			selected_style_thumb->set_texture(Ref<Texture2D>());
		}
	} else {
		selected_style_thumb->set_texture(Ref<Texture2D>());
	}
}

void ArtDirectorPanel::_on_images_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	images_request_in_progress = false;
	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		return;
	}

	String body_str = String::utf8((const char *)p_body.ptr(), p_body.size());
	JSON json;
	if (json.parse(body_str) != OK) {
		return;
	}

	Dictionary data = json.get_data();

	// Parse sessions-based response
	cornerstone_thumbs.clear();
	exploration_sessions.clear();

	if (data.has("sessions")) {
		Array sessions = data.get("sessions", Array());
		for (int s = 0; s < sessions.size(); s++) {
			Dictionary sess = sessions[s];
			SessionInfo si;
			si.id = sess.get("id", "");
			Array images = sess.get("images", Array());
			for (int i = 0; i < images.size(); i++) {
				Dictionary img_info = images[i];
				ThumbInfo info;
				info.res_path = img_info.get("resPath", "");
				info.category = "exploration";
				info.label = img_info.get("label", "");
				info.tooltip = img_info.get("tooltip", "");
				_load_thumbnail(String(img_info.get("absPath", "")), info);
				si.images.push_back(info);
			}
			if (!si.images.is_empty()) {
				exploration_sessions.push_back(si);
			}
		}
	} else if (data.has("images")) {
		// Legacy flat response — put all explorations into one session
		Array images = data.get("images", Array());
		SessionInfo default_session;
		default_session.id = "default";
		for (int i = 0; i < images.size(); i++) {
			Dictionary img_info = images[i];
			String category = img_info.get("category", "");
			ThumbInfo info;
			info.res_path = img_info.get("resPath", "");
			info.category = category;
			info.label = img_info.get("label", "");
			info.tooltip = img_info.get("tooltip", "");
			_load_thumbnail(String(img_info.get("absPath", "")), info);

			if (category == "exploration") {
				default_session.images.push_back(info);
			} else if (category == "cornerstone") {
				cornerstone_thumbs.push_back(info);
			}
		}
		if (!default_session.images.is_empty()) {
			exploration_sessions.push_back(default_session);
		}
	}

	// Parse cornerstone from sessions response
	if (data.has("cornerstone")) {
		Array corner = data.get("cornerstone", Array());
		for (int i = 0; i < corner.size(); i++) {
			Dictionary img_info = corner[i];
			ThumbInfo info;
			info.res_path = img_info.get("resPath", "");
			info.category = "cornerstone";
			info.label = img_info.get("label", "");
			info.tooltip = img_info.get("tooltip", "");
			_load_thumbnail(String(img_info.get("absPath", "")), info);
			cornerstone_thumbs.push_back(info);
		}
	}

	// Default to newest session (index 0, since sorted newest first)
	if (!exploration_sessions.is_empty() && current_session_index >= exploration_sessions.size()) {
		current_session_index = 0;
	}

	_rebuild_exploration_gallery();
	_rebuild_gallery(cornerstone_gallery, cornerstone_thumbs, "cornerstone");
}

void ArtDirectorPanel::_on_models_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		struct ModelEntry { const char *id; const char *label; };
		static const ModelEntry FALLBACK_MODELS[] = {
			{ "nano-banana-2",       "nano-banana-2 ($0.067) - fast, pro-level" },
			{ "flux-2-dev",          "flux-2-dev ($0.012) - FLUX.2 open-source" },
			{ "flux-2-pro",          "flux-2-pro ($0.015) - FLUX.2 flagship" },
			{ "flux-schnell",        "flux-schnell ($0.003) - fastest" },
			{ "flux-kontext-pro",    "flux-kontext-pro ($0.04) - style consistent" },
			{ "flux-kontext-max",    "flux-kontext-max ($0.06) - best quality" },
			{ "sd-3.5-medium",       "sd-3.5-medium ($0.035)" },
			{ "sd-3.5-large-turbo",  "sd-3.5-large-turbo ($0.04)" },
			{ "sdxl",                "sdxl ($0.0055)" },
		};
		model_picker->clear();
		for (const ModelEntry &m : FALLBACK_MODELS) {
			model_picker->add_item(m.label);
			model_picker->set_item_metadata(model_picker->get_item_count() - 1, String(m.id));
		}
		_populate_model_picker(model_picker, "nano-banana-2");
		return;
	}

	String body_str = String::utf8((const char *)p_body.ptr(), p_body.size());
	JSON json;
	if (json.parse(body_str) != OK) {
		return;
	}

	Dictionary data = json.get_data();
	Array models = data.get("models", Array());

	if (models.is_empty()) {
		return;
	}

	model_picker->clear();

	for (int i = 0; i < models.size(); i++) {
		Dictionary m = models[i];
		String id = m.get("id", "");
		String cost_str = "";
		if (m.has("pricing")) {
			Dictionary pricing = m["pricing"];
			if (pricing.has("cost")) {
				cost_str = " ($" + String::num(double(pricing["cost"]), 4) + ")";
			}
		}
		String label = id + cost_str;

		model_picker->add_item(label);
		model_picker->set_item_metadata(model_picker->get_item_count() - 1, id);
	}

	_populate_model_picker(model_picker, "flux-2-dev");
}

// ── UI callbacks ─────────────────────────────────────────────────────────────

void ArtDirectorPanel::_on_open_image_pressed(String p_abs_path) {
	OS::get_singleton()->shell_open(p_abs_path);
}

void ArtDirectorPanel::_on_thumb_gui_input(const Ref<InputEvent> &p_event, String p_abs_path) {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT && mb->is_double_click()) {
		_on_open_image_pressed(p_abs_path);
	}
}

void ArtDirectorPanel::_on_session_prev_pressed() {
	if (current_session_index > 0) {
		current_session_index--;
		_rebuild_exploration_gallery();
	}
}

void ArtDirectorPanel::_on_session_next_pressed() {
	if (current_session_index < exploration_sessions.size() - 1) {
		current_session_index++;
		_rebuild_exploration_gallery();
	}
}

void ArtDirectorPanel::_on_exploration_refresh_pressed() {
	_refresh_images();
}

void ArtDirectorPanel::_on_cornerstone_refresh_pressed() {
	_refresh_images();
}

void ArtDirectorPanel::_on_gallery_refresh_timeout() {
	_refresh_images();
	_refresh_profile();
}

// =============================================================================
// ArtDirectorPlugin
// =============================================================================

ArtDirectorPlugin::ArtDirectorPlugin() {
	panel = memnew(ArtDirectorPanel);
	panel->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	EditorNode::get_singleton()->get_editor_main_screen()->get_control()->add_child(panel);
	panel->hide();
}

void ArtDirectorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		panel->show();
	} else {
		panel->hide();
	}
}
