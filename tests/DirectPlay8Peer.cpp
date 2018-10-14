#include <winsock2.h>
#include <array>
#include <functional>
#include <gtest/gtest.h>
#include <list>
#include <mutex>
#include <stdexcept>
#include <stdint.h>

#include "../src/DirectPlay8Address.hpp"
#include "../src/DirectPlay8Peer.hpp"

// #define INSTANTIATE_FROM_COM

#define PORT 42895

static const GUID APP_GUID_1 = { 0xa6133957, 0x6f42, 0x46ce, { 0xa9, 0x88, 0x22, 0xf7, 0x79, 0x47, 0x08, 0x16 } };
static const GUID APP_GUID_2 = { 0x5917faae, 0x7ab0, 0x42d2, { 0xae, 0x13, 0x9c, 0x54, 0x1b, 0x7f, 0xb5, 0xab } };

static HRESULT CALLBACK callback_shim(PVOID pvUserContext, DWORD dwMessageType, PVOID pMessage)
{
	std::function<HRESULT(DWORD,PVOID)> *callback = (std::function<HRESULT(DWORD,PVOID)>*)(pvUserContext);
	return (*callback)(dwMessageType, pMessage);
}

class IDP8PeerInstance
{
	public:
		IDirectPlay8Peer *instance;
		
		IDP8PeerInstance()
		{
			#ifdef INSTANTIATE_FROM_COM
			CoInitialize(NULL);
			CoCreateInstance(CLSID_DirectPlay8Peer, NULL, CLSCTX_INPROC_SERVER, IID_IDirectPlay8Peer, (void**)(&instance));
			#else
			instance = new DirectPlay8Peer(NULL);
			#endif
		}
		
		~IDP8PeerInstance()
		{
			#ifdef INSTANTIATE_FROM_COM
			instance->Release();
			CoUninitialize();
			#else
			instance->Release();
			#endif
		}
		
		IDirectPlay8Peer &operator*()
		{
			return *instance;
		}
		
		IDirectPlay8Peer *operator->()
		{
			return instance;
		}
};

class IDP8AddressInstance
{
	public:
		IDirectPlay8Address *instance;
		
		IDP8AddressInstance()
		{
			#ifdef INSTANTIATE_FROM_COM
			CoInitialize(NULL);
			CoCreateInstance(CLSID_DirectPlay8Address, NULL, CLSCTX_INPROC_SERVER, IID_IDirectPlay8Address, (void**)(&instance));
			#else
			instance = new DirectPlay8Address(NULL);
			#endif
		}
		
		IDP8AddressInstance(const wchar_t *hostname, DWORD port): IDP8AddressInstance()
		{
			if(instance->SetSP(&CLSID_DP8SP_TCPIP) != S_OK
				|| instance->AddComponent(DPNA_KEY_HOSTNAME, hostname, ((wcslen(hostname) + 1) * sizeof(wchar_t)), DPNA_DATATYPE_STRING) != S_OK
				|| instance->AddComponent(DPNA_KEY_PORT, &port, sizeof(DWORD), DPNA_DATATYPE_DWORD) != S_OK)
			{
				throw std::runtime_error("Address setup failed");
			}
		}
		
		IDP8AddressInstance(GUID service_provider, const wchar_t *hostname, DWORD port): IDP8AddressInstance()
		{
			if(instance->SetSP(&service_provider) != S_OK
				|| instance->AddComponent(DPNA_KEY_HOSTNAME, hostname, ((wcslen(hostname) + 1) * sizeof(wchar_t)), DPNA_DATATYPE_STRING) != S_OK
				|| instance->AddComponent(DPNA_KEY_PORT, &port, sizeof(DWORD), DPNA_DATATYPE_DWORD) != S_OK)
			{
				throw std::runtime_error("Address setup failed");
			}
		}
		
		IDP8AddressInstance(GUID service_provider, DWORD port): IDP8AddressInstance()
		{
			if(instance->SetSP(&service_provider) != S_OK
				|| instance->AddComponent(DPNA_KEY_PORT, &port, sizeof(DWORD), DPNA_DATATYPE_DWORD) != S_OK)
			{
				throw std::runtime_error("Address setup failed");
			}
		}
		
		~IDP8AddressInstance()
		{
			#ifdef INSTANTIATE_FROM_COM
			instance->Release();
			CoUninitialize();
			#else
			instance->Release();
			#endif
		}
		
		IDirectPlay8Address &operator*()
		{
			return *instance;
		}
		
		IDirectPlay8Address *operator->()
		{
			return instance;
		}
		
		operator IDirectPlay8Address*() const
		{
			return instance;
		}
};

/* Wrapper around a DirectPlay8Peer which hosts a session. */
struct SessionHost
{
	std::function<HRESULT(DWORD,PVOID)> cb;
	IDP8PeerInstance dp8p;
	
	SessionHost(
		GUID application_guid,
		const wchar_t *session_description,
		std::function<HRESULT(DWORD,PVOID)> cb =
			[](DWORD dwMessageType, PVOID pMessage)
			{
				return DPN_OK;
			}):
		cb(cb)
	{
		if(dp8p->Initialize(&(this->cb), &callback_shim, 0) != S_OK)
		{
			throw std::runtime_error("DirectPlay8Peer::Initialize failed");
		}
		
		{
			DPN_APPLICATION_DESC app_desc;
			memset(&app_desc, 0, sizeof(app_desc));
			
			app_desc.dwSize = sizeof(app_desc);
			app_desc.guidApplication = application_guid;
			app_desc.pwszSessionName = (wchar_t*)(session_description);
			
			IDP8AddressInstance address;
			address->SetSP(&CLSID_DP8SP_TCPIP);
			
			IDirectPlay8Address *addresses[] = { address };
			
			if(dp8p->Host(&app_desc, addresses, 1, NULL, NULL, (void*)(0xB00), 0) != S_OK)
			{
				throw std::runtime_error("DirectPlay8Peer::Host failed");
			}
		}
	}
	
	SessionHost(
		GUID application_guid,
		const wchar_t *session_description,
		DWORD port,
		std::function<HRESULT(DWORD,PVOID)> cb =
			[](DWORD dwMessageType, PVOID pMessage)
			{
				return DPN_OK;
			},
		DWORD max_players = 0,
		const wchar_t *password = NULL,
		const void *appdata = NULL,
		size_t appdata_size = 0):
		cb(cb)
	{
		if(dp8p->Initialize(&(this->cb), &callback_shim, 0) != S_OK)
		{
			throw std::runtime_error("DirectPlay8Peer::Initialize failed");
		}
		
		{
			DPN_APPLICATION_DESC app_desc;
			memset(&app_desc, 0, sizeof(app_desc));
			
			app_desc.dwSize = sizeof(app_desc);
			app_desc.guidApplication = application_guid;
			app_desc.dwMaxPlayers    = max_players;
			app_desc.pwszSessionName = (wchar_t*)(session_description);
			
			if(password != NULL)
			{
				app_desc.pwszPassword = (wchar_t*)(password);
				app_desc.dwFlags |= DPNSESSION_REQUIREPASSWORD;
			}
			
			app_desc.pvApplicationReservedData     = (void*)(appdata);
			app_desc.dwApplicationReservedDataSize = appdata_size;
			
			IDP8AddressInstance addr;
			
			if(addr->SetSP(&CLSID_DP8SP_TCPIP) != S_OK
				|| addr->AddComponent(DPNA_KEY_PORT, &port, sizeof(DWORD), DPNA_DATATYPE_DWORD) != S_OK)
			{
				throw std::runtime_error("Address setup failed");
			}
			
			if(dp8p->Host(&app_desc, &(addr.instance), 1, NULL, NULL, (void*)(0xB00), 0) != S_OK)
			{
				throw std::runtime_error("DirectPlay8Peer::Host failed");
			}
		}
	}
	
	IDirectPlay8Peer *operator->()
	{
		return dp8p.instance;
	}
};

class TestPeer
{
	public:
		DPNID first_cp_dpnidPlayer;
		DPNID first_cc_dpnidLocal;
		
	private:
		const char *ident;
		
		bool expecting;
		std::list< std::function<HRESULT(DWORD,PVOID)> > callbacks;
		std::mutex lock;
		
		IDirectPlay8Peer *instance;
		
		static HRESULT CALLBACK callback(PVOID pvUserContext, DWORD dwMessageType, PVOID pMessage)
		{
			TestPeer *t = (TestPeer*)(pvUserContext);
			std::unique_lock<std::mutex> l(t->lock);
			
			if(t->expecting)
			{
				if(!t->callbacks.empty())
				{
					auto callback = t->callbacks.front();
					t->callbacks.pop_front();
					
					l.unlock();
					
					return callback(dwMessageType, pMessage);
				}
				else{
					ADD_FAILURE() << "[" << t->ident << "] Unexpected message with type " << dwMessageType;
					return DPN_OK;
				}
			}
			
			if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
			{
				DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
				
				if(t->first_cp_dpnidPlayer == -1)
				{
					t->first_cp_dpnidPlayer = cp->dpnidPlayer;
				}
				
				/* Invert the player ID to serve as a default context pointer. */
				cp->pvPlayerContext = (void*)~(uintptr_t)(cp->dpnidPlayer);
			}
			else if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
			{
				DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
				
				if(t->first_cc_dpnidLocal == -1)
				{
					t->first_cc_dpnidLocal = cc->dpnidLocal;
				}
			}
			
			return DPN_OK;
		}
		
	public:
		TestPeer(const char *ident):
			first_cp_dpnidPlayer(-1),
			first_cc_dpnidLocal(-1),
			ident(ident),
			expecting(false)
		{
			#ifdef INSTANTIATE_FROM_COM
			CoInitialize(NULL);
			CoCreateInstance(CLSID_DirectPlay8Peer, NULL, CLSCTX_INPROC_SERVER, IID_IDirectPlay8Peer, (void**)(&instance));
			#else
			instance = new DirectPlay8Peer(NULL);
			#endif
			
			if(instance->Initialize(this, &callback, 0) != S_OK)
			{
				throw std::runtime_error("IDirectPlay8Peer->Initialize() failed");
			}
		}
		
		~TestPeer()
		{
			#ifdef INSTANTIATE_FROM_COM
			instance->Release();
			CoUninitialize();
			#else
			instance->Release();
			#endif
		}
		
		IDirectPlay8Peer &operator*()
		{
			return *instance;
		}
		
		IDirectPlay8Peer *operator->()
		{
			return instance;
		}
		
		void expect_begin()
		{
			std::unique_lock<std::mutex> l(lock);
			expecting  = true;
		}
		
		void expect_push(const std::function<HRESULT(DWORD,PVOID)> &callback, int count = 1)
		{
			std::unique_lock<std::mutex> l(lock);
			
			for(int i = 0; i < count; ++i)
			{
				callbacks.push_back(callback);
			}
		}
		
		void expect_end()
		{
			std::unique_lock<std::mutex> l(lock);
			expecting = false;
			
			if(!callbacks.empty())
			{
				ADD_FAILURE() << "[" << ident << "] " << callbacks.size() << " missed messages";
			}
			
			callbacks.clear();
		}
};

struct FoundSession
{
	GUID application_guid;
	std::wstring session_description;
	
	FoundSession(GUID application_guid, const std::wstring &session_description):
		application_guid(application_guid),
		session_description(session_description) {}
	
	bool operator==(const FoundSession &rhs) const
	{
		return application_guid == rhs.application_guid
			&& session_description == rhs.session_description;
	}
};

struct CompareGUID {
	bool operator()(const GUID &a, const GUID &b) const
	{
		return memcmp(&a, &b, sizeof(GUID)) < 0;
	}
};

static void EXPECT_SESSIONS(std::map<GUID, FoundSession, CompareGUID> got, const FoundSession *expect_begin, const FoundSession *expect_end)
{
	std::list<FoundSession> expect(expect_begin, expect_end);
	
	for(auto gi = got.begin(); gi != got.end();)
	{
		for(auto ei = expect.begin(); ei != expect.end();)
		{
			if(gi->second == *ei)
			{
				ei = expect.erase(ei);
				gi = got.erase(gi);
				goto NEXT_GI;
			}
			else{
				++ei;
			}
		}
		
		++gi;
		NEXT_GI:
		{}
	}
	
	for(auto gi = got.begin(); gi != got.end(); ++gi)
	{
		wchar_t application_guid_s[128];
		StringFromGUID2(gi->second.application_guid, application_guid_s, 128);
		
		ADD_FAILURE() << "Extra session:" << std::endl
			<< "  application_guid    = " << application_guid_s << std::endl
			<< "  session_description = " << gi->second.session_description;
	}
	
	for(auto ei = expect.begin(); ei != expect.end(); ++ei)
	{
		wchar_t application_guid_s[128];
		StringFromGUID2(ei->application_guid, application_guid_s, 128);
		
		ADD_FAILURE() << "Missing session:" << std::endl
			<< "  application_guid    = " << application_guid_s << std::endl
			<< "  session_description = " << ei->session_description;
	}
	
	if(got.empty() && expect.empty())
	{
		SUCCEED();
	}
}

static void EXPECT_PEERINFO(IDirectPlay8Peer *instance, DPNID player_id, const wchar_t *name, const void *data, size_t data_size, DWORD flags)
{
	DWORD buf_size = 0;
	
	ASSERT_EQ(instance->GetPeerInfo(player_id, NULL, &buf_size, 0), DPNERR_BUFFERTOOSMALL);
	EXPECT_EQ(buf_size, sizeof(DPN_PLAYER_INFO) + ((wcslen(name) + 1) * sizeof(wchar_t)) + data_size);
	
	std::vector<unsigned char> buffer(buf_size);
	DPN_PLAYER_INFO *info = (DPN_PLAYER_INFO*)(buffer.data());
	
	info->dwSize = sizeof(DPN_PLAYER_INFO);
	
	ASSERT_EQ(instance->GetPeerInfo(player_id, info, &buf_size, 0), S_OK);
	
	EXPECT_EQ(info->dwSize,        sizeof(DPN_PLAYER_INFO));
	EXPECT_EQ(info->dwInfoFlags,   (DPNINFO_NAME | DPNINFO_DATA));
	EXPECT_EQ(info->dwPlayerFlags, (flags));
	
	EXPECT_EQ(std::wstring(info->pwszName), std::wstring(name));
	
	EXPECT_EQ(std::string((const char*)(info->pvData), info->dwDataSize),
		std::string((const char*)(data), data_size));
}

TEST(DirectPlay8Peer, EnumHostsSync)
{
	SessionHost a1s1(APP_GUID_1, L"Application 1 Session 1");
	SessionHost a1s2(APP_GUID_1, L"Application 1 Session 2");
	SessionHost a2s1(APP_GUID_2, L"Application 2 Session 1");
	
	std::map<GUID, FoundSession, CompareGUID> sessions;
	
	bool got_async_op_complete = false;
	
	std::function<HRESULT(DWORD,PVOID)> client_cb =
		[&sessions, &got_async_op_complete]
		(DWORD dwMessageType, PVOID pMessage)
	{
		if(dwMessageType == DPN_MSGID_ENUM_HOSTS_RESPONSE)
		{
			DPNMSG_ENUM_HOSTS_RESPONSE *ehr = (DPNMSG_ENUM_HOSTS_RESPONSE*)(pMessage);
			
			EXPECT_EQ(ehr->dwSize,        sizeof(DPNMSG_ENUM_HOSTS_RESPONSE));
			EXPECT_EQ(ehr->pvUserContext, (void*)(0xBEEF));
			
			sessions.emplace(
				ehr->pApplicationDescription->guidInstance,
				FoundSession(
					ehr->pApplicationDescription->guidApplication,
					ehr->pApplicationDescription->pwszSessionName));
		}
		else if(dwMessageType == DPN_MSGID_ASYNC_OP_COMPLETE)
		{
			got_async_op_complete = true;
		}
		
		return DPN_OK;
	};
	
	IDP8PeerInstance client;
	
	ASSERT_EQ(client->Initialize(&client_cb, &callback_shim, 0), S_OK);
	
	IDP8AddressInstance device_address;
	device_address->SetSP(&CLSID_DP8SP_TCPIP);
	
	DWORD start = GetTickCount();
	
	ASSERT_EQ(client->EnumHosts(
		NULL,              /* pApplicationDesc */
		NULL,              /* pdpaddrHost */
		device_address,    /* pdpaddrDeviceInfo */
		NULL,              /* pvUserEnumData */
		0,                 /* dwUserEnumDataSize */
		3,                 /* dwEnumCount */
		500,               /* dwRetryInterval */
		500,               /* dwTimeOut*/
		(void*)(0xBEEF),   /* pvUserContext */
		NULL,              /* pAsyncHandle */
		DPNENUMHOSTS_SYNC  /* dwFlags */
	), S_OK);
	
	DWORD end = GetTickCount();
	
	FoundSession expect_sessions[] = {
		FoundSession(APP_GUID_1, L"Application 1 Session 1"),
		FoundSession(APP_GUID_1, L"Application 1 Session 2"),
		FoundSession(APP_GUID_2, L"Application 2 Session 1"),
	};
	
	EXPECT_SESSIONS(sessions, expect_sessions, expect_sessions + 3);
	
	DWORD enum_time_ms = end - start;
	EXPECT_TRUE((enum_time_ms >= 1250) && (enum_time_ms <= 1750));
	
	EXPECT_FALSE(got_async_op_complete);
}

TEST(DirectPlay8Peer, EnumHostsAsync)
{
	SessionHost a1s1(APP_GUID_1, L"Application 1 Session 1");
	SessionHost a1s2(APP_GUID_1, L"Application 1 Session 2");
	SessionHost a2s1(APP_GUID_2, L"Application 2 Session 1");
	
	std::map<GUID, FoundSession, CompareGUID> sessions;
	
	bool got_async_op_complete = false;
	DWORD got_async_op_complete_at;
	DPNHANDLE async_handle;
	
	std::function<HRESULT(DWORD,PVOID)> callback =
		[&sessions, &got_async_op_complete, &got_async_op_complete_at, &async_handle]
		(DWORD dwMessageType, PVOID pMessage)
	{
		if(dwMessageType == DPN_MSGID_ENUM_HOSTS_RESPONSE)
		{
			DPNMSG_ENUM_HOSTS_RESPONSE *ehr = (DPNMSG_ENUM_HOSTS_RESPONSE*)(pMessage);
			
			EXPECT_EQ(ehr->dwSize,        sizeof(DPNMSG_ENUM_HOSTS_RESPONSE));
			EXPECT_EQ(ehr->pvUserContext, (void*)(0xABCD));
			
			sessions.emplace(
				ehr->pApplicationDescription->guidInstance,
				FoundSession(
					ehr->pApplicationDescription->guidApplication,
					ehr->pApplicationDescription->pwszSessionName));
		}
		else if(dwMessageType == DPN_MSGID_ASYNC_OP_COMPLETE)
		{
			got_async_op_complete_at = GetTickCount();
			
			/* We shouldn't get DPNMSG_ASYNC_OP_COMPLETE multiple times. */
			EXPECT_FALSE(got_async_op_complete);
			
			DPNMSG_ASYNC_OP_COMPLETE *oc = (DPNMSG_ASYNC_OP_COMPLETE*)(pMessage);
			
			EXPECT_EQ(oc->dwSize,        sizeof(DPNMSG_ASYNC_OP_COMPLETE));
			EXPECT_EQ(oc->hAsyncOp,      async_handle);
			EXPECT_EQ(oc->pvUserContext, (void*)(0xABCD));
			EXPECT_EQ(oc->hResultCode,   S_OK);
			
			got_async_op_complete = true;
		}
		
		return DPN_OK;
	};
	
	IDP8PeerInstance client;
	
	ASSERT_EQ(client->Initialize(&callback, &callback_shim, 0), S_OK);
	
	IDP8AddressInstance device_address;
	device_address->SetSP(&CLSID_DP8SP_TCPIP);
	
	DWORD start = GetTickCount();
	
	ASSERT_EQ(client->EnumHosts(
		NULL,              /* pApplicationDesc */
		NULL,              /* pdpaddrHost */
		device_address,    /* pdpaddrDeviceInfo */
		NULL,              /* pvUserEnumData */
		0,                 /* dwUserEnumDataSize */
		3,                 /* dwEnumCount */
		500,               /* dwRetryInterval */
		500,               /* dwTimeOut*/
		(void*)(0xABCD),   /* pvUserContext */
		&async_handle,     /* pAsyncHandle */
		0                  /* dwFlags */
	), DPNSUCCESS_PENDING);
	
	Sleep(3000);
	
	FoundSession expect_sessions[] = {
		FoundSession(APP_GUID_1, L"Application 1 Session 1"),
		FoundSession(APP_GUID_1, L"Application 1 Session 2"),
		FoundSession(APP_GUID_2, L"Application 2 Session 1"),
	};
	
	EXPECT_SESSIONS(sessions, expect_sessions, expect_sessions + 3);
	
	EXPECT_TRUE(got_async_op_complete);
	
	if(got_async_op_complete)
	{
		DWORD enum_time_ms = got_async_op_complete_at - start;
		EXPECT_TRUE((enum_time_ms >= 1250) && (enum_time_ms <= 1750));
	}
}

TEST(DirectPlay8Peer, EnumHostsAsyncCancelByHandle)
{
	bool got_async_op_complete = false;
	DWORD got_async_op_complete_at;
	DPNHANDLE async_handle;
	
	std::function<HRESULT(DWORD,PVOID)> callback =
		[&got_async_op_complete, &got_async_op_complete_at, &async_handle]
		(DWORD dwMessageType, PVOID pMessage)
	{
		if(dwMessageType == DPN_MSGID_ASYNC_OP_COMPLETE)
		{
			got_async_op_complete_at = GetTickCount();
			
			/* We shouldn't get DPNMSG_ASYNC_OP_COMPLETE multiple times. */
			EXPECT_FALSE(got_async_op_complete);
			
			DPNMSG_ASYNC_OP_COMPLETE *oc = (DPNMSG_ASYNC_OP_COMPLETE*)(pMessage);
			
			EXPECT_EQ(oc->dwSize,        sizeof(DPNMSG_ASYNC_OP_COMPLETE));
			EXPECT_EQ(oc->hAsyncOp,      async_handle);
			EXPECT_EQ(oc->pvUserContext, (void*)(0xABCD));
			EXPECT_EQ(oc->hResultCode,   DPNERR_USERCANCEL);
			
			got_async_op_complete = true;
		}
		
		return DPN_OK;
	};
	
	IDP8PeerInstance client;
	
	ASSERT_EQ(client->Initialize(&callback, &callback_shim, 0), S_OK);
	
	IDP8AddressInstance device_address;
	device_address->SetSP(&CLSID_DP8SP_TCPIP);
	
	DWORD start = GetTickCount();
	
	ASSERT_EQ(client->EnumHosts(
		NULL,              /* pApplicationDesc */
		NULL,              /* pdpaddrHost */
		device_address,    /* pdpaddrDeviceInfo */
		NULL,              /* pvUserEnumData */
		0,                 /* dwUserEnumDataSize */
		3,                 /* dwEnumCount */
		500,               /* dwRetryInterval */
		500,               /* dwTimeOut*/
		(void*)(0xABCD),   /* pvUserContext */
		&async_handle,     /* pAsyncHandle */
		0                  /* dwFlags */
	), DPNSUCCESS_PENDING);
	
	ASSERT_EQ(client->CancelAsyncOperation(async_handle, 0), S_OK);
	
	Sleep(500);
	
	EXPECT_TRUE(got_async_op_complete);
	
	if(got_async_op_complete)
	{
		DWORD enum_time_ms = got_async_op_complete_at - start;
		EXPECT_TRUE(enum_time_ms <= 250);
	}
}

