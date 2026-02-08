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
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "editor/ai_asset_generation_manager.h"
#include "editor/docks/filesystem_dock.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/file_system/editor_file_system.h"
#include "scene/gui/separator.h"
#include "scene/main/http_request.h"

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
		} break;
		case NOTIFICATION_THEME_CHANGED: {
			_update_ui();
		} break;
	}
}

void AIAssetInfoControl::_create_ui() {
	// 1. Origin badge
	HBoxContainer *header = memnew(HBoxContainer);
	add_child(header);

	origin_label = memnew(Label);
	origin_label->add_theme_font_size_override(SceneStringName(font_size), 14);
	header->add_child(origin_label);

	add_child(memnew(HSeparator));

	// 2. Prompt (editable TextEdit)
	prompt_title = memnew(Label);
	prompt_title->set_text(TTR("Prompt:"));
	add_child(prompt_title);

	prompt_edit = memnew(TextEdit);
	prompt_edit->set_custom_minimum_size(Size2(0, 200)); // 5 lines
	prompt_edit->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	prompt_edit->set_scroll_past_end_of_file_enabled(false);
	add_child(prompt_edit);

	// 3. Negative prompt (editable TextEdit)
	negative_prompt_title = memnew(Label);
	negative_prompt_title->set_text(TTR("Negative Prompt:"));
	add_child(negative_prompt_title);

	negative_prompt_edit = memnew(TextEdit);
	negative_prompt_edit->set_custom_minimum_size(Size2(0, 130)); // 3 lines
	negative_prompt_edit->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	negative_prompt_edit->set_scroll_past_end_of_file_enabled(false);
	add_child(negative_prompt_edit);

	add_child(memnew(HSeparator));

	// 4. Provider & Model row
	HBoxContainer *model_row = memnew(HBoxContainer);
	add_child(model_row);

	provider_label = memnew(Label);
	model_row->add_child(provider_label);

	Control *spacer1 = memnew(Control);
	spacer1->set_h_size_flags(SIZE_EXPAND_FILL);
	model_row->add_child(spacer1);

	Label *model_title = memnew(Label);
	model_title->set_text(TTR("Model:"));
	model_row->add_child(model_title);

	model_selector = memnew(OptionButton);
	model_selector->set_custom_minimum_size(Size2(120, 0));
	model_row->add_child(model_selector);

	// 5. Seed & options row
	seed_container = memnew(HBoxContainer);
	add_child(seed_container);

	Label *seed_title = memnew(Label);
	seed_title->set_text(TTR("Seed:"));
	seed_container->add_child(seed_title);

	seed_spinbox = memnew(SpinBox);
	seed_spinbox->set_min(-1);
	seed_spinbox->set_max(999999999);
	seed_spinbox->set_value(-1);
	seed_spinbox->set_tooltip_text(TTR("-1 for random seed"));
	seed_container->add_child(seed_spinbox);

	random_seed_button = memnew(Button);
	random_seed_button->set_text(TTR("Random"));
	random_seed_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssetInfoControl::_on_random_seed_pressed));
	seed_container->add_child(random_seed_button);

	Control *spacer2 = memnew(Control);
	spacer2->set_h_size_flags(SIZE_EXPAND_FILL);
	seed_container->add_child(spacer2);

	transparent_bg_checkbox = memnew(CheckBox);
	transparent_bg_checkbox->set_text(TTR("Transparent BG"));
	transparent_bg_checkbox->set_tooltip_text(TTR("Convert white/near-white pixels to transparent after generation"));
	seed_container->add_child(transparent_bg_checkbox);

	add_child(memnew(HSeparator));

	// 6. AI Assist section
	ai_assist_container = memnew(VBoxContainer);
	add_child(ai_assist_container);

	Label *ai_assist_title = memnew(Label);
	ai_assist_title->set_text(TTR("AI Assist:"));
	ai_assist_container->add_child(ai_assist_title);

	HBoxContainer *instruction_row = memnew(HBoxContainer);
	ai_assist_container->add_child(instruction_row);

	instruction_edit = memnew(LineEdit);
	instruction_edit->set_placeholder(TTR("e.g., \"make it more cartoon-like\""));
	instruction_edit->set_h_size_flags(SIZE_EXPAND_FILL);
	instruction_row->add_child(instruction_edit);

	refine_button = memnew(Button);
	refine_button->set_text(TTR("Refine Prompt"));
	refine_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssetInfoControl::_on_refine_pressed));
	instruction_row->add_child(refine_button);

	add_child(memnew(HSeparator));

	// 7. Version label
	version_label = memnew(Label);
	add_child(version_label);

	// 8. Source label (hybrid only)
	source_label = memnew(Label);
	add_child(source_label);

	// 9. Action buttons (no Edit Prompt, no Copy Prompt)
	buttons_container = memnew(HBoxContainer);
	add_child(buttons_container);

	generate_button = memnew(Button);
	generate_button->set_text(TTR("Generate"));
	generate_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssetInfoControl::_on_generate_pressed));
	buttons_container->add_child(generate_button);

	quick_regen_button = memnew(Button);
	quick_regen_button->set_text(TTR("Quick Regenerate"));
	quick_regen_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssetInfoControl::_on_quick_regen_pressed));
	buttons_container->add_child(quick_regen_button);

	enhance_button = memnew(Button);
	enhance_button->set_text(TTR("AI Enhance..."));
	enhance_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssetInfoControl::_on_enhance_pressed));
	buttons_container->add_child(enhance_button);

	view_source_button = memnew(Button);
	view_source_button->set_text(TTR("View Source"));
	view_source_button->connect(SceneStringName(pressed), callable_mp(this, &AIAssetInfoControl::_on_view_source_pressed));
	buttons_container->add_child(view_source_button);

	// 10. Status label
	status_label = memnew(Label);
	status_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	add_child(status_label);

	// 11. History section
	_create_history_ui();

	// 12. HTTP request nodes
	refine_request = memnew(HTTPRequest);
	add_child(refine_request);
	refine_request->connect("request_completed", callable_mp(this, &AIAssetInfoControl::_on_refine_completed));

	models_request = memnew(HTTPRequest);
	add_child(models_request);
	models_request->connect("request_completed", callable_mp(this, &AIAssetInfoControl::_on_models_received));
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

	// Origin badge
	String origin_text;
	Color badge_color;
	switch (origin) {
		case AIAssetMetadata::ORIGIN_PLACEHOLDER:
			origin_text = TTR("AI Placeholder");
			badge_color = Color(1.0, 0.5, 0.0);
			break;
		case AIAssetMetadata::ORIGIN_IMPORTED:
			origin_text = TTR("Imported");
			badge_color = Color(0.4, 0.7, 1.0);
			break;
		case AIAssetMetadata::ORIGIN_GENERATED:
			origin_text = TTR("AI Generated");
			badge_color = Color(0.5, 1.0, 0.5);
			break;
		case AIAssetMetadata::ORIGIN_HYBRID:
			origin_text = TTR("AI Enhanced");
			badge_color = Color(1.0, 0.5, 1.0);
			break;
		default:
			origin_text = TTR("Unknown");
			badge_color = Color(0.6, 0.6, 0.6);
	}
	origin_label->set_text(origin_text);
	origin_label->add_theme_color_override("font_color", badge_color);

	// Populate prompt fields
	String prompt = metadata.get(AIAssetMetadata::KEY_PROMPT, "");
	prompt_edit->set_text(prompt);

	String negative_prompt = metadata.get(AIAssetMetadata::KEY_NEGATIVE_PROMPT, "");
	negative_prompt_edit->set_text(negative_prompt);

	// Provider
	String provider = metadata.get(AIAssetMetadata::KEY_PROVIDER, "");
	if (!provider.is_empty()) {
		provider_label->set_text(vformat(TTR("Provider: %s"), provider));
		provider_label->show();
	} else {
		provider_label->hide();
	}

	// Load models async
	_load_models();

	// Seed
	int seed = (int)metadata.get(AIAssetMetadata::KEY_SEED, -1);
	seed_spinbox->set_value(seed);

	// Transparent BG
	Dictionary params = metadata.get(AIAssetMetadata::KEY_PARAMETERS, Dictionary());
	transparent_bg_checkbox->set_pressed((bool)params.get("transparent_bg", false));

	// Version
	int version = (int)metadata.get(AIAssetMetadata::KEY_VERSION, 0);
	if (version > 0) {
		version_label->set_text(vformat(TTR("Version: %d"), version));
		version_label->show();
	} else {
		version_label->hide();
	}

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
	prompt_edit->set_visible(has_prompt);
	negative_prompt_title->set_visible(has_prompt);
	negative_prompt_edit->set_visible(has_prompt);
	ai_assist_container->set_visible(has_prompt);
	seed_container->set_visible(has_prompt);

	// Show/hide action buttons based on origin
	generate_button->set_visible(origin == AIAssetMetadata::ORIGIN_PLACEHOLDER);
	quick_regen_button->set_visible(origin == AIAssetMetadata::ORIGIN_GENERATED);
	enhance_button->set_visible(origin == AIAssetMetadata::ORIGIN_IMPORTED);
	view_source_button->set_visible(origin == AIAssetMetadata::ORIGIN_HYBRID);

	// Reset history view state
	is_viewing_history = false;
	viewed_version = -1;
	_set_editing_enabled(true);

	_update_history_list();
}

