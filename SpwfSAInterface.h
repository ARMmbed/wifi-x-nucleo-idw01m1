/* mbed Microcontroller Library
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

/**
  ******************************************************************************
  * @file    SpwfSAInterface.h
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

#include <limits.h>

#include "mbed.h"

#define IDW01M1 1
#define IDW04A1 2

#if MBED_CONF_IDW0XX1_EXPANSION_BOARD == IDW01M1
#include "SPWFSA01/SPWFSA01.h"
#elif MBED_CONF_IDW0XX1_EXPANSION_BOARD == IDW04A1
#include "SPWFSA04/SPWFSA04.h"
#else
#error No (valid) Wi-Fi exapnsion board defined (MBED_CONF_IDW0XX1_EXPANSION_BOARD: options are IDW01M1 and IDW04A1)
#endif

/* Max number of sockets */
#define SPWFSA_SOCKET_COUNT 8

// Various timeouts for different SPWF operations
#define SPWF_CONNECT_TIMEOUT    60000
#define SPWF_DISCONNECT_TIMEOUT 30002
#define SPWF_HF_TIMEOUT         30001
#define SPWF_NETLOST_TIMEOUT    30000
#define SPWF_READ_BIN_TIMEOUT   13000
#define SPWF_SEND_TIMEOUT       10000
#define SPWF_INIT_TIMEOUT       6000
#define SPWF_CLOSE_TIMEOUT      5003
#define SPWF_OPEN_TIMEOUT       5002
#define SPWF_CONN_SND_TIMEOUT   5001
#define SPWF_SCAN_TIMEOUT       5000
#define SPWF_MISC_TIMEOUT       301
#define SPWF_RECV_TIMEOUT       300

/** SpwfSAInterface class
 *  Implementation of the NetworkStack for the SPWF Device
 *  NOTE - betzw - TODO: MUST become singleton!
 */
class SpwfSAInterface : public NetworkStack, public WiFiInterface
{
public:
    /** SpwfSAInterface constructor
     * @param tx        TX pin
     * @param rx        RX pin
     * @param rts       RTS pin
     * @param cts       CTS pin
     * @param debug     Enable debugging
     * @param wakeup    Wakeup pin
     * @param reset     Reset pin
     */
    SpwfSAInterface(PinName tx, PinName rx,
                    PinName rts = NC, PinName cts = NC, bool debug = false,
                    PinName wakeup = SPWFSAXX_WAKEUP_PIN, PinName reset = SPWFSAXX_RESET_PIN);

    /** Start the interface
     *
     *  Attempts to connect to a WiFi network. Requires ssid and passphrase to be set.
     *  If passphrase is invalid, NSAPI_ERROR_AUTH_ERROR is returned.
     *
     *  @return         0 on success, negative error code on failure
     */
    virtual nsapi_error_t connect();

    /** Start the interface
     *
     *  Attempts to connect to a WiFi network.
     *
     *  @param ssid      Name of the network to connect to
     *  @param pass      Security passphrase to connect to the network
     *  @param security  Type of encryption for connection (Default: NSAPI_SECURITY_NONE)
     *  @param channel   This parameter is not supported, setting it to anything else than 0 will result in NSAPI_ERROR_UNSUPPORTED
     *  @return          0 on success, or error code on failure
     */
    virtual nsapi_error_t connect(const char *ssid, const char *pass, nsapi_security_t security = NSAPI_SECURITY_NONE,
                                  uint8_t channel = 0);

    /** Set the WiFi network credentials
     *
     *  @param ssid      Name of the network to connect to
     *  @param pass      Security passphrase to connect to the network
     *  @param security  Type of encryption for connection
     *                   (defaults to NSAPI_SECURITY_NONE)
     *  @return          0 on success, or error code on failure
     */
    virtual nsapi_error_t set_credentials(const char *ssid, const char *pass, nsapi_security_t security = NSAPI_SECURITY_NONE);

    /** Set the WiFi network channel - NOT SUPPORTED
     *
     *  This function is not supported and will return NSAPI_ERROR_UNSUPPORTED
     *
     *  @param channel   Channel on which the connection is to be made, or 0 for any (Default: 0)
     *  @return          Not supported, returns NSAPI_ERROR_UNSUPPORTED
     */
    virtual nsapi_error_t set_channel(uint8_t channel);

    /** Stop the interface
     *  @return             0 on success, negative on failure
     */
    virtual nsapi_error_t disconnect();

    /** Get the internally stored IP address
     *  @return             IP address of the interface or null if not yet connected
     */
    virtual const char *get_ip_address();

    /** Get the internally stored MAC address
     *  @return             MAC address of the interface
     */
    virtual const char *get_mac_address();

    /** Get the local gateway
     *
     *  @return         Null-terminated representation of the local gateway
     *                  or null if no network mask has been recieved
     */
    virtual const char *get_gateway();

    /** Get the local network mask
     *
     *  @return         Null-terminated representation of the local network mask
     *                  or null if no network mask has been recieved
     */
    virtual const char *get_netmask();

    /** Gets the current radio signal strength for active connection
     *
     * @return          Connection strength in dBm (negative value)
     */
    virtual int8_t get_rssi();