TEST(DirectPlay8Peer, EnumHostsAsyncCancelAllEnums)
{
	bool got_async_op_complete = false;
	DWORD got_async_op_complete_at;
	DPNHANDLE async_handle;
	
	std::function<HRESULT(DWORD,PVOID)> callback =
		[&got_async_op_complete, &got_async_op_complete_at, &async_handle]
		(DWORD dwMessageType, PVOID pMessage)
	{
		if(dwMessageType == DPN_MSGID_ASYNC_OP_COMPLETE)
		{
			got_async_op_complete_at = GetTickCount();
			
			/* We shouldn't get DPNMSG_ASYNC_OP_COMPLETE multiple times. */
			EXPECT_FALSE(got_async_op_complete);
			
			DPNMSG_ASYNC_OP_COMPLETE *oc = (DPNMSG_ASYNC_OP_COMPLETE*)(pMessage);
			
			EXPECT_EQ(oc->dwSize,        sizeof(DPNMSG_ASYNC_OP_COMPLETE));
			EXPECT_EQ(oc->hAsyncOp,      async_handle);
			EXPECT_EQ(oc->pvUserContext, (void*)(0xABCD));
			EXPECT_EQ(oc->hResultCode,   DPNERR_USERCANCEL);
			
			got_async_op_complete = true;
		}
		
		return DPN_OK;
	};
	
	IDP8PeerInstance client;
	
	ASSERT_EQ(client->Initialize(&callback, &callback_shim, 0), S_OK);
	
	IDP8AddressInstance device_address;
	device_address->SetSP(&CLSID_DP8SP_TCPIP);
	
	DWORD start = GetTickCount();
	
	ASSERT_EQ(client->EnumHosts(
		NULL,              /* pApplicationDesc */
		NULL,              /* pdpaddrHost */
		device_address,    /* pdpaddrDeviceInfo */
		NULL,              /* pvUserEnumData */
		0,                 /* dwUserEnumDataSize */
		3,                 /* dwEnumCount */
		500,               /* dwRetryInterval */
		500,               /* dwTimeOut*/
		(void*)(0xABCD),   /* pvUserContext */
		&async_handle,     /* pAsyncHandle */
		0                  /* dwFlags */
	), DPNSUCCESS_PENDING);
	
	ASSERT_EQ(client->CancelAsyncOperation(0, DPNCANCEL_ENUM), S_OK);
	
	Sleep(500);
	
	EXPECT_TRUE(got_async_op_complete);
	
	if(got_async_op_complete)
	{
		DWORD enum_time_ms = got_async_op_complete_at - start;
		EXPECT_TRUE(enum_time_ms <= 250);
	}
}

TEST(DirectPlay8Peer, EnumHostsAsyncCancelAllOperations)
{
	bool got_async_op_complete = false;
	DWORD got_async_op_complete_at;
	DPNHANDLE async_handle;
	
	std::function<HRESULT(DWORD,PVOID)> callback =
		[&got_async_op_complete, &got_async_op_complete_at, &async_handle]
		(DWORD dwMessageType, PVOID pMessage)
	{
		if(dwMessageType == DPN_MSGID_ASYNC_OP_COMPLETE)
		{
			got_async_op_complete_at = GetTickCount();
			
			/* We shouldn't get DPNMSG_ASYNC_OP_COMPLETE multiple times. */
			EXPECT_FALSE(got_async_op_complete);
			
			DPNMSG_ASYNC_OP_COMPLETE *oc = (DPNMSG_ASYNC_OP_COMPLETE*)(pMessage);
			
			EXPECT_EQ(oc->dwSize,        sizeof(DPNMSG_ASYNC_OP_COMPLETE));
			EXPECT_EQ(oc->hAsyncOp,      async_handle);
			EXPECT_EQ(oc->pvUserContext, (void*)(0xABCD));
			EXPECT_EQ(oc->hResultCode,   DPNERR_USERCANCEL);
			
			got_async_op_complete = true;
		}
		
		return DPN_OK;
	};
	
	IDP8PeerInstance client;
	
	ASSERT_EQ(client->Initialize(&callback, &callback_shim, 0), S_OK);
	
	IDP8AddressInstance device_address;
	device_address->SetSP(&CLSID_DP8SP_TCPIP);
	
	DWORD start = GetTickCount();
	
	ASSERT_EQ(client->EnumHosts(
		NULL,              /* pApplicationDesc */
		NULL,              /* pdpaddrHost */
		device_address,    /* pdpaddrDeviceInfo */
		NULL,              /* pvUserEnumData */
		0,                 /* dwUserEnumDataSize */
		3,                 /* dwEnumCount */
		500,               /* dwRetryInterval */
		500,               /* dwTimeOut*/
		(void*)(0xABCD),   /* pvUserContext */
		&async_handle,     /* pAsyncHandle */
		0                  /* dwFlags */
	), DPNSUCCESS_PENDING);
	
	ASSERT_EQ(client->CancelAsyncOperation(0, DPNCANCEL_ALL_OPERATIONS), S_OK);
	
	Sleep(500);
	
	EXPECT_TRUE(got_async_op_complete);
	
	if(got_async_op_complete)
	{
		DWORD enum_time_ms = got_async_op_complete_at - start;
		EXPECT_TRUE(enum_time_ms <= 250);
	}
}

TEST(DirectPlay8Peer, EnumHostsAsyncCancelByClose)
{
	bool got_async_op_complete = false;
	DWORD got_async_op_complete_at;
	DPNHANDLE async_handle;
	
	std::function<HRESULT(DWORD,PVOID)> callback =
		[&got_async_op_complete, &got_async_op_complete_at, &async_handle]
		(DWORD dwMessageType, PVOID pMessage)
	{
		if(dwMessageType == DPN_MSGID_ASYNC_OP_COMPLETE)
		{
			got_async_op_complete_at = GetTickCount();
			
			/* We shouldn't get DPNMSG_ASYNC_OP_COMPLETE multiple times. */
			EXPECT_FALSE(got_async_op_complete);
			
			DPNMSG_ASYNC_OP_COMPLETE *oc = (DPNMSG_ASYNC_OP_COMPLETE*)(pMessage);
			
			EXPECT_EQ(oc->dwSize,        sizeof(DPNMSG_ASYNC_OP_COMPLETE));
			EXPECT_EQ(oc->hAsyncOp,      async_handle);
			EXPECT_EQ(oc->pvUserContext, (void*)(0xABCD));
			EXPECT_EQ(oc->hResultCode,   DPNERR_USERCANCEL);
			
			got_async_op_complete = true;
		}
		
		return DPN_OK;
	};
	
	IDP8PeerInstance client;
	
	ASSERT_EQ(client->Initialize(&callback, &callback_shim, 0), S_OK);
	
	IDP8AddressInstance device_address;
	device_address->SetSP(&CLSID_DP8SP_TCPIP);
	
	DWORD start = GetTickCount();
	
	ASSERT_EQ(client->EnumHosts(
		NULL,              /* pApplicationDesc */
		NULL,              /* pdpaddrHost */
		device_address,    /* pdpaddrDeviceInfo */
		NULL,              /* pvUserEnumData */
		0,                 /* dwUserEnumDataSize */
		3,                 /* dwEnumCount */
		500,               /* dwRetryInterval */
		500,               /* dwTimeOut*/
		(void*)(0xABCD),   /* pvUserContext */
		&async_handle,     /* pAsyncHandle */
		0                  /* dwFlags */
	), DPNSUCCESS_PENDING);
	
	ASSERT_EQ(client->Close(0), S_OK);
	
	EXPECT_TRUE(got_async_op_complete);
	
	if(got_async_op_complete)
	{
		DWORD enum_time_ms = got_async_op_complete_at - start;
		EXPECT_TRUE(enum_time_ms <= 250);
	}
}

TEST(DirectPlay8Peer, EnumHostsFilterByApplicationGUID)
{
	bool right_app_got_host_enum_query = false;
	bool wrong_app_got_host_enum_query = false;
	
	SessionHost a1s1(APP_GUID_1, L"Application 1 Session 1",
		[&wrong_app_got_host_enum_query]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_ENUM_HOSTS_QUERY)
			{
				wrong_app_got_host_enum_query = true;
			}
			
			return DPN_OK;
		});
	
	SessionHost a1s2(APP_GUID_1, L"Application 1 Session 2",
		[&wrong_app_got_host_enum_query]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_ENUM_HOSTS_QUERY)
			{
				wrong_app_got_host_enum_query = true;
			}
			
			return DPN_OK;
		});
	
	SessionHost a2s1(APP_GUID_2, L"Application 2 Session 1",
		[&right_app_got_host_enum_query]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_ENUM_HOSTS_QUERY)
			{
				DPNMSG_ENUM_HOSTS_QUERY *ehq = (DPNMSG_ENUM_HOSTS_QUERY*)(pMessage);
				
				EXPECT_EQ(ehq->dwSize, sizeof(DPNMSG_ENUM_HOSTS_QUERY));
				
				/* TODO: Check pAddressSender, pAddressDevice */
				
				EXPECT_EQ(ehq->pvReceivedData, nullptr);
				EXPECT_EQ(ehq->dwReceivedDataSize, 0);
				
				EXPECT_EQ(ehq->pvResponseData, nullptr);
				EXPECT_EQ(ehq->dwResponseDataSize, 0);
				
				right_app_got_host_enum_query = true;
			}
			
			return DPN_OK;
		});
	
	std::map<GUID, FoundSession, CompareGUID> sessions;
	
	std::function<HRESULT(DWORD,PVOID)> client_cb =
		[&sessions]
		(DWORD dwMessageType, PVOID pMessage)
	{
		if(dwMessageType == DPN_MSGID_ENUM_HOSTS_RESPONSE)
		{
			DPNMSG_ENUM_HOSTS_RESPONSE *ehr = (DPNMSG_ENUM_HOSTS_RESPONSE*)(pMessage);
			
			EXPECT_EQ(ehr->dwSize,        sizeof(DPNMSG_ENUM_HOSTS_RESPONSE));
			EXPECT_EQ(ehr->pvUserContext, (void*)(0xBEEF));
			
			sessions.emplace(
				ehr->pApplicationDescription->guidInstance,
				FoundSession(
					ehr->pApplicationDescription->guidApplication,
					ehr->pApplicationDescription->pwszSessionName));
		}
		
		return DPN_OK;
	};
	
	IDP8PeerInstance client;
	
	ASSERT_EQ(client->Initialize(&client_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC app_desc;
	memset(&app_desc, 0, sizeof(app_desc));
	
	app_desc.dwSize          = sizeof(app_desc);
	app_desc.guidApplication = APP_GUID_2;
	
	IDP8AddressInstance device_address;
	device_address->SetSP(&CLSID_DP8SP_TCPIP);
	
	ASSERT_EQ(client->EnumHosts(
		&app_desc,         /* pApplicationDesc */
		NULL,              /* pdpaddrHost */
		device_address,    /* pdpaddrDeviceInfo */
		NULL,              /* pvUserEnumData */
		0,                 /* dwUserEnumDataSize */
		3,                 /* dwEnumCount */
		500,               /* dwRetryInterval */
		500,               /* dwTimeOut*/
		(void*)(0xBEEF),   /* pvUserContext */
		NULL,              /* pAsyncHandle */
		DPNENUMHOSTS_SYNC  /* dwFlags */
	), S_OK);
	
	FoundSession expect_sessions[] = {
		FoundSession(APP_GUID_2, L"Application 2 Session 1"),
	};
	
	EXPECT_SESSIONS(sessions, expect_sessions, expect_sessions + 1);
	
	EXPECT_TRUE(right_app_got_host_enum_query);
	EXPECT_FALSE(wrong_app_got_host_enum_query);
}

TEST(DirectPlay8Peer, EnumHostsFilterByNULLApplicationGUID)
{
	SessionHost a1s1(APP_GUID_1, L"Application 1 Session 1");
	SessionHost a1s2(APP_GUID_1, L"Application 1 Session 2");
	SessionHost a2s1(APP_GUID_2, L"Application 2 Session 1");
	
	std::map<GUID, FoundSession, CompareGUID> sessions;
	
	std::function<HRESULT(DWORD,PVOID)> client_cb =
		[&sessions]
		(DWORD dwMessageType, PVOID pMessage)
	{
		if(dwMessageType == DPN_MSGID_ENUM_HOSTS_RESPONSE)
		{
			DPNMSG_ENUM_HOSTS_RESPONSE *ehr = (DPNMSG_ENUM_HOSTS_RESPONSE*)(pMessage);
			
			EXPECT_EQ(ehr->dwSize,        sizeof(DPNMSG_ENUM_HOSTS_RESPONSE));
			EXPECT_EQ(ehr->pvUserContext, (void*)(0xBEEF));
			
			sessions.emplace(
				ehr->pApplicationDescription->guidInstance,
				FoundSession(
					ehr->pApplicationDescription->guidApplication,
					ehr->pApplicationDescription->pwszSessionName));
		}
		
		return DPN_OK;
	};
	
	IDP8PeerInstance client;
	
	ASSERT_EQ(client->Initialize(&client_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC app_desc;
	memset(&app_desc, 0, sizeof(app_desc));
	
	app_desc.dwSize          = sizeof(app_desc);
	app_desc.guidApplication = GUID_NULL;
	
	IDP8AddressInstance device_address;
	device_address->SetSP(&CLSID_DP8SP_TCPIP);
	
	ASSERT_EQ(client->EnumHosts(
		&app_desc,         /* pApplicationDesc */
		NULL,              /* pdpaddrHost */
		device_address,    /* pdpaddrDeviceInfo */
		NULL,              /* pvUserEnumData */
		0,                 /* dwUserEnumDataSize */
		3,                 /* dwEnumCount */
		500,               /* dwRetryInterval */
		500,               /* dwTimeOut*/
		(void*)(0xBEEF),   /* pvUserContext */
		NULL,              /* pAsyncHandle */
		DPNENUMHOSTS_SYNC  /* dwFlags */
	), S_OK);
	
	FoundSession expect_sessions[] = {
		FoundSession(APP_GUID_1, L"Application 1 Session 1"),
		FoundSession(APP_GUID_1, L"Application 1 Session 2"),
		FoundSession(APP_GUID_2, L"Application 2 Session 1"),
	};
	
	EXPECT_SESSIONS(sessions, expect_sessions, expect_sessions + 3);
}

TEST(DirectPlay8Peer, EnumHostsDataInQuery)
{
	static const unsigned char DATA[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };
	
	SessionHost a1s1(APP_GUID_1, L"Application 1 Session 1",
		[]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_ENUM_HOSTS_QUERY)
			{
				DPNMSG_ENUM_HOSTS_QUERY *ehq = (DPNMSG_ENUM_HOSTS_QUERY*)(pMessage);
				
				EXPECT_EQ(ehq->dwSize, sizeof(DPNMSG_ENUM_HOSTS_QUERY));
				
				std::vector<unsigned char> got_data(
					(const unsigned char*)(ehq->pvReceivedData),
					(const unsigned char*)(ehq->pvReceivedData) + ehq->dwReceivedDataSize);
				
				std::vector<unsigned char> expect_data(DATA, DATA + sizeof(DATA));
				
				EXPECT_EQ(got_data, expect_data);
				
				EXPECT_EQ(ehq->pvResponseData, nullptr);
				EXPECT_EQ(ehq->dwResponseDataSize, 0);
			}
			
			return DPN_OK;
		});
	
	std::map<GUID, FoundSession, CompareGUID> sessions;
	
	std::function<HRESULT(DWORD,PVOID)> client_cb =
		[&sessions]
		(DWORD dwMessageType, PVOID pMessage)
	{
		if(dwMessageType == DPN_MSGID_ENUM_HOSTS_RESPONSE)
		{
			DPNMSG_ENUM_HOSTS_RESPONSE *ehr = (DPNMSG_ENUM_HOSTS_RESPONSE*)(pMessage);
			
			EXPECT_EQ(ehr->dwSize,        sizeof(DPNMSG_ENUM_HOSTS_RESPONSE));
			EXPECT_EQ(ehr->pvUserContext, (void*)(0xBEEF));
			
			EXPECT_EQ(ehr->pvResponseData, nullptr);
			EXPECT_EQ(ehr->dwResponseDataSize, 0);
			
			sessions.emplace(
				ehr->pApplicationDescription->guidInstance,
				FoundSession(
					ehr->pApplicationDescription->guidApplication,
					ehr->pApplicationDescription->pwszSessionName));
		}
		
		return DPN_OK;
	};
	
	IDP8PeerInstance client;
	
	ASSERT_EQ(client->Initialize(&client_cb, &callback_shim, 0), S_OK);
	
	IDP8AddressInstance device_address;
	device_address->SetSP(&CLSID_DP8SP_TCPIP);
	
	ASSERT_EQ(client->EnumHosts(
		NULL,              /* pApplicationDesc */
		NULL,              /* pdpaddrHost */
		device_address,    /* pdpaddrDeviceInfo */
		(void*)(DATA),     /* pvUserEnumData */
		sizeof(DATA),      /* dwUserEnumDataSize */
		3,                 /* dwEnumCount */
		500,               /* dwRetryInterval */
		500,               /* dwTimeOut*/
		(void*)(0xBEEF),   /* pvUserContext */
		NULL,              /* pAsyncHandle */
		DPNENUMHOSTS_SYNC  /* dwFlags */
	), S_OK);
	
	FoundSession expect_sessions[] = {
		FoundSession(APP_GUID_1, L"Application 1 Session 1"),
	};
	
	EXPECT_SESSIONS(sessions, expect_sessions, expect_sessions + 1);
}

TEST(DirectPlay8Peer, EnumHostsDataInResponse)
{
	static const unsigned char DATA[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };
	
	bool got_return_buffer = false;
	
	SessionHost a1s1(APP_GUID_1, L"Application 1 Session 1",
		[&got_return_buffer]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_ENUM_HOSTS_QUERY)
			{
				DPNMSG_ENUM_HOSTS_QUERY *ehq = (DPNMSG_ENUM_HOSTS_QUERY*)(pMessage);
				
				EXPECT_EQ(ehq->dwSize, sizeof(DPNMSG_ENUM_HOSTS_QUERY));
				
				EXPECT_EQ(ehq->pvReceivedData, nullptr);
				EXPECT_EQ(ehq->dwReceivedDataSize, 0);
				
				EXPECT_EQ(ehq->pvResponseData, nullptr);
				EXPECT_EQ(ehq->dwResponseDataSize, 0);
				
				ehq->pvResponseData     = (void*)(DATA);
				ehq->dwResponseDataSize = sizeof(DATA);
				ehq->pvResponseContext  = (void*)(0x1234);
			}
			else if(dwMessageType == DPN_MSGID_RETURN_BUFFER)
			{
				DPNMSG_RETURN_BUFFER *rb = (DPNMSG_RETURN_BUFFER*)(pMessage);
				
				EXPECT_EQ(rb->dwSize,        sizeof(*rb));
				EXPECT_EQ(rb->hResultCode,   S_OK);
				EXPECT_EQ(rb->pvBuffer,      DATA);
				EXPECT_EQ(rb->pvUserContext, (void*)(0x1234));
				
				got_return_buffer = true;
			}
			
			return DPN_OK;
		});
	
	std::map<GUID, FoundSession, CompareGUID> sessions;
	
	std::function<HRESULT(DWORD,PVOID)> client_cb =
		[&sessions]
		(DWORD dwMessageType, PVOID pMessage)
	{
		if(dwMessageType == DPN_MSGID_ENUM_HOSTS_RESPONSE)
		{
			DPNMSG_ENUM_HOSTS_RESPONSE *ehr = (DPNMSG_ENUM_HOSTS_RESPONSE*)(pMessage);
			
			EXPECT_EQ(ehr->dwSize,        sizeof(DPNMSG_ENUM_HOSTS_RESPONSE));
			EXPECT_EQ(ehr->pvUserContext, (void*)(0xBEEF));
			
			std::vector<unsigned char> got_data(
				(const unsigned char*)(ehr->pvResponseData),
				(const unsigned char*)(ehr->pvResponseData) + ehr->dwResponseDataSize);
			
			std::vector<unsigned char> expect_data(DATA, DATA + sizeof(DATA));
			
			EXPECT_EQ(got_data, expect_data);
			
			sessions.emplace(
				ehr->pApplicationDescription->guidInstance,
				FoundSession(
					ehr->pApplicationDescription->guidApplication,
					ehr->pApplicationDescription->pwszSessionName));
		}
		
		return DPN_OK;
	};
	
	IDP8PeerInstance client;
	
	ASSERT_EQ(client->Initialize(&client_cb, &callback_shim, 0), S_OK);
	
	IDP8AddressInstance device_address;
	device_address->SetSP(&CLSID_DP8SP_TCPIP);
	
	ASSERT_EQ(client->EnumHosts(
		NULL,              /* pApplicationDesc */
		NULL,              /* pdpaddrHost */
		device_address,    /* pdpaddrDeviceInfo */
		NULL,              /* pvUserEnumData */
		0,                 /* dwUserEnumDataSize */
		3,                 /* dwEnumCount */
		500,               /* dwRetryInterval */
		500,               /* dwTimeOut*/
		(void*)(0xBEEF),   /* pvUserContext */
		NULL,              /* pAsyncHandle */
		DPNENUMHOSTS_SYNC  /* dwFlags */
	), S_OK);
	
	FoundSession expect_sessions[] = {
		FoundSession(APP_GUID_1, L"Application 1 Session 1"),
	};
	
	EXPECT_SESSIONS(sessions, expect_sessions, expect_sessions + 1);
	
	EXPECT_TRUE(got_return_buffer);
}

