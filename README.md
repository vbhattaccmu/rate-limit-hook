# XRPL Rate Limit Hook

The following document presents a well-organized description of the `xrpl_rate_limit.c` components. 
The code relies on the `hookapi.h` header file and can be successfully compiled using XRPL Hooks Builder.

## Algorithm

The rate limiter hook implements the following algorithm, utilizing the `state` component as a key-value storage:

1. Fetch the `sfAccount` field from the originating transaction.
2. Determine the current transaction type. If it's not a `Payment` transaction, the rate limit does not apply, and the transaction is accepted 
   directly.
3. Retrieve the latest ledger timestamp.
4. Fetch the current state value for the `signer` account from the `state` storage.
5. Extract the last ledger timestamp and transaction amount from the `state`-value array.
6. Check if new window has started. (window_end_timestamp >= window_end_timestamp). 
   If `latest_ledger_timestamp` is greater than `window_end_timestamp` then 
   - update `window_end_timestamp` to next window timestamp.
   - reset `total_transacted_amount_this_window` to 0 so that user can send new transactions in the new window.
7. Check if the amount type is `XRP`. If not, rollback the transaction.
8. Verify if the total spent amount within the window exceeds the `MAX_XRP_SPENDING_LIMIT`. If it does, rollback the transaction.
9. If the transaction has reached this point, increment the `total_transacted_amount_this_window` by the current transaction amount.
10. Update the `state`-value array with the updated `total_transacted_amount_this_window` and the ``window_end_timestamp`.
11. Accept the transaction.

## State Protocol

The state protocol outlines the structure and retrieval/update process for variables stored in the `state` component.

### State Structure

The `state` is a key-value storage component associated with the executed hook. It consists of the following:

- `key`: The account address of the transaction signer.
- `value`: An array of 32 bytes.

In this specific case, the `value` component of the `state` array consists of two attributes:

- `window_end_timestamp`: The end timestamp of the current window.
- `total_transacted_amount_this_window`: The accumulated transaction amount in XRP within the current window.

The upcoming sections provide detailed information about these variables.

### Retrieval from State Storage

To retrieve the `window_end_timestamp` and `total_transacted_amount_this_window` from the `state`-value array, follow these steps:

#### Timestamp

- Initialize the `window_end_timestamp` variable as an `int64_t` with a value of 0.
- Iterate from `counter = 0` to `counter < 8` (inclusive).
- In each iteration, convert the first 8 bytes of the `latest_state` array into an `int64_t` value and assign it to the `window_end_timestamp` 
  variable.

```cpp
window_end_timestamp |= ((int64_t)latest_state[counter] << (counter * 8));
```

#### Transaction Amount

- Initialize the `total_transacted_amount_this_window` variable as an `int64_t` with a value of 0.
- Iterate from `counter = 0` to `counter < 8` (inclusive).
- In each iteration, combine the bytes starting from index 8 to 15 (inclusive) of the `latest_state` array into a single 64-bit integer value using 
  bitwise shift and bitwise OR operations. Assign this value to the `total_transacted_amount_this_window` variable.

```cpp
total_transacted_amount_this_window |= ((int64_t)latest_state[counter + 8] << (counter * 8));
```

### Update in State Storage

To update the `window_end_timestamp` and `total_transacted_amount_this_window` fields in the `state`-value array, follow these steps:

#### Timestamp

- Initialize the `counter` variable as 0.
- Iterate from `counter = 0` to `counter < 8` (inclusive).
- In each iteration, perform a bitwise OR operation on the `window_end_timestamp` left-shifted by 
  `counter * 8` bits. Store the result in the `latest_state` array at the corresponding index.

```cpp
latest_state[counter] = (uint8_t)((window_end_timestamp >> (counter * 8)) & 0xFF);
```

#### Transaction Amount

- Initialize the `counter` variable as 0.
- Iterate from `counter = 0` to `counter < 8` (inclusive).
- In each iteration, perform a bitwise OR operation on the `total_transacted_amount_this_window` right-shifted by `counter * 8` bits. Store the 
  result in the `state` array at index `counter + 8`.

```cpp
state[counter + 8] = (uint8_t)((total_transacted_amount_this_window >> (counter * 8)) & 0xFF);
```

## Deployment

The corresponding wasm generated from `rate_limiter.c` has been deployed [here](https://hooks-testnet-v3-explorer.xrpl-labs.com/tx/D4EBCB9BF378437CD5704F5ECA6F8F8072422DCE3EDC0690C17634B260CC300F).

## Testing

The corresponding wasm can be tested [here](https://hooks-builder.xrpl.org/test/).

Sample JSON input:
```
{
  "Destination": "rUyVUT6cYJtcrwNxwtvQzPCZTtvfs7edNP",
  "Amount": "10000000",
  "Fee": "1508",
  "Flags": "2147483648",
  "HookParameters": [],
  "TransactionType": "Payment",
  "Account": "rU5oEoevpEDmuq4x2syQGokTUBka6NXqB1",
  "Memos": []
}
```

## Testing Output
Sample Output from two cases: 

### Success: 
```
22:28:40 View:TRC HookTrace[Ede-Ede]: "rate_limiter.c: Called.": rate_limiter.c: Called.
22:28:40 View:TRC HookTrace[Ede-Ede]: total_transacted_amount_this_window/CONVERSION_FACTOR 49
22:28:40 View:TRC HookTrace[Ede-Ede]: latest_ledger_timestamp 1687098520
22:28:40 View:TRC HookTrace[Ede-Ede]: window_end_timestamp 1687098600
22:28:40 View:TRC HookInfo[Ede-Ede]: ACCEPT RS: '' RC: 0
```

### Failure:
```
22:38:10 View:TRC HookTrace[Ede-Ede]: "rate_limiter.c: Called.": rate_limiter.c: Called.
22:38:10 View:TRC HookInfo[Ede-Ede]: ROLLBACK RS: 'Rate limit exceeded. Maximum XRP spending limit reached.' RC: 10
```

### Transition of next window timestamp and refresh of `total_transacted_amount_this_window`:

Previous Window:
```
22:29:49 View:TRC HookTrace[Ede-Ede]: "rate_limiter.c: Called.": rate_limiter.c: Called.
22:29:49 View:TRC HookTrace[Ede-Ede]: total_transacted_amount_this_window/CONVERSION_FACTOR 99
22:29:49 View:TRC HookTrace[Ede-Ede]: latest_ledger_timestamp 1687098583
22:29:49 View:TRC HookTrace[Ede-Ede]: window_end_timestamp 1687098600
22:29:49 View:TRC HookInfo[Ede-Ede]: ACCEPT RS: '' RC: 0
22:29:49 LedgerConsensus:TRC metadata 
Expand
22:29:49 NetworkOPs:TRC pubAccepted: 
```

New Window(total_transacted_amount_this_window is reset to 0 and next window gets updated):-
```
Debug stream opened for account rMdZLR1Vr1JYKpzLVZKwCkjwKHjLiUUT1K
22:30:40 View:TRC HookInfo[Ede-Ede]: creating wasm instance
22:30:40 View:TRC HookTrace[Ede-Ede]: "rate_limiter.c: Called.": rate_limiter.c: Called.
22:30:40 View:TRC HookTrace[Ede-Ede]: total_transacted_amount_this_window/CONVERSION_FACTOR 0
22:30:40 View:TRC HookTrace[Ede-Ede]: latest_ledger_timestamp 1687098640
22:30:40 View:TRC HookTrace[Ede-Ede]: window_end_timestamp 1687098900
22:30:40 View:TRC HookInfo[Ede-Ede]: ACCEPT RS: '' RC: 0
...
```
