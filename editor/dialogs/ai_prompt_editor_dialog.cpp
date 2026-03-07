/**************************************************************************/
/*  ai_prompt_editor_dialog.cpp                                           */
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

#include "ai_prompt_editor_dialog.h"

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "editor/editor_string_names.h"
#include "scene/gui/separator.h"
#include "scene/main/http_request.h"

void AIPromptEditorDialog::_bind_methods() {
	ADD_SIGNAL(MethodInfo("prompt_confirmed", PropertyInfo(Variant::STRING, "prompt"), PropertyInfo(Variant::STRING, "negative_prompt"), PropertyInfo(Variant::STRING, "model"), PropertyInfo(Variant::INT, "seed")));
}

void AIPromptEditorDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			// Apply theme styling if needed
		} break;
	}
}

void AIPromptEditorDialog::_create_ui() {
	set_title(TTR("Edit Prompt & Regenerate"));
	set_min_size(Size2(600, 550));

	VBoxContainer *main_vbox = memnew(VBoxContainer);
	main_vbox->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, 8);
	main_vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main_vbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(main_vbox);

	// Asset info row
	HBoxContainer *asset_row = memnew(HBoxContainer);
	main_vbox->add_child(asset_row);

	Label *asset_title = memnew(Label);
	asset_title->set_text(TTR("Asset:"));
	asset_row->add_child(asset_title);

	asset_label = memnew(Label);
	asset_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	asset_row->add_child(asset_label);

	// Provider & Model row
	HBoxContainer *model_row = memnew(HBoxContainer);
	main_vbox->add_child(model_row);

	provider_label = memnew(Label);
	provider_label->set_text(TTR("Provider:"));
	model_row->add_child(provider_label);

	model_row->add_child(memnew(Control)); // Spacer

	Label *model_title = memnew(Label);
	model_title->set_text(TTR("Model:"));
	model_row->add_child(model_title);

	model_selector = memnew(OptionButton);
	model_selector->set_custom_minimum_size(Size2(150, 0));
	model_row->add_child(model_selector);

	main_vbox->add_child(memnew(HSeparator));

	// Prompt section
	prompt_title = memnew(Label);
	prompt_title->set_text(TTR("Prompt:"));
	main_vbox->add_child(prompt_title);

	prompt_edit = memnew(TextEdit);
	prompt_edit->set_custom_minimum_size(Size2(0, 120));
	prompt_edit->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	prompt_edit->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	prompt_edit->set_stretch_ratio(3.0);
	main_vbox->add_child(prompt_edit);

	// Negative prompt section
	negative_prompt_title = memnew(Label);
	negative_prompt_title->set_text(TTR("Negative Prompt:"));
	main_vbox->add_child(negative_prompt_title);

	negative_prompt_edit = memnew(TextEdit);
	negative_prompt_edit->set_custom_minimum_size(Size2(0, 60));
	negative_prompt_edit->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	negative_prompt_edit->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	negative_prompt_edit->set_stretch_ratio(1.0);
	main_vbox->add_child(negative_prompt_edit);

	main_vbox->add_child(memnew(HSeparator));

	// AI Assist section
	ai_assist_container = memnew(VBoxContainer);
	main_vbox->add_child(ai_assist_container);

	Label *ai_assist_title = memnew(Label);
	ai_assist_title->set_text(TTR("AI Assist:"));
	ai_assist_container->add_child(ai_assist_title);

	HBoxContainer *instruction_row = memnew(HBoxContainer);
	ai_assist_container->add_child(instruction_row);

	instruction_edit = memnew(LineEdit);
	instruction_edit->set_placeholder(TTR("e.g., \"make it more cartoon-like\" or \"add a shield\""));
	instruction_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	instruction_row->add_child(instruction_edit);

	refine_button = memnew(Button);
	refine_button->set_text(TTR("Refine Prompt"));
	refine_button->connect(SceneStringName(pressed), callable_mp(this, &AIPromptEditorDialog::_on_refine_pressed));
	instruction_row->add_child(refine_button);

	main_vbox->add_child(memnew(HSeparator));

	// Seed control
	seed_container = memnew(HBoxContainer);
	main_vbox->add_child(seed_container);

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
	random_seed_button->connect(SceneStringName(pressed), callable_mp(this, &AIPromptEditorDialog::_on_random_seed_pressed));
	seed_container->add_child(random_seed_button);

	// Version info
	version_label = memnew(Label);
	seed_container->add_child(version_label);

	// Status label
	status_label = memnew(Label);
	status_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	main_vbox->add_child(status_label);

	// Set button text based on mode
	set_ok_button_text(TTR("Regenerate"));
	connect("confirmed", callable_mp(this, &AIPromptEditorDialog::_on_confirmed));
}

Vector<String> AIPromptEditorDialog::_get_headers() const {
	Vector<String> headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back("Accept: application/json");
	String project_path = ProjectSettings::get_singleton()->get_resource_path();
	headers.push_back("x-opencode-directory: " + ProjectSettings::get_singleton()->globalize_path(project_path));
	return headers;
}

void AIPromptEditorDialog::_load_models() {
	model_selector->clear();
	model_selector->add_item(TTR("Loading..."), 0);

	String url = service_url + "/ai-assets/models";
	models_request->request(url, _get_headers());
}

