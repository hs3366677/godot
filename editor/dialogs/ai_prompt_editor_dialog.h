/**************************************************************************/
/*  ai_prompt_editor_dialog.h                                             */
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

#include "core/io/ai_asset_metadata.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"

#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/text_edit.h"

class HTTPRequest;

class AIPromptEditorDialog : public ConfirmationDialog {
	GDCLASS(AIPromptEditorDialog, ConfirmationDialog);

public:
	enum Mode {
		MODE_GENERATE,    // Generate from placeholder
		MODE_REGENERATE,  // Regenerate existing
		MODE_TRANSFORM,   // Transform/enhance
	};

private:
	Mode mode = MODE_REGENERATE;
	String asset_path;
	Dictionary original_metadata;

	// UI Elements
	Label *asset_label = nullptr;
	Label *provider_label = nullptr;
	OptionButton *model_selector = nullptr;

	Label *prompt_title = nullptr;
	TextEdit *prompt_edit = nullptr;
	Label *negative_prompt_title = nullptr;
	TextEdit *negative_prompt_edit = nullptr;

	// AI Assist section
	VBoxContainer *ai_assist_container = nullptr;
	LineEdit *instruction_edit = nullptr;
	Button *refine_button = nullptr;

	// Seed control
	HBoxContainer *seed_container = nullptr;
	SpinBox *seed_spinbox = nullptr;
	Button *random_seed_button = nullptr;

	// Version info
	Label *version_label = nullptr;

	// Progress
	Label *status_label = nullptr;
	bool is_processing = false;

	// HTTP
	String service_url = "http://localhost:4096";
	HTTPRequest *refine_request = nullptr;
	HTTPRequest *models_request = nullptr;
	Vector<String> _get_headers() const;

	void _create_ui();
	void _load_models();
	void _on_refine_pressed();
	void _on_refine_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _on_models_received(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _on_random_seed_pressed();
	void _on_confirmed();
	void _update_version_preview();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void setup_for_asset(const String &p_path, Mode p_mode = MODE_REGENERATE);

	String get_asset_path() const { return asset_path; }
	String get_prompt() const;
	String get_negative_prompt() const;
	String get_selected_model() const;
	int get_seed() const;
	AIPromptEditorDialog();
};
