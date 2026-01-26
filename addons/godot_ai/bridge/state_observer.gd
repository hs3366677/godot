@tool
extends Node
## State Observer - Provides AI with awareness of current engine state.
## Captures scene tree, project state, and enables querying of engine state.


## Observe state based on query type
func observe(query: String) -> Dictionary:
	match query:
		"scene_tree":
			return get_scene_tree()
		"project_state":
			return get_project_state()
		"selected_nodes":
			return get_selected_nodes()
		"resources":
			return get_loaded_resources()
		_:
			return {"error": "Unknown query type: " + query}


## Get the current scene tree structure
func get_scene_tree() -> Dictionary:
	var edited_scene = EditorInterface.get_edited_scene_root()
	if not edited_scene:
		return {"error": "No scene currently edited", "root": null}

	return {
		"scene_path": edited_scene.scene_file_path,
		"root": _serialize_node(edited_scene)
	}


## Recursively serialize a node and its children
func _serialize_node(node: Node, depth: int = 0) -> Dictionary:
	var data = {
		"name": node.name,
		"type": node.get_class(),
		"path": str(node.get_path()),
		"properties": _get_important_properties(node),
		"children": []
	}

	# Limit depth to prevent huge outputs
	if depth < 10:
		for child in node.get_children():
			data.children.append(_serialize_node(child, depth + 1))

	return data


## Get important properties for a node
func _get_important_properties(node: Node) -> Dictionary:
	var props = {}

	# Common properties
	if node.has_method("get_script") and node.get_script():
		props["script"] = node.get_script().resource_path

	# Node2D properties
	if node is Node2D:
		props["position"] = {"x": node.position.x, "y": node.position.y}
		props["rotation"] = node.rotation
		props["scale"] = {"x": node.scale.x, "y": node.scale.y}
		props["visible"] = node.visible

	# Node3D properties
	if node is Node3D:
		props["position"] = {"x": node.position.x, "y": node.position.y, "z": node.position.z}
		props["rotation"] = {"x": node.rotation.x, "y": node.rotation.y, "z": node.rotation.z}
		props["scale"] = {"x": node.scale.x, "y": node.scale.y, "z": node.scale.z}
		props["visible"] = node.visible

	# Control properties
	if node is Control:
		props["position"] = {"x": node.position.x, "y": node.position.y}
		props["size"] = {"x": node.size.x, "y": node.size.y}
		props["visible"] = node.visible

	# Sprite properties
	if node is Sprite2D:
		if node.texture:
			props["texture"] = node.texture.resource_path

	# CharacterBody properties
	if node is CharacterBody2D or node is CharacterBody3D:
		props["velocity"] = _vector_to_dict(node.velocity)

	return props


func _vector_to_dict(v) -> Dictionary:
	if v is Vector2:
		return {"x": v.x, "y": v.y}
	elif v is Vector3:
		return {"x": v.x, "y": v.y, "z": v.z}
	return {}


## Get project state summary
func get_project_state() -> Dictionary:
	var fs = EditorInterface.get_resource_filesystem()

	return {
		"project_name": ProjectSettings.get_setting("application/config/name", "Unnamed"),
		"main_scene": ProjectSettings.get_setting("application/run/main_scene", ""),
		"current_scene": _get_current_scene_path(),
		"scenes": _get_project_scenes(fs.get_filesystem()),
		"scripts": _get_project_scripts(fs.get_filesystem()),
		"input_actions": _get_input_actions()
	}


func _get_current_scene_path() -> String:
	var edited = EditorInterface.get_edited_scene_root()
	if edited:
		return edited.scene_file_path
	return ""


func _get_project_scenes(dir: EditorFileSystemDirectory, scenes: Array = []) -> Array:
	for i in dir.get_file_count():
		var path = dir.get_file_path(i)
		if path.ends_with(".tscn") or path.ends_with(".scn"):
			scenes.append(path)

	for i in dir.get_subdir_count():
		_get_project_scenes(dir.get_subdir(i), scenes)

	return scenes


