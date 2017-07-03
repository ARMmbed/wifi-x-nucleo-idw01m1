/* SPWFInterface Example
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

#include "SPWFSA01.h"
#include "SpwfInterface.h"
#include "mbed_debug.h"

SPWFSA01::SPWFSA01(PinName tx, PinName rx, SpwfSAInterface &ifce, bool debug)
: _serial(tx, rx, 2*1024, 2), _parser(_serial, "\x0d", "\x0a"),
  _wakeup(PC_8, 1), _reset(PC_12, 1),
  _timeout(0), _dbg_on(debug),
  _call_event_callback_blocked(false),
  _pending_sockets_bitmap(0),
  _network_lost_flag(false),
  _associated_interface(ifce),
  _callback_func(),
  _packets(0), _packets_end(&_packets)
{
    _serial.baud(115200);
    _serial.attach(Callback<void()>(this, &SPWFSA01::_event_handler));
    _parser.debugOn(debug);

    _parser.oob("+WIND:55:Pending Data", this, &SPWFSA01::_packet_handler_th);
    _parser.oob("+WIND:33:WiFi Network Lost", this, &SPWFSA01::_network_lost_handler_th);
    _parser.oob("+WIND:8:Hard Fault", this, &SPWFSA01::_hard_fault_handler);
    _parser.oob("+WIND:58:Socket Closed", this, &SPWFSA01::_sock_closed_handler);
    _parser.oob("ERROR: Pending data", this, &SPWFSA01::_pending_data_handler);
}

bool SPWFSA01::startup(int mode)
{
    /*Reset module*/
    hw_reset();

    /* factory reset */
    if(!(_parser.send("AT&F") && _recv_ok()))
    {
        debug_if(_dbg_on, "SPWF> error local echo set\r\n");
        return false;
    }

    /*set local echo to 0*/
    if(!(_parser.send("AT+S.SCFG=localecho1,0") && _recv_ok()))
    {
        debug_if(_dbg_on, "SPWF> error local echo set\r\n");
        return false;
    }

    /*set Wi-Fi mode and rate to b/g/n*/
    if(!(_parser.send("AT+S.SCFG=wifi_ht_mode,1") && _recv_ok()))
    {
        debug_if(_dbg_on, "SPWF> error setting ht_mode\r\n");
        return false;
    }

    /*set the operational rate*/
    if(!(_parser.send("AT+S.SCFG=wifi_opr_rate_mask,0x003FFFCF") && _recv_ok()))
    {
        debug_if(_dbg_on, "SPWF> error setting operational rates\r\n");
        return false;
    }

    /*set idle mode (0->idle, 1->STA,3->miniAP, 2->IBSS)*/
    if(!(_parser.send("AT+S.SCFG=wifi_mode,0") && _recv_ok()))
    {
        debug_if(_dbg_on, "SPWF> error wifi mode set\r\n");
        return false;
    }

    /* sw reset */
    reset();

#ifndef NDEBUG
    /* display all configuration values (only for debug) */
    if(!(_parser.send("AT&V") && _recv_ok()))
    {
        debug_if(_dbg_on, "SPWF> error AT&V\r\n");
        return false;
    }

    /* display the current values of all the status variables (only for debug) */
    if(!(_parser.send("AT+S.STS") && _recv_ok()))
    {
        debug_if(_dbg_on, "SPWF> error AT&V\r\n");
        return false;
    }
#endif

    return true;
}

void SPWFSA01::_wait_console_active(void) {
    while(true) {
        if (_parser.recv("+WIND:0:Console active\x0d") && _recv_delim_lf()) {
            debug_if(true, "AT^ +WIND:0:Console active\r\n"); // betzw - TODO: `true` only for debug!
            return;
        }
    }
}

bool SPWFSA01::hw_reset(void)
{
    /* reset the pin PC12 */  
    _reset.write(0);
    wait_ms(200);
    _reset.write(1); 

    _wait_console_active();
    return true;
}

