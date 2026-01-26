"""Base class for LLM providers."""

from abc import ABC, abstractmethod
from typing import Optional


class BaseProvider(ABC):
    """Abstract base class for LLM providers."""

    def __init__(self, api_key: str, model: Optional[str] = None):
        """
        Initialize provider.

        Args:
            api_key: API key for the provider
            model: Model identifier (provider-specific)
        """
        self.api_key = api_key
        self.model = model

    @abstractmethod
    async def complete(
        self,
        system_prompt: str,
        user_prompt: str,
        max_tokens: int = 4096,
        temperature: float = 0.7
    ) -> str:
        """
        Generate a completion from the LLM.

        Args:
            system_prompt: System instructions
            user_prompt: User's message
            max_tokens: Maximum tokens in response
            temperature: Sampling temperature

        Returns:
            Generated text response
        """
        pass

    @abstractmethod
    async def stream_complete(
        self,
        system_prompt: str,
        user_prompt: str,
        max_tokens: int = 4096,
        temperature: float = 0.7
    ):
        """
        Stream a completion from the LLM.

        Args:
            system_prompt: System instructions
            user_prompt: User's message
            max_tokens: Maximum tokens in response
            temperature: Sampling temperature

        Yields:
            Generated text chunks
        """
        pass

    @property
    @abstractmethod
    def name(self) -> str:
        """Provider name."""
        pass
