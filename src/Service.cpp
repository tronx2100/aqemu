/****************************************************************************
**
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

#include <QApplication>
#include <QResource>
#include <QMessageBox>
#include <QTranslator>
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QtDBus>
#include <QSettings>
#include <QHash>

#ifdef Q_OS_LINUX
#include <unistd.h>
#include <sys/types.h>
#endif
#include <iostream>

#include "Utils.h"
#include "Error_Log_Window.h"

#include "Run_Guard.h"

#include "VM.h"
#include "Service.h"
#include "main.h"

AQEMU_Service::AQEMU_Service()
{
    service = nullptr;
    main = nullptr;
    called_dbus = false;
    main_window = false;
    successful_init = false;
}

namespace
{
struct Archived_Error_Log
{
    QString uid;
    QString vm_name;
    QString vm_xml;
    QStringList entries;
};

static QHash<QString, Archived_Error_Log> Archived_Error_Logs;
static Error_Log_Window *Archived_Error_Window = nullptr;

static void Show_Archived_Error_Log_Window( const QString &title,
                                            const QString &vm_xml,
                                            const QString &uid,
                                            const QStringList &entries )
{
    if( Archived_Error_Window )
    {
        Archived_Error_Window->close();
        Archived_Error_Window->deleteLater();
        Archived_Error_Window = nullptr;
    }

    Archived_Error_Window = new Error_Log_Window();
    QObject::connect( Archived_Error_Window, &QObject::destroyed, []()
    {
        Archived_Error_Window = nullptr;
    } );
    Archived_Error_Window->Set_Clear_Target( vm_xml, uid );
    QObject::connect( Archived_Error_Window, &Error_Log_Window::Clear_Log_Requested,
                      [vm_xml, uid]()
    {
        AQEMU_Service::get().clear_error_log( vm_xml, uid );
    } );
    Archived_Error_Window->Restore_Log( entries );
    Archived_Error_Window->setWindowTitle( title );
    Archived_Error_Window->show();
    Archived_Error_Window->raise();
    Archived_Error_Window->activateWindow();
}
}

AQEMU_Service::~AQEMU_Service()
{
    delete service;
}

void AQEMU_Service::setMainWindow( bool b )
{
    main_window = b;
}

void AQEMU_Service::setMain( AQEMU_Main* m )
{
    main = m;
}

int AQEMU_Service::machineCount() const
{
    return machines.count();
}

void AQEMU_Service::vm_state_changed(Virtual_Machine *vm, VM::VM_State s)
{
    emit vm_state_changed_signal(vm, s);
    
    TEMPODEBUG( "AQEMU_Service::vm_state_changed",
                QString("vm_ptr=%1 uid=\"%2\" name=\"%3\" xml=\"%4\" state=%5 main_window=%6 machineCount=%7")
                .arg(reinterpret_cast<quintptr>(vm))
                .arg(vm ? vm->Get_UID() : QString())
                .arg(vm ? vm->Get_Machine_Name() : QString())
                .arg(vm ? vm->Get_VM_XML_File_Path() : QString())
                .arg(s)
                .arg(main_window)
                .arg(machines.count()) );

    if (! QDBusConnection::sessionBus().isConnected())
    {
        fprintf(stderr, "Cannot connect to the D-Bus session bus.\n"
                "To start it, run:\n"
                "\teval `dbus-launch --auto-syntax`\n");
    }
    else
    {
        QDBusInterface iface("org.aqemu.main_window", "/main_window", "", QDBusConnection::sessionBus());
        if (iface.isValid()) {
            /*QDBusReply<QString> reply =*/

            iface.call(QDBus::NoBlock, "VM_State_Changed", vm->Get_VM_XML_File_Path(), s );

            /*if (reply.isValid()) {
                printf("Reply was: %s\n", qPrintable(reply.value()));*/
            //    return;
            /*}

            fprintf(stderr, "Call failed: %s\n", qPrintable(reply.error().message()));
            return;*/
        }
        else
        {
            fprintf(stderr, "%s\n",
                    qPrintable(QDBusConnection::sessionBus().lastError().message()));
        }
    }

    //remove VM from service and shutdown the service, if there are no VMs running
    if ( s == VM::VMS_Power_Off )
    {
        Archived_Error_Log archived;
        archived.uid = vm->Get_UID();
        archived.vm_name = vm->Get_Machine_Name();
        archived.vm_xml = vm->Get_VM_XML_File_Path();
        archived.entries = vm->Get_QEMU_Error_Log_Entries();
        Archived_Error_Logs.insert( vm->Get_UID(), archived );

        TEMPODEBUG( "AQEMU_Service::vm_state_changed",
                    QString("archived error log uid=\"%1\" entries=%2 name=\"%3\" xml=\"%4\"")
                    .arg(vm->Get_UID())
                    .arg(archived.entries.count())
                    .arg(archived.vm_name)
                    .arg(archived.vm_xml) );
        machines.removeAll(vm);
        //delete vm; //Segfaults //Destructor was already called at this point //Investigate how/why
    }
    if ( machineCount() < 1 )
    {
        if ( ! main_window )
        {
            QMetaObject::invokeMethod(QCoreApplication::instance(), "quit");
            //application->quit();
        }
    }
}

