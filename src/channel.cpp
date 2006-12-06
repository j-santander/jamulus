/******************************************************************************\
 * Copyright (c) 2004-2006
 *
 * Author(s):
 *  Volker Fischer
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later 
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more 
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
\******************************************************************************/

#include "channel.h"


/******************************************************************************\
* CChannelSet                                                                  *
\******************************************************************************/
CChannelSet::CChannelSet()
{
    // make sure we have MAX_NUM_CHANNELS connections!!!
    // send message
    QObject::connect(&vecChannels[0],SIGNAL(MessReadyForSending(CVector<uint8_t>)),this,SLOT(OnSendProtMessCh0(CVector<uint8_t>)));
    QObject::connect(&vecChannels[1],SIGNAL(MessReadyForSending(CVector<uint8_t>)),this,SLOT(OnSendProtMessCh1(CVector<uint8_t>)));
    QObject::connect(&vecChannels[2],SIGNAL(MessReadyForSending(CVector<uint8_t>)),this,SLOT(OnSendProtMessCh2(CVector<uint8_t>)));
    QObject::connect(&vecChannels[3],SIGNAL(MessReadyForSending(CVector<uint8_t>)),this,SLOT(OnSendProtMessCh3(CVector<uint8_t>)));
    QObject::connect(&vecChannels[4],SIGNAL(MessReadyForSending(CVector<uint8_t>)),this,SLOT(OnSendProtMessCh4(CVector<uint8_t>)));
    QObject::connect(&vecChannels[5],SIGNAL(MessReadyForSending(CVector<uint8_t>)),this,SLOT(OnSendProtMessCh5(CVector<uint8_t>)));
    QObject::connect(&vecChannels[6],SIGNAL(MessReadyForSending(CVector<uint8_t>)),this,SLOT(OnSendProtMessCh6(CVector<uint8_t>)));
    QObject::connect(&vecChannels[7],SIGNAL(MessReadyForSending(CVector<uint8_t>)),this,SLOT(OnSendProtMessCh7(CVector<uint8_t>)));
    QObject::connect(&vecChannels[8],SIGNAL(MessReadyForSending(CVector<uint8_t>)),this,SLOT(OnSendProtMessCh8(CVector<uint8_t>)));
    QObject::connect(&vecChannels[9],SIGNAL(MessReadyForSending(CVector<uint8_t>)),this,SLOT(OnSendProtMessCh9(CVector<uint8_t>)));

    // request jitter buffer size
    QObject::connect(&vecChannels[0],SIGNAL(NewConnection()),this,SLOT(OnNewConnectionCh0()));
    QObject::connect(&vecChannels[1],SIGNAL(NewConnection()),this,SLOT(OnNewConnectionCh1()));
    QObject::connect(&vecChannels[2],SIGNAL(NewConnection()),this,SLOT(OnNewConnectionCh2()));
    QObject::connect(&vecChannels[3],SIGNAL(NewConnection()),this,SLOT(OnNewConnectionCh3()));
    QObject::connect(&vecChannels[4],SIGNAL(NewConnection()),this,SLOT(OnNewConnectionCh4()));
    QObject::connect(&vecChannels[5],SIGNAL(NewConnection()),this,SLOT(OnNewConnectionCh5()));
    QObject::connect(&vecChannels[6],SIGNAL(NewConnection()),this,SLOT(OnNewConnectionCh6()));
    QObject::connect(&vecChannels[7],SIGNAL(NewConnection()),this,SLOT(OnNewConnectionCh7()));
    QObject::connect(&vecChannels[8],SIGNAL(NewConnection()),this,SLOT(OnNewConnectionCh8()));
    QObject::connect(&vecChannels[9],SIGNAL(NewConnection()),this,SLOT(OnNewConnectionCh9()));
}

void CChannelSet::CreateAndSendChanListForAllConClients()
{
    int                        i;
    CVector<CChannelShortInfo> vecChanInfo ( 0 );

    // look for free channels
    for ( i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        if ( vecChannels[i].IsConnected() )
        {
            // append channel ID, IP address and channel name to storing vectors
            vecChanInfo.Add ( CChannelShortInfo (
                i, // ID
                vecChannels[i].GetAddress().InetAddr.ip4Addr(), // IP address
                vecChannels[i].GetName() /* name */ ) );
        }
    }

    // now send connected channels list to all connected clients
    for ( i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        if ( vecChannels[i].IsConnected() )
        {
            // send message
            vecChannels[i].CreateConClientListMes ( vecChanInfo );
        }
    }
}

