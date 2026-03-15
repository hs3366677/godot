/**************************************************************************/
/*  ai_asset_generation_manager.cpp                                       */
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

#include "ai_asset_generation_manager.h"

#include "editor/plugins/ai_assistant/ai_assistant_dock.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "editor/dialogs/ai_prompt_editor_dialog.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/gui/editor_toaster.h"
#include "scene/main/http_request.h"
#include "scene/main/timer.h"

AIAssetGenerationManager *AIAssetGenerationManager::singleton = nullptr;

void AIAssetGenerationManager::_bind_methods() {
	ADD_SIGNAL(MethodInfo("pipeline_state_changed", PropertyInfo(Variant::STRING, "asset_path"), PropertyInfo(Variant::BOOL, "active"), PropertyInfo(Variant::FLOAT, "elapsed")));
}

AIAssetGenerationManager::AIAssetGenerationManager() {
	singleton = this;

	// Create HTTP request nodes
	generate_request = memnew(HTTPRequest);
	add_child(generate_request);
	generate_request->connect("request_completed", callable_mp(this, &AIAssetGenerationManager::_on_generate_completed));

	status_request = memnew(HTTPRequest);
	add_child(status_request);
	status_request->connect("request_completed", callable_mp(this, &AIAssetGenerationManager::_on_status_completed));

	download_request = memnew(HTTPRequest);
	add_child(download_request);
	download_request->connect("request_completed", callable_mp(this, &AIAssetGenerationManager::_on_download_completed));

	file_request = memnew(HTTPRequest);
	add_child(file_request);
	file_request->connect("request_completed", callable_mp(this, &AIAssetGenerationManager::_on_file_completed));

	// Create poll timer (for legacy direct generation)
	poll_timer = memnew(Timer);
	poll_timer->set_wait_time(2.0);
	poll_timer->set_one_shot(false);
	add_child(poll_timer);
	poll_timer->connect("timeout", callable_mp(this, &AIAssetGenerationManager::_on_poll_timeout));

	// Pipeline background session HTTP requests
	pipeline_session_request = memnew(HTTPRequest);
	add_child(pipeline_session_request);
	pipeline_session_request->connect("request_completed", callable_mp(this, &AIAssetGenerationManager::_on_pipeline_session_created));

	pipeline_prompt_request = memnew(HTTPRequest);
	add_child(pipeline_prompt_request);
	pipeline_prompt_request->connect("request_completed", callable_mp(this, &AIAssetGenerationManager::_on_pipeline_prompt_sent));

	pipeline_poll_request = memnew(HTTPRequest);
	add_child(pipeline_poll_request);
	pipeline_poll_request->connect("request_completed", callable_mp(this, &AIAssetGenerationManager::_on_pipeline_poll_completed));

	// Pipeline elapsed timer (1s tick)
	pipeline_elapsed_timer = memnew(Timer);
	pipeline_elapsed_timer->set_wait_time(1.0);
	pipeline_elapsed_timer->set_one_shot(false);
	add_child(pipeline_elapsed_timer);
	pipeline_elapsed_timer->connect("timeout", callable_mp(this, &AIAssetGenerationManager::_on_pipeline_elapsed_tick));

	// Pipeline poll timer (check session completion every 3s)
	pipeline_poll_timer = memnew(Timer);
	pipeline_poll_timer->set_wait_time(3.0);
	pipeline_poll_timer->set_one_shot(false);
	add_child(pipeline_poll_timer);
	pipeline_poll_timer->connect("timeout", callable_mp(this, &AIAssetGenerationManager::_on_pipeline_poll_timeout));

	// Create prompt editor dialog
	prompt_dialog = memnew(AIPromptEditorDialog);
	add_child(prompt_dialog);
	prompt_dialog->connect("prompt_confirmed", callable_mp(this, &AIAssetGenerationManager::_on_prompt_confirmed));
}

AIAssetGenerationManager::~AIAssetGenerationManager() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

