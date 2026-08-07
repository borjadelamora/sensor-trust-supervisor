# ADR-0003: Textured simulation world for optical flow

## Status
Accepted, 07/08/2026

## Context
Optical flow is one of the three velocity channels the attribution idea rests on, so before writing any monitor I flew the square mission on the `x500_flow` airframe to see what the sensor reports. On the default world it reports nothing usable: flow speed comes out at 0.12 of true speed, correlation with true speed is 0.07, and `pixel_flow` is exactly zero in 98% of samples.

Using `default.sdf` was the issue. It is a uniform grey plane, so the feature tracker has nothing to match and says so: "Number of good matches: 2, desired: 18". The identical mission in `baylands.sdf`, which has real terrain texture, tracks true speed with a slope of 0.997 and a median disagreement of 0.16 m/s.

This decides whether the monitor works at all. The flow rule from the research is a residual above 1.0 m/s sustained beyond 2.0 s. On the default world a *benign* flight sits above that threshold for 8.7 s continuously, so the monitor would accuse flow on every clean flight. On baylands the longest excursion is 0.64 s, inside the debounce, with the 95th percentile at 0.81 m/s.

## Decision
All flights use `PX4_GZ_WORLD=baylands` with the `x500_flow` airframe, set in `tools/record_flight.sh` rather than left to whoever runs it. The benign dataset was re-flown on this configuration, since the earlier twenty runs used an airframe with no flow sensor, in a world where flow does not work.

## Consequences
Baylands is a Fuel model, so a first run downloads about 400 MB and SETUP.md has to say so. The world sets its own origin, so the seeded spawn now varies yaw only and lets the world place the vehicle. Real-time factor is unchanged: the same mission takes the same 60.7 s in both worlds.

Tuning the flow thresholds to baylands' textures limits how far those thresholds generalise to other ground, which is a limitation for the report.
