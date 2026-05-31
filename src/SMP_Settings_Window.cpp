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

#include "SMP_Settings_Window.h"

#include <QTextStream>

SMP_Settings_Window::SMP_Settings_Window( QWidget *parent )
	: QDialog( parent )
{
	ui.setupUi( this );

	// Keep checkboxes and text field in sync
	auto cb = [this]() { Sync_Checkboxes_To_Text(); };
	connect( ui.CB_KVM_Off, &QCheckBox::toggled, this, cb );
	connect( ui.CB_Hide_Hypervisor, &QCheckBox::toggled, this, cb );
	connect( ui.CB_TopoExt, &QCheckBox::toggled, this, cb );
	connect( ui.CB_HV_Relaxed, &QCheckBox::toggled, this, cb );
	connect( ui.CB_HV_VAPIC, &QCheckBox::toggled, this, cb );
	connect( ui.CB_HV_Time, &QCheckBox::toggled, this, cb );
	connect( ui.CB_HV_Stimer, &QCheckBox::toggled, this, cb );
	connect( ui.CB_HV_SynIC, &QCheckBox::toggled, this, cb );
	connect( ui.CB_HV_Reset, &QCheckBox::toggled, this, cb );
	connect( ui.CB_HV_Frequencies, &QCheckBox::toggled, this, cb );
	connect( ui.CB_HV_Spinlocks, &QCheckBox::toggled, this, cb );
	connect( ui.CB_HV_Vendor_ID, &QCheckBox::toggled, this, cb );
	// Keep checkboxes in sync when text field is typed into
	connect( ui.LE_CPU_Flags, &QLineEdit::textChanged, this, [this]() { Sync_Text_To_Checkboxes(); });
	// Also update text when value sub-fields change
	connect( ui.LE_HV_Spinlocks, &QLineEdit::textChanged, this, [this]() { if(ui.CB_HV_Spinlocks->isChecked()) Sync_Checkboxes_To_Text(); });
	connect( ui.LE_HV_Vendor_ID, &QLineEdit::textChanged, this, [this]() { if(ui.CB_HV_Vendor_ID->isChecked()) Sync_Checkboxes_To_Text(); });

	connect( ui.PB_Hyperthreading, &QPushButton::clicked, this, [this]()
	{
		int count = ui.SB_SMP->value();
		int cores = ( count % 2 == 0 ) ? count / 2 : count;
		int threads = ( count % 2 == 0 ) ? 2 : 1;
		ui.SB_Cores->setValue( cores );
		ui.SB_Threads->setValue( threads );
		ui.SB_Sockets->setValue( 1 );
		ui.SB_MaxCPUs->setValue( count );
	});

	// Optimized presets
	connect( ui.PB_Optimized_AMD, &QPushButton::clicked, this, [this]()
	{
		Set_Optimized_Preset( "AuthenticAMD", "AuthenticAMD" );
	});

	connect( ui.PB_Optimized_Intel, &QPushButton::clicked, this, [this]()
	{
		Set_Optimized_Preset( "GenuineIntel", "GenuineIntel" );
	});

	connect( ui.PB_Detect_Host_CPU, &QPushButton::clicked, this, [this]()
	{
		QFile cpuinfo( "/proc/cpuinfo" );
		if( cpuinfo.open(QIODevice::ReadOnly) )
		{
			QTextStream in( &cpuinfo );
			while( ! in.atEnd() )
			{
				QString line = in.readLine();
				if( line.startsWith("vendor_id") )
				{
					QString vendor = line.section( ':', 1 ).trimmed();
					if( vendor == "AuthenticAMD" )
						Set_Optimized_Preset( "AuthenticAMD", "AuthenticAMD" );
					else
						Set_Optimized_Preset( "GenuineIntel", "GenuineIntel" );
					break;
				}
			}
		}
	});
}

VM::SMP_Options SMP_Settings_Window::Get_Values() const
{
	VM::SMP_Options smp;
	
	smp.SMP_Count = ui.SB_SMP->value();
	smp.SMP_Cores = ui.SB_Cores->value();
	smp.SMP_Threads = ui.SB_Threads->value();
	smp.SMP_Sockets = ui.SB_Sockets->value();
	smp.SMP_MaxCPUs = ui.SB_MaxCPUs->value();
	
	return smp;
}

