#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    if( _arp_table.count(next_hop_ip) == 1){
		send_datagram_with_ip(dgram, next_hop_ip);
	}else{
		if(_waiting_ip.count(next_hop_ip) == 1){
			_waiting_data[next_hop_ip].push(dgram);
			return;
		} 
		//send arp_request
		EthernetFrame arp_request;
		arp_request.header() = { ETHERNET_BROADCAST, _ethernet_address, EthernetHeader::TYPE_ARP};

		ARPMessage arp_request_payload;
		arp_request_payload.opcode = ARPMessage::OPCODE_REQUEST;
		arp_request_payload.sender_ethernet_address = _ethernet_address;
		arp_request_payload.sender_ip_address = _ip_address.ipv4_numeric();
		arp_request_payload.target_ethernet_address = {}; //置空!
		arp_request_payload.target_ip_address = next_hop_ip;

		arp_request.payload() = arp_request_payload.serialize();
		_frames_out.push(arp_request);

		_waiting_ip[next_hop_ip] = 0;
		_waiting_data[next_hop_ip].push(dgram);
	}
}

void NetworkInterface::send_datagram_with_ip(const InternetDatagram &dgram, const uint32_t &next_hop_ip){
	EthernetFrame send_frame;
	send_frame.header() = { _arp_table[next_hop_ip].first, _ethernet_address, EthernetHeader::TYPE_IPv4};
	send_frame.payload() = dgram.serialize();
	_frames_out.push(send_frame);
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if(frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST) return nullopt;
	if(frame.header().type == EthernetHeader::TYPE_IPv4){
			InternetDatagram datagram;
			if(datagram.parse(frame.payload()) != ParseResult::NoError) return nullopt;
			return datagram;
	}
	//arp_packet
	ARPMessage arpmessage;
	if(arpmessage.parse(frame.payload()) != ParseResult::NoError) return nullopt;

	if(arpmessage.opcode == ARPMessage::OPCODE_REQUEST) {
		//arp_request
		if(arpmessage.target_ip_address != _ip_address.ipv4_numeric()) return nullopt; //not my arp_request;
		//my arp_request
		EthernetAddress &target_ethernet_address = arpmessage.sender_ethernet_address;
		uint32_t &target_ip_address = arpmessage.sender_ip_address;

		_arp_table[target_ip_address] = {target_ethernet_address, 0};

		//construct arp_reply
		EthernetFrame arp_reply;
		arp_reply.header() = { target_ethernet_address, _ethernet_address, EthernetHeader::TYPE_ARP};

		ARPMessage arp_reply_payload;
		arp_reply_payload.opcode = ARPMessage::OPCODE_REPLY;
		arp_reply_payload.sender_ethernet_address = _ethernet_address;
		arp_reply_payload.sender_ip_address = _ip_address.ipv4_numeric();
		arp_reply_payload.target_ethernet_address = target_ethernet_address;
		arp_reply_payload.target_ip_address = target_ip_address;

		arp_reply.payload() = arp_reply_payload.serialize();
		_frames_out.push(arp_reply);
	}else{
		//arp_reply
		EthernetAddress &target_ethernet_address = arpmessage.sender_ethernet_address;
		uint32_t &target_ip_address = arpmessage.sender_ip_address;

		_arp_table[target_ip_address] = { target_ethernet_address, 0 };

		while(_waiting_data[target_ip_address].size()){
			send_datagram_with_ip(_waiting_data[target_ip_address].front() ,target_ip_address);
			_waiting_data[target_ip_address].pop();
		}
		_waiting_ip.erase(target_ip_address);		
	}
	return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 

	for(auto it = _arp_table.begin(); it != _arp_table.end(); ){
		it->second.second += ms_since_last_tick;
		if( (it->second.second) > _arp_entry_ttl ) _arp_table.erase(it++);
		else it++; 
	}

	for(auto it = _waiting_ip.begin(); it != _waiting_ip.end(); it++){
		it->second += ms_since_last_tick;
		if( (it->second) > _arp_response_ttl ){
			//resend arp_request
			it->second = 0;
			EthernetFrame arp_request;
			arp_request.header() = { ETHERNET_BROADCAST, _ethernet_address, EthernetHeader::TYPE_ARP};
			
			ARPMessage arp_request_payload;
			arp_request_payload.opcode = ARPMessage::OPCODE_REQUEST;
			arp_request_payload.sender_ethernet_address = _ethernet_address;
			arp_request_payload.sender_ip_address = _ip_address.ipv4_numeric();
			arp_request_payload.target_ethernet_address = {}; 
			arp_request_payload.target_ip_address = it->first;

			arp_request.payload() = arp_request_payload.serialize();
			_frames_out.push(arp_request);
		}
	}

}
