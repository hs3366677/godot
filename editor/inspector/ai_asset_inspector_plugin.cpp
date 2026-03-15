/**************************************************************************/
/*  ai_asset_inspector_plugin.cpp                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                           MAKABAKA ENGINE                              */
/*                  https://github.com/makabaka-engine                    */
/**************************************************************************/
/* Copyright (c) 2024 Makabaka Engine Contributors                        */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "ai_asset_inspector_plugin.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "editor/ai_asset_generation_manager.h"
#include "editor/docks/filesystem_dock.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/file_system/editor_file_system.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/separator.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/style_box_flat.h"
#include "editor/scene/texture/texture_editor_plugin.h"

// =============================================================================
// AIAssetInspectorPlugin
// =============================================================================

void AIAssetInspectorPlugin::_bind_methods() {
}

bool AIAssetInspectorPlugin::can_handle(Object *p_object) {
	Resource *res = Object::cast_to<Resource>(p_object);
	if (res && !res->get_path().is_empty()) {
		String path = res->get_path();
		return AIAssetMetadata::get_origin(path) != AIAssetMetadata::ORIGIN_UNKNOWN;
	}
	return false;
}

bool AIAssetInspectorPlugin::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	return false;
}

void AIAssetInspectorPlugin::parse_begin(Object *p_object) {
	Resource *res = Object::cast_to<Resource>(p_object);
	if (!res || res->get_path().is_empty()) {
		return;
	}

	String path = res->get_path();
	AIAssetMetadata::Origin o = AIAssetMetadata::get_origin(path);
	if (o == AIAssetMetadata::ORIGIN_UNKNOWN) {
		return;
	}

	AIAssetInfoControl *info = memnew(AIAssetInfoControl);
	info->set_asset_path(path);
	add_custom_control(info);
}

AIAssetInspectorPlugin::AIAssetInspectorPlugin() {
}

// =============================================================================
// AIAssetInfoControl
// =============================================================================

void AIAssetInfoControl::_bind_methods() {
}

void AIAssetInfoControl::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			// Defer _update_ui so all children (HTTPRequest etc.) are fully in the tree.
			callable_mp(this, &AIAssetInfoControl::_update_ui).call_deferred();
			// Connect to pipeline state changes for elapsed time updates
			AIAssetGenerationManager *mgr = AIAssetGenerationManager::get_singleton();
			if (mgr && !mgr->is_connected("pipeline_state_changed", callable_mp(this, &AIAssetInfoControl::_on_pipeline_state_changed))) {
				mgr->connect("pipeline_state_changed", callable_mp(this, &AIAssetInfoControl::_on_pipeline_state_changed));
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			AIAssetGenerationManager *mgr = AIAssetGenerationManager::get_singleton();
			if (mgr && mgr->is_connected("pipeline_state_changed", callable_mp(this, &AIAssetInfoControl::_on_pipeline_state_changed))) {
				mgr->disconnect("pipeline_state_changed", callable_mp(this, &AIAssetInfoControl::_on_pipeline_state_changed));
			}
		} break;
		case NOTIFICATION_THEME_CHANGED: {
			_update_ui();
		} break;
	}
}

// Helper: create a sub-section title label with consistent styling.
static Label *_make_subtitle(const String &p_text) {
	Label *l = memnew(Label);
	l->set_text(p_text);
	l->add_theme_color_override("font_color", Color(0.7, 0.7, 0.7));
	return l;
}

// Helper: create a dark-background panel to wrap content.
static PanelContainer *_make_dark_panel() {
	PanelContainer *panel = memnew(PanelContainer);
	Ref<StyleBoxFlat> style;
	style.instantiate();
	style->set_bg_color(Color(0.12, 0.12, 0.12, 1.0));
	style->set_content_margin_all(6);
	style->set_corner_radius_all(3);
	panel->add_theme_style_override("panel", style);
	return panel;
}

