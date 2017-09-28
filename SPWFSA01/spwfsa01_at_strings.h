#ifndef SPWFSAXX_AT_STRINGS_H
#define SPWFSAXX_AT_STRINGS_H

#if !defined(SPWFSAXX_WAKEUP_PIN)
#define SPWFSAXX_WAKEUP_PIN   PC_8                                              // A3
#endif
#if !defined(SPWFSAXX_RESET_PIN)
#define SPWFSAXX_RESET_PIN    PC_12                                             // D7 / NC
#endif

#define SPWFSAXX_TX_MULTIPLE (1)
#define SPWFSAXX_RXBUFFER_SZ (730U)
#define SPWFSAXX_TXBUFFER_SZ (SPWFSAXX_RXBUFFER_SZ * SPWFSAXX_TX_MULTIPLE)

#define BH_HANDLER \
        BlockExecuter bh_handler(Callback<void()>(this, &SPWFSAxx::_execute_bottom_halves))

#define SPWFXX_OOB_ERROR            "ERROR:"                                    // "AT-S.ERROR:"

#define SPWFXX_RECV_OK              "OK%*[\x0d]"                                // "AT-S.OK%*[\x0d]"

#define SPWFXX_SEND_FWCFG           "AT&F"                                      // "AT+S.FCFG"
#define SPWFXX_SEND_LEOUT           "AT+S.SCFG=localecho1,0"                    // "AT+S.SCFG=console_echo,0"
#define SPWFXX_SEND_MODE_RATE       "AT+S.SCFG=wifi_ht_mode,1"                  // "AT+S.SCFG=wifi_ht_mode,1"
#define SPWFXX_SEND_OP_MODE         "AT+S.SCFG=wifi_opr_rate_mask,0x003FFFCF"   // "AT+S.SCFG=wifi_opr_rate_mask,0x003FFFCF"
#define SPWFXX_SEND_WIFI_MODE       "AT+S.SCFG=wifi_mode,%d"                    // "AT+S.SCFG=wifi_mode,%d"
#define SPWFXX_SEND_DISABLE_HWFC    "AT+S.SCFG=console1_hwfc,0"                 // "AT+S.SCFG=console_hwfc,0"
#define SPWFXX_SEND_ENABLE_HWFC     "AT+S.SCFG=console1_hwfc,1"                 // "AT+S.SCFG=console_hwfc,1"
#define SPWFXX_SEND_DSPLY_CFGV      "AT&V"                                      // "AT+S.GCFG"
#define SPWFXX_SEND_GET_CONS_STATE  "AT+S.GCFG=console1_enabled"                // "AT+S.GCFG=console_enabled"
#define SPWFXX_SEND_GET_CONS_SPEED  "AT+S.GCFG=console1_speed"                  // "AT+S.GCFG=console_speed"
#define SPWFXX_SEND_GET_HWFC_STATE  "AT+S.GCFG=console1_hwfc"                   // "AT+S.GCFG=console_hwfc"
#define SPWFXX_SEND_GET_CONS_DELIM  "AT+S.GCFG=console1_delimiter"              // "AT+S.GCFG=console_delimiter"
#define SPWFXX_SEND_GET_CONS_ERRS   "AT+S.GCFG=console1_errs"                   // "AT+S.GCFG=console_errs"
#define SPWFXX_SEND_GET_SLEEP_ENBLD "AT+S.GCFG=sleep_enabled"                   // "AT+S.GCFG=sleep_enabled"
#define SPWFXX_SEND_GET_PWS_MODE    "AT+S.GCFG=wifi_powersave"                  // "AT+S.GCFG=wifi_powersave"
#define SPWFXX_SEND_GET_STBY_ENBLD  "AT+S.GCFG=standby_enabled"                 // "AT+S.GCFG=standby_enabled"

#endif // SPWFSAXX_AT_STRINGS_H