void SMP_Settings_Window::Set_Values( const VM::SMP_Options &smp, unsigned short PSO_SMP_Count, bool PSO_SMP_Cores,
									  bool PSO_SMP_Threads, bool PSO_SMP_Sockets, bool PSO_SMP_MaxCPUs )
{
	Backup_SMP = smp;
	
	ui.SB_SMP->setValue( smp.SMP_Count );
	ui.SB_Cores->setValue( smp.SMP_Cores );
	ui.SB_Threads->setValue( smp.SMP_Threads );
	ui.SB_Sockets->setValue( smp.SMP_Sockets );
	ui.SB_MaxCPUs->setValue( smp.SMP_MaxCPUs );
	
	ui.SB_SMP->setMaximum( PSO_SMP_Count );
	
	ui.SB_Cores->setEnabled( PSO_SMP_Cores );
	ui.Label_Cores->setEnabled( PSO_SMP_Cores );
	ui.SB_Threads->setEnabled( PSO_SMP_Threads );
	ui.Lebel_Threads->setEnabled( PSO_SMP_Threads );
	ui.SB_Sockets->setEnabled( PSO_SMP_Sockets );
	ui.Label_Sockets->setEnabled( PSO_SMP_Sockets );
	ui.SB_MaxCPUs->setEnabled( PSO_SMP_MaxCPUs );
	ui.Label_MaxCPUs->setEnabled( PSO_SMP_MaxCPUs );
}

QString SMP_Settings_Window::Get_CPU_Flags() const
{
	return ui.LE_CPU_Flags->text();
}

void SMP_Settings_Window::Set_CPU_Flags( const QString &flags )
{
	Backup_CPU_Flags = flags;
	ui.LE_CPU_Flags->setText( flags );
	Sync_Text_To_Checkboxes();
}

bool SMP_Settings_Window::Get_CPU_PM_Overcommit() const
{
	return ui.CH_CPU_PM_Overcommit->isChecked();
}

void SMP_Settings_Window::Set_CPU_PM_Overcommit( bool use )
{
	Backup_CPU_PM_Overcommit = use;
	ui.CH_CPU_PM_Overcommit->setChecked( use );
}

void SMP_Settings_Window::Set_Optimized_Preset( const QString &vendor, const QString &vendorId )
{
	ui.CB_KVM_Off->setChecked( true );
	ui.CB_Hide_Hypervisor->setChecked( true );
	ui.CB_TopoExt->setChecked( vendor == "AuthenticAMD" );
	ui.CB_HV_Relaxed->setChecked( true );
	ui.CB_HV_VAPIC->setChecked( true );
	ui.CB_HV_Time->setChecked( true );
	ui.CB_HV_Stimer->setChecked( false );
	ui.CB_HV_SynIC->setChecked( false );
	ui.CB_HV_Reset->setChecked( false );
	ui.CB_HV_Frequencies->setChecked( false );
	ui.CB_HV_Spinlocks->setChecked( true );
	ui.LE_HV_Spinlocks->setText( "0x1fff" );
	ui.CB_HV_Vendor_ID->setChecked( true );
	ui.LE_HV_Vendor_ID->setText( vendorId );
	ui.CH_CPU_PM_Overcommit->setChecked( true );
	Sync_Checkboxes_To_Text();
}

