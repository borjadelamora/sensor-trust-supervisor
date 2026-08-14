# ADR-0006: Attack strength and the relayed range

## Status
Accepted, 14/08/2026. Supersedes parts of [ADR-0004](0004-injector-at-topic-level.md) and
[ADR-0005](0005-attack-profiles-and-episodes.md).

## Context
Flying the battery at d20f48b and measuring the bags found four defects, two of them in claims these records make.

`intensity` set the level a frame is flooded to, so 0.0 meant black and 1.0 meant white. Every camera scenario used 0.0, the one value where the name and the behaviour agree. Any camera episode also zeroed the optical flow outright, so a blackout ramping over ten seconds killed the flow at its first message while the frame sharing its lens was still untouched.

In ADR-0005 it was claimed that raw flow is corrupted by scaling `distance_m` so that it agrees with the derived velocity, when it isn't the case. `distance_m` is 0.0 with `distance_available` false on all 2474 raw messages of ATK-004 and not one of them changed. The range px4 scales flow by arrives on `distance_sensor`, which ADR-0004 kept out of the relay, leaving `vel_*` as the only corrupted field on the channel.

The linear rise eased on and not off, ending ATK-002's 1.0000 m/s spoofed velocity between two fixes. And the camera monitor is pre-registered as a saturation check, mean pixel above 240, against frames whose mean is 0.

## Decision
Intensity is strength: the attack names the level it floods toward and intensity says how far toward it the attack ever gets, along its profile. `camera_blackout` heads for black and `camera_dazzle` for saturation, both the denial class of the threat model, exclusive because a frame carries one brightness field. A freeze is the spoofing class and keeps its quality, since a replayed still scene measures a world that is not moving.

The lens passes through whatever fraction of the frame the flood leaves. That fraction is what the flow sensor still measures and the quality it reports falls with it. px4 takes the gyro out of whatever the sensor measured, so the compensated rate is recomputed from the attenuated measurement.

`distance_sensor` is relayed and its range carries the flow attack. ADR-0004 kept it out so that a monitor could not reconstruct the true velocity from raw flow and range, seeing through the attack for reasons that say nothing about cross-modal arbitration. Corrupting the range by the same factor holds that closed: the reconstruction returns the corrupted velocity and the channel agrees across all four of its topics.

The linear rise mirrors its ease at the top, so a spoofer hands back as carefully as it took over. An intensity of 0 and a scale an overshooting profile would carry through zero are both refused at load, since every attack must have an effect and a reported range is positive.

## Consequences
ATK-005 and ATK-006 keep the attack they always described, with intensity 1.0 the blackout 0.0 used to be. ATK-010 and ATK-011 give the oscillating profile, the camera freeze and the dazzle their first flights. Halving every `ease_s` holds the plateau rates at their pre-registered values, 1.000 m/s over 47.00 m on ATK-002 and 0.300 m/s over 14.00 m on ATK-008. The onsets did not move, and every earlier run is superseded on behaviour.

The relay logs both edges of every window on every mirrored topic, so a second episode and a lens-driven flow corruption each leave a delivered onset. Gazebo stamps frames in a different epoch and rate to px4, 12.26 s of frame time against 14.98 s in ATK-005, so a frame time compares only with another frame time and every other channel counts from the relay's origin.

Four simplifications stand. The flood is spatially uniform; a real dazzle is a bright blob with bloom. Transmission falls linearly with obscuration; real flow holds up until contrast drops past a threshold and then collapses. Under a lens attack the relayed velocity and the compensated rate agree exactly only when the vehicle is not rotating, and a driver reporting quality 0 would have stopped publishing velocity. A channel carries one attack at a time, so a dimmed replay is not expressible.
