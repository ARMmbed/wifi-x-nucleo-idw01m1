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
#include "mbed_debug.h"

#define SPWFSA01_CONNECT_TIMEOUT 15000
#define SPWFSA01_SEND_TIMEOUT    500
#define SPWFSA01_RECV_TIMEOUT    1500 //some commands like AT&F/W takes some time to get the result back!
//#define SPWFSA01_MISC_TIMEOUT    500

SPWFSA01::SPWFSA01(PinName tx, PinName rx, bool debug)
    : _serial(tx, rx, 2048), _parser(_serial),
	    _packets(0), _packets_end(&_packets),
      _wakeup(D14, PIN_INPUT, PullNone, 0), _reset(D15, PIN_INPUT, PullNone, 1),
      //PC_12->D15, PC_8->D14 (re-wires needed in-case used, currently not used)
      dbg_on(debug)
      //Pin PC_8 is wakeup pin
      //Pin PA_12 is reset pin
{
    _serial.baud(115200);
    _reset.output();
    _wakeup.output();
    _parser.debugOn(debug);
		data_pending = false;
		socket_close_id = 9;
}

bool SPWFSA01::startup(int mode)
{
    setTimeout(SPWFSA01_RECV_TIMEOUT);
    
    /*Test module before reset*/
    waitSPWFReady();
    /*Reset module*/
    reset();
     
    /*set local echo to 0*/
    if(!(_parser.send("AT+S.SCFG=localecho1,%d\r", 0) && _parser.recv("OK")))
        {
            debug_if(dbg_on, "SPWF> error local echo set\r\n");
            return false;
        }
    /*reset factory settings*/
    if(!(_parser.send("AT&F") && _parser.recv("OK"))) 
        {
            debug_if(dbg_on, "SPWF> error AT&F\r\n");
            return false;
        }

    /*set Wi-Fi mode and rate to b/g/n*/
    if(!(_parser.send("AT+S.SCFG=wifi_ht_mode,%d\r",1) && _parser.recv("OK")))
        {
            debug_if(dbg_on, "SPWF> error setting ht_mode\r\n");
            return false;
        }
        
    /*set the operational rate*/
    if(!(_parser.send("AT+S.SCFG=wifi_opr_rate_mask,0x003FFFCF\r") && _parser.recv("OK")))
        {
            debug_if(dbg_on, "SPWF> error setting operational rates\r\n");
            return false;
        }

    /*set idle mode (0->idle, 1->STA,3->miniAP, 2->IBSS)*/
    if(!(_parser.send("AT+S.SCFG=wifi_mode,%d\r", mode) && _parser.recv("OK")))
        {
            debug_if(dbg_on, "SPWF> error wifi mode set\r\n");
            return false;
        }

    /* save current setting in flash */
    if(!(_parser.send("AT&W") && _parser.recv("OK")))
        {
            debug_if(dbg_on, "SPWF> error AT&W\r\n");
            return false;
        }

    _parser.oob("+WIND:55:", this, &SPWFSA01::_packet_handler);
		_parser.oob("ERROR: Pending ", this, &SPWFSA01::_error_handler);
		//_parser.oob("+WIND:58:", this, &SPWFSA01::sock_disconnected);

    /*reset again and send AT command and check for result (AT->OK)*/
    reset();

    return true;
}

bool SPWFSA01::hw_reset(void)
{
    /* reset the pin PC12 */  
    _reset.write(0);
    wait_ms(200);
    _reset.write(1); 
    wait_ms(100);
    return 1;
}

bool SPWFSA01::reset(void)
{
    if(!_parser.send("AT+CFUN=1")) return false;
    while(1) {
        if (_parser.recv("+WIND:32:WiFi Hardware Started\r")) {
            return true;
        }
    }
}

void SPWFSA01::waitSPWFReady(void)
{
    //wait_ms(200);
    while(1)
        if(_parser.send("AT") && _parser.recv("OK"))
            //till we get OK from AT command
            //printf("\r\nwaiting for reset to complete..\n");
            return;
}