void SMP_Settings_Window::Set_SMP_Count( int count )
{
	// Keep existing topology intact
	unsigned short cur_cores   = ui.SB_Cores->value();
	unsigned short cur_threads = ui.SB_Threads->value();
	unsigned short cur_sockets = ui.SB_Sockets->value();
	unsigned short cur_maxcpus = ui.SB_MaxCPUs->value();

	// Auto-fill topology when still unconfigured or the product < count
	// (e.g. a template default cores=1 can't accommodate count=8)
	unsigned short cur_product = cur_cores * cur_threads * cur_sockets;
	if( cur_product == 0 || cur_product < (unsigned short)count )
	{
		Backup_SMP.SMP_Cores = count;
		Backup_SMP.SMP_Threads = 1;
		Backup_SMP.SMP_Sockets = 1;
		Backup_SMP.SMP_MaxCPUs = count;
		ui.SB_Cores->setValue( Backup_SMP.SMP_Cores );
		ui.SB_Threads->setValue( Backup_SMP.SMP_Threads );
		ui.SB_Sockets->setValue( Backup_SMP.SMP_Sockets );
		ui.SB_MaxCPUs->setValue( Backup_SMP.SMP_MaxCPUs );
	}
	else
	{
		Backup_SMP.SMP_Cores = cur_cores;
		Backup_SMP.SMP_Threads = cur_threads;
		Backup_SMP.SMP_Sockets = cur_sockets;
		Backup_SMP.SMP_MaxCPUs = cur_maxcpus;
	}

	Backup_SMP.SMP_Count = count;
	ui.SB_SMP->setValue( count );
}

void SMP_Settings_Window::Sync_Checkboxes_To_Text()
{
	if( Syncing_Flags ) return;
	Syncing_Flags = true;
	
	QStringList parts;
	
	if( ui.CB_KVM_Off->isChecked() )        parts << "kvm=off";
	if( ui.CB_Hide_Hypervisor->isChecked() ) parts << "-hypervisor";
	if( ui.CB_TopoExt->isChecked() )         parts << "+topoext";
	if( ui.CB_HV_Relaxed->isChecked() )      parts << "hv_relaxed";
	if( ui.CB_HV_VAPIC->isChecked() )        parts << "hv_vapic";
	if( ui.CB_HV_Time->isChecked() )         parts << "hv_time";
	if( ui.CB_HV_Stimer->isChecked() )       parts << "hv_stimer";
	if( ui.CB_HV_SynIC->isChecked() )        parts << "hv_synic";
	if( ui.CB_HV_Reset->isChecked() )        parts << "hv_reset";
	if( ui.CB_HV_Frequencies->isChecked() )  parts << "hv_frequencies";
	if( ui.CB_HV_Spinlocks->isChecked() )
		parts << "hv_spinlocks=" + ui.LE_HV_Spinlocks->text();
	if( ui.CB_HV_Vendor_ID->isChecked() )
		parts << "hv_vendor_id=" + ui.LE_HV_Vendor_ID->text();
	
	// Preserve any manually typed flags that don't match checkboxes
	QStringList current = ui.LE_CPU_Flags->text().split(",", Qt::SkipEmptyParts);
	QStringList known_prefixes = { "kvm", "-hypervisor", "+topoext", "hv_relaxed", "hv_vapic",
	                               "hv_time", "hv_stimer", "hv_synic", "hv_reset",
	                               "hv_frequencies", "hv_spinlocks", "hv_vendor_id" };
	for( int i = 0; i < current.count(); ++i )
	{
		bool is_known = false;
		for( int k = 0; k < known_prefixes.count(); ++k )
		{
			if( current[i].startsWith(known_prefixes[k]) )
			{
				is_known = true;
				break;
			}
		}
		if( ! is_known )
			parts << current[i];
	}
	
	ui.LE_CPU_Flags->setText( parts.join(",") );
	
	Syncing_Flags = false;
}