int CChannelSet::GetFreeChan()
{
    /* look for a free channel */
    for ( int i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        if ( !vecChannels[i].IsConnected() )
        {
            return i;
        }
    }

    /* no free channel found, return invalid ID */
    return INVALID_CHANNEL_ID;
}

int CChannelSet::CheckAddr ( const CHostAddress& Addr )
{
    CHostAddress InetAddr;

    /* Check for all possible channels if IP is already in use */
    for ( int i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        if ( vecChannels[i].GetAddress ( InetAddr ) )
        {
            /* IP found, return channel number */
            if ( InetAddr == Addr )
            {
                return i;
            }
        }
    }

    /* IP not found, return invalid ID */
    return INVALID_CHANNEL_ID;
}

bool CChannelSet::PutData ( const CVector<unsigned char>& vecbyRecBuf,
                            const int iNumBytesRead,
                            const CHostAddress& HostAdr )
{
    bool bRet            = false;
    bool bCreateChanList = false;

    Mutex.lock();
    {
        bool bChanOK = true;

        /* get channel ID --------------------------------------------------- */
        /* check address */
        int iCurChanID = CheckAddr ( HostAdr );

        if ( iCurChanID == INVALID_CHANNEL_ID )
        {
            /* a new client is calling, look for free channel */
            iCurChanID = GetFreeChan();

            if ( iCurChanID != INVALID_CHANNEL_ID )
            {
                // initialize current channel by storing the calling host
                // address
                vecChannels[iCurChanID].SetAddress ( HostAdr );

                // reset the channel gains of current channel, at the same
                // time reset gains of this channel ID for all other channels
                for ( int i = 0; i < MAX_NUM_CHANNELS; i++ )
                {
                    vecChannels[iCurChanID].SetGain ( i, (double) 1.0 );

                    // other channels (we do not distinguish the case if
                    // i == iCurChanID for simplicity)
                    vecChannels[i].SetGain ( iCurChanID, (double) 1.0 );
                }

                // a new client connected to the server, set flag to create and
                // send all clients the updated channel list, we cannot create
                // the message here since the received data has to be put to the
                // channel first so that this channel is marked as connected
                bCreateChanList = true;
            }
            else
            {
                /* no free channel available */
                bChanOK = false;

                // create and send "server full" message
// TODO

            }
        }


        /* put received data in jitter buffer ------------------------------- */
        if ( bChanOK )
        {
            /* put packet in socket buffer */
            switch ( vecChannels[iCurChanID].PutData ( vecbyRecBuf, iNumBytesRead ) )
            {
            case PS_AUDIO_OK:
                PostWinMessage ( MS_JIT_BUF_PUT, MUL_COL_LED_GREEN, iCurChanID );
                bRet = true; // in case we have an audio packet, return true
                break;

            case PS_AUDIO_ERR:
                PostWinMessage ( MS_JIT_BUF_PUT, MUL_COL_LED_RED, iCurChanID );
                bRet = true; // in case we have an audio packet, return true
                break;

            case PS_PROT_ERR:
                PostWinMessage ( MS_JIT_BUF_PUT, MUL_COL_LED_YELLOW, iCurChanID );
                break;
            }
        }


        // after data is put in channel buffer, create channel list message if
        // requested
        if ( bCreateChanList )
        {
            CreateAndSendChanListForAllConClients();
        }
    }
    Mutex.unlock();

    return bRet;
}

