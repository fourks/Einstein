// ==============================
// File:			TUsermodeNetwork.cp
// Project:			Einstein
//
// Copyright 2010 by Matthias Melcher (mm@matthiasm.com).
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
// ==============================
// $Id$
// ==============================

/*
 USERMODE NETWORK HANDLER
 
 The user mode Network handler works by emulating an ethernet card on a 
 pretty low level for the Newton. To use the network, the NIE2.0 and the 
 Einstein NE2000 driver must be installed on the Newton. By inserting
 the NE2000 card, the driver will be activated and all network calls
 will be forwarded to the Network Handler that was loaded at startup.
 
 All packats from the Newton are at the lowest network level. It is up to
 the handler to simulate or forward packets to the host or host network.
 The Newton expects to receive packets on the same low level, so we must
 generate the network header, IPv4 header, and TCP or UDP header ourselves.
 
 CURRENT STATUS
 
 At the current stage the driver is loaded correctly and all call forwarding 
 works. We can send and receive packets. 
 
 - the initial ARP packet is faked
 - there is currently only one DNS request that is 
   faked (http://borg.org , for no particular reason).
 - TCP connections are created and can send and receive (the code is a mess)
 
 NEXT ACTION
 
 Next we need to implement handlers for all types of packages that we want
 to support. Forwarding packages that the Newton sends should be relatively
 easy. Generating the correct reply is a bit more complex. A working TCP 
 protocol will allow web browsing and possibly reading mail.
 
 The goal is to set up a connection to NCX, Newtsync or Escale running locally. 
 
  - TCP disconnect action
  - TCP cleanup
  - DNS request
  - UDP protocol
  - ARP protocol
  - DHCP protocol
  - socket handling in threads
  - testing: 
     * SimpleMail
     * Courier
     * Newt's Cape 
     * NewtFTP
     * nBlog
     * Raissa
     * NPDS HTTP Server
     * NPDS Tracker Client
     * Dock TCP/IP (w/NCX)
     * IC/VC
     * LPR Driver
 
 done:
  - handler hierarchy
  - packet class
  - protocol class
  - TCP connect
  - TCP send
  - TCP receive

 */

#include <K/Defines/KDefinitions.h>
#include "TUsermodeNetwork.h"
#include "Emulator/Log/TLog.h"

//
// Handle all kinds of network packages
//

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <netinet/in.h>
#include <unistd.h>



/**
 * This class is used to build and interprete TCP/IP packages.
 */
class Packet
{
public:
	/** Used in GetTCPFlags() */
	static const KUInt16 TCPFlagURG = 0x0020;
	static const KUInt16 TCPFlagACK = 0x0010;
	static const KUInt16 TCPFlagPSH = 0x0008;
	static const KUInt16 TCPFlagRST = 0x0004;
	static const KUInt16 TCPFlagSYN = 0x0002;
	static const KUInt16 TCPFlagFIN = 0x0001;
	
	/** Used in GetType(). */
	static const KUInt16 NetTypeIP = 0x0800;
	
	/** Used in GetIPProtocol() */
	static const KUInt8 IPProtocolTCP = 6;
	
	/** 
	 * Create a new packet.
	 * \param data pointer to some place in memory that contains the packet data
	 * \param size number of bytes in this packet
	 * \param copy if set, the packet data will be copied, otherwise this class
	 *        will only point at the buffer
	 */
	Packet(KUInt8 *data, KUInt32 size, bool copy=1) {
		if (copy) {
			mData = (KUInt8*)malloc(size);
			if (data) {
				memcpy(mData, data, size);
			} else {
				memset(mData, 0, size);
			}
			mCopy = 1;
		} else {
			mData = data;
			mCopy = 0;
		}
		mSize = size;
	}
	
	/**
	 * Remove the packet.
	 * If the data was copied, we remove the copy as well.
	 */
	~Packet() {
		if (mData && mCopy) 
			free(mData);
	}
	
