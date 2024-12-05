#!/usr/bin/env python3
#
# Copyright (C) 2024 Wildfire Games.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import argparse
import json
import os
import unittest


def make_plane_combinations(combinations):
    plane_combinations = []
    for combination in combinations:
        plane_combination = [(define["name"], define["value"]) for define in combination]
        plane_combination.sort()
        if plane_combination not in plane_combinations:
            plane_combinations.append(plane_combination)
    plane_combinations.sort()
    return plane_combinations


def merge_plane_combinations(lhs, rhs):
    # We assume that combinations are sorted.
    combined_plane_combinations = []
    lhs_index, rhs_index = 0, 0
    while lhs_index < len(lhs) and rhs_index < len(rhs):
        if lhs[lhs_index] == rhs[rhs_index]:
            combined_plane_combinations.append(lhs[lhs_index])
            lhs_index += 1
            rhs_index += 1
        elif lhs[lhs_index] < rhs[rhs_index]:
            combined_plane_combinations.append(lhs[lhs_index])
            lhs_index += 1
        else:
            combined_plane_combinations.append(rhs[rhs_index])
            rhs_index += 1
    while lhs_index < len(lhs):
        combined_plane_combinations.append(lhs[lhs_index])
        lhs_index += 1
    while rhs_index < len(rhs):
        combined_plane_combinations.append(rhs[rhs_index])
        rhs_index += 1

    combinations = []
    for plane_combination in combined_plane_combinations:
        combinations.append(
            [{"name": define[0], "value": define[1]} for define in plane_combination]
        )
    return combinations


def merge_rules(lhs, rhs):
    rules = {}
    for program_name in set(lhs.keys()) | set(rhs.keys()):
        rules[program_name] = {
            "name": program_name,
            "combinations": merge_plane_combinations(
                make_plane_combinations(
                    lhs[program_name]["combinations"]
                    if program_name in lhs and "combinations" in lhs[program_name]
                    else []
                ),
                make_plane_combinations(
                    rhs[program_name]["combinations"]
                    if program_name in rhs and "combinations" in rhs[program_name]
                    else []
                ),
            ),
        }
    return rules


def run():
    parser = argparse.ArgumentParser()
    parser.add_argument("output_rules_path", help="a path to output merged rules")
    parser.add_argument("input_paths", help="a paths to input rules", nargs=argparse.REMAINDER)
    args = parser.parse_args()

    assert args.input_paths
    for input_path in args.input_paths:
        assert os.path.isfile(input_path)

    rules = {}
    for input_path in args.input_paths:
        with open(input_path) as handle:
            input_rules = json.load(handle)
        rules = merge_rules(rules, input_rules)

    with open(args.output_rules_path, "w") as handle:
        json.dump(rules, handle, sort_keys=True)


if __name__ == "__main__":
    run()


class TestMerge(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.TEST_RULES = {
            "canvas2d": {
                "name": "canvas2d",
                "combinations": [[], [{"name": "USE_DESCRIPTOR_INDEXING", "value": "1"}]],
            }
        }

    def make_rules(self, program_name, combinations):
        return {program_name: {"name": program_name, "combinations": combinations}}

    def test_empty(self):
        self.assertDictEqual(merge_rules({}, {}), {})
        self.assertDictEqual(merge_rules(self.make_rules("A", []), {}), self.make_rules("A", []))
        self.assertDictEqual(merge_rules({}, self.make_rules("A", [])), self.make_rules("A", []))
        self.assertDictEqual(merge_rules(self.TEST_RULES, {}), self.TEST_RULES)
        self.assertDictEqual(merge_rules({}, self.TEST_RULES), self.TEST_RULES)

    def test_equal(self):
        self.assertDictEqual(merge_rules(self.TEST_RULES, self.TEST_RULES), self.TEST_RULES)

    def test_order(self):
        # We need to guarantee an order for reproducible builds.

        cmb_a = {"name": "A", "value": "1"}
        cmb_a2 = {"name": "A", "value": "2"}
        cmb_a3 = {"name": "A", "value": "3"}
        cmb_b = {"name": "B", "value": "1"}
        cmb_c = {"name": "C", "value": "1"}
        cmb_d = {"name": "D", "value": "1"}

        self.assertDictEqual(
            merge_rules(
                self.make_rules("A", [[cmb_c], [cmb_b]]),
                self.make_rules("A", [[cmb_b], [cmb_c], [cmb_a]]),
            ),
            self.make_rules("A", [[cmb_a], [cmb_b], [cmb_c]]),
        )

        self.assertDictEqual(
            merge_rules(
                self.make_rules("A", [[cmb_c], [cmb_a], [cmb_c], [cmb_c]]),
                self.make_rules("A", [[cmb_d], [cmb_b], [cmb_d]]),
            ),
            self.make_rules("A", [[cmb_a], [cmb_b], [cmb_c], [cmb_d]]),
        )

        self.assertDictEqual(
            merge_rules(
                self.make_rules("A", [[cmb_c], [cmb_a], [cmb_d], [cmb_c], [cmb_a3]]),
                self.make_rules("A", [[cmb_b], [cmb_a2], [cmb_a2]]),
            ),
            self.make_rules("A", [[cmb_a], [cmb_a2], [cmb_a3], [cmb_b], [cmb_c], [cmb_d]]),
        )

        self.assertDictEqual(
            merge_rules(
                self.make_rules("A", [[cmb_b, cmb_c], [cmb_a], [cmb_b, cmb_b], [cmb_b, cmb_c]]),
                self.make_rules("A", [[cmb_b], [cmb_b, cmb_a]]),
            ),
            self.make_rules(
                "A", [[cmb_a], [cmb_a, cmb_b], [cmb_b], [cmb_b, cmb_b], [cmb_b, cmb_c]]
            ),
        )
