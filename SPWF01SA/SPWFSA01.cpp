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

SPWFSA01::SPWFSA01(PinName tx, PinName rx, bool debug)
: _serial(tx, rx, 2048), _parser(_serial, "\r", "\n"),
  _wakeup(PC_8, 1), _reset(PC_12, 1),
  _rx_sem(0), _release_sem(false), _callback_func(),
  _timeout(0), _dbg_on(debug),
  _packets(0), _packets_end(&_packets)
{
    _serial.baud(115200);
    _serial.attach(Callback<void()>(this, &SPWFSA01::_event_handler));
    _parser.debugOn(debug);
}

bool SPWFSA01::startup(int mode)
{
    /*Reset module*/
    hw_reset();
    reset();

    /*set local echo to 0*/
    if(!(_parser.send("AT+S.SCFG=localecho1,0") && _parser.recv("OK\r")))
    {
        debug_if(_dbg_on, "SPWF> error local echo set\r\n");
        return false;
    }

    /*set Wi-Fi mode and rate to b/g/n*/
    if(!(_parser.send("AT+S.SCFG=wifi_ht_mode,1") && _parser.recv("OK\r")))
    {
        debug_if(_dbg_on, "SPWF> error setting ht_mode\r\n");
        return false;
    }

    /*set the operational rate*/
    if(!(_parser.send("AT+S.SCFG=wifi_opr_rate_mask,0x003FFFCF") && _parser.recv("OK\r")))
    {
        debug_if(_dbg_on, "SPWF> error setting operational rates\r\n");
        return false;
    }

    /*set idle mode (0->idle, 1->STA,3->miniAP, 2->IBSS)*/
    if(!(_parser.send("AT+S.SCFG=wifi_mode,%d", mode) && _parser.recv("OK\r")))
    {
        debug_if(_dbg_on, "SPWF> error wifi mode set\r\n");
        return false;
    }

    /* set number of consecutive loss beacon to detect the AP disassociation */
    if(!(_parser.send("AT+S.SCFG=wifi_beacon_loss_thresh,10") && _parser.recv("OK\r")))
    {
        debug_if(_dbg_on, "SPWF> error wifi beacon loss thresh set\r\n");
        return false;
    }

#ifndef NDEBUG
    /* display all configuration values (only for debug) */
    if(!(_parser.send("AT&V") && _parser.recv("OK\r")))
    {
        debug_if(_dbg_on, "SPWF> error AT&V\r\n");
        return false;
    }
#endif

    _parser.oob("+WIND:55:", this, &SPWFSA01::_packet_handler);
    _parser.oob("ERROR: Pending ", this, &SPWFSA01::_error_handler);
    _parser.oob("+WIND:41:", this, &SPWFSA01::_disassociation_handler);
    // betzw - TODO: _parser.oob("+WIND:58:", this, &SPWFSA01::_sock_disconnected);

    return true;
}

