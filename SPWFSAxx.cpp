/* SPWFSAxx Devices
 * Copyright (c) 2015 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mbed_debug.h"

#include "SpwfSAInterface.h" /* must be included first */
#include "SPWFSAxx.h"

static const char recv_delim[] = {SPWFSAxx::_lf_, '\0'};
static const char send_delim[] = {SPWFSAxx::_cr_, '\0'};

SPWFSAxx::SPWFSAxx(PinName tx, PinName rx,
                   PinName rts, PinName cts,
                   SpwfSAInterface &ifce, bool debug,
                   PinName wakeup, PinName reset)
: _serial(tx, rx, SPWFSAXX_RXBUFFER_SZ, SPWFSAXX_TX_MULTIPLE), _parser(_serial, recv_delim, send_delim),
  _wakeup(wakeup, 1), _reset(reset, 1),
  _rts(rts), _cts(cts),
  _timeout(SPWF_INIT_TIMEOUT), _dbg_on(debug),
  _pending_sockets_bitmap(0),
  _network_lost_flag(false),
  _associated_interface(ifce),
  _call_event_callback_blocked(false),
  _callback_func(),
  _packets(0), _packets_end(&_packets),
  _err_msg_buffer(ssid_buf)
{
    _serial.baud(115200);
    _serial.attach(Callback<void()>(this, &SPWFSAxx::_event_handler));
    _parser.debugOn(debug);

    _parser.oob("+WIND:55:Pending Data", this, &SPWFSAxx::_packet_handler_th);
    _parser.oob("+WIND:58:Socket Closed", this, &SPWFSAxx::_server_gone_handler);
    _parser.oob("+WIND:33:WiFi Network Lost", this, &SPWFSAxx::_network_lost_handler_th);
    _parser.oob("+WIND:8:Hard Fault", this, &SPWFSAxx::_hard_fault_handler);
    _parser.oob("+WIND:5:WiFi Hardware Failure", this, &SPWFSAxx::_wifi_hwfault_handler);
    _parser.oob(SPWFXX_OOB_ERROR, this, &SPWFSAxx::_error_handler);
}