bool SPWFSA01::reset(void)
{
    /* save current setting in flash */
    if(!(_parser.send("AT&W") && _parser.recv("OK")))
        {
            debug_if(_dbg_on, "SPWF> error AT&W\r\n");
            return false;
        }

    if(!_parser.send("AT+CFUN=1")) return false; /* betzw - NOTE: "keep the current state and reset the device"
                                                                   We assume that the module informs us about the
                                                                   eventual closing of sockets via "WIND" asynchronous
                                                                   indications! So everything regarding the clean-up
                                                                   of these situations is handled there. */
    _wait_console_active();
    return true;
}

/* Security Mode
   None          = 0, 
   WEP           = 1,
   WPA_Personal  = 2,
 */
bool SPWFSA01::connect(const char *ap, const char *passPhrase, int securityMode)
{
    uint32_t n1, n2, n3, n4;

    //AT+S.SCFG=wifi_wpa_psk_text,%s
    if(!(_parser.send("AT+S.SCFG=wifi_wpa_psk_text,%s", passPhrase) && _recv_ok()))
    {
        debug_if(_dbg_on, "SPWF> error pass set\r\n");
        return false;
    } 

    //AT+S.SSIDTXT=%s
    if(!(_parser.send("AT+S.SSIDTXT=%s", ap) && _recv_ok()))
    {
        debug_if(_dbg_on, "SPWF> error ssid set\r\n");
        return false;
    }

    //AT+S.SCFG=wifi_priv_mode,%d
    if(!(_parser.send("AT+S.SCFG=wifi_priv_mode,%d", securityMode) && _recv_ok()))
    {
        debug_if(_dbg_on, "SPWF> error security mode set\r\n");
        return false;
    }

    //"AT+S.SCFG=wifi_mode,%d"
    /*set wifi mode (0->idle, 1->STA,3->miniAP, 2->IBSS)*/
    if(!(_parser.send("AT+S.SCFG=wifi_mode,1") && _recv_ok()))
    {
        debug_if(_dbg_on, "SPWF> error wifi mode set\r\n");
        return false;
    }

    /* sw reset */
    reset();

    while(true)
        if(_parser.recv("+WIND:24:WiFi Up:%u.%u.%u.%u\x0d",&n1, &n2, &n3, &n4) && _recv_delim_lf())
        {
            debug_if(true, "AT^ +WIND:24:WiFi Up:%u.%u.%u.%u\r\n", n1, n2, n3, n4); // betzw - TODO: `true` only for debug!
            break;
        }

#ifndef NDEBUG
    /* trigger scan */
    if(!(_parser.send("AT+S.SCAN") && _recv_ok()))
    {
        debug_if(_dbg_on, "SPWF> error AT+S.SCAN\r\n");
        return false;
    }
#endif

    return true;
}

bool SPWFSA01::disconnect(void)
{
    //"AT+S.SCFG=wifi_mode,%d"
    /*set idle mode (0->idle, 1->STA,3->miniAP, 2->IBSS)*/
    if(!(_parser.send("AT+S.SCFG=wifi_mode,0") && _recv_ok()))
    {
        debug_if(_dbg_on, "SPWF> error wifi mode set\r\n");
        return false;
    }

    // reset module
    reset();

    return true;
}

bool SPWFSA01::dhcp(int mode)
{
    //only 3 valid modes
    //0->off(ip_addr must be set by user), 1->on(auto set by AP), 2->on&customize(miniAP ip_addr can be set by user)
    if(mode < 0 || mode > 2) {
        return false;
    }

    return _parser.send("AT+S.SCFG=ip_use_dhcp,%d", mode)
            && _recv_ok();
}