	/**
	 * Get a pointer to the raw packet data.
	 * \return pointer to the data
	 */
	KUInt8 *Data() { return mData; }
	
	/**
	 * Get the number of bytes in the buffer.
	 * \return number of bytes.
	 */
	KUInt32 Size() { return mSize; }

	/**
	 * Read a six-byte word in network order.
	 * This is used to quickly handle MAC adresses.
	 * \return a six-byte sequence in the buffer as a 64 bit number.
	 */
	KUInt64 Get48(KUInt32 i) { return ((KUInt64(mData[i]))<<40)|((KUInt64(mData[i+1]))<<32)|(mData[i+2]<<24)|(mData[i+3]<<16)|(mData[i+4]<<8)|(mData[i+5]); }
	void Set48(KUInt32 i, KUInt64 v) { 
		mData[i+0] = v>>40;
		mData[i+1] = v>>32;
		mData[i+2] = v>>24;
		mData[i+3] = v>>16;
		mData[i+4] = v>>8;
		mData[i+5] = v; }
	/** Get a 32-bit word in network order. */
	KUInt32 Get32(KUInt32 i) { return (mData[i]<<24)|(mData[i+1]<<16)|(mData[i+2]<<8)|(mData[i+3]); }
	void Set32(KUInt32 i, KUInt32 v) { 
		mData[i+0] = v>>24;
		mData[i+1] = v>>16;
		mData[i+2] = v>>8;
		mData[i+3] = v; }
	/** Get a 16-bit word in network order. */
	KUInt16 Get16(KUInt32 i) { return (mData[i]<<8)|(mData[i+1]); }
	void Set16(KUInt32 i, KUInt16 v) { 
		mData[i+0] = v>>8;
		mData[i+1] = v; }
	
	/**
	 * All the functions below get and set members of raw TCP packets including network header.
	 * Details can be found all over the web.
	 */
	KUInt64 GetDstMAC()			{ return Get48(0); }
	KUInt64 GetSrcMAC()			{ return Get48(6); }
	KUInt16 GetType()			{ return Get16(12); }
	
	KUInt8	GetIPVersion()		{ return mData[14]>>4; }
	KUInt8	GetIPHeaderLength() { return (mData[14]&0x0f)*4; } // in bytes!
	KUInt8	GetIPTOS()			{ return mData[15]; }
	KUInt16 GetIPTotalLength()	{ return Get16(16); }
	KUInt16 GetIPID()			{ return Get16(18); }
	KUInt8	GetIPFlags()		{ return mData[20]>>5; }
	KUInt16 GetIPFrag()			{ return Get16(20)&0x1fff; }
	KUInt8	GetIPTTL()			{ return mData[22]; }
	KUInt8	GetIPProtocol()		{ return mData[23]; }
	KUInt16 GetIPChecksum()		{ return Get16(24); }
	KUInt32	GetIPSrcIP()		{ return Get32(26); }
	KUInt32	GetIPDstIP()		{ return Get32(30); }
	
	KUInt16 GetTCPSrcPort()		{ return Get16(34); }
	KUInt16 GetTCPDstPort()		{ return Get16(36); }
	KUInt32	GetTCPSeq()			{ return Get32(38); }
	KUInt32	GetTCPAck()			{ return Get32(42); }
	KUInt8	GetTCPHeaderLength(){ return (mData[46]>>4)<<2; } // in bytes!
	KUInt16 GetTCPFlags()		{ return Get16(46)&0x0fff; }
	KUInt16 GetTCPWindow()		{ return Get16(48); }
	KUInt16 GetTCPChecksum()	{ return Get16(50); }
	KUInt16 GetTCPUrgent()		{ return Get16(52); }
	KUInt8 *GetTCPPayloadStart(){ return mData + (GetTCPHeaderLength() + 34); }
	KUInt32	GetTCPPayloadSize()	{ return mSize - GetTCPHeaderLength() - 34; }
	
	
	void	SetDstMAC(KUInt64 v)		{ Set48(0, v); }
	void	SetSrcMAC(KUInt64 v)		{ Set48(6, v); }
	void	SetType(KUInt16 v)			{ Set16(12, v); }
	
