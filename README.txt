ÎLOT14: HOLLOW ASHES - base game v0.1.0 (rated 14s)

HOW TO BUILD: put every file from this flat zip in the root of a GitHub repo
(build.yml goes in .github/workflows/build.yml), push, download the artifact.

CONTROLS: stick/D-pad move, A interact/hide, B run (loud), X flashlight, + save & quit.
Auto-save: every 20 seconds, on every door, and on every pickup.

DLC: put .dlc files in sdmc:/switch/ilot14_hollow/dlc/ (the artifact already includes DLC 1 and 2).
UPDATES: a DLC pack's first line is  DLC id|Name|version . Drop a newer file with the same id
and a higher version and it replaces the old one. DLC files can add or replace whole zones
(same text format as zones_base.h), so future updates ship as .dlc files, no rebuild needed.

NSP: set a repo secret PROD_KEYS (base64 of your prod.keys). The NSP step is experimental;
the NRO is always built. If NSP fails, reuse the NSP packing steps from your working
Public Life / Orbitto workflow.
