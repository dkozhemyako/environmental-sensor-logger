# State Machine

The Environmental Sensor Logger uses three states: `IDLE`, `RUNNING`, and `ERROR`.

## IDLE

The device is initialized and ready. Sensor logging is not active.

Transition: `START -> RUNNING`.

## RUNNING

The device periodically:

1. reads all sensors;
2. validates measurements;
3. sends a JSON message to the serial port;
4. writes a CSV record to LittleFS.

Transitions:

- `STOP -> IDLE`
- critical sensor/storage error -> `ERROR`

## ERROR

A critical failure has occurred. Possible reasons:

- sensor initialization failure;
- three consecutive invalid sensor measurements;
- flash storage failure.

Sensor logging stops in this state.

## Diagram

```text
                 START
        +----------------------+
        |                      v
      IDLE                  RUNNING
        ^                      |
        |                      |
        +-------- STOP --------+
                               |
                               | sensor/storage error
                               v
                             ERROR
```

## State Transitions in Firmware

Initial state:

```cpp
DeviceState state = DeviceState::IDLE;
```

`START` command:

```cpp
state = DeviceState::RUNNING;
```

`STOP` command:

```cpp
state = DeviceState::IDLE;
```

Critical error:

```cpp
state = DeviceState::ERROR;
```

Sensor acquisition is executed only in the `RUNNING` state.
