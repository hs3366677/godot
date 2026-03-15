/**************************************************************************/
/*  ai_assistant_manager.cpp                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                           MAKABAKA ENGINE                              */
/*                    AI-powered game creation module                     */
/**************************************************************************/

#include "ai_assistant_manager.h"

#include "ai_assistant_dock.h"
#include "editor/docks/editor_dock_manager.h"
#include "editor/settings/editor_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/config/project_settings.h"
#include "core/os/os.h"

AIAssistantManager *AIAssistantManager::singleton = nullptr;

AIAssistantManager::AIAssistantManager() {
	singleton = this;
}

AIAssistantManager::~AIAssistantManager() {
	// Close all extra instances (skip primary at index 0).
	for (int i = instances.size() - 1; i > 0; i--) {
		if (instances[i].dock) {
			instances[i].dock->cleanup_before_close();
			EditorDockManager::get_singleton()->remove_dock(instances[i].dock);
			memdelete(instances[i].dock);
		}
	}
	instances.clear();
	singleton = nullptr;
}

void AIAssistantManager::_bind_methods() {
}

AIAssistantDock *AIAssistantManager::create_primary_dock() {
	AIAssistantDock *dock = memnew(AIAssistantDock);
	dock->set_instance_id(0);

	InstanceInfo info;
	info.instance_id = 0;
	info.dock = dock;
	instances.push_back(info);
	next_instance_id = 1;

	EditorDockManager *dm = EditorDockManager::get_singleton();
	dm->add_dock(dock);
	// Make AI Assistant the selected tab in its dock slot.
	dm->open_dock(dock, true);
	return dock;
}

AIAssistantDock *AIAssistantManager::spawn_instance() {
	if (!can_spawn()) {
		return nullptr;
	}

	int id = next_instance_id++;

	AIAssistantDock *dock = memnew(AIAssistantDock);
	dock->set_instance_id(id);
	dock->set_create_new_session_if_none(true); // New tabs always get fresh sessions.
	dock->set_title(vformat("AI #%d", id + 1));
	dock->set_layout_key(vformat("AI #%d", id + 1));

	InstanceInfo info;
	info.instance_id = id;
	info.dock = dock;
	instances.push_back(info);

	EditorDockManager::get_singleton()->add_dock(dock);
	EditorDockManager::get_singleton()->open_dock(dock, true);

	return dock;
}

void AIAssistantManager::close_instance(int p_instance_id) {
	// Cannot close primary instance (id 0).
	if (p_instance_id == 0) {
		return;
	}

	for (int i = 0; i < instances.size(); i++) {
		if (instances[i].instance_id == p_instance_id) {
			if (instances[i].dock) {
				instances[i].dock->cleanup_before_close();
				EditorDockManager::get_singleton()->remove_dock(instances[i].dock);
				memdelete(instances[i].dock);
			}
			instances.remove_at(i);
			return;
		}
	}
}

String AIAssistantManager::build_project_context() const {
	String context;

	bool has_worldbuilding = FileAccess::exists("res://docs/worldbuilding.md");
	bool has_game_design = FileAccess::exists("res://docs/game_design.md");

	if (!has_worldbuilding) {
		// ── NO WORLDBUILDING: inform AI that worldbuilding is available ──
		context = "[PROJECT CONTEXT — NEW PROJECT]\n\n";
		context += "This project has no worldbuilding document yet.\n";
		context += "If the user wants to create a game, suggest starting with the /create-game workflow.\n";
		context += "If the user wants art direction or concept art, suggest /worldbuilding first.\n\n";
	} else {
		// ── HAS WORLDBUILDING: inject existing world context ──
		context = "[PROJECT CONTEXT]\n\n";

		// Append worldbuilding content
		Ref<FileAccess> wb = FileAccess::open("res://docs/worldbuilding.md", FileAccess::READ);
		if (wb.is_valid()) {
			context += "## Current Worldbuilding\n\n";
			context += wb->get_as_text();
			context += "\n\n";
		}

		// Append visual bible if exists
		if (FileAccess::exists("res://docs/visual_bible.md")) {
			Ref<FileAccess> vb = FileAccess::open("res://docs/visual_bible.md", FileAccess::READ);
			if (vb.is_valid()) {
				context += "## Current Visual Bible\n\n";
				context += vb->get_as_text();
				context += "\n\n";
			}
		}

		// Append game design if exists
		if (has_game_design) {
			Ref<FileAccess> gd = FileAccess::open("res://docs/game_design.md", FileAccess::READ);
			if (gd.is_valid()) {
				context += "## Current Game Design\n\n";
				context += gd->get_as_text();
				context += "\n\n";
			}
		}
	}

	return context;
}

void AIAssistantManager::save_state() {
	if (!EditorSettings::get_singleton()) {
		return;
	}

	// Save the number of extra instances and their session IDs.
	// Skip primary (id 0) — it is always recreated.
	Array saved;
	for (int i = 1; i < instances.size(); i++) {
		if (instances[i].instance_id <= 0) {
			continue;
		}
		Dictionary entry;
		entry["instance_id"] = instances[i].instance_id;
		entry["session_id"] = instances[i].dock ? instances[i].dock->get_session_id() : instances[i].session_id;
		saved.push_back(entry);
	}
	EditorSettings::get_singleton()->set_setting("ai/assistant/extra_instances", saved);

	// Also save next_instance_id so IDs don't collide after restart.
	EditorSettings::get_singleton()->set_setting("ai/assistant/next_instance_id", next_instance_id);
}

void AIAssistantManager::restore_state() {
	if (!EditorSettings::get_singleton()) {
		return;
	}

	Variant v = EditorSettings::get_singleton()->get_setting("ai/assistant/extra_instances");
	if (v.get_type() != Variant::ARRAY) {
		return;
	}

	Variant nid = EditorSettings::get_singleton()->get_setting("ai/assistant/next_instance_id");
	if (nid.get_type() == Variant::INT) {
		next_instance_id = MAX(next_instance_id, (int)nid);
	}

	Array saved = v;
	for (int i = 0; i < saved.size(); i++) {
		Dictionary entry = saved[i];
		int id = entry.get("instance_id", 0);
		String sess = entry.get("session_id", "");

		if (id <= 0) {
			continue;
		}

		// Ensure next_instance_id stays ahead.
		next_instance_id = MAX(next_instance_id, id + 1);

		AIAssistantDock *dock = memnew(AIAssistantDock);
		dock->set_instance_id(id);
		dock->set_title(vformat("AI #%d", id + 1));
		dock->set_layout_key(vformat("AI #%d", id + 1));
		if (!sess.is_empty()) {
			dock->set_initial_session_id(sess);
		}

		InstanceInfo info;
		info.instance_id = id;
		info.session_id = sess;
		info.dock = dock;
		instances.push_back(info);

		EditorDockManager::get_singleton()->add_dock(dock);
	}
}