	void	SetIPVersion(KUInt8 v)		{ mData[14] = (mData[14]&0x0f) | (v<<4); }
	void	SetIPHeaderLength(KUInt8 v) { mData[14] = (mData[14]&0xf0) | ((v/4)&0x0f); } // in bytes!
	void	SetIPTOS(KUInt8 v)			{ mData[15] = v; }
	void	SetIPTotalLength(KUInt16 v)	{ Set16(16, v); }
	void	SetIPID(KUInt16 v)			{ Set16(18, v); }
	void	SetIPFlags(KUInt8 v)		{ mData[20] = (mData[20]&0x1f) | (v<<5); }
	void	SetIPFrag(KUInt16 v)		{ mData[20] = (mData[20]&0xe0) | ((v>>8)&0x1f); mData[21] = v; }
	void	SetIPTTL(KUInt8 v)			{ mData[22] = v; }
	void	SetIPProtocol(KUInt8 v)		{ mData[23] = v; }
	void	SetIPChecksum(KUInt16 v)	{ Set16(24, v); }
	void	SetIPSrcIP(KUInt32 v)		{ Set32(26, v); }
	void	SetIPDstIP(KUInt32 v)		{ Set32(30, v); }
	
	void	SetTCPSrcPort(KUInt16 v)	{ Set16(34, v); }
	void	SetTCPDstPort(KUInt16 v)	{ Set16(36, v); }
	void	SetTCPSeq(KUInt32 v)		{ Set32(38, v); }
	void	SetTCPAck(KUInt32 v)		{ Set32(42, v); }
	void	SetTCPHeaderLength(KUInt8 v){ mData[46] = (mData[46]&0x0f) | ((v>>2)<<4); } // in bytes!
	void	SetTCPFlags(KUInt16 v)		{ mData[46] = (mData[46]&0xf0) | ((v>>8)&0x0f); mData[47] = v; }
	void	SetTCPWindow(KUInt16 v)		{ Set16(48, v); }
	void	SetTCPChecksum(KUInt16 v)	{ Set16(50, v); }
	void	SetTCPUrgent(KUInt16 v)		{ Set16(52, v); }
	void	SetTCPPayload(KUInt8 *, KUInt32);
	
	Packet *prev, *next;
	
private:
	KUInt8 *mData;
	KUInt32 mSize;
	KUInt8	mCopy;
};



/**
 * This is a generic handler for network packets.
 *
 * To handle new types of packats, a new class should be derived.
 */
class PacketHandler 
{
public:
	
	/**
	 * Create a new packet handler.
	 *
	 * \param h link handler to this network interface
	 */
	PacketHandler(TUsermodeNetwork *h) :
		net(h)
	{
	}
	
	/**
	 * Remove a packet handler, freeing all resources.
	 */
	virtual ~PacketHandler() {
	}
	
	/**
	 * Send a Newton packet to the outside world.
	 * \param p send this packet
	 * \return 1 if the packet was handled ok and it should not be sent by any other handler
	 * \return 0 if we don't know how to handle the packet and another handler should have a go
	 * \return -1 if an error occured and the packet should not be sent by any other handler
	 */
	virtual int send(Packet &p) = 0;
	
	/**
	 * Every 10th of a second or so handle outstanding tasks.
	 */
	virtual void timer() { }
	
	/**
	 * Find out if this packet can be handled.
	 * If it can not be handled, the packet will be offered to the next handler.
	 * If we can handle it, we will either handle it right here, or instantiate 
	 * a new handler, link it into the list of handlers, and then call send().
	 * \param p the Newton packet that could be sent
	 * \param n the network interface
	 * \return 0 if we can not handle this packet
	 * \return 1 if we can handled it and it need not to be propagated any further
	 */
	static int canHandle(Packet &p, TUsermodeNetwork *n) { return 0; }
	
