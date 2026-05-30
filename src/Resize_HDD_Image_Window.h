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

#ifndef RESIZE_HDD_IMAGE_WINDOW_H
#define RESIZE_HDD_IMAGE_WINDOW_H

#include "ui_Resize_HDD_Image_Window.h"

class Resize_HDD_Image_Window : public QDialog
{
	Q_OBJECT

	public:
		Resize_HDD_Image_Window( QWidget *parent = 0 );
		void Set_Image_File_Name( const QString &path );

	public slots:
		void accept();

	private slots:
		void on_Button_Set_1GB_clicked();
		void on_Button_Set_5GB_clicked();
		void on_Button_Set_10GB_clicked();
		void on_Button_Set_20GB_clicked();
		void on_Button_Set_50GB_clicked();

	private:
		void Read_Current_Size();

		Ui::Resize_HDD_Image_Window ui;
		QString Image_File_Name;
};

#endif
