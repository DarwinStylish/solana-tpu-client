## Description
<!-- Describe your changes in detail -->

## Motivation and Context
<!-- Why is this change required? What problem does it solve? -->
<!-- If it fixes an open issue, please link to the issue here. -->

## How Has This Been Tested?
<!-- Please describe in detail how you tested your changes. -->
<!-- Include details of your testing environment, and the tests you ran. -->

## C11 Zero-Allocation Checklist:
- [ ] I have read the [CONTRIBUTING](CONTRIBUTING.md) document.
- [ ] My code strictly compiles with `-std=c11` and throws no warnings with `-Wall -Wextra -Werror`.
- [ ] I have introduced **zero** dynamic allocations (`malloc`, `free`) on the hot path.
- [ ] I have run `make test` and all unit tests pass.
- [ ] I have run `make test-asan` and AddressSanitizer reports no leaks or out-of-bounds access.
- [ ] I have run `make test-ubsan` and UndefinedBehaviorSanitizer reports no issues.
