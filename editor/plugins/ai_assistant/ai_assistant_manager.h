/**************************************************************************/
/*  ai_assistant_manager.h                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                           MAKABAKA ENGINE                              */
/*                    AI-powered game creation module                     */
/**************************************************************************/

#pragma once

#include "core/object/object.h"
#include "core/templates/vector.h"

class AIAssistantDock;
class EditorDockManager;

class AIAssistantManager : public Object {
	GDCLASS(AIAssistantManager, Object);

	static AIAssistantManager *singleton;

	struct InstanceInfo {
		int instance_id = 0;
		String session_id;
		AIAssistantDock *dock = nullptr;
	};

	Vector<InstanceInfo> instances;
	int next_instance_id = 0;

	static const int MAX_INSTANCES = 5;

protected:
	static void _bind_methods();

public:
	static AIAssistantManager *get_singleton() { return singleton; }

	AIAssistantDock *create_primary_dock();
	AIAssistantDock *spawn_instance();
	void close_instance(int p_instance_id);

	void save_state();
	void restore_state();

	String build_project_context() const;

	int get_instance_count() const { return instances.size(); }
	bool can_spawn() const { return instances.size() < MAX_INSTANCES; }

	AIAssistantManager();
	~AIAssistantManager();
};
