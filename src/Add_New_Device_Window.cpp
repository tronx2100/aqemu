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

#include <QFileDialog>

#include "Utils.h"
#include "Add_New_Device_Window.h"
#include "Select_Block_Device_Window.h"

Add_New_Device_Window::Add_New_Device_Window( QWidget *parent )
	: QDialog( parent )
{
	ui.setupUi( this );

	// Auto-enable File + Interface when any native option is toggled on
	connect( ui.CH_Format, &QCheckBox::toggled, this, &Add_New_Device_Window::Update_Native_State );
	connect( ui.CH_Interface, &QCheckBox::toggled, this, &Add_New_Device_Window::Update_Native_State );
	connect( ui.CH_Cache, &QCheckBox::toggled, this, &Add_New_Device_Window::Update_Native_State );
	connect( ui.CH_AIO, &QCheckBox::toggled, this, &Add_New_Device_Window::Update_Native_State );
	connect( ui.CH_Snapshot, &QCheckBox::toggled, this, &Add_New_Device_Window::Update_Native_State );
	connect( ui.CH_Boot, &QCheckBox::toggled, this, &Add_New_Device_Window::Update_Native_State );
	connect( ui.CH_Discard, &QCheckBox::toggled, this, &Add_New_Device_Window::Update_Native_State );
	connect( ui.CH_Media, &QCheckBox::toggled, this, &Add_New_Device_Window::Update_Native_State );
	connect( ui.CH_Index, &QCheckBox::toggled, this, &Add_New_Device_Window::Update_Native_State );
	connect( ui.CH_Bus_Unit, &QCheckBox::toggled, this, &Add_New_Device_Window::Update_Native_State );
	connect( ui.CH_File, &QCheckBox::toggled, this, &Add_New_Device_Window::Update_Native_State );
	connect( ui.GB_hdachs_Settings, &QGroupBox::toggled, this, &Add_New_Device_Window::Update_Native_State );

	// Auto-detect format when file path changes
	connect( ui.Edit_File_Path, &QLineEdit::textChanged, this, &Add_New_Device_Window::Auto_Detect_Format );

	// Block device button (auto-connected via on_TB_Block_Device_clicked)
	ui.TB_Block_Device->setEnabled( true );
}

void Add_New_Device_Window::Update_Native_State()
{
	bool any_native = ui.CH_Index->isChecked()
		|| ui.CH_Bus_Unit->isChecked()
		|| ui.CH_Media->isChecked()
		|| ui.CH_Snapshot->isChecked()
		|| ui.CH_Cache->isChecked()
		|| ui.CH_AIO->isChecked()
		|| ui.CH_Boot->isChecked()
		|| ui.CH_Format->isChecked()
		|| ui.GB_hdachs_Settings->isChecked();

	if( any_native )
	{
		if( ! ui.CH_File->isChecked() )
			ui.CH_File->setChecked( true );

		if( ! ui.CH_Interface->isChecked() )
		{
			ui.CH_Interface->setChecked( true );
			ui.CB_Interface->setCurrentIndex( 6 ); // virtio (auto-creates virtio-blk-pci)
		}
	}
}

QString Add_New_Device_Window::Detect_Format_From_Path( const QString &path )
{
	if( path.startsWith("/dev/") || path.isEmpty() )
		return "raw";

	QString lower = path.toLower();
	if( lower.endsWith(".qcow2") ) return "qcow2";
	if( lower.endsWith(".qcow") )  return "qcow";
	if( lower.endsWith(".vmdk") )  return "vmdk";
	if( lower.endsWith(".vdi") )   return "vdi";
	if( lower.endsWith(".vhd") )   return "vpc";
	if( lower.endsWith(".vhdx") )  return "vhdx";
	if( lower.endsWith(".qed") )   return "qed";
	if( lower.endsWith(".raw") )   return "raw";
	if( lower.endsWith(".img") )   return "raw";

	return "raw";
}