bool SPWFSAxx::startup(int mode)
{
    /*Reset module*/
    hw_reset();

    /* factory reset */
    if(!(_parser.send(SPWFXX_SEND_FWCFG) && _recv_ok()))
    {
        debug_if(_dbg_on, "\r\nSPWF> error restore factory default settings\r\n");
        return false;
    }

    /*switch off led*/
    if(!(_parser.send("AT+S.SCFG=blink_led,0") && _recv_ok()))
    {
        debug_if(_dbg_on, "\r\nSPWF> error stop blinking led (%d)\r\n", __LINE__);
        return false;
    }

    /*set local echo to 0*/
    if(!(_parser.send(SPWFXX_SEND_DISABLE_LE) && _recv_ok()))
    {
        debug_if(_dbg_on, "\r\nSPWF> error local echo set\r\n");
        return false;
    }

    /*set Wi-Fi mode and rate to b/g/n*/
    if(!(_parser.send("AT+S.SCFG=wifi_ht_mode,1") && _recv_ok()))
    {
        debug_if(_dbg_on, "\r\nSPWF> error setting ht_mode\r\n");
        return false;
    }

    /*set the operational rate*/
    if(!(_parser.send("AT+S.SCFG=wifi_opr_rate_mask,0x003FFFCF") && _recv_ok()))
    {
        debug_if(_dbg_on, "\r\nSPWF> error setting operational rates\r\n");
        return false;
    }

    /*set idle mode (0->idle, 1->STA,3->miniAP, 2->IBSS)*/
    if(!(_parser.send("AT+S.SCFG=wifi_mode,%d", mode) && _recv_ok()))
    {
        debug_if(_dbg_on, "\r\nSPWF> error wifi mode set idle (%d)\r\n", __LINE__);
        return false;
    }

#if !DEVICE_SERIAL_FC
    /*disable HW flow control*/
    if(!(_parser.send(SPWFXX_SEND_DISABLE_FC) && _recv_ok()))
    {
        debug_if(_dbg_on, "\r\nSPWF> error disabling HW flow control\r\n");
        return false;
    }
#else
    if((_rts != NC) && (_cts != NC)) {
        /*enable HW flow control*/
        if(!(_parser.send(SPWFXX_SEND_ENABLE_FC) && _recv_ok()))
        {
            debug_if(_dbg_on, "\r\nSPWF> error enabling HW flow control\r\n");
            return false;
        }

        /*configure pins for HW flow control*/
        _serial.set_flow_control(SerialBase::RTSCTS, _rts, _cts);
    } else {
        /*disable HW flow control*/
        if(!(_parser.send(SPWFXX_SEND_DISABLE_FC) && _recv_ok()))
        {
            debug_if(_dbg_on, "\r\nSPWF> error disabling HW flow control\r\n");
            return false;
        }
    }
#endif

    /* Disable selected WINDs */
    _winds_on();

    /* sw reset */
    reset(true);

#ifndef NDEBUG
    /* display all configuration values (only for debug) */
    if(!(_parser.send(SPWFXX_SEND_DSPLY_CFGV) && _recv_ok()))
    {
        debug_if(_dbg_on, "\r\nSPWF> error displaying all config vars\r\n");
        return false;
    }

    if (!(_parser.send(SPWFXX_SEND_GET_CONS_STATE)
            && _recv_ok())) {
        debug_if(_dbg_on, "\r\nSPWF> error getting console state\r\n");
        return false;
    }

    if (!(_parser.send(SPWFXX_SEND_GET_CONS_SPEED)
            && _recv_ok())) {
        debug_if(_dbg_on, "\r\nSPWF> error getting console speed\r\n");
        return false;
    }

    if (!(_parser.send(SPWFXX_SEND_GET_HWFC_STATE)
            && _recv_ok())) {
        debug_if(_dbg_on, "\r\nSPWF> error getting hwfc state\r\n");
        return false;
    }

    if (!(_parser.send(SPWFXX_SEND_GET_CONS_DELIM)
            && _recv_ok())) {
        debug_if(_dbg_on, "\r\nSPWF> error getting console delimiter delimiter\r\n");
        return false;
    }

    if (!(_parser.send(SPWFXX_SEND_GET_CONS_ERRS)
            && _recv_ok())) {
        debug_if(_dbg_on, "\r\nSPWF> error getting console error setting\r\n");
        return false;
    }

    if (!(_parser.send("AT+S.GCFG=sleep_enabled")
            && _recv_ok())) {
        debug_if(_dbg_on, "\r\nSPWF> error getting sleep state enabled\r\n");
        return false;
    }

    if (!(_parser.send("AT+S.GCFG=wifi_powersave")
            && _recv_ok())) {
        debug_if(_dbg_on, "\r\nSPWF> error getting powersave mode\r\n");
        return false;
    }

    if (!(_parser.send("AT+S.GCFG=standby_enabled")
            && _recv_ok())) {
        debug_if(_dbg_on, "\r\nSPWF> error getting standby state enabled\r\n");
        return false;
    }

   /* display the current values of all the status variables (only for debug) */
    if(!(_parser.send("AT+S.STS") && _recv_ok()))
    {
        debug_if(_dbg_on, "\r\nSPWF> error displaying all status vars\r\n");
        return false;
    }
#endif

    return true;
}

void SPWFSAxx::_wait_console_active(void) {
    while(true) {
        if (_parser.recv("+WIND:0:Console active%*[\x0d]") && _recv_delim_lf()) {
            debug_if(_dbg_on, "AT^ +WIND:0:Console active\r\n");
            return;
        }
    }
}

void SPWFSAxx::_wait_wifi_hw_started(void) {
    while(true) {
        if (_parser.recv("+WIND:32:WiFi Hardware Started%*[\x0d]") && _recv_delim_lf()) {
            debug_if(_dbg_on, "AT^ +WIND:32:WiFi Hardware Started\r\n");
            return;
        }
    }
}

