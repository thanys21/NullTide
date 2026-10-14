# Null Tide

Unreal Engine 5.8 project.

## AI Development Rules

- Inspect existing architecture before modifying it.
- Never modify .uasset files directly.
- Use Unreal MCP for Unreal assets and Blueprints.
- Do not delete or rename Blueprint-exposed C++ APIs without checking references.
- Compile after C++ changes.
- Compile affected Blueprints after Blueprint changes.
- Prefer composition over inheritance for inventory items.
- Static item data belongs in ItemDefinition.
- Runtime item state belongs in ItemInstance.
- Inventory logic belongs in InventoryComponent.

## Multi-Agent Workflow — Claude Implementer / Codex Reviewer

This workflow applies to every checkpoint of every milestone. The user can waive or shorten a review step for one specific task. That waiver applies to that task only.

### Roles

**Claude Code is the implementer and the only agent that writes.** Claude:
- audits the source before planning;
- creates and updates the milestone `*_PLAN.md` and `*_PROGRESS.md` files;
- implements only the approved checkpoint;
- runs builds, automation tests, cook and Unreal MCP;
- self-reviews before asking for an independent review;
- investigates and resolves Codex findings;
- stops before every commit until the user approves it.

**Codex is the independent reviewer and works read-only.** Codex:
- reviews architecture plans before implementation;
- reviews the actual code and diffs after Claude's self-review;
- reviews test coverage, invariants, regressions and scope;
- reports findings with verifiable evidence (see "Review quality contract");
- never modifies code, assets, documentation or Git state, never implements fixes, and never commits or pushes.

**The user is the final authority.** The user:
- approves architecture decisions;
- resolves disagreements between Claude and Codex;
- performs manual rendered PIE acceptance where it is required;
- authorizes each commit, and each push separately;
- authorizes the start of the next checkpoint.

### Write ownership

Only one agent owns writes: Claude. That covers source, tests, documentation, config, Content, maps, World Partition packages and Git state (index, branches, commits).

Codex may inspect:
- source files;
- plans and documentation;
- Git status, logs and diffs (read-only commands such as `status`, `log`, `diff`, `show` and `blame`; prefer `git --no-optional-locks status` so that the index is not refreshed);
- test code and existing validation logs.

Codex must not:
- modify, create or delete files;
- apply patches;
- change Git branches or the index;
- run `checkout`, `reset`, `restore`, `clean`, `stash` or any other command that changes the working tree;
- run builds, automation tests, cook or Unreal MCP, unless the user explicitly asks for it;
- commit or push.

If a review needs runtime or Unreal asset evidence (test results, Blueprint or widget contents, map or World Partition state), Codex asks Claude to obtain it. Codex can use another read-only verification method only with the user's explicit approval.

These restrictions apply only to the Codex reviewer role. They do not prevent Claude from doing approved implementation work.

### Checkpoint workflow

```text
Plan → Independent Plan Review → User Approval → Implementation → Claude Self-Review
     → Independent Code Review → Finding Resolution → User Acceptance → Commit
```

1. **Plan (Claude).** Audit the source. Write or update the `*_PLAN.md` with the scope, design decisions, open options and test plan. Then STOP.
2. **Independent plan review (Codex).** The user sends the plan to Codex. Codex reviews architecture, authority boundaries, invariants, failure paths, the test plan and scope.
3. **User approval.** Claude resolves the plan findings (see "Finding resolution"). The user decides the open points. Claude records every approved decision in the `*_PLAN.md` before implementing.
4. **Implementation (Claude).** Implement only the approved checkpoint. Out-of-scope work needs separate approval.
5. **Claude self-review.** Run the validation the checkpoint requires (build, focused, regression and full automation, cook, Content and World Partition audit). Self-review the diff. Record the evidence in the `*_PROGRESS.md`.
6. **Independent code review (Codex).** Claude stops before committing and gives the review handoff defined in `CLAUDE.md`. Claude does not change the working tree while Codex reviews.
7. **Finding resolution (Claude).** Claude classifies every finding, fixes the confirmed ones, reruns the relevant tests and records the outcome in the `*_PROGRESS.md`. Claude lists any change made after the review in a delta handoff, and the user decides whether Codex re-reviews it.
8. **User acceptance.** The user performs manual rendered PIE where it is required. Manual observations are never fabricated.
9. **Commit.** Claude commits only after explicit user approval. A push needs a separate explicit instruction. Claude never starts the next checkpoint automatically.

### Source of truth

- The reviewed `*_PLAN.md` and `*_PROGRESS.md` files and `NULLTIDE_MASTER_PLAN.md` are the shared source of truth.
- Chat history and model memory are not authoritative project documentation.
- Every approved architecture decision is recorded in the relevant `*_PLAN.md`.
- Validation evidence is recorded in the `*_PROGRESS.md`: commands, pass and fail counts, warnings, and what was not run.
- Review rounds and how each finding was resolved are also recorded in the `*_PROGRESS.md`.

### Review quality contract (Codex)

Every actionable finding includes:
- an ID (`R1`, `R2`, …) and a severity:
  - **P0:** crash, item loss or duplication, broken authority or invariant, or damaged asset or World Partition data. Blocks the commit.
  - **P1:** wrong behavior in a reachable case, a missing required test, or a scope violation. Must be resolved before the commit.
  - **P2:** an edge case or a maintainability issue. Can be deferred if the user agrees.
- a class:
  - **Confirmed defect;**
  - **Plausible risk** (needs verification);
  - **Optional improvement.**
- the file path and line number, when it applies;
- the expected behavior and the actual behavior;
- a concrete failure scenario or reproduction;
- supporting source evidence;
- a recommended correction or validation.

Do not report speculative findings without supporting evidence. If there are no findings, say so explicitly and list what the review covered.

### Finding resolution and conflicts (Claude)

Claude classifies every finding as one of:
- **CONFIRMED:** fix it and rerun the relevant tests. If the fix would change an approved architecture decision, treat the finding as NEEDS USER DECISION.
- **REJECTED WITH EVIDENCE:** give precise technical evidence (source lines, tests, runtime results).
- **NEEDS USER DECISION:** describe the options and their trade-offs.

Claude does not blindly implement every reviewer suggestion.

When Claude disagrees with Codex:
- explain the technical disagreement;
- reference the actual source and tests, with evidence;
- do not change the architecture without approval;
- escalate unresolved material disagreements to the user.

The user has final authority.

### Project safety (unchanged)

The rules above and `NULLTIDE_MASTER_PLAN.md` section 3 still apply to every agent:
- Never edit `.uasset` files directly. Authorized asset changes go through the Unreal Editor or Unreal MCP.
- Never use `Save All` unless the user explicitly asks for it. Save only the assets the task intentionally changed.
- Protect World Partition external actor and object packages (`__ExternalActors__`, `__ExternalObjects__`):
  - never delete them manually;
  - audit them in every checkpoint.
- Use clean builds with the editor closed when the change is structural or the editor locks the DLL.
- Preserve the authority boundaries:
  - **Inventory:** `UInventoryComponent` is the only mutator.
  - **Crafting:** `UCraftingComponent`.
  - **Storage:** `UStorageTransferLibrary`.
  - **Building:**
    - `UBuildingSubsystem` is the only writer of the occupancy registry;
    - construction cost goes only through `TryApplyDefinitionTransaction`.

### Precedence

This section records the user's current decision on agent roles and Git approval. `NULLTIDE_MASTER_PLAN.md` summarizes it in two places:
- section 3 ("Validation" and "Git discipline");
- section 4 ("Multi-agent rule").

If those summaries ever differ from this section, this section applies.
