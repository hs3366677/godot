/**************************************************************************/
/*  art_director_dock.cpp                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                           MAKABAKA ENGINE                              */
/*                    AI-powered game creation module                     */
/**************************************************************************/

#include "art_director_dock.h"

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
	ClassDB::bind_method(D_METHOD("_on_set_ref_completed"), &ArtDirectorPanel::_on_set_ref_completed);
	ClassDB::bind_method(D_METHOD("_on_gallery_refresh_timeout"), &ArtDirectorPanel::_on_gallery_refresh_timeout);
	ClassDB::bind_method(D_METHOD("_on_open_image_pressed", "abs_path"), &ArtDirectorPanel::_on_open_image_pressed);
	ClassDB::bind_method(D_METHOD("_on_set_reference_pressed"), &ArtDirectorPanel::_on_set_reference_pressed);
	ClassDB::bind_method(D_METHOD("_on_exploration_refresh_pressed"), &ArtDirectorPanel::_on_exploration_refresh_pressed);
	ClassDB::bind_method(D_METHOD("_on_cornerstone_refresh_pressed"), &ArtDirectorPanel::_on_cornerstone_refresh_pressed);
}

void ArtDirectorPanel::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		// Load initial data
		_refresh_profile();
		_refresh_images();
		_refresh_models();
		gallery_refresh_timer->start();
	}
}

// ── UI Setup ──────────────────────────────────────────────────────────────────

