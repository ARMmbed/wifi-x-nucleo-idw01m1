# Prototype Driver for STM Wi-Fi Expansion Boards based on the SPWFxx Module for STM32 Nucleo #

Currently supported boards:
 * [X-NUCLEO-IDW01M1](http://www.st.com/content/st_com/en/products/ecosystems/stm32-open-development-environment/stm32-nucleo-expansion-boards/stm32-ode-connect-hw/x-nucleo-idw01m1.html), by setting `mbed` configuration variable `idw0xx1.expansion-board` to value `IDW01M1`
 * [X-NUCLEO-IDW04A1](http://www.st.com/content/st_com/en/products/ecosystems/stm32-open-development-environment/stm32-nucleo-expansion-boards/stm32-ode-connect-hw/x-nucleo-idw04a1.html), by setting `mbed` configuration variable `idw0xx1.expansion-board` to value `IDW04A1`. You might also need to define macro `IDW04A1_WIFI_HW_BUG_WA`.
