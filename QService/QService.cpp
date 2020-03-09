#include "QService.h"

Q_LOGGING_CATEGORY(QServiceLog, "QService")



QServiceManager::QServiceManager() {
    this->moveToThread(&thread);
}

bool QServiceManager::init(const QString &serviceName, QService *service) {

    this->service = service;
    this->serviceName = serviceName;

    QObject::connect(this, &QServiceManager::exit, [=](){
        thread.exit();
    });


    QObject::connect(&thread, &QThread::started, [=](){

        SERVICE_TABLE_ENTRYW serviceTable[] =
                {
                        {serviceName.toStdWString().data(), QServiceManager::serviceMain},
                        {nullptr, nullptr}
                };

        StartServiceCtrlDispatcherW(serviceTable);

        emit this->exit();
    });

    thread.start();

    return true;
}


void WINAPI QServiceManager::serviceMain(DWORD argc, LPWSTR *argv) {

    auto instance = QServiceManager::getInstance();

    instance->status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    instance->status.dwCurrentState = SERVICE_START_PENDING;

    DWORD controlsAccepted = 0;

    if (instance->service->canStop())
        controlsAccepted |= SERVICE_ACCEPT_STOP;
    if (instance->service->canShutdown())
        controlsAccepted |= SERVICE_ACCEPT_SHUTDOWN;
    if (instance->service->canPauseContinue())
        controlsAccepted |= SERVICE_ACCEPT_PAUSE_CONTINUE;

    instance->status.dwControlsAccepted = controlsAccepted;

    instance->status.dwWin32ExitCode = NO_ERROR;
    instance->status.dwServiceSpecificExitCode = 0;
    instance->status.dwCheckPoint = 0;
    instance->status.dwWaitHint = 0;

    instance->statusHandle = RegisterServiceCtrlHandlerW(
            instance->serviceName.toStdWString().c_str(), QServiceManager::serviceCtrHandler);

    instance->start(argc, argv);
}


void WINAPI QServiceManager::serviceCtrHandler(DWORD dwCtrl) {

    auto instance = QServiceManager::getInstance();

    switch (dwCtrl) {
        case SERVICE_CONTROL_STOP:
            instance->stop();
            break;
        case SERVICE_CONTROL_PAUSE:
            instance->pause();
            break;
        case SERVICE_CONTROL_CONTINUE:
            instance->resume();
            break;
        case SERVICE_CONTROL_SHUTDOWN:
            instance->shutdown();
            break;
        case SERVICE_CONTROL_INTERROGATE:
            break;
        default:
            break;
    }

}


bool QServiceManager::uninstall(const QString &serviceName) {

    SC_HANDLE schSCManager = nullptr;
    SC_HANDLE schService = nullptr;

    SERVICE_STATUS ssSvcStatus = {};

    schSCManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);

    if (schSCManager == nullptr) {

        qCritical(QServiceLog)  << "OpenSCManager failed: "
                                << QString::number(GetLastError(), 16);

        return false;
    }

    schService = OpenServiceW(schSCManager, serviceName.toStdWString().c_str(),
            SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE);

    if (schService == nullptr) {

        qCritical(QServiceLog)  << "OpenService failed: "
                                << QString::number(GetLastError(), 16);

        return false;
    }

    if (ControlService(schService, SERVICE_CONTROL_STOP, &ssSvcStatus)) {

        qInfo(QServiceLog) << "Stopping " << serviceName;

        ::Sleep(1000);

        while (QueryServiceStatus(schService, &ssSvcStatus)) {
            if (ssSvcStatus.dwCurrentState == SERVICE_STOP_PENDING) {
                ::Sleep(1000);
            } else break;
        }

        if (ssSvcStatus.dwCurrentState == SERVICE_STOPPED) {
            qInfo(QServiceLog) << serviceName << " is stopped";
        } else {
            qCritical(QServiceLog) << serviceName << " stop failed";
        }
    }

    bool result;

    if (!DeleteService(schService)) {

        qCritical(QServiceLog)  << "DeleteService failed: "
                                << QString::number(GetLastError(), 16);

        result = false;

    } else {
        qInfo(QServiceLog) << serviceName << " is removed";
        result = true;
    }

    CloseServiceHandle(schSCManager);
    CloseServiceHandle(schService);

    return result;
}


