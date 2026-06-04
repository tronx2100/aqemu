#include "VFIO_PCI_Editor_Window.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QSet>
#include <QPainter>
#include <QComboBox>

// forward declarations of helpers defined later
static QString pciBaseName( const QString &shortAddr );
#if 0
static int pciFunction( const QString &shortAddr );
#endif
static bool pciClassTop( const QString &classID, quint8 classByte );
#include <QApplication>

VFIO_PCI_Editor_Window::VFIO_PCI_Editor_Window( QWidget *parent )
    : QDialog( parent ), Updating_Table( false )
{
    setWindowTitle( tr("PCIe VFIO Passthrough Selector") );
    resize( 1100, 700 );
    setMinimumSize( 900, 500 );
    setSizeGripEnabled( true );

    QVBoxLayout *mainLayout = new QVBoxLayout( this );

    // === Top toolbar: filter + show all ===
    QHBoxLayout *topLayout = new QHBoxLayout();
    QLabel *filterLabel = new QLabel( tr("Filter:") );
    Filter_Edit = new QLineEdit();
    Filter_Edit->setPlaceholderText( tr("Type to filter devices...") );
    Show_All_Check = new QCheckBox( tr("Show All Devices") );
    Show_All_Check->setToolTip( tr("When checked, show all PCI devices instead of only VFIO-bound ones") );
    topLayout->addWidget( filterLabel );
    topLayout->addWidget( Filter_Edit, 1 );
    topLayout->addWidget( Show_All_Check );
    mainLayout->addLayout( topLayout );

    // === Device table ===
    Device_Table = new QTableWidget( 0, 8 );
    Device_Table->setHorizontalHeaderLabels( QStringList()
        << tr("")          // col 0: enabled checkbox
        << tr("Group") // col 1
        << tr("")          // col 2: status LED
        << tr("Address")   // col 3
        << tr("Device")    // col 4
        << tr("Vendor:Dev") // col 5
        << tr("Type")      // col 6: detected type, user can override
        << tr("Driver")    // col 7
    );
    Device_Table->setSelectionBehavior( QAbstractItemView::SelectRows );
    Device_Table->setSelectionMode( QAbstractItemView::SingleSelection );
    Device_Table->setShowGrid( true );
    Device_Table->verticalHeader()->setVisible( false );
    Device_Table->setAlternatingRowColors( true );
    Device_Table->setSortingEnabled( false );
    Device_Table->horizontalHeader()->setSectionsMovable( true );
    for ( int c = 0; c < Device_Table->columnCount() - 1; c++ )
        Device_Table->horizontalHeader()->setSectionResizeMode( c, QHeaderView::Interactive );
    Device_Table->horizontalHeader()->setSectionResizeMode( Device_Table->columnCount() - 1, QHeaderView::Stretch );
    Device_Table->setColumnWidth( 0, 30 );
    Device_Table->setColumnWidth( 1, 100 );
    Device_Table->setColumnWidth( 2, 24 );
    Device_Table->setColumnWidth( 3, 110 );
    Device_Table->setColumnWidth( 4, 300 );
    Device_Table->setColumnWidth( 5, 130 );
    Device_Table->setColumnWidth( 6, 80 );

    mainLayout->addWidget( Device_Table, 1 );

    // === Flag configuration panel ===
    Flag_Panel = new QWidget();
    Flag_Panel->setVisible( false );
    QVBoxLayout *flagPanelLayout = new QVBoxLayout( Flag_Panel );
    flagPanelLayout->setContentsMargins( 0, 4, 0, 0 );

    Flag_Header_Label = new QLabel( tr("Device Flags") );
    Flag_Header_Label->setStyleSheet( "font-weight: bold; font-size: 12px;" );
    flagPanelLayout->addWidget( Flag_Header_Label );

    QGroupBox *flagGroup = new QGroupBox( tr("QEMU Device Flags") );
    QGridLayout *flagGrid = new QGridLayout( flagGroup );

    // Row 0: Bus, Addr
    flagGrid->addWidget( new QLabel( tr("Bus:") ), 0, 0 );
    Flag_Bus_Edit = new QLineEdit();
    Flag_Bus_Edit->setPlaceholderText( "e.g. root.1" );
    Flag_Bus_Edit->setToolTip( tr("PCIe root port bus identifier (e.g. root.1). Leave empty for default.") );
    flagGrid->addWidget( Flag_Bus_Edit, 0, 1 );

    flagGrid->addWidget( new QLabel( tr("Addr:") ), 0, 2 );
    Flag_Addr_Edit = new QLineEdit();
    Flag_Addr_Edit->setPlaceholderText( "e.g. 00.0" );
    Flag_Addr_Edit->setToolTip( tr("Address on the root port bus (e.g. 00.0)") );
    flagGrid->addWidget( Flag_Addr_Edit, 0, 3 );

    // Row 1: multifunction, x-vga, disable-vga
    Flag_Multifunction = new QCheckBox( tr("Multifunction") );
    Flag_Multifunction->setToolTip( tr("Enable multifunction=on for this device") );
    flagGrid->addWidget( Flag_Multifunction, 1, 0, 1, 2 );

    Flag_XVGA = new QCheckBox( tr("x-vga") );
    Flag_XVGA->setToolTip( tr("Enable x-vga=on for VGA assignment") );
    flagGrid->addWidget( Flag_XVGA, 1, 2 );

    Flag_Disable_VGA = new QCheckBox( tr("Disable VGA") );
    Flag_Disable_VGA->setToolTip( tr("Enable disable-vga=on") );
    flagGrid->addWidget( Flag_Disable_VGA, 1, 3 );

    // Row 2: disable-idle, ROM file
    Flag_Disable_Idle = new QCheckBox( tr("Disable Idle") );
    Flag_Disable_Idle->setToolTip( tr("Enable disable-idle=on") );
    flagGrid->addWidget( Flag_Disable_Idle, 2, 0 );

    Flag_Use_ROM_File = new QCheckBox( tr("Use ROM File") );
    Flag_Use_ROM_File->setToolTip( tr("Specify a custom VGA BIOS ROM file") );
    flagGrid->addWidget( Flag_Use_ROM_File, 2, 1 );

    Flag_ROM_File_Edit = new QLineEdit();
    Flag_ROM_File_Edit->setPlaceholderText( tr("Path to ROM file...") );
    Flag_ROM_File_Edit->setEnabled( false );
    flagGrid->addWidget( Flag_ROM_File_Edit, 2, 2 );

    Flag_ROM_File_Browse = new QPushButton( "..." );
    Flag_ROM_File_Browse->setFixedWidth( 30 );
    Flag_ROM_File_Browse->setEnabled( false );
    flagGrid->addWidget( Flag_ROM_File_Browse, 2, 3 );

    // Row 3: Additional flags
    flagGrid->addWidget( new QLabel( tr("Additional Flags:") ), 3, 0, 1, 4 );

    QHBoxLayout *addFlagLayout = new QHBoxLayout();
    Flag_New_Flag_Edit = new QLineEdit();
    Flag_New_Flag_Edit->setPlaceholderText( tr("key=value") );
    Flag_Add_Button = new QPushButton( tr("Add") );
    Flag_Remove_Button = new QPushButton( tr("Remove Selected") );
    Flag_Remove_Button->setEnabled( false );
    addFlagLayout->addWidget( Flag_New_Flag_Edit, 1 );
    addFlagLayout->addWidget( Flag_Add_Button );
    addFlagLayout->addWidget( Flag_Remove_Button );
    flagGrid->addLayout( addFlagLayout, 4, 0, 1, 4 );

    Flag_Additional_List = new QListWidget();
    Flag_Additional_List->setMaximumHeight( 80 );
    flagGrid->addWidget( Flag_Additional_List, 5, 0, 1, 4 );

    flagPanelLayout->addWidget( flagGroup );
    mainLayout->addWidget( Flag_Panel );

    // === Bottom buttons ===
    QHBoxLayout *buttonLayout = new QHBoxLayout();

    Move_Up_Button = new QPushButton( tr("Move Up") );
    Move_Up_Button->setEnabled( false );
    Move_Up_Button->setToolTip( tr("Move the selected device earlier in the list (lower root port number)") );
    buttonLayout->addWidget( Move_Up_Button );
    Move_Down_Button = new QPushButton( tr("Move Down") );
    Move_Down_Button->setEnabled( false );
    Move_Down_Button->setToolTip( tr("Move the selected device later in the list (higher root port number)") );
    buttonLayout->addWidget( Move_Down_Button );

    buttonLayout->addStretch();
    QPushButton *okBtn = new QPushButton( tr("OK") );
    QPushButton *cancelBtn = new QPushButton( tr("Cancel") );
    buttonLayout->addWidget( okBtn );
    buttonLayout->addWidget( cancelBtn );
    mainLayout->addLayout( buttonLayout );

    connect( Move_Up_Button, &QPushButton::clicked, this, &VFIO_PCI_Editor_Window::on_Move_Up_clicked );
    connect( Move_Down_Button, &QPushButton::clicked, this, &VFIO_PCI_Editor_Window::on_Move_Down_clicked );
    connect( okBtn, &QPushButton::clicked, this, &QDialog::accept );
    connect( cancelBtn, &QPushButton::clicked, this, &QDialog::reject );
    connect( Filter_Edit, &QLineEdit::textChanged, this, &VFIO_PCI_Editor_Window::on_Filter_Text_Changed );
    connect( Show_All_Check, &QCheckBox::stateChanged, this, &VFIO_PCI_Editor_Window::on_Show_All_Changed );
    connect( Device_Table, &QTableWidget::itemChanged, this, &VFIO_PCI_Editor_Window::on_Table_Item_Changed );
    connect( Device_Table, &QTableWidget::currentItemChanged, this, &VFIO_PCI_Editor_Window::on_Table_Current_Item_Changed );
    connect( Device_Table, &QTableWidget::currentCellChanged, this, [this]( int row, int, int, int ) {
        Move_Up_Button->setEnabled( row > 0 && row < Device_Rows.size() );
        Move_Down_Button->setEnabled( row >= 0 && row < Device_Rows.size() - 1 );
    } );
    connect( Flag_Bus_Edit, &QLineEdit::textChanged, this, &VFIO_PCI_Editor_Window::on_Bus_Changed );
    connect( Flag_Addr_Edit, &QLineEdit::textChanged, this, &VFIO_PCI_Editor_Window::on_Addr_Changed );
    connect( Flag_Multifunction, &QCheckBox::stateChanged, this, &VFIO_PCI_Editor_Window::on_Multifunction_Changed );
    connect( Flag_XVGA, &QCheckBox::stateChanged, this, &VFIO_PCI_Editor_Window::on_XVGA_Changed );
    connect( Flag_Use_ROM_File, &QCheckBox::stateChanged, this, &VFIO_PCI_Editor_Window::on_Use_ROM_File_Changed );
    connect( Flag_ROM_File_Browse, &QPushButton::clicked, this, &VFIO_PCI_Editor_Window::on_ROM_File_Browse );
    connect( Flag_Disable_VGA, &QCheckBox::stateChanged, this, &VFIO_PCI_Editor_Window::on_Disable_VGA_Changed );
    connect( Flag_Disable_Idle, &QCheckBox::stateChanged, this, &VFIO_PCI_Editor_Window::on_Disable_Idle_Changed );
    connect( Flag_Add_Button, &QPushButton::clicked, this, &VFIO_PCI_Editor_Window::on_Add_Flag );
    connect( Flag_Remove_Button, &QPushButton::clicked, this, &VFIO_PCI_Editor_Window::on_Remove_Flag );
    connect( Flag_Additional_List, &QListWidget::currentRowChanged, this, [this](int) {
        Flag_Remove_Button->setEnabled( Flag_Additional_List->currentRow() >= 0 );
    });

    // Scan host PCI devices on construction
    Refresh_Device_List();
}