Vector<String> AIAssetGenerationManager::_get_headers() const {
	Vector<String> headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back("Accept: application/json");
	headers.push_back("x-opencode-directory: " + _get_project_directory());
	return headers;
}

String AIAssetGenerationManager::_get_project_directory() const {
	String project_path = ProjectSettings::get_singleton()->get_resource_path();
	return ProjectSettings::get_singleton()->globalize_path(project_path);
}

void AIAssetGenerationManager::_toast(const String &p_message) {
	EditorToaster::get_singleton()->popup_str(p_message, EditorToaster::SEVERITY_INFO);
}

// ── Public API ───────────────────────────────────────────────────────────

void AIAssetGenerationManager::generate_from_placeholder(const String &p_path) {
	Dictionary asset_meta = AIAssetMetadata::get_metadata(p_path);
	if (asset_meta.is_empty()) {
		_toast(TTR("No AI metadata found for this asset."));
		return;
	}

	String prompt = asset_meta.get(AIAssetMetadata::KEY_PROMPT, "");
	String negative_prompt = asset_meta.get(AIAssetMetadata::KEY_NEGATIVE_PROMPT, "");
	String model = asset_meta.get(AIAssetMetadata::KEY_MODEL, "");

	if (prompt.is_empty()) {
		_toast(TTR("Asset has no prompt. Use 'Edit Prompt' first."));
		return;
	}

	_start_pipeline_generation(p_path, prompt, negative_prompt, model);
}

void AIAssetGenerationManager::quick_regenerate(const String &p_path) {
	Dictionary asset_meta = AIAssetMetadata::get_metadata(p_path);
	if (asset_meta.is_empty()) {
		_toast(TTR("No AI metadata found for this asset."));
		return;
	}

	String prompt = asset_meta.get(AIAssetMetadata::KEY_PROMPT, "");
	String negative_prompt = asset_meta.get(AIAssetMetadata::KEY_NEGATIVE_PROMPT, "");
	String model = asset_meta.get(AIAssetMetadata::KEY_MODEL, "");

	_start_pipeline_generation(p_path, prompt, negative_prompt, model);
}

void AIAssetGenerationManager::generate_with_params(const String &p_path, const String &p_prompt, const String &p_negative_prompt, const String &p_model, int p_seed) {
	_start_pipeline_generation(p_path, p_prompt, p_negative_prompt, p_model);
}

void AIAssetGenerationManager::open_prompt_editor(const String &p_path, AIPromptEditorDialog::Mode p_mode) {
	if (is_busy()) {
		_toast(TTR("A generation is already in progress."));
		return;
	}

	prompt_dialog->setup_for_asset(p_path, p_mode);
	prompt_dialog->popup_centered(Size2(650, 600));
}

void AIAssetGenerationManager::open_enhance_dialog(const String &p_path) {
	open_prompt_editor(p_path, AIPromptEditorDialog::MODE_TRANSFORM);
}

// ── Internal ─────────────────────────────────────────────────────────────

void AIAssetGenerationManager::_on_prompt_confirmed(const String &p_prompt, const String &p_negative_prompt, const String &p_model, int p_seed) {
	String path = prompt_dialog->get_asset_path();
	print_line(vformat("AIAssetGenerationManager: prompt_confirmed for '%s', model='%s'", path, p_model));

	Dictionary asset_meta = AIAssetMetadata::get_metadata(path);
	// Save updated prompt/negative_prompt/model/seed
	asset_meta[AIAssetMetadata::KEY_PROMPT] = p_prompt;
	asset_meta[AIAssetMetadata::KEY_NEGATIVE_PROMPT] = p_negative_prompt;
	asset_meta[AIAssetMetadata::KEY_MODEL] = p_model;
	asset_meta[AIAssetMetadata::KEY_SEED] = p_seed;
	AIAssetMetadata::set_metadata(path, asset_meta);

	_start_pipeline_generation(path, p_prompt, p_negative_prompt, p_model);
}

