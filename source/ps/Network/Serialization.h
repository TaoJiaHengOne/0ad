#ifndef _Serialization_H
#define _Serialization_H

#include "types.h"
#include "lib.h"

#define Serialize_int_1(_pos, _val) \
	STMT( *((_pos)++) = (u8)(_val&0xff); )

#define Serialize_int_2(_pos, _val) STMT(\
	Serialize_int_1(_pos, _val>>8); \
	Serialize_int_1(_pos, _val); \
)

#define Serialize_int_3(_pos, _val) STMT(\
	Serialize_int_1(_pos, _val>>16); \
	Serialize_int_2(_pos, _val); \
)

#define Serialize_int_4(_pos, _val) STMT(\
	Serialize_int_1(_pos, _val>>24); \
	Serialize_int_3(_pos, _val); \
)

#define Serialize_int_8(_pos, _val) STMT(\
	Serialize_int_4(_pos, _val>>32); \
	Serialize_int_4(_pos, _val); \
)

#define __shift_de(_pos, _val) STMT( \
	(_val) <<= 8; \
	(_val) += *((_pos)++); )

#define Deserialize_int_1(_pos, _val) STMT(\
	(_val) = *((_pos)++); )

#define Deserialize_int_2(_pos, _val) STMT(\
	Deserialize_int_1(_pos, _val); \
	__shift_de(_pos, _val); )

#define Deserialize_int_3(_pos, _val) STMT(\
	Deserialize_int_2(_pos, _val); \
	__shift_de(_pos, _val); )

#define Deserialize_int_4(_pos, _val) STMT(\
	Deserialize_int_3(_pos, _val); \
	__shift_de(_pos, _val); )

#define Deserialize_int_8(_pos, _val) STMT(\
	uint32 _v1; uint32 _v2; \
	Deserialize_int_4(_pos, _v1); \
	Deserialize_int_4(_pos, _v2); \
	_val=_v1<<32 | _v2; )

/**
 * An interface for serializable objects. For a serializable object to be usable
 * as a network message field, it must have a constructor without arguments.
 */
class ISerializable
{
public:
	/**
	 * Return the length of the serialized form of this object
	 */
	virtual uint GetSerializedLength() const = 0;
	/**
	 * Serialize the object into the passed buffer.
	 * 
	 * @return a pointer to the location in the buffer right after the
	 * serialized object
	 */
	virtual u8 *Serialize(u8 *buffer) const = 0;
	/**
	 * Deserialize the object (i.e. read in data from the buffer and initialize
	 * the object's fields). Note that it is up to the deserializer to detect
	 * errors in data format.
	 *
	 * @param buffer A pointer pointing to the start of the serialized data.
	 * @param end A pointer to the end of the message.
	 * 
	 * @returns a pointer to the location in the buffer right after the
	 * serialized object, or NULL if there was a data format error
	 */
	virtual const u8 *Deserialize(const u8 *buffer, const u8 *end) = 0;
};

#endif
