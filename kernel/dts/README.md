# NXP Simulated Temperature Sensor - Device Tree Configuration

This directory contains the Device Tree Source Include (.dtsi) file and binding documentation for the NXP Simulated Temperature Sensor driver.

## Files

- `nxp_simtemp.dtsi` - Device Tree Source Include file with sensor configuration
- `nxp_simtemp.yaml` - Device Tree binding documentation (YAML schema)

## Usage

### Including in Board Device Tree

To use the temperature sensor in your board's device tree, include the .dtsi file:

```dts
#include "nxp_simtemp.dtsi"
```

### Customizing Configuration

You can override the default properties in your board's device tree:

```dts
&simtemp {
    sampling-ms = <50>;        /* 50ms sampling period */
    threshold-mC = <50000>;    /* 50°C threshold */
    mode = "ramp";             /* Ramp mode for testing */
};
```

## Device Tree Properties

| Property       | Type   | Default         | Description                        |
|----------------|--------|-----------------|------------------------------------|
| `compatible`   | string | `"nxp,simtemp"` | Device compatibility string        |
| `reg`          | array  | `0x40000000`    | Memory region (placeholder)        |
| `interrupts`   | array  | `45`            | Interrupt line (placeholder)       |
| `sampling-ms`  | uint32 | `100`           | Sampling period (1-10000 ms)       |
| `threshold-mC` | int32  | `45000`         | Alert threshold (milli-°C)         |
| `mode`         | string | `"normal"`      | Simulation mode                    |
| `status`       | string | `"okay"`        | Device status                      |

## Simulation Modes

- **`normal`**: Constant temperature at base value (25°C)
- **`noisy`**: Base temperature with random noise (±1°C)
- **`ramp`**: Temperature ramps up and down periodically

## Example Configurations

### Basic Monitoring
```dts
&simtemp {
    sampling-ms = <1000>;      /* 1 second sampling */
    threshold-mC = <60000>;    /* 60°C threshold */
    mode = "normal";
};
```

### High-Frequency Testing
```dts
&simtemp {
    sampling-ms = <25>;        /* 25ms sampling */
    threshold-mC = <24500>;    /* 24.5°C threshold */
    mode = "ramp";             /* Ramp mode for alerts */
};
```

### Stress Testing
```dts
&simtemp {
    sampling-ms = <10>;        /* 10ms sampling */
    threshold-mC = <30000>;    /* 30°C threshold */
    mode = "noisy";            /* Noisy mode */
};
```

## Driver Integration

The driver automatically reads these properties during probe and configures itself accordingly. If properties are missing or invalid, the driver falls back to default values and logs warnings.

## Validation

Use the Device Tree Compiler (dtc) to validate your configuration:

```bash
dtc -I dts -O dtb -o nxp_simtemp.dtb nxp_simtemp.dtsi
```

Or use the YAML schema for validation:

```bash
dt-validate nxp_simtemp.dtsi nxp_simtemp.yaml
```