void AIAssetGenerationManager::_start_generation(const String &p_path, const String &p_prompt, const String &p_negative_prompt, const String &p_model, int p_seed) {
	state = STATE_GENERATING;
	current_asset_path = p_path;
	current_prompt = p_prompt;
	current_model = p_model;
	current_seed = p_seed;
	pending_assets.clear();
	download_index = 0;

	// Detect asset type from metadata or file extension
	Dictionary asset_meta = AIAssetMetadata::get_metadata(p_path);
	String asset_type = asset_meta.get(AIAssetMetadata::KEY_ASSET_TYPE, "texture");

	// Build request body
	Dictionary body;
	body["type"] = asset_type;
	body["prompt"] = p_prompt;
	if (!p_negative_prompt.is_empty()) {
		body["negativePrompt"] = p_negative_prompt;
	}
	if (!p_model.is_empty()) {
		body["model"] = p_model;
	}
	// Pass through metadata parameters (size, style, frames, etc.)
	Dictionary params;
	Dictionary meta_params = asset_meta.get(AIAssetMetadata::KEY_PARAMETERS, Dictionary());
	LocalVector<Variant> param_keys = meta_params.get_key_list();
	for (const Variant &key : param_keys) {
		params[key] = meta_params[key];
	}
	body["parameters"] = params;

	String json_body = JSON::stringify(body);
	String url = service_url + "/ai-assets/generate";

	print_line(vformat("AIGeneration: POST %s body=%s", url, json_body.left(300)));
	_toast(vformat(TTR("Generating asset: %s..."), p_path.get_file()));

	Error err = generate_request->request(url, _get_headers(), HTTPClient::METHOD_POST, json_body);
	if (err != OK) {
		print_line(vformat("AIGeneration: request() returned error %d", (int)err));
		_finish_generation(false, TTR("Failed to start generation."));
	}
}

void AIAssetGenerationManager::_start_pipeline_generation(const String &p_path, const String &p_prompt, const String &p_negative_prompt, const String &p_model) {
	// Route generation through a BACKGROUND session — creates a new session,
	// sends the pipeline prompt, and polls for completion. Does NOT use the
	// visible AA chat session.

	AIAssistantDock *aa = AIAssistantDock::get_singleton();
	if (!aa) {
		_toast(TTR("AI Assistant not available."));
		return;
	}

	pipeline_service_url = aa->get_service_url();
	if (pipeline_service_url.is_empty()) {
		_toast(TTR("AI Assistant not connected. Connect first, then retry."));
		return;
	}

	state = STATE_PIPELINE;
	current_asset_path = p_path;
	pipeline_elapsed_seconds = 0.0;

	// Detect asset type
	Dictionary asset_meta = AIAssetMetadata::get_metadata(p_path);
	String asset_type = asset_meta.get(AIAssetMetadata::KEY_ASSET_TYPE, "sprite");

	// Build the AI prompt that instructs the LLM to call godot_asset_pipeline
	pipeline_ai_prompt = vformat(
			"Generate the asset at `%s` using `godot_asset_pipeline` with these parameters:\n"
			"- prompt: \"%s\"\n"
			"- destination: \"%s\"\n"
			"- asset_type: \"%s\"",
			p_path, p_prompt.replace("\"", "\\\""), p_path, asset_type);
	if (!p_negative_prompt.is_empty()) {
		pipeline_ai_prompt += vformat("\n- negative_prompt: \"%s\"", p_negative_prompt.replace("\"", "\\\""));
	}
	if (!p_model.is_empty()) {
		pipeline_ai_prompt += vformat("\n- model: \"%s\"", p_model);
	}
	pipeline_ai_prompt += "\n\nCall the tool now. Score the result and retry if needed.";

	// Step 1: Create a background session
	Dictionary body;
	body["title"] = vformat("Asset Pipeline: %s", p_path.get_file());
	String json_body = JSON::stringify(body);
	String url = pipeline_service_url + "/session?directory=" + _get_project_directory().uri_encode();

	print_line(vformat("AIGeneration: creating background session for pipeline: %s", p_path));

	Error err = pipeline_session_request->request(url, _get_headers(), HTTPClient::METHOD_POST, json_body);
	if (err != OK) {
		_finish_pipeline(false, TTR("Failed to create background session."));
		return;
	}

	// Start elapsed timer immediately
	pipeline_elapsed_timer->start();
	emit_signal("pipeline_state_changed", current_asset_path, true, 0.0);
}