void Add_New_Device_Window::Auto_Detect_Format()
{
	QString path = ui.Edit_File_Path->text();

	if( path.isEmpty() )
	{
		ui.CH_Format->setChecked( false );
		ui.CH_Format->setVisible( true );
		ui.CB_Format->setVisible( true );
		return;
	}

	QString fmt = Detect_Format_From_Path( path );

	ui.CH_Format->setChecked( true );
	int idx = ui.CB_Format->findText( fmt );
	if( idx != -1 )
		ui.CB_Format->setCurrentIndex( idx );

	// Lock format to raw for block devices
	bool is_block = path.startsWith("/dev/");
	ui.CH_Format->setVisible( !is_block );
	ui.CB_Format->setVisible( !is_block );
}

VM_Native_Storage_Device Add_New_Device_Window::Get_Device() const
{
	return Device;
}

void Add_New_Device_Window::Set_Device( const VM_Native_Storage_Device &dev )
{
	Device = dev;

	// Block textChanged during setup to prevent Auto_Detect_Format from cascading
	// into Update_Native_State and wrongly auto-checking File + Interface
	ui.Edit_File_Path->blockSignals( true );
	
	// Update View...
	ui.CH_Interface->setChecked( Device.Use_Interface() );
	
	// Interface
	switch( Device.Get_Interface() )
	{
		case VM::DI_IDE:
			ui.CB_Interface->setCurrentIndex( 0 );
			break;
			
		case VM::DI_SCSI:
			ui.CB_Interface->setCurrentIndex( 1 );
			break;
			
		case VM::DI_SD:
			ui.CB_Interface->setCurrentIndex( 2 );
			break;
			
		case VM::DI_MTD:
			ui.CB_Interface->setCurrentIndex( 3 );
			break;
			
		case VM::DI_Floppy:
			ui.CB_Interface->setCurrentIndex( 4 );
			break;
			
		case VM::DI_PFlash:
			ui.CB_Interface->setCurrentIndex( 5 );
			break;
			
		case VM::DI_Virtio:
			ui.CB_Interface->setCurrentIndex( 6 );
			break;

        case VM::DI_Virtio_SCSI:
			ui.CB_Interface->setCurrentIndex( 7 );
			break;
			
		default:
            AQError( "void Add_New_Device_Window::Set_Device( const VM_Native_Storage_Device &dev )",
					 "Interface Default Section! Use IDE!" );
			break;
	}
	
	// Media
	ui.CH_Media->setChecked( Device.Use_Media() );
	
	switch( Device.Get_Media() )
	{
		case VM::DM_Disk:
			ui.CB_Media->setCurrentIndex( 0 );
			break;
			
		case VM::DM_CD_ROM:
			ui.CB_Media->setCurrentIndex( 1 );
			break;
			
		default:
            AQError( "void Add_New_Device_Window::Set_Device( const VM_Native_Storage_Device &dev )",
					 "Media Default Section! Use Disk!" );
			break;
	}
	
	// File Path
	ui.CH_File->setChecked( Device.Use_File_Path() );
	ui.Edit_File_Path->setText( Device.Get_File_Path() );
	
	// Index
	ui.CH_Index->setChecked( Device.Use_Index() );
	ui.SB_Index->setValue( Device.Get_Index() );
	
	// Bus, Unit
	ui.CH_Bus_Unit->setChecked( Device.Use_Bus_Unit() );
	ui.SB_Bus->setValue( Device.Get_Bus() );
	ui.SB_Unit->setValue( Device.Get_Unit() );
	
	// Snapshot
	ui.CH_Snapshot->setChecked( Device.Use_Snapshot() );
	ui.CB_Snapshot->setCurrentIndex( Device.Get_Snapshot() ? 0 : 1 );
	
	// Cache
	ui.CH_Cache->setChecked( Device.Use_Cache() );
	int index = ui.CB_Cache->findText( Device.Get_Cache() );
	if( index != -1 )
		ui.CB_Cache->setCurrentIndex( index );
	else
        AQError( "void Add_New_Device_Window::Set_Device( const VM_Native_Storage_Device &dev )",
				 "Cache: " + Device.Get_Cache() );
	
	// AIO
	ui.CH_AIO->setChecked( Device.Use_AIO() );
	index = ui.CB_AIO->findText( Device.Get_AIO() );
	if( index != -1 ) ui.CB_AIO->setCurrentIndex( index );
	else
        AQError( "void Add_New_Device_Window::Set_Device( const VM_Native_Storage_Device &dev )",
				 "AIO: " + Device.Get_AIO() );
	
	// Boot
	ui.CH_Boot->setChecked( Device.Use_Boot() );
	ui.CB_Boot->setCurrentIndex( Device.Get_Boot() ? 0 : 1 );

	// Discard (always start unchecked; user can enable manually)
	ui.CH_Discard->setChecked( false );
	ui.CB_Discard->setCurrentIndex( Device.Get_Discard() ? 0 : 1 );

	// Format
	ui.CH_Format->setChecked( Device.Use_Format() );
	int fmt_idx = ui.CB_Format->findText( Device.Get_Format() );
	if( fmt_idx != -1 )
		ui.CB_Format->setCurrentIndex( fmt_idx );
	else
		AQError( "void Add_New_Device_Window::Set_Device( const VM_Native_Storage_Device &dev )",
				 "Format: " + Device.Get_Format() );

	// Re-allow textChanged signals after setup
	ui.Edit_File_Path->blockSignals( false );

	// Force raw for block devices, hide format UI
	if( ui.Edit_File_Path->text().startsWith("/dev/") )
	{
		ui.CH_Format->setChecked( true );
		ui.CH_Format->setVisible( false );
		ui.CB_Format->setVisible( false );
		int raw_idx = ui.CB_Format->findText( "raw" );
		if( raw_idx != -1 )
			ui.CB_Format->setCurrentIndex( raw_idx );
	}
	else
	{
		ui.CH_Format->setVisible( true );
		ui.CB_Format->setVisible( true );
	}

	// cyls, heads, secs, trans
	ui.GB_hdachs_Settings->setChecked( Device.Use_hdachs() );
	ui.Edit_Cyls->setText( QString::number(Device.Get_Cyls()) );
	ui.Edit_Heads->setText( QString::number(Device.Get_Heads()) );
	ui.Edit_Secs->setText( QString::number(Device.Get_Secs()) );
	ui.Edit_Trans->setText( QString::number(Device.Get_Trans()) );
}