	PacketHandler *prev, *next;
	TUsermodeNetwork *net;
};



/**
 * This class handles TCP packets.
 *
 * TCP is a connection that provides a nicely formed stream of data using all
 * kinds of handshake and tricks. 
 * 
 * In the emulation, we have a 100% connection
 * between Einstein and the Newton, so we only emulate a perfectly working
 * connection. The outside communication is done by the host, so no need for
 * any trickery here.
 */
class TCPPacketHandler : public PacketHandler 
{
public:
	static const KUInt16 StateDisconnected = 0;
	static const KUInt16 StateConnected = 1;
	static const KUInt16 StateLocalDisconnect = 2;
	static const KUInt16 StateRemoteDisconnect = 3;
	
	/**
	 * Create a TCP packet handler.
	 * A TCP connection ca be uniquely identified by the destinatin MAC, IP, and 
	 * port number. Let's remember these now so we can later identify the packets
	 * that we need to handle.
	 */
	TCPPacketHandler(TUsermodeNetwork *h, Packet &packet) :
		PacketHandler(h),
		mSocket(-1)
	{
		state = StateDisconnected;
		
		myMAC = packet.GetSrcMAC();
		myIP = packet.GetIPSrcIP();
		myPort = packet.GetTCPSrcPort();
		mySeqNr = packet.GetTCPSeq();

		theirMAC = packet.GetDstMAC();
		theirIP = packet.GetIPDstIP();
		theirPort = packet.GetTCPDstPort();
		theirSeqNr = 0;
		theirID = 1000;
	}
	
	/**
	 * Delete this handler.
	 */
	~TCPPacketHandler() {
	}
	
	/**
	 * Create a genereic TCP packet.
	 * This is a working TCP packet for this particular connection. Space is 
	 * allocated for the payload. The payload must be copied into this 
	 * packet an the cehcksums must be updated.
	 * \param size this is the desired size of the payload.
	 * \see UpdateChecksums(Packet *p)
	 * \see Packet::SetTCPPayload(KUInt8 *, KUInt32)
	 */
	Packet *NewPacket(KUInt32 size) {
		Packet *p = new Packet(0L, size+54);
		p->SetDstMAC(myMAC);
		p->SetSrcMAC(theirMAC);
		p->SetType(Packet::NetTypeIP);
		
		p->SetIPVersion(4);
		p->SetIPHeaderLength(20);
		p->SetIPTOS(0);
		p->SetIPTotalLength(size+54-14);	// not counting the network header
		p->SetIPID(theirID); theirID++;
		p->SetIPFlags(0);
		p->SetIPFrag(0);
		p->SetIPTTL(64);
		p->SetIPProtocol(Packet::IPProtocolTCP);
		p->SetIPSrcIP(theirIP);
		p->SetIPDstIP(myIP);
		
		p->SetTCPSrcPort(theirPort);
		p->SetTCPDstPort(myPort);
		p->SetTCPSeq(theirSeqNr);
		p->SetTCPAck(mySeqNr);
		p->SetTCPHeaderLength(20);
		p->SetTCPFlags(Packet::TCPFlagACK);
		p->SetTCPWindow(0x1000);		
		
		return p;
	}
	
	void UpdateChecksums(Packet *p) {
		net->SetIPv4Checksum(p->Data(), p->Size());
		net->SetTCPChecksum(p->Data(), p->Size());
	}
	
