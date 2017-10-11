# Prototype Driver for STM Wi-Fi Expansion Boards based on the SPWFxx Module for STM32 Nucleo #

## Currently supported boards
 * [X-NUCLEO-IDW01M1](http://www.st.com/content/st_com/en/products/ecosystems/stm32-open-development-environment/stm32-nucleo-expansion-boards/stm32-ode-connect-hw/x-nucleo-idw01m1.html), by setting `mbed` configuration variable `idw0xx1.expansion-board` to value `IDW01M1`
 * [X-NUCLEO-IDW04A1](http://www.st.com/content/st_com/en/products/ecosystems/stm32-open-development-environment/stm32-nucleo-expansion-boards/stm32-ode-connect-hw/x-nucleo-idw04a1.html), by setting `mbed` configuration variable `idw0xx1.expansion-board` to value `IDW04A1`. You might also need to define macro `IDW04A1_WIFI_HW_BUG_WA`.

## Configuration examples

### IDW01M1

Add the following line to the `target_overrides`-section of your `mbed_app.json` file.

``` json
            "idw0xx1.expansion-board": "IDW01M1"
```

`IDW01M1` is the default value in the [mbed_lib.json](https://github.com/ARMmbed/wifi-x-nucleo-idw01m1/blob/master/mbed_lib.json), so this is not mandatory for `IDW01M1`.


### IDW04A1

Add the following line to the `target_overrides`-section of your `mbed_app.json` file.

``` json
            "idw0xx1.expansion-board": "IDW04A1"
```

### Adding the HW-workaround for IDW04A1

You can add the HW workaround acivation macro to the `macros`-section of your `mbed_app.json` file.

``` json
    "macros": [..., "IDW04A1_WIFI_HW_BUG_WA"]
```

## Firmware upgrade

Please make sure that you are using the latest firmware available for the expansion boards. For information on how to perform a FW upgrade you may refer to [X-CUBE-WIFI1](http://www.st.com/content/st_com/en/products/embedded-software/mcus-embedded-software/stm32-embedded-software/stm32cube-embedded-software-expansion/x-cube-wifi1.html), especially to document **"X-NUCLEO-IDW0xx1- FW upgrading over UART_v1.2.pdf"** which is contained within folder **"Documentation"** of the X-CUBE-WIFI1 software archive you need to download. 

The actual firmware `.bin` or `.hex` files can be found under 
- [STSW-WIFI001](https://my.st.com/content/my_st_com/en/products/embedded-software/wireless-connectivity-software/stsw-wifi001.html) _for what concerns expansion board_ X-NUCLEO-IDW01M1 _and under_
- [STSW-WIFI004](https://my.st.com/content/my_st_com/en/products/embedded-software/wireless-connectivity-software/stsw-wifi004.html) _when considering_ X-NUCLEO-IDW04A1.