void SMP_Settings_Window::Sync_Text_To_Checkboxes()
{
	if( Syncing_Flags ) return;
	Syncing_Flags = true;
	ui.CB_KVM_Off->setChecked( false );
	ui.CB_Hide_Hypervisor->setChecked( false );
	ui.CB_TopoExt->setChecked( false );
	ui.CB_HV_Relaxed->setChecked( false );
	ui.CB_HV_VAPIC->setChecked( false );
	ui.CB_HV_Time->setChecked( false );
	ui.CB_HV_Stimer->setChecked( false );
	ui.CB_HV_SynIC->setChecked( false );
	ui.CB_HV_Reset->setChecked( false );
	ui.CB_HV_Frequencies->setChecked( false );
	ui.CB_HV_Spinlocks->setChecked( false );
	ui.CB_HV_Vendor_ID->setChecked( false );
	
	QStringList parts = ui.LE_CPU_Flags->text().split(",", Qt::SkipEmptyParts);
	for( int i = 0; i < parts.count(); ++i )
	{
		QString p = parts[i].trimmed();
		if( p == "kvm=off" )                    ui.CB_KVM_Off->setChecked( true );
		else if( p == "-hypervisor" )            ui.CB_Hide_Hypervisor->setChecked( true );
		else if( p == "+topoext" )               ui.CB_TopoExt->setChecked( true );
		else if( p == "hv_relaxed" )             ui.CB_HV_Relaxed->setChecked( true );
		else if( p == "hv_vapic" )               ui.CB_HV_VAPIC->setChecked( true );
		else if( p == "hv_time" )                ui.CB_HV_Time->setChecked( true );
		else if( p == "hv_stimer" )              ui.CB_HV_Stimer->setChecked( true );
		else if( p == "hv_synic" )               ui.CB_HV_SynIC->setChecked( true );
		else if( p == "hv_reset" )               ui.CB_HV_Reset->setChecked( true );
		else if( p == "hv_frequencies" )         ui.CB_HV_Frequencies->setChecked( true );
		else if( p.startsWith("hv_spinlocks=") )
		{
			ui.CB_HV_Spinlocks->setChecked( true );
			ui.LE_HV_Spinlocks->setText( p.mid(QString("hv_spinlocks=").length()) );
		}
		else if( p.startsWith("hv_vendor_id=") )
		{
			ui.CB_HV_Vendor_ID->setChecked( true );
			ui.LE_HV_Vendor_ID->setText( p.mid(QString("hv_vendor_id=").length()) );
		}
	}
	
	Syncing_Flags = false;
}

void SMP_Settings_Window::done(int r)
{
    if ( QDialog::Accepted == r )
    {
        unsigned short sockets = ui.SB_Sockets->value();
        unsigned short cores   = ui.SB_Cores->value();
        unsigned short threads = ui.SB_Threads->value();
        unsigned short maxcpus = ui.SB_MaxCPUs->value();
        unsigned short cpus    = ui.SB_SMP->value();
        unsigned short product = sockets * cores * threads;

        if( product > 0 )
        {
            // QEMU 9.x requires: sockets * cores * threads == maxcpus
            if( maxcpus != product )
            {
                // Try to preserve maxcpus by adjusting cores first
                unsigned short base = threads * sockets;
                if( base > 0 && maxcpus % base == 0 )
                {
                    cores = maxcpus / base;
                    ui.SB_Cores->setValue( cores );
                }
                else
                {
                    maxcpus = product;
                    ui.SB_MaxCPUs->setValue( maxcpus );
                }
            }

            // Cap cpus to maxcpus
            if( cpus > maxcpus )
            {
                cpus = maxcpus;
                ui.SB_SMP->setValue( cpus );
            }
        }

        // Remember accepted state so the dialog keeps it between opens
        Backup_SMP.SMP_Count = ui.SB_SMP->value();
        Backup_SMP.SMP_Cores = ui.SB_Cores->value();
        Backup_SMP.SMP_Threads = ui.SB_Threads->value();
        Backup_SMP.SMP_Sockets = ui.SB_Sockets->value();
        Backup_SMP.SMP_MaxCPUs = ui.SB_MaxCPUs->value();
        Backup_CPU_Flags = ui.LE_CPU_Flags->text();
        Backup_CPU_PM_Overcommit = ui.CH_CPU_PM_Overcommit->isChecked();

        QDialog::done(r);
        return;
    }
    else //cancel
    {
	    ui.SB_SMP->setValue( Backup_SMP.SMP_Count );
	    ui.SB_Cores->setValue( Backup_SMP.SMP_Cores );
	    ui.SB_Threads->setValue( Backup_SMP.SMP_Threads );
	    ui.SB_Sockets->setValue( Backup_SMP.SMP_Sockets );
	    ui.SB_MaxCPUs->setValue( Backup_SMP.SMP_MaxCPUs );
	    Set_CPU_Flags( Backup_CPU_Flags );
	    ui.CH_CPU_PM_Overcommit->setChecked( Backup_CPU_PM_Overcommit );
	
	    QDialog::done(r);
    }
}
