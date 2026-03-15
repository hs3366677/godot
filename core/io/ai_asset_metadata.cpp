/**************************************************************************/
/*  ai_asset_metadata.cpp                                                 */
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

#include "ai_asset_metadata.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "core/os/time.h"

// Forward declarations of file-scope helpers (defined in Version History section below).
static Dictionary _read_json_file(const String &p_path);
static Error _write_json_file(const String &p_path, const Dictionary &p_data);

// Metadata key constants
const char *AIAssetMetadata::KEY_ORIGIN = "origin";
const char *AIAssetMetadata::KEY_IMPORTED_FROM = "imported_from";
const char *AIAssetMetadata::KEY_IMPORTED_AT = "imported_at";
const char *AIAssetMetadata::KEY_ORIGINAL_FILENAME = "original_filename";
const char *AIAssetMetadata::KEY_ORIGINAL_SIZE_BYTES = "original_size_bytes";
const char *AIAssetMetadata::KEY_PROMPT = "prompt";
const char *AIAssetMetadata::KEY_NEGATIVE_PROMPT = "negative_prompt";
const char *AIAssetMetadata::KEY_PROVIDER = "provider";
const char *AIAssetMetadata::KEY_MODEL = "model";
const char *AIAssetMetadata::KEY_SEED = "seed";
const char *AIAssetMetadata::KEY_PARAMETERS = "parameters";
const char *AIAssetMetadata::KEY_GENERATED_AT = "generated_at";
const char *AIAssetMetadata::KEY_VERSION = "version";
const char *AIAssetMetadata::KEY_BUNDLE_ID = "bundle_id";
const char *AIAssetMetadata::KEY_BUNDLE_ROLE = "bundle_role";
const char *AIAssetMetadata::KEY_BUNDLE_RELATED = "bundle_related";
const char *AIAssetMetadata::KEY_SOURCE_ASSET = "source_asset";
const char *AIAssetMetadata::KEY_TRANSFORM = "transform";
const char *AIAssetMetadata::KEY_ASSET_TYPE = "asset_type";
const char *AIAssetMetadata::KEY_USAGE = "usage";
const char *AIAssetMetadata::KEY_CREATED_AT = "created_at";
const char *AIAssetMetadata::KEY_CREATED_BY = "created_by";
const char *AIAssetMetadata::KEY_HISTORY = "history";
const char *AIAssetMetadata::KEY_CURRENT_VERSION = "current_version";

void AIAssetMetadata::_bind_methods() {
	// Bind enum
	BIND_ENUM_CONSTANT(ORIGIN_UNKNOWN);
	BIND_ENUM_CONSTANT(ORIGIN_PLACEHOLDER);
	BIND_ENUM_CONSTANT(ORIGIN_IMPORTED);
	BIND_ENUM_CONSTANT(ORIGIN_GENERATED);
	BIND_ENUM_CONSTANT(ORIGIN_HYBRID);

	// Bind static methods
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("create_import_metadata", "source_path", "original_filename", "size_bytes"), &AIAssetMetadata::create_import_metadata, DEFVAL(0));
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("create_placeholder_metadata", "asset_type", "prompt", "provider", "model", "parameters", "usage"), &AIAssetMetadata::create_placeholder_metadata, DEFVAL(Dictionary()), DEFVAL(Dictionary()));
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("create_generation_metadata", "prompt", "provider", "model", "seed", "parameters"), &AIAssetMetadata::create_generation_metadata, DEFVAL(-1), DEFVAL(Dictionary()));
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("create_hybrid_metadata", "source_asset", "transform_type", "prompt", "provider", "model"), &AIAssetMetadata::create_hybrid_metadata);

	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("get_origin", "path"), &AIAssetMetadata::get_origin);
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("origin_to_string", "origin"), &AIAssetMetadata::origin_to_string);
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("string_to_origin", "str"), &AIAssetMetadata::string_to_origin);
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("is_ai_generated", "path"), &AIAssetMetadata::is_ai_generated);
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("is_user_imported", "path"), &AIAssetMetadata::is_user_imported);
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("is_placeholder", "path"), &AIAssetMetadata::is_placeholder);
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("get_metadata", "path"), &AIAssetMetadata::get_metadata);
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("set_metadata", "path", "metadata"), &AIAssetMetadata::set_metadata);
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("update_metadata", "path", "updates"), &AIAssetMetadata::update_metadata);

	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("get_current_timestamp"), &AIAssetMetadata::get_current_timestamp);

	// Version history
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("get_version_dir", "asset_path"), &AIAssetMetadata::get_version_dir);
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("get_version_file_path", "asset_path", "version"), &AIAssetMetadata::get_version_file_path);
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("get_version_meta_path", "asset_path", "version"), &AIAssetMetadata::get_version_meta_path);
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("read_version_index", "asset_path"), &AIAssetMetadata::read_version_index);
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("read_version_meta", "asset_path", "version"), &AIAssetMetadata::read_version_meta);
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("save_version", "asset_path"), &AIAssetMetadata::save_version);
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("use_version", "asset_path", "version"), &AIAssetMetadata::use_version);
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("delete_version", "asset_path", "version"), &AIAssetMetadata::delete_version);
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("list_versions", "asset_path"), &AIAssetMetadata::list_versions);
	ClassDB::bind_static_method("AIAssetMetadata", D_METHOD("get_current_version", "asset_path"), &AIAssetMetadata::get_current_version);
}

