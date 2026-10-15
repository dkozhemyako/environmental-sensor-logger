# Message Format

The Environmental Sensor Logger uses JSON Lines for serial communication. Each JSON message is transmitted on one line.

## Sensor Reading

```json
{"type":"reading","timestamp":"2026-10-15T10:05:12Z","time_synced":true,"temperature_c":21.35,"humidity_pct":56.10,"pressure_hpa":999.60,"light_lx":18.33,"status":"OK"}
```

| Field | Type | Description | Unit |
|---|---|---|---|
| `type` | string | Message type | — |
| `timestamp` | string | UTC ISO 8601 timestamp | UTC |
| `time_synced` | boolean | Indicates successful NTP synchronization | — |
| `temperature_c` | number | Air temperature | °C |
| `humidity_pct` | number | Relative humidity | % |
| `pressure_hpa` | number | Atmospheric pressure | hPa |
| `light_lx` | number | Light intensity | lx |
| `status` | string | Reading validation status (`OK` or `INVALID`) | — |

## Status Message

```json
{"type":"status","state":"IDLE","interval_ms":1000,"sensors_ready":true,"storage_ready":true,"time_synced":true}
```

| Field | Type | Description |
|---|---|---|
| `type` | string | Message type |
| `state` | string | Current state: `IDLE`, `RUNNING`, or `ERROR` |
| `interval_ms` | integer | Measurement interval in milliseconds |
| `sensors_ready` | boolean | Sensor initialization status |
| `storage_ready` | boolean | LittleFS initialization status |
| `time_synced` | boolean | NTP synchronization status |

## Command Response

```json
{"type":"response","command":"START","result":"OK","message":"Logger started"}
```

Possible `result` values: `OK`, `ERR`.

## Error Message

```json
{"type":"error","source":"sensor","message":"Three consecutive invalid readings"}
```

Possible error sources include `sensor` and `storage`.

## Boot Message

```json
{"type":"boot","bh1750":true,"aht20":true,"bmp280":true,"storage":true,"time_synced":true}
```

## CSV Flash Format

```text
timestamp,time_synced,temperature_c,humidity_pct,pressure_hpa,light_lx,status
```

```csv
2026-10-15T10:05:12Z,true,21.35,56.10,999.60,18.33,OK
```

| Field | Type | Unit |
|---|---|---|
| `timestamp` | string | UTC |
| `time_synced` | boolean | — |
| `temperature_c` | number | °C |
| `humidity_pct` | number | % |
| `pressure_hpa` | number | hPa |
| `light_lx` | number | lx |
| `status` | string | — |
