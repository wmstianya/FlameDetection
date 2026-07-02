---
name: work-assignment
description: >-
  Split, synchronize, and verify STM32 FlameDetection firmware work across
  multiple agents or contributors (分工). Use when a task must be divided by
  firmware module, synced from another source (issue, PR, branch, commit, or a
  teammate note), or handed off for review, while keeping module ownership
  clear and STM32 project boundaries intact.
---

# Work Assignment (分工) Skill

## Purpose

Use this skill whenever a task needs to be **split across firmware modules**,
**synchronized from another owner or source**, or **handed off for review**.
The goals are:

- Keep module ownership unambiguous so two agents never edit the same file for
  different reasons.
- Preserve STM32 project boundaries (`application` / `drivers` / `utils` /
  `config` / build) and existing safety behavior.
- Prevent unrelated refactors or vendor-library churn from leaking into a
  focused change.

## When To Use

- A single request touches several modules and should be divided among agents.
- Work must be *synced* from an external source such as issue **#N**, a PR, a
  branch, a commit, or a teammate's written note.
- A change is being handed off and needs a clear ownership + verification record.

## Inputs To Collect

Record these in the working notes before editing:

- **Sync source**: the exact upstream reference (issue/PR number, branch,
  commit SHA, or teammate note). If it cannot be located or accessed, state
  that explicitly instead of guessing its contents.
- **Target branch** and **base branch**.
- **Affected firmware area**: application, drivers, utils, config, integration
  (`main`), startup, Keil build project, or documentation.
- **Hardware assumptions**: MCU model, peripheral pins, watchdog behavior, and
  timing constraints.
- **Verification expectations**: Keil build, static review, unit tests, or
  hardware smoke tests.

## Module Ownership Map (this repo)

Base the split on the real layout of `FlameDetection` (STM32G070CBTx):

| Area | Owner Responsibility | Files |
| --- | --- | --- |
| Application | Flame state machine, hysteresis, startup baseline, alarm/output policy | `Src/application/flameDetector.c`, `Src/application/flameDetector.h` |
| Drivers | ADC polling + DMA pipeline, display bus, HAL MSP, interrupt handlers | `Src/drivers/adcDriver.c/.h`, `Src/drivers/adcDriverDma.c/.h`, `Src/adc.c/.h`, `Src/tm1650.c/.h`, `Src/stm32g0xx_hal_msp.c`, `Src/stm32g0xx_it.c` |
| Config | Tunable thresholds, DMA mode switch, timing constants, feature flags | `Src/config/systemConfig.c/.h` |
| Utilities | Filters, conversions, reusable calculations | `Src/utils/filter.c/.h` |
| Integration | Main loop scheduling, init order, watchdog refresh | `Src/main.c`, `Inc/main.h` |
| Build & docs | Keil project membership, startup, usage/release notes, specs | `MDK-ARM/FlameDetection.uvprojx`, `MDK-ARM/startup_stm32g070xx.s`, `README.md`, `openspec/*`, `docs/*` |
| Vendor (do not edit) | ST HAL / CMSIS library | `Drivers/*` (change only if the task explicitly requires it) |

## Sync Procedure

1. Identify the exact upstream source and record it. If the requested source
   (for example a numeric reference such as an issue/PR number) cannot be
   resolved in the repo, PRs, issues, branches, tags, or commits, report it as
   **unavailable** rather than fabricating its content.
2. Compare only the relevant project areas first; exclude `Drivers/*` vendor
   churn unless the task explicitly requires it.
3. Map each upstream change to exactly one responsibility area before editing.
4. Apply changes in small, logical commits. Keep generated Keil project-file
   edits (`*.uvprojx` / `*.uvoptx`) separate from source edits when practical.
5. Preserve existing STM32 safety behavior (watchdog refresh, timeouts,
   startup baseline) unless the source explicitly changes the requirement.
6. Verify compile-sensitive changes: include paths, function prototypes vs.
   implementations, and Keil project membership for any added/removed file.
7. Document any hardware-only verification that cannot run in the cloud
   environment.

## Embedded Coding Standards (must hold for every change)

These mirror the project's firmware standards and must be enforced on all
synced or split work:

- **Naming**: functions and variables in `camelCase`; **no numeric suffixes**
  in names (use `uartInit` / `uartMainInit`, never `uart1Init`). Constants and
  configuration macros in `UPPER_SNAKE_CASE` (e.g. `BUFFER_SIZE`,
  `FLAME_THRESH_LOW_MV`). Names must be self-explanatory; avoid abbreviations.
- **Structure**: peripheral code in `drivers`, utilities in `utils`,
  configuration in `config`, business logic in `application`. Keep headers and
  sources separated (`.h` / `.c`), and use include guards in every header.
- **Functions**: keep each function short (target ≤ 20 lines); split complex
  logic into helpers. Document parameters and return values for every public
  API, and add a module-level comment (purpose, author, date, revision).
- **Constants**: no magic numbers — declare them as macros or `const`.
- **Globals**: `static` unless cross-file access is genuinely required.
- **Safety**: no unbounded wait loops — every polling path must have a timeout
  or a watchdog-refresh policy; every `while(1)` needs an exit condition or
  watchdog. Prefer struct-encapsulated data over raw pointer arithmetic.
- **Tests**: key functions should have unit tests (Unity / Ceedling / CMock).

## Verification

- **Build**: the primary target is the Keil project
  `MDK-ARM/FlameDetection.uvprojx`. If Keil/MDK-ARM is unavailable in the
  current environment, perform a static review (prototypes, includes, project
  membership) and record the build as a hardware/tooling gap.
- **Unit tests**: run or add tests for changed utility/application logic where
  a host-side harness exists.
- **Hardware smoke test** (record if it cannot be executed here): flame present
  vs. absent, hysteresis boundary near `FLAME_THRESH_LOW_MV`/`HIGH_MV`, and
  high-temperature VREFINT compensation.

## Handoff Checklist

- Sync source is identified, or explicitly reported as unavailable.
- File ownership is clear for every changed module.
- Public headers match their source implementations.
- The Keil project references any added or removed source files.
- Verification result and any remaining hardware-test gap are recorded.
- The PR summary states what was synced and what was intentionally left out
  (e.g. vendor `Drivers/*` churn).
