---
name: todo-runner
description: >-
  Automates the sequential execution, verification, and logging of fixes and refinements from TODO.md in the Eatsbits project. Use when the user asks to work through the todo list, run autonomous refinements, or when executing via /goal.
---

# Eatsbits Todo Runner Skill

This skill guides the autonomous refinement loop for Eatsbits. It processes items from [TODO.md](file:///c:/git/eatsbits/TODO.md) one by one, enforces architectural and audio thread rules, validates with automated builds and tests, and tracks completion.

## Core Rules & Guardrails
Before modifying any code, ensure adherence to the project standards:
1. **Audio Core Zero-Allocation**: Never introduce heap allocations (`new`, `malloc`, `std::vector` resizing, dynamic formatting) in the real-time audio callback path (`src/audio/`, DSP nodes, or audio process loops).
2. **Modular Subsystem Pattern**: Never inline monolithic rendering or event logic into `gui_window.cpp`. Modular view logic belongs in `src/ui/views/` (`ArrangerView`, `EditView`, `MixerView`, `DesignView`) or `src/ui/widgets/`.
3. **Theming**: Use `ThemeTokens` (e.g., `theme.primaryAccent`, `controlBackground`) rather than hardcoded magic color values.
4. **Touch & Multi-Pointer**: Maintain pointer-type awareness (`PointerType::Mouse`, `PointerType::Touch`) where applicable.

---

## The Execution Loop

Repeat the following procedure for each uncompleted task in [TODO.md](file:///c:/git/eatsbits/TODO.md):

### Step 1: Select the Next Task
- Scan [TODO.md](file:///c:/git/eatsbits/TODO.md) for the topmost unchecked item (`- [ ]`).
- If no unchecked items remain, stop and report completion.
- State clearly which task is being addressed.

### Step 2: Understand Context & Plan
- Inspect relevant architectural references:
  - [engine-blueprint.md](file:///c:/git/eatsbits/engine-blueprint.md)
  - [progress.md](file:///c:/git/eatsbits/progress.md)
- Search for the existing implementations and usages using grep or file view.
- Determine the minimal, cleanest set of file edits needed.

### Step 3: Implement Code Changes
- Modify the necessary header and source files.
- Maintain existing comments and docstrings.
- Ensure new or modified functionality includes or updates unit tests in `tests/` if applicable.

### Step 4: Build and Test Verification
- Run the build and test suite via PowerShell:
  ```powershell
  .\build.ps1 -Test
  ```
- Alternatively, test specific suites:
  ```powershell
  ctest --test-dir build --output-on-failure -C Release
  ```
- **Verification Gate**:
  - If the build errors or any test fails, analyze the root cause and fix it immediately.
  - Do NOT commit or mark a task as complete if the build or tests fail.

### Step 5: Git Commit
- Once the build succeeds and tests pass, stage and commit the changes:
  ```powershell
  git add -A
  git commit -m "feat(scope): concise description of task"
  ```
- Keep commits atomic and focused strictly on the single task.

### Step 6: Update Documentation & Backlog
- Update [TODO.md](file:///c:/git/eatsbits/TODO.md):
  - Change `- [ ]` to `- [x]`.
  - Add the short commit hash and date/time.
  - Move the completed task under `## 🕒 Recent Completions (Reference Context)`.
  - **Auto-Archive Maintenance**: Check the count of completed `- [x]` items in [TODO.md](file:///c:/git/eatsbits/TODO.md). If there are more than 5 completed items, move the oldest completed items to the top of [TODO-ARCHIVE.md](file:///c:/git/eatsbits/TODO-ARCHIVE.md), keeping strictly the 5 most recent completed items in [TODO.md](file:///c:/git/eatsbits/TODO.md) for active reference context.
- Append a concise entry to [progress.md](file:///c:/git/eatsbits/progress.md) documenting what was changed and verified, keeping the progress log updated.

### Step 7: Proceed to Next Item
- Check for the next unchecked item in [TODO.md](file:///c:/git/eatsbits/TODO.md) and repeat until the queue is exhausted or user intervention is required.