bool AQEMU_Service::isActive()
{
    return ( machines.count() > 0 );
}

bool AQEMU_Service::call(const QString& command, const QList<QVariant>& params, bool noblock)
{
    called_dbus = true;
    QStringList params_debug;
    for( const QVariant &param : params )
        params_debug << param.toString();
    TEMPODEBUG( "AQEMU_Service::call",
                QString("command=\"%1\" params=[%2] noblock=%3 called_dbus=%4 successful_init=%5 main_window=%6 service_ptr=%7")
                .arg(command)
                .arg(params_debug.join(" | "))
                .arg(noblock)
                .arg(called_dbus)
                .arg(successful_init)
                .arg(main_window)
                .arg(reinterpret_cast<quintptr>(service)) );

    if ( init_service() )
    {
        if ( main )
        {
            int ret = main->load_settings();
            TEMPODEBUG( "AQEMU_Service::call",
                        QString("main->load_settings returned=%1").arg(ret) );
            if ( ret != 0 )
                return false;
        }

        successful_init = true;
    }

    if (!QDBusConnection::sessionBus().isConnected()) {
        TEMPODEBUG( "AQEMU_Service::call",
                    QString("DBus session bus not connected lastError=\"%1\"")
                    .arg(QDBusConnection::sessionBus().lastError().message()) );
        fprintf(stderr, "Cannot connect to the D-Bus session bus.\n"
                "To start it, run:\n"
                "\teval `dbus-launch --auto-syntax`\n");
        return false;
    }

    QDBusInterface iface(SERVICE_NAME, "/", "", QDBusConnection::sessionBus());
    if (iface.isValid()) {
        if ( noblock )
        {
            iface.callWithArgumentList(QDBus::NoBlock, command, params);
            return true;
        }
        else
        {
            QDBusReply<QString> reply = iface.callWithArgumentList(QDBus::AutoDetect, command, params);
            if (reply.isValid()) {
                std::cout<<qPrintable(reply.value())<<std::endl;
                return true;
            }

            TEMPODEBUG( "AQEMU_Service::call",
                        QString("DBus call failed command=\"%1\" error=\"%2\"")
                        .arg(command)
                        .arg(reply.error().message()) );
            fprintf(stderr, "Call failed: %s\n", qPrintable(reply.error().message()));
            return false;
        }
    }

    TEMPODEBUG( "AQEMU_Service::call",
                QString("DBus interface invalid command=\"%1\" lastError=\"%2\"")
                .arg(command)
                .arg(QDBusConnection::sessionBus().lastError().message()) );
    fprintf(stderr, "%s\n",
            qPrintable(QDBusConnection::sessionBus().lastError().message()));
    return false;
}

bool AQEMU_Service::call(const QString &command, Virtual_Machine *vm, bool noblock)
{
    return call(command, vm->Get_VM_XML_File_Path(), noblock);
}

