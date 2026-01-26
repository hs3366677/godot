# GodotAI Service

AI backend service for the GodotAI plugin. Provides natural language processing for game creation.

## Setup

### 1. Install Dependencies

```bash
cd ai_service
pip install -r requirements.txt
```

### 2. Configure API Key

Set your API key as an environment variable:

**For Anthropic (Claude):**
```bash
# Windows
set ANTHROPIC_API_KEY=your_api_key_here

# Linux/macOS
export ANTHROPIC_API_KEY=your_api_key_here
```

**For OpenAI:**
```bash
set OPENAI_API_KEY=your_api_key_here
set GODOT_AI_PROVIDER=openai
```

### 3. Run the Service

```bash
cd src
python main.py
```

The service will start on `ws://localhost:8080` by default.

## Configuration

Environment variables:

| Variable | Default | Description |
|----------|---------|-------------|
| `GODOT_AI_HOST` | `localhost` | Server host |
| `GODOT_AI_PORT` | `8080` | Server port |
| `GODOT_AI_PROVIDER` | `anthropic` | LLM provider (`anthropic` or `openai`) |
| `GODOT_AI_API_KEY` | - | API key (or use provider-specific key) |
| `ANTHROPIC_API_KEY` | - | Anthropic API key |
| `OPENAI_API_KEY` | - | OpenAI API key |

## Architecture

```
ai_service/
├── src/
│   ├── main.py              # Entry point & WebSocket server
│   ├── orchestrator.py      # AI processing coordination
│   ├── providers/           # LLM provider implementations
│   │   ├── base_provider.py
│   │   ├── anthropic_provider.py
│   │   └── openai_provider.py
│   ├── analyzers/           # Prompt analysis
│   │   └── prompt_analyzer.py
│   ├── generators/          # Code & command generation
│   │   ├── code_generator.py
│   │   └── command_generator.py
│   └── context/             # Context management
│       └── context_manager.py
└── requirements.txt
```

## Protocol

The service communicates via WebSocket using JSON messages.

### Message Types

**From Godot:**

```json
// Natural language prompt
{
    "type": "prompt",
    "content": "Create a player that can jump",
    "context": {},
    "project_state": {}
}

// Template request
{
    "type": "template",
    "template_id": "platformer_2d",
    "customizations": {}
}

// Ping
{
    "type": "ping"
}
```

**To Godot:**

```json
// Commands to execute
{
    "type": "commands",
    "commands": [
        {
            "action": "scene.create_node",
            "params": {"type": "CharacterBody2D", "name": "Player"}
        }
    ],
    "message": "Created player character"
}

// Text message
{
    "type": "message",
    "content": "I understand you want to..."
}

// Error
{
    "type": "error",
    "message": "Failed to process request"
}
```

## Offline Mode

If no API key is configured, the service runs in offline mode with limited functionality:
- Basic command parsing (create, delete, run, save)
- Template-based code generation
- No natural language understanding

## Development

### Adding a New Provider

1. Create a new file in `providers/`
2. Extend `BaseProvider`
3. Implement `complete()` and `stream_complete()`
4. Register in `provider_factory.py`

### Adding Code Templates

Add templates to `CodeGenerator.TEMPLATES` in `generators/code_generator.py`.

## Troubleshooting

**Connection refused:**
- Check if the service is running
- Verify host and port settings
- Check firewall settings

**API errors:**
- Verify API key is set correctly
- Check API quota/limits
- Review provider-specific error messages

**Offline mode:**
- Set `GODOT_AI_API_KEY` or provider-specific key
- Check that the provider name is correct