String AIAssetMetadata::get_current_timestamp() {
	Dictionary datetime = Time::get_singleton()->get_datetime_dict_from_system();
	return vformat("%04d-%02d-%02dT%02d:%02d:%02dZ",
			(int)datetime["year"],
			(int)datetime["month"],
			(int)datetime["day"],
			(int)datetime["hour"],
			(int)datetime["minute"],
			(int)datetime["second"]);
}

String AIAssetMetadata::origin_to_string(Origin p_origin) {
	switch (p_origin) {
		case ORIGIN_PLACEHOLDER:
			return "placeholder";
		case ORIGIN_IMPORTED:
			return "imported";
		case ORIGIN_GENERATED:
			return "generated";
		case ORIGIN_HYBRID:
			return "hybrid";
		default:
			return "unknown";
	}
}

AIAssetMetadata::Origin AIAssetMetadata::string_to_origin(const String &p_str) {
	if (p_str == "placeholder") {
		return ORIGIN_PLACEHOLDER;
	} else if (p_str == "imported") {
		return ORIGIN_IMPORTED;
	} else if (p_str == "generated") {
		return ORIGIN_GENERATED;
	} else if (p_str == "hybrid") {
		return ORIGIN_HYBRID;
	}
	return ORIGIN_UNKNOWN;
}

Dictionary AIAssetMetadata::create_import_metadata(
		const String &p_source_path,
		const String &p_original_filename,
		int64_t p_size_bytes) {
	Dictionary metadata;
	metadata[KEY_ORIGIN] = origin_to_string(ORIGIN_IMPORTED);
	metadata[KEY_IMPORTED_FROM] = p_source_path;
	metadata[KEY_IMPORTED_AT] = get_current_timestamp();
	metadata[KEY_ORIGINAL_FILENAME] = p_original_filename;
	if (p_size_bytes > 0) {
		metadata[KEY_ORIGINAL_SIZE_BYTES] = p_size_bytes;
	}
	return metadata;
}

Dictionary AIAssetMetadata::create_placeholder_metadata(
		const String &p_asset_type,
		const String &p_prompt,
		const String &p_provider,
		const String &p_model,
		const Dictionary &p_parameters,
		const Dictionary &p_usage) {
	Dictionary metadata;
	metadata[KEY_ORIGIN] = origin_to_string(ORIGIN_PLACEHOLDER);
	metadata[KEY_ASSET_TYPE] = p_asset_type;
	metadata[KEY_PROMPT] = p_prompt;
	metadata[KEY_PROVIDER] = p_provider;
	metadata[KEY_MODEL] = p_model;
	if (!p_parameters.is_empty()) {
		metadata[KEY_PARAMETERS] = p_parameters;
	}
	metadata[KEY_CREATED_AT] = get_current_timestamp();
	metadata[KEY_CREATED_BY] = "ai_assistant";
	if (!p_usage.is_empty()) {
		metadata[KEY_USAGE] = p_usage;
	}
	return metadata;
}

Dictionary AIAssetMetadata::create_generation_metadata(
		const String &p_prompt,
		const String &p_provider,
		const String &p_model,
		int p_seed,
		const Dictionary &p_parameters) {
	Dictionary metadata;
	metadata[KEY_ORIGIN] = origin_to_string(ORIGIN_GENERATED);
	metadata[KEY_PROMPT] = p_prompt;
	metadata[KEY_PROVIDER] = p_provider;
	metadata[KEY_MODEL] = p_model;
	metadata[KEY_SEED] = p_seed;
	if (!p_parameters.is_empty()) {
		metadata[KEY_PARAMETERS] = p_parameters;
	}
	metadata[KEY_GENERATED_AT] = get_current_timestamp();
	metadata[KEY_VERSION] = 1;
	return metadata;
}

