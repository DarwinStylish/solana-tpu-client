#include <stdio.h>
#include <assert.h>
#include "adapter_monad.h"

int main() {
    printf("[*] Running Monad EVM Adapter Verification...\n");
    uint8_t raw_log[76] = {0};
    monad_evm_log_t* mock_log = (monad_evm_log_t*)raw_log;
    mock_log->amount_in = 1000;
    mock_log->amount_out = 5000;
    
    event_t out;
    assert(parse_monad_evm(raw_log, &out));
    
    printf("[+] Successfully parsed Monad EVM log.\n");
    // Use %ld for fixed_t/long int price components if needed, 
    // though to_double() returns a double, so %f is correct there.
    printf("[+] Calculated Price: %f\n", to_double(out.price));
    
    return 0;
}
