/* mbed Microcontroller Library
* Copyright (c) 20015 ARM Limited
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

/**
  ******************************************************************************
  * @file    SpwfInterface.h 
  * @author  STMicroelectronics
  * @brief   Header file of the NetworkStack for the SPWF Device
  ******************************************************************************
  * @copy
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2016 STMicroelectronics</center></h2>
  ******************************************************************************
  */
  
#ifndef SPWFSA_INTERFACE_H
#define SPWFSA_INTERFACE_H

#include "mbed.h"
#include "SPWFSA01.h"
 
#define SPWFSA_SOCKET_COUNT 8
#define SERVER_SOCKET_NO    9
 
/** SpwfSAInterface class
 *  Implementation of the NetworkStack for the SPWF Device
 */
class SpwfSAInterface : public NetworkStack, public WiFiInterface
{
public:

    SpwfSAInterface(PinName tx, PinName rx, bool debug = false);
    virtual     ~SpwfSAInterface();
 
    // Implementation of WiFiInterface
    virtual     int connect(const char *ssid,
                            const char *pass,
                            nsapi_security_t security = NSAPI_SECURITY_NONE,
														uint8_t channel = 0);

    virtual     int disconnect();    
    virtual     const char *get_mac_address();
    void        debug(const char * string);
		void 				set_cbs(int id,void (*callback)(void *),void *data);			

    //Implementation of NetworkStack
    virtual     const char *get_ip_address();

protected:
    //Implementation of NetworkStack
    virtual     int socket_open(void **handle, nsapi_protocol_t proto);
    virtual     int socket_close(void *handle);
    virtual     int socket_bind(void *handle, const SocketAddress &address);  //not supported
    virtual     int socket_listen(void *handle, int backlog);
    virtual     int socket_connect(void *handle, const SocketAddress &address);
    //virtual     int socket_accept(nsapi_socket_t server, void **handle);
		virtual			int socket_accept(nsapi_socket_t server, nsapi_socket_t *handle, SocketAddress *address=0);  // not supported
    virtual     int socket_send(void *handle, const void *data, unsigned size);
    virtual     int socket_recv(void *handle, void *data, unsigned size);
    virtual     int socket_sendto(void *handle, const SocketAddress &address, const void *data, unsigned size);
    virtual     int socket_recvfrom(void *handle, SocketAddress *address, void *buffer, unsigned size);
    virtual     void socket_attach(void *handle, void (*callback)(void *), void *data);
		/* WiFiInterface */
		virtual 		int set_credentials(const char *ssid, const char *pass, nsapi_security_t security = NSAPI_SECURITY_NONE); //not supported
		virtual 		int set_channel(uint8_t channel);
		virtual 		int8_t get_rssi();
		virtual 		int connect();
		virtual 		int scan(WiFiAccessPoint *res, unsigned count);


    virtual NetworkStack *get_stack()
    {
        return this;
    }

private:
    int         init(void);

    SPWFSA01 _spwf;
		SocketAddress addrs[8];
    bool _ids[SPWFSA_SOCKET_COUNT];
    bool isListening;
    bool isInitialized;
		bool dbg_on;

    void event(void);
    struct {
        void (*callback)(void *);
        void *data;
    } _cbs[SPWFSA_SOCKET_COUNT];
		
		//temporary callback data
		struct {
        void (*callback)(void *);
        void *data;
    } _interim_cb[SPWFSA_SOCKET_COUNT];
};


#endif