void AIAssetInfoControl::_create_ui() {
	// ── AI section header (centered, like CompressedTexture2D type header) ──
	header_button = memnew(Button);
	header_button->set_text(TTR("AI"));
	header_button->set_text_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	header_button->set_disabled(true);
	header_button->set_focus_mode(FOCUS_NONE);
	add_child(header_button);

	// ── Usage ──
	usage_container = memnew(VBoxContainer);
	usage_container->set_visible(false);
	add_child(usage_container);

	usage_container->add_child(_make_subtitle(TTR("Usage")));

	PanelContainer *usage_panel = _make_dark_panel();
	usage_container->add_child(usage_panel);
	VBoxContainer *usage_content = memnew(VBoxContainer);
	usage_panel->add_child(usage_content);

	usage_role_label = memnew(Label);
	usage_role_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	usage_content->add_child(usage_role_label);

	usage_dimensions_label = memnew(Label);
	usage_dimensions_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	usage_content->add_child(usage_dimensions_label);

	usage_scene_label = memnew(Label);
	usage_scene_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	usage_content->add_child(usage_scene_label);

	usage_node_path_label = memnew(Label);
	usage_node_path_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	usage_content->add_child(usage_node_path_label);

	usage_extras_label = memnew(Label);
	usage_extras_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	usage_content->add_child(usage_extras_label);

	// ── Prompt ──
	prompt_title = _make_subtitle(TTR("Prompt"));
	add_child(prompt_title);

	prompt_panel = _make_dark_panel();
	add_child(prompt_panel);
	prompt_label = memnew(RichTextLabel);
	prompt_label->set_use_bbcode(false);
	prompt_label->set_fit_content(true);
	prompt_label->set_selection_enabled(true);
	prompt_label->set_context_menu_enabled(true);
	prompt_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	prompt_panel->add_child(prompt_label);

	// ── Negative Prompt ──
	negative_prompt_title = _make_subtitle(TTR("Negative Prompt"));
	add_child(negative_prompt_title);

	negative_prompt_panel = _make_dark_panel();
	add_child(negative_prompt_panel);
	negative_prompt_label = memnew(RichTextLabel);
	negative_prompt_label->set_use_bbcode(false);
	negative_prompt_label->set_fit_content(true);
	negative_prompt_label->set_selection_enabled(true);
	negative_prompt_label->set_context_menu_enabled(true);
	negative_prompt_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	negative_prompt_panel->add_child(negative_prompt_label);

	// Source label (hybrid only)
	source_label = memnew(Label);
	source_label->set_visible(false);
	add_child(source_label);

	// ── Action buttons ──
	buttons_container = memnew(HBoxContainer);
	buttons_container->set_visible(false);
	add_child(buttons_container);

	generate_button = memnew(Button);
	generate_button->set_text(TTR("Generate"));
	generate_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssetInfoControl::_on_generate_pressed));
	buttons_container->add_child(generate_button);

	enhance_button = memnew(Button);
	enhance_button->set_text(TTR("AI Enhance..."));
	enhance_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssetInfoControl::_on_enhance_pressed));
	buttons_container->add_child(enhance_button);

	view_source_button = memnew(Button);
	view_source_button->set_text(TTR("View Source"));
	view_source_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssetInfoControl::_on_view_source_pressed));
	buttons_container->add_child(view_source_button);

	replace_file_dialog = memnew(FileDialog);
	replace_file_dialog->set_file_mode(FileDialog::FILE_MODE_OPEN_FILE);
	replace_file_dialog->set_access(FileDialog::ACCESS_FILESYSTEM);
	replace_file_dialog->set_title(TTR("Import from File"));
	replace_file_dialog->connect("file_selected", callable_mp(this, &AIAssetInfoControl::_on_replace_file_selected));
	add_child(replace_file_dialog);

	// Status label (shown only during pipeline operations)
	status_label = memnew(Label);
	status_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	status_label->set_visible(false);
	add_child(status_label);

	// ── Generation History ──
	_create_history_ui();
}

void AIAssetInfoControl::_create_history_ui() {
	history_container = memnew(VBoxContainer);
	history_container->set_visible(false);
	add_child(history_container);

	history_title = _make_subtitle(TTR("Generation History"));
	history_container->add_child(history_title);

	history_list = memnew(ItemList);
	history_list->set_max_columns(1);
	history_list->set_select_mode(ItemList::SELECT_SINGLE);
	history_list->set_custom_minimum_size(Size2(0, 100));
	history_list->set_auto_height(true);
	history_list->set_v_size_flags(SIZE_EXPAND_FILL);
	history_list->connect("item_selected", callable_mp(this, &AIAssetInfoControl::_on_history_item_selected));
	history_container->add_child(history_list);

	history_buttons = memnew(HBoxContainer);
	history_container->add_child(history_buttons);

	use_version_button = memnew(Button);
	use_version_button->set_text(TTR("Use Selected"));
	use_version_button->set_disabled(true);
	use_version_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssetInfoControl::_on_use_version_pressed));
	history_buttons->add_child(use_version_button);

	delete_version_button = memnew(Button);
	delete_version_button->set_text(TTR("Delete Selected"));
	delete_version_button->set_disabled(true);
	delete_version_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssetInfoControl::_on_delete_version_pressed));
	history_buttons->add_child(delete_version_button);

	replace_file_button = memnew(Button);
	replace_file_button->set_text(TTR("Import from File..."));
	replace_file_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssetInfoControl::_on_replace_file_pressed));
	history_buttons->add_child(replace_file_button);
}