/* Security Mode
    None          = 0, 
    WEP           = 1,
    WPA_Personal  = 2,
*/
bool SPWFSA01::connect(const char *ap, const char *passPhrase, int securityMode)
{
    uint32_t n1, n2, n3, n4;

    //AT+S.SCFG=wifi_wpa_psk_text,%s\r
    if(!(_parser.send("AT+S.SCFG=wifi_wpa_psk_text,%s", passPhrase) && _parser.recv("OK"))) 
        {
            debug_if(dbg_on, "SPWF> error pass set\r\n");
            return false;
        } 
    //AT+S.SSIDTXT=%s\r
    if(!(_parser.send("AT+S.SSIDTXT=%s", ap) && _parser.recv("OK"))) 
        {
            debug_if(dbg_on, "SPWF> error ssid set\r\n");
            return false;
        }
    //AT+S.SCFG=wifi_priv_mode,%d\r
    if(!(_parser.send("AT+S.SCFG=wifi_priv_mode,%d", securityMode) && _parser.recv("OK"))) 
        {
            debug_if(dbg_on, "SPWF> error security mode set\r\n");
            return false;
        } 
    //"AT+S.SCFG=wifi_mode,%d\r"
    /*set idle mode (0->idle, 1->STA,3->miniAP, 2->IBSS)*/
    if(!(_parser.send("AT+S.SCFG=wifi_mode,%d\r", 1) && _parser.recv("OK")))
        {
            debug_if(dbg_on, "SPWF> error wifi mode set\r\n");
            return false;
        }
    //AT&W
    /* save current setting in flash */
    if(!(_parser.send("AT&W") && _parser.recv("OK")))
        {
            debug_if(dbg_on, "SPWF> error AT&W\r\n");
            return false;
        }
    //reset module
    reset();
    
    while(1)
        if((_parser.recv("+WIND:24:WiFi Up:%u.%u.%u.%u",&n1, &n2, &n3, &n4)))
            {
                break;
            }            
        
    return true;
}

bool SPWFSA01::disconnect(void)
{
    //"AT+S.SCFG=wifi_mode,%d\r"
    /*set idle mode (0->idle, 1->STA,3->miniAP, 2->IBSS)*/
    if(!(_parser.send("AT+S.SCFG=wifi_mode,%d\r", 0) && _parser.recv("OK")))
        {
            debug_if(dbg_on, "SPWF> error wifi mode set\r\n");
            return false;
        }
    //AT&W
    /* save current setting in flash */
    if(!(_parser.send("AT&W") && _parser.recv("OK")))
        {
            debug_if(dbg_on, "SPWF> error AT&W\r\n");
            return false;
        }
    //reset module
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
        
    return _parser.send("AT+S.SCFG=ip_use_dhcp,%d\r", mode)
        && _parser.recv("OK");
}


const char *SPWFSA01::getIPAddress(void)
{
    uint32_t n1, n2, n3, n4;
    
    if (!(_parser.send("AT+S.STS=ip_ipaddr")
        && _parser.recv("#  ip_ipaddr = %u.%u.%u.%u", &n1, &n2, &n3, &n4)
        && _parser.recv("OK"))) {
            debug_if(dbg_on, "SPWF> getIPAddress error\r\n");
        return 0;
    }

    sprintf((char*)_ip_buffer,"%u.%u.%u.%u", n1, n2, n3, n4);

    return _ip_buffer;
}

