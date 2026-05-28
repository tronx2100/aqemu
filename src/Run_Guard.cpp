// this code was found on stackoverflow and later (slightly) modified
// http://stackoverflow.com/a/28172162
// Author: Dmitry Sazonov Date: edited Oct 29 '15 at 11:11 answered Jan 27 '15 at 13:53
// the license (for code posted at stackoverflow at that time) is CC BY-SA 3.0
// that ought to be gplv2 compatible according to the license text
// TODO: contact Dmitry Sazonov for explicit relicensing

#include "Run_Guard.h"
#include "Utils.h"

#include <QCryptographicHash>

Run_Guard::Run_Guard( const QString& key )
    : key( key )
    , memLockKey( generateKeyHash( key, "_memLockKey" ) )
    , sharedmemKey( generateKeyHash( key, "_sharedmemKey" ) )
    , sharedMem( sharedmemKey )
    , memLock( memLockKey, 1 )
{
    TEMPODEBUG( "Run_Guard::Run_Guard",
                QString("key=\"%1\" memLockKey=\"%2\" sharedmemKey=\"%3\"")
                .arg(key)
                .arg(memLockKey)
                .arg(sharedmemKey) );
    memLock.acquire();
    {
        QSharedMemory fix( sharedmemKey );    // Fix for *nix: http://habrahabr.ru/post/173281/
        fix.attach();
    }
    memLock.release();
}

Run_Guard::~Run_Guard()
{
    release();
}

QString Run_Guard::generateKeyHash( const QString& key, const QString& salt )
{
    QByteArray data;

    data.append( key.toUtf8() );
    data.append( salt.toUtf8() );
    data = QCryptographicHash::hash( data, QCryptographicHash::Sha1 ).toHex();

    return data;
}

bool Run_Guard::isAnotherRunning()
{
    TEMPODEBUG( "Run_Guard::isAnotherRunning",
                QString("sharedMem.isAttached=%1").arg(sharedMem.isAttached()) );
    if ( sharedMem.isAttached() )
        return false;

    memLock.acquire();
    const bool isRunning = sharedMem.attach();
    TEMPODEBUG( "Run_Guard::isAnotherRunning",
                QString("attach result=%1").arg(isRunning) );
    if ( isRunning )
        sharedMem.detach();
    memLock.release();

    return isRunning;
}

bool Run_Guard::tryToRun()
{
    TEMPODEBUG( "Run_Guard::tryToRun",
                QString("key=\"%1\" attached_before=%2").arg(key).arg(sharedMem.isAttached()) );
    if ( isAnotherRunning() )   // Extra check
        return false;

    memLock.acquire();
    const bool result = sharedMem.create( sizeof( quint64 ) );
    memLock.release();
    if ( !result )
    {
        TEMPODEBUG( "Run_Guard::tryToRun",
                    QString("create failed error=\"%1\"").arg(sharedMem.errorString()) );
        release();
        return false;
    }

    return true;
}

void Run_Guard::release()
{
    TEMPODEBUG( "Run_Guard::release",
                QString("attached_before=%1").arg(sharedMem.isAttached()) );
    memLock.acquire();
    if ( sharedMem.isAttached() )
        sharedMem.detach();
    memLock.release();
}
