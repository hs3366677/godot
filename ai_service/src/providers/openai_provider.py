"""OpenAI GPT provider implementation."""

import logging
from typing import Optional

import openai

from .base_provider import BaseProvider

logger = logging.getLogger('GodotAI.OpenAIProvider')


class OpenAIProvider(BaseProvider):
    """OpenAI GPT API provider."""

    DEFAULT_MODEL = "gpt-4o"

    def __init__(self, api_key: str, model: Optional[str] = None):
        """
        Initialize OpenAI provider.

        Args:
            api_key: OpenAI API key
            model: Model to use (default: gpt-4o)
        """
        super().__init__(api_key, model or self.DEFAULT_MODEL)
        self.client = openai.AsyncOpenAI(api_key=api_key)
        logger.info(f"Initialized OpenAI provider with model: {self.model}")

    @property
    def name(self) -> str:
        return "openai"

    async def complete(
        self,
        system_prompt: str,
        user_prompt: str,
        max_tokens: int = 4096,
        temperature: float = 0.7
    ) -> str:
        """
        Generate a completion using GPT.

        Args:
            system_prompt: System instructions
            user_prompt: User's message
            max_tokens: Maximum tokens in response
            temperature: Sampling temperature

        Returns:
            Generated text response
        """
        try:
            response = await self.client.chat.completions.create(
                model=self.model,
                max_tokens=max_tokens,
                temperature=temperature,
                messages=[
                    {"role": "system", "content": system_prompt},
                    {"role": "user", "content": user_prompt}
                ]
            )

            if response.choices and len(response.choices) > 0:
                return response.choices[0].message.content or ""

            return ""

        except openai.APIError as e:
            logger.error(f"OpenAI API error: {e}")
            raise
        except Exception as e:
            logger.error(f"Error in OpenAI completion: {e}")
            raise

    async def stream_complete(
        self,
        system_prompt: str,
        user_prompt: str,
        max_tokens: int = 4096,
        temperature: float = 0.7
    ):
        """
        Stream a completion using GPT.

        Args:
            system_prompt: System instructions
            user_prompt: User's message
            max_tokens: Maximum tokens in response
            temperature: Sampling temperature

        Yields:
            Generated text chunks
        """
        try:
            stream = await self.client.chat.completions.create(
                model=self.model,
                max_tokens=max_tokens,
                temperature=temperature,
                stream=True,
                messages=[
                    {"role": "system", "content": system_prompt},
                    {"role": "user", "content": user_prompt}
                ]
            )

            async for chunk in stream:
                if chunk.choices and chunk.choices[0].delta.content:
                    yield chunk.choices[0].delta.content

        except openai.APIError as e:
            logger.error(f"OpenAI API error: {e}")
            raise
        except Exception as e:
            logger.error(f"Error in OpenAI stream: {e}")
            raise