bool AQEMU_Service::call(const QString &command, const QString& vm, bool noblock)
{
    QList<QVariant> list;
    list << vm;
    return call(command, list, noblock);
}

bool AQEMU_Service::call(const QString &command, bool noblock)
{
    QList<QVariant> list;
    return call(command, list, noblock);
}

bool AQEMU_Service::call(const QString &command, Virtual_Machine *vm, const QString& param2, bool noblock)
{
    QList<QVariant> list;
    list << vm->Get_VM_XML_File_Path();
    list << param2;
    return call(command, list, noblock);
}


bool AQEMU_Service::init_service()
{
    service = new Run_Guard( "Gmp[0Ab6000" ); //if service is already running, skip this
    TEMPODEBUG( "AQEMU_Service::init_service",
                QString("run_guard_ptr=%1 key_set service_ptr=%2")
                .arg(reinterpret_cast<quintptr>(service))
                .arg(reinterpret_cast<quintptr>(service)) );
    if (service->tryToRun() == false)
    {
        TEMPODEBUG( "AQEMU_Service::init_service", "Run_Guard refused service startup" );
        delete service;
        service = nullptr;
        return false;
    }

    //dbus listening stuff

    if (!QDBusConnection::sessionBus().isConnected()) {
        TEMPODEBUG( "AQEMU_Service::init_service",
                    QString("DBus session bus not connected lastError=\"%1\"")
                    .arg(QDBusConnection::sessionBus().lastError().message()) );
        fprintf(stderr, "Cannot connect to the D-Bus session bus.\n"
                "To start it, run:\n"
                "\teval `dbus-launch --auto-syntax`\n");
        return false;
    }

    if (!QDBusConnection::sessionBus().registerService(SERVICE_NAME)) {
        TEMPODEBUG( "AQEMU_Service::init_service",
                    QString("registerService failed lastError=\"%1\"")
                    .arg(QDBusConnection::sessionBus().lastError().message()) );
        fprintf(stderr, "%s\n",
                qPrintable(QDBusConnection::sessionBus().lastError().message()));
        return false;
    }

    QDBusConnection::sessionBus().registerObject("/", this, QDBusConnection::ExportAllSlots);
    TEMPODEBUG( "AQEMU_Service::init_service",
                QString("service registered name=\"%1\"").arg(SERVICE_NAME) );
    return true;
}

QString AQEMU_Service::start(const QString& s)
{
    return start( s, QString() );
}