void Add_New_Device_Window::Set_Emulator_Devices( const Available_Devices &devices )
{
	if( devices.PSO_Drive_File )
	{
		ui.CH_File->setVisible( true );
		ui.Edit_File_Path->setVisible( true );
		ui.TB_File_Path_Browse->setVisible( true );
	}
	else
	{
		ui.CH_File->setVisible( false );
		ui.Edit_File_Path->setVisible( false );
		ui.TB_File_Path_Browse->setVisible( false );
	}
	
	if( devices.PSO_Drive_If )
	{
		ui.CH_Interface->setVisible( true );
		ui.CB_Interface->setVisible( true );
	}
	else
	{
		ui.CH_Interface->setVisible( false );
		ui.CB_Interface->setVisible( false );
	}
	
	if( devices.PSO_Drive_Bus_Unit )
	{
		ui.CH_Bus_Unit->setVisible( true );
		ui.SB_Bus->setVisible( true );
		ui.SB_Unit->setVisible( true );
	}
	else
	{
		ui.CH_Bus_Unit->setVisible( false );
		ui.SB_Bus->setVisible( false );
		ui.SB_Unit->setVisible( false );
	}
	
	if( devices.PSO_Drive_Index )
	{
		ui.CH_Index->setVisible( true );
		ui.SB_Index->setVisible( true );
	}
	else
	{
		ui.CH_Index->setVisible( false );
		ui.SB_Index->setVisible( false );
	}
		
	if( devices.PSO_Drive_Media )
	{
		ui.CH_Media->setVisible( true );
		ui.CB_Media->setVisible( true );
	}
	else
	{
		ui.CH_Media->setVisible( false );
		ui.CB_Media->setVisible( false );
	}
		
	if( devices.PSO_Drive_Cyls_Heads_Secs_Trans )
		ui.GB_hdachs_Settings->setVisible( true );
	else
		ui.GB_hdachs_Settings->setVisible( false );
		
	if( devices.PSO_Drive_Snapshot )
	{
		ui.CH_Snapshot->setVisible( true );
		ui.CB_Snapshot->setVisible( true );
	}
	else
	{
		ui.CH_Snapshot->setVisible( false );
		ui.CB_Snapshot->setVisible( false );
	}
		
	if( devices.PSO_Drive_Cache )
	{
		ui.CH_Cache->setVisible( true );
		ui.CB_Cache->setVisible( true );
	}
	else
	{
		ui.CH_Cache->setVisible( false );
		ui.CB_Cache->setVisible( false );
	}
		
	if( devices.PSO_Drive_AIO )
	{
		ui.CH_AIO->setVisible( true );
		ui.CB_AIO->setVisible( true );
	}
	else
	{
		ui.CH_AIO->setVisible( false );
		ui.CB_AIO->setVisible( false );
	}
		
	/* FIXME
	if( devices.PSO_Drive_Format )
	{
		ui.->setVisible( true );
		ui.->setVisible( true );
	}
	else
	{
		ui.->setVisible( false );
		ui.->setVisible( false );
	}
	
	if( devices.PSO_Drive_Serial )
	{
		ui.->setVisible( true );
		ui.->setVisible( true );
	}
	else
	{
		ui.->setVisible( false );
		ui.->setVisible( false );
	}
	
	if( devices.PSO_Drive_ADDR )
	{
		ui.->setVisible( true );
		ui.->setVisible( true );
	}
	else
	{
		ui.->setVisible( false );
		ui.->setVisible( false );
	}*/
	
	if( devices.PSO_Drive_Boot )
	{
		ui.CH_Boot->setVisible( true );
		ui.CB_Boot->setVisible( true );
	}
	else
	{
		ui.CH_Boot->setVisible( false );
		ui.CB_Boot->setVisible( false );
	}

	if( devices.PSO_Drive_Format )
	{
		ui.CH_Format->setVisible( true );
		ui.CB_Format->setVisible( true );
	}
	else
	{
		ui.CH_Format->setVisible( false );
		ui.CB_Format->setVisible( false );
	}

	// Minimum Size
	resize( minimumSizeHint().width(), minimumSizeHint().height() );
}

