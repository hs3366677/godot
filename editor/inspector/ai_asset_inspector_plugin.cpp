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

#include "core/io/resource_loader.h"
#include "editor/docks/filesystem_dock.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/file_system/editor_file_system.h"
#include "scene/gui/separator.h"
#include "servers/display/display_server.h"

// =============================================================================
// AIAssetInspectorPlugin
// =============================================================================

void AIAssetInspectorPlugin::_bind_methods() {
}

bool AIAssetInspectorPlugin::can_handle(Object *p_object) {
	// Handle Resource objects that have a path
	Resource *res = Object::cast_to<Resource>(p_object);
	if (res && !res->get_path().is_empty()) {
		String path = res->get_path();
		// Check if this resource has AI metadata
		return AIAssetMetadata::get_origin(path) != AIAssetMetadata::ORIGIN_UNKNOWN;
	}
	return false;
}

bool AIAssetInspectorPlugin::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	return false; // Let default inspector handle properties
}

void AIAssetInspectorPlugin::parse_begin(Object *p_object) {
	Resource *res = Object::cast_to<Resource>(p_object);
	if (!res || res->get_path().is_empty()) {
		return;
	}

	String path = res->get_path();
	AIAssetMetadata::Origin origin = AIAssetMetadata::get_origin(path);

	if (origin == AIAssetMetadata::ORIGIN_UNKNOWN) {
		return;
	}

	// Create the AI asset info control
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
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			_update_ui();
		} break;
	}
}

void AIAssetInfoControl::_create_ui() {
	// Header with origin badge
	HBoxContainer *header = memnew(HBoxContainer);
	add_child(header);

	origin_label = memnew(Label);
	origin_label->add_theme_font_size_override(SceneStringName(font_size), 14);
	header->add_child(origin_label);

	add_child(memnew(HSeparator));

	// Prompt (for generated/placeholder/hybrid)
	prompt_label = memnew(RichTextLabel);
	prompt_label->set_use_bbcode(true);
	prompt_label->set_fit_content(true);
	prompt_label->set_selection_enabled(true);
	prompt_label->set_custom_minimum_size(Size2(0, 60));
	add_child(prompt_label);

	// Provider & Model
	HBoxContainer *provider_row = memnew(HBoxContainer);
	add_child(provider_row);

	provider_label = memnew(Label);
	provider_row->add_child(provider_label);

	Control *spacer = memnew(Control);
	spacer->set_h_size_flags(SIZE_EXPAND_FILL);
	provider_row->add_child(spacer);

	model_label = memnew(Label);
	provider_row->add_child(model_label);

	// Version (for generated)
	version_label = memnew(Label);
	add_child(version_label);

	// Source asset (for hybrid)
	source_label = memnew(Label);
	add_child(source_label);

	add_child(memnew(HSeparator));

	// Action buttons
	buttons_container = memnew(HBoxContainer);
	add_child(buttons_container);

	generate_button = memnew(Button);
	generate_button->set_text(TTR("Generate"));
	generate_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssetInfoControl::_on_generate_pressed));
	buttons_container->add_child(generate_button);

	edit_prompt_button = memnew(Button);
	edit_prompt_button->set_text(TTR("Edit Prompt..."));
	edit_prompt_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssetInfoControl::_on_edit_prompt_pressed));
	buttons_container->add_child(edit_prompt_button);

	quick_regen_button = memnew(Button);
	quick_regen_button->set_text(TTR("Quick Regenerate"));
	quick_regen_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssetInfoControl::_on_quick_regen_pressed));
	buttons_container->add_child(quick_regen_button);

	enhance_button = memnew(Button);
	enhance_button->set_text(TTR("AI Enhance..."));
	enhance_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssetInfoControl::_on_enhance_pressed));
	buttons_container->add_child(enhance_button);

	copy_prompt_button = memnew(Button);
	copy_prompt_button->set_text(TTR("Copy Prompt"));
	copy_prompt_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssetInfoControl::_on_copy_prompt_pressed));
	buttons_container->add_child(copy_prompt_button);

	view_source_button = memnew(Button);
	view_source_button->set_text(TTR("View Source"));
	view_source_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssetInfoControl::_on_view_source_pressed));
	buttons_container->add_child(view_source_button);

	_create_history_ui();
}

