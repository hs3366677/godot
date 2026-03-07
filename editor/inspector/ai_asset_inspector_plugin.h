/**************************************************************************/
/*  ai_asset_inspector_plugin.h                                           */
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
#include "editor/inspector/editor_inspector.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"

#include "scene/gui/item_list.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/text_edit.h"

class FileDialog;
class HTTPRequest;

class AIAssetInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(AIAssetInspectorPlugin, EditorInspectorPlugin);

protected:
	static void _bind_methods();

public:
	virtual bool can_handle(Object *p_object) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;
	virtual void parse_begin(Object *p_object) override;

	AIAssetInspectorPlugin();
};

// Custom control for displaying and editing AI asset info in inspector
class AIAssetInfoControl : public VBoxContainer {
	GDCLASS(AIAssetInfoControl, VBoxContainer);

private:
	String asset_path;
	AIAssetMetadata::Origin origin = AIAssetMetadata::ORIGIN_UNKNOWN;
	Dictionary metadata;

	// Header
	Label *origin_label = nullptr;

	// Usage section (read-only)
	VBoxContainer *usage_container = nullptr;
	Label *usage_role_label = nullptr;
	Label *usage_dimensions_label = nullptr;
	Label *usage_scene_label = nullptr;
	Label *usage_node_path_label = nullptr;
	Label *usage_extras_label = nullptr;

	// Prompt editing
	Label *prompt_title = nullptr;
	TextEdit *prompt_edit = nullptr;
	Label *negative_prompt_title = nullptr;
	TextEdit *negative_prompt_edit = nullptr;

	// Provider & Model
	Label *provider_label = nullptr;
	OptionButton *model_selector = nullptr;

	// Seed control
	HBoxContainer *seed_container = nullptr;
	SpinBox *seed_spinbox = nullptr;
	Button *random_seed_button = nullptr;

	// AI Assist section
	VBoxContainer *ai_assist_container = nullptr;
	LineEdit *instruction_edit = nullptr;
	Button *refine_button = nullptr;

	// Version & source info
	Label *version_label = nullptr;
	Label *source_label = nullptr;

	// Action buttons
	HBoxContainer *buttons_container = nullptr;
	Button *generate_button = nullptr;
	Button *quick_regen_button = nullptr;
	Button *enhance_button = nullptr;
	Button *view_source_button = nullptr;
	Button *replace_file_button = nullptr;
	FileDialog *replace_file_dialog = nullptr;

	// Status
	Label *status_label = nullptr;

	// History section
	VBoxContainer *history_container = nullptr;
	Label *history_title = nullptr;
	ItemList *history_list = nullptr;
	HBoxContainer *history_buttons = nullptr;
	Button *use_version_button = nullptr;
	Button *delete_version_button = nullptr;

	// HTTP
	String service_url = "http://localhost:4096";
	HTTPRequest *refine_request = nullptr;
	HTTPRequest *models_request = nullptr;

	// State
	bool is_viewing_history = false;
	int viewed_version = -1;

	void _create_ui();
	void _create_history_ui();
	void _update_ui();
	void _update_history_list();

	// Editing controls
	void _set_editing_enabled(bool p_enabled);
	void _populate_from_version(const Dictionary &p_version_meta);
	void _save_fields_to_metadata();

	// HTTP methods
	Vector<String> _get_headers() const;
	void _load_models();
	void _on_models_received(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _on_refine_pressed();
	void _on_refine_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);

	// Button handlers
	void _on_generate_pressed();
	void _on_quick_regen_pressed();
	void _on_enhance_pressed();
	void _on_view_source_pressed();
	void _on_random_seed_pressed();
	void _on_replace_file_pressed();
	void _on_replace_file_selected(const String &p_path);

	// History handlers
	void _on_history_item_selected(int p_index);
	void _on_use_version_pressed();
	void _on_delete_version_pressed();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_asset_path(const String &p_path);
	String get_asset_path() const { return asset_path; }

	String get_prompt() const;
	String get_negative_prompt() const;
	String get_selected_model() const;
	int get_seed() const;
	AIAssetInfoControl();
};