void AIPromptEditorDialog::_on_models_received(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
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

	// Response is Record<providerId, ModelInfo[]>
	Dictionary models_by_provider = json.get_data();
	String current_model = original_metadata.get(AIAssetMetadata::KEY_MODEL, "");
	int select_idx = -1;

	Array provider_ids = models_by_provider.keys();
	for (int p = 0; p < provider_ids.size(); p++) {
		Array models = models_by_provider[provider_ids[p]];
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

void AIPromptEditorDialog::_on_refine_pressed() {
	String instruction = instruction_edit->get_text().strip_edges();
	if (instruction.is_empty()) {
		return;
	}

	String current_prompt = prompt_edit->get_text().strip_edges();
	if (current_prompt.is_empty()) {
		return;
	}

	// Build request body
	Dictionary body;
	body["prompt"] = current_prompt;
	body["instruction"] = instruction;

	String asset_type = original_metadata.get(AIAssetMetadata::KEY_ASSET_TYPE, "");
	if (!asset_type.is_empty()) {
		body["assetType"] = asset_type;
	}

	String json_body = JSON::stringify(body);
	String url = service_url + "/ai-assets/refine-prompt";

	refine_button->set_disabled(true);
	status_label->set_text(TTR("Refining prompt..."));

	refine_request->request(url, _get_headers(), HTTPClient::METHOD_POST, json_body);
}

void AIPromptEditorDialog::_on_refine_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
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
		status_label->set_text(TTR("Prompt refined. Review and confirm."));
	} else {
		status_label->set_text(TTR("No refined prompt returned."));
	}
}

void AIPromptEditorDialog::_on_random_seed_pressed() {
	seed_spinbox->set_value(-1);
}

void AIPromptEditorDialog::_on_confirmed() {
	if (is_processing) {
		return;
	}

	String prompt = get_prompt();
	String negative_prompt = get_negative_prompt();
	String model = get_selected_model();
	int seed = get_seed();

	print_line(vformat("AIPromptEditor: confirmed prompt='%s' model='%s' seed=%d", prompt.left(50), model, seed));
	emit_signal("prompt_confirmed", prompt, negative_prompt, model, seed);
}

void AIPromptEditorDialog::_update_version_preview() {
	int current_version = original_metadata.get(AIAssetMetadata::KEY_VERSION, 0);
	if (current_version > 0) {
		version_label->set_text(vformat(TTR("Version: %d → %d"), current_version, current_version + 1));
		version_label->show();
	} else {
		version_label->hide();
	}
}

void AIPromptEditorDialog::setup_for_asset(const String &p_path, Mode p_mode) {
	asset_path = p_path;
	mode = p_mode;
	original_metadata = AIAssetMetadata::get_metadata(p_path);

	// Update title based on mode
	switch (mode) {
		case MODE_GENERATE:
			set_title(TTR("Edit Prompt & Generate"));
			set_ok_button_text(TTR("Generate"));
			break;
		case MODE_REGENERATE:
			set_title(TTR("Edit Prompt & Regenerate"));
			set_ok_button_text(TTR("Regenerate"));
			break;
		case MODE_TRANSFORM:
			set_title(TTR("Edit Prompt & Transform"));
			set_ok_button_text(TTR("Transform"));
			break;
	}

	// Update asset label
	asset_label->set_text(p_path);

	// Update provider label
	String provider = original_metadata.get(AIAssetMetadata::KEY_PROVIDER, "unknown");
	provider_label->set_text(vformat(TTR("Provider: %s"), provider));

	// Load prompt
	String prompt = original_metadata.get(AIAssetMetadata::KEY_PROMPT, "");
	prompt_edit->set_text(prompt);

	// Load negative prompt
	String negative_prompt = original_metadata.get(AIAssetMetadata::KEY_NEGATIVE_PROMPT, "");
	negative_prompt_edit->set_text(negative_prompt);

	// Load models
	_load_models();

	// Load seed
	int seed = original_metadata.get(AIAssetMetadata::KEY_SEED, -1);
	seed_spinbox->set_value(seed);

	// Update version preview
	_update_version_preview();

	// Clear status
	status_label->set_text("");
	is_processing = false;
}

String AIPromptEditorDialog::get_prompt() const {
	return prompt_edit->get_text().strip_edges();
}

String AIPromptEditorDialog::get_negative_prompt() const {
	return negative_prompt_edit->get_text().strip_edges();
}

String AIPromptEditorDialog::get_selected_model() const {
	if (model_selector->get_selected() >= 0) {
		Variant meta = model_selector->get_item_metadata(model_selector->get_selected());
		if (meta.get_type() == Variant::STRING) {
			return meta;
		}
		return model_selector->get_item_text(model_selector->get_selected());
	}
	return "";
}

int AIPromptEditorDialog::get_seed() const {
	return (int)seed_spinbox->get_value();
}

AIPromptEditorDialog::AIPromptEditorDialog() {
	_create_ui();

	refine_request = memnew(HTTPRequest);
	add_child(refine_request);
	refine_request->connect("request_completed", callable_mp(this, &AIPromptEditorDialog::_on_refine_completed));

	models_request = memnew(HTTPRequest);
	add_child(models_request);
	models_request->connect("request_completed", callable_mp(this, &AIPromptEditorDialog::_on_models_received));
}
