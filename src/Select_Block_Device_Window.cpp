#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QTableWidgetItem>
#include <QRegularExpression>
#include <QProcess>

#include "Select_Block_Device_Window.h"

struct BlkInfo {
	QString label;
	QString fstype;
	QString partlabel;
};

static QMap<QString, BlkInfo> Run_Blkid()
{
	QMap<QString, BlkInfo> result;
	QProcess proc;
	proc.start( "blkid", QStringList() );
	if( ! proc.waitForFinished(5000) )
		return result;

	QByteArray data = proc.readAllStandardOutput();
	QTextStream in( &data, QIODevice::ReadOnly );

	static QRegularExpression kv_re( "(\\w+)=\"([^\"]*)\"" );

	while( ! in.atEnd() )
	{
		QString line = in.readLine().trimmed();
		if( line.isEmpty() )
			continue;

		// /dev/sda1: LABEL="..." UUID="..." TYPE="..." PARTLABEL="..."
		int colon = line.indexOf( ": " );
		if( colon < 0 ) continue;

		QString dev = line.left( colon );
		QString rest = line.mid( colon + 2 );

		BlkInfo info;
		QRegularExpressionMatchIterator it = kv_re.globalMatch( rest );
		while( it.hasNext() )
		{
			QRegularExpressionMatch m = it.next();
			QString key = m.captured(1);
			QString val = m.captured(2);
			if( key == "LABEL" )       info.label = val;
			if( key == "TYPE" )        info.fstype = val;
			if( key == "PARTLABEL" )   info.partlabel = val;
		}
		result[ dev ] = info;
	}
	return result;
}

Select_Block_Device_Window::Select_Block_Device_Window( QWidget *parent )
	: QDialog( parent )
{
	ui.setupUi( this );

	connect( ui.buttonBox, &QDialogButtonBox::accepted,
		this, &Select_Block_Device_Window::ButtonBox_Accepted );

	Refresh_Device_List();
}

QString Select_Block_Device_Window::Get_Device_Path() const
{
	return Selected_Device_Path;
}

void Select_Block_Device_Window::Refresh_Device_List()
{
	ui.Table_Devices->setRowCount( 0 );

	// Mount info from /proc/mounts
	QMap<QString, QString> mounted_fstype = Get_Mounted_Devices();

	// Block device info from blkid
	QMap<QString, BlkInfo> blk_info = Run_Blkid();

	bool show_partitions = ( ui.CB_Mode->currentIndex() == 0 );

	// Build map: kernel device name -> /dev/disk/by-id/ path
	QMap<QString, QString> dev_to_byid;
	QDir by_id_dir( "/dev/disk/by-id" );
	if( by_id_dir.exists() )
	{
		QStringList id_entries = by_id_dir.entryList( QDir::NoDotAndDotDot, QDir::Name );
		for( const QString &e : id_entries )
		{
			QString real = QFileInfo( by_id_dir.absoluteFilePath(e) ).symLinkTarget();
			if( ! real.isEmpty() )
				dev_to_byid[ QFileInfo(real).fileName() ] = by_id_dir.absoluteFilePath(e);
		}
	}

	// Parse /proc/partitions
	QFile parts( "/proc/partitions" );
	if( ! parts.open(QIODevice::ReadOnly) )
		return;

	QTextStream in( &parts );
	in.readLine(); // skip header

	while( ! in.atEnd() )
	{
		QString line = in.readLine().trimmed();
		if( line.isEmpty() )
			continue;

		QStringList fields = line.split( QRegularExpression("\\s+") );
		if( fields.size() < 4 )
			continue;

		quint64 blocks = fields[2].toULongLong();
		QString name = fields[3];

		if( blocks == 0 || name.startsWith("loop") || name.startsWith("ram") )
			continue;

		// Determine if this is a partition by name convention
		QString parent = name;
		if( name.startsWith("nvme") || name.startsWith("mmcblk") )
			parent.remove( QRegularExpression("p\\d+$") );
		else
			parent.remove( QRegularExpression("\\d+$") );

		bool is_partition = ( parent != name );

		if( show_partitions != is_partition )
			continue;

		// Find /dev/disk/by-id/ path
		QString byid_path = dev_to_byid.value( name );
		if( byid_path.isEmpty() )
			continue;

		// Read model from parent device
		QString model;
		QFile mf( "/sys/block/" + parent + "/device/model" );
		if( mf.open(QIODevice::ReadOnly) )
		{
			QTextStream ms( &mf );
			model = ms.readAll().trimmed();
		}

		// Info from blkid
		QString devpath = "/dev/" + name;
		BlkInfo blk = blk_info.value( devpath );

		// Label: use LABEL, fall back to PARTLABEL
		QString label = blk.label.isEmpty() ? blk.partlabel : blk.label;

		// FS type: blkid TYPE, then /proc/mounts fallback
		QString fstype = blk.fstype;
		if( fstype.isEmpty() )
			fstype = mounted_fstype.value( devpath );

		// Mount status from /proc/mounts
		bool is_mounted = mounted_fstype.contains( devpath );

		if( ui.CH_Show_Available_Only->isChecked() && is_mounted )
			continue;

		int row = ui.Table_Devices->rowCount();
		ui.Table_Devices->insertRow( row );

		// Col 0: Model
		QTableWidgetItem *mi = new QTableWidgetItem( model.isEmpty() ? tr("(Unknown)") : model );
		mi->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled );
		ui.Table_Devices->setItem( row, 0, mi );

		// Col 1: Label
		QTableWidgetItem *li = new QTableWidgetItem( label );
		li->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled );
		ui.Table_Devices->setItem( row, 1, li );

		// Col 2: FS (filesystem type)
		QTableWidgetItem *fsi = new QTableWidgetItem( fstype );
		fsi->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled );
		ui.Table_Devices->setItem( row, 2, fsi );

		// Col 3: Size
		QTableWidgetItem *si = new QTableWidgetItem( Human_Readable_Size( blocks * 2 ) );
		si->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled );
		ui.Table_Devices->setItem( row, 3, si );

		// Col 4: Device (short name)
		QTableWidgetItem *di = new QTableWidgetItem( name );
		di->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled );
		ui.Table_Devices->setItem( row, 4, di );

		// Col 5: Path (by-id)
		QTableWidgetItem *pi = new QTableWidgetItem( byid_path );
		pi->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled );
		ui.Table_Devices->setItem( row, 5, pi );

		// Col 6: Status
		QString status = is_mounted ? tr("In Use") : tr("Available");
		QTableWidgetItem *sti = new QTableWidgetItem( status );
		sti->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled );
		sti->setForeground( is_mounted ? QBrush(Qt::red) : QBrush(Qt::darkGreen) );
		ui.Table_Devices->setItem( row, 6, sti );
	}

	ui.Table_Devices->resizeColumnsToContents();
	if( ui.Table_Devices->columnWidth(0) > 250 )
		ui.Table_Devices->setColumnWidth( 0, 250 );
	if( ui.Table_Devices->columnWidth(5) > 400 )
		ui.Table_Devices->setColumnWidth( 5, 400 );
}

