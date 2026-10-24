#!/usr/bin/env python3
"""Refuse to publish an incomplete or incorrectly versioned Linux release."""

import argparse
from pathlib import Path
import tarfile
from email.parser import BytesParser
from zipfile import ZipFile

from packaging.utils import parse_sdist_filename, parse_wheel_filename
from packaging.version import Version


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("version", type=Version)
    args = parser.parse_args()
    expected = {(python, policy) for python in ("cp311", "cp312")
                for policy in ("manylinux_2_17_x86_64", "manylinux_2_28_x86_64")}
    found = set()
    wheels = list(args.directory.glob("*.whl"))
    assert len(wheels) == 4, wheels
    for wheel in wheels:
        name, version, _, tags = parse_wheel_filename(wheel.name)
        assert name == "uc-xcomm" and version == args.version, wheel
        matches = {(tag.interpreter, tag.platform) for tag in tags
                   if tag.interpreter == tag.abi and (tag.interpreter, tag.platform) in expected}
        assert len(matches) == 1 and not found.intersection(matches), (wheel, tags)
        found.update(matches)
        with ZipFile(wheel) as archive:
            metadata, = [name for name in archive.namelist() if name.endswith(".dist-info/METADATA")]
            assert Version(BytesParser().parsebytes(archive.read(metadata))["Version"]) == args.version
    assert found == expected, (found, expected)
    sdists = list(args.directory.glob("*.tar.gz"))
    assert len(sdists) == 1, sdists
    assert parse_sdist_filename(sdists[0].name) == ("uc-xcomm", args.version)
    with tarfile.open(sdists[0]) as archive:
        metadata, = [member for member in archive.getmembers() if member.name.count("/") == 1
                     and member.name.endswith("/PKG-INFO")]
        assert Version(BytesParser().parsebytes(archive.extractfile(metadata).read())["Version"]) == args.version
    assert len(list(args.directory.iterdir())) == 5, list(args.directory.iterdir())
    print(f"Validated uc-xcomm {args.version}: four Linux wheels and one sdist")


if __name__ == "__main__":
    main()
