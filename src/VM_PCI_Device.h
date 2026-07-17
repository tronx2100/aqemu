#ifndef VM_PCI_DEVICE_H
#define VM_PCI_DEVICE_H

#include <QString>
#include <QStringList>
#include <QMap>

// Runtime-detected info for a host PCI device (not persisted)
struct PCI_Host_Device
{
    QString SysfsPath;
    QString Address;       // "0000:04:00.0"
    QString ShortAddress;  // "04:00.0"
    int IOMMUGroup = -1;
    QString VendorID;      // "8086"
    QString DeviceID;      // "10f0"
    QString ClassID;
    QString Driver;        // e.g. "vfio-pci" or ""
    QString DeviceName;    // human-readable from lspci
    bool IsPCIe = false;
    bool AloneInGroup = false;
};

// Persisted configuration for a VFIO PCI passthrough device
class VM_PCI_Device
{
public:
    VM_PCI_Device();
    VM_PCI_Device( const VM_PCI_Device &other );
    ~VM_PCI_Device();

    bool operator==( const VM_PCI_Device &other ) const;
    bool operator!=( const VM_PCI_Device &other ) const;
    VM_PCI_Device &operator=( const VM_PCI_Device &other );

    bool IsEnabled() const;
    void SetEnabled( bool on );

    const QString &Get_Host_Address() const;
    void Set_Host_Address( const QString &addr );

    const QString &Get_Bus() const;
    void Set_Bus( const QString &bus );

    const QString &Get_Addr() const;
    void Set_Addr( const QString &addr );

    bool Get_Multifunction() const;
    void Set_Multifunction( bool on );

    bool Get_XVGA() const;
    void Set_XVGA( bool on );

    bool Get_Use_ROM_File() const;
    void Set_Use_ROM_File( bool on );

    const QString &Get_ROM_File() const;
    void Set_ROM_File( const QString &path );

    bool Get_Use_ROM_Bar() const;
    void Set_Use_ROM_Bar( bool on );

    const QString &Get_ROM_Bar() const;
    void Set_ROM_Bar( const QString &val );

    bool Get_Disable_VGA() const;
    void Set_Disable_VGA( bool on );

    bool Get_ROM_Bar_Zero() const;
    void Set_ROM_Bar_Zero( bool on );

    bool Get_Disable_Idle() const;
    void Set_Disable_Idle( bool on );

    bool Get_No_KVM_MSI() const;
    void Set_No_KVM_MSI( bool on );

    bool Get_No_KVM_MSIX() const;
    void Set_No_KVM_MSIX( bool on );

    QStringList Get_Additional_Flags() const;
    void Set_Additional_Flags( const QStringList &flags );

    // Build the QEMU -device string for this VFIO device
    QString To_QEMU_Device_Arg() const;

    // Build the QEMU -device string for an associated root port
    static QStringList Build_Root_Port_Args( const QList<VM_PCI_Device> &devices );

private:
    bool Enabled;
    QString Host_Address;  // "04:00.0" (short form)
    QString Bus;           // e.g. "root.1"
    QString Addr;          // e.g. "00.0"
    bool Multifunction;
    bool XVGA;
    bool Use_ROM_File;
    QString ROM_File;
    bool Use_ROM_Bar;
    QString ROM_Bar;
    bool Disable_VGA;
    bool ROM_Bar_Zero;
    bool Disable_Idle;
    bool No_KVM_MSI;
    bool No_KVM_MSIX;
    QStringList Additional_Flags; // raw key=value pairs appended to the device string
};

#endif // VM_PCI_DEVICE_H
