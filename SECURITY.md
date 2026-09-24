# Security Policy

## Supported Revisions

Solana TPU Client is pre-release and has no published versioned release tags.

Security support applies to the latest revision of `main`. Older development revisions
may not receive backported fixes.

## Reporting a Vulnerability

Report suspected security vulnerabilities privately to **security@darwinstylish.com**.

Do not open a public issue for an undisclosed vulnerability.

Include, where available:

- the affected commit;
- the affected public API or input boundary;
- the observed impact;
- minimal reproduction steps;
- sanitizer output, crash diagnostics, or backtraces;
- relevant compiler and operating-system information.

## Response Targets

- **Acknowledgment:** within 48 hours of receipt.
- **Initial assessment:** within 7 days of acknowledgment.

These are response targets and do not guarantee a specific remediation date.

## Security-Relevant Boundaries

Reports are particularly useful when they concern:

- memory safety;
- malformed or adversarial input handling;
- public ABI validation;
- topology validation and ownership;
- request or event lifetime errors;
- failure atomicity;
- resource-exhaustion behavior;
- integer overflow or bounds errors;
- incorrect trust-boundary assumptions.

## Hardening

Continuous integration exercises GCC and Clang builds, warning-as-error compilation,
AddressSanitizer, UndefinedBehaviorSanitizer, deterministic fuzz smoke tests,
repository-isolation checks, and clean tracked-archive builds and tests.

These controls reduce risk but do not constitute a claim of complete security.