Dictionary AIAssetMetadata::create_hybrid_metadata(
		const String &p_source_asset,
		const String &p_transform_type,
		const String &p_prompt,
		const String &p_provider,
		const String &p_model) {
	Dictionary metadata;
	metadata[KEY_ORIGIN] = origin_to_string(ORIGIN_HYBRID);
	metadata[KEY_SOURCE_ASSET] = p_source_asset;
	metadata[KEY_TRANSFORM] = p_transform_type;
	metadata[KEY_PROMPT] = p_prompt;
	metadata[KEY_PROVIDER] = p_provider;
	metadata[KEY_MODEL] = p_model;
	metadata[KEY_GENERATED_AT] = get_current_timestamp();
	metadata[KEY_VERSION] = 1;
	return metadata;
}

Dictionary AIAssetMetadata::create_bundle_metadata(
		const String &p_bundle_id,
		const String &p_role,
		const Vector<String> &p_related_paths) {
	Dictionary bundle;
	bundle[KEY_BUNDLE_ID] = p_bundle_id;
	bundle[KEY_BUNDLE_ROLE] = p_role;
	Array related;
	for (int i = 0; i < p_related_paths.size(); i++) {
		related.push_back(p_related_paths[i]);
	}
	bundle[KEY_BUNDLE_RELATED] = related;
	return bundle;
}

Error AIAssetMetadata::append_version_history(const String &p_path, const Dictionary &p_version_entry) {
	Dictionary metadata = get_metadata(p_path);

	Array history;
	if (metadata.has("history")) {
		history = metadata["history"];
	}
	history.push_back(p_version_entry);
	metadata["history"] = history;

	return set_metadata(p_path, metadata);
}

AIAssetMetadata::Origin AIAssetMetadata::get_origin(const String &p_path) {
	Dictionary metadata = get_metadata(p_path);
	if (metadata.is_empty() || !metadata.has(KEY_ORIGIN)) {
		return ORIGIN_UNKNOWN;
	}
	return string_to_origin(metadata[KEY_ORIGIN]);
}

bool AIAssetMetadata::is_ai_generated(const String &p_path) {
	Origin origin = get_origin(p_path);
	return origin == ORIGIN_GENERATED || origin == ORIGIN_HYBRID;
}

bool AIAssetMetadata::is_user_imported(const String &p_path) {
	return get_origin(p_path) == ORIGIN_IMPORTED;
}

bool AIAssetMetadata::is_placeholder(const String &p_path) {
	return get_origin(p_path) == ORIGIN_PLACEHOLDER;
}

Dictionary AIAssetMetadata::get_metadata(const String &p_path) {
	// Single source of truth: .ai.{filename}/metadata.json
	return _read_json_file(get_version_index_path(p_path));
}

Error AIAssetMetadata::set_metadata(const String &p_path, const Dictionary &p_metadata) {
	// Write to version dir metadata.json (single source of truth)
	String ver_dir = get_version_dir(p_path);
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (!da->dir_exists(ver_dir)) {
		Error err = da->make_dir_recursive(ver_dir);
		if (err != OK) {
			return err;
		}
	}
	return _write_json_file(get_version_index_path(p_path), p_metadata);
}

Error AIAssetMetadata::update_metadata(const String &p_path, const Dictionary &p_updates) {
	Dictionary existing = get_metadata(p_path);

	// Merge updates into existing
	Array keys = p_updates.keys();
	for (int i = 0; i < keys.size(); i++) {
		existing[keys[i]] = p_updates[keys[i]];
	}

	return set_metadata(p_path, existing);
}

Vector<String> AIAssetMetadata::get_bundle_members(const String &p_path) {
	Vector<String> members;
	Dictionary metadata = get_metadata(p_path);

	if (!metadata.has(KEY_BUNDLE_RELATED)) {
		return members;
	}

	Array related = metadata[KEY_BUNDLE_RELATED];
	for (int i = 0; i < related.size(); i++) {
		members.push_back(related[i]);
	}

	return members;
}