    /** Scan for available networks
     *
     *  This function will block. If the @a count is 0, function will only return count of available networks, so that
     *  user can allocated necessary memory. If the @count is grater than 0 and the @a ap is not NULL it'll be populated
     *  with discovered networks up to value of @a count.
     *
     *  @param  res      Pointer to allocated array to store discovered AP
     *  @param  count    Size of allocated @a res array, or 0 to only count available AP
     *  @return          Number of entries in @a, or if @a count was 0 number of available networks,
     *                   negative on error see @a nsapi_error
     */
    virtual nsapi_size_or_error_t scan(WiFiAccessPoint *res, unsigned count);

    /** Translates a hostname to an IP address with specific version
     *
     *  The hostname may be either a domain name or an IP address. If the
     *  hostname is an IP address, no network transactions will be performed.
     *
     *  If no stack-specific DNS resolution is provided, the hostname
     *  will be resolve using a UDP socket on the stack.
     *
     *  @param address  Destination for the host SocketAddress
     *  @param host     Hostname to resolve
     *  @param version  IP version of address to resolve, NSAPI_UNSPEC indicates
     *                  version is chosen by the stack (defaults to NSAPI_UNSPEC)
     *  @return         0 on success, negative error code on failure
     */
    using NetworkInterface::gethostbyname;

    /** Add a domain name server to list of servers to query
     *
     *  @param addr     Destination for the host address
     *  @return         0 on success, negative error code on failure
     */
    using NetworkInterface::add_dns_server;

private:
    /** Open a socket
     *  @param handle       Handle in which to store new socket
     *  @param proto        Type of socket to open, NSAPI_TCP or NSAPI_UDP
     *  @return             0 on success, negative on failure
     */
    virtual nsapi_error_t socket_open(void **handle, nsapi_protocol_t proto);

    /** Close the socket
     *  @param handle       Socket handle
     *  @return             0 on success, negative on failure
     *  @note On failure, any memory associated with the socket must still
     *        be cleaned up
     */
    virtual nsapi_error_t socket_close(void *handle);

    /** Bind a server socket to a specific port - NOT SUPPORTED
     *
     *  This function is not supported and will return NSAPI_ERROR_UNSUPPORTED
     *
     *  @param handle       Socket handle
     *  @param address      Local address to listen for incoming connections on
     *  @return             Not supported, returns NSAPI_ERROR_UNSUPPORTED
     */
    virtual nsapi_error_t socket_bind(void *handle, const SocketAddress &address);

    /** Start listening for incoming connectionst - NOT SUPPORTED
     *
     *  This function is not supported and will return NSAPI_ERROR_UNSUPPORTED
     *
     *  @param handle       Socket handle
     *  @param backlog      Number of pending connections that can be queued up at any
     *                      one time [Default: 1]
     *  @return             Not supported, returns NSAPI_ERROR_UNSUPPORTED
     */
    virtual nsapi_error_t socket_listen(void *handle, int backlog);

    /** Connects this TCP socket to the server
     *  @param handle       Socket handle
     *  @param address      SocketAddress to connect to
     *  @return             0 on success, negative on failure
     */
    virtual nsapi_error_t socket_connect(void *handle, const SocketAddress &address);

    /** Accept a new connection - NOT SUPPORTED
     *
     *  This function is not supported and will return NSAPI_ERROR_UNSUPPORTED
     *
     *  @param handle       Handle in which to store new socket
     *  @param server       Socket handle to server to accept from
     *  @return             Not supported, returns NSAPI_ERROR_UNSUPPORTED
     */
    virtual nsapi_error_t socket_accept(void *handle, void **socket, SocketAddress *address);

    /** Send data to the remote host
     *  @param handle       Socket handle
     *  @param data         The buffer to send to the host
     *  @param size         The length of the buffer to send
     *  @return             Number of written bytes on success, negative on failure
     *  @note This call is not-blocking, if this call would block, must
     *        immediately return NSAPI_ERROR_WOULD_WAIT
     */
    virtual nsapi_size_or_error_t socket_send(void *handle, const void *data, unsigned size);

    /** Receive data from the remote host
     *  @param handle       Socket handle
     *  @param data         The buffer in which to store the data received from the host
     *  @param size         The maximum length of the buffer
     *  @return             Number of received bytes on success, negative on failure
     *  @note This call is not-blocking, if this call would block, must
     *        immediately return NSAPI_ERROR_WOULD_WAIT
     */
    virtual nsapi_size_or_error_t socket_recv(void *handle, void *data, unsigned size);

    /** Send a packet to a remote endpoint
     *  @param handle       Socket handle
     *  @param address      The remote SocketAddress
     *  @param data         The packet to be sent
     *  @param size         The length of the packet to be sent
     *  @return             The number of written bytes on success, negative on failure
     *  @note This call is not-blocking, if this call would block, must
     *        immediately return NSAPI_ERROR_WOULD_WAIT
     */
    virtual nsapi_size_or_error_t socket_sendto(void *handle, const SocketAddress &address, const void *data, unsigned size);

