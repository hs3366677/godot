# 2D Platformer Template

A classic side-scrolling platformer template with jumping, running, enemies, and collectibles.

## Quick Start

1. Open the template in Godot
2. Run `main_menu.tscn` or `level_01.tscn`
3. Use arrow keys or WASD to move
4. Press Space to jump

## Input Actions Required

Set up these input actions in Project Settings > Input Map:

| Action | Default Key |
|--------|-------------|
| `move_left` | A, Left Arrow |
| `move_right` | D, Right Arrow |
| `jump` | Space, W, Up Arrow |
| `pause` | Escape |

## File Structure

```
project/
├── scenes/
│   ├── main_menu.tscn      # Main menu scene
│   ├── game.tscn           # Main game scene with HUD
│   ├── player.tscn         # Player character
│   ├── enemies/
│   │   ├── enemy_walker.tscn
│   │   └── enemy_jumper.tscn
│   └── levels/
│       ├── level_01.tscn
│       └── level_02.tscn
├── scripts/
│   ├── player.gd           # Player controller
│   ├── game_manager.gd     # Game state management
│   └── enemy_base.gd       # Base enemy class
└── assets/
    ├── sprites/
    ├── audio/
    └── tilesets/
```

## Customization

### Player Settings

Edit `player.gd` exports:

```gdscript
@export var speed: float = 300.0        # Movement speed
@export var jump_force: float = -400.0  # Jump strength (negative = up)
@export var max_jumps: int = 1          # Set to 2 for double jump
```

### Adding New Enemies

1. Create a new scene with `CharacterBody2D` root
2. Attach a script extending `EnemyBase`
3. Override behavior methods:

```gdscript
extends EnemyBase

func _update_behavior(delta: float) -> void:
    # Custom enemy behavior here
    super._update_behavior(delta)
```

### Creating Levels

1. Create a new scene
2. Add a `TileMap` with your tileset
3. Add spawn point (Node2D in "spawn_point" group)
4. Add player instance
5. Place enemies and collectibles

## Signals

### Player Signals
- `died` - Player has died
- `coin_collected(total)` - Coin picked up
- `health_changed(current, max)` - Health updated

### Game Manager Signals
- `game_started` - New game began
- `game_over` - All lives lost
- `level_completed(level)` - Level finished
- `score_changed(score)` - Score updated

## Modules Used

This template uses these modules from the module library:

- `player_movement` - Movement and jumping
- `player_animation` - Sprite animation control
- `level_manager` - Level loading and transitions
- `enemy_ai` - Enemy behavior patterns
- `game_manager` - Game state and scoring

## Common Modifications

### Make player jump higher
```gdscript
# In player.gd
@export var jump_force: float = -500.0  # Was -400.0
```

### Enable double jump
```gdscript
# In player.gd
@export var max_jumps: int = 2  # Was 1
```

### Add wall jump
See the `PlayerMovement` module for wall jump implementation.

### Make enemies faster
```gdscript
# In enemy scenes or enemy_base.gd
@export var speed: float = 150.0  # Was 100.0
```

## Tips

1. Use TileMaps for level geometry
2. Add Area2D nodes for collectibles and hazards
3. Use groups for easy node finding: "player", "enemies", "coins"
4. Create checkpoints with Area2D in "checkpoint" group