void ArtDirectorPanel::_setup_ui() {
	// Root: margin + two-column HBox
	MarginContainer *margin = memnew(MarginContainer);
	margin->set_h_size_flags(SIZE_EXPAND_FILL);
	margin->set_v_size_flags(SIZE_EXPAND_FILL);
	margin->add_theme_constant_override("margin_left", 8);
	margin->add_theme_constant_override("margin_right", 8);
	margin->add_theme_constant_override("margin_top", 8);
	margin->add_theme_constant_override("margin_bottom", 8);
	add_child(margin);

	ScrollContainer *left_scroll = memnew(ScrollContainer);
	left_scroll->set_h_size_flags(SIZE_EXPAND_FILL);
	left_scroll->set_v_size_flags(SIZE_EXPAND_FILL);
	left_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	margin->add_child(left_scroll);

	VBoxContainer *left_vbox = memnew(VBoxContainer);
	left_vbox->set_h_size_flags(SIZE_EXPAND_FILL);
	left_scroll->add_child(left_vbox);

	// ── Style Profile ────────────────────────────────────────────────────────
	Label *profile_header = memnew(Label);
	profile_header->set_text(TTR("Style Profile"));
	profile_header->add_theme_color_override("font_color", Color(0.8, 0.8, 1.0));
	left_vbox->add_child(profile_header);

	profile_section = memnew(VBoxContainer);
	left_vbox->add_child(profile_section);

	profile_style_label = memnew(Label);
	profile_style_label->set_text(TTR("Style: (not set — run godot_art_explore to begin)"));
	profile_style_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	profile_section->add_child(profile_style_label);

	profile_ref_label = memnew(Label);
	profile_ref_label->set_text(TTR("Reference: (none)"));
	profile_ref_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	profile_section->add_child(profile_ref_label);

	left_vbox->add_child(memnew(HSeparator));

	// ── Style Explorations ───────────────────────────────────────────────────
	exploration_header = memnew(HBoxContainer);
	left_vbox->add_child(exploration_header);

	Label *exp_label = memnew(Label);
	exp_label->set_text(TTR("Style Explorations"));
	exp_label->add_theme_color_override("font_color", Color(0.8, 0.8, 1.0));
	exp_label->set_h_size_flags(SIZE_EXPAND_FILL);
	exploration_header->add_child(exp_label);

	exploration_refresh_button = memnew(Button);
	exploration_refresh_button->set_text(TTR("Refresh"));
	exploration_refresh_button->connect("pressed", callable_mp(this, &ArtDirectorPanel::_on_exploration_refresh_pressed));
	exploration_header->add_child(exploration_refresh_button);

	// Exploration model picker row
	HBoxContainer *exp_model_row = memnew(HBoxContainer);
	left_vbox->add_child(exp_model_row);

	Label *exp_model_label = memnew(Label);
	exp_model_label->set_text(TTR("Explore model:"));
	exp_model_row->add_child(exp_model_label);

	exploration_model_picker = memnew(OptionButton);
	exploration_model_picker->set_h_size_flags(SIZE_EXPAND_FILL);
	exploration_model_picker->add_item(TTR("Loading..."), 0);
	exp_model_row->add_child(exploration_model_picker);

	ScrollContainer *exp_scroll = memnew(ScrollContainer);
	exp_scroll->set_custom_minimum_size(Size2(0, THUMB_SIZE + 32));
	exp_scroll->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	left_vbox->add_child(exp_scroll);

	exploration_gallery = memnew(HBoxContainer);
	exploration_gallery->set_h_size_flags(SIZE_EXPAND_FILL);
	exp_scroll->add_child(exploration_gallery);

	exploration_set_ref_button = memnew(Button);
	exploration_set_ref_button->set_text(TTR("★ Set as Reference"));
	exploration_set_ref_button->set_disabled(true);
	exploration_set_ref_button->connect("pressed", callable_mp(this, &ArtDirectorPanel::_on_set_reference_pressed));
	left_vbox->add_child(exploration_set_ref_button);

	left_vbox->add_child(memnew(HSeparator));

	// ── Cornerstone Assets ───────────────────────────────────────────────────
	cornerstone_header = memnew(HBoxContainer);
	left_vbox->add_child(cornerstone_header);

	Label *corner_label = memnew(Label);
	corner_label->set_text(TTR("Cornerstone Assets"));
	corner_label->add_theme_color_override("font_color", Color(0.8, 0.8, 1.0));
	corner_label->set_h_size_flags(SIZE_EXPAND_FILL);
	cornerstone_header->add_child(corner_label);

	cornerstone_refresh_button = memnew(Button);
	cornerstone_refresh_button->set_text(TTR("Refresh"));
	cornerstone_refresh_button->connect("pressed", callable_mp(this, &ArtDirectorPanel::_on_cornerstone_refresh_pressed));
	cornerstone_header->add_child(cornerstone_refresh_button);

	ScrollContainer *corner_scroll = memnew(ScrollContainer);
	corner_scroll->set_custom_minimum_size(Size2(0, THUMB_SIZE + 32));
	corner_scroll->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	left_vbox->add_child(corner_scroll);

	cornerstone_gallery = memnew(HBoxContainer);
	cornerstone_gallery->set_h_size_flags(SIZE_EXPAND_FILL);
	corner_scroll->add_child(cornerstone_gallery);

	cornerstone_set_ref_button = memnew(Button);
	cornerstone_set_ref_button->set_text(TTR("★ Set as Reference"));
	cornerstone_set_ref_button->set_disabled(true);
	cornerstone_set_ref_button->connect("pressed", callable_mp(this, &ArtDirectorPanel::_on_set_reference_pressed));
	left_vbox->add_child(cornerstone_set_ref_button);

	// ── HTTP infrastructure ──────────────────────────────────────────────────
	profile_http_request = memnew(HTTPRequest);
	add_child(profile_http_request);
	profile_http_request->connect("request_completed", callable_mp(this, &ArtDirectorPanel::_on_profile_completed));

	images_http_request = memnew(HTTPRequest);
	add_child(images_http_request);
	images_http_request->connect("request_completed", callable_mp(this, &ArtDirectorPanel::_on_images_completed));

	models_http_request = memnew(HTTPRequest);
	add_child(models_http_request);
	models_http_request->connect("request_completed", callable_mp(this, &ArtDirectorPanel::_on_models_completed));

	set_ref_http_request = memnew(HTTPRequest);
	add_child(set_ref_http_request);
	set_ref_http_request->connect("request_completed", callable_mp(this, &ArtDirectorPanel::_on_set_ref_completed));

	// Periodically refresh the gallery so new images appear automatically.
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
	// Find current selection to preserve it
	String current = p_picker->get_item_count() > 0 ? p_picker->get_item_text(p_picker->get_selected()) : "";

	// Model list is populated by _on_models_completed; this just sets the default if no selection
	if (current.is_empty() || current == TTR("Loading...")) {
		for (int i = 0; i < p_picker->get_item_count(); i++) {
			if (p_picker->get_item_metadata(i) == p_default_id) {
				p_picker->select(i);
				return;
			}
		}
		// Fallback: select first
		if (p_picker->get_item_count() > 0) {
			p_picker->select(0);
		}
	}
}

String ArtDirectorPanel::get_exploration_model() const {
	if (!exploration_model_picker || exploration_model_picker->get_item_count() == 0) {
		return "flux-schnell";
	}
	Variant meta = exploration_model_picker->get_item_metadata(exploration_model_picker->get_selected());
	return meta.get_type() == Variant::STRING ? String(meta) : "flux-schnell";
}

