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
#include "scene/gui/label.h"
#include "scene/gui/item_list.h"
#include "scene/gui/rich_text_label.h"

class AIAssetInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(AIAssetInspectorPlugin, EditorInspectorPlugin);

private:
	String current_path;
	Dictionary current_metadata;

	void _generate_placeholder_pressed();
	void _edit_prompt_pressed();
	void _quick_regenerate_pressed();
	void _copy_prompt_pressed();
	void _enhance_pressed();
	void _view_source_pressed();

protected:
	static void _bind_methods();

public:
	virtual bool can_handle(Object *p_object) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;
	virtual void parse_begin(Object *p_object) override;

	AIAssetInspectorPlugin();
};

// Custom control for displaying AI asset info in inspector
class AIAssetInfoControl : public VBoxContainer {
	GDCLASS(AIAssetInfoControl, VBoxContainer);

private:
	String asset_path;
	AIAssetMetadata::Origin origin = AIAssetMetadata::ORIGIN_UNKNOWN;
	Dictionary metadata;

	Label *origin_label = nullptr;
	RichTextLabel *prompt_label = nullptr;
	Label *provider_label = nullptr;
	Label *model_label = nullptr;
	Label *version_label = nullptr;
	Label *source_label = nullptr;

	HBoxContainer *buttons_container = nullptr;
	Button *generate_button = nullptr;
	Button *edit_prompt_button = nullptr;
	Button *quick_regen_button = nullptr;
	Button *enhance_button = nullptr;
	Button *copy_prompt_button = nullptr;
	Button *view_source_button = nullptr;

	// History section
	VBoxContainer *history_container = nullptr;
	Label *history_title = nullptr;
	ItemList *history_list = nullptr;
	HBoxContainer *history_buttons = nullptr;
	Button *use_version_button = nullptr;
	Button *delete_version_button = nullptr;

	void _create_ui();
	void _create_history_ui();
	void _update_ui();
	void _update_history_list();
	void _on_generate_pressed();
	void _on_edit_prompt_pressed();
	void _on_quick_regen_pressed();
	void _on_enhance_pressed();
	void _on_copy_prompt_pressed();
	void _on_view_source_pressed();
	void _on_history_item_selected(int p_index);
	void _on_use_version_pressed();
	void _on_delete_version_pressed();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_asset_path(const String &p_path);
	String get_asset_path() const { return asset_path; }

	AIAssetInfoControl();
};