void Select_Block_Device_Window::ButtonBox_Accepted()
{
	int row = ui.Table_Devices->currentRow();
	if( row < 0 )
	{
		reject();
		return;
	}
	QTableWidgetItem *path_item = ui.Table_Devices->item( row, 5 );
	if( path_item )
		Selected_Device_Path = path_item->text();
	accept();
}

void Select_Block_Device_Window::on_CB_Mode_currentIndexChanged( int index )
{
	Refresh_Device_List();
}

void Select_Block_Device_Window::on_CH_Show_Available_Only_toggled()
{
	Refresh_Device_List();
}

void Select_Block_Device_Window::on_Table_Devices_itemDoubleClicked( QTableWidgetItem *item )
{
	int row = item->row();
	QTableWidgetItem *path_item = ui.Table_Devices->item( row, 5 );
	if( path_item )
	{
		Selected_Device_Path = path_item->text();
		accept();
	}
}

void Select_Block_Device_Window::on_BT_Refresh_clicked()
{
	Refresh_Device_List();
}

QString Select_Block_Device_Window::Human_Readable_Size( quint64 sectors ) const
{
	quint64 bytes = sectors * 512;
	double size = (double)bytes;

	if( size >= 1099511627776.0 )
		return QString::number( size / 1099511627776.0, 'f', 2 ) + " TiB";
	else if( size >= 1073741824.0 )
		return QString::number( size / 1073741824.0, 'f', 1 ) + " GiB";
	else if( size >= 1048576.0 )
		return QString::number( size / 1048576.0, 'f', 1 ) + " MiB";
	else
		return QString::number( bytes ) + " B";
}

QMap<QString, QString> Select_Block_Device_Window::Get_Mounted_Devices() const
{
	QMap<QString, QString> mounted;
	QProcess proc;
	proc.start( "findmnt", QStringList() << "-l" << "-n" << "-o" << "SOURCE,TARGET,FSTYPE" );
	if( ! proc.waitForFinished(5000) )
		return mounted;

	QByteArray data = proc.readAllStandardOutput();
	QTextStream in( &data, QIODevice::ReadOnly );

	while( ! in.atEnd() )
	{
		QString line = in.readLine().trimmed();
		if( line.isEmpty() )
			continue;

		QString dev = line.section( ' ', 0, 0 );
		if( ! dev.startsWith("/dev/") )
			continue;

		// Resolve symlinks to get canonical device name
		QString real = QFileInfo( dev ).canonicalFilePath();
		if( ! real.isEmpty() )
			mounted[ real ] = line.section( ' ', 2, 2 );
	}
	return mounted;
}
