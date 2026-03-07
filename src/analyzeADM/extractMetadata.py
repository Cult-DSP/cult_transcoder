import subprocess
from pathlib import Path


# ---------------------------------------------------------------------------
# Locate the cult-transcoder binary (for ADM extraction).
# Lives at: src/cult_transcoder/build/cult_transcoder
# relative to the project root (two levels up from this file's directory).
# ---------------------------------------------------------------------------
_PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
_CULT_TRANSCODER = _PROJECT_ROOT / "src" / "cult_transcoder" / "build" / "cult_transcoder"


def extractMetaData(wavPath, outXmlPath):
    """Extract ADM XML from a BW64/RF64/WAV file and write it to outXmlPath.

    Uses the cult-transcoder binary for ADM extraction.
    Raises FileNotFoundError if the binary has not been built yet.

    Output path is always outXmlPath (processedData/currentMetaData.xml).
    Downstream modules (parser.py, xml_etree_parser.py) are unchanged.
    """
    print("Extracting ADM metadata from WAV file using cult-transcoder...")

    if not _CULT_TRANSCODER.exists():
        raise FileNotFoundError(
            f"cult-transcoder not found at {_CULT_TRANSCODER}\n"
            "Run ./init.sh (or configCPP.buildAdmExtractor()) to build it."
        )

    cmd = [str(_CULT_TRANSCODER), "--extract", "adm", "--input", wavPath, "--output", outXmlPath]
    subprocess.run(cmd, check=True)
    print(f"Exported ADM metadata to {outXmlPath}")
    return outXmlPath