const char *SPWFSA01::getMACAddress(void)
{
    uint32_t n1, n2, n3, n4, n5, n6;
    
    if (!(_parser.send("AT+S.GCFG=nv_wifi_macaddr")
        && _parser.recv("#  nv_wifi_macaddr = %x:%x:%x:%x:%x:%x", &n1, &n2, &n3, &n4, &n5, &n6)
        && _parser.recv("OK"))) {
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
    Timer timer;
    timer.start();
    
#if 1
    if(!_parser.send("AT+S.SOCKON=%s,%d,%s,ind", addr, port, type))
        {
            debug_if(dbg_on, "SPWF> error opening socket\r\n");
            return false;
        }

    while(1)
        {
            if( _parser.recv(" ID: %d", id)
                && _parser.recv("OK"))
                break;

            if (timer.read_ms() > SPWFSA01_CONNECT_TIMEOUT) {
                return false;
            }

            //TODO:implement time-out functionality in case of no response
            //if(timeout) return false;
            //TODO: deal with errors like "ERROR: Failed to resolve name"
            //TODO: deal with errors like "ERROR: Data mode not available"
        }
    return true;
#endif

#if 0
	//IDs only 0-7
		if(id > 7)
			return false;
		
	if(!(_parser.send("AT+S.SOCKON=%s,%d,%s,ind", addr, port, type)
				&& _parser.recv("OK")))
        {
            debug_if(dbg_on, "SPWF> error opening socket\r\n");
            return false;
        }
		return true;
#endif

}

bool SPWFSA01::send(int id, const void *data, uint32_t amount)
{
    char _buf[18];

    setTimeout(SPWFSA01_SEND_TIMEOUT);

    sprintf((char*)_buf,"AT+S.SOCKW=%d,%d\r", id, amount);   
    debug_if(dbg_on, "SPWF> SOCKW\r\n");
    //May take a second try if device is busy
    for (unsigned i = 0; i < 2; i++) {
        if (_parser.write((char*)_buf, strlen(_buf)) >=0
            && _parser.write((char*)data, (int)amount) >= 0
            && _parser.recv("OK")) {
            return true;
        }
    }

    return false;
}

#if 1
void SPWFSA01::_packet_handler()
{
    int id;
    uint32_t amount;

    // parse out the socket id
    if (!_parser.recv("Pending Data:%d:%d", &id, &amount)) {
				if(data_pending) {  //not a callback but from socket_close
					id = socket_close_id;
					socket_close_id = 9;
					data_pending = false;
				}
				else return;
    }

		/* cannot do read without query as in WIND:55 the length of data gets adding up and the actual data may be less at any given time */
	while(true) {
		if (!(_parser.send("AT+S.SOCKQ=%d", id)
			&& _parser.recv(" DATALEN: %u", &amount)
			&& _parser.recv("OK"))) {
				return;
		}

		if (amount==0) break; //no more data to be read

		struct packet *packet = (struct packet*)malloc(
				sizeof(struct packet) + amount);
		if (!packet) {
			return;
		}

		packet->id = id;
		packet->len = amount;
		packet->next = 0;

		_parser.flush(); //clear ring buffer

		if (!(_parser.send("AT+S.SOCKR=%d,%d", id, amount)
			&& (_parser.read((char*)(packet + 1), amount) >0)
			&& _parser.recv("OK"))) {
				free(packet);
				return;
		}

		// append to packet list
		*_packets_end = packet;
		_packets_end = &packet->next;
	}
	//debug_if(dbg_on, "SPWF> Exit packet Handler\r\n");
}
#endif

#if 0
void SPWFSA01::_packet_handler()
{
    int id;
    uint32_t amount;

    // parse out the socket id
    if (!_parser.recv("Pending Data:%d:%d", &id, &amount)) {
				if(data_pending) {  //not a callback but from socket_close
					id = socket_close_id;
					data_pending = false;
				}
				else return;
    }

		/* cannot do read without query as in WIND:55 the length of data gets adding up and the actual data may be less at any given time */
	while(true) {
		if (!(_parser.send("AT+S.SOCKQ=%d", id)
			&& _parser.recv(" DATALEN: %u", &amount)
			&& _parser.recv("OK"))) {
				return;
		}

		if (amount==0) break; //no more data to be read

		if(id != socket_close_id)
		{
				struct packet *packet = (struct packet*)malloc(
						sizeof(struct packet) + amount);
				if (!packet) {
					return;
				}

				packet->id = id;
				packet->len = amount;
				packet->next = 0;

				_parser.flush(); //clear ring buffer

				if (!(_parser.send("AT+S.SOCKR=%d,%d", id, amount)
					&& (_parser.read((char*)(packet + 1), amount) >0)
					&& _parser.recv("OK"))) {
						free(packet);
						return;
				}

				// append to packet list
				*_packets_end = packet;
				_packets_end = &packet->next;
		}
		else
		{
				_parser.flush();
				if (!(_parser.send("AT+S.SOCKR=%d,%d", id, amount)
						&& _parser.recv("OK"))) {
						_parser.flush();
						return;
				}
				_parser.flush();
		}
	}
	//debug_if(dbg_on, "SPWF> Exit packet Handler\r\n");
}
#endif


/**
*
*	Recv Function
*
*/
int32_t SPWFSA01::recv(int id, void *data, uint32_t amount)
{
	Timer timer;
  timer.start();
	bool _timeout = false;
	
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
        if (!_parser.recv("OK")) {
					//debug_if(dbg_on, "SPWF> recv :: ok\r\n");
        	return -1;
				//debug_if(dbg_on, "SPWF> OK not recvd\r\n");
				
						/*	
			if (timer.read_ms() > 1500) {
				debug_if(dbg_on, "SPWF> No Data, timeout\r\n");
                return -1;
            }
			*/
        }
    }

#if 0
    uint32_t recv_amount;
    int recv_id;
    bool wind_recv = true;

    if (!(_parser.recv("+WIND:55:Pending Data:%d:%u", &recv_id, &recv_amount)
        && recv_id == id
        && recv_amount <= amount
        && recv_amount%730
        && _parser.send("AT+S.SOCKQ=%d", id)  //send a query (will be required for secure sockets)
        && _parser.recv(" DATALEN: %u", &recv_amount)
        && _parser.recv("OK") 
        && recv_amount > 0
        && _parser.send("AT+S.SOCKR=%d,%d", id, recv_amount)
        && (_parser.read((char*)data, recv_amount) >0)
        && _parser.recv("OK"))) {
            debug_if(dbg_on, "Some kind of recv error!\r\n");
            if(!(recv_amount%730))
            {
                // receive all the WIND messages
                do {
                    if (!(_parser.recv("+WIND:55:Pending Data:%d:%u", &recv_id, &recv_amount)
                         && recv_id == id 
                         && recv_amount <= amount
                         && recv_amount > 0))
                             wind_recv = false;
                } while (!(recv_amount%730) && wind_recv);

                // Read all the data pending on a socket
                if(!( recv_amount > 0
                    && _parser.send("AT+S.SOCKR=%d,%d", id, recv_amount)
                    && (_parser.read((char*)data, recv_amount) >0)
                    && _parser.recv("OK"))) {
                        return -1;
                    }
            }
            else {
                return -2;
            }
    }    
    return recv_amount;
#endif 
}

