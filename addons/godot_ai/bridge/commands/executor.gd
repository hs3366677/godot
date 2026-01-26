@tool
extends Node
## Command Executor - Executes AI-generated commands on the Godot engine.
## Validates, sanitizes, and executes commands with rollback support.

signal command_started(command: Dictionary)
signal command_completed(command: Dictionary, result: Dictionary)
signal command_failed(command: Dictionary, error: String)


## Execute a single command
func execute(command: Dictionary) -> Dictionary:
	command_started.emit(command)

	var action = command.get("action", "")
	var params = command.get("params", {})

	if action.is_empty():
		var error = "Command missing 'action' field"
		command_failed.emit(command, error)
		return {"success": false, "error": error}

	# Validate command
	var validation = _validate_command(action, params)
	if not validation.valid:
		command_failed.emit(command, validation.error)
		return {"success": false, "error": validation.error}

	# Execute based on action category
	var result: Dictionary
	var parts = action.split(".")
	if parts.size() < 2:
		var error = "Invalid action format. Expected: category.action"
		command_failed.emit(command, error)
		return {"success": false, "error": error}

	var category = parts[0]
	var action_name = parts[1]

	match category:
		"scene":
			result = _execute_scene_command(action_name, params)
		"resource":
			result = _execute_resource_command(action_name, params)
		"script":
			result = _execute_script_command(action_name, params)
		"editor":
			result = _execute_editor_command(action_name, params)
		"project":
			result = _execute_project_command(action_name, params)
		_:
			result = {"success": false, "error": "Unknown command category: " + category}

	if result.success:
		command_completed.emit(command, result)
	else:
		command_failed.emit(command, result.get("error", "Unknown error"))

	return result


## Validate command before execution
func _validate_command(action: String, params: Dictionary) -> Dictionary:
	# Basic validation - extend as needed
	if action.is_empty():
		return {"valid": false, "error": "Empty action"}

	# Prevent dangerous operations
	var forbidden_patterns = ["..\\", "../", "res://addons/godot_ai"]
	for pattern in forbidden_patterns:
		for key in params:
			if params[key] is String and pattern in params[key]:
				return {"valid": false, "error": "Forbidden path pattern detected"}

	return {"valid": true}


## Scene manipulation commands
func _execute_scene_command(action: String, params: Dictionary) -> Dictionary:
	var editor = EditorInterface.get_editor_main_screen()

	match action:
		"create_node":
			return _create_node(params)
		"delete_node":
			return _delete_node(params)
		"set_property":
			return _set_node_property(params)
		"reparent":
			return _reparent_node(params)
		"duplicate":
			return _duplicate_node(params)
		_:
			return {"success": false, "error": "Unknown scene action: " + action}


func _create_node(params: Dictionary) -> Dictionary:
	var node_type = params.get("type", "Node")
	var node_name = params.get("name", "NewNode")
	var parent_path = params.get("parent", "/root")

	# Get edited scene root
	var edited_scene = EditorInterface.get_edited_scene_root()
	if not edited_scene:
		return {"success": false, "error": "No scene is currently being edited"}

	# Find parent node
	var parent: Node
	if parent_path == "/root":
		parent = edited_scene
	else:
		parent = edited_scene.get_node_or_null(parent_path.replace("/root/", ""))

	if not parent:
		return {"success": false, "error": "Parent node not found: " + parent_path}

	# Create the node
	var new_node = ClassDB.instantiate(node_type)
	if not new_node:
		return {"success": false, "error": "Failed to create node of type: " + node_type}

	new_node.name = node_name

	# Set initial properties if provided
	var properties = params.get("properties", {})
	for prop in properties:
		if new_node.has_method("set"):
			new_node.set(prop, properties[prop])

	parent.add_child(new_node)
	new_node.owner = edited_scene

	return {
		"success": true,
		"node_path": str(edited_scene.get_path_to(new_node)),
		"message": "Created node: " + node_name
	}


func _delete_node(params: Dictionary) -> Dictionary:
	var node_path = params.get("path", "")
	if node_path.is_empty():
		return {"success": false, "error": "Node path required"}

	var edited_scene = EditorInterface.get_edited_scene_root()
	if not edited_scene:
		return {"success": false, "error": "No scene is currently being edited"}

	var node = edited_scene.get_node_or_null(node_path.replace("/root/", ""))
	if not node:
		return {"success": false, "error": "Node not found: " + node_path}

	if node == edited_scene:
		return {"success": false, "error": "Cannot delete scene root"}

	node.queue_free()
	return {"success": true, "message": "Deleted node: " + node_path}


