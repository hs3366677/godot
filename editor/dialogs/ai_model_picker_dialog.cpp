/**************************************************************************/
/*  ai_model_picker_dialog.cpp                                            */
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

#include "ai_model_picker_dialog.h"

#include "editor/editor_string_names.h"
#include "scene/gui/separator.h"
#include "scene/gui/split_container.h"

void AIModelPickerDialog::_bind_methods() {
	ADD_SIGNAL(MethodInfo("model_selected", PropertyInfo(Variant::STRING, "provider"), PropertyInfo(Variant::STRING, "model")));
}

void AIModelPickerDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			// Apply theme styling
		} break;
	}
}

void AIModelPickerDialog::_create_ui() {
	set_title(TTR("Select AI Model"));
	set_min_size(Size2(600, 400));

	HSplitContainer *split = memnew(HSplitContainer);
	split->set_split_offset(180);
	add_child(split);

	// Left panel - Provider tree
	VBoxContainer *left_panel = memnew(VBoxContainer);
	split->add_child(left_panel);

	Label *providers_title = memnew(Label);
	providers_title->set_text(TTR("Providers"));
	left_panel->add_child(providers_title);

	provider_tree = memnew(Tree);
	provider_tree->set_hide_root(true);
	provider_tree->set_v_size_flags(SIZE_EXPAND_FILL);
	provider_tree->connect("item_selected", callable_mp(this, &AIModelPickerDialog::_on_provider_selected));
	left_panel->add_child(provider_tree);

	// Right panel - Model list and description
	VBoxContainer *right_panel = memnew(VBoxContainer);
	split->add_child(right_panel);

	Label *models_title = memnew(Label);
	models_title->set_text(TTR("Models"));
	right_panel->add_child(models_title);

	model_list = memnew(ItemList);
	model_list->set_custom_minimum_size(Size2(0, 150));
	model_list->connect("item_selected", callable_mp(this, &AIModelPickerDialog::_on_model_selected));
	model_list->connect("item_activated", callable_mp(this, &AIModelPickerDialog::_on_model_activated));
	right_panel->add_child(model_list);

	right_panel->add_child(memnew(HSeparator));

	Label *desc_title = memnew(Label);
	desc_title->set_text(TTR("Description"));
	right_panel->add_child(desc_title);

	model_description = memnew(RichTextLabel);
	model_description->set_use_bbcode(true);
	model_description->set_v_size_flags(SIZE_EXPAND_FILL);
	model_description->set_custom_minimum_size(Size2(0, 100));
	right_panel->add_child(model_description);

	// Status label
	status_label = memnew(Label);
	status_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	right_panel->add_child(status_label);

	set_ok_button_text(TTR("Select"));
}

void AIModelPickerDialog::_load_providers() {
	provider_tree->clear();
	TreeItem *root = provider_tree->create_item();

	// TODO: Fetch providers from OpenCode API
	// For now, add hardcoded providers

	// Meshy - 3D Models
	TreeItem *meshy = provider_tree->create_item(root);
	meshy->set_text(0, "Meshy AI");
	meshy->set_metadata(0, "meshy");
	meshy->set_tooltip_text(0, TTR("3D model generation"));

	// Doubao - 2D Images
	TreeItem *doubao = provider_tree->create_item(root);
	doubao->set_text(0, "Doubao (Volcano)");
	doubao->set_metadata(0, "doubao");
	doubao->set_tooltip_text(0, TTR("2D image/texture generation"));

	// Suno - Audio
	TreeItem *suno = provider_tree->create_item(root);
	suno->set_text(0, "Suno AI");
	suno->set_metadata(0, "suno");
	suno->set_tooltip_text(0, TTR("Audio/music generation"));

	// Populate all_models with mock data
	// TODO: Replace with API call
	all_models.clear();

	// Meshy models
	ModelInfo meshy6;
	meshy6.id = "meshy-6";
	meshy6.name = "Meshy v6";
	meshy6.provider_id = "meshy";
	meshy6.provider_name = "Meshy AI";
	meshy6.description = "Latest Meshy model with improved quality and faster generation. Supports text-to-3D and image-to-3D.";
	meshy6.supported_types.push_back("model");
	meshy6.supported_types.push_back("mesh");
	meshy6.supported_types.push_back("scene");
	meshy6.supported_transforms.push_back("img2model");
	all_models.push_back(meshy6);

	ModelInfo meshy5;
	meshy5.id = "meshy-5";
	meshy5.name = "Meshy v5";
	meshy5.provider_id = "meshy";
	meshy5.provider_name = "Meshy AI";
	meshy5.description = "Previous generation Meshy model. Good balance of quality and speed.";
	meshy5.supported_types.push_back("model");
	meshy5.supported_types.push_back("mesh");
	all_models.push_back(meshy5);

	// Doubao models
	ModelInfo seedream4;
	seedream4.id = "seedream-v4";
	seedream4.name = "Seedream v4";
	seedream4.provider_id = "doubao";
	seedream4.provider_name = "Doubao";
	seedream4.description = "High-quality image generation up to 4K resolution. Supports multiple styles including photorealistic, anime, and pixel art.";
	seedream4.supported_types.push_back("texture");
	seedream4.supported_types.push_back("sprite");
	seedream4.supported_transforms.push_back("upscale");
	seedream4.supported_transforms.push_back("style_transfer");
	seedream4.supported_transforms.push_back("variation");
	all_models.push_back(seedream4);

	ModelInfo seedream3;
	seedream3.id = "seedream-v3";
	seedream3.name = "Seedream v3";
	seedream3.provider_id = "doubao";
	seedream3.provider_name = "Doubao";
	seedream3.description = "Fast image generation with good quality. Ideal for rapid iteration.";
	seedream3.supported_types.push_back("texture");
	seedream3.supported_types.push_back("sprite");
	all_models.push_back(seedream3);

	// Suno models
	ModelInfo sunov5;
	sunov5.id = "suno-v5";
	sunov5.name = "Suno v5";
	sunov5.provider_id = "suno";
	sunov5.provider_name = "Suno AI";
	sunov5.description = "Latest Suno model for music generation. Creates full songs with vocals and instrumentals.";
	sunov5.supported_types.push_back("audio_music");
	all_models.push_back(sunov5);

	ModelInfo sunosfx;
	sunosfx.id = "suno-sfx";
	sunosfx.name = "Suno SFX";
	sunosfx.provider_id = "suno";
	sunosfx.provider_name = "Suno AI";
	sunosfx.description = "Specialized model for sound effects generation. Creates short audio clips for game events.";
	sunosfx.supported_types.push_back("audio_sfx");
	all_models.push_back(sunosfx);
}