void AIAssetGenerationManager::_on_pipeline_session_created(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (p_result != HTTPRequest::RESULT_SUCCESS || (p_code != 200 && p_code != 201)) {
		_finish_pipeline(false, TTR("Failed to create background session."));
		return;
	}

	String body_str = String::utf8((const char *)p_body.ptr(), p_body.size());
	JSON json;
	if (json.parse(body_str) != OK) {
		_finish_pipeline(false, TTR("Invalid session creation response."));
		return;
	}

	Dictionary resp = json.get_data();
	pipeline_session_id = resp.get("id", "");
	if (pipeline_session_id.is_empty()) {
		_finish_pipeline(false, TTR("No session ID in response."));
		return;
	}

	print_line(vformat("AIGeneration: background session created: %s", pipeline_session_id));

	// Step 2: Send the pipeline prompt via prompt_async
	Dictionary body;
	Array parts;
	Dictionary text_part;
	text_part["type"] = "text";
	text_part["text"] = pipeline_ai_prompt;
	parts.push_back(text_part);
	body["parts"] = parts;

	String json_body = JSON::stringify(body);
	String url = pipeline_service_url + "/session/" + pipeline_session_id + "/prompt_async?directory=" + _get_project_directory().uri_encode();

	Error err = pipeline_prompt_request->request(url, _get_headers(), HTTPClient::METHOD_POST, json_body);
	if (err != OK) {
		_finish_pipeline(false, TTR("Failed to send pipeline prompt."));
	}
}

void AIAssetGenerationManager::_on_pipeline_prompt_sent(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (p_result != HTTPRequest::RESULT_SUCCESS || (p_code != 200 && p_code != 204)) {
		_finish_pipeline(false, TTR("Failed to start pipeline generation."));
		return;
	}

	print_line("AIGeneration: pipeline prompt sent, starting poll timer");

	// Start polling for completion
	pipeline_poll_timer->start();
}

void AIAssetGenerationManager::_on_pipeline_elapsed_tick() {
	pipeline_elapsed_seconds += 1.0;
	emit_signal("pipeline_state_changed", current_asset_path, true, pipeline_elapsed_seconds);
}

void AIAssetGenerationManager::_on_pipeline_poll_timeout() {
	if (state != STATE_PIPELINE || pipeline_session_id.is_empty()) {
		pipeline_poll_timer->stop();
		return;
	}

	// Check session messages — if an assistant message exists, the pipeline is done
	String url = pipeline_service_url + "/session/" + pipeline_session_id + "/message?directory=" + _get_project_directory().uri_encode();
	pipeline_poll_request->request(url, _get_headers());
}

void AIAssetGenerationManager::_on_pipeline_poll_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		return; // Keep polling — transient error
	}

	String body_str = String::utf8((const char *)p_body.ptr(), p_body.size());
	JSON json;
	if (json.parse(body_str) != OK) {
		return;
	}

	// The response is an array of messages. We look for an assistant message
	// which indicates the pipeline has completed (either success or failure).
	Array messages = json.get_data();
	bool has_assistant_response = false;
	for (int i = 0; i < messages.size(); i++) {
		Dictionary msg = messages[i];
		String role = msg.get("role", "");
		if (role == "assistant") {
			has_assistant_response = true;
			break;
		}
	}

	if (has_assistant_response) {
		// Pipeline completed — trigger reimport and finish
		EditorFileSystem::get_singleton()->scan_changes();
		_finish_pipeline(true);
	}
	// Otherwise keep polling
}

