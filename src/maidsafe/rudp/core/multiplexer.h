/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

// Original author: Christopher M. Kohlhoff (chris at kohlhoff dot com)

#ifndef MAIDSAFE_RUDP_CORE_MULTIPLEXER_H_
#define MAIDSAFE_RUDP_CORE_MULTIPLEXER_H_

#include <array>  // NOLINT
#include <mutex>
#include <vector>

#include "boost/asio/io_service.hpp"
#include "boost/asio/ip/udp.hpp"

#include "maidsafe/common/log.h"

#include "maidsafe/rudp/operations/dispatch_op.h"
#include "maidsafe/rudp/core/dispatcher.h"
#include "maidsafe/rudp/packets/packet.h"
#include "maidsafe/rudp/parameters.h"
#include "maidsafe/rudp/return_codes.h"

namespace maidsafe {

namespace rudp {

namespace detail {

class ConnectionManager;
class Socket;

class Multiplexer {
 public:
  explicit Multiplexer(boost::asio::io_service& asio_service);

  // Open the multiplexer.  If endpoint is valid, the new socket will be bound to it.
  ReturnCode Open(const boost::asio::ip::udp::endpoint& endpoint);

  // Whether the multiplexer is open.
  bool IsOpen() const;

  // Close the multiplexer.
  void Close();

  // Asynchronously receive a single packet and dispatch it.
  template <typename DispatchHandler>
  void AsyncDispatch(DispatchHandler handler) {
    DispatchOp<DispatchHandler> op(handler, socket_, boost::asio::buffer(receive_buffer_),
                                   sender_endpoint_, dispatcher_);
    socket_.async_receive_from(boost::asio::buffer(receive_buffer_), sender_endpoint_, 0, op);
  }

  // Called by the socket objects to send a packet. Returns kSuccess if the data was sent
  // successfully, kSendFailure otherwise.
  template <typename Packet>
  ReturnCode SendTo(const Packet& packet, const boost::asio::ip::udp::endpoint& endpoint) {
    std::array<unsigned char, Parameters::kUDPPayload> data;
    auto buffer = boost::asio::buffer(&data[0], Parameters::max_size);
    if (size_t length = packet.Encode(buffer)) {
      boost::system::error_code ec;
      socket_.send_to(boost::asio::buffer(buffer, length), endpoint, 0, ec);
      if (ec) {
#ifndef NDEBUG
        if (!local_endpoint().address().is_unspecified()) {
          LOG(kWarning) << "Error sending " << length << " bytes from " << local_endpoint()
                        << " to << " << endpoint << " - " << ec.message();
        }
#endif
        return kSendFailure;
      } else {
        return kSuccess;
      }
    }
    return kSendFailure;
  }

  boost::asio::ip::udp::endpoint local_endpoint() const;

  // Returns external_endpoint_ if valid, else best_guess_external_endpoint_.
  boost::asio::ip::udp::endpoint external_endpoint() const;

  friend class ConnectionManager;
  friend class Socket;

 private:
  // Disallow copying and assignment.
  Multiplexer(const Multiplexer&);
  Multiplexer& operator=(const Multiplexer&);

  // The UDP socket used for all RUDP protocol communication.
  boost::asio::ip::udp::socket socket_;

  // Data members used to receive information about incoming packets.
  std::vector<unsigned char> receive_buffer_;
  boost::asio::ip::udp::endpoint sender_endpoint_;

  // Dispatcher keeps track of the active sockets.
  Dispatcher dispatcher_;

  // This node's external endpoint - passed to session and set during handshaking.
  boost::asio::ip::udp::endpoint external_endpoint_;

  // This node's best guess at its external endpoint - set when bootstrapping a new transport
  // which is behind symmetric NAT, therefore no actual temporary connection is made.
  boost::asio::ip::udp::endpoint best_guess_external_endpoint_;

  // Mutex to protect access to external_endpoint_.
  mutable std::mutex mutex_;
};

}  // namespace detail

}  // namespace rudp

}  // namespace maidsafe

#endif  // MAIDSAFE_RUDP_CORE_MULTIPLEXER_H_
