#!/usr/bin/env python3
"""
Exception hierarchy for mega document operations.
"""

from .mega_doc_exceptions import (
    MegaDocumentError, FileValidationError, SecurityError, ConfigurationError,
    ProcessingError, ExtractionError, BuildError, CacheError, PerformanceError,
    handle_errors, ErrorContext
)

__all__ = [
    'MegaDocumentError', 'FileValidationError', 'SecurityError', 'ConfigurationError',
    'ProcessingError', 'ExtractionError', 'BuildError', 'CacheError', 'PerformanceError',
    'handle_errors', 'ErrorContext'
]
