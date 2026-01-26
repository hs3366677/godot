"""Anthropic Claude provider implementation."""

import logging
from typing import Optional

import anthropic

from .base_provider import BaseProvider

logger = logging.getLogger('GodotAI.AnthropicProvider')


class AnthropicProvider(BaseProvider):
    """Anthropic Claude API provider."""

    DEFAULT_MODEL = "claude-sonnet-4-20250514"

    def __init__(self, api_key: str, model: Optional[str] = None):
        """
        Initialize Anthropic provider.

        Args:
            api_key: Anthropic API key
            model: Model to use (default: claude-sonnet-4-20250514)
        """
        super().__init__(api_key, model or self.DEFAULT_MODEL)
        self.client = anthropic.AsyncAnthropic(api_key=api_key)
        logger.info(f"Initialized Anthropic provider with model: {self.model}")

    @property
    def name(self) -> str:
        return "anthropic"

    async def complete(
        self,
        system_prompt: str,
        user_prompt: str,
        max_tokens: int = 4096,
        temperature: float = 0.7
    ) -> str:
        """
        Generate a completion using Claude.

        Args:
            system_prompt: System instructions
            user_prompt: User's message
            max_tokens: Maximum tokens in response
            temperature: Sampling temperature

        Returns:
            Generated text response
        """
        try:
            message = await self.client.messages.create(
                model=self.model,
                max_tokens=max_tokens,
                temperature=temperature,
                system=system_prompt,
                messages=[
                    {"role": "user", "content": user_prompt}
                ]
            )

            # Extract text from response
            if message.content and len(message.content) > 0:
                return message.content[0].text

            return ""

        except anthropic.APIError as e:
            logger.error(f"Anthropic API error: {e}")
            raise
        except Exception as e:
            logger.error(f"Error in Anthropic completion: {e}")
            raise

    async def stream_complete(
        self,
        system_prompt: str,
        user_prompt: str,
        max_tokens: int = 4096,
        temperature: float = 0.7
    ):
        """
        Stream a completion using Claude.

        Args:
            system_prompt: System instructions
            user_prompt: User's message
            max_tokens: Maximum tokens in response
            temperature: Sampling temperature

        Yields:
            Generated text chunks
        """
        try:
            async with self.client.messages.stream(
                model=self.model,
                max_tokens=max_tokens,
                temperature=temperature,
                system=system_prompt,
                messages=[
                    {"role": "user", "content": user_prompt}
                ]
            ) as stream:
                async for text in stream.text_stream:
                    yield text

        except anthropic.APIError as e:
            logger.error(f"Anthropic API error: {e}")
            raise
        except Exception as e:
            logger.error(f"Error in Anthropic stream: {e}")
            raise

    async def complete_with_tools(
        self,
        system_prompt: str,
        user_prompt: str,
        tools: list,
        max_tokens: int = 4096,
        temperature: float = 0.7
    ) -> dict:
        """
        Generate a completion with tool use.

        Args:
            system_prompt: System instructions
            user_prompt: User's message
            tools: List of tool definitions
            max_tokens: Maximum tokens in response
            temperature: Sampling temperature

        Returns:
            Response with potential tool calls
        """
        try:
            message = await self.client.messages.create(
                model=self.model,
                max_tokens=max_tokens,
                temperature=temperature,
                system=system_prompt,
                tools=tools,
                messages=[
                    {"role": "user", "content": user_prompt}
                ]
            )

            result = {
                "text": "",
                "tool_calls": []
            }

            for block in message.content:
                if block.type == "text":
                    result["text"] = block.text
                elif block.type == "tool_use":
                    result["tool_calls"].append({
                        "id": block.id,
                        "name": block.name,
                        "input": block.input
                    })

            return result

        except Exception as e:
            logger.error(f"Error in Anthropic tool completion: {e}")
            raise
