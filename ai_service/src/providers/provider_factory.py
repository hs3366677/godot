"""Factory for creating LLM providers."""

from typing import Optional

from .base_provider import BaseProvider
from .anthropic_provider import AnthropicProvider
from .openai_provider import OpenAIProvider


def create_provider(
    provider_name: str,
    api_key: str,
    model: Optional[str] = None
) -> BaseProvider:
    """
    Create an LLM provider instance.

    Args:
        provider_name: Name of the provider ('anthropic', 'openai')
        api_key: API key for the provider
        model: Optional model override

    Returns:
        Provider instance

    Raises:
        ValueError: If provider name is unknown
    """
    provider_name = provider_name.lower()

    if provider_name in ("anthropic", "claude"):
        return AnthropicProvider(api_key, model)
    elif provider_name in ("openai", "gpt"):
        return OpenAIProvider(api_key, model)
    else:
        raise ValueError(f"Unknown provider: {provider_name}. Supported: anthropic, openai")
