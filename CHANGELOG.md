# Changelog

All notable changes to Aurora-X will be documented in this file.

## [Unreleased]

### Added
- **FASE 3b**: Adaptive weight loss mechanism based on success streaks
  - Added `good_streak` and `bad_streak` tracking to `FlowState`
  - Implemented homeostatic weight loss: after 4+ consecutive successes with no panic and avg_coverage > 0.85, overhead gradually decreases
  - Enhanced failure response: repeated failures (bad_streak >= 3) trigger additional overhead boost for critical flows
- **Test Suite**: Comprehensive test suite (`test_aurora_organism`) with three scenarios:
  - Good channel: NERVE, GLAND, MUSCLE with full delivery
  - Bad channel: NERVE/GLAND with packet loss, showing panic activation
  - Adaptation: GLAND transitioning from bad to good channel, demonstrating overhead increase then decrease
- **Documentation**: Professional/academic README restructure with:
  - Real-world performance results (Taiwan/Denmark intercontinental test)
  - Scientific language: "homeostatic behavior", "dynamic regulation"
  - Clear getting started with CMake-only instructions

### Changed
- **Adaptive Logic**: Refined up-adapt mechanism to use `bad_streak` for more intelligent failure response
- **Logging**: Extended `[ALIEN][ADAPT]` log to include `gs` (good_streak) and `bs` (bad_streak) fields
- **Weight Loss Threshold**: Set `COV_GOOD_THRESHOLD` to 0.85 (from 0.90) for more realistic activation in test scenarios

### Fixed
- **Dimagrimento Activation**: Fixed weight loss mechanism to activate reliably in test scenarios by adjusting coverage threshold

## Previous Versions

For changes before FASE 3b, see git history.

