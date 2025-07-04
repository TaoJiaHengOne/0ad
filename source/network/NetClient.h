/* Copyright (C) 2025 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NETCLIENT_H
#define NETCLIENT_H


#include "network/FSM.h"
#include "network/NetFileTransfer.h"
#include "network/NetHost.h"
#include "network/NetMessage.h"
#include "scriptinterface/Object.h"

#include "ps/CStr.h"

#include <ctime>
#include <deque>
#include <optional>
#include <thread>

class CGame;
class CNetClientSession;
class CNetClientTurnManager;
class ScriptInterface;

typedef struct _ENetHost ENetHost;

// NetClient session FSM states
enum
{
	NCS_UNCONNECTED,
	NCS_CONNECT,
	NCS_HANDSHAKE,
	NCS_AUTHENTICATE,
	NCS_PREGAME,
	NCS_LOADING,
	NCS_JOIN_SYNCING,
	NCS_INGAME
};

/**
 * Network client.
 * This code is run by every player (including the host, if they are not
 * a dedicated server).
 * It provides an interface between the GUI, the network (via CNetClientSession),
 * and the game (via CGame and CNetClientTurnManager).
 */
class CNetClient : public CFsm<CNetClient>
{
	NONCOPYABLE(CNetClient);

public:
	/**
	 * Construct a client associated with the given game object.
	 * The game must exist for the lifetime of this object.
	 */
	CNetClient(CGame* game);

	virtual ~CNetClient();

	/**
	 * We assume that adding a tracing function that's only called
	 * during GC is better for performance than using a
	 * PersistentRooted<T> where each value needs to be added to
	 * the root set.
	 */
	static void Trace(JSTracer *trc, void *data)
	{
		reinterpret_cast<CNetClient*>(data)->TraceMember(trc);
	}

	void TraceMember(JSTracer *trc);

	/**
	 * Set the user's name that will be displayed to all players.
	 * This must not be called after the connection setup.
	 */
	void SetUserName(const CStrW& username);

	/**
	 * Store the JID of the host.
	 * This is needed for the secure lobby authentication.
	 */
	void SetHostJID(const CStr& jid);

	void SetControllerSecret(const std::string& secret);

	bool IsController() const { return m_IsController; }

	/**
	 * Set the game password.
	 * Must be called after SetUserName, as that is used to hash further.
	 */
	void SetGamePassword(const CStr& hashedPassword);

	/**
	 * Returns the GUID of the local client.
	 * Used for distinguishing observers.
	 */
	CStr GetGUID() const { return m_GUID; }

	/**
	 * Set connection data to the remote networked server.
	 * @param address IP address or host name to connect to
	 */
	void SetupServerData(CStr address, u16 port);

	/**
	 * Set up a connection to the remote networked server.
	 * Must call SetupServerData first.
	 * @return true on success, false on connection failure
	 */
	bool SetupConnection(ENetHost* enetClient);

	/**
	 * Request connection information over the lobby.
	 */
	void SetupConnectionViaLobby();

	/**
	 * Connect to the remote networked server using lobby.
	 * Push netstatus messages on failure.
	 * @param localNetwork - if true, assume we are trying to connect on the local network.
	 * @return true on success, false on connection failure
	 */
	bool TryToConnectWithSTUN(const CStr& hostJID, bool localNetwork);

	/**
	 * Destroy the connection to the server.
	 * This client probably cannot be used again.
	 */
	void DestroyConnection();

	/**
	 * Poll the connection for messages from the server and process them, and send
	 * any queued messages.
	 * This must be called frequently (i.e. once per frame).
	 */
	void Poll();

	/**
	 * Locally triggers a GUI message if the connection to the server is being lost or has bad latency.
	 */
	void CheckServerConnection();

	/**
	 * Retrieves the next queued GUI message, and removes it from the queue.
	 * The returned value is in the GetScriptInterface() JS context.
	 *
	 * This is the only mechanism for the networking code to send messages to
	 * the GUI - it is pull-based (instead of push) so the engine code does not
	 * need to know anything about the code structure of the GUI scripts.
	 *
	 * The structure of the messages is <code>{ "type": "...", ... }</code>.
	 * The exact types and associated data are not specified anywhere - the
	 * implementation and GUI scripts must make the same assumptions.
	 *
	 * @return next message, or the value 'undefined' if the queue is empty
	 */
	void GuiPoll(JS::MutableHandleValue);

	/**
	 * Add a message to the queue, to be read by GuiPoll.
	 * The script value must be in the GetScriptInterface() JS context.
	 */
	template<typename... Args>
	void PushGuiMessage(Args const&... args)
	{
		ScriptRequest rq(GetScriptInterface());

		JS::RootedValue message(rq.cx);
		Script::CreateObject(rq, &message, args...);
		m_GuiMessageQueue.push_back(JS::Heap<JS::Value>(message));
	}

	/**
	 * Return a concatenation of all messages in the GUI queue,
	 * for test cases to easily verify the queue contents.
	 */
	std::string TestReadGuiMessages();

	/**
	 * Get the script interface associated with this network client,
	 * which is equivalent to the one used by the CGame in the constructor.
	 */
	const ScriptInterface& GetScriptInterface();

	/**
	 * Send a message to the server.
	 * @param message message to send
	 * @return true on success
	 */
	bool SendMessage(const CNetMessage* message);

	/**
	 * Call when the network connection has been successfully initiated.
	 */
	void HandleConnect();

	/**
	 * Call when the network connection has been lost.
	 */
	void HandleDisconnect(u32 reason);

	/**
	 * Call when a message has been received from the network.
	 */
	bool HandleMessage(CNetMessage* message);