QString AQEMU_Service::start(const QString& s, const QString& uid)
{
    TEMPODEBUG( "AQEMU_Service::start",
                QString("request vm=\"%1\" uid=\"%2\" machineCount_before=%3")
                .arg(s).arg(uid).arg(machines.count()) );
    QSettings settings;
    QString vm_dir = QDir::toNativeSeparators(settings.value("VM_Directory", QDir::homePath() + "/.aqemu/").toString());
    QString vm_file = vm_dir+s+".aqemu";
    TEMPODEBUG( "AQEMU_Service::start",
                QString("vm_dir=\"%1\" vm_file=\"%2\"").arg(vm_dir).arg(vm_file) );

    bool success = false;

    auto vm = new Virtual_Machine;
    TEMPODEBUG( "AQEMU_Service::start",
                QString("new VM ptr=%1").arg(reinterpret_cast<quintptr>(vm)) );
    if (QFileInfo(s).exists())
    {
        TEMPODEBUG( "AQEMU_Service::start",
                    QString("loading direct path=\"%1\"").arg(s) );
        success = vm->Load_VM(s);
        TEMPODEBUG( "AQEMU_Service::start",
                    QString("direct load result=%1").arg(success) );
    }

    if ( !success )
    {
        AQError("QString AQEMU_Service::start(const QString& s)",vm_file);
        TEMPODEBUG( "AQEMU_Service::start",
                    QString("direct load failed for \"%1\"; trying fallback vm_file").arg(s) );

        if(QFileInfo(vm_file).exists())
        {
            TEMPODEBUG( "AQEMU_Service::start",
                        QString("loading fallback path=\"%1\"").arg(vm_file) );
            success = vm->Load_VM(vm_file);
            TEMPODEBUG( "AQEMU_Service::start",
                        QString("fallback load result=%1").arg(success) );
        }
        else
        {
            TEMPODEBUG( "AQEMU_Service::start",
                        QString("fallback vm_file missing=\"%1\"").arg(vm_file) );
            return QString("VM \"%1\" could not be started. No such VM found.").arg(s);
        }
    }

    if( ! uid.isEmpty() )
    {
        TEMPODEBUG( "AQEMU_Service::start",
                    QString("setting UID old=\"%1\" new=\"%2\"")
                    .arg(vm->Get_UID()).arg(uid) );
        vm->Set_UID( uid );
    }

    // Connect state changes BEFORE Start(), because QEMU_Started()
    // (which emits State_Changed(VMS_Running)) can fire during
    // waitForStarted() inside Start_impl().
    connect(vm,SIGNAL(State_Changed( Virtual_Machine*, VM::VM_State)),this,SLOT(vm_state_changed(Virtual_Machine*, VM::VM_State)));

    if ( vm->Start() )
    {
        TEMPODEBUG( "AQEMU_Service::start",
                    QString("vm->Start success ptr=%1 uid=\"%2\" name=\"%3\" xml=\"%4\"")
                    .arg(reinterpret_cast<quintptr>(vm))
                    .arg(vm->Get_UID())
                    .arg(vm->Get_Machine_Name())
                    .arg(vm->Get_VM_XML_File_Path()) );
        machines.append(vm);
        TEMPODEBUG( "AQEMU_Service::start",
                    QString("machine appended machineCount_after=%1").arg(machines.count()) );

        AQError("QString AQEMU_Service::start(const QString& s)",s);
        return QString("VM \"%1\" got started.").arg(s);
    }

    TEMPODEBUG( "AQEMU_Service::start",
                QString("vm->Start failed ptr=%1 uid=\"%2\" name=\"%3\" xml=\"%4\"")
                .arg(reinterpret_cast<quintptr>(vm))
                .arg(vm->Get_UID())
                .arg(vm->Get_Machine_Name())
                .arg(vm->Get_VM_XML_File_Path()) );
    return QString("VM \"%1\" could not be started.").arg(s);
}

Virtual_Machine* AQEMU_Service::getMachine(const QString& s)
{
    const QFileInfo requested_info( s );
    const QString requested_canonical = requested_info.canonicalFilePath().isEmpty()
        ? requested_info.absoluteFilePath()
        : requested_info.canonicalFilePath();
    TEMPODEBUG( "AQEMU_Service::getMachine",
                QString("search vm=\"%1\" requested_canonical=\"%2\" machineCount=%3")
                .arg(s).arg(requested_canonical).arg(machines.count()) );

    for ( int i = 0; i < machines.count(); i++ )
    {
        const QFileInfo machine_info( machines.at(i)->Get_VM_XML_File_Path() );
        const QString machine_canonical = machine_info.canonicalFilePath().isEmpty()
            ? machine_info.absoluteFilePath()
            : machine_info.canonicalFilePath();
        TEMPODEBUG( "AQEMU_Service::getMachine",
                    QString("candidate[%1] ptr=%2 uid=\"%3\" name=\"%4\" xml=\"%5\" canonical=\"%6\"")
                    .arg(i)
                    .arg(reinterpret_cast<quintptr>(machines.at(i)))
                    .arg(machines.at(i)->Get_UID())
                    .arg(machines.at(i)->Get_Machine_Name())
                    .arg(machines.at(i)->Get_VM_XML_File_Path())
                    .arg(machine_canonical) );

        if ( ! requested_canonical.isEmpty() &&
             requested_canonical == machine_canonical )
        {
            TEMPODEBUG( "AQEMU_Service::getMachine",
                        QString("matched canonical index=%1").arg(i) );
            return machines.at(i);
        }

        if ( machines.at(i)->Get_VM_XML_File_Path() == s ||
             machines.at(i)->Get_Machine_Name() == s )
        {
            TEMPODEBUG( "AQEMU_Service::getMachine",
                        QString("matched exact path/name index=%1").arg(i) );
            return machines.at(i);
        }
    }

    TEMPODEBUG( "AQEMU_Service::getMachine",
                QString("no machine matched vm=\"%1\"").arg(s) );
    return nullptr;
}

