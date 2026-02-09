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

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "editor/editor_string_names.h"
#include "scene/gui/separator.h"
#include "scene/gui/split_container.h"
#include "scene/main/http_request.h"

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
	provider_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
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
	model_description->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	model_description->set_custom_minimum_size(Size2(0, 100));
	right_panel->add_child(model_description);

	// Status label
	status_label = memnew(Label);
	status_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	right_panel->add_child(status_label);

	set_ok_button_text(TTR("Select"));
}

Vector<String> AIModelPickerDialog::_get_headers() const {
	Vector<String> headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back("Accept: application/json");
	String project_path = ProjectSettings::get_singleton()->get_resource_path();
	headers.push_back("x-opencode-directory: " + ProjectSettings::get_singleton()->globalize_path(project_path));
	return headers;
}

void AIModelPickerDialog::_load_providers() {
	status_label->set_text(TTR("Loading models..."));
	String url = service_url + "/ai-assets/models";
	http_request->request(url, _get_headers());
}

void AIModelPickerDialog::_on_models_received(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	provider_tree->clear();
	all_models.clear();

	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		status_label->set_text(TTR("Failed to load models from server."));
		return;
	}

	String response_str = String::utf8((const char *)p_body.ptr(), p_body.size());
	JSON json;
	if (json.parse(response_str) != OK) {
		status_label->set_text(TTR("Invalid response from server."));
		return;
	}

	// Response is Record<providerId, ModelInfo[]>
	Dictionary models_by_provider = json.get_data();
	TreeItem *root = provider_tree->create_item();

	Array provider_ids = models_by_provider.keys();
	for (int p = 0; p < provider_ids.size(); p++) {
		String provider_id = provider_ids[p];
		Array models = models_by_provider[provider_id];

		// Create provider tree item
		TreeItem *provider_item = provider_tree->create_item(root);
		provider_item->set_text(0, provider_id);
		provider_item->set_metadata(0, provider_id);

		// Parse models for this provider
		for (int m = 0; m < models.size(); m++) {
			Dictionary model_dict = models[m];
			ModelInfo info;
			info.id = model_dict.get("id", "");
			info.name = model_dict.get("name", info.id);
			info.provider_id = provider_id;
			info.provider_name = provider_id;
			info.description = model_dict.get("description", "");

			Array types = model_dict.get("supportedTypes", Array());
			for (int t = 0; t < types.size(); t++) {
				info.supported_types.push_back(types[t]);
			}

			Array transforms = model_dict.get("supportedTransforms", Array());
			for (int t = 0; t < transforms.size(); t++) {
				info.supported_transforms.push_back(transforms[t]);
			}

			if (model_dict.has("pricing")) {
				Dictionary pricing = model_dict["pricing"];
				info.cost = (double)pricing.get("cost", 0.0);
			}

			// Use provider name from first model if available
			if (model_dict.has("providerName")) {
				provider_item->set_text(0, model_dict["providerName"]);
				info.provider_name = model_dict["providerName"];
			}

			all_models.push_back(info);
		}
	}

	status_label->set_text("");

	// Auto-select provider
	if (!selected_provider.is_empty()) {
		TreeItem *child = root->get_first_child();
		while (child) {
			if (String(child->get_metadata(0)) == selected_provider) {
				child->select(0);
				break;
			}
			child = child->get_next();
		}
	} else if (root->get_first_child()) {
		TreeItem *first = root->get_first_child();
		first->select(0);
		selected_provider = first->get_metadata(0);
	}

	_load_models_for_provider(selected_provider);
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
			String label = model.name;
			if (model.cost > 0) {
				label = vformat("%s ($%s)", model.name, String::num(model.cost, model.cost < 0.01 ? 4 : 3));
			}
			model_list->add_item(label);

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

	// Try to pre-select provider from cached models
	if (!p_current_model.is_empty()) {
		for (const ModelInfo &mi : all_models) {
			if (mi.id == p_current_model) {
				selected_provider = mi.provider_id;
				break;
			}
		}
	}

	// Fetch latest from API (callback handles provider/model selection)
	_load_providers();
}

AIModelPickerDialog::AIModelPickerDialog() {
	_create_ui();

	http_request = memnew(HTTPRequest);
	add_child(http_request);
	http_request->connect("request_completed", callable_mp(this, &AIModelPickerDialog::_on_models_received));
}