void SPWFSA01::_wait_console_active(void) {
    while(true) {
        if (_parser.recv("+WIND:0:Console active\r")) {
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
    if(!(_parser.send("AT+S.SCFG=wifi_wpa_psk_text,%s", passPhrase) && _parser.recv("OK\r")))
    {
        debug_if(_dbg_on, "SPWF> error pass set\r\n");
        return false;
    } 
    //AT+S.SSIDTXT=%s
    if(!(_parser.send("AT+S.SSIDTXT=%s", ap) && _parser.recv("OK\r")))
    {
        debug_if(_dbg_on, "SPWF> error ssid set\r\n");
        return false;
    }
    //AT+S.SCFG=wifi_priv_mode,%d
    if(!(_parser.send("AT+S.SCFG=wifi_priv_mode,%d", securityMode) && _parser.recv("OK\r")))
    {
        debug_if(_dbg_on, "SPWF> error security mode set\r\n");
        return false;
    } 
    //"AT+S.SCFG=wifi_mode,%d"
    /*set idle mode (0->idle, 1->STA,3->miniAP, 2->IBSS)*/
    if(!(_parser.send("AT+S.SCFG=wifi_mode,1") && _parser.recv("OK\r")))
    {
        debug_if(_dbg_on, "SPWF> error wifi mode set\r\n");
        return false;
    }

    while(true)
        if((_parser.recv("+WIND:24:WiFi Up:%u.%u.%u.%u\r",&n1, &n2, &n3, &n4)))
        {
            break;
        }

    return true;
}

bool SPWFSA01::disconnect(void)
{
    //"AT+S.SCFG=wifi_mode,%d"
    /*set idle mode (0->idle, 1->STA,3->miniAP, 2->IBSS)*/
    if(!(_parser.send("AT+S.SCFG=wifi_mode,0") && _parser.recv("OK\r")))
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
            && _parser.recv("OK\r");
}


const char *SPWFSA01::getIPAddress(void)
{
    unsigned int n1, n2, n3, n4;

    if (!(_parser.send("AT+S.STS=ip_ipaddr")
            && _parser.recv("#  ip_ipaddr = %u.%u.%u.%u\r", &n1, &n2, &n3, &n4)
            && _parser.recv("OK\r"))) {
        debug_if(_dbg_on, "SPWF> getIPAddress error\r\n");
        return 0;
    }

    sprintf((char*)_ip_buffer,"%u.%u.%u.%u", n1, n2, n3, n4);

    return _ip_buffer;
}

const char *SPWFSA01::getMACAddress(void)
{
    unsigned int n1, n2, n3, n4, n5, n6;

    if (!(_parser.send("AT+S.GCFG=nv_wifi_macaddr")
            && _parser.recv("#  nv_wifi_macaddr = %x:%x:%x:%x:%x:%x\r", &n1, &n2, &n3, &n4, &n5, &n6)
            && _parser.recv("OK\r"))) {
        debug_if(_dbg_on, "SPWF> getMACAddress error\r\n");
        return 0;
    }

    sprintf((char*)_mac_buffer,"%02X:%02X:%02X:%02X:%02X:%02X", n1, n2, n3, n4, n5, n6);

    return _mac_buffer;
}

bool SPWFSA01::isConnected(void)
{
    return getIPAddress() != 0;
}

bool SPWFSA01::open(const char *type, int* id, const char* addr, int port)
{
    int socket_id;

    if(!_parser.send("AT+S.SOCKON=%s,%d,%s,ind", addr, port, type))
    {
        debug_if(_dbg_on, "SPWF> error opening socket\r\n");
        return false;
    }

    if( _parser.recv(" ID: %d\r", &socket_id)
            && _parser.recv("OK\r")) {
        *id = socket_id;
        return true;
    }

    return false;
}

#define SPWFSA01_MAX_WRITE 4096U
bool SPWFSA01::send(int id, const void *data, uint32_t amount)
{
    uint32_t sent = 0U, to_send;

    for(to_send = (amount > SPWFSA01_MAX_WRITE) ? SPWFSA01_MAX_WRITE : amount;
            sent < amount;
            to_send = ((amount - sent) > SPWFSA01_MAX_WRITE) ? SPWFSA01_MAX_WRITE : (amount - sent)) {
        if (!(_parser.send("AT+S.SOCKW=%d,%d", id, (unsigned int)to_send)
                && (_parser.write((char*)data, (int)to_send) == (int)to_send)
                && _parser.recv("OK\r"))) {
            // betzw - TODO: handle different errors more accurately!
            return false;
        }

        sent += to_send;
    }

    return true;
}

int SPWFSA01::_read_len(int id) {
    uint32_t amount;

    if (!(_parser.send("AT+S.SOCKQ=%d", id)
            && _parser.recv(" DATALEN: %u\r", &amount))) {
        return -1;
    }

    return (int)amount;
}

int SPWFSA01::_flush_in(char* buffer, int size) {
    int i = 0;

    for ( ; i < size; i++) {
        int c = _parser.getc();
        if (c < 0) {
            return -1;
        }
    }
    return i;
}

int SPWFSA01::_read_in(char* buffer, int id, uint32_t amount) {
    Callback<int(char*, int)> read_func;

    if(buffer != NULL) {
        Callback<int(char*, int)> parser_func(&_parser, &ATParser::read);
        read_func = parser_func;
    } else {
        Callback<int(char*, int)> flush_func(this, &SPWFSA01::_flush_in);
        read_func = flush_func;
    }

    if (!(_parser.send("AT+S.SOCKR=%d,%d", id, amount)
            && (read_func(buffer, amount) > 0)
            && _parser.recv("OK\r"))) {
        return -1;
    }

    return amount;
}

void SPWFSA01::_packet_handler(void)
{
    int id;
    int total_amount;
    int current_amount;
    struct packet *packet = NULL;

    // parse out the socket id
    if (!_parser.recv("Pending Data:%d:%d\r", &id, &total_amount)) {
        return;
    }

    /* cannot do read without query as in WIND:55 the length of data gets adding up and the actual data may be less at any given time */
    while(total_amount > 0) {
        if ((current_amount = _read_len(id)) <= 0) {
            if(packet != NULL) free(packet);
            return; //no more data to be read
        }

        if(!_parser.recv("OK\r") || (_parser.getc() != '\n')) {
            if(packet != NULL) free(packet);
            return;
        }

        if(packet == NULL) {
            packet = (struct packet*)malloc(
                    sizeof(struct packet) + total_amount);
            if (!packet) {
                return;
            }

            packet->id = id;
            packet->len = (uint32_t)total_amount;
            packet->next = 0;
        }

        if(!(_read_in((char*)(packet + 1), id, (uint32_t)current_amount) > 0)) {
            free(packet);
            return;
        }

        total_amount -= current_amount;
    }

    // append to packet list
    *_packets_end = packet;
    _packets_end = &packet->next;
}

/**
 *
 *	Recv Function
 *
 */
int32_t SPWFSA01::recv(int id, void *data, uint32_t amount)
{
    while (true) {
        // check if any packets are ready for us
        for (struct packet **p = &_packets; *p; p = &(*p)->next) {
            if ((*p)->id == id) {
                debug_if(_dbg_on,"\r\n Read Done on ID %d and length of packet is %d\r\n",id,(*p)->len);
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

        // Wait for inbound packet
        if (!_parser.recv("OK\r")) {
            return -1;
        }
    }
}

bool SPWFSA01::close(int id)
{
    int amount;

    MBED_ASSERT(id != SPWFSA_SOCKET_COUNT);

    // Flush out pending data
    while(true) {
        if((amount = _read_len(id)) < 0) return false;

        if(!_parser.recv("OK\r") || (_parser.getc() != '\n')) {
            return false;
        }

        if(amount == 0) break; // no more data to be read

        if(!(_read_in((char*)NULL, id, (uint32_t)amount) > 0)) {
            return false;
        }
    }

    // Close socket
    if (_parser.send("AT+S.SOCKC=%d", id)
            && _parser.recv("OK\r")) {
        return true;
    }

    return false;
}

/*
 * Handling OOb (Error: Pending Data)
 *
 */
void SPWFSA01::_error_handler()
{
    error("\r\n SPWFSA01::_error_handler()\r\n");
}

/*
 * Buffered serial event handler
 *
 * Note: executed in IRQ context!
 *
 */
void SPWFSA01::_event_handler()
{
    if(_release_sem)
        _rx_sem.release();
    if((bool)_callback_func)
        _callback_func();
}

/*
 * Handling OOb (+WIND:33:WiFi Network Lost)
 *
 * betzw - TODO: error handling still to be implemented!
 *
 */
void SPWFSA01::_disassociation_handler()
{
    int reason;
    uint32_t n1, n2, n3, n4;
    int err;
    int saved_timeout = _timeout;

    setTimeout(SPWF_CONNECT_TIMEOUT);

    // parse out reason
    if (!_parser.recv("WiFi Disassociation: %d\r", &reason)) {
        error("\r\n SPWFSA01::_disassociation_handler() #1\r\n");
    }
    debug_if(true, "Disassociation: %d\r\n", reason); // betzw - TODO: `true` only for debug!

    /* trigger scan */
    if(!(_parser.send("AT+S.SCAN") && _parser.recv("OK\r")))
    {
        error("\r\n SPWFSA01::_disassociation_handler() #3\r\n");
    }

    if(!(_parser.send("AT+S.ROAM") && _parser.recv("OK\r")))
    {
        error("\r\n SPWFSA01::_disassociation_handler() #2\r\n");
    }

    _release_sem = true;
    setTimeout(SPWF_RECV_TIMEOUT);
    while(true) {
        if((_parser.recv("+WIND:24:WiFi Up:%u.%u.%u.%u\r",&n1, &n2, &n3, &n4)))
        {
            _release_sem = false;
            setTimeout(saved_timeout);

            debug_if(true, "Re-connected!\r\n"); // betzw - TODO: `true` only for debug!
            return;
        } else {
            if((err = _rx_sem.wait(SPWF_CONNECT_TIMEOUT)) <= 0) { // wait for IRQ
                error("\r\n SPWFSA01::_disassociation_handler() #4 (%d)\r\n", err);
            }
        }
    }

    error("\r\n SPWFSA01::_disassociation_handler() #0\r\n");
}

/*
 * Handling OOB (+WIND:58)
 * when server closes a client connection
 */
// betzw - TODO
void SPWFSA01::_sock_disconnected()
{
    int id;
    if(!(_parser.recv("Socket Closed:%d\r",&id)))
        return;

    // if we are closing socket by ourselves then the value of _cbs and _ids need to be changed..how ?
    //		_spwfinterface.set_cbs(id,0,0);
    //_spwfinterface.close();
    close(id);

    //@todo actual socket close....to call spwfsa01::close close or spwfInterface::close 
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
