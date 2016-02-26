//
//  ConnectionHandlerLinux.cpp
//  RemoteController
//
//  Created by Le Hoang Quyen on 26/2/16.
//  Copyright © 2016 Le Hoang Quyen. All rights reserved.
//

#include "../ConnectionHandler.h"

namespace HQRemote {
	struct SocketConnectionHandler::Impl {
	};
	
	void SocketConnectionHandler::platformConstruct() {
		m_impl = new Impl();
	}
	void SocketConnectionHandler::platformDestruct() {
		delete m_impl;
	}
	
	int SocketConnectionHandler::platformGetLastSocketErr() const {
		return errno;
	}
}
