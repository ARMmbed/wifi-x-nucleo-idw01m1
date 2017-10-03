#ifndef SPWFSAXX_AT_STRINGS_H
#define SPWFSAXX_AT_STRINGS_H

#if defined(TARGET_FF_MORPHO)

#if !defined(SPWFSAXX_WAKEUP_PIN)
#define SPWFSAXX_WAKEUP_PIN   PC_8                                              // A3
#endif // !defined(SPWFSAXX_WAKEUP_PIN)
#if !defined(SPWFSAXX_RESET_PIN)
#define SPWFSAXX_RESET_PIN    PC_12                                             // D7 / NC
#endif // !defined(SPWFSAXX_RESET_PIN)

#else // !defined(TARGET_FF_MORPHO)

#if !defined(SPWFSAXX_WAKEUP_PIN)
#define SPWFSAXX_WAKEUP_PIN   NC                                                // A3
#endif // !defined(SPWFSAXX_WAKEUP_PIN)
#if !defined(SPWFSAXX_RESET_PIN)
#define SPWFSAXX_RESET_PIN    NC                                                // D7 / NC
#endif // !defined(SPWFSAXX_RESET_PIN)

#endif // !defined(TARGET_FF_MORPHO)

#define SPWFSAXX_TX_MULTIPLE (1)
#define SPWFSAXX_RXBUFFER_SZ (730U)
#define SPWFSAXX_TXBUFFER_SZ (SPWFSAXX_RXBUFFER_SZ * SPWFSAXX_TX_MULTIPLE)

#define SPWFXX_OOB_ERROR            "ERROR:"                                            // "AT-S.ERROR:"

#define SPWFXX_RECV_OK              "OK%*[\x0d]"                                        // "AT-S.OK%*[\x0d]"
#define SPWFXX_RECV_WIFI_UP         "+WIND:24:WiFi Up:%u.%u.%u.%u%*[\x0d]"              // "+WIND:24:WiFi Up:%*u:%u.%u.%u.%u%*[\x0d]"
#define SPWFXX_RECV_IP_ADDR         "#  ip_ipaddr = %u.%u.%u.%u%*[\x0d]"                // "AT-S.Var:ip_ipaddr=%u.%u.%u.%u%*[\x0d]"
#define SPWFXX_RECV_GATEWAY         "#  ip_gw = %u.%u.%u.%u%*[\x0d]"                    // "AT-S.Var:ip_gw=%u.%u.%u.%u%*[\x0d]"
#define SPWFXX_RECV_NETMASK         "#  ip_netmask = %u.%u.%u.%u%*[\x0d]"               // "AT-S.Var:ip_netmask=%u.%u.%u.%u%*[\x0d]"
#define SPWFXX_RECV_RX_RSSI         "#  0.rx_rssi = %d%*[\x0d]"                         // "AT-S.Var:0.rx_rssi=%d%*[\x0d]"
#define SPWFXX_RECV_MAC_ADDR        "#  nv_wifi_macaddr = %x:%x:%x:%x:%x:%x%*[\x0d]"    // "AT-S.Var:nv_wifi_macaddr=%x:%x:%x:%x:%x:%x%*[\x0d]"
#define SPWFXX_RECV_DATALEN         " DATALEN: %u%*[\x0d]"                              // "AT-S.Query:%u%*[\x0d]"
#define SPWFXX_RECV_PENDING_DATA    ":%d:%d%*[\x0d]"                                    // "::%u:%*u:%u%*[\x0d]"
#define SPWFXX_RECV_SOCKET_CLOSED   ":%d%*[\x0d]"                                       // ":%u:%*u%*[\x0d]"

#define SPWFXX_SEND_FWCFG           "AT&F"                                              // "AT+S.FCFG"
#define SPWFXX_SEND_DISABLE_LE      "AT+S.SCFG=localecho1,0"                            // "AT+S.SCFG=console_echo,0"
#define SPWFXX_SEND_DSPLY_CFGV      "AT&V"                                              // "AT+S.GCFG"
#define SPWFXX_SEND_GET_CONS_STATE  "AT+S.GCFG=console1_enabled"                        // "AT+S.GCFG=console_enabled"
#define SPWFXX_SEND_GET_CONS_SPEED  "AT+S.GCFG=console1_speed"                          // "AT+S.GCFG=console_speed"
#define SPWFXX_SEND_GET_HWFC_STATE  "AT+S.GCFG=console1_hwfc"                           // "AT+S.GCFG=console_hwfc"
#define SPWFXX_SEND_GET_CONS_DELIM  "AT+S.GCFG=console1_delimiter"                      // "AT+S.GCFG=console_delimiter"
#define SPWFXX_SEND_GET_CONS_ERRS   "AT+S.GCFG=console1_errs"                           // "AT+S.GCFG=console_errs"
#define SPWFXX_SEND_DISABLE_FC      "AT+S.SCFG=console1_hwfc,0"                         // "AT+S.SCFG=console_hwfc,0"
#define SPWFXX_SEND_ENABLE_FC       "AT+S.SCFG=console1_hwfc,1"                         // "AT+S.SCFG=console_hwfc,1"
#define SPWFXX_SEND_SW_RESET        "AT+CFUN=1"                                         // "AT+S.RESET"
#define SPWFXX_SEND_SAVE_SETTINGS   "AT&W"                                              // "AT+S.WCFG"
#define SPWFXX_SEND_WIND_OFF_HIGH   "AT+S.SCFG=wind_off_high,"                          // "AT+S.SCFG=console_wind_off_high,"
#define SPWFXX_SEND_WIND_OFF_MEDIUM "AT+S.SCFG=wind_off_medium,"                        // "AT+S.SCFG=console_wind_off_medium,"
#define SPWFXX_SEND_WIND_OFF_LOW    "AT+S.SCFG=wind_off_low,"                           // "AT+S.SCFG=console_wind_off_low,"

#define SPWFXX_WINDS_HIGH_ON        "0x00000000"                                        // "0x00100000"
#define SPWFXX_WINDS_MEDIUM_ON      "0x00000000"                                        // "0x80000000"
#define SPWFXX_WINDS_LOW_ON         "0x00000000"

#endif // SPWFSAXX_AT_STRINGS_H
