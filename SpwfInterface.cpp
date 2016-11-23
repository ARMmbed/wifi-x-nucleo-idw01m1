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
  * @file    SpwfInterface.cpp 
  * @author  STMicroelectronics
  * @brief   Implementation of the NetworkStack for the SPWF Device
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

#include "SpwfInterface.h"
#include "mbed_debug.h"

// Various timeouts for different SPWF operations
#define SPWF_CONNECT_TIMEOUT 20000
//#define SPWF_SEND_TIMEOUT    500
#define SPWF_RECV_TIMEOUT    150
#define SPWF_MISC_TIMEOUT    15000

/** spwf_socket class
 *  Implementation of SPWF socket structure
 */
struct spwf_socket {
    int id;
    int server_port;
    nsapi_protocol_t proto;
    bool connected;
};

/**
* @brief  SpwfSAInterface constructor         
* @param  tx: Pin USART TX  
*         rx: Pin USART RX
*         rst: reset pin for Spwf module
*         wkup: reset pin for Spwf module
*         rts: Pin USART RTS
*         debug : not used
* @retval none
*/
SpwfSAInterface::SpwfSAInterface(PinName tx, PinName rx, bool debug)
    : _spwf(tx, rx, debug),
			dbg_on(debug)
{
    memset(_ids, 0, sizeof(_ids));
		memset(_cbs, 0, sizeof(_cbs));
		memset(_interim_cb , 0, sizeof(_interim_cb));

	  _spwf.attach(this, &SpwfSAInterface::event);

	  isInitialized = false;
    isListening = false;
}

/**
* @brief  SpwfSAInterface destructor         
* @param  none
* @retval none
*/
SpwfSAInterface::~SpwfSAInterface()
{
}

/**
* @brief  init function
          initializes SPWF FW and module         
* @param  none
* @retval error value
*/
int SpwfSAInterface::init(void) 
{
    _spwf.setTimeout(SPWF_MISC_TIMEOUT);
    if(_spwf.startup(0)) {
        isInitialized=true;
        return true;
    }
    else return NSAPI_ERROR_DEVICE_ERROR;
}

/**
* @brief  network connect
          connects to Access Point
* @param  ap: Access Point (AP) Name String  
*         pass_phrase: Password String for AP
*         security: type of NSAPI security supported
* @retval NSAPI Error Type
*/
int SpwfSAInterface::connect(const char *ap, 
                             const char *pass_phrase, 
                             nsapi_security_t security,
														 uint8_t channel)
{
    int mode;

    //initialize the device before connecting
    if(!isInitialized)
    {
        if(!init())
            return NSAPI_ERROR_DEVICE_ERROR;
    }

    _spwf.setTimeout(SPWF_CONNECT_TIMEOUT);   

    switch(security)
    {
        case NSAPI_SECURITY_NONE:
            mode = 0;
            pass_phrase = NULL;
            break;
        case NSAPI_SECURITY_WEP:
            mode = 1;
            break;
        case NSAPI_SECURITY_WPA:
        case NSAPI_SECURITY_WPA2:
            mode = 2;
            break;
        default:
            mode = 2;
            break;
    }
    return (_spwf.connect((char*)ap, (char*)pass_phrase, mode));
}

/**
* @brief  network disconnect
          disconnects from Access Point
* @param  none
* @retval NSAPI Error Type
*/
int SpwfSAInterface::disconnect()
{
    return (_spwf.disconnect());
}

/** 
* @brief  Get the local IP address
* @param  none
* @retval Null-terminated representation of the local IP address
*         or null if not yet connected
*/
const char *SpwfSAInterface::get_ip_address()
{
    return _spwf.getIPAddress();
}

/** 
* @brief  Get the MAC address
* @param  none
* @retval Null-terminated representation of the MAC address
*         or null if not yet connected
*/
const char *SpwfSAInterface::get_mac_address()
{
    return _spwf.getMACAddress();
}