void AIAssetInfoControl::_update_ui() {
	if (!header_button) {
		return;
	}

	// Header: "AI - <origin type>"
	String origin_text;
	switch (origin) {
		case AIAssetMetadata::ORIGIN_PLACEHOLDER:
			origin_text = TTR("Placeholder");
			break;
		case AIAssetMetadata::ORIGIN_IMPORTED:
			origin_text = TTR("Imported");
			break;
		case AIAssetMetadata::ORIGIN_GENERATED:
			origin_text = TTR("Generated");
			break;
		case AIAssetMetadata::ORIGIN_HYBRID:
			origin_text = TTR("Enhanced");
			break;
		default:
			origin_text = TTR("Unknown");
	}
	header_button->set_text(vformat("AI - %s", origin_text));

	// Usage section (read-only)
	Dictionary usage = metadata.get(AIAssetMetadata::KEY_USAGE, Dictionary());
	if (!usage.is_empty()) {
		usage_container->show();

		String role = usage.get("role", "");
		usage_role_label->set_text(vformat(TTR("Role: %s"), role));
		usage_role_label->set_visible(!role.is_empty());

		int w = (int)usage.get("width", 0);
		int h = (int)usage.get("height", 0);
		bool has_transparent = usage.has("transparent_bg");
		bool transparent = has_transparent ? (bool)usage.get("transparent_bg", false) : false;
		if (w > 0 && h > 0) {
			String dim_text = vformat(TTR("Size: %dx%d"), w, h);
			if (has_transparent) {
				dim_text += vformat("\n" + TTR("Transparent: %s"), transparent ? TTR("Yes") : TTR("No"));
			}
			usage_dimensions_label->set_text(dim_text);
			usage_dimensions_label->show();
		} else if (has_transparent) {
			usage_dimensions_label->set_text(vformat(TTR("Transparent: %s"), transparent ? TTR("Yes") : TTR("No")));
			usage_dimensions_label->show();
		} else {
			usage_dimensions_label->hide();
		}

		String scene = usage.get("scene", "");
		usage_scene_label->set_text(vformat(TTR("Scene: %s"), scene));
		usage_scene_label->set_visible(!scene.is_empty());

		String node_path = usage.get("node_path", "");
		usage_node_path_label->set_text(vformat(TTR("Node: %s"), node_path));
		usage_node_path_label->set_visible(!node_path.is_empty());

		// Extras: scale, tiling, animation_frames
		Vector<String> extras;
		String scale = usage.get("scale", "");
		if (!scale.is_empty()) {
			extras.push_back(vformat("scale: %s", scale));
		}
		String tiling = usage.get("tiling", "none");
		if (tiling != "none") {
			extras.push_back(vformat("tiling: %s", tiling));
		}
		int frames = (int)usage.get("animation_frames", 0);
		if (frames > 0) {
			extras.push_back(vformat("frames: %d", frames));
		}
		if (!extras.is_empty()) {
			usage_extras_label->set_text(String(", ").join(extras));
			usage_extras_label->show();
		} else {
			usage_extras_label->hide();
		}
	} else {
		usage_container->hide();
	}

	// Populate prompt fields (read-only)
	String prompt = metadata.get(AIAssetMetadata::KEY_PROMPT, "");
	prompt_label->set_text(prompt.is_empty() ? TTR("(none)") : prompt);

	String negative_prompt = metadata.get(AIAssetMetadata::KEY_NEGATIVE_PROMPT, "");
	negative_prompt_label->set_text(negative_prompt.is_empty() ? TTR("(none)") : negative_prompt);

	// Source (hybrid only)
	String source = metadata.get(AIAssetMetadata::KEY_SOURCE_ASSET, "");
	if (!source.is_empty()) {
		source_label->set_text(vformat(TTR("Source: %s"), source));
		source_label->show();
	} else {
		source_label->hide();
	}

	// Show/hide prompt sections based on origin
	bool has_prompt = (origin == AIAssetMetadata::ORIGIN_PLACEHOLDER ||
			origin == AIAssetMetadata::ORIGIN_GENERATED ||
			origin == AIAssetMetadata::ORIGIN_HYBRID);
	prompt_title->set_visible(has_prompt);
	prompt_panel->set_visible(has_prompt);
	negative_prompt_title->set_visible(has_prompt);
	negative_prompt_panel->set_visible(has_prompt);

	// Show/hide action buttons based on origin
	bool show_gen = (origin == AIAssetMetadata::ORIGIN_PLACEHOLDER);
	bool show_enh = (origin == AIAssetMetadata::ORIGIN_IMPORTED);
	bool show_src = (origin == AIAssetMetadata::ORIGIN_HYBRID);
	generate_button->set_visible(show_gen);
	enhance_button->set_visible(show_enh);
	view_source_button->set_visible(show_src);
	buttons_container->set_visible(show_gen || show_enh || show_src);

	// Reset history view state
	is_viewing_history = false;
	viewed_version = -1;
	original_preview_texture.unref();
	_set_editing_enabled(true);

	_update_history_list();
}

