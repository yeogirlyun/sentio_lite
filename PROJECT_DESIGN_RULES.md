# Project Design Rules

## Version Control & Module Management

### Rule 1: One Module Per Purpose
**DO NOT create different filename modules that do the same thing.**

- When improving functionality, ALWAYS work directly on the existing module
- If modification is more difficult than rewriting from scratch, REMOVE the existing module completely and start fresh with the same filename
- Never create variations like `module_v2.py`, `module_new.py`, `module_refactored.py`

**Example:**
```
❌ WRONG: Create rotation_trading_dashboard_v2.py alongside rotation_trading_dashboard.py
✅ CORRECT: Replace rotation_trading_dashboard.py with improved version
```

### Rule 2: Module Retirement
When replacing a module:
1. Delete the old file completely
2. Rewrite with the same filename
3. Update all references in the codebase
4. Document the change in commit message

### Rule 3: Script Consolidation
**Current Standard Modules:**
- `scripts/rotation_trading_dashboard_html.py` - Primary dashboard generator for rotation trading
- `scripts/send_dashboard_email.py` - Email distribution (different purpose, keep separate)

**Deprecated/Removed:**
- ~~`scripts/professional_trading_dashboard.py`~~ - Removed 2025-10-17
- ~~`scripts/rotation_trading_aggregate_dashboard.py`~~ - Removed 2025-10-17
- ~~`scripts/rotation_trading_dashboard.py`~~ - Removed 2025-10-17

## Code Quality Standards

### Rule 4: Direct Modification Over Duplication
When asked to improve a feature:
1. Read the existing implementation
2. Determine if modification or rewrite is more efficient
3. If modifying: Edit the existing file directly
4. If rewriting: Delete old file, write new one with SAME name

### Rule 5: Documentation Requirements
Every module must have:
- Clear docstring explaining purpose
- Usage examples in comments or separate docs
- Version history in git (not in filename)

## Architecture Principles

### Rule 6: Single Source of Truth
- One authoritative module per functionality
- Configuration in `config/` directory
- Data in `data/` directory
- Logs in `logs/` directory

### Rule 7: Backward Compatibility
When replacing a module:
- Maintain the same command-line interface if it's a script
- Maintain the same function signatures if it's a library
- Update dependent code immediately if breaking changes are necessary

## Testing Standards

### Rule 8: Test Before Removal
Before removing/replacing any module:
1. Identify all usages in codebase
2. Create test cases if they don't exist
3. Verify new implementation passes all tests
4. Update integration points

## Commit Message Convention

When replacing modules:
```
refactor: Replace [module_name] with improved implementation

- Removed: old_module.py
- Reason: [cleaner/faster/more maintainable/etc]
- Breaking changes: [list if any]
- Migration guide: [if needed]
```

---

**Last Updated:** 2025-10-17
**Enforcement:** All contributors must follow these rules. Claude Code will enforce during development.
