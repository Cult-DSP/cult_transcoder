# Authoring Validation Tests

This file records manual DAW/tool validation outcomes that are not covered by automated Catch2 tests. Keep this separate from `admAuthoring.md`, which is the stable authoring contract, and mirror durable findings into `AUTHORING.md`.

## Dolby Atmos Conversion Tool Test

- upon import - "Opening an unsupported master. This file is invalid or was not created by Dolby approved software. It may not open or play back correctly."
- earlier conversion attempt displayed: "Conversion output error: The time entered for the FFOA must be in a range between the start time and end time. Enter a new time greater or equal to 00:00:00:00 and less than 00:00:00:00."
- later validation: the FFOA message was not blocking; the Dolby Atmos Conversion Tool successfully converted the file.
- the converted Dolby output opened in Logic.

Interpretation:

- The file can be usable by the Dolby Atmos Conversion Tool while still warning that it is not a Dolby-approved master.
- The warning implies the file is not recognized as created by Dolby-approved software. This may be a provenance/certification warning rather than a structural failure.
- The FFOA message should not be treated as the current blocker because conversion succeeded and the converted output opened in Logic.

Follow-up hypotheses:

- Compare the accepted source BWF and CULT-authored output for Dolby-private `dbmd` content and any timing fields that identify master start/end.
- Inspect whether the conversion tool expects additional Dolby metadata beyond ADM `axml` + `chna`.
- Treat Dolby-approved-master recognition as separate from conversion usability. Current output can convert successfully despite the warning.

## Dolby Candidate: dbmd Copy + ADM End Timing

Implemented experiment:

- `adm-author` now accepts `--dbmd-source <source.wav|dbmd.bin>`.
- If the source is a RIFF/BW64/RF64 WAVE file, CULT extracts its `dbmd` payload and writes it as a post-data `dbmd` chunk in the authored output.
- If the source is not WAVE-like, CULT treats the file as a raw `dbmd` payload.
- Authored ADM XML now writes nonzero `end` attributes alongside existing `start`/`duration` on `audioProgramme` and `audioObject` elements.

Generated candidate:

```bash
build/cult-transcoder package-adm-wav --quiet \
  --in /Users/lucian/projects/spatialroot/sourceData/EDEN-ATMOS-MIX-LFE.wav \
  --out-package exported/eden_dolby_candidate_package \
  --report exported/eden_dolby_candidate_package.report.json

build/cult-transcoder adm-author --quiet \
  --lusid-package exported/eden_dolby_candidate_package \
  --out-xml exported/eden_dolby_candidate.adm.xml \
  --out-wav exported/eden_dolby_candidate.wav \
  --dbmd-source /Users/lucian/projects/spatialroot/sourceData/EDEN-ATMOS-MIX-LFE.wav \
  --report exported/eden_dolby_candidate.report.json
```

Observed local verification:

- `exported/eden_dolby_candidate.report.json` status: `pass`
- `exported/eden_dolby_candidate.wav` contains a `dbmd` chunk.
- `exported/eden_dolby_candidate.adm.xml` has `audioProgramme start="00:00:00.00000" end="00:01:38.00000" duration="00:01:38.00000"`.

Next manual test:

- Track whether future candidates still show the unsupported-master warning.
- Conversion usability is currently validated: the Dolby Atmos Conversion Tool converted the file and the converted output opened in Logic.
