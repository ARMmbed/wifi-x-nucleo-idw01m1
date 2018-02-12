#ifndef SPWFSAXX_AT_STRINGS_H
#define SPWFSAXX_AT_STRINGS_H

/* Define beyond macro if your X-NUCLEO-IDW04A1 expansion board has NOT the `WIFI_RST` HW patch applied on it */
// #define IDW04A1_WIFI_HW_BUG_WA // delegated to mbed config system

#if defined(TARGET_FF_ARDUINO)

#if !defined(SPWFSAXX_WAKEUP_PIN)
#define SPWFSAXX_WAKEUP_PIN   A3
#endif
#if !defined(SPWFSAXX_RESET_PIN)
#ifndef IDW04A1_WIFI_HW_BUG_WA
#define SPWFSAXX_RESET_PIN    D7
#else // IDW04A1_WIFI_HW_PATCH
#define SPWFSAXX_RESET_PIN    NC
#endif // !IDW04A1_WIFI_HW_PATCH
#endif

#else // !defined(TARGET_FF_ARDUINO)

#if !defined(SPWFSAXX_WAKEUP_PIN)
#define SPWFSAXX_WAKEUP_PIN   NC
#endif
#if !defined(SPWFSAXX_RESET_PIN)
#define SPWFSAXX_RESET_PIN    NC
#endif

#endif // !defined(TARGET_FF_ARDUINO)

#define SPWFXX_SEND_RECV_PKTSIZE    (730)

#define SPWFXX_OOB_ERROR            "AT-S.ERROR:"                                           // "ERROR:"

#define SPWFXX_RECV_OK              "AT-S.OK\n"                                             // "OK\n"
#define SPWFXX_RECV_WIFI_UP         "+WIND:24:WiFi Up:%*u:%u.%u.%u.%u\n"                    // "+WIND:24:WiFi Up:%u.%u.%u.%u\n"
#define SPWFXX_RECV_IP_ADDR         "AT-S.Var:ip_ipaddr=%u.%u.%u.%u\n"                      // "#  ip_ipaddr = %u.%u.%u.%u\n"
#define SPWFXX_RECV_GATEWAY         "AT-S.Var:ip_gw=%u.%u.%u.%u\n"                          // "#  ip_gw = %u.%u.%u.%u\n"
#define SPWFXX_RECV_NETMASK         "AT-S.Var:ip_netmask=%u.%u.%u.%u\n"                     // "#  ip_netmask = %u.%u.%u.%u\n"
#define SPWFXX_RECV_RX_RSSI         "AT-S.Var:0.rx_rssi=%d\n"                               // "#  0.rx_rssi = %d\n"
#define SPWFXX_RECV_MAC_ADDR        "AT-S.Var:nv_wifi_macaddr=%x:%x:%x:%x:%x:%x\n"          // "#  nv_wifi_macaddr = %x:%x:%x:%x:%x:%x\n"
#define SPWFXX_RECV_DATALEN         "AT-S.Query:%u\n"                                       // " DATALEN: %u\n"
#define SPWFXX_RECV_PENDING_DATA    "::%u:%*u:%u\n"                                         // ":%d:%d\n"
#define SPWFXX_RECV_SOCKET_CLOSED   ":%u:%*u\n"                                             // ":%d\n"

#define SPWFXX_SEND_FWCFG           "AT+S.FCFG"                                             // "AT&F"
#define SPWFXX_SEND_DISABLE_LE      "AT+S.SCFG=console_echo,0"                              // "AT+S.SCFG=localecho1,0"
#define SPWFXX_SEND_DSPLY_CFGV      "AT+S.GCFG"                                             // "AT&V"
#define SPWFXX_SEND_GET_CONS_STATE  "AT+S.GCFG=console_enabled"                             // "AT+S.GCFG=console1_enabled"
#define SPWFXX_SEND_GET_CONS_SPEED  "AT+S.GCFG=console_speed"                               // "AT+S.GCFG=console1_speed"
#define SPWFXX_SEND_GET_HWFC_STATE  "AT+S.GCFG=console_hwfc"                                // "AT+S.GCFG=console1_hwfc"
#define SPWFXX_SEND_GET_CONS_DELIM  "AT+S.GCFG=console_delimiter"                           // "AT+S.GCFG=console1_delimiter"
#define SPWFXX_SEND_GET_CONS_ERRS   "AT+S.GCFG=console_errs"                                // "AT+S.GCFG=console1_errs"
#define SPWFXX_SEND_DISABLE_FC      "AT+S.SCFG=console_hwfc,0"                              // "AT+S.SCFG=console1_hwfc,0"
#define SPWFXX_SEND_ENABLE_FC       "AT+S.SCFG=console_hwfc,1"                              // "AT+S.SCFG=console1_hwfc,1"
#define SPWFXX_SEND_SW_RESET        "AT+S.RESET"                                            // "AT+CFUN=1"
#define SPWFXX_SEND_SAVE_SETTINGS   "AT+S.WCFG"                                             // "AT&W"
#define SPWFXX_SEND_WIND_OFF_HIGH   "AT+S.SCFG=console_wind_off_high,"                      // "AT+S.SCFG=wind_off_high,"
#define SPWFXX_SEND_WIND_OFF_MEDIUM "AT+S.SCFG=console_wind_off_medium,"                    // "AT+S.SCFG=wind_off_medium,"
#define SPWFXX_SEND_WIND_OFF_LOW    "AT+S.SCFG=console_wind_off_low,"                       // "AT+S.SCFG=wind_off_low,"

#define SPWFXX_WINDS_HIGH_ON        "0x00100000"                                            // "0x00000000"
#define SPWFXX_WINDS_MEDIUM_ON      "0x80000000"                                            // "0x00000000"

#endif // SPWFSAXX_AT_STRINGS_H