void AIAssetInfoControl::_set_editing_enabled(bool p_enabled) {
	prompt_edit->set_editable(p_enabled);
	negative_prompt_edit->set_editable(p_enabled);
	model_selector->set_disabled(!p_enabled);
	seed_spinbox->set_editable(p_enabled);
	random_seed_button->set_disabled(!p_enabled);
	transparent_bg_checkbox->set_disabled(!p_enabled);
	instruction_edit->set_editable(p_enabled);
	refine_button->set_disabled(!p_enabled);
	generate_button->set_disabled(!p_enabled);
	quick_regen_button->set_disabled(!p_enabled);

	if (!p_enabled) {
		status_label->set_text(TTR("Viewing historical version (read-only)"));
	} else {
		status_label->set_text("");
	}
}

void AIAssetInfoControl::_populate_from_version(const Dictionary &p_version_meta) {
	prompt_edit->set_text(p_version_meta.get(AIAssetMetadata::KEY_PROMPT, ""));
	negative_prompt_edit->set_text(p_version_meta.get(AIAssetMetadata::KEY_NEGATIVE_PROMPT, ""));

	String model = p_version_meta.get(AIAssetMetadata::KEY_MODEL, "");
	for (int i = 0; i < model_selector->get_item_count(); i++) {
		Variant item_meta = model_selector->get_item_metadata(i);
		String item_model = (item_meta.get_type() == Variant::STRING) ? String(item_meta) : model_selector->get_item_text(i);
		if (item_model == model) {
			model_selector->select(i);
			break;
		}
	}

	seed_spinbox->set_value((int)p_version_meta.get(AIAssetMetadata::KEY_SEED, -1));

	Dictionary params = p_version_meta.get(AIAssetMetadata::KEY_PARAMETERS, Dictionary());
	transparent_bg_checkbox->set_pressed((bool)params.get("transparent_bg", false));

	int ver = (int)p_version_meta.get(AIAssetMetadata::KEY_VERSION, 0);
	version_label->set_text(vformat(TTR("Version: %d (historical)"), ver));
}