const char *SPWFSA01::getIPAddress(void)
{
    unsigned int n1, n2, n3, n4;

    if (!(_parser.send("AT+S.STS=ip_ipaddr")
            && _parser.recv("#  ip_ipaddr = %u.%u.%u.%u\x0d", &n1, &n2, &n3, &n4)
            && _recv_ok())) {
        debug_if(_dbg_on, "SPWF> getIPAddress error\r\n");
        return NULL;
    }

    debug_if(_dbg_on, "AT^ #  ip_ipaddr = %u.%u.%u.%u\r\n", n1, n2, n3, n4);

    sprintf((char*)_ip_buffer,"%u.%u.%u.%u", n1, n2, n3, n4);
    return _ip_buffer;
}

const char *SPWFSA01::getMACAddress(void)
{
    unsigned int n1, n2, n3, n4, n5, n6;

    if (!(_parser.send("AT+S.GCFG=nv_wifi_macaddr")
            && _parser.recv("#  nv_wifi_macaddr = %x:%x:%x:%x:%x:%x\x0d", &n1, &n2, &n3, &n4, &n5, &n6)
            && _recv_ok())) {
        debug_if(_dbg_on, "SPWF> getMACAddress error\r\n");
        return 0;
    }

    debug_if(_dbg_on, "AT^ #  nv_wifi_macaddr = %x:%x:%x:%x:%x:%x\r\n", n1, n2, n3, n4, n5, n6);

    sprintf((char*)_mac_buffer,"%02X:%02X:%02X:%02X:%02X:%02X", n1, n2, n3, n4, n5, n6);
    return _mac_buffer;
}

bool SPWFSA01::isConnected(void)
{
    return _associated_interface._connected_to_network;
}

bool SPWFSA01::open(const char *type, int* spwf_id, const char* addr, int port)
{
    int socket_id;
    BH_HANDLER;

    if(!_parser.send("AT+S.SOCKON=%s,%d,%s,ind", addr, port, type))
    {
        debug_if(_dbg_on, "SPWF> error opening socket\r\n");
        return false;
    }

    if(_parser.recv(" ID: %d\x0d", &socket_id)
            && _recv_ok()) {
        debug_if(_dbg_on, "AT^  ID: %d\r\n", socket_id);

        *spwf_id = socket_id;
        return true;
    }

    return false;
}

#define SPWFSA01_MAX_WRITE 4096U // betzw - WAS: 4096U // betzw - TRIAL: 64U
bool SPWFSA01::send(int spwf_id, const void *data, uint32_t amount)
{
    uint32_t sent = 0U, to_send;
    bool ret = true;
    BH_HANDLER;

    for(to_send = (amount > SPWFSA01_MAX_WRITE) ? SPWFSA01_MAX_WRITE : amount;
            sent < amount;
            to_send = ((amount - sent) > SPWFSA01_MAX_WRITE) ? SPWFSA01_MAX_WRITE : (amount - sent)) {
        if (!(_parser.send("AT+S.SOCKW=%d,%d", spwf_id, (unsigned int)to_send)
                && (_parser.write(((char*)data)+sent, (int)to_send) == (int)to_send)
                && _recv_ok())) {
            // betzw - TODO: handle different errors more accurately!
            ret = false;
            break;
        }

        sent += to_send;
    }

    return ret;
}

int SPWFSA01::_read_len(int spwf_id) {
    uint32_t amount;

    if (!(_parser.send("AT+S.SOCKQ=%d", spwf_id)
            && _parser.recv(" DATALEN: %u\x0d", &amount)
            && _recv_ok())) {
        return -1;
    }

    return (int)amount;
}

int SPWFSA01::_read_in(char* buffer, int spwf_id, uint32_t amount) {
    int ret = -1;

    MBED_ASSERT(buffer != NULL);

    /* block asynchronous indications */
    if(!_winds_off()) {
        return -1;
    }

    /* read in data */
    if (_parser.send("AT+S.SOCKR=%d,%d", spwf_id, amount)
            && (_parser.read(buffer, amount) > 0)
            && _recv_ok()) {
        ret = amount;
    }

    _winds_on();
    return ret;
}

#define WINDS_OFF "0xFFFFFFFF"
#define WINDS_ON  "0x00000000"