void AIAssetGenerationManager::cancel_pipeline() {
	if (state != STATE_PIPELINE) {
		return;
	}
	print_line("AIGeneration: pipeline cancelled by user");
	_finish_pipeline(false, TTR("Pipeline generation cancelled."));
}

void AIAssetGenerationManager::_finish_pipeline(bool p_success, const String &p_message) {
	pipeline_elapsed_timer->stop();
	pipeline_poll_timer->stop();

	String asset_path = current_asset_path;
	double elapsed = pipeline_elapsed_seconds;

	pipeline_session_id = "";
	pipeline_service_url = "";
	pipeline_ai_prompt = "";
	pipeline_elapsed_seconds = 0.0;
	state = STATE_IDLE;

	if (p_success) {
		_toast(vformat(TTR("Asset generated: %s (%.0fs)"), asset_path.get_file(), elapsed));
	} else {
		String msg = p_message.is_empty() ? TTR("Pipeline generation failed.") : p_message;
		EditorToaster::get_singleton()->popup_str(msg, EditorToaster::SEVERITY_ERROR);
	}

	current_asset_path = "";
	emit_signal("pipeline_state_changed", asset_path, false, elapsed);
}

void AIAssetGenerationManager::_on_generate_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		String body_str;
		if (p_body.size() > 0) {
			body_str = String::utf8((const char *)p_body.ptr(), p_body.size());
		}
		print_line(vformat("AIGeneration: generate failed result=%d code=%d body=%s", p_result, p_code, body_str.left(200)));
		_finish_generation(false, TTR("Failed to start generation."));
		return;
	}

	String response_str = String::utf8((const char *)p_body.ptr(), p_body.size());

	JSON json;
	if (json.parse(response_str) != OK) {
		_finish_generation(false, TTR("Invalid response from server."));
		return;
	}

	Dictionary resp = json.get_data();
	current_generation_id = resp.get("generationId", "");
	current_provider_id = resp.get("providerId", "");
	String status = resp.get("status", "");

	if (current_generation_id.is_empty() || current_provider_id.is_empty()) {
		_finish_generation(false, TTR("Server response missing generationId or providerId."));
		return;
	}

	if (status == "completed") {
		// Already done — go straight to download
		state = STATE_DOWNLOADING;
		String url = vformat("%s/ai-assets/download/%s/%s", service_url, current_provider_id, current_generation_id);
		download_request->request(url, _get_headers());
	} else {
		// Start polling
		state = STATE_POLLING;
		poll_timer->start();
	}
}

void AIAssetGenerationManager::_on_poll_timeout() {
	if (state != STATE_POLLING) {
		poll_timer->stop();
		return;
	}

	String url = vformat("%s/ai-assets/status/%s/%s", service_url, current_provider_id, current_generation_id);
	status_request->request(url, _get_headers());
}

void AIAssetGenerationManager::_on_status_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		String body_str;
		if (p_body.size() > 0) {
			body_str = String::utf8((const char *)p_body.ptr(), p_body.size());
		}
		print_line(vformat("AIGeneration: status check failed result=%d code=%d body=%s", p_result, p_code, body_str.left(200)));
		_finish_generation(false, TTR("Failed to check generation status."));
		return;
	}

	String response_str = String::utf8((const char *)p_body.ptr(), p_body.size());

	JSON json;
	if (json.parse(response_str) != OK) {
		_finish_generation(false, TTR("Invalid status response."));
		return;
	}

	Dictionary resp = json.get_data();
	String status = resp.get("status", "");
	print_line(vformat("AIGeneration: poll status='%s' for %s/%s", status, current_provider_id, current_generation_id));

	if (status == "completed") {
		poll_timer->stop();
		state = STATE_DOWNLOADING;
		String url = vformat("%s/ai-assets/download/%s/%s", service_url, current_provider_id, current_generation_id);
		download_request->request(url, _get_headers());
	} else if (status == "failed") {
		String message = resp.get("message", TTR("Generation failed."));
		print_line(vformat("AIGeneration: server reported failure: %s", message));
		_finish_generation(false, message);
	}
	// Otherwise keep polling (status is "pending" or "processing")
}

