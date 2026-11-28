import json

import numpy as np
import pytest

from lockin import vectors


def test_round_trip(tmp_path):
    arr = np.array([-(1 << 31), -1, 0, 1, (1 << 31) - 1])
    vectors.write_bin(tmp_path / "a.bin", arr)
    np.testing.assert_array_equal(vectors.read_bin(tmp_path / "a.bin"), arr)
    assert (tmp_path / "a.bin").stat().st_size == 4 * len(arr)


def test_little_endian(tmp_path):
    vectors.write_bin(tmp_path / "a.bin", [1])
    assert (tmp_path / "a.bin").read_bytes() == b"\x01\x00\x00\x00"


def test_uint32_phase_stored_as_bit_pattern(tmp_path):
    vectors.write_bin(tmp_path / "p.bin", np.array([0xFFFFFFFF], dtype=np.uint32))
    assert vectors.read_bin(tmp_path / "p.bin")[0] == -1


def test_rejects_out_of_range_and_float(tmp_path):
    with pytest.raises(OverflowError):
        vectors.write_bin(tmp_path / "x.bin", [1 << 31])
    with pytest.raises(TypeError):
        vectors.write_bin(tmp_path / "x.bin", [0.5])


VEC_DIR = __import__("pathlib").Path(__file__).resolve().parents[1] / "vectors"
CASES = sorted(p.name for p in VEC_DIR.iterdir()) if VEC_DIR.exists() else []


@pytest.mark.parametrize("case", CASES)
def test_committed_vectors_match_model(case):
    """Re-run the model on each exported input: every stage must match bit-exactly.

    Fails if the model changed without re-running experiments.export_vectors.
    Cases without a "kind" key are full pipeline runs.
    """
    from lockin import pipeline
    d = VEC_DIR / case
    meta = json.loads((d / "config.json").read_text())
    if meta.get("kind") == "dac_map":
        from lockin import dac
        for stage, got in dac.vector_stages(meta).items():
            np.testing.assert_array_equal(got, vectors.read_bin(d / f"{stage}.bin"),
                                          err_msg=f"{case}/{stage}")
        return
    assert "kind" not in meta, f"{case}: unknown vector kind {meta['kind']!r}"
    fields = {k: meta[k] for k in ("fs", "f_ref", "harmonic", "tau", "order", "m", "sync", "phase0")}
    cfg = pipeline.Config(**fields)
    assert cfg.delta == meta["delta"] and cfg.alpha_q31 == meta["alpha_q31"]
    r = pipeline.run_q31(vectors.read_bin(d / "adc.bin"), cfg)
    for stage in meta["files"]:
        expected = vectors.read_bin(d / f"{stage}.bin")
        got = r.stages[stage]
        if stage == "phase":
            got = np.asarray(got, dtype=np.uint32).view(np.int32)
        np.testing.assert_array_equal(got, expected, err_msg=f"{case}/{stage}")


def test_export(tmp_path):
    d = vectors.export("case", {"fs": 1.0}, {"x": [1, 2, 3]}, out_dir=tmp_path)
    meta = json.loads((d / "config.json").read_text())
    assert meta["fs"] == 1.0
    assert meta["files"]["x"] == {"len": 3, "dtype": "int32"}