void Add_New_Device_Window::Set_Enabled( bool enabled )
{
	ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled( enabled );
}

void Add_New_Device_Window::on_CB_Interface_currentIndexChanged( const QString &text )
{
	if( text == "ide" || text == "floppy" )
	{
		ui.CH_Index->setEnabled( true );
		ui.SB_Index->setEnabled( true );
		
		ui.CH_Bus_Unit->setEnabled( false );
		ui.SB_Bus->setEnabled( false );
		ui.SB_Unit->setEnabled( false );
	}
	else if( text == "scsi" )
	{
		ui.CH_Index->setEnabled( false );
		ui.SB_Index->setEnabled( false );
		
		ui.CH_Bus_Unit->setEnabled( true );
		ui.SB_Bus->setEnabled( true );
		ui.SB_Unit->setEnabled( true );
	}
	else if( text == "virtio" )
	{
		ui.CH_Index->setEnabled( true );
		ui.SB_Index->setEnabled( true );
		
		ui.CH_Bus_Unit->setEnabled( true );
		ui.SB_Bus->setEnabled( true );
		ui.SB_Unit->setEnabled( true );
	}
	else
	{
		ui.CH_Index->setEnabled( false );
		ui.SB_Index->setEnabled( false );
		
		ui.CH_Bus_Unit->setEnabled( false );
		ui.SB_Bus->setEnabled( false );
		ui.SB_Unit->setEnabled( false );
	}
}

void Add_New_Device_Window::on_TB_File_Path_Browse_clicked()
{
	QString file_name = QFileDialog::getOpenFileName( this, tr("Select your device"),
													  Get_Last_Dir_Path(ui.Edit_File_Path->text()),
													  tr("All Files (*)") );
	
	if( ! file_name.isEmpty() )
		ui.Edit_File_Path->setText( QDir::toNativeSeparators(file_name) );
}