void CChannelSet::GetBlockAllConC ( CVector<int>&              vecChanID,
                                    CVector<CVector<double> >& vecvecdData,
                                    CVector<CVector<double> >& vecvecdGains )
{
    int i, j;

    // init temporal data vector and clear input buffers
    CVector<double> vecdData ( MIN_BLOCK_SIZE_SAMPLES );

    vecChanID.Init    ( 0 );
    vecvecdData.Init  ( 0 );
    vecvecdGains.Init ( 0 );

    // make put and get calls thread safe. Do not forget to unlock mutex
    // afterwards!
    Mutex.lock();
    {
        // check all possible channels
        for ( i = 0; i < MAX_NUM_CHANNELS; i++ )
        {
            // read out all input buffers to decrease timeout counter on
            // disconnected channels
            const bool bGetOK = vecChannels[i].GetData ( vecdData );

            if ( vecChannels[i].IsConnected() )
            {
                // add ID and data
                vecChanID.Add ( i );

                const int iOldSize = vecvecdData.Size();
                vecvecdData.Enlarge ( 1 );
                vecvecdData[iOldSize].Init ( vecdData.Size() );
                vecvecdData[iOldSize] = vecdData;

                // send message for get status (for GUI)
                if ( bGetOK )
                {
                    PostWinMessage ( MS_JIT_BUF_GET, MUL_COL_LED_GREEN, i );
                }
                else
                {
                    PostWinMessage ( MS_JIT_BUF_GET, MUL_COL_LED_RED, i );
                }
            }
        }

        // now that we know the IDs of the connected clients, get gains
        const int iNumCurConnChan = vecChanID.Size();
        vecvecdGains.Init ( iNumCurConnChan );

        for ( i = 0; i < iNumCurConnChan; i++ )
        {
            vecvecdGains[i].Init ( iNumCurConnChan );

            for ( j = 0; j < iNumCurConnChan; j++ )
            {
                // The second index of "vecvecdGains" does not represent
                // the channel ID! Therefore we have to use "vecChanID" to
                // query the IDs of the currently connected channels
                vecvecdGains[i][j] = vecChannels[i].GetGain( vecChanID[j] );
            }
        }
    }
    Mutex.unlock(); // release mutex
}

void CChannelSet::GetConCliParam ( CVector<CHostAddress>& vecHostAddresses,
                                   CVector<int>&          veciJitBufSize,
                                   CVector<int>&          veciNetwOutBlSiFact,
                                   CVector<int>&          veciNetwInBlSiFact )
{
    CHostAddress InetAddr;

    /* init return values */
    vecHostAddresses.Init    ( MAX_NUM_CHANNELS );
    veciJitBufSize.Init      ( MAX_NUM_CHANNELS );
    veciNetwOutBlSiFact.Init ( MAX_NUM_CHANNELS );
    veciNetwInBlSiFact.Init  ( MAX_NUM_CHANNELS );

    /* Check all possible channels */
    for ( int i = 0; i < MAX_NUM_CHANNELS; i++ )
    {
        if ( vecChannels[i].GetAddress ( InetAddr ) )
        {
            /* get requested data */
            vecHostAddresses[i]    = InetAddr;
            veciJitBufSize[i]      = vecChannels[i].GetSockBufSize ();
            veciNetwOutBlSiFact[i] = vecChannels[i].GetNetwBufSizeFactOut ();
            veciNetwInBlSiFact[i]  = vecChannels[i].GetNetwBufSizeFactIn ();
        }
    }
}


