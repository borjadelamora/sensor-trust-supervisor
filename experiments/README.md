# Experiment ledger

One folder per run, numbered. Every number I quote in the report carries its experiment id, and
every run is reproducible from its manifest: date, commit, seed, scenario, parameters.

## Layout

```
experiments/
├── templates/                   copy these into a new run folder
└── EXP-YYYYMMDD-NNN-slug/
    ├── manifest.md              what was run, filled in before running it
    ├── results.csv              raw numbers
    └── FINDINGS.md              what it showed
```

## Conventions

- Id is `EXP-YYYYMMDD-NNN-slug`, NNN restarting each day.
- The manifest is written before the run. Once attack scenarios exist they are written down
  first as well, so a threshold or a metric is fixed before any results are visible.
- `results.csv` stays raw. Anything plotted in the report regenerates from it.
- Baseline runs are experiments as well, with their own ids, so comparisons are like for like.
