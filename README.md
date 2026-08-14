# VL53L8CX Zephyr Driver (ST ULP API Port)

This repository contains a **Zephyr sensor driver port** for the ST VL53L8CX ToF sensor, built on top of STMicroelectronics' **Ultra Lite Driver (ULD/ULP API)**.

## Implementation Summary

The driver is implemented as a Zephyr module and uses ST's provided VL53L8CX API as the low-level engine:
- ST API sources are included under `drivers/sensor/st/vl53l8cx/VL53L8_API/`
- Zephyr platform adaptation is handled through the local platform layer (`platform.c` / `platform.h`)
- Zephyr-facing driver logic is implemented in `st_vl53l8cx.c`

Current behavior:
- Initializes the sensor through ST API firmware loading sequence
- Supports 4x4 and 8x8 zone resolution via Kconfig
- Supports continuous/autonomous ranging mode and frequency settings
- Publishes center-zone distance through `SENSOR_CHAN_DISTANCE`
- Exposes full distance matrix through:
  - `int vl53l8cx_get_distance_matrix(const struct device *dev, uint16_t *matrix);`

## Add This Driver to a Zephyr Project

This repository is module-based, so integration is straightforward.

### Method 1: Add it in `west.yml` (recommended)

In your application manifest, add this project entry:

```yaml
manifest:
  projects:
    - name: vl53l8cx_driver
      url: https://github.com/krloSc/vl53l8cx_driver.git
      revision: main
      path: modules/lib/vl53l8cx_driver
```

Then run:

```sh
west update
```

### Method 2: Use it as a Git submodule (nRF Connect for VS Code workflow)

For standalone applications (without changing the base NCS manifest files), add this repository as a git submodule inside your app workspace:

```sh
git submodule add https://github.com/krloSc/vl53l8cx_driver.git modules/lib/vl53l8cx_driver
```

This is often convenient in the nRF Connect VS Code extension when you want per-application dependency control.

If someone clones your app repository later, they must fetch submodules with:

```sh
git submodule update --init --recursive
```

## DeviceTree Overlay Example (I2C)

You also need to add the sensor in your application overlay (for example `boards/<your_board>.overlay`).

Example:

```dts
 &i2c0 {
    status = "okay";
    clock-frequency = <I2C_BITRATE_FAST>;

    vl53l8cx@29 {
        compatible = "st,vl53l8cx";
        reg = <0x29>;
        int-gpios = <&gpio0 11 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>;
        lpn-gpios = <&gpio0 12 GPIO_ACTIVE_HIGH>;
    };
};
```

Notes:
- Replace `&i2c0` with the I2C controller used by your board.
- Keep `reg = <0x29>;` unless you changed the sensor I2C address.
- If your board uses a different GPIO controller/pin mapping, update the optional GPIO phandles accordingly.

## Required Project Configuration

To make the driver work reliably, increase the main thread stack and enable the basic sensor/I2C options in `prj.conf`.

Example `prj.conf`:

```conf
CONFIG_MAIN_STACK_SIZE=2048
CONFIG_I2C=y
CONFIG_SENSOR=y
CONFIG_ST_VL53L8CX=y

# Optional tuning
CONFIG_ST_VL53L8CX_RESOLUTION_8X8=y
CONFIG_ST_VL53L8CX_MODE_CONTINUOUS=y
CONFIG_ST_VL53L8CX_RANGING_FREQUENCY=15
# CONFIG_ST_VL53L8CX_MODE_AUTONOMOUS=y
# CONFIG_ST_VL53L8CX_INTEGRATION_TIME=5
```

## Basic Usage Example (`main.c`)

```c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>
#include "st_vl53l8cx.h"

uint16_t distance_matrix_mm[64] = {0};

int main(void)
{
    const struct device *const dev = DEVICE_DT_GET_ONE(st_vl53l8cx);
    struct sensor_value value;
    int ret;

    if (!device_is_ready(dev)) {
        printk("sensor: device not ready.\n");
        return 0;
    }

    printk("VL53L8CX Zephyr Demo\n");

    while (true) {
        ret = sensor_sample_fetch(dev);
        if (ret) {
            printk("sensor_sample_fetch failed ret %d\n", ret);
            k_msleep(1000);
            continue;
        }

        ret = sensor_channel_get(dev, SENSOR_CHAN_DISTANCE, &value);
        if (ret) {
            printk("sensor_channel_get failed ret %d\n", ret);
            return 0;
        }

        printk("Distance: %d.%06d m\n", value.val1, value.val2);

        ret = vl53l8cx_get_distance_matrix(dev, distance_matrix_mm);
        if (ret) {
            printk("get_distance_matrix failed ret %d\n", ret);
            return 0;
        }

        // You can now use the distance_matrix_mm array for further processing

        k_msleep(100);
    }

    return 0;
}
```

## Notes
- ST vendor API files keep ST copyright/license headers; see `drivers/sensor/st/vl53l8cx/VL53L8_API/LICENSE.txt`.
