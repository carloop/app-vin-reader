/* OBDMessage implements the ISO 15765-2 transport used for OBD-II messages
 *
 * Since CAN messages can only contain 8 bytes and diagnostic messages
 * can be longer, OBD-II uses a protocol called ISO 15765-2 to transmit
 * longer messages inside CAN frames.
 *
 * Short messages (up to 6 bytes) are transmitted in a single CAN frame.
 * 
 * Longer messages are split between a first frame and consecutive
 * frames. When the listener receives a first frame it must send a flow
 * control frame before the sender will transmit the consecutive frames.
 *
 * The type of message is part of the first byte of the ISO 15765-2
 * message. The number of data bytes in the entire message follows the
 * type (either half a byte for a single-frame message or 2 bytes for a
 * multiple-frame message).
 *
 * OBDMessage reconstructs the data sent in multiple frames.
 *
 * Reference: https://en.wikipedia.org/wiki/ISO_15765-2
 *
 * Copyright 2016 1000 Tools, Inc
 *
 * Distributed under the MIT license. See LICENSE.txt for more details.
 */

#include "OBDMessage.h"
#include <algorithm>

#define OBD_REQUEST_RESPONSE_OFFSET 8

// Add the data from this CAN frame to the current OBD message
// Returns true if a flow control frame must be transmitted
bool OBDMessage::addMessageData(const CANMessage &message) {
  _id = message.id;

  bool needsFlowControl = false;
  switch(messageType(message)) {
    case SINGLE:
      clear();
      _size = message.data[0] & 0xF;
      addDataFrom(1, message.data);
      break;
    case FIRST:
      clear();
      _size = ((uint16_t)(message.data[0] & 0x0F) << 8) | message.data[1];
      addDataFrom(2, message.data);
      needsFlowControl = true;
      break;
    case CONSECUTIVE:
      addDataFrom(1, message.data);
      break;
  }

  _complete = _data.size() == _size;
  return needsFlowControl;
}

void OBDMessage::addDataFrom(uint8_t i, const uint8_t *data) {
  for(; _data.size() < _size && i < 8; i++) {
    _data.push_back(data[i]);
  }
}

OBDMessage::MESSAGE_TYPE OBDMessage::messageType(const CANMessage &message) {
  uint8_t headerByte = message.data[0];
  return (MESSAGE_TYPE)(headerByte >> 4);
}

// To reuse the same OBD message over and over, just clear it
void OBDMessage::clear() {
  _data.clear();
  _size = 0;
  _complete = false;
}

// If a flow control frame must be sent, call this method to get the
// appropriate frame to send.
CANMessage OBDMessage::flowControlMessage() {
  CANMessage msg;
  msg.id = _id - OBD_REQUEST_RESPONSE_OFFSET;
  msg.len = 8;
  msg.data[0] = FLOW << 4;
  return msg;
}
