#ifndef DPLITE_MESSAGES_HPP
#define DPLITE_MESSAGES_HPP

#define DPLITE_MSGID_HOST_ENUM_REQUEST 1

/* EnumHosts() request message.
 *
 * GUID        - Application GUID, NULL to search for any
 * DATA | NULL - User data
 * DWORD       - Current tick count, to be returned, for latency measurement
*/

#define DPLITE_MSGID_HOST_ENUM_RESPONSE 2

/* EnumHosts() response message.
 *
 * DWORD       - DPN_APPLICATION_DESC.dwFlags
 * GUID        - DPN_APPLICATION_DESC.guidInstance
 * GUID        - DPN_APPLICATION_DESC.guidApplication
 * DWORD       - DPN_APPLICATION_DESC.dwMaxPlayers
 * DWORD       - DPN_APPLICATION_DESC.dwCurrentPlayers
 * WSTRING     - DPN_APPLICATION_DESC.pwszSessionName
 * DATA | NULL - DPN_APPLICATION_DESC.pvApplicationReservedData
 *
 * DATA | NULL - DPN_MSGID_ENUM_HOSTS_RESPONSE.pvResponseData
 * DWORD       - Tick count from DPLITE_MSGID_HOST_ENUM_REQUEST
*/

#define DPLITE_MSGID_CONNECT_HOST 3

/* Initial Connect() request to host.
 *
 * GUID | NULL    - Instance GUID
 * GUID           - Application GUID
 * WSTRING | NULL - Password
 * DATA | NULL    - Request data
 * WSTRING - Player name (empty = none)
 * DATA    - Player data (empty = none)
*/

#define DPLITE_MSGID_CONNECT_HOST_OK 4

/* Successful response to DPLITE_MSGID_CONNECT_HOST from host.
 *
 * GUID        - Instance GUID
 * DWORD       - Player ID of current host
 * DWORD       - Player ID assigned to receiving client
 * DWORD       - Number of other peers (total - 2)
 *
 * For each peer:
 *   DWORD - Player ID
 *   DWORD - IPv4 address (network byte order)
 *   DWORD - Port (host byte order)
 *
 * DATA | NULL - Response data
 * WSTRING - Host player name (empty = none)
 * DATA    - Host player data (empty = none)
 * DWORD   - DPN_APPLICATION_DESC.dwMaxPlayers
 * WSTRING - DPN_APPLICATION_DESC.pwszSessionName
 * WSTRING - DPN_APPLICATION_DESC.pwszPassword
 * DATA    - DPN_APPLICATION_DESC.pvApplicationReservedData
*/

#define DPLITE_MSGID_CONNECT_HOST_FAIL 5

/* Negative response to DPLITE_MSGID_CONNECT_HOST from host.
 * Host will close the connection after sending this.
 *
 * DWORD       - Error code (DPNERR_HOSTREJECTEDCONNECTION, DPNERR_INVALIDAPPLICATION, etc)
 * DATA | NULL - Response data
*/

#define DPLITE_MSGID_MESSAGE 6

/* Message sent via SendTo() by application.
 *
 * DWORD - Player ID of sender
 * DATA  - Message payload
 * DWORD - Flags (DPNSEND_GUARANTEED, DPNSEND_COALESCE, DPNSEND_COMPLETEONPROCESS)
*/

#define DPLITE_MSGID_PLAYERINFO 7

/* Player info has been updated by the peer using the SetPeerInfo() method.
 *
 * DWORD   - Player ID, will always be that of the sending peer.
 * WSTRING - Player name (empty = none)
 * DATA    - Player data (empty = none)
 * DWORD   - Operation ID to return in DPLITE_MSGID_OP_COMPLETE
*/

#define DPLITE_MSGID_ACK 8

/* The peer has completed processing a message.
 *
 * DWORD - Ack ID, should be unique to the peer, for a time.
 * DWORD - Result code (normally S_OK)
*/

#define DPLITE_MSGID_APPDESC 9

/* The host has modified the session's application description using SetApplicationDesc()
 *
 * DWORD   - DPN_APPLICATION_DESC.dwMaxPlayers
 * WSTRING - DPN_APPLICATION_DESC.pwszSessionName
 * WSTRING - DPN_APPLICATION_DESC.pwszPassword
 * DATA    - DPN_APPLICATION_DESC.pvApplicationReservedData
*/

#define DPLITE_MSGID_CONNECT_PEER 10

/* Initial connect request to a follow non-host peer.
 *
 * GUID    - Instance GUID
 * GUID    - Application GUID
 * WSTRING - Password
 * DWORD   - Player ID
 * WSTRING - Player name (empty = none)
 * DATA    - Player data (empty = none)
*/

#define DPLITE_MSGID_CONNECT_PEER_OK 11

/* Successful response to DPLITE_MSGID_CONNECT_PEER.
 *
 * WSTRING - Player name (empty = none)
 * DATA    - Player data (empty = none)
*/

#define DPLITE_MSGID_CONNECT_PEER_FAIL 12

/* Negative response to DPLITE_MSGID_CONNECT_PEER.
 * Peer will close the connection after sending this.
 *
 * DWORD - Error code (DPNERR_HOSTREJECTEDCONNECTION, DPNERR_INVALIDAPPLICATION, etc)
*/

#define DPLITE_MSGID_DESTROY_PEER 13

/* Peer has been destroyed by the host calling IDirectPlay8Peer::DestroyPeer()
 *
 * Sent from host to peer being terminated, to all other peers and from the terminated peer to each
 * peer when it closes the connection.
 *
 * The victim peer must remove itself from the session after receiving this message. All other
 * peers will eject it when they receive notifcation from the server.
 *
 * DWORD - Player ID of peer being destroyed
 * DATA  - DPNMSG_TERMINATE_SESSION.pvTerminateData (only from host to victim)
*/

#endif /* !DPLITE_MESSAGES_HPP */
