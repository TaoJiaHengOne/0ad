#!/usr/bin/env python3
#
# Copyright (C) 2022 Wildfire Games.
# This file is part of 0 A.D.
#
# 0 A.D. is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# 0 A.D. is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.

import multiprocessing
import os
import re
import sys

from i18n_helper import L10N_FOLDER_NAME, PROJECT_ROOT_DIRECTORY
from i18n_helper.catalog import Catalog
from i18n_helper.globber import get_catalogs


VERBOSE = 0


class MessageChecker:
    """Checks all messages in a catalog against a regex."""

    def __init__(self, human_name, regex):
        self.regex = re.compile(regex, re.IGNORECASE)
        self.human_name = human_name

    def check(self, input_file_path, template_message, translated_catalogs):
        patterns = set(
            self.regex.findall(
                template_message.id[0] if template_message.pluralizable else template_message.id
            )
        )

        # As a sanity check, verify that the template message is coherent.
        # Note that these tend to be false positives.
        # TODO: the pssible tags are usually comments, we ought be able to find them.
        if template_message.pluralizable:
            plural_urls = set(self.regex.findall(template_message.id[1]))
            if plural_urls.difference(patterns):
                print(
                    f"{input_file_path} - Different {self.human_name} in "
                    f"singular and plural source strings "
                    f"for '{template_message}' in '{input_file_path}'"
                )

        for translation_catalog in translated_catalogs:
            translation_message = translation_catalog.get(
                template_message.id, template_message.context
            )
            if not translation_message:
                continue

            translated_patterns = set(
                self.regex.findall(
                    translation_message.string[0]
                    if translation_message.pluralizable
                    else translation_message.string
                )
            )
            unknown_patterns = translated_patterns.difference(patterns)
            if unknown_patterns:
                print(
                    f'{input_file_path} - {translation_catalog.locale}: '
                    f'Found unknown {self.human_name} '
                    f'{", ".join(["`" + x + "`" for x in unknown_patterns])} '
                    f'in the translation which do not match any of the URLs '
                    f'in the template: {", ".join(["`" + x + "`" for x in patterns])}'
                )

            if template_message.pluralizable and translation_message.pluralizable:
                for indx, val in enumerate(translation_message.string):
                    if indx == 0:
                        continue
                    translated_patterns_multi = set(self.regex.findall(val))
                    unknown_patterns_multi = translated_patterns_multi.difference(plural_urls)
                    if unknown_patterns_multi:
                        print(
                            f'{input_file_path} - {translation_catalog.locale}: '
                            f'Found unknown {self.human_name} '
                            f'{", ".join(["`" + x + "`" for x in unknown_patterns_multi])} '
                            f'in the pluralised translation which do not '
                            f'match any of the URLs in the template: '
                            f'{", ".join(["`" + x + "`" for x in plural_urls])}'
                        )


def check_translations(input_file_path):
    if VERBOSE:
        print(f"Checking {input_file_path}")
    template_catalog = Catalog.read_from(input_file_path)

    # If language codes were specified on the command line, filter by those.
    filters = sys.argv[1:]

    # Load existing translation catalogs.
    existing_translation_catalogs = get_catalogs(input_file_path, filters)

    spam = MessageChecker("url", r"https?://(?:[a-z0-9-_$@./&+]|(?:%[0-9a-fA-F][0-9a-fA-F]))+")
    sprintf = MessageChecker("sprintf", r"%\([^)]+\)s")
    tags = MessageChecker("tag", r"[^\\][^\\](\[[^]]+/?\])")

    # Check that there are no spam URLs.
    # Loop through all messages in the .POT catalog for URLs.
    # For each, check for the corresponding key in the .PO catalogs.
    # If found, check that URLS in the .PO keys are the same as those in the .POT key.
    for template_message in template_catalog:
        spam.check(input_file_path, template_message, existing_translation_catalogs)
        sprintf.check(input_file_path, template_message, existing_translation_catalogs)
        tags.check(input_file_path, template_message, existing_translation_catalogs)

    if VERBOSE:
        print(f"Done checking {input_file_path}")


def main():
    print(
        "\n\tWARNING: Remember to regenerate the POT files with “update_templates.py” "
        "before you run this script.\n\tPOT files are not in the repository.\n"
    )
    found_pots = 0
    for root, _folders, filenames in os.walk(PROJECT_ROOT_DIRECTORY):
        for filename in filenames:
            if (
                len(filename) > 4
                and filename[-4:] == ".pot"
                and os.path.basename(root) == L10N_FOLDER_NAME
            ):
                found_pots += 1
                multiprocessing.Process(
                    target=check_translations, args=(os.path.join(root, filename),)
                ).start()
    if found_pots == 0:
        print(
            "This script did not work because no '.pot' files were found. "
            "Please run 'update_templates.py' to generate the '.pot' files, "
            "and run 'pull_translations.py' to pull the latest translations from Transifex. "
            "Then you can run this script to check for spam in translations."
        )


if __name__ == "__main__":
    main()
