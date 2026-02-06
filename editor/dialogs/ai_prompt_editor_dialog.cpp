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

#include "editor/editor_string_names.h"
#include "scene/gui/separator.h"

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
	set_min_size(Size2(500, 400));

	VBoxContainer *main_vbox = memnew(VBoxContainer);
	add_child(main_vbox);

	// Asset info row
	HBoxContainer *asset_row = memnew(HBoxContainer);
	main_vbox->add_child(asset_row);

	Label *asset_title = memnew(Label);
	asset_title->set_text(TTR("Asset:"));
	asset_row->add_child(asset_title);

	asset_label = memnew(Label);
	asset_label->set_h_size_flags(SIZE_EXPAND_FILL);
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
	prompt_edit->set_custom_minimum_size(Size2(0, 80));
	prompt_edit->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
	prompt_edit->set_v_size_flags(SIZE_EXPAND_FILL);
	main_vbox->add_child(prompt_edit);

	// Negative prompt section
	negative_prompt_title = memnew(Label);
	negative_prompt_title->set_text(TTR("Negative Prompt:"));
	main_vbox->add_child(negative_prompt_title);

	negative_prompt_edit = memnew(TextEdit);
	negative_prompt_edit->set_custom_minimum_size(Size2(0, 40));
	negative_prompt_edit->set_line_wrapping_mode(TextEdit::LINE_WRAPPING_BOUNDARY);
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
	instruction_edit->set_h_size_flags(SIZE_EXPAND_FILL);
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

	seed_container->add_child(memnew(Control)); // Spacer
	seed_container->get_child(seed_container->get_child_count() - 1)->set_h_size_flags(SIZE_EXPAND_FILL);

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

void AIPromptEditorDialog::_load_models() {
	model_selector->clear();

	// TODO: Fetch models from OpenCode API based on provider
	// For now, add placeholder models
	String provider = original_metadata.get(AIAssetMetadata::KEY_PROVIDER, "");

	if (provider == "meshy") {
		model_selector->add_item("meshy-6", 0);
		model_selector->add_item("meshy-5", 1);
	} else if (provider == "doubao") {
		model_selector->add_item("seedream-v4", 0);
		model_selector->add_item("seedream-v3", 1);
	} else if (provider == "suno") {
		model_selector->add_item("suno-v5", 0);
		model_selector->add_item("suno-v4", 1);
	} else {
		model_selector->add_item("default", 0);
	}

	// Select current model
	String current_model = original_metadata.get(AIAssetMetadata::KEY_MODEL, "");
	for (int i = 0; i < model_selector->get_item_count(); i++) {
		if (model_selector->get_item_text(i) == current_model) {
			model_selector->select(i);
			break;
		}
	}
}

void AIPromptEditorDialog::_on_refine_pressed() {
	String instruction = instruction_edit->get_text().strip_edges();
	if (instruction.is_empty()) {
		return;
	}

	// TODO: Call OpenCode API to refine prompt
	// For now, just append the instruction
	String current_prompt = prompt_edit->get_text();
	prompt_edit->set_text(current_prompt + ". " + instruction);

	instruction_edit->clear();
	status_label->set_text(TTR("Prompt refined. Review and confirm."));
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
		return model_selector->get_item_text(model_selector->get_selected());
	}
	return "";
}

int AIPromptEditorDialog::get_seed() const {
	return (int)seed_spinbox->get_value();
}

AIPromptEditorDialog::AIPromptEditorDialog() {
	_create_ui();
}