void AIAssetInfoControl::_save_fields_to_metadata() {
	Dictionary asset_meta = AIAssetMetadata::get_metadata(asset_path);
	asset_meta[AIAssetMetadata::KEY_PROMPT] = get_prompt();
	asset_meta[AIAssetMetadata::KEY_NEGATIVE_PROMPT] = get_negative_prompt();
	asset_meta[AIAssetMetadata::KEY_MODEL] = get_selected_model();
	asset_meta[AIAssetMetadata::KEY_SEED] = get_seed();

	Dictionary params = asset_meta.get(AIAssetMetadata::KEY_PARAMETERS, Dictionary());
	if (get_transparent_bg()) {
		params["transparent_bg"] = true;
	} else {
		params.erase("transparent_bg");
	}
	asset_meta[AIAssetMetadata::KEY_PARAMETERS] = params;

	AIAssetMetadata::set_metadata(asset_path, asset_meta);
	metadata = asset_meta;
}

// ── HTTP Methods ─────────────────────────────────────────────────────────

Vector<String> AIAssetInfoControl::_get_headers() const {
	Vector<String> headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back("Accept: application/json");
	String project_path = ProjectSettings::get_singleton()->get_resource_path();
	headers.push_back("x-opencode-directory: " + ProjectSettings::get_singleton()->globalize_path(project_path));
	return headers;
}