void AIAssetInfoControl::_set_editing_enabled(bool p_enabled) {
	generate_button->set_disabled(!p_enabled);

	// Status label no longer used for view state — kept for pipeline status only.
	status_label->set_text("");
}

void AIAssetInfoControl::_populate_from_version(const Dictionary &p_version_meta) {
	String prompt = p_version_meta.get(AIAssetMetadata::KEY_PROMPT, "");
	prompt_label->set_text(prompt.is_empty() ? TTR("(none)") : prompt);
	String neg = p_version_meta.get(AIAssetMetadata::KEY_NEGATIVE_PROMPT, "");
	negative_prompt_label->set_text(neg.is_empty() ? TTR("(none)") : neg);
}

// ── History ──────────────────────────────────────────────────────────────

void AIAssetInfoControl::_update_history_list() {
	if (!history_list) {
		return;
	}

	history_list->clear();

	Array versions = AIAssetMetadata::list_versions(asset_path);

	// Always show history section for trackable origins
	bool has_history_section = (origin == AIAssetMetadata::ORIGIN_GENERATED ||
			origin == AIAssetMetadata::ORIGIN_HYBRID ||
			origin == AIAssetMetadata::ORIGIN_IMPORTED ||
			origin == AIAssetMetadata::ORIGIN_PLACEHOLDER);
	history_container->set_visible(has_history_section);

	if (versions.size() == 0) {
		history_list->add_item(TTR("No generation history yet"));
		history_list->set_item_disabled(0, true);
		use_version_button->set_disabled(true);
		delete_version_button->set_disabled(true);
		return;
	}

	for (int i = versions.size() - 1; i >= 0; i--) {
		Dictionary entry = versions[i];
		int ver = (int)entry.get(AIAssetMetadata::KEY_VERSION, 0);
		bool is_current = (bool)entry.get("is_current", false);
		bool file_exists = (bool)entry.get("file_exists", false);

		String label;
		if (is_current) {
			label = vformat(U"\u2605 v%d (current)", ver);
		} else {
			label = vformat(U"   v%d", ver);
		}

		int idx = history_list->get_item_count();
		history_list->add_item(label);
		history_list->set_item_metadata(idx, ver);
		history_list->set_item_disabled(idx, !file_exists);
	}

	use_version_button->set_disabled(true);
	delete_version_button->set_disabled(true);
}

// ── Button Handlers ──────────────────────────────────────────────────────

void AIAssetInfoControl::set_asset_path(const String &p_path) {
	asset_path = p_path;
	origin = AIAssetMetadata::get_origin(p_path);
	metadata = AIAssetMetadata::get_metadata(p_path);
	// Don't call _update_ui() here — the node may not be in the tree yet.
	// NOTIFICATION_ENTER_TREE will call _update_ui() via call_deferred.
	if (is_inside_tree()) {
		_update_ui();
	}
}