	/**
	 * Call when the game has started and all data files have been loaded,
	 * to signal to the server that we are ready to begin the game.
	 */
	void LoadFinished();

	void SendGameSetupMessage(JS::MutableHandleValue attrs, const ScriptInterface& scriptInterface);

	void SendAssignPlayerMessage(const int playerID, const CStr& guid);

	/**
	 * @param text The message to send.
	 * @param receivers The GUID of the receiving clients. If empty send it to
	 *	all clients.
	 */
	void SendChatMessage(const std::wstring& text, std::optional<std::vector<std::string>> receivers);

	void SendReadyMessage(const int status);

	void SendClearAllReadyMessage();

	void SendStartGameMessage(const CStr& initAttribs);

	void SendStartSavedGameMessage(const CStr& initAttribs, const CStr& savedState);

	/**
	 * Call when the client (player or observer) has sent a flare.
	 */
	void SendFlareMessage(const CStr& positionX, const CStr& positionY, const CStr& positionZ);

	/**
	 * Call when the client has rejoined a running match and finished
	 * the loading screen.
	 */
	void SendRejoinedMessage();

	/**
	 * Call to kick/ban a client
	 */
	void SendKickPlayerMessage(const CStrW& playerName, bool ban);

	/**
	 * Call when the client has paused or unpaused the game.
	 */
	void SendPausedMessage(bool pause);

	/**
	 * @return Whether the NetClient is shutting down.
	 */
	bool ShouldShutdown() const;

	/**
	 * Called when fetching connection data from the host failed, to inform JS code.
	 */
	void HandleGetServerDataFailed(const CStr& error);
private:

	void SendAuthenticateMessage();

	// Net message / FSM transition handlers
	static bool OnConnect(CNetClient* client, CFsmEvent* event);
	static bool OnHandshake(CNetClient* client, CFsmEvent* event);
	static bool OnHandshakeResponse(CNetClient* client, CFsmEvent* event);
	static bool OnAuthenticateRequest(CNetClient* client, CFsmEvent* event);
	static bool OnAuthenticate(CNetClient* client, CFsmEvent* event);
	static bool OnChat(CNetClient* client, CFsmEvent* event);
	static bool OnReady(CNetClient* client, CFsmEvent* event);
	static bool OnGameSetup(CNetClient* client, CFsmEvent* event);
	static bool OnPlayerAssignment(CNetClient* client, CFsmEvent* event);
	static bool OnInGame(CNetClient* client, CFsmEvent* event);
	static bool OnGameStart(CNetClient* client, CFsmEvent* event);
	static bool OnSavedGameStart(CNetClient* client, CFsmEvent* event);
	static bool OnJoinSyncStart(CNetClient* client, CFsmEvent* event);
	static bool OnJoinSyncEndCommandBatch(CNetClient* client, CFsmEvent* event);
	static bool OnFlare(CNetClient* client, CFsmEvent* event);
	static bool OnRejoined(CNetClient* client, CFsmEvent* event);
	static bool OnKicked(CNetClient* client, CFsmEvent* event);
	static bool OnClientTimeout(CNetClient* client, CFsmEvent* event);
	static bool OnClientPerformance(CNetClient* client, CFsmEvent* event);
	static bool OnClientsLoading(CNetClient* client, CFsmEvent* event);
	static bool OnClientPaused(CNetClient* client, CFsmEvent* event);
	static bool OnLoadedGame(CNetClient* client, CFsmEvent* event);

	/**
	 * Take ownership of a session object, and use it for all network communication.
	 */
	void SetAndOwnSession(CNetClientSession* session);

	/**
	 * Starts a game with the specified init attributes and saved state. Called
	 * by the start game and start saved game callbacks.
	 */
	void StartGame(const JS::MutableHandleValue initAttributes, const std::string& savedState);

	/**
	 * Push a message onto the GUI queue listing the current player assignments.
	 */
	void PostPlayerAssignmentsToScript();

	CGame *m_Game;
	CStrW m_UserName;

	CStr m_HostJID;
	CStr m_ServerAddress;
	u16 m_ServerPort;

	/**
	 * Password to join the game.
	 */
	CStr m_Password;

	/// The 'secret' used to identify the controller of the game.
	std::string m_ControllerSecret;

	/// Note that this is just a "gui hint" with no actual impact on being controller.
	bool m_IsController = false;

	/// Current network session (or NULL if not connected)
	CNetClientSession* m_Session;

	std::thread m_PollingThread;

	/// Turn manager associated with the current game (or NULL if we haven't started the game yet)
	CNetClientTurnManager* m_ClientTurnManager;

	/// Unique-per-game identifier of this client, used to identify the sender of simulation commands
	u32 m_HostID;

	/// True if the player is currently rejoining or has already rejoined the game.
	bool m_Rejoin;

	/// Latest copy of player assignments heard from the server
	PlayerAssignmentMap m_PlayerAssignments;

	/// Globally unique identifier to distinguish users beyond the lifetime of a single network session
	CStr m_GUID;

	/// Queue of messages for GuiPoll
	std::deque<JS::Heap<JS::Value>> m_GuiMessageQueue;

	/// Serialized game state received when joining an in-progress game
	std::string m_JoinSyncBuffer;

	std::string m_SavedState;

	/// Time when the server was last checked for timeouts and bad latency
	std::time_t m_LastConnectionCheck;

	/// Record of the server engine version and loaded mods
	CSrvHandshakeMessage m_ServerHandshake;
};

/// Global network client for the standard game
extern CNetClient *g_NetClient;

#endif	// NETCLIENT_H