void AIAssetInfoControl::_load_models() {
	if (!is_inside_tree() || !models_request->is_inside_tree()) {
		// Not in tree yet; the deferred _update_ui call will retry.
		return;
	}

	model_selector->clear();
	model_selector->add_item(TTR("Loading..."), 0);

	// Cancel any in-flight request before starting a new one.
	models_request->cancel_request();

	String url = service_url + "/ai-assets/models";
	Error err = models_request->request(url, _get_headers());
	if (err != OK) {
		model_selector->clear();
		model_selector->add_item("default", 0);
	}
}

void AIAssetInfoControl::_on_models_received(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	model_selector->clear();

	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		model_selector->add_item("default", 0);
		return;
	}

	String response_str = String::utf8((const char *)p_body.ptr(), p_body.size());
	JSON json;
	if (json.parse(response_str) != OK) {
		model_selector->add_item("default", 0);
		return;
	}

	Variant data = json.get_data();
	if (data.get_type() != Variant::DICTIONARY) {
		model_selector->add_item("default", 0);
		return;
	}

	Dictionary models_by_provider = data;
	String current_model = metadata.get(AIAssetMetadata::KEY_MODEL, "");
	int select_idx = -1;

	Array provider_ids = models_by_provider.keys();
	for (int p = 0; p < provider_ids.size(); p++) {
		String provider_id = provider_ids[p];
		Variant provider_val = models_by_provider[provider_id];
		if (provider_val.get_type() != Variant::ARRAY) {
			continue;
		}
		Array models = provider_val;
		for (int m = 0; m < models.size(); m++) {
			Dictionary model_dict = models[m];
			String model_id = model_dict.get("id", "");

			// Build display label with price if available
			String label = model_id;
			if (model_dict.has("pricing")) {
				Dictionary pricing = model_dict["pricing"];
				double cost = (double)pricing.get("cost", 0.0);
				if (cost > 0) {
					label = vformat("%s ($%s)", model_id, String::num(cost, cost < 0.01 ? 4 : 3));
				}
			}

			int idx = model_selector->get_item_count();
			model_selector->add_item(label, idx);
			model_selector->set_item_metadata(idx, model_id);
			if (model_id == current_model) {
				select_idx = idx;
			}
		}
	}

	if (model_selector->get_item_count() == 0) {
		model_selector->add_item("default", 0);
	}

	if (select_idx >= 0) {
		model_selector->select(select_idx);
	}
}

void AIAssetInfoControl::_on_refine_pressed() {
	String instruction = instruction_edit->get_text().strip_edges();
	if (instruction.is_empty()) {
		return;
	}

	String current_prompt = prompt_edit->get_text().strip_edges();
	if (current_prompt.is_empty()) {
		return;
	}

	Dictionary body;
	body["prompt"] = current_prompt;
	body["instruction"] = instruction;

	String asset_type = metadata.get(AIAssetMetadata::KEY_ASSET_TYPE, "");
	if (!asset_type.is_empty()) {
		body["assetType"] = asset_type;
	}

	String json_body = JSON::stringify(body);
	String url = service_url + "/ai-assets/refine-prompt";

	refine_button->set_disabled(true);
	status_label->set_text(TTR("Refining prompt..."));

	refine_request->request(url, _get_headers(), HTTPClient::METHOD_POST, json_body);
}