void AIAssetInfoControl::_on_generate_pressed() {
	AIAssetGenerationManager *mgr = AIAssetGenerationManager::get_singleton();
	if (!mgr) {
		return;
	}
	// If pipeline is running for this asset, stop it
	if (mgr->get_pipeline_asset_path() == asset_path) {
		mgr->cancel_pipeline();
		return;
	}
	if (mgr->is_busy()) {
		return;
	}
	String sel_model = metadata.get(AIAssetMetadata::KEY_MODEL, "");
	print_line(vformat("[AIInspector] Generate pressed: path='%s' model='%s'", asset_path, sel_model));
	mgr->generate_with_params(asset_path, get_prompt(), get_negative_prompt(), sel_model, -1);
}

void AIAssetInfoControl::_on_pipeline_state_changed(const String &p_path, bool p_active, double p_elapsed) {
	if (p_path != asset_path) {
		return; // Not our asset
	}

	if (p_active) {
		// Show elapsed time — button acts as Stop button (stays enabled)
		int secs = (int)p_elapsed;
		int mins = secs / 60;
		secs = secs % 60;
		String time_str = mins > 0 ? vformat("%d:%02d", mins, secs) : vformat("%ds", secs);
		String stop_text = vformat(TTR("Stop (%s)"), time_str);
		generate_button->set_text(stop_text);
		generate_button->set_disabled(false);
	} else {
		// Pipeline finished — restore button text and refresh UI
		generate_button->set_text(TTR("Generate"));
		generate_button->set_disabled(false);
		// Reload metadata and refresh the inspector
		callable_mp(this, &AIAssetInfoControl::_update_ui).call_deferred();
	}
}

void AIAssetInfoControl::_on_enhance_pressed() {
	if (AIAssetGenerationManager::get_singleton()) {
		AIAssetGenerationManager::get_singleton()->open_enhance_dialog(asset_path);
	}
}

void AIAssetInfoControl::_on_view_source_pressed() {
	String source = metadata.get(AIAssetMetadata::KEY_SOURCE_ASSET, "");
	if (!source.is_empty() && FileSystemDock::get_singleton()) {
		FileSystemDock::get_singleton()->navigate_to_path(source);
	}
}

void AIAssetInfoControl::_on_replace_file_pressed() {
	replace_file_dialog->clear_filters();
	String asset_type = metadata.get(AIAssetMetadata::KEY_ASSET_TYPE, "");
	if (asset_type == "model" || asset_type == "mesh" || asset_type == "scene" || asset_path.ends_with(".glb")) {
		replace_file_dialog->add_filter("*.glb, *.gltf", TTR("3D Models"));
	} else {
		replace_file_dialog->add_filter("*.png, *.jpg, *.jpeg, *.webp", TTR("Images"));
	}
	replace_file_dialog->popup_centered(Size2(600, 400));
}

void AIAssetInfoControl::_on_replace_file_selected(const String &p_path) {
	// Save current as version before replacing.
	AIAssetMetadata::save_version(asset_path);

	// Copy source file to destination.
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String global_dest = ProjectSettings::get_singleton()->globalize_path(asset_path);
	da->copy(p_path, global_dest);

	// Update metadata: origin → IMPORTED, preserve prompt/provider, add import info.
	Dictionary meta = AIAssetMetadata::get_metadata(asset_path);
	meta[AIAssetMetadata::KEY_ORIGIN] = AIAssetMetadata::origin_to_string(AIAssetMetadata::ORIGIN_IMPORTED);
	meta[AIAssetMetadata::KEY_IMPORTED_FROM] = p_path;
	meta[AIAssetMetadata::KEY_IMPORTED_AT] = AIAssetMetadata::get_current_timestamp();
	meta[AIAssetMetadata::KEY_ORIGINAL_FILENAME] = p_path.get_file();
	int new_version = (int)meta.get(AIAssetMetadata::KEY_VERSION, 0) + 1;
	meta[AIAssetMetadata::KEY_VERSION] = new_version;
	AIAssetMetadata::set_metadata(asset_path, meta);

	// Save the imported version too (so it appears in history).
	AIAssetMetadata::save_version(asset_path);

	// Refresh.
	EditorFileSystem::get_singleton()->scan_changes();
	set_asset_path(asset_path);
}

