# include "ListenerManager.hpp"
# include "colors.h"
# include "Server.hpp"
# include "utils.hpp"

# include <fcntl.h>
# include <iostream>
# include <netinet/in.h> // htons, sockaddr_in struct
# include <sys/socket.h> // bind, listen, setsockopt, socket
# include <unistd.h> // close

using std::cout;
using std::map;
using std::runtime_error;
using std::string;
using std::vector;

void ListenerManager::_setupAllListeners(const vector<Server>&servers)
{
	vector<Server>::const_iterator serverIt = servers.begin();
	for (; serverIt != servers.end(); ++serverIt)
	{
		vector<int>ports = serverIt->getPorts();
		vector<int>::const_iterator portIt = ports.begin();
		for (; portIt != ports.end(); ++portIt)
		{
			if (!_isPortAlreadyListening(*portIt))
				_setUpListener(*portIt);
		}
	}
}

bool ListenerManager::isListener(int fd)
{
	return _listenerMap.count(fd);
}

const map<int, int> &ListenerManager::getListenerMap() const
{
	return _listenerMap;
}

int ListenerManager::getPort(int listenerFd) const
{
	return _listenerMap.at(listenerFd);
}

void ListenerManager::_setUpListener(int port)
{
	string portString = utils::toString(port);

	// AF_INET = internet address (IPv4)
	// SOCK_STREAM = TCP connection
	// 0 = default protocol for the socket type
	int listenerFd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenerFd < 0)
		throw runtime_error("Failed to create listener socket for port " + portString + ".");

	// fcntl is used to change the behaviour of the socket.
	// F_SETFL = set file status flags
	// set socket to non-blocking mode (call returns immediately if no data in socket).
	// prevent blocking accept/read/write calls.
	if (fcntl(listenerFd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(listenerFd);
		throw runtime_error("Failed to set non-blocking mode for listener on port " + portString + ".");
	}

	// setsockopt modifies low-level networking behaviour of the socket.
	// sockets can be modified at the socket (SOL_SOCKET), TCP (IPPROTO_TCP) and IP level (IPPROTO_IP).
	// optval and optlen necessary because setsockopt is a generic API - doesn't always just handle int.
	int opt = 1; // 1 to toggle true/on
	// SO_REUSEADDR = allows a server to bind to an address/port that is in a TIME_WAIT state
	// e.g. when restarting a server quickly and attempting to bind to the same port.
	// OS will typically prevents port binding until the port is fully closed.
	if (setsockopt(listenerFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		close(listenerFd);
		throw runtime_error("Failed to set socket option (SO_REUSEADDR) for listener on port " + portString + ".");
	}

	// "socket address, internet" stores IP address, port, and family info.
	sockaddr_in listenerAddress;
	listenerAddress.sin_family = AF_INET; // IPv4
	// htons() converts port from host byte order to network byte order.
	// Host byte order can be big or little endian. Network byte order is always big endian.
	// htons() ensures that the port number is in the correct byte order for network transmission.
	listenerAddress.sin_port = htons(port);
	// INADDR_ANY means bind to all available interfaces (all local IPs on the machine).
	// (e.g., 127.0.0.1, 192.168.x.x, etc.).
	listenerAddress.sin_addr.s_addr = INADDR_ANY;

	// associates the socket with an address and port on the local machine.
	if (bind(listenerFd, (sockaddr *)&listenerAddress, sizeof(listenerAddress)) < 0)
	{
		close(listenerFd);
		throw runtime_error("Failed to bind listener socket to port " + portString + ".");
	}

	// listener socket will listen for incoming connection requests.
	// SOMAXCONN is the maximum possible queue size for pending connections.
	if (listen(listenerFd, SOMAXCONN) < 0)
	{
		close(listenerFd);
		throw runtime_error("Failed to listen to socket on port " + portString + ".");
	}

	_listenerMap[listenerFd] = port;

	cout << BLUE << "Listener with fd " << listenerFd << " set up on port " << port << "!\n" << RESET;
}

// This function checks if a port is already being listened to by any listener,
// and is necessary to prevent multiple listeners from trying to bind to the same port.
bool ListenerManager::_isPortAlreadyListening(int port) const
{
	map<int, int>::const_iterator it = _listenerMap.begin();
	for (; it != _listenerMap.end(); ++it)
	{
		if (it->second == port)
			return true;
	}
	return false;
}