	/**
	 * Send a Newton packet to the outside world.
	 * \param packet send this packet
	 * \return 1 if the packet was handled ok
	 * \return 0 if we don't know how to handle the packet
	 * \return -1 if an error occured and no other handler should handle this packet
	 */
	virtual int send(Packet &packet) {
		printf("---> TCP send\n");
		// FIXME: verify that mac, port, and IP are the same for src and dst!
		switch (state) {
			case StateDisconnected: {
				// The class was just created. The first package that we expect 
				// is the SYN package that opens the socket and connects to the 
				// remote system.
				// FIXME: we assume that the original package is correct
				printf("-----> Initiate socket connection\n");
				
				mSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (mSocket==-1) 
					return -1;
				struct sockaddr_in sa;
				memset(&sa, 0, sizeof(sa));
				sa.sin_family = AF_INET;
				sa.sin_port = htons(theirPort);
				sa.sin_addr.s_addr = htonl(theirIP);
				int err = connect(mSocket, (struct sockaddr*)&sa, sizeof(sa));
				if (err==-1)
					return -1;
				int fl = fcntl(mSocket, F_GETFL);
				err = fcntl(mSocket, F_SETFL, fl|O_NONBLOCK);
				if (err==-1)
					return -1;
				
				//net->LogBuffer(packet.Data(), packet.Size());
				//net->LogPacket(packet.Data(), packet.Size());
				Packet *reply = NewPacket(4); // 4 bytes are reserved for the extended TCP header
				mySeqNr++; 
				reply->SetIPFlags(2); // don't fragment
				reply->SetTCPAck(mySeqNr);
				reply->SetTCPFlags(Packet::TCPFlagACK|Packet::TCPFlagSYN);
				reply->SetTCPHeaderLength(24);
				reply->Set32(54, 0x02040578); // set Maximum Segment Size
				UpdateChecksums(reply);
				//net->LogBuffer(reply->Data(), reply->Size());
				//net->LogPacket(reply->Data(), reply->Size());
				net->Enqueue(reply);
				state = StateConnected;
				printf("-----> DONE: Initiate socket connection\n");
				return 1; }
			case StateConnected:
				// The socket is connected. Traffic can come from either side
				printf("-----> Send a package\n");
				if (packet.GetTCPPayloadSize()==0) { 
					// this is an acknowledge package
					theirSeqNr = packet.GetTCPAck();
					printf("       Not replying to an ACK package\n");
					net->LogBuffer(packet.Data(), packet.Size());
					net->LogPacket(packet.Data(), packet.Size());
					theirSeqNr = packet.GetTCPAck();
				} else { // this is a data package, ack needed
					printf("       This goes out to the world\n");
					write(mSocket, packet.GetTCPPayloadStart(), packet.GetTCPPayloadSize());					
					net->LogBuffer(packet.Data(), packet.Size());
					net->LogPacket(packet.Data(), packet.Size());
					Packet *reply = NewPacket(0);
					mySeqNr += packet.GetTCPPayloadSize(); 
					reply->SetTCPAck(mySeqNr);
					reply->SetTCPFlags(Packet::TCPFlagACK);
					UpdateChecksums(reply);
					net->LogBuffer(reply->Data(), reply->Size());
					net->LogPacket(reply->Data(), reply->Size());
					net->Enqueue(reply);
					// TODO: now enqueue the reply from the remote client
				}
				printf("-----> DONE: Send a package\n");
				return 1;
				break;
			case StateLocalDisconnect:
				// Newton has close the connection
				break;
			case StateRemoteDisconnect:
				// Remote client has close the connection
				break;
		}
		return 0;
	}
	