bool SPWFSAxx::hw_reset(void)
{
#if (MBED_CONF_IDW0XX1_EXPANSION_BOARD != IDW04A1) || !defined(IDW04A1_WIFI_HW_BUG_WA) // betzw: HW reset doesn't work as expected on unmodified X_NUCLEO_IDW04A1 expansion boards
    _reset.write(0);
    wait_ms(200);
    _reset.write(1); 
#else // (MBED_CONF_IDW0XX1_EXPANSION_BOARD == IDW04A1) && defined(IDW04A1_WIFI_HW_BUG_WA): substitute with SW reset
    _parser.send(SPWFXX_SEND_SW_RESET);
#endif // (MBED_CONF_IDW0XX1_EXPANSION_BOARD == IDW04A1) && defined(IDW04A1_WIFI_HW_BUG_WA)
    _wait_console_active();
    return true;
}

bool SPWFSAxx::reset(bool wifi_on)
{
    /* save current setting in flash */
    if(!(_parser.send(SPWFXX_SEND_SAVE_SETTINGS) && _recv_ok()))
    {
        debug_if(_dbg_on, "\r\nSPWF> error saving configuration to flash\r\n");
        return false;
    }

    if(!_parser.send(SPWFXX_SEND_SW_RESET)) return false; /* betzw - NOTE: "keep the current state and reset the device".
                                                                   We assume that the module informs us about the
                                                                   eventual closing of sockets via "WIND" asynchronous
                                                                   indications! So everything regarding the clean-up
                                                                   of these situations is handled there. */
    if(wifi_on) {
        _wait_wifi_hw_started();
    } else {
        _wait_console_active();
    }

    return true;
}

/* Security Mode
   None          = 0, 
   WEP           = 1,
   WPA_Personal  = 2,
 */
bool SPWFSAxx::connect(const char *ap, const char *passPhrase, int securityMode)
{
    uint32_t n1, n2, n3, n4;

    //AT+S.SCFG=wifi_wpa_psk_text,%s
    if(!(_parser.send("AT+S.SCFG=wifi_wpa_psk_text,%s", passPhrase) && _recv_ok()))
    {
        debug_if(_dbg_on, "\r\nSPWF> error pass set\r\n");
        return false;
    } 

    //AT+S.SSIDTXT=%s
    if(!(_parser.send("AT+S.SSIDTXT=%s", ap) && _recv_ok()))
    {
        debug_if(_dbg_on, "\r\nSPWF> error ssid set\r\n");
        return false;
    }

    //AT+S.SCFG=wifi_priv_mode,%d
    if(!(_parser.send("AT+S.SCFG=wifi_priv_mode,%d", securityMode) && _recv_ok()))
    {
        debug_if(_dbg_on, "\r\nSPWF> error security mode set\r\n");
        return false;
    }

    /*set STA mode (0->idle, 1->STA,3->miniAP, 2->IBSS)*/
    if(!(_parser.send("AT+S.SCFG=wifi_mode,1") && _recv_ok()))
    {
        debug_if(_dbg_on, "\r\nSPWF> error wifi mode set 1 (STA)\r\n");
        return false;
    }

    /* sw reset */
    reset(true);

    while(true)
        if(_parser.recv(SPWFXX_RECV_WIFI_UP, &n1, &n2, &n3, &n4) && _recv_delim_lf())
        {
            debug_if(_dbg_on, "AT^ +WIND:24:WiFi Up:%u.%u.%u.%u\r\n", n1, n2, n3, n4);
            break;
        }

    return true;
}

bool SPWFSAxx::disconnect(void)
{
#if MBED_CONF_IDW0XX1_EXPANSION_BOARD == IDW04A1
    /*disable Wi-Fi device*/
    if(!(_parser.send("AT+S.WIFI=0") && _recv_ok()))
    {
        debug_if(_dbg_on, "\r\nSPWF> error disable WiFi\r\n");
        return false;
    }
#endif // IDW04A1

    /*set idle mode (0->idle, 1->STA,3->miniAP, 2->IBSS)*/
    if(!(_parser.send("AT+S.SCFG=wifi_mode,0") && _recv_ok()))
    {
        debug_if(_dbg_on, "\r\nSPWF> error wifi mode set idle (%d)\r\n", __LINE__);
        return false;
    }

    // reset module
    reset(false);

    return true;
}

