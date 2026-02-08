/**************************************************************************/
/*  ai_model_picker_dialog.h                                              */
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

#pragma once

#include "scene/gui/dialogs.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/item_list.h"
#include "scene/gui/label.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/tree.h"

class HTTPRequest;

class AIModelPickerDialog : public ConfirmationDialog {
	GDCLASS(AIModelPickerDialog, ConfirmationDialog);

public:
	struct ModelInfo {
		String id;
		String name;
		String provider_id;
		String provider_name;
		String description;
		double cost = 0.0;
		Vector<String> supported_types;
		Vector<String> supported_transforms;
	};

private:
	String selected_provider;
	String selected_model;
	String filter_type; // Filter by asset type

	// UI Elements
	Tree *provider_tree = nullptr;
	ItemList *model_list = nullptr;
	RichTextLabel *model_description = nullptr;
	Label *status_label = nullptr;

	Vector<ModelInfo> all_models;
	Vector<ModelInfo> filtered_models;

	// HTTP
	String service_url = "http://localhost:4096";
	HTTPRequest *http_request = nullptr;
	Vector<String> _get_headers() const;

	void _create_ui();
	void _load_providers();
	void _load_models_for_provider(const String &p_provider);
	void _on_provider_selected();
	void _on_model_selected(int p_index);
	void _on_model_activated(int p_index);
	void _on_models_received(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _filter_models();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void setup(const String &p_filter_type = "", const String &p_current_model = "");

	String get_selected_provider() const { return selected_provider; }
	String get_selected_model() const { return selected_model; }

	AIModelPickerDialog();
};