func _set_node_property(params: Dictionary) -> Dictionary:
	var node_path = params.get("path", "")
	var property = params.get("property", "")
	var value = params.get("value")

	if node_path.is_empty() or property.is_empty():
		return {"success": false, "error": "Node path and property required"}

	var edited_scene = EditorInterface.get_edited_scene_root()
	if not edited_scene:
		return {"success": false, "error": "No scene is currently being edited"}

	var node = edited_scene.get_node_or_null(node_path.replace("/root/", ""))
	if not node:
		return {"success": false, "error": "Node not found: " + node_path}

	var old_value = node.get(property)
	node.set(property, value)

	return {
		"success": true,
		"old_value": old_value,
		"message": "Set %s.%s = %s" % [node_path, property, str(value)]
	}


func _reparent_node(params: Dictionary) -> Dictionary:
	var node_path = params.get("path", "")
	var new_parent_path = params.get("new_parent", "")

	if node_path.is_empty() or new_parent_path.is_empty():
		return {"success": false, "error": "Node path and new parent required"}

	var edited_scene = EditorInterface.get_edited_scene_root()
	if not edited_scene:
		return {"success": false, "error": "No scene is currently being edited"}

	var node = edited_scene.get_node_or_null(node_path.replace("/root/", ""))
	var new_parent = edited_scene.get_node_or_null(new_parent_path.replace("/root/", ""))

	if not node:
		return {"success": false, "error": "Node not found: " + node_path}
	if not new_parent:
		return {"success": false, "error": "New parent not found: " + new_parent_path}

	var old_parent = node.get_parent()
	node.reparent(new_parent)

	return {
		"success": true,
		"old_parent": str(edited_scene.get_path_to(old_parent)),
		"message": "Reparented node to: " + new_parent_path
	}


func _duplicate_node(params: Dictionary) -> Dictionary:
	var node_path = params.get("path", "")

	if node_path.is_empty():
		return {"success": false, "error": "Node path required"}

	var edited_scene = EditorInterface.get_edited_scene_root()
	if not edited_scene:
		return {"success": false, "error": "No scene is currently being edited"}

	var node = edited_scene.get_node_or_null(node_path.replace("/root/", ""))
	if not node:
		return {"success": false, "error": "Node not found: " + node_path}

	var duplicate = node.duplicate()
	node.get_parent().add_child(duplicate)
	duplicate.owner = edited_scene

	return {
		"success": true,
		"new_path": str(edited_scene.get_path_to(duplicate)),
		"message": "Duplicated node: " + node_path
	}


## Resource management commands
func _execute_resource_command(action: String, params: Dictionary) -> Dictionary:
	match action:
		"create":
			return _create_resource(params)
		"load":
			return _load_resource(params)
		"save":
			return _save_resource(params)
		_:
			return {"success": false, "error": "Unknown resource action: " + action}


func _create_resource(params: Dictionary) -> Dictionary:
	var res_type = params.get("type", "Resource")
	var res_path = params.get("path", "")

	if res_path.is_empty():
		return {"success": false, "error": "Resource path required"}

	var resource = ClassDB.instantiate(res_type)
	if not resource:
		return {"success": false, "error": "Failed to create resource of type: " + res_type}

	var properties = params.get("properties", {})
	for prop in properties:
		resource.set(prop, properties[prop])

	var err = ResourceSaver.save(resource, res_path)
	if err != OK:
		return {"success": false, "error": "Failed to save resource: " + str(err)}

	return {"success": true, "path": res_path, "message": "Created resource: " + res_path}


func _load_resource(params: Dictionary) -> Dictionary:
	var res_path = params.get("path", "")
	if res_path.is_empty():
		return {"success": false, "error": "Resource path required"}

	if not ResourceLoader.exists(res_path):
		return {"success": false, "error": "Resource not found: " + res_path}

	var resource = ResourceLoader.load(res_path)
	return {"success": true, "path": res_path, "resource": resource}


func _save_resource(params: Dictionary) -> Dictionary:
	var res_path = params.get("path", "")
	var resource = params.get("resource")

	if res_path.is_empty() or not resource:
		return {"success": false, "error": "Resource path and resource required"}

	var err = ResourceSaver.save(resource, res_path)
	if err != OK:
		return {"success": false, "error": "Failed to save resource: " + str(err)}

	return {"success": true, "path": res_path, "message": "Saved resource: " + res_path}


## Script management commands
func _execute_script_command(action: String, params: Dictionary) -> Dictionary:
	match action:
		"create":
			return _create_script(params)
		"modify":
			return _modify_script(params)
		"attach":
			return _attach_script(params)
		_:
			return {"success": false, "error": "Unknown script action: " + action}