const char *SPWFSAxx::getIPAddress(void)
{
    unsigned int n1, n2, n3, n4;

    if (!(_parser.send("AT+S.STS=ip_ipaddr")
            && _parser.recv(SPWFXX_RECV_IP_ADDR, &n1, &n2, &n3, &n4)
            && _recv_ok())) {
        debug_if(_dbg_on, "\r\nSPWF> get IP address error\r\n");
        return NULL;
    }

    debug_if(_dbg_on, "AT^ ip_ipaddr = %u.%u.%u.%u\r\n", n1, n2, n3, n4);

    sprintf((char*)_ip_buffer,"%u.%u.%u.%u", n1, n2, n3, n4);
    return _ip_buffer;
}

const char *SPWFSAxx::getGateway(void)
{
    unsigned int n1, n2, n3, n4;

    if (!(_parser.send("AT+S.STS=ip_gw")
            && _parser.recv(SPWFXX_RECV_GATEWAY, &n1, &n2, &n3, &n4)
            && _recv_ok())) {
        debug_if(_dbg_on, "\r\nSPWF> get gateway error\r\n");
        return NULL;
    }

    debug_if(_dbg_on, "AT^ ip_gw = %u.%u.%u.%u\r\n", n1, n2, n3, n4);

    sprintf((char*)_gateway_buffer,"%u.%u.%u.%u", n1, n2, n3, n4);
    return _gateway_buffer;
}

const char *SPWFSAxx::getNetmask(void)
{
    unsigned int n1, n2, n3, n4;

    if (!(_parser.send("AT+S.STS=ip_netmask")
            && _parser.recv(SPWFXX_RECV_NETMASK, &n1, &n2, &n3, &n4)
            && _recv_ok())) {
        debug_if(_dbg_on, "\r\nSPWF> get netmask error\r\n");
        return NULL;
    }

    debug_if(_dbg_on, "AT^ ip_netmask = %u.%u.%u.%u\r\n", n1, n2, n3, n4);

    sprintf((char*)_netmask_buffer,"%u.%u.%u.%u", n1, n2, n3, n4);
    return _netmask_buffer;
}

int8_t SPWFSAxx::getRssi(void)
{
    int ret;

    if (!(_parser.send("AT+S.PEERS=0,rx_rssi")
            && _parser.recv(SPWFXX_RECV_RX_RSSI, &ret)
            && _recv_ok())) {
        debug_if(_dbg_on, "\r\nSPWF> get RX rssi error\r\n");
        return 0;
    }

    return (int8_t)ret;
}

const char *SPWFSAxx::getMACAddress(void)
{
    unsigned int n1, n2, n3, n4, n5, n6;

    if (!(_parser.send("AT+S.GCFG=nv_wifi_macaddr")
            && _parser.recv(SPWFXX_RECV_MAC_ADDR, &n1, &n2, &n3, &n4, &n5, &n6)
            && _recv_ok())) {
        debug_if(_dbg_on, "\r\nSPWF> get MAC address error\r\n");
        return 0;
    }

    debug_if(_dbg_on, "AT^ nv_wifi_macaddr = %x:%x:%x:%x:%x:%x\r\n", n1, n2, n3, n4, n5, n6);

    sprintf((char*)_mac_buffer,"%02X:%02X:%02X:%02X:%02X:%02X", n1, n2, n3, n4, n5, n6);
    return _mac_buffer;
}

bool SPWFSAxx::isConnected(void)
{
    return _associated_interface._connected_to_network;
}

