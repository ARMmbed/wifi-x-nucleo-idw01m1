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
  * @file    SpwfSAInterface.cpp
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

#include "SpwfSAInterface.h"
#include "mbed_debug.h"

/**
 * @brief  SpwfSAInterface constructor
 * @param  tx: Pin USART TX
 *         rx: Pin USART RX
 *         rts: Pin USART RTS
 *         cts: Pin USART RTS
 *         debug : not used
 * @retval none
 */
SpwfSAInterface::SpwfSAInterface(PinName tx, PinName rx,
                                 PinName rts, PinName cts, bool debug,
                                 PinName wakeup, PinName reset)
: _spwf(tx, rx, rts, cts, *this, debug, wakeup, reset),
  _dbg_on(debug)
{
    inner_constructor();
}

/**
 * @brief  init function
 *         initializes SPWF FW and module
 * @param  none
 * @retval error value
 */
nsapi_error_t SpwfSAInterface::init(void)
{
    _spwf.setTimeout(SPWF_INIT_TIMEOUT);

    if(_spwf.startup(0)) {
        return NSAPI_ERROR_OK;
    }
    else return NSAPI_ERROR_DEVICE_ERROR;
}

nsapi_error_t SpwfSAInterface::connect(void)
{
    int mode;
    char *pass_phrase = ap_pass;

    switch(ap_sec)
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

    // First: disconnect
    if(_connected_to_network) {
        if(!disconnect()) {
            return NSAPI_ERROR_DEVICE_ERROR;
        }
    }

    //initialize the device before connecting
    if(!_isInitialized)
    {
        if(init() != NSAPI_ERROR_OK) return NSAPI_ERROR_DEVICE_ERROR;
        _isInitialized=true;
    }

    // Then: (re-)connect
    _spwf.setTimeout(SPWF_CONNECT_TIMEOUT);

    if (!_spwf.connect(ap_ssid, pass_phrase, mode)) {
        return NSAPI_ERROR_NO_CONNECTION;
    }

    if (!_spwf.getIPAddress()) {
        return NSAPI_ERROR_DHCP_FAILURE;
    }

    _connected_to_network = true;
    return NSAPI_ERROR_OK;
}

/**
 * @brief  network connect
 *         connects to Access Point
 * @param  ap: Access Point (AP) Name String
 *         pass_phrase: Password String for AP
 *         security: type of NSAPI security supported
 * @retval NSAPI Error Type
 */
nsapi_error_t SpwfSAInterface::connect(const char *ssid, const char *pass, nsapi_security_t security,
                                       uint8_t channel)
{
    if (channel != 0) {
        return NSAPI_ERROR_UNSUPPORTED;
    }

    set_credentials(ssid, pass, security);
    return connect();
}

/**
 * @brief  network disconnect
 *         disconnects from Access Point
 * @param  none
 * @retval NSAPI Error Type
 */
