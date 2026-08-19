/**
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include "platform.h"

LOG_MODULE_REGISTER(st_vl53l8cx_platform, CONFIG_SENSOR_LOG_LEVEL);

#define VL53L8CX_I2C_CHUNK_SIZE 1024U

uint8_t VL53L8CX_RdByte(
		VL53L8CX_Platform *p_platform,
		uint16_t RegisterAddress,
		uint8_t *p_value)
{
	uint8_t status = 255;
	
	status = VL53L8CX_RdMulti(p_platform, RegisterAddress, p_value, 1);

	return status;
}

uint8_t VL53L8CX_WrByte(
		VL53L8CX_Platform *p_platform,
		uint16_t RegisterAddress,
		uint8_t value)
{
	uint8_t tx[3] = {
                (uint8_t)((RegisterAddress >> 8) & 0xFFU),
                (uint8_t)(RegisterAddress & 0xFFU),
                value,
        };
    return i2c_write(p_platform->i2c_dev, tx, sizeof(tx), p_platform->address);
}

uint8_t VL53L8CX_WrMulti(
		VL53L8CX_Platform *p_platform,
		uint16_t RegisterAddress,
		uint8_t *p_values,
		uint32_t size)
{
	uint32_t offset = 0U;

    while (offset < size) {
        uint16_t reg = (uint16_t)(RegisterAddress + offset);
        uint32_t chunk = size - offset;
        uint8_t reg_buffer[2];
        struct i2c_msg msgs[2];

        if (chunk > VL53L8CX_I2C_CHUNK_SIZE) {
            chunk = VL53L8CX_I2C_CHUNK_SIZE;
        }

        reg_buffer[0] = (uint8_t)((reg >> 8) & 0xFFU);
        reg_buffer[1] = (uint8_t)(reg & 0xFFU);

        // Mensaje 1: Enviar la dirección del registro de 16 bits
        msgs[0].buf = reg_buffer;
        msgs[0].len = 2;
        msgs[0].flags = I2C_MSG_WRITE;

        // Mensaje 2: Enviar el bloque de datos continuo
        msgs[1].buf = &p_values[offset];
        msgs[1].len = (uint16_t)chunk;
        msgs[1].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

        int ret = i2c_transfer(p_platform->i2c_dev, msgs, 2, p_platform->address);
        if (ret != 0) {
            LOG_ERR("WrMulti I2C error reg=0x%04X chunk=%u ret=%d", reg, (unsigned int)chunk, ret);
            return 1;
        }

        offset += chunk;
    }

    return 0; // ST espera 0 para éxito, 1 para error
}

uint8_t VL53L8CX_RdMulti(
		VL53L8CX_Platform *p_platform,
		uint16_t RegisterAddress,
		uint8_t *p_values,
		uint32_t size)
{
	uint32_t offset = 0U;

    while (offset < size) {
        uint16_t reg = (uint16_t)(RegisterAddress + offset);
        uint32_t chunk = size - offset;
        uint8_t reg_buffer[2];
        struct i2c_msg msgs[2];

        if (chunk > VL53L8CX_I2C_CHUNK_SIZE) {
            chunk = VL53L8CX_I2C_CHUNK_SIZE;
        }

        reg_buffer[0] = (uint8_t)((reg >> 8) & 0xFFU);
        reg_buffer[1] = (uint8_t)(reg & 0xFFU);

        // Mensaje 1: Escribir el registro que queremos leer
        msgs[0].buf = reg_buffer;
        msgs[0].len = 2;
        msgs[0].flags = I2C_MSG_WRITE;

        // Mensaje 2: Leer los datos que nos mande el sensor
        msgs[1].buf = &p_values[offset];
        msgs[1].len = (uint16_t)chunk;
        msgs[1].flags = I2C_MSG_RESTART | I2C_MSG_READ | I2C_MSG_STOP;

        int ret = i2c_transfer(p_platform->i2c_dev, msgs, 2, p_platform->address);
        if (ret != 0) {
            LOG_ERR("RdMulti I2C error reg=0x%04X chunk=%u ret=%d", reg, (unsigned int)chunk, ret);
            return 1;
        }

        offset += chunk;
    }

    return 0;
}

uint8_t VL53L8CX_Reset_Sensor(
		VL53L8CX_Platform *p_platform)
{
	if (p_platform == NULL) {
        return 1;
    }

    if (p_platform->lpn != NULL) {
        gpio_pin_set(p_platform->lpn, p_platform->lpn_pin, 0);
        VL53L8CX_WaitMs(p_platform, 100);

        gpio_pin_set(p_platform->lpn, p_platform->lpn_pin, 1);
        VL53L8CX_WaitMs(p_platform, 100);
    }

    return 0;
}

void VL53L8CX_SwapBuffer(
		uint8_t 		*buffer,
		uint16_t 	 	 size)
{
	uint32_t i, tmp;
	
	/* Example of possible implementation using <string.h> */
	for(i = 0; i < size; i = i + 4) 
	{
		tmp = (
		  buffer[i]<<24)
		|(buffer[i+1]<<16)
		|(buffer[i+2]<<8)
		|(buffer[i+3]);
		
		memcpy(&(buffer[i]), &tmp, 4);
	}
}	

uint8_t VL53L8CX_WaitMs(
		VL53L8CX_Platform *p_platform,
		uint32_t TimeMs)
{

	k_msleep(TimeMs);
	
	return 0;
}