Error AIAssetMetadata::link_bundle_assets(const Vector<String> &p_paths, const String &p_bundle_id) {
	if (p_paths.size() < 2) {
		return ERR_INVALID_PARAMETER;
	}

	// Link all assets to each other
	for (int i = 0; i < p_paths.size(); i++) {
		Vector<String> related;
		for (int j = 0; j < p_paths.size(); j++) {
			if (i != j) {
				related.push_back(p_paths[j]);
			}
		}

		String role = (i == 0) ? "primary" : "related";
		Dictionary bundle = create_bundle_metadata(p_bundle_id, role, related);

		Dictionary existing = get_metadata(p_paths[i]);
		existing[KEY_BUNDLE_ID] = bundle[KEY_BUNDLE_ID];
		existing[KEY_BUNDLE_ROLE] = bundle[KEY_BUNDLE_ROLE];
		existing[KEY_BUNDLE_RELATED] = bundle[KEY_BUNDLE_RELATED];

		Error err = set_metadata(p_paths[i], existing);
		if (err != OK) {
			return err;
		}
	}

	return OK;
}

// ── Version History ──────────────────────────────────────────────────────

static Dictionary _read_json_file(const String &p_path) {
	if (!FileAccess::exists(p_path)) {
		return Dictionary();
	}
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return Dictionary();
	}
	String content = f->get_as_text();
	f.unref();
	JSON json;
	if (json.parse(content) != OK) {
		return Dictionary();
	}
	Variant data = json.get_data();
	if (data.get_type() != Variant::DICTIONARY) {
		return Dictionary();
	}
	return data;
}

static Error _write_json_file(const String &p_path, const Dictionary &p_data) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.is_null()) {
		return ERR_CANT_CREATE;
	}
	f->store_string(JSON::stringify(p_data, "\t"));
	f.unref();
	return OK;
}

String AIAssetMetadata::get_version_dir(const String &p_asset_path) {
	return p_asset_path.get_base_dir().path_join(".ai." + p_asset_path.get_file());
}

String AIAssetMetadata::get_version_file_path(const String &p_asset_path, int p_version) {
	return get_version_dir(p_asset_path).path_join(vformat("v%d.%s", p_version, p_asset_path.get_extension()));
}

String AIAssetMetadata::get_version_meta_path(const String &p_asset_path, int p_version) {
	return get_version_dir(p_asset_path).path_join(vformat("v%d.json", p_version));
}

String AIAssetMetadata::get_version_index_path(const String &p_asset_path) {
	return get_version_dir(p_asset_path).path_join("metadata.json");
}

Dictionary AIAssetMetadata::read_version_index(const String &p_asset_path) {
	return _read_json_file(get_version_index_path(p_asset_path));
}

Error AIAssetMetadata::write_version_index(const String &p_asset_path, const Dictionary &p_index) {
	return _write_json_file(get_version_index_path(p_asset_path), p_index);
}

Dictionary AIAssetMetadata::read_version_meta(const String &p_asset_path, int p_version) {
	return _read_json_file(get_version_meta_path(p_asset_path, p_version));
}

