#include <zephyr/types.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include "vl53l8cx_api.h"
#include "st_vl53l8cx.h"
#include "platform.h"

#define DT_DRV_COMPAT st_vl53l8cx
#define SPIOP	SPI_WORD_SET(8) | SPI_TRANSFER_MSB

LOG_MODULE_REGISTER(st_vl53l8cx, CONFIG_SENSOR_LOG_LEVEL);

#if DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 0
#warning "Custom VL53L8CX driver enabled without any devices"
#endif


struct st_vl53l8cx_config {
	struct i2c_dt_spec i2c;
};

static int st_vl53l8cx_sample_fetch(const struct device *dev,
                      enum sensor_channel chan)
{
    VL53L8CX_ResultsData results;
    uint8_t data_ready;
    uint8_t resolution;
    uint8_t status;
    uint16_t zone_count;
    struct st_vl53l8cx_data *data;

    if (dev == NULL) {
        return -EINVAL;
    }

    if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_DISTANCE) {
        return -ENOTSUP;
    }

    data = dev->data;


    status = vl53l8cx_check_data_ready(&data->st_dev, &data_ready);
    if (status != 0U) {
        return -EIO;
    }

    if (data_ready == 0U) {
        return -EAGAIN;
    }

    status = vl53l8cx_get_ranging_data(&data->st_dev, &results);
    if (status != 0U) {
        return -EIO;
    }

    status = vl53l8cx_get_resolution(&data->st_dev, &resolution);
    if (status != 0U) {
        return -EIO;
    }

    zone_count = (resolution == VL53L8CX_RESOLUTION_8X8) ? 64U : 16U;
    for (uint16_t i = 0U; i < zone_count; i++) {
        uint16_t distance_mm = 0U;

        if (results.distance_mm[i] > 0) {
            distance_mm = (uint16_t)results.distance_mm[i];
        }

        data->distance_matrix_mm[i] = distance_mm;
    }
#ifdef CONFIG_ST_VL53L8CX_RESOLUTION_8X8
    data->distance_mm = results.distance_mm[VL53L8CX_RESOLUTION_8X8 / 2]; // save the distance of the center zone for 8x8 resolution
#else
    data->distance_mm = results.distance_mm[VL53L8CX_RESOLUTION_4X4 / 2]; // save the distance of the center zone for 4x4 resolution
#endif
    return 0;
}

static int st_vl53l8cx_channel_get(const struct device *dev,
                     enum sensor_channel chan,
                     struct sensor_value *val)
{

    struct st_vl53l8cx_data *data;
    uint32_t distance_mm;

    if (dev == NULL || val == NULL) {
        return -EINVAL;
    }

    if (chan != SENSOR_CHAN_DISTANCE) {
        return -ENOTSUP;
    }

    data = dev->data;
    distance_mm = data->distance_mm;

    val->val1 = (int32_t)(distance_mm / 1000U);
    val->val2 = (int32_t)((distance_mm % 1000U) * 1000U);

    return 0;
}

int vl53l8cx_get_distance_matrix(const struct device *dev, uint16_t *matrix)
{
    struct st_vl53l8cx_data *data;

    if (dev == NULL || matrix == NULL) {
        return -EINVAL;
    }

    data = dev->data;

    memcpy(matrix, data->distance_matrix_mm, sizeof(data->distance_matrix_mm));

    return 0;
}

static const struct sensor_driver_api st_vl53l8cx_api = {
	.sample_fetch = &st_vl53l8cx_sample_fetch,
	.channel_get = &st_vl53l8cx_channel_get,
};

