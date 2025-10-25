# Production Winners Index

This directory tracks all validated optimization winners ready for live deployment.

## Current Production Config (For Monday 2025-10-27)

**Directory:** `2000trial_2025-10-25_055133`
- **Performance:** +0.384% eval, +0.327% val, 14.8% degradation
- **Trial:** 868 out of 2000
- **Pass Rate:** 80.5%

## Winner History

| Date Generated | Directory | Eval MRD | Val MRD | Degradation | Status |
|----------------|-----------|----------|---------|-------------|--------|
| 2025-10-25 05:51 | 2000trial_2025-10-25_055133 | +0.384% | +0.327% | +14.8% | ✅ ACTIVE |

## Deployment Checklist

Before deploying a new winner:

1. ✅ Review performance metrics in winner's README.md
2. ✅ Copy winner's sigor_params.json to config/sigor_params.json
3. ✅ Rebuild binary: `cmake --build build -j`
4. ✅ Test in mock mode: `./build/sentio_lite mock --date 10-24 --strategy sigor`
5. ✅ Launch live: `./scripts/launch_sigor_live.sh`

## Notes

- Each winner directory is self-contained with all necessary files
- Always keep at least 3 most recent winners for rollback capability
- Update this INDEX.md when adding new winners
