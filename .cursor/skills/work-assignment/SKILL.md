---
name: work-assignment
description: Use this skill to split, sync, and verify embedded firmware work across agents or contributors.
---

# Work Assignment Skill

## Purpose

Use this skill when a task needs to be split across firmware modules, synchronized from another owner, or handed off for review. The goal is to keep ownership clear, preserve STM32 project boundaries, and prevent unrelated refactors from entering the same change.

## Inputs To Collect

- Requested source to sync from, such as an issue, PR, branch, commit, or teammate note.
- Target branch and base branch.
- Affected firmware area: application, drivers, utils, config, startup, build project, or documentation.
- Hardware assumptions, including MCU model, peripheral pins, watchdog behavior, and timing constraints.
- Verification expectations, such as Keil build, static review, unit tests, or hardware smoke tests.

## Work Split

| Area | Owner Responsibility | Typical Files |
| --- | --- | --- |
| Application logic | State machines, safety behavior, startup rules, alarm policy | `Src/application/*`, feature-specific modules |
| Drivers | HAL/LL peripheral access, DMA, ADC, GPIO, display bus timing | `Src/drivers/*`, peripheral source files |
| Config | Tunable thresholds, timing constants, feature switches | `Src/config/*` |
| Utilities | Filters, conversions, reusable calculations | `Src/utils/*` |
| Integration | Main loop scheduling, watchdog refresh, init sequence | `Src/main.c`, `Inc/main.h` |
| Build and docs | Keil project membership, usage notes, release notes | `MDK-ARM/*`, `docs/*`, `README.md` |

## Sync Procedure

1. Identify the exact upstream source and record it in the working notes.
2. Compare only relevant project areas first; exclude vendor library churn unless the task explicitly requires it.
3. Map each upstream change to one responsibility area before editing.
4. Apply changes in small logical commits, keeping generated project-file changes separate when practical.
5. Preserve existing STM32 safety behavior unless the upstream source explicitly changes the requirement.
6. Verify compile-sensitive changes by checking include paths, function prototypes, and Keil project membership.
7. Document any hardware-only verification that cannot be executed in the cloud environment.

## Embedded Coding Guardrails

- Keep C APIs in camelCase and avoid numeric suffixes in new names.
- Put peripheral code in `drivers`, utility code in `utils`, configuration in `config`, and business logic in `application`.
- Replace magic numbers with named macros or existing configuration constants.
- Keep new functions short and single-purpose; split complex logic into helpers.
- Use static globals unless cross-file access is required.
- Include parameter and return value documentation for public APIs.
- Avoid unbounded wait loops; every polling path must have a timeout or watchdog refresh policy.

## Handoff Checklist

- Source of synchronization is identified or explicitly reported as unavailable.
- File ownership is clear for every changed module.
- Public headers match source implementations.
- Build project references any added or removed source files.
- Verification result and remaining hardware test gap are recorded.
- PR summary states what was synced and what was intentionally left out.
