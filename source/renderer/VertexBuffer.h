///////////////////////////////////////////////////////////////////////////////
//
// Name:		VertexBuffer.h
// Author:		Rich Cross
// Contact:		rich@wildfiregames.com
//
///////////////////////////////////////////////////////////////////////////////

#ifndef _VERTEXBUFFER_H
#define _VERTEXBUFFER_H

#include "lib.h"
#include "res/tex.h"

#include <list>
#include <vector>

// absolute maximum (bytewise) size of each GL vertex buffer object
#define MAX_VB_SIZE_BYTES		(32*8192)

///////////////////////////////////////////////////////////////////////////////
// CVertexBuffer: encapsulation of ARB_vertex_buffer_object, also supplying 
// some additional functionality for batching and sharing buffers between
// multiple objects
class CVertexBuffer
{
public:
	// Batch: batch definition - defines indices into the VB to use when rendering,
	// and the texture used when doing so
	struct Batch {
		// list of indices into the vertex buffer of primitives within the batch
		std::vector<std::pair<size_t,u16*> > m_IndexData;
		// texture to apply when rendering batch
		Handle m_Texture;
	};

	// VBChunk: describes a portion of this vertex buffer
	struct VBChunk
	{
		// owning buffer
		CVertexBuffer* m_Owner;
		// start index of this chunk in owner
		size_t m_Index;
		// number of vertices used by chunk
		size_t m_Count;
	};

public:
	// constructor, destructor
	CVertexBuffer(size_t vertexSize,bool dynamic);
	~CVertexBuffer();

	// bind to this buffer; return pointer to address required as parameter
	// to glVertexPointer ( + etc) calls
	u8* Bind();

	// clear lists of all batches 
	void ClearBatchIndices();

	// add a batch to the render list for this buffer
	void AppendBatch(VBChunk* chunk,Handle texture,size_t numIndices,u16* indices);

	// update vertex data for given chunk
	void UpdateChunkVertices(VBChunk* chunk,void* data);

	// return this VBs batch list
	const std::vector<Batch*>& GetBatches() const { return m_Batches; }

protected:
	friend class CVertexBufferManager;		// allow allocate only via CVertexBufferManager
	
	// try to allocate a buffer of given number of vertices (each of given size), 
	// and with the given type - return null if no free chunks available
	VBChunk* Allocate(size_t vertexSize,size_t numVertices,bool dynamic);
	// return given chunk to this buffer
	void Release(VBChunk* chunk);
	
	
private:	
	// set of all possible batches that can be used by this VB
	std::vector<Batch*> m_Batches;
	// vertex size of this vertex buffer
	size_t m_VertexSize;
	// number of vertices of above size in this buffer
	size_t m_MaxVertices;
	// list of free chunks in this buffer
	std::list<VBChunk*> m_FreeList;
	// available free vertices - total of all free vertices in the free list
	size_t m_FreeVertices;
	// handle to the actual GL vertex buffer object
	GLuint m_Handle;
	// raw system memory for systems not supporting VBOs
	u8* m_SysMem;
	// type of the buffer - dynamic?
	bool m_Dynamic;
	// list of all spare batches, shared between all vbs
	static std::vector<Batch*> m_FreeBatches;
};

#endif