bool SPWFSAxx::send(int spwf_id, const void *data, uint32_t amount)
{
    uint32_t sent = 0U, to_send;
    bool ret = true;

    for(to_send = (amount > SPWFSAXX_TXBUFFER_SZ) ? SPWFSAXX_TXBUFFER_SZ : amount;
            sent < amount;
            to_send = ((amount - sent) > SPWFSAXX_TXBUFFER_SZ) ? SPWFSAXX_TXBUFFER_SZ : (amount - sent)) {
        {
            BH_HANDLER;

            if (!(_parser.send("AT+S.SOCKW=%d,%d", spwf_id, (unsigned int)to_send)
                    && (_parser.write(((char*)data)+sent, (int)to_send) == (int)to_send)
                    && _recv_ok())) {
                // betzw - TODO: handle different errors more accurately!
                ret = false;
                break;
            }
        }

        sent += to_send;
    }

    return ret;
}

int SPWFSAxx::_read_len(int spwf_id) {
    uint32_t amount;

    if (!(_parser.send("AT+S.SOCKQ=%d", spwf_id)
            && _parser.recv(SPWFXX_RECV_DATALEN, &amount)
            && _recv_ok())) {
        return 0;
    }

    return (int)amount;
}

#define SPWFXX_WINDS_OFF "0xFFFFFFFF"

void SPWFSAxx::_winds_on(void) {
    _parser.send(SPWFXX_SEND_WIND_OFF_HIGH SPWFXX_WINDS_HIGH_ON) && _recv_ok();
    _parser.send(SPWFXX_SEND_WIND_OFF_MEDIUM SPWFXX_WINDS_MEDIUM_ON) && _recv_ok();
    _parser.send(SPWFXX_SEND_WIND_OFF_LOW SPWFXX_WINDS_LOW_ON) && _recv_ok();
}

/* Note: in case of error blocking has been (tried to be) lifted */
bool SPWFSAxx::_winds_off(void) {
    if (!(_parser.send(SPWFXX_SEND_WIND_OFF_LOW SPWFXX_WINDS_OFF)
            && _recv_ok())) {
        _winds_on();
        return false;
    }

    if (!(_parser.send(SPWFXX_SEND_WIND_OFF_MEDIUM SPWFXX_WINDS_OFF)
            && _recv_ok())) {
        _winds_on();
        return false;
    }

    if (!(_parser.send(SPWFXX_SEND_WIND_OFF_HIGH SPWFXX_WINDS_OFF)
            && _recv_ok())) {
        _winds_on();
        return false;
    }

    return true;
}

void SPWFSAxx::_execute_bottom_halves(void) {
    _network_lost_handler_bh();
    _packet_handler_bh();
}

void SPWFSAxx::_read_in_pending(void) {
    static int internal_id_cnt = 0;

    while(_is_data_pending()) {
        if(_associated_interface._socket_has_connected(internal_id_cnt)) {
            int spwf_id = _associated_interface._ids[internal_id_cnt].spwf_id;

            if(_is_data_pending(spwf_id)) {
                int amount;

                amount = _read_in_packet(spwf_id);
                if(amount < 0) {
                    return; /* out of memory */
                }
            }

            if(!_is_data_pending(spwf_id)) {
                internal_id_cnt++;
                internal_id_cnt %= SPWFSA_SOCKET_COUNT;
            }
        } else {
            internal_id_cnt++;
            internal_id_cnt %= SPWFSA_SOCKET_COUNT;
        }
    }
}

/* Note: returns
 * '1'  in case of success
 * '0'  in case of `_read_in()` error
 * '-1' in case of "out of memory"
 */
int SPWFSAxx::_read_in_packet(int spwf_id, int amount) {
    struct packet *packet = (struct packet*)malloc(sizeof(struct packet) + amount);
    if (!packet) {
#ifndef NDEBUG
        error("%s(%d): Out of memory!", __func__, __LINE__);
#else // NDEBUG
        debug("%s(%d): Out of memory!", __func__, __LINE__);
#endif
        return -1; /* out of memory: give up here! */
    }

    /* init packet */
    packet->id = spwf_id;
    packet->len = (uint32_t)amount;
    packet->next = 0;

    /* read data in */
    if(!(_read_in((char*)(packet + 1), spwf_id, (uint32_t)amount) > 0)) {
        free(packet);
        return 0;
    } else {
        /* append to packet list */
        *_packets_end = packet;
        _packets_end = &packet->next;

        /* force call of (external) callback */
        _call_callback();
    }

    return 1;
}