QString AQEMU_Service::stop(const QString& s)
{
    if(auto machine = getMachine(s))
    {
        machine->Stop();
        return QString("VM \"%1\" got stopped.").arg(s);
    }

    return QString("VM \"%1\" could not be stopped.").arg(s);
}

QString AQEMU_Service::shutdown(const QString& s)
{
    if(auto machine = getMachine(s))
    {
        machine->Shutdown();
        return QString("shutting down VM \"%1\".").arg(s);
    }

    return QString("VM \"%1\" could not be shut down.").arg(s);
}

QString AQEMU_Service::reset(const QString& s)
{
    if(auto machine = getMachine(s))
    {
        machine->Reset();
        return QString("VM \"%1\" got reset.").arg(s);
    }

    return QString("VM \"%1\" could  not be reset.").arg(s);
}


QString AQEMU_Service::pause(const QString& s)
{
    if(auto machine = getMachine(s))
    {
        machine->Pause();
        return QString("VM \"%1\" got paused.").arg(s);
    }

    return QString("VM \"%1\" could not be paused.").arg(s);
}

QString AQEMU_Service::save(const QString& s)
{
    if(auto machine = getMachine(s))
    {
        machine->Save_VM_State();
        return QString("VM state of \"%1\" got saved.").arg(s);
    }

    return QString("VM state of \"%1\" could not be saved.").arg(s);
}

QString AQEMU_Service::monitor(const QString& s)
{
    if(auto machine = getMachine(s))
    {
        machine->Show_Monitor_Window();
        return QString("VM monitor window of \"%1\" got shown.").arg(s);
    }

    return QString("VM monitor window of \"%1\" could not be shown.").arg(s);
}

QString AQEMU_Service::error(const QString& s)
{
    return error( s, QString() );
}