/******************************************************************************\
* CChannel                                                                     *
\******************************************************************************/
CChannel::CChannel() : sName ( "" ),
    vecdGains ( MAX_NUM_CHANNELS, (double) 1.0 )
{
    // query all possible network in buffer sizes for determining if an
    // audio packet was received
    for ( int i = 0; i < MAX_NET_BLOCK_SIZE_FACTOR; i++ )
    {
        // network block size factor must start from 1 -> ( i + 1 )
        vecNetwInBufSizes[i] = AudioCompressionIn.Init (
            ( i + 1 ) * MIN_BLOCK_SIZE_SAMPLES,
            CAudioCompression::CT_IMAADPCM );
    }

    iCurNetwInBlSiFact = DEF_NET_BLOCK_SIZE_FACTOR;

    /* init the socket buffer */
    SetSockBufSize ( DEF_NET_BUF_SIZE_NUM_BL );

    // set initial input and output block size factors
    SetNetwBufSizeFactOut ( iCurNetwInBlSiFact );
    SetNetwInBlSiFact ( iCurNetwInBlSiFact );

    /* init time-out for the buffer with zero -> no connection */
    iConTimeOut = 0;


    /* connections ---------------------------------------------------------- */
    QObject::connect ( &Protocol,
        SIGNAL ( MessReadyForSending ( CVector<uint8_t> ) ),
        this, SLOT ( OnSendProtMessage ( CVector<uint8_t> ) ) );

    QObject::connect ( &Protocol,
        SIGNAL ( ChangeJittBufSize ( int ) ),
        this, SLOT ( OnJittBufSizeChange ( int ) ) );

    QObject::connect ( &Protocol,
        SIGNAL ( ReqJittBufSize() ),
        SIGNAL ( ReqJittBufSize() ) );

    QObject::connect ( &Protocol,
        SIGNAL ( ConClientListMesReceived ( CVector<CChannelShortInfo> ) ),
        SIGNAL ( ConClientListMesReceived ( CVector<CChannelShortInfo> ) ) );

    QObject::connect ( &Protocol,
        SIGNAL ( ChangeNetwBlSiFact ( int ) ),
        this, SLOT ( OnNetwBlSiFactChange ( int ) ) );
}

void CChannel::SetNetwInBlSiFact ( const int iNewBlockSizeFactor )
{
    // store new value
    iCurNetwInBlSiFact = iNewBlockSizeFactor;

    /* init audio compression unit */
    iAudComprSizeIn = AudioCompressionIn.Init (
        iNewBlockSizeFactor * MIN_BLOCK_SIZE_SAMPLES,
        CAudioCompression::CT_IMAADPCM );

    // initial value for connection time out counter
    iConTimeOutStartVal = ( CON_TIME_OUT_SEC_MAX * 1000 ) /
        ( iNewBlockSizeFactor * MIN_BLOCK_DURATION_MS );

    // socket buffer must be adjusted
    SetSockBufSize ( GetSockBufSize() );
}

void CChannel::SetNetwBufSizeFactOut ( const int iNewNetwBlSiFactOut )
{
    // store new value
    iCurNetwOutBlSiFact = iNewNetwBlSiFactOut;

    /* init audio compression unit */
    iAudComprSizeOut = AudioCompressionOut.Init (
        iNewNetwBlSiFactOut * MIN_BLOCK_SIZE_SAMPLES,
        CAudioCompression::CT_IMAADPCM );

    /* init conversion buffer */
    ConvBuf.Init ( iNewNetwBlSiFactOut * MIN_BLOCK_SIZE_SAMPLES );
}

void CChannel::OnSendProtMessage ( CVector<uint8_t> vecMessage )
{
    // only send messages if we are connected, otherwise delete complete queue
    if ( IsConnected() )
    {
        // emit message to actually send the data
        emit MessReadyForSending ( vecMessage );
    }
    else
    {
        // delete send message queue
        Protocol.DeleteSendMessQueue();
    }
}

void CChannel::SetSockBufSize ( const int iNumBlocks )
{
    /* this opperation must be done with mutex */
    Mutex.lock();
    {
        iCurSockBufSize = iNumBlocks;

        // the idea of setting the jitter buffer is as follows:
        // The network block size is a multiple of the internal minimal
        // block size. Therefore, the minimum jitter buffer size must be
        // so that it can take one network buffer -> NET_BLOCK_SIZE_FACTOR.
        // The actual jitter compensation are then the additional blocks of
        // the internal block size, which is set with SetSockBufSize
        SockBuf.Init ( MIN_BLOCK_SIZE_SAMPLES,
            iNumBlocks + iCurNetwInBlSiFact );
    }
    Mutex.unlock();
}

void CChannel::OnNetwBlSiFactChange ( int iNewNetwBlSiFact )
{
// TEST
//qDebug ( "new network block size factor: %d", iNewNetwBlSiFact );

    SetNetwBufSizeFactOut ( iNewNetwBlSiFact );
}

void CChannel::OnJittBufSizeChange ( int iNewJitBufSize )
{
// TEST
//qDebug ( "new jitter buffer size: %d", iNewJitBufSize );

    SetSockBufSize ( iNewJitBufSize );
}