void SPWFSA01::_winds_on() {
    _parser.send("AT+S.SCFG=wind_off_high," WINDS_ON) && _recv_ok();
    _parser.send("AT+S.SCFG=wind_off_medium," WINDS_ON) && _recv_ok();
    _parser.send("AT+S.SCFG=wind_off_low," WINDS_ON) && _recv_ok();
}

/* Note: in case of error blocking has been (tried to be) lifted */
bool SPWFSA01::_winds_off() {
    if (!(_parser.send("AT+S.SCFG=wind_off_low," WINDS_OFF)
            && _recv_ok())) {
        _winds_on();
        return false;
    }

    if (!(_parser.send("AT+S.SCFG=wind_off_medium," WINDS_OFF)
            && _recv_ok())) {
        _winds_on();
        return false;
    }

    if (!(_parser.send("AT+S.SCFG=wind_off_high," WINDS_OFF)
            && _recv_ok())) {
        _winds_on();
        return false;
    }

    return true;
}

void SPWFSA01::_execute_bottom_halves() {
    _network_lost_handler_bh();
    _packet_handler_bh();
}

void SPWFSA01::_read_in_pending(void) {
    static int spwf_id_cnt = 0;

    while(_is_data_pending()) {
        if(_is_data_pending(spwf_id_cnt)) {
            int amount;

            amount = _read_in_packet(spwf_id_cnt);
            if(amount < 0) {
                return; /* out of memory */
            }
        }

        if(!_is_data_pending(spwf_id_cnt)) {
            spwf_id_cnt++;
            spwf_id_cnt %= SPWFSA_SOCKET_COUNT;
        }
    }
}

/* Note: returns `false` only in case of "out of memory" */
bool SPWFSA01::_read_in_packet(int spwf_id, int amount) {
    struct packet *packet = (struct packet*)malloc(sizeof(struct packet) + amount);
    if (!packet) {
#ifndef NDEBUG
        error("%s(%d): Out of memory!", __func__, __LINE__);
#else // NDEBUG
        debug("%s(%d): Out of memory!", __func__, __LINE__);
#endif
        return false; /* out of memory: give up here! */
    }

    /* init packet */
    packet->id = spwf_id;
    packet->len = (uint32_t)amount;
    packet->next = 0;

    /* read data in */
    if(!(_read_in((char*)(packet + 1), spwf_id, (uint32_t)amount) > 0)) {
        free(packet);
    } else {
        /* append to packet list */
        *_packets_end = packet;
        _packets_end = &packet->next;
    }

    return true;
}

int SPWFSA01::_read_in_packet(int spwf_id) {
    int amount;
    BlockExecuter netsock_wa_obj(Callback<void()>(this, &SPWFSA01::_unblock_event_callback),
                                 Callback<void()>(this, &SPWFSA01::_block_event_callback)); /* work around NETSOCKET's timeout bug */

    _clear_pending_data(spwf_id);
    amount = _read_len(spwf_id);
    if(amount > 0) {
        if(!_read_in_packet(spwf_id, (uint32_t)amount)) { /* out of memory */
            return -1;
        }
    }
    return amount;
}

