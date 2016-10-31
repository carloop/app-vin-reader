/* OBDMessage implements the ISO 15765-2 transport used for OBD-II messages
 *
 * Since CAN messages can only contain 8 bytes and diagnostic messages
 * can be longer, OBD-II uses a protocol called ISO 15762-2 to transmit
 * longer messages inside CAN frames.
 *
 * Short messages (up to 6 bytes) are transmitted in a single CAN frame.
 * 
 * Longer messages are split between a first frame and consecutive
 * frames. When the listener receives a first frame it must send a flow
 * control frame before the sender will transmit the consecutive frames.
 *
 * OBDMessage reconstructs the data sent in multiple frames.
 *
 * Reference: https://en.wikipedia.org/wiki/ISO_15765-2
 *
 * Copyright 2016 1000 Tools, Inc
 *
 * Distributed under the MIT license. See LICENSE.txt for more details.
 */

#pragma once
#ifdef UNIT_TEST
#include "../tests/application_test.h"
#else
#include "application.h"
#endif
#include <vector>

class OBDMessage {
public:
  using DataT = std::vector<uint8_t>;

  // The CAN message ID of received OBD message
  uint32_t id() const {
    return _id;
  }

  // Number of bytes of data expected
  uint16_t size() const {
    return _size;
  }

  // Have all the data bytes been received
  uint32_t complete() const {
    return _complete;
  }

  // All the data bytes received
  const DataT &data() const {
    return _data;
  }

  void clear();

  bool addMessageData(const CANMessage &message);

  CANMessage flowControlMessage();
    
private:
  enum MESSAGE_TYPE {
    SINGLE,
    FIRST,
    CONSECUTIVE,
    FLOW
  };

  MESSAGE_TYPE messageType(const CANMessage &message);
  void addDataFrom(uint8_t i, const uint8_t *data);

  uint32_t _id;
  uint16_t _size;
  bool _complete;
  DataT _data;
};