#if 1
bool SPWFSA01::close(int id)
{
	  debug_if(dbg_on,"\r\n SPWFSA01::close\r\n");
		socket_close_id = id;
		int length=0;

    //May take a second try if device is busy or error is returned
    for (unsigned i = 0; i < 2; i++) {
				if (_parser.send("AT+S.SOCKC=%d", id)
          && _parser.recv("OK")) {
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
						data_pending = false;
						debug_if(dbg_on,"\r\nClose complete..returning\r\n");
						socket_close_id = 9;
						return true;
				}
		}
		socket_close_id = 9;
		return false;
}
#endif

#if 0
bool SPWFSA01::close(int id)
{
	  debug_if(dbg_on,"\r\n SPWFSA01::close\r\n");
		socket_close_id = id;
		int length=0;
		uint8_t *packet = (uint8_t *)malloc(512);

    //May take a second try if device is busy or error is returned
    for (unsigned i = 0; i < 2; i++) {
        if (_parser.send("AT+S.SOCKC=%d", id)
            && _parser.recv("OK")) {
							// print the size of the packets
							for (struct packet **p = &_packets; *p; p = &(*p)->next)
							{
								struct packet *q = *p;
								debug_if(dbg_on,"sizeof packets q is %d\r\n",sizeof(q));
								debug_if(dbg_on,"sizeof packets p is %d\r\n",sizeof(*p));
								debug_if(dbg_on,"data after q is: %d\r\n",sizeof(q+1));
							}

							
							for (struct packet **p = &_packets; *p; p = &(*p)->next)
							{
									length++;
									debug_if(dbg_on,"\r\n Id of packet %d",(*p)->id);
									debug_if(dbg_on,"\r\n Length of packet %d",(*p)->len);
									if((*p)->id == id) {
											while((*p)->len >0)
												recv(id,packet,512);
										
											struct packet *q = *p;
											if (_packets_end == &(*p)->next)
                        _packets_end = p;
											memset((q+1),0x00,q->len);
											free(q);
									}
									debug_if(dbg_on,"\r\n Packet Removed ! \r\n");
							}
							debug_if(dbg_on,"\r\n Length of linked list is %d \r\n",length);

							// do we need to delete the packets of data binded with this id?
							/*
							debug_if(true,"Removing all the packets with ID = %d\r\n",id);
							// remove all packets and return
							for (struct packet **p = &_packets; *p; p = &(*p)->next)
							{
								debug_if(true,"\r\n In Close with packet id %d \r\n",(*p)->id);
								if ((*p)->id == id) {
									struct packet *q = *p;
									if((*p)->len > 0) {
										debug_if(true,"\r\n Packet has data of length %d \r\n",(*p)->len);
										//memset(q+1,0x00,(*p)->len);
										free(q+1);
									}
									debug_if(true,"\r\n After removing data \r\n");
									if (_packets_end == &(*p)->next)
                        _packets_end = p;
                    //*p = (*p)->next;
                    free(q);
									debug_if(true,"\r\n Packet free !! \r\n");
								}
							}
							*/
            return true;
        }
        //TODO: Deal with "ERROR: Pending data" (Closing a socket with pending data)
    }
    return false;
}
#endif

#if 0
bool SPWFSA01::close(int id)
{
	  debug_if(dbg_on,"\r\n SPWFSA01::close\r\n");
		socket_close_id = id;
		sock_close_ongoing = true;

    //May take a second try if device is busy or error is returned
    for (unsigned i = 0; i < 2; i++) {
        if (_parser.send("AT+S.SOCKC=%d", id)
            && _parser.recv("OK")) {
							socket_close_id = 9;
							sock_close_ongoing = false;
							return true;
				}
		}
		sock_close_ongoing = false;
		socket_close_id = 9;
		return false;
}
#endif

/*
 * Handling OOb (Error: Pending Data)
 *
*/
void SPWFSA01::_error_handler()
{
		debug_if(dbg_on,"\r\n SPWFSA01::_error_handle \r\n");
		if(!(_parser.recv("data")))
			return;

		data_pending = true;
		_packet_handler();
}

/*
 * Handling OOB (+WIND:58)
 * when server closes a client connection
*/
void SPWFSA01::sock_disconnected()
{
		int id;
		if(!(_parser.recv("Socket Closed:%d",&id)))
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

