# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| > 0.1.0 | :white_check_mark: |
| <= 0.1.0| :x:                |

## Reporting a Vulnerability

Please report security vulnerabilities by emailing **security@darwinstylish.com**.
Do **not** open public issues for security vulnerabilities.

## Response Timeline

- **Acknowledgment:** Within 48 hours of your report.
- **Assessment:** Within 7 days of the acknowledgment.

## Responsible Disclosure Policy

We ask that you:
- Give us a reasonable amount of time to investigate and mitigate the issue before public disclosure.
- Make a good faith effort to avoid privacy violations, destruction of data, and interruption or degradation of our services during your research.

## Out of Scope

- Theoretical vulnerabilities without a working proof of concept.
- Volumetric DoS attacks (these are handled at the infrastructure level).

## Security Hardening Notes

Specific to the HFT nature of this repository:
- **Compiler Flags:** We compile with `-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2`, and `-Wextra` to ensure binary hardening.
- **Sanitizers:** Our continuous integration thoroughly tests the system using AddressSanitizer and UndefinedBehaviorSanitizer targets.
- **Zero-Allocation Hot Path:** The core engine operates in a strict zero-allocation mode on the hot path, mitigating broad classes of heap vulnerabilities entirely.