static int st_vl53l8cx_init(const struct device *dev)
{
    LOG_DBG("st_vl53l8cx_init() called");
    const struct st_vl53l8cx_config *config = dev->config;
    struct st_vl53l8cx_data *data = dev->data;
    uint8_t is_alive;
    uint8_t status;

    LOG_DBG("Inicializando VL53L8CX...");

    // Check if the I2C bus is ready
    if (!i2c_is_ready_dt(&config->i2c)) {
        LOG_ERR("I2C bus %s is not ready", config->i2c.bus->name);
        return -ENODEV;
    }

    /* Maps the Zephyr I2C device to the ST driver platform structure */
    data->st_dev.platform.i2c_dev = config->i2c.bus;
    data->st_dev.platform.address = config->i2c.addr;

    /* Ping the sensor to check if it's alive */
    status = vl53l8cx_is_alive(&data->st_dev, &is_alive);
    if (status != 0 || !is_alive) {
        LOG_ERR("VL53L8CX not detected at I2C address 0x%02X", config->i2c.addr);
        return -ENODEV;
    }

    /* Load the internal firmware of the sensor (heavy function from the ST API) */
    status = vl53l8cx_init(&data->st_dev);
    if (status != 0) {
        LOG_ERR("Failed to initialize VL53L8CX firmware");
        return -EIO;
    }

    /* Configure the resolution based on the user's choice in Kconfig */
#ifdef CONFIG_ST_VL53L8CX_RESOLUTION_8X8
    vl53l8cx_set_resolution(&data->st_dev, VL53L8CX_RESOLUTION_8X8);
#else
    vl53l8cx_set_resolution(&data->st_dev, VL53L8CX_RESOLUTION_4X4);
#endif

    LOG_DBG("VL53L8CX initialized successfully");
    // getting raging mode
    uint8_t ranging_mode;

    // Setting ranging mode according to configuration
#ifdef CONFIG_ST_VL53L8CX_MODE_CONTINUOUS
    status = vl53l8cx_set_ranging_mode(&data->st_dev, VL53L8CX_RANGING_MODE_CONTINUOUS);
#else
    status = vl53l8cx_set_ranging_mode(&data->st_dev, VL53L8CX_RANGING_MODE_AUTONOMOUS);
#endif

    if (status != 0) {
        LOG_WRN("Failed to set VL53L8CX ranging mode");
    }


#ifdef CONFIG_ST_VL53L8CX_MODE_AUTONOMOUS
    if (status == 0) {
        // Setting integration time according to configuration
        status = vl53l8cx_set_integration_time_ms(&data->st_dev, CONFIG_ST_VL53L8CX_INTEGRATION_TIME);
        if (status != 0) {
            LOG_WRN("Failed to set VL53L8CX integration time to %d ms", CONFIG_ST_VL53L8CX_INTEGRATION_TIME);
        }
    }
#endif

    // Setting ranging frequency according to configuration
    status = vl53l8cx_set_ranging_frequency_hz(&data->st_dev, CONFIG_ST_VL53L8CX_RANGING_FREQUENCY);
    if (status != 0) {
        LOG_WRN("Failed to set VL53L8CX ranging frequency to %d Hz", CONFIG_ST_VL53L8CX_RANGING_FREQUENCY);
    }

    // Getting the current ranging mode
    status = vl53l8cx_get_ranging_mode(&data->st_dev, &ranging_mode);
    if (status != 0) {
        LOG_WRN("Failed to get VL53L8CX ranging mode");
    } else {
        LOG_DBG("VL53L8CX ranging mode: %s", (ranging_mode == VL53L8CX_RANGING_MODE_CONTINUOUS) ? "Continuous" : "Autonomous");
    }

#ifdef CONFIG_ST_VL53L8CX_MODE_AUTONOMOUS
    // getting the current integration time
    uint32_t integration_time_ms;
    status = vl53l8cx_get_integration_time_ms(&data->st_dev, &integration_time_ms);
    if (status != 0) {
        LOG_WRN("Failed to get VL53L8CX integration time");
    } else {
        LOG_DBG("VL53L8CX integration time: %d ms", integration_time_ms);
    }
#endif

#ifdef CONFIG_ST_VL53L8CX_TARGET_ORDER_STRONGEST
    // Setting target order to strongest
    status = vl53l8cx_set_target_order(&data->st_dev, VL53L8CX_TARGET_ORDER_STRONGEST);
    if (status != 0) {
        LOG_WRN("Failed to set VL53L8CX target order to strongest");
    }
#else
    // Setting target order to closest
    status = vl53l8cx_set_target_order(&data->st_dev, VL53L8CX_TARGET_ORDER_CLOSEST);
    if (status != 0) {
        LOG_WRN("Failed to set VL53L8CX target order to closest");
    }
#endif

    // getting the current ranging frequency
    uint8_t ranging_frequency_hz;
    status = vl53l8cx_get_ranging_frequency_hz(&data->st_dev, &ranging_frequency_hz);
    if (status != 0) {
        LOG_WRN("Failed to get VL53L8CX ranging frequency");
    } else {
        LOG_DBG("VL53L8CX ranging frequency: %d Hz", ranging_frequency_hz);
    }

    // Start ranging
    status = vl53l8cx_start_ranging(&data->st_dev);
    if (status != 0) {
        LOG_ERR("Failed to start VL53L8CX ranging");
        return -EIO;
    }

    return 0;
}

#define ST_VL53L8CX_DEFINE(inst)                                            \
    static struct st_vl53l8cx_data st_vl53l8cx_data_##inst;                 \
    static const struct st_vl53l8cx_config st_vl53l8cx_config_##inst = {    \
        .i2c = I2C_DT_SPEC_INST_GET(inst),                           \
    };                                                                      \
                                                                            \
    DEVICE_DT_INST_DEFINE(inst,                                             \
                st_vl53l8cx_init,										    \
                NULL,														\
                &st_vl53l8cx_data_##inst,									\
                &st_vl53l8cx_config_##inst,									\
                POST_KERNEL, 												\
                CONFIG_SENSOR_INIT_PRIORITY, 								\
                &st_vl53l8cx_api);

DT_INST_FOREACH_STATUS_OKAY(ST_VL53L8CX_DEFINE)