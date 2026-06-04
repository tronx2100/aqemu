#include <QSet>
#include <QStringList>
#include <algorithm>
#include "VM_PCI_Device.h"

VM_PCI_Device::VM_PCI_Device()
    : Enabled(false), Multifunction(false), XVGA(false),
      Use_ROM_File(false), Use_ROM_Bar(true), Disable_VGA(false), Disable_Idle(false)
{
}

VM_PCI_Device::VM_PCI_Device( const VM_PCI_Device &other )
    : Enabled(other.Enabled), Host_Address(other.Host_Address),
      Bus(other.Bus), Addr(other.Addr),
      Multifunction(other.Multifunction), XVGA(other.XVGA),
      Use_ROM_File(other.Use_ROM_File), ROM_File(other.ROM_File),
      Use_ROM_Bar(other.Use_ROM_Bar), ROM_Bar(other.ROM_Bar),
      Disable_VGA(other.Disable_VGA), Disable_Idle(other.Disable_Idle),
      Additional_Flags(other.Additional_Flags)
{
}

VM_PCI_Device::~VM_PCI_Device()
{
}

bool VM_PCI_Device::operator==( const VM_PCI_Device &other ) const
{
    return Enabled == other.Enabled
        && Host_Address == other.Host_Address
        && Bus == other.Bus
        && Addr == other.Addr
        && Multifunction == other.Multifunction
        && XVGA == other.XVGA
        && Use_ROM_File == other.Use_ROM_File
        && ROM_File == other.ROM_File
        && Use_ROM_Bar == other.Use_ROM_Bar
        && ROM_Bar == other.ROM_Bar
        && Disable_VGA == other.Disable_VGA
        && Disable_Idle == other.Disable_Idle
        && Additional_Flags == other.Additional_Flags;
}

bool VM_PCI_Device::operator!=( const VM_PCI_Device &other ) const
{
    return !( *this == other );
}

VM_PCI_Device &VM_PCI_Device::operator=( const VM_PCI_Device &other )
{
    if ( this != &other )
    {
        Enabled = other.Enabled;
        Host_Address = other.Host_Address;
        Bus = other.Bus;
        Addr = other.Addr;
        Multifunction = other.Multifunction;
        XVGA = other.XVGA;
        Use_ROM_File = other.Use_ROM_File;
        ROM_File = other.ROM_File;
        Use_ROM_Bar = other.Use_ROM_Bar;
        ROM_Bar = other.ROM_Bar;
        Disable_VGA = other.Disable_VGA;
        Disable_Idle = other.Disable_Idle;
        Additional_Flags = other.Additional_Flags;
    }
    return *this;
}

bool VM_PCI_Device::IsEnabled() const { return Enabled; }
void VM_PCI_Device::SetEnabled( bool on ) { Enabled = on; }

const QString &VM_PCI_Device::Get_Host_Address() const { return Host_Address; }
void VM_PCI_Device::Set_Host_Address( const QString &addr ) { Host_Address = addr; }

const QString &VM_PCI_Device::Get_Bus() const { return Bus; }
void VM_PCI_Device::Set_Bus( const QString &bus ) { Bus = bus; }

const QString &VM_PCI_Device::Get_Addr() const { return Addr; }
void VM_PCI_Device::Set_Addr( const QString &addr ) { Addr = addr; }

bool VM_PCI_Device::Get_Multifunction() const { return Multifunction; }
void VM_PCI_Device::Set_Multifunction( bool on ) { Multifunction = on; }

bool VM_PCI_Device::Get_XVGA() const { return XVGA; }
void VM_PCI_Device::Set_XVGA( bool on ) { XVGA = on; }

bool VM_PCI_Device::Get_Use_ROM_File() const { return Use_ROM_File; }
void VM_PCI_Device::Set_Use_ROM_File( bool on ) { Use_ROM_File = on; }