    /** Receive a packet from a remote endpoint
     *  @param handle       Socket handle
     *  @param address      Destination for the remote SocketAddress or null
     *  @param buffer       The buffer for storing the incoming packet data
     *                      If a packet is too long to fit in the supplied buffer,
     *                      excess bytes are discarded
     *  @param size         The length of the buffer
     *  @return             The number of received bytes on success, negative on failure
     *  @note This call is not-blocking, if this call would block, must
     *        immediately return NSAPI_ERROR_WOULD_WAIT
     */
    virtual nsapi_size_or_error_t socket_recvfrom(void *handle, SocketAddress *address, void *buffer, unsigned size);

    /** Register a callback on state change of the socket
     *  @param handle       Socket handle
     *  @param callback     Function to call on state change
     *  @param data         Argument to pass to callback
     *  @note Callback may be called in an interrupt context.
     */
    virtual void socket_attach(void *handle, void (*callback)(void *), void *data);

    /** Provide access to the NetworkStack object
     *
     *  @return The underlying NetworkStack object
     */
    virtual NetworkStack *get_stack()
    {
        return this;
    }

private:
    /** spwf_socket class
     *  Implementation of SPWF socket structure
     */
    typedef struct spwf_socket {
        int8_t internal_id;
        int spwf_id;
        bool server_gone;
        bool no_more_data;
        nsapi_protocol_t proto;
        SocketAddress addr;
    } spwf_socket_t;

    static bool _socket_is_open(spwf_socket_t *sock) {
        return (sock->internal_id != SPWFSA_SOCKET_COUNT);
    }

    static bool _socket_has_connected(spwf_socket_t *sock) {
        return (_socket_is_open(sock) && (sock->spwf_id != SPWFSA_SOCKET_COUNT));
    }

    bool _socket_is_still_connected(spwf_socket_t *sock) {
        return (_socket_has_connected(sock) && !sock->server_gone);
    }

    bool _socket_might_have_data(spwf_socket_t *sock) {
        return (_socket_has_connected(sock) && !sock->no_more_data);
    }

    bool _socket_is_open(int internal_id) {
        return (internal_id != SPWFSA_SOCKET_COUNT);
    }

    bool _socket_has_connected(int internal_id) {
        if(!_socket_is_open(internal_id)) return false;

        spwf_socket_t &sock = _ids[internal_id];
        return (sock.spwf_id != SPWFSA_SOCKET_COUNT);
    }

    bool _socket_is_still_connected(int internal_id) {
        if(!_socket_has_connected(internal_id)) return false;

        spwf_socket_t &sock = _ids[internal_id];
        return (!sock.server_gone);
    }

    bool _socket_might_have_data(int internal_id) {
        if(!_socket_is_still_connected(internal_id)) return false;

        spwf_socket_t &sock = _ids[internal_id];
        return (!sock.no_more_data);
    }

#if MBED_CONF_IDW0XX1_EXPANSION_BOARD == IDW01M1
    SPWFSA01 _spwf;
#elif MBED_CONF_IDW0XX1_EXPANSION_BOARD == IDW04A1
    SPWFSA04 _spwf;
#endif

    bool _isInitialized;
    bool _dbg_on;
    bool _connected_to_network;

    spwf_socket_t _ids[SPWFSA_SOCKET_COUNT];
    struct {
        void (*callback)(void *);
        void *data;
    } _cbs[SPWFSA_SOCKET_COUNT];
    int _internal_ids[SPWFSA_SOCKET_COUNT];

    char ap_ssid[33]; /* 32 is what 802.11 defines as longest possible name; +1 for the \0 */
    nsapi_security_t ap_sec;
    char ap_pass[64]; /* The longest allowed passphrase */

private:
    void event(void);
    nsapi_error_t init(void);

    int get_internal_id(int spwf_id) { // checks also if `spwf_id` is still "valid"
        MBED_ASSERT(spwf_id != SPWFSA_SOCKET_COUNT);

        int internal_id = _internal_ids[spwf_id];
        if((_socket_is_open(internal_id)) && (_ids[internal_id].spwf_id == spwf_id)) {
            return internal_id;
        } else {
            return SPWFSA_SOCKET_COUNT;
        }
    }

    /* Called at initialization or after module hard fault */
    void inner_constructor() {
        memset(_ids, 0, sizeof(_ids));
        memset(_cbs, 0, sizeof(_cbs));
        memset(_internal_ids, SPWFSA_SOCKET_COUNT, sizeof(_internal_ids));

        for (int internal_id = 0; internal_id < SPWFSA_SOCKET_COUNT; internal_id++) {
            _ids[internal_id].internal_id = SPWFSA_SOCKET_COUNT;
            _ids[internal_id].spwf_id = SPWFSA_SOCKET_COUNT;
        }

        _spwf.attach(this, &SpwfSAInterface::event);

        _connected_to_network = false;
        _isInitialized = false;
    }

private:
    friend class SPWFSAxx;
    friend class SPWFSA01;
    friend class SPWFSA04;
};

#define CHECK_NOT_CONNECTED_ERR() { \
        if(!_connected_to_network) return NSAPI_ERROR_NO_CONNECTION; \
} \


#endif
