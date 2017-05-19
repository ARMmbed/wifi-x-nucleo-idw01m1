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
  _wakeup(PC_8), _reset(PC_12, 1),
  _packets(0), _packets_end(&_packets),
  dbg_on(debug)
{
    _serial.baud(115200);
    _parser.debugOn(debug);
}

bool SPWFSA01::startup(int mode)
{
    /*Reset module*/
    hw_reset();

    /*set local echo to 0*/
    if(!(_parser.send("AT+S.SCFG=localecho1,%d", 0) && _parser.recv("OK\r")))
    {
        debug_if(dbg_on, "SPWF> error local echo set\r\n");
        return false;
    }
    /*reset factory settings*/
    if(!(_parser.send("AT&F") && _parser.recv("OK\r")))
    {
        debug_if(dbg_on, "SPWF> error AT&F\r\n");
        return false;
    }

    /*set Wi-Fi mode and rate to b/g/n*/
    if(!(_parser.send("AT+S.SCFG=wifi_ht_mode,%d",1) && _parser.recv("OK\r")))
    {
        debug_if(dbg_on, "SPWF> error setting ht_mode\r\n");
        return false;
    }

    /*set the operational rate*/
    if(!(_parser.send("AT+S.SCFG=wifi_opr_rate_mask,0x003FFFCF") && _parser.recv("OK\r")))
    {
        debug_if(dbg_on, "SPWF> error setting operational rates\r\n");
        return false;
    }

    /*set idle mode (0->idle, 1->STA,3->miniAP, 2->IBSS)*/
    if(!(_parser.send("AT+S.SCFG=wifi_mode,%d", mode) && _parser.recv("OK\r")))
    {
        debug_if(dbg_on, "SPWF> error wifi mode set\r\n");
        return false;
    }

    /* save current setting in flash */
    if(!(_parser.send("AT&W") && _parser.recv("OK\r")))
    {
        debug_if(dbg_on, "SPWF> error AT&W\r\n");
        return false;
    }

    _parser.oob("+WIND:55:", this, &SPWFSA01::_packet_handler);
    _parser.oob("ERROR: Pending ", this, &SPWFSA01::_error_handler);
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
        debug_if(dbg_on, "SPWF> error pass set\r\n");
        return false;
    } 
    //AT+S.SSIDTXT=%s
    if(!(_parser.send("AT+S.SSIDTXT=%s", ap) && _parser.recv("OK\r")))
    {
        debug_if(dbg_on, "SPWF> error ssid set\r\n");
        return false;
    }
    //AT+S.SCFG=wifi_priv_mode,%d
    if(!(_parser.send("AT+S.SCFG=wifi_priv_mode,%d", securityMode) && _parser.recv("OK\r")))
    {
        debug_if(dbg_on, "SPWF> error security mode set\r\n");
        return false;
    } 
    //"AT+S.SCFG=wifi_mode,%d"
    /*set idle mode (0->idle, 1->STA,3->miniAP, 2->IBSS)*/
    if(!(_parser.send("AT+S.SCFG=wifi_mode,%d", 1) && _parser.recv("OK\r")))
    {
        debug_if(dbg_on, "SPWF> error wifi mode set\r\n");
        return false;
    }
    //AT&W
    /* save current setting in flash */
    if(!(_parser.send("AT&W") && _parser.recv("OK\r")))
    {
        debug_if(dbg_on, "SPWF> error AT&W\r\n");
        return false;
    }

    while(1)
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
    if(!(_parser.send("AT+S.SCFG=wifi_mode,%d", 0) && _parser.recv("OK\r")))
    {
        debug_if(dbg_on, "SPWF> error wifi mode set\r\n");
        return false;
    }
    //AT&W
    /* save current setting in flash */
    if(!(_parser.send("AT&W") && _parser.recv("OK\r")))
    {
        debug_if(dbg_on, "SPWF> error AT&W\r\n");
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
        debug_if(dbg_on, "SPWF> getIPAddress error\r\n");
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
        debug_if(dbg_on, "SPWF> getMACAddress error\r\n");
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
        debug_if(dbg_on, "SPWF> error opening socket\r\n");
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
        unsigned int i;

        // May take a second try if device is busy (betzw: TOINVESTIGATE - TODO)
        for (i = 0; i < 2; i++) {
            if (_parser.send("AT+S.SOCKW=%d,%d", id, (unsigned int)to_send)
                    && (_parser.write((char*)data, (int)to_send) == (int)to_send)
                    && _parser.recv("OK\r")) {
                break;
            }
        }

        if(i >= 2) {
            // betzw - TODO: handle different errors more accurately!
            return false;
        }

        sent += to_send;
    }

    return true;
}

void SPWFSA01::_packet_handler()
{
    int id;
    uint32_t amount;

    // parse out the socket id
    if (!_parser.recv("Pending Data:%d:%d\r", &id, &amount)) {
        return;
    }

    /* cannot do read without query as in WIND:55 the length of data gets adding up and the actual data may be less at any given time */
    while(true) {
        if (!(_parser.send("AT+S.SOCKQ=%d", id)
                && _parser.recv(" DATALEN: %u\r", &amount))) {
            break;
        }

        if (amount==0) break; //no more data to be read

        // Let it up to `SPWFSA01::recv()` to receive OK
        if(!_parser.recv("OK\r") || (_parser.getc() != '\n')) {
            break;
        }

        struct packet *packet = (struct packet*)malloc(
                sizeof(struct packet) + amount);
        if (!packet) {
            break;
        }

        packet->id = id;
        packet->len = amount;
        packet->next = 0;

        if (!(_parser.send("AT+S.SOCKR=%d,%d", id, amount)
                && (_parser.read((char*)(packet + 1), amount) >0)
                && _parser.recv("OK\r"))) {
            free(packet);
            return;
        }

        // append to packet list
        *_packets_end = packet;
        _packets_end = &packet->next;
    }
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
                debug_if(dbg_on,"\r\n Read Done on ID %d and length of packet is %d\r\n",id,(*p)->len);
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
    debug_if(dbg_on,"\r\n SPWFSA01::close\r\n");
    int length=0;

    //May take a second try if device is busy or error is returned
    // betzw - TODO: to be reviewed!
    for (unsigned i = 0; i < 2; i++) {
        if (_parser.send("AT+S.SOCKC=%d", id)
                && _parser.recv("OK\r")) {
            uint8_t *packet = (uint8_t *)malloc(512);
            for(struct packet **p = &_packets; *p; p=&(*p)) {
                debug_if(dbg_on,"\r\n Iterating over packets\r\n");
                if((*p)->id == id) {
                    length = (*p)->len;
                    while(length > 0) {
                        debug_if(dbg_on,"\r\n Data of one packet %d \r\n",(*p)->len);
                        struct packet *q = *p;
                        if (q->len <= 512) { // Return and remove full packet
                            memcpy(packet, q+1, q->len);

                            if (_packets_end == &(*p)->next) {
                                _packets_end = p;
                            }
                            *p = (*p)->next;
                            q->len = 0;
                            length = 0;
                            free(q);
                            debug_if(dbg_on,"\r\n packet free!\r\n");
                            break;
                        }
                        else { // return only partial packet
                            memcpy(packet, q+1, 512);
                            q->len -= 512;
                            length = q->len;
                            memmove(q+1, (uint8_t*)(q+1) + 512, q->len);
                        }
                    }
                }
            }
            //free (packet);
            debug_if(dbg_on,"\r\nClose complete..returning\r\n");
            return true;
        }
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
    _serial.attach(func);
}