Error AIAssetMetadata::save_version(const String &p_asset_path) {
	if (!FileAccess::exists(p_asset_path)) {
		return ERR_FILE_NOT_FOUND;
	}

	Dictionary meta = get_metadata(p_asset_path);
	int version = (int)meta.get(KEY_VERSION, 1);

	// Ensure version dir exists.
	String ver_dir = get_version_dir(p_asset_path);
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (!da->dir_exists(ver_dir)) {
		Error err = da->make_dir_recursive(ver_dir);
		if (err != OK) {
			return err;
		}
	}

	// Already saved — skip.
	String ver_file = get_version_file_path(p_asset_path, version);
	if (FileAccess::exists(ver_file)) {
		return OK;
	}

	// Copy asset binary to version file.
	Error err = da->copy(p_asset_path, ver_file);
	if (err != OK) {
		return err;
	}

	// Write per-version metadata (vN.json).
	Dictionary ver_meta;
	ver_meta[KEY_VERSION] = version;
	ver_meta[KEY_ORIGIN] = meta.get(KEY_ORIGIN, "");
	ver_meta[KEY_PROMPT] = meta.get(KEY_PROMPT, "");
	ver_meta[KEY_NEGATIVE_PROMPT] = meta.get(KEY_NEGATIVE_PROMPT, "");
	ver_meta[KEY_PROVIDER] = meta.get(KEY_PROVIDER, "");
	ver_meta[KEY_MODEL] = meta.get(KEY_MODEL, "");
	ver_meta[KEY_SEED] = meta.get(KEY_SEED, -1);
	ver_meta[KEY_PARAMETERS] = meta.get(KEY_PARAMETERS, Dictionary());
	ver_meta[KEY_GENERATED_AT] = meta.get(KEY_GENERATED_AT, get_current_timestamp());
	ver_meta[KEY_ORIGINAL_FILENAME] = meta.get(KEY_ORIGINAL_FILENAME, "");
	ver_meta[KEY_IMPORTED_FROM] = meta.get(KEY_IMPORTED_FROM, "");

	err = _write_json_file(get_version_meta_path(p_asset_path, version), ver_meta);
	if (err != OK) {
		return err;
	}

	// Update metadata.json — merge full asset metadata with version tracking fields.
	// Start from the full asset metadata so all fields are preserved.
	Dictionary index = meta.duplicate();
	index[KEY_CURRENT_VERSION] = version;

	Array history;
	if (index.has(KEY_HISTORY)) {
		history = index[KEY_HISTORY];
	}
	bool found = false;
	for (int i = 0; i < history.size(); i++) {
		if ((int)history[i] == version) {
			found = true;
			break;
		}
	}
	if (!found) {
		history.push_back(version);
	}
	index[KEY_HISTORY] = history;

	return write_version_index(p_asset_path, index);
}

Error AIAssetMetadata::use_version(const String &p_asset_path, int p_version) {
	String ver_file = get_version_file_path(p_asset_path, p_version);
	if (!FileAccess::exists(ver_file)) {
		return ERR_FILE_NOT_FOUND;
	}

	// Save current version first if not already saved.
	save_version(p_asset_path);

	// Copy selected version to main asset path.
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	Error err = da->copy(ver_file, p_asset_path);
	if (err != OK) {
		return err;
	}

	// Apply the version's prompt/model/seed to the metadata.
	Dictionary ver_meta = read_version_meta(p_asset_path, p_version);
	Dictionary asset_meta = get_metadata(p_asset_path);
	if (!asset_meta.is_empty()) {
		if (ver_meta.has(KEY_PROMPT)) {
			asset_meta[KEY_PROMPT] = ver_meta[KEY_PROMPT];
		}
		if (ver_meta.has(KEY_NEGATIVE_PROMPT)) {
			asset_meta[KEY_NEGATIVE_PROMPT] = ver_meta[KEY_NEGATIVE_PROMPT];
		}
		if (ver_meta.has(KEY_MODEL)) {
			asset_meta[KEY_MODEL] = ver_meta[KEY_MODEL];
		}
		if (ver_meta.has(KEY_SEED)) {
			asset_meta[KEY_SEED] = ver_meta[KEY_SEED];
		}
		if (ver_meta.has(KEY_PROVIDER)) {
			asset_meta[KEY_PROVIDER] = ver_meta[KEY_PROVIDER];
		}
		if (ver_meta.has(KEY_PARAMETERS)) {
			asset_meta[KEY_PARAMETERS] = ver_meta[KEY_PARAMETERS];
		}
		asset_meta[KEY_VERSION] = p_version;
		set_metadata(p_asset_path, asset_meta);
	}

	// Update version index current_version.
	Dictionary index = read_version_index(p_asset_path);
	index[KEY_CURRENT_VERSION] = p_version;
	return write_version_index(p_asset_path, index);
}

Error AIAssetMetadata::delete_version(const String &p_asset_path, int p_version) {
	int current = get_current_version(p_asset_path);
	if (p_version == current) {
		return ERR_INVALID_PARAMETER;
	}

	String ver_file = get_version_file_path(p_asset_path, p_version);
	if (FileAccess::exists(ver_file)) {
		String global_path = ProjectSettings::get_singleton()->globalize_path(ver_file);
		Error err = OS::get_singleton()->move_to_trash(global_path);
		if (err != OK) {
			Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
			da->remove(ver_file);
		}
	}

	String ver_meta_path = get_version_meta_path(p_asset_path, p_version);
	if (FileAccess::exists(ver_meta_path)) {
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		da->remove(ver_meta_path);
	}

	// Remove from version index history.
	Dictionary index = read_version_index(p_asset_path);
	if (index.has(KEY_HISTORY)) {
		Array history = index[KEY_HISTORY];
		Array new_history;
		for (int i = 0; i < history.size(); i++) {
			if ((int)history[i] != p_version) {
				new_history.push_back(history[i]);
			}
		}
		index[KEY_HISTORY] = new_history;
		return write_version_index(p_asset_path, index);
	}

	return OK;
}