VFIO_PCI_Editor_Window::~VFIO_PCI_Editor_Window()
{
}

QString VFIO_PCI_Editor_Window::Make_Address_Short( const QString &fullAddr ) const
{
    // "0000:04:00.0" -> "04:00.0"
    if ( fullAddr.length() > 7 && fullAddr.startsWith( "0000:" ) )
        return fullAddr.mid( 5 );
    return fullAddr;
}

void VFIO_PCI_Editor_Window::Refresh_Device_List()
{
    All_Host_Devices.clear();
    QSet<int> iommu_groups_seen;

    QDir sysfsDir( "/sys/bus/pci/devices" );
    QStringList entries = sysfsDir.entryList( QDir::Dirs | QDir::NoDotAndDotDot );

    for ( int i = 0; i < entries.size(); i++ )
    {
        const QString &entry = entries[i];
        PCI_Host_Device dev;
        dev.SysfsPath = QString( "/sys/bus/pci/devices/%1" ).arg( entry );
        dev.Address = entry;

        // Short address
        dev.ShortAddress = Make_Address_Short( entry );

        // Read vendor
        QFile vendorFile( dev.SysfsPath + "/vendor" );
        if ( vendorFile.open( QIODevice::ReadOnly ) )
        {
            dev.VendorID = QString::fromUtf8( vendorFile.readAll() ).trimmed();
            if ( dev.VendorID.startsWith( "0x" ) )
                dev.VendorID = dev.VendorID.mid( 2 );
        }

        // Read device ID
        QFile deviceFile( dev.SysfsPath + "/device" );
        if ( deviceFile.open( QIODevice::ReadOnly ) )
        {
            dev.DeviceID = QString::fromUtf8( deviceFile.readAll() ).trimmed();
            if ( dev.DeviceID.startsWith( "0x" ) )
                dev.DeviceID = dev.DeviceID.mid( 2 );
        }

        // Read class
        QFile classFile( dev.SysfsPath + "/class" );
        if ( classFile.open( QIODevice::ReadOnly ) )
        {
            dev.ClassID = QString::fromUtf8( classFile.readAll() ).trimmed();
        }

        // IOMMU group
        QFileInfo iommuLink( dev.SysfsPath + "/iommu_group" );
        if ( iommuLink.isSymLink() )
        {
            QString target = iommuLink.symLinkTarget();
            // target like /.../iommu_group/24
            int lastSlash = target.lastIndexOf( '/' );
            if ( lastSlash >= 0 )
            {
                bool ok = false;
                int grp = target.mid( lastSlash + 1 ).toInt( &ok );
                if ( ok )
                    dev.IOMMUGroup = grp;
            }
        }

        // Driver (via symlink)
        QFileInfo driverLink( dev.SysfsPath + "/driver" );
        if ( driverLink.isSymLink() )
        {
            QString driverPath = driverLink.symLinkTarget();
            int lastSlash = driverPath.lastIndexOf( '/' );
            if ( lastSlash >= 0 )
                dev.Driver = driverPath.mid( lastSlash + 1 );
        }

        // Check PCIe capability
        QFileInfo pcieLink( dev.SysfsPath + "/pcie_cap" );
        dev.IsPCIe = pcieLink.exists();

        // Track IOMMU group membership
        if ( dev.IOMMUGroup >= 0 )
            iommu_groups_seen.insert( dev.IOMMUGroup );

        All_Host_Devices << dev;
    }

    // Determine how many devices are in each IOMMU group
    QMap<int, int> iommu_group_counts;
    for ( int i = 0; i < All_Host_Devices.size(); i++ )
    {
        int g = All_Host_Devices[i].IOMMUGroup;
        if ( g >= 0 )
            iommu_group_counts[g]++;
    }

    for ( int i = 0; i < All_Host_Devices.size(); i++ )
    {
        int g = All_Host_Devices[i].IOMMUGroup;
        All_Host_Devices[i].AloneInGroup = ( g >= 0 && iommu_group_counts.value( g, 0 ) == 1 );
    }

    // Try to get human-readable names via lspci
    QProcess lspci;
    lspci.start( "lspci", QStringList() << "-nns" << "*" );
    if ( lspci.waitForFinished( 5000 ) )
    {
        QString output = QString::fromUtf8( lspci.readAllStandardOutput() );
        QStringList lines = output.split( '\n', Qt::SkipEmptyParts );
        for ( int l = 0; l < lines.size(); l++ )
        {
            // Format: "04:00.0 0300 8086:10f0 [Device Name]"
            QString line = lines[l].trimmed();
            int spaceIdx = line.indexOf( ' ' );
            if ( spaceIdx < 0 ) continue;
            QString addr = line.left( spaceIdx );
            QString rest = line.mid( spaceIdx + 1 ).trimmed();

            // rest might be "0300 8086:10f0 Driver ... [Device Name]"
            // Let's find the bracket part
            int bracketStart = rest.indexOf( '[' );
            int bracketEnd = rest.lastIndexOf( ']' );
            QString name;
            if ( bracketStart >= 0 && bracketEnd > bracketStart )
                name = rest.mid( bracketStart + 1, bracketEnd - bracketStart - 1 );

            if ( !name.isEmpty() )
            {
                for ( int d = 0; d < All_Host_Devices.size(); d++ )
                {
                    if ( All_Host_Devices[d].ShortAddress == addr )
                    {
                        All_Host_Devices[d].DeviceName = name;
                        break;
                    }
                }
            }
        }
    }

    Populate_Table();
}

