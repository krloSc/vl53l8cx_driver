#ifndef ZEPHYR_DRIVERS_SENSOR_VL53L8CX_H_
#define ZEPHYR_DRIVERS_SENSOR_VL53L8CX_H_
#include <zephyr/types.h>
#include "vl53l8cx_api.h"

struct st_vl53l8cx_data {
    VL53L8CX_Configuration st_dev;
    uint16_t distance_matrix_mm[VL53L8CX_RESOLUTION_8X8];
    int16_t distance_mm;
};


int vl53l8cx_get_distance_matrix(const struct device *dev, uint16_t *matrix);

#endif /* ZEPHYR_DRIVERS_SENSOR_VL53L8CX_H_ */