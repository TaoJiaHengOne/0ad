/* $Id$

	Xeromyces - XMB reading library

	Philip Taylor (philip@zaynar.demon.co.uk / @wildfiregames.com)

*/

/* 

Brief outline:

XMB is a binary representation of XML, with some limitations
but much more efficiency (particularly for loading simple data
classes that don't need much initialisation).

Main limitations:
 * Only handles UTF16 internally. (It's meant to be a feature, but
   can be detrimental if it's always being converted back to
   ASCII.)
 * Can't correctly handle mixed text/elements inside elements -
   "<div> <b> Text </b> </div>" and "<div> Te<b/>xt </div>" are
   considered identical.
 * Tries to avoid using strings - you usually have to load the
   numeric IDs and use them instead.
 * Case-sensitive (but converts all element/attribute names in
   the XML file to lowercase, so you only have to be careful in
   the code)


Theoretical file structure:

XMB_File {
	char Header[4]; // because everyone has one; currently "XMB0"

	int ElementNameCount;
	ZStrA ElementNames[];

	int AttributeNameCount;
	ZStrA AttributeNames[];

	XMB_Node Root;
}

XMB_Node {
0)	int Length; // of entire struct, so it can be skipped over

4)	int ElementName;

8)	int AttributeCount;
12)	int ChildCount;

16)	int ChildrenOffset; // == sizeof(Text)+sizeof(Attributes)
20)	XMB_Text Text;
	XMB_Attribute Attributes[];
	XMB_Node Children[];

}

XMB_Attribute {
	int Name;
	ZStrW Value;
}

ZStrA {
	int Length; // in bytes
	char* Text; // null-terminated ASCII
}

ZStrW {
	int Length; // in bytes
	char16* Text; // null-terminated UTF16
}

XMB_Text {
20)	int Length; // 0 if there's no text, else 4+sizeof(Text) in bytes including terminator
	// If Length != 0:
24)	int LineNumber; // for e.g. debugging scripts
28)	char16* Text; // null-terminated UTF16
}


*/

#ifndef _XEROXMB_H_
#define _XEROXMB_H_

// Define to use a std::map for name lookups rather than a linear search.
// (The map is usually slower.)
//#define XERO_USEMAP 

#include <string>

#include "ps/utf16string.h"

#ifdef XERO_USEMAP
# include <map>
#endif

// File headers, to make sure it doesn't try loading anything other than an XMB
extern const int HeaderMagic;
extern const char* HeaderMagicStr;

class XMBElement;
class XMBElementList;
class XMBAttributeList;


class XMBFile
{
public:

	XMBFile() : m_Pointer(NULL) {};

	// Initialise from the contents of an XMB file.
	// FileData must remain allocated and unchanged while
	// the XMBFile is being used.
	void Initialise(char* FileData);

	// Returns the root element
	XMBElement getRoot() const;

	
	// Returns internal ID for a given ASCII element/attribute string.
	int getElementID(const char* Name) const;
	int getAttributeID(const char* Name) const;

	// For lazy people (e.g. me) when speed isn't vital:

	// Returns element/attribute string for a given internal ID
	std::string getElementString(const int ID) const;
	std::string getAttributeString(const int ID) const;

private:
	char* m_Pointer;

#ifdef XERO_USEMAP
	std::map<std::string, int> m_ElementNames;
	std::map<std::string, int> m_AttributeNames;
#else
	int m_ElementNameCount;
	int m_AttributeNameCount;
	char* m_ElementPointer;
	char* m_AttributePointer;
#endif

	std::string ReadZStrA();
};

class XMBElement
{
public:
	// janwas: default ctor needed for ReadXML
	XMBElement()
		: m_Pointer(0) {}

	XMBElement(char* offset)
		: m_Pointer(offset)	{}

	int getNodeName() const;
	XMBElementList getChildNodes() const;
	XMBAttributeList getAttributes() const;
	utf16string getText() const;
	int getLineNumber() const;

private:
	// Pointer to the start of the node
	char* m_Pointer;
};

class XMBElementList
{
public:
	// janwas: default ctor needed for ReadXML
	XMBElementList()
		: Count(0), m_Pointer(0), m_LastItemID(-2) {}

	XMBElementList(char* offset, int count)
		: Count(count),
		  m_Pointer(offset),
		  m_LastItemID(-2) {} // use -2 because it isn't x-1 where x is a non-negative integer

	XMBElement item(const int id); // returns Children[id]

	int Count;

private:
	char* m_Pointer;

	// For optimised sequential access:
	int m_LastItemID;
	char* m_LastPointer;
};


struct XMBAttribute
{
	XMBAttribute() {}
	XMBAttribute(int name, utf16string value)
		: Name(name), Value(value) {};

	int Name;
	utf16string Value;
};

class XMBAttributeList
{
public:
	XMBAttributeList(char* offset, int count)
		: Count(count), m_Pointer(offset), m_LastItemID( -2 ) {};

	// Get the attribute value directly (unlike Xerces)
	utf16string getNamedItem(const int AttributeName) const;

	// Returns an attribute by position in the list
	XMBAttribute item(const int id);

	int Count;

private:
	// Pointer to start of attribute list
	char* m_Pointer;

	// For optimised sequential access:
	int m_LastItemID;
	char* m_LastPointer;
};

#endif // _XEROXMB_H_
