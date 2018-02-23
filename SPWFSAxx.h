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
 
#ifndef SPWFSAXX_H
#define SPWFSAXX_H

#include "mbed.h"
#include "ATCmdParser.h"
#include "BlockExecuter.h"

/* Common SPWFSAxx macros */
#define SPWFXX_WINDS_LOW_ON         "0x00000000"
#define SPWFXX_DEFAULT_BAUD_RATE    115200
#define SPWFXX_MAX_TRIALS           3

#if !defined(SPWFSAXX_RTS_PIN)
#define SPWFSAXX_RTS_PIN    NC
#endif // !defined(SPWFSAXX_RTS_PIN)
#if !defined(SPWFSAXX_CTS_PIN)
#define SPWFSAXX_CTS_PIN    NC
#endif // !defined(SPWFSAXX_CTS_PIN)

#define SPWFXX_ERR_OK               (+1)
#define SPWFXX_ERR_OOM              (-1)
#define SPWFXX_ERR_READ             (-2)
#define SPWFXX_ERR_LEN              (-3)


/* Max number of sockets & packets */
#define SPWFSA_SOCKET_COUNT         (8)
#define SPWFSA_MAX_PACKETS          (4)

#define PENDING_DATA_SLOTS          (13)

/* Pending data packets size buffer */
class SpwfRealPendingPackets {
public:
    SpwfRealPendingPackets() {
        reset();
    }

    void add(uint32_t new_cum_size) {
        MBED_ASSERT(new_cum_size >= cumulative_size);

        if(new_cum_size == cumulative_size) {
            /* nothing to do */
            return;
        }

        /* => `new_cum_size > cumulative_size` */
        real_pkt_sizes[last_pkt_ptr] = (uint16_t)(new_cum_size - cumulative_size);
        cumulative_size = new_cum_size;

        last_pkt_ptr = (last_pkt_ptr + 1) % PENDING_DATA_SLOTS;

        MBED_ASSERT(first_pkt_ptr != last_pkt_ptr);
    }

    uint32_t get(void) {
        if(empty()) return 0;

        return real_pkt_sizes[first_pkt_ptr];
    }

    uint32_t cumulative(void) {
        return cumulative_size;
    }

    uint32_t remove(uint32_t size) {
        MBED_ASSERT(!empty());

        uint32_t ret = real_pkt_sizes[first_pkt_ptr];
        first_pkt_ptr = (first_pkt_ptr + 1) % PENDING_DATA_SLOTS;

        MBED_ASSERT(ret == size);
        MBED_ASSERT(ret <= cumulative_size);
        cumulative_size -= ret;

        return ret;
    }

    void reset(void) {
        memset(this, 0, sizeof(*this));
    }

private:
    bool empty(void) {
        if(first_pkt_ptr == last_pkt_ptr) {
            MBED_ASSERT(cumulative_size == 0);
            return true;
        }
        return false;
    }

    uint16_t real_pkt_sizes[PENDING_DATA_SLOTS];
    uint8_t  first_pkt_ptr;
    uint8_t  last_pkt_ptr;
    uint32_t cumulative_size;
};

class SpwfSAInterface;

/** SPWFSAxx Interface class.
    This is an interface to a SPWFSAxx module.
 */
class SPWFSAxx
{
private:
    /* abstract class*/
    SPWFSAxx(PinName tx, PinName rx, PinName rts, PinName cts,
             SpwfSAInterface &ifce, bool debug,
             PinName wakeup, PinName reset);

public:
    /**
     * Init the SPWFSAxx
     *
     * @param mode mode in which to startup
     * @return true only if SPWFSAxx has started up correctly
     */
    bool startup(int mode);

    /**
     * Connect SPWFSAxx to AP
     *
     * @param ap the name of the AP
     * @param passPhrase the password of AP
     * @param securityMode the security mode of AP (WPA/WPA2, WEP, Open)
     * @return true only if SPWFSAxx is connected successfully
     */
    bool connect(const char *ap, const char *passPhrase, int securityMode);

    /**
     * Disconnect SPWFSAxx from AP
     *
     * @return true only if SPWFSAxx is disconnected successfully
     */
    bool disconnect(void);

