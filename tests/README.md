# UART flow tests

Run from the repository root with a C++17 compiler:

```sh
c++ -std=c++17 -Wall -Wextra -Itests/stubs -I. tests/uart_flow_test.cpp \
  rotenso.cpp frame_parser.cpp rotenso_frame_builder.cpp rotenso_swing.cpp -o /tmp/rotenso-uart-flow-test
/tmp/rotenso-uart-flow-test
```

These tests simulate the ESPHome clock, UART, climate, and select APIs. The swing tests compare complete commands and reported positions against the captured `SetCommand` and `Heartbeat` rows in the supplied Rotenso spreadsheet. They do not replace an ESPHome firmware build or testing with an air conditioner.