void ArtDirectorPanel::_load_thumbnail(const String &p_abs_path, ThumbInfo &r_info) {
	Ref<Image> img = Image::load_from_file(p_abs_path);
	if (img.is_null()) {
		return;
	}
	img->resize(THUMB_SIZE, THUMB_SIZE, Image::INTERPOLATE_BILINEAR);
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

		VBoxContainer *vbox = memnew(VBoxContainer);
		p_gallery->add_child(vbox);

		Button *thumb_btn = memnew(Button);
		thumb_btn->set_custom_minimum_size(Size2(THUMB_SIZE, THUMB_SIZE));
		thumb_btn->set_flat(false);
		if (info.texture.is_valid()) {
			thumb_btn->set_button_icon(info.texture);
			thumb_btn->set_icon_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		}
		// Tooltip: show full description on hover, fall back to generic hint
		String tip = info.tooltip.is_empty()
			? TTR("Click to select  •  Double-click to open")
			: info.tooltip + "\n\n" + TTR("Click to select  •  Double-click to open");
		thumb_btn->set_tooltip_text(tip);

		thumb_btn->set_meta("category", p_category);
		thumb_btn->set_meta("index", i);
		thumb_btn->connect("pressed", callable_mp(this, p_category == "exploration"
			? &ArtDirectorPanel::_on_exploration_thumb_pressed
			: &ArtDirectorPanel::_on_cornerstone_thumb_pressed).bind(i));
		vbox->add_child(thumb_btn);

		Label *name_lbl = memnew(Label);
		String display_name = info.label.is_empty() ? info.res_path.get_file().get_basename() : info.label;
		name_lbl->set_text(display_name);
		name_lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		name_lbl->set_custom_minimum_size(Size2(THUMB_SIZE, 0));
		name_lbl->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		name_lbl->set_tooltip_text(tip);
		vbox->add_child(name_lbl);
	}
}

// ── HTTP callbacks ─────────────────────────────────────────────────────────────

void ArtDirectorPanel::_on_profile_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		profile_style_label->set_text(TTR("Style: (server not available)"));
		return;
	}

	String body_str = String::utf8((const char *)p_body.ptr(), p_body.size());
	JSON json;
	if (json.parse(body_str) != OK) {
		return;
	}

	Dictionary data = json.get_data();
	if (!data.has("profile") || data["profile"].get_type() == Variant::NIL) {
		profile_style_label->set_text(TTR("Style: (not set — run godot_art_explore to begin)"));
		profile_ref_label->set_text(TTR("Reference: (none)"));
		return;
	}

	Dictionary profile = data["profile"];
	String art_direction = profile.get("art_direction", "");
	String reference_asset = profile.get("reference_asset", "");

	profile_style_label->set_text(TTR("Style: ") + (art_direction.is_empty() ? "(empty)" : art_direction));
	profile_ref_label->set_text(TTR("Reference: ") + (reference_asset.is_empty() ? "(none)" : reference_asset));
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
	Array images = data.get("images", Array());

	exploration_thumbs.clear();
	cornerstone_thumbs.clear();

	for (int i = 0; i < images.size(); i++) {
		Dictionary img_info = images[i];
		String category = img_info.get("category", "");
		String res_path = img_info.get("resPath", "");
		String abs_path = img_info.get("absPath", "");

		ThumbInfo info;
		info.res_path = res_path;
		info.category = category;
		info.label = img_info.get("label", "");
		info.tooltip = img_info.get("tooltip", "");
		_load_thumbnail(abs_path, info);

		if (category == "exploration") {
			exploration_thumbs.push_back(info);
		} else if (category == "cornerstone") {
			cornerstone_thumbs.push_back(info);
		}
	}

	_rebuild_gallery(exploration_gallery, exploration_thumbs, "exploration");
	_rebuild_gallery(cornerstone_gallery, cornerstone_thumbs, "cornerstone");
}