/**
* @brief  open a socket handle
* @param  handle: Pointer to handle
*         proto: TCP/UDP protocol
* @retval NSAPI Error Type
*/
int SpwfSAInterface::socket_open(void **handle, nsapi_protocol_t proto)
{
    int id = -1;
		// id won't have -1 value now
		for (int i = 0; i < SPWFSA_SOCKET_COUNT; i++) {
        if (!_ids[i]) {
            id = i;
            _ids[i] = true;
						debug_if(dbg_on, "\r\nSocket open with id = %d \r\n",i);
            break;
        }
    }

    struct spwf_socket *socket = new struct spwf_socket;
    if (!socket) {
				debug_if(dbg_on, "NO Socket Error\r\n");
        return NSAPI_ERROR_NO_SOCKET;
    }

    socket->id = id;
    socket->server_port = id;
    socket->proto = proto;
    socket->connected = false;
    *handle = socket;
    return 0;
}

/**
* @brief  connect to a remote socket
* @param  handle: Pointer to socket handle
*         addr: Address to connect to
* @retval NSAPI Error Type
*/

/*
int SpwfSAInterface::socket_connect(void *handle, const SocketAddress &addr)
{
    int sock_id = 99;
    struct spwf_socket *socket = (struct spwf_socket *)handle;

    const char *proto = (socket->proto == NSAPI_UDP) ? "u" : "t";//"s" for secure socket?

		// Is callback attached to the socket we are trying to connect?
		if(!_cbs[socket->id].callback)
		{
			debug_if(true, "No callback attached on socket = %d \r\n ",socket->id);
			_cbs[socket->id].callback = _interim_cb.callback;
			_cbs[socket->id].data 		= _interim_cb.data;
			_ids[socket->id] = true;
		}

    if (!_spwf.open(proto, &sock_id, addr.get_ip_address(), addr.get_port())) {	//sock ID is allocated NOW
        return NSAPI_ERROR_DEVICE_ERROR;
    }

		addrs[sock_id] = addr;
    //TODO: Maintain a socket table to map socket ID to host & port
    //TODO: lookup on client table to see if already socket is allocated to same host/port
    //multimap <char *, vector <uint16_t> > ::iterator i = c_table.find((char*)ip);

		socket->connected = true;
		if(sock_id != socket->id)
			{
				debug_if(true, "socket->id = %d and sock_id = %d\r\n\r\n",socket->id, sock_id);

				if(!_interim_cb.callback)  // copy the data from the socket's callback that is no longer used to an extra callback
				{
					debug_if(true,"\r\n I check...\r\n");
					_interim_cb.callback = _cbs[sock_id].callback;
					_interim_cb.data    =  _cbs[sock_id].data;
				}
				_ids[socket->id] = false;
				_cbs[socket->id].callback = 0;
				_cbs[socket->id].data 		= 0;

				// initialize the new socket
        socket->id = sock_id;  //the socket ID of this Socket instance
        _ids[socket->id] = true;

				//sanity check, the new socket id that we got has a callback attached
				if(!_cbs[socket->id].callback)
				{
						debug_if(true,"\r\n II check...\r\n");
						_cbs[socket->id].callback = _interim_cb.callback;
						_cbs[socket->id].data     = _interim_cb.data;
				}
			}
    else
        return NSAPI_ERROR_NO_SOCKET;

    return 0;
}
*/

int SpwfSAInterface::socket_connect(void *handle, const SocketAddress &addr)
{
    int sock_id = 99;
    struct spwf_socket *socket = (struct spwf_socket *)handle;

    const char *proto = (socket->proto == NSAPI_UDP) ? "u" : "t";//"s" for secure socket?
		
		// do we need callback before connectng the socket ?
		//_cbs[socket->id].callback = _interim_cb[socket->id].callback;
		//_cbs[socket->id].data 		= _interim_cb[socket->id].data;

    if (!_spwf.open(proto, &sock_id, addr.get_ip_address(), addr.get_port())) {	//sock ID is allocated NOW
        return NSAPI_ERROR_DEVICE_ERROR;
    }
		addrs[sock_id] = addr;
		
		socket->connected = true;
		//if we have above callback
#if 0
		if(socket->id != sock_id)
		{
				_cbs[sock_id].callback = _interim_cb[socket->id].callback;
				_cbs[sock_id].data     = _interim_cb[socket->id].data;
				_cbs[socket->id].callback		= 0;
				_cbs[socket->id].data       = 0;
				_ids[socket->id]			 = false;
				_ids[sock_id]          = true;
				socket->id						 = sock_id;
		}
#endif
		
#if 1
		// if we don't have above callback
		_cbs[sock_id].callback = _interim_cb[socket->id].callback;
		_cbs[sock_id].data     = _interim_cb[socket->id].data;
		_ids[sock_id]    			 = true;
		if(socket->id != sock_id)
		{
			_ids[socket->id] = false;
			socket->id 			 = sock_id;
		}
#endif

    //TODO: Maintain a socket table to map socket ID to host & port
    //TODO: lookup on client table to see if already socket is allocated to same host/port
    //multimap <char *, vector <uint16_t> > ::iterator i = c_table.find((char*)ip);
    return 0;
}