void Add_New_Device_Window::on_TB_Block_Device_clicked()
{
	Select_Block_Device_Window win( this );
	if( win.exec() == QDialog::Accepted )
	{
		QString dev_path = win.Get_Device_Path();
		if( ! dev_path.isEmpty() )
		{
			ui.CH_File->setChecked( true );
			ui.Edit_File_Path->setText( dev_path );

			// Auto-set defaults for block device
			ui.CH_Format->setChecked( true );
			int raw_idx = ui.CB_Format->findText( "raw" );
			if( raw_idx != -1 ) ui.CB_Format->setCurrentIndex( raw_idx );

			ui.CH_Interface->setChecked( true );
			ui.CB_Interface->setCurrentIndex( 6 ); // virtio

			ui.CH_Media->setChecked( true );
			ui.CB_Media->setCurrentIndex( 0 ); // Disk

			ui.CH_Cache->setChecked( true );
			int none_idx = ui.CB_Cache->findText( "none" );
			if( none_idx != -1 ) ui.CB_Cache->setCurrentIndex( none_idx );
		}
	}
}

void Add_New_Device_Window::done(int r)
{
    if ( r == QDialog::Accepted )
    {
	    // Compute native mode before auto-enable
	    bool any_native = ui.CH_Interface->isChecked()
		|| ui.CH_Index->isChecked()
		|| ui.CH_Bus_Unit->isChecked()
		|| ui.CH_Media->isChecked()
		|| ui.CH_Snapshot->isChecked()
		|| ui.CH_Cache->isChecked()
		|| ui.CH_AIO->isChecked()
		|| ui.CH_Boot->isChecked()
		|| ui.CH_Format->isChecked()
		|| ui.CH_File->isChecked()
		|| ui.GB_hdachs_Settings->isChecked();

	    // Auto-enable File + Interface when any native option is active
	    if( any_native && ! ui.CH_File->isChecked() )
	    {
		    ui.CH_File->setChecked( true );
		    if( ui.Edit_File_Path->text().isEmpty() )
		    {
			    AQGraphic_Warning( tr("Error!"), tr("You must select a file for native storage device!") );
			    return;
		    }
	    }

	    if( any_native && ! ui.CH_Interface->isChecked() )
	    {
		    ui.CH_Interface->setChecked( true );
		    ui.CB_Interface->setCurrentIndex( 6 ); // virtio (auto-creates virtio-blk-pci)
	    }

	    // Interface
	    switch( ui.CB_Interface->currentIndex() )
	    {
		    case 0:
			    Device.Set_Interface( VM::DI_IDE );
			    break;

		    case 1:
			    Device.Set_Interface( VM::DI_SCSI );
			    break;

		    case 2:
			    Device.Set_Interface( VM::DI_SD );
			    break;

		    case 3:
			    Device.Set_Interface( VM::DI_MTD );
			    break;

		    case 4:
			    Device.Set_Interface( VM::DI_Floppy );
			    break;

		    case 5:
			    Device.Set_Interface( VM::DI_PFlash );
			    break;

		    case 6:
			    Device.Set_Interface( VM::DI_Virtio );
			    break;

		    case 7:
                Device.Set_Interface( VM::DI_Virtio_SCSI );
			    break;

		    default:
			    AQError( "void Add_New_Device_Window::done(int)",
					     "Invalid Interface Index! Use IDE" );
			    Device.Set_Interface( VM::DI_IDE );
			    break;
	    }

	    Device.Use_Interface( ui.CH_Interface->isChecked() );

	    // Media
	    switch( ui.CB_Media->currentIndex() )
	    {
		    case 0:
			    Device.Set_Media( VM::DM_Disk );
			    break;

		    case 1:
			    Device.Set_Media( VM::DM_CD_ROM );
			    break;

		    default:
			    AQError( "void Add_New_Device_Window::done(int)",
					     "Invalid Media Index! Use Disk" );
			    Device.Set_Media( VM::DM_Disk );
			    break;
	    }

	    Device.Use_Media( ui.CH_Media->isChecked() );

	    // File Path
	    if( ui.CH_File->isChecked() )
	    {
		    if( ! QFile::exists(ui.Edit_File_Path->text()) )
		    {
			    AQGraphic_Warning( tr("Error!"), tr("File does not exist!") );
			    return;
		    }
	    }

	    Device.Use_File_Path( ui.CH_File->isChecked() );
	    Device.Set_File_Path( ui.Edit_File_Path->text() );

	    // Index
	    Device.Use_Index( ui.CH_Index->isChecked() );
	    Device.Set_Index( ui.SB_Index->value() );

	    // Bus, Unit
	    Device.Use_Bus_Unit( ui.CH_Bus_Unit->isChecked() );
	    Device.Set_Bus( ui.SB_Bus->value() );
	    Device.Set_Unit( ui.SB_Unit->value() );

	    // Snapshot
	    Device.Use_Snapshot( ui.CH_Snapshot->isChecked() );
	    Device.Set_Snapshot( (ui.CB_Snapshot->currentIndex() == 0) ? true : false );

	    // Cache
	    Device.Use_Cache( ui.CH_Cache->isChecked() );
	    Device.Set_Cache( ui.CB_Cache->currentText() );

	    // AIO
	    Device.Use_AIO( ui.CH_AIO->isChecked() );
	    Device.Set_AIO( ui.CB_AIO->currentText() );

	    // Boot
	    Device.Use_Boot( ui.CH_Boot->isChecked() );
	    Device.Set_Boot( (ui.CB_Boot->currentIndex() == 0) ? true : false );


	// Discard
	Device.Use_Discard( ui.CH_Discard->isChecked() );
	Device.Set_Discard( (ui.CB_Discard->currentIndex() == 0) ? true : false );

	// Format
	if( ui.Edit_File_Path->text().startsWith("/dev/") )
	{
		// Force format=raw for block devices
		Device.Use_Format( true );
		Device.Set_Format( "raw" );
	}
	else
	{
		if( ui.CH_Format->isChecked() && ui.CB_Format->currentText() == "raw" )
		{
			QString ext_warning = Detect_Format_From_Path( ui.Edit_File_Path->text() );
			if( ext_warning != "raw" )
			{
				AQGraphic_Warning( tr("Warning!"),
					tr("The selected file appears to be a %1 image.\n"
					   "Using format=raw may cause errors.\n"
					   "Select \"%1\" instead, or uncheck Format for auto-detect.")
						.arg(ext_warning) );
			}
		}

		Device.Use_Format( ui.CH_Format->isChecked() );
		Device.Set_Format( ui.CB_Format->currentText() );
	}

	    // hdachs
	    if( ui.GB_hdachs_Settings->isChecked() )
	    {
		    bool ok;
		
		    qulonglong cyls = ui.Edit_Cyls->text().toULongLong( &ok, 10 );
		    if( ! ok )
		    {
			    AQGraphic_Warning( tr("Warning!"), tr("\"Cyls\" value is incorrect!") );
			    return;
		    }
		
		    qulonglong heads = ui.Edit_Heads->text().toULongLong( &ok, 10 );
		    if( ! ok )
		    {
			    AQGraphic_Warning( tr("Warning!"), tr("\"Heads\" value is incorrect!") );
			    return;
		    }
		
		    qulonglong secs = ui.Edit_Secs->text().toULongLong( &ok, 10) ;
		    if( ! ok )
		    {
			    AQGraphic_Warning( tr("Warning!"), tr("\"Secs\" value is incorrect!") );
			    return;
		    }
		
		    qulonglong trans = ui.Edit_Trans->text().toULongLong( &ok, 10 );
		    if( ! ok )
		    {
			    AQGraphic_Warning( tr("Warning!"), tr("\"Trans\" value is incorrect!") );
			    return;
		    }
		
		    Device.Use_hdachs( ui.GB_hdachs_Settings->isChecked() );
		    Device.Set_Cyls( cyls );
		    Device.Set_Heads( heads );
		    Device.Set_Secs( secs );
		    Device.Set_Trans( trans );
	    }
	}
	QDialog::done(r);
}
