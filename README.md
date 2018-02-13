# Prototype Driver for STM Wi-Fi Expansion Boards based on the SPWFxx Module for STM32 Nucleo #

## Currently supported expansion boards
 * [X-NUCLEO-IDW01M1](http://www.st.com/content/st_com/en/products/ecosystems/stm32-open-development-environment/stm32-nucleo-expansion-boards/stm32-ode-connect-hw/x-nucleo-idw01m1.html), by setting `mbed` configuration variable `idw0xx1.expansion-board` to value `IDW01M1`
 * [X-NUCLEO-IDW04A1](http://www.st.com/content/st_com/en/products/ecosystems/stm32-open-development-environment/stm32-nucleo-expansion-boards/stm32-ode-connect-hw/x-nucleo-idw04a1.html), by setting `mbed` configuration variable `idw0xx1.expansion-board` to value `IDW04A1`. You might also need to define macro `IDW04A1_WIFI_HW_BUG_WA` _(see beyond)_.

## Configuration examples

### Generic concepts

For the ones, which might be less familiar with the **"The mbed configuration system"** in general, here is a [link](https://docs.mbed.com/docs/mbed-os-handbook/en/latest/advanced/config_system/) which points to the latest version of the respective _mbed OS 5 handbook tutorial_.

Furthermore, with respect to this driver, pls. refer to files [`mbed_app_idw01m1.json`](https://github.com/ARMmbed/wifi-x-nucleo-idw01m1/blob/master/mbed_app_idw01m1.json) and [`mbed_app_idw04a1.json`](https://github.com/ARMmbed/wifi-x-nucleo-idw01m1/blob/master/mbed_app_idw04a1.json) regarding additional reference for what is explained beyond.

### IDW01M1

Add the following lines to the `target_overrides`-section of your `mbed_app.json` file.

``` json
            "idw0xx1.expansion-board": "IDW01M1",
            "drivers.uart-serial-txbuf-size": 512,
            "drivers.uart-serial-rxbuf-size": 512
```

`IDW01M1` is the default value in the [`mbed_lib.json`](https://github.com/ARMmbed/wifi-x-nucleo-idw01m1/blob/master/mbed_lib.json) file, so setting the expansion board is not mandatory for `IDW01M1`, while setting the TX & RX buffer sizes is highly recommended.

### IDW04A1

Add the following lines to the `target_overrides`-section of your `mbed_app.json` file.

``` json
            "idw0xx1.expansion-board": "IDW04A1",
            "drivers.uart-serial-txbuf-size": 512,
            "drivers.uart-serial-rxbuf-size": 512
```

### Further configuration macros

All configuration options mentioned in this section are optional and when required have to be added to the `macros`-section of your `mbed_app.json` file, e.g.:

``` json
    "macros": [..., "IDW04A1_WIFI_HW_BUG_WA", "SPWFSAXX_RESET_PIN=D7"]
```

Beyond you can find the list of available configuration macros each with a short explanation:
 * `IDW04A1_WIFI_HW_BUG_WA`: activates the HW bug workaround for `IDW04A1`
 * `SPWFSAXX_WAKEUP_PIN`:    defines module wakeup pin _(requires value)_ 
 * `SPWFSAXX_RESET_PIN`:     defines module reset pin _(requires value)_ 
 * `SPWFSAXX_RTS_PIN`:       defines RTS pin of the UART device used _(requires value)_ 
 * `SPWFSAXX_CTS_PIN`:       defines CTS pin of the UART device used _(requires value)_ 

**Note**: if the values of both `SPWFSAXX_RTS_PIN` and `SPWFSAXX_CTS_PIN` are different from `NC`, hardware flow control - if available on your development board - will be enabled on the used UART device (provided you are using `mbed-os` version greater than or equal to `v5.7.0`).


## Firmware upgrade

Please make sure that you are using the latest firmware available for the expansion boards. For information on how to perform a FW upgrade you may refer to [X-CUBE-WIFI1](http://www.st.com/content/st_com/en/products/embedded-software/mcus-embedded-software/stm32-embedded-software/stm32cube-embedded-software-expansion/x-cube-wifi1.html), especially to document **"X-NUCLEO-IDW0xx1- FW upgrading over UART_v1.2.pdf"** which is contained within folder **"Documentation"** of the X-CUBE-WIFI1 software archive you need to download. 

The actual firmware `.bin` or `.hex` files can be found under 
- [STSW-WIFI001](http://www.st.com/content/st_com/en/products/embedded-software/wireless-connectivity-software/stsw-wifi001.html) _for what concerns expansion board_ X-NUCLEO-IDW01M1 _and under_
- [STSW-WIFI004](http://www.st.com/content/st_com/en/products/embedded-software/wireless-connectivity-software/stsw-wifi004.html) _when considering_ X-NUCLEO-IDW04A1.