bool QServiceManager::install(
        const QString &serviceName,
        const QString &displayName,
        const QString &description,
        const QString &dependencies,
        const QServiceManager::StartType &startType,
        const QString &account,
        const QString &password) {

    wchar_t szPath[MAX_PATH];

    SC_HANDLE schSCManager = nullptr;
    SC_HANDLE schService = nullptr;

    if (GetModuleFileNameW(nullptr, szPath, ARRAYSIZE(szPath)) == 0) {

        qCritical(QServiceLog)  << "GetModuleFileName failed: "
                                << QString::number(GetLastError(), 16);

        return false;
    }

    schSCManager = OpenSCManagerW(nullptr, nullptr,
                                  SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);

    if (schSCManager == nullptr) {

        qCritical(QServiceLog)  << "OpenSCManager failed: "
                                << QString::number(GetLastError(), 16);

        return false;
    }

    schService = CreateServiceW(
            schSCManager,
            serviceName.toStdWString().c_str(),
            displayName.toStdWString().c_str(),
            SERVICE_ALL_ACCESS,
            SERVICE_WIN32_OWN_PROCESS,
            static_cast<DWORD>(startType),
            SERVICE_ERROR_NORMAL,
            szPath,
            nullptr,
            nullptr,
            dependencies.toStdWString().c_str(),
            account.toStdWString().c_str(),
            password.toStdWString().c_str()
    );

    if (schService == nullptr) {

        qCritical(QServiceLog)  << "CreateService failed: "
                                << QString::number(GetLastError(), 16);

        CloseServiceHandle(schSCManager);
        return false;
    }

    auto descriptionString = new wchar_t[description.length() + 1];
    auto descriptionLength = description.toWCharArray(descriptionString);
    descriptionString[descriptionLength] = '\0';
    SERVICE_DESCRIPTIONW info = { descriptionString };

    if (!ChangeServiceConfig2W(schService, SERVICE_CONFIG_DESCRIPTION, &info)) {

        qCritical(QServiceLog)  << "ChangeServiceConfig2 failed: "
                                << QString::number(GetLastError(), 16);

        delete[] descriptionString;
        CloseServiceHandle(schSCManager);
        return false;
    }

    delete[] descriptionString;

    qInfo(QServiceLog) << serviceName << " is installed";

    CloseServiceHandle(schSCManager);
    CloseServiceHandle(schService);

    return true;
}


void QServiceManager::start(DWORD argc, PWSTR *argv) {

    QStringList params;

    for (DWORD i = 0; i < argc; ++i)
        params.append(QString::fromStdWString(argv[i]));

    try {
        setServiceStatus(SERVICE_START_PENDING);
        emit service->onStart(params);
        setServiceStatus(SERVICE_RUNNING);
    }
    catch (DWORD dwError) {
        qCritical(QServiceLog)  << "Service start failed: "
                                << QString::number(dwError, 16);

        setServiceStatus(SERVICE_STOPPED, dwError);
    }
    catch (...) {
        qCritical(QServiceLog)  << "Service start failed";
        setServiceStatus(SERVICE_STOPPED);
    }
}


void QServiceManager::stop() {

    DWORD originalState = status.dwCurrentState;

    try {
        setServiceStatus(SERVICE_STOP_PENDING);
        emit service->onStop();
        setServiceStatus(SERVICE_STOPPED);
    }
    catch (DWORD dwError) {
        qCritical(QServiceLog)  << "Service stop failed: "
                                << QString::number(dwError, 16);

        setServiceStatus(originalState);
    }
    catch (...) {
        qCritical(QServiceLog)  << "Service stop failed";
        setServiceStatus(originalState);
    }
}


void QServiceManager::pause() {

    try {
        setServiceStatus(SERVICE_PAUSE_PENDING);
        emit service->onPause();
        setServiceStatus(SERVICE_PAUSED);
    }
    catch (DWORD dwError) {
        qCritical(QServiceLog)  << "Service pause failed: "
                                << QString::number(dwError, 16);

        setServiceStatus(SERVICE_RUNNING);
    }
    catch (...) {
        qCritical(QServiceLog)  << "Service pause failed";
        setServiceStatus(SERVICE_RUNNING);
    }
}


void QServiceManager::resume() {

    try {
        setServiceStatus(SERVICE_CONTINUE_PENDING);
        emit service->onContinue();
        setServiceStatus(SERVICE_RUNNING);
    }
    catch (DWORD dwError) {
        qCritical(QServiceLog)  << "Service resume failed: "
                                << QString::number(dwError, 16);

        setServiceStatus(SERVICE_PAUSED);
    }
    catch (...) {
        qCritical(QServiceLog)  << "Service resume failed";
        setServiceStatus(SERVICE_PAUSED);
    }
}


void QServiceManager::shutdown() {
    try {
        emit service->onShutdown();
        setServiceStatus(SERVICE_STOPPED);
    }
    catch (DWORD dwError) {
        qCritical(QServiceLog)  << "Service shutdown failed: "
                                << QString::number(dwError, 16);

    }
    catch (...) {
        qCritical(QServiceLog)  << "Service shutdown failed";
    }

}


void QServiceManager::setServiceStatus(DWORD currentState, DWORD win32ExitCode, DWORD waitHint) {

    static DWORD checkPoint = 1;

    status.dwCurrentState = currentState;
    status.dwWin32ExitCode = win32ExitCode;
    status.dwWaitHint = waitHint;

    status.dwCheckPoint =
            ((currentState == SERVICE_RUNNING) ||
             (currentState == SERVICE_STOPPED)) ?
            0 : checkPoint++;

    SetServiceStatus(statusHandle, &status);
}



