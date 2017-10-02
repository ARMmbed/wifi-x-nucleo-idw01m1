/* SPWFSA01 Device
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
#include "SpwfSAInterface.h"
#include "mbed_debug.h"

#if MBED_CONF_IDW0XX1_EXPANSION_BOARD == IDW01M1

SPWFSA01::SPWFSA01(PinName tx, PinName rx,
                   PinName rts, PinName cts,
                   SpwfSAInterface &ifce, bool debug,
                   PinName wakeup, PinName reset)
: SPWFSAxx(tx, rx, rts, cts, ifce, debug, wakeup, reset) {
}

bool SPWFSA01::open(const char *type, int* spwf_id, const char* addr, int port)
{
    int socket_id;
    int value;
    BH_HANDLER;

    if(!_parser.send("AT+S.SOCKON=%s,%d,%s,ind", addr, port, type))
    {
        debug_if(_dbg_on, "\r\nSPWF> `SPWFSA01::open`: error opening socket (%d)\r\n", __LINE__);
        return false;
    }

    /* handle both response possibilities here before returning
     * otherwise module seems to remain in inconsistent state.
     */

    /* wait for first character */
    while((value = _parser.getc()) < 0);

    if(value != _cr_) { // Note: this is different to what the spec exactly says
        debug_if(_dbg_on, "\r\nSPWF> error opening socket (%d)\r\n", __LINE__);
        return false;
    }

    if(!_recv_delim_lf()) { // Note: this is different to what the spec exactly says
        debug_if(_dbg_on, "\r\nSPWF> error opening socket (%d)\r\n", __LINE__);
        return false;
    }

    value = _parser.getc();
    switch(value) {
        case ' ':
            if(_parser.recv("ID: %d%*[\x0d]", &socket_id)
                    && _recv_ok()) {
                debug_if(_dbg_on, "AT^  ID: %d\r\n", socket_id);

                *spwf_id = socket_id;
                return true;
            }
            break;
        case 'E':
            if(_parser.recv("RROR: %[^\x0d]%*[\x0d]", _err_msg_buffer) && _recv_delim_lf()) {
                debug_if(_dbg_on, "AT^ ERROR: %s (%d)\r\n", _err_msg_buffer, __LINE__);
            } else {
                debug_if(_dbg_on, "\r\nSPWF> error opening socket (%d)\r\n", __LINE__);
            }
            break;
        default:
            debug_if(_dbg_on, "\r\nSPWF> error opening socket (%d)\r\n", __LINE__);
            break;
    }

    return false;
}

int SPWFSA01::_read_in(char* buffer, int spwf_id, uint32_t amount) {
    int ret = -1;

    MBED_ASSERT(buffer != NULL);

    /* block asynchronous indications */
    if(!_winds_off()) {
        return -1;
    }

    /* read in data */
    if(_parser.send("AT+S.SOCKR=%d,%d", spwf_id, amount)) {
        /* set high timeout */
        _parser.setTimeout(SPWF_READ_BIN_TIMEOUT);
        /* read in binary data */
        int read = _parser.read(buffer, amount);
        /* reset timeout value */
        _parser.setTimeout(_timeout);
        if(read > 0) {
            if(_recv_ok()) {
                ret = amount;
            } else {
                debug_if(_dbg_on, "%s(%d): failed to receive OK\r\n", __func__, __LINE__);
            }
        } else {
            debug_if(_dbg_on, "%s(%d): failed to read binary data\r\n", __func__, __LINE__);
        }
    } else {
        debug_if(_dbg_on, "%s(%d): failed to send SOCKR\r\n", __func__, __LINE__);
    }

    /* unblock asynchronous indications */
    _winds_on();

    return ret;
}

bool SPWFSA01::_recv_ap(nsapi_wifi_ap_t *ap)
{
    bool ret;
    unsigned int channel;

    ap->security = NSAPI_SECURITY_UNKNOWN;

    /* check for end of list */
    if(_recv_delim_cr_lf()) {
        return false;
    }

    /* run to 'horizontal tab' */
    while(_parser.getc() != '\x09');


    /* read in next line */
    ret = _parser.recv(" %*s %hhx:%hhx:%hhx:%hhx:%hhx:%hhx CHAN: %u RSSI: %hhd SSID: \'%256[^\']\' CAPS:",
                       &ap->bssid[0], &ap->bssid[1], &ap->bssid[2], &ap->bssid[3], &ap->bssid[4], &ap->bssid[5],
                       &channel, &ap->rssi, ssid_buf);

    if(ret) {
        int value;

        /* copy values */
        memcpy(&ap->ssid, ssid_buf, 32);
        ap->ssid[32] = '\0';
        ap->channel = channel;

        /* skip 'CAPS' */
        for(int i = 0; i < 6; i++) { // read next six characters (" 0421 ")
            _parser.getc();
        }

        /* get next character */
        value = _parser.getc();
        if(value != 'W') { // no security
            ap->security = NSAPI_SECURITY_NONE;
            goto recv_ap_get_out;
        }

        /* determine security */
        {
            char buffer[10];

            if(!_parser.recv("%s%*[\x20]", &buffer)) {
                goto recv_ap_get_out;
            } else if(strncmp("EP", buffer, 10) == 0) {
                ap->security = NSAPI_SECURITY_WEP;
                goto recv_ap_get_out;
            } else if(strncmp("PA2", buffer, 10) == 0) {
                ap->security = NSAPI_SECURITY_WPA2;
                goto recv_ap_get_out;
            } else if(strncmp("PA", buffer, 10) != 0) {
                goto recv_ap_get_out;
            }

            /* got a "WPA", check for "WPA2" */
            value = _parser.getc();
            if(value == _cr_) { // no further protocol
                ap->security = NSAPI_SECURITY_WPA;
                goto recv_ap_get_out;
            } else { // assume "WPA2"
                ap->security = NSAPI_SECURITY_WPA_WPA2;
                goto recv_ap_get_out;
            }
        }
    } else {
        debug("%s - ERROR: Should never happen!\r\n", __func__);
    }

recv_ap_get_out:
    if(ret) {
        /* wait for next line feed */
        while(!_recv_delim_lf());
    }

    return ret;
}

int SPWFSA01::scan(WiFiAccessPoint *res, unsigned limit)
{
    unsigned cnt = 0;
    nsapi_wifi_ap_t ap;

    if (!_parser.send("AT+S.SCAN")) {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    while (_recv_ap(&ap)) {
        if (cnt < limit) {
            res[cnt] = WiFiAccessPoint(ap);
        }

        cnt++;
        if (limit != 0 && cnt >= limit) {
            break;
        }
    }

    _recv_ok();

    return cnt;
}

#endif // MBED_CONF_IDW0XX1_EXPANSION_BOARD