nsapi_error_t SpwfSAInterface::disconnect(void)
{
    _spwf.setTimeout(SPWF_DISCONNECT_TIMEOUT);

    if (!_spwf.disconnect()) {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    _connected_to_network = false;
    _isInitialized = false;

    return NSAPI_ERROR_OK;
}

/** 
 * @brief  Get the local IP address
 * @param  none
 * @retval Null-terminated representation of the local IP address
 *         or null if not yet connected
 */
const char *SpwfSAInterface::get_ip_address(void)
{
    _spwf.setTimeout(SPWF_MISC_TIMEOUT);
    return _spwf.getIPAddress();
}

/** 
 * @brief  Get the MAC address
 * @param  none
 * @retval Null-terminated representation of the MAC address
 *         or null if not yet connected
 */
const char *SpwfSAInterface::get_mac_address(void)
{
    _spwf.setTimeout(SPWF_MISC_TIMEOUT);
    return _spwf.getMACAddress();
}

const char *SpwfSAInterface::get_gateway(void)
{
    if(!_connected_to_network) return NULL;

    _spwf.setTimeout(SPWF_MISC_TIMEOUT);
    return _spwf.getGateway();
}

const char *SpwfSAInterface::get_netmask(void)
{
    if(!_connected_to_network) return NULL;

    _spwf.setTimeout(SPWF_MISC_TIMEOUT);
    return _spwf.getNetmask();
}

/**
 * @brief  open a socket handle
 * @param  handle: Pointer to handle
 *         proto: TCP/UDP protocol
 * @retval NSAPI Error Type
 */
nsapi_error_t SpwfSAInterface::socket_open(void **handle, nsapi_protocol_t proto)
{
    int internal_id;

    for (internal_id = 0; internal_id < SPWFSA_SOCKET_COUNT; internal_id++) {
        if (_ids[internal_id].internal_id == SPWFSA_SOCKET_COUNT) break;
    }

    if(internal_id == SPWFSA_SOCKET_COUNT) {
        debug_if(_dbg_on, "NO Socket ID Error\r\n");
        return NSAPI_ERROR_NO_SOCKET;
    }

    spwf_socket_t *socket = &_ids[internal_id];
    socket->internal_id = internal_id;
    socket->spwf_id = SPWFSA_SOCKET_COUNT;
    socket->server_gone = false;
    socket->no_more_data = false;
    socket->proto = proto;
    socket->addr = SocketAddress();

    *handle = socket;
    return NSAPI_ERROR_OK;
}

/**
 * @brief  connect to a remote socket
 * @param  handle: Pointer to socket handle
 *         addr: Address to connect to
 * @retval NSAPI Error Type
 */
nsapi_error_t SpwfSAInterface::socket_connect(void *handle, const SocketAddress &addr)
{
    spwf_socket_t *socket = (spwf_socket_t*)handle;

    MBED_ASSERT(socket->internal_id != SPWFSA_SOCKET_COUNT);

    if(_socket_has_connected(socket->internal_id)) return NSAPI_ERROR_IS_CONNECTED;

    _spwf.setTimeout(SPWF_OPEN_TIMEOUT);

    const char *proto = (socket->proto == NSAPI_UDP) ? "u" : "t"; //"s" for secure socket?

    if (!_spwf.open(proto, &socket->spwf_id, addr.get_ip_address(), addr.get_port())) {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    /* check for the module to report a valid id */
    MBED_ASSERT(((unsigned int)socket->spwf_id) < ((unsigned int)SPWFSA_SOCKET_COUNT));

    _internal_ids[socket->spwf_id] = socket->internal_id;
    socket->addr = addr;
    return NSAPI_ERROR_OK;
}

nsapi_error_t SpwfSAInterface::socket_bind(void *handle, const SocketAddress &address)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t SpwfSAInterface::socket_listen(void *handle, int backlog)
{      
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t SpwfSAInterface::socket_accept(nsapi_socket_t server, nsapi_socket_t *handle, SocketAddress *address)
{    
    return NSAPI_ERROR_UNSUPPORTED;
}

nsapi_error_t SpwfSAInterface::socket_close(void *handle)
{
    spwf_socket_t *socket = (spwf_socket_t*)handle;
    int internal_id = socket->internal_id;

    if(!_socket_is_open(internal_id)) return NSAPI_ERROR_NO_SOCKET;

    if(_socket_has_connected(socket)) {
        _spwf.setTimeout(SPWF_CLOSE_TIMEOUT);
        if (!_spwf.close(socket->spwf_id)) {
            return NSAPI_ERROR_DEVICE_ERROR;
        }
        _internal_ids[socket->spwf_id] = SPWFSA_SOCKET_COUNT;
    }

    _ids[internal_id].internal_id = SPWFSA_SOCKET_COUNT;
    _ids[internal_id].spwf_id = SPWFSA_SOCKET_COUNT;

    return NSAPI_ERROR_OK;
}

/**
 * @brief  write to a socket
 * @param  handle: Pointer to handle
 *         data: pointer to data
 *         size: size of data
 * @retval no of bytes sent
 */
nsapi_size_or_error_t SpwfSAInterface::socket_send(void *handle, const void *data, unsigned size)
{
    spwf_socket_t *socket = (spwf_socket_t*)handle;

    CHECK_NOT_CONNECTED_ERR();

    _spwf.setTimeout(SPWF_SEND_TIMEOUT);

    if(!_socket_is_still_connected(socket)) {
        return NSAPI_ERROR_CONNECTION_LOST;
    }

    if (!_spwf.send(socket->spwf_id, data, size)) {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    return size;
}

/**
 * @brief  receive data on a socket
 * @param  handle: Pointer to handle
 *         data: pointer to data
 *         size: size of data
 * @retval no of bytes read or negative error code in case of error
 */
nsapi_size_or_error_t SpwfSAInterface::socket_recv(void *handle, void *data, unsigned size)
{
    spwf_socket_t *socket = (spwf_socket_t*)handle;

    CHECK_NOT_CONNECTED_ERR();

    if(!_socket_might_have_data(socket)) {
        return NSAPI_ERROR_CONNECTION_LOST;
    }

    _spwf.setTimeout(SPWF_RECV_TIMEOUT);

    int32_t recv = _spwf.recv(socket->spwf_id, (char*)data, (uint32_t)size);

    MBED_ASSERT((recv != 0) || (size == 0));

    if (recv < 0) {
        if(!_socket_is_still_connected(socket)) {
            socket->no_more_data = true;
            return NSAPI_ERROR_CONNECTION_LOST;
        }

        return NSAPI_ERROR_WOULD_BLOCK;
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
nsapi_size_or_error_t SpwfSAInterface::socket_sendto(void *handle, const SocketAddress &addr, const void *data, unsigned size)
{
    spwf_socket_t *socket = (spwf_socket_t*)handle;

    CHECK_NOT_CONNECTED_ERR();

    if ((_socket_has_connected(socket)) && (socket->addr != addr)) {
        _spwf.setTimeout(SPWF_CLOSE_TIMEOUT);
        if (!_spwf.close(socket->spwf_id)) {
            return NSAPI_ERROR_DEVICE_ERROR;
        }
        _internal_ids[socket->spwf_id] = SPWFSA_SOCKET_COUNT;
        socket->spwf_id = SPWFSA_SOCKET_COUNT;
    }

    _spwf.setTimeout(SPWF_CONN_SND_TIMEOUT);
    if (!_socket_has_connected(socket)) {
        nsapi_error_t err = socket_connect(socket, addr);
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
nsapi_size_or_error_t SpwfSAInterface::socket_recvfrom(void *handle, SocketAddress *addr, void *data, unsigned size)
{
    spwf_socket_t *socket = (spwf_socket_t*)handle;
    nsapi_error_t ret;

    CHECK_NOT_CONNECTED_ERR();

    ret = socket_recv(socket, data, size);
    if (ret >= 0 && addr) {
        *addr = socket->addr;
    }

    return ret;
}

/**
 * @brief  attach function/callback to the socket
 * @param  handle: Pointer to handle
 *         callback: callback function pointer
 *         data: pointer to data
 * @retval none
 */
void SpwfSAInterface::socket_attach(void *handle, void (*callback)(void *), void *data)
{
    spwf_socket_t *socket = (spwf_socket_t*)handle;

    if(!_socket_is_open(socket)) return; // might happen after module hard fault

    _cbs[socket->internal_id].callback = callback;
    _cbs[socket->internal_id].data = data;
}

void SpwfSAInterface::event(void) {
    for (int internal_id = 0; internal_id < SPWFSA_SOCKET_COUNT; internal_id++) {
        if (_cbs[internal_id].callback && (_ids[internal_id].internal_id != SPWFSA_SOCKET_COUNT)) {
            _cbs[internal_id].callback(_cbs[internal_id].data);
        }
    }
}

nsapi_error_t SpwfSAInterface::set_credentials(const char *ssid, const char *pass, nsapi_security_t security)
{
    memset(ap_ssid, 0, sizeof(ap_ssid));
    strncpy(ap_ssid, ssid, sizeof(ap_ssid));

    memset(ap_pass, 0, sizeof(ap_pass));
    strncpy(ap_pass, pass, sizeof(ap_pass));

    ap_sec = security;

    return NSAPI_ERROR_OK;
}

nsapi_error_t SpwfSAInterface::set_channel(uint8_t channel)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

int8_t SpwfSAInterface::get_rssi(void)
{
    if(!_connected_to_network) return 0;

    _spwf.setTimeout(SPWF_MISC_TIMEOUT);
    return _spwf.getRssi();
}

nsapi_size_or_error_t SpwfSAInterface::scan(WiFiAccessPoint *res, unsigned count)
{
    nsapi_size_or_error_t ret;

    //initialize the device before scanning
    if(!_isInitialized)
    {
        if(init() != NSAPI_ERROR_OK) return NSAPI_ERROR_DEVICE_ERROR;
    }

    _spwf.setTimeout(SPWF_SCAN_TIMEOUT);
    ret = _spwf.scan(res, count);

    //de-initialize the device after scanning
    if(!_isInitialized)
    {
        nsapi_error_t err = disconnect();
        if(err != NSAPI_ERROR_OK) return err;
    }

    return ret;
}
