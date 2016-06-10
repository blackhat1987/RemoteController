#ifndef HQREMOTE_CONNECTION_HANDLER_H
#define HQREMOTE_CONNECTION_HANDLER_H

#ifdef WIN32
#	include <Winsock2.h>
#else
#	include <sys/types.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <arpa/inet.h>
#endif

#include "Common.h"
#include "CString.h"
#include "Data.h"
#include "Event.h"
#include "Timer.h"

#include <string>
#include <map>
#include <set>
#include <list>
#include <vector>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <memory>
#include <condition_variable>

#if defined WIN32 || defined _MSC_VER
#	pragma warning(push)
#	pragma warning(disable:4251)
#endif

namespace HQRemote {
#ifdef WIN32
	typedef SOCKET socket_t;
#else
	typedef int socket_t;
#endif

	struct HQREMOTE_API ConnectionEndpoint {
		ConnectionEndpoint(const char* addr, int _port)
		: address(addr), port(_port)
		{}
		
		CString address;
		int port;
	};

	//interface
	class HQREMOTE_API IConnectionHandler {
	public:
		class Delegate {
		public:
			virtual void onConnected() = 0;
		};

		virtual ~IConnectionHandler();

		bool start();
		void stop();
		bool running() const;

		double timeSinceStart() const;
		virtual bool connected() const = 0;

		//return obtained data to user
		DataRef receiveData(bool &isReliable);
		DataRef receiveDataBlock(bool &isReliable);//this function will block until there is some data available

		void sendData(ConstDataRef data);
		void sendDataUnreliable(ConstDataRef data);
		void sendData(const void* data, size_t size);
		void sendDataUnreliable(const void* data, size_t size);
		
		float getReceiveRate() const {
			return m_recvRate.load(std::memory_order_relaxed);
		}

		std::shared_ptr<const CString> getInternalErrorMsg() const
		{
			return m_internalError;
		}

		void registerDelegate(Delegate* delegate);
		void unregisterDelegate(Delegate* delegate);
		
	protected:
		IConnectionHandler();
		
		virtual bool startImpl() = 0;
		virtual void stopImpl() = 0;
		
		//these functions may send only a portion of the data, number of bytes send must be returned. Return negative value on error.
		virtual _ssize_t sendRawDataImpl(const void* data, size_t size) = 0;
		virtual void flushRawDataImpl() = 0;
		virtual _ssize_t sendRawDataUnreliableImpl(const void* data, size_t size) = 0;
		
		struct MsgChunk;
		
		struct MsgBuf {
			DataRef data;
			uint32_t filledSize;
		};
		
		
		//this should be called when data is received from reliable channel
		void onReceiveReliableData(const void* data, size_t size);
		//this should be called when data is received from unreliable channel
		void onReceivedUnreliableDataFragment(const void* data, size_t size);
		//this should be called when endpoints connected successfully
		void onConnected();
		
		void setInternalError(const char* msg);
		
		std::atomic<bool> m_running;
		
	private:
		struct ReceivedData {
			ReceivedData(const DataRef& _data, bool reliable)
				:data(_data), isReliable(reliable)
			{}

			DataRef data;
			bool isReliable;
		};

		void sendRawDataAtomic(const void* data, size_t size);
		void fillReliableBuffer(const void* &data, size_t& size);
		void invalidateUnusedReliableData();
		
		void pushDataToQueue(DataRef data, bool reliable, bool discardIfFull);
		
		std::shared_ptr<CString> m_internalError;
		
		int m_reliableBufferState;
		MsgBuf m_reliableBuffer;
		std::map<uint64_t, MsgBuf> m_unreliableBuffers;
		std::list<ReceivedData> m_dataQueue;
		std::mutex m_dataLock;
		std::condition_variable m_dataCv;
		
		time_checkpoint_t m_lastRecvTime;
		size_t m_numLastestDataReceived;
		std::atomic<float> m_recvRate;

		std::set<Delegate*> m_delegates;
		
		time_checkpoint_t m_startTime;
	};

	//socket based handler
	class HQREMOTE_API SocketConnectionHandler : public IConnectionHandler {
	public:
		static const int RANDOM_PORT;//use this to specify random port for sockets

		~SocketConnectionHandler();

		virtual bool connected() const override;
		
		static int platformSetSocketBlockingMode(socket_t socket, bool blocking);
		static int platformGetLastSocketErr();
	private:

		void platformConstruct();
		void platformDestruct();
		
		virtual bool startImpl() override;
		virtual void stopImpl() override;
		
		
		virtual _ssize_t sendRawDataImpl(const void* data, size_t size) override;
		virtual void flushRawDataImpl() override;
		virtual _ssize_t sendRawDataUnreliableImpl(const void* data, size_t size) override;

		_ssize_t recvDataUnreliableNoLock(socket_t socket);

		_ssize_t pingUnreliableNoLock(time_checkpoint_t sendTime);
		
		void recvProc();
		
	protected:
		SocketConnectionHandler();
		
		struct UnreliablePingInfo {
			uint64_t sendTime;
			double rtt;
		};

		virtual bool socketInitImpl() = 0;
		virtual void initConnectionImpl() = 0;
		virtual void addtionalRcvThreadCleanupImpl() = 0;
		virtual void addtionalSocketCleanupImpl() = 0;

