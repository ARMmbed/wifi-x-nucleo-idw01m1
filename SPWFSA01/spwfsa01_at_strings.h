#ifndef SPWFSAXX_AT_STRINGS_H
#define SPWFSAXX_AT_STRINGS_H

#if !defined(SPWFSAXX_WAKEUP_PIN)
#define SPWFSAXX_WAKEUP_PIN   PC_8
#endif
#if !defined(SPWFSAXX_RESET_PIN)
#define SPWFSAXX_RESET_PIN    PC_12
#endif

#define SPWFSAXX_TX_MULTIPLE (1)
#define SPWFSAXX_RXBUFFER_SZ (730U)
#define SPWFSAXX_TXBUFFER_SZ (SPWFSAXX_RXBUFFER_SZ * SPWFSAXX_TX_MULTIPLE)

#define BH_HANDLER \
        BlockExecuter bh_handler(Callback<void()>(this, &SPWFSAXX::_execute_bottom_halves))

#define SPWFXX_OOB_PENDING_DATA     "+WIND:55:Pending Data"
#define SPWFXX_OOB_SOCKET_CLOSED    "+WIND:58:Socket Closed"
#define SPWFXX_OOB_NET_LOST         "+WIND:33:WiFi Network Lost"
#define SPWFXX_OOB_HARD_FAULT       "+WIND:8:Hard Fault"
#define SPWFXX_OOB_HW_FAILURE       "+WIND:5:WiFi Hardware Failure"
#define SPWFXX_OOB_ERROR            "ERROR:"

#define SPWFXX_RECV_OK              "OK%*[\x0d]"

#endif SPWFSAXX_AT_STRINGS_H