TEST(DirectPlay8Peer, EnumHostsSpecifyPort)
{
	SessionHost a1s1(APP_GUID_1, L"Application 1 Session 1", PORT);
	SessionHost a1s2(APP_GUID_1, L"Application 1 Session 2", PORT + 1);
	
	std::map<GUID, FoundSession, CompareGUID> sessions;
	
	std::function<HRESULT(DWORD,PVOID)> client_cb =
		[&sessions]
		(DWORD dwMessageType, PVOID pMessage)
	{
		if(dwMessageType == DPN_MSGID_ENUM_HOSTS_RESPONSE)
		{
			DPNMSG_ENUM_HOSTS_RESPONSE *ehr = (DPNMSG_ENUM_HOSTS_RESPONSE*)(pMessage);
			
			sessions.emplace(
				ehr->pApplicationDescription->guidInstance,
				FoundSession(
					ehr->pApplicationDescription->guidApplication,
					ehr->pApplicationDescription->pwszSessionName));
		}
		
		return DPN_OK;
	};
	
	IDP8PeerInstance client;
	
	ASSERT_EQ(client->Initialize(&client_cb, &callback_shim, 0), S_OK);
	
	IDP8AddressInstance host_address(L"127.0.0.1", PORT);
	
	IDP8AddressInstance device_address;
	device_address->SetSP(&CLSID_DP8SP_TCPIP);
	
	ASSERT_EQ(client->EnumHosts(
		NULL,              /* pApplicationDesc */
		host_address,      /* pdpaddrHost */
		device_address,    /* pdpaddrDeviceInfo */
		NULL,              /* pvUserEnumData */
		0,                 /* dwUserEnumDataSize */
		3,                 /* dwEnumCount */
		500,               /* dwRetryInterval */
		500,               /* dwTimeOut*/
		NULL,              /* pvUserContext */
		NULL,              /* pAsyncHandle */
		DPNENUMHOSTS_SYNC  /* dwFlags */
	), S_OK);
	
	FoundSession expect_sessions[] = {
		FoundSession(APP_GUID_1, L"Application 1 Session 1"),
	};
	
	EXPECT_SESSIONS(sessions, expect_sessions, expect_sessions + 1);
}

TEST(DirectPlay8Peer, ConnectSync)
{
	std::atomic<bool> testing(true);
	
	std::atomic<int> host_seq(0), p1_seq(0);
	DPNID host_player_id = -1, p1_player_id = -1;
	
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[&testing, &host_seq, &host_player_id, &p1_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				case 1:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						host_player_id = cp->dpnidPlayer;
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0xB00));
						
						cp->pvPlayerContext = (void*)(0xB00B00);
					}
					
					break;
					
				case 2:
					EXPECT_EQ(dwMessageType, DPN_MSGID_INDICATE_CONNECT);
					
					if(dwMessageType == DPN_MSGID_INDICATE_CONNECT)
					{
						DPNMSG_INDICATE_CONNECT *ic = (DPNMSG_INDICATE_CONNECT*)(pMessage);
						
						EXPECT_EQ(ic->dwSize, sizeof(DPNMSG_INDICATE_CONNECT));
						
						EXPECT_EQ(ic->pvUserConnectData,     (void*)(NULL));
						EXPECT_EQ(ic->dwUserConnectDataSize, 0);
						
						EXPECT_EQ(ic->pvReplyData,     (void*)(NULL));
						EXPECT_EQ(ic->dwReplyDataSize, 0);
						
						EXPECT_EQ(ic->pvReplyContext,  (void*)(NULL));
						EXPECT_EQ(ic->pvPlayerContext, (void*)(NULL));
						
						/* TODO: Check pAddressPlayer, pAddressDevice */
						
						ic->pvPlayerContext = (void*)(0xB441);
					}
					
					break;
					
				case 3:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						p1_player_id = cp->dpnidPlayer;
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0xB441));
						
						cp->pvPlayerContext = (void*)(0xFEED);
					}
					
					break;
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		});
	
	/* Give the host instance a moment to settle. */
	Sleep(1000);
	
	DPNID p1_cp_dpnidPlayer = -1, p1_cc_dpnidLocal = -1;
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&testing, &p1_seq, &host_player_id, &p1_player_id, &p1_cp_dpnidPlayer, &p1_cc_dpnidLocal]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++p1_seq;
			
			switch(seq)
			{
				case 1:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						p1_cp_dpnidPlayer = cp->dpnidPlayer;
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0xBCDE));
						
						cp->pvPlayerContext = (void*)(0xCDEF);
					}
					
					break;
					
				case 2:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->dpnidPlayer,     host_player_id);
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0));
						
						cp->pvPlayerContext = (void*)(0xBAA);
					}
					
					break;
					
				case 3:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CONNECT_COMPLETE);
					
					if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
					{
						DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
						
						EXPECT_EQ(cc->dwSize,      sizeof(DPNMSG_CONNECT_COMPLETE));
						EXPECT_EQ(cc->hAsyncOp,    0);
						EXPECT_EQ(cc->hResultCode, S_OK);
						
						EXPECT_EQ(cc->pvApplicationReplyData,     (PVOID)(NULL));
						EXPECT_EQ(cc->dwApplicationReplyDataSize, 0);
						
						p1_cc_dpnidLocal = cc->dpnidLocal;
					}
					
					break;
					
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	EXPECT_EQ(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		(void*)(0xBCDE),  /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	/* Give the host instance a moment to settle. */
	Sleep(1000);
	
	testing = false;
	
	EXPECT_EQ(host_seq, 3);
	EXPECT_EQ(p1_seq, 3);
	
	EXPECT_EQ(p1_cp_dpnidPlayer, p1_player_id);
	EXPECT_EQ(p1_cc_dpnidLocal,  p1_player_id);
}

TEST(DirectPlay8Peer, ConnectSyncFail)
{
	SessionHost host(APP_GUID_2, L"Session 1", PORT,
		[]
		(DWORD dwMessageType, PVOID pMessage)
		{
			return DPN_OK;
		});
	
	std::atomic<int> p1_seq(0);
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&p1_seq]
		(DWORD dwMessageType, PVOID pMessage)
		{
			int seq = ++p1_seq;
			
			switch(seq)
			{
				case 1:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CONNECT_COMPLETE);
					
					if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
					{
						DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
						
						EXPECT_EQ(cc->dwSize,      sizeof(DPNMSG_CONNECT_COMPLETE));
						EXPECT_EQ(cc->hAsyncOp,    0);
						EXPECT_NE(cc->hResultCode, S_OK);
						
						EXPECT_EQ(cc->pvApplicationReplyData,     (PVOID)(NULL));
						EXPECT_EQ(cc->dwApplicationReplyDataSize, 0);
					}
					
					break;
					
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	EXPECT_NE(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		NULL,             /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	EXPECT_EQ(p1_seq, 1);
}

TEST(DirectPlay8Peer, ConnectAsync)
{
	std::atomic<bool> testing(true);
	
	std::atomic<int> host_seq(0), p1_seq(0);
	DPNID host_player_id = -1, p1_player_id = -1;
	
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[&testing, &host_seq, &host_player_id, &p1_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				case 1:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						host_player_id = cp->dpnidPlayer;
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0xB00));
						
						cp->pvPlayerContext = (void*)(0xB00B00);
					}
					
					break;
					
				case 2:
					EXPECT_EQ(dwMessageType, DPN_MSGID_INDICATE_CONNECT);
					
					if(dwMessageType == DPN_MSGID_INDICATE_CONNECT)
					{
						DPNMSG_INDICATE_CONNECT *ic = (DPNMSG_INDICATE_CONNECT*)(pMessage);
						
						EXPECT_EQ(ic->dwSize, sizeof(DPNMSG_INDICATE_CONNECT));
						
						EXPECT_EQ(ic->pvUserConnectData,     (void*)(NULL));
						EXPECT_EQ(ic->dwUserConnectDataSize, 0);
						
						EXPECT_EQ(ic->pvReplyData,     (void*)(NULL));
						EXPECT_EQ(ic->dwReplyDataSize, 0);
						
						EXPECT_EQ(ic->pvReplyContext,  (void*)(NULL));
						EXPECT_EQ(ic->pvPlayerContext, (void*)(NULL));
						
						/* TODO: Check pAddressPlayer, pAddressDevice */
						
						ic->pvPlayerContext = (void*)(0xB441);
					}
					
					break;
					
				case 3:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						p1_player_id = cp->dpnidPlayer;
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0xB441));
						
						cp->pvPlayerContext = (void*)(0xFEED);
					}
					
					break;
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		});
	
	/* Give the host instance a moment to settle. */
	Sleep(1000);
	
	DPNID p1_cp_dpnidPlayer = -1, p1_cc_dpnidLocal = -1;
	DPNHANDLE p1_connect_handle;
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&testing, &p1_seq, &host_player_id, &p1_player_id, &p1_cp_dpnidPlayer, &p1_connect_handle, &p1_cc_dpnidLocal]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++p1_seq;
			
			switch(seq)
			{
				case 1:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						p1_cp_dpnidPlayer = cp->dpnidPlayer;
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0xBCDE));
						
						cp->pvPlayerContext = (void*)(0xCDEF);
					}
					
					break;
					
				case 2:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->dpnidPlayer,     host_player_id);
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0));
						
						cp->pvPlayerContext = (void*)(0xBAA);
					}
					
					break;
					
				case 3:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CONNECT_COMPLETE);
					
					if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
					{
						DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
						
						EXPECT_EQ(cc->dwSize,        sizeof(DPNMSG_CONNECT_COMPLETE));
						EXPECT_EQ(cc->hAsyncOp,      p1_connect_handle);
						EXPECT_EQ(cc->pvUserContext, (void*)(0xABCD));
						EXPECT_EQ(cc->hResultCode,   S_OK);
						
						EXPECT_EQ(cc->pvApplicationReplyData,     (PVOID)(NULL));
						EXPECT_EQ(cc->dwApplicationReplyDataSize, 0);
						
						p1_cc_dpnidLocal = cc->dpnidLocal;
					}
					
					break;
					
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	EXPECT_EQ(p1->Connect(
		&connect_to_app,     /* pdnAppDesc */
		connect_to_addr,     /* pHostAddr */
		NULL,                /* pDeviceInfo */
		NULL,                /* pdnSecurity */
		NULL,                /* pdnCredentials */
		NULL,                /* pvUserConnectData */
		0,                   /* dwUserConnectDataSize */
		(void*)(0xBCDE),     /* pvPlayerContext */
		(void*)(0xABCD),     /* pvAsyncContext */
		&p1_connect_handle,  /* phAsyncHandle */
		0                    /* dwFlags */
	), DPNSUCCESS_PENDING);
	
	/* Give the connect a chance to complete. */
	Sleep(5000);
	
	testing = false;
	
	EXPECT_EQ(host_seq, 3);
	EXPECT_EQ(p1_seq, 3);
	
	EXPECT_EQ(p1_cp_dpnidPlayer, p1_player_id);
	EXPECT_EQ(p1_cc_dpnidLocal,  p1_player_id);
	
	void *host_host_player_ctx;
	EXPECT_EQ(host->GetPlayerContext(host_player_id, &host_host_player_ctx, 0), S_OK);
	EXPECT_EQ(host_host_player_ctx, (void*)(0xB00B00));
	
	void *host_p1_player_ctx;
	EXPECT_EQ(host->GetPlayerContext(p1_player_id, &host_p1_player_ctx, 0), S_OK);
	EXPECT_EQ(host_p1_player_ctx, (void*)(0xFEED));
	
	void *p1_host_player_ctx;
	EXPECT_EQ(p1->GetPlayerContext(host_player_id, &p1_host_player_ctx, 0), S_OK);
	EXPECT_EQ(p1_host_player_ctx, (void*)(0xBAA));
	
	void *p1_p1_player_ctx;
	EXPECT_EQ(p1->GetPlayerContext(p1_player_id, &p1_p1_player_ctx, 0), S_OK);
	EXPECT_EQ(p1_p1_player_ctx, (void*)(0xCDEF));
}

TEST(DirectPlay8Peer, ConnectAsyncFail)
{
	SessionHost host(APP_GUID_2, L"Session 1", PORT,
		[]
		(DWORD dwMessageType, PVOID pMessage)
		{
			return DPN_OK;
		});
	
	std::atomic<int> p1_seq(0);
	
	DPNHANDLE p1_connect_handle;
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&p1_seq, &p1_connect_handle]
		(DWORD dwMessageType, PVOID pMessage)
		{
			int seq = ++p1_seq;
			
			switch(seq)
			{
				case 1:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CONNECT_COMPLETE);
					
					if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
					{
						DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
						
						EXPECT_EQ(cc->dwSize,        sizeof(DPNMSG_CONNECT_COMPLETE));
						EXPECT_EQ(cc->hAsyncOp,      p1_connect_handle);
						EXPECT_EQ(cc->pvUserContext, (void*)(0xABCD));
						EXPECT_NE(cc->hResultCode,   S_OK);
						
						EXPECT_EQ(cc->pvApplicationReplyData,     (PVOID)(NULL));
						EXPECT_EQ(cc->dwApplicationReplyDataSize, 0);
					}
					
					break;
					
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	EXPECT_EQ(p1->Connect(
		&connect_to_app,     /* pdnAppDesc */
		connect_to_addr,     /* pHostAddr */
		NULL,                /* pDeviceInfo */
		NULL,                /* pdnSecurity */
		NULL,                /* pdnCredentials */
		NULL,                /* pvUserConnectData */
		0,                   /* dwUserConnectDataSize */
		NULL,                /* pvPlayerContext */
		(void*)(0xABCD),     /* pvAsyncContext */
		&p1_connect_handle,  /* phAsyncHandle */
		0                    /* dwFlags */
	), DPNSUCCESS_PENDING);
	
	/* Give the connect a chance to complete. */
	Sleep(5000);
	
	EXPECT_EQ(p1_seq, 1);
}

TEST(DirectPlay8Peer, ConnectAsyncCancelByHandle)
{
	/* We set up a host which blocks when processing DPN_MSGID_INDICATE_CONNECT, causing any
	 * attempted Connect() by a peer to take a while, giving us time to cancel it.
	*/
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_INDICATE_CONNECT)
			{
				Sleep(2000);
			}
			
			return DPN_OK;
		});
	
	TestPeer peer1("peer1");
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	DPNHANDLE p1_connect_handle;
	ASSERT_EQ(peer1->Connect(
		&connect_to_app,     /* pdnAppDesc */
		connect_to_addr,     /* pHostAddr */
		NULL,                /* pDeviceInfo */
		NULL,                /* pdnSecurity */
		NULL,                /* pdnCredentials */
		NULL,                /* pvUserConnectData */
		0,                   /* dwUserConnectDataSize */
		NULL,                /* pvPlayerContext */
		(void*)(0xABCD),     /* pvAsyncContext */
		&p1_connect_handle,  /* phAsyncHandle */
		0                    /* dwFlags */
	), DPNSUCCESS_PENDING);
	
	peer1.expect_begin();
	peer1.expect_push([&p1_connect_handle](DWORD dwMessageType, PVOID pMessage)
	{
		EXPECT_EQ(dwMessageType, DPN_MSGID_CONNECT_COMPLETE);
		
		if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
		{
			DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
			
			EXPECT_EQ(cc->dwSize,        sizeof(DPNMSG_CONNECT_COMPLETE));
			EXPECT_EQ(cc->hAsyncOp,      p1_connect_handle);
			EXPECT_EQ(cc->pvUserContext, (void*)(0xABCD));
			EXPECT_EQ(cc->hResultCode,   DPNERR_USERCANCEL);
			
			EXPECT_EQ(cc->pvApplicationReplyData,     (PVOID)(NULL));
			EXPECT_EQ(cc->dwApplicationReplyDataSize, 0);
		}
		
		return DPN_OK;
	});
	
	ASSERT_EQ(peer1->CancelAsyncOperation(p1_connect_handle, 0), S_OK);
	
	Sleep(250);
	
	peer1.expect_end();
}

TEST(DirectPlay8Peer, ConnectAsyncCancelAllConnects)
{
	/* We set up a host which blocks when processing DPN_MSGID_INDICATE_CONNECT, causing any
	 * attempted Connect() by a peer to take a while, giving us time to cancel it.
	*/
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_INDICATE_CONNECT)
			{
				Sleep(2000);
			}
			
			return DPN_OK;
		});
	
	TestPeer peer1("peer1");
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	DPNHANDLE p1_connect_handle;
	ASSERT_EQ(peer1->Connect(
		&connect_to_app,     /* pdnAppDesc */
		connect_to_addr,     /* pHostAddr */
		NULL,                /* pDeviceInfo */
		NULL,                /* pdnSecurity */
		NULL,                /* pdnCredentials */
		NULL,                /* pvUserConnectData */
		0,                   /* dwUserConnectDataSize */
		NULL,                /* pvPlayerContext */
		(void*)(0xABCD),     /* pvAsyncContext */
		&p1_connect_handle,  /* phAsyncHandle */
		0                    /* dwFlags */
	), DPNSUCCESS_PENDING);
	
	peer1.expect_begin();
	peer1.expect_push([&p1_connect_handle](DWORD dwMessageType, PVOID pMessage)
	{
		EXPECT_EQ(dwMessageType, DPN_MSGID_CONNECT_COMPLETE);
		
		if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
		{
			DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
			
			EXPECT_EQ(cc->dwSize,        sizeof(DPNMSG_CONNECT_COMPLETE));
			EXPECT_EQ(cc->hAsyncOp,      p1_connect_handle);
			EXPECT_EQ(cc->pvUserContext, (void*)(0xABCD));
			EXPECT_EQ(cc->hResultCode,   DPNERR_USERCANCEL);
			
			EXPECT_EQ(cc->pvApplicationReplyData,     (PVOID)(NULL));
			EXPECT_EQ(cc->dwApplicationReplyDataSize, 0);
		}
		
		return DPN_OK;
	});
	
	ASSERT_EQ(peer1->CancelAsyncOperation(NULL, DPNCANCEL_CONNECT), S_OK);
	
	Sleep(250);
	
	peer1.expect_end();
}

TEST(DirectPlay8Peer, ConnectAsyncCancelAllOperations)
{
	/* We set up a host which blocks when processing DPN_MSGID_INDICATE_CONNECT, causing any
	 * attempted Connect() by a peer to take a while, giving us time to cancel it.
	*/
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_INDICATE_CONNECT)
			{
				Sleep(2000);
			}
			
			return DPN_OK;
		});
	
	TestPeer peer1("peer1");
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	DPNHANDLE p1_connect_handle;
	ASSERT_EQ(peer1->Connect(
		&connect_to_app,     /* pdnAppDesc */
		connect_to_addr,     /* pHostAddr */
		NULL,                /* pDeviceInfo */
		NULL,                /* pdnSecurity */
		NULL,                /* pdnCredentials */
		NULL,                /* pvUserConnectData */
		0,                   /* dwUserConnectDataSize */
		NULL,                /* pvPlayerContext */
		(void*)(0xABCD),     /* pvAsyncContext */
		&p1_connect_handle,  /* phAsyncHandle */
		0                    /* dwFlags */
	), DPNSUCCESS_PENDING);
	
	peer1.expect_begin();
	peer1.expect_push([&p1_connect_handle](DWORD dwMessageType, PVOID pMessage)
	{
		EXPECT_EQ(dwMessageType, DPN_MSGID_CONNECT_COMPLETE);
		
		if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
		{
			DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
			
			EXPECT_EQ(cc->dwSize,        sizeof(DPNMSG_CONNECT_COMPLETE));
			EXPECT_EQ(cc->hAsyncOp,      p1_connect_handle);
			EXPECT_EQ(cc->pvUserContext, (void*)(0xABCD));
			EXPECT_EQ(cc->hResultCode,   DPNERR_USERCANCEL);
			
			EXPECT_EQ(cc->pvApplicationReplyData,     (PVOID)(NULL));
			EXPECT_EQ(cc->dwApplicationReplyDataSize, 0);
		}
		
		return DPN_OK;
	});
	
	ASSERT_EQ(peer1->CancelAsyncOperation(NULL, DPNCANCEL_ALL_OPERATIONS), S_OK);
	
	Sleep(250);
	
	peer1.expect_end();
}

TEST(DirectPlay8Peer, ConnectAsyncCancelByClose)
{
	/* We set up a host which blocks when processing DPN_MSGID_INDICATE_CONNECT, causing any
	 * attempted Connect() by a peer to take a while, giving us time to cancel it.
	*/
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_INDICATE_CONNECT)
			{
				Sleep(2000);
			}
			
			return DPN_OK;
		});
	
	TestPeer peer1("peer1");
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	DPNHANDLE p1_connect_handle;
	ASSERT_EQ(peer1->Connect(
		&connect_to_app,     /* pdnAppDesc */
		connect_to_addr,     /* pHostAddr */
		NULL,                /* pDeviceInfo */
		NULL,                /* pdnSecurity */
		NULL,                /* pdnCredentials */
		NULL,                /* pvUserConnectData */
		0,                   /* dwUserConnectDataSize */
		NULL,                /* pvPlayerContext */
		(void*)(0xABCD),     /* pvAsyncContext */
		&p1_connect_handle,  /* phAsyncHandle */
		0                    /* dwFlags */
	), DPNSUCCESS_PENDING);
	
	peer1.expect_begin();
	peer1.expect_push([&p1_connect_handle](DWORD dwMessageType, PVOID pMessage)
	{
		EXPECT_EQ(dwMessageType, DPN_MSGID_CONNECT_COMPLETE);
		
		if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
		{
			DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
			
			EXPECT_EQ(cc->dwSize,        sizeof(DPNMSG_CONNECT_COMPLETE));
			EXPECT_EQ(cc->hAsyncOp,      p1_connect_handle);
			EXPECT_EQ(cc->pvUserContext, (void*)(0xABCD));
			EXPECT_EQ(cc->hResultCode,   DPNERR_NOCONNECTION);
			
			EXPECT_EQ(cc->pvApplicationReplyData,     (PVOID)(NULL));
			EXPECT_EQ(cc->dwApplicationReplyDataSize, 0);
		}
		
		return DPN_OK;
	});
	
	peer1->Close(DPNCLOSE_IMMEDIATE);
	
	Sleep(250);
	
	peer1.expect_end();
}