void AIModelPickerDialog::_load_models_for_provider(const String &p_provider) {
	model_list->clear();
	model_description->clear();
	filtered_models.clear();

	for (const ModelInfo &model : all_models) {
		if (model.provider_id == p_provider) {
			// Apply type filter if set
			if (!filter_type.is_empty()) {
				bool type_match = false;
				for (const String &type : model.supported_types) {
					if (type == filter_type) {
						type_match = true;
						break;
					}
				}
				if (!type_match) {
					continue;
				}
			}

			filtered_models.push_back(model);
			model_list->add_item(model.name);

			// Mark current selection
			if (model.id == selected_model) {
				model_list->select(model_list->get_item_count() - 1);
			}
		}
	}

	if (model_list->get_item_count() == 0) {
		status_label->set_text(TTR("No models available for this provider."));
	} else {
		status_label->set_text("");
	}
}

void AIModelPickerDialog::_on_provider_selected() {
	TreeItem *selected = provider_tree->get_selected();
	if (!selected) {
		return;
	}

	selected_provider = selected->get_metadata(0);
	_load_models_for_provider(selected_provider);
}

void AIModelPickerDialog::_on_model_selected(int p_index) {
	if (p_index < 0 || p_index >= filtered_models.size()) {
		model_description->clear();
		return;
	}

	const ModelInfo &model = filtered_models[p_index];
	selected_model = model.id;

	// Update description
	String desc = "[b]" + model.name + "[/b]\n\n";
	desc += model.description + "\n\n";

	desc += "[b]" + TTR("Supported Types:") + "[/b] ";
	for (int i = 0; i < model.supported_types.size(); i++) {
		if (i > 0) {
			desc += ", ";
		}
		desc += model.supported_types[i];
	}

	if (!model.supported_transforms.is_empty()) {
		desc += "\n[b]" + TTR("Transforms:") + "[/b] ";
		for (int i = 0; i < model.supported_transforms.size(); i++) {
			if (i > 0) {
				desc += ", ";
			}
			desc += model.supported_transforms[i];
		}
	}

	model_description->set_text(desc);
}

void AIModelPickerDialog::_on_model_activated(int p_index) {
	_on_model_selected(p_index);
	emit_signal("confirmed");
	hide();
}

void AIModelPickerDialog::_filter_models() {
	if (selected_provider.is_empty()) {
		return;
	}
	_load_models_for_provider(selected_provider);
}

void AIModelPickerDialog::setup(const String &p_filter_type, const String &p_current_model) {
	filter_type = p_filter_type;
	selected_model = p_current_model;
	selected_provider = "";

	_load_providers();

	// Select provider based on current model
	if (!p_current_model.is_empty()) {
		for (const ModelInfo &model : all_models) {
			if (model.id == p_current_model) {
				selected_provider = model.provider_id;
				break;
			}
		}
	}

	// Select first provider if none selected
	if (selected_provider.is_empty()) {
		TreeItem *root = provider_tree->get_root();
		if (root && root->get_first_child()) {
			TreeItem *first = root->get_first_child();
			first->select(0);
			selected_provider = first->get_metadata(0);
		}
	} else {
		// Find and select the provider in tree
		TreeItem *root = provider_tree->get_root();
		if (root) {
			TreeItem *child = root->get_first_child();
			while (child) {
				if (String(child->get_metadata(0)) == selected_provider) {
					child->select(0);
					break;
				}
				child = child->get_next();
			}
		}
	}

	_load_models_for_provider(selected_provider);

	status_label->set_text("");
}

AIModelPickerDialog::AIModelPickerDialog() {
	_create_ui();
}