void SPWFSA01::_free_packets(int spwf_id) {
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

void SPWFSA01::_free_all_packets() {
    for (int spwf_id = 0; spwf_id < SPWFSA_SOCKET_COUNT; spwf_id++) {
        _free_packets(spwf_id);
    }
}

/**
 *
 *	Recv Function
 *
 */
int32_t SPWFSA01::recv(int spwf_id, void *data, uint32_t amount)
{
    BH_HANDLER;

    while (true) {
        /* check if any packets are ready for us */
        for (struct packet **p = &_packets; *p; p = &(*p)->next) {
            if ((*p)->id == spwf_id) {
                debug_if(_dbg_on, "\r\n Read Done on ID %d and length of packet is %d\r\n",spwf_id,(*p)->len);
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
            if(len <= 0)  {
                return -1;
            }
        }
    }
}

bool SPWFSA01::close(int spwf_id)
{
    int amount;
    bool ret = false;

    if(spwf_id == SPWFSA_SOCKET_COUNT) {
        goto read_in_pending;
    }

    // Flush out pending data
    while(true) {
        amount = _read_in_packet(spwf_id);
        if(amount < 0) goto read_in_pending;
        if(amount == 0) break; // no more data to be read
    }

    // Close socket
    if (_parser.send("AT+S.SOCKC=%d", spwf_id)
            && _recv_ok()) {
        ret = true;
        goto read_in_pending;
    }

read_in_pending:
    /* first we need to handle a potential network loss */
    _network_lost_handler_bh();

    /* handle bottom halve of packet handler (include `_read_in_pending()`) */
    _packet_handler_bh();

    if(ret) {
        /* clear pending data flag */
        _clear_pending_data(spwf_id); // betzw: should be redundant.

        /* free packets for this socket */
        _free_packets(spwf_id);
    }

    return ret;
}

/*
 * Handling oob ("ERROR: Pending data")
 *
 */
void SPWFSA01::_pending_data_handler()
{
    debug("\r\nwarning: SPWFSA01::_pending_data_handler()\r\n");
}

/*
 * Buffered serial event handler
 *
 * Note: executed in IRQ context!
 *
 */
void SPWFSA01::_event_handler()
{
    if((bool)_callback_func && !_is_event_callback_blocked())
        _callback_func();
}

/*
 * Handling oob ("+WIND:33:WiFi Network Lost")
 *
 */
void SPWFSA01::_network_lost_handler_th()
{
#ifndef NDEBUG
    static unsigned int net_loss_cnt = 0;
    net_loss_cnt++;
#endif

    debug_if(true, "AT^ +WIND:33:WiFi Network Lost\r\n"); // betzw - TODO: `true` only for debug!

#ifndef NDEBUG
    debug_if(true, "Getting out of SPWFSA01::_network_lost_handler_th: %d\r\n", net_loss_cnt); // betzw - TODO: `true` only for debug!
#else // NDEBUG
    debug_if(true, "Getting out of SPWFSA01::_network_lost_handler_th: %d\r\n", __LINE__); // betzw - TODO: `true` only for debug!
#endif // NDEBUG

    /* set flag to signal network loss */
    _network_lost_flag = true;

    return;
}

/*
 * Handling oob ("+WIND:55:Pending Data")
 *
 */
void SPWFSA01::_packet_handler_th(void)
{
    int spwf_id;
    int amount;

    /* parse out the socket id & amount */
    if (!(_parser.recv(":%d:%d\x0d", &spwf_id, &amount) && _recv_delim_lf())) {
        return;
    }

    debug_if(_dbg_on, "AT^ +WIND:55:Pending Data:%d:%d\r\n", spwf_id, amount);

    /* set that there is pending data for socket */
    /* NOTE: it seems as if asynchronous indications might report not up-to-date data length values
     *       therefore we just record the socket id without considering the `amount` of data reported!
     */
    _set_pending_data(spwf_id);
}

void SPWFSA01::_network_lost_handler_bh()
{
    if(!_network_lost_flag) return;
    _network_lost_flag = false;

    {
        bool were_connected;
        BlockExecuter netsock_wa_obj(Callback<void()>(this, &SPWFSA01::_unblock_event_callback),
                                     Callback<void()>(this, &SPWFSA01::_block_event_callback)); /* work around NETSOCKET's timeout bug */
        Timer timer;
        timer.start();

        _parser.setTimeout(SPWF_NETLOST_TIMEOUT);

        were_connected = isConnected();
        _associated_interface._connected_to_network = false;

        if(were_connected) {
            uint32_t n1, n2, n3, n4;

            while(true) {
                if (timer.read_ms() > SPWF_CONNECT_TIMEOUT) {
                    debug_if(true, "\r\n SPWFSA01::_network_lost_handler_bh() #%d\r\n", __LINE__); // betzw - TODO: `true` only for debug!
                    goto get_out;
                }

                if((_parser.recv("+WIND:24:WiFi Up:%u.%u.%u.%u\x0d",&n1, &n2, &n3, &n4)) && _recv_delim_lf()) {
                    debug_if(true, "Re-connected (%u.%u.%u.%u)!\r\n", n1, n2, n3, n4); // betzw - TODO: `true` only for debug!

                    _associated_interface._connected_to_network = true;
                    goto get_out;
                }
            }
        } else {
            debug_if(true, "Leaving SPWFSA01::_network_lost_handler_bh\r\n"); // betzw - TODO: `true` only for debug!
            goto get_out;
        }

        get_out:
        debug_if(true, "Getting out of SPWFSA01::_network_lost_handler_bh\r\n"); // betzw - TODO: `true` only for debug!

        _parser.setTimeout(_timeout);

        return;
    }
}

/*
 * Handling oob ("+WIND:8:Hard Fault")
 *
 */
void SPWFSA01::_hard_fault_handler()
{
    int console_nr = -1;
    int reg0 = 0xFFFFFFFF,
            reg1 = 0xFFFFFFFF,
            reg2 = 0xFFFFFFFF,
            reg3 = 0xFFFFFFFF,
            reg12 = 0xFFFFFFFF;

    _parser.setTimeout(SPWF_RECV_TIMEOUT);
    _parser.recv(":Console%d: r0 %x, r1 %x, r2 %x, r3 %x, r12 %x\x0d",
                 &console_nr,
                 &reg0, &reg1, &reg2, &reg3, &reg12);
#ifndef NDEBUG
    error("\r\nSPWFSA01 hard fault error: Console%d: r0 %08X, r1 %08X, r2 %08X, r3 %08X, r12 %08X\r\n",
          console_nr,
          reg0, reg1, reg2, reg3, reg12);
#else // NDEBUG
    debug("\r\nSPWFSA01 hard fault error: Console%d: r0 %08X, r1 %08X, r2 %08X, r3 %08X, r12 %08X\r\n",
          console_nr,
          reg0, reg1, reg2, reg3, reg12);

    // This is most likely the best we can do to recover from this module hard fault
    _associated_interface.inner_constructor();
    _parser.setTimeout(_timeout);
#endif // NDEBUG
}

/*
 * Handling oob ("+WIND:58:Socket Closed")
 * when server closes a client connection
 */
void SPWFSA01::_sock_closed_handler()
{
    int spwf_id, internal_id;

    if(!(_parser.recv(":%d\x0d", &spwf_id) && _recv_delim_lf())) {
#ifndef NDEBUG
        error("\r\nSPWFSA01 %s failed!\r\n");
#endif
        return;
    }

    debug_if(true, "AT^ +WIND:58:Socket Closed:%d\r\n", spwf_id); // betzw - TODO: `true` only for debug!

    /* clear pending data flag */
    /* betzw - NOTE / TODO: do we need to read in eventually pending data from the module?
     *                      Currently, assuming that this may NOT be the case!
     */
    _clear_pending_data(spwf_id);

    /* free packets for this socket */
    _free_packets(spwf_id);

    internal_id = _associated_interface.get_internal_id(spwf_id);
    _associated_interface._ids[internal_id].internal_id = SPWFSA_SOCKET_COUNT;
    _associated_interface._ids[internal_id].spwf_id = SPWFSA_SOCKET_COUNT;
}

void SPWFSA01::setTimeout(uint32_t timeout_ms)
{
    _timeout = timeout_ms;
    _parser.setTimeout(timeout_ms);
}

bool SPWFSA01::readable()
{
    return _serial.readable();
}

bool SPWFSA01::writeable()
{
    return _serial.writeable();
}

void SPWFSA01::attach(Callback<void()> func)
{
    _callback_func = func;
}
