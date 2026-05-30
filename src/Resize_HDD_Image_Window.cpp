/****************************************************************************
**
** Copyright (C) 2008-2010 Andrey Rijov <ANDron142@yandex.ru>
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
#include <QMessageBox>
#include <QProcess>
#include <QSettings>

#include "Utils.h"
#include "Resize_HDD_Image_Window.h"

Resize_HDD_Image_Window::Resize_HDD_Image_Window( QWidget *parent )
	: QDialog( parent )
{
	ui.setupUi( this );

	connect( ui.Slider_Add_GB, SIGNAL(valueChanged(int)),
			 ui.SB_Add_GB, SLOT(setValue(int)) );
	connect( ui.SB_Add_GB, SIGNAL(valueChanged(int)),
			 ui.Slider_Add_GB, SLOT(setValue(int)) );
}

void Resize_HDD_Image_Window::Set_Image_File_Name( const QString &path )
{
	Image_File_Name = path;
	Read_Current_Size();
}

void Resize_HDD_Image_Window::Read_Current_Size()
{
	if( Image_File_Name.isEmpty() || ! QFile::exists( Image_File_Name ) )
	{
		ui.Label_Current_Size->setText( tr("Current Size: --") );
		return;
	}

	QSettings settings;
	QString qemuImgPath = settings.value( "QEMU-IMG_Path", "qemu-img" ).toString();

	QProcess proc;
	proc.start( qemuImgPath, QStringList() << "info" << Image_File_Name );

	if( ! proc.waitForFinished( 10000 ) || proc.exitCode() != 0 )
	{
		ui.Label_Current_Size->setText( tr("Current Size: --") );
		return;
	}

	QString output = QString::fromUtf8( proc.readAllStandardOutput() );
	for( const QString &line : output.split( '\n' ) )
	{
		if( line.trimmed().startsWith( "virtual size:" ) )
		{
			ui.Label_Current_Size->setText(
				tr("Current Size: %1").arg( line.section( ':', 1 ).trimmed() ) );
			return;
		}
	}
}

void Resize_HDD_Image_Window::on_Button_Set_1GB_clicked()
{
	ui.SB_Add_GB->setValue( 1 );
}

void Resize_HDD_Image_Window::on_Button_Set_5GB_clicked()
{
	ui.SB_Add_GB->setValue( 5 );
}

void Resize_HDD_Image_Window::on_Button_Set_10GB_clicked()
{
	ui.SB_Add_GB->setValue( 10 );
}

void Resize_HDD_Image_Window::on_Button_Set_20GB_clicked()
{
	ui.SB_Add_GB->setValue( 20 );
}

void Resize_HDD_Image_Window::on_Button_Set_50GB_clicked()
{
	ui.SB_Add_GB->setValue( 50 );
}

void Resize_HDD_Image_Window::accept()
{
	if( Image_File_Name.isEmpty() )
	{
		AQGraphic_Warning( tr("Error!"), tr("Image File Name is Empty!") );
		return;
	}

	if( ! QFile::exists( Image_File_Name ) )
	{
		AQGraphic_Warning( tr("Error!"), tr("Image File doesn't Exist!") );
		return;
	}

	int addGB = ui.SB_Add_GB->value();

	if( addGB < 1 || addGB > 1024 )
	{
		AQGraphic_Warning( tr("Error!"), tr("Invalid size increment!") );
		return;
	}

	QSettings settings;
	QString qemuImgPath = settings.value( "QEMU-IMG_Path", "qemu-img" ).toString();

	QStringList args;
	args << "resize" << Image_File_Name << ( "+" + QString::number( addGB ) + "G" );

	QProcess proc;
	proc.start( qemuImgPath, args );

	if( ! proc.waitForFinished( 30000 ) )
	{
		AQGraphic_Warning( tr("Error!"),
			tr("qemu-img resize timed out!") );
		return;
	}

	if( proc.exitCode() != 0 )
	{
		QString err = QString::fromUtf8( proc.readAllStandardError() );
		AQGraphic_Warning( tr("Error!"),
			tr("qemu-img resize failed:\n%1").arg(err) );
		return;
	}

	QDialog::accept();
}