/* Note: returns
 * '>0'  in case of success, amount of read in data (in bytes)
 * '0'   in case of `_read_in()` error no more data to be read
 * '-1'  in case of "out of memory"
 */
int SPWFSAxx::_read_in_packet(int spwf_id) {
    int amount;
    BlockExecuter netsock_wa_obj(Callback<void()>(this, &SPWFSAxx::_unblock_event_callback),
                                 Callback<void()>(this, &SPWFSAxx::_block_event_callback)); /* call (external) callback only while not receiving */

    _clear_pending_data(spwf_id);
    amount = _read_len(spwf_id);
    if(amount > 0) {
        int ret = _read_in_packet(spwf_id, (uint32_t)amount);
        if(ret <= 0) { /* "out of memory" or `_read_in()` error */
            return ret;
        }
    }
    return amount;
}

void SPWFSAxx::_free_packets(int spwf_id) {
    // check if any packets are ready for `spwf_id`
    for(struct packet **p = &_packets; *p;) {
        if ((*p)->id == spwf_id) {
            struct packet *q = *p;
            if (_packets_end == &(*p)->next) {
                _packets_end = p;
            }
            *p = (*p)->next;
            free(q);
        } else {
            p = &(*p)->next;
        }
    }
}

void SPWFSAxx::_free_all_packets() {
    for (int spwf_id = 0; spwf_id < SPWFSA_SOCKET_COUNT; spwf_id++) {
        _free_packets(spwf_id);
    }
}

/**
 *	Recv Function
 */
int32_t SPWFSAxx::recv(int spwf_id, void *data, uint32_t amount)
{
    BH_HANDLER;

    while (true) {
        /* check if any packets are ready for us */
        for (struct packet **p = &_packets; *p; p = &(*p)->next) {
            if ((*p)->id == spwf_id) {
                debug_if(_dbg_on, "\r\nRead done on ID %d and length of packet is %d\r\n",spwf_id,(*p)->len);
                struct packet *q = *p;
                if (q->len <= amount) { // Return and remove full packet
                    memcpy(data, q+1, q->len);

                    if (_packets_end == &(*p)->next) {
                        _packets_end = p;
                    }
                    *p = (*p)->next;
                    uint32_t len = q->len;
                    free(q);
                    return len;
                }
                else { // return only partial packet
                    memcpy(data, q+1, amount);
                    q->len -= amount;
                    memmove(q+1, (uint8_t*)(q+1) + amount, q->len);
                    return amount;
                }
            }
        }

        /* check for pending data on module */
        {
            int len;

            len = _read_in_packet(spwf_id);
            if(len <= 0)  { /* "out of memory", `_read_in()` error, or no more data to be read */
                return -1;
            }
        }
    }
}

#define CLOSE_MAX_RETRY (3)
bool SPWFSAxx::close(int spwf_id)
{
    bool ret = false;
    int retry_cnt = 0;

    if(spwf_id == SPWFSA_SOCKET_COUNT) {
        goto close_bh_handling;
    }

close_flush:
    // Flush out pending data
    while(true) {
        int amount = _read_in_packet(spwf_id);
        if(amount < 0) goto close_bh_handling; // out of memory
        if(amount == 0) break; // no more data to be read or `_read_in()` error

        /* immediately free packet (to avoid "out of memory") */
        _free_packets(spwf_id);

        /* interleave bottom halves */
        _execute_bottom_halves();
    }

    // Close socket
    if (_parser.send("AT+S.SOCKC=%d", spwf_id)
            && _recv_ok()) {
        ret = true;
        goto close_bh_handling;
    } else {
        if(retry_cnt++ < CLOSE_MAX_RETRY) {
            /* interleave bottom halves */
            _execute_bottom_halves();

            /* retry flushing (assuming data is still pending) */
            goto close_flush;
        }
    }

close_bh_handling:
    /* anticipate bottom halves */
    _execute_bottom_halves();

    if(ret) {
        /* clear pending data flag (should be redundant) */
        _clear_pending_data(spwf_id);

        /* free packets for this socket */
        _free_packets(spwf_id);
    } else {
        debug_if(_dbg_on, "\r\nSPWF> SPWFSAxx::close failed  (%d)\r\n", __LINE__);
    }

    return ret;
}

