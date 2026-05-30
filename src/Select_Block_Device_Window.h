#ifndef SELECT_BLOCK_DEVICE_WINDOW_H
#define SELECT_BLOCK_DEVICE_WINDOW_H

#include <QDialog>
#include <QStringList>
#include <QMap>
#include "ui_Select_Block_Device_Window.h"

class Select_Block_Device_Window: public QDialog
{
	Q_OBJECT

	public:
		Select_Block_Device_Window( QWidget *parent = 0 );

		QString Get_Device_Path() const;

	private slots:
		void Refresh_Device_List();
		void ButtonBox_Accepted();
		void on_CB_Mode_currentIndexChanged( int index );
		void on_CH_Show_Available_Only_toggled();
		void on_Table_Devices_itemDoubleClicked( QTableWidgetItem *item );
		void on_BT_Refresh_clicked();

	private:
		QString Human_Readable_Size( quint64 sectors ) const;
		QMap<QString, QString> Get_Mounted_Devices() const;

		Ui::Select_Block_Device_Window ui;
		QString Selected_Device_Path;
};

#endif
