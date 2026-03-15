/**************************************************************************/
/*  art_director_dock.h                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                           MAKABAKA ENGINE                              */
/*                    AI-powered game creation module                     */
/**************************************************************************/

#pragma once

#include "editor/plugins/editor_plugin.h"

#include "core/input/input_event.h"
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

	// ── Image data (must be before SessionInfo) ─────────────────────────
	struct ThumbInfo {
		String res_path;
		String category; // "exploration" or "cornerstone"
		String label;    // short display name (from metadata, cornerstone only)
		String tooltip;  // full subject description for hover
		Ref<ImageTexture> texture;
	};

	// ── Model selection (top) ────────────────────────────────────────────
	OptionButton *model_picker = nullptr;

	// ── ① Style Explorations ────────────────────────────────────────────
	HBoxContainer *exploration_gallery = nullptr;
	Button *session_prev_button = nullptr;
	Button *session_next_button = nullptr;
	Label *session_label = nullptr;
	Button *exploration_refresh_button = nullptr;

	// Session tracking
	int current_session_index = 0;
	struct SessionInfo {
		String id;
		Vector<ThumbInfo> images;
	};
	Vector<SessionInfo> exploration_sessions;

	// ── ② Selected Style ────────────────────────────────────────────────
	TextureRect *selected_style_thumb = nullptr;
	Label *selected_style_label = nullptr;
	Label *selected_ref_label = nullptr;

	// ── ③ Cornerstone Assets ────────────────────────────────────────────
	HBoxContainer *cornerstone_gallery = nullptr;
	Button *cornerstone_refresh_button = nullptr;
	Vector<ThumbInfo> cornerstone_thumbs;

	// ── HTTP infrastructure ──────────────────────────────────────────────
	HTTPRequest *profile_http_request = nullptr;
	HTTPRequest *images_http_request = nullptr;
	HTTPRequest *models_http_request = nullptr;
	Timer *gallery_refresh_timer = nullptr;
	bool images_request_in_progress = false;

	String service_url = "http://localhost:4096";

	// Thumbnail interaction
	void _on_thumb_gui_input(const Ref<InputEvent> &p_event, String p_abs_path);

	// ── Internal helpers ─────────────────────────────────────────────────
	void _setup_ui();
	void _refresh_profile();
	void _refresh_images();
	void _load_thumbnail(const String &p_abs_path, ThumbInfo &r_info);
	void _rebuild_gallery(HBoxContainer *p_gallery, const Vector<ThumbInfo> &p_thumbs, const String &p_category);
	void _rebuild_exploration_gallery();

	// Internal helpers (continued)
	void _refresh_models();
	void _populate_model_picker(OptionButton *p_picker, const String &p_default_id);

	// HTTP callbacks
	void _on_profile_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _on_images_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _on_models_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);

	// UI callbacks
	void _on_open_image_pressed(String p_abs_path);
	void _on_exploration_refresh_pressed();
	void _on_cornerstone_refresh_pressed();
	void _on_gallery_refresh_timeout();
	void _on_session_prev_pressed();
	void _on_session_next_pressed();

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
