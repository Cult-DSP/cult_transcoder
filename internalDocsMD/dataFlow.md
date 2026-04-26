# CULT Transcoder — ADM ➜ LUSID Data Flow (Left-to-Right)

This diagram describes the ingest/parity-critical `transcode` path from ADM input to LUSID scene output.

```mermaid
flowchart LR
	A[Source ADM Input\n(--in, --in-format)] --> B{Input format?}

	B -->|adm_wav| C[Extract ADM XML (axml)\ntranscoding/adm/adm_reader.cpp]
	B -->|adm_xml| D[Load ADM XML file]

	C --> E[Parse XML with pugixml\n(encounter-order sensitive)]
	D --> E

	E --> F[Resolve ADM profile + LFE mode\ntranscoding/adm/adm_profile_resolver.cpp]
	F --> G{Profile branch}

	G -->|Sony360RA| H[360RA converter\ntranscoding/adm/sony360ra_to_lusid.cpp]
	G -->|Generic/Atmos/Unknown| I[Core ADM->LUSID mapper\ntranscoding/adm/adm_to_lusid.cpp]

	H --> J[Build internal LUSID scene model\nframes + nodes + metadata]
	I --> J

	J --> K[Write scene.lusid.json\n(src/transcoder.cpp)]
	J --> L[Build report (status/warnings/losses)\n(src/report.cpp)]

	K --> M[Output: LUSID Scene JSON]
	L --> N[Output: report.json\n(and optional --stdout-report)]
```

## Object Metadata Handling (ADM ➜ Internal ➜ LUSID)

1. ADM object/channel metadata is read from XML in encounter order.
2. Metadata is mapped into CULT's internal scene representation (`frames`, `nodes`, timing, cart positions, node types).
3. The internal model is then serialized to LUSID JSON.
4. Any profile notes, losses, or warnings are captured in the report path.

## Core invariants in this flow

- Parity path remains separate from export-side `adm-author`.
- Ordering is preserved to match the Python oracle contract.
- `timeUnit` output is seconds for current ingest parity behavior.
- Failures produce a fail-status report with diagnostics.