Array AIAssetMetadata::list_versions(const String &p_asset_path) {
	Array result;
	Dictionary index = read_version_index(p_asset_path);

	// If no history array but metadata exists with a version, synthesize a single entry
	// from the current metadata. This handles assets generated before history tracking.
	if (!index.has(KEY_HISTORY)) {
		int ver = (int)index.get(KEY_VERSION, 0);
		if (ver > 0 && !index.is_empty()) {
			Dictionary entry;
			entry[KEY_VERSION] = ver;
			entry[KEY_ORIGIN] = index.get(KEY_ORIGIN, "");
			entry[KEY_PROMPT] = index.get(KEY_PROMPT, "");
			entry[KEY_MODEL] = index.get(KEY_MODEL, "");
			entry[KEY_NEGATIVE_PROMPT] = index.get(KEY_NEGATIVE_PROMPT, "");
			entry[KEY_PROVIDER] = index.get(KEY_PROVIDER, "");
			entry[KEY_SEED] = index.get(KEY_SEED, -1);
			entry[KEY_PARAMETERS] = index.get(KEY_PARAMETERS, Dictionary());
			entry[KEY_GENERATED_AT] = index.get(KEY_GENERATED_AT, index.get(KEY_CREATED_AT, ""));
			entry[KEY_ORIGINAL_FILENAME] = index.get(KEY_ORIGINAL_FILENAME, "");
			entry["is_current"] = true;
			entry["file_exists"] = FileAccess::exists(p_asset_path);
			entry["has_raw"] = false;
			entry["post_processing"] = Array();
			entry["raw_dimensions"] = String();
			entry["final_dimensions"] = String();
			result.push_back(entry);
		}
		return result;
	}

	Array history = index[KEY_HISTORY];
	int current = (int)index.get(KEY_CURRENT_VERSION, -1);

	for (int i = 0; i < history.size(); i++) {
		int ver = (int)history[i];
		Dictionary ver_meta = read_version_meta(p_asset_path, ver);

		Dictionary entry;
		entry[KEY_VERSION] = ver;
		entry[KEY_ORIGIN] = ver_meta.get(KEY_ORIGIN, "");
		entry[KEY_PROMPT] = ver_meta.get(KEY_PROMPT, "");
		entry[KEY_MODEL] = ver_meta.get(KEY_MODEL, "");
		entry[KEY_NEGATIVE_PROMPT] = ver_meta.get(KEY_NEGATIVE_PROMPT, "");
		entry[KEY_PROVIDER] = ver_meta.get(KEY_PROVIDER, "");
		entry[KEY_SEED] = ver_meta.get(KEY_SEED, -1);
		entry[KEY_PARAMETERS] = ver_meta.get(KEY_PARAMETERS, Dictionary());
		entry[KEY_GENERATED_AT] = ver_meta.get(KEY_GENERATED_AT, "");
		entry[KEY_ORIGINAL_FILENAME] = ver_meta.get(KEY_ORIGINAL_FILENAME, "");
		entry["is_current"] = (ver == current);
		entry["file_exists"] = FileAccess::exists(get_version_file_path(p_asset_path, ver));

		// Check for raw file (pre-post-processing output from AI model)
		String raw_path = get_version_dir(p_asset_path).path_join(vformat("v%d_raw.%s", ver, p_asset_path.get_extension()));
		entry["has_raw"] = FileAccess::exists(raw_path);

		// Pass through post-processing and dimension info
		entry["post_processing"] = ver_meta.get("post_processing", Array());
		entry["raw_dimensions"] = ver_meta.get("raw_dimensions", "");
		entry["final_dimensions"] = ver_meta.get("final_dimensions", "");

		result.push_back(entry);
	}

	return result;
}

int AIAssetMetadata::get_current_version(const String &p_asset_path) {
	Dictionary index = read_version_index(p_asset_path);
	if (index.has(KEY_CURRENT_VERSION)) {
		return (int)index[KEY_CURRENT_VERSION];
	}
	Dictionary meta = get_metadata(p_asset_path);
	return (int)meta.get(KEY_VERSION, 0);
}
