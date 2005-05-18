#ifndef _NetMessage_H
#define _NetMessage_H

#include "lib/types.h"
#include "Serialization.h"
#include "Network/SocketBase.h"

// We need the enum from AllNetMessages.h, but we can't create any classes in
// AllNetMessages, since they in turn require CNetMessage to be defined
#define ALLNETMSGS_DONT_CREATE_NMTS
#include "AllNetMessages.h"
#undef ALLNETMSGS_DONT_CREATE_NMTS

class CCommand;
class CEntityList;

/**
 * The base class for network messages
 */
class CNetMessage: public ISerializable
{
	ENetMessageType m_Type;
protected:
	inline CNetMessage(ENetMessageType type):
		m_Type(type)
	{}

public:
	virtual ~CNetMessage();

	inline ENetMessageType GetType() const
	{ return m_Type; }

	/**
	 * @returns The length of the message when serialized.
	 */
	virtual uint GetSerializedLength() const;
	/**
	 * Serialize the message into the buffer. The buffer will have the size
	 * returned from the last call to GetSerializedLength()
	 */
	virtual u8 *Serialize(u8 *buffer) const;
	virtual const u8 *Deserialize(const u8 *pos, const u8 *end);
	
	/**
	 * Make a string representation of the message. The default implementation
	 * returns the empty string
	 */
	virtual CStr GetString() const;
	inline operator CStr() const
	{ return GetString(); }

	/**
	 * Copy the message
	 */
	virtual CNetMessage *Copy() const;

	/**
	 * Deserialize a net message, using the globally registered deserializers.
	 *
	 * @param type The NetMessageType of the message
	 * @param buffer A pointer to the buffer holding the message data
	 * @param length The length of the message data
	 *
	 * @returns a pointer to a newly created CNetMessage subclass, or NULL if
	 * there was an error in data format.
	 */
	static CNetMessage *DeserializeMessage(ENetMessageType type, u8 *buffer, uint length);
	
	/**
	 * Register a selection of message types as JS constants.
	 * The constant's names will be the same as those of the enums
	 */
	static void ScriptingInit();
	
	static CCommand *CommandFromJSArgs(const CEntityList &entities, JSContext* cx, uintN argc, jsval* argv);
};

typedef CNetMessage * (*NetMessageDeserializer) (const u8 *buffer, uint length);

#include "EntityHandles.h"

struct SNetMessageDeserializerRegistration
{
	ENetMessageType m_Type;
	NetMessageDeserializer m_pDeserializer;
};

// This time, the classes are created
#include "AllNetMessages.h"

#endif // #ifndef _NetMessage_H