/*
 * Buffered serial event handler
 *
 * Note: executed in IRQ context!
 * Note: call (external) callback only while not receiving
 */
void SPWFSAxx::_event_handler(void)
{
    if(!_is_event_callback_blocked()) {
        _call_callback();
    }
}

/*
 * Common error handler
 */
void SPWFSAxx::_error_handler(void)
{
    if(_parser.recv("%[^\x0d]%*[\x0d]", _err_msg_buffer) && _recv_delim_lf()) {
        debug_if(_dbg_on, "AT^ ERROR:%s (%d)\r\n", _err_msg_buffer, __LINE__);
    } else {
        debug_if(_dbg_on, "\r\nSPWF> Unknown ERROR string in SPWFSAxx::_error_handler (%d)\r\n", __LINE__);
    }

    /* force call of (external) callback */
    _call_callback();
}

/*
 * Handling oob ("+WIND:33:WiFi Network Lost")
 */
void SPWFSAxx::_network_lost_handler_th(void)
{
#ifndef NDEBUG
    static unsigned int net_loss_cnt = 0;
    net_loss_cnt++;
#endif

    _recv_delim_cr_lf();

    debug_if(_dbg_on, "AT^ +WIND:33:WiFi Network Lost\r\n");

#ifndef NDEBUG
    debug_if(_dbg_on, "Getting out of SPWFSAxx::_network_lost_handler_th: %d\r\n", net_loss_cnt);
#else // NDEBUG
    debug_if(_dbg_on, "Getting out of SPWFSAxx::_network_lost_handler_th: %d\r\n", __LINE__);
#endif // NDEBUG

    /* set flag to signal network loss */
    _network_lost_flag = true;

    return;
}

/*
 * Handling oob ("+WIND:55:Pending Data")
 */
void SPWFSAxx::_packet_handler_th(void)
{
    int spwf_id;
    int amount;

    /* parse out the socket id & amount */
    if (!(_parser.recv(SPWFXX_RECV_PENDING_DATA, &spwf_id, &amount) && _recv_delim_lf())) {
#ifndef NDEBUG
        error("\r\nSPWFSAxx::%s failed!\r\n", __func__);
#endif
        return;
    }

    debug_if(_dbg_on, "AT^ +WIND:55:Pending Data:%d:%d\r\n", spwf_id, amount);

    /* set that there is pending data for socket */
    /* NOTE: it seems as if asynchronous indications might report not up-to-date data length values
     *       therefore we just record the socket id without considering the `amount` of data reported!
     */
    _set_pending_data(spwf_id);
}

void SPWFSAxx::_network_lost_handler_bh(void)
{
    if(!_network_lost_flag) return;
    _network_lost_flag = false;

    {
        bool were_connected;
        BlockExecuter netsock_wa_obj(Callback<void()>(this, &SPWFSAxx::_unblock_and_callback),
                                     Callback<void()>(this, &SPWFSAxx::_block_event_callback)); /* call (external) callback only while not receiving */
        Timer timer;
        timer.start();

        _parser.setTimeout(SPWF_NETLOST_TIMEOUT);

        were_connected = isConnected();
        _associated_interface._connected_to_network = false;

        if(were_connected) {
            uint32_t n1, n2, n3, n4;

            while(true) {
                if (timer.read_ms() > SPWF_CONNECT_TIMEOUT) {
                    debug_if(_dbg_on, "\r\nSPWFSAxx::_network_lost_handler_bh() #%d\r\n", __LINE__);
                    disconnect();
                    goto nlh_get_out;
                }

                if((_parser.recv(SPWFXX_RECV_WIFI_UP, &n1, &n2, &n3, &n4)) && _recv_delim_lf()) {
                    debug_if(_dbg_on, "Re-connected (%u.%u.%u.%u)!\r\n", n1, n2, n3, n4);

                    _associated_interface._connected_to_network = true;
                    goto nlh_get_out;
                }
            }
        } else {
            debug_if(_dbg_on, "Leaving SPWFSAxx::_network_lost_handler_bh\r\n");
            goto nlh_get_out;
        }

    nlh_get_out:
        debug_if(_dbg_on, "Getting out of SPWFSAxx::_network_lost_handler_bh\r\n");
        _parser.setTimeout(_timeout);

        return;
    }
}