/**
* @brief  bind to a port number and address
* @param  handle: Pointer to socket handle
*         proto: address to bind to
* @retval NSAPI Error Type
*/
int SpwfSAInterface::socket_bind(void *handle, const SocketAddress &address)
{
    struct spwf_socket *socket = (struct spwf_socket *)handle;    
    socket->server_port = address.get_port();
    return 0;
}

/**
* @brief  start listening on a port and address
* @param  handle: Pointer to handle
*         backlog: not used (always value is 1)
* @retval NSAPI Error Type
*/
int SpwfSAInterface::socket_listen(void *handle, int backlog)
{      
    return NSAPI_ERROR_UNSUPPORTED;
}

/**
* @brief  accept connections from remote sockets
* @param  handle: Pointer to handle of client socket (connecting)
*         proto: handle of server socket which will accept connections
* @retval NSAPI Error Type
*/
int SpwfSAInterface::socket_accept(nsapi_socket_t server, nsapi_socket_t *handle, SocketAddress *address)
{    
    return NSAPI_ERROR_UNSUPPORTED;
}

/**
* @brief  close a socket
* @param  handle: Pointer to handle
* @retval NSAPI Error Type
*/
int SpwfSAInterface::socket_close(void *handle)
{
	int err = 0;
	struct spwf_socket *socket = (struct spwf_socket *)handle;
	if(socket->id != SERVER_SOCKET_NO && _ids[socket->id]) {
	  debug_if(dbg_on,"\r\n SpwfSAInterface::socket_close \r\n");
    _spwf.setTimeout(SPWF_MISC_TIMEOUT);

    if(socket->id != -1) {
        if (_spwf.close(socket->id)) {
            if(socket->id==SERVER_SOCKET_NO)
                isListening = false;
            else {
                _ids[socket->id] = false;
								addrs[socket->id] = 0;
								//memset(addrs[socket->id],0x00,sizeof(addrs[socket->id]));
						}
        }
        else err = NSAPI_ERROR_DEVICE_ERROR;
    }
	}

  delete socket;
  return err;
}

/**
* @brief  write to a socket
* @param  handle: Pointer to handle
*         data: pointer to data
*         size: size of data
* @retval no of bytes sent
*/
int SpwfSAInterface::socket_send(void *handle, const void *data, unsigned size)
{
    struct spwf_socket *socket = (struct spwf_socket *)handle;
    //int err;

    /*if(socket->id==SERVER_SOCKET_NO)
        {
            if(socket->server_port==-1 || !isListening) 
                return NSAPI_ERROR_NO_SOCKET; //server socket not bound or not listening        

            err = _spwf.socket_server_write((uint16_t)size, (char*)data);
        }
    else
        {
            err = _spwf.send(socket->id, (char*)data, (uint32_t)size);
        }*/
    if (!_spwf.send(socket->id, (char*)data, (uint32_t)size)) {
        return NSAPI_ERROR_DEVICE_ERROR;
    }
    return size;
}

