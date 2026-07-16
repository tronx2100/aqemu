/****************************************************************************
**
** Copyright (C) 2008-2010 Andrey Rijov <ANDron142@yandex.ru>
** Copyright (C) 2016 Tobias Gläßer
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

#ifndef ADVANCED_SETTINGS_WINDOW_H
#define ADVANCED_SETTINGS_WINDOW_H

#include <QSettings>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include "VM_Devices.h"
#include "ui_Advanced_Settings_Window.h"

class Settings_Widget;

struct UsbHubGroup
{
    QString DisplayName;
    QStringList KernelNames; // e.g. ["2-2", "3-2"] for a paired HS+SS hub
};

class Advanced_Settings_Window: public QDialog
{
	Q_OBJECT

	public:
		Advanced_Settings_Window( QWidget *parent = 0 );
        ~Advanced_Settings_Window();

	public slots:
		void done(int);

	private slots:
        /// Old Setttings Window
		void on_Button_Create_Template_from_VM_clicked();
		void on_TB_VM_Folder_clicked();
		void CB_Language_currentIndexChanged( int index );
		void CB_Icons_Theme_currentIndexChanged( int index );
		void VNC_Warning( bool state );
		void Load_Templates();
        ///

		void on_TB_Browse_Before_clicked();
		void on_TB_Browse_After_clicked();
		void on_TB_Log_File_clicked();
		void on_TB_Screenshot_Folder_clicked();
		void on_TB_QEMU_IMG_Browse_clicked();

		void on_TB_Add_Emulator_clicked();
		void on_TB_Delete_Emulator_clicked();
		void on_TB_Edit_Emulator_clicked();
		void on_TB_Use_Default_clicked();
		void on_TB_Find_All_Emulators_clicked();
		void on_Emulators_Table_cellDoubleClicked( int row, int column );
		void on_Button_CDROM_Add_clicked();
		void on_Button_CDROM_Edit_clicked();
		void on_Button_CDROM_Delete_clicked();

		// Dynamic Hotplug tab
		void on_Hotplug_Scan_clicked();
		void on_Hotplug_Install_clicked();
		void on_Hotplug_Remove_clicked();

		bool Load_Emulators_Info();
		bool Save_Emulators_Info();
		void Update_Emulators_Info();

		QStringList Get_All_Emulators_Names() const;

	private:
		Ui::Advanced_Settings_Window ui;
		QSettings Settings;
        Settings_Widget* settings_widget;

		QList<Emulator> Emulators;

		// Dynamic Hotplug tab widgets
		QListWidget  *Hotplug_Hub_List;
		QLabel       *Hotplug_Status_Label;
		QPushButton  *Hotplug_Install_Btn;
		QPushButton  *Hotplug_Remove_Btn;
		QPushButton  *Hotplug_Detect_Btn;
		QLabel       *Hotplug_Detect_Label;
		QRadioButton *Hotplug_RB_Mode1; // pass only when QEMU running
		QRadioButton *Hotplug_RB_Mode2; // always block on host
		QList<UsbHubGroup> Hotplug_Groups;

		// Hub auto-detect state
		QTimer       *Hotplug_Detect_Timer;
		QStringList   Hotplug_Detect_KnownKernels;  // kernel name snapshot
		QStringList   Hotplug_Detect_KnownVidPids;  // VID:PID snapshot (reconnect guard)
		int           Hotplug_Detect_SecondsLeft;
		bool          Hotplug_Detect_FoundNew;       // new hub was seen, 10s cooldown running

		void Setup_Hotplug_Tab();
		QList<UsbHubGroup> Scan_USB_Hubs();
		QStringList        Current_Hub_Kernels();
		void Update_Hotplug_Status();
		void on_Hotplug_Detect_clicked();
		void on_Hotplug_Detect_Tick();
		bool Write_File_As_Root( const QString &path, const QString &content, QString &error );
		bool Remove_File_As_Root( const QString &path, QString &error );
};

#endif