// Find the TexturePreview sibling to update the existing preview.
TexturePreview *AIAssetInfoControl::_find_texture_preview() {
	Node *parent = get_parent();
	if (!parent) {
		return nullptr;
	}
	for (int i = 0; i < parent->get_child_count(); i++) {
		TexturePreview *tp = Object::cast_to<TexturePreview>(parent->get_child(i));
		if (tp) {
			return tp;
		}
	}
	return nullptr;
}

void AIAssetInfoControl::_on_history_item_selected(int p_index) {
	int selected_version = history_list->get_item_metadata(p_index);
	int current_version = AIAssetMetadata::get_current_version(asset_path);
	bool is_current = (selected_version == current_version);
	bool is_disabled = history_list->is_item_disabled(p_index);

	use_version_button->set_disabled(is_current || is_disabled);
	delete_version_button->set_disabled(is_current || is_disabled);

	TexturePreview *preview = _find_texture_preview();

	if (is_current) {
		// Restore current metadata to fields
		is_viewing_history = false;
		viewed_version = -1;
		String cur_prompt = metadata.get(AIAssetMetadata::KEY_PROMPT, "");
		prompt_label->set_text(cur_prompt.is_empty() ? TTR("(none)") : cur_prompt);
		String cur_neg = metadata.get(AIAssetMetadata::KEY_NEGATIVE_PROMPT, "");
		negative_prompt_label->set_text(cur_neg.is_empty() ? TTR("(none)") : cur_neg);
		_set_editing_enabled(true);

		// Restore original texture in the existing preview
		if (preview && original_preview_texture.is_valid()) {
			TextureRect *display = preview->get_texture_display();
			display->set_texture(original_preview_texture);
			preview->_update_texture_display_ratio();
		}
	} else {
		// Load historical version metadata into fields (read-only)
		Dictionary ver_meta = AIAssetMetadata::read_version_meta(asset_path, selected_version);
		is_viewing_history = true;
		viewed_version = selected_version;
		_populate_from_version(ver_meta);
		_set_editing_enabled(false);

		// Load version image into the existing inspector preview
		if (preview) {
			TextureRect *display = preview->get_texture_display();
			// Save original texture on first switch
			if (original_preview_texture.is_null()) {
				original_preview_texture = display->get_texture();
			}
			String ver_file = AIAssetMetadata::get_version_file_path(asset_path, selected_version);
			String global_ver_file = ProjectSettings::get_singleton()->globalize_path(ver_file);
			Ref<Image> img;
			img.instantiate();
			if (img->load(global_ver_file) == OK) {
				Ref<ImageTexture> tex = ImageTexture::create_from_image(img);
				display->set_texture(tex);
				preview->_update_texture_display_ratio();
			}
		}
	}
}

void AIAssetInfoControl::_on_use_version_pressed() {
	Vector<int> selected = history_list->get_selected_items();
	if (selected.is_empty()) {
		return;
	}

	int version = history_list->get_item_metadata(selected[0]);
	Error err = AIAssetMetadata::use_version(asset_path, version);
	if (err != OK) {
		ERR_PRINT(vformat("Failed to use version %d of %s", version, asset_path));
		return;
	}

	// Re-apply post-processing settings (transparent BG, crop, resize)
	if (AIAssetGenerationManager::get_singleton()) {
		AIAssetGenerationManager::get_singleton()->post_process_asset(asset_path);
	}

	EditorFileSystem::get_singleton()->scan_changes();
	set_asset_path(asset_path);
}

void AIAssetInfoControl::_on_delete_version_pressed() {
	Vector<int> selected = history_list->get_selected_items();
	if (selected.is_empty()) {
		return;
	}

	int version = history_list->get_item_metadata(selected[0]);
	Error err = AIAssetMetadata::delete_version(asset_path, version);
	if (err != OK) {
		ERR_PRINT(vformat("Failed to delete version %d of %s", version, asset_path));
		return;
	}

	EditorFileSystem::get_singleton()->scan_changes();
	_update_history_list();
}

// ── Getters ──────────────────────────────────────────────────────────────

String AIAssetInfoControl::get_prompt() const {
	return metadata.get(AIAssetMetadata::KEY_PROMPT, "");
}

String AIAssetInfoControl::get_negative_prompt() const {
	return metadata.get(AIAssetMetadata::KEY_NEGATIVE_PROMPT, "");
}

AIAssetInfoControl::AIAssetInfoControl() {
	set_name("AIAssetInfo");
	_create_ui();
}
