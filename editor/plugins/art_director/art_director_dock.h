/**************************************************************************/
/*  art_director_dock.h                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                           MAKABAKA ENGINE                              */
/*                    AI-powered game creation module                     */
/**************************************************************************/

#pragma once

#include "editor/plugins/editor_plugin.h"

#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/texture_rect.h"
#include "scene/main/http_request.h"
#include "scene/main/timer.h"
#include "scene/resources/image_texture.h"

// ── ArtDirectorPanel: the actual UI (lives inside the main screen) ──────────

class ArtDirectorPanel : public PanelContainer {
	GDCLASS(ArtDirectorPanel, PanelContainer);

	// ── Style Profile section ────────────────────────────────────────────
	VBoxContainer *profile_section = nullptr;
	Label *profile_style_label = nullptr;
	Label *profile_ref_label = nullptr;

	// ── Style Explorations gallery ───────────────────────────────────────
	HBoxContainer *exploration_header = nullptr;
	Button *exploration_refresh_button = nullptr;
	HBoxContainer *exploration_gallery = nullptr;
	int selected_exploration_index = -1;
	Button *exploration_set_ref_button = nullptr;

	// ── Cornerstone Assets gallery ───────────────────────────────────────
	HBoxContainer *cornerstone_header = nullptr;
	Button *cornerstone_refresh_button = nullptr;
	HBoxContainer *cornerstone_gallery = nullptr;
	int selected_cornerstone_index = -1;
	Button *cornerstone_set_ref_button = nullptr;

	// ── Model selection ──────────────────────────────────────────────────
	OptionButton *exploration_model_picker = nullptr;

	// ── HTTP infrastructure ──────────────────────────────────────────────
	HTTPRequest *profile_http_request = nullptr;
	HTTPRequest *images_http_request = nullptr;
	HTTPRequest *models_http_request = nullptr;
	HTTPRequest *set_ref_http_request = nullptr;
	Timer *gallery_refresh_timer = nullptr;
	bool images_request_in_progress = false;

	String service_url = "http://localhost:4096";

	// ── Image data ────────────────────────────────────────────────────────
	struct ThumbInfo {
		String res_path;
		String category; // "exploration" or "cornerstone"
		String label;    // short display name (from metadata, cornerstone only)
		String tooltip;  // full subject description for hover
		Ref<ImageTexture> texture;
	};
	Vector<ThumbInfo> exploration_thumbs;
	Vector<ThumbInfo> cornerstone_thumbs;

	// Double-click tracking for thumbnails
	uint64_t last_thumb_click_time = 0;
	String last_thumb_click_category;
	int last_thumb_click_index = -1;

	// ── Internal helpers ─────────────────────────────────────────────────
	void _setup_ui();
	void _refresh_profile();
	void _refresh_images();
	void _load_thumbnail(const String &p_abs_path, ThumbInfo &r_info);
	void _rebuild_gallery(HBoxContainer *p_gallery, const Vector<ThumbInfo> &p_thumbs, const String &p_category);

	// Internal helpers (continued)
	void _refresh_models();
	void _populate_model_picker(OptionButton *p_picker, const String &p_default_id);

	// HTTP callbacks
	void _on_profile_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _on_images_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _on_models_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _on_set_ref_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);

	// UI callbacks
	void _on_exploration_thumb_pressed(int p_index);
	void _on_cornerstone_thumb_pressed(int p_index);
	void _on_open_image_pressed(String p_abs_path);
	void _on_set_reference_pressed();
	void _on_exploration_refresh_pressed();
	void _on_cornerstone_refresh_pressed();
	void _on_gallery_refresh_timeout();

	String _get_project_root() const;
	String _abs_path_from_res(const String &p_res_path) const;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	static const int THUMB_SIZE = 120; // Larger since we have the full main screen

	String get_exploration_model() const;

	ArtDirectorPanel();
	~ArtDirectorPanel() = default;
};

// ── ArtDirectorPlugin: registers the panel as a main screen tab ─────────────

class ArtDirectorPlugin : public EditorPlugin {
	GDCLASS(ArtDirectorPlugin, EditorPlugin);

	ArtDirectorPanel *panel = nullptr;

protected:
	static void _bind_methods() {}

public:
	virtual String get_plugin_name() const override { return "Art Director"; }
	virtual bool has_main_screen() const override { return true; }
	virtual void make_visible(bool p_visible) override;

	ArtDirectorPlugin();
	~ArtDirectorPlugin() = default;
};
