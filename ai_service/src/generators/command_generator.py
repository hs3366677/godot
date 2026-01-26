"""Command Generator - Generates Godot commands from analyzed prompts."""

import logging
from typing import Any

logger = logging.getLogger('GodotAI.CommandGenerator')


class CommandGenerator:
    """Generates Godot bridge commands."""

    def __init__(self):
        """Initialize command generator."""
        pass

    def create_node(
        self,
        node_type: str,
        name: str,
        parent: str = "/root",
        properties: dict = None
    ) -> dict:
        """
        Generate a create node command.

        Args:
            node_type: Godot node class name
            name: Name for the new node
            parent: Parent node path
            properties: Initial properties to set

        Returns:
            Command dictionary
        """
        cmd = {
            "action": "scene.create_node",
            "params": {
                "type": node_type,
                "name": name,
                "parent": parent
            }
        }

        if properties:
            cmd["params"]["properties"] = properties

        # Add rollback command
        cmd["rollback"] = {
            "action": "scene.delete_node",
            "params": {"path": f"{parent}/{name}"}
        }

        return cmd

    def delete_node(self, path: str) -> dict:
        """
        Generate a delete node command.

        Args:
            path: Path to the node to delete

        Returns:
            Command dictionary
        """
        return {
            "action": "scene.delete_node",
            "params": {"path": path}
        }

    def set_property(
        self,
        node_path: str,
        property_name: str,
        value: Any
    ) -> dict:
        """
        Generate a set property command.

        Args:
            node_path: Path to the node
            property_name: Name of the property
            value: Value to set

        Returns:
            Command dictionary
        """
        return {
            "action": "scene.set_property",
            "params": {
                "path": node_path,
                "property": property_name,
                "value": value
            }
        }

    def create_script(
        self,
        path: str,
        content: str,
        base_type: str = "Node"
    ) -> dict:
        """
        Generate a create script command.

        Args:
            path: Path for the new script
            content: Script content
            base_type: Base class for the script

        Returns:
            Command dictionary
        """
        return {
            "action": "script.create",
            "params": {
                "path": path,
                "content": content,
                "base_type": base_type
            }
        }

    def attach_script(
        self,
        node_path: str,
        script_path: str
    ) -> dict:
        """
        Generate an attach script command.

        Args:
            node_path: Path to the node
            script_path: Path to the script

        Returns:
            Command dictionary
        """
        return {
            "action": "script.attach",
            "params": {
                "node_path": node_path,
                "script_path": script_path
            }
        }

    def save_scene(self, path: str = "") -> dict:
        """
        Generate a save scene command.

        Args:
            path: Path to save to (empty for current scene)

        Returns:
            Command dictionary
        """
        return {
            "action": "editor.save_scene",
            "params": {"path": path} if path else {}
        }

    def run_project(self, scene: str = "") -> dict:
        """
        Generate a run project command.

        Args:
            scene: Specific scene to run (empty for main scene)

        Returns:
            Command dictionary
        """
        return {
            "action": "project.run",
            "params": {"scene": scene} if scene else {}
        }

    def stop_project(self) -> dict:
        """Generate a stop project command."""
        return {
            "action": "project.stop",
            "params": {}
        }

    def open_scene(self, path: str) -> dict:
        """
        Generate an open scene command.

        Args:
            path: Path to the scene to open

        Returns:
            Command dictionary
        """
        return {
            "action": "editor.open_scene",
            "params": {"path": path}
        }

    def select_node(self, path: str) -> dict:
        """
        Generate a select node command.

        Args:
            path: Path to the node to select

        Returns:
            Command dictionary
        """
        return {
            "action": "editor.select",
            "params": {"path": path}
        }

    def create_resource(
        self,
        resource_type: str,
        path: str,
        properties: dict = None
    ) -> dict:
        """
        Generate a create resource command.

        Args:
            resource_type: Type of resource to create
            path: Path to save the resource
            properties: Initial properties

        Returns:
            Command dictionary
        """
        cmd = {
            "action": "resource.create",
            "params": {
                "type": resource_type,
                "path": path
            }
        }

        if properties:
            cmd["params"]["properties"] = properties

        return cmd

    def batch_commands(self, commands: list) -> list:
        """
        Create a batch of commands with transaction support.

        Args:
            commands: List of command dictionaries

        Returns:
            List with rollback information added
        """
        # Add indices for rollback ordering
        for i, cmd in enumerate(commands):
            cmd["_order"] = i

        return commands

    def create_player_setup(
        self,
        name: str = "Player",
        node_type: str = "CharacterBody2D",
        with_sprite: bool = True,
        with_collision: bool = True,
        with_script: bool = True
    ) -> list:
        """
        Generate commands to create a complete player setup.

        Args:
            name: Player node name
            node_type: Base node type
            with_sprite: Include sprite node
            with_collision: Include collision shape
            with_script: Include movement script

        Returns:
            List of commands
        """
        commands = []

        # Create main player node
        commands.append(self.create_node(node_type, name))
        player_path = f"/root/{name}"

        # Add sprite
        if with_sprite:
            sprite_type = "Sprite2D" if "2D" in node_type else "Sprite3D"
            commands.append(self.create_node(
                sprite_type,
                "Sprite",
                player_path
            ))

        # Add collision
        if with_collision:
            collision_type = "CollisionShape2D" if "2D" in node_type else "CollisionShape3D"
            commands.append(self.create_node(
                collision_type,
                "CollisionShape",
                player_path
            ))

        # Add script
        if with_script:
            script_content = self._generate_player_script(node_type)
            script_path = f"res://scripts/{name.lower()}.gd"
            commands.append(self.create_script(script_path, script_content, node_type))
            commands.append(self.attach_script(player_path, script_path))

        return commands

    def _generate_player_script(self, base_type: str) -> str:
        """Generate a basic player script."""
        if "2D" in base_type:
            return '''extends CharacterBody2D

@export var speed: float = 300.0
@export var jump_force: float = -400.0
@export var gravity: float = 980.0


func _physics_process(delta: float) -> void:
	# Gravity
	if not is_on_floor():
		velocity.y += gravity * delta

	# Jump
	if Input.is_action_just_pressed("jump") and is_on_floor():
		velocity.y = jump_force

	# Movement
	var direction = Input.get_axis("move_left", "move_right")
	velocity.x = direction * speed

	move_and_slide()
'''
        else:
            return '''extends CharacterBody3D

@export var speed: float = 5.0
@export var jump_force: float = 4.5
@export var gravity: float = 9.8


func _physics_process(delta: float) -> void:
	# Gravity
	if not is_on_floor():
		velocity.y -= gravity * delta

	# Jump
	if Input.is_action_just_pressed("jump") and is_on_floor():
		velocity.y = jump_force

	# Movement
	var input_dir = Input.get_vector("move_left", "move_right", "move_forward", "move_back")
	var direction = (transform.basis * Vector3(input_dir.x, 0, input_dir.y)).normalized()

	if direction:
		velocity.x = direction.x * speed
		velocity.z = direction.z * speed
	else:
		velocity.x = move_toward(velocity.x, 0, speed)
		velocity.z = move_toward(velocity.z, 0, speed)

	move_and_slide()
'''

    def create_enemy_setup(
        self,
        name: str = "Enemy",
        behavior: str = "patrol",
        node_type: str = "CharacterBody2D"
    ) -> list:
        """
        Generate commands to create an enemy setup.

        Args:
            name: Enemy node name
            behavior: Enemy behavior type
            node_type: Base node type

        Returns:
            List of commands
        """
        commands = []

        # Create main enemy node
        commands.append(self.create_node(node_type, name))
        enemy_path = f"/root/{name}"

        # Add sprite
        commands.append(self.create_node("Sprite2D", "Sprite", enemy_path))

        # Add collision
        commands.append(self.create_node("CollisionShape2D", "CollisionShape", enemy_path))

        # Add detection area for chase behavior
        if behavior == "chase":
            commands.append(self.create_node("Area2D", "DetectionArea", enemy_path))
            commands.append(self.create_node(
                "CollisionShape2D",
                "DetectionShape",
                f"{enemy_path}/DetectionArea"
            ))

        return commands
