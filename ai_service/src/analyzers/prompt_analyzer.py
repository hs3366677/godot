"""Prompt Analyzer - Extracts intent and entities from user prompts."""

import logging
import re
from typing import Any, Optional

logger = logging.getLogger('GodotAI.PromptAnalyzer')


class PromptAnalyzer:
    """Analyzes user prompts to extract intent and entities."""

    # Intent patterns
    INTENT_PATTERNS = {
        "create": [
            r"\b(create|make|add|build|generate|spawn)\b",
        ],
        "modify": [
            r"\b(change|modify|update|edit|adjust|set)\b",
        ],
        "delete": [
            r"\b(delete|remove|destroy|kill|erase)\b",
        ],
        "query": [
            r"\b(what|how|where|why|show|list|find)\b",
        ],
        "run": [
            r"\b(run|play|start|execute|test|launch)\b",
        ],
        "save": [
            r"\b(save|export|store)\b",
        ],
    }

    # Entity patterns for Godot concepts
    ENTITY_PATTERNS = {
        "node_type": [
            r"\b(CharacterBody2D|CharacterBody3D|RigidBody2D|RigidBody3D)\b",
            r"\b(Sprite2D|Sprite3D|AnimatedSprite2D|AnimatedSprite3D)\b",
            r"\b(Camera2D|Camera3D|Node2D|Node3D|Control)\b",
            r"\b(Area2D|Area3D|CollisionShape2D|CollisionShape3D)\b",
            r"\b(TileMap|TileMapLayer|Label|Button|TextureRect)\b",
            r"\b(AudioStreamPlayer|AudioStreamPlayer2D|AudioStreamPlayer3D)\b",
            r"\b(player|enemy|npc|boss|projectile|bullet)\b",
            r"\b(platform|wall|floor|obstacle|spike|hazard)\b",
        ],
        "game_genre": [
            r"\b(platformer|shooter|rpg|puzzle|racing|fighting)\b",
            r"\b(tower defense|td|strategy|simulation|adventure)\b",
            r"\b(roguelike|roguelite|metroidvania|souls-like)\b",
        ],
        "mechanic": [
            r"\b(jump|double jump|wall jump|dash|slide)\b",
            r"\b(shoot|attack|block|dodge|parry)\b",
            r"\b(patrol|chase|flee|wander|follow)\b",
            r"\b(spawn|respawn|checkpoint|save point)\b",
            r"\b(health|damage|heal|die|kill)\b",
            r"\b(collect|pickup|inventory|equip)\b",
        ],
        "property": [
            r"\b(speed|velocity|acceleration|friction)\b",
            r"\b(health|damage|armor|defense)\b",
            r"\b(position|rotation|scale|size)\b",
            r"\b(color|texture|sprite|animation)\b",
        ],
    }

    def __init__(self, provider=None):
        """
        Initialize analyzer.

        Args:
            provider: LLM provider for advanced analysis (optional)
        """
        self.provider = provider

    async def analyze(self, prompt: str, project_state: dict = None) -> dict:
        """
        Analyze a user prompt.

        Args:
            prompt: User's natural language prompt
            project_state: Current project state for context

        Returns:
            Analysis result with intent, entities, and metadata
        """
        prompt_lower = prompt.lower()

        # Basic pattern matching
        intent = self._detect_intent(prompt_lower)
        entities = self._extract_entities(prompt)
        scope = self._determine_scope(prompt_lower, entities)

        result = {
            "intent": intent,
            "entities": entities,
            "scope": scope,
            "original_prompt": prompt
        }

        # If provider available, enhance with LLM analysis
        if self.provider and self._needs_advanced_analysis(result):
            result = await self._enhance_with_llm(prompt, result, project_state)

        logger.debug(f"Analysis result: {result}")
        return result

    def _detect_intent(self, prompt: str) -> str:
        """Detect the primary intent from the prompt."""
        for intent, patterns in self.INTENT_PATTERNS.items():
            for pattern in patterns:
                if re.search(pattern, prompt, re.IGNORECASE):
                    return intent

        return "unknown"

    def _extract_entities(self, prompt: str) -> dict:
        """Extract entities from the prompt."""
        entities = {}

        for entity_type, patterns in self.ENTITY_PATTERNS.items():
            matches = []
            for pattern in patterns:
                found = re.findall(pattern, prompt, re.IGNORECASE)
                matches.extend(found)

            if matches:
                entities[entity_type] = list(set(matches))

        # Extract quoted names
        quoted = re.findall(r'["\']([^"\']+)["\']', prompt)
        if quoted:
            entities["names"] = quoted

        # Extract "named X" pattern
        named_match = re.search(r'\bnamed\s+(\w+)', prompt, re.IGNORECASE)
        if named_match:
            entities.setdefault("names", []).append(named_match.group(1))

        # Extract numbers for properties
        numbers = re.findall(r'\b(\d+(?:\.\d+)?)\b', prompt)
        if numbers:
            entities["numbers"] = [float(n) if '.' in n else int(n) for n in numbers]

        return entities

    def _determine_scope(self, prompt: str, entities: dict) -> str:
        """Determine the scope of the request."""
        # Check for project-wide keywords
        if any(word in prompt for word in ["project", "game", "entire", "all"]):
            return "project"

        # Check for scene-level keywords
        if any(word in prompt for word in ["scene", "level", "stage"]):
            return "scene"

        # Check for node-level keywords or specific entities
        if entities.get("node_type") or any(word in prompt for word in ["node", "object", "element"]):
            return "node"

        # Check for code/script level
        if any(word in prompt for word in ["script", "code", "function", "variable"]):
            return "script"

        return "unknown"

    def _needs_advanced_analysis(self, result: dict) -> bool:
        """Determine if advanced LLM analysis is needed."""
        # If intent is unknown or entities are sparse
        if result["intent"] == "unknown":
            return True

        if result["scope"] == "unknown":
            return True

        # If the prompt seems complex
        if len(result["original_prompt"].split()) > 20:
            return True

        return False

    async def _enhance_with_llm(
        self,
        prompt: str,
        initial_result: dict,
        project_state: dict
    ) -> dict:
        """Enhance analysis using LLM."""
        try:
            analysis_prompt = f"""Analyze this game development request and extract:
1. Primary intent (create, modify, delete, query, run, save)
2. Target entities (node types, game objects, properties)
3. Scope (project, scene, node, script)
4. Any specific values or parameters mentioned

Request: "{prompt}"

Initial analysis detected:
- Intent: {initial_result['intent']}
- Entities: {initial_result['entities']}
- Scope: {initial_result['scope']}

Provide corrections or additions in JSON format:
{{"intent": "...", "entities": {{...}}, "scope": "...", "additional_context": "..."}}"""

            response = await self.provider.complete(
                system_prompt="You are a game development request analyzer. Respond with JSON only.",
                user_prompt=analysis_prompt,
                max_tokens=500,
                temperature=0.3
            )

            # Try to parse LLM response and merge
            import json
            try:
                llm_analysis = json.loads(response)
                # Merge with initial result, preferring LLM analysis
                initial_result.update({
                    k: v for k, v in llm_analysis.items()
                    if v and v != "unknown"
                })
            except json.JSONDecodeError:
                logger.warning("Could not parse LLM analysis response")

        except Exception as e:
            logger.warning(f"LLM enhancement failed: {e}")

        return initial_result