    /**
     * Get the IP address of SPWFSAxx
     *
     * @return null-terminated IP address or null if no IP address is assigned
     */
    const char *getIPAddress(void);

    /**
     * Get the MAC address of SPWFSAxx
     *
     * @return null-terminated MAC address or null if no MAC address is assigned
     */
    const char *getMACAddress(void);

    /** Get the local gateway
     *
     *  @return         Null-terminated representation of the local gateway
     *                  or null if no network mask has been received
     */
    const char *getGateway(void);

    /** Get the local network mask
     *
     *  @return         Null-terminated representation of the local network mask
     *                  or null if no network mask has been received
     */
    const char *getNetmask(void);

    /** Gets the current radio signal strength for active connection
     *
     * @return          Connection strength in dBm (negative value)
     */
    int8_t getRssi();

    /**
     * Sends data to an open socket
     *
     * @param id id of socket to send to
     * @param data data to be sent
     * @param amount amount of data to be sent - max 1024
     * @return true only if data sent successfully
     */
    bool send(int id, const void *data, uint32_t amount);

    /**
     * Receives data from an open socket
     *
     * @param id id to receive from
     * @param data placeholder for returned information
     * @param amount number of bytes to be received
     * @param datagram receive a datagram packet
     * @return the number of bytes received
     */
    int32_t recv(int id, void *data, uint32_t amount, bool datagram);

    /**
     * Closes a socket
     *
     * @param id id of socket to close, valid only 0-4
     * @return true only if socket is closed successfully
     */
    bool close(int id);

    /**
     * Allows timeout to be changed between commands
     *
     * @param timeout_ms timeout of the connection
     */
    void setTimeout(uint32_t timeout_ms);

    /**
     * Attach a function to call whenever network state has changed
     *
     * @param func A pointer to a void function, or 0 to set as none
     */
    void attach(Callback<void()> func);

    /**
     * Attach a function to call whenever network state has changed
     *
     * @param obj pointer to the object to call the member function on
     * @param method pointer to the member function to call
     */
    template <typename T, typename M>
    void attach(T *obj, M method) {
        attach(Callback<void()>(obj, method));
    }

    static const char _cr_ = '\x0d'; // '\r' carriage return
    static const char _lf_ = '\x0a'; // '\n' line feed

private:
    UARTSerial _serial;
    ATCmdParser _parser;

    DigitalOut _wakeup;
    DigitalOut _reset;
    PinName _rts;
    PinName _cts;

    int _timeout;
    bool _dbg_on;

    int _pending_sockets_bitmap;
    SpwfRealPendingPackets _pending_pkt_sizes[SPWFSA_SOCKET_COUNT];

    bool _network_lost_flag;
    SpwfSAInterface &_associated_interface;

    /**
     * Reset SPWFSAxx
     *
     * @return true only if SPWFSAxx resets successfully
     */
    bool hw_reset(void);
    bool reset(void);

    /**
     * Check if SPWFSAxx is connected
     *
     * @return true only if the chip has an IP address
     */
    bool isConnected(void);

    /**
     * Checks if data is available
     */
    bool readable(void) {
        return _serial.FileHandle::readable();
    }

    /**
     * Checks if data can be written
     */
    bool writeable(void) {
        return _serial.FileHandle::writable();
    }

    /**
     * Try to empty RX buffer
     * Can be used when commands fail receiving expected response to try to recover situation
     * @note Gives no guarantee that situation improves
     */
    void empty_rx_buffer(void) {
        while(readable()) _parser.getc();
    }

    /* block calling (external) callback */
    volatile unsigned int _call_event_callback_blocked;
    Callback<void()> _callback_func;

    struct packet {
        struct packet *next;
        int id;
        uint32_t len;
        // data follows
    } *_packets, **_packets_end;

