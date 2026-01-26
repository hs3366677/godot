"""Context Manager - Manages conversation and project context for AI processing."""

import json
import logging
import os
from typing import Any, Optional

logger = logging.getLogger('GodotAI.ContextManager')


class ContextManager:
    """Manages context for AI processing including project state and conversation history."""

    def __init__(self, modules_path: str = None, templates_path: str = None):
        """
        Initialize context manager.

        Args:
            modules_path: Path to modules directory
            templates_path: Path to templates directory
        """
        # Use relative paths from project root if not specified
        self.modules_path = modules_path or "../modules"
        self.templates_path = templates_path or "../templates"

        # State storage
        self.project_state: dict = {}
        self.conversation_history: list = []
        self.entity_registry: dict = {}

        # Cached data
        self._modules_cache: Optional[dict] = None
        self._templates_cache: Optional[dict] = None

    def update_project_state(self, state: dict) -> None:
        """
        Update the current project state.

        Args:
            state: New project state from Godot
        """
        self.project_state = state
        logger.debug(f"Project state updated: {list(state.keys())}")

    def add_conversation_turn(self, role: str, content: str) -> None:
        """
        Add a turn to conversation history.

        Args:
            role: 'user' or 'assistant'
            content: Message content
        """
        self.conversation_history.append({
            "role": role,
            "content": content
        })

        # Keep history manageable
        if len(self.conversation_history) > 50:
            self.conversation_history = self.conversation_history[-50:]

    def register_entity(self, name: str, entity_type: str, path: str) -> None:
        """
        Register a named entity for reference tracking.

        Args:
            name: Entity name (e.g., "Player", "Enemy1")
            entity_type: Type of entity (e.g., "CharacterBody2D")
            path: Scene path to the entity
        """
        self.entity_registry[name.lower()] = {
            "name": name,
            "type": entity_type,
            "path": path
        }

    def resolve_entity(self, name: str) -> Optional[dict]:
        """
        Resolve an entity name to its details.

        Args:
            name: Entity name to look up

        Returns:
            Entity details or None
        """
        return self.entity_registry.get(name.lower())

    def get_modules(self) -> dict:
        """
        Get available modules.

        Returns:
            Module registry data
        """
        if self._modules_cache is None:
            self._modules_cache = self._load_modules()
        return self._modules_cache

    def _load_modules(self) -> dict:
        """Load module registry and interfaces."""
        modules = {"categories": {}, "modules": {}}

        registry_path = os.path.join(self.modules_path, "module_registry.json")
        if os.path.exists(registry_path):
            try:
                with open(registry_path, 'r') as f:
                    registry = json.load(f)
                    modules["categories"] = registry.get("categories", {})
            except Exception as e:
                logger.warning(f"Failed to load module registry: {e}")

        # Load individual module interfaces
        for category, category_data in modules["categories"].items():
            for module_id in category_data.get("modules", []):
                interface_path = os.path.join(
                    self.modules_path,
                    category,
                    module_id,
                    "interface.json"
                )
                if os.path.exists(interface_path):
                    try:
                        with open(interface_path, 'r') as f:
                            modules["modules"][module_id] = json.load(f)
                    except Exception as e:
                        logger.warning(f"Failed to load module {module_id}: {e}")

        return modules

    def get_templates(self) -> dict:
        """
        Get available templates.

        Returns:
            Template registry data
        """
        if self._templates_cache is None:
            self._templates_cache = self._load_templates()
        return self._templates_cache

    def _load_templates(self) -> dict:
        """Load template registry and manifests."""
        templates = {"templates": []}

        registry_path = os.path.join(self.templates_path, "template_registry.json")
        if os.path.exists(registry_path):
            try:
                with open(registry_path, 'r') as f:
                    registry = json.load(f)
                    templates["templates"] = registry.get("templates", [])
            except Exception as e:
                logger.warning(f"Failed to load template registry: {e}")

        # Load individual template manifests
        for template_info in templates["templates"]:
            template_id = template_info.get("id")
            if template_id:
                manifest_path = os.path.join(
                    self.templates_path,
                    template_id,
                    "template.json"
                )
                if os.path.exists(manifest_path):
                    try:
                        with open(manifest_path, 'r') as f:
                            template_info["manifest"] = json.load(f)
                    except Exception as e:
                        logger.warning(f"Failed to load template {template_id}: {e}")

        return templates

    def get_relevant_modules(self, prompt: str) -> list:
        """
        Get modules relevant to a prompt.

        Args:
            prompt: User prompt

        Returns:
            List of relevant module IDs
        """
        prompt_lower = prompt.lower()
        relevant = []

        modules = self.get_modules()

        # Keywords to module mapping
        keyword_map = {
            "player": ["player_movement", "player_stats", "player_combat"],
            "movement": ["player_movement"],
            "jump": ["player_movement"],
            "enemy": ["enemy_ai"],
            "patrol": ["enemy_ai"],
            "chase": ["enemy_ai"],
            "save": ["save_system"],
            "load": ["save_system"],
            "audio": ["audio_manager"],
            "sound": ["audio_manager"],
            "music": ["audio_manager"],
            "ui": ["ui_manager"],
            "menu": ["ui_manager"],
            "input": ["input_handler"],
            "control": ["input_handler"],
        }

        for keyword, module_ids in keyword_map.items():
            if keyword in prompt_lower:
                for module_id in module_ids:
                    if module_id not in relevant and module_id in modules.get("modules", {}):
                        relevant.append(module_id)

        return relevant

    def get_module_interface(self, module_id: str) -> Optional[dict]:
        """
        Get interface definition for a module.

        Args:
            module_id: Module identifier

        Returns:
            Module interface or None
        """
        modules = self.get_modules()
        return modules.get("modules", {}).get(module_id)

    def get_template(self, template_id: str) -> Optional[dict]:
        """
        Get template details.

        Args:
            template_id: Template identifier

        Returns:
            Template info with manifest or None
        """
        templates = self.get_templates()
        for template in templates.get("templates", []):
            if template.get("id") == template_id:
                return template
        return None

    def build_context_summary(self, max_tokens: int = 2000) -> str:
        """
        Build a summary of current context for LLM.

        Args:
            max_tokens: Approximate max length

        Returns:
            Context summary string
        """
        parts = []

        # Project info
        if self.project_state:
            parts.append("Current Project:")
            if self.project_state.get("project_name"):
                parts.append(f"  Name: {self.project_state['project_name']}")
            if self.project_state.get("current_scene"):
                parts.append(f"  Current Scene: {self.project_state['current_scene']}")

        # Registered entities
        if self.entity_registry:
            parts.append("\nKnown Entities:")
            for name, entity in list(self.entity_registry.items())[:10]:
                parts.append(f"  {entity['name']} ({entity['type']}): {entity['path']}")

        # Recent conversation
        if self.conversation_history:
            parts.append("\nRecent Context:")
            for turn in self.conversation_history[-5:]:
                content = turn['content'][:100] + "..." if len(turn['content']) > 100 else turn['content']
                parts.append(f"  {turn['role']}: {content}")

        return "\n".join(parts)

    def clear_conversation(self) -> None:
        """Clear conversation history."""
        self.conversation_history = []

    def clear_entities(self) -> None:
        """Clear entity registry."""
        self.entity_registry = {}

    def refresh_caches(self) -> None:
        """Refresh module and template caches."""
        self._modules_cache = None
        self._templates_cache = None
