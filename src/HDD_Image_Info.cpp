/****************************************************************************
**
** Copyright (C) 2009-2010 Andrey Rijov <ANDron142@yandex.ru>
**
** This file is part of AQEMU.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA  02110-1301, USA.
**
****************************************************************************/

#include <QFile>
#include <QSettings>

#include "Utils.h"
#include "HDD_Image_Info.h"

HDD_Image_Info::HDD_Image_Info( QObject *parent )
		: QObject( parent )
{
	QEMU_IMG_Proc = new QProcess( this );
}

HDD_Image_Info::~HDD_Image_Info()
{
    delete QEMU_IMG_Proc;
}

VM::Disk_Info HDD_Image_Info::Get_Disk_Info() const
{
	return Info;
}

void HDD_Image_Info::Update_Disk_Info( const QString &path )
{
    TEMPODEBUG( "void HDD_Image_Info::Update_Disk_Info( const QString &path )",
                QString("path=\"%1\"").arg(path) );
	Info.Image_File_Name = path;
	
	if( Info.Image_File_Name.isEmpty() )
	{
		Clear_Info();
		return;
	}
	
	if( QFile::exists(Info.Image_File_Name) == false )
	{
		AQWarning( "void QEMU_IMG_Thread::run()",
                   "Image \"" + Info.Image_File_Name + "\" does not exist!" );
		Clear_Info();
		return;
	}
	else
	{
		QStringList args;
		args << "info" << Info.Image_File_Name;
		
		QEMU_IMG_Proc = new QProcess( this );
		QSettings settings;
		QEMU_IMG_Proc->start( settings.value("QEMU-IMG_Path", "qemu-img").toString(), args );
        TEMPODEBUG( "void HDD_Image_Info::Update_Disk_Info( const QString &path )",
                    QString("qemu_img_path=\"%1\" args=\"%2\"")
                    .arg(settings.value("QEMU-IMG_Path", "qemu-img").toString())
                    .arg(args.join(" ")) );
		
		connect( QEMU_IMG_Proc, SIGNAL(finished(int, QProcess::ExitStatus)),
				 this, SLOT(Parse_Info(int, QProcess::ExitStatus)), Qt::DirectConnection );
		
		connect( QEMU_IMG_Proc, SIGNAL(error(QProcess::ProcessError)),
				 this, SLOT(Clear_Info()), Qt::DirectConnection );
	}
}

void HDD_Image_Info::Clear_Info()
{
	AQDebug( "void HDD_Image_Info::Clear_Info()",
			 "HDD Info Not Read!" );
    TEMPODEBUG( "void HDD_Image_Info::Clear_Info()",
                QString("image=\"%1\"").arg(Info.Image_File_Name) );
	
	VM_HDD tmp_hdd;
	Info.Disk_Format = "";
	Info.Virtual_Size = tmp_hdd.String_to_Device_Size( "0 G" );
	Info.Disk_Size = tmp_hdd.String_to_Device_Size( "0 G" );
	Info.Cluster_Size = 0;
	
	emit Completed( false );
}

void HDD_Image_Info::Parse_Info( int exitCode, QProcess::ExitStatus exitStatus )
{
    TEMPODEBUG( "void HDD_Image_Info::Parse_Info( int exitCode, QProcess::ExitStatus exitStatus )",
                QString("image=\"%1\" exitCode=%2 exitStatus=%3")
                .arg(Info.Image_File_Name)
                .arg(exitCode)
                .arg(exitStatus) );


	QByteArray info_ba = QEMU_IMG_Proc->readAll();
	QString info_str = QString::fromUtf8( info_ba );
	if( info_str.isEmpty() )
	{
		AQDebug( "void HDD_Image_Info::Parse_Info( int exitCode, QProcess::ExitStatus exitStatus )",
				 "Data is empty." );
        TEMPODEBUG( "void HDD_Image_Info::Parse_Info( int exitCode, QProcess::ExitStatus exitStatus )",
                    QString("empty output image=\"%1\"").arg(Info.Image_File_Name) );
		Clear_Info();
		return;
	}
	
	QStringList lines = info_str.split( '\n', Qt::SkipEmptyParts );
	QString img, fmt, vsize, dsize;
	int csize = 0;
	
	for( int i = 0; i < lines.count(); ++i )
	{
		QString line = lines[i].trimmed();
		if( line.startsWith( "image:" ) )
			img = line.section( ' ', 1 ).trimmed();
		else if( line.startsWith( "file format:" ) )
			fmt = line.section( ' ', 2 ).trimmed();
		else if( line.startsWith( "virtual size:" ) )
			vsize = line.section( ' ', 2 ).trimmed();
		else if( line.startsWith( "disk size:" ) )
			dsize = line.section( ' ', 2 ).trimmed();
		else if( line.startsWith( "cluster_size:" ) )
			csize = line.section( ' ', 1 ).trimmed().toUInt();
	}
	
	// Strip binary suffix (e.g. "64 GiB" -> "64 G") for String_to_Device_Size compatibility
	QRegExp sufRx( "([\\d]+[.,]?[\\d]*)\\s*([KMG])" );
	if( sufRx.indexIn(vsize) >= 0 ) vsize = sufRx.cap(1) + " " + sufRx.cap(2);
	if( sufRx.indexIn(dsize) >= 0 ) dsize = sufRx.cap(1) + " " + sufRx.cap(2);
	
	if( img.isEmpty() || fmt.isEmpty() || vsize.isEmpty() || dsize.isEmpty() )
	{
		AQError( "void HDD_Image_Info::Parse_Info( int exitCode, QProcess::ExitStatus exitStatus )",
				 "Cannot parse qemu-img info output! Image: " + Info.Image_File_Name + "\nData: " + info_str );
        TEMPODEBUG( "void HDD_Image_Info::Parse_Info( int exitCode, QProcess::ExitStatus exitStatus )",
                    QString("failed to parse image=\"%1\" data=\"%2\"")
                    .arg(Info.Image_File_Name)
                    .arg(info_str) );
		Clear_Info();
		return;
	}
	
	Info.Disk_Format = fmt;
	VM_HDD tmp_hdd;
	Info.Virtual_Size = tmp_hdd.String_to_Device_Size( vsize );
	Info.Disk_Size = tmp_hdd.String_to_Device_Size( dsize );
	Info.Cluster_Size = csize;
	
	TEMPODEBUG( "void HDD_Image_Info::Parse_Info( int exitCode, QProcess::ExitStatus exitStatus )",
				QString("parsed image=\"%1\" format=\"%2\" virtual_size=\"%3\" disk_size=\"%4\" cluster_size=%5")
				.arg(Info.Image_File_Name)
				.arg(Info.Disk_Format)
				.arg(vsize)
				.arg(dsize)
				.arg(Info.Cluster_Size) );
	
	emit Completed( true );
}
