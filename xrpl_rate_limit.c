/**
 * Spending Rate Limit Hook
 */
#include "hookapi.h"

// Payment type
#define ttPAYMENT 0

// Rate limiter constants
#define DURATION_WINDOW_LIMIT 300 
#define MAX_XRP_SPENDING_LIMIT 100

// Convert XRP to Drops
#define CONVERSION_FACTOR 1000000
// Linux offset for latest ledger timestamp
#define LINUX_OFFSET 946684800

int64_t hook(uint32_t reserved ) {
    TRACESTR("rate_limiter.c: Called.");

    _g(1,1);  

    // fetch the sfAccount field from the originating transaction
    uint8_t account_field[20];
    int32_t account_field_len = otxn_field(SBUF(account_field), sfAccount);
    if (account_field_len < 20) {                                 
        rollback(SBUF("sfAccount field missing!!!"), 10); 
    }
                                                            
    // get current transaction type
    int64_t transactionType = otxn_type();
    if (transactionType != ttPAYMENT) {
        // if not a payment transaction, rate limit not applicable, proceed 
        // directly to accept a transaction 
        accept (0, 0, 0); 
        return 0;
    }

    // get the latest ledger timestamp
    int64_t latest_ledger_timestamp = ledger_last_time() + LINUX_OFFSET;

    // initialize attributes
    int64_t window_end_timestamp = 0;
    int64_t total_transacted_amount_this_window = 0;

    // get the current state value for the account
    uint8_t latest_state[32]; 
    state(SBUF(latest_state), SBUF(account_field));

    // read the timestamp from the latest_state array
    for (int counter = 0; GUARD(8), counter < 8; counter++) {
        window_end_timestamp |= ((int64_t)latest_state[counter] << (counter * 8));
    }

    // read the amount from the latest_state array
    for (int counter = 0; GUARD(8), counter < 8; counter++) {
        total_transacted_amount_this_window |= ((int64_t)latest_state[counter + 8] << (counter * 8));
    }

    // Check if new window has started. (latest_ledger_timestamp >= window_end_timestamp)
    // if `latest_timestamp` is greater than `window_end_timestamp` then 
    // 1. update `window_end_timestamp` to next window 
    // 2. reset `total_transacted_amount_this_window` to 0 so that user can send new transactions in the new window
    if (latest_ledger_timestamp >= window_end_timestamp) { 
        window_end_timestamp = latest_ledger_timestamp + DURATION_WINDOW_LIMIT - (latest_ledger_timestamp % DURATION_WINDOW_LIMIT);  
        total_transacted_amount_this_window = 0;
    }

    // process the amount sent, which could be either xrp 
    // to do this we 'slot' the originating txn, that is: we place it into a slot so we can use the slot api to examine its internals
    int64_t oslot = otxn_slot(0);
    if (oslot < 0)
        rollback(SBUF("Could not slot originating txn."), 1);

    // specifically we're interested in the amount sent
    int64_t amt_slot = slot_subfield(oslot, sfAmount, 0);
    if (amt_slot < 0) {
        rollback(SBUF("Could not slot otxn.sfAmount"), 2);
    }

    int64_t transaction_amount = 0;
    // fetch the sent Amount
    // If the Amount is an XRP value it will be 64 bits.
    unsigned char amount_buffer[48];
    int64_t amount_len = otxn_field(SBUF(amount_buffer), sfAmount);

    if (amount_len != 8) {
        rollback(SBUF("Could not determine sent amount type"), 3);
    } else {
        transaction_amount = AMOUNT_TO_DROPS(amount_buffer);
    }

    // check if amount type is XRP before comparison this means passing flag=1
    int64_t is_xrp = slot_type(amt_slot, 1);
    if (is_xrp < 0) {
        rollback(SBUF("Could not determine sent amount type"), 3);
    }

    // check if the total spent amount within the window exceeds the maximum allowed
    if (total_transacted_amount_this_window + transaction_amount >= MAX_XRP_SPENDING_LIMIT*CONVERSION_FACTOR) {
        rollback(SBUF("Rate limit exceeded. Maximum XRP spending limit reached."), 10);
    }

    // increment the total_transacted_amount_this_window by the transaction amount
    total_transacted_amount_this_window += transaction_amount;

    // updated state
    uint8_t updated_state[32];

    // store the window_end_timestamp in the state_data array
    for (int counter = 0; GUARD(8), counter < 8; counter++) {
        updated_state[counter] = (uint8_t)((window_end_timestamp >> (counter * 8)) & 0xFF);
    }

    // store the total_transacted_amount_this_window in the state_data array
    for (int counter = 0; GUARD(8), counter < 8; counter++) {
        updated_state[counter + 8] = (uint8_t)((total_transacted_amount_this_window >> (counter * 8)) & 0xFF);
    }

    // set the updated state value for the next call
    state_set(SBUF(updated_state), SBUF(account_field));

    // show current accumulated XRP (DROPS/CONVERSION_FACTOR), latest timestamp and next window
    TRACEVAR(total_transacted_amount_this_window/CONVERSION_FACTOR);
    TRACEVAR(latest_ledger_timestamp);
    TRACEVAR(window_end_timestamp);

    accept(0, 0, 0);

    // unreachable
    return 0;
}