	/**
	 * Handle all reoccuring events.
	 * We currently use polling to read from the sockets. This is easy on the
	 * host and does not delay package delivery too much. In a future version
	 * it would be better to used threads to read from the sockets and 
	 * interrupts to send data to the Newton.
	 */
	virtual void timer() {
		if (mSocket!=-1) {
			for(;;) {
				KUInt8 buf[1];
				int err = recv(mSocket, buf, 1, MSG_PEEK);
				// TODO: if this returns 0, the peer has closed the connection and we must initiate the 3-way FIN handshake
				printf("--: (%d)\n", err);
				if (err==0) {
					// FIXME: we are simply throwing out a message here. Much more needs to be done.
					Packet *reply = NewPacket(0);
					reply->SetTCPFlags(Packet::TCPFlagFIN|Packet::TCPFlagACK);
					UpdateChecksums(reply);
					net->Enqueue(reply);
					close(mSocket);
					net->RemovePacketHandler(this);
					delete this;
					return;
					// send FIN to Newt
					// wait for ACK
					// wait for FIN
					// send ACK
					// remove self from list
				}
				int avail;
				ioctl(mSocket, FIONREAD, &avail);
				if (avail>0) {
					printf("----------> %d bytes available\n", avail);
					//if (avail>200) avail = 200;
					Packet *reply = NewPacket(avail);
					ssize_t n = read(mSocket, reply->GetTCPPayloadStart(), avail);
					reply->SetTCPFlags(Packet::TCPFlagACK|Packet::TCPFlagPSH);
					UpdateChecksums(reply);
					net->LogBuffer(reply->Data(), reply->Size());
					net->LogPacket(reply->Data(), reply->Size());
					net->Enqueue(reply);
				} else {
					break;
				}
			}
		}
	}
	
	/**
	 * Can we handle the given package?
	 * This function is static and can be called before this class is instantiated.
	 * \param packet the Newton packet that could be sent
	 * \param n the network interface
	 * \return 0 if we can not handle this packet
	 * \return 1 if we can handled it and it need not to be propagated any further
	 * \return -1 if an error occurred an no other handler should handle this packet
	 */
	static int canHandle(Packet &packet, TUsermodeNetwork *net) {
		if ( packet.GetType() != Packet::NetTypeIP ) 
			return 0;
		if ( packet.GetIPProtocol() != Packet::IPProtocolTCP ) 
			return 0;
		if ( packet.GetTCPFlags() != Packet::TCPFlagSYN ) 
			return 0; // only SYN is set
		printf("---> TCP can handle this, link ourselve into the handler list\n");
		PacketHandler *ph = new TCPPacketHandler(net, packet);
		net->AddPacketHandler(ph);
		return ph->send(packet);
	}
	
	KUInt64		myMAC, theirMAC;
	KUInt32		myIP, theirIP;
	KUInt16		myPort, theirPort;
	KUInt32		mySeqNr, theirSeqNr;
	KUInt16		theirID;
	KUInt16		state;
	int			mSocket;
};




/**
 * Create an interface betweenthe Newton network driver and Einstein.
 */
TUsermodeNetwork::TUsermodeNetwork(TLog* inLog) :
	TNetworkManager( inLog ),
	mFirstPacketHandler( 0L ),
	mFirstPacket( 0L ),
	mLastPacket( 0L )
{
	// create the package pipe for the Newton package receiver
	// create the active handler list
	//AddPacketHandler(new ARPPacketHandler());
}



/**
 * Free all resources.
 */
TUsermodeNetwork::~TUsermodeNetwork()
{
	// release the package pipe
	while (mFirstPacket)
		DropPacket();
	// release all package handlers
	// TODO: delete handlers
	// release all other resources
}


/**
 * This function handles packet that are sent by the Newton to the outside 
 * world. In user mode, these packages are emulated for information that is 
 * already available on the host, or forwarded to a host socket. A possible
 * reply must be interpreted and a raw package must be generated for the Newton.
 */