void ArtDirectorPanel::_on_models_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		// Server not ready — populate with known static list as fallback
		struct ModelEntry { const char *id; const char *label; };
		static const ModelEntry FALLBACK_MODELS[] = {
			{ "nano-banana-2",       "nano-banana-2 ($0.067) — fast, pro-level" },
			{ "flux-2-dev",          "flux-2-dev ($0.012) — FLUX.2 open-source" },
			{ "flux-2-pro",          "flux-2-pro ($0.015) — FLUX.2 flagship" },
			{ "flux-schnell",        "flux-schnell ($0.003) — fastest" },
			{ "flux-kontext-pro",    "flux-kontext-pro ($0.04) — style consistent" },
			{ "flux-kontext-max",    "flux-kontext-max ($0.06) — best quality" },
			{ "sd-3.5-medium",       "sd-3.5-medium ($0.035)" },
			{ "sd-3.5-large-turbo",  "sd-3.5-large-turbo ($0.04)" },
			{ "sdxl",                "sdxl ($0.0055)" },
		};
		exploration_model_picker->clear();
		for (const ModelEntry &m : FALLBACK_MODELS) {
			exploration_model_picker->add_item(m.label);
			exploration_model_picker->set_item_metadata(exploration_model_picker->get_item_count() - 1, String(m.id));
		}
		_populate_model_picker(exploration_model_picker, "nano-banana-2");
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

	exploration_model_picker->clear();

	for (int i = 0; i < models.size(); i++) {
		Dictionary m = models[i];
		String id = m.get("id", "");
		String name = m.get("name", id);
		String cost_str = "";
		if (m.has("pricing")) {
			Dictionary pricing = m["pricing"];
			if (pricing.has("cost")) {
				cost_str = " ($" + String::num(double(pricing["cost"]), 4) + ")";
			}
		}
		String label = id + cost_str;

		exploration_model_picker->add_item(label);
		exploration_model_picker->set_item_metadata(exploration_model_picker->get_item_count() - 1, id);
	}

	_populate_model_picker(exploration_model_picker, "flux-2-dev");
}

void ArtDirectorPanel::_on_set_ref_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (p_result == HTTPRequest::RESULT_SUCCESS && p_code == 200) {
		_refresh_profile();
	}
}

// ── UI callbacks ─────────────────────────────────────────────────────────────

void ArtDirectorPanel::_on_open_image_pressed(String p_abs_path) {
	OS::get_singleton()->shell_open(p_abs_path);
}

void ArtDirectorPanel::_on_exploration_thumb_pressed(int p_index) {
	uint64_t now = OS::get_singleton()->get_ticks_msec();
	if (last_thumb_click_category == "exploration" && last_thumb_click_index == p_index && (now - last_thumb_click_time) < 400) {
		// Double-click: open in OS viewer
		if (p_index >= 0 && p_index < exploration_thumbs.size()) {
			_on_open_image_pressed(_abs_path_from_res(exploration_thumbs[p_index].res_path));
		}
		last_thumb_click_time = 0;
		return;
	}
	last_thumb_click_time = now;
	last_thumb_click_category = "exploration";
	last_thumb_click_index = p_index;

	selected_exploration_index = p_index;
	selected_cornerstone_index = -1;
	exploration_set_ref_button->set_disabled(false);
	cornerstone_set_ref_button->set_disabled(true);
}

void ArtDirectorPanel::_on_cornerstone_thumb_pressed(int p_index) {
	uint64_t now = OS::get_singleton()->get_ticks_msec();
	if (last_thumb_click_category == "cornerstone" && last_thumb_click_index == p_index && (now - last_thumb_click_time) < 400) {
		// Double-click: open in OS viewer
		if (p_index >= 0 && p_index < cornerstone_thumbs.size()) {
			_on_open_image_pressed(_abs_path_from_res(cornerstone_thumbs[p_index].res_path));
		}
		last_thumb_click_time = 0;
		return;
	}
	last_thumb_click_time = now;
	last_thumb_click_category = "cornerstone";
	last_thumb_click_index = p_index;

	selected_cornerstone_index = p_index;
	selected_exploration_index = -1;
	cornerstone_set_ref_button->set_disabled(false);
	exploration_set_ref_button->set_disabled(true);
}

void ArtDirectorPanel::_on_set_reference_pressed() {
	String res_path;

	if (selected_exploration_index >= 0 && selected_exploration_index < exploration_thumbs.size()) {
		res_path = exploration_thumbs[selected_exploration_index].res_path;
	} else if (selected_cornerstone_index >= 0 && selected_cornerstone_index < cornerstone_thumbs.size()) {
		res_path = cornerstone_thumbs[selected_cornerstone_index].res_path;
	}

	if (res_path.is_empty()) {
		return;
	}

	String project_root = _get_project_root();
	Dictionary body_dict;
	body_dict["reference_asset"] = res_path;
	body_dict["directory"] = project_root;
	String body = JSON::stringify(body_dict);
	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	set_ref_http_request->request(service_url + "/godot/art-director/set-reference", headers, HTTPClient::METHOD_POST, body);
}

void ArtDirectorPanel::_on_exploration_refresh_pressed() {
	_refresh_images();
}

void ArtDirectorPanel::_on_cornerstone_refresh_pressed() {
	_refresh_images();
}

void ArtDirectorPanel::_on_gallery_refresh_timeout() {
	_refresh_images();
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