		//optional
		virtual void addtionalRcvThreadHandlerImpl() {}
		virtual _ssize_t handleUnwantedDataFromImpl(const sockaddr_in& srcAddr, const void* data, size_t size) { return size; }

		_ssize_t sendRawDataUnreliableNoLock(socket_t socket, const sockaddr_in* pDstAddr, const void* data, size_t size);//connectionless socket only
		_ssize_t sendRawDataNoLock(socket_t socket, const void* data, size_t size);//connection oriented socket only
		
		_ssize_t sendChunkUnreliableNoLock(socket_t socket, const sockaddr_in* pDstAddr, const MsgChunk& chunk, size_t size);//connectionless only socket
		_ssize_t recvChunkUnreliableNoLock(socket_t socket, MsgChunk& chunk, sockaddr_in& srcAddr);//connectionless only socket
		_ssize_t recvRawDataNoLock(socket_t socket);
		
		//check if we're able to connect to the remote endpoint on an unreliable channel
		bool testUnreliableRemoteEndpointNoLock();
		
		std::mutex m_socketLock;
		//receiving thread
		std::unique_ptr<std::thread> m_recvThread;

		std::atomic<socket_t> m_connSocket;
		std::atomic<socket_t> m_connLessSocket;//connection less socket
		std::unique_ptr<sockaddr_in> m_connLessSocketDestAddr;//destination endpoint of connectionless socket
		
		UnreliablePingInfo m_lastConnLessPing;

		bool m_enableReconnect;
		
		//platform dependent
		struct Impl;
		Impl * m_impl;
	};


	//connection-less socket based handler
	class HQREMOTE_API BaseUnreliableSocketHandler : public SocketConnectionHandler {
	public:
		BaseUnreliableSocketHandler(int connLessListeningPort);
		~BaseUnreliableSocketHandler();

	protected:
		virtual bool socketInitImpl() override;
		virtual void initConnectionImpl() override;
		virtual void addtionalRcvThreadCleanupImpl() override;
		virtual void addtionalSocketCleanupImpl() override;

		static socket_t createUnreliableSocket(int port, bool reuseAddr = true);

		int m_connLessPort;
	};

	//socket based server handler
	class HQREMOTE_API SocketServerHandler : public BaseUnreliableSocketHandler {
	public:
		//pass <connLessListeningPort> = 0 if you don't want to use unreliable socket
		SocketServerHandler(int listeningPort, int connLessListeningPort);
		~SocketServerHandler();

		virtual bool connected() const override;
	private:
		virtual bool socketInitImpl() override;
		virtual void initConnectionImpl() override;
		virtual void addtionalRcvThreadCleanupImpl() override;
		virtual void addtionalSocketCleanupImpl() override;

		virtual void addtionalRcvThreadHandlerImpl() override;

		void pollingMulticastData();

		int m_port;
		std::atomic<socket_t> m_serverSocket;

		std::atomic<socket_t> m_multicastSocket;//multicast socket
	};
	
	//socket based ureliable client handler
	class HQREMOTE_API UnreliableSocketClientHandler : public BaseUnreliableSocketHandler {
	public:
		UnreliableSocketClientHandler(int connLessListeningPort, const ConnectionEndpoint& connLessRemoteEndpoint);
		~UnreliableSocketClientHandler();
		
		virtual bool connected() const override;
	protected:
		virtual void initConnectionImpl() override;
		virtual void addtionalRcvThreadCleanupImpl() override;
		virtual void addtionalSocketCleanupImpl() override;
		
		ConnectionEndpoint m_connLessRemoteEndpoint;
		
		std::atomic<bool> m_connLessRemoteReachable;
	};
	
	//socket based client handler
	class HQREMOTE_API SocketClientHandler : public UnreliableSocketClientHandler {
	public:
		//pass <remoteConnLessEndpoint.port> = 0 if you don't want to use unreliable socket
		SocketClientHandler(int listeningPort, int connLessListeningPort, const ConnectionEndpoint& remoteEndpoint, const ConnectionEndpoint& remoteConnLessEndpoint);
		~SocketClientHandler();
		
		virtual bool connected() const override;
	private:
		virtual void initConnectionImpl() override;
		virtual void addtionalRcvThreadCleanupImpl() override;
		virtual void addtionalSocketCleanupImpl() override;
		
		ConnectionEndpoint m_remoteEndpoint;//reliable endpoint
	};

	//socket based server discovery client
	class HQREMOTE_API SocketServerDiscoverClientHandler : public BaseUnreliableSocketHandler {
	public:
		class DiscoveryDelegate {
		public:
			virtual void onNewServerDiscovered(SocketServerDiscoverClientHandler* handler, uint64_t request_id, const char* addr, int reliablePort, int unreliablePort);
		};

		SocketServerDiscoverClientHandler(DiscoveryDelegate* delegate);
		~SocketServerDiscoverClientHandler();

		void findOtherServers(uint64_t request_id);

		void setDiscoveryDelegate(DiscoveryDelegate* delegate);
	private:
		virtual _ssize_t handleUnwantedDataFromImpl(const sockaddr_in& srcAddr, const void* data, size_t size) override;

		std::mutex m_discoveryDelegateLock;
		DiscoveryDelegate* m_discoveryDelegate;
	};
}

#if defined WIN32 || defined _MSC_VER
#	pragma warning(pop)
#endif

#endif