int TUsermodeNetwork::SendPacket(KUInt8 *data, KUInt32 size)
{
	int err = 0;
	Packet packet(data, size, 0); // convert data into a packet
	
	// offer this package to all active handlers
	PacketHandler *ph = mFirstPacketHandler;
	while (ph) {
		switch( ph->send(packet) ) {				
			case -1:	return -1;	// an error occured, packet must not be handled
			case 1:		return 0;	// packet was handled successfuly, leave
			case 0:		break;		// another handler should deal with this packet
		}
		ph = ph->next;
	}
	
	// now offer the package to possible new handlers
	switch( TCPPacketHandler::canHandle(packet, this) ) {				
		case -1:	return -1;	// an error occured, packet must not be handled
		case 1:		return 0;	// packet was handled successfuly, leave
		case 0:		break;		// another handler should deal with this packet
	}
	
	// FIXME: simulate the ARP request for now
	// see: man 3 getaddrinfo()
	static KUInt8 b1[] = {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x58, 0xb0, 0x35, 0x77, 0xd7, 0x23, 0x08, 0x06, 0x00, 0x01,
		0x08, 0x00, 0x06, 0x04, 0x00, 0x01, 0x58, 0xb0, 0x35, 0x77, 0xd7, 0x23, 0xc0, 0xa8, 0x00, 0xc6,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xa8, 0x00, 0x01,
	};
	static KUInt8 r1[] = {
		0x58, 0xb0, 0x35, 0x77, 0xd7, 0x23, 0x00, 0x24, 0x36, 0xa2, 0xa7, 0xe4, 0x08, 0x06, 0x00, 0x01,
		0x08, 0x00, 0x06, 0x04, 0x00, 0x02, 0x00, 0x24, 0x36, 0xa2, 0xa7, 0xe4, 0xc0, 0xa8, 0x00, 0x01, 
		0x58, 0xb0, 0x35, 0x77, 0xd7, 0x23, 0xc0, 0xa8, 0x00, 0xc6,
	};
	if (size==sizeof(b1) && memcmp(b1, data, size)==0) {
		printf("---> Send Packet: faking ARP request\n");
		Enqueue(new Packet(r1, sizeof(r1)));
		return 0;
	}
	
	// FIXME: simulate the DNS request
	static KUInt8 b2[] = {
		0x00, 0x24, 0x36, 0xa2, 0xa7, 0xe4, 0x58, 0xb0, 0x35, 0x77, 0xd7, 0x23, 0x08, 0x00, 0x45, 0x00,
		0x00, 0x36, 0x00, 0x00, 0x00, 0x00, 0x40, 0x11, 0xf8, 0x9f, 0xc0, 0xa8, 0x00, 0xc6, 0xc0, 0xa8,
		0x00, 0x01, 0x08, 0x00, 0x00, 0x35, 0x00, 0x22, 0xc3, 0x0e, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x62, 0x6f, 0x72, 0x67, 0x03, 0x6f, 0x72, 0x67, 0x00,
		0x00, 0x01, 0x00, 0x01
	};
	static KUInt8 r2[] = {
		0x58, 0xb0, 0x35, 0x77, 0xd7, 0x23, 0x00, 0x24, 0x36, 0xa2, 0xa7, 0xe4, 0x08, 0x00, 0x45, 0x00, 
		0x00, 0x46, 0x05, 0x97, 0x00, 0x00, 0x40, 0x11, 0xf2, 0xf6, 0xc0, 0xa8, 0x00, 0x01, 0xc0, 0xa8, 
		0x00, 0xc8, 0x00, 0x35, 0xcd, 0x0c, 0x00, 0x32, 0xaa, 0xe8, 0x00, 0x01, 0x81, 0x80, 0x00, 0x01, 
		0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x04, 0x62, 0x6f, 0x72, 0x67, 0x03, 0x6f, 0x72, 0x67, 0x00, 
		0x00, 0x01, 0x00, 0x01, 0xc0, 0x0c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x04, 0x7e, 0x00, 0x04, 
		0x40, 0x69, 0xcd, 0x7b,                   
	};
	if (size==sizeof(b2) && memcmp(b2, data, /*size*/14)==0) {
		printf("---> Send Packet: faking DNS request\n");
		r2[34] = b2[36]; r2[35] = b2[37];
		r2[36] = b2[34]; r2[37] = b2[35]; // swap ports
		r2[33] = 198;
		SetIPv4Checksum(r2, sizeof(r2));
		SetUDPChecksum(r2, sizeof(r2));
		LogPacket(b2, sizeof(b2));
		LogPacket(r2, sizeof(r2));
		Enqueue(new Packet(r2, sizeof(r2)));
		return 0;
	}
	
	
	// if we can't interprete the package, we offer some debugging information
	printf("---> Send Packet: I can't handle this packet:\n");
	if (mLog) {
		mLog->LogLine("\nTUsermodeNetwork: Newton is sending an unsupported package:");
		LogBuffer(data, size);
		LogPacket(data, size);
	}
	return err;
}