    void _packet_handler_th(void);
    void _execute_bottom_halves(void);
    void _network_lost_handler_th(void);
    void _network_lost_handler_bh(void);
    void _hard_fault_handler(void);
    void _wifi_hwfault_handler(void);
    void _server_gone_handler(void);
#if MBED_CONF_IDW0XX1_EXPANSION_BOARD == IDW04A1
    void _skip_oob(void);
#endif
    bool _wait_wifi_hw_started(void);
    bool _wait_console_active(void);
    int _read_len(int);
    int _flush_in(char*, int);
    bool _winds_off(void);
    void _winds_on(void);
    void _read_in_pending(void);
    int _read_in_pkt(int spwf_id, bool close);
    int _read_in_packet(int spwf_id, uint32_t amount);
    void _recover_from_hard_faults(void);
    void _free_packets(int spwf_id);
    void _free_all_packets(void);
    void _process_winds();

    virtual int _read_in(char*, int, uint32_t) = 0;

    bool _recv_delim_lf(void) {
        return (_parser.getc() == _lf_);
    }

    bool _recv_delim_cr(void) {
        return (_parser.getc() == _cr_);
    }

    bool _recv_delim_cr_lf(void) {
        return _recv_delim_cr() && _recv_delim_lf();
    }

    bool _recv_ok(void) {
        return _parser.recv(SPWFXX_RECV_OK) && _recv_delim_lf();
    }

    void _add_pending_packet_sz(int spwf_id, uint32_t size);
    void _add_pending_pkt_size(int spwf_id, uint32_t size) {
        _pending_pkt_sizes[spwf_id].add(size);
    }

    uint32_t _get_cumulative_size(int spwf_id) {
        return _pending_pkt_sizes[spwf_id].cumulative();
    }

    uint32_t _remove_pending_pkt_size(int spwf_id, uint32_t size) {
        return _pending_pkt_sizes[spwf_id].remove(size);
    }

    uint32_t _get_pending_pkt_size(int spwf_id) {
        return _pending_pkt_sizes[spwf_id].get();
    }

   void _reset_pending_pkt_sizes(int spwf_id) {
        _pending_pkt_sizes[spwf_id].reset();
    }

   void _set_pending_data(int spwf_id) {
       _pending_sockets_bitmap |= (1 << spwf_id);
   }

   void _clear_pending_data(int spwf_id) {
        _pending_sockets_bitmap &= ~(1 << spwf_id);
    }

    bool _is_data_pending(int spwf_id) {
        return (_pending_sockets_bitmap & (1 << spwf_id)) ? true : false;
    }

    bool _is_data_pending(void) {
        if(_pending_sockets_bitmap != 0) return true;
        else return false;
    }

    void _packet_handler_bh(void) {
        /* read in other eventually pending packages */
        _read_in_pending();
    }

    /* Do not call the (external) callback in IRQ context while performing critical module operations */
    void _event_handler(void);

    void _error_handler(void);

    void _call_callback(void) {
        if((bool)_callback_func) {
            _callback_func();
        }
    }

    bool _is_event_callback_blocked(void) {
        return (_call_event_callback_blocked != 0);
    }

    void _block_event_callback(void) {
        _call_event_callback_blocked++;
    }

    void _unblock_event_callback(void) {
        MBED_ASSERT(_call_event_callback_blocked > 0);
        _call_event_callback_blocked--;
        if(_call_event_callback_blocked == 0) {
            _trigger_event_callback();
        }
    }

    /* unblock & force call of (external) callback */
    void _unblock_and_callback(void) {
        MBED_ASSERT(_call_event_callback_blocked == 1);
        _call_event_callback_blocked = 0;
        _call_callback();
    }

    /* trigger call of (external) callback in case there is still data */
    void _trigger_event_callback(void) {
        MBED_ASSERT(_call_event_callback_blocked == 0);
        /* if still data available */
        if(readable()) {
            _call_callback();
        }
    }

    char _ip_buffer[16];
    char _gateway_buffer[16];
    char _netmask_buffer[16];
    char _mac_buffer[18];

    char ssid_buf[256]; /* required to handle not 802.11 compliant ssid's */
    char *_msg_buffer;

private:
    friend class SPWFSA01;
    friend class SPWFSA04;
    friend class SpwfSAInterface;
};

#endif // SPWFSAXX_H
