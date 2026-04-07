/*
 * SPDX-FileCopyrightText: 2017-2024 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "ConsumerIr"

#include "ConsumerIr.h"

#include <android-base/logging.h>
#include <fcntl.h>
#include <linux/lirc.h>
#include <string>

using std::vector;

namespace aidl {
namespace android {
namespace hardware {
namespace ir {

static const std::string kIrDevice = "/dev/spidev0.1";

static vector<ConsumerIrFreqRange> kRangeVec{
        {.minHz = 30000, .maxHz = 60000},
};

::ndk::ScopedAStatus ConsumerIr::getCarrierFreqs(vector<ConsumerIrFreqRange>* _aidl_return) {
    *_aidl_return = kRangeVec;

    return ::ndk::ScopedAStatus::ok();
}

#define MAX_TX_BUFFER 0x200000

unsigned char default_tx[MAX_TX_BUFFER];

::ndk::ScopedAStatus ConsumerIr::transmit(int32_t carrierFreqHz, const vector<int32_t>& pattern) {
    size_t entries = pattern.size();

    if (entries == 0) {
        return ::ndk::ScopedAStatus::ok();
    }

    int fd = open(kIrDevice.c_str(), O_RDWR);
    if (fd < 0) {
        LOG(ERROR) << "Failed to open " << kIrDevice << ", error " << fd;

        return ::ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    memset(default_tx, 0, MAX_TX_BUFFER);

    // Calculate pattern to send using spi
    // NOTE: The following code was a result of decompilation from original consumer ir library and processing by gemini.
    for (unsigned int i = 0; i < entries; i++) {
        // Calculate scaling based on pulse duration
        int pulse_duration = pattern[i];
        int scaled_len = (pulse_duration * 38) / 1000; // 0x26 = 38

        if ((i % 2) == 0) {
            // PULSE CASE (Even indices): Generate the modulated carrier wave
            // This loop fills the buffer with bit-patterns that simulate 38kHz oscillation
            int bit_count = scaled_len * 26; // 0x1a = 26
            int state = 0;

            if (bit_count > 8) {
                int remaining_bits = bit_count + 8;
                while (remaining_bits > 16) {
                    unsigned char wave_byte;
                    // This switch constructs a square wave bitmask
                    switch(state) {
                        case 0:  wave_byte = 0x00; state = 1;  break;
                        case 1:  wave_byte = 0x0F; state = 2;  break;
                        case 2:  wave_byte = 0xFF; state = 3;  break;
                        case 3:  wave_byte = 0x80; state = 4;  break;
                        case 4:  wave_byte = 0x07; state = 5;  break;
                        case 5:  wave_byte = 0xFF; state = 6;  break;
                        case 6:  wave_byte = 0xC0; state = 7;  break;
                        case 7:  wave_byte = 0x03; state = 8;  break;
                        case 8:  wave_byte = 0xFF; state = 9;  break;
                        case 9:  wave_byte = 0xE0; state = 10; break;
                        case 10: wave_byte = 0x01; state = 11; break;
                        case 11: wave_byte = 0xFF; state = 12; break;
                        case 12: wave_byte = 0xF0; state = 13; break;
                        case 13: wave_byte = 0x00; state = 14; break;
                        case 14: wave_byte = 0xFF; state = 15; break;
                        case 15: wave_byte = 0xF8; state = 16; break;
                        case 16: wave_byte = 0x00; state = 17; break;
                        case 17: wave_byte = 0x7F; state = 18; break;
                        case 18: wave_byte = 0xFC; state = 19; break;
                        case 19: wave_byte = 0x00; state = 20; break;
                        case 20: wave_byte = 0x3F; state = 21; break;
                        case 21: wave_byte = 0xFE; state = 22; break;
                        case 22: wave_byte = 0x00; state = 23; break;
                        case 23: wave_byte = 0x1F; state = 24; break;
                        case 24: wave_byte = 0xFF; state = 0;  break;
                        default: wave_byte = 0x00; break;
                    }
                    default_tx[bytes_to_send++] = wave_byte;
                    remaining_bits -= 8;
                }
            }
        } 
        else {
            // SPACE CASE (Odd indices): Silent period
            if (pulse_duration * 38 > 999) {
                int space_bytes = (scaled_len * 25); // Scaling for silence
                int clear_len = (space_bytes >> 3) + 1;
                
                // Set silence (all zeros) in the SPI buffer
                memset(&default_tx[bytes_to_send], 0, clear_len);
                bytes_to_send += clear_len;
            }
        }
    }

    write(g_spidev_fd, default_tx, bytes_to_send); // and now, send a pattern... using spi. Yes, a fcking spi.

    close(fd);

    return ::ndk::ScopedAStatus::ok();
}

}  // namespace ir
}  // namespace hardware
}  // namespace android
}  // namespace aidl
