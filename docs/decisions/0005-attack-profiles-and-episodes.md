# ADR-0005: Attack profiles and episodes

## Status
Accepted, 13/08/2026. Supersedes parts of [ADR-0004](0004-injector-at-topic-level.md).

## Context
After analysing my implementation of the injector, concretely whether the attacks it produces are worth detecting, I decided to revisit how attack profiles work and how an attack is timed.

Every attack had one rate of change fixed in its own maths, so the battery has a single character and a threshold tuned against any of them transfers to all of them. An attack also never ended: only the camera took a duration, and it took it as an attack parameter rather than as timing, so no channel could be attacked twice and nothing reached the arbiter's recovery to trusted. Two claims about flow were also wrong. ADR-0004 leaves `distance_sensor` out because raw flow and range give back the true velocity, but `sensor_optical_flow` is relayed uncorrupted and carries `distance_m` itself. And `optical_flow/model.sdf` puts `flow_camera` and the flow plugin on one `flow_link`, sharing a lens, while ATK-006 assumes flow survives the camera going.

## Decision
Magnitude and shape are separate. The attack says what it corrupts and by how much, a profile says how it gets there, as a fraction of that magnitude and the rate that fraction is changing. GPS takes the fraction for position and the rate for velocity, flow interpolates its scale, the camera interpolates frame brightness.

Four shapes, so the battery is not one curve repeated. A step is the asynchronous spoof. A linear rise with an ease is the synchronous one, and is what the drift already did. A sigmoid moves slowly while a residual would notice and quickly once it has settled. An oscillating rise crosses a threshold repeatedly instead of once. Same offset, four signatures, and a detector tuned to one has to be shown against the rest.

All four are closed forms in elapsed time. EXP-20260812-004 found stamp bursts reaching 6.13 s and one seed delivering different spacings run to run, so a corruption built up over whichever messages arrived would not repeat. That rules out the shape I wanted most, a rate tied to the vehicle's own speed.

Jump and drift become one attack differing only in profile, since the derivative of a step is zero either side of it. A channel carries one or more episodes, each with an onset, jitter and duration, and no duration runs to the end. Raw flow is corrupted by scaling `distance_m`, so it agrees with the derived velocity. Camera and flow are one device, so a blackout or freeze zeroes the flow too, and only the blackout drops `quality`, because a frozen scene still reads as a good measurement of a world that is not moving.

The spoof stays horizontal and leaves altitude alone. A vertical component is what a barometer catches, and there is no barometer monitor in v1.

## Consequences
ATK-007 attacks GPS and flow at once, so two of the three velocity estimates lie together and the arbiter has no majority to vote on.

ATK-005 and ATK-006 behave differently and get re-flown. EXP-20260812-002, -003 and -004 describe a relay whose flow stayed clean through a camera attack, and their numbers stand for the code they name.

The onsets did not move: each channel still draws once per episode in the order gps, flow, camera, so ATK-004 plans 21.802 s and ATK-006 plans 20.047 s and 24.577 s as recorded. A second episode shifts the channels after it. The seed still reaches nothing else, so twenty seeds are twenty onsets rather than twenty attacks.

None of this closes the open loop. PX4 never fuses the spoof, so detection latency and attribution are measurable while trajectory error, mission outcome against PX4's own failsafe and covert spoofing are not. That is a limitation for the report, not future work.