void VFIO_PCI_Editor_Window::Populate_Table()
{
    Updating_Table = true;
    Device_Table->setRowCount( 0 );
    Device_Table->setRowCount( All_Host_Devices.size() );

    QString filterText = Filter_Edit->text().trimmed().toLower();
    bool showAll = Show_All_Check->isChecked();

    // Preserve existing configs before clearing
    QMap<QString,VM_PCI_Device> existingConfigs;
    for ( const DeviceRow &dr : qAsConst(Device_Rows) )
        existingConfigs[dr.Info.Address] = dr.Config;

    Device_Rows.clear();
    int visibleRow = 0;

    for ( int i = 0; i < All_Host_Devices.size(); i++ )
    {
        const PCI_Host_Device &info = All_Host_Devices[i];

        // Filter: only VFIO by default
        if ( !showAll && info.Driver != "vfio-pci" )
            continue;

        // Text filter
        if ( !filterText.isEmpty() )
        {
            bool matches = info.ShortAddress.contains( filterText, Qt::CaseInsensitive )
                        || info.DeviceName.contains( filterText, Qt::CaseInsensitive )
                        || info.VendorID.contains( filterText, Qt::CaseInsensitive )
                        || info.DeviceID.contains( filterText, Qt::CaseInsensitive )
                        || info.Driver.contains( filterText, Qt::CaseInsensitive );
            if ( !matches )
                continue;
        }

        // Restore existing config for this device
        VM_PCI_Device cfg = existingConfigs.value( info.Address );

        DeviceRow dr;
        dr.Row = visibleRow;
        dr.Info = info;
        dr.Config = cfg;
        Device_Rows << dr;

        bool enabled = cfg.IsEnabled();
        Fill_Row( visibleRow, info, enabled );
        visibleRow++;

        // connect enabled checkbox
        QWidget *cbWidget = Device_Table->cellWidget( dr.Row, 0 );
        if ( cbWidget )
        {
            QCheckBox *cb = cbWidget->findChild<QCheckBox*>();
            if ( cb )
            {
                connect( cb, &QCheckBox::toggled, this, [this, dr]( bool checked ) {
                    on_Enabled_Check_Changed( dr.Row, checked );
                });
            }
        }
    }

    Device_Table->setRowCount( visibleRow );
    Updating_Table = false;
}