TEST(DirectPlay8Peer, ConnectToIPX)
{
	std::atomic<bool> testing(true);
	
	std::atomic<int> host_seq(0), p1_seq(0);
	DPNID host_player_id = -1, p1_player_id = -1;
	
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[&testing, &host_seq, &host_player_id, &p1_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				case 1:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						host_player_id = cp->dpnidPlayer;
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0xB00));
						
						cp->pvPlayerContext = (void*)(0xB00B00);
					}
					
					break;
					
				case 2:
					EXPECT_EQ(dwMessageType, DPN_MSGID_INDICATE_CONNECT);
					
					if(dwMessageType == DPN_MSGID_INDICATE_CONNECT)
					{
						DPNMSG_INDICATE_CONNECT *ic = (DPNMSG_INDICATE_CONNECT*)(pMessage);
						
						EXPECT_EQ(ic->dwSize, sizeof(DPNMSG_INDICATE_CONNECT));
						
						EXPECT_EQ(ic->pvUserConnectData,     (void*)(NULL));
						EXPECT_EQ(ic->dwUserConnectDataSize, 0);
						
						EXPECT_EQ(ic->pvReplyData,     (void*)(NULL));
						EXPECT_EQ(ic->dwReplyDataSize, 0);
						
						EXPECT_EQ(ic->pvReplyContext,  (void*)(NULL));
						EXPECT_EQ(ic->pvPlayerContext, (void*)(NULL));
						
						/* TODO: Check pAddressPlayer, pAddressDevice */
						
						ic->pvPlayerContext = (void*)(0xB441);
					}
					
					break;
					
				case 3:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						p1_player_id = cp->dpnidPlayer;
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0xB441));
						
						cp->pvPlayerContext = (void*)(0xFEED);
					}
					
					break;
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		});
	
	/* Give the host instance a moment to settle. */
	Sleep(1000);
	
	DPNID p1_cp_dpnidPlayer = -1, p1_cc_dpnidLocal = -1;
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&testing, &p1_seq, &host_player_id, &p1_player_id, &p1_cp_dpnidPlayer, &p1_cc_dpnidLocal]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++p1_seq;
			
			switch(seq)
			{
				case 1:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						p1_cp_dpnidPlayer = cp->dpnidPlayer;
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0xBCDE));
						
						cp->pvPlayerContext = (void*)(0xCDEF);
					}
					
					break;
					
				case 2:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->dpnidPlayer,     host_player_id);
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0));
						
						cp->pvPlayerContext = (void*)(0xBAA);
					}
					
					break;
					
				case 3:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CONNECT_COMPLETE);
					
					if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
					{
						DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
						
						EXPECT_EQ(cc->dwSize,      sizeof(DPNMSG_CONNECT_COMPLETE));
						EXPECT_EQ(cc->hAsyncOp,    0);
						EXPECT_EQ(cc->hResultCode, S_OK);
						
						EXPECT_EQ(cc->pvApplicationReplyData,     (PVOID)(NULL));
						EXPECT_EQ(cc->dwApplicationReplyDataSize, 0);
						
						p1_cc_dpnidLocal = cc->dpnidLocal;
					}
					
					break;
					
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(CLSID_DP8SP_IPX, L"00000000,00007F000001", PORT);
	
	EXPECT_EQ(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		(void*)(0xBCDE),  /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	/* Give the host instance a moment to settle. */
	Sleep(1000);
	
	testing = false;
	
	EXPECT_EQ(host_seq, 3);
	EXPECT_EQ(p1_seq, 3);
	
	EXPECT_EQ(p1_cp_dpnidPlayer, p1_player_id);
	EXPECT_EQ(p1_cc_dpnidLocal,  p1_player_id);
}

TEST(DirectPlay8Peer, ConnectTwoPeersToHost)
{
	std::atomic<bool> testing(true);
	
	std::atomic<int> host_seq(0), p1_seq(0), p2_seq(0);
	DPNID host_player_id = -1, p1_player_id = -1, p2_player_id = -1;
	
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[&testing, &host_seq, &host_player_id, &p1_player_id, &p2_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				case 1:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						host_player_id = cp->dpnidPlayer;
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0xB00));
						
						cp->pvPlayerContext = (void*)(0xB00B00);
					}
					
					break;
					
				case 2:
					EXPECT_EQ(dwMessageType, DPN_MSGID_INDICATE_CONNECT);
					
					if(dwMessageType == DPN_MSGID_INDICATE_CONNECT)
					{
						DPNMSG_INDICATE_CONNECT *ic = (DPNMSG_INDICATE_CONNECT*)(pMessage);
						
						EXPECT_EQ(ic->dwSize, sizeof(DPNMSG_INDICATE_CONNECT));
						
						EXPECT_EQ(ic->pvUserConnectData,     (void*)(NULL));
						EXPECT_EQ(ic->dwUserConnectDataSize, 0);
						
						EXPECT_EQ(ic->pvReplyData,     (void*)(NULL));
						EXPECT_EQ(ic->dwReplyDataSize, 0);
						
						EXPECT_EQ(ic->pvReplyContext,  (void*)(NULL));
						EXPECT_EQ(ic->pvPlayerContext, (void*)(NULL));
						
						/* TODO: Check pAddressPlayer, pAddressDevice */
						
						ic->pvPlayerContext = (void*)(0xB441);
					}
					
					break;
					
				case 3:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						p1_player_id = cp->dpnidPlayer;
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0xB441));
						
						cp->pvPlayerContext = (void*)(0xFEED);
					}
					
					break;
					
				case 4:
					EXPECT_EQ(dwMessageType, DPN_MSGID_INDICATE_CONNECT);
					
					if(dwMessageType == DPN_MSGID_INDICATE_CONNECT)
					{
						DPNMSG_INDICATE_CONNECT *ic = (DPNMSG_INDICATE_CONNECT*)(pMessage);
						
						EXPECT_EQ(ic->dwSize, sizeof(DPNMSG_INDICATE_CONNECT));
						
						EXPECT_EQ(ic->pvUserConnectData,     (void*)(NULL));
						EXPECT_EQ(ic->dwUserConnectDataSize, 0);
						
						EXPECT_EQ(ic->pvReplyData,     (void*)(NULL));
						EXPECT_EQ(ic->dwReplyDataSize, 0);
						
						EXPECT_EQ(ic->pvReplyContext,  (void*)(NULL));
						EXPECT_EQ(ic->pvPlayerContext, (void*)(NULL));
						
						/* TODO: Check pAddressPlayer, pAddressDevice */
						
						ic->pvPlayerContext = (void*)(0xB442);
					}
					
					break;
					
				case 5:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						p2_player_id = cp->dpnidPlayer;
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0xB442));
						
						cp->pvPlayerContext = (void*)(0xFEEE);
					}
					
					break;
					
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		});
	
	Sleep(1000);
	
	DPNID p1_cp1_dpnidPlayer = -1, p1_cc_dpnidLocal = -1, p1_cp2_dpnidPlayer = -1;
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&testing, &p1_seq, &host_player_id, &p1_player_id, &p1_cp1_dpnidPlayer, &p1_cc_dpnidLocal, &p1_cp2_dpnidPlayer]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++p1_seq;
			
			switch(seq)
			{
				case 1:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						p1_cp1_dpnidPlayer = cp->dpnidPlayer;
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0xBCDE));
						
						cp->pvPlayerContext = (void*)(0xCDEF);
					}
					
					break;
					
				case 2:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->dpnidPlayer,     host_player_id);
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0));
						
						cp->pvPlayerContext = (void*)(0xBAA);
					}
					
					break;
					
				case 3:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CONNECT_COMPLETE);
					
					if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
					{
						DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
						
						EXPECT_EQ(cc->dwSize,      sizeof(DPNMSG_CONNECT_COMPLETE));
						EXPECT_EQ(cc->hAsyncOp,    0);
						EXPECT_EQ(cc->hResultCode, S_OK);
						
						EXPECT_EQ(cc->pvApplicationReplyData,     (PVOID)(NULL));
						EXPECT_EQ(cc->dwApplicationReplyDataSize, 0);
						
						p1_cc_dpnidLocal = cc->dpnidLocal;
					}
					
					break;
					
				case 4:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						p1_cp2_dpnidPlayer = cp->dpnidPlayer;
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0));
						
						cp->pvPlayerContext = (void*)(0xBAB);
					}
					
					break;
					
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	EXPECT_EQ(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		(void*)(0xBCDE),  /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	Sleep(1000);
	
	DPNID p2_cp1_dpnidPlayer = -1, p2_cc_dpnidLocal = -1, p2_cp2_dpnidPlayer = -1;
	
	std::function<HRESULT(DWORD,PVOID)> p2_cb =
		[&testing, &p2_seq, &host_player_id, &p2_cp1_dpnidPlayer, &p2_cc_dpnidLocal, &p2_cp2_dpnidPlayer]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++p2_seq;
			
			switch(seq)
			{
				case 1:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						p2_cp1_dpnidPlayer = cp->dpnidPlayer;
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0xCDEF));
						
						cp->pvPlayerContext = (void*)(0xCDEF);
					}
					
					break;
					
				case 2:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->dpnidPlayer,     host_player_id);
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0));
						
						cp->pvPlayerContext = (void*)(0xBAA);
					}
					
					break;
					
				case 3:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CREATE_PLAYER);
					
					if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
					{
						DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
						p2_cp2_dpnidPlayer = cp->dpnidPlayer;
						
						EXPECT_EQ(cp->dwSize,          sizeof(DPNMSG_CREATE_PLAYER));
						EXPECT_EQ(cp->pvPlayerContext, (void*)(0));
						
						cp->pvPlayerContext = (void*)(0xBAB);
					}
					
					break;
					
				case 4:
					EXPECT_EQ(dwMessageType, DPN_MSGID_CONNECT_COMPLETE);
					
					if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
					{
						DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
						
						EXPECT_EQ(cc->dwSize,      sizeof(DPNMSG_CONNECT_COMPLETE));
						EXPECT_EQ(cc->hAsyncOp,    0);
						EXPECT_EQ(cc->hResultCode, S_OK);
						
						EXPECT_EQ(cc->pvApplicationReplyData,     (PVOID)(NULL));
						EXPECT_EQ(cc->dwApplicationReplyDataSize, 0);
						
						p2_cc_dpnidLocal = cc->dpnidLocal;
					}
					
					break;
					
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p2;
	
	ASSERT_EQ(p2->Initialize(&p2_cb, &callback_shim, 0), S_OK);
	
	EXPECT_EQ(p2->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		(void*)(0xCDEF),  /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	Sleep(1000);
	
	testing = false;
	
	EXPECT_EQ(host_seq, 5);
	EXPECT_EQ(p1_seq, 4);
	EXPECT_EQ(p2_seq, 4);
	
	EXPECT_EQ(p1_cp1_dpnidPlayer, p1_player_id);
	EXPECT_EQ(p1_cc_dpnidLocal,   p1_player_id);
	EXPECT_EQ(p1_cp2_dpnidPlayer, p2_player_id);
	
	EXPECT_EQ(p2_cp1_dpnidPlayer, p2_player_id);
	EXPECT_EQ(p2_cc_dpnidLocal,   p2_player_id);
	EXPECT_EQ(p2_cp2_dpnidPlayer, p1_player_id);
}

