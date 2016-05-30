#pragma once
#include <Windows.h>
#ifdef _DEBUG
#include <cassert>
#else
#define assert(a) (0)
#endif

class CHandle
{
public:
	CHandle() throw();
	CHandle(_Inout_ CHandle& h) throw();
	explicit CHandle(_In_ HANDLE h) throw();
	~CHandle() throw();

	CHandle& operator=(_Inout_ CHandle& h) throw();

	operator HANDLE() const throw();

	// Attach to an existing handle (takes ownership).
	void Attach(_In_ HANDLE h) throw();
	// Detach the handle from the object (releases ownership).
	HANDLE Detach() throw();

	// Close the handle.
	void Close() throw();

public:
	HANDLE m_h;
};

inline CHandle::CHandle() throw() :
	m_h( NULL )
{
}

inline CHandle::CHandle(_Inout_ CHandle& h) throw() :
	m_h( NULL )
{
	Attach( h.Detach() );
}

inline CHandle::CHandle(_In_ HANDLE h) throw() :
	m_h( h )
{
}

inline CHandle::~CHandle() throw()
{
	if( m_h != NULL )
	{
		Close();
	}
}

inline CHandle& CHandle::operator=(_Inout_ CHandle& h) throw()
{
	if( this != &h )
	{
		if( m_h != NULL )
		{
			Close();
		}
		Attach( h.Detach() );
	}

	return( *this );
}

inline CHandle::operator HANDLE() const throw()
{
	return( m_h );
}

inline void CHandle::Attach(_In_ HANDLE h) throw()
{
	assert( m_h == NULL );
	m_h = h;  // Take ownership
}

inline HANDLE CHandle::Detach() throw()
{
	HANDLE h;

	h = m_h;  // Release ownership
	m_h = NULL;

	return( h );
}

inline void CHandle::Close() throw()
{
	if( m_h != NULL )
	{
		::CloseHandle( m_h );
		m_h = NULL;
	}
}