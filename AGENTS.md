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

## Development Workflow — Claude Plans and Implements, User Approves

The user's decision (2026-10-14): Claude Code does both the planning and the implementation, and the user approves. There is no standing independent reviewer agent. The earlier Codex review rounds (Building B2 design gate) are kept as history in the milestone docs.

This workflow applies to every checkpoint of every milestone. The user can shorten a step for one specific task. That waiver applies to that task only.

### Roles

**Claude Code** is the only agent that writes. Claude:
- audits the source before planning;
- writes the milestone `*_PLAN.md` and `*_PROGRESS.md` files and critiques its own plans;
- implements only the approved checkpoint;
- runs builds, automation tests, cook and Unreal MCP;
- self-reviews and reports before every commit;
- stops before every commit until the user approves it.

**The user** is the final authority. The user:
- approves plans and architecture decisions;
- performs manual rendered PIE acceptance where it is required;
- authorizes each commit, and each push separately;
- authorizes the start of the next checkpoint.

**Other agents** (optional) take part only when the user explicitly brings one in for a specific task. Such an agent is a read-only reviewer. It must not:
- modify files;
- change Git state;
- run builds, tests, cook or Unreal MCP.

Claude resolves its findings as described under "Findings" below.

### Checkpoint workflow

```text
Plan → Plan Self-Critique → User Approval → Implementation → Validation
     → Self-Review → Handoff Report → User Acceptance → Commit
```

1. **Plan.** Audit the source. Write or update the `*_PLAN.md` with:
   - the scope;
   - each decision, with its options and a recommendation;
   - failure paths;
   - the test plan.
2. **Plan self-critique.** Before handing the plan to the user, review it adversarially:
   - verify every source claim against the code (file:line);
   - check authority boundaries and invariants, failure, reentrancy and lifetime paths, test coverage and scope.

   Record the open risks in the plan. Then STOP.
3. **User approval.** The user decides. Claude records every approved decision in the `*_PLAN.md` before implementing.
4. **Implementation.** Implement only the approved checkpoint. Out-of-scope work needs separate approval.
5. **Validation.** Run what the checkpoint requires:
   - a build;
   - focused, regression and full automation;
   - Blueprint compiles;
   - the cook;
   - a Content and World Partition audit.

   Record the evidence in the `*_PROGRESS.md`.
6. **Self-review.**
   - Review the full diff against the plan and the invariants.
   - For native code changes, also run a fresh-context review pass (for example `/code-review`, or a review subagent). That way the second reading is not biased by the implementation context.
   - Resolve every finding (see "Findings") and rerun the relevant tests.
7. **Handoff report.** Stop before committing and give the user the handoff report defined in `CLAUDE.md`. Do not change the working tree while the user reviews, unless the user asks.
8. **User acceptance.** The user performs manual rendered PIE where it is required. Manual observations are never fabricated.
9. **Commit.**
   - Commit only after explicit user approval.
   - A push needs a separate explicit instruction.
   - Never start the next checkpoint automatically.

### Source of truth

- The approved `*_PLAN.md` and `*_PROGRESS.md` files and `NULLTIDE_MASTER_PLAN.md` are the shared source of truth.
- Chat history and model memory are not authoritative project documentation.
- Every approved architecture decision is recorded in the relevant `*_PLAN.md`.
- Validation evidence is recorded in the `*_PROGRESS.md`: commands, pass and fail counts, warnings, and what was not run.

### Findings

These rules cover findings from Claude's self-review, from the fresh-context review pass, from the user, and from any optional reviewer.

Every finding has a severity:
- **P0:** crash, item loss or duplication, broken authority or invariant, or damaged asset or World Partition data. Blocks the commit.
- **P1:** wrong behavior in a reachable case, a missing required test, or a scope violation. Must be resolved before the commit.
- **P2:** an edge case or a maintainability issue. Can be deferred if the user agrees.

Claude classifies every finding as one of:
- **CONFIRMED:** fix it and rerun the relevant tests. If the fix would change an approved architecture decision, treat the finding as NEEDS USER DECISION.
- **REJECTED WITH EVIDENCE:** give precise technical evidence (source lines, tests, runtime results).
- **NEEDS USER DECISION:** describe the options and their trade-offs.

The handoff report lists the findings and how each one was resolved. Claude does not change approved architecture without the user's approval.

### Project safety (unchanged)

The rules above and `NULLTIDE_MASTER_PLAN.md` section 3 still apply:
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

This section records the user's current decision on roles and Git approval. `NULLTIDE_MASTER_PLAN.md` summarizes it in two places:
- section 3 ("Validation" and "Git discipline");
- section 4 ("Agent roles").

If those summaries ever differ from this section, this section applies.
