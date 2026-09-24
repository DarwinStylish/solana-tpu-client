# Examples

The examples use only public headers and public library artifacts.

They demonstrate local API behavior. None of the examples sends a transaction to a
validator TPU endpoint.

Build the libraries first:

```bash
make all
```

Compile the examples with a C11 compiler:

```bash
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude \
  examples/ingress_decode.c build/libsolana_ingress.a \
  -o /tmp/solana-ingress-decode

cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude \
  examples/delivery_local_acceptance.c build/libsolana_delivery.a \
  -o /tmp/solana-delivery-local-acceptance

cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude \
  examples/discovery_refresh.c build/libsolana_delivery.a \
  -o /tmp/solana-discovery-refresh
```

Run them:

```bash
/tmp/solana-ingress-decode
/tmp/solana-delivery-local-acceptance
/tmp/solana-discovery-refresh
```