/**
* @brief  receive data on a socket
* @param  handle: Pointer to handle
*         data: pointer to data
*         size: size of data
* @retval no of bytes read
*/
int SpwfSAInterface::socket_recv(void *handle, void *data, unsigned size)
{
    struct spwf_socket *socket = (struct spwf_socket *)handle;
    int32_t recv;

    _spwf.setTimeout(SPWF_RECV_TIMEOUT);

    //CHECK:Receive for both Client and Server Sockets same?
    recv = _spwf.recv(socket->id, (char*)data, (uint32_t)size); 
		//debug_if(true,"\r\n recv = %d \r\n",recv);
    if (recv < 0) {
        //wait_ms(1);//delay of 1ms <for F4>??
        //printf(".");
        if (recv == -1) return NSAPI_ERROR_WOULD_BLOCK;//send this if we want to block call (else timeout will happen)
        else return NSAPI_ERROR_DEVICE_ERROR;
    }
    return recv;
}

/**
* @brief  send data to a udp socket
* @param  handle: Pointer to handle
*         addr: address of udp socket
*         data: pointer to data
*         size: size of data
* @retval no of bytes sent
*/
int SpwfSAInterface::socket_sendto(void *handle, const SocketAddress &addr, const void *data, unsigned size)
{
    struct spwf_socket *socket = (struct spwf_socket *)handle;
    if (!socket->connected) {
        int err = socket_connect(socket, addr);
        if (err < 0) {
            return err;
        }
    }
    return socket_send(socket, data, size);
}

/**
* @brief  receive data on a udp socket
* @param  handle: Pointer to handle
*         addr: address of udp socket
*         data: pointer to data
*         size: size of data
* @retval no of bytes read
*/
int SpwfSAInterface::socket_recvfrom(void *handle, SocketAddress *addr, void *data, unsigned size)
{
	  int32_t recv;
    struct spwf_socket *socket = (struct spwf_socket *)handle;
		struct SocketAddress *address = (struct SocketAddress *)addr;
		recv = socket_recv(socket, data, size);
		if(recv > 0)
				*address = addrs[socket->id];
    return recv;
}

/**
* @brief  attach function/callback to the socket
* @param  handle: Pointer to handle
*         callback: callback function pointer
*         data: pointer to data
* @retval none
*/
/*
void SpwfSAInterface::socket_attach(void *handle, void (*callback)(void *), void *data)
{
  //  debug_if(dbg_on, "SPWFINTERFACE :: socket_attach\r\n");
    struct spwf_socket *socket = (struct spwf_socket *)handle;
    _cbs[socket->id].callback = callback;
    _cbs[socket->id].data = data;   
}
*/

void SpwfSAInterface::socket_attach(void *handle, void (*callback)(void *), void *data)
{
    struct spwf_socket *socket = (struct spwf_socket *)handle;
		if(!callback) {
			set_cbs(socket->id,callback,data);
		}
		else {
			_interim_cb[socket->id].callback = callback;
			_interim_cb[socket->id].data = data;
		}
}

void SpwfSAInterface::event(void) {
    for (int i = 0; i < SPWFSA_SOCKET_COUNT; i++) {
        if (_cbs[i].callback) {
            _cbs[i].callback(_cbs[i].data);
        }
    }
}

void SpwfSAInterface::set_cbs(int id,void (*callback)(void *),void *data)
{
	_cbs[id].callback = callback;
	_cbs[id].data			= data;
}

/**
* @brief  utility debug function for printing to serial terminal
* @param  string: Pointer to data
* @retval none
*/
void SpwfSAInterface::debug(const char * string)
{
    //_spwf.debug_print(string);
}

int SpwfSAInterface::set_credentials(const char *ssid, const char *pass, nsapi_security_t security) //not supported
{
		return NSAPI_ERROR_UNSUPPORTED;
}


int SpwfSAInterface::set_channel(uint8_t channel)
{
		return NSAPI_ERROR_UNSUPPORTED;
}

int8_t SpwfSAInterface::get_rssi()
{
		return NSAPI_ERROR_UNSUPPORTED;
}

int SpwfSAInterface::connect()
{
		return NSAPI_ERROR_UNSUPPORTED;
}

int SpwfSAInterface::scan(WiFiAccessPoint *res, unsigned count)
{
		return NSAPI_ERROR_UNSUPPORTED;
}


