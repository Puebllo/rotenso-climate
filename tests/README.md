# UART flow tests

Run from the repository root with a C++17 compiler:

```sh
c++ -std=c++17 -Wall -Wextra -Itests/stubs -Icomponents/rotenso tests/uart_flow_test.cpp \
  components/rotenso/rotenso.cpp components/rotenso/frame_parser.cpp \
  components/rotenso/rotenso_frame_builder.cpp components/rotenso/rotenso_swing.cpp \
  -o /tmp/rotenso-uart-flow-test
/tmp/rotenso-uart-flow-test
```

These tests simulate the ESPHome clock, UART, climate, and select APIs. The swing tests compare complete commands and reported positions against the captured `SetCommand` and `Heartbeat` rows in [Rotenso.xlsx](../Rotenso.xlsx). They do not replace an ESPHome firmware build or testing with an air conditioner.
