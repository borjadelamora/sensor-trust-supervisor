# ADR-0002: Development environment and PX4 version pin

## Status
Accepted, 27/07/2026

## Context
This decision was triggered by the choice of dev environment (18 Jul) and a PX4/px4_msgs version mismatch (22 Jul). Native Linux dual-boot and a cloud VM were briefly considered, but WSL2 on Windows was the final choice. WSL2 provides a proper native solution without any x86 emulation, and a working GPU for the camera work later on. The mismatch surfaced while chasing a different symptom—`ros2 topic hz` returning zero samples, which turned out to be a QoS limitation in the tool. Digging further showed PX4 and px4_msgs were pinned to different commits, a mismatch that fails silently.

## Decision
Moved development to the Windows laptop, using WSL2 for native x86 and GPU access. Pinned PX4 and px4_msgs to the matching `v1.17.0` tag, which is the current stable release, while `v1.16.2` was kept as a fallback. Additionally, `v1.17.0` was the release nearest to the exact build I have already tested the system worked with (the old main checkout), so switching to it changed the least and carried the least risk of breaking the system. The `release/1.17` branch wasn't used, since a tag can't move, so a clone months from now gets identical code.

## Consequences
The pin forced a full re-verification: hover, topics and rates were re-run to confirm nothing broke. tools/SETUP.md now has to name these exact tags instead of 'latest stable'.