void AIAssetInfoControl::_on_refine_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	refine_button->set_disabled(false);

	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		status_label->set_text(TTR("Failed to refine prompt."));
		return;
	}

	String response_str = String::utf8((const char *)p_body.ptr(), p_body.size());
	JSON json;
	if (json.parse(response_str) != OK) {
		status_label->set_text(TTR("Invalid response from server."));
		return;
	}

	Dictionary resp = json.get_data();
	String refined = resp.get("refinedPrompt", "");

	if (!refined.is_empty()) {
		prompt_edit->set_text(refined);
		instruction_edit->clear();
		status_label->set_text(TTR("Prompt refined."));
	} else {
		status_label->set_text(TTR("No refined prompt returned."));
	}
}

// ── History ──────────────────────────────────────────────────────────────

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
			label = vformat(U"\u2605 v%d \u2014 %s", ver, truncated);
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
	if (!AIAssetGenerationManager::get_singleton() || AIAssetGenerationManager::get_singleton()->is_busy()) {
		return;
	}
	_save_fields_to_metadata();
	AIAssetGenerationManager::get_singleton()->generate_with_params(
			asset_path, get_prompt(), get_negative_prompt(), get_selected_model(), get_seed());
}

void AIAssetInfoControl::_on_quick_regen_pressed() {
	if (!AIAssetGenerationManager::get_singleton() || AIAssetGenerationManager::get_singleton()->is_busy()) {
		return;
	}
	_save_fields_to_metadata();
	AIAssetGenerationManager::get_singleton()->generate_with_params(
			asset_path, get_prompt(), get_negative_prompt(), get_selected_model(), -1);
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

void AIAssetInfoControl::_on_random_seed_pressed() {
	seed_spinbox->set_value(-1);
}

void AIAssetInfoControl::_on_history_item_selected(int p_index) {
	int selected_version = history_list->get_item_metadata(p_index);
	int current_version = AIAssetMetadata::get_current_version(asset_path);
	bool is_current = (selected_version == current_version);
	bool is_disabled = history_list->is_item_disabled(p_index);

	use_version_button->set_disabled(is_current || is_disabled);
	delete_version_button->set_disabled(is_current || is_disabled);

	if (is_current) {
		// Restore current metadata to fields
		is_viewing_history = false;
		viewed_version = -1;
		prompt_edit->set_text(metadata.get(AIAssetMetadata::KEY_PROMPT, ""));
		negative_prompt_edit->set_text(metadata.get(AIAssetMetadata::KEY_NEGATIVE_PROMPT, ""));
		seed_spinbox->set_value((int)metadata.get(AIAssetMetadata::KEY_SEED, -1));
		Dictionary params = metadata.get(AIAssetMetadata::KEY_PARAMETERS, Dictionary());
		transparent_bg_checkbox->set_pressed((bool)params.get("transparent_bg", false));
		int ver = (int)metadata.get(AIAssetMetadata::KEY_VERSION, 0);
		version_label->set_text(vformat(TTR("Version: %d"), ver));
		_set_editing_enabled(true);
	} else {
		// Load historical version metadata into fields (read-only)
		Dictionary ver_meta = AIAssetMetadata::read_version_meta(asset_path, selected_version);
		is_viewing_history = true;
		viewed_version = selected_version;
		_populate_from_version(ver_meta);
		_set_editing_enabled(false);
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
	return prompt_edit->get_text().strip_edges();
}

String AIAssetInfoControl::get_negative_prompt() const {
	return negative_prompt_edit->get_text().strip_edges();
}

String AIAssetInfoControl::get_selected_model() const {
	if (model_selector->get_selected() >= 0) {
		Variant meta = model_selector->get_item_metadata(model_selector->get_selected());
		if (meta.get_type() == Variant::STRING) {
			return meta;
		}
		return model_selector->get_item_text(model_selector->get_selected());
	}
	return "";
}

int AIAssetInfoControl::get_seed() const {
	return (int)seed_spinbox->get_value();
}

bool AIAssetInfoControl::get_transparent_bg() const {
	return transparent_bg_checkbox->is_pressed();
}

AIAssetInfoControl::AIAssetInfoControl() {
	set_name("AIAssetInfo");
	_create_ui();
}