void CChannel::OnChangeChanGain ( int iChanID, double dNewGain )
{
    ASSERT ( ( iChanID >= 0 ) && ( iChanID < MAX_NUM_CHANNELS ) );

    // set value
    vecdGains[iChanID] = dNewGain;
}

bool CChannel::GetAddress(CHostAddress& RetAddr)
{
    if ( IsConnected() )
    {
        RetAddr = InetAddr;
        return true;
    }
    else
    {
        RetAddr = CHostAddress();
        return false;
    }
}

EPutDataStat CChannel::PutData ( const CVector<unsigned char>& vecbyData,
                                 int iNumBytes )
{
    EPutDataStat    eRet = PS_GEN_ERROR;
    bool            bNewConnection = false;
    bool            bIsAudioPacket = false;

    // check if this is an audio packet by checking all possible lengths
    for ( int i = 0; i < MAX_NET_BLOCK_SIZE_FACTOR; i++ )
    {
        if ( iNumBytes == vecNetwInBufSizes[i] )
        {
            bIsAudioPacket = true;

            // check if we are correctly initialized
            const int iNewNetwInBlSiFact = i + 1;
            if ( iNewNetwInBlSiFact != iCurNetwInBlSiFact )
            {
                // re-initialize to new value
                SetNetwInBlSiFact ( iNewNetwInBlSiFact );
            }
        }
    }

    /* only process if packet has correct size */
    if ( bIsAudioPacket )
    {
        Mutex.lock();
        {
            /* decompress audio */
            CVector<short> vecsDecomprAudio ( AudioCompressionIn.Decode ( vecbyData ) );

            /* do resampling to compensate for sample rate offsets in the
               different sound cards of the clients */
/*
for (int i = 0; i < BLOCK_SIZE_SAMPLES; i++)
    vecdResInData[i] = (double) vecsData[i];

const int iInSize = ResampleObj.Resample(vecdResInData, vecdResOutData,
    (double) SAMPLE_RATE / (SAMPLE_RATE - dSamRateOffset));
*/

vecdResOutData.Init ( iCurNetwInBlSiFact * MIN_BLOCK_SIZE_SAMPLES );
for ( int i = 0; i < iCurNetwInBlSiFact * MIN_BLOCK_SIZE_SAMPLES; i++ ) {
    vecdResOutData[i] = (double) vecsDecomprAudio[i];
}

            if ( SockBuf.Put ( vecdResOutData ) )
            {
                eRet = PS_AUDIO_OK;
            }
            else
            {
                eRet = PS_AUDIO_ERR;
            }

            // check if channel was not connected, this is a new connection
            bNewConnection = !IsConnected();

            // reset time-out counter
            iConTimeOut = iConTimeOutStartVal;
        }
        Mutex.unlock();
    }
    else
    {
        // only use protocol data if channel is connected
        if ( IsConnected() )
        {
            // this seems not to be an audio block, parse the message
            if ( Protocol.ParseMessage ( vecbyData, iNumBytes ) )
            {
                eRet = PS_PROT_OK;

                // create message for protocol status
                emit ProtocolStatus ( true );
            }
            else
            {
                eRet = PS_PROT_ERR;

                // create message for protocol status
                emit ProtocolStatus ( false );
            }
        }
    }

    // inform other objects that new connection was established
    if ( bNewConnection )
    {
        // log new connection
        CHostAddress address ( GetAddress() );
        qDebug ( CLogTimeDate::toString() +  "Connected with IP %s",
            address.InetAddr.toString().latin1() );

        emit NewConnection();
    }

    return eRet;
}

bool CChannel::GetData ( CVector<double>& vecdData )
{
    bool bGetOK = false;

    Mutex.lock(); /* get mutex lock */
    {
        bGetOK = SockBuf.Get ( vecdData );

        if ( !bGetOK )
        {
            /* decrease time-out counter */
            if ( iConTimeOut > 0 )
            {
                iConTimeOut--;
            }
        }
    }
    Mutex.unlock(); /* get mutex unlock */

    return bGetOK;
}