void AIAssetGenerationManager::_on_download_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		_finish_generation(false, TTR("Failed to download asset bundle."));
		return;
	}

	String response_str = String::utf8((const char *)p_body.ptr(), p_body.size());

	JSON json;
	if (json.parse(response_str) != OK) {
		_finish_generation(false, TTR("Invalid download response."));
		return;
	}

	Dictionary resp = json.get_data();
	Array assets = resp.get("assets", Array());

	if (assets.is_empty()) {
		_finish_generation(false, TTR("No assets in bundle."));
		return;
	}

	pending_assets.clear();
	for (int i = 0; i < assets.size(); i++) {
		Dictionary asset = assets[i];
		PendingAsset pa;
		pa.type = asset.get("type", "");
		pa.role = asset.get("role", "");
		pa.filename = asset.get("filename", "");
		pa.size = (int)asset.get("size", 0);
		pending_assets.push_back(pa);
	}

	download_index = 0;
	state = STATE_DOWNLOADING_FILE;
	_download_next_file();
}

void AIAssetGenerationManager::_download_next_file() {
	if (download_index >= pending_assets.size()) {
		// All files downloaded — post-process the active asset, then trigger reimport.
		state = STATE_SAVING;

		_post_process_asset();

		// Trigger reimport
		EditorFileSystem::get_singleton()->scan_changes();

		_finish_generation(true);
		return;
	}

	const PendingAsset &pa = pending_assets[download_index];
	String url = vformat("%s/ai-assets/download/%s/%s/%s",
			service_url, current_provider_id, current_generation_id, pa.filename);
	file_request->request(url, _get_headers());
}

void AIAssetGenerationManager::_on_file_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (p_result != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		_finish_generation(false, vformat(TTR("Failed to download file: %s"), pending_assets[download_index].filename));
		return;
	}

	// Write the binary to disk — primary asset goes to current_asset_path,
	// additional bundle files go to the same directory
	String dest_path;
	if (download_index == 0) {
		dest_path = current_asset_path;
	} else {
		String dir = current_asset_path.get_base_dir();
		dest_path = dir.path_join(pending_assets[download_index].filename);
	}

	Ref<FileAccess> f = FileAccess::open(dest_path, FileAccess::WRITE);
	if (f.is_valid()) {
		f->store_buffer(p_body.ptr(), p_body.size());
		f.unref();
	} else {
		_finish_generation(false, vformat(TTR("Failed to write file: %s"), dest_path));
		return;
	}

	// For the primary asset: save the RAW file as a version snapshot before any post-processing.
	// The version history preserves the unmodified provider output; the active file will be post-processed later.
	if (download_index == 0) {
		// Save current (old) version if it exists
		AIAssetMetadata::save_version(current_asset_path);

		// Update metadata: origin → "generated", increment version
		Dictionary asset_meta = AIAssetMetadata::get_metadata(current_asset_path);
		asset_meta[AIAssetMetadata::KEY_ORIGIN] = AIAssetMetadata::origin_to_string(AIAssetMetadata::ORIGIN_GENERATED);
		asset_meta[AIAssetMetadata::KEY_PROMPT] = current_prompt;
		asset_meta[AIAssetMetadata::KEY_MODEL] = current_model;
		asset_meta[AIAssetMetadata::KEY_SEED] = current_seed;
		asset_meta[AIAssetMetadata::KEY_GENERATED_AT] = AIAssetMetadata::get_current_timestamp();
		int version = (int)asset_meta.get(AIAssetMetadata::KEY_VERSION, 0) + 1;
		asset_meta[AIAssetMetadata::KEY_VERSION] = version;
		AIAssetMetadata::set_metadata(current_asset_path, asset_meta);

		// Save the new version — this copies the raw (unprocessed) file to .ai.{name}/vN.png
		AIAssetMetadata::save_version(current_asset_path);
	}

	download_index++;
	_download_next_file();
}