func _get_project_scripts(dir: EditorFileSystemDirectory, scripts: Array = []) -> Array:
	for i in dir.get_file_count():
		var path = dir.get_file_path(i)
		if path.ends_with(".gd") or path.ends_with(".cs"):
			scripts.append(path)

	for i in dir.get_subdir_count():
		_get_project_scripts(dir.get_subdir(i), scripts)

	return scripts


func _get_input_actions() -> Dictionary:
	var actions = {}
	for action in InputMap.get_actions():
		if not action.begins_with("ui_"):  # Skip built-in UI actions
			var events = []
			for event in InputMap.action_get_events(action):
				events.append(event.as_text())
			actions[action] = events
	return actions


## Get currently selected nodes
func get_selected_nodes() -> Dictionary:
	var selection = EditorInterface.get_selection()
	var selected = selection.get_selected_nodes()

	var nodes = []
	for node in selected:
		nodes.append(_serialize_node(node))

	return {"selected": nodes, "count": nodes.size()}


## Get loaded resources in the project
func get_loaded_resources() -> Dictionary:
	var fs = EditorInterface.get_resource_filesystem()
	var resources = {
		"textures": [],
		"audio": [],
		"fonts": [],
		"materials": [],
		"other": []
	}

	_categorize_resources(fs.get_filesystem(), resources)
	return resources


func _categorize_resources(dir: EditorFileSystemDirectory, resources: Dictionary) -> void:
	for i in dir.get_file_count():
		var path = dir.get_file_path(i)
		var ext = path.get_extension().to_lower()

		match ext:
			"png", "jpg", "jpeg", "webp", "svg":
				resources.textures.append(path)
			"wav", "ogg", "mp3":
				resources.audio.append(path)
			"ttf", "otf", "woff", "woff2":
				resources.fonts.append(path)
			"material", "tres" when "material" in path.to_lower():
				resources.materials.append(path)
			"tres", "res":
				resources.other.append(path)

	for i in dir.get_subdir_count():
		_categorize_resources(dir.get_subdir(i), resources)


## Take a screenshot of the current editor view
func capture_screenshot() -> Dictionary:
	# This would require access to viewport, typically done through editor
	return {"error": "Screenshot capture not yet implemented"}


## Get module interfaces from the modules folder
func get_available_modules() -> Array:
	var modules = []
	var dir = DirAccess.open("res://modules")
	if dir:
		dir.list_dir_begin()
		var category = dir.get_next()
		while category != "":
			if dir.current_is_dir() and not category.begins_with("."):
				var category_path = "res://modules/" + category
				var category_dir = DirAccess.open(category_path)
				if category_dir:
					category_dir.list_dir_begin()
					var module = category_dir.get_next()
					while module != "":
						if category_dir.current_is_dir() and not module.begins_with("."):
							var interface_path = category_path + "/" + module + "/interface.json"
							if FileAccess.file_exists(interface_path):
								var file = FileAccess.open(interface_path, FileAccess.READ)
								var json = JSON.new()
								if json.parse(file.get_as_text()) == OK:
									modules.append(json.data)
								file.close()
						module = category_dir.get_next()
			category = dir.get_next()
	return modules


## Get available templates
func get_available_templates() -> Array:
	var templates = []
	var dir = DirAccess.open("res://templates")
	if dir:
		dir.list_dir_begin()
		var template = dir.get_next()
		while template != "":
			if dir.current_is_dir() and not template.begins_with("."):
				var manifest_path = "res://templates/" + template + "/template.json"
				if FileAccess.file_exists(manifest_path):
					var file = FileAccess.open(manifest_path, FileAccess.READ)
					var json = JSON.new()
					if json.parse(file.get_as_text()) == OK:
						templates.append(json.data)
					file.close()
			template = dir.get_next()
	return templates
