#!/usr/bin/env python3

import sys
from logging import INFO, Formatter, StreamHandler, getLogger
from pathlib import Path

from collada import Collada, DaeBrokenRefError, DaeError, common, controller
from scriptlib import find_files


def validate_vertex(path_dae, log=print):
    has_weightless = False
    had_exception = False
    dae = None
    try:
        dae = Collada(path_dae, ignore=[common.DaeUnsupportedError, DaeBrokenRefError])
    except DaeError as inst:
        log("Failed to load %s", path_dae)
        log(type(inst), inst)
        had_exception = True
    for ctr in dae.controllers:
        if type(ctr) is not controller.Skin:
            had_exception = True
            break
        totalv = len(ctr.vcounts)
        totalv_0 = len(ctr.vcounts[ctr.vcounts == 0])
        if totalv_0 > 0:
            log(
                "Mesh %s has %i (out of %i) vertices with no weight"
                " and no bone assigned. Use P294 to find them in Blender.",
                path_dae,
                totalv_0,
                totalv,
            )
            has_weightless = True
    return (int(has_weightless) << 1) | int(had_exception)


class DaeValidator:
    def __init__(self, vfs_root, mods):
        self.has_weightless_vtx = []
        self.vfs_root = vfs_root
        self.mods = mods

        self.log = getLogger()
        self.log.setLevel(INFO)
        sh = StreamHandler(sys.stdout)
        sh.setLevel(INFO)
        sh.setFormatter(Formatter("%(levelname)s - %(message)s"))
        self.log.addHandler(sh)

    def run(self):
        is_ok = True
        files = find_files(self.vfs_root, self.mods, Path("art/meshes"), ["dae"])
        i = 0
        for _, dae in files:
            i += 1
            status = validate_vertex(dae.as_posix(), self.log.warning)
            if status >= 1:
                is_ok = False
            if status >= 2:
                self.has_weightless_vtx.append(dae)
        self.log.info(
            "%i out of %i files have vertices with no weight or bones.",
            len(self.has_weightless_vtx),
            i,
        )
        return is_ok


if __name__ == "__main__":
    vfsr = Path(__file__).resolve().parents[3] / "binaries" / "data" / "mods"
    mods = ["public"]
    dv = DaeValidator(vfsr, mods)
    print(f"DaeValidator returns {dv.run()}")
    print(dv.has_weightless_vtx)
