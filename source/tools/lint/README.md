# Linters

Linters for use in CI or by developers. Also providing configurations for IDEs.

## cppcheck

### suppression-list

The suppression list is ideally empty, restricting to file scope is preferred.

The format for an error suppression is one of:
[error id]:[filename]:[line]
[error id]:[filename2]
[error id]

### libraries

Adding library cfg's for other deps could improve cppchecks ability to find issues.