void AIAssetInfoControl::_create_history_ui() {
	add_child(memnew(HSeparator));

	history_container = memnew(VBoxContainer);
	history_container->set_visible(false);
	add_child(history_container);

	history_title = memnew(Label);
	history_title->set_text(TTR("Generation History"));
	history_title->add_theme_font_size_override(SceneStringName(font_size), 13);
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
}

void AIAssetInfoControl::_update_ui() {
	if (!origin_label) {
		return;
	}

	// Update origin badge
	String origin_text;
	Color badge_color;

	switch (origin) {
		case AIAssetMetadata::ORIGIN_PLACEHOLDER:
			origin_text = TTR("AI Placeholder");
			badge_color = Color(1.0, 0.5, 0.0); // Orange
			break;
		case AIAssetMetadata::ORIGIN_IMPORTED:
			origin_text = TTR("Imported");
			badge_color = Color(0.4, 0.7, 1.0); // Blue
			break;
		case AIAssetMetadata::ORIGIN_GENERATED:
			origin_text = TTR("AI Generated");
			badge_color = Color(0.5, 1.0, 0.5); // Green
			break;
		case AIAssetMetadata::ORIGIN_HYBRID:
			origin_text = TTR("AI Enhanced");
			badge_color = Color(1.0, 0.5, 1.0); // Purple
			break;
		default:
			origin_text = TTR("Unknown");
			badge_color = Color(0.6, 0.6, 0.6);
	}

	origin_label->set_text(origin_text);
	origin_label->add_theme_color_override("font_color", badge_color);

	// Update prompt display
	String prompt = metadata.get(AIAssetMetadata::KEY_PROMPT, "");
	if (!prompt.is_empty()) {
		prompt_label->set_text(vformat("[i]%s[/i]", prompt));
		prompt_label->show();
	} else {
		prompt_label->hide();
	}

	// Update provider/model
	String provider = metadata.get(AIAssetMetadata::KEY_PROVIDER, "");
	String model = metadata.get(AIAssetMetadata::KEY_MODEL, "");
	if (!provider.is_empty()) {
		provider_label->set_text(vformat(TTR("Provider: %s"), provider));
		provider_label->show();
	} else {
		provider_label->hide();
	}
	if (!model.is_empty()) {
		model_label->set_text(vformat(TTR("Model: %s"), model));
		model_label->show();
	} else {
		model_label->hide();
	}

	// Update version
	int version = metadata.get(AIAssetMetadata::KEY_VERSION, 0);
	if (version > 0) {
		version_label->set_text(vformat(TTR("Version: %d"), version));
		version_label->show();
	} else {
		version_label->hide();
	}

	// Update source (for hybrid)
	String source = metadata.get(AIAssetMetadata::KEY_SOURCE_ASSET, "");
	if (!source.is_empty()) {
		source_label->set_text(vformat(TTR("Source: %s"), source));
		source_label->show();
	} else {
		source_label->hide();
	}

	// Show/hide buttons based on origin
	generate_button->set_visible(origin == AIAssetMetadata::ORIGIN_PLACEHOLDER);
	edit_prompt_button->set_visible(origin == AIAssetMetadata::ORIGIN_PLACEHOLDER ||
			origin == AIAssetMetadata::ORIGIN_GENERATED ||
			origin == AIAssetMetadata::ORIGIN_HYBRID);
	quick_regen_button->set_visible(origin == AIAssetMetadata::ORIGIN_GENERATED);
	enhance_button->set_visible(origin == AIAssetMetadata::ORIGIN_IMPORTED);
	copy_prompt_button->set_visible(!prompt.is_empty());
	view_source_button->set_visible(origin == AIAssetMetadata::ORIGIN_HYBRID);

	_update_history_list();
}

