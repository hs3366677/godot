"""Code Generator - Generates GDScript code for game functionality."""

import logging
from typing import Optional

logger = logging.getLogger('GodotAI.CodeGenerator')


class CodeGenerator:
    """Generates GDScript code using templates and LLM."""

    # Code templates for common patterns
    TEMPLATES = {
        "player_movement_2d": '''extends CharacterBody2D
## Player movement script with {features}

{exports}

var velocity: Vector2 = Vector2.ZERO
{variables}

func _physics_process(delta: float) -> void:
{physics_code}
	move_and_slide()
''',

        "enemy_basic": '''extends CharacterBody2D
## Basic enemy with {behavior} behavior

@export var speed: float = {speed}
@export var damage: int = {damage}

var direction: int = 1

func _physics_process(delta: float) -> void:
{behavior_code}
	move_and_slide()

func _on_body_entered(body: Node2D) -> void:
	if body.is_in_group("player"):
		if body.has_method("take_damage"):
			body.take_damage(damage)
''',

        "collectible": '''extends Area2D
## Collectible item: {item_type}

@export var value: int = {value}

func _ready() -> void:
	body_entered.connect(_on_body_entered)

func _on_body_entered(body: Node2D) -> void:
	if body.is_in_group("player"):
		_collect(body)
		queue_free()

func _collect(player: Node2D) -> void:
{collect_code}
''',

        "state_machine": '''extends Node
## State machine for {entity}

enum State {{{states}}}

var current_state: State = State.{initial_state}

func _physics_process(delta: float) -> void:
	match current_state:
{state_handlers}

func change_state(new_state: State) -> void:
	if new_state == current_state:
		return
	_exit_state(current_state)
	current_state = new_state
	_enter_state(new_state)

func _enter_state(state: State) -> void:
	match state:
{enter_handlers}

func _exit_state(state: State) -> void:
	match state:
{exit_handlers}
''',
    }

    def __init__(self, provider=None):
        """
        Initialize code generator.

        Args:
            provider: LLM provider for advanced generation
        """
        self.provider = provider

    async def generate(
        self,
        code_type: str,
        parameters: dict,
        context: dict = None
    ) -> str:
        """
        Generate GDScript code.

        Args:
            code_type: Type of code to generate
            parameters: Parameters for generation
            context: Additional context

        Returns:
            Generated GDScript code
        """
        # Try template-based generation first
        if code_type in self.TEMPLATES:
            return self._generate_from_template(code_type, parameters)

        # Fall back to LLM generation
        if self.provider:
            return await self._generate_with_llm(code_type, parameters, context)

        # Basic fallback
        return self._generate_basic_script(parameters)

    def _generate_from_template(self, template_name: str, params: dict) -> str:
        """Generate code from a template."""
        template = self.TEMPLATES[template_name]

        # Process parameters based on template type
        if template_name == "player_movement_2d":
            return self._fill_player_template(template, params)
        elif template_name == "enemy_basic":
            return self._fill_enemy_template(template, params)
        elif template_name == "collectible":
            return self._fill_collectible_template(template, params)
        elif template_name == "state_machine":
            return self._fill_state_machine_template(template, params)

        return template.format(**params)

    def _fill_player_template(self, template: str, params: dict) -> str:
        """Fill player movement template."""
        features = params.get("features", ["movement", "jump"])
        speed = params.get("speed", 300.0)
        jump_force = params.get("jump_force", -400.0)
        gravity = params.get("gravity", 980.0)

        exports = []
        variables = []
        physics_code = []

        # Basic movement
        exports.append(f"@export var speed: float = {speed}")
        physics_code.append("\tvar input_dir = Input.get_axis(\"move_left\", \"move_right\")")
        physics_code.append("\tvelocity.x = input_dir * speed")

        # Gravity
        physics_code.insert(0, "\tif not is_on_floor():")
        physics_code.insert(1, f"\t\tvelocity.y += {gravity} * delta")

        # Jump
        if "jump" in features:
            exports.append(f"@export var jump_force: float = {jump_force}")
            physics_code.append("\tif Input.is_action_just_pressed(\"jump\") and is_on_floor():")
            physics_code.append("\t\tvelocity.y = jump_force")

        # Double jump
        if "double_jump" in features:
            exports.append("@export var max_jumps: int = 2")
            variables.append("var jump_count: int = 0")
            # Modify jump logic for double jump
            physics_code = [line.replace("and is_on_floor()", "and jump_count < max_jumps") for line in physics_code]
            physics_code.append("\t\tjump_count += 1")
            physics_code.append("\tif is_on_floor():")
            physics_code.append("\t\tjump_count = 0")

        return template.format(
            features=", ".join(features),
            exports="\n".join(exports),
            variables="\n".join(variables) if variables else "",
            physics_code="\n".join(physics_code)
        )

    def _fill_enemy_template(self, template: str, params: dict) -> str:
        """Fill enemy template."""
        behavior = params.get("behavior", "patrol")
        speed = params.get("speed", 100)
        damage = params.get("damage", 1)

        behavior_code = []

        if behavior == "patrol":
            behavior_code = [
                "\tvelocity.x = speed * direction",
                "\t",
                "\t# Turn around at walls",
                "\tif is_on_wall():",
                "\t\tdirection *= -1"
            ]
        elif behavior == "chase":
            behavior_code = [
                "\tvar player = get_tree().get_first_node_in_group(\"player\")",
                "\tif player:",
                "\t\tvar dir = sign(player.global_position.x - global_position.x)",
                "\t\tvelocity.x = speed * dir"
            ]
        elif behavior == "stationary":
            behavior_code = ["\tvelocity.x = 0"]

        return template.format(
            behavior=behavior,
            speed=speed,
            damage=damage,
            behavior_code="\n".join(behavior_code)
        )

    def _fill_collectible_template(self, template: str, params: dict) -> str:
        """Fill collectible template."""
        item_type = params.get("item_type", "coin")
        value = params.get("value", 1)

        collect_code = []

        if item_type == "coin":
            collect_code = [
                "\tif player.has_method(\"add_coins\"):",
                "\t\tplayer.add_coins(value)",
                "\telif \"coins\" in player:",
                "\t\tplayer.coins += value"
            ]
        elif item_type == "health":
            collect_code = [
                "\tif player.has_method(\"heal\"):",
                "\t\tplayer.heal(value)"
            ]
        elif item_type == "powerup":
            collect_code = [
                "\tif player.has_method(\"apply_powerup\"):",
                "\t\tplayer.apply_powerup(\"{}\")".format(params.get("powerup_type", "speed"))
            ]

        return template.format(
            item_type=item_type,
            value=value,
            collect_code="\n".join(collect_code)
        )

    def _fill_state_machine_template(self, template: str, params: dict) -> str:
        """Fill state machine template."""
        entity = params.get("entity", "Entity")
        states = params.get("states", ["IDLE", "MOVING"])
        initial_state = params.get("initial_state", states[0])

        state_handlers = []
        enter_handlers = []
        exit_handlers = []

        for state in states:
            state_handlers.append(f"\t\tState.{state}:")
            state_handlers.append(f"\t\t\t_process_{state.lower()}(delta)")

            enter_handlers.append(f"\t\tState.{state}:")
            enter_handlers.append(f"\t\t\tpass  # Enter {state}")

            exit_handlers.append(f"\t\tState.{state}:")
            exit_handlers.append(f"\t\t\tpass  # Exit {state}")

        return template.format(
            entity=entity,
            states=", ".join(states),
            initial_state=initial_state,
            state_handlers="\n".join(state_handlers),
            enter_handlers="\n".join(enter_handlers),
            exit_handlers="\n".join(exit_handlers)
        )

    async def _generate_with_llm(
        self,
        code_type: str,
        parameters: dict,
        context: dict
    ) -> str:
        """Generate code using LLM."""
        prompt = f"""Generate GDScript code for: {code_type}

Parameters:
{parameters}

Context:
{context}

Requirements:
- Use Godot 4.x syntax
- Include appropriate @export variables
- Add helpful comments
- Follow GDScript style guidelines
- Make the code modular and reusable

Generate only the code, no explanations."""

        try:
            return await self.provider.complete(
                system_prompt="You are a GDScript expert. Generate clean, efficient Godot 4.x code.",
                user_prompt=prompt,
                temperature=0.3
            )
        except Exception as e:
            logger.error(f"LLM code generation failed: {e}")
            return self._generate_basic_script(parameters)

    def _generate_basic_script(self, params: dict) -> str:
        """Generate a basic script as fallback."""
        base_type = params.get("base_type", "Node")
        script_name = params.get("name", "NewScript")

        return f'''extends {base_type}
## {script_name}
## Generated by GodotAI


func _ready() -> void:
	pass


func _process(delta: float) -> void:
	pass
'''
