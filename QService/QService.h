#ifndef QSERVICE_QSERVICE_H
#define QSERVICE_QSERVICE_H

#include <QtCore>
#include <Windows.h>
#include <thread>

Q_DECLARE_LOGGING_CATEGORY(QServiceLog)

#if defined(SHARED_LIBRARY)
#  define Q_SHARED Q_DECL_EXPORT
#else
#  define Q_SHARED Q_DECL_IMPORT
#endif

class Q_SHARED QService : public QObject {

    Q_OBJECT

signals:
    void onStart(const QStringList &params);
    void onStop();
    void onPause();
    void onContinue();
    void onShutdown();

public:
    [[nodiscard]] virtual bool canStop() const { return false; };
    [[nodiscard]] virtual bool canShutdown() const { return false; };
    [[nodiscard]] virtual bool canPauseContinue() const { return false; };

};


class Q_SHARED QServiceManager : public QObject {

    Q_OBJECT

public:
    enum class StartType : DWORD {
        Boot=SERVICE_BOOT_START,
        System=SERVICE_SYSTEM_START,
        Auto=SERVICE_AUTO_START,
        Demand=SERVICE_DEMAND_START,
        Disabled=SERVICE_DISABLED,
    };

private:
    QThread thread;

    QString serviceName = "";
    QService *service = nullptr;
    SERVICE_STATUS status = {};
    SERVICE_STATUS_HANDLE statusHandle = nullptr;

private:
    QServiceManager();
    QServiceManager(const QServiceManager&) {};
    QServiceManager& operator=(QServiceManager&) { return *this; };
    ~QServiceManager() override = default;

signals:
    void exit();

public:

    static QServiceManager* getInstance() {
        static QServiceManager instance;
        return &instance;
    }

    static bool install(
            const QString &serviceName,
            const QString &displayName,
            const QString &description,
            const QString &dependencies,
            const QServiceManager::StartType &startType,
            const QString &account = "NT AUTHORITY\\LocalService",
            const QString &password = "");

    static bool uninstall(const QString &serviceName);

    bool init(const QString &serviceName, QService *serviceItem);

private:
    void start(DWORD argc, PWSTR *argv);
    void stop();
    void pause();
    void resume();
    void shutdown();

    static void WINAPI serviceMain(DWORD argc, LPWSTR *argv);
    static void WINAPI serviceCtrHandler(DWORD dwCtrl);

    void setServiceStatus(DWORD currentState, DWORD win32ExitCode = NO_ERROR, DWORD waitHint = 0);
};


#endif //QSERVICE_QSERVICE_H