void VFIO_PCI_Editor_Window::Fill_Row( int row, const PCI_Host_Device &info, bool enabled )
{
    // Col 0: enabled checkbox
    QWidget *cbWidget = new QWidget();
    QHBoxLayout *cbLayout = new QHBoxLayout( cbWidget );
    cbLayout->setContentsMargins( 0, 0, 0, 0 );
    cbLayout->setAlignment( Qt::AlignCenter );
    QCheckBox *cb = new QCheckBox();
    cb->setChecked( enabled );
    cbLayout->addWidget( cb );
    Device_Table->setCellWidget( row, 0, cbWidget );

    // Col 1: IOMMU Group
    QTableWidgetItem *iommuItem = new QTableWidgetItem();
    if ( info.IOMMUGroup >= 0 )
        iommuItem->setText( QString::number( info.IOMMUGroup ) );
    else
        iommuItem->setText( "-" );
    iommuItem->setTextAlignment( Qt::AlignCenter );
    iommuItem->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable );
    Device_Table->setItem( row, 1, iommuItem );

    // Col 2: Status LED (green if alone in group, yellow if shared)
    QColor ledColor;
    QString tooltip;
    if ( info.AloneInGroup )
    {
        ledColor = QColor( 0, 160, 0 );   // dark green
        tooltip = tr("Device is alone in its IOMMU group (safe for passthrough)");
    }
    else
    {
        ledColor = QColor( 200, 180, 0 ); // dark yellow
        tooltip = tr("Device shares IOMMU group with other devices (may need to pass all of them)");
    }

    QPixmap ledPixmap( 16, 16 );
    ledPixmap.fill( Qt::transparent );
    QPainter p( &ledPixmap );
    p.setRenderHint( QPainter::Antialiasing );
    p.setBrush( ledColor );
    p.setPen( Qt::NoPen );
    p.drawEllipse( 1, 1, 14, 14 );
    p.end();

    QLabel *ledLabel = new QLabel();
    ledLabel->setPixmap( ledPixmap );
    ledLabel->setToolTip( tooltip );

    QWidget *ledWidget = new QWidget();
    QHBoxLayout *ledLayout = new QHBoxLayout( ledWidget );
    ledLayout->setContentsMargins( 0, 0, 0, 0 );
    ledLayout->addStretch();
    ledLayout->addWidget( ledLabel );
    ledLayout->addStretch();
    Device_Table->setCellWidget( row, 2, ledWidget );

    // Col 3: Address
    QTableWidgetItem *addrItem = new QTableWidgetItem( info.ShortAddress );
    addrItem->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable );
    Device_Table->setItem( row, 3, addrItem );

    // Col 4: Device Name
    QString devName = info.DeviceName;
    if ( devName.isEmpty() )
        devName = QString( "PCI Device %1:%2" ).arg( info.VendorID, info.DeviceID );
    QTableWidgetItem *nameItem = new QTableWidgetItem( devName );
    nameItem->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable );
    Device_Table->setItem( row, 4, nameItem );

    // Col 5: Vendor:Device ID
    QString idStr = QString( "%1:%2" ).arg( info.VendorID, info.DeviceID );
    QTableWidgetItem *idItem = new QTableWidgetItem( idStr );
    idItem->setTextAlignment( Qt::AlignCenter );
    idItem->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable );
    Device_Table->setItem( row, 5, idItem );

    // Col 6: Type (dropdown: Audio / VGA / USB / Other)
    QComboBox *typeCombo = new QComboBox();
    typeCombo->addItem( tr("Other"), "other" );
    typeCombo->addItem( tr("Audio"), "audio" );
    typeCombo->addItem( tr("VGA"),   "vga" );
    typeCombo->addItem( tr("USB"),   "usb" );
    // Detect from class
    if      ( pciClassTop( info.ClassID, 0x03 ) ) typeCombo->setCurrentIndex( 2 ); // VGA
    else if ( pciClassTop( info.ClassID, 0x04 ) ) typeCombo->setCurrentIndex( 1 ); // Audio
    else if ( pciClassTop( info.ClassID, 0x0C ) )
    {
        quint32 classVal = info.ClassID.toUInt( nullptr, 16 );
        quint16 sub = classVal & 0xFFFF;
        if ( ( sub & 0xFF00 ) == 0x0300 ) typeCombo->setCurrentIndex( 3 ); // USB
        else                               typeCombo->setCurrentIndex( 0 ); // Other
    }
    else                                         typeCombo->setCurrentIndex( 0 ); // Other
    int captureRow = row;
    connect( typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
             this, [this, captureRow]( int idx ) {
                 on_Type_Combo_Changed( captureRow, idx );
             } );
    Device_Table->setCellWidget( row, 6, typeCombo );

    // Col 7: Driver
    QString driverStr = info.Driver.isEmpty() ? "-" : info.Driver;
    QTableWidgetItem *driverItem = new QTableWidgetItem( driverStr );
    driverItem->setTextAlignment( Qt::AlignCenter );
    driverItem->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable );
    Device_Table->setItem( row, 7, driverItem );

    Device_Table->setRowHeight( row, 22 );
}