QString AQEMU_Service::error(const QString& s, const QString& uid)
{
    TEMPODEBUG( "AQEMU_Service::error",
                QString("request vm=\"%1\" uid=\"%2\" machineCount=%3")
                .arg(s).arg(uid).arg(machines.count()) );
    if( ! uid.isEmpty() )
    {
        for( int i = 0; i < machines.count(); i++ )
        {
            if( machines.at(i)->Get_UID() == uid )
            {
                TEMPODEBUG( "AQEMU_Service::error",
                            QString("matched UID index=%1 ptr=%2 uid=\"%3\"")
                            .arg(i)
                            .arg(reinterpret_cast<quintptr>(machines.at(i)))
                            .arg(machines.at(i)->Get_UID()) );
                TEMPODEBUG( "AQEMU_Service::error",
                            QString("log count=%1 first=\"%2\" last=\"%3\"")
                            .arg(machines.at(i)->Get_QEMU_Error_Log_Entries().count())
                            .arg(machines.at(i)->Get_QEMU_Error_Log_Entries().value(0))
                            .arg(machines.at(i)->Get_QEMU_Error_Log_Entries().isEmpty()
                                 ? QString()
                                 : machines.at(i)->Get_QEMU_Error_Log_Entries().last()) );
                if( ! machines.at(i)->Get_QEMU_Error_Log_Entries().isEmpty() )
                {
                    machines.at(i)->Show_Error_Log_Window();
                    return QString("VM error log window of \"%1\" got shown.").arg(s);
                }

                const auto archived_it = Archived_Error_Logs.constFind( uid );
                if( archived_it != Archived_Error_Logs.constEnd() && ! archived_it->entries.isEmpty() )
                {
                    TEMPODEBUG( "AQEMU_Service::error",
                                QString("using archived logs uid=\"%1\" entries=%2")
                                .arg(uid)
                                .arg(archived_it->entries.count()) );
                    Show_Archived_Error_Log_Window(
                        QString("QEMU Error Log (%1)").arg(archived_it->vm_name.isEmpty() ? s : archived_it->vm_name),
                        archived_it->vm_xml,
                        archived_it->uid,
                        archived_it->entries );
                    return QString("VM error log window of \"%1\" got shown.").arg(s);
                }
                TEMPODEBUG( "AQEMU_Service::error",
                            QString("active VM had no entries and no archive uid=\"%1\"").arg(uid) );
                machines.at(i)->Show_Error_Log_Window();
                return QString("VM error log window of \"%1\" got shown.").arg(s);
            }
        }
        TEMPODEBUG( "AQEMU_Service::error",
                    QString("UID not found uid=\"%1\"").arg(uid) );
    }

    const auto archived_it = Archived_Error_Logs.constFind( uid );
    if( archived_it != Archived_Error_Logs.constEnd() && ! archived_it->entries.isEmpty() )
    {
        TEMPODEBUG( "AQEMU_Service::error",
                    QString("opening archived logs uid=\"%1\" entries=%2")
                    .arg(uid)
                    .arg(archived_it->entries.count()) );
        Show_Archived_Error_Log_Window(
            QString("QEMU Error Log (%1)").arg(archived_it->vm_name.isEmpty() ? s : archived_it->vm_name),
            archived_it->vm_xml,
            archived_it->uid,
            archived_it->entries );
        return QString("VM error log window of \"%1\" got shown.").arg(s);
    }

    if(auto machine = getMachine(s))
    {
        TEMPODEBUG( "AQEMU_Service::error",
                    QString("matched fallback ptr=%1 uid=\"%2\" name=\"%3\"")
                    .arg(reinterpret_cast<quintptr>(machine))
                    .arg(machine->Get_UID())
                    .arg(machine->Get_Machine_Name()) );
        TEMPODEBUG( "AQEMU_Service::error",
                    QString("log count=%1 first=\"%2\" last=\"%3\"")
                    .arg(machine->Get_QEMU_Error_Log_Entries().count())
                    .arg(machine->Get_QEMU_Error_Log_Entries().value(0))
                    .arg(machine->Get_QEMU_Error_Log_Entries().isEmpty()
                         ? QString()
                         : machine->Get_QEMU_Error_Log_Entries().last()) );
        if( ! machine->Get_QEMU_Error_Log_Entries().isEmpty() )
        {
            machine->Show_Error_Log_Window();
            return QString("VM error log window of \"%1\" got shown.").arg(s);
        }

        for( auto it = Archived_Error_Logs.begin(); it != Archived_Error_Logs.end(); ++it )
        {
            if( it.value().vm_xml == s && ! it.value().entries.isEmpty() )
            {
                TEMPODEBUG( "AQEMU_Service::error",
                            QString("opening archived logs by xml vm=\"%1\" entries=%2")
                            .arg(s)
                            .arg(it.value().entries.count()) );
                Show_Archived_Error_Log_Window(
                    QString("QEMU Error Log (%1)").arg(it.value().vm_name.isEmpty() ? s : it.value().vm_name),
                    it.value().vm_xml,
                    it.value().uid,
                    it.value().entries );
                return QString("VM error log window of \"%1\" got shown.").arg(s);
            }
        }

        machine->Show_Error_Log_Window();
        return QString("VM error log window of \"%1\" got shown.").arg(s);
    }

    TEMPODEBUG( "AQEMU_Service::error",
                QString("could not resolve vm=\"%1\" uid=\"%2\"").arg(s).arg(uid) );
    return QString("VM error log window of \"%1\" could not be shown.").arg(s);
}

QString AQEMU_Service::control(const QString& s)
{
    if(auto machine = getMachine(s))
    {
        machine->Show_Emu_Ctl_Win();
        return QString("VM control window of \"%1\" got shown.").arg(s);
    }

    return QString("VM control window of \"%1\" could not be shown.").arg(s);
}