void SPWFSAxx::_recover_from_hard_faults(void) {
    disconnect();
    _associated_interface.inner_constructor();
    _free_all_packets();

    /* force call of (external) callback */
    _call_callback();
}

/*
 * Handling oob ("+WIND:8:Hard Fault")
 */
void SPWFSAxx::_hard_fault_handler(void)
{
    _parser.setTimeout(SPWF_RECV_TIMEOUT);
    if(_parser.recv("%[^\x0d]%*[\x0d]", _err_msg_buffer) && _recv_delim_lf()) {}

#ifndef NDEBUG
    error("\r\nSPWFSAXX hard fault error:\r\n%s\r\n", _err_msg_buffer);
#else // NDEBUG
    debug("\r\nSPWFSAXX hard fault error:\r\n%s\r\n", _err_msg_buffer);

    // This is most likely the best we can do to recover from this module hard fault
    _parser.setTimeout(SPWF_HF_TIMEOUT);
    _recover_from_hard_faults();
    _parser.setTimeout(_timeout);
#endif // NDEBUG

    /* force call of (external) callback */
    _call_callback();
}

/*
 * Handling oob ("+WIND:5:WiFi Hardware Failure")
 */
void SPWFSAxx::_wifi_hwfault_handler(void)
{
    unsigned int failure_nr;

    /* parse out the socket id & amount */
    _parser.recv(":%u%*[\x0d]", &failure_nr);
    _recv_delim_lf();

#ifndef NDEBUG
    error("\r\nSPWFSAXX wifi HW fault error: %u\r\n", failure_nr);
#else // NDEBUG
    debug("\r\nSPWFSAXX wifi HW fault error: %u\r\n", failure_nr);

    // This is most likely the best we can do to recover from this module hard fault
    _parser.setTimeout(SPWF_HF_TIMEOUT);
    _recover_from_hard_faults();
    _parser.setTimeout(_timeout);
#endif // NDEBUG

    /* force call of (external) callback */
    _call_callback();
}

/*
 * Handling oob ("+WIND:58:Socket Closed")
 * when server closes a client connection
 *
 * NOTE: When a socket client receives an indication about socket server gone (only for TCP sockets, WIND:58),
 *       the socket connection is NOT automatically closed!
 */
void SPWFSAxx::_server_gone_handler(void)
{
    int spwf_id, internal_id;

    if(!(_parser.recv(SPWFXX_RECV_SOCKET_CLOSED, &spwf_id) && _recv_delim_lf())) {
#ifndef NDEBUG
        error("\r\nSPWFSAxx::%s failed!\r\n", __func__);
#endif
        goto _get_out;
    }

    debug_if(_dbg_on, "AT^ +WIND:58:Socket Closed:%d\r\n", spwf_id);

    /* check for the module to report a valid id */
    MBED_ASSERT(((unsigned int)spwf_id) < ((unsigned int)SPWFSA_SOCKET_COUNT));

    /* only set `server_gone`
     * user still can receive date & must still explicitly close the socket
     */
    internal_id = _associated_interface.get_internal_id(spwf_id);
    if(internal_id != SPWFSA_SOCKET_COUNT) {
        _associated_interface._ids[internal_id].server_gone = true;
    }

_get_out:
    /* force call of (external) callback */
    _call_callback();
}

void SPWFSAxx::setTimeout(uint32_t timeout_ms)
{
    _timeout = timeout_ms;
    _parser.setTimeout(timeout_ms);
}

void SPWFSAxx::attach(Callback<void()> func)
{
    _callback_func = func; /* call (external) callback only while not receiving */
}
