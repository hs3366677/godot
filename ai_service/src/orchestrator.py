"""
AI Orchestrator - Coordinates AI processing for game creation.
Analyzes prompts, plans tasks, and generates Godot commands.
"""

import json
import logging
from typing import Any, Optional

from analyzers.prompt_analyzer import PromptAnalyzer
from generators.code_generator import CodeGenerator
from generators.command_generator import CommandGenerator
from context.context_manager import ContextManager

logger = logging.getLogger('GodotAI.Orchestrator')


class AIOrchestrator:
    """Orchestrates AI processing for GodotAI."""

    def __init__(self, provider=None):
        """
        Initialize orchestrator.

        Args:
            provider: LLM provider instance (or None for offline mode)
        """
        self.provider = provider
        self.analyzer = PromptAnalyzer(provider)
        self.code_generator = CodeGenerator(provider)
        self.command_generator = CommandGenerator()
        self.context_manager = ContextManager()

        # System prompt for the AI
        self.system_prompt = self._build_system_prompt()

    def _build_system_prompt(self) -> str:
        """Build the system prompt for the AI."""
        return """You are GodotAI, an AI assistant specialized in creating games using the Godot Engine.

Your capabilities:
1. Create and modify Godot scenes and nodes
2. Generate GDScript code
3. Design game mechanics and systems
4. Work with the modular game architecture

When responding to requests, you should:
1. Analyze what the user wants to create or modify
2. Break down the task into specific Godot operations
3. Generate the necessary commands in JSON format

Available command categories:
- scene.create_node: Create a new node in the scene
- scene.delete_node: Delete a node
- scene.set_property: Set a node property
- script.create: Create a new script file
- script.modify: Modify an existing script
- script.attach: Attach a script to a node
- resource.create: Create a resource
- editor.open_scene: Open a scene file
- editor.save_scene: Save the current scene
- project.run: Run the project

Always respond with valid JSON containing either:
1. A "commands" array with Godot commands to execute
2. A "message" string for informational responses
3. Both, when you want to explain what you're doing

Example response format:
{
    "message": "I'll create a player character with movement.",
    "commands": [
        {
            "action": "scene.create_node",
            "params": {
                "type": "CharacterBody2D",
                "name": "Player",
                "parent": "/root"
            }
        }
    ]
}
"""

    async def process_prompt(
        self,
        prompt: str,
        context: dict = None,
        project_state: dict = None
    ) -> dict:
        """
        Process a natural language prompt and generate Godot commands.

        Args:
            prompt: User's natural language request
            context: Conversation context
            project_state: Current state of the Godot project

        Returns:
            dict with 'commands' and/or 'message'
        """
        context = context or {}
        project_state = project_state or {}

        # Update context manager
        self.context_manager.update_project_state(project_state)

        # If no provider, use offline mode
        if not self.provider:
            return await self._process_offline(prompt, project_state)

        try:
            # Analyze the prompt
            analysis = await self.analyzer.analyze(prompt, project_state)
            logger.info(f"Prompt analysis: {analysis}")

            # Build the full prompt with context
            full_prompt = self._build_full_prompt(prompt, analysis, project_state)

            # Get AI response
            response = await self.provider.complete(
                system_prompt=self.system_prompt,
                user_prompt=full_prompt
            )

            # Parse the response
            return self._parse_ai_response(response)

        except Exception as e:
            logger.error(f"Error in process_prompt: {e}")
            return {
                "message": f"I encountered an error: {str(e)}. Please try again.",
                "commands": []
            }

    def _build_full_prompt(
        self,
        prompt: str,
        analysis: dict,
        project_state: dict
    ) -> str:
        """Build the full prompt with context."""
        parts = [f"User request: {prompt}"]

        # Add analysis context
        if analysis.get("intent"):
            parts.append(f"\nDetected intent: {analysis['intent']}")

        if analysis.get("entities"):
            parts.append(f"Entities: {json.dumps(analysis['entities'])}")

        # Add relevant project state
        if project_state.get("current_scene"):
            parts.append(f"\nCurrent scene: {project_state['current_scene']}")

        if project_state.get("scenes"):
            parts.append(f"Available scenes: {', '.join(project_state['scenes'][:10])}")

        # Add module context if relevant
        relevant_modules = self.context_manager.get_relevant_modules(prompt)
        if relevant_modules:
            parts.append(f"\nRelevant modules available: {', '.join(relevant_modules)}")

        parts.append("\nGenerate the appropriate Godot commands to fulfill this request.")

        return "\n".join(parts)

    def _parse_ai_response(self, response: str) -> dict:
        """Parse AI response into commands and message."""
        try:
            # Try to parse as JSON
            # First, try to find JSON in the response
            json_start = response.find('{')
            json_end = response.rfind('}') + 1

            if json_start >= 0 and json_end > json_start:
                json_str = response[json_start:json_end]
                data = json.loads(json_str)

                return {
                    "message": data.get("message", ""),
                    "commands": data.get("commands", [])
                }

        except json.JSONDecodeError:
            pass

        # If not JSON, treat as a message
        return {
            "message": response,
            "commands": []
        }

    async def _process_offline(self, prompt: str, project_state: dict) -> dict:
        """Process prompt in offline mode with basic command parsing."""
        prompt_lower = prompt.lower()

        # Basic command patterns
        if "create" in prompt_lower:
            return self._handle_create_offline(prompt, project_state)
        elif "add" in prompt_lower:
            return self._handle_add_offline(prompt, project_state)
        elif "delete" in prompt_lower or "remove" in prompt_lower:
            return self._handle_delete_offline(prompt, project_state)
        elif "run" in prompt_lower or "play" in prompt_lower:
            return {
                "message": "Running the project...",
                "commands": [{"action": "project.run", "params": {}}]
            }
        elif "save" in prompt_lower:
            return {
                "message": "Saving the scene...",
                "commands": [{"action": "editor.save_scene", "params": {}}]
            }
        else:
            return {
                "message": "I'm running in offline mode. Connect to an AI service for full natural language processing.\n\nBasic commands I understand:\n- Create a [node type] named [name]\n- Add a script to [node]\n- Delete [node]\n- Run/Play\n- Save",
                "commands": []
            }

    def _handle_create_offline(self, prompt: str, project_state: dict) -> dict:
        """Handle create commands in offline mode."""
        prompt_lower = prompt.lower()

        # Detect node type
        node_types = {
            "player": "CharacterBody2D",
            "enemy": "CharacterBody2D",
            "sprite": "Sprite2D",
            "camera": "Camera2D",
            "tilemap": "TileMap",
            "area": "Area2D",
            "collision": "CollisionShape2D",
            "label": "Label",
            "button": "Button",
            "node2d": "Node2D",
            "node3d": "Node3D",
            "rigidbody": "RigidBody2D",
        }

        detected_type = "Node2D"
        node_name = "NewNode"

        for keyword, node_type in node_types.items():
            if keyword in prompt_lower:
                detected_type = node_type
                node_name = keyword.capitalize()
                break

        # Try to extract name from "named X" pattern
        if "named " in prompt_lower:
            idx = prompt_lower.find("named ") + 6
            name_part = prompt[idx:].split()[0] if idx < len(prompt) else ""
            if name_part:
                node_name = name_part.strip('.,!?"\' ')

        return {
            "message": f"Creating a {detected_type} node named '{node_name}'",
            "commands": [{
                "action": "scene.create_node",
                "params": {
                    "type": detected_type,
                    "name": node_name,
                    "parent": "/root"
                }
            }]
        }

    def _handle_add_offline(self, prompt: str, project_state: dict) -> dict:
        """Handle add commands in offline mode."""
        if "script" in prompt.lower():
            return {
                "message": "To add a script, I need more context. Try: 'Create a script for Player with movement'",
                "commands": []
            }

        return self._handle_create_offline(prompt, project_state)

    def _handle_delete_offline(self, prompt: str, project_state: dict) -> dict:
        """Handle delete commands in offline mode."""
        # Try to find node name in prompt
        words = prompt.split()
        node_name = None

        for i, word in enumerate(words):
            if word.lower() in ["delete", "remove"] and i + 1 < len(words):
                node_name = words[i + 1].strip('.,!?"\' ')
                break

        if node_name:
            return {
                "message": f"Deleting node: {node_name}",
                "commands": [{
                    "action": "scene.delete_node",
                    "params": {"path": f"/root/{node_name}"}
                }]
            }

        return {
            "message": "Please specify which node to delete. Example: 'Delete Player'",
            "commands": []
        }

    async def apply_template(self, template_id: str, customizations: dict) -> dict:
        """
        Apply a game template with customizations.

        Args:
            template_id: ID of the template to apply
            customizations: Dictionary of customization options

        Returns:
            dict with commands to set up the template
        """
        # Template application would load from templates folder
        # For now, return a basic response
        return {
            "message": f"Applied template: {template_id}",
            "commands": []
        }