void VFIO_PCI_Editor_Window::Set_PCI_Devices( const QList<VM_PCI_Device> &devices )
{
    // Merge existing config into our device rows
    for ( int r = 0; r < Device_Rows.size(); r++ )
    {
        for ( int d = 0; d < devices.size(); d++ )
        {
            if ( Device_Rows[r].Info.ShortAddress == devices[d].Get_Host_Address() )
            {
                Device_Rows[r].Config = devices[d];
                break;
            }
        }
    }

    // Restore manual order by sorting on bus number (root.1, root.2, …)
    // Devices without a bus (previously unconfigured) stay at the end.
    std::sort( Device_Rows.begin(), Device_Rows.end(), []( const DeviceRow &a, const DeviceRow &b ) {
        bool aOk = false, bOk = false;
        int busA = a.Config.Get_Bus().section( '.', -1 ).toInt( &aOk );
        int busB = b.Config.Get_Bus().section( '.', -1 ).toInt( &bOk );
        if ( aOk != bOk )
            return aOk; // configured before unconfigured
        if ( aOk )
            return busA < busB;
        return a.Info.ShortAddress < b.Info.ShortAddress;
    } );

    // Rebuild the table to reflect the new row order
    Rebuild_Table_From_Rows();
}

QList<VM_PCI_Device> VFIO_PCI_Editor_Window::Get_PCI_Devices() const
{
    // Ensure topology is assigned before returning
    const_cast<VFIO_PCI_Editor_Window*>( this )->Assign_Topology();

    QList<VM_PCI_Device> result;
    for ( int i = 0; i < Device_Rows.size(); i++ )
    {
        if ( Device_Rows[i].Config.IsEnabled() )
            result << Device_Rows[i].Config;
    }
    return result;
}