void AIAssetGenerationManager::post_process_asset(const String &p_path) {
	String saved = current_asset_path;
	current_asset_path = p_path;
	_post_process_asset();
	current_asset_path = saved;
}

void AIAssetGenerationManager::_post_process_asset() {
	// Post-process the active asset file (crop, bg removal, resize).
	// The raw file has already been saved to version history; this only modifies the working copy.
	Dictionary asset_meta = AIAssetMetadata::get_metadata(current_asset_path);
	Dictionary meta_params = asset_meta.get(AIAssetMetadata::KEY_PARAMETERS, Dictionary());

	String global_path = ProjectSettings::get_singleton()->globalize_path(current_asset_path);
	Ref<Image> img;
	img.instantiate();
	if (img->load(global_path) != OK) {
		return; // Not an image (e.g. .glb) — skip all post-processing.
	}

	bool modified = false;

	// Step 1+2: Crop and resize (only if size is specified)
	String size_str = meta_params.get("size", "");
	Vector<String> parts = size_str.split("x");
	if (parts.size() == 2) {
		int target_w = parts[0].to_int();
		int target_h = parts[1].to_int();
		if (target_w > 0 && target_h > 0) {
			// Auto-crop blank borders
			int left = img->get_width(), top = img->get_height(), right = 0, bottom = 0;
			for (int y = 0; y < img->get_height(); y++) {
				for (int x = 0; x < img->get_width(); x++) {
					Color c = img->get_pixel(x, y);
					if (c.a > 0.1f) {
						left = MIN(left, x);
						top = MIN(top, y);
						right = MAX(right, x);
						bottom = MAX(bottom, y);
					}
				}
			}

			// Crop and resize to target dimensions
			if (right >= left && bottom >= top) {
				Ref<Image> cropped = img->get_region(Rect2i(left, top, right - left + 1, bottom - top + 1));
				cropped->resize(target_w, target_h, Image::INTERPOLATE_NEAREST);
				cropped->save_png(global_path);
			} else {
				img->resize(target_w, target_h, Image::INTERPOLATE_NEAREST);
				img->save_png(global_path);
			}
			print_line(vformat("AI Asset: cropped and resized to %dx%d", target_w, target_h));
			return;
		}
	}

	// Transparent BG without size: auto-crop to solid pixels and save
	if (modified) {
		int left = img->get_width(), top = img->get_height(), right = 0, bottom = 0;
		for (int y = 0; y < img->get_height(); y++) {
			for (int x = 0; x < img->get_width(); x++) {
				Color c = img->get_pixel(x, y);
				if (c.a > 0.1f) {
					left = MIN(left, x);
					top = MIN(top, y);
					right = MAX(right, x);
					bottom = MAX(bottom, y);
				}
			}
		}
		if (right >= left && bottom >= top) {
			Ref<Image> cropped = img->get_region(Rect2i(left, top, right - left + 1, bottom - top + 1));
			cropped->save_png(global_path);
			print_line(vformat("AI Asset: cropped to solid pixels %dx%d", right - left + 1, bottom - top + 1));
		} else {
			img->save_png(global_path);
		}
	}
}

void AIAssetGenerationManager::_finish_generation(bool p_success, const String &p_message) {
	poll_timer->stop();
	state = STATE_IDLE;
	current_generation_id = "";
	current_provider_id = "";
	pending_assets.clear();
	download_index = 0;

	if (p_success) {
		_toast(vformat(TTR("Asset generated: %s"), current_asset_path.get_file()));
	} else {
		String msg = p_message.is_empty() ? TTR("Asset generation failed.") : p_message;
		EditorToaster::get_singleton()->popup_str(msg, EditorToaster::SEVERITY_ERROR);
	}

	current_asset_path = "";
}
