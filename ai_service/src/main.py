#!/usr/bin/env python3
"""
GodotAI Service - Main Entry Point
WebSocket server that connects Godot to AI capabilities.
"""

import asyncio
import json
import logging
import os
from typing import Optional

import websockets
from websockets.server import WebSocketServerProtocol

from orchestrator import AIOrchestrator
from providers.provider_factory import create_provider

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger('GodotAI')


class GodotAIServer:
    """WebSocket server for GodotAI communication."""

    def __init__(self, host: str = "localhost", port: int = 8080):
        self.host = host
        self.port = port
        self.orchestrator: Optional[AIOrchestrator] = None
        self.clients: set[WebSocketServerProtocol] = set()

    async def initialize(self):
        """Initialize AI components."""
        # Get provider from environment or default to anthropic
        provider_name = os.getenv("GODOT_AI_PROVIDER", "anthropic")
        api_key = os.getenv("GODOT_AI_API_KEY") or os.getenv("ANTHROPIC_API_KEY")

        if not api_key:
            logger.warning("No API key found. Set GODOT_AI_API_KEY or ANTHROPIC_API_KEY environment variable.")
            logger.info("Running in offline mode with limited functionality.")
            provider = None
        else:
            provider = create_provider(provider_name, api_key)

        self.orchestrator = AIOrchestrator(provider)
        logger.info(f"AI Orchestrator initialized with provider: {provider_name if provider else 'offline'}")

    async def handle_client(self, websocket: WebSocketServerProtocol):
        """Handle a connected Godot client."""
        self.clients.add(websocket)
        client_id = id(websocket)
        logger.info(f"Client connected: {client_id}")

        try:
            # Send welcome message
            await websocket.send(json.dumps({
                "type": "connected",
                "message": "Connected to GodotAI Service",
                "version": "0.1.0"
            }))

            async for message in websocket:
                await self.handle_message(websocket, message)

        except websockets.exceptions.ConnectionClosed:
            logger.info(f"Client disconnected: {client_id}")
        except Exception as e:
            logger.error(f"Error handling client {client_id}: {e}")
        finally:
            self.clients.discard(websocket)

    async def handle_message(self, websocket: WebSocketServerProtocol, message: str):
        """Process incoming message from Godot."""
        try:
            data = json.loads(message)
            msg_type = data.get("type", "")

            logger.info(f"Received message type: {msg_type}")

            if msg_type == "prompt":
                await self.handle_prompt(websocket, data)
            elif msg_type == "observe":
                await self.handle_observe(websocket, data)
            elif msg_type == "template":
                await self.handle_template_request(websocket, data)
            elif msg_type == "ping":
                await websocket.send(json.dumps({"type": "pong"}))
            else:
                await websocket.send(json.dumps({
                    "type": "error",
                    "message": f"Unknown message type: {msg_type}"
                }))

        except json.JSONDecodeError:
            await websocket.send(json.dumps({
                "type": "error",
                "message": "Invalid JSON"
            }))
        except Exception as e:
            logger.error(f"Error processing message: {e}")
            await websocket.send(json.dumps({
                "type": "error",
                "message": str(e)
            }))

    async def handle_prompt(self, websocket: WebSocketServerProtocol, data: dict):
        """Process a natural language prompt."""
        prompt = data.get("content", "")
        context = data.get("context", {})
        project_state = data.get("project_state", {})

        if not prompt:
            await websocket.send(json.dumps({
                "type": "error",
                "message": "Empty prompt"
            }))
            return

        logger.info(f"Processing prompt: {prompt[:100]}...")

        # Send acknowledgment
        await websocket.send(json.dumps({
            "type": "message",
            "content": "Processing your request..."
        }))

        # Process with orchestrator
        try:
            result = await self.orchestrator.process_prompt(
                prompt=prompt,
                context=context,
                project_state=project_state
            )

            # Send response based on result type
            if result.get("commands"):
                await websocket.send(json.dumps({
                    "type": "commands",
                    "commands": result["commands"],
                    "message": result.get("message", "")
                }))
            else:
                await websocket.send(json.dumps({
                    "type": "message",
                    "content": result.get("message", "I processed your request.")
                }))

        except Exception as e:
            logger.error(f"Error processing prompt: {e}")
            await websocket.send(json.dumps({
                "type": "error",
                "message": f"Failed to process: {str(e)}"
            }))

    async def handle_observe(self, websocket: WebSocketServerProtocol, data: dict):
        """Handle state observation request."""
        # Echo back the observed state (Godot sends its state, we acknowledge)
        await websocket.send(json.dumps({
            "type": "observe_ack",
            "received": True
        }))

    async def handle_template_request(self, websocket: WebSocketServerProtocol, data: dict):
        """Handle template selection and customization."""
        template_id = data.get("template_id", "")
        customizations = data.get("customizations", {})

        try:
            result = await self.orchestrator.apply_template(template_id, customizations)
            await websocket.send(json.dumps({
                "type": "commands",
                "commands": result.get("commands", []),
                "message": result.get("message", f"Applied template: {template_id}")
            }))
        except Exception as e:
            await websocket.send(json.dumps({
                "type": "error",
                "message": f"Failed to apply template: {str(e)}"
            }))

    async def start(self):
        """Start the WebSocket server."""
        await self.initialize()

        logger.info(f"Starting GodotAI server on ws://{self.host}:{self.port}")

        async with websockets.serve(self.handle_client, self.host, self.port):
            logger.info("Server is running. Press Ctrl+C to stop.")
            await asyncio.Future()  # Run forever


def main():
    """Main entry point."""
    host = os.getenv("GODOT_AI_HOST", "localhost")
    port = int(os.getenv("GODOT_AI_PORT", "8080"))

    server = GodotAIServer(host, port)

    try:
        asyncio.run(server.start())
    except KeyboardInterrupt:
        logger.info("Server stopped by user")


if __name__ == "__main__":
    main()
