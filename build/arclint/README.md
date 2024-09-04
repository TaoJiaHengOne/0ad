# Linting

This folder contains tools for linting 0 A.D. code
Linting is done via Arcanist:
https://secure.phabricator.com/book/phabricator/article/arcanist_lint/

## Linters

- `text` is configured to detect whitespace issues.
- `json` detects JSON syntax errors.

## Installation

This assumes you have arcanist already installed. If not, consult
https://gitea.wildfiregames.com/0ad/0ad/wiki/Phabricator#UsingArcanist .

The linting is done via custom PHP scripts, residing in `pyrolint/`.
Configuration is at the root of the project, under `.arclint`.

### Installing linters

We provide dummy replacement for external linters, so that they are not required.
