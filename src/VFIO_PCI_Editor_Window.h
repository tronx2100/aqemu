#ifndef VFIO_PCI_EDITOR_WINDOW_H
#define VFIO_PCI_EDITOR_WINDOW_H

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QMap>

#include "VM_PCI_Device.h"

class VFIO_PCI_Editor_Window : public QDialog
{
    Q_OBJECT

public:
    explicit VFIO_PCI_Editor_Window( QWidget *parent = 0 );
    ~VFIO_PCI_Editor_Window();

    // Set/get the persisted device configurations
    void Set_PCI_Devices( const QList<VM_PCI_Device> &devices );
    QList<VM_PCI_Device> Get_PCI_Devices() const;

private slots:
    void on_Filter_Text_Changed( const QString &text );
    void on_Show_All_Changed( int state );
    void on_Table_Item_Changed( QTableWidgetItem *item );
    void on_Table_Current_Item_Changed( QTableWidgetItem *current, QTableWidgetItem *previous );
    void on_Enabled_Check_Changed( int row, bool checked );
    void on_Bus_Changed( const QString &text );
    void on_Addr_Changed( const QString &text );
    void on_Type_Combo_Changed( int row, int index );
    void on_Multifunction_Changed( int state );
    void on_XVGA_Changed( int state );
    void on_Use_ROM_File_Changed( int state );
    void on_ROM_File_Browse();
    void on_Disable_VGA_Changed( int state );
    void on_No_KVM_MSI_Changed( int state );
    void on_No_KVM_MSIX_Changed( int state );
    void on_Add_Flag();
    void on_Remove_Flag();
    void on_Move_Up_clicked();
    void on_Move_Down_clicked();

private:
    struct DeviceRow {
        int Row;
        PCI_Host_Device Info;
        VM_PCI_Device Config;

        bool MatchesAll;
    };

    void Refresh_Device_List();
    void Populate_Table();
    void Fill_Row( int row, const PCI_Host_Device &info, bool enabled );
    void Update_Flag_UI( int row );
    void Clear_Flag_UI();
    QString Make_Address_Short( const QString &fullAddr ) const;
    void Assign_Topology();
    void Rebuild_Table_From_Rows();

    QLineEdit *Filter_Edit;
    QCheckBox *Show_All_Check;
    QTableWidget *Device_Table;
    QWidget *Flag_Panel;
    QLineEdit *Flag_Bus_Edit;
    QLineEdit *Flag_Addr_Edit;
    QCheckBox *Flag_Multifunction;
    QCheckBox *Flag_XVGA;
    QCheckBox *Flag_Use_ROM_File;
    QLineEdit *Flag_ROM_File_Edit;
    QPushButton *Flag_ROM_File_Browse;
    QCheckBox *Flag_Disable_VGA;
    QCheckBox *Flag_No_KVM_MSI;
    QCheckBox *Flag_No_KVM_MSIX;
    QListWidget *Flag_Additional_List;
    QLineEdit *Flag_New_Flag_Edit;
    QPushButton *Flag_Add_Button;
    QPushButton *Flag_Remove_Button;
    QLabel *Flag_Header_Label;
    QPushButton *Move_Up_Button;
    QPushButton *Move_Down_Button;

    QList<DeviceRow> Device_Rows;
    QList<PCI_Host_Device> All_Host_Devices;
    bool Updating_Table;
};

#endif // VFIO_PCI_EDITOR_WINDOW_H