TEST(DirectPlay8Peer, HostPeerSoftClose)
{
	DPN_APPLICATION_DESC app_desc;
	memset(&app_desc, 0, sizeof(app_desc));
	
	app_desc.dwSize          = sizeof(app_desc);
	app_desc.guidApplication = APP_GUID_1;
	app_desc.pwszSessionName = L"Session 1";
	
	IDP8AddressInstance host_addr(CLSID_DP8SP_TCPIP, PORT);
	
	TestPeer host("host");
	ASSERT_EQ(host->Host(&app_desc, &(host_addr.instance), 1, NULL, NULL, 0, 0), S_OK);
	
	IDP8AddressInstance connect_addr(CLSID_DP8SP_TCPIP, L"127.0.0.1", PORT);
	
	TestPeer peer1("peer1");
	ASSERT_EQ(peer1->Connect(
		&app_desc,        /* pdnAppDesc */
		connect_addr,     /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		0,                /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	TestPeer peer2("peer2");
	ASSERT_EQ(peer2->Connect(
		&app_desc,        /* pdnAppDesc */
		connect_addr,     /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		0,                /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	Sleep(100);
	
	DPNID host_dp1_dpnidPlayer;
	DPNID host_dp2_dpnidPlayer;
	
	host.expect_begin();
	host.expect_push([&host, &peer1, &peer2](DWORD dwMessageType, PVOID pMessage)
	{
		EXPECT_EQ(dwMessageType, DPN_MSGID_DESTROY_PLAYER);
		
		if(dwMessageType == DPN_MSGID_DESTROY_PLAYER)
		{
			DPNMSG_DESTROY_PLAYER *dp = (DPNMSG_DESTROY_PLAYER*)(pMessage);
			
			EXPECT_EQ(dp->dwSize, sizeof(DPNMSG_DESTROY_PLAYER));
			
			EXPECT_TRUE((dp->dpnidPlayer == host.first_cp_dpnidPlayer))
				<< "(dpnidPlayer = " << dp->dpnidPlayer
				<< ", host = " << host.first_cp_dpnidPlayer
				<< ", peer1 = " << peer1.first_cc_dpnidLocal
				<< ", peer2 = " << peer2.first_cc_dpnidLocal << ")";
			
			EXPECT_EQ(dp->pvPlayerContext, (void*)~(uintptr_t)(dp->dpnidPlayer));
			EXPECT_EQ(dp->dwReason,        DPNDESTROYPLAYERREASON_NORMAL);
		}
		
		return DPN_OK;
	});
	host.expect_push([&host, &peer1, &peer2, &host_dp1_dpnidPlayer](DWORD dwMessageType, PVOID pMessage)
	{
		EXPECT_EQ(dwMessageType, DPN_MSGID_DESTROY_PLAYER);
		
		if(dwMessageType == DPN_MSGID_DESTROY_PLAYER)
		{
			DPNMSG_DESTROY_PLAYER *dp = (DPNMSG_DESTROY_PLAYER*)(pMessage);
			
			EXPECT_EQ(dp->dwSize, sizeof(DPNMSG_DESTROY_PLAYER));
			
			EXPECT_TRUE((dp->dpnidPlayer == peer1.first_cc_dpnidLocal || dp->dpnidPlayer == peer2.first_cc_dpnidLocal))
				<< "(dpnidPlayer = " << dp->dpnidPlayer
				<< ", host = " << host.first_cp_dpnidPlayer
				<< ", peer1 = " << peer1.first_cc_dpnidLocal
				<< ", peer2 = " << peer2.first_cc_dpnidLocal << ")";
			
			EXPECT_EQ(dp->pvPlayerContext, (void*)~(uintptr_t)(dp->dpnidPlayer));
			EXPECT_EQ(dp->dwReason,        DPNDESTROYPLAYERREASON_NORMAL);
			
			host_dp1_dpnidPlayer = dp->dpnidPlayer;
		}
		
		return DPN_OK;
	});
	host.expect_push([&host, &peer1, &peer2, &host_dp2_dpnidPlayer](DWORD dwMessageType, PVOID pMessage)
	{
		EXPECT_EQ(dwMessageType, DPN_MSGID_DESTROY_PLAYER);
		
		if(dwMessageType == DPN_MSGID_DESTROY_PLAYER)
		{
			DPNMSG_DESTROY_PLAYER *dp = (DPNMSG_DESTROY_PLAYER*)(pMessage);
			
			EXPECT_EQ(dp->dwSize, sizeof(DPNMSG_DESTROY_PLAYER));
			
			EXPECT_TRUE((dp->dpnidPlayer == peer1.first_cc_dpnidLocal || dp->dpnidPlayer == peer2.first_cc_dpnidLocal))
				<< "(dpnidPlayer = " << dp->dpnidPlayer
				<< ", host = " << host.first_cp_dpnidPlayer
				<< ", peer1 = " << peer1.first_cc_dpnidLocal
				<< ", peer2 = " << peer2.first_cc_dpnidLocal << ")";
			
			EXPECT_EQ(dp->pvPlayerContext, (void*)~(uintptr_t)(dp->dpnidPlayer));
			EXPECT_EQ(dp->dwReason,        DPNDESTROYPLAYERREASON_NORMAL);
			
			host_dp2_dpnidPlayer = dp->dpnidPlayer;
		}
		
		return DPN_OK;
	});
	
	std::set<DPNID> p1_dp_dpnidPlayer;
	bool p1_ts = false;
	
	peer1.expect_begin();
	peer1.expect_push([&host, &peer1, &peer2, &p1_dp_dpnidPlayer, &p1_ts](DWORD dwMessageType, PVOID pMessage)
	{
		if(dwMessageType == DPN_MSGID_DESTROY_PLAYER)
		{
			DPNMSG_DESTROY_PLAYER *dp = (DPNMSG_DESTROY_PLAYER*)(pMessage);
			
			EXPECT_EQ(dp->dwSize, sizeof(DPNMSG_DESTROY_PLAYER));
			
			EXPECT_TRUE((dp->dpnidPlayer == host.first_cp_dpnidPlayer || dp->dpnidPlayer == peer1.first_cc_dpnidLocal || dp->dpnidPlayer == peer2.first_cc_dpnidLocal))
				<< "(dpnidPlayer = " << dp->dpnidPlayer
				<< ", host = " << host.first_cp_dpnidPlayer
				<< ", peer1 = " << peer1.first_cc_dpnidLocal
				<< ", peer2 = " << peer2.first_cc_dpnidLocal << ")";
			
			EXPECT_EQ(dp->pvPlayerContext, (void*)~(uintptr_t)(dp->dpnidPlayer));
			
			if(dp->dpnidPlayer == host.first_cp_dpnidPlayer)
			{
				EXPECT_EQ(dp->dwReason, DPNDESTROYPLAYERREASON_NORMAL);
			}
			else{
				EXPECT_TRUE((dp->dwReason == DPNDESTROYPLAYERREASON_NORMAL || dp->dwReason == DPNDESTROYPLAYERREASON_CONNECTIONLOST))
					<< "dwReason = " << dp->dwReason;
			}
			
			p1_dp_dpnidPlayer.insert(dp->dpnidPlayer);
		}
		else if(dwMessageType == DPN_MSGID_TERMINATE_SESSION)
		{
			DPNMSG_TERMINATE_SESSION *ts = (DPNMSG_TERMINATE_SESSION*)(pMessage);
			
			EXPECT_EQ(ts->dwSize,              sizeof(DPNMSG_TERMINATE_SESSION));
			EXPECT_EQ(ts->hResultCode,         DPNERR_CONNECTIONLOST);
			EXPECT_EQ(ts->pvTerminateData,     (void*)(NULL));
			EXPECT_EQ(ts->dwTerminateDataSize, 0);
			
			p1_ts = true;
		}
		else{
			ADD_FAILURE() << "Unexpected message type: " << dwMessageType;
		}
		
		return DPN_OK;
	}, 4);
	
	std::set<DPNID> p2_dp_dpnidPlayer;
	bool p2_ts = false;
	
	peer2.expect_begin();
	peer2.expect_push([&host, &peer1, &peer2, &p2_dp_dpnidPlayer, &p2_ts](DWORD dwMessageType, PVOID pMessage)
	{
		if(dwMessageType == DPN_MSGID_DESTROY_PLAYER)
		{
			DPNMSG_DESTROY_PLAYER *dp = (DPNMSG_DESTROY_PLAYER*)(pMessage);
			
			EXPECT_EQ(dp->dwSize, sizeof(DPNMSG_DESTROY_PLAYER));
			
			EXPECT_TRUE((dp->dpnidPlayer == host.first_cp_dpnidPlayer || dp->dpnidPlayer == peer1.first_cc_dpnidLocal || dp->dpnidPlayer == peer2.first_cc_dpnidLocal))
				<< "(dpnidPlayer = " << dp->dpnidPlayer
				<< ", host = " << host.first_cp_dpnidPlayer
				<< ", peer1 = " << peer1.first_cc_dpnidLocal
				<< ", peer2 = " << peer2.first_cc_dpnidLocal << ")";
			
			EXPECT_EQ(dp->pvPlayerContext, (void*)~(uintptr_t)(dp->dpnidPlayer));
			
			if(dp->dpnidPlayer == host.first_cp_dpnidPlayer)
			{
				EXPECT_EQ(dp->dwReason, DPNDESTROYPLAYERREASON_NORMAL);
			}
			else{
				EXPECT_TRUE((dp->dwReason == DPNDESTROYPLAYERREASON_NORMAL || dp->dwReason == DPNDESTROYPLAYERREASON_CONNECTIONLOST))
					<< "dwReason = " << dp->dwReason;
			}
			
			p2_dp_dpnidPlayer.insert(dp->dpnidPlayer);
		}
		else if(dwMessageType == DPN_MSGID_TERMINATE_SESSION)
		{
			DPNMSG_TERMINATE_SESSION *ts = (DPNMSG_TERMINATE_SESSION*)(pMessage);
			
			EXPECT_EQ(ts->dwSize,              sizeof(DPNMSG_TERMINATE_SESSION));
			EXPECT_EQ(ts->hResultCode,         DPNERR_CONNECTIONLOST);
			EXPECT_EQ(ts->pvTerminateData,     (void*)(NULL));
			EXPECT_EQ(ts->dwTerminateDataSize, 0);
			
			p2_ts = true;
		}
		else{
			ADD_FAILURE() << "Unexpected message type: " << dwMessageType;
		}
		
		return DPN_OK;
	}, 4);
	
	host->Close(0);
	
	Sleep(100);
	
	peer2.expect_end();
	peer1.expect_end();
	host.expect_end();
	
	EXPECT_NE(host_dp1_dpnidPlayer, host_dp2_dpnidPlayer);
	
	std::set<DPNID> all_players;
	all_players.insert(host.first_cp_dpnidPlayer);
	all_players.insert(peer1.first_cc_dpnidLocal);
	all_players.insert(peer2.first_cc_dpnidLocal);
	
	EXPECT_EQ(p1_dp_dpnidPlayer, all_players);
	EXPECT_TRUE(p1_ts);
	
	EXPECT_EQ(p2_dp_dpnidPlayer, all_players);
	EXPECT_TRUE(p2_ts);
}

TEST(DirectPlay8Peer, HostPeerHardClose)
{
	DPN_APPLICATION_DESC app_desc;
	memset(&app_desc, 0, sizeof(app_desc));
	
	app_desc.dwSize          = sizeof(app_desc);
	app_desc.guidApplication = APP_GUID_1;
	app_desc.pwszSessionName = L"Session 1";
	
	IDP8AddressInstance host_addr(CLSID_DP8SP_TCPIP, PORT);
	
	TestPeer host("host");
	ASSERT_EQ(host->Host(&app_desc, &(host_addr.instance), 1, NULL, NULL, 0, 0), S_OK);
	
	IDP8AddressInstance connect_addr(CLSID_DP8SP_TCPIP, L"127.0.0.1", PORT);
	
	TestPeer peer1("peer1");
	ASSERT_EQ(peer1->Connect(
		&app_desc,        /* pdnAppDesc */
		connect_addr,     /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		0,                /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	TestPeer peer2("peer2");
	ASSERT_EQ(peer2->Connect(
		&app_desc,        /* pdnAppDesc */
		connect_addr,     /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		0,                /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	Sleep(100);
	
	DPNID host_dp1_dpnidPlayer;
	DPNID host_dp2_dpnidPlayer;
	
	host.expect_begin();
	host.expect_push([&host, &peer1, &peer2](DWORD dwMessageType, PVOID pMessage)
	{
		EXPECT_EQ(dwMessageType, DPN_MSGID_DESTROY_PLAYER);
		
		if(dwMessageType == DPN_MSGID_DESTROY_PLAYER)
		{
			DPNMSG_DESTROY_PLAYER *dp = (DPNMSG_DESTROY_PLAYER*)(pMessage);
			
			EXPECT_EQ(dp->dwSize, sizeof(DPNMSG_DESTROY_PLAYER));
			
			EXPECT_TRUE((dp->dpnidPlayer == host.first_cp_dpnidPlayer))
				<< "(dpnidPlayer = " << dp->dpnidPlayer
				<< ", host = " << host.first_cp_dpnidPlayer
				<< ", peer1 = " << peer1.first_cc_dpnidLocal
				<< ", peer2 = " << peer2.first_cc_dpnidLocal << ")";
			
			EXPECT_EQ(dp->pvPlayerContext, (void*)~(uintptr_t)(dp->dpnidPlayer));
			EXPECT_EQ(dp->dwReason,        DPNDESTROYPLAYERREASON_NORMAL);
		}
		
		return DPN_OK;
	});
	host.expect_push([&host, &peer1, &peer2, &host_dp1_dpnidPlayer](DWORD dwMessageType, PVOID pMessage)
	{
		EXPECT_EQ(dwMessageType, DPN_MSGID_DESTROY_PLAYER);
		
		if(dwMessageType == DPN_MSGID_DESTROY_PLAYER)
		{
			DPNMSG_DESTROY_PLAYER *dp = (DPNMSG_DESTROY_PLAYER*)(pMessage);
			
			EXPECT_EQ(dp->dwSize, sizeof(DPNMSG_DESTROY_PLAYER));
			
			EXPECT_TRUE((dp->dpnidPlayer == peer1.first_cc_dpnidLocal || dp->dpnidPlayer == peer2.first_cc_dpnidLocal))
				<< "(dpnidPlayer = " << dp->dpnidPlayer
				<< ", host = " << host.first_cp_dpnidPlayer
				<< ", peer1 = " << peer1.first_cc_dpnidLocal
				<< ", peer2 = " << peer2.first_cc_dpnidLocal << ")";
			
			EXPECT_EQ(dp->pvPlayerContext, (void*)~(uintptr_t)(dp->dpnidPlayer));
			EXPECT_EQ(dp->dwReason,        DPNDESTROYPLAYERREASON_NORMAL);
			
			host_dp1_dpnidPlayer = dp->dpnidPlayer;
		}
		
		return DPN_OK;
	});
	host.expect_push([&host, &peer1, &peer2, &host_dp2_dpnidPlayer](DWORD dwMessageType, PVOID pMessage)
	{
		EXPECT_EQ(dwMessageType, DPN_MSGID_DESTROY_PLAYER);
		
		if(dwMessageType == DPN_MSGID_DESTROY_PLAYER)
		{
			DPNMSG_DESTROY_PLAYER *dp = (DPNMSG_DESTROY_PLAYER*)(pMessage);
			
			EXPECT_EQ(dp->dwSize, sizeof(DPNMSG_DESTROY_PLAYER));
			
			EXPECT_TRUE((dp->dpnidPlayer == peer1.first_cc_dpnidLocal || dp->dpnidPlayer == peer2.first_cc_dpnidLocal))
				<< "(dpnidPlayer = " << dp->dpnidPlayer
				<< ", host = " << host.first_cp_dpnidPlayer
				<< ", peer1 = " << peer1.first_cc_dpnidLocal
				<< ", peer2 = " << peer2.first_cc_dpnidLocal << ")";
			
			EXPECT_EQ(dp->pvPlayerContext, (void*)~(uintptr_t)(dp->dpnidPlayer));
			EXPECT_EQ(dp->dwReason,        DPNDESTROYPLAYERREASON_NORMAL);
			
			host_dp2_dpnidPlayer = dp->dpnidPlayer;
		}
		
		return DPN_OK;
	});
	
	std::set<DPNID> p1_dp_dpnidPlayer;
	bool p1_ts = false;
	
	peer1.expect_begin();
	peer1.expect_push([&host, &peer1, &peer2, &p1_dp_dpnidPlayer, &p1_ts](DWORD dwMessageType, PVOID pMessage)
	{
		if(dwMessageType == DPN_MSGID_DESTROY_PLAYER)
		{
			DPNMSG_DESTROY_PLAYER *dp = (DPNMSG_DESTROY_PLAYER*)(pMessage);
			
			EXPECT_EQ(dp->dwSize, sizeof(DPNMSG_DESTROY_PLAYER));
			
			EXPECT_TRUE((dp->dpnidPlayer == host.first_cp_dpnidPlayer || dp->dpnidPlayer == peer1.first_cc_dpnidLocal || dp->dpnidPlayer == peer2.first_cc_dpnidLocal))
				<< "(dpnidPlayer = " << dp->dpnidPlayer
				<< ", host = " << host.first_cp_dpnidPlayer
				<< ", peer1 = " << peer1.first_cc_dpnidLocal
				<< ", peer2 = " << peer2.first_cc_dpnidLocal << ")";
			
			EXPECT_EQ(dp->pvPlayerContext, (void*)~(uintptr_t)(dp->dpnidPlayer));
			
			if(dp->dpnidPlayer == host.first_cp_dpnidPlayer)
			{
				EXPECT_EQ(dp->dwReason, DPNDESTROYPLAYERREASON_CONNECTIONLOST);
			}
			else{
				EXPECT_TRUE((dp->dwReason == DPNDESTROYPLAYERREASON_NORMAL || dp->dwReason == DPNDESTROYPLAYERREASON_CONNECTIONLOST))
					<< "dwReason = " << dp->dwReason;
			}
			
			p1_dp_dpnidPlayer.insert(dp->dpnidPlayer);
		}
		else if(dwMessageType == DPN_MSGID_TERMINATE_SESSION)
		{
			DPNMSG_TERMINATE_SESSION *ts = (DPNMSG_TERMINATE_SESSION*)(pMessage);
			
			EXPECT_EQ(ts->dwSize,              sizeof(DPNMSG_TERMINATE_SESSION));
			EXPECT_EQ(ts->hResultCode,         DPNERR_CONNECTIONLOST);
			EXPECT_EQ(ts->pvTerminateData,     (void*)(NULL));
			EXPECT_EQ(ts->dwTerminateDataSize, 0);
			
			p1_ts = true;
		}
		else{
			ADD_FAILURE() << "Unexpected message type: " << dwMessageType;
		}
		
		return DPN_OK;
	}, 4);
	
	std::set<DPNID> p2_dp_dpnidPlayer;
	bool p2_ts = false;
	
	peer2.expect_begin();
	peer2.expect_push([&host, &peer1, &peer2, &p2_dp_dpnidPlayer, &p2_ts](DWORD dwMessageType, PVOID pMessage)
	{
		if(dwMessageType == DPN_MSGID_DESTROY_PLAYER)
		{
			DPNMSG_DESTROY_PLAYER *dp = (DPNMSG_DESTROY_PLAYER*)(pMessage);
			
			EXPECT_EQ(dp->dwSize, sizeof(DPNMSG_DESTROY_PLAYER));
			
			EXPECT_TRUE((dp->dpnidPlayer == host.first_cp_dpnidPlayer || dp->dpnidPlayer == peer1.first_cc_dpnidLocal || dp->dpnidPlayer == peer2.first_cc_dpnidLocal))
				<< "(dpnidPlayer = " << dp->dpnidPlayer
				<< ", host = " << host.first_cp_dpnidPlayer
				<< ", peer1 = " << peer1.first_cc_dpnidLocal
				<< ", peer2 = " << peer2.first_cc_dpnidLocal << ")";
			
			EXPECT_EQ(dp->pvPlayerContext, (void*)~(uintptr_t)(dp->dpnidPlayer));
			
			if(dp->dpnidPlayer == host.first_cp_dpnidPlayer)
			{
				EXPECT_EQ(dp->dwReason, DPNDESTROYPLAYERREASON_CONNECTIONLOST);
			}
			else{
				EXPECT_TRUE((dp->dwReason == DPNDESTROYPLAYERREASON_NORMAL || dp->dwReason == DPNDESTROYPLAYERREASON_CONNECTIONLOST))
					<< "dwReason = " << dp->dwReason;
			}
			
			p2_dp_dpnidPlayer.insert(dp->dpnidPlayer);
		}
		else if(dwMessageType == DPN_MSGID_TERMINATE_SESSION)
		{
			DPNMSG_TERMINATE_SESSION *ts = (DPNMSG_TERMINATE_SESSION*)(pMessage);
			
			EXPECT_EQ(ts->dwSize,              sizeof(DPNMSG_TERMINATE_SESSION));
			EXPECT_EQ(ts->hResultCode,         DPNERR_CONNECTIONLOST);
			EXPECT_EQ(ts->pvTerminateData,     (void*)(NULL));
			EXPECT_EQ(ts->dwTerminateDataSize, 0);
			
			p2_ts = true;
		}
		else{
			ADD_FAILURE() << "Unexpected message type: " << dwMessageType;
		}
		
		return DPN_OK;
	}, 4);
	
	host->Close(DPNCLOSE_IMMEDIATE);
	
	Sleep(100);
	
	peer2.expect_end();
	peer1.expect_end();
	host.expect_end();
	
	EXPECT_NE(host_dp1_dpnidPlayer, host_dp2_dpnidPlayer);
	
	std::set<DPNID> all_players;
	all_players.insert(host.first_cp_dpnidPlayer);
	all_players.insert(peer1.first_cc_dpnidLocal);
	all_players.insert(peer2.first_cc_dpnidLocal);
	
	EXPECT_EQ(p1_dp_dpnidPlayer, all_players);
	EXPECT_TRUE(p1_ts);
	
	EXPECT_EQ(p2_dp_dpnidPlayer, all_players);
	EXPECT_TRUE(p2_ts);
}

TEST(DirectPlay8Peer, NonHostPeerSoftClose)
{
	DPN_APPLICATION_DESC app_desc;
	memset(&app_desc, 0, sizeof(app_desc));
	
	app_desc.dwSize          = sizeof(app_desc);
	app_desc.guidApplication = APP_GUID_1;
	app_desc.pwszSessionName = L"Session 1";
	
	IDP8AddressInstance host_addr(CLSID_DP8SP_TCPIP, PORT);
	
	TestPeer host("host");
	ASSERT_EQ(host->Host(&app_desc, &(host_addr.instance), 1, NULL, NULL, 0, 0), S_OK);
	
	IDP8AddressInstance connect_addr(CLSID_DP8SP_TCPIP, L"127.0.0.1", PORT);
	
	TestPeer peer1("peer1");
	ASSERT_EQ(peer1->Connect(
		&app_desc,        /* pdnAppDesc */
		connect_addr,     /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		0,                /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	TestPeer peer2("peer2");
	ASSERT_EQ(peer2->Connect(
		&app_desc,        /* pdnAppDesc */
		connect_addr,     /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		0,                /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	Sleep(100);
	
	host.expect_begin();
	host.expect_push([&peer2](DWORD dwMessageType, PVOID pMessage)
	{
		EXPECT_EQ(dwMessageType, DPN_MSGID_DESTROY_PLAYER);
		
		if(dwMessageType == DPN_MSGID_DESTROY_PLAYER)
		{
			DPNMSG_DESTROY_PLAYER *dp = (DPNMSG_DESTROY_PLAYER*)(pMessage);
			
			EXPECT_EQ(dp->dwSize,          sizeof(DPNMSG_DESTROY_PLAYER));
			EXPECT_EQ(dp->dpnidPlayer,     peer2.first_cc_dpnidLocal);
			EXPECT_EQ(dp->pvPlayerContext, (void*)~(uintptr_t)(dp->dpnidPlayer));
			EXPECT_EQ(dp->dwReason,        DPNDESTROYPLAYERREASON_NORMAL);
		}
		
		return DPN_OK;
	});
	
	peer1.expect_begin();
	peer1.expect_push([&peer2](DWORD dwMessageType, PVOID pMessage)
	{
		EXPECT_EQ(dwMessageType, DPN_MSGID_DESTROY_PLAYER);
		
		if(dwMessageType == DPN_MSGID_DESTROY_PLAYER)
		{
			DPNMSG_DESTROY_PLAYER *dp = (DPNMSG_DESTROY_PLAYER*)(pMessage);
			
			EXPECT_EQ(dp->dwSize,          sizeof(DPNMSG_DESTROY_PLAYER));
			EXPECT_EQ(dp->dpnidPlayer,     peer2.first_cc_dpnidLocal);
			EXPECT_EQ(dp->pvPlayerContext, (void*)~(uintptr_t)(dp->dpnidPlayer));
			EXPECT_EQ(dp->dwReason,        DPNDESTROYPLAYERREASON_NORMAL);
		}
		
		return DPN_OK;
	});
	
	DPNID p2_dp1_dpnidPlayer;
	DPNID p2_dp2_dpnidPlayer;
	
	peer2.expect_begin();
	peer2.expect_push([&host, &peer1, &peer2, &p2_dp1_dpnidPlayer](DWORD dwMessageType, PVOID pMessage)
	{
		EXPECT_EQ(dwMessageType, DPN_MSGID_DESTROY_PLAYER);
		
		if(dwMessageType == DPN_MSGID_DESTROY_PLAYER)
		{
			DPNMSG_DESTROY_PLAYER *dp = (DPNMSG_DESTROY_PLAYER*)(pMessage);
			
			EXPECT_EQ(dp->dwSize, sizeof(DPNMSG_DESTROY_PLAYER));
			
			EXPECT_TRUE((dp->dpnidPlayer == host.first_cp_dpnidPlayer || dp->dpnidPlayer == peer1.first_cc_dpnidLocal))
				<< "(dpnidPlayer = " << dp->dpnidPlayer
				<< ", host = " << host.first_cp_dpnidPlayer
				<< ", peer1 = " << peer1.first_cc_dpnidLocal
				<< ", peer2 = " << peer2.first_cc_dpnidLocal << ")";
			
			EXPECT_EQ(dp->pvPlayerContext, (void*)~(uintptr_t)(dp->dpnidPlayer));
			EXPECT_EQ(dp->dwReason,        DPNDESTROYPLAYERREASON_NORMAL);
			
			p2_dp1_dpnidPlayer = dp->dpnidPlayer;
		}
		
		return DPN_OK;
	});
	peer2.expect_push([&host, &peer1, &peer2, &p2_dp2_dpnidPlayer](DWORD dwMessageType, PVOID pMessage)
	{
		EXPECT_EQ(dwMessageType, DPN_MSGID_DESTROY_PLAYER);
		
		if(dwMessageType == DPN_MSGID_DESTROY_PLAYER)
		{
			DPNMSG_DESTROY_PLAYER *dp = (DPNMSG_DESTROY_PLAYER*)(pMessage);
			
			EXPECT_EQ(dp->dwSize, sizeof(DPNMSG_DESTROY_PLAYER));
			
			EXPECT_TRUE((dp->dpnidPlayer == host.first_cp_dpnidPlayer || dp->dpnidPlayer == peer1.first_cc_dpnidLocal))
				<< "(dpnidPlayer = " << dp->dpnidPlayer
				<< ", host = " << host.first_cp_dpnidPlayer
				<< ", peer1 = " << peer1.first_cc_dpnidLocal
				<< ", peer2 = " << peer2.first_cc_dpnidLocal << ")";
			
			EXPECT_EQ(dp->pvPlayerContext, (void*)~(uintptr_t)(dp->dpnidPlayer));
			EXPECT_EQ(dp->dwReason,        DPNDESTROYPLAYERREASON_NORMAL);
			
			p2_dp2_dpnidPlayer = dp->dpnidPlayer;
		}
		
		return DPN_OK;
	});
	peer2.expect_push([&host, &peer1, &peer2](DWORD dwMessageType, PVOID pMessage)
	{
		EXPECT_EQ(dwMessageType, DPN_MSGID_DESTROY_PLAYER);
		
		if(dwMessageType == DPN_MSGID_DESTROY_PLAYER)
		{
			DPNMSG_DESTROY_PLAYER *dp = (DPNMSG_DESTROY_PLAYER*)(pMessage);
			
			EXPECT_EQ(dp->dwSize, sizeof(DPNMSG_DESTROY_PLAYER));
			
			EXPECT_TRUE((dp->dpnidPlayer == peer2.first_cc_dpnidLocal))
				<< "(dpnidPlayer = " << dp->dpnidPlayer
				<< ", host = " << host.first_cp_dpnidPlayer
				<< ", peer1 = " << peer1.first_cc_dpnidLocal
				<< ", peer2 = " << peer2.first_cc_dpnidLocal << ")";
			
			EXPECT_EQ(dp->pvPlayerContext, (void*)~(uintptr_t)(dp->dpnidPlayer));
			EXPECT_EQ(dp->dwReason,        DPNDESTROYPLAYERREASON_NORMAL);
		}
		
		return DPN_OK;
	});
	
	peer2->Close(0);
	
	Sleep(100);
	
	peer2.expect_end();
	peer1.expect_end();
	host.expect_end();
	
	EXPECT_NE(p2_dp1_dpnidPlayer, p2_dp2_dpnidPlayer);
}

TEST(DirectPlay8Peer, NonHostPeerHardClose)
{
	DPN_APPLICATION_DESC app_desc;
	memset(&app_desc, 0, sizeof(app_desc));
	
	app_desc.dwSize          = sizeof(app_desc);
	app_desc.guidApplication = APP_GUID_1;
	app_desc.pwszSessionName = L"Session 1";
	
	IDP8AddressInstance host_addr(CLSID_DP8SP_TCPIP, PORT);
	
	TestPeer host("host");
	ASSERT_EQ(host->Host(&app_desc, &(host_addr.instance), 1, NULL, NULL, 0, 0), S_OK);
	
	IDP8AddressInstance connect_addr(CLSID_DP8SP_TCPIP, L"127.0.0.1", PORT);
	
	TestPeer peer1("peer1");
	ASSERT_EQ(peer1->Connect(
		&app_desc,        /* pdnAppDesc */
		connect_addr,     /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		0,                /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	TestPeer peer2("peer2");
	ASSERT_EQ(peer2->Connect(
		&app_desc,        /* pdnAppDesc */
		connect_addr,     /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		0,                /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	Sleep(100);
	
	host.expect_begin();
	host.expect_push([&peer2](DWORD dwMessageType, PVOID pMessage)
	{
		EXPECT_EQ(dwMessageType, DPN_MSGID_DESTROY_PLAYER);
		
		if(dwMessageType == DPN_MSGID_DESTROY_PLAYER)
		{
			DPNMSG_DESTROY_PLAYER *dp = (DPNMSG_DESTROY_PLAYER*)(pMessage);
			
			EXPECT_EQ(dp->dwSize,          sizeof(DPNMSG_DESTROY_PLAYER));
			EXPECT_EQ(dp->dpnidPlayer,     peer2.first_cc_dpnidLocal);
			EXPECT_EQ(dp->pvPlayerContext, (void*)~(uintptr_t)(dp->dpnidPlayer));
			EXPECT_EQ(dp->dwReason,        DPNDESTROYPLAYERREASON_CONNECTIONLOST);
		}
		
		return DPN_OK;
	});
	
	peer1.expect_begin();
	peer1.expect_push([&peer2](DWORD dwMessageType, PVOID pMessage)
	{
		EXPECT_EQ(dwMessageType, DPN_MSGID_DESTROY_PLAYER);
		
		if(dwMessageType == DPN_MSGID_DESTROY_PLAYER)
		{
			DPNMSG_DESTROY_PLAYER *dp = (DPNMSG_DESTROY_PLAYER*)(pMessage);
			
			EXPECT_EQ(dp->dwSize,          sizeof(DPNMSG_DESTROY_PLAYER));
			EXPECT_EQ(dp->dpnidPlayer,     peer2.first_cc_dpnidLocal);
			EXPECT_EQ(dp->pvPlayerContext, (void*)~(uintptr_t)(dp->dpnidPlayer));
			EXPECT_EQ(dp->dwReason,        DPNDESTROYPLAYERREASON_CONNECTIONLOST);
		}
		
		return DPN_OK;
	});
	
	DPNID p2_dp1_dpnidPlayer;
	DPNID p2_dp2_dpnidPlayer;
	
	peer2.expect_begin();
	peer2.expect_push([&host, &peer1, &peer2, &p2_dp1_dpnidPlayer](DWORD dwMessageType, PVOID pMessage)
	{
		EXPECT_EQ(dwMessageType, DPN_MSGID_DESTROY_PLAYER);
		
		if(dwMessageType == DPN_MSGID_DESTROY_PLAYER)
		{
			DPNMSG_DESTROY_PLAYER *dp = (DPNMSG_DESTROY_PLAYER*)(pMessage);
			
			EXPECT_EQ(dp->dwSize, sizeof(DPNMSG_DESTROY_PLAYER));
			
			EXPECT_TRUE((dp->dpnidPlayer == host.first_cp_dpnidPlayer || dp->dpnidPlayer == peer1.first_cc_dpnidLocal))
				<< "(dpnidPlayer = " << dp->dpnidPlayer
				<< ", host = " << host.first_cp_dpnidPlayer
				<< ", peer1 = " << peer1.first_cc_dpnidLocal
				<< ", peer2 = " << peer2.first_cc_dpnidLocal << ")";
			
			EXPECT_EQ(dp->pvPlayerContext, (void*)~(uintptr_t)(dp->dpnidPlayer));
			EXPECT_EQ(dp->dwReason,        DPNDESTROYPLAYERREASON_NORMAL);
			
			p2_dp1_dpnidPlayer = dp->dpnidPlayer;
		}
		
		return DPN_OK;
	});
	peer2.expect_push([&host, &peer1, &peer2, &p2_dp2_dpnidPlayer](DWORD dwMessageType, PVOID pMessage)
	{
		EXPECT_EQ(dwMessageType, DPN_MSGID_DESTROY_PLAYER);
		
		if(dwMessageType == DPN_MSGID_DESTROY_PLAYER)
		{
			DPNMSG_DESTROY_PLAYER *dp = (DPNMSG_DESTROY_PLAYER*)(pMessage);
			
			EXPECT_EQ(dp->dwSize, sizeof(DPNMSG_DESTROY_PLAYER));
			
			EXPECT_TRUE((dp->dpnidPlayer == host.first_cp_dpnidPlayer || dp->dpnidPlayer == peer1.first_cc_dpnidLocal))
				<< "(dpnidPlayer = " << dp->dpnidPlayer
				<< ", host = " << host.first_cp_dpnidPlayer
				<< ", peer1 = " << peer1.first_cc_dpnidLocal
				<< ", peer2 = " << peer2.first_cc_dpnidLocal << ")";
			
			EXPECT_EQ(dp->pvPlayerContext, (void*)~(uintptr_t)(dp->dpnidPlayer));
			EXPECT_EQ(dp->dwReason,        DPNDESTROYPLAYERREASON_NORMAL);
			
			p2_dp2_dpnidPlayer = dp->dpnidPlayer;
		}
		
		return DPN_OK;
	});
	peer2.expect_push([&host, &peer1, &peer2](DWORD dwMessageType, PVOID pMessage)
	{
		EXPECT_EQ(dwMessageType, DPN_MSGID_DESTROY_PLAYER);
		
		if(dwMessageType == DPN_MSGID_DESTROY_PLAYER)
		{
			DPNMSG_DESTROY_PLAYER *dp = (DPNMSG_DESTROY_PLAYER*)(pMessage);
			
			EXPECT_EQ(dp->dwSize, sizeof(DPNMSG_DESTROY_PLAYER));
			
			EXPECT_TRUE((dp->dpnidPlayer == peer2.first_cc_dpnidLocal))
				<< "(dpnidPlayer = " << dp->dpnidPlayer
				<< ", host = " << host.first_cp_dpnidPlayer
				<< ", peer1 = " << peer1.first_cc_dpnidLocal
				<< ", peer2 = " << peer2.first_cc_dpnidLocal << ")";
			
			EXPECT_EQ(dp->pvPlayerContext, (void*)~(uintptr_t)(dp->dpnidPlayer));
			EXPECT_EQ(dp->dwReason,        DPNDESTROYPLAYERREASON_NORMAL);
		}
		
		return DPN_OK;
	});
	
	peer2->Close(DPNCLOSE_IMMEDIATE);
	
	Sleep(100);
	
	peer2.expect_end();
	peer1.expect_end();
	host.expect_end();
	
	EXPECT_NE(p2_dp1_dpnidPlayer, p2_dp2_dpnidPlayer);
}

TEST(DirectPlay8Peer, GetApplicationDesc)
{
	const unsigned char APP_DATA[] = { 0x00, 0x01, 0x02, 0x03, 0x04 };
	
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[]
		(DWORD dwMessageType, PVOID pMessage)
		{
			return DPN_OK;
		},
		10,
		NULL,
		APP_DATA,
		sizeof(APP_DATA));
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[](DWORD dwMessageType, PVOID pMessage)
		{
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	ASSERT_EQ(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		NULL,             /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	/* Host GetApplicationDesc() */
	
	DWORD h_appdesc_size = 0;
	ASSERT_EQ(host->GetApplicationDesc(NULL, &h_appdesc_size, 0), DPNERR_BUFFERTOOSMALL);
	
	ASSERT_TRUE(h_appdesc_size >= sizeof(DPN_APPLICATION_DESC));
	
	std::vector<unsigned char> h_appdesc_buf(h_appdesc_size);
	DPN_APPLICATION_DESC *h_appdesc = (DPN_APPLICATION_DESC*)(h_appdesc_buf.data());
	
	memset(h_appdesc, 0xFF, h_appdesc_size);
	h_appdesc->dwSize = sizeof(DPN_APPLICATION_DESC);
	
	/* Instance GUID should be random, so we just check our initial value got overwritten. */
	GUID orig_guid = h_appdesc->guidInstance;
	
	DWORD h_appdesc_small = h_appdesc_size - 1;
	ASSERT_EQ(host->GetApplicationDesc(h_appdesc, &h_appdesc_small, 0), DPNERR_BUFFERTOOSMALL);
	
	ASSERT_EQ(host->GetApplicationDesc(h_appdesc, &h_appdesc_size, 0), S_OK);
	
	EXPECT_EQ((h_appdesc->dwFlags & DPNSESSION_REQUIREPASSWORD), 0);
	EXPECT_NE(h_appdesc->guidInstance, orig_guid);
	EXPECT_EQ(h_appdesc->guidApplication, APP_GUID_1);
	EXPECT_EQ(h_appdesc->dwMaxPlayers, 10);
	EXPECT_EQ(h_appdesc->dwCurrentPlayers, 2);
	EXPECT_EQ(std::wstring(h_appdesc->pwszSessionName), std::wstring(L"Session 1"));
	EXPECT_EQ(h_appdesc->pwszPassword, (WCHAR*)(NULL));
	
	EXPECT_EQ(std::string((const char*)(h_appdesc->pvApplicationReservedData), h_appdesc->dwApplicationReservedDataSize),
		std::string((const char*)(APP_DATA), sizeof(APP_DATA)));
	
	/* Peer GetApplicationDesc() */
	
	DWORD p_appdesc_size = 0;
	
	ASSERT_EQ(p1->GetApplicationDesc(NULL, &p_appdesc_size, 0), DPNERR_BUFFERTOOSMALL);
	
	ASSERT_TRUE(p_appdesc_size >= sizeof(DPN_APPLICATION_DESC));
	
	std::vector<unsigned char> p_appdesc_buf(p_appdesc_size);
	DPN_APPLICATION_DESC *p_appdesc = (DPN_APPLICATION_DESC*)(p_appdesc_buf.data());
	
	p_appdesc->dwSize = sizeof(DPN_APPLICATION_DESC);
	
	ASSERT_EQ(p1->GetApplicationDesc(p_appdesc, &p_appdesc_size, 0), S_OK);
	
	EXPECT_EQ((p_appdesc->dwFlags & DPNSESSION_REQUIREPASSWORD), 0);
	EXPECT_EQ(p_appdesc->guidInstance, h_appdesc->guidInstance);
	EXPECT_EQ(p_appdesc->guidApplication, APP_GUID_1);
	EXPECT_EQ(p_appdesc->dwMaxPlayers, 10);
	EXPECT_EQ(p_appdesc->dwCurrentPlayers, 2);
	EXPECT_EQ(std::wstring(p_appdesc->pwszSessionName), std::wstring(L"Session 1"));
	EXPECT_EQ(p_appdesc->pwszPassword, (WCHAR*)(NULL));
	
	EXPECT_EQ(std::string((const char*)(p_appdesc->pvApplicationReservedData), p_appdesc->dwApplicationReservedDataSize),
		std::string((const char*)(APP_DATA), sizeof(APP_DATA)));
}

TEST(DirectPlay8Peer, SetApplicationDesc)
{
	const unsigned char APP_DATA[] = { 0x00, 0x01, 0x02, 0x03, 0x04 };
	
	std::atomic<int> h_appdesc_msg_count(0);
	IDirectPlay8Peer *host_i;
	
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[&host_i, &h_appdesc_msg_count, &APP_DATA]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_APPLICATION_DESC)
			{
				/* Check GetApplicationDesc() within the DPN_MSGID_APPLICATION_DESC
				 * handler sees the new data.
				*/
				
				DWORD appdesc_size = 0;
				EXPECT_EQ(host_i->GetApplicationDesc(NULL, &appdesc_size, 0), DPNERR_BUFFERTOOSMALL);
				
				EXPECT_TRUE(appdesc_size >= sizeof(DPN_APPLICATION_DESC));
				
				std::vector<unsigned char> appdesc_buf(appdesc_size);
				DPN_APPLICATION_DESC *appdesc = (DPN_APPLICATION_DESC*)(appdesc_buf.data());
				
				appdesc->dwSize = sizeof(DPN_APPLICATION_DESC);
				
				EXPECT_EQ(host_i->GetApplicationDesc(appdesc, &appdesc_size, 0), S_OK);
				
				EXPECT_EQ((appdesc->dwFlags & DPNSESSION_REQUIREPASSWORD), DPNSESSION_REQUIREPASSWORD);
				EXPECT_EQ(appdesc->dwMaxPlayers, 20);
				EXPECT_EQ(std::wstring(appdesc->pwszSessionName), std::wstring(L"Best Session"));
				EXPECT_EQ(std::wstring(appdesc->pwszPassword),    std::wstring(L"P4ssword"));
				
				EXPECT_EQ(std::string((const char*)(appdesc->pvApplicationReservedData), appdesc->dwApplicationReservedDataSize),
					std::string((const char*)(APP_DATA), sizeof(APP_DATA)));
				
				++h_appdesc_msg_count;
			}
			
			return DPN_OK;
		});
	
	host_i = host.dp8p.instance;
	
	std::atomic<int> p_appdesc_msg_count(0);
	IDirectPlay8Peer *p1_i;
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&p1_i, &p_appdesc_msg_count, &APP_DATA]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_APPLICATION_DESC)
			{
				/* Check GetApplicationDesc() within the DPN_MSGID_APPLICATION_DESC
				 * handler sees the new data.
				*/
				
				DWORD appdesc_size = 0;
				
				EXPECT_EQ(p1_i->GetApplicationDesc(NULL, &appdesc_size, 0), DPNERR_BUFFERTOOSMALL);
				
				EXPECT_TRUE(appdesc_size >= sizeof(DPN_APPLICATION_DESC));
				
				std::vector<unsigned char> appdesc_buf(appdesc_size);
				DPN_APPLICATION_DESC *appdesc = (DPN_APPLICATION_DESC*)(appdesc_buf.data());
				
				appdesc->dwSize = sizeof(DPN_APPLICATION_DESC);
				
				EXPECT_EQ(p1_i->GetApplicationDesc(appdesc, &appdesc_size, 0), S_OK);
				
				EXPECT_EQ((appdesc->dwFlags & DPNSESSION_REQUIREPASSWORD), DPNSESSION_REQUIREPASSWORD);
				EXPECT_EQ(appdesc->dwMaxPlayers, 20);
				EXPECT_EQ(std::wstring(appdesc->pwszSessionName), std::wstring(L"Best Session"));
				EXPECT_EQ(std::wstring(appdesc->pwszPassword),    std::wstring(L"P4ssword"));
				
				EXPECT_EQ(std::string((const char*)(appdesc->pvApplicationReservedData), appdesc->dwApplicationReservedDataSize),
					std::string((const char*)(APP_DATA), sizeof(APP_DATA)));
					
				++p_appdesc_msg_count;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	p1_i = p1.instance;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	ASSERT_EQ(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		NULL,             /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	/* SetApplicationDesc() */
	
	{
		DWORD appdesc_size = 0;
		ASSERT_EQ(host->GetApplicationDesc(NULL, &appdesc_size, 0), DPNERR_BUFFERTOOSMALL);
		
		ASSERT_TRUE(appdesc_size >= sizeof(DPN_APPLICATION_DESC));
		
		std::vector<unsigned char> appdesc_buf(appdesc_size);
		DPN_APPLICATION_DESC *appdesc = (DPN_APPLICATION_DESC*)(appdesc_buf.data());
		
		appdesc->dwSize = sizeof(DPN_APPLICATION_DESC);
		
		ASSERT_EQ(host->GetApplicationDesc(appdesc, &appdesc_size, 0), S_OK);
		
		appdesc->dwMaxPlayers = 20;
		appdesc->pwszSessionName = L"Best Session";
		
		appdesc->dwFlags |= DPNSESSION_REQUIREPASSWORD;
		appdesc->pwszPassword = L"P4ssword";
		
		appdesc->pvApplicationReservedData     = (void*)(APP_DATA);
		appdesc->dwApplicationReservedDataSize = sizeof(APP_DATA);
		
		ASSERT_EQ(host->SetApplicationDesc(appdesc, 0), S_OK);
	}
	
	Sleep(250);
	
	EXPECT_EQ(h_appdesc_msg_count, 1);
	EXPECT_EQ(p_appdesc_msg_count, 1);
	
	/* Host GetApplicationDesc() */
	
	DWORD h_appdesc_size = 0;
	ASSERT_EQ(host->GetApplicationDesc(NULL, &h_appdesc_size, 0), DPNERR_BUFFERTOOSMALL);
	
	ASSERT_TRUE(h_appdesc_size >= sizeof(DPN_APPLICATION_DESC));
	
	std::vector<unsigned char> h_appdesc_buf(h_appdesc_size);
	DPN_APPLICATION_DESC *h_appdesc = (DPN_APPLICATION_DESC*)(h_appdesc_buf.data());
	
	h_appdesc->dwSize = sizeof(DPN_APPLICATION_DESC);
	
	ASSERT_EQ(host->GetApplicationDesc(h_appdesc, &h_appdesc_size, 0), S_OK);
	
	EXPECT_EQ((h_appdesc->dwFlags & DPNSESSION_REQUIREPASSWORD), DPNSESSION_REQUIREPASSWORD);
	EXPECT_EQ(h_appdesc->dwMaxPlayers, 20);
	EXPECT_EQ(std::wstring(h_appdesc->pwszSessionName), std::wstring(L"Best Session"));
	EXPECT_EQ(std::wstring(h_appdesc->pwszPassword),    std::wstring(L"P4ssword"));
	
	EXPECT_EQ(std::string((const char*)(h_appdesc->pvApplicationReservedData), h_appdesc->dwApplicationReservedDataSize),
		std::string((const char*)(APP_DATA), sizeof(APP_DATA)));
	
	/* Peer GetApplicationDesc() */
	
	DWORD p_appdesc_size = 0;
	
	ASSERT_EQ(p1->GetApplicationDesc(NULL, &p_appdesc_size, 0), DPNERR_BUFFERTOOSMALL);
	
	ASSERT_TRUE(p_appdesc_size >= sizeof(DPN_APPLICATION_DESC));
	
	std::vector<unsigned char> p_appdesc_buf(p_appdesc_size);
	DPN_APPLICATION_DESC *p_appdesc = (DPN_APPLICATION_DESC*)(p_appdesc_buf.data());
	
	p_appdesc->dwSize = sizeof(DPN_APPLICATION_DESC);
	
	ASSERT_EQ(p1->GetApplicationDesc(p_appdesc, &p_appdesc_size, 0), S_OK);
	
	EXPECT_EQ((p_appdesc->dwFlags & DPNSESSION_REQUIREPASSWORD), DPNSESSION_REQUIREPASSWORD);
	EXPECT_EQ(p_appdesc->dwMaxPlayers, 20);
	EXPECT_EQ(std::wstring(p_appdesc->pwszSessionName), std::wstring(L"Best Session"));
	EXPECT_EQ(std::wstring(p_appdesc->pwszPassword),    std::wstring(L"P4ssword"));
	
	EXPECT_EQ(std::string((const char*)(p_appdesc->pvApplicationReservedData), p_appdesc->dwApplicationReservedDataSize),
		std::string((const char*)(APP_DATA), sizeof(APP_DATA)));
}

TEST(DirectPlay8Peer, AsyncSendToPeerToHost)
{
	std::atomic<bool> testing(false);
	
	std::atomic<int> host_seq(0), p1_seq(0);
	DPNID host_player_id = -1, p1_player_id = -1;
	
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[&testing, &host_seq, &host_player_id, &p1_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
			{
				DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
				
				if(host_player_id == -1)
				{
					host_player_id = cp->dpnidPlayer;
					cp->pvPlayerContext = (void*)(0x0001);
				}
				else{
					cp->pvPlayerContext = (void*)(0x0002);
				}
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				case 1:
				{
					EXPECT_EQ(dwMessageType, DPN_MSGID_RECEIVE);
					
					if(dwMessageType == DPN_MSGID_RECEIVE)
					{
						DPNMSG_RECEIVE *r = (DPNMSG_RECEIVE*)(pMessage);
						
						EXPECT_EQ(r->dwSize,          sizeof(*r));
						EXPECT_EQ(r->dpnidSender,     p1_player_id);
						EXPECT_EQ(r->pvPlayerContext, (void*)(0x0002));
						EXPECT_EQ(r->dwReceiveFlags,  0);
						
						EXPECT_EQ(
							std::string((const char*)(r->pReceiveData), r->dwReceiveDataSize),
							std::string("Hello, world"));
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		});
	
	DPNHANDLE send_handle;
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&testing, &p1_seq, &p1_player_id, &send_handle]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
			{
				DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
				p1_player_id = cc->dpnidLocal;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++p1_seq;
			
			switch(seq)
			{
				case 1:
				{
					EXPECT_EQ(dwMessageType, DPN_MSGID_SEND_COMPLETE);
					
					if(dwMessageType == DPN_MSGID_SEND_COMPLETE)
					{
						DPNMSG_SEND_COMPLETE *sc = (DPNMSG_SEND_COMPLETE*)(pMessage);
						
						EXPECT_EQ(sc->dwSize,              sizeof(*sc));
						EXPECT_EQ(sc->hAsyncOp,            send_handle);
						EXPECT_EQ(sc->pvUserContext,       (void*)(0xABCD));
						EXPECT_EQ(sc->hResultCode,         DPN_OK);
						EXPECT_EQ(sc->dwSendCompleteFlags, 0);
						EXPECT_EQ(sc->pBuffers,            (DPN_BUFFER_DESC*)(NULL));
						EXPECT_EQ(sc->dwNumBuffers,        0);
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	ASSERT_EQ(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		NULL,             /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	/* Give everything a moment to settle. */
	Sleep(250);
	
	testing = true;
	
	DPN_BUFFER_DESC bd[] = {
		{ 12, (BYTE*)("Hello, world") },
	};
	
	ASSERT_EQ(p1->SendTo(
		host_player_id,
		bd,
		1,
		0,
		(void*)(0xABCD),
		&send_handle,
		0
	), DPNSUCCESS_PENDING);
	
	/* Let the message get through any any resultant messages happen. */
	Sleep(250);
	
	EXPECT_EQ(host_seq, 1);
	EXPECT_EQ(p1_seq, 1);
	
	testing = false;
}

TEST(DirectPlay8Peer, AsyncSendToPeerToSelf)
{
	std::atomic<bool> testing(false);
	
	std::atomic<int> host_seq(0), p1_seq(0);
	DPNID host_player_id = -1, p1_player_id = -1;
	DPNHANDLE send_handle;
	
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[&testing, &host_seq, &host_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CREATE_PLAYER && host_player_id == -1)
			{
				DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
				host_player_id = cp->dpnidPlayer;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		});
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&testing, &p1_seq, &p1_player_id, &send_handle]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
			{
				DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
				p1_player_id = cc->dpnidLocal;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++p1_seq;
			
			switch(seq)
			{
				case 1:
				case 2:
				{
					EXPECT_TRUE(dwMessageType == DPN_MSGID_SEND_COMPLETE || dwMessageType == DPN_MSGID_RECEIVE);
					
					if(dwMessageType == DPN_MSGID_SEND_COMPLETE)
					{
						DPNMSG_SEND_COMPLETE *sc = (DPNMSG_SEND_COMPLETE*)(pMessage);
						
						EXPECT_EQ(sc->dwSize,              sizeof(*sc));
						EXPECT_EQ(sc->hAsyncOp,            send_handle);
						EXPECT_EQ(sc->pvUserContext,       (void*)(0xABCD));
						EXPECT_EQ(sc->hResultCode,         DPN_OK);
						EXPECT_EQ(sc->dwSendCompleteFlags, 0);
						EXPECT_EQ(sc->pBuffers,            (DPN_BUFFER_DESC*)(NULL));
						EXPECT_EQ(sc->dwNumBuffers,        0);
					}
					else if(dwMessageType == DPN_MSGID_RECEIVE)
					{
						DPNMSG_RECEIVE *r = (DPNMSG_RECEIVE*)(pMessage);
						
						EXPECT_EQ(r->dwSize,          sizeof(*r));
						EXPECT_EQ(r->dpnidSender,     p1_player_id);
						EXPECT_EQ(r->pvPlayerContext, (void*)(NULL));
						EXPECT_EQ(r->dwReceiveFlags,  0);
						
						EXPECT_EQ(
							std::string((const char*)(r->pReceiveData), r->dwReceiveDataSize),
							std::string("Hello, world"));
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	ASSERT_EQ(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		NULL,             /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	/* Give everything a moment to settle. */
	Sleep(250);
	
	testing = true;
	
	DPN_BUFFER_DESC bd[] = {
		{ 12, (BYTE*)("Hello, world") },
	};
	
	ASSERT_EQ(p1->SendTo(
		p1_player_id,
		bd,
		1,
		0,
		(void*)(0xABCD),
		&send_handle,
		0
	), DPNSUCCESS_PENDING);
	
	/* Let the message get through any any resultant messages happen. */
	Sleep(250);
	
	EXPECT_EQ(host_seq, 0);
	EXPECT_EQ(p1_seq, 2);
	
	testing = false;
}

TEST(DirectPlay8Peer, AsyncSendToPeerToAll)
{
	std::atomic<bool> testing(false);
	
	std::atomic<int> host_seq(0), p1_seq(0);
	DPNID host_player_id = -1, p1_player_id = -1;
	
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[&testing, &host_seq, &host_player_id, &p1_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
			{
				DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
				
				if(host_player_id == -1)
				{
					host_player_id = cp->dpnidPlayer;
					cp->pvPlayerContext = (void*)(0x0001);
				}
				else{
					cp->pvPlayerContext = (void*)(0x0002);
				}
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				case 1:
				{
					EXPECT_EQ(dwMessageType, DPN_MSGID_RECEIVE);
					
					if(dwMessageType == DPN_MSGID_RECEIVE)
					{
						DPNMSG_RECEIVE *r = (DPNMSG_RECEIVE*)(pMessage);
						
						EXPECT_EQ(r->dwSize,          sizeof(*r));
						EXPECT_EQ(r->dpnidSender,     p1_player_id);
						EXPECT_EQ(r->pvPlayerContext, (void*)(0x0002));
						EXPECT_EQ(r->dwReceiveFlags,  0);
						
						EXPECT_EQ(
							std::string((const char*)(r->pReceiveData), r->dwReceiveDataSize),
							std::string("Hello, world"));
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		});
	
	DPNHANDLE send_handle;
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&testing, &p1_seq, &p1_player_id, &send_handle]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
			{
				DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
				p1_player_id = cc->dpnidLocal;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++p1_seq;
			
			switch(seq)
			{
				case 1:
				case 2:
				{
					EXPECT_TRUE(dwMessageType == DPN_MSGID_SEND_COMPLETE || dwMessageType == DPN_MSGID_RECEIVE);
					
					if(dwMessageType == DPN_MSGID_SEND_COMPLETE)
					{
						DPNMSG_SEND_COMPLETE *sc = (DPNMSG_SEND_COMPLETE*)(pMessage);
						
						EXPECT_EQ(sc->dwSize,              sizeof(*sc));
						EXPECT_EQ(sc->hAsyncOp,            send_handle);
						EXPECT_EQ(sc->pvUserContext,       (void*)(0xABCD));
						EXPECT_EQ(sc->hResultCode,         DPN_OK);
						EXPECT_EQ(sc->dwSendCompleteFlags, 0);
						EXPECT_EQ(sc->pBuffers,            (DPN_BUFFER_DESC*)(NULL));
						EXPECT_EQ(sc->dwNumBuffers,        0);
					}
					else if(dwMessageType == DPN_MSGID_RECEIVE)
					{
						DPNMSG_RECEIVE *r = (DPNMSG_RECEIVE*)(pMessage);
						
						EXPECT_EQ(r->dwSize,          sizeof(*r));
						EXPECT_EQ(r->dpnidSender,     p1_player_id);
						EXPECT_EQ(r->pvPlayerContext, (void*)(NULL));
						EXPECT_EQ(r->dwReceiveFlags,  0);
						
						EXPECT_EQ(
							std::string((const char*)(r->pReceiveData), r->dwReceiveDataSize),
							std::string("Hello, world"));
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	ASSERT_EQ(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		NULL,             /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	/* Give everything a moment to settle. */
	Sleep(250);
	
	testing = true;
	
	DPN_BUFFER_DESC bd[] = {
		{ 12, (BYTE*)("Hello, world") },
	};
	
	ASSERT_EQ(p1->SendTo(
		DPNID_ALL_PLAYERS_GROUP,
		bd,
		1,
		0,
		(void*)(0xABCD),
		&send_handle,
		0
	), DPNSUCCESS_PENDING);
	
	/* Let the message get through any any resultant messages happen. */
	Sleep(250);
	
	EXPECT_EQ(host_seq, 1);
	EXPECT_EQ(p1_seq, 2);
	
	testing = false;
}

TEST(DirectPlay8Peer, AsyncSendToPeerToAllButSelf)
{
	std::atomic<bool> testing(false);
	
	std::atomic<int> host_seq(0), p1_seq(0);
	DPNID host_player_id = -1, p1_player_id = -1;
	
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[&testing, &host_seq, &host_player_id, &p1_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
			{
				DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
				
				if(host_player_id == -1)
				{
					host_player_id = cp->dpnidPlayer;
					cp->pvPlayerContext = (void*)(0x0001);
				}
				else{
					cp->pvPlayerContext = (void*)(0x0002);
				}
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				case 1:
				{
					EXPECT_EQ(dwMessageType, DPN_MSGID_RECEIVE);
					
					if(dwMessageType == DPN_MSGID_RECEIVE)
					{
						DPNMSG_RECEIVE *r = (DPNMSG_RECEIVE*)(pMessage);
						
						EXPECT_EQ(r->dwSize,          sizeof(*r));
						EXPECT_EQ(r->dpnidSender,     p1_player_id);
						EXPECT_EQ(r->pvPlayerContext, (void*)(0x0002));
						EXPECT_EQ(r->dwReceiveFlags,  0);
						
						EXPECT_EQ(
							std::string((const char*)(r->pReceiveData), r->dwReceiveDataSize),
							std::string("Hello, world"));
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		});
	
	DPNHANDLE send_handle;
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&testing, &p1_seq, &p1_player_id, &send_handle]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
			{
				DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
				p1_player_id = cc->dpnidLocal;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++p1_seq;
			
			switch(seq)
			{
				case 1:
				{
					EXPECT_EQ(dwMessageType, DPN_MSGID_SEND_COMPLETE);
					
					if(dwMessageType == DPN_MSGID_SEND_COMPLETE)
					{
						DPNMSG_SEND_COMPLETE *sc = (DPNMSG_SEND_COMPLETE*)(pMessage);
						
						EXPECT_EQ(sc->dwSize,              sizeof(*sc));
						EXPECT_EQ(sc->hAsyncOp,            send_handle);
						EXPECT_EQ(sc->pvUserContext,       (void*)(0xABCD));
						EXPECT_EQ(sc->hResultCode,         DPN_OK);
						EXPECT_EQ(sc->dwSendCompleteFlags, 0);
						EXPECT_EQ(sc->pBuffers,            (DPN_BUFFER_DESC*)(NULL));
						EXPECT_EQ(sc->dwNumBuffers,        0);
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	ASSERT_EQ(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		NULL,             /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	/* Give everything a moment to settle. */
	Sleep(250);
	
	testing = true;
	
	DPN_BUFFER_DESC bd[] = {
		{ 12, (BYTE*)("Hello, world") },
	};
	
	ASSERT_EQ(p1->SendTo(
		DPNID_ALL_PLAYERS_GROUP,
		bd,
		1,
		0,
		(void*)(0xABCD),
		&send_handle,
		DPNSEND_NOLOOPBACK
	), DPNSUCCESS_PENDING);
	
	/* Let the message get through any any resultant messages happen. */
	Sleep(250);
	
	EXPECT_EQ(host_seq, 1);
	EXPECT_EQ(p1_seq, 1);
	
	testing = false;
}

TEST(DirectPlay8Peer, AsyncSendToHostToPeer)
{
	std::atomic<bool> testing(false);
	
	std::atomic<int> host_seq(0), p1_seq(0);
	DPNID host_player_id = -1, p1_player_id = -1;
	DPNHANDLE send_handle;
	
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[&testing, &host_seq, &host_player_id, &send_handle]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CREATE_PLAYER && host_player_id == -1)
			{
				DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
				host_player_id = cp->dpnidPlayer;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				case 1:
				{
					EXPECT_EQ(dwMessageType, DPN_MSGID_SEND_COMPLETE);
					
					if(dwMessageType == DPN_MSGID_SEND_COMPLETE)
					{
						DPNMSG_SEND_COMPLETE *sc = (DPNMSG_SEND_COMPLETE*)(pMessage);
						
						EXPECT_EQ(sc->dwSize,              sizeof(*sc));
						EXPECT_EQ(sc->hAsyncOp,            send_handle);
						EXPECT_EQ(sc->pvUserContext,       (void*)(0xABCD));
						EXPECT_EQ(sc->hResultCode,         DPN_OK);
						EXPECT_EQ(sc->dwSendCompleteFlags, 0);
						EXPECT_EQ(sc->pBuffers,            (DPN_BUFFER_DESC*)(NULL));
						EXPECT_EQ(sc->dwNumBuffers,        0);
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		});
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&testing, &p1_seq, &p1_player_id, &host_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
			{
				DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
				p1_player_id = cc->dpnidLocal;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++p1_seq;
			
			switch(seq)
			{
				case 1:
				{
					EXPECT_EQ(dwMessageType, DPN_MSGID_RECEIVE);
					
					if(dwMessageType == DPN_MSGID_RECEIVE)
					{
						DPNMSG_RECEIVE *r = (DPNMSG_RECEIVE*)(pMessage);
						
						EXPECT_EQ(r->dwSize,          sizeof(*r));
						EXPECT_EQ(r->dpnidSender,     host_player_id);
						EXPECT_EQ(r->pvPlayerContext, (void*)(NULL));
						EXPECT_EQ(r->dwReceiveFlags,  0);
						
						EXPECT_EQ(
							std::string((const char*)(r->pReceiveData), r->dwReceiveDataSize),
							std::string("Hello, world"));
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	ASSERT_EQ(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		NULL,             /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	/* Give everything a moment to settle. */
	Sleep(250);
	
	testing = true;
	
	DPN_BUFFER_DESC bd[] = {
		{ 12, (BYTE*)("Hello, world") },
	};
	
	ASSERT_EQ(host->SendTo(
		p1_player_id,
		bd,
		1,
		0,
		(void*)(0xABCD),
		&send_handle,
		0
	), DPNSUCCESS_PENDING);
	
	/* Let the message get through any any resultant messages happen. */
	Sleep(250);
	
	EXPECT_EQ(host_seq, 1);
	EXPECT_EQ(p1_seq, 1);
	
	testing = false;
}

TEST(DirectPlay8Peer, AsyncSendToHostToNone)
{
	std::atomic<bool> testing(false);
	
	std::atomic<int> host_seq(0);
	DPNID host_player_id = -1;
	DPNHANDLE send_handle;
	
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[&testing, &host_seq, &host_player_id, &send_handle]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CREATE_PLAYER && host_player_id == -1)
			{
				DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
				host_player_id = cp->dpnidPlayer;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				case 1:
				{
					EXPECT_EQ(dwMessageType, DPN_MSGID_SEND_COMPLETE);
					
					if(dwMessageType == DPN_MSGID_SEND_COMPLETE)
					{
						DPNMSG_SEND_COMPLETE *sc = (DPNMSG_SEND_COMPLETE*)(pMessage);
						
						EXPECT_EQ(sc->dwSize,              sizeof(*sc));
						EXPECT_EQ(sc->hAsyncOp,            send_handle);
						EXPECT_EQ(sc->pvUserContext,       (void*)(0xABCD));
						EXPECT_EQ(sc->hResultCode,         DPN_OK);
						EXPECT_EQ(sc->dwSendCompleteFlags, 0);
						EXPECT_EQ(sc->pBuffers,            (DPN_BUFFER_DESC*)(NULL));
						EXPECT_EQ(sc->dwNumBuffers,        0);
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		});
	
	/* Give everything a moment to settle. */
	Sleep(250);
	
	testing = true;
	
	DPN_BUFFER_DESC bd[] = {
		{ 12, (BYTE*)("Hello, world") },
	};
	
	ASSERT_EQ(host->SendTo(
		DPNID_ALL_PLAYERS_GROUP,
		bd,
		1,
		0,
		(void*)(0xABCD),
		&send_handle,
		DPNSEND_NOLOOPBACK
	), DPNSUCCESS_PENDING);
	
	/* Let the message get through any any resultant messages happen. */
	Sleep(250);
	
	EXPECT_EQ(host_seq, 1);
	
	testing = false;
}

TEST(DirectPlay8Peer, SyncSendToPeerToHost)
{
	std::atomic<bool> testing(false);
	
	std::atomic<int> host_seq(0), p1_seq(0);
	DPNID host_player_id = -1, p1_player_id = -1;
	
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[&testing, &host_seq, &host_player_id, &p1_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CREATE_PLAYER && host_player_id == -1)
			{
				DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
				host_player_id = cp->dpnidPlayer;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				case 1:
				{
					EXPECT_EQ(dwMessageType, DPN_MSGID_RECEIVE);
					
					if(dwMessageType == DPN_MSGID_RECEIVE)
					{
						DPNMSG_RECEIVE *r = (DPNMSG_RECEIVE*)(pMessage);
						
						EXPECT_EQ(r->dwSize,          sizeof(*r));
						EXPECT_EQ(r->dpnidSender,     p1_player_id);
						EXPECT_EQ(r->pvPlayerContext, (void*)(NULL));
						EXPECT_EQ(r->dwReceiveFlags,  0);
						
						EXPECT_EQ(
							std::string((const char*)(r->pReceiveData), r->dwReceiveDataSize),
							std::string("Hello, world"));
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		});
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&testing, &p1_seq, &p1_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
			{
				DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
				p1_player_id = cc->dpnidLocal;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++p1_seq;
			
			ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	ASSERT_EQ(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		NULL,             /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	/* Give everything a moment to settle. */
	Sleep(250);
	
	testing = true;
	
	DPN_BUFFER_DESC bd[] = {
		{ 12, (BYTE*)("Hello, world") },
	};
	
	ASSERT_EQ(p1->SendTo(
		host_player_id,
		bd,
		1,
		0,
		NULL,
		NULL,
		DPNSEND_SYNC
	), DPN_OK);
	
	/* Let the message get through any any resultant messages happen. */
	Sleep(250);
	
	EXPECT_EQ(host_seq, 1);
	EXPECT_EQ(p1_seq, 0);
	
	testing = false;
}

TEST(DirectPlay8Peer, SyncSendToPeerToSelf)
{
	std::atomic<bool> testing(false);
	
	std::atomic<int> host_seq(0), p1_seq(0);
	DPNID host_player_id = -1, p1_player_id = -1;
	
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[&testing, &host_seq, &host_player_id, &p1_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CREATE_PLAYER && host_player_id == -1)
			{
				DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
				host_player_id = cp->dpnidPlayer;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		});
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&testing, &p1_seq, &p1_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
			{
				DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
				p1_player_id = cc->dpnidLocal;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++p1_seq;
			
			switch(seq)
			{
				case 1:
				{
					EXPECT_EQ(dwMessageType, DPN_MSGID_RECEIVE);
					
					if(dwMessageType == DPN_MSGID_RECEIVE)
					{
						DPNMSG_RECEIVE *r = (DPNMSG_RECEIVE*)(pMessage);
						
						EXPECT_EQ(r->dwSize,          sizeof(*r));
						EXPECT_EQ(r->dpnidSender,     p1_player_id);
						EXPECT_EQ(r->pvPlayerContext, (void*)(NULL));
						EXPECT_EQ(r->dwReceiveFlags,  0);
						
						EXPECT_EQ(
							std::string((const char*)(r->pReceiveData), r->dwReceiveDataSize),
							std::string("Hello, world"));
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	ASSERT_EQ(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		NULL,             /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	/* Give everything a moment to settle. */
	Sleep(250);
	
	testing = true;
	
	DPN_BUFFER_DESC bd[] = {
		{ 12, (BYTE*)("Hello, world") },
	};
	
	ASSERT_EQ(p1->SendTo(
		p1_player_id,
		bd,
		1,
		0,
		NULL,
		NULL,
		DPNSEND_SYNC
	), DPN_OK);
	
	/* Let the message get through any any resultant messages happen. */
	Sleep(250);
	
	EXPECT_EQ(host_seq, 0);
	EXPECT_EQ(p1_seq, 1);
	
	testing = false;
}

TEST(DirectPlay8Peer, SyncSendToPeerToAll)
{
	std::atomic<bool> testing(false);
	
	std::atomic<int> host_seq(0), p1_seq(0);
	DPNID host_player_id = -1, p1_player_id = -1;
	
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[&testing, &host_seq, &host_player_id, &p1_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CREATE_PLAYER && host_player_id == -1)
			{
				DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
				host_player_id = cp->dpnidPlayer;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				case 1:
				{
					EXPECT_EQ(dwMessageType, DPN_MSGID_RECEIVE);
					
					if(dwMessageType == DPN_MSGID_RECEIVE)
					{
						DPNMSG_RECEIVE *r = (DPNMSG_RECEIVE*)(pMessage);
						
						EXPECT_EQ(r->dwSize,          sizeof(*r));
						EXPECT_EQ(r->dpnidSender,     p1_player_id);
						EXPECT_EQ(r->pvPlayerContext, (void*)(NULL));
						EXPECT_EQ(r->dwReceiveFlags,  0);
						
						EXPECT_EQ(
							std::string((const char*)(r->pReceiveData), r->dwReceiveDataSize),
							std::string("Hello, world"));
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		});
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&testing, &p1_seq, &p1_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
			{
				DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
				p1_player_id = cc->dpnidLocal;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++p1_seq;
			
			switch(seq)
			{
				case 1:
				{
					EXPECT_EQ(dwMessageType, DPN_MSGID_RECEIVE);
					
					if(dwMessageType == DPN_MSGID_RECEIVE)
					{
						DPNMSG_RECEIVE *r = (DPNMSG_RECEIVE*)(pMessage);
						
						EXPECT_EQ(r->dwSize,          sizeof(*r));
						EXPECT_EQ(r->dpnidSender,     p1_player_id);
						EXPECT_EQ(r->pvPlayerContext, (void*)(NULL));
						EXPECT_EQ(r->dwReceiveFlags,  0);
						
						EXPECT_EQ(
							std::string((const char*)(r->pReceiveData), r->dwReceiveDataSize),
							std::string("Hello, world"));
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	ASSERT_EQ(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		NULL,             /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	/* Give everything a moment to settle. */
	Sleep(250);
	
	testing = true;
	
	DPN_BUFFER_DESC bd[] = {
		{ 12, (BYTE*)("Hello, world") },
	};
	
	ASSERT_EQ(p1->SendTo(
		DPNID_ALL_PLAYERS_GROUP,
		bd,
		1,
		0,
		NULL,
		NULL,
		DPNSEND_SYNC
	), DPN_OK);
	
	/* Let the message get through any any resultant messages happen. */
	Sleep(250);
	
	EXPECT_EQ(host_seq, 1);
	EXPECT_EQ(p1_seq, 1);
	
	testing = false;
}

TEST(DirectPlay8Peer, SyncSendToPeerToAllButSelf)
{
	std::atomic<bool> testing(false);
	
	std::atomic<int> host_seq(0), p1_seq(0);
	DPNID host_player_id = -1, p1_player_id = -1;
	
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[&testing, &host_seq, &host_player_id, &p1_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CREATE_PLAYER && host_player_id == -1)
			{
				DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
				host_player_id = cp->dpnidPlayer;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				case 1:
				{
					EXPECT_EQ(dwMessageType, DPN_MSGID_RECEIVE);
					
					if(dwMessageType == DPN_MSGID_RECEIVE)
					{
						DPNMSG_RECEIVE *r = (DPNMSG_RECEIVE*)(pMessage);
						
						EXPECT_EQ(r->dwSize,          sizeof(*r));
						EXPECT_EQ(r->dpnidSender,     p1_player_id);
						EXPECT_EQ(r->pvPlayerContext, (void*)(NULL));
						EXPECT_EQ(r->dwReceiveFlags,  0);
						
						EXPECT_EQ(
							std::string((const char*)(r->pReceiveData), r->dwReceiveDataSize),
							std::string("Hello, world"));
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		});
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&testing, &p1_seq, &p1_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
			{
				DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
				p1_player_id = cc->dpnidLocal;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++p1_seq;
			
			switch(seq)
			{
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	ASSERT_EQ(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		NULL,             /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	/* Give everything a moment to settle. */
	Sleep(250);
	
	testing = true;
	
	DPN_BUFFER_DESC bd[] = {
		{ 12, (BYTE*)("Hello, world") },
	};
	
	ASSERT_EQ(p1->SendTo(
		DPNID_ALL_PLAYERS_GROUP,
		bd,
		1,
		0,
		NULL,
		NULL,
		DPNSEND_SYNC | DPNSEND_NOLOOPBACK
	), DPN_OK);
	
	/* Let the message get through any any resultant messages happen. */
	Sleep(250);
	
	EXPECT_EQ(host_seq, 1);
	EXPECT_EQ(p1_seq, 0);
	
	testing = false;
}

TEST(DirectPlay8Peer, SyncSendToHostToNone)
{
	std::atomic<bool> testing(false);
	
	std::atomic<int> host_seq(0);
	DPNID host_player_id = -1;
	
	SessionHost host(APP_GUID_1, L"Session 1", PORT,
		[&testing, &host_seq, &host_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CREATE_PLAYER && host_player_id == -1)
			{
				DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
				host_player_id = cp->dpnidPlayer;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		});
	
	/* Give everything a moment to settle. */
	Sleep(250);
	
	testing = true;
	
	DPN_BUFFER_DESC bd[] = {
		{ 12, (BYTE*)("Hello, world") },
	};
	
	ASSERT_EQ(host->SendTo(
		DPNID_ALL_PLAYERS_GROUP,
		bd,
		1,
		0,
		NULL,
		NULL,
		DPNSEND_SYNC | DPNSEND_NOLOOPBACK
	), DPN_OK);
	
	/* Let the message get through any any resultant messages happen. */
	Sleep(250);
	
	EXPECT_EQ(host_seq, 0);
	
	testing = false;
}

TEST(DirectPlay8Peer, SetPeerInfoSyncBeforeHost)
{
	std::atomic<bool> testing(false);
	
	std::atomic<int> host_seq(0), p1_seq(0);
	DPNID host_player_id = -1, p1_player_id = -1;
	
	std::function<HRESULT(DWORD,PVOID)> host_cb =
		[&testing, &host_seq, &host_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CREATE_PLAYER && host_player_id == -1)
			{
				DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
				host_player_id = cp->dpnidPlayer;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance host;
	
	ASSERT_EQ(host->Initialize(&host_cb, &callback_shim, 0), S_OK);
	
	const wchar_t *HOST_NAME = L"Da Host";
	const unsigned char HOST_DATA[] = { 0x00, 0x01, 0x02, 0x03, 0x00, 0xAA, 0xBB, 0xCC };
	
	{
		DPN_PLAYER_INFO info;
		memset(&info, 0, sizeof(info));
		
		info.dwSize      = sizeof(info);
		info.dwInfoFlags = DPNINFO_NAME | DPNINFO_DATA;
		info.pwszName    = (wchar_t*)(HOST_NAME);
		info.pvData      = (void*)(HOST_DATA);
		info.dwDataSize  = sizeof(HOST_DATA);
		
		ASSERT_EQ(host->SetPeerInfo(&info, NULL, NULL, DPNSETPEERINFO_SYNC), S_OK);
	}
	
	{
		DPN_APPLICATION_DESC app_desc;
		memset(&app_desc, 0, sizeof(app_desc));
		
		app_desc.dwSize = sizeof(app_desc);
		app_desc.guidApplication = APP_GUID_1;
		app_desc.pwszSessionName = (wchar_t*)(L"Session 1");
		
		IDP8AddressInstance addr;
		
		DWORD port = PORT;
		
		ASSERT_EQ(addr->SetSP(&CLSID_DP8SP_TCPIP), S_OK);
		ASSERT_EQ(addr->AddComponent(DPNA_KEY_PORT, &port, sizeof(DWORD), DPNA_DATATYPE_DWORD), S_OK);
		
		ASSERT_EQ(host->Host(&app_desc, &(addr.instance), 1, NULL, NULL, (void*)(0xB00), 0), S_OK);
	}
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&testing, &p1_seq, &p1_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
			{
				DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
				p1_player_id = cc->dpnidLocal;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++p1_seq;
			
			switch(seq)
			{
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	ASSERT_EQ(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		NULL,             /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	/* Give everything a moment to settle. */
	Sleep(250);
	
	/* Check the host has its own player data. */
	EXPECT_PEERINFO(host.instance, host_player_id,
		HOST_NAME, HOST_DATA, sizeof(HOST_DATA), (DPNPLAYER_LOCAL | DPNPLAYER_HOST));
	
	/* Check the peer has the host's data. */
	EXPECT_PEERINFO(p1.instance, host_player_id,
		HOST_NAME, HOST_DATA, sizeof(HOST_DATA), DPNPLAYER_HOST);
}

TEST(DirectPlay8Peer, SetPeerInfoSyncAfterPeerConnects)
{
	const wchar_t *HOST_NAME = L"Da Host";
	const unsigned char HOST_DATA[] = { 0x00, 0x01, 0x02, 0x03, 0x00, 0xAA, 0xBB, 0xCC };
	
	std::atomic<bool> testing(false);
	
	std::atomic<int> host_seq(0), p1_seq(0);
	DPNID host_player_id = -1, p1_player_id = -1;
	
	IDirectPlay8Peer *hostp;
	
	std::function<HRESULT(DWORD,PVOID)> host_cb =
		[&testing, &host_seq, &host_player_id, &hostp, &HOST_NAME, &HOST_DATA]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CREATE_PLAYER && host_player_id == -1)
			{
				DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
				host_player_id = cp->dpnidPlayer;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				case 1:
				{
					EXPECT_EQ(dwMessageType, DPN_MSGID_PEER_INFO);
					
					if(dwMessageType == DPN_MSGID_PEER_INFO)
					{
						DPNMSG_PEER_INFO *pi = (DPNMSG_PEER_INFO*)(pMessage);
						
						EXPECT_EQ(pi->dwSize,          sizeof(DPNMSG_PEER_INFO));
						EXPECT_EQ(pi->dpnidPeer,       host_player_id);
						EXPECT_EQ(pi->pvPlayerContext, (void*)(0xB00));
						
						EXPECT_PEERINFO(hostp, host_player_id,
							HOST_NAME, HOST_DATA, sizeof(HOST_DATA), (DPNPLAYER_LOCAL | DPNPLAYER_HOST));
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance host;
	hostp = host.instance;
	
	ASSERT_EQ(host->Initialize(&host_cb, &callback_shim, 0), S_OK);
	
	{
		DPN_APPLICATION_DESC app_desc;
		memset(&app_desc, 0, sizeof(app_desc));
		
		app_desc.dwSize = sizeof(app_desc);
		app_desc.guidApplication = APP_GUID_1;
		app_desc.pwszSessionName = (wchar_t*)(L"Session 1");
		
		IDP8AddressInstance addr;
		
		DWORD port = PORT;
		
		ASSERT_EQ(addr->SetSP(&CLSID_DP8SP_TCPIP), S_OK);
		ASSERT_EQ(addr->AddComponent(DPNA_KEY_PORT, &port, sizeof(DWORD), DPNA_DATATYPE_DWORD), S_OK);
		
		ASSERT_EQ(host->Host(&app_desc, &(addr.instance), 1, NULL, NULL, (void*)(0xB00), 0), S_OK);
	}
	
	IDirectPlay8Peer *p1p;
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&testing, &p1_seq, &p1_player_id, &host_player_id, &p1p, &HOST_NAME, &HOST_DATA]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
			{
				DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
				p1_player_id = cc->dpnidLocal;
			}
			else if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
			{
				DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
				
				if(cp->dpnidPlayer == host_player_id)
				{
					cp->pvPlayerContext = (void*)(0x1234);
				}
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++p1_seq;
			
			switch(seq)
			{
				case 1:
				{
					EXPECT_EQ(dwMessageType, DPN_MSGID_PEER_INFO);
					
					if(dwMessageType == DPN_MSGID_PEER_INFO)
					{
						DPNMSG_PEER_INFO *pi = (DPNMSG_PEER_INFO*)(pMessage);
						
						EXPECT_EQ(pi->dwSize,          sizeof(DPNMSG_PEER_INFO));
						EXPECT_EQ(pi->dpnidPeer,       host_player_id);
						EXPECT_EQ(pi->pvPlayerContext, (void*)(0x1234));
						
						EXPECT_PEERINFO(p1p, host_player_id,
							HOST_NAME, HOST_DATA, sizeof(HOST_DATA), DPNPLAYER_HOST);
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	p1p = p1.instance;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	ASSERT_EQ(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		NULL,             /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	/* Give everything a moment to settle. */
	Sleep(250);
	
	testing = true;
	
	{
		
		DPN_PLAYER_INFO info;
		memset(&info, 0, sizeof(info));
		
		info.dwSize      = sizeof(info);
		info.dwInfoFlags = DPNINFO_NAME | DPNINFO_DATA;
		info.pwszName    = (wchar_t*)(HOST_NAME);
		info.pvData      = (void*)(HOST_DATA);
		info.dwDataSize  = sizeof(HOST_DATA);
		
		ASSERT_EQ(host->SetPeerInfo(&info, NULL, NULL, DPNSETPEERINFO_SYNC), S_OK);
	}
	
	Sleep(250);
	
	/* Check the host has its own player data. */
	EXPECT_PEERINFO(host.instance, host_player_id,
		HOST_NAME, HOST_DATA, sizeof(HOST_DATA), (DPNPLAYER_LOCAL | DPNPLAYER_HOST));
	
	/* Check the peer has the host's data. */
	EXPECT_PEERINFO(p1.instance, host_player_id,
		HOST_NAME, HOST_DATA, sizeof(HOST_DATA), DPNPLAYER_HOST);
	
	testing = false;
	
	EXPECT_EQ(host_seq, 1);
	EXPECT_EQ(p1_seq, 1);
}

TEST(DirectPlay8Peer, SetPeerInfoSyncBeforeConnect)
{
	std::atomic<bool> testing(false);
	
	std::atomic<int> host_seq(0), p1_seq(0);
	DPNID host_player_id = -1, p1_player_id = -1;
	
	std::function<HRESULT(DWORD,PVOID)> host_cb =
		[&testing, &host_seq, &host_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CREATE_PLAYER && host_player_id == -1)
			{
				DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
				host_player_id = cp->dpnidPlayer;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance host;
	
	ASSERT_EQ(host->Initialize(&host_cb, &callback_shim, 0), S_OK);
	
	{
		DPN_APPLICATION_DESC app_desc;
		memset(&app_desc, 0, sizeof(app_desc));
		
		app_desc.dwSize = sizeof(app_desc);
		app_desc.guidApplication = APP_GUID_1;
		app_desc.pwszSessionName = (wchar_t*)(L"Session 1");
		
		IDP8AddressInstance addr;
		
		DWORD port = PORT;
		
		ASSERT_EQ(addr->SetSP(&CLSID_DP8SP_TCPIP), S_OK);
		ASSERT_EQ(addr->AddComponent(DPNA_KEY_PORT, &port, sizeof(DWORD), DPNA_DATATYPE_DWORD), S_OK);
		
		ASSERT_EQ(host->Host(&app_desc, &(addr.instance), 1, NULL, NULL, (void*)(0xB00), 0), S_OK);
	}
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&testing, &p1_seq, &p1_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
			{
				DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
				p1_player_id = cc->dpnidLocal;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++p1_seq;
			
			switch(seq)
			{
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	const wchar_t *P1_NAME = L"Not Da Host";
	const unsigned char P1_DATA[] = { 0x00, 0x01, 0x02, 0x03, 0x00, 0xAA, 0xBB, 0xCC };
	
	{
		DPN_PLAYER_INFO info;
		memset(&info, 0, sizeof(info));
		
		info.dwSize      = sizeof(info);
		info.dwInfoFlags = DPNINFO_NAME | DPNINFO_DATA;
		info.pwszName    = (wchar_t*)(P1_NAME);
		info.pvData      = (void*)(P1_DATA);
		info.dwDataSize  = sizeof(P1_DATA);
		
		ASSERT_EQ(p1->SetPeerInfo(&info, NULL, NULL, DPNSETPEERINFO_SYNC), S_OK);
	}
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	ASSERT_EQ(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		NULL,             /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	/* Give everything a moment to settle. */
	Sleep(250);
	
	/* Check the host has the peer's data. */
	EXPECT_PEERINFO(host.instance, p1_player_id,
		P1_NAME, P1_DATA, sizeof(P1_DATA), 0);
	
	/* Check the peer has its own data. */
	EXPECT_PEERINFO(p1.instance, p1_player_id,
		P1_NAME, P1_DATA, sizeof(P1_DATA), DPNPLAYER_LOCAL);
}

TEST(DirectPlay8Peer, SetPeerInfoSyncAfterConnect)
{
	const wchar_t *P1_NAME = L"Not Da Host";
	const unsigned char P1_DATA[] = { 0x00, 0x01, 0x02, 0x03, 0x00, 0xAA, 0xBB, 0xCC };
	
	std::atomic<bool> testing(false);
	
	std::atomic<int> host_seq(0), p1_seq(0);
	DPNID host_player_id = -1, p1_player_id = -1;
	
	IDirectPlay8Peer *hostp;
	
	std::function<HRESULT(DWORD,PVOID)> host_cb =
		[&testing, &host_seq, &host_player_id, &p1_player_id, &hostp, &P1_NAME, &P1_DATA]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CREATE_PLAYER && host_player_id == -1)
			{
				DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
				host_player_id = cp->dpnidPlayer;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				case 1:
				{
					EXPECT_EQ(dwMessageType, DPN_MSGID_PEER_INFO);
					
					if(dwMessageType == DPN_MSGID_PEER_INFO)
					{
						DPNMSG_PEER_INFO *pi = (DPNMSG_PEER_INFO*)(pMessage);
						
						EXPECT_EQ(pi->dwSize,          sizeof(DPNMSG_PEER_INFO));
						EXPECT_EQ(pi->dpnidPeer,       p1_player_id);
						EXPECT_EQ(pi->pvPlayerContext, (void*)(0x0000));
						
						EXPECT_PEERINFO(hostp, p1_player_id,
							P1_NAME, P1_DATA, sizeof(P1_DATA), 0);
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance host;
	hostp = host.instance;
	
	ASSERT_EQ(host->Initialize(&host_cb, &callback_shim, 0), S_OK);
	
	{
		DPN_APPLICATION_DESC app_desc;
		memset(&app_desc, 0, sizeof(app_desc));
		
		app_desc.dwSize = sizeof(app_desc);
		app_desc.guidApplication = APP_GUID_1;
		app_desc.pwszSessionName = (wchar_t*)(L"Session 1");
		
		IDP8AddressInstance addr;
		
		DWORD port = PORT;
		
		ASSERT_EQ(addr->SetSP(&CLSID_DP8SP_TCPIP), S_OK);
		ASSERT_EQ(addr->AddComponent(DPNA_KEY_PORT, &port, sizeof(DWORD), DPNA_DATATYPE_DWORD), S_OK);
		
		ASSERT_EQ(host->Host(&app_desc, &(addr.instance), 1, NULL, NULL, (void*)(0xB00), 0), S_OK);
	}
	
	IDirectPlay8Peer *p1p;
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&testing, &p1_seq, &p1_player_id, &p1p, &P1_NAME, &P1_DATA]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
			{
				DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
				p1_player_id = cc->dpnidLocal;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++p1_seq;
			
			switch(seq)
			{
				case 1:
				{
					EXPECT_EQ(dwMessageType, DPN_MSGID_PEER_INFO);
					
					if(dwMessageType == DPN_MSGID_PEER_INFO)
					{
						DPNMSG_PEER_INFO *pi = (DPNMSG_PEER_INFO*)(pMessage);
						
						EXPECT_EQ(pi->dwSize,          sizeof(DPNMSG_PEER_INFO));
						EXPECT_EQ(pi->dpnidPeer,       p1_player_id);
						EXPECT_EQ(pi->pvPlayerContext, (void*)(0x0000));
						
						EXPECT_PEERINFO(p1p, p1_player_id,
							P1_NAME, P1_DATA, sizeof(P1_DATA), DPNPLAYER_LOCAL);
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	p1p = p1.instance;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	ASSERT_EQ(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		NULL,             /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	/* Give everything a moment to settle. */
	Sleep(250);
	
	testing = true;
	
	{
		
		DPN_PLAYER_INFO info;
		memset(&info, 0, sizeof(info));
		
		info.dwSize      = sizeof(info);
		info.dwInfoFlags = DPNINFO_NAME | DPNINFO_DATA;
		info.pwszName    = (wchar_t*)(P1_NAME);
		info.pvData      = (void*)(P1_DATA);
		info.dwDataSize  = sizeof(P1_DATA);
		
		ASSERT_EQ(p1->SetPeerInfo(&info, NULL, NULL, DPNSETPEERINFO_SYNC), S_OK);
	}
	
	Sleep(250);
	
	/* Check the host has the peer's data. */
	EXPECT_PEERINFO(host.instance, p1_player_id,
		P1_NAME, P1_DATA, sizeof(P1_DATA), 0);
	
	/* Check the peer has its own data. */
	EXPECT_PEERINFO(p1.instance, p1_player_id,
		P1_NAME, P1_DATA, sizeof(P1_DATA), DPNPLAYER_LOCAL);
	
	testing = false;
	
	EXPECT_EQ(host_seq, 1);
	EXPECT_EQ(p1_seq, 1);
}

TEST(DirectPlay8Peer, SetPeerInfoAsyncBeforeHost)
{
	std::atomic<bool> testing(false);
	
	std::atomic<int> host_seq(0), p1_seq(0);
	DPNID host_player_id = -1, p1_player_id = -1;
	
	std::function<HRESULT(DWORD,PVOID)> host_cb =
		[&testing, &host_seq, &host_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CREATE_PLAYER && host_player_id == -1)
			{
				DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
				host_player_id = cp->dpnidPlayer;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance host;
	
	ASSERT_EQ(host->Initialize(&host_cb, &callback_shim, 0), S_OK);
	
	const wchar_t *HOST_NAME = L"Da Host";
	const unsigned char HOST_DATA[] = { 0x00, 0x01, 0x02, 0x03, 0x00, 0xAA, 0xBB, 0xCC };
	
	{
		testing = true;
		
		DPN_PLAYER_INFO info;
		memset(&info, 0, sizeof(info));
		
		info.dwSize      = sizeof(info);
		info.dwInfoFlags = DPNINFO_NAME | DPNINFO_DATA;
		info.pwszName    = (wchar_t*)(HOST_NAME);
		info.pvData      = (void*)(HOST_DATA);
		info.dwDataSize  = sizeof(HOST_DATA);
		
		/* SetPeerInfo() behaves as if called synchronously if called asynchronously
		 * before joining or creating a session.
		*/
		
		DPNHANDLE setinfo_handle;
		ASSERT_EQ(host->SetPeerInfo(&info, NULL, &setinfo_handle, 0), S_OK);
		
		Sleep(250);
		
		testing = false;
	}
	
	{
		DPN_APPLICATION_DESC app_desc;
		memset(&app_desc, 0, sizeof(app_desc));
		
		app_desc.dwSize = sizeof(app_desc);
		app_desc.guidApplication = APP_GUID_1;
		app_desc.pwszSessionName = (wchar_t*)(L"Session 1");
		
		IDP8AddressInstance addr;
		
		DWORD port = PORT;
		
		ASSERT_EQ(addr->SetSP(&CLSID_DP8SP_TCPIP), S_OK);
		ASSERT_EQ(addr->AddComponent(DPNA_KEY_PORT, &port, sizeof(DWORD), DPNA_DATATYPE_DWORD), S_OK);
		
		ASSERT_EQ(host->Host(&app_desc, &(addr.instance), 1, NULL, NULL, (void*)(0xB00), 0), S_OK);
	}
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&testing, &p1_seq, &p1_player_id]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
			{
				DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
				p1_player_id = cc->dpnidLocal;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++p1_seq;
			
			switch(seq)
			{
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	ASSERT_EQ(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		NULL,             /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	/* Give everything a moment to settle. */
	Sleep(250);
	
	/* Check the host has its own player data. */
	EXPECT_PEERINFO(host.instance, host_player_id,
		HOST_NAME, HOST_DATA, sizeof(HOST_DATA), (DPNPLAYER_LOCAL | DPNPLAYER_HOST));
	
	/* Check the peer has the host's data. */
	EXPECT_PEERINFO(p1.instance, host_player_id,
		HOST_NAME, HOST_DATA, sizeof(HOST_DATA), DPNPLAYER_HOST);
	
	EXPECT_EQ(host_seq, 0);
	EXPECT_EQ(p1_seq, 0);
}

TEST(DirectPlay8Peer, SetPeerInfoAsyncAfterPeerConnects)
{
	const wchar_t *HOST_NAME = L"Da Host";
	const unsigned char HOST_DATA[] = { 0x00, 0x01, 0x02, 0x03, 0x00, 0xAA, 0xBB, 0xCC };
	
	std::atomic<bool> testing(false);
	
	std::atomic<int> host_seq(0), p1_seq(0);
	DPNID host_player_id = -1, p1_player_id = -1;
	
	IDirectPlay8Peer *hostp;
	DPNHANDLE setinfo_handle;
	
	std::function<HRESULT(DWORD,PVOID)> host_cb =
		[&testing, &host_seq, &host_player_id, &hostp, &HOST_NAME, &HOST_DATA, &setinfo_handle]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CREATE_PLAYER && host_player_id == -1)
			{
				DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
				host_player_id = cp->dpnidPlayer;
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++host_seq;
			
			switch(seq)
			{
				case 1:
				{
					EXPECT_EQ(dwMessageType, DPN_MSGID_PEER_INFO);
					
					if(dwMessageType == DPN_MSGID_PEER_INFO)
					{
						DPNMSG_PEER_INFO *pi = (DPNMSG_PEER_INFO*)(pMessage);
						
						EXPECT_EQ(pi->dwSize,          sizeof(DPNMSG_PEER_INFO));
						EXPECT_EQ(pi->dpnidPeer,       host_player_id);
						EXPECT_EQ(pi->pvPlayerContext, (void*)(0xB00));
						
						EXPECT_PEERINFO(hostp, host_player_id,
							HOST_NAME, HOST_DATA, sizeof(HOST_DATA), (DPNPLAYER_LOCAL | DPNPLAYER_HOST));
					}
					
					break;
				}
				
				case 2:
				{
					EXPECT_EQ(dwMessageType, DPN_MSGID_ASYNC_OP_COMPLETE);
					
					if(dwMessageType == DPN_MSGID_ASYNC_OP_COMPLETE)
					{
						DPNMSG_ASYNC_OP_COMPLETE *oc = (DPNMSG_ASYNC_OP_COMPLETE*)(pMessage);
						
						EXPECT_EQ(oc->dwSize,        sizeof(DPNMSG_ASYNC_OP_COMPLETE));
						EXPECT_EQ(oc->hAsyncOp,      setinfo_handle);
						EXPECT_EQ(oc->pvUserContext, (void*)(0x9999));
						EXPECT_EQ(oc->hResultCode,   S_OK);
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance host;
	hostp = host.instance;
	
	ASSERT_EQ(host->Initialize(&host_cb, &callback_shim, 0), S_OK);
	
	{
		DPN_APPLICATION_DESC app_desc;
		memset(&app_desc, 0, sizeof(app_desc));
		
		app_desc.dwSize = sizeof(app_desc);
		app_desc.guidApplication = APP_GUID_1;
		app_desc.pwszSessionName = (wchar_t*)(L"Session 1");
		
		IDP8AddressInstance addr;
		
		DWORD port = PORT;
		
		ASSERT_EQ(addr->SetSP(&CLSID_DP8SP_TCPIP), S_OK);
		ASSERT_EQ(addr->AddComponent(DPNA_KEY_PORT, &port, sizeof(DWORD), DPNA_DATATYPE_DWORD), S_OK);
		
		ASSERT_EQ(host->Host(&app_desc, &(addr.instance), 1, NULL, NULL, (void*)(0xB00), 0), S_OK);
	}
	
	IDirectPlay8Peer *p1p;
	
	std::function<HRESULT(DWORD,PVOID)> p1_cb =
		[&testing, &p1_seq, &p1_player_id, &host_player_id, &p1p, &HOST_NAME, &HOST_DATA]
		(DWORD dwMessageType, PVOID pMessage)
		{
			if(dwMessageType == DPN_MSGID_CONNECT_COMPLETE)
			{
				DPNMSG_CONNECT_COMPLETE *cc = (DPNMSG_CONNECT_COMPLETE*)(pMessage);
				p1_player_id = cc->dpnidLocal;
			}
			else if(dwMessageType == DPN_MSGID_CREATE_PLAYER)
			{
				DPNMSG_CREATE_PLAYER *cp = (DPNMSG_CREATE_PLAYER*)(pMessage);
				
				if(cp->dpnidPlayer == host_player_id)
				{
					cp->pvPlayerContext = (void*)(0x1234);
				}
			}
			
			if(!testing)
			{
				return DPN_OK;
			}
			
			int seq = ++p1_seq;
			
			switch(seq)
			{
				case 1:
				{
					EXPECT_EQ(dwMessageType, DPN_MSGID_PEER_INFO);
					
					if(dwMessageType == DPN_MSGID_PEER_INFO)
					{
						DPNMSG_PEER_INFO *pi = (DPNMSG_PEER_INFO*)(pMessage);
						
						EXPECT_EQ(pi->dwSize,          sizeof(DPNMSG_PEER_INFO));
						EXPECT_EQ(pi->dpnidPeer,       host_player_id);
						EXPECT_EQ(pi->pvPlayerContext, (void*)(0x1234));
						
						EXPECT_PEERINFO(p1p, host_player_id,
							HOST_NAME, HOST_DATA, sizeof(HOST_DATA), DPNPLAYER_HOST);
					}
					
					break;
				}
				
				default:
					ADD_FAILURE() << "Unexpected message of type " << dwMessageType <<", sequence " << seq;
					break;
			}
			
			return DPN_OK;
		};
	
	IDP8PeerInstance p1;
	p1p = p1.instance;
	
	ASSERT_EQ(p1->Initialize(&p1_cb, &callback_shim, 0), S_OK);
	
	DPN_APPLICATION_DESC connect_to_app;
	memset(&connect_to_app, 0, sizeof(connect_to_app));
	
	connect_to_app.dwSize = sizeof(connect_to_app);
	connect_to_app.guidApplication = APP_GUID_1;
	
	IDP8AddressInstance connect_to_addr(L"127.0.0.1", PORT);
	
	ASSERT_EQ(p1->Connect(
		&connect_to_app,  /* pdnAppDesc */
		connect_to_addr,  /* pHostAddr */
		NULL,             /* pDeviceInfo */
		NULL,             /* pdnSecurity */
		NULL,             /* pdnCredentials */
		NULL,             /* pvUserConnectData */
		0,                /* dwUserConnectDataSize */
		NULL,             /* pvPlayerContext */
		NULL,             /* pvAsyncContext */
		NULL,             /* phAsyncHandle */
		DPNCONNECT_SYNC   /* dwFlags */
	), S_OK);
	
	/* Give everything a moment to settle. */
	Sleep(250);
	
	testing = true;
	
	{
		
		DPN_PLAYER_INFO info;
		memset(&info, 0, sizeof(info));
		
		info.dwSize      = sizeof(info);
		info.dwInfoFlags = DPNINFO_NAME | DPNINFO_DATA;
		info.pwszName    = (wchar_t*)(HOST_NAME);
		info.pvData      = (void*)(HOST_DATA);
		info.dwDataSize  = sizeof(HOST_DATA);
		
		ASSERT_EQ(host->SetPeerInfo(&info, (void*)(0x9999), &setinfo_handle, 0), DPNSUCCESS_PENDING);
	}
	
	Sleep(250);
	
	/* Check the host has its own player data. */
	EXPECT_PEERINFO(host.instance, host_player_id,
		HOST_NAME, HOST_DATA, sizeof(HOST_DATA), (DPNPLAYER_LOCAL | DPNPLAYER_HOST));
	
	/* Check the peer has the host's data. */
	EXPECT_PEERINFO(p1.instance, host_player_id,
		HOST_NAME, HOST_DATA, sizeof(HOST_DATA), DPNPLAYER_HOST);
	
	testing = false;
	
	EXPECT_EQ(host_seq, 2);
	EXPECT_EQ(p1_seq, 1);
}