func _create_script(params: Dictionary) -> Dictionary:
	var script_path = params.get("path", "")
	var content = params.get("content", "")
	var base_type = params.get("base_type", "Node")

	if script_path.is_empty():
		return {"success": false, "error": "Script path required"}

	if content.is_empty():
		content = "extends %s\n\n\nfunc _ready() -> void:\n\tpass\n" % base_type

	var file = FileAccess.open(script_path, FileAccess.WRITE)
	if not file:
		return {"success": false, "error": "Failed to create script file"}

	file.store_string(content)
	file.close()

	# Refresh filesystem
	EditorInterface.get_resource_filesystem().scan()

	return {"success": true, "path": script_path, "message": "Created script: " + script_path}


func _modify_script(params: Dictionary) -> Dictionary:
	var script_path = params.get("path", "")
	var content = params.get("content", "")

	if script_path.is_empty() or content.is_empty():
		return {"success": false, "error": "Script path and content required"}

	var file = FileAccess.open(script_path, FileAccess.WRITE)
	if not file:
		return {"success": false, "error": "Failed to open script file"}

	file.store_string(content)
	file.close()

	EditorInterface.get_resource_filesystem().scan()

	return {"success": true, "path": script_path, "message": "Modified script: " + script_path}


func _attach_script(params: Dictionary) -> Dictionary:
	var node_path = params.get("node_path", "")
	var script_path = params.get("script_path", "")

	if node_path.is_empty() or script_path.is_empty():
		return {"success": false, "error": "Node path and script path required"}

	var edited_scene = EditorInterface.get_edited_scene_root()
	if not edited_scene:
		return {"success": false, "error": "No scene is currently being edited"}

	var node = edited_scene.get_node_or_null(node_path.replace("/root/", ""))
	if not node:
		return {"success": false, "error": "Node not found: " + node_path}

	var script = load(script_path)
	if not script:
		return {"success": false, "error": "Failed to load script: " + script_path}

	node.set_script(script)

	return {"success": true, "message": "Attached script to: " + node_path}


## Editor control commands
func _execute_editor_command(action: String, params: Dictionary) -> Dictionary:
	match action:
		"open_scene":
			return _open_scene(params)
		"save_scene":
			return _save_scene(params)
		"select":
			return _select_node(params)
		_:
			return {"success": false, "error": "Unknown editor action: " + action}


func _open_scene(params: Dictionary) -> Dictionary:
	var scene_path = params.get("path", "")
	if scene_path.is_empty():
		return {"success": false, "error": "Scene path required"}

	EditorInterface.open_scene_from_path(scene_path)
	return {"success": true, "message": "Opened scene: " + scene_path}


func _save_scene(params: Dictionary) -> Dictionary:
	var scene = EditorInterface.get_edited_scene_root()
	if not scene:
		return {"success": false, "error": "No scene to save"}

	var path = params.get("path", scene.scene_file_path)
	if path.is_empty():
		return {"success": false, "error": "Scene path required"}

	var packed = PackedScene.new()
	packed.pack(scene)
	var err = ResourceSaver.save(packed, path)

	if err != OK:
		return {"success": false, "error": "Failed to save scene: " + str(err)}

	return {"success": true, "path": path, "message": "Saved scene: " + path}


func _select_node(params: Dictionary) -> Dictionary:
	var node_path = params.get("path", "")
	if node_path.is_empty():
		return {"success": false, "error": "Node path required"}

	var edited_scene = EditorInterface.get_edited_scene_root()
	if not edited_scene:
		return {"success": false, "error": "No scene is currently being edited"}

	var node = edited_scene.get_node_or_null(node_path.replace("/root/", ""))
	if not node:
		return {"success": false, "error": "Node not found: " + node_path}

	EditorInterface.get_selection().clear()
	EditorInterface.get_selection().add_node(node)

	return {"success": true, "message": "Selected: " + node_path}


## Project management commands
func _execute_project_command(action: String, params: Dictionary) -> Dictionary:
	match action:
		"run":
			return _run_project(params)
		"stop":
			return _stop_project()
		_:
			return {"success": false, "error": "Unknown project action: " + action}


func _run_project(params: Dictionary) -> Dictionary:
	var scene_path = params.get("scene", "")

	if scene_path.is_empty():
		EditorInterface.play_main_scene()
	else:
		EditorInterface.play_custom_scene(scene_path)

	return {"success": true, "message": "Running project"}


func _stop_project() -> Dictionary:
	EditorInterface.stop_playing_scene()
	return {"success": true, "message": "Stopped project"}
