//
// Created by Paul A. Place on 12/16/15.
//

#ifndef BASHO_BIGSET_ACC_H
#define BASHO_BIGSET_ACC_H

#include <list>

#include "BigsetClock.h"
#include "util/buffer.h"

namespace basho {
namespace bigset {

class BigsetAccumulator
{
private:
    typedef utils::Buffer<32> Buffer;

    Actor       m_ThisActor;
    Buffer      m_CurrentSetName;
    Buffer      m_CurrentElement;
    BigsetClock m_CurrentContext;
    Dots        m_CurrentDots;

    Buffer      m_ReadyKey;
    Buffer      m_ReadyValue;
    bool        m_RecordReady;

    void FinalizeElement();

public:
    BigsetAccumulator( const Actor& ThisActor ) : m_ThisActor( ThisActor ), m_RecordReady( false )
    { }

    ~BigsetAccumulator() { }

    void AddRecord( Slice key, Slice value );

    bool
    RecordReady()
    {
        return m_RecordReady;
    }

    void
    GetCurrentElement( Slice& key, Slice& value )
    {
        if ( m_RecordReady )
        {
            Slice readyKey( m_ReadyKey.GetCharBuffer(), m_ReadyKey.GetBuffSize() );
            key = readyKey;

            Slice readyValue( m_ReadyValue.GetCharBuffer(), m_ReadyValue.GetBuffSize() );
            value = readyValue;

            m_RecordReady = false; // prepare for the next record
        }
    }
};

} // namespace bigset
} // namespace basho

#endif // BASHO_BIGSET_ACC_H