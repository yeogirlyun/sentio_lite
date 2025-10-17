#!/usr/bin/env python3
"""
Service layer for mega document creation.
"""

from .mega_document_service import FileExtractor, MegaDocumentBuilder, MegaDocumentService

__all__ = ['FileExtractor', 'MegaDocumentBuilder', 'MegaDocumentService']