/**
 * We forward the timer of the Newton driver to all packet handlers.
 */
int TUsermodeNetwork::TimerExpired()
{
	PacketHandler *ph = mFirstPacketHandler;
	while (ph) {
		PacketHandler *pNext = ph->next;
		ph->timer();
		ph = pNext;
	}
	return 0;
}


/**
 * Return the MAC address of the card that we emulate.
 */
int TUsermodeNetwork::GetDeviceAddress(KUInt8 *data, KUInt32 size)
{
	// TODO: of course we need the true MAC of this ethernet card
	// see: ioctl ? getifaddrs ? http://othermark.livejournal.com/3005.html
	static KUInt8 gLocalMAC[]   = { 0x58, 0xb0, 0x35, 0x77, 0xd7, 0x23 };
	assert(size==6);
	memcpy(data, gLocalMAC, 6);
	return 0;
}


/**
 * Return the number of bytes available for the Newton driver.
 */
KUInt32 TUsermodeNetwork::DataAvailable()
{	
	if (mLastPacket) {
		return mLastPacket->Size();
	} else {
		return 0;
	}
}


/**
 * Fill the provided buffer with a raw packet.
 * NE2000 buffer sizes:
 * - 1514 Max Tx  (1500 + hdr)
 * - 60   Min Tx  (46 + hdr)
 * - 1518 Max Rx  ( + crc )
 * - 64   Min Rx  ( + crc )
 */
int TUsermodeNetwork::ReceiveData(KUInt8 *data, KUInt32 size)
{
	
	Packet *pkt = mLastPacket;
	if (pkt) {
		assert(pkt->Size()==size);
		// copy the data over
		memcpy(data, pkt->Data(), size);
		// remove this package from the pipe
		DropPacket();
		return 0;
	} else {
		return -1;
	}
}


/**
 * Add a new handler for a package type to the list.
 */
void TUsermodeNetwork::AddPacketHandler(PacketHandler *inPacketHandler) 
{
	PacketHandler *n = mFirstPacketHandler;
	if (n) 
		n->prev = inPacketHandler;
	else
		mLastPacketHandler = inPacketHandler;
	inPacketHandler->next = n;
	inPacketHandler->prev = 0;
	mFirstPacketHandler = inPacketHandler;
}


/**
 * Remove a packet handler from the list.
 */
void TUsermodeNetwork::RemovePacketHandler(PacketHandler *ph) 
{
	PacketHandler *pp = ph->prev;
	PacketHandler *pn = ph->next;
	if (pp) pp->next = pn; else mFirstPacketHandler = pn;
	if (pn) pn->prev = pp; else mLastPacketHandler = pp;
	delete ph;
}


/**
 * Add a new packet to the beginning of the pipe.
 * This makes the give block ready to be sent at the next possible occasion.
 *
 * \param inPacket the package that will be queued
 */
void TUsermodeNetwork::Enqueue(Packet *inPacket)
{
	Packet *n = mFirstPacket;
	if (n) 
		n->prev = inPacket;
	else
		mLastPacket = inPacket;
	inPacket->next = n;
	inPacket->prev = 0;
	mFirstPacket = inPacket;
}


/**
 * Drops the last packet in the pipe.
 */
void TUsermodeNetwork::DropPacket()
{
	Packet *pkt = mLastPacket;
	if (pkt) {
		Packet *prevPkt = pkt->prev;
		if (prevPkt) 
			prevPkt->next = 0L;
		else
			mFirstPacket = 0L;
		mLastPacket = prevPkt;
		delete pkt;
	}
}


// ================================================================== //
// We are experiencing system trouble -- do not adjust your terminal. //
// ================================================================== //