const QString &VM_PCI_Device::Get_ROM_File() const { return ROM_File; }
void VM_PCI_Device::Set_ROM_File( const QString &path ) { ROM_File = path; }

bool VM_PCI_Device::Get_Use_ROM_Bar() const { return Use_ROM_Bar; }
void VM_PCI_Device::Set_Use_ROM_Bar( bool on ) { Use_ROM_Bar = on; }

const QString &VM_PCI_Device::Get_ROM_Bar() const { return ROM_Bar; }
void VM_PCI_Device::Set_ROM_Bar( const QString &val ) { ROM_Bar = val; }

bool VM_PCI_Device::Get_Disable_VGA() const { return Disable_VGA; }
void VM_PCI_Device::Set_Disable_VGA( bool on ) { Disable_VGA = on; }

bool VM_PCI_Device::Get_Disable_Idle() const { return Disable_Idle; }
void VM_PCI_Device::Set_Disable_Idle( bool on ) { Disable_Idle = on; }

QStringList VM_PCI_Device::Get_Additional_Flags() const { return Additional_Flags; }
void VM_PCI_Device::Set_Additional_Flags( const QStringList &flags ) { Additional_Flags = flags; }

QString VM_PCI_Device::To_QEMU_Device_Arg() const
{
    if ( !Enabled || Host_Address.isEmpty() )
        return QString();

    QString arg = QString( "vfio-pci,host=%1" ).arg( Host_Address );

    if ( !Bus.isEmpty() )
        arg += QString( ",bus=%1" ).arg( Bus );

    if ( !Addr.isEmpty() )
        arg += QString( ",addr=%1" ).arg( Addr );

    if ( Multifunction )
        arg += ",multifunction=on";

    if ( XVGA )
        arg += ",x-vga=on";

    if ( Disable_VGA )
        arg += ",disable-vga=on";

    if ( Disable_Idle )
        arg += ",disable-idle=on";

    if ( Use_ROM_File && !ROM_File.isEmpty() )
        arg += QString( ",romfile=%1" ).arg( ROM_File );

    if ( !Use_ROM_Bar )
        arg += ",rombar=0";

    if ( !ROM_Bar.isEmpty() )
        arg += QString( ",rombar=%1" ).arg( ROM_Bar );

    for ( int i = 0; i < Additional_Flags.count(); i++ )
    {
        const QString &flag = Additional_Flags[i];
        if ( !flag.isEmpty() )
            arg += "," + flag;
    }

    return arg;
}

QStringList VM_PCI_Device::Build_Root_Port_Args( const QList<VM_PCI_Device> &devices )
{
    // Collect unique bus names
    QSet<QString> seenBuses;
    for ( int i = 0; i < devices.count(); i++ )
    {
        if ( !devices[i].IsEnabled() )
            continue;

        const QString &bus = devices[i].Get_Bus();
        if ( !bus.isEmpty() )
            seenBuses.insert( bus );
    }

    // Sort by numeric suffix so root.1 is always generated before root.2 etc.
    QStringList sortedBuses = seenBuses.values();
    std::sort( sortedBuses.begin(), sortedBuses.end(), []( const QString &a, const QString &b ) {
        return a.section( '.', -1 ).toInt() < b.section( '.', -1 ).toInt();
    } );

    // Generate ioh3420 root port, using the numeric suffix directly as port/chassis
    QStringList args;
    for ( int i = 0; i < sortedBuses.size(); i++ )
    {
        const QString &bus = sortedBuses[i];
        int portNum = bus.section( '.', -1 ).toInt(); // "root.1" -> 1
        int hexAddr = 0x1c + ( portNum - 1 ) * 2;
        QString rootAddr = QString( "%1.0" ).arg( hexAddr, 0, 16 );
        args << QString( "ioh3420,bus=pcie.0,addr=%1,multifunction=on,port=%2,chassis=%2,id=%3" )
                    .arg( rootAddr )
                    .arg( portNum )
                    .arg( bus );
    }

    return args;
}