// Slots

void VFIO_PCI_Editor_Window::on_Filter_Text_Changed( const QString &text )
{
    Q_UNUSED(text);
    Populate_Table();
}

void VFIO_PCI_Editor_Window::on_Show_All_Changed( int state )
{
    Q_UNUSED(state);
    Populate_Table();
}

void VFIO_PCI_Editor_Window::on_Table_Item_Changed( QTableWidgetItem *item )
{
    Q_UNUSED(item);
    if ( Updating_Table )
        return;
    // only col 3 (Address) matters - but we handle checkboxes via cellWidget
}

void VFIO_PCI_Editor_Window::on_Table_Current_Item_Changed( QTableWidgetItem *current, QTableWidgetItem *previous )
{
    Q_UNUSED(previous);
    if ( current )
    {
        int row = current->row();
        Update_Flag_UI( row );
    }
    else
    {
        Clear_Flag_UI();
    }
}

static QString pciBaseName( const QString &shortAddr )
{
    int dot = shortAddr.lastIndexOf( '.' );
    return ( dot >= 0 ) ? shortAddr.left( dot ) : shortAddr;
}

// classID from sysfs looks like "0x030000" – return true if the top class byte matches
static bool pciClassTop( const QString &classID, quint8 classByte )
{
    bool ok = false;
    quint32 val = classID.toUInt( &ok, 16 );
    return ok && ( ( val >> 16 ) & 0xFF ) == classByte;
}

#if 0
static int pciFunction( const QString &shortAddr )
{
    int dot = shortAddr.lastIndexOf( '.' );
    if ( dot < 0 ) return 0;
    return shortAddr.mid( dot + 1 ).toInt();
}
#endif

void VFIO_PCI_Editor_Window::Rebuild_Table_From_Rows()
{
    Updating_Table = true;
    Device_Table->setRowCount( 0 );
    Device_Table->setRowCount( Device_Rows.size() );

    for ( int i = 0; i < Device_Rows.size(); i++ )
    {
        Device_Rows[i].Row = i;
        Fill_Row( i, Device_Rows[i].Info, Device_Rows[i].Config.IsEnabled() );

        QWidget *cbWidget = Device_Table->cellWidget( i, 0 );
        if ( cbWidget )
        {
            QCheckBox *cb = cbWidget->findChild<QCheckBox*>();
            if ( cb )
            {
                int captureRow = i;
                connect( cb, &QCheckBox::toggled, this, [this, captureRow]( bool checked ) {
                    on_Enabled_Check_Changed( captureRow, checked );
                });
            }
        }
    }
    Updating_Table = false;
}

void VFIO_PCI_Editor_Window::on_Move_Up_clicked()
{
    int row = Device_Table->currentRow();
    if ( row <= 0 || row >= Device_Rows.size() )
        return;

    DeviceRow temp = Device_Rows[row];
    Device_Rows[row] = Device_Rows[row - 1];
    Device_Rows[row - 1] = temp;

    Rebuild_Table_From_Rows();
    Device_Table->selectRow( row - 1 );
    Assign_Topology();
    Update_Flag_UI( row - 1 );
}

void VFIO_PCI_Editor_Window::on_Move_Down_clicked()
{
    int row = Device_Table->currentRow();
    if ( row < 0 || row >= Device_Rows.size() - 1 )
        return;

    DeviceRow temp = Device_Rows[row];
    Device_Rows[row] = Device_Rows[row + 1];
    Device_Rows[row + 1] = temp;

    Rebuild_Table_From_Rows();
    Device_Table->selectRow( row + 1 );
    Assign_Topology();
    Update_Flag_UI( row + 1 );
}

