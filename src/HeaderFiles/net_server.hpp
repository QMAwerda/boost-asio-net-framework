#pragma once

#include "net_common.hpp"
#include "net_connection.hpp"
#include "net_message.hpp"
#include "net_tsqueue.hpp"
#include <boost/asio/io_context.hpp>
#include <exception>
#include <memory>

namespace olc {
namespace net {
template <typename T> class server_interface {
public:
  // 16 bit integer = 2 bytes [-32767, +32767]
  server_interface(uint16_t port)
      : m_asioAcceptor(m_asioContext, boost::asio::ip::tcp::endpoint(
                                          boost::asio::ip::tcp::v4(), port)) {}

  virtual ~server_interface() { Stop(); }

  bool Start() {
    try {
      WaitForClientConnection(); // we give work for asio, so it won't stop
                                 // before the running the context

      m_threadContext = std::thread([this]() { m_asioContext.run(); });
    } catch (std::exception &e) {
      // Something prohibited the server from listening
      std::cerr << "[SERVER] Exception: " << e.what() << "\n";
      return false;
    }

    std::cout << "[SERVER] Started!\n";
    return true;
  }

  void Stop() {
    // Request the context to close
    m_asioContext.stop();

    // Tidy up the context thread
    if (m_threadContext.joinable())
      m_threadContext
          .join(); // we will wait the stop of the context on the thread

    // Inform someone, anybody, if they care...
    std::cout << "[SERVER] Stopped!\n";
  }

  // ASYNC - Instruct asio to wait for connection
  void WaitForClientConnection() {
    m_asioAcceptor.async_accept([this](std::error_code ec,
                                       boost::asio::ip::tcp::socket socket) {
      if (!ec) {
        std::cout << "[SERVER] New Conntection: " << socket.remote_endpoint()
                  << "\n";

        std::shared_ptr<connection<T>> newconn =
            std::make_shared<connection<T>>(connection<T>::owner::server,
                                            m_asioContext, std::move(socket),
                                            m_qMessagesIn);

        // Give the user server a chance to deny connection
        if (OnClientConnect(newconn)) {
          // Connection allowed, so add to container of new connections
          m_deqConnections.push_back(std::move(newconn));

          m_deqConnections.back()->ConnectToClient(nIDCounter++);

          std::cout << "[" << m_deqConnections.back()->GetID()
                    << "] Connection Approved\n";
        } else {
          std::cout << "[-----] Connection Denied\n";
          // if the connection denied, than it will go out of scope and deleted
          // (cause smart ptr)
        }

      } else {
        // Error has occured during acceptance
        std::cout << "[SERVER] New Connection Error: " << ec.message() << "\n";
      }

      // Prime the asio context with more work - again simply wait for another
      // connection...
      WaitForClientConnection(); // re-register another async task to asio
    });
  }

  // Send a message to a specific client
  void MessageClient(std::shared_ptr<connection<T>> client,
                     const message<T> &msg) {
    if (client && client->IsConnected()) {
      client->Send(msg);
    } else {
      OnClientDisconnect(client);
      client.reset(); // When this client will be deleted?
      m_deqConnections.erase(
          std::remove(m_deqConnections.begin(), m_deqConnections.end(), client),
          m_deqConnections.end());
    }
  }

  // Send message to all clients
  // ignore a specific client
  void
  MessageAllClients(const message<T> &msg,
                    std::shared_ptr<connection<T>> pIgnoreClient = nullptr) {
    bool bInvalidClientExists = false;

    for (auto &client : m_deqConnections) {
      // Check client is connected...
      if (client && client->IsConnected()) {
        // ..it is!
        if (client != pIgnoreClient)
          client->Send(msg);
      } else {
        OnClientDisconnect(client);
        client.reset();
        bInvalidClientExists = true;
      }
    }

    if (bInvalidClientExists) {
      m_deqConnections.erase(std::remove(m_deqConnections.begin(),
                                         m_deqConnections.end(), nullptr),
                             m_deqConnections.end());
    }
  }

  void Update(size_t nMaxMessages = -1) {
    size_t nMessageCount = 0;
    while (nMessageCount < nMaxMessages && !m_qMessagesIn.empty()) {
      // Grab the front message
      auto msg = m_qMessagesIn.pop_front();
      // Pass to message handler
      OnMessage(msg.remote, msg.msg);

      nMessageCount++;
    }
  }

protected:
  // Called when a client connects, you can veto the connection by returning
  // false
  virtual bool OnClientConnect(std::shared_ptr<connection<T>> client) {
    return false;
  }

  // Called when a client appears to have disconnected
  virtual void OnClientDisconnect(std::shared_ptr<connection<T>> client) {}

  // Called when a message arrives
  // allow server to deal with message from a specific client
  virtual void OnMessage(std::shared_ptr<connection<T>> client,
                         message<T> &msg) {}

protected:
  // Thread Safe Queue for incoming message packets
  tsqueue<owned_message<T>> m_qMessagesIn;

  // Container of active validated connections
  std::deque<std::shared_ptr<connection<T>>> m_deqConnections;

  // Order of declaration is important - it is also the order if initialisation
  boost::asio::io_context
      m_asioContext; // it's shared across all of the connected clients
  std::thread m_threadContext;

  // These things need an asio context
  boost::asio::ip::tcp::acceptor m_asioAcceptor;

  // Client will be identified in the "wider system" via an ID
  uint32_t nIDCounter = 10000;
};
} // namespace net
} // namespace olc
