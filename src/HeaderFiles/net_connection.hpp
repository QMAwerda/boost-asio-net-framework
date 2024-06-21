#pragma once

#include "net_common.hpp"
#include "net_message.hpp"
#include "net_tsqueue.hpp"
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/write.hpp>

namespace olc {
namespace net {
template <typename T>
class connection : public std::enable_shared_from_this<connection<T>> {
public:
  enum class owner { server, client };
  connection(owner parent, boost::asio::io_context &asioContext,
             boost::asio::ip::tcp::socket socket,
             tsqueue<owned_message<T>> &qIn)
      : m_asioContext(asioContext), m_socket(std::move(socket)),
        m_qMessagesIn(qIn) {
    m_nOwnerType = parent;
  }

  virtual ~connection() {}

  uint32_t GetID() const { return id; }

public:
  void ConnectToClient(uint32_t uid = 0) {
    if (m_nOwnerType == owner::server) {
      if (m_socket.is_open()) {
        id = uid;
        ReadHeader(); // async
      }
    }
  }
  void ConnectToServer(
      const boost::asio::ip::tcp::resolver::results_type &endpoints) {
    // Only clients can connect to servers
    if (m_nOwnerType == owner::client) {
      // Request asio attempts to connect to an endpoint
      boost::asio::async_connect(
          m_socket, endpoints,
          [this](std::error_code ec, boost::asio::ip::tcp::endpoint endpoint) {
            if (!ec) {
              ReadHeader();
            }
          });
    }
  }

  bool Disconnect() {
    if (IsConnected())
      boost::asio::post(m_asioContext, [this]() { m_socket.close(); });
  }
  // Check is socket ready for reading and sending data
  // ?Hey, but it's not mean that it's connected? innit?
  bool IsConnected() const { return m_socket.is_open(); }

public:
  void Send(const message<T> &msg) {
    // Can make refactor here
    boost::asio::post(m_asioContext, [this, msg]() {
      bool bWritingMessage = !m_qMessagesOut.empty();
      m_qMessagesOut.push_back(msg);
      if (!bWritingMessage) {
        WriteHeader();
      }
    }); // We don't need to add WriteHeader() to asio context (asio::post
        // function allow us to do that), if we already have messages in our
        // OutQueue, it mean, that our asio is already process WriteHeader() for
        // elements of our Queue. If we will add more WriteHeader() we will get
        // undefined behaviour, so we need to control it.
  }

private:
  // ASYNC - Prime the context ready to read a message header
  void ReadHeader() {
    boost::asio::async_read(
        m_socket,
        boost::asio::buffer(&m_msgTemporaryIn.header,
                            sizeof(message_header<T>)),
        [this](std::error_code ec, std::size_t length) {
          if (!ec) {
            if (m_msgTemporaryIn.header.size > 0) {
              // WHY DO WE NEED TO DO A RESIZE HERE?
              m_msgTemporaryIn.body.resize(m_msgTemporaryIn.header.size);
              ReadBody();
            } else {
              AddToIncomingMessageQueue();
            }
          } else {
            std::cout << "[" << id << "] Read Header Fail.\n";
            m_socket.close();
          }
        });
  }

  // ASYNC - Prime context ready to read a message body
  void ReadBody() {
    boost::asio::async_read(
        m_socket,
        boost::asio::buffer(
            m_msgTemporaryIn.body.data(),
            m_msgTemporaryIn.body.size()), // IDK IS THIS SIZE CORRECT OR NOT
        [this](std::error_code ec, std::size_t length) {
          if (!ec) {
            AddToIncomingMessageQueue();
          } else {
            std::cout << "[" << id << "] Read Body Fail.\n";
            m_socket.close();
          }
        });
  }

  // ASYNC - Prime context ready to read a message body
  void WriteHeader() {
    boost::asio::async_write(m_socket,
                             boost::asio::buffer(&m_qMessagesOut.front().header,
                                                 sizeof(message_header<T>)),
                             [this](std::error_code ec, std::size_t length) {
                               if (!ec) {
                                 // we not using message<T> size() method here.
                                 // Because if the header > 0 and body == 0 we
                                 // will get 'true'. So we use the size() method
                                 // from the vector, because we call it not for
                                 // the message obj, but for the message.body
                                 // vector
                                 if (m_qMessagesOut.front().body.size() > 0) {
                                   WriteBody();
                                 } else {
                                   m_qMessagesOut.pop_front();

                                   if (!m_qMessagesOut.empty()) {
                                     WriteHeader();
                                   }
                                 }
                               } else {
                                 std::cout << "[" << id
                                           << "] Write Header Fail.\n";
                                 m_socket.close();
                               }
                             });
  }

  // ASYNC - Prime context ready to read a message body
  void WriteBody() {
    boost::asio::async_write(
        m_socket,
        boost::asio::buffer(
            m_qMessagesOut.front().body.data(),
            m_qMessagesOut.front().body.size()), // IDK IS THE CORRECT SIZE HERE
        [this](std::error_code ec, std::size_t length) {
          if (!ec) {
            m_qMessagesOut.pop_front();

            if (!m_qMessagesOut.empty()) {
              WriteHeader(); // WHY WE SHOUL REPEAT IT AGAIN?
              // WE ALREADY WRITTEN IT in WriteHeader func, innit?
            }
          } else {
            std::cout << "[" << id << "] Write Body Fail.\n";
            m_socket.close();
          }
        });
  }

  void AddToIncomingMessageQueue() {
    if (m_nOwnerType == owner::server)
      m_qMessagesIn.push_back({this->shared_from_this(), m_msgTemporaryIn});
    else // we don't need to specify the pointer to the connection here, because
         // client has only one connection, sp this field is redundunt
      m_qMessagesIn.push_back(
          {nullptr, m_msgTemporaryIn}); // in the client interface we will store
                                        // the connection as unic pointer
    ReadHeader();
  }

protected:
  // Each connection has a unique socket to a remote
  boost::asio::ip::tcp::socket m_socket;

  // This context is shared with the whole asio instance
  boost::asio::io_context &m_asioContext;

  // This queue holds all messages to be sent to the remote side of this
  // connection
  tsqueue<message<T>> m_qMessagesOut;

  // This queue holds all messages that have been recieved from the remote side
  // of this connection. Note it is a reference as the "owner" of this
  // connection is expected to provide a queue
  tsqueue<owned_message<T>> &m_qMessagesIn;
  message<T> m_msgTemporaryIn;

  // The "owner" decides how some of the connection behaves
  owner m_nOwnerType = owner::server;
  uint32_t id = 0; // client id
};
} // namespace net
} // namespace olc