QString AQEMU_Service::status(const QString& s)
{
    if(auto machine = getMachine(s))
    {
        vm_state_changed(machine,machine->Get_State());
        QString state = machine->Get_State_Text();
        return QString("VM state:  %1.").arg(state);
    }
    else if ( s.isEmpty() )
    {
        QString text;
        for ( int i = 0; i < machines.count(); i++ )
        {
            vm_state_changed(machines.at(i),machines.at(i)->Get_State());
            text += QString("VM \"%1\" state: %2.\n").arg(machines.at(i)->Get_Machine_Name(),machines.at(i)->Get_State_Text());
        }

        if ( ! text.isEmpty() )
            return text;
        else
            return QString("No VMs running.");
    }

    return QString("Could not show state of VM \"%1\".").arg(s);
}

QString AQEMU_Service::status()
{
    QString text;
    for ( int i = 0; i < machines.count(); i++ )
    {
        vm_state_changed(machines.at(i),machines.at(i)->Get_State());
        text += QString("VM \"%1\" state: %2.\n").arg(machines.at(i)->Get_Machine_Name(),machines.at(i)->Get_State_Text());
    }

    if ( ! text.isEmpty() )
        return text;
    else
        return QString("No VMs running.");
}

QString AQEMU_Service::command(const QString &vm, const QString &command)
{
    if(auto machine = getMachine(vm))
    {
        machine->Send_Emulator_Command(command);
        return QString("Sent command to VM \"%1\".").arg(vm);
    }

    return QString("Could not send command to VM \"%1\".").arg(vm);
}

QString AQEMU_Service::list()
{
    QSettings settings;
    QDir vm_dir( QDir::toNativeSeparators(settings.value("VM_Directory", "~").toString()) );
    QFileInfoList file = vm_dir.entryInfoList( QStringList("*.aqemu"), QDir::Files, QDir::Name );

    if( file.count() <= 0 )
        return "";

    QStringList list;
    for( int ix = 0; ix < file.count(); ix++ )
        list << file[ix].filePath();

    //return names of all available machines
    return list.join("\n");
}

void AQEMU_Service::clear_error_log(const QString& vm, const QString& uid)
{
    TEMPODEBUG( "AQEMU_Service::clear_error_log",
                QString("request vm=\"%1\" uid=\"%2\" machineCount=%3 archiveCount=%4")
                .arg(vm)
                .arg(uid)
                .arg(machines.count())
                .arg(Archived_Error_Logs.count()) );

    bool cleared_live = false;
    bool cleared_archive = false;

    for( Virtual_Machine *machine : machines )
    {
        if( machine == nullptr )
            continue;

        if( ( !uid.isEmpty() && machine->Get_UID() == uid ) ||
            ( !vm.isEmpty() && (machine->Get_VM_XML_File_Path() == vm || machine->Get_Machine_Name() == vm) ) )
        {
            machine->Set_QEMU_Error_Log_Entries( QStringList() );
            cleared_live = true;
            TEMPODEBUG( "AQEMU_Service::clear_error_log",
                        QString("cleared live log ptr=%1 uid=\"%2\" name=\"%3\" xml=\"%4\"")
                        .arg(reinterpret_cast<quintptr>(machine))
                        .arg(machine->Get_UID())
                        .arg(machine->Get_Machine_Name())
                        .arg(machine->Get_VM_XML_File_Path()) );
        }
    }

    if( !uid.isEmpty() && Archived_Error_Logs.contains(uid) )
    {
        Archived_Error_Logs[uid].entries.clear();
        cleared_archive = true;
        TEMPODEBUG( "AQEMU_Service::clear_error_log",
                    QString("cleared archived log by uid=\"%1\"").arg(uid) );
    }

    for( auto it = Archived_Error_Logs.begin(); it != Archived_Error_Logs.end(); ++it )
    {
        if( !vm.isEmpty() && (it.value().vm_xml == vm || it.value().vm_name == vm) )
        {
            it.value().entries.clear();
            cleared_archive = true;
            TEMPODEBUG( "AQEMU_Service::clear_error_log",
                        QString("cleared archived log by vm=\"%1\" uid=\"%2\"")
                        .arg(vm)
                        .arg(it.key()) );
        }
    }

    TEMPODEBUG( "AQEMU_Service::clear_error_log",
                QString("result live=%1 archive=%2").arg(cleared_live).arg(cleared_archive) );
}
