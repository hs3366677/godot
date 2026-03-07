/**************************************************************************/
/*  ai_asset_metadata.h                                                   */
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

#include "core/object/object.h"
#include "core/string/ustring.h"
#include "core/variant/binder_common.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class AIAssetMetadata : public Object {
	GDCLASS(AIAssetMetadata, Object);

public:
	// Asset origin tracking - how the asset entered the project
	enum Origin {
		ORIGIN_UNKNOWN,     // No metadata available
		ORIGIN_PLACEHOLDER, // AI-created placeholder with pre-filled generation metadata
		ORIGIN_IMPORTED,    // User-provided file (drag-drop, file dialog)
		ORIGIN_GENERATED,   // AI-generated from prompt
		ORIGIN_HYBRID,      // User file + AI transform (upscale, style transfer, etc.)
	};

	// Metadata key constants
	static const char *KEY_ORIGIN;
	static const char *KEY_IMPORTED_FROM;
	static const char *KEY_IMPORTED_AT;
	static const char *KEY_ORIGINAL_FILENAME;
	static const char *KEY_ORIGINAL_SIZE_BYTES;
	static const char *KEY_PROMPT;
	static const char *KEY_NEGATIVE_PROMPT;
	static const char *KEY_PROVIDER;
	static const char *KEY_MODEL;
	static const char *KEY_SEED;
	static const char *KEY_PARAMETERS;
	static const char *KEY_GENERATED_AT;
	static const char *KEY_VERSION;
	static const char *KEY_BUNDLE_ID;
	static const char *KEY_BUNDLE_ROLE;
	static const char *KEY_BUNDLE_RELATED;
	static const char *KEY_SOURCE_ASSET;
	static const char *KEY_TRANSFORM;
	static const char *KEY_ASSET_TYPE;
	static const char *KEY_USAGE;
	static const char *KEY_CREATED_AT;
	static const char *KEY_CREATED_BY;
	static const char *KEY_HISTORY;
	static const char *KEY_CURRENT_VERSION;

	// Metadata creation helpers
	static Dictionary create_import_metadata(
			const String &p_source_path,
			const String &p_original_filename,
			int64_t p_size_bytes = 0);

	static Dictionary create_placeholder_metadata(
			const String &p_asset_type,
			const String &p_prompt,
			const String &p_provider,
			const String &p_model,
			const Dictionary &p_parameters = Dictionary(),
			const Dictionary &p_usage = Dictionary());

	static Dictionary create_generation_metadata(
			const String &p_prompt,
			const String &p_provider,
			const String &p_model,
			int p_seed = -1,
			const Dictionary &p_parameters = Dictionary());

	static Dictionary create_hybrid_metadata(
			const String &p_source_asset,
			const String &p_transform_type,
			const String &p_prompt,
			const String &p_provider,
			const String &p_model);

	static Dictionary create_bundle_metadata(
			const String &p_bundle_id,
			const String &p_role,
			const Vector<String> &p_related_paths);

	// Metadata read/write operations
	static Origin get_origin(const String &p_path);
	static String origin_to_string(Origin p_origin);
	static Origin string_to_origin(const String &p_str);
	static bool is_ai_generated(const String &p_path);
	static bool is_user_imported(const String &p_path);
	static bool is_placeholder(const String &p_path);
	static Dictionary get_metadata(const String &p_path);
	static Error set_metadata(const String &p_path, const Dictionary &p_metadata);
	static Error update_metadata(const String &p_path, const Dictionary &p_updates);

	static Error append_version_history(const String &p_path, const Dictionary &p_version_entry);

	// Bundle operations
	static Vector<String> get_bundle_members(const String &p_path);
	static Error link_bundle_assets(const Vector<String> &p_paths, const String &p_bundle_id);

	// Version history (.ai.{filename}/ folder)
	static String get_version_dir(const String &p_asset_path);
	static String get_version_file_path(const String &p_asset_path, int p_version);
	static String get_version_meta_path(const String &p_asset_path, int p_version);
	static String get_version_index_path(const String &p_asset_path);
	static Dictionary read_version_index(const String &p_asset_path);
	static Error write_version_index(const String &p_asset_path, const Dictionary &p_index);
	static Dictionary read_version_meta(const String &p_asset_path, int p_version);
	static Error save_version(const String &p_asset_path);
	static Error use_version(const String &p_asset_path, int p_version);
	static Error delete_version(const String &p_asset_path, int p_version);
	static Array list_versions(const String &p_asset_path);
	static int get_current_version(const String &p_asset_path);

	// Utility
	static String get_current_timestamp();

protected:
	static void _bind_methods();
};

VARIANT_ENUM_CAST(AIAssetMetadata::Origin);
