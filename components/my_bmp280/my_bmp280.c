#include "my_bmp280.h"

#include "i2c_bus.h"

esp_err_t bmp280_init(bmp280_t *dev, i2c_bus_t *bus, uint8_t addr)
{
    i2c_device_config_t cfg = {
        .device_address = addr,
        .scl_speed_hz = 400000,
    };

    dev->bus = bus;

    bmp280_read_trim(dev);
    return i2c_master_bus_add_device(bus->handle, &cfg, &dev->handle);
}

esp_err_t bmp280_read_raw(bmp280_t *dev)
{
    uint8_t reg = BMP280_REG_PRESS_MSB;
    uint8_t data[6] = {0, 0, 0, 0, 0, 0};

    esp_err_t err = i2c_master_transmit_receive(dev->handle, &reg, 1, data, sizeof(data), -1);
    if (err != ESP_OK) {
        return err;
    }

    dev->raw_data.press = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | (data[2] >> 4);
    dev->raw_data.temp = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) | (data[5] >> 4);    

    return ESP_OK;
}

esp_err_t bmp280_read_trim(bmp280_t *dev)
{
    uint8_t reg = BMP280_REG_TRIM_START;
    uint8_t data[24] = {0};

    esp_err_t err = i2c_master_transmit_receive(dev->handle, &reg, 1, data, sizeof(data), -1);
    if (err != ESP_OK) {
        return err;
    }

    dev->calib_data.dig_T1 = (uint16_t)(data[0] | (data[1] << 8));
    dev->calib_data.dig_T2 = (int16_t)(data[2] | (data[3] << 8));
    dev->calib_data.dig_T3 = (int16_t)(data[4] | (data[5] << 8));
    dev->calib_data.dig_P1 = (uint16_t)(data[6] | (data[7] << 8));
    dev->calib_data.dig_P2 = (int16_t)(data[8] | (data[9] << 8));
    dev->calib_data.dig_P3 = (int16_t)(data[10] | (data[11] << 8));
    dev->calib_data.dig_P4 = (int16_t)(data[12] | (data[13] << 8));
    dev->calib_data.dig_P5 = (int16_t)(data[14] | (data[15] << 8));
    dev->calib_data.dig_P6 = (int16_t)(data[16] | (data[17] << 8));
    dev->calib_data.dig_P7 = (int16_t)(data[18] | (data[19] << 8));
    dev->calib_data.dig_P8 = (int16_t)(data[20] | (data[21] << 8));
    dev->calib_data.dig_P9 = (int16_t)(data[22] | (data[23] << 8));

    return ESP_OK;
}

esp_err_t bmp280_compensate(bmp280_t *dev)
{
    int32_t var1, var2, t_fine;

    var1 = ((((dev->raw_data.temp >> 3) - ((int32_t)dev->calib_data.dig_T1 << 1))) * ((int32_t)dev->calib_data.dig_T2)) >> 11;
    var2 = (((((dev->raw_data.temp >> 4) - ((int32_t)dev->calib_data.dig_T1)) * ((dev->raw_data.temp >> 4) - ((int32_t)dev->calib_data.dig_T1))) >> 12) * ((int32_t)dev->calib_data.dig_T3)) >> 14;
    t_fine = var1 + var2;
    dev->res_data.temp = (t_fine * 5 + 128) >> 8;

    int64_t var1_p, var2_p;
    var1_p = ((int64_t)t_fine) - 128000;
    var2_p = var1_p * var1_p * (int64_t)dev->calib_data.dig_P6;
    var2_p = var2_p + ((var1_p * (int64_t)dev->calib_data.dig_P5) << 17);
    var2_p = var2_p + (((int64_t)dev->calib_data.dig_P4) << 35);
    var1_p = ((var1_p * var1_p * (int64_t)dev->calib_data.dig_P3) >> 8) + ((var1_p * (int64_t)dev->calib_data.dig_P2) << 12);
    var1_p = (((((int64_t)1) << 47) + var1_p)) * ((int64_t)dev->calib_data.dig_P1) >> 33;

    if (var1_p == 0) {
        return ESP_ERR_INVALID_ARG; // avoid exception caused by division by zero
    }

    int64_t p_acc = 1048576 - dev->raw_data.press;
    p_acc = (((p_acc << 31) - var2_p) * 3125) / var1_p;
    var1_p = (((int64_t)dev->calib_data.dig_P9) * (p_acc >> 13) * (p_acc >> 13)) >> 25;
    var2_p = (((int64_t)dev->calib_data.dig_P8) * p_acc) >> 19;
    dev->res_data.press = ((p_acc + var1_p + var2_p) >> 8) + (((int64_t)dev->calib_data.dig_P7) << 4);

    return ESP_OK;
}


esp_err_t bmp280_output(bmp280_t *dev)
{
    bmp280_read_raw(dev);
    bmp280_compensate(dev);

    printf("Temperature: %.2f °C\n", dev->res_data.temp / 100.0);
    printf("Pressure: %.2f hPa\n", dev->res_data.press / 25600.0);
    return ESP_OK;
}