void VFIO_PCI_Editor_Window::Assign_Topology()
{
    // Assign root port numbers based on visual order in Device_Rows.
    // Devices sharing the same base address (bus:device, without function)
    // form a group and get the same root bus.
    QMap<QString, int> baseToPort;   // base address -> root port number
    QMap<QString, int> baseToCount;  // base address -> total enabled devices in group

    // First pass: assign port numbers on first occurrence, count group sizes
    int portNum = 0;
    for ( int i = 0; i < Device_Rows.size(); i++ )
    {
        if ( !Device_Rows[i].Config.IsEnabled() )
            continue;

        QString base = pciBaseName( Device_Rows[i].Info.ShortAddress );
        if ( !baseToPort.contains( base ) )
        {
            portNum++;
            baseToPort[base] = portNum;
        }
        baseToCount[base]++;
    }

    // Second pass: assign bus, addr, multifunction, x-vga
    QMap<QString, int> baseToFunc;
    for ( int i = 0; i < Device_Rows.size(); i++ )
    {
        if ( !Device_Rows[i].Config.IsEnabled() )
            continue;

        QString base = pciBaseName( Device_Rows[i].Info.ShortAddress );
        int p = baseToPort[base];
        int f = baseToFunc[base];
        baseToFunc[base] = f + 1;

        Device_Rows[i].Config.Set_Bus( QString( "root.%1" ).arg( p ) );
        Device_Rows[i].Config.Set_Addr( QString( "00.%1" ).arg( f ) );
        Device_Rows[i].Config.Set_Multifunction( baseToCount[base] > 1 );

        // x-vga=on for any device typed VGA
        QComboBox *combo = static_cast<QComboBox*>( Device_Table->cellWidget( i, 6 ) );
        if ( combo && combo->currentData().toString() == "vga" )
            Device_Rows[i].Config.Set_XVGA( true );
    }
}

void VFIO_PCI_Editor_Window::on_Enabled_Check_Changed( int row, bool checked )
{
    if ( row < 0 || row >= Device_Rows.size() )
        return;

    Device_Rows[row].Config.SetEnabled( checked );

    if ( checked )
    {
        // Set the host address from the detected device
        if ( Device_Rows[row].Config.Get_Host_Address().isEmpty() )
            Device_Rows[row].Config.Set_Host_Address( Device_Rows[row].Info.ShortAddress );
    }
    else
    {
        // Reset auto-assigned fields so re-enable triggers fresh topology
        Device_Rows[row].Config.Set_Bus( QString() );
        Device_Rows[row].Config.Set_Addr( QString() );
        Device_Rows[row].Config.Set_Multifunction( false );
    }

    Assign_Topology();

    // If the selected row changed, update flag UI
    if ( Device_Table->currentRow() == row )
    {
        if ( checked )
            Update_Flag_UI( row );
        else
            Clear_Flag_UI();
    }
}

void VFIO_PCI_Editor_Window::Update_Flag_UI( int row )
{
    if ( row < 0 || row >= Device_Rows.size() )
    {
        Clear_Flag_UI();
        return;
    }

    VM_PCI_Device &cfg = Device_Rows[row].Config;
    const PCI_Host_Device &info = Device_Rows[row].Info;

    if ( !cfg.IsEnabled() )
    {
        Clear_Flag_UI();
        return;
    }

    Updating_Table = true;

    Flag_Header_Label->setText( tr("Device Flags: %1 - %2").arg( info.ShortAddress, info.DeviceName ) );
    Flag_Panel->setVisible( true );

    Flag_Bus_Edit->setText( cfg.Get_Bus() );
    Flag_Addr_Edit->setText( cfg.Get_Addr() );
    Flag_Multifunction->setChecked( cfg.Get_Multifunction() );
    Flag_XVGA->setChecked( cfg.Get_XVGA() );
    Flag_Use_ROM_File->setChecked( cfg.Get_Use_ROM_File() );
    Flag_ROM_File_Edit->setText( cfg.Get_ROM_File() );
    Flag_ROM_File_Edit->setEnabled( cfg.Get_Use_ROM_File() );
    Flag_ROM_File_Browse->setEnabled( cfg.Get_Use_ROM_File() );
    Flag_Disable_VGA->setChecked( cfg.Get_Disable_VGA() );
    Flag_Disable_Idle->setChecked( cfg.Get_Disable_Idle() );

    Flag_Additional_List->clear();
    QStringList flags = cfg.Get_Additional_Flags();
    for ( int i = 0; i < flags.size(); i++ )
        Flag_Additional_List->addItem( flags[i] );
    Flag_Remove_Button->setEnabled( false );

    Updating_Table = false;
}

void VFIO_PCI_Editor_Window::Clear_Flag_UI()
{
    Flag_Panel->setVisible( false );
    Flag_Header_Label->setText( tr("Device Flags") );
    Flag_Bus_Edit->clear();
    Flag_Addr_Edit->clear();
    Flag_Multifunction->setChecked( false );
    Flag_XVGA->setChecked( false );
    Flag_Use_ROM_File->setChecked( false );
    Flag_ROM_File_Edit->clear();
    Flag_ROM_File_Edit->setEnabled( false );
    Flag_ROM_File_Browse->setEnabled( false );
    Flag_Disable_VGA->setChecked( false );
    Flag_Disable_Idle->setChecked( false );
    Flag_Additional_List->clear();
    Flag_New_Flag_Edit->clear();
    Flag_Remove_Button->setEnabled( false );
}

