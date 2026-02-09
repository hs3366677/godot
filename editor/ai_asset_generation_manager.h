/**************************************************************************/
/*  ai_asset_generation_manager.h                                         */
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
#include "editor/dialogs/ai_prompt_editor_dialog.h"
#include "scene/main/node.h"
class HTTPRequest;
class Timer;

class AIAssetGenerationManager : public Node {
	GDCLASS(AIAssetGenerationManager, Node);

public:
	enum State {
		STATE_IDLE,
		STATE_GENERATING,
		STATE_POLLING,
		STATE_DOWNLOADING,
		STATE_DOWNLOADING_FILE,
		STATE_SAVING,
	};

private:
	static AIAssetGenerationManager *singleton;

	String service_url = "http://localhost:4096";

	// HTTP requests
	HTTPRequest *generate_request = nullptr;
	HTTPRequest *status_request = nullptr;
	HTTPRequest *download_request = nullptr;
	HTTPRequest *file_request = nullptr;

	// Polling
	Timer *poll_timer = nullptr;

	// Dialog
	AIPromptEditorDialog *prompt_dialog = nullptr;

	// Current job state
	State state = STATE_IDLE;
	String current_asset_path;
	String current_generation_id;
	String current_provider_id;
	String current_prompt;
	String current_model;
	int current_seed = -1;

	// Download state
	struct PendingAsset {
		String type;
		String role;
		String filename;
		int size = 0;
	};
	Vector<PendingAsset> pending_assets;
	int download_index = 0;

	// HTTP helpers
	Vector<String> _get_headers() const;
	String _get_project_directory() const;

	// HTTP callbacks
	void _on_generate_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _on_status_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _on_download_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _on_file_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);

	// Internal
	void _on_poll_timeout();
	void _on_prompt_confirmed(const String &p_prompt, const String &p_negative_prompt, const String &p_model, int p_seed);
	void _start_generation(const String &p_path, const String &p_prompt, const String &p_negative_prompt, const String &p_model, int p_seed);
	void _finish_generation(bool p_success, const String &p_message = "");
	void _download_next_file();
	void _post_process_asset();
	void _toast(const String &p_message);

protected:
	static void _bind_methods();

public:
	static AIAssetGenerationManager *get_singleton() { return singleton; }

	void generate_from_placeholder(const String &p_path);
	void quick_regenerate(const String &p_path);
	void generate_with_params(const String &p_path, const String &p_prompt, const String &p_negative_prompt, const String &p_model, int p_seed);
	void open_prompt_editor(const String &p_path, AIPromptEditorDialog::Mode p_mode);
	void open_enhance_dialog(const String &p_path);

	bool is_busy() const { return state != STATE_IDLE; }
	State get_state() const { return state; }

	AIAssetGenerationManager();
	~AIAssetGenerationManager();
};