CVector<unsigned char> CChannel::PrepSendPacket ( const CVector<short>& vecsNPacket )
{
    /* if the block is not ready we have to initialize with zero length to
       tell the following network send routine that nothing should be sent */
    CVector<unsigned char> vecbySendBuf ( 0 );

    /* use conversion buffer to convert sound card block size in network
       block size */
    if ( ConvBuf.Put ( vecsNPacket ) )
    {
        /* a packet is ready, compress audio */
        vecbySendBuf.Init ( iAudComprSizeOut );
        vecbySendBuf = AudioCompressionOut.Encode ( ConvBuf.Get() );
    }

    return vecbySendBuf;
}









/******************************************************************************\
* CSampleOffsetEst                                                             *
\******************************************************************************/
void CSampleOffsetEst::Init()
{
    /* init sample rate estimation */
    dSamRateEst = SAMPLE_RATE;

    /* init vectors storing the data */
    veciTimeElapsed.Init(VEC_LEN_SAM_OFFS_EST);
    veciTiStIdx.Init(VEC_LEN_SAM_OFFS_EST);

    /* start reference time (the counter wraps to zero 24 hours after the last
       call to start() or restart, but this should not concern us since this
       software will most probably not be used that long) */
    RefTime.start();

    /* init accumulated time stamp variable */
    iAccTiStVal = 0;

    /* init count (do not ship any result in init phase) */
    iInitCnt = VEC_LEN_SAM_OFFS_EST + 1;
}

void CSampleOffsetEst::AddTimeStampIdx(const int iTimeStampIdx)
{
    int i;

    const int iLastIdx = VEC_LEN_SAM_OFFS_EST - 1;

    /* take care of wrap of the time stamp index (byte wrap) */
    if (iTimeStampIdx < veciTiStIdx[iLastIdx] - iAccTiStVal)
        iAccTiStVal += _MAXBYTE + 1;

    /* add new data pair to the FIFO */
    for (i = 1; i < VEC_LEN_SAM_OFFS_EST; i++)
    {
        /* move old data */
        veciTimeElapsed[i - 1] = veciTimeElapsed[i];
        veciTiStIdx[i - 1] = veciTiStIdx[i];
    }

    /* add new data */
    veciTimeElapsed[iLastIdx] = RefTime.elapsed();
    veciTiStIdx[iLastIdx] = iAccTiStVal + iTimeStampIdx;

/*
static FILE* pFile = fopen("v.dat", "w");
for (i = 0; i < VEC_LEN_SAM_OFFS_EST; i++)
    fprintf(pFile, "%d\n", veciTimeElapsed[i]);
fflush(pFile);
*/


    /* calculate linear regression for sample rate estimation */
    /* first, calculate averages */
    double dTimeAv = 0;
    double dTiStAv = 0;
    for (i = 0; i < VEC_LEN_SAM_OFFS_EST; i++)
    {
        dTimeAv += veciTimeElapsed[i];
        dTiStAv += veciTiStIdx[i];
    }
    dTimeAv /= VEC_LEN_SAM_OFFS_EST;
    dTiStAv /= VEC_LEN_SAM_OFFS_EST;

    /* calculate gradient */
    double dNom = 0;
    double dDenom = 0;
    for (i = 0; i < VEC_LEN_SAM_OFFS_EST; i++)
    {
        const double dCurTimeNoAv = veciTimeElapsed[i] - dTimeAv;
        dNom += dCurTimeNoAv * (veciTiStIdx[i] - dTiStAv);
        dDenom += dCurTimeNoAv * dCurTimeNoAv;
    }

    /* final sample rate offset estimation calculation */
    if (iInitCnt > 0)
        iInitCnt--;
    else
    {
        dSamRateEst = dNom / dDenom * NUM_BL_TIME_STAMPS * MIN_BLOCK_SIZE_SAMPLES * 1000;

/*
static FILE* pFile = fopen("v.dat", "w");
for (i = 0; i < VEC_LEN_SAM_OFFS_EST; i++)
    fprintf(pFile, "%d %d\n", veciTimeElapsed[i], veciTiStIdx[i]);
fflush(pFile);
*/
    }

/*
static FILE* pFile = fopen("v.dat", "w");
fprintf(pFile, "%e\n", dSamRateEst);
fflush(pFile);
*/

}
