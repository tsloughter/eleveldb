//
// Created by Paul A. Place on 1/6/16.
//

#ifndef BASHOUTILS_BUFFER_H
#define BASHOUTILS_BUFFER_H

#include <cstddef>
#include <cstring>
#include <stdexcept>

#include <leveldb/slice.h>

using leveldb::Slice;

namespace basho {
namespace utils {

// Buffer template class: This class provides basic buffer management, using a
// built-in buffer whose size is specified by the template parameter. This
// class is useful when you need a buffer whose size does not vary too often.
// Using a built-in buffer that can grow if needed allows allocating a buffer
// that is large enough most of the time but allows for growth when needed.
//
// NOTE: Because a call to the EnsureSize() method (or one of the Assign()
// methods, which call the EnsureSize() method) may reallocate the internal
// buffer, you must take care to refresh your buffer pointer after a call
// to EnsureSize().
//
// NOTE: The Buffer class has an optional property called BytesUsed. This
// property is maintained by the Assign() methods; if those methods are not
// used to populate the Buffer object, then the user must maintain the
// BytesUsed property manually (or not use the property).
//
// NOTE: If you try to do something stupid (such as setting the BytesUsed value
// to a number larger than the buffer size), then the method that detects the
// stupidity throws a std::logic_error exception.
template<size_t SIZE>
class Buffer
{
    char   m_Buffer[ SIZE ]; // built-in buffer, intended to avoid a heap alloc, if possible; NOTE: this is declared first to ensure optimal alignment of the user's buffer
    char*  m_pBuff;          // pointer to the actual buffer; either m_Buffer or a buffer from the heap
    size_t m_BuffSize;       // size of the buffer currently pointed to by m_pBuff
    size_t m_BytesUsed;      // number of bytes in the buffer currently used

public:
    // ctors and assignment operators
    //
    // NOTE: for now, if you want to copy/assign a Buffer with a different
    // built-in buffer size, you'll have to use one of the Assign() methods
    Buffer( size_t BuffSize = SIZE ) : m_pBuff( m_Buffer ), m_BuffSize( SIZE ), m_BytesUsed( 0 )
    {
        if ( BuffSize > m_BuffSize )
        {
            m_pBuff = new char[ BuffSize ];
            m_BuffSize = (NULL != m_pBuff) ? BuffSize : 0;
        }
    }

    Buffer( const Buffer& That ) : m_pBuff( m_Buffer ), m_BuffSize( SIZE ), m_BytesUsed( 0 )
    {
        // ensure we have a large enough buffer
        size_t buffSize = That.m_BuffSize;
        if ( buffSize > m_BuffSize )
        {
            m_pBuff = new char[ buffSize ];
            m_BuffSize = (NULL != m_pBuff) ? buffSize : 0;
        }

        // if buffer allocation was successful, copy the data
        if ( m_BuffSize >= buffSize )
        {
            ::memcpy( m_pBuff, That.m_pBuff, buffSize );
            m_BytesUsed = That.m_BytesUsed;
        }
    }

    Buffer& operator=( const Buffer& That )
    {
        if ( this != &That )
        {
            if ( EnsureSize( That.m_BuffSize ) )
            {
                ::memcpy( m_pBuff, That.m_pBuff, That.m_BuffSize );
                m_BytesUsed = That.m_BytesUsed;
            }
            else
            {
                // throw an exception?
            }
        }
        return *this;
    }

    ~Buffer()
    {
        ResetBuffer();
    }

    // frees any allocated resources, resetting back to the built-in buffer
    //
    // NOTE: any data in the buffer is lost
    void
    ResetBuffer()
    {
        if ( m_pBuff != m_Buffer )
        {
            delete [] m_pBuff;
        }
        m_pBuff     = m_Buffer;
        m_BuffSize  = sizeof m_Buffer;
        m_BytesUsed = 0;
    }

