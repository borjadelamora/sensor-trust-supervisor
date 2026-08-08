# Experiment ledger

One folder per run, numbered. Every number I quote in the report carries its experiment id, and
every run is reproducible from its manifest: date, commit, seed, scenario, parameters.

## Layout

```
experiments/
├── templates/                   copy these into a new run folder
└── EXP-YYYYMMDD-NNN-slug/
    ├── manifest.md              what was flown, and with which commit and seed
    ├── results.csv              raw numbers
    └── FINDINGS.md              what it showed
```

## How a run gets here

1. Write the implementation and the unit tests that cover it.
2. Run the unit tests, build and fly it as often as needed. These are debug flights, so the
   bags get overwritten and stay out of the ledger. Back to 1 whenever something breaks.
3. Once it stops moving, commit the implementation, then the unit tests.
4. Fly it once more from that commit and keep this one. Same flight as the debug runs, with the
   bag and the numbers stored.
5. Write the entry and commit it.

The commit comes before the kept flight because `record_flight.sh` stamps whatever `HEAD` is
into the bag's `manifest.txt`. With the code already in, that stamp is the code that flew, and
the manifest here just copies it.

A kept run is reported as it came out, including the ones that disappoint.

## Conventions

- Id is `EXP-YYYYMMDD-NNN-slug`, NNN restarting each day.
- The goal, the attack and the parameters go in the manifest before the kept flight. The commit
  and the numbers go in after it. Once attack scenarios exist they are written down first as
  well, so a threshold or a metric is fixed before any results are visible.
- `results.csv` stays raw. Anything plotted in the report regenerates from it.
- Baseline runs are experiments as well, with their own ids, so comparisons are like for like.
