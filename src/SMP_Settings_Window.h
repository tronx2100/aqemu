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

#ifndef SMP_SETTINGS_WINDOW_H
#define SMP_SETTINGS_WINDOW_H

#include "ui_SMP_Settings_Window.h"
#include "VM_Devices.h"

class SMP_Settings_Window: public QDialog
{
	Q_OBJECT
	
	public:
		SMP_Settings_Window( QWidget *parent = 0 );
		
		VM::SMP_Options Get_Values() const;
		void Set_Values( const VM::SMP_Options &smp, unsigned short PSO_SMP_Count, bool PSO_SMP_Cores,
						 bool PSO_SMP_Threads, bool PSO_SMP_Sockets, bool PSO_SMP_MaxCPUs );
		QString Get_CPU_Flags() const;
		void Set_CPU_Flags( const QString &flags );

		bool Get_CPU_PM_Overcommit() const;
		void Set_CPU_PM_Overcommit( bool use );

		QString Get_CPU_Pinning() const;
		void Set_CPU_Pinning( const QString &pinning );
		
		void Set_SMP_Count( int count );

	signals:
		void Set_CPU_Type( const QString &cpu_type );
	
	private slots:
		void done(int);
	
	private:
		void Sync_Checkboxes_To_Text();
		void Sync_Text_To_Checkboxes();
		
		Ui::SMP_Settings_Window ui;
		VM::SMP_Options Backup_SMP;
		QString Backup_CPU_Flags;
		bool Backup_CPU_PM_Overcommit;
		QString Backup_CPU_Pinning;
		bool Syncing_Flags = false;

		void Set_Optimized_Preset( const QString &vendor, const QString &vendorId );
		void Clear_All_Flags();
};

#endif
