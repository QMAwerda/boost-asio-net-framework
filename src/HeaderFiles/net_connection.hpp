#pragma once

#include "net_common.hpp"
#include "net_message.hpp"
#include "net_tsqueue.hpp"
#include <boost/asio/io_context.hpp>

namespace olc {
namespace net {
template <typename T>
class connection : public std::enable_shared_from_this<connection<T>> {
public:
  connection() {}

  virtual ~connection() {}

public:
  bool ConnectToServer();
  bool Disconnect();
  bool IsConnetcted() const;

public:
  bool Send(const message<T> &msg);

protected:
  // Each connection has a unique socket to a remote
  boost::asio::ip::tcp::socket m_socket;

  // This context is shared with the whole asio instance
  boost::asio::io_context &m_asioContext;

  // This queue holds all messages to be sent to the remote side of this
  // connection
  tsqueue<message<T>> m_qMessagesQut;

  // This queue holds all messages that have been recieved from the remote side
  // of this connection. Note it is a reference as the "owner" of this
  // connection is expected to provide a queue
  tsqueue<owned_message<T>> &m_qMessagesIn;
};
} // namespace net
} // namespace olc