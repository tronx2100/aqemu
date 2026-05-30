/* This file was created by Tobias Gläßer to make the latest
   vncclient/vncview sources work. */

#include <QtDebug>

#undef qCDebug
#undef qCritical
#define qCDebug(KRDC) qDebug()
#define qCritical(KRDC) qDebug()