void VFIO_PCI_Editor_Window::on_Bus_Changed( const QString &text )
{
    if ( Updating_Table ) return;
    int row = Device_Table->currentRow();
    if ( row >= 0 && row < Device_Rows.size() )
        Device_Rows[row].Config.Set_Bus( text );
}

void VFIO_PCI_Editor_Window::on_Addr_Changed( const QString &text )
{
    if ( Updating_Table ) return;
    int row = Device_Table->currentRow();
    if ( row >= 0 && row < Device_Rows.size() )
        Device_Rows[row].Config.Set_Addr( text );
}

void VFIO_PCI_Editor_Window::on_Multifunction_Changed( int state )
{
    if ( Updating_Table ) return;
    int row = Device_Table->currentRow();
    if ( row >= 0 && row < Device_Rows.size() )
        Device_Rows[row].Config.Set_Multifunction( state == Qt::Checked );
}

void VFIO_PCI_Editor_Window::on_XVGA_Changed( int state )
{
    if ( Updating_Table ) return;
    int row = Device_Table->currentRow();
    if ( row >= 0 && row < Device_Rows.size() )
        Device_Rows[row].Config.Set_XVGA( state == Qt::Checked );
}

void VFIO_PCI_Editor_Window::on_Use_ROM_File_Changed( int state )
{
    bool on = ( state == Qt::Checked );
    Flag_ROM_File_Edit->setEnabled( on );
    Flag_ROM_File_Browse->setEnabled( on );

    if ( Updating_Table ) return;
    int row = Device_Table->currentRow();
    if ( row >= 0 && row < Device_Rows.size() )
        Device_Rows[row].Config.Set_Use_ROM_File( on );
}

void VFIO_PCI_Editor_Window::on_ROM_File_Browse()
{
    QString path = QFileDialog::getOpenFileName( this, tr("Select ROM File"),
                                                  Flag_ROM_File_Edit->text(),
                                                  tr("ROM Files (*.bin *.rom);;All Files (*)") );
    if ( !path.isEmpty() )
    {
        Flag_ROM_File_Edit->setText( path );
        int row = Device_Table->currentRow();
        if ( row >= 0 && row < Device_Rows.size() )
            Device_Rows[row].Config.Set_ROM_File( path );
    }
}

void VFIO_PCI_Editor_Window::on_Disable_VGA_Changed( int state )
{
    if ( Updating_Table ) return;
    int row = Device_Table->currentRow();
    if ( row >= 0 && row < Device_Rows.size() )
        Device_Rows[row].Config.Set_Disable_VGA( state == Qt::Checked );
}

void VFIO_PCI_Editor_Window::on_Disable_Idle_Changed( int state )
{
    if ( Updating_Table ) return;
    int row = Device_Table->currentRow();
    if ( row >= 0 && row < Device_Rows.size() )
        Device_Rows[row].Config.Set_Disable_Idle( state == Qt::Checked );
}

void VFIO_PCI_Editor_Window::on_Add_Flag()
{
    QString flag = Flag_New_Flag_Edit->text().trimmed();
    if ( flag.isEmpty() )
        return;

    int row = Device_Table->currentRow();
    if ( row >= 0 && row < Device_Rows.size() )
    {
        QStringList flags = Device_Rows[row].Config.Get_Additional_Flags();
        flags << flag;
        Device_Rows[row].Config.Set_Additional_Flags( flags );

        Flag_Additional_List->addItem( flag );
    }
    Flag_New_Flag_Edit->clear();
}

void VFIO_PCI_Editor_Window::on_Remove_Flag()
{
    int listRow = Flag_Additional_List->currentRow();
    if ( listRow < 0 )
        return;

    int row = Device_Table->currentRow();
    if ( row >= 0 && row < Device_Rows.size() )
    {
        QStringList flags = Device_Rows[row].Config.Get_Additional_Flags();
        if ( listRow < flags.size() )
        {
            flags.removeAt( listRow );
            Device_Rows[row].Config.Set_Additional_Flags( flags );
        }
    }

    delete Flag_Additional_List->takeItem( listRow );
    Flag_Remove_Button->setEnabled( Flag_Additional_List->currentRow() >= 0 );
}

void VFIO_PCI_Editor_Window::on_Type_Combo_Changed( int row, int index )
{
    if ( row < 0 || row >= Device_Rows.size() )
        return;

    Q_UNUSED(index);
    QWidget *w = Device_Table->cellWidget( row, 6 );
    if ( !w ) return;
    QComboBox *combo = static_cast<QComboBox*>( w );

    QString type = combo->currentData().toString();

    if ( type == "vga" )
    {
        Device_Rows[row].Config.Set_XVGA( true );
        Device_Rows[row].Config.Set_Multifunction( true );
    }
    else
    {
        Device_Rows[row].Config.Set_XVGA( false );
        // Re-evaluate multifunction from topology
        Assign_Topology();
    }

    // Refresh flag panel if this row is selected
    if ( Device_Table->currentRow() == row )
        Update_Flag_UI( row );
}
