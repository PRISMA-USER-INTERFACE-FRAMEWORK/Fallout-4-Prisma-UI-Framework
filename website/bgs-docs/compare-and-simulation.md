---
title: Compare & Simulation
---

# Compare & Simulation

The **Compare** and graph simulation tools are two of the best ways to understand what an edit actually changed before launching Fallout 4.

Use **Compare** to review file differences. Use **Simulation** to exercise state-machine logic inside the open graph.

## Compare two behaviour files

Open the behaviour you are actively working on, then use the **Compare** tab to load another behaviour as the reference.

The comparison view identifies added, removed, and changed objects and shows information such as:

- Change type.
- Havok class.
- Field or object name.
- Value in the open file.
- Value in the comparison file.

This is useful for both development and reverse engineering.

## Compare against your original before release

A simple release workflow is:

1. Keep an untouched copy of the original behaviour.
2. Edit the working copy.
3. Validate and save it.
4. Open the edited file.
5. Compare it against the untouched original.
6. Review every reported difference.

You should be able to explain why each difference exists.

If a field changed that you never intended to touch, investigate it before releasing the file.

### Tip: compare after each major stage

If a large task involves three separate changes, compare after each one instead of only at the end. This makes unexpected drift much easier to trace.

## Comparing two mods

Compare is also useful when two mods change the same behaviour file.

You can use it to answer:

- Did both mods edit the same state?
- Did one mod add an object the other does not have?
- Are both changing the same clip timing?
- Did one version rename or remove a symbol?
- Is a conflict structural or only a field-value difference?

The comparison does not automatically merge two mods, but it gives you a much better view of what a manual merge would need to preserve.

## Graph simulation

The Graph view includes simulation controls for exercising supported state-machine logic without starting the game.

Simulation can track active states, transition timing, blend weights, events, variable values, and reasons a transition did not advance.

The simulation area includes tools for:

- Choosing and firing an event.
- Setting a variable value.
- Stepping time forward.
- Viewing currently active states and weights.
- Reading the event and transition log.
- Seeing transitions that are held back by conditions.
- Seeing stops or blockers that prevent progression.

## Starting with a known state

Before firing events randomly, identify the part of the graph you want to test.

A good process is:

1. Find the state machine in Graph view.
2. Highlight the relevant path.
3. Identify the starting state.
4. Identify the event or variable that should move it forward.
5. Start or reset the simulation as needed.
6. Apply the trigger.
7. Step time forward.
8. Read the log.

This makes the simulation a focused test rather than a stream of unrelated state changes.

## Firing an event

For event-driven transitions:

1. Select the relevant event from the simulation controls.
2. Fire it.
3. Use **Step 0.1s** to advance the graph.
4. Watch the active state list.
5. Read the transition log.

If the transition does not fire, inspect the **Held back** and **Stops** information.

The event may be correct while a timing gate, condition, or other state requirement is preventing the transition.

## Setting a variable

For variable-driven conditions:

1. Select the variable.
2. Enter a value.
3. Use **Set variable**.
4. Step the graph.
5. Observe whether the condition changes and whether the intended transition becomes available.

Use the **Symbols** tab first if you are unsure about the variable's type, initial value, or bounds.

## Understanding Held back

A transition shown as held back is useful information. It means the graph knows about the transition but a condition is not currently satisfied.

Check:

- Variable values.
- Event state.
- Transition conditions.
- Timing requirements.
- The currently active source state.

Do not immediately rewrite the transition. First determine whether the simulator is correctly showing that your trigger conditions have not been met.

## Understanding Stops

A stop or blocker indicates that simulation could not progress through a path for a specific reason.

Use the explanation to locate the relevant state, transition, or unsupported condition. A stop can reveal a problem in the graph, but it can also indicate that part of the runtime behavior depends on game-side information the standalone simulator does not have.

## Simulation is not the game runtime

The simulator is designed to help reason about graph logic. Fallout 4 can supply external events, variables, native state, animation-system data, and script behavior that the standalone editor does not reproduce completely.

Treat simulation as an early test for graph logic, not as a replacement for in-game verification.

## Debugging a transition that never fires

Use this sequence:

1. Find the transition in Graph view.
2. Confirm the source and destination states.
3. Inspect the event or condition.
4. Open **Symbols** and confirm the referenced symbol.
5. Start simulation from the relevant state.
6. Fire the event or set the variable.
7. Step time forward.
8. Read **Held back**, **Stops**, and the transition log.
9. Fix the graph if the simulator reveals a real logic problem.
10. Run **Check graph**.
11. Save and test the same path in Fallout 4.

## Debugging an unexpected edit

If the graph works differently after a change but you cannot see why:

1. Compare the edited file against the original.
2. Find every changed object related to the affected state machine.
3. Select those objects in Graph view.
4. Highlight their paths.
5. Simulate the relevant event or variable change.
6. Undo or correct any unintended difference.

For save checks and release preparation, continue with [Saving & Validating](saving-and-validating).
