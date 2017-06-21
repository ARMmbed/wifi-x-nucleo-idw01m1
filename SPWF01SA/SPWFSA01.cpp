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
: _serial(tx, rx, 2*1024, 2), _parser(_serial, "\r", "\n"),
  _wakeup(PC_8, 1), _reset(PC_12, 1),
  _rx_sem(0), _release_rx_sem(false),
  _disassoc_handler_recursive_cnt(-1),
  _timeout(0), _dbg_on(debug),
  _send_at(false), _read_in_pending_blocked(false),
  _call_event_callback_blocked_cnt(0),
  _pending_sockets_bitmap(0),
  _associated_interface(ifce),
  _callback_func(),
  _packets(0), _packets_end(&_packets)
{
    _serial.baud(115200);
    _serial.attach(Callback<void()>(this, &SPWFSA01::_event_handler));
    _parser.debugOn(debug);

    _parser.oob("+WIND:55:Pending Data", this, &SPWFSA01::_packet_handler);
    _parser.oob("+WIND:41:WiFi Disassociation", this, &SPWFSA01::_disassociation_handler);
    _parser.oob("+WIND:8:Hard Fault", this, &SPWFSA01::_hard_fault_handler);
    _parser.oob("+WIND:58:Socket Closed", this, &SPWFSA01::_sock_closed_handler);
    _parser.oob("ERROR: Pending data", this, &SPWFSA01::_pending_data_handler);
}

bool SPWFSA01::startup(int mode)
{
    /*Reset module*/
    hw_reset();
    reset();

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

    /* set number of consecutive loss beacon to detect the AP disassociation */
    if(!(_parser.send("AT+S.SCFG=wifi_beacon_loss_thresh,10") && _recv_ok()))
    {
        debug_if(_dbg_on, "SPWF> error wifi beacon loss thresh set\r\n");
        return false;
    }

    /*set idle mode (0->idle, 1->STA,3->miniAP, 2->IBSS)*/
    if(!(_parser.send("AT+S.SCFG=wifi_mode,%d", mode) && _recv_ok()))
    {
        debug_if(_dbg_on, "SPWF> error wifi mode set\r\n");
        return false;
    }

#ifndef NDEBUG
    /* display all configuration values (only for debug) */
    if(!(_parser.send("AT&V") && _recv_ok()))
    {
        debug_if(_dbg_on, "SPWF> error AT&V\r\n");
        return false;
    }
#endif

    return true;
}

void SPWFSA01::_wait_console_active(void) {
    while(true) {
        if (_parser.recv("+WIND:0:Console active\r") && _recv_delim()) {
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
    if(!_parser.send("AT+CFUN=0")) return false;
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
    /*set idle mode (0->idle, 1->STA,3->miniAP, 2->IBSS)*/
    if(!(_parser.send("AT+S.SCFG=wifi_mode,1") && _recv_ok()))
    {
        debug_if(_dbg_on, "SPWF> error wifi mode set\r\n");
        return false;
    }

    while(true)
        if(_parser.recv("+WIND:24:WiFi Up:%u.%u.%u.%u\r",&n1, &n2, &n3, &n4) && _recv_delim())
        {
            break;
        }

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
            && _parser.recv("#  ip_ipaddr = %u.%u.%u.%u\r", &n1, &n2, &n3, &n4)
            && _recv_ok())) {
        debug_if(_dbg_on, "SPWF> getIPAddress error\r\n");
        return NULL;
    }

    sprintf((char*)_ip_buffer,"%u.%u.%u.%u", n1, n2, n3, n4);

    return _ip_buffer;
}