void AIAssetInfoControl::_update_history_list() {
	if (!history_list) {
		return;
	}

	history_list->clear();

	Array versions = AIAssetMetadata::list_versions(asset_path);
	bool is_generated = (origin == AIAssetMetadata::ORIGIN_GENERATED ||
			origin == AIAssetMetadata::ORIGIN_HYBRID);
	history_container->set_visible(is_generated && versions.size() > 0);

	if (versions.size() == 0) {
		return;
	}

	for (int i = versions.size() - 1; i >= 0; i--) {
		Dictionary entry = versions[i];
		int ver = (int)entry.get(AIAssetMetadata::KEY_VERSION, 0);
		String ver_prompt = entry.get(AIAssetMetadata::KEY_PROMPT, "");
		bool is_current = (bool)entry.get("is_current", false);
		bool file_exists = (bool)entry.get("file_exists", false);

		String truncated = ver_prompt.length() > 40 ? ver_prompt.left(40) + "..." : ver_prompt;
		String label;
		if (is_current) {
			label = vformat(U"★ v%d \u2014 %s", ver, truncated);
		} else {
			label = vformat(U"   v%d \u2014 %s", ver, truncated);
		}

		int idx = history_list->get_item_count();
		history_list->add_item(label);
		history_list->set_item_metadata(idx, ver);
		history_list->set_item_tooltip(idx, ver_prompt);
		history_list->set_item_disabled(idx, !file_exists);
	}

	use_version_button->set_disabled(true);
	delete_version_button->set_disabled(true);
}

void AIAssetInfoControl::set_asset_path(const String &p_path) {
	asset_path = p_path;
	origin = AIAssetMetadata::get_origin(p_path);
	metadata = AIAssetMetadata::get_metadata(p_path);
	_update_ui();
}

void AIAssetInfoControl::_on_generate_pressed() {
	// TODO: Call OpenCode API to generate from placeholder
	print_line(vformat("TODO: Generate from placeholder: %s", asset_path));
}

void AIAssetInfoControl::_on_edit_prompt_pressed() {
	// TODO: Open prompt editor dialog
	print_line(vformat("TODO: Open prompt editor for: %s", asset_path));
}

void AIAssetInfoControl::_on_quick_regen_pressed() {
	// TODO: Call OpenCode API to regenerate with new seed
	print_line(vformat("TODO: Quick regenerate: %s", asset_path));
}

void AIAssetInfoControl::_on_enhance_pressed() {
	// TODO: Open AI enhance dialog
	print_line(vformat("TODO: Open AI enhance dialog for: %s", asset_path));
}

void AIAssetInfoControl::_on_copy_prompt_pressed() {
	String prompt = metadata.get(AIAssetMetadata::KEY_PROMPT, "");
	if (!prompt.is_empty()) {
		DisplayServer::get_singleton()->clipboard_set(prompt);
	}
}

void AIAssetInfoControl::_on_view_source_pressed() {
	String source = metadata.get(AIAssetMetadata::KEY_SOURCE_ASSET, "");
	if (!source.is_empty() && FileSystemDock::get_singleton()) {
		FileSystemDock::get_singleton()->navigate_to_path(source);
	}
}

void AIAssetInfoControl::_on_history_item_selected(int p_index) {
	int selected_version = history_list->get_item_metadata(p_index);
	int current_version = AIAssetMetadata::get_current_version(asset_path);
	bool is_current = (selected_version == current_version);
	bool is_disabled = history_list->is_item_disabled(p_index);

	use_version_button->set_disabled(is_current || is_disabled);
	delete_version_button->set_disabled(is_current || is_disabled);
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

AIAssetInfoControl::AIAssetInfoControl() {
	set_name("AIAssetInfo");
	_create_ui();
}
