# ADR-0007: Uncapped accusations and the all-untrusted fail-safe

## Status
Accepted, 27/08/2026.

## Context
In deciding how the arbiter works with the monitors, there are many assumptions to consider. One of them was how many sensors may be marked as untrusted at once. Additionally, I brainstormed the question of how the trust levels are consumed by the behaviour tree alongside estimates and sensor readings. When considering capping how many sensors could be marked as untrusted, I had to choose whether to limit the count so that trusted sensors always outnumber untrusted ones, or to diagnose without a cap. I also considered the course of action if all sensors are untrustworthy: continue based on estimates solely, or trigger an instantaneous fail-safe?

## Decision
No cap on how many sensors the vote can accuse (up to three), rather than at most one. I believe it is more important to identify and label which sensors cannot be trusted, even if there is a possibility that a single-sensor attack causes the other two to be marked as untrustworthy. Additionally, an automatic fail-safe when all sensors are untrusted is the chosen behaviour, running under the assumption that a safe landing can be performed immediately and that safety takes priority over reaching the goal on estimates alone. The behaviour tree consumes the trust levels alone; how it acts on each combination is decided when the tree is built.

## Consequences
As a result, the number of false alarms could potentially increase as noise and unpredictable circumstances may cause a false positive to deem a sensor unreliable. Moreover, a sophisticated single-sensor attack could gaslight the system into triggering the other two sensors to be marked as untrustworthy.

Still, the system will now try to prioritise detecting which sensors have been attacked, focusing on detection rather than blindly listening to compromised sensors. It also allows the architecture to be strengthened later with more estimates and back-up metrics. Finally, when all sensors are untrusted, the drone will immediately land without reaching the target destination.