const char *SPWFSA01::getMACAddress(void)
{
    unsigned int n1, n2, n3, n4, n5, n6;

    if (!(_parser.send("AT+S.GCFG=nv_wifi_macaddr")
            && _parser.recv("#  nv_wifi_macaddr = %x:%x:%x:%x:%x:%x\r", &n1, &n2, &n3, &n4, &n5, &n6)
            && _recv_ok())) {
        debug_if(_dbg_on, "SPWF> getMACAddress error\r\n");
        return 0;
    }

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

    if(!_parser.send("AT+S.SOCKON=%s,%d,%s,ind", addr, port, type))
    {
        debug_if(_dbg_on, "SPWF> error opening socket\r\n");
        return false;
    }

    if(_parser.recv(" ID: %d\r", &socket_id)
            && _recv_ok()) {
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

    /* block from calling `_read_in_pending()` in packet handler */
    _block_read_in_pending();

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

    /* unblock `_read_in_pending()` */
    _unblock_read_in_pending();

    /* read in eventually pending data */
    _read_in_pending();

    return ret;
}

int SPWFSA01::_read_len(int spwf_id) {
    uint32_t amount;

    /* block from calling `_read_in_pending()` in packet handler */
    _block_read_in_pending();

    if (!(_parser.send("AT+S.SOCKQ=%d", spwf_id)
            && _parser.recv(" DATALEN: %u\r", &amount)
            && _recv_ok())) {
        /* unblock `_read_in_pending()` */
        _unblock_read_in_pending();

        return -1;
    }

    /* unblock `_read_in_pending()` */
    _unblock_read_in_pending();

    return (int)amount;
}

int SPWFSA01::_read_in(char* buffer, int spwf_id, uint32_t amount) {
    MBED_ASSERT(buffer != NULL);

    /* block asynchronous indications */
    if(_block_async_indications() != 0) return -1;

    /* read in data */
    if (!(_parser.send("+S.SOCKR=%d,%d", spwf_id, amount)
            && (_parser.read(buffer, amount) > 0)
            && _recv_ok())) {
        /* Note: not sure if block of async indications has been lifted at this point */
        return -1;
    } else {
        /* Note: block of async indications has been lifted at this point */
        return amount;
    }
}

/* Note: in case of error (return -1) blocking has been (tried to be) lifted */
int SPWFSA01::_block_async_indications() {
    /* Send 'AT' without delimiter */
    if(_parser.printf("AT") <= 0) return -1;

    /* Wait for command being sent */
    {
        Timer timer;
        timer.start();

        while (_serial.pending()) {
            if (timer.read_ms() > _timeout) {
                /* try to unblock asynchronous indications */
                _parser.send("");
                _recv_ok();
                return -1;
            }
        }
    }

    /* block from calling `_read_in_pending()` in packet handler */
    _block_read_in_pending();

    /* Read all pending indications (by receiving anything) */
    while(_serial.readable()) _parser.recv("betzw"); // Note: "betzw" is just a non-empty placeholder

    /* unblock `_read_in_pending()` */
    _unblock_read_in_pending();

    return 0;
}

void SPWFSA01::_packet_handler(void)
{
    int spwf_id;
    int amount;
    bool local_send_at;

    /* parse out the socket id & amount */
    if (!(_parser.recv(":%d:%d\r", &spwf_id, &amount) && _recv_delim())) {
        return;
    }

    /* set that ther is pending data for socket */
    /* NOTE: it seems as if asynchronous indications might report not up-to-date data length values
     *       therefore we just record the socket id without considering the `amount` of data reported!
     */
    _set_pending_data(spwf_id);

    if(_is_read_in_pending_blocked()) {
        MBED_ASSERT(!_send_at);
        return;
    }

    /* pre-handling of `_send_at` */
    local_send_at = _send_at;
    _send_at = false;

    /* read in other eventually pending packages */
    _read_in_pending();

    /* send 'AT' command to trigger an 'OK' response */
    if(local_send_at) {
        _parser.send("AT");
    }
}

void SPWFSA01::_read_in_pending(void) {
    static int spwf_id_cnt = 0;

    MBED_ASSERT(!_is_read_in_pending_blocked() && !_send_at);

    while(_is_data_pending()) {
        if(_is_data_pending(spwf_id_cnt)) {
            int amount;

            _clear_pending_data(spwf_id_cnt);
            amount = _read_len(spwf_id_cnt);
            if(amount > 0) {
                if(!_read_in_packet(spwf_id_cnt, (uint32_t)amount)) { /* out of memory */
                    return;
                }
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

/**
 *
 *	Recv Function
 *
 */
int32_t SPWFSA01::recv(int spwf_id, void *data, uint32_t amount)
{
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

            bool ret;
            int len;
            BlockExecuter netsock_wa_obj(Callback<void()>(this, &SPWFSA01::_unblock_event_callback),
                                         Callback<void()>(this, &SPWFSA01::_block_event_callback)); /* work around NETSOCKET's timeout bug */

            _clear_pending_data(spwf_id);
            len = _read_len(spwf_id);
            if(len > 0)  {
                /* read in pending packet */
                ret = !_read_in_packet(spwf_id, (uint32_t)len);
                if(ret) { /* out of memory */
                    return -1;
                }
            } else {
                return -1;
            }
        }
    }
}

bool SPWFSA01::close(int spwf_id)
{
    int amount;
    bool ret = false;

    if(spwf_id == SPWFSA_SOCKET_COUNT) return false;

    // Flush out pending data
    while(true) {
        BlockExecuter netsock_wa_obj(Callback<void()>(this, &SPWFSA01::_unblock_event_callback),
                                     Callback<void()>(this, &SPWFSA01::_block_event_callback)); /* work around NETSOCKET's timeout bug */

        if((amount = _read_len(spwf_id)) < 0) goto read_in_pending;
        if(amount == 0) break; // no more data to be read
        if(!_read_in_packet(spwf_id, (uint32_t)amount)) {
            goto read_in_pending; /* out of memory */
        }
    }

    // Close socket
    if (_parser.send("AT+S.SOCKC=%d", spwf_id)
            && _recv_ok()) {
        ret = true;
        goto read_in_pending;
    }

read_in_pending:
    /* read in eventually pending data */
    _read_in_pending();

    if(ret) {
        /* free packets for this socket */
        _free_packets(spwf_id);
    }

    return ret;
}

/*
 * Handling oob ("Error: Pending Data")
 *
 */
void SPWFSA01::_pending_data_handler()
{
#ifndef NDEBUG
    error("\r\n SPWFSA01::_pending_data_handler()\r\n");
#else // NDEBUG
    debug("\r\n SPWFSA01::_pending_data_handler()\r\n");
#endif // NDEBUG
}

/*
 * Buffered serial event handler
 *
 * Note: executed in IRQ context!
 *
 */
void SPWFSA01::_event_handler()
{
    if(_release_rx_sem)
        _rx_sem.release();
    if((bool)_callback_func && !_is_event_callback_blocked())
        _callback_func();
}

/*
 * Handling oob ("+WIND:33:WiFi Network Lost")
 *
 */
void SPWFSA01::_disassociation_handler()
{
    int reason;
    uint32_t n1, n2, n3, n4;
    int saved_timeout = _timeout;
    bool were_connected;

#ifndef NDEBUG
    static unsigned int disassoc_cnt = 0;
    disassoc_cnt++;
#endif

    were_connected = isConnected();
    _associated_interface._connected_to_network = false;

    _disassoc_handler_recursive_cnt++;
    setTimeout(SPWF_DISASSOC_TIMEOUT);

    // parse out reason
    if (!(_parser.recv(": %d\r", &reason) && _recv_delim())) {
        debug_if(true, "\r\n SPWFSA01::_disassociation_handler() #1\r\n"); // betzw - TODO: `true` only for debug!
        goto get_out;
    }
    debug_if(true, "Disassociation: %d\r\n", reason); // betzw - TODO: `true` only for debug!

    /* trigger scan */
    if(!(_parser.send("AT+S.SCAN") && _recv_ok()))
    {
        debug_if(true, "\r\n SPWFSA01::_disassociation_handler() #3\r\n"); // betzw - TODO: `true` only for debug!
        goto get_out;
    }

    if(!(_parser.send("AT+S.ROAM") && _recv_ok()))
    {
        debug_if(true, "\r\n SPWFSA01::_disassociation_handler() #2\r\n"); // betzw - TODO: `true` only for debug!
        goto get_out;
    }

    setTimeout(SPWF_RECV_TIMEOUT);

    if((_disassoc_handler_recursive_cnt == 0) && (were_connected)) {
        _release_rx_sem = true;
        while(true) {
            if((_parser.recv("+WIND:24:WiFi Up:%u.%u.%u.%u\r",&n1, &n2, &n3, &n4)) && _recv_delim()) {
                debug_if(true, "Re-connected (%u.%u.%u.%u)!\r\n", n1, n2, n3, n4); // betzw - TODO: `true` only for debug!

                _associated_interface._connected_to_network = true;
                _release_rx_sem = false;
                goto get_out;
            } else {
                int err;
                if((err = _rx_sem.wait(SPWF_CONNECT_TIMEOUT)) <= 0) { // wait for IRQ
                    debug_if(true, "\r\n SPWFSA01::_disassociation_handler() #4 (%d)\r\n", err); // betzw - TODO: `true` only for debug!

                    _release_rx_sem = false;
                    goto get_out;
                }
            }
        }
    } else {
        debug_if(true, "Leaving SPWFSA01::_disassociation_handler: %d\r\n", _disassoc_handler_recursive_cnt); // betzw - TODO: `true` only for debug!
        goto get_out;
    }

get_out:
#ifndef NDEBUG
    debug_if(true, "Getting out of SPWFSA01::_disassociation_handler: %d\r\n", disassoc_cnt); // betzw - TODO: `true` only for debug!
#else // NDEBUG
    debug_if(true, "Getting out of SPWFSA01::_disassociation_handler\r\n"); // betzw - TODO: `true` only for debug!
#endif // NDEBUG

    setTimeout(saved_timeout);
    _disassoc_handler_recursive_cnt--;
    return;
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

    setTimeout(SPWF_RECV_TIMEOUT);
    _parser.recv(":Console%d: r0 %x, r1 %x, r2 %x, r3 %x, r12 %x\r",
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

    // This is most likely the best we can to recover from this module hard fault
    _associated_interface.inner_constructor();
#endif // NDEBUG
}

/*
 * Handling oob ("+WIND:58")
 * when server closes a client connection
 */
void SPWFSA01::_sock_closed_handler()
{
    int spwf_id, internal_id;

    if(!(_parser.recv(":%d\r",&spwf_id) && _recv_delim())) {
        return;
    }

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
