// Socket class - implements BaseSocket for INET domain sockets

//Please refer to http://dansguardian.org/?page=copyright2
//for the license for this code.

//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#ifndef __HPP_SOCKET
#define __HPP_SOCKET


// INCLUDES

#include "BaseSocket.hpp"

#ifdef __SSLCERT
#include "openssl/ssl.h"
#include "String.hpp" 
#endif

// DECLARATIONS

class Socket : public BaseSocket
{
	friend class FDTunnel;
public:
	// create INET socket & clear address structs
	Socket();
	// create socket using pre-existing FD (address structs will be empty!)
	Socket(int fd);
	// create socket from pre-existing FD, storing given local & remote IPs
	Socket(int newfd, struct sockaddr_in myip, struct sockaddr_in peerip);
	
	// connect to given IP & port (following default constructor)
	int connect(const std::string& ip, int port);
	
	// bind to given port
	int bind(int port);
	// bind to given IP & port, for machines with multiple NICs
	int bind(const std::string& ip, int port);

	// accept incoming connections & return new Socket
	Socket* accept();
	
	// close socket & clear address structs
	void reset();

	// get remote IP/port
	std::string getPeerIP();
	int getPeerSourcePort();
	int getPort();
	void setPort(int port);
	unsigned long int getPeerSourceAddr();
	
	// get local IP
	std::string getLocalIP();
	
#ifdef __SSLCERT
	//use this socket as an ssl server
	int startSslClient(const std::string& certPath);

	//is this a SSL connection
	bool isSsl();

	bool isSslServer();

	//shuts down the current ssl connection
	void stopSsl();

	//check that everything in this certificate is correct appart from the hostname
	long checkCertValid();
	
	//check the common name and altnames of a certificate against hostname
	int checkCertHostname(const std::string& hostame);
	
	void close();
#endif //__SSLCERT

#ifdef __SSLMITM
	//use this socket as an ssl server
	int startSslServer(X509 * x, EVP_PKEY * privKey);
	
	// non-blocking check for writable socket
	bool readyForOutput();
	// blocking check, can break on config reloads
	void readyForOutput(int timeout, bool honour_reloadconfig = false) throw(std::exception);
	
	// non-blocking check for input data
	bool checkForInput();
	
	// blocking check for data, can be told to break on signal triggered config reloads (-r)
	void checkForInput(int timeout, bool honour_reloadconfig = false) throw(std::exception);
	
	// get a line from the socket - can break on config reloads
	int getLine(char *buff, int size, int timeout, bool honour_reloadconfig = false, bool *chopped = NULL, bool *truncated = NULL) throw(std::exception);
	
	// write buffer to string - throws std::exception on error
	void writeString(const char *line) throw(std::exception);
	// write buffer to string - can be told not to do an initial readyForOutput, and told to break on -r
	bool writeToSocket(const char *buff, int len, unsigned int flags, int timeout, bool check_first = true, bool honour_reloadconfig = false);
	// read from socket, returning number of bytes read
	int readFromSocketn(char *buff, int len, unsigned int flags, int timeout);
	// read from socket, returning error status - can be told to skip initial checkForInput, and to break on -r
	int readFromSocket(char *buff, int len, unsigned int flags, int timeout, bool check_first = true, bool honour_reloadconfig = false);
	// write to socket, throwing std::exception on error - can be told to break on -r
	void writeToSockete(const char *buff, int len, unsigned int flags, int timeout, bool honour_reloadconfig = false) throw(std::exception);
#endif //__SSLMITM


private:
#ifdef __SSLCERT
	SSL * ssl;
	SSL_CTX * ctx;
	bool isssl;
	bool issslserver;
#endif//__SSLCERT
	
	// local & remote addresses
	struct sockaddr_in my_adr;
	struct sockaddr_in peer_adr;
	int my_port;
};

#endif