    // ensures the buffer is at least the specified size
    //
    // NOTE: any previous data in the buffer is preserved
    bool // true => buffer is now (at least) the specified size
    EnsureSize(
        size_t NewSize,          // IN:  new size of the buffer; if the buffer is already at least this large, nothing happens
        bool*  pRealloc = NULL ) // OUT: receives true if the buffer was reallocated, else false; only used if true returned by this method
    {
        bool success = true;
        if ( NewSize > m_BuffSize )
        {
            // we need to allocate a larger buffer
            char* pNewBuff = new char[ NewSize ];
            if ( NULL != pNewBuff )
            {
                // preserve the old buffer's contents
                ::memcpy( pNewBuff, m_pBuff, m_BuffSize );

                // clean up any previously-allocated resources
                ResetBuffer();

                m_pBuff = pNewBuff;
                m_BuffSize = NewSize;
                if ( pRealloc != NULL )
                {
                    *pRealloc = true;
                }
            }
            else
            {
                success = false;
            }
        }
        else if ( pRealloc != NULL )
        {
            *pRealloc = false;
        }
        return success;
    }

    ///////////////////////////////////
    // Accessor Methods
    void* GetBuffer() const { return static_cast<void*>( m_pBuff ); }

    char* GetCharBuffer() const { return m_pBuff; }

    size_t GetBuffSize() const { return m_BuffSize; }

    size_t GetBuiltinBuffSize() const { return SIZE; }

    ///////////////////////////////////
    // BytesUsed Methods
    //
    // These methods are used to manage the amount of data in the buffer;
    // this must be maintained by the user
    size_t GetBytesUsed() const { return m_BytesUsed; }
    void SetBytesUsed( size_t BytesUsed )
    {
        if ( BytesUsed <= m_BuffSize )
        {
            m_BytesUsed = BytesUsed;
        }
        else
        {
            throw std::logic_error( "BytesUsed value is too large" );
        }
    }

    bool IsEmpty() const { return m_BytesUsed == 0; }

    ///////////////////////////////////
    // Assignment Methods
    //
    // NOTE: These return true on success and false on failure (typically due
    // to an allocation failure).
    //
    // TODO: Write an override of Assign() that transfers ownership of an allocated buffer
    bool Assign( const char* pData, size_t SizeInBytes )
    {
        if ( !EnsureSize( SizeInBytes ) )
        {
            return false;
        }
        ::memcpy( m_pBuff, pData, SizeInBytes );
        m_BytesUsed = SizeInBytes;
        return true;
    }

    bool Assign( const Slice& Data )
    {
        return Assign( Data.data(), Data.size() );
    }

    ///////////////////////////////////
    // Comparison Methods
    //
    // NOTE: The methods that return an integer return 0 when the two buffers
    // contain the same data, else -1 (first is less than second) or +1 (first
    // is greater than second).
    int Compare( const char* pData, const size_t SizeInBytes, bool CompareBytesUsed ) const
    {
        // first determine how many bytes we compare from this object
        // (total buffer size or only BytesUsed)
        const size_t thisBytesToCompare = CompareBytesUsed ? m_BytesUsed : m_BuffSize;

        // use memcmp() to compare as many bytes as possible
        size_t bytesToCompare = thisBytesToCompare;
        if ( SizeInBytes < bytesToCompare )
        {
            bytesToCompare = SizeInBytes;
        }

        int retVal = 0;
        if ( bytesToCompare > 0 )
        {
            retVal = ::memcmp( m_pBuff, pData, bytesToCompare );
        }

        if ( 0 == retVal )
        {
            // so far the buffers contain the same bytes; does either buffer
            // contain more stuff?
            if ( thisBytesToCompare > SizeInBytes )
            {
                retVal = 1;
            }
            else if ( thisBytesToCompare < SizeInBytes )
            {
                retVal = -1;
            }
        }
        else if ( retVal < 0 )
        {
            // memcmp() only promises < 0, but we want -1
            retVal = -1;
        }
        else
        {
            retVal = 1;
        }
        return retVal;
    }

    int Compare( const char* pData, const size_t SizeInBytes ) const
    {
        bool compareBytesUsed = (m_BytesUsed > 0);
        return Compare( pData, SizeInBytes, compareBytesUsed );
    }

    int Compare( const Slice& Data ) const
    {
        return Compare( Data.data(), Data.size() );
    }

    bool operator==( const Slice& Data ) const { return 0 == Compare( Data ); }
    bool operator!=( const Slice& Data ) const { return 0 != Compare( Data ); }
};

} // namespace utils
} // namespace basho

#endif // BASHOUTILS_BUFFER_H